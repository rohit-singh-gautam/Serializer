#include <collaboration_model.hpp>
#include <gtest/gtest.h>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
namespace records = managed::collaboration;
using ledger = ledger_example::ledger;
using authority_type = managed::collaboration_authority<ledger>;
using replica_type = managed::collaboration_replica<ledger>;
using status = managed::collaboration_status;

// Supply trusted session bindings and always bring the caller to the accepted baseline.
auto submit(auto& authority, auto& replica, const auto& proposal) {
  const auto result = authority.submit_change(proposal, proposal.session, 0);
  replica.synchronize(authority.snapshot(), authority.context());
  return result;
}

// Exercise ordinary generated editing without handwritten command classes.
auto edit(auto& authority, auto& replica, const auto& session, std::uint64_t operation,
          auto&& callback) {
  return submit(authority, replica, replica.propose(session, operation, callback));
}

// Each case starts from two writable users and a nonempty identified document.
class collaboration_history_test : public testing::Test {
protected:
  authority_type authority{ledger{"Expenses", {{100, {"Supplies", 1500}}}}, 1};
  replica_type replica;

  // Register application-owned identities before authoring any operation.
  void SetUp() override {
    authority.open_session(10);
    authority.open_session(20);
    replica.synchronize(authority.snapshot(), authority.context());
  }
};
} // namespace

// Records group fields by transaction and carry stable addresses, original values and versions.
TEST_F(collaboration_history_test, records_round_trip_and_undo_preserves_other_fields) {
  const auto alice = edit(authority, replica, 10u, 1, [](auto& tx) {
    tx.root().entries().edit(2).set_memo("Office supplies");
  });
  ASSERT_EQ(alice->status, status::accepted);
  const auto bytes = managed::encode_collaboration_record(*alice->accepted);
  const auto record = managed::decode_collaboration_record<records::accepted_change>(bytes);
  EXPECT_EQ(record.session, 10u);
  EXPECT_EQ(record.operation, 1u);
  EXPECT_EQ(record.history.action, 1u);
  ASSERT_EQ(record.history.fields.size(), 1u);
  const auto& field = record.history.fields.front();
  EXPECT_EQ(field.entity, 2u);
  EXPECT_EQ(field.field, 1u);
  EXPECT_TRUE(field.before_present);
  EXPECT_TRUE(field.after_present);
  EXPECT_EQ(field.before_version, 0u);
  EXPECT_EQ(field.after_version, record.sequence);
  EXPECT_EQ(managed::detail::decode<std::string>(field.before, {}), "Supplies");
  EXPECT_EQ(managed::detail::decode<std::string>(field.after, {}), "Office supplies");
  const auto undo = replica.undo(10, 2, 1);
  ASSERT_EQ(edit(authority, replica, 20u, 1,
                 [](auto& tx) { tx.root().entries().edit(2).set_amount_minor_units(2000); })
                ->status,
            status::accepted);
  const auto undone = submit(authority, replica, undo);
  ASSERT_EQ(undone->status, status::accepted);
  EXPECT_EQ(undone->accepted->history.action, 2u);
  EXPECT_EQ(undone->accepted->history.target_operation, 1u);
  EXPECT_EQ(authority.read()->entries.at(100).memo, "Supplies");
  EXPECT_EQ(authority.read()->entries.at(100).amount_minor_units, 2000);
  EXPECT_EQ(authority.submit_change(undo, 10, 0), undone);
  ASSERT_EQ(submit(authority, replica, replica.redo(10, 3, 2))->status, status::accepted);
  EXPECT_EQ(authority.read()->entries.at(100).memo, "Office supplies");
  EXPECT_EQ(authority.read()->entries.at(100).amount_minor_units, 2000);
}

// Equal current bytes cannot hide another user's intervening write; compound undo stays atomic.
TEST_F(collaboration_history_test, same_value_aba_conflicts_without_partial_undo) {
  ASSERT_EQ(edit(authority, replica, 10u, 1,
                 [](auto& tx) {
                   tx.root().set_name("Renamed");
                   tx.root().entries().edit(2).set_memo("Changed");
                 })
                ->status,
            status::accepted);
  ASSERT_EQ(edit(authority, replica, 20u, 1,
                 [](auto& tx) { tx.root().entries().edit(2).set_memo("Other"); })
                ->status,
            status::accepted);
  ASSERT_EQ(edit(authority, replica, 20u, 2,
                 [](auto& tx) { tx.root().entries().edit(2).set_memo("Changed"); })
                ->status,
            status::accepted);
  const auto before = authority.save_document();
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::conflict);
  EXPECT_EQ(authority.save_document(), before);
  EXPECT_EQ(authority.undo_operation(10), 1u);
  EXPECT_EQ(authority.redo_operation(10), 0u);
}

