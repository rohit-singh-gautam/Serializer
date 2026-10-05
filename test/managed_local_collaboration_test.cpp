#include <collaboration_model.hpp>
#include <gtest/gtest.h>
#include <rohit/managed_collaboration.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <thread>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;
using authority_type = managed::collaboration_authority<ledger>;
using pending_status = managed::pending_change_status;

// Two stores use normal transactions while their independently authenticated transports synchronize.
class local_collaboration_test : public testing::Test {
protected:
  authority_type authority{ledger{"Original", {{100, {"Entry", 5}}}}, 1};
  store_type alice{ledger{}}, bob{ledger{}};
  managed::collaboration_transport<authority_type> alice_transport{authority, 10};
  managed::collaboration_transport<authority_type> bob_transport{authority, 20};
  // Join before editing; subsequent disconnected edits need no network calls.
  void SetUp() override {
    authority.open_session(10);
    authority.open_session(20);
    alice.collaborate(std::uint64_t{10}).bind(alice_transport);
    bob.collaborate(std::uint64_t{20}).bind(bob_transport);
    alice.synchronize();
    bob.synchronize();
  }
};

// Preserve the real authority decision while simulating a lost response or an offline connection.
struct unreliable_transport {
  managed::collaboration_transport<authority_type> direct;
  bool lose_reply{};
  bool offline{};
  std::size_t sends{};
  std::function<void()> after_submit{};
  // Bind a trusted test session independently of incoming messages.
  unreliable_transport(authority_type& authority, std::uint64_t session)
      : direct{authority, session} {}
  // Context and reads remain available so a lost write response can be exercised independently.
  auto context() {
    return direct.context();
  }
  auto snapshot() {
    return direct.snapshot();
  }
  auto accepted_since(std::uint64_t cursor) {
    return direct.accepted_since(cursor);
  }
  // Fail before or after acceptance without changing the submitted bytes.
  auto submit_change(const authority_type::change_proposal_type& request) {
    ++sends;
    if (offline) {
      throw std::runtime_error{"offline"};
    }
    auto result = direct.submit_change(request);
    if (after_submit) {
      after_submit();
    }
    if (lose_reply) {
      lose_reply = false;
      throw std::runtime_error{"lost reply"};
    }
    return result;
  }
};
} // namespace

// Normal callback edits immediately publish locally; explicit synchronization never reruns the callback.
TEST_F(local_collaboration_test, immediate_edit_and_separate_sync) {
  std::size_t callbacks = 0;
  const auto outcome = alice.execute_transaction([&](auto& edit) {
    ++callbacks;
    edit.root().set_name("Local");
  });
  EXPECT_EQ(outcome.status, managed::transaction_status::committed);
  EXPECT_EQ(alice.read()->name, "Local");
  EXPECT_EQ(authority.read()->name, "Original");
  ASSERT_EQ(alice.collaboration().state()->pending.size(), 1u);
  alice.synchronize();
  bob.synchronize();
  EXPECT_EQ(authority.read()->name, "Local");
  EXPECT_EQ(bob.read()->name, "Local");
  EXPECT_EQ(callbacks, 1u);
  EXPECT_TRUE(alice.collaboration().state()->pending.empty());
}

// Manual/scoped transactions share capture, including no-op, revert and failure behavior.
TEST_F(local_collaboration_test, all_existing_transaction_forms) {
  managed::transaction_outcome outcome;
  {
    auto edit = alice.begin_transaction(outcome);
    edit.root().set_name("Manual");
    edit.commit();
  }
  outcome.throw_if_failed();
  {
    auto edit = alice.begin_transaction(outcome);
    edit.root().entries().edit(2).set_memo("Scoped");
  }
  outcome.throw_if_failed();
  EXPECT_EQ(alice.execute_transaction([](auto&) {}).status, managed::transaction_status::no_change);
  EXPECT_EQ(alice
                .execute_transaction([](auto& edit) {
                  edit.root().set_name("No");
                  edit.revert();
                })
                .status,
            managed::transaction_status::reverted);
  EXPECT_EQ(alice
                .execute_transaction([](auto& edit) {
                  edit.root().set_name("No");
                  throw std::runtime_error{"fail"};
                })
                .status,
            managed::transaction_status::failed);
  EXPECT_EQ(alice.collaboration().state()->pending.size(), 2u);
  alice.synchronize();
  EXPECT_EQ(authority.read()->name, "Manual");
  EXPECT_EQ(authority.read()->entries.at(100).memo, "Scoped");
}

