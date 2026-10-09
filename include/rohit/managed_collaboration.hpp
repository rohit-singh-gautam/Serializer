// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/collaboration_history.hpp>
#include <rohit/collaboration_records.hpp>
#include <rohit/collaboration_sessions.hpp>
#include <rohit/managed.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed {

namespace collaboration = collaboration_records;
inline constexpr std::uint32_t collaboration_protocol_version =
    collaboration_record_types<>::command_protocol;
inline constexpr std::uint32_t collaboration_model_protocol_version =
    collaboration_record_types<>::model_protocol;
inline constexpr std::uint32_t collaboration_model_change_version = 1;
inline constexpr std::uint32_t collaboration_history_change_version = 2;

enum class collaboration_status {
  accepted,
  no_change,
  conflict,
  denied,
  obsolete_grant,
  invalid,
  reverted,
  failed,
  indeterminate
};
enum class edit_lock_scope : std::uint32_t { entity = 1, owned_subtree = 2 };
enum class edit_lock_action : std::uint32_t { acquire = 1, renew = 2, release = 3 };
enum class lock_update_action : std::uint32_t { grant = 1, release = 2, expire = 3, revoke = 4 };
enum class presence_action : std::uint32_t { update = 1, end = 2 };
enum class replication_status { applied, duplicate, resync_required };

struct collaboration_options {
  std::size_t max_sessions{1024};
  std::size_t max_operations{4096};
  std::size_t max_retained_bytes{256 * serializer::decode_limits::mebibyte};
  std::size_t max_command_bytes{serializer::decode_limits::mebibyte};
  std::size_t max_entities{100000};
  std::size_t max_locks{4096};
  std::size_t max_lock_updates{16384};
  std::size_t max_presence{4096};
  std::size_t max_preview_bytes{4096};
  std::uint64_t max_lease_ms{60000};
  std::uint64_t presence_timeout_ms{15000};
  bool require_edit_lock{false};
  std::size_t max_created_ids{4096};
  std::size_t max_tracked_fields{100000};
};

template <typename Session = std::uint64_t,
          typename SessionTraits = collaboration_session_traits<Session>>
struct basic_collaboration_result {
  using records = collaboration_record_types<Session, SessionTraits>;
  collaboration_status status{collaboration_status::failed};
  std::shared_ptr<const typename records::accepted_change> accepted{};
  typename records::lock_grant grant{};
  std::exception_ptr error{};
};
using collaboration_result = basic_collaboration_result<>;

// Encode a generated command/envelope with a bounded keyed binary message contract.
template <typename Record>
std::vector<std::uint8_t>
encode_collaboration_record(const Record& value,
                            std::size_t max_bytes = store_options::default_snapshot_bytes) {
  if constexpr (requires { typename Record::session_traits; }) {
    using policy = typename Record::session_traits;
    collaboration::session_envelope envelope{
        std::string{policy::wire_name}, detail::encode(value.wire(), max_bytes), {}};
    if (envelope.session_format.empty()) {
      throw std::invalid_argument{"Session wire format must have a stable name"};
    }
    std::size_t remaining = max_bytes;
    for (const auto& session : value.session_values()) {
      auto bytes = policy::encode(session);
      if (bytes.size() > policy::max_encoded_bytes || bytes.size() > remaining) {
        throw std::length_error{"Collaboration session encoding budget exceeded"};
      }
      remaining -= bytes.size();
      envelope.sessions.push_back({std::move(bytes)});
    }
    return detail::encode(envelope, max_bytes);
  } else {
    return detail::encode(value, max_bytes);
  }
}

// Decode a fresh generated record with exact consumption and explicit resource limits.
template <typename Record>
Record decode_collaboration_record(std::span<const std::uint8_t> bytes,
                                   serializer::decode_limits limits = {}) {
  if constexpr (requires { typename Record::session_traits; }) {
    using policy = typename Record::session_traits;
    auto envelope = detail::decode<collaboration::session_envelope>(bytes, limits);
    if (envelope.session_format.empty() || envelope.session_format != policy::wire_name) {
      throw std::invalid_argument{"Incompatible collaboration session format"};
    }
    using session_type = decltype(policy::decode(bytes, limits));
    std::vector<session_type> sessions;
    sessions.reserve(envelope.sessions.size());
    for (const auto& value : envelope.sessions) {
      if (value.bytes.size() > policy::max_encoded_bytes) {
        throw std::length_error{"Collaboration session decoding budget exceeded"};
      }
      auto session = policy::decode(value.bytes, limits);
      if (policy::encode(session) != value.bytes) {
        throw std::invalid_argument{"Noncanonical collaboration session encoding"};
      }
      sessions.push_back(std::move(session));
    }
    return Record::from_wire(detail::decode<typename Record::wire_type>(envelope.payload, limits),
                             std::move(sessions));
  } else {
    return detail::decode<Record>(bytes, limits);
  }
}

namespace detail {

// Bind opaque application commands separately from the built-in model payload protocol.
template <typename Records>
inline bool valid_collaboration_protocol(std::uint32_t version) {
  return version == Records::command_protocol || version == Records::model_protocol;
}

// Validate scope codes at typed/wire boundaries without accepting unknown future semantics.
inline bool valid_collaboration_scope(std::uint32_t scope) {
  return scope == static_cast<std::uint32_t>(edit_lock_scope::entity) ||
         scope == static_cast<std::uint32_t>(edit_lock_scope::owned_subtree);
}

// Compare the entire protocol binding, including the host-provided authority lifetime.
inline bool same_domain(const collaboration::domain& left, const collaboration::domain& right) {
  return left.protocol_version == right.protocol_version && left.schema_id == right.schema_id &&
         left.id_bits == right.id_bits && left.document_high == right.document_high &&
         left.document_low == right.document_low && left.epoch == right.epoch;
}

// Refuse exhausted counters before a state transition; zero remains the initial sequence.
inline std::uint64_t next_collaboration_sequence(std::uint64_t current) {
  if (current == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error{"Collaboration sequence exhausted; start a new fenced epoch"};
  }
  return current + 1;
}

// Validate a host monotonic duration before constructing a local deadline.
inline std::uint64_t collaboration_deadline(std::uint64_t now, std::uint64_t duration) {
  if (duration == 0 || duration > std::numeric_limits<std::uint64_t>::max() - now) {
    throw std::invalid_argument{"Invalid collaboration duration"};
  }
  return now + duration;
}

struct collaboration_entity {
  std::uint64_t parent{};
  std::vector<std::uint8_t> location{};
  std::vector<std::uint8_t> values{};
};
using collaboration_entities = std::map<std::uint64_t, collaboration_entity>;

// Generated traversal encodes each ordinary value once; independently managed children supply IDs.
class collaboration_collector {
  std::size_t max_entities_{};
  std::size_t remaining_bytes_{};
  stream_limits limits_{};
  full_stream_auto_alloc_limits stream_;
  collaboration_entity* current_{};
  std::vector<std::uint8_t> edge_{};

public:
  collaboration_entities entities{};

  // Bound both the entity table and aggregate value bytes, reusing one scratch stream.
  collaboration_collector(std::size_t max_entities, std::size_t max_bytes)
      : max_entities_{max_entities}, remaining_bytes_{max_bytes},
        limits_{std::min(stream_limits{}.min_read_buffer_bytes, max_bytes), max_bytes},
        stream_{&limits_} {}

  // Require preorder ownership and unique, nonzero persistent identities.
  void begin_entity(std::uint64_t id, std::uint64_t parent) {
    if (id == 0 || entities.size() >= max_entities_ ||
        (parent != 0 && !entities.contains(parent))) {
      throw std::invalid_argument{"Invalid or over-budget collaboration ownership"};
    }
    const auto [entry, inserted] =
        entities.try_emplace(id, collaboration_entity{parent, std::move(edge_), {}});
    if (!inserted) {
      throw std::invalid_argument{"Duplicate collaboration identity"};
    }
    current_ = &entry->second;
    stream_.reset();
  }

  // Identify ownership within a parent by stable field ID and collection key/position.
  void edge(std::uint32_t field, const auto& position) {
    stream_.reset();
    value(field);
    value(position);
    const auto size = stream_.current_offset();
    if (size > remaining_bytes_) {
      throw std::length_error{"Collaboration ownership byte budget exceeded"};
    }
    edge_.assign(stream_.begin(), stream_.begin() + size);
    remaining_bytes_ -= size;
  }

