// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/managed.hpp>
#include <rohit/native_database_file.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace rohit::managed {

// An operation key is scoped to one persisted document and authority epoch.
struct database_operation_id {
  std::uint64_t high{};
  std::uint64_t low{};
  bool operator==(const database_operation_id&) const = default;
};

// The result remains the original revision on every exact retry, including after reopen.
struct database_outcome {
  database_operation_id operation{};
  std::uint64_t epoch{};
  std::uint64_t base_revision{};
  std::uint64_t revision{};
  bool operator==(const database_outcome&) const = default;
};

struct database_options : detail::native_database_file_options {
  std::size_t max_request_bytes{mebibyte};
  store_options model{};
  notification_options notifications{};
};

// Restore can preserve reservations exposed after an archived generation was originally accepted.
struct database_migration_options {
  std::uint64_t minimum_allocated_id{};
};

namespace detail {

// Append a fixed-width little-endian database wire integer.
inline void database_put_word(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) {
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

// Read one bounded wire integer, rejecting malformed metadata before indexing.
inline std::uint64_t database_get_word(std::span<const std::uint8_t> bytes,
                                     std::size_t& offset) {
  constexpr std::size_t word_bytes = 8;
  if (offset > bytes.size() || bytes.size() - offset < word_bytes) {
    throw std::invalid_argument{"Truncated native database metadata"};
  }
  std::uint64_t value{};
  for (unsigned shift = 0; shift < 64; shift += 8) {
    value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
  }
  return value;
}

} // namespace detail

// Bounded eager, owner-thread native database. One log owns data, identities and retry outcomes;
// model_store journals/collaboration/history cannot be attached through this API. This is not paging.
template <typename Root, typename Traits = model_traits<Root>>
class native_database {
  template <typename, typename> friend class native_database;
public:
  using store_type = model_store<Root, history_mode::disabled, history_labels::disabled,
      Traits, store_features::none>;
  using storage_type = typename store_type::storage_type;
  using validator_type = std::function<void(const storage_type&)>;

private:
  struct staged_generation {};
  using request_digest = std::array<std::uint8_t, digest_size(digest_algorithm::sha256)>;
  struct operation_record {
    database_outcome outcome{};
    request_digest digest{};
  };

public:
  // Bind one complete accepted root and its local query catalog to an exact native revision.
  // This eager snapshot pins whole-root storage; it supplies no lazy object/page loading.
  class snapshot_type {
    friend class native_database;
    std::shared_ptr<const storage_type> root_;
    registry_view<Root, Traits> registry_;
    std::uint64_t epoch_{};
    std::uint64_t revision_{};
    std::uint64_t allocated_id_{};
    // Transfer prepared immutable pins and the bounded catalog without changing accepted state.
    snapshot_type(std::shared_ptr<const storage_type> root, registry_view<Root, Traits> registry,
                  std::uint64_t epoch, std::uint64_t revision, std::uint64_t allocated_id)
        : root_{std::move(root)}, registry_{std::move(registry)}, epoch_{epoch}, revision_{revision},
          allocated_id_{allocated_id} {}
  public:
    snapshot_type(const snapshot_type&) = delete;
    snapshot_type& operator=(const snapshot_type&) = delete;
    // Transfer immutable pins, including their existing resource charges.
    snapshot_type(snapshot_type&&) noexcept = default;
    // Replace this snapshot with another pinned accepted revision.
    snapshot_type& operator=(snapshot_type&&) noexcept = default;
    // Return the complete immutable eager root; no file read or decode occurs.
    const std::shared_ptr<const storage_type>& read() const noexcept { return root_; }
    // Query only the entities belonging to this exact accepted snapshot.
    const auto& registry() const noexcept { return registry_; }
    // Inspect the namespace shared by this snapshot and its typed entity handles.
    document_id document() const noexcept { return registry_.document(); }
    // Identify the persisted authority epoch independently of the process lifetime.
    std::uint64_t epoch() const noexcept { return epoch_; }
    // Inspect this snapshot's accepted revision after the database has advanced or closed.
    std::uint64_t revision() const noexcept { return revision_; }
    // Preserve the exact accepted watermark, including deleted entities and unused reserved gaps.
    std::uint64_t allocated_id() const noexcept { return allocated_id_; }
  };

