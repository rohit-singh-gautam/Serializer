// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {EvolutionOrder, Protocol} from './schema.mjs';

// Require an external typed consumer to retain the same native shape and methods.
function check(condition: boolean, message: string): asserts condition {
  if (!condition) { throw Error(message); }
}

// Check native optional values and readonly callables without relying on XML field spelling.
function inspect(value: EvolutionOrder, customer: string, note: [] | [string]): void {
  check(value.id === 7 && value.customer === customer, 'Native id/customer changed');
  check(value.note.length === note.length && value.note.every((item, index) => item === note[index]), 'Native note changed');
  check(value.hasId(7) && !value.hasId(8), 'Native readonly callable changed');
}

// Author an owning model through independently compiled generated declarations.
function authored(note: [] | [string]): EvolutionOrder {
  const value: EvolutionOrder = new EvolutionOrder();
  value.id = 7;
  value.customer = 'R&D';
  value.note = note;
  inspect(value, 'R&D', note);
  return value;
}

// Compare complete native bytes for the independently selected XML projection.
function equal(first: Uint8Array, second: Uint8Array): boolean {
  return first.length === second.length && first.every((value, index) => value === second[index]);
}

const args: string[] = process.argv.slice(2);
check(args.length === 3 || args.length === 4, 'Expected VARIANT ACTION INPUT/OUTDIR [OUTPUT]');
const [variant, action, input] = args;
check(variant === 'v1' || variant === 'v2', 'Unknown variant');
const xmlNote: [] | [string] = ['memo'];
if (action === 'emit' && args.length === 3) {
  const fresh: EvolutionOrder = new EvolutionOrder();
  inspect(fresh, 'native default', []);
  const value: EvolutionOrder = authored(['memo']);
  const xml: string = value.encodeXml();
  const nativeBytes: Uint8Array = value.encode(Protocol.BINARY_NONE);
  const integerBytes: Uint8Array = value.encode(Protocol.BINARY_INTEGER);
  const stringBytes: Uint8Array = value.encode(Protocol.BINARY_STRING);
  const defaults: Uint8Array = fresh.encode(Protocol.BINARY_NONE);
  const projected: Uint8Array = authored(xmlNote).encode(Protocol.BINARY_NONE);
  const decoded: EvolutionOrder = EvolutionOrder.decodeXml(xml);
  inspect(decoded, 'R&D', xmlNote);
  check(equal(decoded.encode(Protocol.BINARY_NONE), projected), 'XML projection differs');
  inspect(EvolutionOrder.decode(nativeBytes, Protocol.BINARY_NONE), 'R&D', ['memo']);
  mkdirSync(input, {recursive: true});
  const outputs: [string, string | Uint8Array][] = [['authored.xml', xml], ['authored.bin', nativeBytes],
    ['authored-integer.bin', integerBytes], ['authored-string.bin', stringBytes],
    ['default.bin', defaults], ['xml-projection.bin', projected]];
  for (const [name, bytes] of outputs) { writeFileSync(join(input, name), bytes); }
} else {
  check(args.length === 4, 'Expected input and output paths');
  let output: string | Uint8Array;
  if (action === 'check-xml') {
    const value: EvolutionOrder = EvolutionOrder.decodeXml(readFileSync(input, 'utf8'));
    inspect(value, 'R&D', xmlNote);
    output = value.encode(Protocol.BINARY_NONE);
  } else if (action === 'check-native') {
    const value: EvolutionOrder = EvolutionOrder.decode(readFileSync(input), Protocol.BINARY_NONE);
    inspect(value, 'R&D', ['memo']);
    output = value.encodeXml();
  } else {
    throw Error('Unknown action');
  }
  writeFileSync(args[3], output);
}
