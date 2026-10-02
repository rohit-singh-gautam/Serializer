#include <gtest/gtest.h>
#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
namespace records = managed::collaboration;
using ledger = ledger_example::ledger;
using authority_type = managed::collaboration_authority<ledger>;

// Deliver the authoritative baseline without publishing local proposal drafts.
void synchronize(managed::collaboration_replica<ledger>& replica, const auto& authority) {
  replica.synchronize(authority.snapshot(), authority.context());
}

// Exercise the default model payload through every supported history/label specialization.
template <managed::history_mode Mode, managed::history_labels Labels>
void verify_history_policy() {
  managed::collaboration_authority<ledger, Mode, Labels> authority{ledger{"Shared"}, 1};
  authority.open_session(10);
  managed::collaboration_replica<ledger> replica;
  synchronize(replica, authority);
  const auto proposal = replica.propose(10, 1, [](auto& edit) { edit.root().set_name("Changed"); });
  const auto result = authority.submit_change(proposal, 10, 0);
  ASSERT_EQ(result->status, managed::collaboration_status::accepted);
  replica.apply_accepted_change(*result->accepted, authority.context());
  EXPECT_EQ(replica.read()->name, "Changed");
}
} // namespace

// Generated editors provide compound creation, nested edits and deletion without application commands.
TEST(managed_collaboration_proposal, editors_retry_conflict_and_read_only_replication) {
  authority_type authority{ledger{"Shared"}, 1};
  authority.open_session(10);
  authority.open_session(20, false);
  managed::collaboration_replica<ledger> replica;
  synchronize(replica, authority);
  std::uint32_t provisional_id{};
  auto proposal = replica.propose(10, 1, [&](auto& edit) {
    edit.root().set_name("Updated");
    auto entries = edit.root().entries();
    provisional_id = entries.insert(100, {"Supplies", 1500});
    entries.edit(provisional_id).set_amount_minor_units(1800);
  });
  EXPECT_EQ(replica.read()->name, "Shared");
  EXPECT_TRUE(replica.read()->entries.empty());
  EXPECT_TRUE(authority.read()->entries.empty());
  proposal = managed::decode_collaboration_record<records::change_proposal>(
      managed::encode_collaboration_record(proposal));
  const auto accepted = authority.submit_change(proposal, 10, 0);
  ASSERT_EQ(accepted->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.submit_change(proposal, 10, 0), accepted);
  auto stale = proposal;
  stale.operation = 2;
  EXPECT_EQ(authority.submit_change(stale, 10, 0)->status, managed::collaboration_status::conflict);
  replica.apply_accepted_change(*accepted->accepted, authority.context());
  EXPECT_EQ(replica.read()->entries.at(100).persistent_id, provisional_id);
  EXPECT_EQ(replica.read()->entries.at(100).amount_minor_units, 1800);
  const auto denied = replica.propose(20, 1, [](auto& edit) { edit.root().set_name("Forbidden"); });
  EXPECT_EQ(authority.submit_change(denied, 20, 0)->status, managed::collaboration_status::denied);
  const auto erased =
      replica.propose(10, 3, [&](auto& edit) { edit.root().entries().erase(provisional_id); });
  EXPECT_EQ(authority.submit_change(erased, 10, 0)->status,
            managed::collaboration_status::accepted);
  EXPECT_TRUE(authority.read()->entries.empty());
  synchronize(replica, authority);
  auto forged = replica.propose(10, 4, [](auto&) {});
  auto change = managed::decode_collaboration_record<records::model_change>(forged.command);
  auto value = managed::decode_collaboration_record<ledger>(change.snapshot);
  value.entries.emplace(200, ledger_example::entry{"Resurrected", 1, provisional_id});
  change.snapshot = managed::encode_collaboration_record(value);
  forged.command = managed::encode_collaboration_record(change);
  EXPECT_EQ(authority.submit_change(forged, 10, 0)->status, managed::collaboration_status::failed);
  EXPECT_TRUE(authority.read()->entries.empty());
}

