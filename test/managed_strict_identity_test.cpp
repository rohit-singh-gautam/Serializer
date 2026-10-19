// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <collaboration_model.hpp>
#include <gtest/gtest.h>
#include <rohit/managed_collaboration.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using authority_type = managed::collaboration_authority<ledger>;
using store_type = managed::model_store<ledger>;
using replica_type = managed::collaboration_replica<ledger>;

// Reserve an exclusively owned temporary journal directory and remove only that tree.
class identity_directory {
  std::filesystem::path path_;
public:
  // Create a fresh directory before writing any durable fixture.
  identity_directory() : path_{std::filesystem::current_path() /
      ("serializer-strict-identity-" + std::to_string(managed::make_document_id().high))} {
    if (!std::filesystem::create_directory(path_)) { throw std::runtime_error{"Cannot reserve identity test directory"}; }
  }
  // Release only the exact test-owned directory after journal handles have closed.
  ~identity_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  // Return a stable file prefix while this directory remains alive.
  std::filesystem::path journal() const { return path_ / "client"; }
};
} // namespace

// Native document/entity IDs are equal before and after each participant accepts independent creations.
TEST(managed_strict_identity, offline_participants_keep_exact_ids_without_mapping_tables) {
  using workspace = collaboration_test::workspace;
  managed::collaboration_authority<workspace> authority{workspace{{{1, {"A"}}, {2, {"B"}}}}, 1};
  authority.open_session(10);
  authority.open_session(20);
  managed::model_store<workspace> alice{workspace{}}, bob{workspace{}};
  managed::collaboration_transport a{authority, std::uint64_t{10}}, b{authority, std::uint64_t{20}};
  alice.collaborate(std::uint64_t{10}).bind(a);
  bob.collaborate(std::uint64_t{20}).bind(b);
  alice.synchronize();
  bob.synchronize();
  const auto alice_range = alice.allocation_context();
  const auto bob_range = bob.allocation_context();
  EXPECT_LT(alice_range.last, bob_range.first);
  EXPECT_EQ(alice.document(), bob.document());
  EXPECT_EQ(alice.document().high, authority.context().document_high);
  EXPECT_EQ(alice.document().low, authority.context().document_low);
  std::uint32_t first{}, second{};
  alice.execute_transaction([&](auto& edit) {
    first = edit.root().ledgers().edit(2).entries().insert(100, {"Alice", 1});
  }).throw_if_failed();
  bob.execute_transaction([&](auto& edit) {
    second = edit.root().ledgers().edit(3).entries().insert(200, {"Bob", 2});
  }).throw_if_failed();
  EXPECT_NE(first, second);
  EXPECT_GE(first, alice_range.first);
  EXPECT_LE(first, alice_range.last);
  EXPECT_GE(second, bob_range.first);
  EXPECT_LE(second, bob_range.last);
  bob.synchronize();
  alice.synchronize();
  bob.synchronize();
  for (auto* store : {&alice, &bob}) {
    EXPECT_EQ(store->read()->ledgers.at(1).entries.at(100).persistent_id, first);
    EXPECT_EQ(store->read()->ledgers.at(2).entries.at(200).persistent_id, second);
    EXPECT_TRUE(store->collaboration().state()->identities.empty());
    EXPECT_TRUE(store->collaboration().state()->in_flight_ids.empty());
  }
  EXPECT_EQ(authority.read()->ledgers.at(1).entries.at(100).persistent_id, first);
  EXPECT_EQ(authority.read()->ledgers.at(2).entries.at(200).persistent_id, second);
}

