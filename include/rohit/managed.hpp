#pragma once

#include <rohit/managed_collaboration_hooks.hpp>
#include <rohit/managed_editor.hpp>
#include <rohit/managed_file_journal.hpp>
#include <rohit/managed_journal.hpp>
#include <rohit/managed_records.hpp>
#include <rohit/serializer.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed {

// Values are part of the version-one envelope contract.
enum class history_mode : std::uint32_t { disabled = 0, linear = 1, tree = 2 };
// Optional action descriptions are compiled out by default.
enum class history_labels { disabled, enabled };
// Runtime attachments can be compiled out independently; defaults preserve existing callers.
enum class store_features { none, journal, collaboration, all };

#if defined(_MSC_VER)
#define SERIALIZER_MANAGED_EMPTY_MEMBER [[msvc::no_unique_address]]
#else
#define SERIALIZER_MANAGED_EMPTY_MEMBER [[no_unique_address]]
#endif
// Only the authority may restore identities from its own validated, retained history.
template <typename Root, history_mode Mode, history_labels Labels, typename Traits,
          typename Session, typename SessionTraits, store_features Features>
class collaboration_authority;
enum class transaction_status { pending, committed, no_change, reverted, failed, indeterminate };

// Completion belongs to the caller and must outlive a manually scoped transaction.
struct transaction_outcome {
  transaction_status status{transaction_status::pending};
  std::exception_ptr error{};

  // Rethrow the recorded failure after transaction cleanup; cancellation is not failure.
  void throw_if_failed() const {
    if (error) {
      std::rethrow_exception(error);
    }
    if (status == transaction_status::indeterminate) {
      throw journal_indeterminate_error{"Managed transaction requires recovery"};
    }
    if (status == transaction_status::failed) {
      throw std::runtime_error{"Managed transaction failed"};
    }
  }
};

// Tree transactions additionally identify the committed or unchanged revision.
struct tree_transaction_outcome : transaction_outcome {
  std::uint64_t revision{};
};

// Stable document namespace, separate from the dense document-local object IDs.
struct document_id {
  std::uint64_t high{};
  std::uint64_t low{};
};

// Generate a new document namespace using the platform random source; entropy failures propagate.
// This runs once per new document, never for individual managed object IDs. Applications may
// still supply their own namespace when coordinating identity externally or writing fixtures.
inline document_id make_document_id() {
  std::random_device entropy;
  std::uniform_int_distribution<std::uint64_t> word;
  document_id result;
  do {
    result = {word(entropy), word(entropy)};
  } while (result.high == 0 && result.low == 0);
  return result;
}

struct store_options {
  static constexpr std::size_t default_snapshot_bytes = 64 * serializer::decode_limits::mebibyte;
  static constexpr std::size_t default_history_bytes = 256 * serializer::decode_limits::mebibyte;
  // Retained states including current and redo; linear mode evicts oldest, tree mode rejects excess.
  std::size_t max_revisions{1024};
  std::size_t max_snapshot_bytes{default_snapshot_bytes};
  std::size_t max_history_bytes{default_history_bytes};
  serializer::decode_limits decode{};
};

// Specialize for an ordinary generated payload and its explicit generated ID-bearing storage.
template <typename Root>
struct model_traits;

namespace detail {

// History-free stores retain only limits used by their model and decoder.
template <bool History>
struct store_configuration : store_options {
  // Preserve the caller's configured history and model limits.
  explicit store_configuration(store_options options) : store_options{options} {}
};
template <>
struct store_configuration<false> {
  static constexpr std::size_t max_revisions = 0;
  static constexpr std::size_t max_history_bytes = 0;
  std::size_t max_snapshot_bytes;
  serializer::decode_limits decode;
  // History limits have no storage or runtime role in a history-free specialization.
  explicit store_configuration(store_options options)
      : max_snapshot_bytes{options.max_snapshot_bytes}, decode{options.decode} {}
  // Reconstruct options only when building an isolated recovery or synchronization receiver.
  operator store_options() const {
    store_options result;
    result.max_revisions = max_revisions;
    result.max_history_bytes = max_history_bytes;
    result.max_snapshot_bytes = max_snapshot_bytes;
    result.decode = decode;
    return result;
  }
};

// Disabled attachments are compile-time null constants and occupy no object storage.
template <bool Enabled>
struct store_journal_storage {
  static constexpr journal_sink* sink = nullptr;
  static constexpr bool indeterminate = false;
};
template <>
struct store_journal_storage<true> {
  std::unique_ptr<journal_sink> sink{};
  bool indeterminate{};
  std::vector<std::uint8_t> saved_snapshot{};
};

template <bool History, typename IdentityTable>
struct store_restoration_storage {
  static constexpr const IdentityTable* restoration = nullptr;
};
template <typename IdentityTable>
struct store_restoration_storage<true, IdentityTable> {
  const IdentityTable* restoration{};
};

template <bool Enabled, typename Storage, typename IdentityTable, bool History = true>
struct store_collaboration_storage {
  static constexpr collaboration_attachment* attachment = nullptr;
  static constexpr bool busy = false;
  static constexpr const IdentityTable* restoration = nullptr;
};
template <typename Storage, typename IdentityTable, bool History>
struct store_collaboration_storage<true, Storage, IdentityTable, History>
    : store_restoration_storage<History, IdentityTable> {
  std::unique_ptr<collaboration_attachment> attachment{};
  bool busy{};
  void* prepare_context{};
  void (*prepare)(void*, const Storage&, const std::vector<std::uint8_t>&){};
};

// Encode with the existing keyed codec; native object layout is never a snapshot.
template <typename Value>
std::vector<std::uint8_t> encode(const Value& value,
                                 std::size_t max_bytes = store_options::default_snapshot_bytes) {
  if (max_bytes == 0) {
    throw std::length_error{"Managed encoding budget is zero"};
  }
  const stream_limits limits{std::min(stream_limits{}.min_read_buffer_bytes, max_bytes), max_bytes};
  full_stream_auto_alloc_limits stream{&limits};
  value.template serialize_out<serializer::binary_integer>(stream);
  return {stream.begin(), stream.begin() + stream.current_offset()};
}

// Decode a fresh, bounded value and reject trailing bytes before publishing anything.
template <typename Value>
Value decode(std::span<const std::uint8_t> bytes, serializer::decode_limits limits) {
  const auto input = make_constant_full_stream(bytes.data(), bytes.size());
  return serializer::deserialize_exact<Value, serializer::binary_integer>(input, limits);
}

// An empty base contributes no string storage when labels are disabled.
template <history_labels Labels>
struct label_storage {};

template <>
struct label_storage<history_labels::enabled> {
  std::string label{};
};

// Count only enabled metadata; disabled histories never read or construct a label.
template <history_labels Labels, typename Entry>
std::size_t label_bytes(const Entry& entry) noexcept {
  if constexpr (Labels == history_labels::enabled) {
    return entry.label.size();
  } else {
    static_cast<void>(entry);
    return 0;
  }
}

// Linear ancestry is implicit in deque order; no per-revision parent or shared ownership is needed.
template <history_labels Labels = history_labels::disabled>
struct history_entry : label_storage<Labels> {
  std::vector<std::uint8_t> snapshot{};

  // Return retained byte cost; append/load validate the sum before admitting a revision.
  std::size_t bytes() const noexcept {
    return label_bytes<Labels>(*this) + snapshot.size();
  }
};

template <history_labels Labels = history_labels::disabled>
struct linear_history {
  using entry_type = history_entry<Labels>;
  std::deque<entry_type> entries{};
  std::size_t cursor{};
  std::size_t bytes{};

  struct append_plan {
    std::size_t keep_count{};
    std::size_t evict_count{};
    std::size_t retained_bytes{};
    std::size_t node_bytes{};
  };