  // Move-only preparation owns every allocation needed by acceptance/publication. Destroying it
  // cancels without I/O. Candidate editors and unaccepted identities must not escape callbacks.
  class prepared_transaction {
    friend class native_database;
    const native_database* owner_{};
    std::shared_ptr<const std::uint8_t> lifetime_{};
    operation_record operation_{};
    std::unique_ptr<store_type> candidate_{};
    std::vector<operation_record> operations_{};
    std::vector<std::uint8_t> bytes_{};
    std::shared_ptr<const storage_type> before_{};
    std::shared_ptr<const storage_type> after_{};
    prepared_change_set<typename Traits::id_type> changes_{};
    bool retry_{};
    // Construct an empty preparation only inside its owning database.
    prepared_transaction() = default;
  public:
    prepared_transaction(const prepared_transaction&) = delete;
    prepared_transaction& operator=(const prepared_transaction&) = delete;
    // Transfer exclusive preparation ownership without allocation.
    prepared_transaction(prepared_transaction&&) noexcept = default;
    // Replace a cancelled preparation with another exclusively owned preparation.
    prepared_transaction& operator=(prepared_transaction&&) noexcept = default;
    // Inspect the predicted/original outcome without publishing it.
    database_outcome outcome() const noexcept { return operation_.outcome; }
    // Distinguish durable retry resolution from a newly prepared candidate.
    bool is_retry() const noexcept { return retry_; }
    // Inspect bounded immutable differences prepared before any durable append; retries are empty.
    const auto& changes() const noexcept { return changes_; }
    // Pin the accepted source used by this preparation, independently of later database publications.
    const std::shared_ptr<const storage_type>& before() const noexcept { return before_; }
    // Pin the validated candidate without exposing mutable storage; retries carry no new candidate.
    const std::shared_ptr<const storage_type>& after() const noexcept { return after_; }
  };

  // Create a new database with a native identified baseline. The parent directory must exist.
  native_database(const std::filesystem::path& path, Root initial, document_id document,
                  std::uint64_t epoch, database_options options = {}, validator_type validate = {})
      : options_{std::move(options)}, validate_{std::move(validate)}, epoch_{epoch},
        current_{std::make_unique<store_type>(std::move(initial), document, options_.model,
                                            validate_)} {
    check_options();
    const auto baseline = encode_record({}, current_->save());
    file_ = std::make_unique<detail::native_database_file>(path, true, options_);
    file_->append(baseline);
  }

  // Reopen under an exclusive lock, validating all retained native envelopes and exact sequence
  // bindings before tail repair. No edit, authentication, business or notification callbacks run.
  explicit native_database(const std::filesystem::path& path, database_options options = {},
                           validator_type validate = {})
      : options_{std::move(options)}, validate_{std::move(validate)} {
    check_options(false);
    file_ = std::make_unique<detail::native_database_file>(path, false, options_);
    const auto records = file_->recover();
    for (const auto& record : records) {
      recover_record(record);
    }
    if (!current_) {
      throw std::invalid_argument{"Native database has no committed baseline"};
    }
    file_->finish_recovery();
  }

  native_database(const native_database&) = delete;
  native_database& operator=(const native_database&) = delete;
  native_database(native_database&&) = delete;
  native_database& operator=(native_database&&) = delete;

  // Return an immutable accepted pin; existing pins remain valid across publications.
  std::shared_ptr<const storage_type> read() const { check_owner(); return current_->read(); }
  // Build a bounded local registry pinned to the proven accepted snapshot; never issue hidden I/O.
  auto registry(registry_options options = {}) const {
    check_owner();
    return current_->registry(options);
  }
  // Pin accepted revision/epoch and build a bounded local catalog on the owner thread.
  snapshot_type snapshot(registry_options options = {}) const {
    check_owner();
    return {current_->read(), current_->registry(options), epoch_, revision_, current_->allocated_id()};
  }
  // Return the current accepted revision, independently of the original revision of a retry.
  std::uint64_t revision() const { check_owner(); return revision_; }
  // Return the durable authority epoch; reopening preserves it rather than inventing a new one.
  std::uint64_t epoch() const { check_owner(); return epoch_; }
  // Inspect the durable identity watermark independently of current live entity membership.
  std::uint64_t allocated_id() const { check_owner(); return current_->allocated_id(); }
  // Return the native document namespace restored from the authoritative envelope.
  document_id document() const { check_owner(); return current_->document(); }
  // Query uncertainty; reads remain pinned to the last proven in-process accepted state.
  bool needs_recovery() const noexcept { return fenced_ || (file_ && file_->needs_recovery()); }
  // A successful migration transfers writer authority; existing source pins remain readable.
  bool retired() const { check_owner(); return retired_; }

