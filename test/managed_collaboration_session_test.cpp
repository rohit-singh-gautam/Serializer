#include <gtest/gtest.h>
#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using string_sessions = managed::collaboration_session<std::string>;

// Exercise the public codec, including typed envelopes and exact session decoding.
template <typename Record>
Record round_trip(const Record& value) {
  return managed::decode_collaboration_record<Record>(managed::encode_collaboration_record(value));
}

// An application-owned identifier deliberately has no numeric conversion or comparison operators.
struct app_session {
  std::string key{};
};
struct app_session_policy {
  static constexpr std::string_view wire_name = "serializer.example.app-session.v1";
  static constexpr std::size_t max_encoded_bytes = 4096;
  // This application allows the empty key; registration, rather than a sentinel, establishes liveness.
  static bool valid(const app_session& value) {
    return value.key.size() < 100;
  }
  // Define the complete identity without requiring operators on the application's value type.
  static bool less(const app_session& left, const app_session& right) {
    return left.key < right.key;
  }
  // Reuse a bounded built-in codec while keeping an application-specific wire-format name.
  static std::vector<std::uint8_t> encode(const app_session& value) {
    return managed::collaboration_session_traits<std::string>::encode(value.key);
  }
  // Restore the exact application key using the caller's limits.
  static app_session decode(std::span<const std::uint8_t> bytes,
                            rohit::serializer::decode_limits limits) {
    return {managed::collaboration_session_traits<std::string>::decode(bytes, limits)};
  }
};
} // namespace

// Applications register their own string IDs; every accepted origin and retry retains that value.
TEST(managed_collaboration_session, string_changes_retries_and_trusted_binding) {
  string_sessions::authority<ledger> authority{ledger{"Shared"}, 1};
  authority.open_session("desktop");
  authority.open_session("viewer", false);
  string_sessions::replica<ledger> replica;
  replica.synchronize(round_trip(authority.snapshot()), authority.context());
  const auto proposal = round_trip(
      replica.propose("desktop", 1, [](auto& edit) { edit.root().set_name("Updated"); }));
  EXPECT_THROW(authority.submit_change(proposal, "viewer", 0), std::invalid_argument);
  const auto result = authority.submit_change(proposal, "desktop", 0);
  ASSERT_EQ(result->status, managed::collaboration_status::accepted);
  EXPECT_EQ(result->accepted->session, "desktop");
  EXPECT_EQ(authority.submit_change(proposal, "desktop", 0), result);
  EXPECT_EQ(replica.apply_accepted_change(round_trip(*result->accepted), authority.context()),
            managed::replication_status::applied);
  EXPECT_EQ(replica.read()->name, "Updated");
  const auto denied =
      replica.propose("viewer", 1, [](auto& edit) { edit.root().set_name("Denied"); });
  EXPECT_EQ(authority.submit_change(denied, "viewer", 0)->status,
            managed::collaboration_status::denied);
  EXPECT_THROW(authority.open_session(""), std::invalid_argument);
  EXPECT_ANY_THROW(authority.open_session(std::string(1, static_cast<char>(0xff))));
  EXPECT_THROW(authority.open_session("desktop"), std::invalid_argument);
  authority.close_session("desktop");
  EXPECT_THROW(authority.open_session("desktop"), std::invalid_argument);
  EXPECT_THROW(authority.submit_change(proposal, "desktop", 0), std::invalid_argument);
  authority.open_session("desktop-new-lifetime");
}

// Lock grants, cached events, presence and shutdown all use the same application identity.
TEST(managed_collaboration_session, string_locks_presence_and_cache) {
  string_sessions::authority<ledger> authority{ledger{"Shared"}, 1};
  authority.open_session("left");
  authority.open_session("right");
  string_sessions::lock_cache cache;
  cache.synchronize(round_trip(authority.lock_snapshot()), authority.context());
  string_sessions::records::lock_request request{authority.context(), "left", 1, 1, 1, 2, 0, 1000};
  const auto grant = authority.change_lock(round_trip(request), "left", 0);
  ASSERT_EQ(grant->status, managed::collaboration_status::accepted);
  EXPECT_EQ(round_trip(grant->grant).session, "left");
  for (const auto& event : authority.locks_since(0)) {
    cache.receive_lock_update(round_trip(*event), authority.context());
  }
  EXPECT_EQ(cache.grants().at(1).session, "left");
  cache.synchronize(round_trip(authority.lock_snapshot()), authority.context());
  string_sessions::records::editing_presence presence{
      authority.context(), "left", 1, 1, 1, 1, "typing"};
  EXPECT_TRUE(authority.receive_presence(round_trip(presence), "left", 0));
  EXPECT_EQ(authority.editing_presence().front().session, "left");
  string_sessions::replica<ledger> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto edit = [](auto& tx) { tx.root().set_name("Locked edit"); };
  EXPECT_EQ(authority.submit_change(replica.propose("right", 1, edit), "right", 0)->status,
            managed::collaboration_status::conflict);
  const auto proposal = replica.propose("left", 2, edit, {{1, grant->grant.generation}});
  EXPECT_EQ(authority.submit_change(round_trip(proposal), "left", 0)->status,
            managed::collaboration_status::accepted);
  authority.close_session("left");
  for (const auto& event : authority.locks_since(1)) {
    cache.receive_lock_update(round_trip(*event), authority.context());
  }
  EXPECT_TRUE(cache.grants().empty());
  EXPECT_TRUE(authority.editing_presence().empty());
}

