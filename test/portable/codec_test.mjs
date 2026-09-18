import assert from 'node:assert/strict';
import {CheckVisibility, CheckEmpty, CheckBytesValue, CheckBoolValue, CheckTextValue, CheckFloatValue, CheckIntegerValue,
  CheckRecursive, CheckDefaultValue, InteropMessage, Protocol, Limits} from './schema.mjs';

const utf8 = new TextEncoder();
// Require rejection without exposing a partially decoded object.
function reject(Type, bytes, protocol, limits) {
  assert.throws(() => Type.decode(typeof bytes === 'string' ? utf8.encode(bytes) : Uint8Array.from(bytes), protocol, limits));
}

for (const protocol of Object.values(Protocol)) {
  const value = new InteropMessage(), bytes = value.encode(protocol);
  assert.deepEqual(InteropMessage.decode(bytes, protocol).encode(protocol), bytes);
  for (let end = 0; end < bytes.length; ++end) reject(InteropMessage, bytes.subarray(0, end), protocol);
  reject(InteropMessage, [...bytes, 0], protocol);
  reject(InteropMessage, bytes, protocol, new Limits(bytes.length - 1));
  reject(InteropMessage, bytes, protocol, new Limits(undefined, undefined, undefined, 0));
}
reject(CheckBoolValue, [2], Protocol.BINARY_NONE);
reject(CheckBoolValue, '{"value":null}', Protocol.JSON);
for (const text of ['{"value":01}', '{"value":-1}', '{"value":18446744073709551616}', '{"value":1e3}', '{"value":1,}', '{"unknown":1}']) reject(CheckIntegerValue, text, Protocol.JSON);
for (const text of ['{"value":NaN}', '{"value":1e100}', '{"value":1e-100}', '{"value":+1}']) reject(CheckFloatValue, text, Protocol.JSON);
for (const text of ['{"value":"\\ud800"}', '{"value":"\\udc00"}', '{"value":"\\ud800x"}', '{"value":"x\n"}']) reject(CheckTextValue, text, Protocol.JSON);
reject(CheckTextValue, [2, 0xc0, 0xaf], Protocol.BINARY_NONE);
reject(CheckTextValue, '{"value":"abcdef"}', Protocol.JSON, new Limits(100, 3));
reject(CheckTextValue, '{"value":"\\u0061"}', Protocol.JSON, new Limits(100, 3));
reject(CheckBytesValue, [0xff, 0xff, 0xff, 0xff], Protocol.BINARY_NONE);
reject(CheckBytesValue, [3, 1, 2], Protocol.BINARY_NONE);
reject(CheckBytesValue, '{"values":[1,2,3]}', Protocol.JSON, new Limits(100, 100, 2));
reject(CheckBytesValue, '{"values":[1,]}', Protocol.JSON);
reject(InteropMessage, '{"payload:missing":0}', Protocol.JSON);
reject(InteropMessage, '{"states":["missing"]}', Protocol.JSON);
reject(InteropMessage, '{"counts":[{"key":"a"}]}', Protocol.JSON);
reject(InteropMessage, '{"counts":[{"key":"a","value":1,"extra":0}]}', Protocol.JSON);
assert.equal(CheckIntegerValue.decode(utf8.encode('{"value":18446744073709551615}'), Protocol.JSON).value, 18446744073709551615n);
assert.equal(CheckTextValue.decode(Uint8Array.of(0x40, 0), Protocol.BINARY_NONE).value, '');
assert.equal(CheckTextValue.decode(utf8.encode('{"value":"\\ud83d\\ude80"}'), Protocol.JSON).value, '🚀');
assert.equal(CheckTextValue.decode(Uint8Array.of(3, 0xef, 0xbb, 0xbf), Protocol.BINARY_NONE).value, '\ufeff');
const merged = InteropMessage.decode(utf8.encode('{"nested":{"code":99},"nested":{"note":"merged"},"numbers":[1,2],"numbers":[3],"counts":[{"value":1,"key":"a"},{"key":"a","value":2}]}'), Protocol.JSON);
assert.equal(merged.nested.code, 99); assert.equal(merged.nested.note, 'merged');
assert.deepEqual(merged.numbers, [3]); assert.equal(merged.counts.get('a'), 2n);
assert.equal(InteropMessage.decode(utf8.encode('{} \r\n\t'), Protocol.JSON).bigUnsigned, 18446744073709551615n);
assert.equal(new CheckDefaultValue().escaped, 'a\\b\n"c');
const cycle = new CheckRecursive(); cycle.children.push(cycle);
assert.throws(() => cycle.encode(Protocol.BINARY_NONE));
const nested = new CheckRecursive(); nested.children.push(new CheckRecursive());
reject(CheckRecursive, nested.encode(Protocol.BINARY_NONE), Protocol.BINARY_NONE, new Limits(100, 100, 100, 1));
const invalid = new CheckIntegerValue(); invalid.value = 9007199254740992;
assert.throws(() => invalid.encode(Protocol.JSON));
invalid.value = 1n << 64n; assert.throws(() => invalid.encode(Protocol.BINARY_NONE));
const malformedText = new CheckTextValue(); malformedText.value = '\ud800'; assert.throws(() => malformedText.encode(Protocol.JSON));
assert.throws(() => CheckEmpty.decode(new Uint8Array(), 99));
assert.throws(() => new Limits(-1));
console.log('JavaScript codec boundary and rejection tests passed');

const visibility = new CheckVisibility();
assert.equal(visibility.hidden, undefined);
assert.equal(new TextDecoder().decode(visibility.encode(Protocol.JSON)), '{"hidden":"secret","counter":42,"visible":7}');