  // Convert the current native generation under its stable writer lock. Preserve every identity
  // and stable class tag (or legacy type key); start a strictly newer epoch at revision zero
  // with an empty retry horizon. Explicit class tags permit schema and generated type renames.
  // The create-only archive retains all original outcomes. An uncertain replacement requires
  // reopening the named database; neither converter nor host validation runs during recovery.
  template <typename DestinationRoot = Root, typename DestinationTraits = model_traits<DestinationRoot>,
            typename Callable>
  auto migrate(std::uint64_t next_epoch, const std::filesystem::path& archive,
               Callable&& converter,
               typename native_database<DestinationRoot, DestinationTraits>::validator_type validate = {},
               database_migration_options migration = {}) {
    using destination_database = native_database<DestinationRoot, DestinationTraits>;
    using destination_storage = typename destination_database::storage_type;
    static_assert(std::same_as<typename Traits::id_type, typename DestinationTraits::id_type>,
                  "Native migration preserves identity width");
    require_idle();
    if (next_epoch <= epoch_) {
      throw std::invalid_argument{"Native migration requires a strictly newer authority epoch"};
    }
    if (migration.minimum_allocated_id > std::numeric_limits<typename Traits::id_type>::max()) {
      throw std::invalid_argument{"Native migration identity watermark exceeds its selected width"};
    }
    struct migration_guard {
      bool& busy;
      // Serialize conversion, validation, archival and replacement with ordinary acceptance.
      explicit migration_guard(bool& value) : busy{value} { busy = true; }
      // Restore normal admission after a rejected conversion or uncertain replacement.
      ~migration_guard() { busy = false; }
    } guard{busy_};
    auto converted = [&] {
      if constexpr (requires(const destination_storage& value) { value.serializer_payload_resource(); }) {
        serializer::payload_allocation_scope scope{options_.model.allocation_resource};
        return std::invoke(std::forward<Callable>(converter), std::as_const(*current_->read()));
      } else {
        return std::invoke(std::forward<Callable>(converter), std::as_const(*current_->read()));
      }
    }();
    auto next = std::unique_ptr<destination_database>{new destination_database{
        typename destination_database::staged_generation{}, std::move(converted), document(),
        std::max(current_->allocated_id(), migration.minimum_allocated_id), next_epoch,
        options_, std::move(validate)}};
    const auto previous = current_->registry({options_.model.decode.max_collection_elements,
                                             options_.model.decode.max_allocation_bytes});
    std::size_t count{};
    detail::visit_registry_entities<DestinationTraits>(*next->read(),
        [&]<typename Plain>(const auto& entity) {
      const auto id = entity.persistent_id;
      constexpr auto destination_class_id = detail::registry_class_id<Plain>();
      if (count >= previous.size() || !previous.contains(id)) {
        throw std::invalid_argument{"Native migration changes entity identities"};
      }
      const auto source_class_id = previous.class_id(id);
      // Source spellings may change only when both schemas explicitly retain the same class tag.
      const bool same_type = source_class_id != 0 && destination_class_id != 0
          ? source_class_id == destination_class_id
          : previous.type(id) == model_traits<Plain>::type_key;
      if (!same_type) {
        throw std::invalid_argument{"Native migration changes entity class identities"};
      }
      ++count;
    });
    if (count != previous.size()) {
      throw std::invalid_argument{"Native migration removes managed identities"};
    }
    const auto baseline = next->encode_record({}, next->current_->save());
    try {
      file_->rebaseline(baseline, archive);
    } catch (...) {
      fenced_ = file_->needs_recovery();
      throw;
    }
    // Destination resources, model validation and encoded metadata all precede atomic replacement.
    next->file_ = std::move(file_);
    retired_ = true;
    return next;
  }