// Smaller unsigned IDs keep their type and full range, while default uint64 messages stay unchanged.
TEST(managed_collaboration_session, uint32_and_default_compatibility) {
  using sessions = managed::collaboration_session<std::uint32_t>;
  constexpr auto session = std::numeric_limits<std::uint32_t>::max();
  sessions::authority<ledger> authority{ledger{"Shared"}, 1};
  authority.open_session(session);
  sessions::replica<ledger> replica;
  replica.synchronize(round_trip(authority.snapshot()), authority.context());
  const auto proposal =
      round_trip(replica.propose(session, 1, [](auto& edit) { edit.root().set_name("32"); }));
  static_assert(std::same_as<std::remove_cvref_t<decltype(proposal.session)>, std::uint32_t>);
  ASSERT_EQ(authority.submit_change(proposal, session, 0)->status,
            managed::collaboration_status::accepted);
  static_assert(std::same_as<managed::collaboration_session<>::records::change_proposal,
                             managed::collaboration::change_proposal>);
  const managed::collaboration::change_proposal original{
      {1, "schema", 32, 1, 2, 1}, 42, 1, 0, {}, {}};
  EXPECT_EQ(managed::encode_collaboration_record(original), managed::detail::encode(original));
  managed::collaboration_replica<ledger> wrong_type;
  EXPECT_THROW(wrong_type.synchronize(authority.snapshot().wire(), authority.context()),
               std::invalid_argument);
}

// Policy hooks see the original custom type and can choose their own valid default value.
TEST(managed_collaboration_session, application_policy_and_custom_commands) {
  using sessions = managed::collaboration_session<app_session, app_session_policy>;
  const app_session local{""};
  const app_session other{"other"};
  bool policy_called = false;
  bool lock_policy_called = false;
  sessions::authority<ledger> authority{
      ledger{"Shared"},
      1,
      managed::document_id{1, 2},
      {},
      {},
      [&](const app_session& session, const auto&, const auto&, const auto&) {
        policy_called = true;
        return session.key.empty();
      },
      [&](const app_session& session, auto, auto) {
        lock_policy_called = true;
        return session.key.empty();
      }};
  authority.open_session(local);
  authority.open_session(other);
  sessions::replica<ledger> replica;
  replica.synchronize(round_trip(authority.snapshot()), authority.context());
  const auto proposal =
      round_trip(replica.propose(local, 1, [](auto& edit) { edit.root().set_name("Custom"); }));
  EXPECT_THROW(authority.submit_change(proposal, other, 0), std::invalid_argument);
  ASSERT_EQ(authority.submit_change(proposal, local, 0)->status,
            managed::collaboration_status::accepted);
  EXPECT_TRUE(policy_called);
  EXPECT_TRUE(authority.accepted_since(0).front()->session.key.empty());
  sessions::lock_cache cache;
  cache.synchronize(round_trip(authority.lock_snapshot()), authority.context());
  const auto granted =
      authority.change_lock({authority.context(), local, 2, 1, 1, 2, 0, 1000}, local, 0);
  ASSERT_EQ(granted->status, managed::collaboration_status::accepted);
  EXPECT_TRUE(lock_policy_called);
  for (const auto& event : authority.locks_since(0)) {
    cache.receive_lock_update(round_trip(*event), authority.context());
  }
  EXPECT_TRUE(cache.grants().at(1).session.key.empty());
  authority.close_session(local);
  for (const auto& event : authority.locks_since(1)) {
    cache.receive_lock_update(round_trip(*event), authority.context());
  }
  EXPECT_TRUE(cache.grants().empty());
  sessions::authority<ledger> custom{ledger{"Shared"}, 1,
                                     [](auto& edit, auto) { edit.root().set_name("Command"); }};
  custom.open_session(other);
  sessions::records::change_proposal command{custom.context(), other, 1, 0, {}, {}};
  EXPECT_EQ(custom.context().protocol_version, 7u);
  EXPECT_EQ(custom.submit_change(round_trip(command), other, 0)->status,
            managed::collaboration_status::accepted);
}

