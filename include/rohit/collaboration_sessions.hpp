#pragma once

#include <rohit/collaboration_records.hpp>
#include <rohit/managed.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed {

namespace detail {
// Encode a scalar or application value with the ordinary binary codec and a fixed byte budget.
template <typename Value>
std::vector<std::uint8_t> encode_collaboration_session_value(const Value& value,
                                                             std::size_t max_bytes) {
  if (max_bytes == 0) {
    throw std::length_error{"Collaboration session encoding budget is zero"};
  }
  const stream_limits limits{std::min(stream_limits{}.min_read_buffer_bytes, max_bytes), max_bytes};
  full_stream_auto_alloc_limits stream{&limits};
  serializer::binary_integer<serializer::serialize_type::out, full_stream_auto_alloc_limits>
      encoder{stream};
  encoder.struct_serialize_out(value);
  return {stream.begin(), stream.begin() + stream.current_offset()};
}
} // namespace detail

// Applications specialize this policy, or pass their own policy alongside a session type.
// IDs are application-owned values; the runtime never allocates or maps them to numeric handles.
template <typename Session>
struct collaboration_session_traits;

// Unsigned IDs retain their exact width in the custom-session codec; zero is reserved by default.
template <std::unsigned_integral Session>
  requires(!std::same_as<Session, bool>)
struct collaboration_session_traits<Session> {
  static constexpr std::string_view wire_name = [] {
    if constexpr (sizeof(Session) == sizeof(std::uint64_t)) {
      return std::string_view{"serializer.session.uint64.v1"};
    } else if constexpr (sizeof(Session) == sizeof(std::uint32_t)) {
      return std::string_view{"serializer.session.uint32.v1"};
    } else if constexpr (sizeof(Session) == sizeof(std::uint16_t)) {
      return std::string_view{"serializer.session.uint16.v1"};
    } else {
      static_assert(sizeof(Session) == sizeof(std::uint8_t));
      return std::string_view{"serializer.session.uint8.v1"};
    }
  }();
  static constexpr std::size_t max_encoded_bytes = sizeof(Session);
  // Reserve zero for an absent origin in default numeric sessions.
  static bool valid(Session value) noexcept {
    return value != 0;
  }
  // Order values consistently with equality for session and retry tables.
  static bool less(Session left, Session right) noexcept {
    return left < right;
  }
  // Encode the exact unsigned value without narrowing or allocating a registry handle.
  static std::vector<std::uint8_t> encode(Session value) {
    return detail::encode_collaboration_session_value(value, max_encoded_bytes);
  }
  // Decode one exact scalar within caller-supplied limits.
  static Session decode(std::span<const std::uint8_t> bytes, serializer::decode_limits limits) {
    return detail::decode<Session>(bytes, limits);
  }
};

// String sessions use their complete byte value; empty is reserved by this default policy.
template <>
struct collaboration_session_traits<std::string> {
  static constexpr std::string_view wire_name = "serializer.session.string.v1";
  static constexpr std::size_t max_encoded_bytes = 4096;
  // Exclude empty and oversized application session keys before retaining them.
  static bool valid(const std::string& value) noexcept {
    return !value.empty() && value.size() <= max_encoded_bytes &&
           serializer::detail::fixed_prefix_bytes(static_cast<std::uint32_t>(value.size())) <=
               max_encoded_bytes - value.size();
  }
  // Preserve exact case-sensitive string identity.
  static bool less(const std::string& left, const std::string& right) noexcept {
    return left < right;
  }
  // Use the normal bounded string codec, never a truncated hash.
  static std::vector<std::uint8_t> encode(const std::string& value) {
    return detail::encode_collaboration_session_value(value, max_encoded_bytes);
  }
  // Require exact input consumption with the transport's decoding limits.
  static std::string decode(std::span<const std::uint8_t> bytes, serializer::decode_limits limits) {
    return detail::decode<std::string>(bytes, limits);
  }
};

