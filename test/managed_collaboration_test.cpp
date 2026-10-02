#include "resources/ledger_commands.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
namespace managed = rohit::managed;
namespace records = managed::collaboration;
using namespace collaboration_example;
using ledger = ledger_example::ledger;

// Use two independently identified entries under a root for lock and diff assertions.
ledger initial_ledger() {
  return {"Ledger", {{10, {"One", 100}}, {20, {"Two", 200}}}};
}

// Issue a single entity/subtree lease with explicit operation identity.
records::lock_request acquire(const authority_type& authority, std::uint64_t session,
                              std::uint64_t operation, std::uint64_t target,
                              managed::edit_lock_scope scope = managed::edit_lock_scope::entity) {
  return {authority.context(),
          session,
          operation,
          1,
          target,
          static_cast<std::uint32_t>(scope),
          0,
          100};
}
} // namespace

// Duplicate proposals return the same result without running the handler or consuming IDs twice.
TEST(managed_collaboration, retries_conflicts_and_trusted_sessions) {
  int calls = 0;
  authority_type authority{initial_ledger(), 7, [&](auto& tx, auto bytes) {
                             ++calls;
                             apply_command(tx, bytes);
                           }};
  authority.open_session(10);
  authority.open_session(20);
  auto proposal = make_proposal(authority, 10, 1, {2, 0, 30, "Three", 300});
  EXPECT_THROW(authority.submit_change(proposal, 20, 0), std::invalid_argument);
  const auto first = authority.submit_change(proposal, 10, 0);
  ASSERT_EQ(first->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.submit_change(proposal, 10, 1), first);
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(authority.read()->entries.at(30).persistent_id, 4u);
  proposal.command.push_back(0);
  EXPECT_THROW(authority.submit_change(proposal, 10, 1), std::invalid_argument);
  proposal.operation = 2;
  EXPECT_EQ(authority.submit_change(proposal, 10, 1)->status,
            managed::collaboration_status::conflict);
  EXPECT_EQ(calls, 1);
  proposal.context.epoch = 6;
  EXPECT_THROW(authority.submit_change(proposal, 10, 1), std::invalid_argument);
}

// Read-only participants receive accepted state, while holes and corrupt snapshots never apply.
TEST(managed_collaboration, replica_ordering_and_atomic_decode) {
  authority_type authority{initial_ledger(), 7, apply_command};
  authority.open_session(10);
  authority.open_session(20, false);
  managed::collaboration_replica<ledger> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  auto proposal = make_proposal(authority, 20, 1, {1, 0, 0, "Denied", 0});
  EXPECT_EQ(authority.submit_change(proposal, 20, 0)->status,
            managed::collaboration_status::denied);
  const auto first =
      authority.submit_change(make_proposal(authority, 10, 1, {1, 0, 0, "First", 0}), 10, 0)
          ->accepted;
  const auto second =
      authority.submit_change(make_proposal(authority, 10, 2, {1, 0, 0, "Second", 0}), 10, 0)
          ->accepted;
  ASSERT_TRUE(first && second);
  EXPECT_EQ(replica.apply_accepted_change(*second, authority.context()),
            managed::replication_status::resync_required);
  EXPECT_EQ(replica.read()->name, "Ledger");
  EXPECT_EQ(replica.apply_accepted_change(*first, authority.context()),
            managed::replication_status::applied);
  auto corrupt = *second;
  corrupt.snapshot.push_back(0);
  EXPECT_ANY_THROW(replica.apply_accepted_change(corrupt, authority.context()));
  EXPECT_EQ(replica.read()->name, "First");
  EXPECT_EQ(replica.sequence(), 1u);
  EXPECT_EQ(replica.apply_accepted_change(*second, authority.context()),
            managed::replication_status::applied);
  EXPECT_EQ(replica.apply_accepted_change(*first, authority.context()),
            managed::replication_status::duplicate);
  EXPECT_EQ(replica.apply_accepted_change(*second, authority.context()),
            managed::replication_status::duplicate);
  corrupt = *second;
  corrupt.snapshot = first->snapshot;
  EXPECT_THROW(replica.apply_accepted_change(corrupt, authority.context()), std::invalid_argument);
  replica.synchronize(authority.snapshot(), authority.context());
  EXPECT_EQ(replica.apply_accepted_change(*second, authority.context()),
            managed::replication_status::duplicate);
}

