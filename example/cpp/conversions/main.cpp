#include "message.hpp"
#include <cstdint>
#include <type_traits>
// Owner-bound generic conversions remain explicit and support eligible constant evaluation.
constexpr auto converted_ticket = [] { short_ticket value{}; value.value = 17; return value.to_view(); }();
static_assert(converted_ticket.value == 17 && converted_ticket.revision == 3);
static_assert(!std::is_convertible_v<shipment_ticket, postal_address>);
// Copy nested values independently, preserving mapped transients and fresh target defaults.
int main() {
  shipment_ticket source{}; source.number = UINT64_MAX; source.contact.delivery_count = 19;
  source.region = routing_mode::express; source.priority = 7;
  auto result = source.to_notice();
  if (result.number != UINT64_MAX || result.destination.delivery_count != 19 || !result.preview ||
      result.mode != routing_mode::express || result.priority != 7) { return 1; }
  result.destination.street = "Changed";
  if (source.contact.street != "Harbor Road") { return 2; }
  auto address = static_cast<postal_address>(source);
  if (address.postcode != 410 || address.delivery_count != 12) { return 3; }
  return 0;
}
