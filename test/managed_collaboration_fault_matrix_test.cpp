// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;

// Force each record through its public bounded codec at the simulated transport boundary.
template <typename Record>
Record wire_copy(const Record& value) {
  return managed::decode_collaboration_record<Record>(managed::encode_collaboration_record(value));
}

// Simulate failures around an authenticated transport; the transport never chooses the trusted session.
template <typename Authority>
class fault_wire_transport {
  Authority& authority_;
  typename Authority::session_type session_;
public:
  bool fail_before_submit{};
  bool fail_after_submit{};
  bool fail_poll{};
  std::vector<std::vector<std::uint8_t>> submissions{};

  // Bind a host-authenticated session independently from the submitted record's claimed origin.
  fault_wire_transport(Authority& authority, typename Authority::session_type session)
      : authority_{authority}, session_{std::move(session)} {}
  // Context is supplied by the bound authority, never inferred from the request.
  auto context() { return wire_copy(authority_.context()); }
  // Baselines use the same actual typed wire envelope as accepted updates.
  auto snapshot() { return wire_copy(authority_.snapshot()); }
  // Poll failure leaves the caller's receive cursor untouched and permits a later exact replay.
  auto accepted_since(std::uint64_t cursor) {
    if (std::exchange(fail_poll, false)) { throw std::runtime_error{"Injected poll failure"}; }
    auto result = authority_.accepted_since(cursor);
    for (auto& event : result) {
      event = std::make_shared<const typename Authority::accepted_change_type>(wire_copy(*event));
    }
    return result;
  }
  // Reserve exact native identity ranges through the trusted authority-owned allocation service.
  auto reserve_ids(std::uint64_t count) { return authority_.reserve_ids(session_, count); }
  // Capture full request bytes and fail on either side of the authoritative acceptance boundary.
  auto submit_change(const typename Authority::change_proposal_type& request) {
    submissions.push_back(managed::encode_collaboration_record(request));
    if (std::exchange(fail_before_submit, false)) { throw std::runtime_error{"Injected offline send"}; }
    auto result = authority_.submit_change(wire_copy(request), session_, 0);
    if (std::exchange(fail_after_submit, false)) { throw std::runtime_error{"Injected lost acknowledgement"}; }
    return result;
  }
};

// Qualify outbox checkpoint retry and fenced authority reconnect with real record codecs.
template <typename Session>
void check_retry_and_epoch_reconnect(Session session) {
  using sessions = managed::collaboration_session<Session>;
  using authority_type = typename sessions::template authority<ledger>;
  using store_type = managed::model_store<ledger>;
  authority_type authority{ledger{"Before"}, 1};
  authority.open_session(session);
  fault_wire_transport transport{authority, session};
  auto client = std::make_unique<store_type>(ledger{});
  client->collaborate(session).bind(transport);
  client->synchronize();
  typename store_type::id_type inserted{};
  client->execute_transaction([&](auto& tx) {
    tx.root().set_name("Accepted");
    inserted = tx.root().entries().insert(10, {"Native identity", 100});
  }).throw_if_failed();
  transport.fail_before_submit = true;
  EXPECT_THROW(client->synchronize(), std::runtime_error);
  EXPECT_EQ(authority.sequence(), 0U);
  const auto offline = client->save();
  client.reset();
  client = std::make_unique<store_type>(ledger{});
  client->collaborate(session).bind(transport);
  client->load(offline);
  transport.fail_after_submit = true;
  EXPECT_THROW(client->synchronize(), std::runtime_error);
  EXPECT_EQ(authority.sequence(), 1U);
  const auto uncertain = client->save();
  client.reset();
  client = std::make_unique<store_type>(ledger{});
  client->collaborate(session).bind(transport);
  client->load(uncertain);
  client->synchronize();
  EXPECT_EQ(authority.sequence(), 1U);
  ASSERT_EQ(transport.submissions.size(), 3U);
  EXPECT_EQ(transport.submissions[0], transport.submissions[1]);
  EXPECT_EQ(transport.submissions[1], transport.submissions[2]);
  EXPECT_EQ(authority.read()->entries.at(10).persistent_id, inserted);
  EXPECT_EQ(client->read()->entries.at(10).persistent_id, inserted);
  EXPECT_EQ(client->template collaboration<Session>().pending_count(), 0U);
  transport.fail_poll = true;
  const auto accepted = client->read();
  EXPECT_THROW(client->receive_changes(), std::runtime_error);
  EXPECT_EQ(client->read(), accepted);
  client->receive_changes();

  client->execute_transaction([](auto& tx) { tx.root().set_name("Old epoch draft"); }).throw_if_failed();
  authority_type restarted{ledger{}, 2};
  restarted.load_document(authority.save_document());
  restarted.open_session(session);
  fault_wire_transport new_transport{restarted, session};
  client->template collaboration<Session>().bind(new_transport);
  client->synchronize();
  EXPECT_EQ(restarted.sequence(), 0U);
  EXPECT_TRUE(new_transport.submissions.empty());
  EXPECT_EQ(client->read()->name, "Old epoch draft");
  EXPECT_EQ(client->template collaboration<Session>().pending_count(), 1U);
  EXPECT_EQ(client->template collaboration<Session>().state()->transactions.back().status,
            static_cast<std::uint32_t>(managed::pending_change_status::uncertain));
  static_cast<void>(client->template collaboration<Session>().discard_pending());
  EXPECT_EQ(client->read()->name, "Accepted");
  client->execute_transaction([](auto& tx) { tx.root().set_name("New epoch"); }).throw_if_failed();
  client->synchronize();
  EXPECT_EQ(restarted.sequence(), 1U);
  EXPECT_EQ(restarted.read()->name, "New epoch");
}

