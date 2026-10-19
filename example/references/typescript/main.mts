// Export original payload buffers through generated typed references.
import {Payload, SharedOwner, SharedValueConsumer, Owner, OwningConsumer, BorrowedConsumer, SharedConsumer, WeakConsumer, Protocol} from './schema.mjs';

const owner = new Owner();
owner.b.values = [1, 2, 3]; owner.b.codes = [0, 1, 2];
const borrowed = new BorrowedConsumer(); borrowed.d.bind(owner);
const shared = new SharedConsumer(); shared.d.bind(owner);
const weak = new WeakConsumer(); weak.d.bind(owner);
if (shared.d.readValue() !== owner.b || shared.d.readValue().values !== owner.b.values) throw Error('Copied payload');
owner.b.values[0] = 4;
const expected = new OwningConsumer(); expected.d = owner.b;
const equal = (left: Uint8Array, right: Uint8Array): boolean => left.length === right.length && left.every((value, index) => value === right[index]);
// Both ordinary records retain the same independent payload.
let value: Payload | null = new Payload(); value.values = [7, 8];
const a = new SharedOwner(); a.b.bind(value);
const c = new SharedValueConsumer(); c.d.bind(value);
if (a.b.readValue() !== value || c.d.readValue() !== value) throw Error('Independent sharing');
value = null; a.b.unbind();
const independentExpected = new OwningConsumer(); independentExpected.d = c.d.readValue();
for (const protocol of Object.values(Protocol)) if (!equal(c.encode(protocol), independentExpected.encode(protocol))) throw Error('Shared value wire mismatch');
for (const protocol of Object.values(Protocol)) {
  const encoded = expected.encode(protocol);
  for (const reference of [borrowed, shared, weak]) if (!equal(reference.encode(protocol), encoded)) throw Error('Wire mismatch');
  owner.withAsConsumer(projected => { if (!equal(projected.encode(protocol), encoded)) throw Error('Translator mismatch'); });
  const loaded = OwningConsumer.decode(encoded, protocol);
  if (!equal(loaded.encode(protocol), encoded)) throw Error('Owned decode');
  loaded.d.values[0] = 99;
  if (owner.b.values[0] !== 4) throw Error('Decode changed source');
}
console.log(new TextDecoder().decode(shared.encode(Protocol.JSON)));
