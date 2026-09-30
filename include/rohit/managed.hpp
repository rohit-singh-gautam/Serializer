#pragma once

#include <rohit/managed_editor.hpp>
#include <rohit/managed_records.hpp>
#include <rohit/serializer.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <random>
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
enum class transaction_status { pending, committed, no_change, reverted, failed };

// Completion belongs to the caller and must outlive a manually scoped transaction.
struct transaction_outcome {
  transaction_status status{transaction_status::pending};
  std::exception_ptr error{};

  // Rethrow the recorded failure after transaction cleanup; cancellation is not failure.
  void throw_if_failed() const {
    if (error) {
      std::rethrow_exception(error);
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
Value decode(const std::vector<std::uint8_t>& bytes, serializer::decode_limits limits) {
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

  // Append atomically, dropping redo and evicting oldest states to satisfy both retention limits.
  void append(entry_type node, const store_options& options) {
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

    // Single-element deque insertion has no effects on failure. Allocate before deleting history.
    // Everything following it is nonthrowing, including the caller's publication swaps.
    static_assert(std::is_nothrow_swappable_v<entry_type>);
    entries.push_back(std::move(node));
    if (keep_count < entries.size() - 1) {
      std::swap(entries[keep_count], entries.back());
      while (entries.size() > keep_count + 1) {
        entries.pop_back();
      }
    }
    for (std::size_t index = 0; index < evict_count; ++index) {
      entries.pop_front();
    }
    cursor = entries.size() - 1;
    bytes = retained_bytes + node_bytes;
  }
};

template <history_labels Labels>
using tree_revision = std::conditional_t<Labels == history_labels::enabled,
                                         records::revision, records::unlabeled_revision>;

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
  std::uint64_t current_revision{1};
  std::uint64_t revision_high_water{1};

  // Publish a prepared tree without allocating or moving retained records.
  void swap(history_storage& other) noexcept {
    revisions.swap(other.revisions);
    std::swap(current_revision, other.current_revision);
    std::swap(revision_high_water, other.revision_high_water);
  }
};

} // namespace detail

// Single-thread-confined owning snapshot store. Adapters and callbacks must not leak mutable aliases.
template <typename Root, history_mode Mode = history_mode::linear,
          history_labels Labels = history_labels::disabled, typename Traits = model_traits<Root>>
class model_store {
public:
  using outcome_type = std::conditional_t<Mode == history_mode::tree,
                                          tree_transaction_outcome, transaction_outcome>;
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
  static constexpr bool has_history = Mode != history_mode::disabled;
  static constexpr bool has_labels = Labels == history_labels::enabled;
  static constexpr std::uint32_t format_version = has_labels ? (Mode == history_mode::tree ? 1 : 2) : 3;
  using label_type = detail::label_storage<Labels>;
  using entry_type = detail::history_entry<Labels>;
  using revision_type = detail::tree_revision<Labels>;
  using saved_entry_type = std::conditional_t<has_labels,
                                              records::history_entry, records::unlabeled_history_entry>;
  using tree_envelope_type = std::conditional_t<has_labels,
                                                records::envelope, records::unlabeled_envelope>;
  using state_envelope_type = std::conditional_t<has_labels,
                                                 records::state_envelope, records::unlabeled_state_envelope>;
  using envelope_type = std::conditional_t<Mode == history_mode::tree,
                                           tree_envelope_type, state_envelope_type>;
  using identity_table = std::map<id_type, std::string>;

  document_id document_{};
  store_options options_{};
  std::shared_ptr<const storage_type> current_{};
  std::vector<std::uint8_t> snapshot_{};
  identity_table identities_{};
  std::uint64_t allocated_id_{};
  bool writer_active_{};
  bool callback_active_{};
  const std::thread::id thread_{std::this_thread::get_id()};
  std::function<void(const storage_type&)> validate_{};
  [[no_unique_address]] detail::history_storage<Mode, Labels> history_{};

  // Reject cross-thread use before touching store state; external serialization is still required.
  void check_thread() const {
    if (thread_ != std::this_thread::get_id()) {
      throw std::logic_error{"Managed store used from another thread"};
    }
  }

  // Require a closed writer for navigation and export; callbacks cannot reenter the store.
  void require_idle() const {
    check_thread();
    if (writer_active_ || callback_active_) {
      throw std::logic_error{"Managed store already has an active operation"};
    }
  }

  // Consume an ID permanently, including on later cancellation or publication failure.
  id_type allocate_id() {
    if (allocated_id_ == std::numeric_limits<id_type>::max()) {
      throw std::overflow_error{"Managed entity ID space exhausted"};
    }
    return static_cast<id_type>(++allocated_id_);
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

  // Validate all retained encoded bytes and labels without arithmetic overflow.
  void check_history(const auto& revisions) const {
    if (revisions.size() > options_.max_revisions) {
      throw std::length_error{"Managed revision budget exceeded"};
    }
    std::size_t remaining = options_.max_history_bytes;
    for (const auto& [number, node] : revisions) {
      static_cast<void>(number);
      for (const auto size : {node->snapshot.size(), detail::label_bytes<Labels>(*node)}) {
        if (size > remaining) {
          throw std::length_error{"Managed history byte budget exceeded"};
        }
        remaining -= size;
      }
    }
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
      result.revisions.emplace(history_.current_revision, std::move(node));
      check_history(result.revisions);
    }
    return result;
  }