// Entity locks protect ordinary values; changing a sibling does not spuriously change its parent.
TEST(managed_collaboration, entity_lock_and_actual_changed_entities) {
  std::vector<std::uint64_t> affected;
  authority_type authority{initial_ledger(),
                           1,
                           apply_command,
                           {1, 2},
                           {},
                           {},
                           [&](auto, const auto&, const auto&, const auto& ids) {
                             affected = ids;
                             return true;
                           }};
  authority.open_session(10);
  authority.open_session(20);
  ASSERT_EQ(authority.change_lock(acquire(authority, 10, 1, 1), 10, 0)->status,
            managed::collaboration_status::accepted);
  auto edit = make_proposal(authority, 20, 1, {3, 2, 0, "", 500});
  EXPECT_EQ(authority.submit_change(edit, 20, 0)->status, managed::collaboration_status::accepted);
  EXPECT_EQ(affected, (std::vector<std::uint64_t>{2}));
  auto insert = make_proposal(authority, 20, 2, {2, 0, 30, "Three", 300});
  EXPECT_EQ(authority.submit_change(insert, 20, 0)->status,
            managed::collaboration_status::conflict);
  EXPECT_EQ(authority.read()->entries.size(), 2u);
}

// Parent/subtree acquisitions and even same-session overlapping acquisitions must fail fast.
TEST(managed_collaboration, overlapping_locks_and_deletion) {
  authority_type authority{initial_ledger(), 1, apply_command};
  authority.open_session(10);
  authority.open_session(20);
  const auto lock = authority.change_lock(acquire(authority, 10, 1, 2), 10, 0);
  ASSERT_EQ(lock->status, managed::collaboration_status::accepted);
  EXPECT_EQ(
      authority
          .change_lock(acquire(authority, 10, 2, 1, managed::edit_lock_scope::owned_subtree), 10, 0)
          ->status,
      managed::collaboration_status::conflict);
  EXPECT_EQ(
      authority
          .change_lock(acquire(authority, 20, 1, 1, managed::edit_lock_scope::owned_subtree), 20, 0)
          ->status,
      managed::collaboration_status::conflict);
  EXPECT_EQ(
      authority.submit_change(make_proposal(authority, 20, 2, {4, 2, 0, "", 0}), 20, 0)->status,
      managed::collaboration_status::conflict);
  EXPECT_EQ(
      authority.submit_change(make_proposal(authority, 10, 3, {3, 2, 0, "", 900}), 10, 0)->status,
      managed::collaboration_status::obsolete_grant);
  auto valid = make_proposal(authority, 10, 4, {3, 2, 0, "", 900});
  valid.grants.push_back({2, lock->grant.generation});
  EXPECT_EQ(authority.submit_change(valid, 10, 0)->status, managed::collaboration_status::accepted);
}

// Retried acquisition never renews a lease; delayed release/renew cannot remove a newer generation.
TEST(managed_collaboration, expiry_and_stale_grant_fencing) {
  authority_type authority{initial_ledger(), 1, apply_command};
  authority.open_session(10);
  auto request = acquire(authority, 10, 1, 2);
  const auto first = authority.change_lock(request, 10, 0);
  ASSERT_EQ(first->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.change_lock(request, 10, 90), first);
  authority.advance_expiry(100);
  EXPECT_TRUE(authority.lock_snapshot().grants.empty());
  request.operation = 2;
  const auto next = authority.change_lock(request, 10, 100);
  ASSERT_EQ(next->status, managed::collaboration_status::accepted);
  EXPECT_GT(next->grant.generation, first->grant.generation);
  request.operation = 3;
  request.action = 3;
  request.generation = first->grant.generation;
  EXPECT_EQ(authority.change_lock(request, 10, 100)->status,
            managed::collaboration_status::obsolete_grant);
  request.operation = 4;
  request.action = 2;
  EXPECT_EQ(authority.change_lock(request, 10, 100)->status,
            managed::collaboration_status::obsolete_grant);
  EXPECT_EQ(authority.lock_snapshot().grants.front().generation, next->grant.generation);
  EXPECT_THROW(authority.advance_expiry(99), std::invalid_argument);
}