namespace detail {

// Require owning, copyable IDs and an explicit bounded codec; session allocation remains external.
template <typename Session, typename Policy>
concept collaboration_session_policy =
    std::default_initializable<Session> && std::copyable<Session> &&
    requires(const Session& value, std::span<const std::uint8_t> bytes,
             serializer::decode_limits limits) {
      { Policy::wire_name } -> std::convertible_to<std::string_view>;
      { Policy::max_encoded_bytes } -> std::convertible_to<std::size_t>;
      { Policy::valid(value) } -> std::same_as<bool>;
      { Policy::less(value, value) } -> std::same_as<bool>;
      { Policy::encode(value) } -> std::same_as<std::vector<std::uint8_t>>;
      { Policy::decode(bytes, limits) } -> std::same_as<Session>;
    };

// Use policy ordering for all identity comparisons, including application types without operators.
template <typename Session, typename Policy>
struct collaboration_session_less {
  // Provide a stable strict weak ordering for the lifetime of the authority.
  bool operator()(const Session& left, const Session& right) const {
    return Policy::less(left, right);
  }
};

// Pair the application session with a numeric operation/presence counter.
template <typename Session, typename Policy>
struct collaboration_operation_less {
  // Keep retry keys scoped to the complete application-defined session.
  bool operator()(const std::pair<Session, std::uint64_t>& left,
                  const std::pair<Session, std::uint64_t>& right) const {
    if (Policy::less(left.first, right.first)) {
      return true;
    }
    if (Policy::less(right.first, left.first)) {
      return false;
    }
    return left.second < right.second;
  }
};

// Typed facades project into generated records plus a generated session envelope.
// Numeric session fields in the inner record are always zero; no wire field IDs are duplicated here.
template <typename Session, typename Policy>
struct typed_collaboration_records {
  struct change_proposal {
    using session_traits = Policy;
    using wire_type = collaboration_records::change_proposal;
    collaboration_records::domain context{};
    Session session{};
    std::uint64_t operation{};
    std::uint64_t base_sequence{};
    std::vector<std::uint8_t> command{};
    std::vector<collaboration_records::grant_reference> grants{};
    // Project ordinary fields through their canonical generated wire record.
    wire_type wire() const {
      return {context, 0, operation, base_sequence, command, grants};
    }
    // Supply the application identity separately from the legacy numeric placeholder.
    std::vector<Session> session_values() const {
      return {session};
    }
    // Reconstruct only the expected session count and reject a second numeric identity.
    static change_proposal from_wire(wire_type value, std::vector<Session> sessions) {
      if (value.session != 0 || sessions.size() != 1) {
        throw std::invalid_argument{"Invalid typed collaboration session record"};
      }
      return {std::move(value.context),   std::move(sessions.front()),
              std::move(value.operation), std::move(value.base_sequence),
              std::move(value.command),   std::move(value.grants)};
    }
  };
  struct accepted_change {
    using session_traits = Policy;
    using wire_type = collaboration_records::accepted_change;
    collaboration_records::domain context{};
    std::uint64_t sequence{};
    Session session{};
    std::uint64_t operation{};
    std::uint64_t allocated_id{};
    std::vector<std::uint8_t> snapshot{};
    collaboration_records::transaction_record history{};
    // Project ordinary fields through their canonical generated wire record.
    wire_type wire() const {
      return {context, sequence, 0, operation, allocated_id, snapshot, history};
    }
    // Supply the application identity separately from the legacy numeric placeholder.
    std::vector<Session> session_values() const {
      return {session};
    }
    // Reconstruct only the expected session count and reject a second numeric identity.
    static accepted_change from_wire(wire_type value, std::vector<Session> sessions) {
      if (value.session != 0 || sessions.size() != 1) {
        throw std::invalid_argument{"Invalid typed collaboration session record"};
      }
      return {std::move(value.context),      std::move(value.sequence),
              std::move(sessions.front()),   std::move(value.operation),
              std::move(value.allocated_id), std::move(value.snapshot),
              std::move(value.history)};
    }
  };
  struct lock_request {
    using session_traits = Policy;
    using wire_type = collaboration_records::lock_request;
    collaboration_records::domain context{};
    Session session{};
    std::uint64_t operation{};
    std::uint32_t action{};
    std::uint64_t target{};
    std::uint32_t scope{};
    std::uint64_t generation{};
    std::uint64_t duration_ms{};
    // Project ordinary fields through their canonical generated wire record.
    wire_type wire() const {
      return {context, 0, operation, action, target, scope, generation, duration_ms};
    }
    // Supply the application identity separately from the legacy numeric placeholder.
    std::vector<Session> session_values() const {
      return {session};
    }
    // Reconstruct only the expected session count and reject a second numeric identity.
    static lock_request from_wire(wire_type value, std::vector<Session> sessions) {
      if (value.session != 0 || sessions.size() != 1) {
        throw std::invalid_argument{"Invalid typed collaboration session record"};
      }
      return {std::move(value.context),    std::move(sessions.front()), std::move(value.operation),
              std::move(value.action),     std::move(value.target),     std::move(value.scope),
              std::move(value.generation), std::move(value.duration_ms)};
    }
  };
  struct lock_grant {
    using session_traits = Policy;
    using wire_type = collaboration_records::lock_grant;
    std::uint64_t target{};
    std::uint32_t scope{};
    Session session{};
    std::uint64_t generation{};
    // Project ordinary fields through their canonical generated wire record.
    wire_type wire() const {
      return {target, scope, 0, generation};
    }
    // Supply the application identity separately from the legacy numeric placeholder.
    std::vector<Session> session_values() const {
      return {session};
    }
    // Reconstruct only the expected session count and reject a second numeric identity.
    static lock_grant from_wire(wire_type value, std::vector<Session> sessions) {
      if (value.session != 0 || sessions.size() != 1) {
        throw std::invalid_argument{"Invalid typed collaboration session record"};
      }
      return {std::move(value.target), std::move(value.scope), std::move(sessions.front()),
              std::move(value.generation)};
    }
  };
  struct editing_presence {
    using session_traits = Policy;
    using wire_type = collaboration_records::editing_presence;
    collaboration_records::domain context{};
    Session session{};
    std::uint64_t presence{};
    std::uint64_t sequence{};
    std::uint64_t target{};
    std::uint32_t action{};
    std::string preview{};
    // Project ordinary fields through their canonical generated wire record.
    wire_type wire() const {
      return {context, 0, presence, sequence, target, action, preview};
    }
    // Supply the application identity separately from the legacy numeric placeholder.
    std::vector<Session> session_values() const {
      return {session};
    }
    // Reconstruct only the expected session count and reject a second numeric identity.
    static editing_presence from_wire(wire_type value, std::vector<Session> sessions) {
      if (value.session != 0 || sessions.size() != 1) {
        throw std::invalid_argument{"Invalid typed collaboration session record"};
      }
      return {std::move(value.context),  std::move(sessions.front()), std::move(value.presence),
              std::move(value.sequence), std::move(value.target),     std::move(value.action),
              std::move(value.preview)};
    }
  };

