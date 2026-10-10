import {readFileSync, writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
import {CollectionPayload, ExampleModel, Protocol, Sample, TextPayload} from './schema.mjs';

// Select owning alternatives directly; generated codecs retain their named classes.
const original = new Sample();
const text = new TextPayload();
text.message = 'owned by the generated variant';
text.tags = ['first', 'second'];
original.payload = {kind: 'text', value: text};
const copy = Sample.decode(original.encode(Protocol.BINARY_NONE), Protocol.BINARY_NONE);
const collection = new CollectionPayload();
collection.numbers = [1, 2, 3];
collection.labels.set('region', 'south');
copy.payload = {kind: 'collection', value: collection};
assert.deepEqual(original.payload.kind, 'text');
assert.deepEqual(copy.payload.kind, 'collection');
for (const sample of [original, copy]) {
  for (const protocol of Object.values(Protocol)) {
    const decoded = Sample.decode(sample.encode(protocol), protocol);
    assert.deepEqual(decoded.encode(Protocol.BINARY_NONE), sample.encode(Protocol.BINARY_NONE));
  }
}

// Read both raw and owning alternatives and verify the complete independent fixture.
const value = ExampleModel.decode(readFileSync(process.argv[2]), Protocol.JSON);
const canonical = value.encode(Protocol.BINARY_NONE);
for (const protocol of Object.values(Protocol)) {
  const copy = ExampleModel.decode(value.encode(protocol), protocol);
  assert.deepEqual(copy.encode(Protocol.BINARY_NONE), canonical);
}
// Generated encoders reject a future revision; the runner also checks every language's reader.
const unsupported = ExampleModel.decode(canonical, Protocol.BINARY_NONE);
unsupported.version = 2;
for (const protocol of Object.values(Protocol)) {
  assert.throws(() => unsupported.encode(protocol), /Unsupported schema version/);
}
writeFileSync(process.argv[3], value.encode(Protocol.JSON));
console.log('Four protocols passed');
