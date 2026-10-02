#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>

// Edit through model_store while host coordination manages presence and fenced subtree leases.
int main() {
  namespace managed = rohit::managed;
  using ledger = ledger_example::ledger;
  constexpr std::uint64_t holder_session = 10;
  constexpr std::uint64_t competing_session = 20;
  constexpr std::uint64_t lease_duration_ms = 1000;

  // 1. Attach both sessions to independent stores and load the authority's starting document.
  managed::collaboration_authority<ledger> authority{
      ledger{"Shared ledger", {{100, {"Supplies", 1500}}}}, 1};
  authority.open_session(holder_session);
  authority.open_session(competing_session);
  managed::collaboration_transport holder_connection{authority, holder_session};
  managed::collaboration_transport competing_connection{authority, competing_session};
  managed::model_store<ledger> holder{ledger{}}, competing{ledger{}};
  holder.collaborate(holder_session).bind(holder_connection);
  competing.collaborate(competing_session).bind(competing_connection);
  holder.synchronize();
  competing.synchronize();
  const auto holder_entry = holder.read()->entries.at(100).persistent_id;
  const auto competing_entry = competing.read()->entries.at(100).persistent_id;
  const auto root_target = holder.collaboration().remote_id(holder.read()->persistent_id);
  const auto entry_target = holder.collaboration().remote_id(holder_entry);

  // 2. Presence and lease requests are host coordination, separate from document transactions.
  // They use authority IDs; the cache is for displaying locks, not granting edit permission.
  managed::collaboration_lock_cache cache;
  cache.synchronize(authority.lock_snapshot(), authority.context());
  authority.receive_presence(
      {authority.context(), holder_session, 1, 1, entry_target, 1, "Editing amount"},
      holder_session, 0);
  // Lock and edit requests share one operation namespace. Reserve through the store session so
  // its next document submission cannot reuse this ID. Keep the same request for any exact retry.
  managed::collaboration::lock_request request{
      authority.context(), holder_session, holder.collaboration().reserve_operation_id(),
      static_cast<std::uint32_t>(managed::edit_lock_action::acquire), root_target,
      static_cast<std::uint32_t>(managed::edit_lock_scope::owned_subtree), 0, lease_duration_ms};
  const auto acquired = authority.change_lock(request, holder_session, 0);
  if (acquired->status != managed::collaboration_status::accepted) {
    throw std::runtime_error{"Lock acquisition failed"};
  }
  for (const auto& event : authority.locks_since(0)) {
    cache.receive_lock_update(*event, authority.context());
  }
  // 3. The competing store can commit locally, but synchronization cannot publish through the
  // other session's subtree lease. The rejected local amount remains available for inspection.
  competing.execute_transaction([competing_entry](auto& edit) {
             edit.root().entries().edit(competing_entry).set_amount_minor_units(2000);
           })
      .throw_if_failed();
  competing.synchronize();
  if (competing.collaboration().pending_status() != managed::pending_change_status::conflict ||
      competing.read()->entries.at(100).amount_minor_units != 2000 ||
      authority.read()->entries.at(100).amount_minor_units != 1500) {
    throw std::runtime_error{"Competing edit bypassed subtree lock"};
  }
  // 4. Supply the holder's grant to its store session, then edit through a normal transaction.
  holder.collaboration().set_grants({{root_target, acquired->grant.generation}});
  holder.execute_transaction([holder_entry](auto& edit) {
          edit.root().entries().edit(holder_entry).set_amount_minor_units(1800);
        })
      .throw_if_failed();
  holder_connection.set_time(1);
  holder.synchronize();
  if (holder.collaboration().pending_count() != 0 ||
      authority.read()->entries.at(100).amount_minor_units != 1800) {
    throw std::runtime_error{"Granted edit failed"};
  }
  // 5. Expire the lease on the host clock; renewing its old generation must fail.
  authority.advance_expiry(lease_duration_ms);
  request.operation = holder.collaboration().reserve_operation_id();
  request.action = static_cast<std::uint32_t>(managed::edit_lock_action::renew);
  request.generation = acquired->grant.generation;
  if (authority.change_lock(request, holder_session, lease_duration_ms)->status !=
      managed::collaboration_status::obsolete_grant) {
    throw std::runtime_error{"Expired grant was revived"};
  }
  cache.synchronize(authority.lock_snapshot(), authority.context());
  authority.close_session(holder_session);
  if (!cache.complete() || !cache.grants().empty() || !authority.editing_presence().empty()) {
    throw std::runtime_error{"Coordination cleanup failed"};
  }
  std::cout << "Subtree lock blocked competing edit; holder changed amount to 1800\n"
            << "Expiry fenced old grant; snapshot refreshed cache; session end cleared presence\n";
}