// Undo restores contribution provenance, allowing multiple consecutive reversals of one field.
TEST_F(collaboration_history_test, repeated_undo_and_redo_follow_transaction_groups) {
  ASSERT_EQ(edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().set_name("First"); })->status,
            status::accepted);
  ASSERT_EQ(
      edit(authority, replica, 10u, 2, [](auto& tx) { tx.root().set_name("Second"); })->status,
      status::accepted);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 3))->status, status::accepted);
  EXPECT_EQ(authority.read()->name, "First");
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 4))->status, status::accepted);
  EXPECT_EQ(authority.read()->name, "Expenses");
  EXPECT_EQ(submit(authority, replica, replica.redo(10, 5))->status, status::accepted);
  EXPECT_EQ(authority.read()->name, "First");
  EXPECT_EQ(submit(authority, replica, replica.redo(10, 6))->status, status::accepted);
  EXPECT_EQ(authority.read()->name, "Second");
}

// Erasing a new object must not discard edits made inside it by another author.
TEST_F(collaboration_history_test, creation_undo_protects_remote_children) {
  std::uint32_t created{};
  ASSERT_EQ(edit(authority, replica, 10u, 1,
                 [&](auto& tx) { created = tx.root().entries().insert(200, {"New", 5}); })
                ->status,
            status::accepted);
  ASSERT_EQ(edit(authority, replica, 20u, 1,
                 [&](auto& tx) { tx.root().entries().edit(created).set_memo("Bob's work"); })
                ->status,
            status::accepted);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::conflict);
  ASSERT_EQ(submit(authority, replica, replica.undo(20, 2))->status, status::accepted);
  ASSERT_EQ(submit(authority, replica, replica.undo(10, 3))->status, status::accepted);
  EXPECT_FALSE(authority.read()->entries.contains(200));
  ASSERT_EQ(submit(authority, replica, replica.redo(10, 4))->status, status::accepted);
  EXPECT_EQ(authority.read()->entries.at(200).persistent_id, created);
  EXPECT_EQ(authority.read()->entries.at(200).memo, "New");
}

// Restoring a deletion retains IDs, keeps unrelated current fields, and remains redoable.
TEST_F(collaboration_history_test, deleted_objects_restore_identity_without_reusing_allocations) {
  ASSERT_EQ(
      edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().entries().erase(2); })->status,
      status::accepted);
  ASSERT_EQ(
      edit(authority, replica, 20u, 1, [](auto& tx) { tx.root().set_name("Bob's name"); })->status,
      status::accepted);
  ASSERT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::accepted);
  EXPECT_EQ(authority.read()->entries.at(100).persistent_id, 2u);
  EXPECT_EQ(authority.read()->name, "Bob's name");
  ASSERT_EQ(submit(authority, replica, replica.redo(10, 3))->status, status::accepted);
  std::uint32_t next{};
  ASSERT_EQ(edit(authority, replica, 10u, 4,
                 [&](auto& tx) { next = tx.root().entries().insert(100, {"New identity", 5}); })
                ->status,
            status::accepted);
  EXPECT_GT(next, 2u);
  EXPECT_EQ(authority.redo_operation(10), 0u);
}

// The same key now naming a different entity cannot receive a historical restoration.
TEST_F(collaboration_history_test, reused_map_key_blocks_deletion_undo) {
  ASSERT_EQ(
      edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().entries().erase(2); })->status,
      status::accepted);
  ASSERT_EQ(edit(authority, replica, 20u, 1,
                 [](auto& tx) { tx.root().entries().insert(100, {"Replacement", 10}); })
                ->status,
            status::accepted);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::conflict);
  EXPECT_EQ(authority.read()->entries.at(100).memo, "Replacement");
}