  // Encode one schema-ordered value or ownership edge with the existing binary codec.
  void value(const auto& input) {
    serializer::binary_none<serializer::serialize_type::out, full_stream_auto_alloc_limits> encoder{
        stream_};
    encoder.struct_serialize_out(input);
  }

  // Retain only entity-local bytes; no descendant payload is copied into its ancestors.
  void end_entity() {
    const auto size = stream_.current_offset();
    if (size > remaining_bytes_) {
      throw std::length_error{"Collaboration entity byte budget exceeded"};
    }
    current_->values.assign(stream_.begin(), stream_.begin() + size);
    remaining_bytes_ -= size;
  }
};

// Determine coverage using ownership, never arbitrary application references.
inline bool collaboration_contains(const collaboration_entities& entities, std::uint64_t ancestor,
                                   std::uint64_t target) {
  while (target != 0) {
    if (target == ancestor) {
      return true;
    }
    const auto found = entities.find(target);
    if (found == entities.end()) {
      return false;
    }
    target = found->second.parent;
  }
  return false;
}

// Moving an ancestor affects its complete subtree, even where local field bytes did not change.
inline bool same_collaboration_ancestry(const collaboration_entities& before,
                                        const collaboration_entities& after, std::uint64_t id) {
  while (id != 0) {
    const auto old = before.find(id);
    const auto next = after.find(id);
    if (old == before.end() || next == after.end() || old->second.parent != next->second.parent ||
        old->second.location != next->second.location) {
      return false;
    }
    id = old->second.parent;
  }
  return true;
}

} // namespace detail

// A single-thread-confined logical authority. Host callbacks are synchronous and must not leak
// candidate aliases, reenter the authority, or perform external effects before acceptance.
template <typename Root, history_mode Mode = history_mode::linear,
          history_labels Labels = history_labels::disabled, typename Traits = model_traits<Root>,
          typename Session = std::uint64_t,
          typename SessionTraits = collaboration_session_traits<Session>,
          store_features Features = store_features::all>
class collaboration_authority {
public:
  using session_type = Session;
  using records_type = collaboration_record_types<Session, SessionTraits>;
  using change_proposal_type = typename records_type::change_proposal;
  using accepted_change_type = typename records_type::accepted_change;
  using lock_request_type = typename records_type::lock_request;
  using lock_grant_type = typename records_type::lock_grant;
  using lock_update_type = typename records_type::lock_update;
  using lock_snapshot_type = typename records_type::lock_snapshot;
  using editing_presence_type = typename records_type::editing_presence;
  using result_type = basic_collaboration_result<Session, SessionTraits>;
  using store_type = model_store<Root, Mode, Labels, Traits, Features>;
  static_assert(store_type::has_collaboration, "An authority requires collaboration");
  using storage_type = typename store_type::storage_type;
  using edit_type = typename store_type::transaction_edit;
  using command_handler = std::function<void(edit_type&, std::span<const std::uint8_t>)>;
  using change_policy = std::function<bool(const session_type&, const storage_type&,
                                           const storage_type&, const std::vector<std::uint64_t>&)>;
  using lock_policy = std::function<bool(const session_type&, std::uint64_t, edit_lock_scope)>;

private:
  static constexpr bool has_history = Mode != history_mode::disabled;
  struct no_history {};
  struct session_history {
    std::vector<std::uint64_t> undo{};
    std::vector<std::uint64_t> redo{};
  };
  struct session_state : std::conditional_t<has_history, session_history, no_history> {
    bool active{true};
    bool writable{true};
  };
  struct retained_history {
    std::vector<std::uint8_t> before{};
  };
  struct retained_operation : std::conditional_t<has_history, retained_history, no_history> {
    std::vector<std::uint8_t> request{};
    std::uint32_t kind{};
    std::shared_ptr<result_type> result{};
  };
  struct active_lock {
    lock_grant_type grant{};
    std::uint64_t deadline_ms{};
  };
  struct presence_state {
    editing_presence_type record{};
    std::uint64_t deadline_ms{};
    bool active{};
  };
  using operation_key = std::pair<session_type, std::uint64_t>;
  using session_less = detail::collaboration_session_less<Session, SessionTraits>;
  using operation_less = detail::collaboration_operation_less<Session, SessionTraits>;
  const std::thread::id thread_{std::this_thread::get_id()};
  collaboration_options options_{};
  detail::store_configuration<store_type::has_history> store_options_{store_options{}};
  command_handler handler_{};
  change_policy policy_{};
  lock_policy lock_policy_{};
  std::unique_ptr<store_type> store_{};
  collaboration::domain domain_{};
  detail::collaboration_entities entities_{};
  std::map<session_type, session_state, session_less> sessions_{};
  std::map<operation_key, retained_operation, operation_less> operations_{};
  std::map<std::uint64_t, active_lock> locks_{};
  std::vector<std::shared_ptr<const lock_update_type>> lock_updates_{};
  std::map<operation_key, presence_state, operation_less> presence_{};
  std::size_t retained_bytes_{};
  std::uint64_t sequence_{};
  std::uint64_t lock_sequence_{};
  std::uint64_t now_ms_{};
  bool busy_{};
  struct journal_fence {
    bool fenced{};
  };
  struct no_journal_fence {
    static constexpr bool fenced = false;
  };
#if defined(_MSC_VER)
  [[msvc::no_unique_address]]
#else
  [[no_unique_address]]
#endif
  std::conditional_t<store_type::has_journal, journal_fence, no_journal_fence> journal_state_{};
  const change_proposal_type* proposal_{};
  result_type* preparing_{};
  const collaboration::model_change* decoded_change_{}; // Borrowed for one synchronous submission.
  detail::collaboration_entities prepared_entities_{};
  std::shared_ptr<accepted_change_type> prepared_batch_{};
  struct inverse_state {
    detail::collaboration_entity_versions prepared_entity_versions{};
    detail::collaboration_field_versions prepared_field_versions{};
    detail::collaboration_entity_versions entity_versions{};
    detail::collaboration_field_versions field_versions{};
    std::vector<std::uint8_t> before{};
    std::vector<std::uint64_t> undo{};
    std::vector<std::uint64_t> redo{};
    const retained_operation* reversing{};
    collaboration_history_action action{collaboration_history_action::edit};
    typename store_type::identity_table restoration{};
  };
#if defined(_MSC_VER)
  [[msvc::no_unique_address]]
#else
  [[no_unique_address]]
#endif
  std::conditional_t<has_history, inverse_state, no_history> inverse_{};
  std::size_t prepared_bytes_{};

  // Borrow this authority through a non-owning callback; the authority owns and outlives the store.
  void bind_store(store_type& store) {
    store.collaboration_state_.prepare_context = this;
    store.collaboration_state_.prepare = [](void* owner, const storage_type& candidate,
                                            const std::vector<std::uint8_t>& snapshot) {
      static_cast<collaboration_authority*>(owner)->validate_candidate(candidate, snapshot);
    };
  }

  // Reject cross-thread and callback reentry before touching authority state.
  void check_access() const {
    if (thread_ != std::this_thread::get_id() || busy_) {
      throw std::logic_error{"Collaboration authority is thread-confined and non-reentrant"};
    }
    if (journal_state_.fenced) {
      throw journal_indeterminate_error{
          "Collaboration authority requires recovery in a fresh epoch"};
    }
  }

  struct operation_scope {
    collaboration_authority& owner;
    // Hold one ordering boundary across timers, policy, command execution, and publication.
    explicit operation_scope(collaboration_authority& value) : owner{value} {
      owner.check_access();
      owner.busy_ = true;
    }
    // No cleanup allocation or callback can change an already determined outcome.
    ~operation_scope() {
      owner.busy_ = false;
    }
  };

  // Build the generated ownership projection without modifying model storage.
  detail::collaboration_entities collect(const storage_type& value) const {
    detail::collaboration_collector collector{options_.max_entities,
                                              store_options_.max_snapshot_bytes};
    Traits::visit_collaboration(value, typename Traits::id_type{}, collector);
    return std::move(collector.entities);
  }

  // Session policy ordering defines identity consistently across tables and trusted bindings.
  static bool same_session(const session_type& left, const session_type& right) {
    return !SessionTraits::less(left, right) && !SessionTraits::less(right, left);
  }

