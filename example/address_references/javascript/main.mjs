import { Consumer, Owner, ProjectedValue, Protocol } from './schema.mjs';

// The owner stores b. Binding d retains that owner without copying b.
const document = new Owner();
document.b.text = 'initial';
document.b.count = 1;
const observer = new Consumer();
observer.d.bind(document, 'document-1', 7n, 1n);

// The reference follows the live field slot, so the next read sees this edit.
document.b.text = 'updated';
document.b.count = 2;
const output = observer.d.withValue(live =>
  live.withToWire(projected => projected.encode(Protocol.JSON)));
const value = ProjectedValue.decode(output, Protocol.JSON);
if (value.label !== 'updated' || value.count !== 2) {
  throw Error('The projection must read the current owner field');
}

// Owners and reference addresses are persisted separately. Loading an address
// does not recover its runtime binding; the application resolves its identity.
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