// Remote edits and unsuccessful local edits preserve redo; a successful new local edit clears it.
TEST_F(collaboration_history_test, redo_lifetime_and_empty_history) {
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 1))->status, status::no_change);
  EXPECT_EQ(submit(authority, replica, replica.redo(10, 2))->status, status::no_change);
  ASSERT_EQ(edit(authority, replica, 10u, 3, [](auto& tx) { tx.root().set_name("Name"); })->status,
            status::accepted);
  ASSERT_EQ(submit(authority, replica, replica.undo(10, 4))->status, status::accepted);
  const auto redo = authority.redo_operation(10);
  EXPECT_EQ(edit(authority, replica, 10u, 5, [](auto&) {})->status, status::no_change);
  EXPECT_EQ(authority.redo_operation(10), redo);
  EXPECT_EQ(edit(authority, replica, 20u, 1,
                 [](auto& tx) { tx.root().entries().edit(2).set_memo("Remote"); })
                ->status,
            status::accepted);
  EXPECT_EQ(authority.redo_operation(10), redo);
  EXPECT_THROW(replica.propose(10, 6, [](auto& tx) { tx.revert(); }), std::logic_error);
  EXPECT_EQ(authority.redo_operation(10), redo);
  EXPECT_EQ(edit(authority, replica, 10u, 7,
                 [](auto& tx) { tx.root().entries().edit(2).set_amount_minor_units(123); })
                ->status,
            status::accepted);
  EXPECT_EQ(authority.redo_operation(10), 0u);
}

// Undo must satisfy current write permissions and cannot choose another session's history.
TEST_F(collaboration_history_test, authorization_and_history_ownership) {
  ASSERT_EQ(edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().set_name("Name"); })->status,
            status::accepted);
  EXPECT_EQ(submit(authority, replica, replica.undo(20, 1, 1))->status, status::conflict);
  const auto request = replica.undo(10, 2);
  EXPECT_THROW(authority.submit_change(request, 20, 0), std::invalid_argument);
  authority.set_session_writable(10, false);
  EXPECT_EQ(submit(authority, replica, request)->status, status::denied);
  authority.set_session_writable(10, true);
  EXPECT_EQ(submit(authority, replica, request)->status, status::denied);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 3))->status, status::accepted);
}

// An outdated replica may reverse a known tip across unrelated changes, never an unseen own edit.
TEST_F(collaboration_history_test, stale_self_view_and_wrong_tip_conflict) {
  const auto stale = replica.undo(10, 2);
  ASSERT_EQ(edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().set_name("Name"); })->status,
            status::accepted);
  EXPECT_EQ(submit(authority, replica, stale)->status, status::conflict);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 3, 999))->status, status::conflict);
  EXPECT_EQ(authority.undo_operation(10), 1u);
}

// Inverse data is authority-owned; a history message cannot smuggle a replacement snapshot.
TEST_F(collaboration_history_test, malformed_history_requests_are_atomic) {
  auto proposal = replica.undo(10, 1);
  auto change = managed::decode_collaboration_record<records::model_change>(proposal.command);
  change.snapshot = {1};
  proposal.command = managed::encode_collaboration_record(change);
  EXPECT_EQ(submit(authority, replica, proposal)->status, status::failed);
  EXPECT_EQ(authority.sequence(), 0u);
  EXPECT_THROW(replica.history_request(10, 2, managed::collaboration_history_action::edit),
               std::invalid_argument);
  EXPECT_THROW(replica.undo(0, 1), std::invalid_argument);
  EXPECT_THROW(replica.undo(10, 0), std::invalid_argument);
  auto legacy = authority.snapshot();
  legacy.context.protocol_version = 2;
  replica_type receiver;
  EXPECT_THROW(receiver.synchronize(legacy, legacy.context), std::invalid_argument);
  proposal.context.protocol_version = 2;
  EXPECT_THROW(authority.submit_change(proposal, 10, 0), std::invalid_argument);
}