  // Require a host-bound live session independently of request metadata.
  bool valid_session(const session_type& session, const session_type& trusted_session) const {
    const auto found = sessions_.find(session);
    return SessionTraits::valid(session) && same_session(session, trusted_session) &&
           found != sessions_.end() && found->second.active;
  }

  // Reserve retry state before any model/lock mutation. Conflicting ID reuse is always rejected.
  std::pair<std::shared_ptr<result_type>, bool> remember(const session_type& session,
                                                         std::uint64_t operation,
                                                         std::uint32_t kind,
                                                         std::vector<std::uint8_t> request) {
    if (operation == 0) {
      throw std::invalid_argument{"Operation ID must be nonzero"};
    }
    const operation_key key{session, operation};
    const auto previous = operations_.find(key);
    if (previous != operations_.end()) {
      if (previous->second.kind != kind || previous->second.request != request) {
        throw std::invalid_argument{"Operation ID reused with different payload"};
      }
      return {previous->second.result, false};
    }
    if (operations_.size() >= options_.max_operations ||
        request.size() > options_.max_retained_bytes - retained_bytes_) {
      throw std::length_error{"Collaboration retry budget exhausted"};
    }
    const auto size = request.size();
    auto result = std::make_shared<result_type>();
    operations_.emplace(key, retained_operation{{}, std::move(request), kind, result});
    retained_bytes_ += size;
    return {std::move(result), true};
  }

  // Allocate each event before changing its associated grant; delivery occurs through the outbox.
  void emit_lock(lock_update_action action, const lock_grant_type& grant) {
    if (lock_updates_.size() >= options_.max_lock_updates) {
      throw std::length_error{"Collaboration lock outbox full; prune acknowledged updates"};
    }
    auto event = std::make_shared<lock_update_type>();
    event->context = domain_;
    event->sequence = detail::next_collaboration_sequence(lock_sequence_);
    event->action = static_cast<std::uint32_t>(action);
    event->grant = grant;
    lock_updates_.push_back(std::move(event));
    ++lock_sequence_;
  }

  // Apply timer transitions in the same ordering boundary as acceptance; expiry is host-clock driven.
  void expire(std::uint64_t now_ms) {
    if (now_ms < now_ms_) {
      throw std::invalid_argument{"Collaboration monotonic clock moved backwards"};
    }
    now_ms_ = now_ms;
    for (auto entry = locks_.begin(); entry != locks_.end();) {
      if (entry->second.deadline_ms <= now_ms_) {
        emit_lock(lock_update_action::expire, entry->second.grant);
        entry = locks_.erase(entry);
      } else {
        ++entry;
      }
    }
    for (auto& [key, state] : presence_) {
      static_cast<void>(key);
      if (state.deadline_ms <= now_ms_) {
        state.active = false;
      }
    }
  }

  // Determine whether a grant protects this entity in a particular ownership version.
  static bool covers(const lock_grant_type& grant, const detail::collaboration_entities& entities,
                     std::uint64_t id) {
    return grant.target == id ||
           (grant.scope == static_cast<std::uint32_t>(edit_lock_scope::owned_subtree) &&
            detail::collaboration_contains(entities, grant.target, id));
  }

  // Fail the private candidate with a structured collaboration reason.
  [[noreturn]] void reject_candidate(collaboration_status status) {
    preparing_->status = status;
    throw std::runtime_error{"Collaboration candidate rejected"};
  }

  // Capture generated field addresses using bounded codecs, including ownership-only membership.
  detail::collaboration_fields collect_fields(const storage_type& value) const {
    detail::collaboration_field_collector collector{store_options_.max_snapshot_bytes,
                                                    options_.max_tracked_fields};
    Traits::visit_collaboration_fields(value, collector);
    return std::move(collector.fields);
  }

  // Prepare history and active contribution versions before any journal publication can succeed.
  void prepare_history(const storage_type& candidate, const detail::collaboration_entities& next,
                       accepted_change_type& batch) {
    if constexpr (!has_history) {
      const auto before = collect_fields(*store_->read());
      const auto after = collect_fields(candidate);
      batch.history.action = static_cast<std::uint32_t>(collaboration_history_action::edit);
      // Addresses and ownership changes support pending-edit conflict detection, not undo.
      for (const auto& [address, bytes] : before) {
        const auto found = after.find(address);
        if (found == after.end() || found->second != bytes) {
          collaboration::field_change field;
          field.entity = address.first;
          field.field = address.second;
          batch.history.fields.push_back(std::move(field));
        }
      }
      for (const auto& [address, bytes] : after) {
        static_cast<void>(bytes);
        if (!before.contains(address)) {
          collaboration::field_change field;
          field.entity = address.first;
          field.field = address.second;
          batch.history.fields.push_back(std::move(field));
        }
      }
      for (const auto& [id, entity] : entities_) {
        static_cast<void>(entity);
        if (!detail::same_collaboration_ancestry(entities_, next, id)) {
          batch.history.dependencies.push_back({id, 0, batch.sequence});
        }
      }
      const auto metadata = encode_collaboration_record(batch.history, options_.max_retained_bytes);
      if (metadata.size() > options_.max_retained_bytes - retained_bytes_ - batch.snapshot.size()) {
        throw std::length_error{"Collaboration change byte budget exhausted"};
      }
      prepared_bytes_ = batch.snapshot.size() + metadata.size();
    } else {
      const auto before = collect_fields(*store_->read());
      const auto after = collect_fields(candidate);
      std::set<detail::collaboration_field_address> addresses;
      for (const auto& [address, value] : before) {
        static_cast<void>(value);
        addresses.insert(address);
      }
      for (const auto& [address, value] : after) {
        static_cast<void>(value);
        addresses.insert(address);
      }
      std::set<std::uint64_t> relocated;
      for (const auto& [id, value] : entities_) {
        static_cast<void>(value);
        if (!detail::same_collaboration_ancestry(entities_, next, id)) {
          relocated.insert(id);
        }
      }
      for (const auto& [id, value] : next) {
        static_cast<void>(value);
        if (!entities_.contains(id)) {
          relocated.insert(id);
        }
      }
      inverse_.prepared_field_versions = inverse_.field_versions;
      inverse_.prepared_entity_versions = inverse_.entity_versions;
      detail::collaboration_field_versions restored_fields;
      detail::collaboration_entity_versions restored_entities;
      if (inverse_.reversing) {
        const auto& previous = inverse_.reversing->result->accepted->history;
        for (const auto& field : previous.fields) {
          restored_fields.emplace(detail::collaboration_field_address{field.entity, field.field},
                                  field.before_version);
        }
        for (const auto& dependency : previous.dependencies) {
          restored_entities.emplace(dependency.entity, dependency.before_version);
        }
      }
      auto& history = batch.history;
      history.action = static_cast<std::uint32_t>(inverse_.action);
      history.target_operation =
          inverse_.reversing ? inverse_.reversing->result->accepted->operation : 0;
      std::set<std::uint64_t> dependencies = relocated;
      for (const auto& address : addresses) {
        const auto old = before.find(address);
        const auto current = after.find(address);
        const bool changed =
            old == before.end() || current == after.end() || old->second != current->second;
        if (!changed && !relocated.contains(address.first)) {
          continue;
        }
        collaboration::field_change field;
        field.entity = address.first;
        field.field = address.second;
        field.before_present = old != before.end();
        field.after_present = current != after.end();
        if (field.before_present) {
          field.before = old->second;
        }
        if (field.after_present) {
          field.after = current->second;
        }
        field.before_version = detail::collaboration_version(inverse_.field_versions, address);
        field.after_version = changed ? batch.sequence : field.before_version;
        if (const auto restored = restored_fields.find(address);
            restored != restored_fields.end()) {
          field.after_version = restored->second;
        }
        inverse_.prepared_field_versions[address] = field.after_version;
        history.fields.push_back(std::move(field));
        dependencies.insert(address.first);
      }
      for (const auto id : dependencies) {
        const auto before_version = detail::collaboration_version(inverse_.entity_versions, id);
        auto after_version = relocated.contains(id) ? batch.sequence : before_version;
        if (const auto restored = restored_entities.find(id); restored != restored_entities.end()) {
          after_version = restored->second;
        }
        inverse_.prepared_entity_versions[id] = after_version;
        history.dependencies.push_back({id, before_version, after_version});
      }
      if (inverse_.prepared_field_versions.size() > options_.max_tracked_fields ||
          inverse_.prepared_entity_versions.size() > options_.max_tracked_fields) {
        throw std::length_error{"Collaboration version budget exhausted"};
      }
      const auto& session = sessions_.at(proposal_->session);
      inverse_.undo = session.undo;
      inverse_.redo = session.redo;
      if (inverse_.action == collaboration_history_action::edit) {
        inverse_.undo.push_back(proposal_->operation);
        inverse_.redo.clear();
      } else if (inverse_.action == collaboration_history_action::undo) {
        inverse_.undo.pop_back();
        inverse_.redo.push_back(proposal_->operation);
      } else {
        inverse_.redo.pop_back();
        inverse_.undo.push_back(proposal_->operation);
      }
      inverse_.before = store_->snapshot_;
      const auto metadata = encode_collaboration_record(history, options_.max_retained_bytes);
      prepared_bytes_ = 0;
      for (const auto size : {batch.snapshot.size(), inverse_.before.size(), metadata.size()}) {
        if (size > options_.max_retained_bytes - retained_bytes_ - prepared_bytes_) {
          throw std::length_error{"Collaboration history byte budget exhausted"};
        }
        prepared_bytes_ += size;
      }
    }
  }

