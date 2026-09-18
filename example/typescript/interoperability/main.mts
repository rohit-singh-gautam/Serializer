import assert from 'node:assert/strict';
import {readFileSync, writeFileSync, mkdirSync} from 'node:fs';
import {join} from 'node:path';
import {performance} from 'node:perf_hooks';
import {InteropMessage, InteropDetail, InteropState, Protocol} from './schema.mjs';

const variants = 3;

// Construct the independently specified shared fixture using generated defaults.
function fixture(variant: number) {
  const value = new InteropMessage();
  value.text = 'Ada "Lovelace" 🚀\n' + 'x'.repeat(64);
  value.numbers = [-2147483648, -1, 0, 2147483647];
  value.decimals = [-0, 1.5, -2.25];
  value.flags = [false, true]; value.labels = ['', 'é', '🚀'];
  const child = new InteropDetail(); child.code = -9; child.note = 'child';
  value.children = [new InteropDetail(), child];
  value.states = [InteropState.Paused, InteropState.Ready];
  value.counts = new Map([['🚀', 18446744073709551615n], ['é', 7n], ['a', 0n]]);
  value.indexed = new Map([[18446744073709551615n, child], [0n, new InteropDetail()]]);
  value.toggles = new Map([[true, 'yes'], [false, 'no']]);
  value.enums = new Map([[InteropState.Paused, -2], [InteropState.Ready, 1]]);
  value.payloadIndex = variant;
  value.payloadNumber = -1234567890123456789n; value.payloadRatio = -3.5; value.payloadState = InteropState.Paused;
  return value;
}

// Exchange all protocols and verify complete values through canonical positional encoding.
function exchange(directory: string, mode: string) {
  mkdirSync(directory, {recursive: true});
  const languages = readFileSync(join(directory, "producers.txt"), "utf8").trim().split(/\s+/);
  for (let variant = 0; variant < variants; ++variant) {
    const expected = fixture(variant);
    for (const [name, protocol] of Object.entries(Protocol)) {
      const bytes = expected.encode(protocol);
      if (mode === 'emit') writeFileSync(join(directory, `typescript_${name}_${variant}.bin`), bytes);
      else for (const language of languages) {
        const input = readFileSync(join(directory, `${language}_${name}_${variant}.bin`));
        const actual = InteropMessage.decode(input, protocol);
        assert.deepEqual(actual.encode(Protocol.BINARY_NONE), expected.encode(Protocol.BINARY_NONE), `${language} -> typescript ${name}/${variant}`);
        if (protocol !== Protocol.JSON) assert.deepEqual(new Uint8Array(input), bytes, 'Canonical binary mismatch');
      }
    }
  }
}

// Report a repeatable local throughput sample; this is not a cross-runtime ranking.
function benchmark() {
  const value = fixture(0), iterations = 10000;
  for (const [name, protocol] of Object.entries(Protocol)) {
    const bytes = value.encode(protocol);
    for (let i = 0; i < 1000; ++i) InteropMessage.decode(value.encode(protocol), protocol);
    let start = performance.now();
    for (let i = 0; i < iterations; ++i) value.encode(protocol);
    const encodeMs = performance.now() - start; start = performance.now();
    for (let i = 0; i < iterations; ++i) InteropMessage.decode(bytes, protocol);
    console.log(`js ${name}: ${bytes.length} bytes; encode ${(iterations * 1000 / encodeMs).toFixed(0)}/s; decode ${(iterations * 1000 / (performance.now() - start)).toFixed(0)}/s`);
  }
}

const [directory, mode = 'verify'] = process.argv.slice(2);
if (mode === 'benchmark') benchmark();
else if (directory && ['emit', 'verify'].includes(mode)) { exchange(directory, mode); console.log(`typescript ${mode} passed`); }
else throw new Error('Usage: node main.mjs <fixtures-directory> emit|verify|benchmark');