// Strict mode requires covering rights for insertions as well as scalar updates.
TEST(managed_collaboration, required_subtree_grant_and_permission_revocation) {
  managed::collaboration_options options;
  options.require_edit_lock = true;
  bool allow = true;
  authority_type authority{initial_ledger(),
                           1,
                           apply_command,
                           {1, 2},
                           options,
                           {},
                           [&](auto, const auto&, const auto&, const auto&) { return allow; }};
  authority.open_session(10);
  auto insert = make_proposal(authority, 10, 1, {2, 0, 30, "Three", 300});
  EXPECT_EQ(authority.submit_change(insert, 10, 0)->status, managed::collaboration_status::denied);
  const auto lock = authority.change_lock(
      acquire(authority, 10, 2, 1, managed::edit_lock_scope::owned_subtree), 10, 0);
  insert.operation = 3;
  insert.grants.push_back({1, lock->grant.generation});
  allow = false;
  EXPECT_EQ(authority.submit_change(insert, 10, 0)->status, managed::collaboration_status::denied);
  allow = true;
  insert.operation = 4;
  EXPECT_EQ(authority.submit_change(insert, 10, 0)->status,
            managed::collaboration_status::accepted);
  EXPECT_GT(authority.read()->entries.at(30).persistent_id, 4u);
  authority.set_session_writable(10, false);
  auto edit = make_proposal(authority, 10, 5, {1, 0, 0, "Denied", 0});
  EXPECT_EQ(authority.submit_change(edit, 10, 0)->status, managed::collaboration_status::denied);
}

// Presence is advisory, ordered, bounded, and terminal after end, timeout, or session shutdown.
TEST(managed_collaboration, presence_lifetimes) {
  managed::collaboration_options options;
  options.presence_timeout_ms = 10;
  options.max_preview_bytes = 8;
  authority_type authority{initial_ledger(), 1, apply_command, {1, 2}, options};
  authority.open_session(10, false);
  records::editing_presence presence{authority.context(), 10, 1, 1, 2, 1, "Editing"};
  EXPECT_TRUE(authority.receive_presence(presence, 10, 0));
  EXPECT_FALSE(authority.receive_presence(presence, 10, 1));
  presence.sequence = 2;
  presence.action = 2;
  EXPECT_TRUE(authority.receive_presence(presence, 10, 1));
  presence.sequence = 3;
  presence.action = 1;
  EXPECT_FALSE(authority.receive_presence(presence, 10, 1));
  presence.presence = 2;
  EXPECT_TRUE(authority.receive_presence(presence, 10, 1));
  authority.advance_expiry(11);
  presence.sequence = 4;
  EXPECT_FALSE(authority.receive_presence(presence, 10, 11));
  EXPECT_TRUE(authority.editing_presence().empty());
  presence.presence = 3;
  presence.preview = "Oversized preview";
  EXPECT_THROW(authority.receive_presence(presence, 10, 11), std::invalid_argument);
  authority.close_session(10);
  EXPECT_THROW(authority.open_session(10), std::invalid_argument);
  EXPECT_THROW(authority.receive_presence(presence, 10, 11), std::invalid_argument);
}

// A missed lock event makes absence inconclusive until a trusted snapshot replaces the cache.
TEST(managed_collaboration, lock_cache_gap_and_snapshot_handoff) {
  authority_type authority{initial_ledger(), 1, apply_command};
  authority.open_session(10);
  managed::collaboration_lock_cache cache;
  cache.synchronize(authority.lock_snapshot(), authority.context());
  authority.change_lock(acquire(authority, 10, 1, 2), 10, 0);
  authority.change_lock(acquire(authority, 10, 2, 3), 10, 0);
  const auto updates = authority.locks_since(0);
  ASSERT_EQ(updates.size(), 2u);
  EXPECT_EQ(cache.receive_lock_update(*updates[1], authority.context()),
            managed::replication_status::resync_required);
  EXPECT_FALSE(cache.complete());
  EXPECT_EQ(cache.receive_lock_update(*updates[0], authority.context()),
            managed::replication_status::resync_required);
  cache.synchronize(authority.lock_snapshot(), authority.context());
  EXPECT_TRUE(cache.complete());
  EXPECT_EQ(cache.grants().size(), 2u);
  authority.close_session(10);
  for (const auto& update : authority.locks_since(2)) {
    EXPECT_EQ(cache.receive_lock_update(*update, authority.context()),
              managed::replication_status::applied);
  }
  EXPECT_TRUE(cache.grants().empty());
  authority.prune_lock_updates(4);
  EXPECT_THROW(authority.locks_since(0), std::out_of_range);
}

