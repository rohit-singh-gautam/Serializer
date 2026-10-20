// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once
#include <rohit/managed_memory.hpp>
#include <rohit/managed_changes.hpp>
#include <rohit/managed_resident.hpp>
#include <rohit/managed_registry.hpp>

#include <rohit/managed_collaboration_hooks.hpp>
#include <rohit/managed_editor.hpp>
#include <rohit/managed_editable.hpp>
#include <rohit/managed_delta.hpp>
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
// Detached copies preserve memory-only values unless the caller explicitly requests defaults.
enum class runtime_copy_policy { preserve, reset_to_defaults };
struct clone_options {
  runtime_copy_policy runtime_fields{runtime_copy_policy::preserve};
};
// A view stamp advances on every publication; durable generation excludes runtime-only updates.
struct state_stamp {
  std::uint64_t view_generation{};
  std::uint64_t durable_generation{};
  bool operator==(const state_stamp&) const = default;
};
enum class publication_kind { durable_edit, history_navigation, load, runtime_update };
// Callbacks receive pinned accepted values and conservatively invalidated entity identities.
template <typename Storage, typename Id>
struct publication_event {
  publication_kind kind{};
  state_stamp stamp{};
  std::shared_ptr<const Storage> before{};
  std::shared_ptr<const Storage> after{};
  std::vector<Id> invalidated_ids{};
  // Durable field/membership changes and entity births/deletions, independently of cache resets.
  std::vector<Id> changed_ids{};
  // Handwritten legacy traits lacking generated field traversal expose a conservative fallback.
  bool changed_ids_precise{true};
};

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
  // Admission estimates include owning models, retained snapshots/history and outstanding pins.
  // This is not a hard limit on temporary decoder/container allocations inside an edit callback.
  std::size_t max_resident_bytes{std::numeric_limits<std::size_t>::max()};
  // Scoped root/control-block allocation hook; generated standard containers keep their allocators.
  std::shared_ptr<std::pmr::memory_resource> allocation_resource{};
  // Opt-in deltas preserve default full-state frames; full states bound replay depth.
  bool delta_journal{};
  std::size_t max_delta_chain{32};
};

// Specialize for an ordinary generated payload and its explicit generated ID-bearing storage.
template <typename Root>
struct model_traits;

namespace detail {
struct migration_store_access;
// Generated hooks reset memory-only members without changing persistent values or identities.
template <typename Traits, typename Storage>
void reset_runtime_fields(Storage& value) {
  if constexpr (requires { Traits::reset_runtime_fields(value); }) {
    Traits::reset_runtime_fields(value);
  }
}
// Reattach memory-only values to an isolated candidate; older traits need no additional hooks.
template <typename Traits, typename Storage>
void copy_runtime_fields(Storage& value, const Storage& source) {
  if constexpr (requires { Traits::copy_runtime_fields(value, source); }) {
    Traits::copy_runtime_fields(value, source);
  }
}

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
  std::size_t max_resident_bytes;
  std::shared_ptr<std::pmr::memory_resource> allocation_resource;
  bool delta_journal{};
  std::size_t max_delta_chain{32};
  // History limits have no storage or runtime role in a history-free specialization.
  explicit store_configuration(store_options options)
      : max_snapshot_bytes{options.max_snapshot_bytes}, decode{options.decode},
        max_resident_bytes{options.max_resident_bytes},
        allocation_resource{std::move(options.allocation_resource)}, delta_journal{options.delta_journal}, max_delta_chain{options.max_delta_chain} {}
  // Reconstruct options only when building an isolated recovery or synchronization receiver.
  operator store_options() const {
    store_options result;
    result.max_revisions = max_revisions;
    result.max_history_bytes = max_history_bytes;
    result.max_snapshot_bytes = max_snapshot_bytes;
    result.decode = decode;
    result.max_resident_bytes = max_resident_bytes;
    result.allocation_resource = allocation_resource;
    result.delta_journal = delta_journal;
    result.max_delta_chain = max_delta_chain;
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
  std::size_t delta_count{};
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
  static constexpr identity_lease_book* identity_leases = nullptr;
  static constexpr const records::identity_lease* prepared_identity_lease = nullptr;
};
template <typename Storage, typename IdentityTable, bool History>
struct store_collaboration_storage<true, Storage, IdentityTable, History>
    : store_restoration_storage<true, IdentityTable> {
  std::unique_ptr<collaboration_attachment> attachment{};
  bool busy{};
  std::unique_ptr<identity_lease_book> identity_leases{};
  const records::identity_lease* prepared_identity_lease{};
  void* prepare_context{};
  void (*prepare)(void*, const Storage&, const std::vector<std::uint8_t>&){};
};

// Isolated allocation storage is compiled in only for private preview traits that explicitly request it.
template <bool Enabled>
struct isolated_allocation_storage {};
template <>
struct isolated_allocation_storage<true> {
  identity_allocation_context context{};
  bool initialized{};
};

// Only isolated workspaces retain a source reservation callback; ordinary stores keep empty storage.
template <bool Enabled, typename Id>
struct isolated_reservation_storage {};

template <typename Id>
struct isolated_reservation_storage<true, Id> {
  std::function<std::pair<Id, identity_allocation_context>()> reserve{};
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
  template <typename, history_mode, history_labels, typename, store_features>
  friend class model_store;
  template <typename, typename, typename>
  friend class store_collaboration;
  template <typename, history_mode, history_labels, typename, typename, typename, store_features>
  friend class collaboration_authority;
  friend struct detail::migration_store_access;

public:
  static constexpr bool has_journal =
      Features == store_features::journal || Features == store_features::all;
  static constexpr bool has_collaboration =
      Features == store_features::collaboration || Features == store_features::all;
  static constexpr bool has_history = Mode != history_mode::disabled;
  static constexpr bool has_isolated_allocator = [] {
    if constexpr (requires { Traits::isolated_allocation_context; }) {
      return static_cast<bool>(Traits::isolated_allocation_context);
    } else {
      return false;
    }
  }();
  static_assert(Features == store_features::none || Features == store_features::journal ||
                Features == store_features::collaboration || Features == store_features::all);
  using traits_type = Traits;
  using outcome_type =
      std::conditional_t<Mode == history_mode::tree, tree_transaction_outcome, transaction_outcome>;
  using storage_type = typename Traits::storage_type;
  using id_type = typename Traits::id_type;
  using identity_reservation_callback = std::function<std::pair<id_type, identity_allocation_context>()>;
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
  std::shared_ptr<detail::resident_ledger> resident_ledger_{
      std::make_shared<detail::resident_ledger>(options_.max_resident_bytes)};
  detail::resident_charge resident_auxiliary_{resident_ledger_};
  std::shared_ptr<const storage_type> current_{};
  std::vector<std::uint8_t> snapshot_{};
  identity_table identities_{};
  SERIALIZER_MANAGED_EMPTY_MEMBER
  detail::store_collaboration_storage<has_collaboration, storage_type, identity_table, has_history>
      collaboration_state_{};
  SERIALIZER_MANAGED_EMPTY_MEMBER
  detail::isolated_allocation_storage<has_isolated_allocator> isolated_allocation_{};
  SERIALIZER_MANAGED_EMPTY_MEMBER
  detail::isolated_reservation_storage<has_isolated_allocator, id_type> isolated_reservation_{};
  std::uint64_t allocated_id_{};
  bool writer_active_{};
  bool callback_active_{};
  state_stamp stamp_{};
  using event_type = publication_event<storage_type, id_type>;
  std::function<void(const event_type&)> publication_callback_{};
  std::exception_ptr notification_error_{};
  const std::thread::id thread_{std::this_thread::get_id()};
  std::function<void(const storage_type&)> validate_{};
  // Opt-in index invariants are schema-state checks, distinct from host authorization policy.
  std::shared_ptr<const std::function<void(const storage_type&)>> registry_validate_{};
  SERIALIZER_MANAGED_EMPTY_MEMBER detail::history_storage<Mode, Labels> history_{};
  SERIALIZER_MANAGED_EMPTY_MEMBER detail::store_journal_storage<has_journal> journal_state_{};

  struct recovery_tag {};

  // Build a private empty receiver so recovery can validate everything before live publication.
  model_store(recovery_tag, store_options options,
              std::function<void(const storage_type&)> validate,
               std::shared_ptr<detail::resident_ledger> ledger = {})
      : options_{options},
        resident_ledger_{ledger ? std::move(ledger)
                                : std::make_shared<detail::resident_ledger>(options.max_resident_bytes)},
        resident_auxiliary_{resident_ledger_}, validate_{std::move(validate)} {}

  // Require a complete generated estimate when opting into a finite resident admission budget.
  std::size_t estimate_storage(const storage_type& value) const {
    std::size_t result = sizeof(storage_type);
    if constexpr (requires { Traits::estimate_memory(value); }) {
      result = Traits::estimate_memory(value);
      if (result < sizeof(storage_type)) {
        throw std::invalid_argument{"Managed estimate omits inline storage"};
      }
    } else if (options_.max_resident_bytes != std::numeric_limits<std::size_t>::max()) {
      throw std::logic_error{"Finite resident budgets require model memory traits"};
    }
    // Qualified libraries use a small shared-control block; conservatively include its overhead.
    detail::add_memory_estimate(result, sizeof(detail::resident_storage<storage_type>) -
                                           sizeof(storage_type) + 8 * sizeof(void*));
    return result;
  }