// An invalid draft, cancellation or escaped editor cannot modify acknowledged state or produce a partial edit.
TEST(managed_collaboration_proposal, draft_failure_noop_and_editor_lifetime) {
  authority_type authority{ledger{"Shared"}, 1};
  authority.open_session(10);
  managed::collaboration_replica<ledger> replica;
  EXPECT_THROW(replica.propose(10, 1, [](auto&) {}), std::invalid_argument);
  synchronize(replica, authority);
  EXPECT_THROW(replica.propose(0, 1, [](auto&) {}), std::invalid_argument);
  EXPECT_THROW(replica.propose(10, 0, [](auto&) {}), std::invalid_argument);
  EXPECT_THROW(replica.propose(10, 1,
                               [](auto& edit) {
                                 edit.root().set_name("Partial");
                                 throw std::runtime_error{"Draft failed"};
                               }),
               std::runtime_error);
  EXPECT_THROW(replica.propose(10, 1, [](auto& edit) { edit.revert(); }), std::logic_error);
  EXPECT_THROW(replica.propose(10, 1,
                               [](auto& edit) {
                                 auto entries = edit.root().entries();
                                 entries.insert(100, {"First", 1});
                                 try {
                                   entries.insert(100, {"Duplicate", 2});
                                 } catch (const std::invalid_argument&) {
                                 }
                               }),
               std::invalid_argument);
  std::function<void()> stale;
  const auto noop = replica.propose(10, 1, [&](auto& edit) {
    auto root = edit.root();
    stale = [root] { root.set_name("Expired"); };
  });
  EXPECT_THROW(stale(), std::logic_error);
  EXPECT_EQ(authority.submit_change(noop, 10, 0)->status, managed::collaboration_status::no_change);
  EXPECT_EQ(replica.read()->name, "Shared");
  EXPECT_EQ(replica.sequence(), 0u);
}

// An application-command endpoint cannot accidentally interpret a built-in model payload, or vice versa.
TEST(managed_collaboration_proposal, protocol_binding_preserves_custom_command_endpoints) {
  const managed::document_id document{1, 2};
  authority_type automatic{ledger{"Shared"}, 1, document};
  bool called = false;
  authority_type custom{ledger{"Shared"}, 1, [&](auto&, auto) { called = true; }, document};
  automatic.open_session(10);
  custom.open_session(10);
  managed::collaboration_replica<ledger> replica;
  synchronize(replica, custom);
  EXPECT_THROW(replica.propose(10, 1, [](auto&) {}), std::invalid_argument);
  records::change_proposal custom_proposal{custom.context(), 10, 1, 0, {}, {}};
  EXPECT_THROW(automatic.submit_change(custom_proposal, 10, 0), std::invalid_argument);
  managed::collaboration_replica<ledger> writer;
  synchronize(writer, automatic);
  const auto proposal = writer.propose(10, 1, [](auto&) {});
  EXPECT_THROW(custom.submit_change(proposal, 10, 0), std::invalid_argument);
  EXPECT_FALSE(called);
}

// A denied creation still consumes authoritative IDs; a fresh same-sequence baseline is required.
TEST(managed_collaboration_proposal, allocator_conflict_prevents_reuse_after_denied_creation) {
  bool permit = false;
  authority_type authority{ledger{"Shared"},
                           1,
                           managed::document_id{1, 2},
                           {},
                           {},
                           [&](auto, const auto&, const auto&, const auto&) { return permit; }};
  authority.open_session(10);
  managed::collaboration_replica<ledger> replica;
  synchronize(replica, authority);
  const auto create = [](auto& edit) { edit.root().entries().insert(100, {"Entry", 1}); };
  const auto denied = replica.propose(10, 1, create);
  ASSERT_EQ(authority.submit_change(denied, 10, 0)->status, managed::collaboration_status::denied);
  EXPECT_EQ(authority.snapshot().allocated_id, 2u);
  EXPECT_EQ(authority.sequence(), 0u);
  permit = true;
  const auto stale = replica.propose(10, 2, create);
  EXPECT_EQ(authority.submit_change(stale, 10, 0)->status, managed::collaboration_status::conflict);
  synchronize(replica, authority);
  const auto fresh = replica.propose(10, 3, create);
  ASSERT_EQ(authority.submit_change(fresh, 10, 0)->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.read()->entries.at(100).persistent_id, 3u);
}