  // Calculate retention before any allocation or durable write changes the selected history.
  append_plan prepare_append(const entry_type& node, const store_options& options) const {
    if (options.max_revisions == 0 || node.snapshot.size() > options.max_history_bytes ||
        label_bytes<Labels>(node) > options.max_history_bytes - node.snapshot.size()) {
      throw std::length_error{"Managed revision cannot fit history budget"};
    }
    const auto node_bytes = node.bytes();
    const auto keep_count = entries.empty() ? 0 : cursor + 1;
    auto retained_bytes = bytes;
    for (auto index = keep_count; index < entries.size(); ++index) {
      retained_bytes -= entries[index].bytes();
    }
    std::size_t evict_count = 0;
    while (keep_count - evict_count >= options.max_revisions ||
           retained_bytes > options.max_history_bytes - node_bytes) {
      retained_bytes -= entries[evict_count++].bytes();
    }

    return {keep_count, evict_count, retained_bytes, node_bytes};
  }

  // Publish an already allocated tail entry using only swaps and removals; cannot fail after flush.
  void publish_append(const append_plan& plan) noexcept {
    static_assert(std::is_nothrow_swappable_v<entry_type>);
    if (plan.keep_count < entries.size() - 1) {
      std::swap(entries[plan.keep_count], entries.back());
      while (entries.size() > plan.keep_count + 1) {
        entries.pop_back();
      }
    }
    for (std::size_t index = 0; index < plan.evict_count; ++index) {
      entries.pop_front();
    }
    cursor = entries.size() - 1;
    bytes = plan.retained_bytes + plan.node_bytes;
  }

  // Append atomically; single-element deque insertion leaves existing entries intact on failure.
  void append(entry_type node, const store_options& options) {
    const auto plan = prepare_append(node, options);
    entries.push_back(std::move(node));
    publish_append(plan);
  }
};

template <history_labels Labels>
using tree_revision = std::conditional_t<Labels == history_labels::enabled, records::revision,
                                         records::unlabeled_revision>;

template <history_labels Labels>
using tree_history = std::map<std::uint64_t, std::shared_ptr<const tree_revision<Labels>>>;

// Each store specialization contains only its selected history representation.
template <history_mode Mode, history_labels Labels = history_labels::disabled>
struct history_storage {};

template <history_labels Labels>
struct history_storage<history_mode::linear, Labels> : linear_history<Labels> {
  // Publish prepared linear history without allocating or moving retained entries.
  void swap(history_storage& other) noexcept {
    this->entries.swap(other.entries);
    std::swap(this->cursor, other.cursor);
    std::swap(this->bytes, other.bytes);
  }
};

template <history_labels Labels>
struct history_storage<history_mode::tree, Labels> {
  tree_history<Labels> revisions{};
  std::size_t bytes{};
  std::uint64_t current_revision{1};
  std::uint64_t revision_high_water{1};

  // Publish a prepared tree without allocating or moving retained records.
  void swap(history_storage& other) noexcept {
    revisions.swap(other.revisions);
    std::swap(bytes, other.bytes);
    std::swap(current_revision, other.current_revision);
    std::swap(revision_high_water, other.revision_high_water);
  }
};

} // namespace detail

// Single-thread-confined owning snapshot store. Adapters and callbacks must not leak mutable aliases.
template <typename Root, history_mode Mode = history_mode::linear,
          history_labels Labels = history_labels::disabled, typename Traits = model_traits<Root>,
          store_features Features = store_features::all>
class model_store {
  template <typename, typename, typename>
  friend class store_collaboration;
  template <typename, history_mode, history_labels, typename, typename, typename, store_features>
  friend class collaboration_authority;

public:
  static constexpr bool has_journal =
      Features == store_features::journal || Features == store_features::all;
  static constexpr bool has_collaboration =
      Features == store_features::collaboration || Features == store_features::all;
  static constexpr bool has_history = Mode != history_mode::disabled;
  static_assert(Features == store_features::none || Features == store_features::journal ||
                Features == store_features::collaboration || Features == store_features::all);
  using traits_type = Traits;
  using outcome_type =
      std::conditional_t<Mode == history_mode::tree, tree_transaction_outcome, transaction_outcome>;
  using storage_type = typename Traits::storage_type;
  using id_type = typename Traits::id_type;
  static_assert(std::same_as<id_type, std::uint32_t> || std::same_as<id_type, std::uint64_t>);
  static_assert(std::same_as<decltype(storage_type{}.persistent_id), id_type>);
  static_assert(Labels == history_labels::disabled || Labels == history_labels::enabled);
  static_assert(Mode != history_mode::disabled || Labels == history_labels::disabled,
                "Labels require enabled history");
  static_assert(Mode == history_mode::disabled || Mode == history_mode::linear ||
                Mode == history_mode::tree);

private:
  static constexpr bool has_labels = Labels == history_labels::enabled;
  static constexpr std::uint32_t format_version =
      has_labels ? (Mode == history_mode::tree ? 1 : 2) : 3;
  using label_type = detail::label_storage<Labels>;
  using entry_type = detail::history_entry<Labels>;
  using revision_type = detail::tree_revision<Labels>;
  using saved_entry_type =
      std::conditional_t<has_labels, records::history_entry, records::unlabeled_history_entry>;
  using tree_envelope_type =
      std::conditional_t<has_labels, records::envelope, records::unlabeled_envelope>;
  using state_envelope_type =
      std::conditional_t<has_labels, records::state_envelope, records::unlabeled_state_envelope>;
  using envelope_type =
      std::conditional_t<Mode == history_mode::tree, tree_envelope_type, state_envelope_type>;
  using identity_table = std::map<id_type, std::string>;
  using history_type = detail::history_storage<Mode, Labels>;

  document_id document_{};
  detail::store_configuration<has_history> options_{store_options{}};
  std::shared_ptr<const storage_type> current_{};
  std::vector<std::uint8_t> snapshot_{};
  identity_table identities_{};
  SERIALIZER_MANAGED_EMPTY_MEMBER
  detail::store_collaboration_storage<has_collaboration, storage_type, identity_table, has_history>
      collaboration_state_{};
  std::uint64_t allocated_id_{};
  bool writer_active_{};
  bool callback_active_{};
  const std::thread::id thread_{std::this_thread::get_id()};
  std::function<void(const storage_type&)> validate_{};
  SERIALIZER_MANAGED_EMPTY_MEMBER detail::history_storage<Mode, Labels> history_{};
  SERIALIZER_MANAGED_EMPTY_MEMBER detail::store_journal_storage<has_journal> journal_state_{};

  struct recovery_tag {};

  // Build a private empty receiver so recovery can validate everything before live publication.
  model_store(recovery_tag, store_options options,
              std::function<void(const storage_type&)> validate)
      : options_{options}, validate_{std::move(validate)} {}

  // Preserve the uncertainty of issued durable writes in all transaction completion forms.
  transaction_status failure_status() const noexcept {
    return journal_state_.indeterminate ||
                   (journal_state_.sink && journal_state_.sink->needs_recovery())
               ? transaction_status::indeterminate
               : transaction_status::failed;
  }

  // Block reentrant store mutation from storage hooks and restore the guard on every exception.
  void journal_operation(auto&& operation) {
    static_assert(has_journal);
    if (callback_active_) {
      throw std::logic_error{"Reentrant managed journal operation"};
    }
    callback_active_ = true;
    try {
      operation();
      callback_active_ = false;
    } catch (const journal_indeterminate_error&) {
      // Creation can publish a file before returning its adapter; fence that case as well.
      journal_state_.indeterminate = true;
      callback_active_ = false;
      throw;
    } catch (...) {
      callback_active_ = false;
      throw;
    }
  }

