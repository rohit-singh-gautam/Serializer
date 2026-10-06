import {readFileSync, writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
import {ExampleModel, Protocol, Limits, ReadPolicy, Uint16Version, Uint32Version, Uint64Version, FloatVersion, DoubleVersion, Dotted2Version, Dotted3Version, Dotted4Version} from './schema.mjs';

// Edit a typed owning model and verify every field through canonical binary bytes.
{
  const typed = new Uint16Version();
  assert.deepEqual(typed.encode(Protocol.BINARY_NONE), new Uint8Array([44, 1]));
  for (const protocol of Object.values(Protocol)) {
    assert.deepEqual(Uint16Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE), typed.encode(Protocol.BINARY_NONE));
  }
}
{
  const typed = new Uint32Version();
  assert.deepEqual(typed.encode(Protocol.BINARY_NONE), new Uint8Array([112, 17, 1, 0]));
  for (const protocol of Object.values(Protocol)) {
    assert.deepEqual(Uint32Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE), typed.encode(Protocol.BINARY_NONE));
  }
}
{
  const typed = new Uint64Version();
  assert.deepEqual(typed.encode(Protocol.BINARY_NONE), new Uint8Array([255, 255, 255, 255, 255, 255, 255, 255]));
  for (const protocol of Object.values(Protocol)) {
    assert.deepEqual(Uint64Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE), typed.encode(Protocol.BINARY_NONE));
  }
}
{
  const typed = new FloatVersion();
  assert.deepEqual(typed.encode(Protocol.BINARY_NONE), new Uint8Array([205, 204, 204, 61]));
  for (const protocol of Object.values(Protocol)) {
    assert.deepEqual(FloatVersion.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE), typed.encode(Protocol.BINARY_NONE));
  }
}
{
  const typed = new DoubleVersion();
  assert.deepEqual(typed.encode(Protocol.BINARY_NONE), new Uint8Array([0, 0, 0, 0, 0, 0, 4, 64]));
  for (const protocol of Object.values(Protocol)) {
    assert.deepEqual(DoubleVersion.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE), typed.encode(Protocol.BINARY_NONE));
  }
}
{
  const typed = new Dotted2Version();
  assert.deepEqual(typed.encode(Protocol.BINARY_NONE), new Uint8Array([1, 0, 10, 0]));
  for (const protocol of Object.values(Protocol)) {
    assert.deepEqual(Dotted2Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE), typed.encode(Protocol.BINARY_NONE));
  }
}
{
  const typed = new Dotted3Version();
  assert.deepEqual(typed.encode(Protocol.BINARY_NONE), new Uint8Array([1, 0, 10, 0, 0, 0]));
  for (const protocol of Object.values(Protocol)) {
    assert.deepEqual(Dotted3Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE), typed.encode(Protocol.BINARY_NONE));
  }
}
{
  const typed = new Dotted4Version();
  assert.deepEqual(typed.encode(Protocol.BINARY_NONE), new Uint8Array([1, 0, 10, 0, 0, 0, 4, 0]));
  for (const protocol of Object.values(Protocol)) {
    assert.deepEqual(Dotted4Version.decode(typed.encode(protocol), protocol).encode(Protocol.BINARY_NONE), typed.encode(Protocol.BINARY_NONE));
  }
}
const value = ExampleModel.decode(readFileSync(process.argv[2]), Protocol.JSON, new Limits(undefined, undefined, undefined, undefined, ReadPolicy.COMPATIBLE));
const old = ExampleModel.decode(value.encode(Protocol.BINARY_NONE), Protocol.BINARY_NONE, new Limits(undefined, undefined, undefined, undefined, ReadPolicy.COMPATIBLE));
assert.equal(old.oldName, "Ada");
value.name = value.oldName;
value.version = 10;
const canonical = value.encode(Protocol.BINARY_NONE);
for (const protocol of Object.values(Protocol)) {
  const copy = ExampleModel.decode(value.encode(protocol), protocol);
  assert.deepEqual(copy.encode(Protocol.BINARY_NONE), canonical);
}
writeFileSync(process.argv[3], value.encode(Protocol.JSON));
console.log('Version migration and four protocols passed');
