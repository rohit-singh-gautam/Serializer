package main

import "serializer.example/go/conversions/internal/model"

// main checks explicit record conversions, defaults, and independent nested ownership.
func main() {
	source := model.NewShipmentTicket()
	source.Number = ^uint64(0)
	source.Contact.DeliveryCount = 19
	source.Region = model.RoutingModeExpress
	source.Base0.Priority = 7
	result := source.ToNotice()
	if result.Number != source.Number || result.Destination.DeliveryCount != 19 || !result.Preview ||
		result.Mode != model.RoutingModeExpress || result.Priority != 7 {
		panic("Conversion")
	}
	result.Destination.Street = "Changed"
	if source.Contact.Street != "Harbor Road" {
		panic("Aliased record")
	}
	address := source.ToAddress()
	if address.Postcode != 410 || address.DeliveryCount != 12 {
		panic("Defaults")
	}
	ticket := model.NewShortTicket()
	ticket.Value = 17
	if ticket.ToView().Value != 17 || ticket.ToView().Revision != 3 {
		panic("Generic conversion")
	}
}