  // Shared holders keep the conservative charge alive as long as any immutable reader pin.
  auto make_storage_candidate(storage_type value) const {
    const auto estimate = estimate_storage(value);
    return detail::make_resident_storage(std::move(value), resident_ledger_,
                                         options_.allocation_resource, estimate);
  }
  // Read-only roots use the same resource/lifetime accounting as mutable private candidates.
  std::shared_ptr<const storage_type> make_published_storage(storage_type value) const {
    return make_storage_candidate(std::move(value)).first;
  }

  // Account retained capacities and metadata conservatively, independently of wire-size limits.
  std::size_t estimate_auxiliary(const std::vector<std::uint8_t>& snapshot,
                                const identity_table& identities, const history_type& history,
                                const std::vector<std::uint8_t>* saved = nullptr) const {
    std::size_t result = snapshot.capacity();
    detail::add_memory_estimate(result, detail::estimate_dynamic_memory(identities));
    if constexpr (Mode == history_mode::linear) {
      // A conservative block/map allowance accompanies every retained deque entry.
      detail::add_memory_estimate(result, detail::multiply_memory_estimate(
          history.entries.size(), sizeof(entry_type) + 512 + 2 * sizeof(void*)));
      for (const auto& entry : history.entries) {
        detail::add_memory_estimate(result, entry.snapshot.capacity());
        if constexpr (has_labels) {
          detail::add_memory_estimate(result, detail::estimate_dynamic_memory(entry.label));
        }
      }
    } else if constexpr (Mode == history_mode::tree) {
      detail::add_memory_estimate(result, detail::multiply_memory_estimate(
          history.revisions.size(), sizeof(typename decltype(history.revisions)::value_type) +
                                        sizeof(revision_type) + 14 * sizeof(void*)));
      for (const auto& [number, entry] : history.revisions) {
        detail::add_memory_estimate(result, entry->snapshot.capacity());
        if constexpr (has_labels) {
          detail::add_memory_estimate(result, detail::estimate_dynamic_memory(entry->label));
        }
      }
    }
    if constexpr (has_journal) {
      detail::add_memory_estimate(result, saved ? saved->capacity()
                                                : journal_state_.saved_snapshot.capacity());
    }
    return result;
  }

  // Stage a superset of the next history before journal I/O; eviction can only reduce this estimate.
  std::size_t estimate_edit_auxiliary(const std::vector<std::uint8_t>& snapshot,
                                     const identity_table& identities, const label_type& action) const {
    auto result = estimate_auxiliary(snapshot, identities, history_);
    if constexpr (Mode == history_mode::linear) {
      detail::add_memory_estimate(result, sizeof(entry_type) + 512 + 2 * sizeof(void*));
      detail::add_memory_estimate(result, snapshot.size());
    } else if constexpr (Mode == history_mode::tree) {
      detail::add_memory_estimate(result, sizeof(typename decltype(history_.revisions)::value_type) +
                                             sizeof(revision_type) + 14 * sizeof(void*));
      detail::add_memory_estimate(result, snapshot.size());
    }
    if constexpr (has_labels) {
      // Copies may retain capacity; admit the caller's capacity rather than only serialized length.
      detail::add_memory_estimate(result, detail::estimate_dynamic_memory(action.label));
    }
    return result;
  }

  // Prepare every notification allocation and generation overflow check before durable publication.
  event_type prepare_publication(publication_kind kind, const identity_table& identities,
                                 const storage_type* next = nullptr,
                                 bool replaced_namespace = false) const {
    if (stamp_.view_generation == std::numeric_limits<std::uint64_t>::max() ||
        (kind != publication_kind::runtime_update &&
         stamp_.durable_generation == std::numeric_limits<std::uint64_t>::max())) {
      throw std::overflow_error{"Managed publication generation exhausted"};
    }
    event_type event;
    event.kind = kind;
    event.before = current_;
    if (kind != publication_kind::runtime_update && publication_callback_) {
      event.invalidated_ids.reserve(identities_.size() + identities.size());
      for (const auto& [id, type] : identities_) {
        static_cast<void>(type);
        event.invalidated_ids.push_back(id);
      }
      for (const auto& [id, type] : identities) {
        static_cast<void>(type);
        if (!identities_.contains(id)) {
          event.invalidated_ids.push_back(id);
        }
      }
      std::sort(event.invalidated_ids.begin(), event.invalidated_ids.end());
      if constexpr (requires(const storage_type& value, detail::publication_field_collector& visitor) {
                      Traits::visit_collaboration_fields(value, visitor);
                    }) {
        if (next) {
          event.changed_ids = detail::changed_entity_ids<Traits>(
              current_.get(), *next, identities_, identities, options_.max_snapshot_bytes,
              replaced_namespace);
        } else {
          event.changed_ids = event.invalidated_ids;
          event.changed_ids_precise = false;
        }
      } else {
        event.changed_ids = event.invalidated_ids;
        event.changed_ids_precise = false;
      }
    }
    return event;
  }

  // Notify only accepted state; observer failures never turn an acknowledged commit into a failure.
  void publish_notification(event_type event) noexcept {
    ++stamp_.view_generation;
    if (event.kind != publication_kind::runtime_update) {
      ++stamp_.durable_generation;
    }
    event.stamp = stamp_;
    event.after = current_;
    if (publication_callback_) {
      callback_active_ = true;
      try {
        publication_callback_(event);
      } catch (...) {
        notification_error_ = std::current_exception();
      }
      callback_active_ = false;
    }
  }

  // Preserve the uncertainty of issued durable writes in all transaction completion forms.
  transaction_status failure_status() const noexcept {
    if constexpr (has_journal) {
      return journal_state_.indeterminate ||
                     (journal_state_.sink && journal_state_.sink->needs_recovery())
                 ? transaction_status::indeterminate
                 : transaction_status::failed;
    } else {
      return transaction_status::failed;
    }
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
    auto bytes = detail::encode(envelope, options_.decode.max_input_bytes);
    if constexpr (has_collaboration) {
      if (collaboration_state_.identity_leases &&
          !collaboration_state_.identity_leases->entries().empty()) {
        records::identity_allocation_checkpoint checkpoint;
        checkpoint.format_version = 1;
        checkpoint.model = std::move(bytes);
        checkpoint.leases = collaboration_state_.identity_leases->entries();
        if (options_.decode.max_input_bytes <= allocation_checkpoint_magic.size()) {
          throw std::length_error{"Native identity checkpoint exceeds its byte budget"};
        }
        bytes = detail::encode(checkpoint,
            options_.decode.max_input_bytes - allocation_checkpoint_magic.size());
        bytes.insert(bytes.begin(), allocation_checkpoint_magic.begin(), allocation_checkpoint_magic.end());
      }
    }
    return bytes;
  }

  // Build the data-only dictionary for an external document; IDs stay in managed sidecar metadata.
  std::vector<std::uint8_t> journal_document_snapshot() const {
    // Decode durable bytes instead of copying potentially large memory-only caches.
    auto value = detail::decode<storage_type>(snapshot_, options_.decode);
    Traits::visit_entities(value, [](id_type& id, std::string_view) { id = 0; });
    detail::reset_runtime_fields<Traits>(value);
    return detail::encode(value, options_.max_snapshot_bytes);
  }

  static constexpr auto allocation_checkpoint_magic = detail::identity_allocation_checkpoint_magic;

  // Unwrap optional authority allocation metadata while preserving the ordinary envelope encoding.
  records::identity_allocation_checkpoint decode_allocation_checkpoint(
      const std::vector<std::uint8_t>& bytes) const {
    records::identity_allocation_checkpoint checkpoint;
    if (bytes.size() >= allocation_checkpoint_magic.size() &&
        std::equal(allocation_checkpoint_magic.begin(), allocation_checkpoint_magic.end(), bytes.begin())) {
      if constexpr (!has_collaboration) {
        throw std::invalid_argument{"Identity lease checkpoints require collaboration storage"};
      }
      checkpoint = detail::decode<records::identity_allocation_checkpoint>(
          {bytes.begin() + allocation_checkpoint_magic.size(), bytes.end()}, options_.decode);
      if (checkpoint.format_version != 1 || checkpoint.leases.empty()) {
        throw std::invalid_argument{"Invalid native identity allocation checkpoint"};
      }
    }
    return checkpoint;
  }

