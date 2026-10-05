#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// Use model_store transactions, undo/redo and journals for two independently synchronized clients.
int main() {
  namespace managed = rohit::managed;
  using ledger = ledger_example::ledger;
  constexpr std::uint64_t alice_session = 10;
  constexpr std::uint64_t bob_session = 20;

  // 1. Initialize the shared document and register two writable sessions on the authority.
  managed::collaboration_authority<ledger> authority{
      ledger{"Expenses", {{100, {"Supplies", 1500}}}}, 1};
  authority.open_session(alice_session);
  authority.open_session(bob_session);
  managed::collaboration_transport alice_connection{authority, alice_session};
  managed::collaboration_transport bob_connection{authority, bob_session};
  const auto directory =
      std::filesystem::current_path() /
      ("serializer-store-undo-" + std::to_string(managed::make_document_id().high));
  std::filesystem::create_directory(directory);
  {
    // 2. Attach each session to its own store and join before editing or creating its journal.
    managed::model_store<ledger> alice{ledger{}}, bob{ledger{}};
    alice.collaborate(alice_session).bind(alice_connection);
    bob.collaborate(bob_session).bind(bob_connection);
    alice.synchronize();
    bob.synchronize();

    // Use the ordinary model_store journal API for edits, undo, redo and synchronization state.
    // Each client has its own file; model changes and matching session/outbox state flush together.
    alice.create_journal(directory / "alice", managed::journal_storage_mode::sidecar);
    bob.create_journal(directory / "bob", managed::journal_storage_mode::sidecar);

    // Check both the local result and authority acceptance after each successful synchronization.
    const auto verify_alice = [&](const char* name, std::int64_t amount) {
      if (alice.read()->name != name || alice.read()->entries.at(100).amount_minor_units != amount ||
          authority.read()->name != name ||
          authority.read()->entries.at(100).amount_minor_units != amount ||
          alice.collaboration().pending_count() != 0) {
        throw std::runtime_error{"Unexpected synchronized ledger state"};
      }
    };

    // 3. Rename locally, then send the queued edit. Bob receives it on his next synchronization.
    alice.execute_transaction([](auto& edit) { edit.root().set_name("Office expenses"); })
        .throw_if_failed();
    alice.synchronize();
    bob.synchronize();

    // Bob edits a different field through his own store. Alice then receives his accepted amount.
    const auto entry_id = bob.read()->entries.at(100).persistent_id;
    bob.execute_transaction([entry_id](auto& edit) {
         edit.root().entries().edit(entry_id).set_amount_minor_units(2000);
       })
        .throw_if_failed();
    bob.synchronize();
    alice.synchronize();
    verify_alice("Office expenses", 2000);

    // 4. Undo Alice's rename through model_store. This journals and queues an inverse transaction
    // immediately; Bob's unrelated amount survives. Synchronization submits it for validation.
    alice.undo();
    alice.synchronize();
    verify_alice("Expenses", 2000);
    std::cout << "Undo: " << alice.read()->name
              << ", amount = " << alice.read()->entries.at(100).amount_minor_units << '\n';

    // Redo uses the same store transaction, journal and synchronization path as undo.
    alice.redo();
    alice.synchronize();
    verify_alice("Office expenses", 2000);
    std::cout << "Redo: " << alice.read()->name << '\n';

    // 5. Bob overwrites the name and sends it. Alice has not received this same-field edit yet,
    // so her next undo can still succeed locally and enter her durable outgoing queue.
    bob.synchronize();
    bob.execute_transaction([](auto& edit) { edit.root().set_name("Bob's name"); })
        .throw_if_failed();
    bob.synchronize();
    alice.undo();

    // Synchronization discovers the conflict and keeps Alice's local inverse for inspection.
    // Her acknowledged view exposes Bob's accepted name; his change is never overwritten.
    // If Alice had received Bob's edit before undo(), undo would instead throw immediately.
    alice.synchronize();
    if (alice.collaboration().pending_status() != managed::pending_change_status::conflict ||
        alice.collaboration().pending_count() != 1 || alice.read()->name != "Expenses" ||
        alice.read()->entries.at(100).amount_minor_units != 2000 ||
        alice.collaboration().acknowledged_read()->name != "Bob's name" ||
        authority.read()->name != "Bob's name" ||
        authority.read()->entries.at(100).amount_minor_units != 2000 || authority.sequence() != 5) {
      throw std::runtime_error{"Conflicting undo did not preserve local and accepted state"};
    }
    std::cout << "Conflicting undo kept local: " << alice.read()->name
              << "; server: " << authority.read()->name << '\n';
    std::cout << "Recorded transactions: " << authority.accepted_since(0).size() << '\n';

    // Full Save retains pending/conflicting work too; it does not send it or resolve a conflict.
    alice.save_journal();
    bob.save_journal();
  }
  // Store destruction closes both journals before removing this example's temporary directory.
  std::filesystem::remove_all(directory);
}
