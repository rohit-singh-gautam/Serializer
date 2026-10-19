import { Consumer, Owner, ProjectedValue, Protocol } from './schema.mjs';

// Shared binding retains the storage owner; d does not contain a copy of b.
const document = new Owner();
document.b.text = 'initial';
document.b.count = 1;
const observer = new Consumer();
observer.d.bind(document, 'document-1', 7n, 1n);

document.b.text = 'updated';
document.b.count = 2;
// Both callbacks finish before returning. Only the encoded bytes escape.
const output = observer.d.withValue(live =>
  live.withToWire(projected => projected.encode(Protocol.JSON)));
const value = ProjectedValue.decode(output, Protocol.JSON);
if (value.label !== 'updated' || value.count !== 2) {
  throw Error('The projection must read the current owner field');
}

// Persist owners once, then resolve separately decoded reference addresses.
const loadedDocument = Owner.decode(document.encode(Protocol.JSON), Protocol.JSON);
const loadedObserver = Consumer.decode(observer.encode(Protocol.JSON), Protocol.JSON);
if (loadedObserver.d.isBound()) {
  throw Error('A decoded address must start unbound');
}
if (!loadedObserver.d.resolve(loadedDocument, 'document-1', 7n, 1n)) {
  throw Error('The loaded address must resolve to its matching owner');
}
loadedObserver.d.withValue(live => {
  if (live.text !== 'updated') {
    throw Error('Resolution must expose the loaded owner field');
  }
});

console.log(new TextDecoder().decode(output));
