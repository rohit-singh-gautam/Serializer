import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {pathToFileURL} from 'node:url';

const [compiler, producer, schema, directory] = process.argv.slice(2);
assert.ok(compiler && producer && schema && directory, 'Compiler, native producer, schema and work directory are required');
mkdirSync(directory, {recursive: true});

// Generate the public JS implementation and TypeScript declarations with the actual compiler.
function run(executable, arguments_) {
  const result = spawnSync(executable, arguments_, {encoding: 'utf8', windowsHide: true});
  assert.ifError(result.error);
  assert.equal(result.status, 0, result.stdout + result.stderr);
}
const javascript = join(directory, 'owning_variants.mjs');
const declarations = join(directory, 'owning_variants.d.mts');
run(compiler, ['--input', schema, '--language', 'js,typescript',
  '--js.output', javascript, '--typescript.output', declarations]);
run(producer, [directory]);

const {OwningVariantsRecord: Record, OwningVariantsTextPayload: TextPayload,
  OwningVariantsPrimitiveRecord: PrimitiveRecord, OwningVariantsRawRecord: RawRecord,
  Protocol, Limits} = await import(pathToFileURL(javascript).href);
const protocols = [
  ['none', Protocol.BINARY_NONE], ['integer', Protocol.BINARY_INTEGER],
  ['string', Protocol.BINARY_STRING], ['json', Protocol.JSON]
];
const kinds = ['code', 'text', 'event', 'alias'];
const utf8 = new TextEncoder();

// Match the owning C++ fixtures without constructing any inactive alternative.
function fixture(alternative) {
  const record = new Record();
  let value;
  if (alternative === 0) value = 17;
  else if (alternative === 1) value = 'owned text';
  else if (alternative === 3) value = 'alias text';
  else {
    value = new TextPayload();
    value.text = 'nested event';
    value.numbers = [1, 2, 3];
    value.labels = new Map([['alpha', 'one'], ['beta', 'two']]);
  }
  record.payload = {kind: kinds[alternative], value};
  return record;
}

for (const [name, protocol] of protocols) {
  for (let alternative = 0; alternative < kinds.length; ++alternative) {
    const bytes = readFileSync(join(directory, `record-alt${alternative}-${name}.wire`));
    const decoded = Record.decode(bytes, protocol);
    assert.equal(decoded.payload.kind, kinds[alternative]);
    assert.deepEqual(decoded, fixture(alternative));
    assert.deepEqual(Buffer.from(decoded.encode(protocol)), bytes);
    assert.deepEqual(Buffer.from(fixture(alternative).encode(protocol)), bytes);
    for (let end = 0; end < bytes.length; ++end) {
      assert.throws(() => Record.decode(bytes.subarray(0, end), protocol));
    }
    assert.throws(() => Record.decode(Buffer.concat([bytes, Buffer.from([0])]), protocol));
  }
  for (let alternative = 0; alternative < 2; ++alternative) {
    const value = alternative === 0 ? 42 : 1.25;
    const kind = alternative === 0 ? 'code' : 'measure';
    const owned = new PrimitiveRecord();
    owned.payload = {kind, value};
    const raw = new RawRecord();
    raw.payloadIndex = alternative;
    raw[alternative === 0 ? 'payloadCode' : 'payloadMeasure'] = value;
    const nativeOwned = readFileSync(join(directory, `primitive-alt${alternative}-${name}.wire`));
    const nativeRaw = readFileSync(join(directory, `raw-alt${alternative}-${name}.wire`));
    assert.deepEqual(nativeOwned, nativeRaw);
    assert.deepEqual(Buffer.from(owned.encode(protocol)), nativeRaw);
    assert.deepEqual(Buffer.from(raw.encode(protocol)), nativeOwned);
    assert.deepEqual(PrimitiveRecord.decode(nativeRaw, protocol), owned);
    assert.equal(RawRecord.decode(nativeOwned, protocol).payloadIndex, alternative);
  }
  const invalid = new Record();
  invalid.payload = {kind: 'missing', value: 0};
  assert.throws(() => invalid.encode(protocol), /Unknown union alternative/);
  assert.throws(() => Record.decode(fixture(2).encode(protocol), protocol,
    new Limits(undefined, undefined, 1)));
}

assert.deepEqual(Object.keys(new Record()), ['payload']);
assert.deepEqual(new Record().payload, {kind: 'code', value: 0});
assert.deepEqual(Record.decode(utf8.encode('{}'), Protocol.JSON).payload, {kind: 'code', value: 0});
assert.throws(() => Record.decode(Uint8Array.of(kinds.length), Protocol.BINARY_NONE), /Unknown union alternative/);
const generatedJson = new TextDecoder().decode(fixture(2).encode(Protocol.JSON));
assert.ok(generatedJson.includes('"payload:event"'));
assert.ok(!generatedJson.includes('"kind"'));

// A later selected object replaces an earlier object rather than merging stale fields.
const repeated = Record.decode(utf8.encode('{"payload:event":{"text":"discard","numbers":[1]},' +
  '"payload:event":{"labels":[{"key":"last","value":"owned"}]}}'), Protocol.JSON);
assert.equal(repeated.payload.kind, 'event');
assert.equal(repeated.payload.value.text, '');
assert.deepEqual(repeated.payload.value.numbers, []);
assert.deepEqual(repeated.payload.value.labels, new Map([['last', 'owned']]));
const independent = fixture(2);
independent.payload.value.numbers.push(99);
assert.deepEqual(fixture(2).payload.value.numbers, [1, 2, 3]);
assert.ok(Object.hasOwn(new RawRecord(), 'payloadIndex'));

const typescript = readFileSync(declarations, 'utf8');
assert.ok(typescript.includes('{ kind: "event"; value: OwningVariantsTextPayload }'));
assert.ok(typescript.includes('{ kind: "alias"; value: string }'));
// The optional tsc qualification consumes this file after the codec test generated its declarations.
writeFileSync(join(directory, 'consumer.mts'), `
import {OwningVariantsRecord, OwningVariantsTextPayload} from './owning_variants.mjs';
const record = new OwningVariantsRecord();
record.payload = {kind: 'event', value: new OwningVariantsTextPayload()};
if (record.payload.kind === 'event') {
  record.payload.value.numbers.push(1);
}
// @ts-expect-error The discriminant requires the corresponding declared payload type.
record.payload = {kind: 'event', value: 'invalid'};
// @ts-expect-error Owning variants expose one active payload rather than the legacy selector.
record.payloadIndex = 0;
`);
console.log('C++/JavaScript owning variant wire parity, lifetime shape, defaults and rejection checks passed');