// String sessions retain their full authorship through record serialization and reversal.
TEST(collaboration_history, application_owned_string_sessions) {
  using sessions = managed::collaboration_session<std::string>;
  sessions::authority<ledger> authority{ledger{"Original"}, 1};
  sessions::replica<ledger> replica;
  const std::string user = "application/session";
  authority.open_session(user);
  replica.synchronize(authority.snapshot(), authority.context());
  ASSERT_EQ(
      edit(authority, replica, user, 1, [](auto& tx) { tx.root().set_name("Changed"); })->status,
      status::accepted);
  auto request = managed::decode_collaboration_record<sessions::records::change_proposal>(
      managed::encode_collaboration_record(replica.undo(user, 2)));
  const auto result = submit(authority, replica, request);
  ASSERT_EQ(result->status, status::accepted);
  const auto accepted = managed::decode_collaboration_record<sessions::records::accepted_change>(
      managed::encode_collaboration_record(*result->accepted));
  EXPECT_EQ(accepted.session, user);
  EXPECT_EQ(accepted.history.action, 2u);
  EXPECT_EQ(accepted.history.fields.size(), 1u);
  EXPECT_EQ(authority.read()->name, "Original");
}

// Final-diff policy is checked again for undo, regardless of the original author's permission.
TEST(collaboration_history, change_policy_applies_to_undo_and_redo) {
  bool allowed = true;
  authority_type authority{
      ledger{"Original"},
      1,
      managed::make_document_id(),
      {},
      {},
      [&](const auto&, const auto&, const auto&, const auto&) { return allowed; }};
  authority.open_session(10);
  replica_type replica;
  replica.synchronize(authority.snapshot(), authority.context());
  ASSERT_EQ(edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().set_name("New"); })->status,
            status::accepted);
  allowed = false;
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::denied);
  EXPECT_EQ(authority.undo_operation(10), 1u);
  allowed = true;
  ASSERT_EQ(submit(authority, replica, replica.undo(10, 3))->status, status::accepted);
  allowed = false;
  EXPECT_EQ(submit(authority, replica, replica.redo(10, 4))->status, status::denied);
  EXPECT_EQ(authority.redo_operation(10), 3u);
}

// A denied restoration cannot leave ordinary client snapshots able to resurrect retired IDs.
TEST(collaboration_history, denied_undo_cannot_leak_identity_restoration) {
  bool allowed = true;
  authority_type authority{
      ledger{"Original", {{100, {"Entry", 5}}}},
      1,
      managed::make_document_id(),
      {},
      {},
      [&](const auto&, const auto&, const auto&, const auto&) { return allowed; }};
  authority.open_session(10);
  replica_type replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto original = authority.snapshot();
  ASSERT_EQ(
      edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().entries().erase(2); })->status,
      status::accepted);
  allowed = false;
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::denied);
  allowed = true;
  auto forged = replica.propose(10, 3, [](auto&) {});
  auto change = managed::decode_collaboration_record<records::model_change>(forged.command);
  change.snapshot = original.snapshot;
  forged.command = managed::encode_collaboration_record(change);
  EXPECT_EQ(submit(authority, replica, forged)->status, status::failed);
  EXPECT_TRUE(authority.read()->entries.empty());
  EXPECT_EQ(authority.undo_operation(10), 1u);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 4))->status, status::accepted);
  EXPECT_EQ(authority.read()->entries.at(100).persistent_id, 2u);
}

// Both journal layouts recover restored identities even when local snapshot history is disabled.
TEST(collaboration_history, journal_recovers_deletion_undo) {
  for (const auto mode :
       {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    const auto document = managed::make_document_id();
    const auto directory =
        std::filesystem::current_path() / ("collaboration-undo-" + std::to_string(document.high));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    const auto path = directory / "document";
    using durable_authority =
        managed::collaboration_authority<ledger, managed::history_mode::disabled>;
    {
      durable_authority authority{ledger{"Original", {{100, {"Entry", 5}}}}, 1, document};
      authority.create_journal(path, mode);
      authority.open_session(10);
      replica_type replica;
      replica.synchronize(authority.snapshot(), authority.context());
      ASSERT_EQ(
          edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().entries().erase(2); })->status,
          status::accepted);
      ASSERT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::accepted);
    }
    {
      durable_authority recovered{ledger{}, 2};
      recovered.recover_journal(path);
      EXPECT_EQ(recovered.read()->entries.at(100).persistent_id, 2u);
      EXPECT_EQ(recovered.read()->entries.at(100).memo, "Entry");
    }
    std::filesystem::remove_all(directory);
  }
}