// Independent remote fields merge into the optimistic view without dropping queued local intent.
TEST_F(local_collaboration_test, reconcile_independent_remote_changes) {
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Alice"); }).throw_if_failed();
  bob.execute_transaction([](auto& edit) { edit.root().entries().edit(2).set_memo("Bob"); })
      .throw_if_failed();
  bob.synchronize();
  alice.synchronize();
  EXPECT_EQ(alice.read()->name, "Alice");
  EXPECT_EQ(alice.read()->entries.at(100).memo, "Bob");
  EXPECT_EQ(authority.read()->name, "Alice");
}

// A conflict preserves the local view and archived transaction while exposing the latest authority view.
TEST_F(local_collaboration_test, same_field_conflict_and_explicit_resolution) {
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Alice"); }).throw_if_failed();
  bob.execute_transaction([](auto& edit) { edit.root().set_name("Bob"); }).throw_if_failed();
  bob.synchronize();
  alice.synchronize();
  EXPECT_EQ(alice.read()->name, "Alice");
  EXPECT_EQ(alice.collaboration().acknowledged_read()->name, "Bob");
  EXPECT_EQ(alice.collaboration().state()->transactions.front().status,
            static_cast<std::uint32_t>(pending_status::conflict));
  const auto retained = alice.collaboration().discard_pending();
  EXPECT_EQ(retained->name, "Alice");
  EXPECT_FALSE(alice.collaboration().state()->transactions.front().after.empty());
  alice.execute_transaction([&](auto& edit) { edit.root().set_name(retained->name + " and Bob"); })
      .throw_if_failed();
  alice.synchronize();
  EXPECT_EQ(authority.read()->name, "Alice and Bob");
}

// Remote write-away/write-back is still a conflict even though ordinary three-way values match.
TEST_F(local_collaboration_test, aba_changes_are_not_silently_overwritten) {
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Alice"); }).throw_if_failed();
  bob.execute_transaction([](auto& edit) { edit.root().set_name("Temporary"); }).throw_if_failed();
  bob.execute_transaction([](auto& edit) { edit.root().set_name("Original"); }).throw_if_failed();
  bob.synchronize();
  alice.synchronize();
  EXPECT_EQ(authority.read()->name, "Original");
  EXPECT_EQ(alice.read()->name, "Alice");
  EXPECT_FALSE(alice.collaboration().state()->pending.empty());
}

// Rejected authorization preserves local work and retries only after an explicit application decision.
TEST_F(local_collaboration_test, rejection_and_retry) {
  authority.set_session_writable(10, false);
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Pending"); }).throw_if_failed();
  alice.synchronize();
  EXPECT_EQ(alice.read()->name, "Pending");
  EXPECT_EQ(alice.collaboration().state()->transactions.front().status,
            static_cast<std::uint32_t>(pending_status::denied));
  authority.set_session_writable(10, true);
  alice.collaboration().retry_pending();
  alice.synchronize();
  EXPECT_EQ(authority.read()->name, "Pending");
}

// Queued undo/redo operates locally immediately and synchronizes as authority-validated inverses.
TEST_F(local_collaboration_test, offline_and_accepted_undo_redo) {
  alice.execute_transaction([](auto& edit) { edit.root().set_name("First"); }).throw_if_failed();
  alice.undo();
  EXPECT_EQ(alice.read()->name, "Original");
  alice.redo();
  EXPECT_EQ(alice.read()->name, "First");
  EXPECT_EQ(authority.sequence(), 0u);
  alice.synchronize();
  EXPECT_EQ(authority.sequence(), 3u);
  bob.synchronize();
  bob.execute_transaction([](auto& edit) { edit.root().entries().edit(2).set_memo("Other"); })
      .throw_if_failed();
  bob.synchronize();
  alice.synchronize();
  alice.undo();
  EXPECT_EQ(alice.read()->name, "Original");
  EXPECT_EQ(alice.read()->entries.at(100).memo, "Other");
  alice.synchronize();
  EXPECT_EQ(authority.read()->name, "Original");
  EXPECT_EQ(authority.read()->entries.at(100).memo, "Other");
}