  // Export prepared state without publishing it; existing envelope wire versions remain unchanged.
  std::vector<std::uint8_t> encode_state(const std::vector<std::uint8_t>& snapshot,
                                         const history_type& history,
                                         std::uint64_t allocated_id) const {
    envelope_type envelope;
    envelope.format_version = format_version;
    envelope.schema_id = std::string{Traits::schema_id};
    envelope.id_bits = std::numeric_limits<id_type>::digits;
    envelope.document_high = document_.high;
    envelope.document_low = document_.low;
    envelope.allocated_id = allocated_id;
    envelope.mode = static_cast<std::uint32_t>(Mode);
    envelope.current_snapshot = snapshot;
    if constexpr (Mode == history_mode::linear) {
      envelope.cursor = history.cursor;
      for (const auto& node : history.entries) {
        saved_entry_type entry;
        if constexpr (has_labels) {
          entry.label = node.label;
        }
        entry.snapshot = node.snapshot;
        envelope.entries.push_back(std::move(entry));
      }
    } else if constexpr (Mode == history_mode::tree) {
      envelope.revision_high_water = history.revision_high_water;
      envelope.current_revision = history.current_revision;
      for (const auto& [number, node] : history.revisions) {
        static_cast<void>(number);
        envelope.revisions.push_back(*node);
      }
    }
    return detail::encode(envelope, options_.decode.max_input_bytes);
  }

  // Check that this writer's envelope fits the same decoder budgets used by recovery.
  void check_journal_envelope(const std::vector<std::uint8_t>& bytes) const {
    static_cast<void>(detail::decode<envelope_type>(bytes, options_.decode));
  }

  static constexpr std::size_t journal_word_bytes = 8;

  // Persist navigation, reservation, or reset with a fixed stack buffer and no model encoding.
  void journal_control(detail::journal_record_kind kind, std::uint64_t argument = 0) {
    if constexpr (has_journal) {
      if (journal_state_.sink) {
        std::array<std::uint8_t, 1 + journal_word_bytes> record{};
        record.front() = static_cast<std::uint8_t>(kind);
        const auto length = kind == detail::journal_record_kind::reset ? 1 : record.size();
        if (length != 1) {
          detail::journal_put_word(record, 1, argument);
        }
        journal_operation([&] { journal_state_.sink->append(std::span{record}.first(length)); });
      }
    }
  }

  // Append the already serialized snapshot directly, once. Framing/metadata use stack storage;
  // prior history, document/schema metadata, and unchanged base bytes are never re-encoded here.
  void journal_edit(const std::vector<std::uint8_t>& snapshot, const label_type& action,
                    std::uint64_t allocation_base, std::size_t evicted) {
    if (collaboration_state_.attachment) {
      if constexpr (has_labels) {
        collaboration_state_.attachment->prepare(snapshot, action.label);
      } else {
        collaboration_state_.attachment->prepare(snapshot, {});
      }
    }
    if constexpr (has_journal) {
      if (!journal_state_.sink) {
        return;
      }
      constexpr auto prefix_bytes =
          1 +
          journal_word_bytes * (1 + (Mode == history_mode::linear ? 1 : 0) + (has_labels ? 1 : 0));
      std::array<std::uint8_t, prefix_bytes> prefix{};
      prefix.front() = static_cast<std::uint8_t>(collaboration_state_.restoration
                                                     ? detail::journal_record_kind::restore_edit
                                                     : detail::journal_record_kind::edit);
      std::size_t offset = 1;
      if constexpr (Mode == history_mode::linear) {
        detail::journal_put_word(prefix, offset, evicted);
        offset += journal_word_bytes;
      } else {
        static_cast<void>(evicted);
      }
      detail::journal_put_word(prefix, offset, allocation_base);
      offset += journal_word_bytes;
      std::span<const std::uint8_t> label;
      if constexpr (has_labels) {
        detail::journal_put_word(prefix, offset, action.label.size());
        label = {reinterpret_cast<const std::uint8_t*>(action.label.data()), action.label.size()};
      } else {
        static_cast<void>(action);
      }
      journal_operation([&] { journal_state_.sink->append(prefix, label, snapshot); });
    }
  }

  // Replay stored bytes into an isolated receiver. Never invoke editing callbacks or external effects.
  // The base supplies schema/profile/policy; the file adapter checks sequence and checksum chaining.
  void replay_journal_record(std::span<const std::uint8_t> record) {
    if (record.empty()) {
      throw std::invalid_argument{"Empty managed journal operation"};
    }
    std::size_t offset = 1;
    const auto kind = static_cast<detail::journal_record_kind>(record.front());
    if (kind == detail::journal_record_kind::reset) {
      if (record.size() != offset) {
        throw std::invalid_argument{"Trailing managed history reset bytes"};
      }
      if constexpr (has_history) {
        reset_history();
        return;
      } else {
        throw std::invalid_argument{"History reset in a history-disabled journal"};
      }
    }
    if (kind == detail::journal_record_kind::reserve ||
        kind == detail::journal_record_kind::select) {
      const auto argument = detail::journal_get_word(record, offset);
      if (offset != record.size()) {
        throw std::invalid_argument{"Trailing managed journal control bytes"};
      }
      if (kind == detail::journal_record_kind::reserve) {
        if (allocated_id_ == std::numeric_limits<id_type>::max() || argument != allocated_id_ + 1) {
          throw std::invalid_argument{"Invalid managed journal ID reservation"};
        }
        allocated_id_ = argument;
      } else if constexpr (Mode == history_mode::linear) {
        if (argument >= history_.entries.size()) {
          throw std::invalid_argument{"Invalid managed journal linear selection"};
        }
        checkout_linear(static_cast<std::size_t>(argument));
      } else if constexpr (Mode == history_mode::tree) {
        checkout(argument);
      } else {
        throw std::invalid_argument{"History selection in a history-disabled journal"};
      }
      return;
    }
    if (kind != detail::journal_record_kind::edit &&
        kind != detail::journal_record_kind::restore_edit) {
      throw std::invalid_argument{"Unsupported managed journal operation"};
    }
    std::uint64_t evicted = 0;
    if constexpr (Mode == history_mode::linear) {
      evicted = detail::journal_get_word(record, offset);
    }
    const auto allocation_base = detail::journal_get_word(record, offset);
    label_type action;
    if constexpr (has_labels) {
      const auto length = detail::journal_get_word(record, offset);
      if (length > record.size() - offset || length > options_.max_history_bytes) {
        throw std::length_error{"Managed journal label budget exceeded"};
      }
      action.label.assign(reinterpret_cast<const char*>(record.data() + offset),
                          static_cast<std::size_t>(length));
      offset += static_cast<std::size_t>(length);
    }
    if (allocation_base > allocated_id_) {
      throw std::invalid_argument{"Invalid managed journal allocation boundary"};
    }
    const auto length = record.size() - offset;
    if (length > options_.max_snapshot_bytes || length > options_.decode.max_input_bytes) {
      throw std::length_error{"Managed journal snapshot budget exceeded"};
    }
    std::vector<std::uint8_t> snapshot{record.begin() + static_cast<std::ptrdiff_t>(offset),
                                       record.end()};
    auto value = detail::decode<storage_type>(snapshot, options_.decode);
    auto identities = inspect(value, allocated_id_);
    if (value.persistent_id != current_->persistent_id || snapshot == snapshot_) {
      throw std::invalid_argument{"Journal edit changes root identity or records no change"};
    }
    for (const auto& [id, type] : identities) {
      const auto old = identities_.find(id);
      if ((old != identities_.end() && old->second != type) ||
          (old == identities_.end() && id <= allocation_base &&
           kind != detail::journal_record_kind::restore_edit)) {
        throw std::invalid_argument{"Journal edit reuses or retypes an identity"};
      }
    }
    validate(value, false);
    auto published = std::make_shared<const storage_type>(std::move(value));
    if constexpr (Mode == history_mode::linear) {
      const auto keep_count = history_.cursor + 1;
      if (evicted > keep_count) {
        throw std::invalid_argument{"Invalid managed journal history eviction"};
      }
      // Replay the writer's exact eviction decision; reader limits may reject it, never change it.
      while (history_.entries.size() > keep_count) {
        history_.bytes -= history_.entries.back().bytes();
        history_.entries.pop_back();
      }
      for (std::uint64_t index = 0; index < evicted; ++index) {
        history_.bytes -= history_.entries.front().bytes();
        history_.entries.pop_front();
      }
      entry_type entry;
      entry.snapshot = snapshot;
      if constexpr (has_labels) {
        entry.label = std::move(action.label);
      }
      if (history_.entries.size() >= options_.max_revisions ||
          entry.snapshot.size() > options_.max_history_bytes ||
          detail::label_bytes<Labels>(entry) > options_.max_history_bytes - entry.snapshot.size() ||
          history_.bytes > options_.max_history_bytes - entry.bytes()) {
        throw std::length_error{"Managed journal history budget exceeded"};
      }
      const auto entry_bytes = entry.bytes();
      history_.entries.push_back(std::move(entry));
      history_.bytes += entry_bytes;
      history_.cursor = history_.entries.size() - 1;
    } else {
      if constexpr (Mode == history_mode::tree) {
        auto node = std::make_shared<revision_type>();
        node->number = next_revision();
        node->parent = history_.current_revision;
        node->snapshot = snapshot;
        if constexpr (has_labels) {
          node->label = std::move(action.label);
        }
        const auto bytes = tree_append_bytes(history_, *node);
        history_.revisions.emplace(node->number, node);
        history_.bytes = bytes;
        history_.current_revision = node->number;
        history_.revision_high_water = node->number;
      }
    }
    current_.swap(published);
    snapshot_.swap(snapshot);
    identities_.swap(identities);
  }