// A later sibling edit survives structural undo when membership itself has not changed.
TEST_F(collaboration_history_test, structural_undo_keeps_unrelated_child_edits) {
  ASSERT_EQ(edit(authority, replica, 10u, 1,
                 [](auto& tx) { tx.root().entries().insert(200, {"New", 5}); })
                ->status,
            status::accepted);
  ASSERT_EQ(edit(authority, replica, 20u, 1,
                 [](auto& tx) { tx.root().entries().edit(2).set_memo("Preserve me"); })
                ->status,
            status::accepted);
  ASSERT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::accepted);
  EXPECT_FALSE(authority.read()->entries.contains(200));
  EXPECT_EQ(authority.read()->entries.at(100).memo, "Preserve me");
}

// Redo is conditional too: another author's same-field assignment prevents overwriting it.
TEST_F(collaboration_history_test, redo_conflict_preserves_its_stack_tip) {
  ASSERT_EQ(edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().set_name("Alice"); })->status,
            status::accepted);
  ASSERT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::accepted);
  ASSERT_EQ(edit(authority, replica, 20u, 1, [](auto& tx) { tx.root().set_name("Bob"); })->status,
            status::accepted);
  EXPECT_EQ(submit(authority, replica, replica.redo(10, 3))->status, status::conflict);
  EXPECT_EQ(authority.read()->name, "Bob");
  EXPECT_EQ(authority.redo_operation(10), 2u);
}

// Both deletion and reparenting invalidate an old field edit's ownership dependency.
TEST(collaboration_history, owner_relocation_and_parent_deletion_conflict) {
  using workspace = collaboration_test::workspace;
  for (const bool remove_parent : {false, true}) {
    managed::collaboration_authority<workspace> authority{
        workspace{{{1, {"First", {{100, {"Entry", 1}}}}}, {2, {"Second"}}}}, 1};
    authority.open_session(10);
    authority.open_session(20);
    managed::collaboration_replica<workspace> replica;
    replica.synchronize(authority.snapshot(), authority.context());
    ASSERT_EQ(edit(authority, replica, 10u, 1,
                   [](auto& tx) {
                     tx.update(
                         [](auto& value) { value.ledgers.at(1).entries.at(100).memo = "Alice"; });
                   })
                  ->status,
              status::accepted);
    ASSERT_EQ(edit(authority, replica, 20u, 1,
                   [&](auto& tx) {
                     tx.update([&](auto& value) {
                       if (remove_parent) {
                         value.ledgers.erase(1);
                       } else {
                         auto child = value.ledgers.at(1).entries.extract(100);
                         value.ledgers.at(2).entries.insert(std::move(child));
                       }
                     });
                   })
                  ->status,
              status::accepted);
    EXPECT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::conflict);
    ASSERT_EQ(submit(authority, replica, replica.undo(20, 2))->status, status::accepted);
    ASSERT_EQ(submit(authority, replica, replica.undo(10, 3))->status, status::accepted);
    EXPECT_EQ(authority.read()->ledgers.at(1).entries.at(100).memo, "Entry");
  }
}

// A move's inverse also checks unchanged descendant fields before discarding any later work.
TEST(collaboration_history, undo_relocation_protects_descendant_edits) {
  using workspace = collaboration_test::workspace;
  managed::collaboration_authority<workspace> authority{
      workspace{{{1, {"First", {{100, {"Entry", 1}}}}}, {2, {"Second"}}}}, 1};
  authority.open_session(10);
  authority.open_session(20);
  managed::collaboration_replica<workspace> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  ASSERT_EQ(edit(authority, replica, 10u, 1,
                 [](auto& tx) {
                   tx.update([](auto& value) {
                     auto child = value.ledgers.at(1).entries.extract(100);
                     value.ledgers.at(2).entries.insert(std::move(child));
                   });
                 })
                ->status,
            status::accepted);
  ASSERT_EQ(edit(authority, replica, 20u, 1,
                 [](auto& tx) {
                   tx.update([](auto& value) { value.ledgers.at(2).entries.at(100).memo = "Bob"; });
                 })
                ->status,
            status::accepted);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 2))->status, status::conflict);
  EXPECT_EQ(authority.read()->ledgers.at(2).entries.at(100).memo, "Bob");
}