  // Check that this writer's envelope fits the same decoder budgets used by recovery.
  void check_journal_envelope(const std::vector<std::uint8_t>& bytes) const {
    const auto checkpoint = decode_allocation_checkpoint(bytes);
    static_cast<void>(detail::decode<envelope_type>(
        checkpoint.format_version == 0 ? bytes : checkpoint.model, options_.decode));
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
                    std::uint64_t allocation_base, std::size_t evicted,
                    bool admit_reserved_identities = false) {
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
      constexpr auto ordinary_prefix_bytes =
          1 +
          journal_word_bytes * (1 + (Mode == history_mode::linear ? 1 : 0) + (has_labels ? 1 : 0));
      constexpr auto lease_prefix_bytes = 1 + 2 * journal_word_bytes;
      std::array<std::uint8_t, ordinary_prefix_bytes + (has_collaboration ? lease_prefix_bytes : 0)> prefix{};
      const auto* lease = collaboration_state_.prepared_identity_lease;
      const bool restoring = collaboration_state_.restoration != nullptr ||
                             collaboration_state_.attachment != nullptr || admit_reserved_identities;
      std::vector<std::uint8_t> delta;
      if (options_.delta_journal && options_.max_delta_chain != 0 &&
          journal_state_.delta_count < options_.max_delta_chain && !collaboration_state_.attachment && !lease) {
        auto patch = make_snapshot_patch(snapshot_, snapshot, options_.max_snapshot_bytes);
        try {
          delta = detail::encode(patch, options_.max_snapshot_bytes);
        } catch (const std::length_error&) {
          // A full snapshot remains valid even when an unhelpful patch exceeds its encoding budget.
          delta.clear();
        }
        if (delta.size() >= snapshot.size()) {
          delta.clear();
        }
      }
      std::size_t start{};
      if (lease) {
        prefix.front() = static_cast<std::uint8_t>(detail::journal_record_kind::leased_edit);
        detail::journal_put_word(prefix, 1, lease->first);
        detail::journal_put_word(prefix, 1 + journal_word_bytes, lease->committed);
        start = lease_prefix_bytes;
      }
      prefix[start] = static_cast<std::uint8_t>(
          delta.empty() ? (restoring ? detail::journal_record_kind::restore_edit
                                    : detail::journal_record_kind::edit)
                        : (restoring ? detail::journal_record_kind::delta_restore_edit
                                     : detail::journal_record_kind::delta_edit));
      std::size_t offset = start + 1;
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
      journal_operation([&] {
        journal_state_.sink->append(std::span{prefix}.first(start + ordinary_prefix_bytes), label,
                                    delta.empty() ? std::span{snapshot} : std::span{delta});
      });
      journal_state_.delta_count = delta.empty() ? 0 : journal_state_.delta_count + 1;
    }
  }

  // Replay stored bytes into an isolated receiver. Never invoke editing callbacks or external effects.
  // The base supplies schema/profile/policy; the file adapter checks sequence and checksum chaining.
  void replay_journal_record(std::span<const std::uint8_t> record,
                               const records::identity_lease* lease_admission = nullptr) {
    if (record.empty()) {
      throw std::invalid_argument{"Empty managed journal operation"};
    }
    std::size_t offset = 1;
    const auto kind = static_cast<detail::journal_record_kind>(record.front());
    if (kind == detail::journal_record_kind::leased_edit) {
      if constexpr (has_collaboration) {
        const auto first = detail::journal_get_word(record, offset);
        const auto committed = detail::journal_get_word(record, offset);
        auto* lease = collaboration_state_.identity_leases
            ? collaboration_state_.identity_leases->find(first) : nullptr;
        if (!lease || committed <= lease->committed || committed > lease->last ||
            offset == record.size() ||
            record[offset] != static_cast<std::uint8_t>(detail::journal_record_kind::restore_edit)) {
          throw std::invalid_argument{"Invalid native leased identity journal edit"};
        }
        auto admission = *lease;
        admission.committed = committed;
        replay_journal_record(record.subspan(offset), &admission);
        lease->committed = committed;
        return;
      } else {
        throw std::invalid_argument{"Leased identity journal requires collaboration storage"};
      }
    }
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
    if (kind == detail::journal_record_kind::reserve_range) {
      const auto upper = detail::journal_get_word(record, offset);
      if (offset != record.size() || upper <= allocated_id_ ||
          upper > std::numeric_limits<id_type>::max()) {
        throw std::invalid_argument{"Invalid native journal range reservation"};
      }
      allocated_id_ = upper;
      return;
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
    const bool delta = kind == detail::journal_record_kind::delta_edit ||
                       kind == detail::journal_record_kind::delta_restore_edit;
    const bool restoring = kind == detail::journal_record_kind::restore_edit ||
                           kind == detail::journal_record_kind::delta_restore_edit;
    if (kind != detail::journal_record_kind::edit && !restoring && !delta) {
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
    if (delta) {
      if constexpr (has_journal) {
        if (journal_state_.delta_count >= options_.max_delta_chain) {
          throw std::length_error{"Managed journal delta chain budget exceeded"};
        }
      }
      auto patch = detail::decode<records::snapshot_patch>(snapshot, options_.decode);
      snapshot = apply_snapshot_patch(snapshot_, patch, options_.max_snapshot_bytes);
    }
    auto value = detail::decode<storage_type>(snapshot, options_.decode);
    detail::reset_runtime_fields<Traits>(value);
    auto identities = inspect(value, allocated_id_);
    if (value.persistent_id != current_->persistent_id || snapshot == snapshot_) {
      throw std::invalid_argument{"Journal edit changes root identity or records no change"};
    }
    for (const auto& [id, type] : identities) {
      const auto old = identities_.find(id);
      bool admitted = restoring;
      if (lease_admission && old == identities_.end()) {
        if constexpr (has_collaboration) {
          const auto* previous = collaboration_state_.identity_leases->find(lease_admission->first);
          admitted = previous && id > previous->committed && id <= lease_admission->committed;
        } else {
          admitted = false;
        }
      }
      if ((old != identities_.end() && old->second != type) ||
          (old == identities_.end() && id <= allocation_base &&
           !admitted)) {
        throw std::invalid_argument{"Journal edit reuses or retypes an identity"};
      }
    }
    validate(value, false);
    auto published = make_published_storage(std::move(value));
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
    resident_auxiliary_.resize(estimate_auxiliary(snapshot, identities, history_));
    current_.swap(published);
    snapshot_.swap(snapshot);
    identities_.swap(identities);
    if constexpr (has_journal) {
      journal_state_.delta_count = delta ? journal_state_.delta_count + 1 : 0;
    }
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
    if constexpr (has_journal) {
      if (journal_state_.indeterminate ||
          (journal_state_.sink && journal_state_.sink->needs_recovery())) {
        throw journal_indeterminate_error{"Reopen the managed journal before further operations"};
      }
    }
  }

  // Consume an ID permanently, including on later cancellation or publication failure.
  id_type allocate_id() {
    if constexpr (has_isolated_allocator) {
      auto& cursor = isolated_allocation_.context;
      if (isolated_reservation_.reserve) {
        const auto [next, reserved] = isolated_reservation_.reserve();
        reserved.validate(std::numeric_limits<id_type>::max());
        const bool unleased = cursor.first == 0;
        const bool valid = unleased
            ? cursor.watermark < std::numeric_limits<id_type>::max() &&
                next == cursor.watermark + 1U && reserved.watermark == next &&
                reserved.first == 0 && reserved.last == 0 && reserved.consumed == 0
            : cursor.available() && next == cursor.consumed + 1U &&
                reserved.watermark == cursor.watermark && reserved.first == cursor.first &&
                reserved.last == cursor.last && reserved.consumed == next;
        if (next == 0 || !valid) {
          throw std::invalid_argument{"Preview reservation changed its native identity context"};
        }
        // The source completed its durable reservation before this workspace exposes the identity.
        cursor = reserved;
        allocated_id_ = reserved.watermark;
        return next;
      }
      if (cursor.first != 0) {
        return static_cast<id_type>(cursor.take());
      }
      if constexpr (requires { Traits::requires_identity_lease; }) {
        if constexpr (Traits::requires_identity_lease) {
          if (isolated_allocation_.initialized) {
            throw std::logic_error{"Reserve an authority native identity lease before creating collaboration entities"};
          }
        }
      }
    }
    if (collaboration_state_.attachment) {
      const auto next = collaboration_state_.attachment->allocate_identity();
      if (next == 0 || next > allocated_id_) {
        throw std::invalid_argument{"Collaboration issued an unreserved native identity"};
      }
      return static_cast<id_type>(next);
    }
    if (allocated_id_ == std::numeric_limits<id_type>::max()) {
      throw std::overflow_error{"Managed entity ID space exhausted"};
    }
    const auto next = allocated_id_ + 1;
    // Reservations precede exposing an ID, so canceled and failed edits cannot recycle it on restart.
    journal_control(detail::journal_record_kind::reserve, next);
    allocated_id_ = next;
    if constexpr (has_isolated_allocator) {
      isolated_allocation_.context.watermark = next;
    }
    return static_cast<id_type>(next);
  }

  // Burn a disjoint authority range; an attached journal flushes before IDs reach an offline writer.
  identity_allocation_context reserve_identity_range(std::string_view format,
      std::span<const std::uint8_t> session, std::uint64_t count, std::size_t maximum_ranges)
    requires(has_collaboration)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      throw std::logic_error{"Only an authority reserves native identity ranges"};
    }
    auto book = collaboration_state_.identity_leases
        ? std::make_unique<detail::identity_lease_book>(collaboration_state_.identity_leases->entries())
        : std::make_unique<detail::identity_lease_book>();
    auto lease = book->prepare(format, session, allocated_id_, count,
                               std::numeric_limits<id_type>::max(), maximum_ranges);
    const identity_allocation_context result{lease.last, lease.first, lease.last, lease.committed};
    book->publish(std::move(lease));
    // Only the infrequent block reservation rewrites a checkpoint; object creation consumes a cursor.
    if constexpr (has_journal) {
      if (journal_state_.sink) {
        collaboration_state_.identity_leases.swap(book);
        std::vector<std::uint8_t> bytes;
        try {
          bytes = encode_state(snapshot_, history_, result.watermark);
          check_journal_envelope(bytes);
        } catch (...) {
          collaboration_state_.identity_leases.swap(book);
          throw;
        }
        collaboration_state_.identity_leases.swap(book);
        journal_operation([&] { journal_state_.sink->save(bytes); });
      }
    }
    collaboration_state_.identity_leases.swap(book);
    allocated_id_ = result.watermark;
    return result;
  }