  // Reverse the session's current stack tip, never trusting client-supplied inverse values or IDs.
  void apply_history_change(edit_type& edit, const collaboration::model_change& change) {
    if (change.base_allocated_id != 0 || change.allocated_id != 0 || !change.snapshot.empty() ||
        (change.history_action != static_cast<std::uint32_t>(collaboration_history_action::undo) &&
         change.history_action != static_cast<std::uint32_t>(collaboration_history_action::redo))) {
      throw std::invalid_argument{"Invalid collaborative history request"};
    }
    const auto action = static_cast<collaboration_history_action>(change.history_action);
    const auto& session = sessions_.at(proposal_->session);
    const auto& stack = action == collaboration_history_action::undo ? session.undo : session.redo;
    if (stack.empty()) {
      if (change.target_operation != 0) {
        reject_candidate(collaboration_status::conflict);
      }
      return;
    }
    if (change.target_operation != 0 && change.target_operation != stack.back()) {
      reject_candidate(collaboration_status::conflict);
    }
    const auto& target = operations_.at({proposal_->session, stack.back()});
    const auto& accepted = *target.result->accepted;
    // A stale view must not accidentally undo an unseen edit from the same application session.
    if (accepted.sequence > proposal_->base_sequence) {
      reject_candidate(collaboration_status::conflict);
    }
    const auto fields = collect_fields(*store_->read());
    for (const auto& field : accepted.history.fields) {
      const detail::collaboration_field_address address{field.entity, field.field};
      const auto current = fields.find(address);
      if (detail::collaboration_version(inverse_.field_versions, address) != field.after_version ||
          (current != fields.end()) != field.after_present ||
          (current != fields.end() && current->second != field.after)) {
        reject_candidate(collaboration_status::conflict);
      }
    }
    for (const auto& dependency : accepted.history.dependencies) {
      if (detail::collaboration_version(inverse_.entity_versions, dependency.entity) !=
          dependency.after_version) {
        reject_candidate(collaboration_status::conflict);
      }
    }
    const auto expected = detail::decode<storage_type>(accepted.snapshot, store_options_.decode);
    const auto desired = detail::decode<storage_type>(target.before, store_options_.decode);
    inverse_.restoration = store_->inspect(desired, store_->allocated_id());
    store_->collaboration_state_.restoration = &inverse_.restoration;
    inverse_.reversing = &target;
    inverse_.action = action;
    try {
      edit.update([&](storage_type& current) {
        detail::collaboration_history_merger merger{store_options_.max_snapshot_bytes};
        Traits::merge_collaboration(current, expected, desired, merger);
      });
    } catch (const detail::collaboration_history_conflict&) {
      reject_candidate(collaboration_status::conflict);
    }
  }

  // Apply the default bounded model payload through the same transaction, identity and policy gates.
  void apply_model_change(edit_type& edit) {
    const auto& change = *decoded_change_;
    if (change.format_version == collaboration_history_change_version) {
      if constexpr (has_history) {
        apply_history_change(edit, change);
      } else {
        throw std::invalid_argument{"Collaborative history is disabled"};
      }
      return;
    }
    if (change.format_version != collaboration_model_change_version || change.history_action != 0 ||
        change.target_operation != 0 || change.allocated_id < change.base_allocated_id ||
        change.allocated_id > std::numeric_limits<typename Traits::id_type>::max() ||
        change.allocated_id - change.base_allocated_id > options_.max_created_ids ||
        change.snapshot.size() > store_options_.max_snapshot_bytes) {
      throw std::invalid_argument{"Invalid or over-budget collaboration model change"};
    }
    // Failed edits can reserve IDs without advancing the accepted sequence. Never reuse those IDs.
    if (change.base_allocated_id != store_->allocated_id()) {
      reject_candidate(collaboration_status::conflict);
    }
    auto candidate = detail::decode<storage_type>(change.snapshot, store_options_.decode);
    for (auto allocated = change.base_allocated_id; allocated < change.allocated_id; ++allocated) {
      static_cast<void>(edit.create_id());
    }
    edit.update([&](storage_type& value) { value = std::move(candidate); });
  }

  // Validate the actual semantic diff and prepare replication bytes before store publication.
  void validate_candidate(const storage_type& candidate,
                          const std::vector<std::uint8_t>& snapshot) {
    if (!proposal_) {
      return; // Initial construction and explicit document restore have no collaboration request.
    }
    auto next = collect(candidate);
    std::vector<std::uint64_t> affected;
    bool ownership_changed = false;
    for (const auto& [id, old] : entities_) {
      const auto found = next.find(id);
      ownership_changed =
          ownership_changed || found == next.end() || old.parent != found->second.parent;
      if (found == next.end() || old.values != found->second.values ||
          !detail::same_collaboration_ancestry(entities_, next, id)) {
        affected.push_back(id);
      }
    }
    for (const auto& [id, value] : next) {
      static_cast<void>(value);
      if (!entities_.contains(id)) {
        affected.push_back(id);
      }
    }
    // A relocation must not turn previously disjoint grants into overlapping rights,
    // even when their owner is the same session and supplies both references.
    if (ownership_changed) {
      for (auto left = locks_.begin(); left != locks_.end(); ++left) {
        if (!next.contains(left->first)) {
          continue;
        }
        for (auto right = std::next(left); right != locks_.end(); ++right) {
          if (next.contains(right->first) && (covers(left->second.grant, next, right->first) ||
                                              covers(right->second.grant, next, left->first))) {
            reject_candidate(collaboration_status::conflict);
          }
        }
      }
    }
    for (const auto& reference : proposal_->grants) {
      const auto found = locks_.find(reference.target);
      if (found == locks_.end() || found->second.grant.generation != reference.generation ||
          !same_session(found->second.grant.session, proposal_->session)) {
        reject_candidate(collaboration_status::obsolete_grant);
      }
    }
    for (const auto id : affected) {
      bool covered = false;
      for (const auto& [target, lock] : locks_) {
        const auto& grant = lock.grant;
        if (!covers(grant, entities_, id) && !covers(grant, next, id)) {
          continue;
        }
        if (!same_session(grant.session, proposal_->session)) {
          reject_candidate(collaboration_status::conflict);
        }
        const auto supplied = std::ranges::any_of(proposal_->grants, [&](const auto& reference) {
          return reference.target == target && reference.generation == grant.generation;
        });
        if (!supplied) {
          reject_candidate(collaboration_status::obsolete_grant);
        }
        covered = true;
      }
      if (options_.require_edit_lock && !covered) {
        reject_candidate(collaboration_status::denied);
      }
    }
    if (policy_ && !policy_(proposal_->session, *store_->read(), candidate, affected)) {
      reject_candidate(collaboration_status::denied);
    }
    auto batch = std::make_shared<accepted_change_type>();
    batch->context = domain_;
    batch->sequence = detail::next_collaboration_sequence(sequence_);
    batch->session = proposal_->session;
    batch->operation = proposal_->operation;
    batch->allocated_id = store_->allocated_id();
    batch->snapshot = snapshot;
    if (batch->snapshot.size() > options_.max_retained_bytes - retained_bytes_) {
      throw std::length_error{"Collaboration accepted snapshot budget exceeded"};
    }
    prepare_history(candidate, next, *batch);
    prepared_entities_.swap(next);
    prepared_batch_ = std::move(batch);
  }

public:
  // Use generated model editors by default; custom command schemas and dispatchers are optional.
  collaboration_authority(Root initial, std::uint64_t epoch,
                          document_id document = make_document_id(),
                          collaboration_options options = {}, store_options storage_options = {},
                          change_policy policy = {}, lock_policy lock_permissions = {})
      : collaboration_authority{
            std::move(initial),
            epoch,
            [this](edit_type& edit, std::span<const std::uint8_t>) { apply_model_change(edit); },
            document,
            options,
            storage_options,
            std::move(policy),
            std::move(lock_permissions)} {
    domain_.protocol_version = records_type::model_protocol;
  }

