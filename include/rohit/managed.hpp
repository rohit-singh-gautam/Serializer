#pragma once

#include <rohit/managed_editor.hpp>
#include <rohit/managed_records.hpp>
#include <rohit/serializer.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
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

enum class supported_mechanism { none, history };
// Values are part of the version-one envelope contract.
enum class history_mode : std::uint32_t { disabled = 0, linear = 1, tree = 2 };
enum class transaction_status { pending, committed, no_change, reverted, failed };

// Completion belongs to the caller and must outlive a manually scoped transaction.
struct transaction_outcome {
  transaction_status status{transaction_status::pending};
  std::uint64_t revision{};
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
  history_mode mode{history_mode::linear};
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

template <bool Enabled>
struct history_storage {};

template <>
struct history_storage<true> {
  std::map<std::uint64_t, std::shared_ptr<const records::revision>> revisions{};
};

} // namespace detail

// Single-thread-confined owning snapshot store. Adapters and callbacks must not leak mutable aliases.
template <typename Root, supported_mechanism Support = supported_mechanism::history,
          typename Traits = model_traits<Root>>
class model_store {
public:
  using storage_type = typename Traits::storage_type;
  using id_type = typename Traits::id_type;
  static_assert(std::same_as<id_type, std::uint32_t> || std::same_as<id_type, std::uint64_t>);
  static_assert(std::same_as<decltype(storage_type{}.persistent_id), id_type>);
  static_assert(Support == supported_mechanism::none || Support == supported_mechanism::history);

private:
  static constexpr bool has_history = Support == supported_mechanism::history;
  static constexpr std::uint32_t format_version = 1;
  using identity_table = std::map<id_type, std::string>;

  document_id document_{};
  store_options options_{};
  std::shared_ptr<const storage_type> current_{};
  std::vector<std::uint8_t> snapshot_{};
  identity_table identities_{};
  std::uint64_t allocated_id_{};
  std::uint64_t revision_high_water_{};
  std::uint64_t current_revision_{};
  bool writer_active_{};
  bool callback_active_{};
  const std::thread::id thread_{std::this_thread::get_id()};
  std::function<void(const storage_type&)> validate_{};
  [[no_unique_address]] detail::history_storage<has_history> history_{};

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
  std::uint64_t next_revision() const {
    if (revision_high_water_ == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error{"Managed revision ID space exhausted"};
    }
    return revision_high_water_ + 1;
  }