  struct lock_update {
    using session_traits = Policy;
    using wire_type = collaboration_records::lock_update;
    collaboration_records::domain context{};
    std::uint64_t sequence{};
    std::uint32_t action{};
    lock_grant grant{};
    // Preserve the generated lock update layout and its nested numeric placeholder.
    wire_type wire() const {
      return {context, sequence, action, grant.wire()};
    }
    // Carry the nested grant owner's complete application identity.
    std::vector<Session> session_values() const {
      return {grant.session};
    }
    // Decode the nested grant under the same session policy.
    static lock_update from_wire(wire_type value, std::vector<Session> sessions) {
      return {std::move(value.context), value.sequence, value.action,
              lock_grant::from_wire(std::move(value.grant), std::move(sessions))};
    }
  };
  struct lock_snapshot {
    using session_traits = Policy;
    using wire_type = collaboration_records::lock_snapshot;
    collaboration_records::domain context{};
    std::uint64_t sequence{};
    std::vector<lock_grant> grants{};
    // Preserve grant order so each generated record has exactly one corresponding session.
    wire_type wire() const {
      wire_type value{context, sequence, {}};
      value.grants.reserve(grants.size());
      for (const auto& grant : grants) {
        value.grants.push_back(grant.wire());
      }
      return value;
    }
    // Encode every grant owner, including repeated owners, in the same order as the grants.
    std::vector<Session> session_values() const {
      std::vector<Session> sessions;
      sessions.reserve(grants.size());
      for (const auto& grant : grants) {
        sessions.push_back(grant.session);
      }
      return sessions;
    }
    // Reject mismatched owner counts before constructing a typed cache baseline.
    static lock_snapshot from_wire(wire_type value, std::vector<Session> sessions) {
      if (value.grants.size() != sessions.size()) {
        throw std::invalid_argument{"Invalid typed collaboration lock snapshot"};
      }
      lock_snapshot result{std::move(value.context), value.sequence, {}};
      result.grants.reserve(value.grants.size());
      for (std::size_t index = 0; index < sessions.size(); ++index) {
        result.grants.push_back(
            lock_grant::from_wire(std::move(value.grants[index]), {std::move(sessions[index])}));
      }
      return result;
    }
  };
};
} // namespace detail

// History-bearing protocols fence older peers whose keyed readers reject added record fields.
template <typename Session = std::uint64_t, typename Policy = collaboration_session_traits<Session>>
  requires detail::collaboration_session_policy<Session, Policy>
struct collaboration_record_types : detail::typed_collaboration_records<Session, Policy> {
  static constexpr std::uint32_t command_protocol = 7;
  static constexpr std::uint32_t model_protocol = 8;
};
template <>
struct collaboration_record_types<std::uint64_t, collaboration_session_traits<std::uint64_t>> {
  using change_proposal = collaboration_records::change_proposal;
  using accepted_change = collaboration_records::accepted_change;
  using lock_request = collaboration_records::lock_request;
  using lock_grant = collaboration_records::lock_grant;
  using lock_update = collaboration_records::lock_update;
  using lock_snapshot = collaboration_records::lock_snapshot;
  using editing_presence = collaboration_records::editing_presence;
  static constexpr std::uint32_t command_protocol = 5;
  static constexpr std::uint32_t model_protocol = 6;
};

} // namespace rohit::managed