// Typed wire messages reject mismatched formats, duplicate numeric identities and malformed framing.
TEST(managed_collaboration_session, malformed_and_mismatched_sessions) {
  using record = string_sessions::records::change_proposal;
  record proposal{{4, "schema", 32, 1, 2, 1}, "desktop", 1, 0, {}, {}};
  const auto bytes = managed::encode_collaboration_record(proposal);
  const auto valid =
      managed::decode_collaboration_record<managed::collaboration::session_envelope>(bytes);
  auto malformed = valid;
  malformed.session_format = "unknown";
  EXPECT_THROW(
      managed::decode_collaboration_record<record>(managed::encode_collaboration_record(malformed)),
      std::invalid_argument);
  malformed = valid;
  malformed.sessions.clear();
  EXPECT_THROW(
      managed::decode_collaboration_record<record>(managed::encode_collaboration_record(malformed)),
      std::invalid_argument);
  malformed = valid;
  malformed.sessions.push_back(malformed.sessions.front());
  EXPECT_THROW(
      managed::decode_collaboration_record<record>(managed::encode_collaboration_record(malformed)),
      std::invalid_argument);
  malformed = valid;
  auto inner = managed::decode_collaboration_record<managed::collaboration::change_proposal>(
      malformed.payload);
  inner.session = 99;
  malformed.payload = managed::encode_collaboration_record(inner);
  EXPECT_THROW(
      managed::decode_collaboration_record<record>(managed::encode_collaboration_record(malformed)),
      std::invalid_argument);
  malformed = valid;
  malformed.sessions.front().bytes.push_back(0);
  EXPECT_ANY_THROW(managed::decode_collaboration_record<record>(
      managed::encode_collaboration_record(malformed)));
  using numeric_record = managed::collaboration_session<std::uint32_t>::records::change_proposal;
  EXPECT_THROW(managed::decode_collaboration_record<numeric_record>(bytes), std::invalid_argument);
  auto limits = rohit::serializer::decode_limits{};
  limits.max_input_bytes = bytes.size() - 1;
  EXPECT_ANY_THROW(managed::decode_collaboration_record<record>(bytes, limits));
  malformed = valid;
  malformed.payload.push_back(0);
  EXPECT_ANY_THROW(managed::decode_collaboration_record<record>(
      managed::encode_collaboration_record(malformed)));
  malformed = valid;
  malformed.sessions.front().bytes.resize(
      managed::collaboration_session_traits<std::string>::max_encoded_bytes + 1);
  EXPECT_THROW(
      managed::decode_collaboration_record<record>(managed::encode_collaboration_record(malformed)),
      std::length_error);
  string_sessions::records::lock_snapshot snapshot{
      {4, "schema", 32, 1, 2, 1}, 1, {{1, 2, "owner", 1}}};
  malformed = managed::decode_collaboration_record<managed::collaboration::session_envelope>(
      managed::encode_collaboration_record(snapshot));
  malformed.sessions.clear();
  EXPECT_THROW(managed::decode_collaboration_record<string_sessions::records::lock_snapshot>(
                   managed::encode_collaboration_record(malformed)),
               std::invalid_argument);
  proposal.session.assign(managed::collaboration_session_traits<std::string>::max_encoded_bytes + 1,
                          'x');
  EXPECT_ANY_THROW(managed::encode_collaboration_record(proposal));
}

// Byte and uint16 IDs, embedded NULs and longest supported strings round-trip without hashing.
TEST(managed_collaboration_session, built_in_session_boundaries) {
  using byte_policy = managed::collaboration_session_traits<std::uint8_t>;
  using short_policy = managed::collaboration_session_traits<std::uint16_t>;
  using string_policy = managed::collaboration_session_traits<std::string>;
  EXPECT_EQ(byte_policy::decode(byte_policy::encode(255), {}), 255);
  EXPECT_EQ(short_policy::decode(short_policy::encode(65535), {}), 65535);
  const std::string embedded{"left\0right", 10};
  EXPECT_EQ(string_policy::decode(string_policy::encode(embedded), {}), embedded);
  const std::string longest(
      string_policy::max_encoded_bytes -
          rohit::serializer::detail::fixed_prefix_bytes(string_policy::max_encoded_bytes),
      'x');
  ASSERT_TRUE(string_policy::valid(longest));
  EXPECT_EQ(string_policy::decode(string_policy::encode(longest), {}), longest);
}
