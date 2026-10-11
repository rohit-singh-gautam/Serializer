public final class Main {
  public static void main(String[] args) {
    var source = new Schema.ShipmentTicket(); source.number = -1L;
    source.contact.deliveryCount = 19; source.region = Schema.RoutingMode.EXPRESS; source.base0.priority = 7;
    var result = source.toNotice();
    if (result.number != -1L || result.destination.deliveryCount != 19 || !result.preview ||
        result.mode != Schema.RoutingMode.EXPRESS || result.priority != 7) { throw new AssertionError(); }
    result.destination.street = "Changed";
    if (!source.contact.street.equals("Harbor Road")) { throw new AssertionError("Aliased record"); }
    var address = source.toAddress();
    if (address.postcode != 410 || address.deliveryCount != 12) { throw new AssertionError(); }
    var ticket = new Schema.ShortTicket(); ticket.value = 17;
    if (ticket.toView().value != 17 || ticket.toView().revision != 3) { throw new AssertionError(); }
  }
}
