#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// Ordinary model_store transactions edit locally; the store separately synchronizes its durable outbox.
int main() {
  namespace managed = rohit::managed;
  using ledger = ledger_example::ledger;
  constexpr std::uint64_t alice_session = 10;
  constexpr std::uint64_t bob_session = 20;
  constexpr std::uint64_t sync_interval_ms = 5000;

  // 1. Initialize the shared authority (the server role) in epoch 1 with one ledger entry.
  // Both clients start from this document: name = "Expenses", entry 100 amount = 1500.
  managed::collaboration_authority<ledger> authority{
      ledger{"Expenses", {{100, {"Supplies", 1500}}}}, 1};
  // Register both writable sessions on the authority before attaching any client stores.
  authority.open_session(alice_session);
  authority.open_session(bob_session);
  // Each in-process connection binds one trusted session to the same authority.
  // The authority and connections outlive the stores that use them; no network service is started.
  managed::collaboration_transport alice_connection{authority, alice_session};
  managed::collaboration_transport bob_connection{authority, bob_session};
  const auto directory =
      std::filesystem::current_path() /
      ("serializer-local-sync-" + std::to_string(managed::make_document_id().high));
  std::filesystem::create_directory(directory);
  {
    // 2. Create independent client stores with empty placeholder documents.
    managed::model_store<ledger> alice{ledger{}}, bob{ledger{}};
    // These are the initial client-side attachments: each store owns its collaboration session,
    // and bind() borrows the matching connection. Attachment alone does not fetch shared data.
    alice.collaborate(alice_session).bind(alice_connection);
    bob.collaborate(bob_session).bind(bob_connection);

    // 3. First synchronization for each client: join the authority's baseline, replacing the
    // placeholder. Both stores now contain "Expenses" and amount 1500; editing can begin.
    alice.synchronize();
    bob.synchronize();

    // Journal Alice's joined state and subsequent commits, including her pending outgoing work.
    // Bob has no journal in this example, so his client state is kept only in memory.
    alice.create_journal(directory / "alice", managed::journal_storage_mode::sidecar);
    alice.collaboration().set_sync_interval(sync_interval_ms);
    // The first timer tick only establishes time zero; it does not synchronize again.
    static_cast<void>(alice.synchronize_if_due(0));

    // 4. Update Alice's local data through a normal transaction. Commit immediately publishes
    // and journals the rename, then queues it for sending. The authority and Bob still see
    // "Expenses" because Alice has not synchronized this edit yet.
    alice.execute_transaction([](auto& edit) { edit.root().set_name("Office expenses"); })
        .throw_if_failed();
    std::cout << "Local: " << alice.read()->name << "; server: " << authority.read()->name << '\n';

    // Independently update Bob's local amount. Map key 100 locates the entry; its persistent ID
    // identifies the managed entity passed to edit(). This commit also queues an outgoing change.
    const auto entry = bob.read()->entries.at(100).persistent_id;
    bob.execute_transaction(
           [entry](auto& edit) { edit.root().entries().edit(entry).set_amount_minor_units(2000); })
        .throw_if_failed();
    // 5. Bob's second synchronization sends his queued amount edit to the authority and receives
    // its acceptance. Bob and the authority now have amount 2000; Alice still has amount 1500.
    bob.synchronize();

    // 6. Alice's second synchronization runs when the timer reaches 5000 ms. It receives Bob's
    // amount change, merges it with her pending rename (different fields), sends that rename,
    // and receives its acceptance. Alice and the authority now have both edits.
    // Bob still sees "Expenses" until he synchronizes again; this example does not poll him again.
    // A real owner-thread event loop supplies monotonic ticks; no background timer is started.
    static_cast<void>(alice.synchronize_if_due(sync_interval_ms));
    std::cout << "Synchronized: " << alice.read()->name
              << ", amount = " << alice.read()->entries.at(100).amount_minor_units << '\n';

    // 7. Undo only Alice's rename locally, preserving Bob's amount 2000. The inverse is journaled
    // and queued like an ordinary edit; explicit synchronization sends it without waiting for a tick.
    alice.undo();
    std::cout << "Local undo: " << alice.read()->name << '\n';
    alice.synchronize();
    // Save a full client checkpoint, including collaboration state and undo/redo history.
    alice.save_journal();
  }
  {
    // 8. Simulate restarting Alice's client while the same authority and connection stay alive.
    // Attach the same session to a fresh store, then recover BEFORE any initial synchronization:
    // the journal supplies her document, collaboration state and history instead of a fresh join.
    managed::model_store<ledger> alice{ledger{}};
    alice.collaborate(alice_session).bind(alice_connection);
    alice.recover_journal(directory / "alice");
    // Redo the recovered rename locally, then explicitly send it. Bob's accepted amount survives.
    alice.redo();
    alice.synchronize();
    if (authority.read()->name != "Office expenses" ||
        authority.read()->entries.at(100).amount_minor_units != 2000) {
      throw std::runtime_error{"Local synchronization or recovered undo history failed"};
    }
    std::cout << "Recovered redo: " << alice.read()->name << '\n';
  }
  // Both client scopes have ended, so their journal handles are closed before deleting demo files.
  std::filesystem::remove_all(directory);
}