  // Epoch is a nonzero host-fenced lifetime, never derived from a saved document or remote claim.
  collaboration_authority(Root initial, std::uint64_t epoch, command_handler handler,
                          document_id document = make_document_id(),
                          collaboration_options options = {}, store_options storage_options = {},
                          change_policy policy = {}, lock_policy lock_permissions = {})
      : options_{options}, store_options_{storage_options}, handler_{std::move(handler)},
        policy_{std::move(policy)}, lock_policy_{std::move(lock_permissions)} {
    if (epoch == 0 || !handler_ || options_.max_retained_bytes == 0 ||
        storage_options.max_snapshot_bytes == 0) {
      throw std::invalid_argument{"Invalid collaboration authority configuration"};
    }
    store_ = std::make_unique<store_type>(std::move(initial), document, storage_options);
    bind_store(*store_);
    domain_ = {records_type::command_protocol,
               std::string{Traits::schema_id},
               std::numeric_limits<typename Traits::id_type>::digits,
               document.high,
               document.low,
               epoch};
    entities_ = collect(*store_->read());
  }

  collaboration_authority(const collaboration_authority&) = delete;
  collaboration_authority& operator=(const collaboration_authority&) = delete;
  collaboration_authority(collaboration_authority&&) = delete;
  collaboration_authority& operator=(collaboration_authority&&) = delete;

  // Return an immutable model pin; no local mutation path bypasses authoritative acceptance.
  auto read() const {
    check_access();
    return store_->read();
  }

  // Copy the complete wire binding for proposals and trusted replica initialization.
  collaboration::domain context() const {
    check_access();
    return domain_;
  }

  // Return the accepted model cursor independently of locks, presence, and undo history.
  std::uint64_t sequence() const {
    check_access();
    return sequence_;
  }

  // Register a host-bound session; IDs cannot be reused during this epoch, even after shutdown.
  void open_session(const session_type& session, bool writable = true) {
    operation_scope guard{*this};
    if (!SessionTraits::valid(session) || sessions_.size() >= options_.max_sessions ||
        sessions_.contains(session)) {
      throw std::invalid_argument{"Invalid, reused, or over-budget collaboration session"};
    }
    if constexpr (records_type::command_protocol != collaboration_protocol_version) {
      // Fail unusable IDs at registration, before any locks, retries or presence can refer to them.
      if (std::string_view{SessionTraits::wire_name}.empty() ||
          SessionTraits::encode(session).size() > SessionTraits::max_encoded_bytes) {
        throw std::invalid_argument{"Invalid collaboration session codec or encoding budget"};
      }
    }
    session_state state;
    state.writable = writable;
    sessions_.emplace(session, std::move(state));
  }

  // Update host permission for future operations; retries retain their original outcome.
  void set_session_writable(const session_type& session, bool writable) {
    operation_scope guard{*this};
    sessions_.at(session).writable = writable;
  }

  // Revoke current locks and retire presence; delayed records cannot revive the closed session.
  void close_session(const session_type& session) {
    operation_scope guard{*this};
    auto& state = sessions_.at(session);
    for (auto entry = locks_.begin(); entry != locks_.end();) {
      if (same_session(entry->second.grant.session, session)) {
        emit_lock(lock_update_action::revoke, entry->second.grant);
        entry = locks_.erase(entry);
      } else {
        ++entry;
      }
    }
    state.active = false;
    for (auto& [key, presence] : presence_) {
      if (same_session(key.first, session)) {
        presence.active = false;
      }
    }
  }

  // Accept one typed-command payload through the trusted handler. Exact retries never rerun it.
  std::shared_ptr<const result_type> submit_change(const change_proposal_type& proposal,
                                                   const session_type& trusted_session,
                                                   std::uint64_t now_ms) {
    operation_scope guard{*this};
    expire(now_ms);
    if (!detail::same_domain(proposal.context, domain_) ||
        !valid_session(proposal.session, trusted_session)) {
      throw std::invalid_argument{"Untrusted collaboration proposal context"};
    }
    if (proposal.command.size() > options_.max_command_bytes ||
        proposal.grants.size() > options_.max_locks) {
      throw std::length_error{"Collaboration proposal budget exceeded"};
    }
    auto [result, fresh] =
        remember(proposal.session, proposal.operation, 1,
                 encode_collaboration_record(proposal, store_options_.decode.max_input_bytes));
    if (!fresh) {
      return result;
    }
    if (!sessions_.at(proposal.session).writable) {
      result->status = collaboration_status::denied;
      return result;
    }
    bool history_request = false;
    collaboration::model_change decoded_change;
    if (domain_.protocol_version == records_type::model_protocol) {
      try {
        decoded_change = decode_collaboration_record<collaboration::model_change>(
            proposal.command, store_options_.decode);
        history_request = decoded_change.format_version == collaboration_history_change_version;
      } catch (...) {
        result->error = std::current_exception();
        return result;
      }
    }
    if (proposal.base_sequence > sequence_ ||
        (!history_request && proposal.base_sequence != sequence_)) {
      result->status = collaboration_status::conflict;
      return result;
    }
    auto& retained = operations_.at({proposal.session, proposal.operation});
    proposal_ = &proposal;
    preparing_ = result.get();
    decoded_change_ = &decoded_change;
    if constexpr (has_history) {
      inverse_.action = collaboration_history_action::edit;
    }
    const auto outcome =
        store_->execute_transaction([&](auto& edit) { handler_(edit, proposal.command); });
    proposal_ = nullptr;
    preparing_ = nullptr;
    decoded_change_ = nullptr;
    if constexpr (has_history) {
      store_->collaboration_state_.restoration = nullptr;
      inverse_.reversing = nullptr;
      inverse_.restoration.clear();
    }
    result->error = outcome.error;
    if (outcome.status == transaction_status::committed) {
      entities_.swap(prepared_entities_);
      sequence_ = prepared_batch_->sequence;
      retained_bytes_ += prepared_bytes_;
      if constexpr (has_history) {
        inverse_.field_versions.swap(inverse_.prepared_field_versions);
        inverse_.entity_versions.swap(inverse_.prepared_entity_versions);
        auto& session = sessions_.at(proposal.session);
        session.undo.swap(inverse_.undo);
        session.redo.swap(inverse_.redo);
        retained.before.swap(inverse_.before);
      }
      result->accepted = std::move(prepared_batch_);
      result->status = collaboration_status::accepted;
      // Deleted targets lose advisory presence; grants remain fenced until release/expiry.
      for (auto& [key, presence] : presence_) {
        static_cast<void>(key);
        if (!entities_.contains(presence.record.target)) {
          presence.active = false;
        }
      }
    } else if (outcome.status == transaction_status::no_change) {
      result->status = collaboration_status::no_change;
    } else if (outcome.status == transaction_status::reverted) {
      result->status = collaboration_status::reverted;
    } else if (outcome.status == transaction_status::indeterminate) {
      result->status = collaboration_status::indeterminate;
      if constexpr (store_type::has_journal) {
        journal_state_.fenced = true;
      }
    }
    prepared_batch_.reset();
    prepared_entities_.clear();
    if constexpr (has_history) {
      inverse_.prepared_field_versions.clear();
      inverse_.prepared_entity_versions.clear();
      inverse_.before.clear();
      inverse_.undo.clear();
      inverse_.redo.clear();
    }
    return result;
  }

  // Inspect this application's undo tip; zero means empty, not a permission or conflict guarantee.
  std::uint64_t undo_operation(const session_type& session) const
    requires(has_history)
  {
    check_access();
    const auto& state = sessions_.at(session);
    return state.undo.empty() ? 0 : state.undo.back();
  }