// Independent offline allocators retain their local handles and map new entities to fresh server IDs.
TEST(local_collaboration, offline_identity_mapping) {
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
  std::uint32_t local_a{}, local_b{};
  alice
      .execute_transaction([&](auto& edit) {
        local_a = edit.root().ledgers().edit(2).entries().insert(100, {"Alice", 1});
      })
      .throw_if_failed();
  bob.execute_transaction([&](auto& edit) {
       local_b = edit.root().ledgers().edit(3).entries().insert(200, {"Bob", 2});
     })
      .throw_if_failed();
  EXPECT_EQ(local_a, local_b);
  bob.synchronize();
  alice.synchronize();
  bob.synchronize();
  EXPECT_EQ(alice.read()->ledgers.at(1).entries.at(100).persistent_id, local_a);
  EXPECT_NE(alice.collaboration().remote_id(local_a), bob.collaboration().remote_id(local_b));
  alice
      .execute_transaction([&](auto& edit) {
        edit.root().ledgers().edit(2).entries().edit(local_a).set_memo("Again");
      })
      .throw_if_failed();
  alice.synchronize();
  EXPECT_EQ(authority.read()->ledgers.at(1).entries.at(100).memo, "Again");
}

// Lost replies leave exact request bytes until authority deduplication resolves the delivery.
TEST_F(local_collaboration_test, lost_acknowledgement_retries_exactly) {
  unreliable_transport transport{authority, 10};
  alice.collaboration().bind(transport);
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Once"); }).throw_if_failed();
  transport.lose_reply = true;
  EXPECT_THROW(alice.synchronize(), std::runtime_error);
  const auto pending = alice.collaboration().state()->in_flight;
  EXPECT_FALSE(pending.empty());
  EXPECT_THROW(alice.collaboration().discard_pending(), std::logic_error);
  EXPECT_EQ(authority.sequence(), 1u);
  alice.synchronize();
  EXPECT_EQ(authority.sequence(), 1u);
  EXPECT_EQ(transport.sends, 2u);
  EXPECT_TRUE(alice.collaboration().state()->pending.empty());
}

// Owner-thread timer ticks and explicit flush honor independently configurable synchronization cadence.
TEST_F(local_collaboration_test, timer_and_explicit_flush) {
  alice.collaboration().set_sync_interval(2000);
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Timer"); }).throw_if_failed();
  EXPECT_FALSE(alice.synchronize_if_due(100));
  EXPECT_FALSE(alice.synchronize_if_due(2099));
  EXPECT_TRUE(alice.synchronize_if_due(2100));
  EXPECT_EQ(authority.read()->name, "Timer");
  EXPECT_THROW(alice.synchronize_if_due(2000), std::invalid_argument);
  alice.collaboration().set_sync_interval(0);
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Flush"); }).throw_if_failed();
  EXPECT_FALSE(alice.synchronize_if_due(9999));
  alice.synchronize();
  EXPECT_EQ(authority.read()->name, "Flush");
}

// Sending and receiving are separate store operations; receiving never submits queued local work.
TEST_F(local_collaboration_test, independent_send_and_receive) {
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Alice"); }).throw_if_failed();
  bob.execute_transaction([](auto& edit) { edit.root().entries().edit(2).set_memo("Bob"); })
      .throw_if_failed();
  bob.send_pending();
  EXPECT_EQ(bob.collaboration().state()->sequence, 0u);
  alice.receive_changes();
  EXPECT_EQ(authority.read()->name, "Original");
  EXPECT_EQ(alice.read()->name, "Alice");
  EXPECT_EQ(alice.read()->entries.at(100).memo, "Bob");
  alice.send_pending();
  EXPECT_EQ(authority.read()->name, "Alice");
  bob.receive_changes();
  EXPECT_EQ(bob.read()->name, "Alice");
  EXPECT_TRUE(alice.collaboration().state()->pending.empty());
}

