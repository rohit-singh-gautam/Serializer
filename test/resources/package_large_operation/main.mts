// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
import {readFileSync, writeFileSync} from 'node:fs';
import {join} from 'node:path';
import {performance} from 'node:perf_hooks';
import {SerializerPackage as P, SerializerCompression as C, IdentityPackageArchive as A} from './schema.mjs';

const directory = process.argv[2], count = Number(process.argv[3]);
const provider = await C.nodeProvider(), host = await P.nodeHost();
const preserve = new P.ExtensionPolicy({parts: 'preserve', relationships: 'preserve'});
const resources = ['references', 'workUnits', 'decoderAllocationBytes', 'preservedBytes'] as const;
const ceilings = ['maxReferences', 'maxWorkUnits', 'maxDecoderAllocationBytes', 'maxPreservedBytes'] as const;
const options = {compressionProvider: provider, profiles: {registries: 'deflate' as const}};
let checks = 0;

// Reject false decisions independently of generated package exception paths.
function require(value: unknown, message: string): asserts value { if (!value) throw Error(message); }
// Grant finite capacity to the complete repeated logical pipeline.
function limits(extra: P.LimitOptions = {}): P.Limits {
  return new P.Limits({maxReferences: 4_000_000, maxWorkUnits: 4 * 2 ** 30, maxDecoderAllocationBytes: 2 * 2 ** 30, ...extra});
}
// Capture failure outside the operation under test; arbitrary caller error values are retained.
async function failure(operation: () => unknown): Promise<unknown> {
  try { await operation(); } catch (error) { return error; }
  throw Error('Unexpected operation success');
}
// Record fully completed independent decisions only.
function pass(name: string): void { ++checks; console.log('PASS ' + name); }
// Require complete large native models and preserved inert parts/source-scoped links.
function shape(value: A.Value): void {
  require(value.registries.length === 3 && value.extensions?.parts.length === 1 && value.extensions.relationships.length === 3, 'Aggregate/preservation shape');
  for (const item of value.registries) require(item.value.requests.length === count && item.value.entries.length === count, 'Incomplete large model');
}
// Invoke real generated identity-aware codecs with explicit Node DEFLATE admission.
async function load(bytes: Uint8Array, budget: P.Budget): Promise<P.Result<A.Value>> { return A.load(bytes, {...options, budget, policy: preserve}); }
// Span load, explicit DEFLATE encode, reload and atomic Save with the same public ledger.
async function pipeline(bytes: Uint8Array, path: string, budget: P.Budget): Promise<void> {
  let previous = budget.used.references; const value = (await load(bytes, budget)).value; shape(value);
  require(budget.used.references > previous, 'Load bypassed references'); previous = budget.used.references;
  const encoded = await A.encode(value, {...options, budget}); require(budget.used.references > previous, 'Encode reset references'); previous = budget.used.references;
  shape((await load(encoded, budget)).value); require(budget.used.references > previous, 'Reload reset references');
  await A.save(path, value, {...options, budget, host});
}
// Serialize measured logical counters without exposing private runtime state.
function metrics(budget: P.Budget): Record<string, number> { return {references: budget.used.references, work: budget.used.workUnits, allocation: budget.used.decoderAllocationBytes, preserved: budget.used.preservedBytes}; }
// Compare owning bytes without treating successful generated decoding as an independent oracle.
function equal(first: Uint8Array, second: Uint8Array): boolean { return first.length === second.length && first.every((value, index) => value === second[index]); }