  // Reject cross-thread use before touching store state; external serialization is still required.
  void check_thread() const {
    if (thread_ != std::this_thread::get_id()) {
      throw std::logic_error{"Managed store used from another thread"};
    }
  }

  // Require a closed writer for navigation and export; callbacks cannot reenter the store.
  void require_idle() const {
    check_thread();
    if (writer_active_ || callback_active_ || collaboration_state_.busy) {
      throw std::logic_error{"Managed store already has an active operation"};
    }
    if (journal_state_.indeterminate ||
        (journal_state_.sink && journal_state_.sink->needs_recovery())) {
      throw journal_indeterminate_error{"Reopen the managed journal before further operations"};
    }
  }

  // Consume an ID permanently, including on later cancellation or publication failure.
  id_type allocate_id() {
    if (allocated_id_ == std::numeric_limits<id_type>::max()) {
      throw std::overflow_error{"Managed entity ID space exhausted"};
    }
    const auto next = allocated_id_ + 1;
    // Reservations precede exposing an ID, so canceled and failed edits cannot recycle it on restart.
    journal_control(detail::journal_record_kind::reserve, next);
    allocated_id_ = next;
    return static_cast<id_type>(next);
  }

  // Inspect explicit generated ID fields; adapters supply stable type keys for all owned entities.
  identity_table inspect(const storage_type& value, std::uint64_t high_water) const {
    identity_table result;
    Traits::visit_entities(value, [&](id_type id, std::string_view type) {
      if (id == 0 || id > high_water || type.empty() ||
          !result.emplace(id, std::string{type}).second) {
        throw std::invalid_argument{"Invalid managed entity identity"};
      }
    });
    if (value.persistent_id == 0 || !result.contains(value.persistent_id)) {
      throw std::invalid_argument{"Managed root identity is missing"};
    }
    return result;
  }

  // Apply application invariants without allowing recursive navigation or nested publication.
  void validate(const storage_type& value, bool application_policy = true) {
    if (callback_active_) {
      throw std::logic_error{"Reentrant managed validation"};
    }
    callback_active_ = true;
    try {
      Traits::validate(value);
      if (application_policy && validate_) {
        validate_(value);
      }
      callback_active_ = false;
    } catch (...) {
      callback_active_ = false;
      throw;
    }
  }

  // Reject a snapshot exceeding the configured retained or decoding boundary.
  void check_snapshot(const std::vector<std::uint8_t>& snapshot) const {
    if (snapshot.size() > options_.max_snapshot_bytes ||
        snapshot.size() > options_.decode.max_input_bytes) {
      throw std::length_error{"Managed snapshot budget exceeded"};
    }
  }

  // Reserve monotonically increasing revision identity before publication; never wrap.
  std::uint64_t next_revision() const
    requires(Mode == history_mode::tree)
  {
    if (history_.revision_high_water == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error{"Managed revision ID space exhausted"};
    }
    return history_.revision_high_water + 1;
  }

  // Check one prospective tree append in constant time, before allocation or durable publication.
  std::size_t tree_append_bytes(const history_type& history, const revision_type& node) const
    requires(Mode == history_mode::tree)
  {
    if (history.revisions.size() >= options_.max_revisions) {
      throw std::length_error{"Managed revision budget exceeded"};
    }
    const auto labels = detail::label_bytes<Labels>(node);
    if (node.snapshot.size() > options_.max_history_bytes ||
        labels > options_.max_history_bytes - node.snapshot.size() ||
        history.bytes > options_.max_history_bytes - node.snapshot.size() - labels) {
      throw std::length_error{"Managed history byte budget exceeded"};
    }
    return history.bytes + node.snapshot.size() + labels;
  }

  // Prepare a fresh baseline in the compile-time history mode without modifying retained state.
  detail::history_storage<Mode, Labels> make_history() const {
    detail::history_storage<Mode, Labels> result;
    if constexpr (Mode == history_mode::linear) {
      entry_type entry;
      entry.snapshot = snapshot_;
      result.append(std::move(entry), options_);
    } else if constexpr (Mode == history_mode::tree) {
      result.current_revision = history_.current_revision;
      result.revision_high_water = history_.revision_high_water;
      auto node = std::make_shared<revision_type>();
      node->number = history_.current_revision;
      node->snapshot = snapshot_;
      result.bytes = tree_append_bytes(result, *node);
      result.revisions.emplace(history_.current_revision, std::move(node));
    }
    return result;
  }

  // Decode and validate a retained state before publishing; failure leaves the cursor unchanged.
  void restore_snapshot(const std::vector<std::uint8_t>& bytes, std::uint64_t selection) {
    auto value = detail::decode<storage_type>(bytes, options_.decode);
    auto identities = inspect(value, allocated_id_);
    validate(value);
    auto published = std::make_shared<const storage_type>(std::move(value));
    auto snapshot = bytes;
    journal_control(detail::journal_record_kind::select, selection);
    current_.swap(published);
    snapshot_.swap(snapshot);
    identities_.swap(identities);
  }

  // Navigate by deque position; the caller has checked that the index is retained.
  void checkout_linear(std::size_t index)
    requires(Mode == history_mode::linear)
  {
    const auto& node = history_.entries[index];
    restore_snapshot(node.snapshot, index);
    history_.cursor = index;
  }

public:
  class transaction;

  // Borrowed callback facade: mutation/cancellation only, with no early commit or guard transfer.
  class transaction_edit {
    transaction& owner_;
    friend class model_store;

    // Bind the facade to one synchronous callback invocation.
    explicit transaction_edit(transaction& owner) : owner_{owner} {}

  public:
    transaction_edit(const transaction_edit&) = delete;
    transaction_edit& operator=(const transaction_edit&) = delete;

    // Edit an isolated generated storage candidate; any escaping error poisons the transaction.
    template <typename Callable>
      requires std::same_as<std::invoke_result_t<Callable, storage_type&>, void>
    void update(Callable&& callback) {
      owner_.update(std::forward<Callable>(callback));
    }

