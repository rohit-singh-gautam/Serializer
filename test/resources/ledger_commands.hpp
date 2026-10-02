#pragma once

#include <collaboration_commands.hpp>
#include <ledger.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace collaboration_example {

enum class ledger_action : std::uint32_t { rename = 1, insert = 2, adjust = 3, erase = 4 };
using authority_type = rohit::managed::collaboration_authority<ledger_example::ledger>;

// Decode one exact application command, then use generated editors inside the authority transaction.
inline void apply_command(authority_type::edit_type& transaction,
                          std::span<const std::uint8_t> bytes) {
  const auto command = rohit::managed::decode_collaboration_record<ledger_command>(bytes);
  switch (static_cast<ledger_action>(command.action)) {
  case ledger_action::rename:
    transaction.root().set_name(command.memo);
    break;
  case ledger_action::insert:
    transaction.root().entries().insert(command.key, {command.memo, command.amount_minor_units});
    break;
  case ledger_action::adjust:
    transaction.root()
        .entries()
        .edit(command.entry_id)
        .set_amount_minor_units(command.amount_minor_units);
    break;
  case ledger_action::erase:
    transaction.root().entries().erase(command.entry_id);
    break;
  default:
    throw std::invalid_argument{"Unknown ledger collaboration command"};
  }
}

// Build an application proposal with a caller-owned, stable operation ID and acknowledged base.
inline rohit::managed::collaboration::change_proposal make_proposal(const authority_type& authority,
                                                                    std::uint64_t session,
                                                                    std::uint64_t operation,
                                                                    const ledger_command& command) {
  return {authority.context(),
          session,
          operation,
          authority.sequence(),
          rohit::managed::encode_collaboration_record(command),
          {}};
}

} // namespace collaboration_example