// Both authoring and authority admission bound payload bytes and new-ID reservations.
TEST(managed_collaboration_proposal, limits_and_malformed_model_payloads) {
  managed::collaboration_options options;
  options.max_created_ids = 1;
  authority_type authority{ledger{"Shared"}, 1, managed::document_id{1, 2}, options};
  authority.open_session(10);
  managed::collaboration_replica<ledger> replica;
  synchronize(replica, authority);
  auto bounded = options;
  bounded.max_created_ids = 0;
  EXPECT_THROW(
      replica.propose(
          10, 1, [](auto& edit) { edit.root().entries().insert(1, {"New", 1}); }, {}, bounded),
      std::length_error);
  bounded.max_command_bytes = 1;
  EXPECT_ANY_THROW(replica.propose(10, 1, [](auto&) {}, {}, bounded));
  auto proposal = replica.propose(10, 1, [](auto&) {});
  const auto valid = managed::decode_collaboration_record<records::model_change>(proposal.command);
  std::vector<records::model_change> invalid;
  auto change = valid;
  ++change.format_version;
  invalid.push_back(change);
  change = valid;
  change.allocated_id = change.base_allocated_id - 1;
  invalid.push_back(change);
  change = valid;
  change.allocated_id = change.base_allocated_id + options.max_created_ids + 1;
  invalid.push_back(change);
  change = valid;
  change.allocated_id = std::numeric_limits<std::uint64_t>::max();
  invalid.push_back(change);
  change = valid;
  change.snapshot.push_back(0);
  invalid.push_back(change);
  change = valid;
  auto value = managed::decode_collaboration_record<ledger>(change.snapshot);
  value.persistent_id = 0;
  change.snapshot = managed::encode_collaboration_record(value);
  invalid.push_back(change);
  for (const auto& payload : invalid) {
    proposal.command = managed::encode_collaboration_record(payload);
    EXPECT_EQ(authority.submit_change(proposal, 10, 0)->status,
              managed::collaboration_status::failed);
    ++proposal.operation;
    EXPECT_EQ(authority.read()->name, "Shared");
    EXPECT_EQ(authority.sequence(), 0u);
    EXPECT_EQ(authority.snapshot().allocated_id, 1u);
  }
  proposal.command = managed::encode_collaboration_record(valid);
  proposal.command.push_back(0);
  EXPECT_EQ(authority.submit_change(proposal, 10, 0)->status,
            managed::collaboration_status::failed);
}

// Direct and subtree locks apply to the actual candidate diff even with a default snapshot payload.
TEST(managed_collaboration_proposal, final_diff_still_requires_lock_grants) {
  managed::collaboration_options options;
  options.require_edit_lock = true;
  authority_type authority{ledger{"Shared", {{100, {"Entry", 1}}}}, 1, managed::document_id{1, 2},
                           options};
  authority.open_session(10);
  authority.open_session(20);
  managed::collaboration_replica<ledger> replica;
  synchronize(replica, authority);
  const auto modify = [](auto& edit) { edit.root().entries().edit(2).set_amount_minor_units(2); };
  EXPECT_EQ(authority.submit_change(replica.propose(10, 1, modify), 10, 0)->status,
            managed::collaboration_status::denied);
  const auto lock = authority.change_lock({authority.context(), 10, 2, 1, 1, 2, 0, 1000}, 10, 0);
  ASSERT_EQ(lock->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.submit_change(replica.propose(20, 1, modify), 20, 0)->status,
            managed::collaboration_status::conflict);
  EXPECT_EQ(authority.submit_change(replica.propose(10, 3, modify), 10, 0)->status,
            managed::collaboration_status::obsolete_grant);
  const auto proposal = replica.propose(10, 4, modify, {{1, lock->grant.generation}});
  EXPECT_EQ(authority.submit_change(proposal, 10, 0)->status,
            managed::collaboration_status::accepted);
}

// Generic proposals use the existing history policy and journal rather than a separate publication path.
TEST(managed_collaboration_proposal, history_modes_and_journal_recovery) {
  verify_history_policy<managed::history_mode::linear, managed::history_labels::disabled>();
  verify_history_policy<managed::history_mode::linear, managed::history_labels::enabled>();
  verify_history_policy<managed::history_mode::tree, managed::history_labels::disabled>();
  verify_history_policy<managed::history_mode::tree, managed::history_labels::enabled>();
  verify_history_policy<managed::history_mode::disabled, managed::history_labels::disabled>();
  const auto document = managed::make_document_id();
  const auto directory =
      std::filesystem::current_path() / ("collaboration-proposal-" + std::to_string(document.high));
  ASSERT_TRUE(std::filesystem::create_directory(directory));
  const auto journal = directory / "document";
  {
    authority_type authority{ledger{"Shared"}, 1, document};
    authority.create_journal(journal);
    authority.open_session(10);
    managed::collaboration_replica<ledger> replica;
    synchronize(replica, authority);
    const auto proposal = replica.propose(
        10, 1, [](auto& edit) { edit.root().entries().insert(100, {"Durable", 123}); });
    ASSERT_EQ(authority.submit_change(proposal, 10, 0)->status,
              managed::collaboration_status::accepted);
  }
  {
    authority_type restored{ledger{}, 2};
    restored.recover_journal(journal);
    EXPECT_EQ(restored.read()->entries.at(100).memo, "Durable");
    EXPECT_EQ(restored.read()->entries.at(100).persistent_id, 2u);
  }
  std::filesystem::remove_all(directory);
}