    // Borrow the generated root editor for this callback's transaction.
    auto root() {
      return owner_.root();
    }

    // Reserve a persistent ID for a newly inserted generated storage record.
    id_type create_id() {
      return owner_.create_id();
    }

    // Cancel the unpublished action; callback return cannot commit it afterward.
    void revert() {
      owner_.revert();
    }
  };

  // Move-only scope guard. Store and outcome outlive it, and all operations stay on its owner thread.
  class transaction : private label_type {
    model_store* store_{};
    outcome_type* outcome_{};
    std::unique_ptr<storage_type> candidate_{};
    std::uint64_t allocation_base_{};
    int exceptions_{};
    bool editing_{};
    std::shared_ptr<detail::edit_channel<storage_type, id_type>> channel_{};
    friend class model_store;

    // Acquire the sole writer only after candidate construction has succeeded.
    transaction(model_store& store, outcome_type& outcome, label_type label = {})
        : label_type{std::move(label)}, store_{&store}, outcome_{&outcome},
          allocation_base_{store.allocated_id_}, exceptions_{std::uncaught_exceptions()} {
      store.require_idle();
      if (exceptions_ != 0) {
        throw std::logic_error{"Cannot begin a transaction during stack unwinding"};
      }
      candidate_ = std::make_unique<storage_type>(
          detail::decode<storage_type>(store.snapshot_, store.options_.decode));
      outcome = {};
      store.writer_active_ = true;
    }

    // Require an active guard without exposing stale candidate access.
    void require_active() {
      if (!store_) {
        throw std::logic_error{"Managed transaction is closed"};
      }
      store_->check_thread();
      if (store_->callback_active_) {
        reject("Cannot edit during managed validation");
      }
      outcome_->throw_if_failed();
    }

    // Release candidate and writer exactly once; no allocations or inverse decoding are needed.
    void close(transaction_status status, std::exception_ptr error = {}) noexcept {
      outcome_->status = status;
      outcome_->error = std::move(error);
      if (channel_) {
        channel_->owner = nullptr;
      }
      candidate_.reset();
      store_->writer_active_ = false;
      store_ = nullptr;
    }

    // Record failure without throwing, including from a destructor or poisoned edit.
    void fail(std::exception_ptr error) noexcept {
      if (store_) {
        if (editing_ || store_->callback_active_) {
          outcome_->status = store_->failure_status();
          outcome_->error = std::move(error);
        } else {
          close(store_->failure_status(), std::move(error));
        }
      }
    }

    // Poison invalid reentrant edits without destroying a candidate still borrowed by a callback.
    [[noreturn]] void reject(const char* message) {
      try {
        throw std::logic_error{message};
      } catch (...) {
        fail(std::current_exception());
        throw;
      }
    }

  public:
    transaction(const transaction&) = delete;
    transaction& operator=(const transaction&) = delete;
    transaction& operator=(transaction&&) = delete;

    // Transfer sole cleanup responsibility; moving during a borrowed edit is prohibited.
    transaction(transaction&& other) {
      other.require_active();
      if (other.editing_) {
        other.reject("Cannot move a transaction during an edit");
      }
      store_ = std::exchange(other.store_, nullptr);
      outcome_ = other.outcome_;
      candidate_ = std::move(other.candidate_);
      static_cast<label_type&>(*this) = std::move(static_cast<label_type&>(other));
      allocation_base_ = other.allocation_base_;
      exceptions_ = other.exceptions_;
      channel_ = std::move(other.channel_);
      if (channel_) {
        channel_->owner = this;
      }
    }

    // Complete once on healthy normal scope exit; never throw while releasing resources.
    ~transaction() noexcept {
      if (!store_) {
        return;
      }
      if (store_->thread_ != std::this_thread::get_id()) {
        std::terminate();
      }
      if (std::uncaught_exceptions() > exceptions_) {
        close(transaction_status::reverted);
      } else {
        try {
          commit();
        } catch (...) {
          fail(std::current_exception());
        }
      }
    }

    // Create a generated editor with checked lifetime and no writable identity fields.
    auto root() {
      require_active();
      if (!channel_) {
        channel_ = std::make_shared<detail::edit_channel<storage_type, id_type>>();
        channel_->owner = this;
        channel_->mutate = [](void* owner, void* callable, void (*invoke)(void*, storage_type&)) {
          static_cast<transaction*>(owner)->update(
              [&](storage_type& value) { invoke(callable, value); });
        };
        channel_->allocate = [](void* owner) {
          return static_cast<transaction*>(owner)->create_id();
        };
        channel_->fail = [](void* owner, std::exception_ptr error) noexcept {
          static_cast<transaction*>(owner)->fail(std::move(error));
        };
      }
      auto resolve = [](storage_type& value) -> storage_type& { return value; };
      return Traits::make_editor(
          detail::editor_access<storage_type, storage_type, true, decltype(resolve)>{channel_,
                                                                                     resolve});
    }

    // Mutate only the private candidate; failure closes the entire action even if caught by the caller.
    template <typename Callable>
      requires std::same_as<std::invoke_result_t<Callable, storage_type&>, void>
    void update(Callable&& callback) {
      require_active();
      if (editing_) {
        reject("Nested candidate callbacks are not supported");
      }
      editing_ = true;
      try {
        std::invoke(std::forward<Callable>(callback), *candidate_);
        editing_ = false;
        outcome_->throw_if_failed();
      } catch (...) {
        editing_ = false;
        fail(std::current_exception());
        throw;
      }
    }

    // Allocate outside undoable state. Call before update when initializing a new record.
    id_type create_id() {
      require_active();
      if (editing_) {
        reject("Allocate IDs before entering a candidate update callback");
      }
      try {
        return store_->allocate_id();
      } catch (...) {
        fail(std::current_exception());
        throw;
      }
    }

    // Cancel all edits; forbid destruction of a candidate while its callback is executing.
    void revert() {
      require_active();
      if (editing_) {
        reject("Revert must occur outside a candidate update callback");
      }
      close(transaction_status::reverted);
    }