// Missing/exhausted native authority capacity rejects creation before an ID escapes its editor.
TEST(managed_strict_identity, standalone_replica_needs_capacity_before_exposure) {
  authority_type authority{ledger{"Shared"}, 1};
  authority.open_session(10);
  replica_type replica;
  replica.synchronize(authority.snapshot(), authority.context());
  std::uint32_t exposed{};
  EXPECT_ANY_THROW(replica.propose(10, 1, [&](auto& edit) {
    exposed = edit.root().entries().insert(100, {"No capacity", 1});
  }));
  EXPECT_EQ(exposed, 0U);
  replica.initialize_identity_lease(10, authority.reserve_ids(10, 1));
  const auto proposal = replica.propose(10, 2, [&](auto& edit) {
    exposed = edit.root().entries().insert(100, {"Issued", 1});
  });
  ASSERT_NE(exposed, 0U);
  const auto issued = exposed;
  exposed = 0;
  EXPECT_ANY_THROW(replica.propose(10, 3, [&](auto& edit) {
    exposed = edit.root().entries().insert(200, {"Exhausted", 2});
  }));
  EXPECT_EQ(exposed, 0U);
  const auto accepted = authority.submit_change(proposal, 10, 0);
  ASSERT_EQ(accepted->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.read()->entries.at(100).persistent_id, issued);
}

// Canceled draft identities and authority-reserved ranges remain burned after checkpoint recovery.
TEST(managed_strict_identity, canceled_ids_and_authority_restart_never_reuse_ranges) {
  authority_type authority{ledger{"Shared"}, 1};
  authority.open_session(10);
  replica_type replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto reservation = authority.reserve_ids(10, 2);
  replica.initialize_identity_lease(10, reservation);
  std::uint32_t canceled{}, following{};
  EXPECT_ANY_THROW(replica.propose(10, 1, [&](auto& edit) {
    canceled = edit.root().entries().insert(100, {"Canceled", 1});
    edit.revert();
  }));
  ASSERT_NE(canceled, 0U);
  const auto proposal = replica.propose(10, 2, [&](auto& edit) {
    following = edit.root().entries().insert(200, {"Following", 2});
  });
  EXPECT_GT(following, canceled);
  EXPECT_EQ(authority.submit_change(proposal, 10, 0)->status, managed::collaboration_status::accepted);
  authority_type recovered{ledger{}, 2};
  recovered.load_document(authority.save_document());
  recovered.open_session(10);
  EXPECT_EQ(recovered.read()->entries.at(200).persistent_id, following);
  EXPECT_EQ(recovered.context().document_high, authority.context().document_high);
  EXPECT_EQ(recovered.context().document_low, authority.context().document_low);
  EXPECT_GT(recovered.reserve_ids(10, 1).first, reservation.last);
}

// An unused issued range is durable before return, even without a later Save or accepted edit.
TEST(managed_strict_identity, authority_journal_recovers_unacknowledged_range_reservation) {
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    identity_directory directory;
    managed::identity_allocation_context issued;
    {
      authority_type authority{ledger{"Shared"}, 1};
      authority.create_journal(directory.journal(), mode);
      authority.open_session(10);
      issued = authority.reserve_ids(10, 3);
      ASSERT_NE(issued.first, 0U);
    }
    {
      authority_type recovered{ledger{}, 2};
      recovered.recover_journal(directory.journal());
      recovered.open_session(10);
      EXPECT_GT(recovered.reserve_ids(10, 1).first, issued.last);
      EXPECT_TRUE(recovered.read()->entries.empty());
    }
  }
}