  // Record a consumed cursor, flushing an attached journal even when no model fields change.
  void commit_identity_cursor(const records::identity_lease& prepared)
    requires(has_collaboration)
  {
    require_idle();
    auto book = std::make_unique<detail::identity_lease_book>(
        collaboration_state_.identity_leases->entries());
    auto* entry = book->find(prepared.first);
    if (!entry || prepared.committed < entry->committed || prepared.committed > entry->last) {
      throw std::invalid_argument{"Invalid native identity lease cursor"};
    }
    entry->committed = prepared.committed;
    if constexpr (has_journal) {
      if (journal_state_.sink) {
        collaboration_state_.identity_leases.swap(book);
        std::vector<std::uint8_t> bytes;
        try {
          bytes = encode_state(snapshot_, history_, allocated_id_);
          check_journal_envelope(bytes);
        } catch (...) {
          collaboration_state_.identity_leases.swap(book);
          throw;
        }
        collaboration_state_.identity_leases.swap(book);
        journal_operation([&] { journal_state_.sink->save(bytes); });
      }
    }
    collaboration_state_.identity_leases.swap(book);
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
  void validate(const storage_type& value, bool application_policy = true,
                const document_id* reference_document = nullptr) {
    if (callback_active_) {
      throw std::logic_error{"Reentrant managed validation"};
    }
    callback_active_ = true;
    try {
      Traits::validate(value);
      if constexpr (requires { Traits::has_entity_references; }) {
        if constexpr (Traits::has_entity_references) {
          detail::validate_entity_references<Traits>(value,
              reference_document ? *reference_document : document_, options_.decode, resident_ledger_);
        }
      }
      if (registry_validate_) { (*registry_validate_)(value); }
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
  event_type restore_snapshot(const std::vector<std::uint8_t>& bytes, std::uint64_t selection) {
    auto value = detail::decode<storage_type>(bytes, options_.decode);
    auto identities = inspect(value, allocated_id_);
    detail::reset_runtime_fields<Traits>(value);
    validate(value);
    auto published = make_published_storage(std::move(value));
    auto snapshot = bytes;
    auto event = prepare_publication(publication_kind::history_navigation, identities, published.get());
    detail::resident_admission admission{
        resident_auxiliary_, estimate_auxiliary(snapshot, identities, history_)};
    journal_control(detail::journal_record_kind::select, selection);
    current_.swap(published);
    snapshot_.swap(snapshot);
    identities_.swap(identities);
    admission.commit();
    return event;
  }

  // Navigate by deque position; the caller has checked that the index is retained.
  void checkout_linear(std::size_t index)
    requires(Mode == history_mode::linear)
  {
    const auto& node = history_.entries[index];
    auto event = restore_snapshot(node.snapshot, index);
    history_.cursor = index;
    publish_notification(std::move(event));
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

    // Resolve typed entities through this callback's existing transaction channel.
    auto registry() { return owner_.registry(); }

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
    std::shared_ptr<storage_type> candidate_{};
    std::shared_ptr<detail::resident_charge> candidate_charge_{};
    std::uint64_t allocation_base_{};
    SERIALIZER_MANAGED_EMPTY_MEMBER
    detail::isolated_allocation_storage<has_collaboration || has_isolated_allocator> allocation_state_{};
    int exceptions_{};
    bool editing_{};
    std::shared_ptr<detail::edit_channel<storage_type, id_type>> channel_{};
    friend class model_store;

    // Acquire the sole writer only after candidate construction has succeeded.
    transaction(model_store& store, outcome_type& outcome, label_type label = {})
        : label_type{std::move(label)}, store_{&store}, outcome_{&outcome},
          allocation_base_{store.allocated_id_}, exceptions_{std::uncaught_exceptions()} {
      store.require_idle();
      if constexpr (has_collaboration || has_isolated_allocator) {
        if (store.collaboration_state_.attachment || has_isolated_allocator) {
          allocation_state_.context = store.allocation_context();
        }
      }
      if (exceptions_ != 0) {
        throw std::logic_error{"Cannot begin a transaction during stack unwinding"};
      }
      auto candidate = store.make_storage_candidate(
          detail::decode<storage_type>(store.snapshot_, store.options_.decode));
      candidate_ = std::move(candidate.first);
      candidate_charge_ = std::move(candidate.second);
      detail::reset_runtime_fields<Traits>(*candidate_);
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
      candidate_charge_.reset();
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

    // Commit this exact private preview's burned IDs without adding state to ordinary transactions.
    // The enclosing preview owns the sorted span until completion; callers cannot grant admission.
    void commit_preview(const identity_allocation_context& expected,
                        std::span<const id_type> reserved) {
      require_active();
      if (editing_ || store_->allocation_context() != expected) {
        reject("Preview identity admission has a stale or active transaction base");
      }
      id_type previous{};
      for (const auto id : reserved) {
        if (id == 0 || id <= previous || id > store_->allocated_id_ || store_->identities_.contains(id) ||
            (expected.first != 0 && (id < expected.first || id > expected.consumed))) {
          reject("Preview identity admission contains a reused or unreserved native identity");
        }
        previous = id;
      }
      commit_impl(reserved);
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
      candidate_charge_ = std::move(other.candidate_charge_);
      static_cast<label_type&>(*this) = std::move(static_cast<label_type&>(other));
      allocation_base_ = other.allocation_base_;
      if constexpr (has_collaboration || has_isolated_allocator) {
        allocation_state_ = other.allocation_state_;
      }
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
        if constexpr (detail::edit_channel<storage_type, id_type>::has_reference_scope) {
          channel_->reference_document = store_->document_;
        }
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

    // Reuse editor lifetime and failure boundaries for generated typed entity lookup.
    auto registry() {
      static_cast<void>(root());
      return registry_edit<Root, Traits>{channel_, store_->document_};
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
        if constexpr (requires { Traits::type_key; }) {
          if (channel_ && channel_->registry_reservations && !channel_->registry_reservations->empty()) {
            // A trusted raw insertion also consumes the reservation once, even if later erased.
            auto& reservations = *channel_->registry_reservations;
            detail::visit_registry_entities<Traits>(*candidate_, [&]<typename Plain>(const auto& node) {
              static_cast<void>(sizeof(Plain));
              std::erase(reservations, node.persistent_id);
            });
          }
        }
        editing_ = false;
        candidate_charge_->resize(store_->estimate_storage(*candidate_));
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

    // Complete an ordinary transaction without admitting identities reserved by a preview.
    void commit() {
      commit_impl({});
    }

  private:
    // Prepare every allocation before atomic pointer/index swaps; record and rethrow failures.
    void commit_impl(std::span<const id_type> preview_reservations) {
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
          if constexpr (has_collaboration || has_isolated_allocator) {
            if (const auto* restoration = store.collaboration_state_.restoration) {
              const auto found = restoration->find(id);
              restored = found != restoration->end() && found->second == type;
            }
            if ((store.collaboration_state_.attachment || has_isolated_allocator) &&
                allocation_state_.context.first != 0) {
              const auto cursor = store.allocation_context();
              restored = restored || (cursor.first == allocation_state_.context.first &&
                  id > allocation_state_.context.consumed && id <= cursor.consumed);
            }
          }
          if (previous == store.identities_.end() && id <= allocation_base_ && !restored) {
            restored = std::binary_search(preview_reservations.begin(), preview_reservations.end(), id);
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
        detail::reset_runtime_fields<Traits>(*candidate_);
        auto event = store.prepare_publication(publication_kind::durable_edit, identities, candidate_.get());
        candidate_charge_->resize(store.estimate_storage(*candidate_));
        auto published = std::shared_ptr<const storage_type>{std::move(candidate_)};
        detail::resident_admission admission{
            store.resident_auxiliary_, store.estimate_edit_auxiliary(
                                          snapshot, identities, static_cast<const label_type&>(*this))};
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
                                 plan.evict_count, !preview_reservations.empty());
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
                                 0, !preview_reservations.empty());
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
          store.journal_edit(snapshot, static_cast<const label_type&>(*this), allocation_base_, 0,
                             !preview_reservations.empty());
        }
        store.current_.swap(published);
        store.snapshot_.swap(snapshot);
        store.identities_.swap(identities);
        if constexpr (has_collaboration) {
          if (const auto* prepared = store.collaboration_state_.prepared_identity_lease) {
            store.collaboration_state_.identity_leases->find(prepared->first)->committed =
                prepared->committed;
          }
        }
        admission.commit();
        if (store.collaboration_state_.attachment) {
          store.collaboration_state_.attachment->publish();
        }
        close(transaction_status::committed);
        store.publish_notification(std::move(event));
      } catch (...) {
        if (store_->collaboration_state_.attachment) {
          store_->collaboration_state_.attachment->abort();
        }
        fail(std::current_exception());
        throw;
      }
    }
  };

  // A synchronous memory-only candidate. Generated runtime editors cannot set durable members.
  class runtime_edit {
    model_store* store_{};
    std::shared_ptr<storage_type> candidate_{};
    std::shared_ptr<detail::resident_charge> candidate_charge_{};
    std::shared_ptr<detail::edit_channel<storage_type, id_type>> channel_{};
    std::exception_ptr error_{};
    state_stamp expected_{};
    bool editing_{};
    friend class model_store;

    // Copy runtime values onto a fresh durable decode so no published pin is mutated.
    explicit runtime_edit(model_store& store) : store_{&store}, expected_{store.stamp_} {
      store.require_idle();
      auto candidate = store.make_storage_candidate(
          detail::decode<storage_type>(store.snapshot_, store.options_.decode));
      candidate_ = std::move(candidate.first);
      candidate_charge_ = std::move(candidate.second);
      detail::copy_runtime_fields<Traits>(*candidate_, *store.current_);
      candidate_charge_->resize(store.estimate_storage(*candidate_));
      store.writer_active_ = true;
    }

    // Reject stale, closed, cross-thread and poisoned access before borrowing candidate storage.
    void require_active() const {
      if (!store_) {
        throw std::logic_error{"Managed runtime editor is closed"};
      }
      store_->check_thread();
      if (store_->stamp_ != expected_ || store_->callback_active_) {
        throw std::logic_error{"Managed runtime editor has a stale or reentrant state"};
      }
      if (error_) {
        std::rethrow_exception(error_);
      }
    }

    // Release the writer and invalidate escaped editors without publishing a partial value.
    void close() noexcept {
      if (channel_) {
        channel_->owner = nullptr;
      }
      if (store_) {
        store_->writer_active_ = false;
        store_ = nullptr;
      }
    }

  public:
    runtime_edit(const runtime_edit&) = delete;
    runtime_edit& operator=(const runtime_edit&) = delete;
    // Unwinding discards the private candidate and leaves accepted state and history unchanged.
    ~runtime_edit() { close(); }

    // Mutate an isolated candidate; durable changes are independently checked before publication.
    template <typename Callable>
      requires std::same_as<std::invoke_result_t<Callable, storage_type&>, void>
    void update(Callable&& callback) {
      require_active();
      if (editing_) {
        error_ = std::make_exception_ptr(std::logic_error{"Nested managed runtime update"});
        std::rethrow_exception(error_);
      }
      editing_ = true;
      try {
        std::invoke(std::forward<Callable>(callback), *candidate_);
        editing_ = false;
        candidate_charge_->resize(store_->estimate_storage(*candidate_));
        if (error_) {
          std::rethrow_exception(error_);
        }
      } catch (...) {
        editing_ = false;
        error_ = std::current_exception();
        throw;
      }
    }

    // Bind a checked generated root editor; runtime access never allocates persistent identities.
    auto root() {
      require_active();
      if (!channel_) {
        channel_ = std::make_shared<detail::edit_channel<storage_type, id_type>>();
        if constexpr (detail::edit_channel<storage_type, id_type>::has_reference_scope) {
          channel_->reference_document = store_->document_;
        }
        channel_->owner = this;
        channel_->mutate = [](void* owner, void* callable, void (*invoke)(void*, storage_type&)) {
          static_cast<runtime_edit*>(owner)->update(
              [&](storage_type& value) { invoke(callable, value); });
        };
        channel_->allocate = [](void*) -> id_type {
          throw std::logic_error{"Runtime updates cannot allocate persistent identities"};
        };
        channel_->fail = [](void* owner, std::exception_ptr error) noexcept {
          static_cast<runtime_edit*>(owner)->error_ = std::move(error);
        };
      }
      auto resolve = [](storage_type& value) -> storage_type& { return value; };
      return Traits::make_editor(
          detail::editor_access<storage_type, storage_type, true, decltype(resolve), true>{
              channel_, resolve});
    }
  };

  // Publish a guarded runtime update without journal writes, dirty changes or history entries.
  template <typename Callable>
    requires std::same_as<std::invoke_result_t<Callable, runtime_edit&>, void>
  [[nodiscard]] transaction_outcome update_runtime(Callable&& callback) {
    transaction_outcome outcome;
    try {
      runtime_edit edit{*this};
      std::invoke(std::forward<Callable>(callback), edit);
      edit.require_active();
      const auto snapshot = detail::encode(*edit.candidate_, options_.max_snapshot_bytes);
      if (snapshot != snapshot_ || inspect(*edit.candidate_, allocated_id_) != identities_) {
        throw std::invalid_argument{"Runtime update changes durable data or identity"};
      }
      validate(*edit.candidate_);
      if constexpr (requires { Traits::runtime_fields_equal(*edit.candidate_, *current_); }) {
        if (Traits::runtime_fields_equal(*edit.candidate_, *current_)) {
          outcome.status = transaction_status::no_change;
          return outcome;
        }
      }
      auto event = prepare_publication(publication_kind::runtime_update, identities_);
      edit.candidate_charge_->resize(estimate_storage(*edit.candidate_));
      auto published = std::shared_ptr<const storage_type>{std::move(edit.candidate_)};
      current_.swap(published);
      edit.close();
      outcome.status = transaction_status::committed;
      publish_notification(std::move(event));
    } catch (...) {
      outcome.status = failure_status();
      outcome.error = std::current_exception();
    }
    return outcome;
  }

  // Create a fresh document. All generated managed IDs must initially be zero and are assigned here.
  explicit model_store(Root value, document_id document = make_document_id(),
                       store_options options = {},
                       std::function<void(const storage_type&)> validate = {})
      : model_store{std::move(value), document, options, std::move(validate), {}} {}

  // Initialize required/forward reference edges after assigning native IDs and before any baseline
  // is accepted. The trusted synchronous callback assigns values/edges, retaining initial ownership.
  model_store(Root value, document_id document, store_options options,
              std::function<void(const storage_type&)> validate,
              const std::function<void(storage_type&, const reference_builder<Root, Traits>&)>& initialize)
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
    if (initialize) {
      if constexpr (requires { Traits::type_key; Traits::class_id; }) {
        const reference_builder<Root, Traits> references{storage, document_,
            {options_.decode.max_collection_elements, options_.decode.max_allocation_bytes},
            resident_ledger_};
        initialize(storage, references);
      } else {
        throw std::logic_error{"Reference initialization requires generated managed type metadata"};
      }
    }
    this->validate(storage);
    identities_ = inspect(storage, allocated_id_);
    snapshot_ = detail::encode(storage, options_.max_snapshot_bytes);
    check_snapshot(snapshot_);
    current_ = make_published_storage(std::move(storage));
    if constexpr (has_history) {
      auto history = make_history();
      history_.swap(history);
    }
    resident_auxiliary_.resize(estimate_auxiliary(snapshot_, identities_, history_));
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

  // Build an opt-in derived catalog pinned to this document's selected immutable snapshot.
  auto registry(registry_options options = {}) const {
    check_thread();
    return registry_view<Root, Traits>{current_, options, resident_ledger_, document_};
  }

  // Enforce a declared scoped/composite key at every existing validation/publication boundary.
  // The descriptor key remains ordinary durable model data and never replaces persistent_id.
  template <typename Descriptor, typename Key = typename Descriptor::value_type,
            typename Compare = std::less<Key>>
  void declare_unique_key(Descriptor descriptor, Compare compare = {}) {
    require_idle();
    using Plain = typename Descriptor::plain_type;
    static_assert(std::same_as<typename Descriptor::storage_type, typename model_traits<Plain>::storage_type>,
                  "Registry descriptors must match the generated entity storage");
    auto invariant = [descriptor, compare = std::move(compare)](const storage_type& root) {
      std::map<Key, id_type, Compare> keys{compare};
      detail::visit_registry_entities<Traits>(root, [&]<typename Actual>(const auto& storage) {
        if constexpr (std::same_as<Plain, Actual>) {
          if (!keys.emplace(descriptor.read(storage), storage.persistent_id).second) {
            throw registry_error{registry_error_code::duplicate_key, "Duplicate registry key"};
          }
        }
      });
    };
    callback_active_ = true;
    try {
      invariant(*current_);
      // User comparison functions cannot navigate or publish while a snapshot is being traversed.
      // A new invariant must permit every retained future/past before it becomes active.
      if constexpr (Mode == history_mode::linear) {
        for (const auto& entry : history_.entries) {
          invariant(detail::decode<storage_type>(entry.snapshot, options_.decode));
        }
      } else if constexpr (Mode == history_mode::tree) {
        for (const auto& [number, revision] : history_.revisions) {
          static_cast<void>(number);
          invariant(detail::decode<storage_type>(revision->snapshot, options_.decode));
        }
      }
      callback_active_ = false;
    } catch (...) {
      callback_active_ = false;
      throw;
    }
    auto previous = registry_validate_;
    registry_validate_ = std::make_shared<const std::function<void(const storage_type&)>>(
        [previous = std::move(previous), invariant = std::move(invariant)](const storage_type& root) {
      if (previous) { (*previous)(root); }
      invariant(root);
    });
  }

  // Report conservative admitted model/metadata storage; this is not a process peak-memory meter.
  std::size_t resident_bytes() const noexcept { return resident_ledger_->bytes(); }

  // Materialize an independent value; only the opt-in separate-values representation removes IDs.
  Root clone_value(clone_options options = {}) const {
    check_thread();
    if (options.runtime_fields == runtime_copy_policy::preserve) {
      return Traits::clone_value(*current_);
    }
    auto value = *current_;
    detail::reset_runtime_fields<Traits>(value);
    return Traits::clone_value(value);
  }

  // Own an isolated history-free workspace; its source store outlives this owner-thread scope.
  // Immutable reads may retain their pins, but workspace editors never become live-store editors.
  class preview_edit {
    struct preview_traits : Traits {
      static constexpr bool isolated_allocation_context = true;
    };
    // Keep native reservations stable across workspace moves; cancellation never recycles exposed IDs.
    struct preview_reservations {
      identity_allocation_context expected{};
      std::vector<id_type> identities{};
      detail::resident_charge charge;

      // Admit the workspace's reservation metadata against the same ledger as its pinned model.
      preview_reservations(const std::shared_ptr<detail::resident_ledger>& ledger,
                           identity_allocation_context context)
          : expected{context}, charge{ledger, sizeof(preview_reservations)} {}

      // Grow before native reservation so no allocation can fail after an ID is exposed to the workspace.
      void prepare_identity(std::size_t maximum) {
        maximum = std::min(maximum, identities.max_size());
        if (identities.size() >= maximum) {
          throw std::length_error{"Managed preview identity budget exceeded"};
        }
        if (identities.size() < identities.capacity()) { return; }
        const auto capacity = identities.capacity() == 0 ? std::size_t{1}
            : identities.capacity() <= maximum / 2 ? identities.capacity() * 2 : maximum;
        auto estimate = sizeof(preview_reservations);
        detail::add_memory_estimate(estimate, detail::multiply_memory_estimate(capacity, sizeof(id_type)));
        detail::resident_admission admission{charge, estimate};
        identities.reserve(capacity);
        admission.commit();
      }
    };
  public:
    using working_store_type = model_store<Root, history_mode::disabled,
        history_labels::disabled, preview_traits, store_features::none>;

  private:
    friend class model_store;
    const model_store* source_{};
    std::shared_ptr<const storage_type> base_{};
    state_stamp base_stamp_{};
    identity_allocation_context allocation_base_{};
    std::shared_ptr<preview_reservations> reservations_{};
    std::unique_ptr<working_store_type> working_{};
    bool accepting_{};

    // Share the accepted root and copy only current snapshot/index metadata, never retained history.
    explicit preview_edit(model_store& source)
        : source_{&source}, base_{source.current_}, base_stamp_{source.stamp_},
          allocation_base_{source.allocation_context()} {
      reservations_ = std::allocate_shared<preview_reservations>(
          detail::resident_allocator<preview_reservations>{source.options_.allocation_resource},
          source.resident_ledger_, allocation_base_);
      auto working = std::unique_ptr<working_store_type>{new working_store_type{
          typename working_store_type::recovery_tag{}, source.options_, source.validate_,
          source.resident_ledger_}};
      working->document_ = source.document_;
      working->allocated_id_ = source.allocated_id_;
      working->current_ = source.current_;
      working->registry_validate_ = source.registry_validate_;
      // Admit the derived metadata before copying it. The shared root keeps its existing charge.
      working->resident_auxiliary_.resize(working->estimate_auxiliary(
          source.snapshot_, source.identities_, working->history_));
      working->snapshot_ = source.snapshot_;
      working->identities_ = source.identities_;
      working->initialize_allocation_context(allocation_base_,
          [owner = &source, base = base_, stamp = base_stamp_, reservations = reservations_]() {
            if (owner->current_ != base || owner->stamp_ != stamp) {
              throw std::logic_error{"Managed preview has a stale base"};
            }
            reservations->prepare_identity(owner->options_.decode.max_collection_elements);
            auto reserved = owner->reserve_preview_identity(reservations->expected);
            reservations->expected = reserved.second;
            reservations->identities.push_back(reserved.first);
            return reserved;
          });
      working->resident_auxiliary_.resize(working->estimate_auxiliary(
          working->snapshot_, working->identities_, working->history_));
      working_ = std::move(working);
    }

  public:
    preview_edit(const preview_edit&) = delete;
    preview_edit& operator=(const preview_edit&) = delete;
    // Transfer an idle workspace without invalidating its source reservation callback.
    preview_edit(preview_edit&& other) {
      if (other.accepting_) { throw std::logic_error{"Cannot move a preview during acceptance"}; }
      if (other.working_) { other.working_->require_idle(); }
      source_ = other.source_;
      base_ = std::move(other.base_);
      base_stamp_ = other.base_stamp_;
      allocation_base_ = other.allocation_base_;
      reservations_ = std::move(other.reservations_);
      working_ = std::move(other.working_);
    }
    preview_edit& operator=(preview_edit&&) = delete;

    // Borrow this isolated store for normal generated editing; closed previews reject access.
    working_store_type& working() {
      if (!working_ || accepting_) { throw std::logic_error{"Managed preview is closed or accepting"}; }
      return *working_;
    }

    // Report whether this proposal still owns an editable workspace.
    bool active() const noexcept { return working_ != nullptr; }

    // Discard private values/history; native IDs already exposed by the workspace remain consumed.
    void cancel() {
      if (accepting_) { throw std::logic_error{"Cannot cancel a preview during acceptance"}; }
      if (working_) { working_->require_idle(); }
      working_.reset();
      reservations_.reset();
      base_.reset();
    }
  };

  // Start a bounded editing workspace without encoding or copying the retained native history.
  [[nodiscard]] preview_edit preview() requires (!has_isolated_allocator) {
    require_idle();
    if (collaboration_state_.attachment && allocation_context().first == 0U) {
      throw std::logic_error{"Collaborative preview requires an initialized identity reservation"};
    }
    return preview_edit{*this};
  }

  // Accept only an exact-base proposal as one native transaction, preserving all proposed IDs.
  // Successful acceptance closes the workspace. Failed live reservations obey normal nonreuse rules.
  [[nodiscard]] outcome_type accept_preview(preview_edit& proposal) {
    try {
      require_idle();
      if (proposal.source_ != this || !proposal.working_ || !proposal.reservations_ || proposal.accepting_ ||
          proposal.base_ != current_ || proposal.base_stamp_ != stamp_ ||
          proposal.reservations_->expected != allocation_context()) {
        throw std::logic_error{"Managed preview has a stale or foreign base"};
      }
      auto& working = *proposal.working_;
      working.require_idle();
      if (working.document_.high != document_.high || working.document_.low != document_.low ||
          working.allocation_context() != proposal.reservations_->expected) {
        throw std::logic_error{"Managed preview identity context changed"};
      }
      const auto selected = working.current_;
      struct acceptance_scope {
        bool* flag;
        bool* workspace_callback;
        // Release both fences before closing the workspace, including on exceptional completion.
        void release() noexcept {
          if (flag) {
            *flag = false;
            *workspace_callback = false;
            flag = nullptr;
            workspace_callback = nullptr;
          }
        }
        // Restore mutation access when acceptance leaves the workspace available for inspection.
        ~acceptance_scope() { release(); }
      } scope{&proposal.accepting_, &working.callback_active_};
      proposal.accepting_ = true;
      working.callback_active_ = true;
      validate(*selected);
      auto result = execute_transaction([&](auto& edit) {
        edit.update([&](storage_type& candidate) { candidate = *selected; });
        edit.owner_.commit_preview(proposal.reservations_->expected,
                                   proposal.reservations_->identities);
      });
      if (result.status == transaction_status::committed || result.status == transaction_status::no_change) {
        scope.release();
        proposal.cancel();
      }
      return result;
    } catch (...) {
      outcome_type result;
      result.status = failure_status();
      result.error = std::current_exception();
      return result;
    }
  }

  // Inspect the accepted state generation without changing history or a journal sequence.
  state_stamp state() const {
    check_thread();
    return stamp_;
  }

  // Read the selected tree revision; linear histories deliberately do not expose revision IDs.
  std::uint64_t current_revision() const
    requires(Mode == history_mode::tree)
  {
    check_thread();
    return history_.current_revision;
  }

  // Read the retained linear cursor; eviction may change its index without recycling entity IDs.
  std::size_t history_cursor() const
    requires(Mode == history_mode::linear)
  {
    check_thread();
    return history_.cursor;
  }

  // Install one owner-thread observer; callbacks may read pins but must not reenter mutation.
  void set_publication_callback(std::function<void(const event_type&)> callback) {
    require_idle();
    publication_callback_ = std::move(callback);
  }

  // Consume an observer error separately from the successfully accepted store operation.
  std::exception_ptr take_notification_error() {
    require_idle();
    return std::exchange(notification_error_, {});
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

  // Capture the complete allocator base for isolated previews, including the durable client cursor.
  identity_allocation_context allocation_context() const {
    check_thread();
    if constexpr (has_isolated_allocator) {
      auto result = isolated_allocation_.context;
      result.watermark = allocated_id_;
      return result;
    }
    auto result = collaboration_state_.attachment
        ? collaboration_state_.attachment->allocation_context() : identity_allocation_context{};
    result.watermark = allocated_id_;
    return result;
  }

  // Initialize a private preview cursor after its baseline has been loaded; never attach live services.
  void initialize_allocation_context(const identity_allocation_context& context)
    requires(has_isolated_allocator)
  {
    require_idle();
    context.validate(std::numeric_limits<id_type>::max());
    if (context.watermark != allocated_id_) {
      throw std::invalid_argument{"Isolated allocation baseline differs from native watermark"};
    }
    isolated_allocation_.context = context;
    isolated_allocation_.initialized = true;
  }

  // Bind an isolated workspace to a source which durably burns every identity before returning it.
  // The callback outlives this workspace and may reject stale source state; failures may burn reservations.
  void initialize_allocation_context(const identity_allocation_context& context,
                                     identity_reservation_callback reserve)
    requires(has_isolated_allocator)
  {
    if (!reserve) {
      throw std::invalid_argument{"Preview identity reservation callback is empty"};
    }
    initialize_allocation_context(context);
    isolated_reservation_.reserve = std::move(reserve);
  }

  // Permanently reserve one preview identity through the ordinary native/client journal boundary.
  // The source must be idle at the exact captured context; cancellation never recycles the returned ID.
  std::pair<id_type, identity_allocation_context> reserve_preview_identity(
      const identity_allocation_context& expected) requires (!has_isolated_allocator) {
    require_idle();
    if (allocation_context() != expected) {
      throw std::logic_error{"Preview identity reservation has a stale native allocation base"};
    }
    const auto id = allocate_id();
    return {id, allocation_context()};
  }

  // Admit an exact preview's IDs inside its accepting native transaction; failures may burn reservations.
  void advance_allocation_context(const identity_allocation_context& expected,
                                   const identity_allocation_context& desired) {
    check_thread();
    if (!writer_active_ || callback_active_ || allocation_context() != expected) {
      throw std::logic_error{"Preview allocation base changed or has no accepting transaction"};
    }
    desired.validate(std::numeric_limits<id_type>::max());
    if (collaboration_state_.attachment) {
      collaboration_state_.attachment->advance_identity_context(desired);
    } else {
      if (expected.first != 0 || desired.first != 0 || desired.last != 0 ||
          desired.consumed != 0 || desired.watermark < allocated_id_) {
        throw std::invalid_argument{"Preview changes native allocation ownership"};
      }
      while (allocated_id_ < desired.watermark) {
        static_cast<void>(allocate_id());
      }
    }
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
    auto event = restore_snapshot(found->second->snapshot, revision);
    history_.current_revision = revision;
    publish_notification(std::move(event));
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
    detail::resident_admission admission{
        resident_auxiliary_, estimate_auxiliary(snapshot_, identities_, history)};
    journal_control(detail::journal_record_kind::reset);
    history_.swap(history);
    admission.commit();
  }

  // Export a coherent memory envelope; this does not mark the journaled document saved.
  std::vector<std::uint8_t> save() const {
    require_idle();
    if (collaboration_state_.attachment) {
      return collaboration_state_.attachment->save();
    }
    return encode_state(snapshot_, history_, allocated_id_);
  }

  // Export a generated patch record for transfer/storage against a caller-retained full envelope.
  records::snapshot_patch save_delta(std::span<const std::uint8_t> base) const {
    return make_snapshot_patch(base, save(), options_.decode.max_input_bytes);
  }

  // Apply a checked envelope patch and reuse full model/history validation before publication.
  void load_delta(std::span<const std::uint8_t> base, const records::snapshot_patch& patch) {
    require_idle();
    load(apply_snapshot_patch(base, patch, options_.decode.max_input_bytes));
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
    const auto document = options.document_codec ? journal_document_snapshot()
                                                 : std::vector<std::uint8_t>{};
    detail::resident_admission admission{
        resident_auxiliary_, estimate_auxiliary(snapshot_, identities_, history_, &saved)};
    journal_operation([&] {
      journal_state_.sink = detail::create_file_journal(path, mode, bytes, std::move(options),
                                                        journal_state_.indeterminate, document);
    });
    journal_state_.saved_snapshot.swap(saved);
    admission.commit();
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
    model_store recovered{recovery_tag{}, options_, {}, resident_ledger_};
    recovered.registry_validate_ = registry_validate_;
    std::vector<std::uint8_t> saved;
    event_type event;
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
      recovered.resident_auxiliary_.resize(recovered.estimate_auxiliary(
          recovered.snapshot_, recovered.identities_, recovered.history_, &saved));
      event = prepare_publication(publication_kind::load, recovered.identities_,
                                  recovered.current_.get(), document_.high != recovered.document_.high ||
                                                                document_.low != recovered.document_.low);
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
      resident_auxiliary_.swap(recovered.resident_auxiliary_);
      current_.swap(recovered.current_);
      snapshot_.swap(recovered.snapshot_);
      identities_.swap(recovered.identities_);
      journal_state_.saved_snapshot.swap(saved);
      document_ = recovered.document_;
      allocated_id_ = recovered.allocated_id_;
      if constexpr (has_collaboration) {
        collaboration_state_.identity_leases.swap(recovered.collaboration_state_.identity_leases);
      }
      journal_state_.sink.swap(journal);
      journal_state_.delta_count = recovered.journal_state_.delta_count;
    });
    publish_notification(std::move(event));
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
    detail::resident_admission admission{
        resident_auxiliary_, estimate_auxiliary(snapshot_, identities_, history_, &saved)};
    const auto document = journal_state_.sink->uses_document_codec()
                              ? journal_document_snapshot() : std::vector<std::uint8_t>{};
    journal_operation([&] { journal_state_.sink->save_document(bytes, document); });
    journal_state_.saved_snapshot.swap(saved);
    journal_state_.delta_count = 0;
    admission.commit();
  }

  // Restore the full saved baseline/history durably; never recycle abandoned object/revision IDs.
  // Unattached/shared stores reject. Failure before replacement preserves accepted state; uncertain
  // replacement fences the writer and requires fresh recovery. Saved baseline reads are lazy.
  void discard_journal_changes()
    requires(has_journal)
  {
    require_idle();
    if (collaboration_state_.attachment) {
      throw std::logic_error{"Discard is unavailable while collaboration is attached"};
    }
    if (!journal_state_.sink) {
      throw std::logic_error{"Managed store has no journal"};
    }
    event_type event;
    journal_operation([&] {
      model_store restored{recovery_tag{}, options_, {}, resident_ledger_};
      restored.registry_validate_ = registry_validate_;
      restored.validate_ = validate_;
      restored.load(journal_state_.sink->saved_envelope());
      if (restored.document_.high != document_.high || restored.document_.low != document_.low ||
          restored.current_->persistent_id != current_->persistent_id) {
        throw std::invalid_argument{"Saved baseline belongs to another managed document"};
      }
      restored.allocated_id_ = std::max(restored.allocated_id_, allocated_id_);
      if constexpr (Mode == history_mode::tree) {
        restored.history_.revision_high_water =
            std::max(restored.history_.revision_high_water, history_.revision_high_water);
      }
      auto saved = restored.snapshot_;
      auto bytes = restored.save();
      check_journal_envelope(bytes);
      restored.resident_auxiliary_.resize(restored.estimate_auxiliary(
          restored.snapshot_, restored.identities_, restored.history_, &saved));
      event = prepare_publication(publication_kind::load, restored.identities_, restored.current_.get());
      journal_state_.sink->discard(bytes);
      // Every allocation/validation precedes durable replacement; publication below cannot throw.
      if constexpr (has_history) { history_.swap(restored.history_); }
      resident_auxiliary_.swap(restored.resident_auxiliary_);
      current_.swap(restored.current_);
      snapshot_.swap(restored.snapshot_);
      identities_.swap(restored.identities_);
      journal_state_.saved_snapshot.swap(saved);
      journal_state_.delta_count = 0;
      allocated_id_ = restored.allocated_id_;
    });
    publish_notification(std::move(event));
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

  // Convert every retained source payload, preserving graph, labels, namespace and identities.
  // The converter receives old storage and returns new storage; executable commands are never replayed.
  template <typename SourceRoot, typename SourceTraits = model_traits<SourceRoot>, typename Callable>
    requires std::same_as<std::invoke_result_t<Callable, const typename SourceTraits::storage_type&>,
                          storage_type>
  void load_migrated(const std::vector<std::uint8_t>& bytes, Callable&& converter) {
    require_idle();
    if (journal_state_.sink || collaboration_state_.attachment) {
      throw std::logic_error{"Migrate an unattached managed document before attaching services"};
    }
    using source_store = model_store<SourceRoot, Mode, Labels, SourceTraits, Features>;
    static_assert(std::same_as<typename SourceTraits::id_type, id_type>,
                  "Managed migration preserves identity width");
    source_store source{typename source_store::recovery_tag{}, options_, {}, resident_ledger_};
    source.load(bytes); // Validate the original graph and all payloads before invoking conversion.
    auto allocation = decode_allocation_checkpoint(bytes);
    auto envelope = detail::decode<envelope_type>(
        allocation.format_version == 0 ? bytes : allocation.model, options_.decode);
    std::map<std::vector<std::uint8_t>, std::vector<std::uint8_t>> converted;
    callback_active_ = true;
    try {
      const auto convert = [&](std::vector<std::uint8_t>& snapshot) {
        const auto found = converted.find(snapshot);
        if (found != converted.end()) {
          snapshot = found->second;
          return;
        }
        auto old_value = detail::decode<typename SourceTraits::storage_type>(snapshot, options_.decode);
        auto value = std::invoke(converter, std::as_const(old_value));
        detail::reset_runtime_fields<Traits>(value);
        if (source.inspect(old_value, envelope.allocated_id) != inspect(value, envelope.allocated_id)) {
          throw std::invalid_argument{"Managed migration changes entity identities or type keys"};
        }
        auto replacement = detail::encode(value, options_.max_snapshot_bytes);
        check_snapshot(replacement);
        converted.emplace(snapshot, replacement);
        snapshot = std::move(replacement);
      };
      convert(envelope.current_snapshot);
      if constexpr (Mode == history_mode::tree) {
        for (auto& revision : envelope.revisions) {
          convert(revision.snapshot);
        }
      } else {
        for (auto& entry : envelope.entries) {
          convert(entry.snapshot);
        }
      }
      envelope.schema_id = std::string{Traits::schema_id};
      callback_active_ = false;
    } catch (...) {
      callback_active_ = false;
      throw;
    }
    // Existing load validates all new states and publishes only after every fallible step succeeds.
    auto migrated = detail::encode(envelope, options_.decode.max_input_bytes);
    if (allocation.format_version != 0) {
      allocation.model = std::move(migrated);
      if (options_.decode.max_input_bytes <= allocation_checkpoint_magic.size()) {
        throw std::length_error{"Native identity checkpoint exceeds its byte budget"};
      }
      migrated = detail::encode(allocation,
          options_.decode.max_input_bytes - allocation_checkpoint_magic.size());
      migrated.insert(migrated.begin(), allocation_checkpoint_magic.begin(), allocation_checkpoint_magic.end());
    }
    load(migrated);
  }

  // Recover an old journal and create a migrated journal at a distinct new path before publication.
  // The old journal remains the recovery source; no mixed-schema records are appended to it.
  template <typename SourceRoot, typename SourceTraits = model_traits<SourceRoot>, typename Callable>
    requires(has_journal &&
             std::same_as<std::invoke_result_t<Callable, const typename SourceTraits::storage_type&>,
                          storage_type>)
  void recover_migrated_journal(const std::filesystem::path& source_path,
                                const std::filesystem::path& destination_path,
                                Callable&& converter,
                                journal_storage_mode mode = journal_storage_mode::appended,
                                journal_options options = {}) {
    require_idle();
    if (journal_state_.sink || collaboration_state_.attachment) {
      throw std::logic_error{"Recover migration into an unattached managed document"};
    }
    if (std::filesystem::absolute(source_path).lexically_normal() ==
        std::filesystem::absolute(destination_path).lexically_normal()) {
      throw std::invalid_argument{"Managed migration requires a distinct journal destination"};
    }
    event_type event;
    journal_operation([&] {
      using source_store = model_store<SourceRoot, Mode, Labels, SourceTraits, Features>;
      source_store source{typename source_store::recovery_tag{}, options_, {}, resident_ledger_};
      source.recover_journal(source_path, options);
      model_store migrated{recovery_tag{}, options_, validate_, resident_ledger_};
      migrated.registry_validate_ = registry_validate_;
      migrated.template load_migrated<SourceRoot, SourceTraits>(source.save(),
                                                               std::forward<Callable>(converter));
      const bool same_document = document_.high == migrated.document_.high &&
                                 document_.low == migrated.document_.low;
      if (same_document && (allocated_id_ > migrated.allocated_id_ ||
                            current_->persistent_id != migrated.current_->persistent_id)) {
        throw std::invalid_argument{"Migration discards an existing document identity watermark"};
      }
      if constexpr (Mode == history_mode::tree) {
        if (same_document && history_.revision_high_water > migrated.history_.revision_high_water) {
          throw std::invalid_argument{"Migration discards an existing revision watermark"};
        }
      }
      event = prepare_publication(publication_kind::load, migrated.identities_,
                                  migrated.current_.get(), !same_document);
      migrated.create_journal(destination_path, mode, std::move(options));
      if constexpr (has_history) {
        history_.swap(migrated.history_);
      }
      resident_auxiliary_.swap(migrated.resident_auxiliary_);
      current_.swap(migrated.current_);
      snapshot_.swap(migrated.snapshot_);
      identities_.swap(migrated.identities_);
      document_ = migrated.document_;
      allocated_id_ = migrated.allocated_id_;
      if constexpr (has_collaboration) {
        collaboration_state_.identity_leases.swap(migrated.collaboration_state_.identity_leases);
      }
      journal_state_.sink.swap(migrated.journal_state_.sink);
      journal_state_.saved_snapshot.swap(migrated.journal_state_.saved_snapshot);
    });
    publish_notification(std::move(event));
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
    auto allocation = decode_allocation_checkpoint(bytes);
    auto envelope = detail::decode<envelope_type>(
        allocation.format_version == 0 ? bytes : allocation.model, options_.decode);
    std::unique_ptr<detail::identity_lease_book> leases;
    if (allocation.format_version != 0) {
      leases = std::make_unique<detail::identity_lease_book>(std::move(allocation.leases));
      leases->validate(envelope.allocated_id, std::numeric_limits<id_type>::max(),
                       options_.decode.max_collection_elements);
    }
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
      if constexpr (has_collaboration) {
        if (collaboration_state_.identity_leases) {
          const auto& current = collaboration_state_.identity_leases->entries();
          const auto* incoming = leases ? &leases->entries() : nullptr;
          if (!incoming || incoming->size() < current.size()) {
            throw std::invalid_argument{"Reload would discard native identity leases"};
          }
          for (std::size_t index = 0; index < current.size(); ++index) {
            const auto& old = current[index];
            const auto& next = (*incoming)[index];
            if (old.first != next.first || old.last != next.last ||
                old.session_format != next.session_format || old.session != next.session ||
                old.committed > next.committed) {
              throw std::invalid_argument{"Reload changes or regresses native identity lease ownership"};
            }
          }
        }
      }
      for (const auto& [id, type] : identities) {
        const auto old = identities_.find(id);
        if (old != identities_.end() && old->second != type) {
          throw std::invalid_argument{"Reload changes an existing entity's type"};
        }
      }
    }
    detail::reset_runtime_fields<Traits>(value);
    const document_id imported_document{envelope.document_high, envelope.document_low};
    validate(value, true, &imported_document);
    auto event = prepare_publication(publication_kind::load, identities, &value, !same_document);
    auto published = make_published_storage(std::move(value));
    std::size_t auxiliary_estimate{};
    if constexpr (has_history) {
      detail::history_storage<Mode, Labels> history;
      auto historical_identities = identities;
      // Both storage policies validate every historical model before replacing the live state.
      const auto validate_entry = [&](const auto& node) {
        auto historical = detail::decode<storage_type>(node.snapshot, options_.decode);
        validate(historical, false, &imported_document);
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
      auxiliary_estimate = estimate_auxiliary(envelope.current_snapshot, identities, history);
      detail::resident_admission admission{resident_auxiliary_, auxiliary_estimate};
      history_.swap(history);
      admission.commit();
    } else {
      auxiliary_estimate = estimate_auxiliary(envelope.current_snapshot, identities, history_);
      resident_auxiliary_.resize(auxiliary_estimate);
    }
    current_.swap(published);
    identities_.swap(identities);
    snapshot_.swap(envelope.current_snapshot);
    document_ = {envelope.document_high, envelope.document_low};
    allocated_id_ =
        same_document ? std::max(allocated_id_, envelope.allocated_id) : envelope.allocated_id;
    if constexpr (has_collaboration) {
      collaboration_state_.identity_leases.swap(leases);
    }
    publish_notification(std::move(event));
  }
};

template <typename Root, history_mode Mode = history_mode::linear,
          history_labels Labels = history_labels::disabled, typename Traits = model_traits<Root>,
          store_features Features = store_features::all>
using managed = model_store<Root, Mode, Labels, Traits, Features>;

#undef SERIALIZER_MANAGED_EMPTY_MEMBER

} // namespace rohit::managed