    // Prepare every allocation before atomic pointer/index swaps; record and rethrow failures.
    void commit() {
      require_active();
      if (editing_) {
        reject("Commit must occur outside a candidate update callback");
      }
      try {
        auto& store = *store_;
        store.validate(*candidate_);
        outcome_->throw_if_failed();
        auto identities = store.inspect(*candidate_, store.allocated_id_);
        if (candidate_->persistent_id != store.current_->persistent_id) {
          throw std::invalid_argument{"Cannot replace the root identity"};
        }
        for (const auto& [id, type] : identities) {
          const auto previous = store.identities_.find(id);
          bool restored = false;
          if constexpr (has_collaboration && has_history) {
            if (const auto* restoration = store.collaboration_state_.restoration) {
              const auto found = restoration->find(id);
              restored = found != restoration->end() && found->second == type;
            }
          }
          if ((previous != store.identities_.end() && previous->second != type) ||
              (previous == store.identities_.end() && id <= allocation_base_ && !restored)) {
            throw std::invalid_argument{"Cannot reuse or retype a managed identity"};
          }
        }
        auto snapshot = detail::encode(*candidate_, store.options_.max_snapshot_bytes);
        store.check_snapshot(snapshot);
        if constexpr (has_collaboration) {
          if (store.collaboration_state_.prepare) {
            // Authority policies have the same reentry boundary as application validation.
            store.callback_active_ = true;
            try {
              store.collaboration_state_.prepare(store.collaboration_state_.prepare_context,
                                                 *candidate_, snapshot);
              store.callback_active_ = false;
            } catch (...) {
              store.callback_active_ = false;
              throw;
            }
            outcome_->throw_if_failed();
          }
        }
        if (snapshot == store.snapshot_) {
          if constexpr (Mode == history_mode::tree) {
            outcome_->revision = store.history_.current_revision;
          }
          close(transaction_status::no_change);
          return;
        }
        auto published = std::shared_ptr<const storage_type>{std::move(candidate_)};
        if constexpr (has_history) {
          if constexpr (Mode == history_mode::linear) {
            entry_type entry;
            entry.snapshot = snapshot;
            if constexpr (has_labels) {
              entry.label = this->label;
            }
            const auto plan = store.history_.prepare_append(entry, store.options_);
            store.history_.entries.push_back(std::move(entry));
            try {
              store.journal_edit(snapshot, static_cast<const label_type&>(*this), allocation_base_,
                                 plan.evict_count);
            } catch (...) {
              // The unselected tail is private while the writer is active; failure restores it exactly.
              store.history_.entries.pop_back();
              throw;
            }
            store.history_.publish_append(plan);
          } else if constexpr (Mode == history_mode::tree) {
            const auto revision = store.next_revision();
            auto node = std::make_shared<revision_type>();
            node->number = revision;
            node->parent = store.history_.current_revision;
            if constexpr (has_labels) {
              node->label = this->label;
            }
            node->snapshot = snapshot;
            const auto bytes = store.tree_append_bytes(store.history_, *node);
            const auto inserted = store.history_.revisions.emplace(revision, std::move(node)).first;
            try {
              store.journal_edit(snapshot, static_cast<const label_type&>(*this), allocation_base_,
                                 0);
            } catch (...) {
              // Only the new, unselected node is provisional; retained nodes are never copied.
              store.history_.revisions.erase(inserted);
              throw;
            }
            store.history_.bytes = bytes;
            store.history_.revision_high_water = revision;
            store.history_.current_revision = revision;
            outcome_->revision = revision;
          }
        } else {
          store.journal_edit(snapshot, static_cast<const label_type&>(*this), allocation_base_, 0);
        }
        store.current_.swap(published);
        store.snapshot_.swap(snapshot);
        store.identities_.swap(identities);
        if (store.collaboration_state_.attachment) {
          store.collaboration_state_.attachment->publish();
        }
        close(transaction_status::committed);
      } catch (...) {
        if (store_->collaboration_state_.attachment) {
          store_->collaboration_state_.attachment->abort();
        }
        fail(std::current_exception());
        throw;
      }
    }
  };

  // Create a fresh document. All generated managed IDs must initially be zero and are assigned here.
  explicit model_store(Root value, document_id document = make_document_id(),
                       store_options options = {},
                       std::function<void(const storage_type&)> validate = {})
      : document_{document}, options_{options}, validate_{std::move(validate)} {
    if ((document.high == 0 && document.low == 0) || Traits::schema_id.empty()) {
      throw std::invalid_argument{"A nonzero document namespace is required"};
    }
    auto storage = Traits::make_storage(std::move(value));
    Traits::visit_entities(storage, [&](id_type& id, std::string_view) {
      if (id != 0) {
        throw std::invalid_argument{"New managed storage must have unassigned IDs"};
      }
      id = allocate_id();
    });
    this->validate(storage);
    identities_ = inspect(storage, allocated_id_);
    snapshot_ = detail::encode(storage, options_.max_snapshot_bytes);
    check_snapshot(snapshot_);
    current_ = std::make_shared<const storage_type>(std::move(storage));
    if constexpr (has_history) {
      auto history = make_history();
      history_.swap(history);
    }
  }

  model_store(const model_store&) = delete;
  model_store& operator=(const model_store&) = delete;
  model_store(model_store&&) = delete;
  model_store& operator=(model_store&&) = delete;

  // Associate an application-owned session with this store; include managed_collaboration.hpp.
  template <typename Session = std::uint64_t,
            typename Policy = collaboration_session_traits<Session>>
  store_collaboration<model_store, Session, Policy>&
  collaborate(Session session, collaboration_client_options options = {})
    requires(has_collaboration);

  // Access the attached session's typed synchronization state without changing ownership.
  template <typename Session = std::uint64_t,
            typename Policy = collaboration_session_traits<Session>>
  store_collaboration<model_store, Session, Policy>& collaboration()
    requires(has_collaboration);

  // Exchange queued transactions and remote changes through the associated transport.
  void synchronize()
    requires(has_collaboration)
  {
    require_idle();
    if (!collaboration_state_.attachment) {
      throw std::logic_error{"Managed store has no collaboration session"};
    }
    collaboration_state_.attachment->synchronize();
  }

  // Send the next pending transaction independently of remote-change application.
  void send_pending()
    requires(has_collaboration)
  {
    require_idle();
    if (!collaboration_state_.attachment) {
      throw std::logic_error{"Managed store has no collaboration session"};
    }
    collaboration_state_.attachment->send_pending();
  }

  // Apply ordered remote changes and reconcile local intent without submitting new local work.
  void receive_changes()
    requires(has_collaboration)
  {
    require_idle();
    if (!collaboration_state_.attachment) {
      throw std::logic_error{"Managed store has no collaboration session"};
    }
    collaboration_state_.attachment->receive_changes();
  }

  // Call from the owner's event loop; network work occurs only when the interval is due.
  bool synchronize_if_due(std::uint64_t now_ms)
    requires(has_collaboration)
  {
    require_idle();
    if (!collaboration_state_.attachment) {
      throw std::logic_error{"Managed store has no collaboration session"};
    }
    return collaboration_state_.attachment->synchronize_if_due(now_ms);
  }

  // Return an immutable pin whose lifetime can extend across commits and navigation.
  std::shared_ptr<const storage_type> read() const {
    check_thread();
    return current_;
  }

  // Materialize an independent value; only the opt-in separate-values representation removes IDs.
  Root clone_value() const {
    check_thread();
    return Traits::clone_value(*current_);
  }

  // Return this document's namespace; undo preserves it and load restores the saved namespace.
  document_id document() const {
    check_thread();
    return document_;
  }

  // Read the non-reusable allocation watermark, including reservations by an active candidate.
  std::uint64_t allocated_id() const {
    check_thread();
    return allocated_id_;
  }

  // Begin an unnamed action; disabled-label stores create no label storage.
  [[nodiscard]] transaction begin_transaction(outcome_type& outcome) {
    try {
      return transaction{*this, outcome};
    } catch (...) {
      outcome = {};
      outcome.status = failure_status();
      outcome.error = std::current_exception();
      throw;
    }
  }

  // Copy an optional action description before acquiring the writer; enabled-label stores only.
  [[nodiscard]] transaction begin_transaction(std::string_view label, outcome_type& outcome)
    requires(has_labels)
  {
    try {
      return transaction{*this, outcome, {std::string{label}}};
    } catch (...) {
      outcome = {};
      outcome.status = failure_status();
      outcome.error = std::current_exception();
      throw;
    }
  }

private:
  // Share completion logic without retaining metadata in disabled-label specializations.
  template <typename Callable>
  outcome_type execute_transaction_impl(label_type label, Callable&& callback) {
    outcome_type outcome;
    try {
      auto scope = transaction{*this, outcome, std::move(label)};
      transaction_edit edit{scope};
      try {
        std::invoke(std::forward<Callable>(callback), edit);
      } catch (...) {
        scope.fail(std::current_exception());
        outcome.status = failure_status();
        outcome.error = std::current_exception();
      }
    } catch (...) {
      outcome = {};
      outcome.status = failure_status();
      outcome.error = std::current_exception();
    }
    return outcome;
  }

public:
  // Execute an unnamed action exactly once and return after completion.
  template <typename Callable>
    requires std::same_as<std::invoke_result_t<Callable, transaction_edit&>, void>
  [[nodiscard]] outcome_type execute_transaction(Callable&& callback) {
    return execute_transaction_impl({}, std::forward<Callable>(callback));
  }