const valid = readFileSync(join(directory, 'valid.opc')), invalid = readFileSync(join(directory, 'invalid.opc'));
const baseline = new P.Budget(limits()), value = (await load(valid, baseline)).value; shape(value);
require(baseline.used.references >= 9 * count, 'Identity tuples bypassed ledger'); pass('large generated identity load and preserved opaque/link metadata');
let validated = false; const sentinel = {}; let published: unknown = sentinel;
const bad = await failure(async () => { published = await A.load(invalid, {...options, budget: new P.Budget(limits()), policy: preserve, validate() { validated = true; }}); });
require(bad instanceof P.Error && !validated && published === sentinel, 'Invalid final-owner keyref published'); pass('invalid final owner has no aggregate publication');
for (const [index, resource] of resources.entries()) for (const tight of [false, true]) {
  const budget = new P.Budget(limits({[ceilings[index]]: baseline.used[resource] - (tight ? 1 : 0)}));
  if (tight) { const error = await failure(() => load(valid, budget)); require(error instanceof P.Error && error.code === P.Code.ResourceLimit, 'Wrong tight quota failure'); }
  else shape((await load(valid, budget)).value);
  pass(resource + (tight ? ' one tight' : ' exact'));
}
for (const mode of ['middle', 'late', 'expiry', 'original']) {
  const marker = Error('original ' + mode), secondary = Error('secondary cancellation'); let armed = false, expiry = Infinity;
  let budget: P.Budget | undefined;
  budget = new P.Budget(limits({checkCancelled() {
    if (!budget) return;
    if ((mode === 'middle' || mode === 'expiry') && budget.used.references >= 9 * count / 2) {
      if (mode === 'middle') throw marker;
      if (expiry === Infinity) expiry = performance.now();
      if (performance.now() >= expiry) throw marker;
    }
    if (armed) throw mode === 'original' ? secondary : marker;
  }}));
  published = sentinel;
  const error = await failure(async () => { published = await A.load(valid, {...options, budget, policy: preserve, validate(candidate, active) {
    require(active === budget, 'Aggregate validator received a fresh ledger'); shape(candidate); armed = true;
    if (mode === 'original') throw marker;
  }}); });
  require(error === marker && published === sentinel, 'Cancellation/error identity or publication changed: ' + mode);
  if (mode === 'expiry') require(expiry !== Infinity, 'Host monotonic expiry not reached');
  pass('large load ' + mode);
}
const marker = Error('original encode cancellation'); let selected: P.Budget | undefined;
selected = new P.Budget(limits({checkCancelled() { if (selected && selected.used.references >= 9 * count / 2) throw marker; }}));
let writes = 0;
require(await failure(() => A.write({write() { ++writes; }}, value, {...options, budget: selected})) === marker && writes === 0, 'Incomplete staged XML reached borrowed sink'); pass('middle encode cancellation publishes no sink bytes');
const old = new TextEncoder().encode('existing complete sentinel'), guarded = join(directory, 'guarded.opc'); writeFileSync(guarded, old);
let armed = false; const saveBudget = new P.Budget(limits({checkCancelled() { if (armed) throw marker; }}));
require(await failure(() => A.save(guarded, value, {...options, budget: saveBudget, host, fault(stage) { if (stage === 'before_replace') armed = true; }})) === marker, 'Late Save error changed');
require(armed && equal(readFileSync(guarded), old), 'Late Save replaced sentinel'); pass('late atomic Save preserves previous file');
const combined = new P.Budget(limits()); await pipeline(valid, join(directory, 'output.opc'), combined); pass('shared load DEFLATE reload Save pipeline');
for (const tight of [false, true]) {
  const budget = new P.Budget(limits({maxReferences: combined.used.references - (tight ? 1 : 0)})), path = join(directory, tight ? 'tight.opc' : 'exact.opc'); writeFileSync(path, old);
  if (tight) { const error = await failure(() => pipeline(valid, path, budget)); require(error instanceof P.Error && error.code === P.Code.ResourceLimit && equal(readFileSync(path), old), 'Combined quota published Save'); }
  else await pipeline(valid, path, budget);
  pass(tight ? 'combined reference one tight' : 'combined reference exact');
}
shape((await load(readFileSync(join(directory, 'output.opc')), new P.Budget(limits()))).value); pass('fresh load recovery');
console.log('LEDGER\t' + JSON.stringify({load: metrics(baseline), combined: metrics(combined), checks}));