  // Decode and validate a retained state before publishing; failure leaves the cursor unchanged.
  void restore_snapshot(const std::vector<std::uint8_t>& bytes) {
    auto value = detail::decode<storage_type>(bytes, options_.decode);
    auto identities = inspect(value, allocated_id_);
    validate(value);
    auto published = std::make_shared<const storage_type>(std::move(value));
    auto snapshot = bytes;
    current_.swap(published);
    snapshot_.swap(snapshot);
    identities_.swap(identities);
  }

  // Navigate by deque position; the caller has checked that the index is retained.
  void checkout_linear(std::size_t index)
    requires(Mode == history_mode::linear)
  {
    const auto& node = history_.entries[index];
    restore_snapshot(node.snapshot);
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
          allocation_base_{store.allocated_id_},
          exceptions_{std::uncaught_exceptions()} {
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
          outcome_->status = transaction_status::failed;
          outcome_->error = std::move(error);
        } else {
          close(transaction_status::failed, std::move(error));
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
          if ((previous != store.identities_.end() && previous->second != type) ||
              (previous == store.identities_.end() && id <= allocation_base_)) {
            throw std::invalid_argument{"Cannot reuse or retype a managed identity"};
          }
        }
        auto snapshot = detail::encode(*candidate_, store.options_.max_snapshot_bytes);
        store.check_snapshot(snapshot);
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
            store.history_.append(std::move(entry), store.options_);
          } else if constexpr (Mode == history_mode::tree) {
            const auto revision = store.next_revision();
            auto revisions = store.history_.revisions;
            auto node = std::make_shared<revision_type>();
            node->number = revision;
            node->parent = store.history_.current_revision;
            if constexpr (has_labels) {
              node->label = this->label;
            }
            node->snapshot = snapshot;
            revisions.emplace(revision, std::move(node));
            store.check_history(revisions);
            store.history_.revisions.swap(revisions);
            store.history_.revision_high_water = revision;
            store.history_.current_revision = revision;
            outcome_->revision = revision;
          }
        }
        store.current_.swap(published);
        store.snapshot_.swap(snapshot);
        store.identities_.swap(identities);
        close(transaction_status::committed);
      } catch (...) {
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

  // Begin an unnamed action; disabled-label stores create no label storage.
  [[nodiscard]] transaction begin_transaction(outcome_type& outcome) {
    try {
      return transaction{*this, outcome};
    } catch (...) {
      outcome = {};
      outcome.status = transaction_status::failed;
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
      outcome.status = transaction_status::failed;
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
        outcome.status = transaction_status::failed;
        outcome.error = std::current_exception();
      }
    } catch (...) {
      outcome = {};
      outcome.status = transaction_status::failed;
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
      outcome.status = transaction_status::failed;
      outcome.error = std::current_exception();
      return outcome;
    }
  }

  // Copy the description of the action undo would reverse; reject an unavailable undo state.
  std::string undo_label() const
    requires(has_labels && has_history)
  {
    require_idle();
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
    const auto found = history_.revisions.find(revision);
    if (found == history_.revisions.end()) {
      throw std::out_of_range{"Managed revision is unavailable"};
    }
    restore_snapshot(found->second->snapshot);
    history_.current_revision = revision;
  }

  // Undo follows the sole parent; the baseline has no predecessor.
  void undo()
    requires(has_history)
  {
    require_idle();
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
    auto history = make_history();
    history_.swap(history);
  }

  // Export a coherent versioned memory envelope; the host owns durable file replacement.
  std::vector<std::uint8_t> save() const {
    require_idle();
    envelope_type envelope;
    envelope.format_version = format_version;
    envelope.schema_id = std::string{Traits::schema_id};
    envelope.id_bits = std::numeric_limits<id_type>::digits;
    envelope.document_high = document_.high;
    envelope.document_low = document_.low;
    envelope.allocated_id = allocated_id_;
    envelope.mode = static_cast<std::uint32_t>(Mode);
    envelope.current_snapshot = snapshot_;
    if constexpr (Mode == history_mode::linear) {
      envelope.cursor = history_.cursor;
      for (const auto& node : history_.entries) {
        saved_entry_type entry;
        if constexpr (has_labels) {
          entry.label = node.label;
        }
        entry.snapshot = node.snapshot;
        envelope.entries.push_back(std::move(entry));
      }
    } else if constexpr (Mode == history_mode::tree) {
      envelope.revision_high_water = history_.revision_high_water;
      envelope.current_revision = history_.current_revision;
      for (const auto& [number, node] : history_.revisions) {
        static_cast<void>(number);
        envelope.revisions.push_back(*node);
      }
    }
    return detail::encode(envelope, options_.decode.max_input_bytes);
  }

  // Validate the entire history before replacing this document; reject malformed or incompatible data.
  void load(const std::vector<std::uint8_t>& bytes) {
    require_idle();
    auto envelope = detail::decode<envelope_type>(bytes, options_.decode);
    if (envelope.format_version != format_version || envelope.schema_id != Traits::schema_id ||
        envelope.id_bits != std::numeric_limits<id_type>::digits ||
        (envelope.document_high == 0 && envelope.document_low == 0) ||
        envelope.allocated_id > std::numeric_limits<id_type>::max() ||
        envelope.mode != static_cast<std::uint32_t>(Mode)) {
      throw std::invalid_argument{"Incompatible managed envelope"};
    }
    if constexpr (Mode == history_mode::tree) {
      if (envelope.current_revision == 0 || envelope.current_revision > envelope.revision_high_water) {
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
        history.current_revision = envelope.current_revision;
        history.revision_high_water = same_document
            ? std::max(history_.revision_high_water, envelope.revision_high_water)
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
          history_labels Labels = history_labels::disabled, typename Traits = model_traits<Root>>
using managed = model_store<Root, Mode, Labels, Traits>;

} // namespace rohit::managed