// Compare ordered and deliberately disrupted advisory streams with an independent grant-count oracle.
template <typename Session>
void check_lock_presence_fault_stream(Session author, Session observer) {
  using sessions = managed::collaboration_session<Session>;
  using authority_type = typename sessions::template authority<ledger>;
  using record_types = typename sessions::records;
  managed::collaboration_options options;
  options.presence_timeout_ms = 5;
  authority_type authority{ledger{"Before"}, 1, managed::make_document_id(), options};
  authority.open_session(author);
  authority.open_session(observer, false);
  typename sessions::lock_cache cache;
  cache.synchronize(wire_copy(authority.lock_snapshot()), authority.context());
  constexpr auto cycles = std::uint64_t{128};
  std::uint64_t operation{};
  for (std::uint64_t cycle = 0; cycle < cycles; ++cycle) {
    const auto now_ms = cycle * 10;
    const auto cursor = authority.lock_snapshot().sequence;
    typename record_types::lock_request acquire{authority.context(), author, ++operation,
        static_cast<std::uint32_t>(managed::edit_lock_action::acquire),
        authority.read()->persistent_id, static_cast<std::uint32_t>(managed::edit_lock_scope::entity), 0, 9};
    const auto grant = authority.change_lock(wire_copy(acquire), author, now_ms);
    ASSERT_EQ(grant->status, managed::collaboration_status::accepted);
    EXPECT_EQ(authority.change_lock(wire_copy(acquire), author, now_ms + 1), grant);
    auto release = acquire;
    release.operation = ++operation;
    release.action = static_cast<std::uint32_t>(managed::edit_lock_action::release);
    release.generation = grant->grant.generation;
    ASSERT_EQ(authority.change_lock(wire_copy(release), author, now_ms + 1)->status,
              managed::collaboration_status::accepted);
    const auto events = authority.locks_since(cursor);
    ASSERT_EQ(events.size(), 2U);
    if (cycle % 3 == 0) {
      EXPECT_EQ(cache.receive_lock_update(wire_copy(*events[1]), authority.context()),
                managed::replication_status::resync_required);
      EXPECT_FALSE(cache.complete());
      EXPECT_EQ(cache.receive_lock_update(wire_copy(*events[0]), authority.context()),
                managed::replication_status::resync_required);
      cache.synchronize(wire_copy(authority.lock_snapshot()), authority.context());
    } else {
      EXPECT_EQ(cache.receive_lock_update(wire_copy(*events[0]), authority.context()),
                managed::replication_status::applied);
      EXPECT_EQ(cache.grants().size(), 1U);
      EXPECT_EQ(cache.receive_lock_update(wire_copy(*events[0]), authority.context()),
                managed::replication_status::duplicate);
      EXPECT_EQ(cache.receive_lock_update(wire_copy(*events[1]), authority.context()),
                managed::replication_status::applied);
    }
    EXPECT_TRUE(cache.complete());
    EXPECT_TRUE(cache.grants().empty());
    auto invalid = authority.lock_snapshot();
    invalid.grants = {grant->grant, grant->grant};
    EXPECT_THROW(cache.synchronize(wire_copy(invalid), authority.context()), std::invalid_argument);
    EXPECT_TRUE(cache.complete());
    EXPECT_TRUE(cache.grants().empty());
    typename record_types::editing_presence presence{authority.context(), observer,
        cycle + 1, 1, authority.read()->persistent_id,
        static_cast<std::uint32_t>(managed::presence_action::update), "Advisory only"};
    EXPECT_TRUE(authority.receive_presence(wire_copy(presence), observer, now_ms + 1));
    EXPECT_FALSE(authority.receive_presence(wire_copy(presence), observer, now_ms + 1));
    authority.advance_expiry(now_ms + 6);
    EXPECT_TRUE(authority.editing_presence().empty());
    ++presence.sequence;
    EXPECT_FALSE(authority.receive_presence(wire_copy(presence), observer, now_ms + 6));
  }
  const auto saved = authority.save_document();
  authority_type restarted{ledger{}, 2};
  restarted.load_document(saved);
  restarted.open_session(author);
  const auto stale = authority.lock_snapshot();
  cache.synchronize(wire_copy(restarted.lock_snapshot()), restarted.context());
  EXPECT_THROW(cache.synchronize(wire_copy(stale), restarted.context()), std::invalid_argument);
  EXPECT_TRUE(cache.complete());
  EXPECT_TRUE(cache.grants().empty());
  EXPECT_TRUE(restarted.editing_presence().empty());
}
} // namespace

// Typed numeric and string session identities preserve exact retries and isolate old-epoch drafts.
TEST(managed_collaboration_fault_matrix, retry_checkpoint_and_epoch_reconnect) {
  check_retry_and_epoch_reconnect(std::uint32_t{10});
  check_retry_and_epoch_reconnect(std::uint64_t{10});
  check_retry_and_epoch_reconnect(std::string{"authenticated-session"});
}

// Hundreds of grant/removal/presence transitions reject reordered, duplicate and malformed packets safely.
TEST(managed_collaboration_fault_matrix, lock_cache_and_presence_stream_oracle) {
  check_lock_presence_fault_stream(std::uint32_t{10}, std::uint32_t{20});
  check_lock_presence_fault_stream(std::uint64_t{10}, std::uint64_t{20});
  check_lock_presence_fault_stream(std::string{"author"}, std::string{"observer"});
}
