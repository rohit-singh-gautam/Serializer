package main

func main() {
  source := NewShipmentTicket(); source.Number = ^uint64(0)
  source.Contact.DeliveryCount = 19; source.Region = RoutingModeExpress; source.Base0.Priority = 7
  result := source.ToNotice()
  if result.Number != source.Number || result.Destination.DeliveryCount != 19 || !result.Preview ||
      result.Mode != RoutingModeExpress || result.Priority != 7 { panic("Conversion") }
  result.Destination.Street = "Changed"
  if source.Contact.Street != "Harbor Road" { panic("Aliased record") }
  address := source.ToAddress()
  if address.Postcode != 410 || address.DeliveryCount != 12 { panic("Defaults") }
  ticket := NewShortTicket(); ticket.Value = 17
  if ticket.ToView().Value != 17 || ticket.ToView().Revision != 3 { panic("Generic conversion") }
}
