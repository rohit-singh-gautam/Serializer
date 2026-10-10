import {readFileSync, writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
import {ExampleModel, Protocol} from './schema.mjs';

// Edit a typed owning model and verify every field through canonical binary bytes.
const defaults = new ExampleModel();
assert.deepEqual([defaults.pages.length, defaults.columns.length, defaults.headings.length], [2, 3, 2]);
defaults.pages[0].title = 'First';
assert.deepEqual(defaults.pages[1].title, '');
const value = ExampleModel.decode(readFileSync(process.argv[2]), Protocol.JSON);
value.lookupCache = 77n;
value.revision += 1;
const canonical = value.encode(Protocol.BINARY_NONE);
for (const protocol of Object.values(Protocol)) {
  const copy = ExampleModel.decode(value.encode(protocol), protocol);
  assert.equal(copy.lookupCache, 0n);
  assert.deepEqual(copy.encode(Protocol.BINARY_NONE), canonical);
}
writeFileSync(process.argv[3], value.encode(Protocol.JSON));
console.log('Four protocols passed');
