fun main() {
  val source = ShipmentTicket(); source.number = ULong.MAX_VALUE
  source.contact.deliveryCount = 19u; source.region = RoutingMode.Express; source.base0.priority = 7u
  val result = source.toNotice()
  check(result.number == ULong.MAX_VALUE && result.destination.deliveryCount == 19u && result.preview)
  check(result.mode == RoutingMode.Express && result.priority == 7u.toUByte())
  result.destination.street = "Changed"
  check(source.contact.street == "Harbor Road")
  val address = source.toAddress(); check(address.postcode == 410u && address.deliveryCount == 12u)
  val ticket = ShortTicket(); ticket.value = 17u
  check(ticket.toView().value == 17u.toUShort() && ticket.toView().revision == 3u)
}
