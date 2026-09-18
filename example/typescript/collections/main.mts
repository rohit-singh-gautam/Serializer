import {readFileSync, writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
import {ExampleModel, Protocol} from './schema.mjs';

// Edit a typed owning model and verify every field through canonical binary bytes.
const value = ExampleModel.decode(readFileSync(process.argv[2]), Protocol.JSON);
value.revision += 1;
const canonical = value.encode(Protocol.BINARY_NONE);
for (const protocol of Object.values(Protocol)) {
  const copy = ExampleModel.decode(value.encode(protocol), protocol);
  assert.deepEqual(copy.encode(Protocol.BINARY_NONE), canonical);
}
writeFileSync(process.argv[3], value.encode(Protocol.JSON));
console.log('Four protocols passed');