  // Redo reverses the accepted undo transaction, preserving its current dependency checks.
  std::uint64_t redo_operation(const session_type& session) const
    requires(has_history)
  {
    check_access();
    const auto& state = sessions_.at(session);
    return state.redo.empty() ? 0 : state.redo.back();
  }

  // Capture a join/reconnect baseline atomically with its accepted cursor on the owner thread.
  accepted_change_type snapshot() const {
    check_access();
    return {domain_, sequence_, session_type{}, 0, store_->allocated_id(), store_->snapshot_};
  }

  // Return immutable accepted batches in order; transport/sink failures cannot roll back acceptance.
  std::vector<std::shared_ptr<const accepted_change_type>>
  accepted_since(std::uint64_t cursor) const {
    check_access();
    if (cursor > sequence_) {
      throw std::invalid_argument{"Accepted cursor is ahead of authority"};
    }
    std::vector<std::shared_ptr<const accepted_change_type>> result;
    for (const auto& [key, operation] : operations_) {
      static_cast<void>(key);
      if (operation.result->accepted && operation.result->accepted->sequence > cursor) {
        result.push_back(operation.result->accepted);
      }
    }
    std::ranges::sort(result, {}, [](const auto& batch) { return batch->sequence; });
    return result;
  }

  // Acquire, renew, or release a fenced lease. Retrying acquisition does not extend its deadline.
  std::shared_ptr<const result_type> change_lock(const lock_request_type& request,
                                                 const session_type& trusted_session,
                                                 std::uint64_t now_ms) {
    operation_scope guard{*this};
    expire(now_ms);
    if (!detail::same_domain(request.context, domain_) ||
        !valid_session(request.session, trusted_session)) {
      throw std::invalid_argument{"Untrusted lock request context"};
    }
    auto [result, fresh] =
        remember(request.session, request.operation, 2,
                 encode_collaboration_record(request, store_options_.decode.max_input_bytes));
    if (!fresh) {
      return result;
    }
    try {
      const auto action = static_cast<edit_lock_action>(request.action);
      const auto scope = static_cast<edit_lock_scope>(request.scope);
      if ((scope != edit_lock_scope::entity && scope != edit_lock_scope::owned_subtree) ||
          (action != edit_lock_action::acquire && action != edit_lock_action::renew &&
           action != edit_lock_action::release)) {
        result->status = collaboration_status::invalid;
        return result;
      }
      if (action != edit_lock_action::release &&
          (!sessions_.at(request.session).writable ||
           (lock_policy_ && !lock_policy_(request.session, request.target, scope)))) {
        result->status = collaboration_status::denied;
        return result;
      }
      auto found = locks_.find(request.target);
      if (action == edit_lock_action::acquire) {
        if (request.generation != 0 || !entities_.contains(request.target)) {
          result->status = collaboration_status::invalid;
          return result;
        }
        for (const auto& [target, lock] : locks_) {
          if (covers(lock.grant, entities_, request.target) ||
              (scope == edit_lock_scope::owned_subtree &&
               detail::collaboration_contains(entities_, request.target, target))) {
            result->status = collaboration_status::conflict;
            return result;
          }
        }
        if (locks_.size() >= options_.max_locks || request.duration_ms > options_.max_lease_ms) {
          throw std::length_error{"Collaboration lock budget exceeded"};
        }
        const auto deadline = detail::collaboration_deadline(now_ms_, request.duration_ms);
        lock_grant_type grant{request.target, request.scope, request.session,
                              detail::next_collaboration_sequence(lock_sequence_)};
        const auto inserted = locks_.emplace(request.target, active_lock{grant, deadline}).first;
        try {
          emit_lock(lock_update_action::grant, grant);
        } catch (...) {
          locks_.erase(inserted);
          throw;
        }
        result->grant = grant;
      } else {
        if (found == locks_.end() || !same_session(found->second.grant.session, request.session) ||
            found->second.grant.generation != request.generation ||
            found->second.grant.scope != request.scope) {
          result->status = collaboration_status::obsolete_grant;
          return result;
        }
        result->grant = found->second.grant;
        if (action == edit_lock_action::renew) {
          if (request.duration_ms > options_.max_lease_ms) {
            throw std::length_error{"Collaboration lease duration exceeded"};
          }
          found->second.deadline_ms = detail::collaboration_deadline(now_ms_, request.duration_ms);
        } else {
          emit_lock(lock_update_action::release, found->second.grant);
          locks_.erase(found);
        }
      }
      result->status = collaboration_status::accepted;
    } catch (...) {
      result->error = std::current_exception();
      result->status = collaboration_status::failed;
    }
    return result;
  }

  // Drive idle-time expiry with the same host monotonic milliseconds supplied to requests.
  void advance_expiry(std::uint64_t now_ms) {
    operation_scope guard{*this};
    expire(now_ms);
  }

  // Capture the complete lock table and sequence in one owner-thread operation.
  lock_snapshot_type lock_snapshot() const {
    check_access();
    lock_snapshot_type result{domain_, lock_sequence_, {}};
    for (const auto& [target, lock] : locks_) {
      static_cast<void>(target);
      result.grants.push_back(lock.grant);
    }
    return result;
  }

  // Replay retained lock events; a pruned cursor must rejoin from lock_snapshot().
  std::vector<std::shared_ptr<const lock_update_type>> locks_since(std::uint64_t cursor) const {
    check_access();
    if (cursor > lock_sequence_ ||
        (cursor < lock_sequence_ &&
         (lock_updates_.empty() || cursor < lock_updates_.front()->sequence - 1))) {
      throw std::out_of_range{"Lock cursor needs resynchronization"};
    }
    std::vector<std::shared_ptr<const lock_update_type>> result;
    for (const auto& event : lock_updates_) {
      if (event->sequence > cursor) {
        result.push_back(event);
      }
    }
    return result;
  }

  // Retire delivered lock events explicitly; lagging replicas subsequently require a snapshot.
  void prune_lock_updates(std::uint64_t through) {
    operation_scope guard{*this};
    if (through > lock_sequence_) {
      throw std::invalid_argument{"Lock acknowledgement is ahead of authority"};
    }
    std::erase_if(lock_updates_, [&](const auto& event) { return event->sequence <= through; });
  }

  // Receive bounded advisory presence. An ended/expired lifetime must use a new presence ID.
  bool receive_presence(const editing_presence_type& record, const session_type& trusted_session,
                        std::uint64_t now_ms) {
    operation_scope guard{*this};
    expire(now_ms);
    if (!detail::same_domain(record.context, domain_) ||
        !valid_session(record.session, trusted_session) || record.presence == 0 ||
        record.sequence == 0 || record.preview.size() > options_.max_preview_bytes ||
        (record.action != static_cast<std::uint32_t>(presence_action::update) &&
         record.action != static_cast<std::uint32_t>(presence_action::end))) {
      throw std::invalid_argument{"Invalid collaboration presence"};
    }
    const operation_key key{record.session, record.presence};
    const auto previous = presence_.find(key);
    if (previous != presence_.end() &&
        (!previous->second.active || previous->second.record.sequence >= record.sequence)) {
      return false;
    }
    if (previous == presence_.end() && presence_.size() >= options_.max_presence) {
      throw std::length_error{"Presence lifetime budget exceeded"};
    }
    presence_state state{record,
                         detail::collaboration_deadline(now_ms_, options_.presence_timeout_ms),
                         record.action == static_cast<std::uint32_t>(presence_action::update) &&
                             entities_.contains(record.target)};
    if (previous == presence_.end()) {
      presence_.emplace(key, std::move(state));
    } else {
      std::swap(previous->second, state);
    }
    return true;
  }

  // Copy current advisory state for UI/transport; tombstones remain internal until a fresh epoch.
  std::vector<editing_presence_type> editing_presence() const {
    check_access();
    std::vector<editing_presence_type> result;
    for (const auto& [key, state] : presence_) {
      static_cast<void>(key);
      if (state.active) {
        result.push_back(state.record);
      }
    }
    return result;
  }

  // Persist document/history only; session rights, retries, presence, and epochs are never restored.
  std::vector<std::uint8_t> save_document() const {
    check_access();
    return store_->save();
  }