// Reject malformed command bytes and handler failures atomically, retaining their retry outcomes.
TEST(managed_collaboration, malformed_failure_noop_and_reentry) {
  authority_type* owner = nullptr;
  authority_type authority{initial_ledger(), 1, [&](auto& tx, auto bytes) {
                             if (bytes.empty()) {
                               tx.root().set_name("Must roll back");
                               owner->close_session(10);
                             } else {
                               apply_command(tx, bytes);
                             }
                           }};
  owner = &authority;
  authority.open_session(10);
  auto proposal = make_proposal(authority, 10, 1, {1, 0, 0, "Ledger", 0});
  EXPECT_EQ(authority.submit_change(proposal, 10, 0)->status,
            managed::collaboration_status::no_change);
  EXPECT_EQ(authority.sequence(), 0u);
  proposal.operation = 2;
  proposal.command.push_back(0);
  EXPECT_EQ(authority.submit_change(proposal, 10, 0)->status,
            managed::collaboration_status::failed);
  proposal.operation = 3;
  proposal.command.clear();
  const auto failed = authority.submit_change(proposal, 10, 0);
  EXPECT_EQ(failed->status, managed::collaboration_status::failed);
  EXPECT_EQ(authority.submit_change(proposal, 10, 0), failed);
  EXPECT_EQ(authority.read()->name, "Ledger");
  EXPECT_TRUE(authority.accepted_since(0).empty());
}

// Retry tables are bounded without forgetting old operation IDs and accidentally accepting them twice.
TEST(managed_collaboration, budgets_and_lock_transition_rollback) {
  managed::collaboration_options options;
  options.max_operations = 2;
  options.max_lock_updates = 1;
  authority_type authority{initial_ledger(), 1, apply_command, {1, 2}, options};
  authority.open_session(10);
  const auto first = acquire(authority, 10, 1, 2);
  EXPECT_EQ(authority.change_lock(first, 10, 0)->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.change_lock(acquire(authority, 10, 2, 3), 10, 0)->status,
            managed::collaboration_status::failed);
  EXPECT_EQ(authority.lock_snapshot().grants.size(), 1u);
  EXPECT_THROW(authority.change_lock(acquire(authority, 10, 3, 3), 10, 0), std::length_error);
  EXPECT_EQ(authority.change_lock(first, 10, 0)->status, managed::collaboration_status::accepted);
  authority.prune_lock_updates(1);
  authority.advance_expiry(100);
  EXPECT_TRUE(authority.lock_snapshot().grants.empty());
}

// Recovery restores values and allocation only; a new authority epoch rejects all old coordination.
TEST(managed_collaboration, document_and_journal_recovery_clear_rights) {
  const auto directory =
      std::filesystem::current_path() /
      ("serializer-collaboration-" + std::to_string(managed::make_document_id().high));
  ASSERT_TRUE(std::filesystem::create_directory(directory));
  const auto path = directory / "document";
  records::change_proposal old;
  std::vector<std::uint8_t> saved;
  {
    authority_type authority{initial_ledger(), 1, apply_command};
    authority.create_journal(path);
    EXPECT_THROW(authority.load_document(authority.save_document()), std::logic_error);
    authority.open_session(10);
    authority.change_lock(acquire(authority, 10, 1, 2), 10, 0);
    old = make_proposal(authority, 10, 2, {1, 0, 0, "Durable", 0});
    ASSERT_EQ(authority.submit_change(old, 10, 0)->status, managed::collaboration_status::accepted);
    saved = authority.save_document();
    authority.save_journal();
  }
  {
    authority_type authority{ledger{}, 2, apply_command};
    authority.recover_journal(path);
    EXPECT_EQ(authority.read()->name, "Durable");
    EXPECT_EQ(authority.sequence(), 0u);
    EXPECT_TRUE(authority.lock_snapshot().grants.empty());
    authority.open_session(10);
    EXPECT_THROW(authority.submit_change(old, 10, 0), std::invalid_argument);
  }
  authority_type restored{ledger{}, 3, apply_command};
  restored.load_document(saved);
  EXPECT_EQ(restored.read()->name, "Durable");
  EXPECT_TRUE(restored.lock_snapshot().grants.empty());
  std::filesystem::remove_all(directory);
}