// Recovery retains committed lease cursors, so deleting an accepted object cannot revive its old ID.
TEST(managed_strict_identity, deleted_accepted_identity_cannot_resurrect_after_authority_recovery) {
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    identity_directory directory;
    managed::identity_allocation_context issued;
    std::uint32_t retired{};
    {
      authority_type authority{ledger{"Shared"}, 1};
      authority.create_journal(directory.journal(), mode);
      authority.open_session(10);
      replica_type replica;
      replica.synchronize(authority.snapshot(), authority.context());
      issued = authority.reserve_ids(10, 2);
      replica.initialize_identity_lease(10, issued);
      const auto create = replica.propose(10, 1, [&](auto& edit) {
        retired = edit.root().entries().insert(100, {"Retired", 1});
      });
      ASSERT_EQ(authority.submit_change(create, 10, 0)->status, managed::collaboration_status::accepted);
      replica.synchronize(authority.snapshot(), authority.context());
      const auto erase = replica.propose(10, 2, [&](auto& edit) { edit.root().entries().erase(retired); });
      ASSERT_EQ(authority.submit_change(erase, 10, 0)->status, managed::collaboration_status::accepted);
    }
    {
      authority_type recovered{ledger{}, 2};
      recovered.recover_journal(directory.journal());
      recovered.open_session(10);
      ASSERT_TRUE(recovered.read()->entries.empty());
      replica_type replica;
      replica.synchronize(recovered.snapshot(), recovered.context());
      auto forged = replica.propose(10, 3, [](auto&) {});
      auto change = managed::decode_collaboration_record<managed::collaboration::model_change>(forged.command);
      auto value = managed::decode_collaboration_record<ledger>(change.snapshot);
      value.entries.emplace(200, ledger_example::entry{"Resurrected", 1, retired});
      change.snapshot = managed::encode_collaboration_record(value);
      change.base_allocated_id = issued.first;
      change.allocated_id = retired;
      forged.command = managed::encode_collaboration_record(change);
      EXPECT_NE(recovered.submit_change(forged, 10, 0)->status, managed::collaboration_status::accepted);
      EXPECT_TRUE(recovered.read()->entries.empty());
      EXPECT_GT(recovered.reserve_ids(10, 1).first, issued.last);
    }
  }
}

// Metadata-only consumption precedes exposure, so an unsaved canceled edit survives journal replay.
TEST(managed_strict_identity, canceled_client_allocation_is_durable_without_full_save) {
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    identity_directory directory;
    authority_type authority{ledger{"Shared"}, 1};
    authority.open_session(10);
    managed::collaboration_transport transport{authority, std::uint64_t{10}};
    std::uint32_t canceled{}, following{};
    {
      store_type store{ledger{}};
      store.collaborate(std::uint64_t{10}).bind(transport);
      store.synchronize();
      store.create_journal(directory.journal(), mode);
      EXPECT_EQ(store.execute_transaction([&](auto& edit) {
        canceled = edit.root().entries().insert(100, {"Canceled", 1});
        edit.revert();
      }).status, managed::transaction_status::reverted);
      ASSERT_NE(canceled, 0U);
    }
    {
      store_type recovered{ledger{}};
      recovered.collaborate(std::uint64_t{10}).bind(transport);
      recovered.recover_journal(directory.journal());
      EXPECT_TRUE(recovered.read()->entries.empty());
      recovered.execute_transaction([&](auto& edit) {
        following = edit.root().entries().insert(200, {"Recovered", 2});
      }).throw_if_failed();
      EXPECT_GT(following, canceled);
      recovered.synchronize();
      EXPECT_EQ(authority.read()->entries.at(200).persistent_id, following);
    }
  }
}