  // Validate all retained encoded bytes and labels without arithmetic overflow.
  void check_history(const auto& revisions) const {
    if (revisions.size() > options_.max_revisions) {
      throw std::length_error{"Managed revision budget exceeded"};
    }
    std::size_t remaining = options_.max_history_bytes;
    for (const auto& [number, node] : revisions) {
      static_cast<void>(number);
      for (const auto size : {node->snapshot.size(), node->label.size()}) {
        if (size > remaining) {
          throw std::length_error{"Managed history byte budget exceeded"};
        }
        remaining -= size;
      }
    }
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
  class transaction {
    model_store* store_{};
    transaction_outcome* outcome_{};
    std::unique_ptr<storage_type> candidate_{};
    std::string label_{};
    std::uint64_t allocation_base_{};
    int exceptions_{};
    bool editing_{};
    std::shared_ptr<detail::edit_channel<storage_type, id_type>> channel_{};
    friend class model_store;

    // Acquire the sole writer only after candidate construction has succeeded.
    transaction(model_store& store, std::string_view label, transaction_outcome& outcome)
        : store_{&store}, outcome_{&outcome}, label_{label}, allocation_base_{store.allocated_id_},
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
    transaction(transaction&& other) : label_{} {
      other.require_active();
      if (other.editing_) {
        other.reject("Cannot move a transaction during an edit");
      }
      store_ = std::exchange(other.store_, nullptr);
      outcome_ = other.outcome_;
      candidate_ = std::move(other.candidate_);
      label_ = std::move(other.label_);
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
          outcome_->revision = store.current_revision_;
          close(transaction_status::no_change);
          return;
        }
        auto published = std::shared_ptr<const storage_type>{std::move(candidate_)};
        const auto revision = store.next_revision();
        if constexpr (has_history) {
          auto revisions = store.history_.revisions;
          if (store.options_.mode == history_mode::disabled) {
            revisions.clear();
          } else {
            if (store.options_.mode == history_mode::linear) {
              std::erase_if(revisions, [&](const auto& entry) {
                return entry.first > store.current_revision_;
              });
            }
            auto node = std::make_shared<records::revision>();
            node->number = revision;
            node->parent = store.current_revision_;
            node->label = label_;
            node->snapshot = snapshot;
            revisions.emplace(revision, std::move(node));
          }
          store.check_history(revisions);
          store.history_.revisions.swap(revisions);
        }
        store.current_.swap(published);
        store.snapshot_.swap(snapshot);
        store.identities_.swap(identities);
        store.revision_high_water_ = revision;
        store.current_revision_ = revision;
        outcome_->revision = revision;
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
    if (options.mode > history_mode::tree ||
        (!has_history && options.mode != history_mode::disabled)) {
      throw std::invalid_argument{"Unsupported managed history mode"};
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
    current_revision_ = revision_high_water_ = 1;
    if constexpr (has_history) {
      if (options_.mode != history_mode::disabled) {
        auto node = std::make_shared<records::revision>();
        node->number = current_revision_;
        node->snapshot = snapshot_;
        history_.revisions.emplace(current_revision_, std::move(node));
        check_history(history_.revisions);
      }
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

  // Begin an explicit or automatically completed scope; failures leave the writer slot available.
  [[nodiscard]] transaction begin_transaction(std::string_view label,
                                              transaction_outcome& outcome) {
    try {
      return transaction{*this, label, outcome};
    } catch (...) {
      outcome = {transaction_status::failed, 0, std::current_exception()};
      throw;
    }
  }

  // Execute exactly once, then complete before returning; callback errors become recorded failures.
  template <typename Callable>
    requires std::same_as<std::invoke_result_t<Callable, transaction_edit&>, void>
  [[nodiscard]] transaction_outcome execute_transaction(std::string_view label,
                                                        Callable&& callback) {
    transaction_outcome outcome;
    try {
      auto scope = begin_transaction(label, outcome);
      transaction_edit edit{scope};
      try {
        std::invoke(std::forward<Callable>(callback), edit);
      } catch (...) {
        scope.fail(std::current_exception());
        outcome.status = transaction_status::failed;
        outcome.error = std::current_exception();
      }
    } catch (...) {
      outcome = {transaction_status::failed, 0, std::current_exception()};
    }
    return outcome;
  }

  // Return direct redo alternatives in deterministic revision order; the caller chooses a branch.
  std::vector<std::uint64_t> redo_children() const
    requires(has_history)
  {
    require_idle();
    std::vector<std::uint64_t> result;
    for (const auto& [id, node] : history_.revisions) {
      if (node->parent == current_revision_) {
        result.push_back(id);
      }
    }
    return result;
  }

  // Restore one retained revision atomically after decoding and current application validation.
  void checkout(std::uint64_t revision)
    requires(has_history)
  {
    require_idle();
    const auto found = history_.revisions.find(revision);
    if (found == history_.revisions.end()) {
      throw std::out_of_range{"Managed revision is unavailable"};
    }
    auto value = detail::decode<storage_type>(found->second->snapshot, options_.decode);
    auto identities = inspect(value, allocated_id_);
    validate(value);
    auto published = std::make_shared<const storage_type>(std::move(value));
    auto snapshot = found->second->snapshot;
    current_.swap(published);
    snapshot_.swap(snapshot);
    identities_.swap(identities);
    current_revision_ = revision;
  }

  // Undo follows the sole parent; the baseline has no predecessor.
  void undo()
    requires(has_history)
  {
    require_idle();
    const auto found = history_.revisions.find(current_revision_);
    if (found == history_.revisions.end() || found->second->parent == 0) {
      throw std::out_of_range{"No managed undo revision"};
    }
    checkout(found->second->parent);
  }

  // Redo only to a direct child, never silently choose among competing branches.
  void redo(std::uint64_t revision)
    requires(has_history)
  {
    require_idle();
    const auto found = history_.revisions.find(revision);
    if (found == history_.revisions.end() || found->second->parent != current_revision_) {
      throw std::out_of_range{"Revision is not a redo child"};
    }
    checkout(revision);
  }

  // Explicitly discard retained history and start a baseline in the selected runtime mode.
  void reset_history(history_mode mode)
    requires(has_history)
  {
    require_idle();
    if (mode > history_mode::tree) {
      throw std::invalid_argument{"Unsupported managed history mode"};
    }
    decltype(history_.revisions) revisions;
    if (mode != history_mode::disabled) {
      auto node = std::make_shared<records::revision>();
      node->number = current_revision_;
      node->snapshot = snapshot_;
      revisions.emplace(current_revision_, std::move(node));
      check_history(revisions);
    }
    history_.revisions.swap(revisions);
    options_.mode = mode;
  }

  // Export a coherent versioned memory envelope; the host owns durable file replacement.
  std::vector<std::uint8_t> save() const {
    require_idle();
    records::envelope envelope;
    envelope.format_version = format_version;
    envelope.schema_id = std::string{Traits::schema_id};
    envelope.id_bits = std::numeric_limits<id_type>::digits;
    envelope.document_high = document_.high;
    envelope.document_low = document_.low;
    envelope.allocated_id = allocated_id_;
    envelope.revision_high_water = revision_high_water_;
    envelope.current_revision = current_revision_;
    envelope.mode = static_cast<std::uint32_t>(options_.mode);
    envelope.current_snapshot = snapshot_;
    if constexpr (has_history) {
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
    auto envelope = detail::decode<records::envelope>(bytes, options_.decode);
    if (envelope.format_version != format_version || envelope.schema_id != Traits::schema_id ||
        envelope.id_bits != std::numeric_limits<id_type>::digits ||
        (envelope.document_high == 0 && envelope.document_low == 0) ||
        envelope.allocated_id > std::numeric_limits<id_type>::max() ||
        envelope.mode != static_cast<std::uint32_t>(options_.mode) ||
        envelope.current_revision == 0 ||
        envelope.current_revision > envelope.revision_high_water) {
      throw std::invalid_argument{"Incompatible managed envelope"};
    }
    if (envelope.revisions.size() > options_.max_revisions) {
      throw std::length_error{"Managed revision budget exceeded"};
    }
    std::size_t remaining_history_bytes = options_.max_history_bytes;
    for (const auto& node : envelope.revisions) {
      check_snapshot(node.snapshot);
      for (const auto size : {node.snapshot.size(), node.label.size()}) {
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
      decltype(history_.revisions) revisions;
      auto historical_identities = identities;
      std::size_t roots = 0;
      for (auto& node : envelope.revisions) {
        if (node.number == 0 || node.number > envelope.revision_high_water ||
            node.parent >= node.number || revisions.contains(node.number)) {
          throw std::invalid_argument{"Invalid managed revision graph"};
        }
        roots += node.parent == 0 ? 1 : 0;
        check_snapshot(node.snapshot);
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
        auto entry = std::make_shared<const records::revision>(std::move(node));
        revisions.emplace(entry->number, std::move(entry));
      }
      check_history(revisions);
      std::map<std::uint64_t, std::size_t> child_counts;
      for (const auto& [number, node] : revisions) {
        static_cast<void>(number);
        if (node->parent != 0 && !revisions.contains(node->parent)) {
          throw std::invalid_argument{"Missing managed revision parent"};
        }
        if (node->parent != 0 && ++child_counts[node->parent] > 1 &&
            options_.mode == history_mode::linear) {
          throw std::invalid_argument{"Branching history in a linear store"};
        }
      }
      if (options_.mode == history_mode::disabled) {
        if (!revisions.empty()) {
          throw std::invalid_argument{"Disabled history contains revisions"};
        }
      } else if (roots != 1 || !revisions.contains(envelope.current_revision) ||
                 revisions.at(envelope.current_revision)->snapshot != envelope.current_snapshot) {
        throw std::invalid_argument{"Invalid managed history cursor"};
      }
      history_.revisions.swap(revisions);
    } else if (!envelope.revisions.empty()) {
      throw std::invalid_argument{"History is unsupported by this store"};
    }
    current_.swap(published);
    identities_.swap(identities);
    snapshot_.swap(envelope.current_snapshot);
    document_ = {envelope.document_high, envelope.document_low};
    allocated_id_ =
        same_document ? std::max(allocated_id_, envelope.allocated_id) : envelope.allocated_id;
    revision_high_water_ = same_document
                               ? std::max(revision_high_water_, envelope.revision_high_water)
                               : envelope.revision_high_water;
    current_revision_ = envelope.current_revision;
  }
};

template <typename Root, supported_mechanism Support = supported_mechanism::history,
          typename Traits = model_traits<Root>>
using managed = model_store<Root, Support, Traits>;

} // namespace rohit::managed