  // Validate one complete isolated managed candidate at the exact base. Exact retries return
  // before invoking callback. Request bytes must canonically bind every host command parameter.
  template <typename Callable>
  prepared_transaction prepare(database_operation_id operation, std::uint64_t epoch,
                               std::uint64_t expected_revision,
                               std::span<const std::uint8_t> request, Callable&& callback) {
    require_idle();
    if ((operation.high == 0 && operation.low == 0) || epoch != epoch_) {
      throw std::invalid_argument{"Invalid database operation identity or epoch"};
    }
    if (request.size() > options_.max_request_bytes) {
      throw std::length_error{"Native database request budget exceeded"};
    }
    prepared_transaction prepared;
    prepared.owner_ = this;
    prepared.lifetime_ = lifetime_;
    prepared.operation_.digest = make_digest<digest_algorithm::sha256>(request);
    const auto existing = find_operation(operation);
    if (existing != operations_.end()) {
      if (existing->digest != prepared.operation_.digest ||
          existing->outcome.base_revision != expected_revision) {
        throw std::invalid_argument{"Database operation ID reused with a different request"};
      }
      prepared.operation_ = *existing;
      prepared.retry_ = true;
      return prepared;
    }
    if (expected_revision != revision_) {
      throw std::invalid_argument{"Stale native database acceptance base"};
    }
    if (revision_ == std::numeric_limits<std::uint64_t>::max() ||
        operations_.size() >= options_.max_operations) {
      throw std::length_error{"Native database operation retention budget exceeded"};
    }
    struct busy_guard {
      bool& busy;
      // Fence recursive database calls throughout validation and the edit callback.
      explicit busy_guard(bool& value) : busy{value} { busy = true; }
      // Release the synchronous callback barrier on every exit path.
      ~busy_guard() { busy = false; }
    } guard{busy_};
    const auto make_candidate = [&] {
      return std::make_unique<store_type>(identified_baseline,
          *current_->read(), current_->document(), current_->allocated_id(), options_.model, validate_);
    };
    if constexpr (requires(const storage_type& value) { value.serializer_payload_resource(); }) {
      // The by-value identified-baseline argument is itself a library-owned payload copy.
      serializer::payload_allocation_scope scope{options_.model.allocation_resource};
      prepared.candidate_ = make_candidate();
    } else {
      prepared.candidate_ = make_candidate();
    }
    const auto outcome = prepared.candidate_->execute_transaction(std::forward<Callable>(callback));
    outcome.throw_if_failed();
    if (outcome.status != transaction_status::committed &&
        outcome.status != transaction_status::no_change) {
      throw std::invalid_argument{"Native database preparation was cancelled"};
    }
    prepared.operation_.outcome = {operation, epoch_, revision_, revision_ + 1};
    prepared.before_ = current_->read();
    prepared.after_ = prepared.candidate_->read();
    prepared.changes_ = detail::prepare_change_set<Traits>(prepared.before_.get(),
        *prepared.after_, options_.model.decode.max_collection_elements,
        options_.model.max_snapshot_bytes, options_.notifications);
    prepared.operations_ = operations_;
    prepared.operations_.push_back(prepared.operation_);
    prepared.bytes_ = encode_record(prepared.operation_, prepared.candidate_->save());
    return prepared;
  }

  // Persist one already validated candidate. After proven flush, publication only swaps owning
  // pointers/vectors and assigns integers: no allocation, user callback or throwing work occurs.
  database_outcome accept(prepared_transaction&& prepared) {
    require_idle();
    if (prepared.owner_ != this || prepared.lifetime_ != lifetime_) {
      throw std::invalid_argument{"Preparation belongs to another native database"};
    }
    if (prepared.retry_) {
      prepared.owner_ = nullptr;
      return prepared.operation_.outcome;
    }
    const auto existing = find_operation(prepared.operation_.outcome.operation);
    if (existing != operations_.end()) {
      if (existing->digest != prepared.operation_.digest ||
          existing->outcome != prepared.operation_.outcome) {
        throw std::invalid_argument{"Database operation ID reused with a different preparation"};
      }
      prepared.owner_ = nullptr;
      return existing->outcome;
    }
    if (!prepared.candidate_ || prepared.operation_.outcome.base_revision != revision_) {
      throw std::invalid_argument{"Native database preparation is consumed or stale"};
    }
    struct acceptance_guard {
      bool& busy;
      // Block reentrant preparation/acceptance throughout I/O and test instrumentation.
      explicit acceptance_guard(bool& value) : busy{value} { busy = true; }
      // Restore owner-thread admission after success or a rejected/uncertain append.
      ~acceptance_guard() { busy = false; }
    } guard{busy_};
    bool durable{};
    try {
      file_->append(prepared.bytes_);
      durable = true;
      if (options_.fault_injector) {
        options_.fault_injector(database_io_event::before_publish);
      }
    } catch (...) {
      fenced_ = durable || file_->needs_recovery();
      if (durable) {
        std::throw_with_nested(journal_indeterminate_error{
            "Native database accepted state requires recovery before publication"});
      }
      throw;
    }
    publish(prepared);
    return prepared.operation_.outcome;
  }

private:
  // Build an identified destination completely in memory before changing its durable generation.
  native_database(staged_generation, storage_type value, document_id document,
                  std::uint64_t allocated_id, std::uint64_t epoch,
                  database_options options, validator_type validate)
      : options_{std::move(options)}, validate_{std::move(validate)}, epoch_{epoch},
        current_{std::make_unique<store_type>(identified_baseline, std::move(value), document,
                                            allocated_id, options_.model, validate_)} {
    check_options();
  }