// Preview reservations burn the native client cursor before exposure, including failed edits and journal reopening.
TEST(managed_strict_identity, canceled_preview_tokens_never_retarget_after_client_journal_recovery) {
  using entry = ledger_example::entry;
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    identity_directory directory;
    authority_type authority{ledger{"Shared"}, 1};
    authority.open_session(10);
    managed::collaboration_transport transport{authority, std::uint64_t{10}};
    managed::managed_ref<entry> discarded;
    std::uint32_t failed{}, canceled{}, following{};
    std::size_t guarded_reservations{};
    {
      store_type store{ledger{}};
      store.collaborate(std::uint64_t{10}).bind(transport);
      store.synchronize();
      bool armed{};
      managed::journal_options options;
      options.fault_injector = [&](const auto point) {
        if (armed && point == managed::journal_io_event::before_append) {
          ++guarded_reservations;
          EXPECT_THROW(store.reserve_preview_identity(store.allocation_context()), std::logic_error);
          EXPECT_EQ(store.execute_transaction([](auto& edit) { edit.root().set_name("Nested"); }).status,
                    managed::transaction_status::failed);
        }
      };
      store.create_journal(directory.journal(), mode, options);
      armed = true;
      const auto pin = store.read();
      auto proposal = store.preview();
      EXPECT_EQ(proposal.working().execute_transaction([&](auto& edit) {
        failed = edit.root().entries().insert(100, {"Failed preview", 1});
        throw std::runtime_error{"Discard this working transaction"};
      }).status, managed::transaction_status::failed);
      ASSERT_NE(failed, 0U);
      proposal.working().execute_transaction([&](auto& edit) {
        canceled = edit.root().entries().insert(200, {"Canceled preview", 2});
      }).throw_if_failed();
      EXPECT_GT(canceled, failed);
      discarded = proposal.working().registry().template reference<entry>(canceled);
      proposal.cancel();
      EXPECT_EQ(store.read(), pin);
      EXPECT_TRUE(store.collaboration().state()->pending.empty());
      EXPECT_THROW(store.registry().resolve(discarded), managed::registry_error);
      EXPECT_GE(guarded_reservations, 2U);
    }
    {
      store_type recovered{ledger{}};
      recovered.collaborate(std::uint64_t{10}).bind(transport);
      recovered.recover_journal(directory.journal());
      EXPECT_TRUE(recovered.read()->entries.empty());
      recovered.execute_transaction([&](auto& edit) {
        following = edit.root().entries().insert(300, {"Live after recovery", 3});
      }).throw_if_failed();
      EXPECT_GT(following, canceled);
      EXPECT_THROW(recovered.registry().resolve(discarded), managed::registry_error);
      recovered.synchronize();
      EXPECT_EQ(authority.read()->entries.at(300).persistent_id, following);
      EXPECT_THROW(recovered.registry().resolve(discarded), managed::registry_error);
    }
  }
}

// A range is bound to its authenticated owner; changing the session cannot transfer its IDs.
TEST(managed_strict_identity, foreign_session_cannot_publish_a_leased_identity) {
  authority_type authority{ledger{"Shared"}, 1};
  authority.open_session(10);
  authority.open_session(20);
  replica_type replica;
  replica.synchronize(authority.snapshot(), authority.context());
  replica.initialize_identity_lease(10, authority.reserve_ids(10, 2));
  std::uint32_t created{};
  const auto proposal = replica.propose(10, 1, [&](auto& edit) {
    created = edit.root().entries().insert(100, {"Alice", 1});
  });
  auto forged = proposal;
  forged.session = 20;
  EXPECT_NE(authority.submit_change(forged, 20, 0)->status, managed::collaboration_status::accepted);
  EXPECT_TRUE(authority.read()->entries.empty());
  ASSERT_EQ(authority.submit_change(proposal, 10, 0)->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.read()->entries.at(100).persistent_id, created);
}

// Old mapped checkpoints fail atomically instead of activating a translated-identity mode.
TEST(managed_strict_identity, mapped_client_checkpoint_is_rejected_before_publication) {
  authority_type authority{ledger{"Shared"}, 1};
  authority.open_session(10);
  managed::collaboration_transport transport{authority, std::uint64_t{10}};
  store_type source{ledger{}};
  source.collaborate(std::uint64_t{10}).bind(transport);
  source.synchronize();
  auto checkpoint = managed::decode_collaboration_record<managed::collaboration::client_checkpoint>(source.save());
  checkpoint.state.format_version = 1;
  checkpoint.state.identities.push_back({1U, 2U});
  store_type fresh{ledger{"Unchanged"}};
  fresh.collaborate(std::uint64_t{10});
  const auto pin = fresh.read();
  EXPECT_THROW(fresh.load(managed::encode_collaboration_record(checkpoint)), std::invalid_argument);
  EXPECT_EQ(fresh.read(), pin);
  EXPECT_EQ(fresh.read()->name, "Unchanged");
}
