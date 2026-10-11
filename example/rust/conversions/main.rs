mod schema;
use schema::*;
fn main() {
  let mut source = ShipmentTicket::default(); source.number = u64::MAX;
  source.contact.delivery_count = 19; source.region = RoutingMode::Express; source.base0.priority = 7;
  let mut result = source.to_notice();
  assert_eq!(result.number, u64::MAX); assert_eq!(result.destination.delivery_count, 19);
  assert!(result.preview); assert_eq!(result.mode, RoutingMode::Express); assert_eq!(result.priority, 7);
  result.destination.street = "Changed".into(); assert_eq!(source.contact.street, "Harbor Road");
  let address = source.to_address(); assert_eq!(address.postcode, 410); assert_eq!(address.delivery_count, 12);
  let mut ticket = ShortTicket::default(); ticket.value = 17;
  assert_eq!(ticket.to_view().value, 17); assert_eq!(ticket.to_view().revision, 3);
}