  // Execute a named action when labels are enabled; even label allocation failures are recorded.
  template <typename Callable>
    requires(has_labels && std::same_as<std::invoke_result_t<Callable, transaction_edit&>, void>)
  [[nodiscard]] outcome_type execute_transaction(std::string_view label, Callable&& callback) {
    try {
      return execute_transaction_impl({std::string{label}}, std::forward<Callable>(callback));
    } catch (...) {
      outcome_type outcome;
      outcome.status = failure_status();
      outcome.error = std::current_exception();
      return outcome;
    }
  }

  // Copy the description of the action undo would reverse; reject an unavailable undo state.
  std::string undo_label() const
    requires(has_labels && has_history)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      return collaboration_state_.attachment->history_label(false);
    }
    if constexpr (Mode == history_mode::linear) {
      if (history_.cursor != 0) {
        return history_.entries[history_.cursor].label;
      }
    } else {
      const auto& node = history_.revisions.at(history_.current_revision);
      if (node->parent != 0) {
        return node->label;
      }
    }
    throw std::out_of_range{"No managed undo label"};
  }

  // Copy the next linear action's description; an unnamed available action has an empty label.
  std::string redo_label() const
    requires(has_labels && Mode == history_mode::linear)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      return collaboration_state_.attachment->history_label(true);
    }
    const auto next = history_.cursor + 1;
    if (next >= history_.entries.size()) {
      throw std::out_of_range{"No managed redo label"};
    }
    return history_.entries[next].label;
  }

  // Copy a direct tree child's description without choosing a branch implicitly.
  std::string redo_label(std::uint64_t revision) const
    requires(has_labels && Mode == history_mode::tree)
  {
    require_idle();
    const auto found = history_.revisions.find(revision);
    if (found == history_.revisions.end() || found->second->parent != history_.current_revision) {
      throw std::out_of_range{"Revision is not a redo child"};
    }
    return found->second->label;
  }

  // Return direct redo alternatives in deterministic revision order for tree history only.
  std::vector<std::uint64_t> redo_children() const
    requires(Mode == history_mode::tree)
  {
    require_idle();
    std::vector<std::uint64_t> result;
    for (const auto& [id, node] : history_.revisions) {
      if (node->parent == history_.current_revision) {
        result.push_back(id);
      }
    }
    return result;
  }

  // Restore a retained tree revision after decoding and application validation.
  void checkout(std::uint64_t revision)
    requires(Mode == history_mode::tree)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      throw std::logic_error{"Shared history uses collaborative undo/redo, not branch checkout"};
    }
    const auto found = history_.revisions.find(revision);
    if (found == history_.revisions.end()) {
      throw std::out_of_range{"Managed revision is unavailable"};
    }
    restore_snapshot(found->second->snapshot, revision);
    history_.current_revision = revision;
  }

  // Undo follows the sole parent; the baseline has no predecessor.
  void undo()
    requires(has_history)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      collaboration_state_.attachment->undo(false);
      return;
    }
    if constexpr (Mode == history_mode::linear) {
      if (history_.cursor != 0) {
        checkout_linear(history_.cursor - 1);
        return;
      }
    } else if constexpr (Mode == history_mode::tree) {
      const auto found = history_.revisions.find(history_.current_revision);
      if (found != history_.revisions.end() && found->second->parent != 0) {
        checkout(found->second->parent);
        return;
      }
    }
    throw std::out_of_range{"No managed undo revision"};
  }

  // Restore the next linear entry; there is exactly one possible redo state.
  void redo()
    requires(Mode == history_mode::linear)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      collaboration_state_.attachment->undo(true);
      return;
    }
    const auto next = history_.cursor + 1;
    if (next >= history_.entries.size()) {
      throw std::out_of_range{"No managed redo state"};
    }
    checkout_linear(next);
  }

  // Select a direct tree child explicitly rather than choosing a branch implicitly.
  void redo(std::uint64_t revision)
    requires(Mode == history_mode::tree)
  {
    require_idle();
    const auto found = history_.revisions.find(revision);
    if (found == history_.revisions.end() || found->second->parent != history_.current_revision) {
      throw std::out_of_range{"Revision is not a redo child"};
    }
    checkout(revision);
  }

  // Discard retained history and start a new baseline; the template-selected mode cannot change.
  void reset_history()
    requires(has_history)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      throw std::logic_error{"Collaboration history is retained with pending synchronization"};
    }
    auto history = make_history();
    journal_control(detail::journal_record_kind::reset);
    history_.swap(history);
  }

  // Export a coherent memory envelope; this does not mark the journaled document saved.
  std::vector<std::uint8_t> save() const {
    require_idle();
    if (collaboration_state_.attachment) {
      return collaboration_state_.attachment->save();
    }
    return encode_state(snapshot_, history_, allocated_id_);
  }

  // Create a durable saved baseline at a new path and journal all subsequent persistent operations.
  void create_journal(const std::filesystem::path& path,
                      journal_storage_mode mode = journal_storage_mode::appended,
                      journal_options options = {})
    requires(has_journal)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      collaboration_state_.attachment->create_journal(path, mode, std::move(options));
      return;
    }
    if (journal_state_.sink) {
      throw std::logic_error{"Managed store already owns a journal"};
    }
    auto saved = snapshot_;
    auto bytes = save();
    check_journal_envelope(bytes);
    journal_operation([&] {
      journal_state_.sink = detail::create_file_journal(path, mode, bytes, std::move(options),
                                                        journal_state_.indeterminate);
    });
    journal_state_.saved_snapshot.swap(saved);
  }

  // Recover into an unattached store atomically, then acquire responsibility for future durable writes.
  // Destroy/recreate a fenced store before calling this; recovery never reruns editing callbacks.
  void recover_journal(const std::filesystem::path& path, journal_options options = {})
    requires(has_journal)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      collaboration_state_.attachment->recover_journal(path, std::move(options));
      return;
    }
    if (journal_state_.sink) {
      throw std::logic_error{"Recover into a store without an attached journal"};
    }
    // Intermediate replay validates schema invariants; application validation runs once on the result.
    model_store recovered{recovery_tag{}, options_, {}};
    std::vector<std::uint8_t> saved;
    journal_operation([&] {
      auto journal = detail::recover_file_journal(
          path, std::move(options), [&](bool base, std::span<const std::uint8_t> bytes) {
            if (base) {
              recovered.load({bytes.begin(), bytes.end()});
              saved = recovered.snapshot_;
            } else {
              recovered.replay_journal_record(bytes);
            }
          });
      recovered.validate_ = validate_;
      recovered.validate(*recovered.current_);
      if (document_.high == recovered.document_.high && document_.low == recovered.document_.low) {
        if (allocated_id_ > recovered.allocated_id_ ||
            current_->persistent_id != recovered.current_->persistent_id) {
          throw std::invalid_argument{
              "Recovery would discard live document identity; use a fresh store"};
        }
        if constexpr (Mode == history_mode::tree) {
          if (history_.revision_high_water > recovered.history_.revision_high_water) {
            throw std::invalid_argument{"Recovery would discard live revision allocation"};
          }
        }
      }
      try {
        journal->finish_recovery();
      } catch (...) {
        journal_state_.indeterminate = journal->needs_recovery();
        throw;
      }
      // Nothing below allocates or calls application code after the recovered state becomes durable.
      if constexpr (has_history) {
        history_.swap(recovered.history_);
      }
      current_.swap(recovered.current_);
      snapshot_.swap(recovered.snapshot_);
      identities_.swap(recovered.identities_);
      journal_state_.saved_snapshot.swap(saved);
      document_ = recovered.document_;
      allocated_id_ = recovered.allocated_id_;
      journal_state_.sink.swap(journal);
    });
  }

  // Flush and atomically replace the base with all current history and allocator dependencies.
  // The synchronous, thread-confined writer cannot admit edits during this Save.
  void save_journal()
    requires(has_journal)
  {
    require_idle();
    if (!journal_state_.sink) {
      throw std::logic_error{"Managed store has no journal"};
    }
    auto saved = snapshot_;
    auto bytes = save();
    if (!collaboration_state_.attachment) {
      check_journal_envelope(bytes);
    }
    journal_operation([&] { journal_state_.sink->save(bytes); });
    journal_state_.saved_snapshot.swap(saved);
  }

  // Compare current persistent values/identities with the last full Save, independently of history.
  bool journal_dirty() const
    requires(has_journal)
  {
    check_thread();
    if (!journal_state_.sink) {
      throw std::logic_error{"Managed store has no journal"};
    }
    return snapshot_ != journal_state_.saved_snapshot;
  }

  // Return acknowledged durable operation identity; reservations and history navigation also advance it.
  std::uint64_t journal_sequence() const
    requires(has_journal)
  {
    check_thread();
    if (!journal_state_.sink) {
      throw std::logic_error{"Managed store has no journal"};
    }
    return journal_state_.sink->sequence();
  }

  // Distinguish an uncertain disk decision from an ordinary validation or preparation failure.
  bool journal_needs_recovery() const
    requires(has_journal)
  {
    check_thread();
    return journal_state_.indeterminate ||
           (journal_state_.sink && journal_state_.sink->needs_recovery());
  }

  // Validate the entire history before replacing this document; reject malformed or incompatible data.
  void load(const std::vector<std::uint8_t>& bytes) {
    require_idle();
    if (collaboration_state_.attachment) {
      collaboration_state_.attachment->load(bytes);
      return;
    }
    if (journal_state_.sink) {
      throw std::logic_error{"Cannot replace a journaled document with a memory envelope"};
    }
    auto envelope = detail::decode<envelope_type>(bytes, options_.decode);
    if (envelope.format_version != format_version || envelope.schema_id != Traits::schema_id ||
        envelope.id_bits != std::numeric_limits<id_type>::digits ||
        (envelope.document_high == 0 && envelope.document_low == 0) ||
        envelope.allocated_id > std::numeric_limits<id_type>::max() ||
        envelope.mode != static_cast<std::uint32_t>(Mode)) {
      throw std::invalid_argument{"Incompatible managed envelope"};
    }
    if constexpr (Mode == history_mode::tree) {
      if (envelope.current_revision == 0 ||
          envelope.current_revision > envelope.revision_high_water) {
        throw std::invalid_argument{"Invalid managed tree cursor"};
      }
    } else if constexpr (Mode == history_mode::linear) {
      if (envelope.cursor >= envelope.entries.size() ||
          envelope.entries[static_cast<std::size_t>(envelope.cursor)].snapshot !=
              envelope.current_snapshot) {
        throw std::invalid_argument{"Invalid managed linear cursor"};
      }
    } else if (!envelope.entries.empty() || envelope.cursor != 0) {
      throw std::invalid_argument{"History is unsupported by this store"};
    }
    const auto& entries = [&]() -> const auto& {
      if constexpr (Mode == history_mode::tree) {
        return envelope.revisions;
      } else {
        return envelope.entries;
      }
    }();
    if (entries.size() > options_.max_revisions) {
      throw std::length_error{"Managed history entry budget exceeded"};
    }
    std::size_t remaining_history_bytes = options_.max_history_bytes;
    for (const auto& node : entries) {
      check_snapshot(node.snapshot);
      for (const auto size : {node.snapshot.size(), detail::label_bytes<Labels>(node)}) {
        if (size > remaining_history_bytes) {
          throw std::length_error{"Managed history byte budget exceeded"};
        }
        remaining_history_bytes -= size;
      }
    }
    check_snapshot(envelope.current_snapshot);
    auto value = detail::decode<storage_type>(envelope.current_snapshot, options_.decode);
    auto identities = inspect(value, envelope.allocated_id);
    const bool same_document =
        document_.high == envelope.document_high && document_.low == envelope.document_low;
    if (same_document && value.persistent_id != current_->persistent_id) {
      throw std::invalid_argument{"Cannot replace the identity of an open document root"};
    }
    if (same_document) {
      for (const auto& [id, type] : identities) {
        const auto old = identities_.find(id);
        if (old != identities_.end() && old->second != type) {
          throw std::invalid_argument{"Reload changes an existing entity's type"};
        }
      }
    }
    validate(value);
    auto published = std::make_shared<const storage_type>(std::move(value));
    if constexpr (has_history) {
      detail::history_storage<Mode, Labels> history;
      auto historical_identities = identities;
      // Both storage policies validate every historical model before replacing the live state.
      const auto validate_entry = [&](const auto& node) {
        auto historical = detail::decode<storage_type>(node.snapshot, options_.decode);
        validate(historical, false);
        const auto node_identities = inspect(historical, envelope.allocated_id);
        for (const auto& [id, type] : node_identities) {
          const auto [entry, inserted] = historical_identities.emplace(id, type);
          if (!inserted && entry->second != type) {
            throw std::invalid_argument{"Historical entity identity changes type"};
          }
        }
        if (historical.persistent_id != published->persistent_id) {
          throw std::invalid_argument{"History changes the root identity"};
        }
      };
      if constexpr (Mode == history_mode::linear) {
        for (auto& node : envelope.entries) {
          validate_entry(node);
          entry_type entry;
          entry.snapshot = std::move(node.snapshot);
          if constexpr (has_labels) {
            entry.label = std::move(node.label);
          }
          history.entries.push_back(std::move(entry));
        }
        history.cursor = static_cast<std::size_t>(envelope.cursor);
        history.bytes = options_.max_history_bytes - remaining_history_bytes;
      } else {
        // Tree wire records may arrive in any order; parents precede children after sorting.
        std::sort(envelope.revisions.begin(), envelope.revisions.end(),
                  [](const auto& left, const auto& right) { return left.number < right.number; });
        std::size_t roots = 0;
        bool found_current = false;
        for (auto& node : envelope.revisions) {
          if (node.number == 0 || node.number > envelope.revision_high_water ||
              node.parent >= node.number || history.revisions.contains(node.number) ||
              (node.parent != 0 && !history.revisions.contains(node.parent))) {
            throw std::invalid_argument{"Invalid managed revision graph"};
          }
          roots += node.parent == 0 ? 1 : 0;
          if (node.number == envelope.current_revision) {
            if (node.snapshot != envelope.current_snapshot) {
              throw std::invalid_argument{"Invalid managed history cursor"};
            }
            found_current = true;
          }
          validate_entry(node);
          auto entry = std::make_shared<const revision_type>(std::move(node));
          history.revisions.emplace(entry->number, std::move(entry));
        }
        if (roots != 1 || !found_current) {
          throw std::invalid_argument{"Invalid managed history cursor"};
        }
        history.bytes = options_.max_history_bytes - remaining_history_bytes;
        history.current_revision = envelope.current_revision;
        history.revision_high_water =
            same_document ? std::max(history_.revision_high_water, envelope.revision_high_water)
                          : envelope.revision_high_water;
      }
      history_.swap(history);
    }
    current_.swap(published);
    identities_.swap(identities);
    snapshot_.swap(envelope.current_snapshot);
    document_ = {envelope.document_high, envelope.document_low};
    allocated_id_ =
        same_document ? std::max(allocated_id_, envelope.allocated_id) : envelope.allocated_id;
  }
};

template <typename Root, history_mode Mode = history_mode::linear,
          history_labels Labels = history_labels::disabled, typename Traits = model_traits<Root>,
          store_features Features = store_features::all>
using managed = model_store<Root, Mode, Labels, Traits, Features>;

#undef SERIALIZER_MANAGED_EMPTY_MEMBER

} // namespace rohit::managed
