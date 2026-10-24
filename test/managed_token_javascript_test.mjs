// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Actual generated token-only JS records versus independently authored wire fixtures.
import * as model from './model.mjs';
import * as fs from 'node:fs';
import * as path from 'node:path';
const [directory, widthText] = process.argv.slice(2), width = Number(widthText);
const {TokenWireEnvelope: Envelope, TokenWireContainers: Containers, TokenWireItem: Item,
  ManagedReference, Limits, ReadPolicy, Protocol} = model;
let checks = 0;
/** Keep wire checks independent of generated runtime assertion settings. */
function check(value) { ++checks; if (!value) throw new Error(`JavaScript managed wire check ${checks}`); }
/** Only ordinary native data failures satisfy malformed wire expectations. */
function reject(action) {
  let error; try { action(); } catch (caught) { error = caught; }
  check(error instanceof RangeError || error instanceof TypeError);
}
/** Exact bytes include metadata framing, unsigned values, field order and selected compact encoding. */
function equal(first, second) { return first.length === second.length && first.every((value, index) => value === second[index]); }
check(Item.MANAGED_CLASS_ID === 101n); check(!('ManagedStore' in model));
reject(() => { ManagedReference.prototype.validate = () => {}; });
/** A forged subtype cannot override the exact generated envelope validator. */
class ForgedReference extends ManagedReference { validate() {} }
for (const row of JSON.parse(fs.readFileSync(path.join(directory, 'manifest.json'), 'utf8'))) {
  const bytes = fs.readFileSync(path.join(directory, row.file));
  const limits = policy => new Limits(64 * 1024 * 1024, 16 * 1024 * 1024, 1000000, 64, policy);
  if (!row.valid) {
    for (const policy of Object.values(ReadPolicy)) reject(() => Envelope.decode(bytes, row.protocol, limits(policy)));
    continue;
  }
  const id = width === 4 ? Number(row.id) : BigInt(row.id);
  const reference = new ManagedReference(0xffffffffffffffffn, 2n, 101n, id), envelope = new Envelope();
  envelope.target = reference; check(equal(envelope.encode(row.protocol), bytes));
  const decoded = Envelope.decode(bytes, row.protocol);
  check(decoded.target.documentHigh === 0xffffffffffffffffn && decoded.target.documentLow === 2n);
  check(decoded.target.classId === 101n && decoded.target.objectId === id);
  check(equal(decoded.encode(row.protocol), bytes));
  for (let end = 0; end < bytes.length; ++end) reject(() => Envelope.decode(bytes.subarray(0, end), row.protocol));
  reject(() => Envelope.decode(Buffer.concat([bytes, Buffer.from('!')]), row.protocol));
  const wrapped = new Containers(); wrapped.selected = [reference]; wrapped.links = [reference, reference]; wrapped.lookup.set(7, reference);
  const restored = Containers.decode(wrapped.encode(row.protocol), row.protocol);
  check(restored.selected[0].objectId === id && restored.links[1].classId === 101n && restored.lookup.get(7).documentHigh === 0xffffffffffffffffn);
  const item = new Item(); item.next = [reference]; item.persistentId = width === 4 ? 0xffffffff : 0xffffffffffffffffn;
  check(Item.decode(item.encode(row.protocol), row.protocol).next[0].objectId === id);
  for (const invalid of [new ManagedReference(0n, 0n, 101n, id), new ManagedReference(1n, 2n, 102n, id),
    new ManagedReference(1n, 2n, 101n, width === 4 ? 0 : 0n), new ManagedReference(1, 2n, 101n, id),
    new ManagedReference(1n, 2n, 101n, width === 4 ? BigInt(id) : Number(id))]) {
    envelope.target = invalid; reject(() => envelope.encode(row.protocol));
  }
  envelope.target = new ForgedReference(1n, 2n, 102n, id); reject(() => envelope.encode(row.protocol));
  for (const invalid of [true, -1, width === 4 ? 0n : 0, width === 4 ? 0x100000000 : (1n << 64n)]) {
    item.persistentId = invalid; reject(() => item.encode(row.protocol));
  }
  fs.writeFileSync(path.join(directory, 'corpus-' + row.protocol + '-' + row.id + '.bin'), bytes);
}
reject(() => Envelope.decode(new TextEncoder().encode('{}'), Protocol.JSON));
reject(() => model.TokenHolder.decode(new TextEncoder().encode('{}'), Protocol.JSON));
console.log(`Actual generated JavaScript token checks: ${checks}`);