  // Restore before opening sessions. The host supplies a fresh fenced epoch to this authority.
  void load_document(const std::vector<std::uint8_t>& bytes) {
    operation_scope guard{*this};
    if (!sessions_.empty() || sequence_ != 0 || store_->journal_state_.sink) {
      throw std::logic_error{"Restore collaboration document before opening sessions"};
    }
    auto replacement = std::make_unique<store_type>(Root{}, make_document_id(), store_options_);
    bind_store(*replacement);
    replacement->load(bytes);
    auto entities = collect(*replacement->read());
    const auto document = replacement->document();
    store_.swap(replacement);
    entities_.swap(entities);
    domain_.document_high = document.high;
    domain_.document_low = document.low;
  }

  // Enable existing synchronous document durability; accepted outcomes follow the journal flush.
  void create_journal(const std::filesystem::path& path,
                      journal_storage_mode mode = journal_storage_mode::appended,
                      journal_options options = {})
    requires(store_type::has_journal)
  {
    operation_scope guard{*this};
    try {
      store_->create_journal(path, mode, std::move(options));

    } catch (const journal_indeterminate_error&) {
      journal_state_.fenced = true;
      throw;
    }
  }

  // Publish a full document Save without changing the collaboration cursor or temporary rights.
  void save_journal()
    requires(store_type::has_journal)
  {
    operation_scope guard{*this};
    try {
      store_->save_journal();
    } catch (const journal_indeterminate_error&) {
      journal_state_.fenced = true;
      throw;
    }
  }

  // Reopen durable document state in a fresh authority before binding sessions or accepting edits.
  void recover_journal(const std::filesystem::path& path, journal_options options = {})
    requires(store_type::has_journal)
  {
    operation_scope guard{*this};
    if (!sessions_.empty() || sequence_ != 0 || store_->journal_state_.sink) {
      throw std::logic_error{"Recover collaboration document before opening sessions"};
    }
    auto replacement = std::make_unique<store_type>(Root{}, make_document_id(), store_options_);
    bind_store(*replacement);
    replacement->recover_journal(path, std::move(options));
    auto entities = collect(*replacement->read());
    const auto document = replacement->document();
    store_.swap(replacement);
    entities_.swap(entities);
    domain_.document_high = document.high;
    domain_.document_low = document.low;
  }
};

// Accepted-state replica with isolated proposal authoring. Host transport must authenticate the
// authority; authoring a proposal never changes acknowledged state or grants edit permission.
template <typename Root, typename Traits = model_traits<Root>, typename Session = std::uint64_t,
          typename SessionTraits = collaboration_session_traits<Session>>
class collaboration_replica {
public:
  using session_type = Session;
  using records_type = collaboration_record_types<Session, SessionTraits>;
  using change_proposal_type = typename records_type::change_proposal;
  using accepted_change_type = typename records_type::accepted_change;
  using lock_request_type = typename records_type::lock_request;
  using lock_grant_type = typename records_type::lock_grant;
  using lock_update_type = typename records_type::lock_update;
  using lock_snapshot_type = typename records_type::lock_snapshot;
  using editing_presence_type = typename records_type::editing_presence;

private:
  using store_type = model_store<Root, history_mode::disabled, history_labels::disabled, Traits,
                                 store_features::none>;
  store_type store_;
  store_options options_{};
  collaboration::domain domain_{};
  std::uint64_t sequence_{};
  std::vector<std::uint8_t> last_batch_{};
  std::uint64_t allocated_id_{};
  bool initialized_{};

  // Validate protocol/schema/ID widths before constructing a bounded ordinary managed envelope.
  std::vector<std::uint8_t> envelope(const accepted_change_type& batch) const {
    const auto& context = batch.context;
    if (!detail::valid_collaboration_protocol<records_type>(context.protocol_version) ||
        context.schema_id != Traits::schema_id || context.epoch == 0 ||
        context.id_bits != std::numeric_limits<typename Traits::id_type>::digits ||
        batch.snapshot.size() > options_.max_snapshot_bytes) {
      throw std::invalid_argument{"Incompatible collaboration snapshot"};
    }
    records::unlabeled_state_envelope value;
    value.format_version = 3;
    value.schema_id = context.schema_id;
    value.id_bits = context.id_bits;
    value.document_high = context.document_high;
    value.document_low = context.document_low;
    value.allocated_id = batch.allocated_id;
    value.mode = static_cast<std::uint32_t>(history_mode::disabled);
    value.current_snapshot = batch.snapshot;
    return detail::encode(value, options_.decode.max_input_bytes);
  }

public:
  // Prepare a receiver with explicit decode budgets; synchronized snapshots replace its placeholder.
  explicit collaboration_replica(store_options options = {})
      : store_{Root{}, make_document_id(), options}, options_{options} {}

  // Edit an isolated draft with generated editors and encode a proposal without changing this replica.
  // IDs created in the callback are provisional until acceptance. Keep the returned proposal for
  // exact retries; refresh the replica and use a new operation ID after a conflict. Callback errors
  // and explicit revert produce no proposal. The host still binds sessions and sends the result.
  template <typename Callable>
    requires std::same_as<std::invoke_result_t<Callable, typename store_type::transaction_edit&>,
                          void>
  change_proposal_type propose(const session_type& session, std::uint64_t operation,
                               Callable&& callback,
                               std::vector<collaboration::grant_reference> grants = {},
                               collaboration_options options = {}) const {
    static_cast<void>(store_.read());
    if (!initialized_ || domain_.protocol_version != records_type::model_protocol ||
        !SessionTraits::valid(session) || operation == 0 || grants.size() > options.max_locks) {
      throw std::invalid_argument{"Invalid collaboration proposal authoring context"};
    }
    // Capture the base before running application code; even a reentrant delivery cannot rebase it.
    const auto context = domain_;
    const auto base_sequence = sequence_;
    const auto base_allocated_id = allocated_id_;
    // The placeholder belongs to a fresh document: its default graph may assign the same numeric
    // IDs to different entity types. Loading the real baseline establishes the draft's namespace.
    store_type draft{Root{}, make_document_id(), options_};
    draft.load(store_.save());
    const auto outcome = draft.execute_transaction(std::forward<Callable>(callback));
    outcome.throw_if_failed();
    if (outcome.status == transaction_status::reverted) {
      throw std::logic_error{"Collaboration proposal was reverted"};
    }
    const auto allocated_id = draft.allocated_id();
    if (allocated_id - base_allocated_id > options.max_created_ids) {
      throw std::length_error{"Collaboration proposal identity budget exceeded"};
    }
    collaboration::model_change change{collaboration_model_change_version, base_allocated_id,
                                       allocated_id,
                                       detail::encode(*draft.read(), options_.max_snapshot_bytes)};
    return {context,
            session,
            operation,
            base_sequence,
            encode_collaboration_record(change, options.max_command_bytes),
            std::move(grants)};
  }

  // Build an undo/redo request with no client-authored inverse; retain bytes for exact retries.
  // A zero target chooses the authority's latest visible stack tip. The host binds the session.
  change_proposal_type
  history_request(const session_type& session, std::uint64_t operation,
                  collaboration_history_action action, std::uint64_t target_operation = 0,
                  std::vector<collaboration::grant_reference> grants = {}) const {
    static_cast<void>(store_.read());
    if (!initialized_ || domain_.protocol_version != records_type::model_protocol ||
        !SessionTraits::valid(session) || operation == 0 ||
        (action != collaboration_history_action::undo &&
         action != collaboration_history_action::redo)) {
      throw std::invalid_argument{"Invalid collaborative history authoring context"};
    }
    collaboration::model_change change;
    change.format_version = collaboration_history_change_version;
    change.history_action = static_cast<std::uint32_t>(action);
    change.target_operation = target_operation;
    return {domain_,          session, operation, sequence_, encode_collaboration_record(change),
            std::move(grants)};
  }

  // Request one atomic, conditional reversal of this session's latest accepted edit.
  change_proposal_type undo(const session_type& session, std::uint64_t operation,
                            std::uint64_t target_operation = 0,
                            std::vector<collaboration::grant_reference> grants = {}) const {
    return history_request(session, operation, collaboration_history_action::undo, target_operation,
                           std::move(grants));
  }

  // Request reversal of the latest accepted undo; normal authored edits clear that redo stack.
  change_proposal_type redo(const session_type& session, std::uint64_t operation,
                            std::uint64_t target_operation = 0,
                            std::vector<collaboration::grant_reference> grants = {}) const {
    return history_request(session, operation, collaboration_history_action::redo, target_operation,
                           std::move(grants));
  }