// Uncertain durability never emits an acceptance; recovery under a fresh epoch reconciles the model.
TEST(managed_collaboration, indeterminate_journal_fences_authority) {
  const auto directory =
      std::filesystem::current_path() /
      ("serializer-collaboration-fault-" + std::to_string(managed::make_document_id().high));
  ASSERT_TRUE(std::filesystem::create_directory(directory));
  const auto path = directory / "document";
  bool armed = false;
  managed::journal_options options;
  options.fault_injector = [&](auto event) {
    if (armed && event == managed::journal_io_event::after_flush) {
      throw std::runtime_error{"Injected uncertain acknowledgement"};
    }
  };
  {
    authority_type authority{initial_ledger(), 1, apply_command};
    authority.create_journal(path, managed::journal_storage_mode::sidecar, options);
    authority.open_session(10);
    const auto proposal = make_proposal(authority, 10, 1, {1, 0, 0, "Durable", 0});
    const auto before = authority.read();
    armed = true;
    const auto result = authority.submit_change(proposal, 10, 0);
    EXPECT_EQ(result->status, managed::collaboration_status::indeterminate);
    EXPECT_FALSE(result->accepted);
    EXPECT_EQ(before->name, "Ledger");
    EXPECT_THROW(authority.submit_change(proposal, 10, 0), managed::journal_indeterminate_error);
    EXPECT_THROW(authority.snapshot(), managed::journal_indeterminate_error);
  }
  {
    authority_type authority{ledger{}, 2, apply_command};
    authority.recover_journal(path);
    EXPECT_EQ(authority.read()->name, "Durable");
    EXPECT_EQ(authority.sequence(), 0u);
    EXPECT_TRUE(authority.lock_snapshot().grants.empty());
  }
  std::filesystem::remove_all(directory);
}

// Generated portable envelopes preserve exact widths and reject trailing bytes with bounded decoding.
TEST(managed_collaboration, portable_records_and_counter_exhaustion) {
  records::lock_request request{
      {1, "schema", 64, 1, 2, 3}, 4, 5, 1, std::numeric_limits<std::uint64_t>::max(), 2, 0, 100};
  auto bytes = managed::detail::encode(request);
  EXPECT_EQ(managed::detail::decode<records::lock_request>(bytes, {}).target, request.target);
  bytes.push_back(0);
  EXPECT_ANY_THROW(managed::detail::decode<records::lock_request>(bytes, {}));
  EXPECT_THROW(
      managed::detail::next_collaboration_sequence(std::numeric_limits<std::uint64_t>::max()),
      std::overflow_error);
  EXPECT_THROW(
      managed::detail::collaboration_deadline(std::numeric_limits<std::uint64_t>::max(), 1),
      std::invalid_argument);
}

// Owner-thread confinement is checked before consulting mutable authority tables.
TEST(managed_collaboration, wrong_thread_rejected) {
  authority_type authority{initial_ledger(), 1, apply_command};
  bool rejected = false;
  std::thread other{[&] {
    try {
      authority.open_session(10);
    } catch (const std::logic_error&) {
      rejected = true;
    }
  }};
  other.join();
  EXPECT_TRUE(rejected);
}

// Relocating a child within one parent's map still requires permission for the moved child.
TEST(managed_collaboration, same_parent_move_checks_child_lock) {
  authority_type authority{initial_ledger(), 1, [](auto& tx, auto) {
                             tx.update([](auto& value) {
                               auto node = value.entries.extract(10);
                               node.key() = 30;
                               value.entries.insert(std::move(node));
                             });
                           }};
  authority.open_session(10);
  authority.open_session(20);
  ASSERT_EQ(authority.change_lock(acquire(authority, 10, 1, 2), 10, 0)->status,
            managed::collaboration_status::accepted);
  EXPECT_EQ(authority.submit_change({authority.context(), 20, 1, 0, {}, {}}, 20, 0)->status,
            managed::collaboration_status::conflict);
  EXPECT_TRUE(authority.read()->entries.contains(10));
  EXPECT_FALSE(authority.read()->entries.contains(30));
}
