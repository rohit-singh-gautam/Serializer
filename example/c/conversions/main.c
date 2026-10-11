#include <stdlib.h>
#include <string.h>
static size_t remaining_allocations = (size_t)-1;
/* Inject allocation failure without changing the generated runtime's ownership contract. */
static void* conversion_malloc(size_t size) {
  if (remaining_allocations == 0) { return NULL; }
  if (remaining_allocations != (size_t)-1) { --remaining_allocations; }
  return malloc(size);
}
#define malloc conversion_malloc
#include "schema.h"
#undef malloc
/* Prove deep-copy publication, failure rollback, defaults and unchanged borrowed source ownership. */
int main(void) {
  shipment_ticket source = {0}; delivery_notice result = {0}; postal_address address = {0}; short_ticket ticket = {0};
  if (shipment_ticket_init(&source) != srl_ok || delivery_notice_init(&result) != srl_ok ||
      postal_address_init(&address) != srl_ok || short_ticket_init(&ticket) != srl_ok) { return 1; }
  source.number = UINT64_MAX; source.contact.delivery_count = 19; source.region = routing_mode_express; source.base0.priority = 7;
  if (shipment_ticket_to_notice(&result, &source) != srl_ok || result.number != UINT64_MAX ||
      result.destination.delivery_count != 19 || !result.preview || result.mode != routing_mode_express || result.priority != 7) { return 2; }
  if (result.destination.street.data == source.contact.street.data || srl_string_set_cstr(&result.destination.street, "Changed") != srl_ok ||
      strcmp(source.contact.street.data, "Harbor Road") != 0) { return 3; }
  char* retained = result.destination.street.data;
  for (size_t budget = 0; budget < 2; ++budget) {
    remaining_allocations = budget;
    if (shipment_ticket_to_notice(&result, &source) != srl_allocation || result.destination.street.data != retained ||
        strcmp(source.contact.street.data, "Harbor Road") != 0 || result.number != UINT64_MAX) { return 4; }
  }
  remaining_allocations = (size_t)-1;
  if (shipment_ticket_to_address(&address, &source) != srl_ok || address.postcode != 410 || address.delivery_count != 12) { return 5; }
  ticket.value = 17;
  short_ticket_to_view_result converted = {0};
  if (short_ticket_to_view(&converted, &ticket) != srl_ok || converted.value != 17 || converted.revision != 3) { return 7; }
  if (shipment_ticket_to_notice(NULL, &source) != srl_invalid || shipment_ticket_to_notice(&result, NULL) != srl_invalid) { return 6; }
  shipment_ticket_free(&source); delivery_notice_free(&result); postal_address_free(&address); short_ticket_free(&ticket);
  short_ticket_to_view_result_free(&converted);
  return 0;
}
