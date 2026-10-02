#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>

// Demonstrate store-local edits, explicit conflict resolution and authority-enforced read-only access.
int main() {
  namespace managed = rohit::managed;
  using ledger = ledger_example::ledger;
  // Local/remote are client perspectives, not properties encoded by a session number.
  constexpr std::uint64_t local_session = 10;
  constexpr std::uint64_t remote_session = 20;
  constexpr std::uint64_t viewer_session = 30;

  // 1. Register sessions, attach separate stores and fetch the shared starting document.
  managed::collaboration_authority<ledger> authority{ledger{"Shared ledger"}, 1};
  authority.open_session(local_session);
  authority.open_session(remote_session);
  authority.open_session(viewer_session, false);
  managed::collaboration_transport local_connection{authority, local_session};
  managed::collaboration_transport remote_connection{authority, remote_session};
  managed::collaboration_transport viewer_connection{authority, viewer_session};
  managed::model_store<ledger> local{ledger{}}, remote{ledger{}}, viewer{ledger{}};
  local.collaborate(local_session).bind(local_connection);
  remote.collaborate(remote_session).bind(remote_connection);
  viewer.collaborate(viewer_session).bind(viewer_connection);
  for (auto* store : {&local, &remote, &viewer}) {
    store->synchronize();
  }

  // 2. Both stores edit locally before either synchronizes. Their competing name changes conflict;
  // disjoint field edits would merge instead. The insertion belongs to the same local transaction.
  local.execute_transaction([](auto& edit) {
         edit.root().entries().insert(100, {"Supplies", 1500});
         edit.root().set_name("Local ledger");
       })
      .throw_if_failed();
  remote.execute_transaction([](auto& edit) { edit.root().set_name("Renamed ledger"); })
      .throw_if_failed();
  local.synchronize();
  const auto entry_id = local.read()->entries.at(100).persistent_id;
  // Polling again with an empty queue does not resubmit the insertion or allocate another ID.
  local.synchronize();
  if (authority.sequence() != 1 || local.read()->entries.at(100).persistent_id != entry_id) {
    throw std::runtime_error{"Repeated synchronization changed the accepted transaction"};
  }
  remote.synchronize();
  if (remote.collaboration().pending_status() != managed::pending_change_status::conflict ||
      remote.read()->name != "Renamed ledger" ||
      remote.collaboration().acknowledged_read()->name != "Local ledger") {
    throw std::runtime_error{"Conflicting local edit was not retained"};
  }

  // 3. Explicitly resolve the conflict: archive the pending draft, adopt accepted state, then
  // reapply the chosen name through a new model_store transaction. This preserves the insertion.
  const auto retained = remote.collaboration().discard_pending();
  remote.execute_transaction([&](auto& edit) { edit.root().set_name(retained->name); })
      .throw_if_failed();
  remote.synchronize();
  for (auto* store : {&local, &remote, &viewer}) {
    store->synchronize();
    if (store->read()->name != "Renamed ledger" ||
        store->read()->entries.at(100).amount_minor_units != 1500 ||
        store->collaboration().pending_count() != 0) {
      throw std::runtime_error{"Resolved document did not reach every store"};
    }
  }

  // 4. Read-only permission is enforced by the authority. A client can still edit its local store;
  // synchronization denies publication and keeps that local draft separately from accepted state.
  viewer.execute_transaction([](auto& edit) { edit.root().set_name("Forbidden"); })
      .throw_if_failed();
  viewer.synchronize();
  if (viewer.collaboration().pending_status() != managed::pending_change_status::denied ||
      viewer.read()->name != "Forbidden" ||
      viewer.collaboration().acknowledged_read()->name != "Renamed ledger" ||
      authority.read()->name != "Renamed ledger" || authority.sequence() != 2) {
    throw std::runtime_error{"Read-only synchronization failed"};
  }
  // The example explicitly discards the denied draft so the viewer displays accepted state again.
  static_cast<void>(viewer.collaboration().discard_pending());
  std::cout << "Local session: " << local_session << ", remote session: " << remote_session
            << ", read-only viewer: " << viewer_session << '\n'
            << "Accepted sequence: " << authority.sequence() << '\n'
            << "Store: " << viewer.read()->name
            << ", Supplies = " << viewer.read()->entries.at(100).amount_minor_units << '\n'
            << "Repeated sync preserved IDs; same-field conflict resolved; read-only edit denied\n";
}
