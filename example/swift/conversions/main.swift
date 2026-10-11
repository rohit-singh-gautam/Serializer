import Foundation
var source = ShipmentTicket(); source.number = UInt64.max
source.contact.deliveryCount = 19; source.region = .Express; source.base0.priority = 7
var result = source.toNotice()
precondition(result.number == UInt64.max && result.destination.deliveryCount == 19 && result.preview)
precondition(result.mode == .Express && result.priority == 7)
result.destination.street = "Changed"
precondition(source.contact.street == "Harbor Road")
let address = source.toAddress(); precondition(address.postcode == 410 && address.deliveryCount == 12)
var ticket = ShortTicket(); ticket.value = 17
precondition(ticket.toView().value == 17 && ticket.toView().revision == 3)