// Undo must present a current lease generation and pass the same final-diff lock gates as edits.
TEST(collaboration_history, undo_obeys_lock_fencing) {
  managed::collaboration_options options;
  options.require_edit_lock = true;
  authority_type authority{ledger{"Original"}, 1, managed::make_document_id(), options};
  authority.open_session(10);
  replica_type replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto lock = authority.change_lock({authority.context(), 10, 1, 1, 1, 2, 0, 1000}, 10, 0);
  ASSERT_EQ(lock->status, status::accepted);
  const auto grants = std::vector<records::grant_reference>{{1, lock->grant.generation}};
  ASSERT_EQ(submit(authority, replica,
                   replica.propose(
                       10, 2, [](auto& tx) { tx.root().set_name("New"); }, grants))
                ->status,
            status::accepted);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 3))->status, status::obsolete_grant);
  EXPECT_EQ(
      submit(authority, replica, replica.undo(10, 4, 0, {{1, lock->grant.generation + 1}}))->status,
      status::obsolete_grant);
  EXPECT_EQ(submit(authority, replica, replica.undo(10, 5, 0, grants))->status, status::accepted);
}

// Resource rejection happens before publication and does not grant a restoration capability.
TEST(collaboration_history, field_and_history_byte_budgets) {
  for (const bool limit_fields : {false, true}) {
    managed::collaboration_options options;
    if (limit_fields) {
      options.max_tracked_fields = 1;
    } else {
      options.max_retained_bytes = 400;
    }
    authority_type authority{ledger{"Original", {{100, {std::string(200, 'x'), 5}}}}, 1,
                             managed::make_document_id(), options};
    authority.open_session(10);
    replica_type replica;
    replica.synchronize(authority.snapshot(), authority.context());
    const auto before = authority.save_document();
    const auto proposal = replica.propose(10, 1, [](auto& tx) { tx.root().set_name("Changed"); });
    try {
      EXPECT_EQ(submit(authority, replica, proposal)->status, status::failed);
    } catch (const std::length_error&) {
      // The bounded request itself may exhaust admission before candidate preparation.
    }
    EXPECT_EQ(authority.save_document(), before);
    EXPECT_EQ(authority.undo_operation(10), 0u);
    EXPECT_EQ(authority.sequence(), 0u);
  }
}

// A failed durable reversal cannot advance versions or history; an uncertain write fences the host.
TEST(collaboration_history, journal_failures_preserve_or_fence_undo) {
  for (const auto boundary :
       {managed::journal_io_event::before_append, managed::journal_io_event::after_flush}) {
    const auto document = managed::make_document_id();
    const auto directory =
        std::filesystem::current_path() / ("undo-fault-" + std::to_string(document.high));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    bool fail = false;
    managed::journal_options options;
    options.fault_injector = [&](auto event) {
      if (fail && event == boundary) {
        throw std::runtime_error{"Injected undo journal failure"};
      }
    };
    {
      authority_type authority{ledger{"Original"}, 1, document};
      authority.create_journal(directory / "document", managed::journal_storage_mode::appended,
                               options);
      authority.open_session(10);
      replica_type replica;
      replica.synchronize(authority.snapshot(), authority.context());
      ASSERT_EQ(
          edit(authority, replica, 10u, 1, [](auto& tx) { tx.root().set_name("New"); })->status,
          status::accepted);
      fail = true;
      const auto outcome = authority.submit_change(replica.undo(10, 2), 10, 0);
      if (boundary == managed::journal_io_event::before_append) {
        EXPECT_EQ(outcome->status, status::failed);
        EXPECT_EQ(authority.undo_operation(10), 1u);
        EXPECT_EQ(authority.read()->name, "New");
        fail = false;
        EXPECT_EQ(submit(authority, replica, replica.undo(10, 3))->status, status::accepted);
      } else {
        EXPECT_EQ(outcome->status, status::indeterminate);
        EXPECT_THROW(authority.read(), managed::journal_indeterminate_error);
      }
    }
    {
      authority_type recovered{ledger{}, 2};
      recovered.recover_journal(directory / "document");
      EXPECT_EQ(recovered.read()->name, "Original");
    }
    std::filesystem::remove_all(directory);
  }
}
