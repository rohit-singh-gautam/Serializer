using System;
using static SerializerGenerated.Schema;
internal static class Program {
  private static void Main() {
    var source = new ShipmentTicket(); source.Number = ulong.MaxValue;
    source.Contact.DeliveryCount = 19; source.Region = RoutingMode.Express; source.Base0.Priority = 7;
    var result = source.ToNotice();
    if (result.Number != source.Number || result.Destination.DeliveryCount != 19 || !result.Preview ||
        result.Mode != RoutingMode.Express || result.Priority != 7) { throw new Exception("Conversion"); }
    result.Destination.Street = "Changed";
    if (source.Contact.Street != "Harbor Road") { throw new Exception("Aliased record"); }
    var address = source.ToAddress();
    if (address.Postcode != 410 || address.DeliveryCount != 12) { throw new Exception("Defaults"); }
    var ticket = new ShortTicket(); ticket.Value = 17;
    if (ticket.ToView().Value != 17 || ticket.ToView().Revision != 3) { throw new Exception("Generic conversion"); }
  }
}