  // Establish/re-establish a trusted snapshot boundary, explicitly allowing a fenced epoch change.
  void synchronize(const accepted_change_type& batch,
                   const collaboration::domain& trusted_authority) {
    static_cast<void>(store_.read());
    if (!detail::same_domain(batch.context, trusted_authority) ||
        (initialized_ && (batch.context.document_high != domain_.document_high ||
                          batch.context.document_low != domain_.document_low)) ||
        (initialized_ && batch.allocated_id < allocated_id_) ||
        (initialized_ && detail::same_domain(batch.context, domain_) &&
         (batch.sequence < sequence_ ||
          (batch.sequence == sequence_ && batch.snapshot != last_batch_)))) {
      throw std::invalid_argument{"Untrusted or regressing collaboration baseline"};
    }
    auto state = envelope(batch);
    auto domain = batch.context;
    auto bytes = batch.snapshot;
    store_.load(state);
    std::swap(domain_, domain);
    last_batch_.swap(bytes);
    sequence_ = batch.sequence;
    allocated_id_ = batch.allocated_id;
    initialized_ = true;
  }

  // Apply exactly the next accepted change; gaps require reconciliation and never partially apply.
  replication_status apply_accepted_change(const accepted_change_type& batch,
                                           const collaboration::domain& trusted_authority) {
    static_cast<void>(store_.read());
    if (!initialized_ || !detail::same_domain(batch.context, trusted_authority) ||
        !detail::same_domain(batch.context, domain_)) {
      throw std::invalid_argument{"Untrusted collaboration acceptance"};
    }
    if (batch.snapshot.size() > options_.max_snapshot_bytes) {
      throw std::length_error{"Collaboration snapshot budget exceeded"};
    }
    if (batch.sequence <= sequence_) {
      if (batch.sequence == sequence_ && batch.snapshot != last_batch_) {
        throw std::invalid_argument{"Conflicting accepted sequence"};
      }
      return replication_status::duplicate;
    }
    if (sequence_ == std::numeric_limits<std::uint64_t>::max() || batch.sequence != sequence_ + 1) {
      return replication_status::resync_required;
    }
    if (batch.allocated_id < allocated_id_) {
      throw std::invalid_argument{"Collaboration allocator watermark regressed"};
    }
    auto state = envelope(batch);
    auto bytes = batch.snapshot;
    store_.load(state);
    last_batch_.swap(bytes);
    sequence_ = batch.sequence;
    allocated_id_ = batch.allocated_id;
    return replication_status::applied;
  }

  // Pin acknowledged state; replication never requires local authored-edit permission.
  auto read() const {
    return store_.read();
  }

  // Return the last acknowledged model cursor for catch-up requests.
  std::uint64_t sequence() const {
    static_cast<void>(store_.read());
    return sequence_;
  }
};

// Replica lock state is advisory; a missing grant in an incomplete cache conveys no edit right.
template <typename Session = std::uint64_t,
          typename SessionTraits = collaboration_session_traits<Session>>
class basic_collaboration_lock_cache {
public:
  using session_type = Session;
  using records_type = collaboration_record_types<Session, SessionTraits>;
  using change_proposal_type = typename records_type::change_proposal;
  using accepted_change_type = typename records_type::accepted_change;
  using lock_request_type = typename records_type::lock_request;
  using lock_grant_type = typename records_type::lock_grant;
  using lock_update_type = typename records_type::lock_update;
  using lock_snapshot_type = typename records_type::lock_snapshot;
  using editing_presence_type = typename records_type::editing_presence;

private:
  collaboration::domain domain_{};
  std::map<std::uint64_t, lock_grant_type> grants_{};
  std::uint64_t sequence_{};
  bool initialized_{};
  bool complete_{};
  std::size_t max_locks_{};

public:
  // Bound cached grants independently of the transport decoder's collection limits.
  explicit basic_collaboration_lock_cache(std::size_t max_locks = collaboration_options{}.max_locks)
      : max_locks_{max_locks} {}

  // Atomically replace the table at a trusted snapshot boundary.
  void synchronize(const lock_snapshot_type& snapshot,
                   const collaboration::domain& trusted_authority) {
    if (!detail::same_domain(snapshot.context, trusted_authority) ||
        !detail::valid_collaboration_protocol<records_type>(snapshot.context.protocol_version) ||
        snapshot.context.epoch == 0 || snapshot.grants.size() > max_locks_ ||
        (initialized_ && detail::same_domain(snapshot.context, domain_) &&
         snapshot.sequence < sequence_)) {
      throw std::invalid_argument{"Invalid collaboration lock baseline"};
    }
    std::map<std::uint64_t, lock_grant_type> grants;
    for (const auto& grant : snapshot.grants) {
      if (grant.target == 0 || !SessionTraits::valid(grant.session) || grant.generation == 0 ||
          grant.generation > snapshot.sequence || !detail::valid_collaboration_scope(grant.scope) ||
          !grants.emplace(grant.target, grant).second) {
        throw std::invalid_argument{"Invalid collaboration cached grant"};
      }
    }
    auto domain = snapshot.context;
    grants_.swap(grants);
    std::swap(domain_, domain);
    sequence_ = snapshot.sequence;
    initialized_ = complete_ = true;
  }

  // Apply ordered authority transitions. A gap permanently marks this cache incomplete until sync.
  replication_status receive_lock_update(const lock_update_type& event,
                                         const collaboration::domain& trusted_authority) {
    if (!initialized_ || !detail::same_domain(event.context, trusted_authority) ||
        !detail::same_domain(event.context, domain_)) {
      throw std::invalid_argument{"Untrusted collaboration lock update"};
    }
    if (event.sequence <= sequence_) {
      return replication_status::duplicate;
    }
    if (!complete_ || sequence_ == std::numeric_limits<std::uint64_t>::max() ||
        event.sequence != sequence_ + 1) {
      complete_ = false;
      return replication_status::resync_required;
    }
    const auto& grant = event.grant;
    const auto found = grants_.find(grant.target);
    if (event.action == static_cast<std::uint32_t>(lock_update_action::grant)) {
      if (grant.target == 0 || !SessionTraits::valid(grant.session) ||
          grant.generation != event.sequence || !detail::valid_collaboration_scope(grant.scope) ||
          found != grants_.end() || grants_.size() >= max_locks_) {
        throw std::invalid_argument{"Invalid collaboration grant event"};
      }
      grants_.emplace(grant.target, grant);
    } else {
      if (event.action < static_cast<std::uint32_t>(lock_update_action::release) ||
          event.action > static_cast<std::uint32_t>(lock_update_action::revoke) ||
          found == grants_.end() || found->second.generation != grant.generation ||
          (SessionTraits::less(found->second.session, grant.session) ||
           SessionTraits::less(grant.session, found->second.session)) ||
          found->second.scope != grant.scope) {
        throw std::invalid_argument{"Invalid collaboration removal event"};
      }
      grants_.erase(found);
    }
    sequence_ = event.sequence;
    return replication_status::applied;
  }

  // Report whether the observed stream is continuous, never whether an edit will be accepted.
  bool complete() const noexcept {
    return complete_;
  }

  // Borrow the current advisory table until the next cache mutation.
  const auto& grants() const noexcept {
    return grants_;
  }
};

using collaboration_lock_cache = basic_collaboration_lock_cache<>;

// Group session-specific APIs without changing existing model/history template argument positions.
// Applications own ID creation, connection binding and lifetime; this facade adds no session registry.
template <typename Session = std::uint64_t,
          typename SessionTraits = collaboration_session_traits<Session>>
struct collaboration_session {
  using session_type = Session;
  using records = collaboration_record_types<Session, SessionTraits>;
  using result = basic_collaboration_result<Session, SessionTraits>;
  using lock_cache = basic_collaboration_lock_cache<Session, SessionTraits>;
  template <typename Root, history_mode Mode = history_mode::linear,
            history_labels Labels = history_labels::disabled, typename Traits = model_traits<Root>,
            store_features Features = store_features::all>
  using authority =
      collaboration_authority<Root, Mode, Labels, Traits, Session, SessionTraits, Features>;
  template <typename Root, typename Traits = model_traits<Root>>
  using replica = collaboration_replica<Root, Traits, Session, SessionTraits>;
};

} // namespace rohit::managed

#include <rohit/managed_local_collaboration.hpp>