// Labels describe local actions after synchronization, and all native history modes use the same store path.
TEST(local_collaboration, labeled_and_disabled_history) {
  authority_type authority{ledger{"Original", {{100, {"Entry", 5}}}}, 1};
  authority.open_session(10);
  authority.open_session(20);
  managed::collaboration_transport a{authority, std::uint64_t{10}}, b{authority, std::uint64_t{20}};
  managed::model_store<ledger, managed::history_mode::linear, managed::history_labels::enabled>
      labeled{ledger{}};
  labeled.collaborate(std::uint64_t{10}).bind(a);
  labeled.synchronize();
  labeled.execute_transaction("Rename", [](auto& edit) { edit.root().set_name("Named"); })
      .throw_if_failed();
  labeled.synchronize();
  EXPECT_EQ(labeled.undo_label(), "Rename");
  labeled.undo();
  EXPECT_EQ(labeled.redo_label(), "Rename");
  labeled.redo();
  labeled.synchronize();
  managed::model_store<ledger, managed::history_mode::disabled> disabled{ledger{}};
  disabled.collaborate(std::uint64_t{20}).bind(b);
  disabled.synchronize();
  disabled
      .execute_transaction([](auto& edit) { edit.root().entries().edit(2).set_memo("Disabled"); })
      .throw_if_failed();
  disabled.synchronize();
  EXPECT_EQ(authority.read()->entries.at(100).memo, "Disabled");
}

// Save/load binds the application session and preserves local history, drafts and synchronization position.
TEST_F(local_collaboration_test, memory_checkpoint_and_session_binding) {
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Saved"); }).throw_if_failed();
  const auto bytes = alice.save();
  store_type restored{ledger{}};
  restored.collaborate(std::uint64_t{10}).bind(alice_transport);
  restored.load(bytes);
  EXPECT_EQ(restored.read()->name, "Saved");
  restored.undo();
  EXPECT_EQ(restored.read()->name, "Original");
  restored.synchronize();
  EXPECT_EQ(authority.read()->name, "Original");
  store_type wrong{ledger{}};
  wrong.collaborate(std::uint64_t{20});
  EXPECT_THROW(wrong.load(bytes), std::invalid_argument);
}

