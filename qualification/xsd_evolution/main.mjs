// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {EvolutionOrder, Protocol} from './schema.mjs';

// Stop before publishing any candidate whose native fields or callable changed.
function check(condition, message) {
  if (!condition) { throw Error(message); }
}

// Inspect native names and optional representation independently of XML associations.
function inspect(value, customer, note) {
  check(value.id === 7 && value.customer === customer, 'Native id/customer changed');
  check(value.note.length === note.length && value.note.every((item, index) => item === note[index]), 'Native note changed');
  check(value.hasId(7) && !value.hasId(8), 'Native readonly callable changed');
}

// Construct either variant through the same generated public API.
function authored(note) {
  const value = new EvolutionOrder();
  value.id = 7;
  value.customer = 'R&D';
  value.note = note;
  inspect(value, 'R&D', note);
  return value;
}

// Compare the independent expected native projection with the native XML decoder result.
function equal(first, second) {
  return first.length === second.length && first.every((value, index) => value === second[index]);
}

const args = process.argv.slice(2);
check(args.length === 3 || args.length === 4, 'Expected VARIANT ACTION INPUT/OUTDIR [OUTPUT]');
const [variant, action, input] = args;
check(variant === 'v1' || variant === 'v2', 'Unknown variant');
const xmlNote = ['memo'];
if (action === 'emit' && args.length === 3) {
  const fresh = new EvolutionOrder();
  inspect(fresh, 'native default', []);
  const value = authored(['memo']);
  const xml = value.encodeXml();
  const nativeBytes = value.encode(Protocol.BINARY_NONE);
  const integerBytes = value.encode(Protocol.BINARY_INTEGER);
  const stringBytes = value.encode(Protocol.BINARY_STRING);
  const defaults = fresh.encode(Protocol.BINARY_NONE);
  const projected = authored(xmlNote).encode(Protocol.BINARY_NONE);
  const decoded = EvolutionOrder.decodeXml(xml);
  inspect(decoded, 'R&D', xmlNote);
  check(equal(decoded.encode(Protocol.BINARY_NONE), projected), 'XML projection differs');
  inspect(EvolutionOrder.decode(nativeBytes, Protocol.BINARY_NONE), 'R&D', ['memo']);
  mkdirSync(input, {recursive: true});
  for (const [name, bytes] of [['authored.xml', xml], ['authored.bin', nativeBytes],
    ['authored-integer.bin', integerBytes], ['authored-string.bin', stringBytes],
    ['default.bin', defaults], ['xml-projection.bin', projected]]) {
    writeFileSync(join(input, name), bytes);
  }
} else {
  check(args.length === 4, 'Expected input and output paths');
  let output;
  if (action === 'check-xml') {
    const value = EvolutionOrder.decodeXml(readFileSync(input, 'utf8'));
    inspect(value, 'R&D', xmlNote);
    output = value.encode(Protocol.BINARY_NONE);
  } else if (action === 'check-native') {
    const value = EvolutionOrder.decode(readFileSync(input), Protocol.BINARY_NONE);
    inspect(value, 'R&D', ['memo']);
    output = value.encodeXml();
  } else {
    throw Error('Unknown action');
  }
  writeFileSync(args[3], output);
}