  // Require the owner thread even for immutable metadata access.
  void check_owner() const {
    if (owner_ != std::this_thread::get_id()) {
      throw std::logic_error{"Native database accessed from another thread"};
    }
  }
  // Reject callbacks, incompatible writes and any operation after an uncertain acceptance.
  void require_idle() const {
    check_owner();
    if (busy_) {
      throw std::logic_error{"Reentrant native database operation"};
    }
    if (retired_) {
      throw std::logic_error{"Native database writer was retired by migration"};
    }
    if (needs_recovery()) {
      throw journal_indeterminate_error{"Native database requires reopen before mutation"};
    }
  }
  // Validate bounded-profile configuration independently of native file I/O.
  void check_options(bool creating = true) const {
    if ((creating && epoch_ == 0) || options_.max_record_bytes == 0 ||
        options_.max_file_bytes < options_.max_record_bytes || options_.max_operations == 0) {
      throw std::invalid_argument{"Invalid native database options"};
    }
  }
  // Locate a retained operation without changing or expiring the deduplication horizon.
  auto find_operation(database_operation_id operation) const {
    return std::find_if(operations_.begin(), operations_.end(), [&](const auto& entry) {
      return entry.outcome.operation == operation;
    });
  }
  // Encode namespace/schema-bound managed bytes plus the accepted operation in one durable frame.
  std::vector<std::uint8_t> encode_record(const operation_record& operation,
                                        const std::vector<std::uint8_t>& envelope) const {
    constexpr std::uint64_t format_version = 1;
    constexpr std::size_t metadata_bytes = 8 * 7 + digest_size(digest_algorithm::sha256);
    if (envelope.size() > options_.max_record_bytes ||
        options_.max_record_bytes - envelope.size() < metadata_bytes) {
      throw std::length_error{"Native database record budget exceeded"};
    }
    std::vector<std::uint8_t> result;
    result.reserve(metadata_bytes + envelope.size());
    detail::database_put_word(result, format_version);
    detail::database_put_word(result, epoch_);
    detail::database_put_word(result, operation.outcome.operation.high);
    detail::database_put_word(result, operation.outcome.operation.low);
    detail::database_put_word(result, operation.outcome.base_revision);
    detail::database_put_word(result, operation.outcome.revision);
    detail::database_put_word(result, envelope.size());
    result.insert(result.end(), operation.digest.begin(), operation.digest.end());
    result.insert(result.end(), envelope.begin(), envelope.end());
    return result;
  }
  // Recover every record through real managed decoding, identity and reference validation.
  void recover_record(std::span<const std::uint8_t> bytes) {
    std::size_t offset{};
    if (detail::database_get_word(bytes, offset) != 1) {
      throw std::invalid_argument{"Unsupported native database record version"};
    }
    const auto epoch = detail::database_get_word(bytes, offset);
    operation_record operation;
    operation.outcome.epoch = epoch;
    operation.outcome.operation.high = detail::database_get_word(bytes, offset);
    operation.outcome.operation.low = detail::database_get_word(bytes, offset);
    operation.outcome.base_revision = detail::database_get_word(bytes, offset);
    operation.outcome.revision = detail::database_get_word(bytes, offset);
    const auto envelope_bytes = detail::database_get_word(bytes, offset);
    if (bytes.size() - offset < operation.digest.size()) {
      throw std::invalid_argument{"Truncated native database request digest"};
    }
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), operation.digest.size(),
                operation.digest.begin());
    offset += operation.digest.size();
    if (epoch == 0 || envelope_bytes != bytes.size() - offset) {
      throw std::invalid_argument{"Invalid native database record extent or epoch"};
    }
    if (!current_) {
      if (operation.outcome.operation != database_operation_id{} ||
          operation.outcome.base_revision != 0 || operation.outcome.revision != 0 ||
          operation.digest != request_digest{}) {
        throw std::invalid_argument{"Invalid native database baseline"};
      }
      epoch_ = epoch;
    } else if (epoch != epoch_ || operation.outcome.operation == database_operation_id{} ||
               operation.outcome.base_revision != revision_ ||
               revision_ == std::numeric_limits<std::uint64_t>::max() ||
               operation.outcome.revision != revision_ + 1 ||
               find_operation(operation.outcome.operation) != operations_.end() ||
               operations_.size() >= options_.max_operations) {
      throw std::invalid_argument{"Invalid native database operation sequence"};
    }
    // Recovery performs native schema validation only. Host policy is re-established before edits.
    const auto envelope = detail::decode<records::unlabeled_state_envelope>(
        bytes.subspan(offset), options_.model.decode);
    if (envelope.format_version != 3 || envelope.schema_id != Traits::schema_id ||
        envelope.id_bits != std::numeric_limits<typename Traits::id_type>::digits ||
        envelope.mode != static_cast<std::uint32_t>(history_mode::disabled) ||
        !envelope.entries.empty() || envelope.cursor != 0) {
      throw std::invalid_argument{"Incompatible native database managed envelope"};
    }
    const auto decode_snapshot = [&] {
      return detail::decode<storage_type>(envelope.current_snapshot, options_.model.decode);
    };
    auto storage = [&] {
      if constexpr (requires(const storage_type& value) { value.serializer_payload_resource(); }) {
        serializer::payload_allocation_scope scope{options_.model.allocation_resource};
        return decode_snapshot();
      } else {
        return decode_snapshot();
      }
    }();
    auto restored = std::make_unique<store_type>(identified_baseline, std::move(storage),
        document_id{envelope.document_high, envelope.document_low}, envelope.allocated_id,
        options_.model);
    if (current_ && (restored->document() != current_->document() ||
                     restored->allocated_id() < current_->allocated_id())) {
      throw std::invalid_argument{"Native database namespace or identity watermark changed"};
    }
    if (current_) {
      // A checksummed envelope must still obey transition identity rules: a deleted ID cannot
      // be reborn below the old high-water mark, and a surviving ID cannot change exact type.
      const auto previous = current_->registry({options_.model.decode.max_collection_elements,
                                               options_.model.decode.max_allocation_bytes});
      const auto allocation_base = current_->allocated_id();
      detail::visit_registry_entities<Traits>(*restored->read(), [&]<typename Plain>(const auto& entity) {
        if (entity.persistent_id <= allocation_base &&
            (!previous.contains(entity.persistent_id) ||
             previous.type(entity.persistent_id) != model_traits<Plain>::type_key)) {
          throw std::invalid_argument{"Native database recovery reuses or retypes an identity"};
        }
      });
      operations_.push_back(operation);
    }
    revision_ = operation.outcome.revision;
    current_.swap(restored);
  }
  // Perform the allocation-free, callback-free installation after durable acceptance.
  void publish(prepared_transaction& prepared) noexcept {
    current_.swap(prepared.candidate_);
    operations_.swap(prepared.operations_);
    revision_ = prepared.operation_.outcome.revision;
    prepared.owner_ = nullptr;
  }

  database_options options_{};
  validator_type validate_{};
  std::uint64_t epoch_{};
  std::uint64_t revision_{};
  std::unique_ptr<store_type> current_{};
  std::vector<operation_record> operations_{};
  std::unique_ptr<detail::native_database_file> file_{};
  std::shared_ptr<const std::uint8_t> lifetime_{std::make_shared<const std::uint8_t>(std::uint8_t{})};
  std::thread::id owner_{std::this_thread::get_id()};
  bool busy_{};
  bool fenced_{};
  bool retired_{};
};

} // namespace rohit::managed
