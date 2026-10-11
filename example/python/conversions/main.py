from schema import ShipmentTicket, ShortTicket, RoutingMode
source = ShipmentTicket(); source.number = (1 << 64) - 1
source.contact.delivery_count = 19; source.region = RoutingMode.Express; source.base0.priority = 7
result = source.to_notice()
assert result.number == source.number and result.destination.delivery_count == 19 and result.preview
assert result.mode == RoutingMode.Express and result.priority == 7
result.destination.street = "Changed"
assert source.contact.street == "Harbor Road"
address = source.to_address()
assert address.postcode == 410 and address.delivery_count == 12
ticket = ShortTicket(); ticket.value = 17
assert ticket.to_view().value == 17 and ticket.to_view().revision == 3