// Lock reservations share edit IDs, and journal recovery must never hand out a reserved ID again.
TEST(local_collaboration, coordination_operation_ids_survive_recovery) {
  constexpr std::uint64_t session = 10;
  constexpr std::uint64_t lease_duration_ms = 1000;
  for (const auto mode :
       {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    authority_type authority{ledger{"Original"}, 1};
    authority.open_session(session);
    managed::collaboration_transport transport{authority, session};
    const auto directory = std::filesystem::current_path() /
                           ("local-lock-ids-" + std::to_string(managed::make_document_id().high));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    managed::collaboration::lock_request request;
    std::uint64_t reserved_before_recovery{};
    {
      store_type store{ledger{}};
      auto& client = store.collaborate(session);
      client.bind(transport);
      EXPECT_THROW(client.reserve_operation_id(), std::logic_error);
      store.synchronize();
      store.create_journal(directory / "client", mode);
      request = {authority.context(), session, client.reserve_operation_id(),
                 static_cast<std::uint32_t>(managed::edit_lock_action::acquire),
                 client.remote_id(store.read()->persistent_id),
                 static_cast<std::uint32_t>(managed::edit_lock_scope::owned_subtree),
                 0, lease_duration_ms};
      const auto acquired = authority.change_lock(request, session, 0);
      ASSERT_EQ(acquired->status, managed::collaboration_status::accepted);
      EXPECT_EQ(authority.change_lock(request, session, 0), acquired);
      client.set_grants({{request.target, acquired->grant.generation}});
      store.execute_transaction([](auto& edit) { edit.root().set_name("With lease"); })
          .throw_if_failed();
      store.synchronize();
      ASSERT_EQ(client.pending_count(), 0u);
      EXPECT_GT(authority.accepted_since(0).back()->operation, request.operation);
      request.generation = acquired->grant.generation;
      reserved_before_recovery = client.reserve_operation_id();
      // No full Save: replay must recover the metadata-only reservation from the journal tail.
    }
    {
      store_type recovered{ledger{}};
      auto& client = recovered.collaborate(session);
      client.bind(transport);
      recovered.recover_journal(directory / "client");
      request.operation = client.reserve_operation_id();
      EXPECT_GT(request.operation, reserved_before_recovery);
      request.action = static_cast<std::uint32_t>(managed::edit_lock_action::renew);
      ASSERT_EQ(authority.change_lock(request, session, 0)->status,
                managed::collaboration_status::accepted);
      // Recovery retains queued request grants; new edits need freshly supplied host lease state.
      client.set_grants({{request.target, request.generation}});
      recovered.undo();
      recovered.synchronize();
      EXPECT_EQ(authority.read()->name, "Original");
      EXPECT_EQ(client.pending_count(), 0u);
      EXPECT_GT(authority.accepted_since(0).back()->operation, request.operation);
      recovered.redo();
      recovered.synchronize();
      EXPECT_EQ(authority.read()->name, "With lease");
      EXPECT_EQ(client.pending_count(), 0u);
    }
    std::filesystem::remove_all(directory);
  }
}

// Both existing journal layouts recover edits, undo and uncertain sends as one model/session state.
TEST(local_collaboration, durable_queue_and_lost_reply_recovery) {
  for (const auto mode :
       {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    authority_type authority{ledger{"Original"}, 1};
    authority.open_session(10);
    unreliable_transport transport{authority, 10};
    const auto directory = std::filesystem::current_path() /
                           ("local-client-" + std::to_string(managed::make_document_id().high));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    {
      store_type store{ledger{}};
      store.collaborate(std::uint64_t{10}).bind(transport);
      store.synchronize();
      store.create_journal(directory / "client", mode);
      store.execute_transaction([](auto& edit) { edit.root().entries().insert(100, {"Saved", 5}); })
          .throw_if_failed();
      transport.lose_reply = true;
      EXPECT_THROW(store.synchronize(), std::runtime_error);
    }
    {
      store_type recovered{ledger{}};
      recovered.collaborate(std::uint64_t{10}).bind(transport);
      recovered.recover_journal(directory / "client");
      EXPECT_EQ(recovered.read()->entries.at(100).memo, "Saved");
      recovered.synchronize();
      EXPECT_EQ(authority.sequence(), 1u);
      recovered.undo();
      recovered.redo();
      recovered.synchronize();
      EXPECT_EQ(authority.sequence(), 3u);
      recovered.save_journal();
    }
    {
      store_type recovered{ledger{}};
      recovered.collaborate(std::uint64_t{10}).bind(transport);
      recovered.recover_journal(directory / "client");
      EXPECT_EQ(recovered.read()->entries.at(100).memo, "Saved");
      EXPECT_TRUE(recovered.collaboration().state()->pending.empty());
    }
    std::filesystem::remove_all(directory);
  }
}

// A fresh authority epoch reports uncertainty rather than silently replaying a possibly accepted edit.
TEST_F(local_collaboration_test, fresh_epoch_preserves_uncertain_work) {
  unreliable_transport transport{authority, 10};
  alice.collaboration().bind(transport);
  alice.execute_transaction([](auto& edit) { edit.root().set_name("Maybe"); }).throw_if_failed();
  transport.lose_reply = true;
  EXPECT_THROW(alice.synchronize(), std::runtime_error);
  authority_type restarted{ledger{}, 2};
  restarted.load_document(authority.save_document());
  restarted.open_session(10);
  managed::collaboration_transport replacement{restarted, std::uint64_t{10}};
  alice.collaboration().bind(replacement);
  alice.synchronize();
  EXPECT_EQ(alice.read()->name, "Maybe");
  EXPECT_FALSE(alice.collaboration().state()->in_flight.empty());
  EXPECT_EQ(alice.collaboration().state()->transactions.front().status,
            static_cast<std::uint32_t>(pending_status::uncertain));
  EXPECT_EQ(restarted.sequence(), 0u);
  alice.collaboration().discard_pending();
  EXPECT_EQ(alice.read()->name, "Maybe");
}

// String sessions use the same store association, persisted identity and synchronization path.
TEST(local_collaboration, string_session_store) {
  using sessions = managed::collaboration_session<std::string>;
  sessions::authority<ledger> authority{ledger{"Original"}, 1};
  authority.open_session("client");
  managed::collaboration_transport transport{authority, std::string{"client"}};
  store_type store{ledger{}};
  store.collaborate(std::string{"client"}).bind(transport);
  store.synchronize();
  store.execute_transaction([](auto& edit) { edit.root().set_name("Typed"); }).throw_if_failed();
  store.synchronize();
  EXPECT_EQ(store.collaboration<std::string>().session(), "client");
  EXPECT_EQ(authority.read()->name, "Typed");
}

// Durable queue failures are atomic before writes and fence the same store after uncertain writes.
TEST(local_collaboration, journal_publication_failure_boundaries) {
  for (const auto boundary :
       {managed::journal_io_event::before_append, managed::journal_io_event::after_flush}) {
    authority_type authority{ledger{"Original"}, 1};
    authority.open_session(10);
    managed::collaboration_transport transport{authority, std::uint64_t{10}};
    const auto directory = std::filesystem::current_path() /
                           ("local-fault-" + std::to_string(managed::make_document_id().high));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    bool fail = false;
    managed::journal_options options;
    options.fault_injector = [&](auto event) {
      if (fail && event == boundary) {
        throw std::runtime_error{"injected"};
      }
    };
    {
      store_type store{ledger{}};
      store.collaborate(std::uint64_t{10}).bind(transport);
      store.synchronize();
      store.create_journal(directory / "client", managed::journal_storage_mode::appended, options);
      fail = true;
      const auto outcome =
          store.execute_transaction([](auto& edit) { edit.root().set_name("Durable"); });
      EXPECT_EQ(store.read()->name, "Original");
      EXPECT_EQ(store.collaboration().pending_count(), 0u);
      if (boundary == managed::journal_io_event::before_append) {
        EXPECT_EQ(outcome.status, managed::transaction_status::failed);
      } else {
        EXPECT_EQ(outcome.status, managed::transaction_status::indeterminate);
        EXPECT_THROW(store.synchronize(), managed::journal_indeterminate_error);
      }
    }
    {
      store_type recovered{ledger{}};
      recovered.collaborate(std::uint64_t{10}).bind(transport);
      recovered.recover_journal(directory / "client");
      const bool committed = boundary == managed::journal_io_event::after_flush;
      EXPECT_EQ(recovered.read()->name, committed ? "Durable" : "Original");
      EXPECT_EQ(recovered.collaboration().pending_count(), committed ? 1u : 0u);
      recovered.synchronize();
      EXPECT_EQ(authority.read()->name, committed ? "Durable" : "Original");
    }
    std::filesystem::remove_all(directory);
  }
}

// If storing an acknowledgement fails, the durable in-flight request remains exactly retryable.
TEST(local_collaboration, acknowledgement_persistence_failure) {
  authority_type authority{ledger{"Original"}, 1};
  authority.open_session(10);
  unreliable_transport transport{authority, 10};
  const auto directory = std::filesystem::current_path() /
                         ("local-ack-" + std::to_string(managed::make_document_id().high));
  ASSERT_TRUE(std::filesystem::create_directory(directory));
  bool fail = false;
  managed::journal_options options;
  options.fault_injector = [&](auto event) {
    if (fail && event == managed::journal_io_event::before_append) {
      throw std::runtime_error{"injected"};
    }
  };
  {
    store_type store{ledger{}};
    store.collaborate(std::uint64_t{10}).bind(transport);
    store.synchronize();
    store.create_journal(directory / "client", managed::journal_storage_mode::appended, options);
    store.execute_transaction([](auto& edit) { edit.root().set_name("Once"); }).throw_if_failed();
    transport.after_submit = [&] { fail = true; };
    EXPECT_THROW(store.synchronize(), std::runtime_error);
    EXPECT_FALSE(store.collaboration().state()->in_flight.empty());
    EXPECT_EQ(authority.sequence(), 1u);
  }
  transport.after_submit = {};
  {
    store_type recovered{ledger{}};
    recovered.collaborate(std::uint64_t{10}).bind(transport);
    recovered.recover_journal(directory / "client");
    recovered.synchronize();
    EXPECT_EQ(authority.sequence(), 1u);
    EXPECT_TRUE(recovered.collaboration().state()->pending.empty());
  }
  std::filesystem::remove_all(directory);
}

// Session APIs obey the existing store's transaction, callback and owner-thread boundaries.
TEST_F(local_collaboration_test, reentry_and_thread_confinement) {
  alice
      .execute_transaction([&](auto& edit) {
        EXPECT_THROW(alice.synchronize(), std::logic_error);
        edit.root().set_name("Fine");
      })
      .throw_if_failed();
  bool rejected = false;
  std::thread worker{[&] {
    try {
      alice.synchronize();
    } catch (const std::logic_error&) {
      rejected = true;
    }
  }};
  worker.join();
  EXPECT_TRUE(rejected);
  unreliable_transport transport{authority, 10};
  transport.after_submit = [&] {
    EXPECT_THROW(alice.synchronize(), std::logic_error);
    EXPECT_EQ(
        alice.execute_transaction([](auto& edit) { edit.root().set_name("Reentered"); }).status,
        managed::transaction_status::failed);
  };
  alice.collaboration().bind(transport);
  alice.synchronize();
  EXPECT_EQ(alice.read()->name, "Fine");
}

// A bounded outbox rejects the whole local commit; corrupt checkpoint state never replaces live data.
TEST(local_collaboration, resource_limits_and_malformed_checkpoint) {
  authority_type authority{ledger{"Original"}, 1};
  authority.open_session(10);
  managed::collaboration_transport transport{authority, std::uint64_t{10}};
  store_type store{ledger{}};
  managed::collaboration_client_options options;
  options.max_transactions = 1;
  store.collaborate(std::uint64_t{10}, options).bind(transport);
  store.synchronize();
  store.execute_transaction([](auto& edit) { edit.root().set_name("One"); }).throw_if_failed();
  const auto before = store.save();
  EXPECT_EQ(store.execute_transaction([](auto& edit) { edit.root().set_name("Two"); }).status,
            managed::transaction_status::failed);
  EXPECT_EQ(store.save(), before);
  auto malformed =
      managed::decode_collaboration_record<managed::collaboration::client_checkpoint>(before);
  malformed.state.pending.push_back(999);
  store_type fresh{ledger{}};
  fresh.collaborate(std::uint64_t{10});
  EXPECT_THROW(fresh.load(managed::encode_collaboration_record(malformed)), std::invalid_argument);
  EXPECT_THROW(store.load(before), std::logic_error);
  EXPECT_EQ(store.save(), before);
}

// Remote baselines advance native tree revision watermarks while local undo remains a transaction.
TEST(local_collaboration, tree_history_checkpoint_recovery) {
  using tree_store = managed::model_store<ledger, managed::history_mode::tree>;
  authority_type authority{ledger{"Original"}, 1};
  authority.open_session(10);
  managed::collaboration_transport transport{authority, std::uint64_t{10}};
  const auto directory = std::filesystem::current_path() /
                         ("local-tree-" + std::to_string(managed::make_document_id().high));
  ASSERT_TRUE(std::filesystem::create_directory(directory));
  {
    tree_store store{ledger{}};
    store.collaborate(std::uint64_t{10}).bind(transport);
    store.synchronize();
    store.create_journal(directory / "client");
    store.execute_transaction([](auto& edit) { edit.root().set_name("Tree"); }).throw_if_failed();
    store.synchronize();
    store.undo();
    store.synchronize();
    store.save_journal();
    EXPECT_THROW(store.reset_history(), std::logic_error);
  }
  {
    tree_store store{ledger{}};
    store.collaborate(std::uint64_t{10}).bind(transport);
    store.recover_journal(directory / "client");
    EXPECT_EQ(store.read()->name, "Original");
    store.collaboration().undo(true);
    store.synchronize();
    EXPECT_EQ(authority.read()->name, "Tree");
  }
  std::filesystem::remove_all(directory);
}

// Encoding budgets must never admit local client state that its configured recovery decoder rejects.
TEST(local_collaboration, checkpoint_decoder_budget_is_checked_before_publication) {
  authority_type authority{ledger{"Original"}, 1};
  authority.open_session(10);
  managed::collaboration_transport transport{authority, std::uint64_t{10}};
  managed::store_options options;
  options.decode.max_string_bytes = 256;
  managed::model_store<ledger, managed::history_mode::linear, managed::history_labels::enabled>
      store{ledger{}, managed::make_document_id(), options};
  store.collaborate(std::uint64_t{10}).bind(transport);
  store.synchronize();
  const auto outcome = store.execute_transaction(
      std::string(257, 'x'), [](auto& edit) { edit.root().set_name("Too much metadata"); });
  EXPECT_EQ(outcome.status, managed::transaction_status::failed);
  EXPECT_EQ(store.read()->name, "Original");
  EXPECT_EQ(store.collaboration().pending_count(), 0u);
}
