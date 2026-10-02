#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace managed = rohit::managed;

// Use application-chosen IDs across a simulated transport without allocating numeric aliases.
template <typename Session>
void demonstrate_sessions(const Session& local, const Session& remote) {
  using sessions = managed::collaboration_session<Session>;
  using ledger = ledger_example::ledger;
  using proposal_type = typename sessions::records::change_proposal;
  using accepted_type = typename sessions::records::accepted_change;
  typename sessions::template authority<ledger> authority{ledger{"Shared"}, 1};
  authority.open_session(local);
  authority.open_session(remote);

  typename sessions::template replica<ledger> local_replica;
  typename sessions::template replica<ledger> remote_replica;
  const auto binding = authority.context();
  const auto baseline = managed::decode_collaboration_record<accepted_type>(
      managed::encode_collaboration_record(authority.snapshot()));
  local_replica.synchronize(baseline, binding);
  remote_replica.synchronize(baseline, binding);

  const auto proposal = local_replica.propose(
      local, 1, [](auto& edit) { edit.root().set_name("Application sessions"); });
  const auto request_bytes = managed::encode_collaboration_record(proposal);
  const auto request = managed::decode_collaboration_record<proposal_type>(request_bytes);
  // The host obtains this ID from its connection binding, independently of the decoded request.
  const Session trusted_connection_session = local;
  const auto result = authority.submit_change(request, trusted_connection_session, 0);
  if (result->status != managed::collaboration_status::accepted) {
    throw std::runtime_error{"Session proposal was not accepted"};
  }
  const auto accepted = managed::decode_collaboration_record<accepted_type>(
      managed::encode_collaboration_record(*result->accepted));
  local_replica.apply_accepted_change(accepted, binding);
  remote_replica.apply_accepted_change(accepted, binding);
  if (accepted.session != local || remote_replica.read()->name != "Application sessions") {
    throw std::runtime_error{"Application session identity was not preserved"};
  }
  std::cout << "Local session: " << local << ", remote session: " << remote
            << ", accepted origin: " << accepted.session << '\n';

  // The application decides when this participation ends; closed IDs cannot be reused in the epoch.
  authority.close_session(local);
  authority.close_session(remote);
}
} // namespace

// Demonstrate direct string and uint32 IDs; existing examples keep the uint64 default.
int main() {
  demonstrate_sessions(std::string{"desktop-session"}, std::string{"mobile-session"});
  demonstrate_sessions(std::uint32_t{100}, std::uint32_t{200});
}
