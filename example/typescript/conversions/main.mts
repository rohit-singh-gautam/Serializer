import {ShipmentTicket, ShortTicket, RoutingMode} from './schema.mjs';
const source = new ShipmentTicket(); source.number = 18446744073709551615n;
source.contact.deliveryCount = 19; source.region = RoutingMode.Express; source.base0.priority = 7;
const result = source.toNotice();
if (result.number !== source.number || result.destination.deliveryCount !== 19 || !result.preview ||
    result.mode !== RoutingMode.Express || result.priority !== 7) throw Error('Conversion');
result.destination.street = 'Changed';
if (source.contact.street !== 'Harbor Road') throw Error('Aliased record');
const address = source.toAddress();
if (address.postcode !== 410 || address.deliveryCount !== 12) throw Error('Defaults');
const ticket = new ShortTicket(); ticket.value = 17;
if (ticket.toView().value !== 17 || ticket.toView().revision !== 3) throw Error('Generic conversion');
