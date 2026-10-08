import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {join} from 'node:path';
import {Fixture, FixedBytes, VariableBytes, Protocol, Limits} from './schema.mjs';

const directory = process.argv[2];
const utf8 = new TextEncoder();

// Construct exactly the native fixture, including fixed-array union alternatives.
function fixture(alternative) {
  const value = new Fixture();
  value.shortHash.bytes = Array.from({length: 20}, (_, index) => index + 1);
  value.longHash.bytes = Array.from({length: 32}, (_, index) => index + 21);
  value.nested[0].value = 41;
  value.nested[1].value = 42;
  value.commitIndex = alternative;
  value.commitSha1.bytes = value.shortHash.bytes.slice();
  value.commitSha256.bytes = value.longHash.bytes.slice();
  return value;
}

for (const protocol of Object.values(Protocol)) {
  for (let alternative = 0; alternative < 3; ++alternative) {
    const bytes = readFileSync(join(directory, `${alternative}-${protocol}.bin`));
    const value = Fixture.decode(bytes, protocol);
    assert.deepEqual(Buffer.from(value.encode(protocol)), bytes);
    assert.deepEqual(Buffer.from(fixture(alternative).encode(protocol)), bytes);
  }
  for (const count of [0, 20, 31, 33]) {
    const variable = new VariableBytes(); variable.bytes = Array(count).fill(1);
    assert.throws(() => FixedBytes.decode(variable.encode(protocol), protocol), /extent/);
    const invalid = new FixedBytes(); invalid.bytes = Array(count).fill(1);
    assert.throws(() => invalid.encode(protocol), /extent/);
  }
  const value = new FixedBytes(), bytes = value.encode(protocol);
  assert.equal(FixedBytes.decode(bytes, protocol).bytes.length, 32);
  for (let end = 0; end < bytes.length; ++end) {
    assert.throws(() => FixedBytes.decode(bytes.subarray(0, end), protocol));
  }
  assert.throws(() => FixedBytes.decode(bytes, protocol, new Limits(undefined, undefined, 31)));
}
assert.throws(() => FixedBytes.decode(Uint8Array.of(33), Protocol.BINARY_NONE), /extent/);
assert.equal(FixedBytes.decode(utf8.encode('{}'), Protocol.JSON).bytes.length, 32);
const defaults = new Fixture(), separate = new Fixture();
defaults.nested[0].value = 99;
assert.equal(defaults.nested[1].value, 7);
assert.equal(separate.nested[0].value, 7);
assert.throws(() => FixedBytes.decode(utf8.encode('{"bytes":[' + '1,'.repeat(32) + '{}]}'), Protocol.JSON), /extent/);
console.log('C++/JavaScript fixed-array wire, defaults, union, and rejection checks passed');
