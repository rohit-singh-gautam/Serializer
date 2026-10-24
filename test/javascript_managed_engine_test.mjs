// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Independent native-kernel qualification. This fixture is not a generated SDK.
import * as testFs from 'node:fs';
import * as testPath from 'node:path';
import {Worker} from 'node:worker_threads';
import {runInNewContext} from 'node:vm';

/*SERIALIZER_MANAGED_ENGINE_RUNTIME*/

let checks = 0;
/** Count assertions without weakening the native error class being tested. */
function check(condition, message) {
  ++checks; if (!condition) throw new Error(`Check ${checks}: ${message}`);
}
/** Reject only an expected managed failure, preserving unexpected implementation exceptions. */
function fails(action, type = ManagedError) {
  let error; try { action(); } catch (caught) { error = caught; }
  check(error instanceof type, `Expected ${type.name}; got ${error?.stack}`); return error;
}
/** Check asynchronous native transitions with the same exact failure requirement. */
async function rejects(action, type = ManagedError) {
  let error; try { await action(); } catch (caught) { error = caught; }
  check(error instanceof type, `Expected async ${type.name}; got ${error?.stack}`); return error;
}
/** Exact graph bytes remain independent of JSON/string object identity. */
function same(first, second) { return first.length === second.length && first.every((value, index) => value === second[index]); }

/** Mutable ordinary payload classes have no engine or fixture-business behavior. */
class WireLeaf { constructor(id, text = 'leaf') { this.id = id; this.text = text; } }
/** One owning branch can occur either inside or outside managed participation. */
class WireBranch { constructor(id, leaves = []) { this.id = id; this.leaves = leaves; } }
/** Owning graph metadata identifies participation, including an ordinary ancestor boundary. */
class WireGraph {
  constructor(zero) {
    this.id = zero; this.label = 'baseline';
    this.branches = [new WireBranch(zero, [new WireLeaf(zero)])];
    this.ordinary = new WireBranch(zero, [new WireLeaf(zero, 'ordinary')]);
    this.link = null; this.entries = new Map([[20, 'twenty'], [3, 'three']]);
  }
}
const rootTag = 11n, branchTag = 22n, leafTag = 33n;
const document = new ManagedDocument(0x1122334455667788n, 0x8877665544332211n);

/** Supply an independent finite ordinary codec and generated-shaped metadata for either native ID width. */
function fixture(width, limits) {
  const zero = width === 4 ? 0 : 0n;
  /** Encode the selected native ID representation without coercing malformed host scalars. */
  function idText(id) {
    if (width === 4) {
      if (typeof id !== 'number' || !Number.isInteger(id) || id < 0 || id > 0xffffffff) throw new Error('Invalid uint32');
    } else if (typeof id !== 'bigint' || id < 0n || id > ((1n << 64n) - 1n)) throw new Error('Invalid uint64');
    return String(id);
  }
  /** Decode canonical decimal IDs into the native width, never rounded Numbers for uint64. */
  function parseId(text) {
    if (typeof text !== 'string' || !/^(0|[1-9][0-9]*)$/.test(text) || text.length > 20) throw new Error('Invalid decimal');
    const value = width === 4 ? Number(text) : BigInt(text); idText(value); return value;
  }
  /** Keep record cardinalities and ordinary scalar types strict in the independently supplied codec. */
  function array(value, length) { if (!Array.isArray(value) || value.length !== length) throw new Error('Invalid record'); return value; }
  /** Reject malformed text before a native UTF-8 encoder can replace unmatched surrogates. */
  function text(value) {
    if (typeof value !== 'string' || value.length > 4096 || !value.isWellFormed()) throw new Error('Invalid string'); return value;
  }
  /** Produce canonical numeric-map order, intentionally different from ordinary insertion order. */
  function encode(value) {
    const leaf = item => [idText(item.id), text(item.text)];
    const branch = item => [idText(item.id), item.leaves.map(leaf)];
    const edge = value.link === null ? null : [String(value.link.document.high), String(value.link.document.low),
      String(value.link.classId), idText(value.link.objectId)];
    const entries = [...value.entries].sort((a, b) => a[0] - b[0]).map(([key, item]) => {
      if (!Number.isSafeInteger(key)) throw new Error('Invalid map key'); return [key, text(item)];
    });
    return new TextEncoder().encode(JSON.stringify([idText(value.id), text(value.label), value.branches.map(branch),
      branch(value.ordinary), edge, entries]));
  }
  /** Reconstruct fresh native record instances and selected-width tokens from complete ordinary bytes. */
  function decode(bytes) {
    const record = array(JSON.parse(new TextDecoder('utf-8', {fatal: true}).decode(bytes)), 6);
    const leaf = item => { array(item, 2); return new WireLeaf(parseId(item[0]), text(item[1])); };
    const branch = item => { array(item, 2); return new WireBranch(parseId(item[0]), item[1].map(leaf)); };
    const value = new WireGraph(zero); value.id = parseId(record[0]); value.label = text(record[1]);
    value.branches = record[2].map(branch); value.ordinary = branch(record[3]);
    value.link = record[4] === null ? null : (() => {
      const edge = array(record[4], 4);
      return new ManagedToken(new ManagedDocument(BigInt(edge[0]), BigInt(edge[1])), BigInt(edge[2]), parseId(edge[3]));
    })();
    value.entries = new Map(record[5].map(item => {
      array(item, 2); if (!Number.isSafeInteger(item[0])) throw new Error('Invalid map key'); return [item[0], text(item[1])];
    }));
    if (value.entries.size !== record[5].length) throw new Error('Duplicate map key'); return value;
  }
  /** Traverse all owning records once, propagating ordinary boundaries and exact typed reference targets. */
  function visit(value, visitor) {
    visitor.entity(value, rootTag, () => value.id, id => { value.id = id; }, true);
    const branch = (item, managed) => {
      visitor.entity(item, branchTag, () => item.id, id => { item.id = id; }, managed);
      for (const leaf of item.leaves) visitor.entity(leaf, leafTag, () => leaf.id, id => { leaf.id = id; }, managed);
    };
    for (const item of value.branches) branch(item, true); branch(value.ordinary, false);
    if (value.link !== null) visitor.reference(value.link.document, value.link.classId, value.link.objectId, leafTag);
  }
  const codec = {encode, decode, visit};
  return {zero, codec, schema: new ManagedSchema(WireGraph, 'fixture:owning:v1', width, codec, limits),
    initial: () => new WireGraph(zero), leaf: () => new WireLeaf(zero)};
}

/** Qualify pure ownership, exact scalar identity, scope, history and collaboration for one native width. */
async function memory(width) {
  const f = fixture(width), one = width === 4 ? 1 : 1n;
  const store = new ManagedStore(f.schema, f.initial(), document, new ManagedOptions(), edit => {
    edit.root.link = edit.token(edit.root.branches[0].leaves[0]);
  });
  const baseline = store.read(), original = baseline.bytes(), graph = baseline.copyValue();
  check(Object.isFrozen(baseline) && Object.isFrozen(store), 'Opaque wrappers frozen');
  check(graph.id === one && store.allocatedId() === (width === 4 ? 3 : 3n), 'Preorder identities');
  check(graph.ordinary.id === f.zero && graph.ordinary.leaves[0].id === f.zero, 'Ordinary ancestor suppresses descendants');
  check([...graph.entries.keys()].join(',') === '3,20', 'Canonical map clone order');
  const token = graph.link; check(baseline.resolve(token, WireLeaf).text === 'leaf', 'Typed live resolution');
  fails(() => baseline.resolve(token, WireBranch));
  fails(() => baseline.resolve(new ManagedToken(new ManagedDocument(1n, 2n), leafTag, token.objectId), WireLeaf));
  fails(() => baseline.resolve(new ManagedToken(document, branchTag, token.objectId), WireLeaf));
  fails(() => baseline.resolve(new ManagedToken(document, leafTag, width === 4 ? BigInt(token.objectId) : Number(token.objectId)), WireLeaf));
  graph.label = 'outside'; graph.branches[0].leaves[0].text = 'outside';
  const bytes = baseline.bytes(); bytes.fill(0); check(same(baseline.bytes(), original), 'Copy mutations cannot alter pin');
  const transferred = baseline.bytes(); structuredClone(transferred, {transfer: [transferred.buffer]});
  check(same(baseline.bytes(), original), 'Transfer detaches only exported copy');
  const shared = new Uint8Array(new SharedArrayBuffer(original.length)); shared.set(original);
  const imported = f.schema.importSnapshot(document, 0n, 0n, shared); shared.fill(0);
  check(same(imported.bytes(), original), 'Shared input is copied before acceptance');
  fails(() => ManagedSnapshot.prototype.bytes.call(structuredClone(baseline)));
  fails(() => ManagedStore.prototype.read.call(structuredClone(store)));
  let escaped, draft;
  store.transact(edit => { escaped = edit; draft = edit.root; draft.label = 'first'; });
  draft.label = 'escaped'; fails(() => escaped.root); fails(() => escaped.token(draft));
  check(store.read().copyValue().label === 'first' && baseline.copyValue().label === 'baseline', 'Escaped drafts remain detached');
  const pin = store.read(); check(store.transact(() => {}) === pin, 'No-op preserves accepted pin');
  let ran = false; fails(() => store.transact(async () => { ran = true; })); check(!ran, 'Async rejected before invocation');
  fails(() => store.transact(function* () { yield 1; }));
  fails(() => store.transact(() => Promise.reject(new Error('detached rejection'))));
  fails(() => store.transact(() => ({then() { throw new Error('Must not execute thenable'); }})));
  fails(() => store.transact(edit => { edit.root.label = 'nested'; try { store.transact(() => {}); } catch {} }));
  fails(() => store.transact(() => { try { store.close(); } catch {} }));
  check(store.read() === pin, 'Caught nested operations poison acceptance');
  const sentinel = new Error('callback identity'); check(fails(() => store.transact(() => { throw sentinel; }), Error) === sentinel, 'Callback error preserved');
  fails(() => store.transact(edit => { edit.root.id = f.zero; }));
  fails(() => store.transact(edit => { edit.root.ordinary.leaves[0].id = one; }));
  const beforeBurn = store.allocatedId();
  fails(() => store.transact(edit => { edit.root.branches[0].leaves.push(f.leaf()); }, () => { throw new ManagedError('after reservation'); }));
  check(store.allocatedId() === beforeBurn + one && store.read() === pin, 'Reservation burns without publishing failed draft');
  store.transact(edit => { edit.root.branches[0].leaves.push(f.leaf()); });
  const newId = store.read().copyValue().branches[0].leaves[1].id;
  check(newId === beforeBurn + one + one, 'Burned identity not reused');
  store.undo(); check(store.read().copyValue().branches[0].leaves.length === 1, 'Undo restores owning graph');
  store.redo(); check(store.read().copyValue().branches[0].leaves[1].id === newId, 'Redo preserves identity');
  fails(() => store.transact(edit => { edit.root.branches[0].leaves.shift(); }));
  store.transact(edit => { edit.root.link = null; edit.root.branches[0].leaves.shift(); });
  fails(() => store.read().resolve(token, WireLeaf)); check(baseline.resolve(token, WireLeaf).text === 'leaf', 'Old immutable pin retains deleted target');

  const authority = new ManagedAuthority(store), first = new ManagedClient(f.schema, store.read(), 'first');
  const second = new ManagedClient(f.schema, store.read(), 'second');
  const proposal = first.propose('one', value => { value.label = 'authority'; value.branches[0].leaves.push(f.leaf()); });
  const stale = second.propose('stale', value => { value.label = 'stale'; });
  check(first.retry() === proposal, 'Retry retains exact request');
  const proposalBytes = proposal.bytes(); proposalBytes.fill(0); check(proposal.bytes()[0] !== 0, 'Proposal bytes copied');
  fails(() => first.propose('overlap', () => {}));
  const receipt = authority.accept(proposal); first.acknowledge(receipt);
  check(first.read().copyValue().label === 'authority', 'Client adopts accepted immutable outcome');
  check(authority.accept(proposal) === receipt, 'Authority exact retry retains original receipt');
  const conflict = authority.accept(stale); check(conflict.status === 'conflict', 'Stale proposal becomes retained conflict');
  second.acknowledge(conflict); check(second.read().copyValue().label === 'authority', 'Conflict imports latest proven pin');
  const changed = new ManagedProposal(proposal.clientId, proposal.requestId, document, f.schema.schemaId, proposal.baseEpoch, original);
  fails(() => authority.accept(changed));
  const highwater = store.allocatedId(), candidate = store.read().copyValue(); candidate.id = f.zero;
  const rootProposal = new ManagedProposal('root', 'replacement', document, f.schema.schemaId, store.read().epoch, f.codec.encode(candidate));
  fails(() => authority.accept(rootProposal)); check(store.allocatedId() === highwater, 'Root replacement rejected before reservation');
  const pending = first.propose('root-receipt', value => { value.label = 'pending'; });
  const foreignValue = store.read().copyValue(); foreignValue.id = highwater + one;
  const foreign = f.schema.importSnapshot(document, pending.baseEpoch + 1n, store.read().revision + 1n, f.codec.encode(foreignValue));
  fails(() => first.acknowledge(new ManagedReceipt(pending, 'accepted', foreign)));
  check(first.retry() === pending, 'Malformed root receipt preserves pending request');
  first.acknowledge(authority.accept(pending));
  fails(() => first.acknowledge(receipt));
  const equivalent = fixture(width); check(new ManagedClient(equivalent.schema, store.read(), 'equivalent').read().epoch === store.read().epoch, 'Equivalent descriptors transfer exact binding');
  fails(() => new ManagedClient(fixture(width === 4 ? 8 : 4).schema, store.read(), 'wrong-width'));
  await store.close(); fails(() => store.transact(() => {}), ManagedIndeterminate);

  const bounded = new ManagedStore(f.schema, f.initial(), document, new ManagedOptions({maxHistoryEntries: 2}));
  bounded.transact(edit => { edit.root.label = 'a'; }); bounded.transact(edit => { edit.root.label = 'b'; });
  bounded.undo(); fails(() => bounded.undo()); await bounded.close();
  const tiny = fixture(width, {maxEntities: 2}); fails(() => new ManagedStore(tiny.schema, tiny.initial(), document));
  const wrong = f.initial(); wrong.id = width === 4 ? 0n : 0; fails(() => new ManagedStore(f.schema, wrong, document));
  const aliases = f.initial(); aliases.branches[0].leaves.push(aliases.branches[0].leaves[0]);
  // Ordinary codecs may normalize aliases to independent owning values; accepted graphs still never share mutable storage.
  const normalized = new ManagedStore(f.schema, aliases, document);
  const normalizedValue = normalized.read().copyValue();
  check(normalizedValue.branches[0].leaves[0] !== normalizedValue.branches[0].leaves[1], 'Codec clone owns independent occurrences');
  await normalized.close();
  const wide = new ManagedStore(f.schema, f.initial(), document), internals = _mgStores.get(wide), initial = internals.state;
  const unsignedBoundary = width === 4 ? 0x7fffffff : (1n << 63n);
  internals.state = _mgState(unsignedBoundary, initial.epoch, initial.revision, initial.history, initial.cursor, initial.current, initial.receipts);
  wide.transact(edit => { edit.root.branches[0].leaves.push(f.leaf()); });
  check(wide.read().copyValue().branches[0].leaves[1].id === unsignedBoundary + one, 'High unsigned IDs retain exact native representation');
  const acceptedWide = wide.read(), current = internals.state, maximum = width === 4 ? 0xffffffff : ((1n << 64n) - 1n);
  internals.state = _mgState(maximum, current.epoch, current.revision, current.history, current.cursor, current.current, current.receipts);
  fails(() => wide.transact(edit => { edit.root.branches[0].leaves.push(f.leaf()); }));
  check(wide.read() === acceptedWide && wide.allocatedId() === maximum, 'ID exhaustion cannot wrap or publish'); await wide.close();
  const initialSize = f.codec.encode(f.initial()).length;
  fails(() => new ManagedStore(f.schema, f.initial(), document, new ManagedOptions({maxHistoryBytes: initialSize - 1})));
  const ledger = new ManagedStore(f.schema, f.initial(), document, new ManagedOptions({maxReceipts: 1}));
  const ledgerAuthority = new ManagedAuthority(ledger);
  const firstRequest = new ManagedClient(f.schema, ledger.read(), 'ledger-first').propose('one', value => { value.label = 'first'; });
  ledgerAuthority.accept(firstRequest);
  const secondRequest = new ManagedClient(f.schema, ledger.read(), 'ledger-second').propose('two', value => { value.label = 'second'; });
  const ledgerPin = ledger.read(); fails(() => ledgerAuthority.accept(secondRequest));
  check(ledger.read() === ledgerPin && ledgerAuthority.accept(firstRequest).proposal === firstRequest, 'Full ledger preserves exact prior retry'); await ledger.close();
}

/** Returned native promises from another realm must also be rejected without unhandled process failure. */
async function foreignCallbacks() {
  const f = fixture(8), store = new ManagedStore(f.schema, f.initial(), document), pin = store.read();
  const foreign = runInNewContext('Promise.reject(new Error("foreign draft rejection"))');
  fails(() => store.transact(() => foreign));
  const client = new ManagedClient(f.schema, pin, 'foreign');
  const foreignClient = runInNewContext('Promise.reject(new Error("foreign proposal rejection"))');
  fails(() => client.propose('one', () => foreignClient));
  await new Promise(resolve => setImmediate(resolve));
  check(store.read() === pin && client.read().epoch === pin.epoch, 'Cross-realm asynchronous results never publish');
  await store.close();
}

/** Hold a native writer at an externally observable process-death boundary, without running cleanup. */
function block(ready) { testFs.writeFileSync(ready, 'ready'); Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0); }
/** Construct one baseline with the same finite generated-shaped provider in every child process. */
function nativeFixture(width = 8, options = new ManagedOptions()) {
  const f = fixture(width); return {...f, options, store: () => new ManagedStore(f.schema, f.initial(), document, options)};
}

/** Qualify actual filesystem semantics, bounded replay and corruption handling on the supported Windows Node host. */
async function native(directory) {
  const unsupported = fixture(8);
  const networkError = await rejects(() => ManagedStore.openJournal(unsupported.schema, '\\\\serializer-invalid-host\\share\\state.srjsmg'));
  check(networkError.message.includes('local Windows drive'), 'UNC paths rejected before network filesystem access');
  const transitioning = nativeFixture(), acquiring = transitioning.store(), proven = acquiring.read();
  const transitionPath = testPath.join(directory, 'overlapping-acquisition.srjsmg');
  const opening = acquiring.createJournal(transitionPath);
  check(acquiring.read() === proven, 'Reads retain a proven pin during asynchronous native acquisition');
  fails(() => acquiring.transact(() => {})); fails(() => acquiring.close());
  await rejects(() => opening);
  check(acquiring.read() === proven && testFs.statSync(transitionPath).size === 0, 'Caught overlap aborts acquisition before a journal frame is written');
  acquiring.transact(edit => { edit.root.label = 'memory-after-abort'; });
  check(acquiring.read().copyValue().label === 'memory-after-abort', 'Failed acquisition releases its leases without fencing unmodified memory state');
  await acquiring.close();
  for (const width of [4, 8]) {
    const f = nativeFixture(width), path = testPath.join(directory, `journal-${width}.srjsmg`);
    const store = f.store(); await store.createJournal(path);
    await rejects(() => ManagedStore.openJournal(f.schema, path));
    const alias = testPath.join(directory, `alias-${width}.srjsmg`); testFs.linkSync(path, alias);
    await rejects(() => ManagedStore.openJournal(f.schema, alias));
    let count = 0; const client = new ManagedClient(f.schema, store.read(), 'durable');
    const proposal = client.propose('exact', value => { ++count; value.label = 'accepted'; });
    const receipt = new ManagedAuthority(store).accept(proposal); client.acknowledge(receipt);
    const accepted = store.read().bytes();
    fails(() => store.transact(edit => { edit.root.branches[0].leaves.push(f.leaf()); }, () => { throw new ManagedError('burn'); }));
    const burned = store.allocatedId(); await store.close();
    let forced = 0;
    const reopened = await _mgRecoverStore(_mgSchemas.get(f.schema), path, f.options, phase => { if (phase === 'RECOVERY_AFTER_FORCE') ++forced; });
    check(forced === 1, 'Complete generation forced before exposing replay');
    check(same(reopened.read().bytes(), accepted) && reopened.allocatedId() === burned, 'State and reservation recovered');
    const retry = new ManagedAuthority(reopened).accept(proposal);
    check(count === 1 && same(retry.snapshot.bytes(), receipt.snapshot.bytes()), 'Exact durable retry survives reopen');
    await reopened.close();
    const original = testFs.readFileSync(path); testFs.appendFileSync(path, new Uint8Array([4, 2, 1]));
    const recoveredTail = await ManagedStore.openJournal(f.schema, alias);
    check(testFs.statSync(path).size === original.length, 'Tail repair operates under native file-object lease'); await recoveredTail.close();
    testFs.unlinkSync(alias);
    const forceFailure = testPath.join(directory, `recovery-force-${width}.srjsmg`); testFs.copyFileSync(path, forceFailure);
    await rejects(() => _mgRecoverStore(_mgSchemas.get(f.schema), forceFailure, f.options, phase => {
      if (phase === 'RECOVERY_BEFORE_FORCE') throw new Error('Recovery force injection');
    }), ManagedIndeterminate);
    check(same(testFs.readFileSync(forceFailure), original), 'Failed recovery force does not expose an unforced store');
    for (const position of [8, 16, 48, original.length - 1]) {
      const corrupt = testPath.join(directory, `corrupt-${width}-${position}.srjsmg`), data = Uint8Array.from(original);
      data[position] ^= 1; testFs.writeFileSync(corrupt, data); testFs.appendFileSync(corrupt, new Uint8Array([9, 8]));
      const before = testFs.readFileSync(corrupt); await rejects(() => ManagedStore.openJournal(f.schema, corrupt));
      check(same(testFs.readFileSync(corrupt), before), 'Invalid complete frame never repairs a later tail');
    }
    const bounded = testPath.join(directory, `bounded-${width}.srjsmg`), options = new ManagedOptions({maxJournalRecords: 1});
    const limited = new ManagedStore(f.schema, f.initial(), document, options); await limited.createJournal(bounded);
    const old = limited.read(); fails(() => limited.transact(edit => { edit.root.label = 'capacity'; }));
    check(limited.read() === old, 'Journal record admission preserves accepted state'); await limited.close();
    const uncertain = testPath.join(directory, `uncertain-${width}.srjsmg`), fenced = f.store(); await fenced.createJournal(uncertain);
    const proven = fenced.read(), journal = _mgStores.get(fenced).journal;
    journal.observer = phase => { if (phase === 'AFTER_FORCE') throw new Error('publication interruption'); };
    fails(() => fenced.transact(edit => { edit.root.label = 'forced'; }), ManagedIndeterminate);
    check(fenced.read() === proven, 'Failed publication retains proven read pin'); fails(() => fenced.transact(() => {}), ManagedIndeterminate);
    await fenced.close(); const confirmed = await ManagedStore.openJournal(f.schema, uncertain);
    check(confirmed.read().copyValue().label === 'forced', 'Forced uncertain generation recovered'); await confirmed.close();

    const forgedPath = testPath.join(directory, `forged-root-${width}.srjsmg`), forgedStore = f.store(); await forgedStore.createJournal(forgedPath);
    const state = _mgStores.get(forgedStore), oldState = state.state, value = forgedStore.read().copyValue();
    const nextId = oldState.highwater + (width === 4 ? 1 : 1n); _mgReserve(state, nextId); value.id = nextId;
    const pin = _mgSnapshot(state.schema, value, document, nextId, 1n, 1n);
    const malicious = _mgState(nextId, 1n, 1n, [...oldState.history, pin], 1, pin, new Map());
    state.journal.append('accept', _mgEncodeState(state, malicious, state.journal.host)); await forgedStore.close();
    testFs.appendFileSync(forgedPath, new Uint8Array([1, 2, 3])); const forgedBytes = testFs.readFileSync(forgedPath);
    await rejects(() => ManagedStore.openJournal(f.schema, forgedPath));
    check(same(forgedBytes, testFs.readFileSync(forgedPath)), 'Semantic root rejection precedes tail repair');
  }
}

/** Verify that a worker can own a transferred copy but cannot acquire mutable aliases into an accepted isolate. */
async function workerOwnership() {
  const f = fixture(8), store = new ManagedStore(f.schema, f.initial(), document), original = store.read().bytes();
  const copy = store.read().bytes();
  const worker = new Worker(`const {parentPort} = require('node:worker_threads'); parentPort.once('message', bytes => {
    bytes.fill(0); parentPort.postMessage(bytes.byteLength); });`, {eval: true});
  try {
    const length = await new Promise((resolve, reject) => { worker.once('message', resolve); worker.once('error', reject); worker.postMessage(copy, [copy.buffer]); });
    check(copy.byteLength === 0 && length === original.length && same(store.read().bytes(), original), 'Worker transfer changes only owned copy');
  } finally { await worker.terminate(); await store.close(); }
}

/** Execute one external-process mode, or the complete standalone kernel fixture. */
async function main() {
  const [mode, path, argument, ready] = process.argv.slice(2);
  if (mode?.startsWith('--')) {
    const f = nativeFixture(8);
    if (mode === '--seed') { const store = f.store(); await store.createJournal(path); await store.close(); return; }
    if (mode === '--locked') { await rejects(() => ManagedStore.openJournal(f.schema, path)); return; }
    if (mode === '--hold-writer') { await ManagedStore.openJournal(f.schema, path); block(argument); return; }
    if (mode === '--crash') {
      const store = await ManagedStore.openJournal(f.schema, path);
      _mgStores.get(store).journal.observer = phase => { if (phase === argument) block(ready); };
      store.transact(edit => { edit.root.label = 'crashed-generation'; }); throw new Error('Crash boundary was not observed');
    }
    if (mode === '--verify-crash') {
      const store = await ManagedStore.openJournal(f.schema, path);
      const complete = ['AFTER_CHECKSUM', 'AFTER_FORCE'].includes(argument);
      check(store.read().copyValue().label === (complete ? 'crashed-generation' : 'baseline'), 'Crash recovered only complete generations');
      store.transact(edit => { edit.root.label = 'writer-reopened'; }); await store.close(); return;
    }
    throw new Error('Unknown child mode');
  }
  for (const input of [null, 5, false, [], {unknown: 1}, {maxHistoryBytes: NaN}, {maxReceipts: true}, {maxFrameBytes: 2n}]) fails(() => new ManagedOptions(input));
  fails(() => new ManagedDocument(1, 2n)); fails(() => new ManagedDocument(0n, 0n));
  fails(() => new ManagedToken(document, 1, 1)); fails(() => new ManagedToken(document, 1n, Number.MAX_SAFE_INTEGER));
  fails(() => new ManagedProposal('bad\ud800', 'request', document, 'schema', 0n, new Uint8Array()));
  const f = fixture(8); fails(() => new ManagedStore(f.schema, f.initial(), document, Object.create(ManagedOptions.prototype)));
  await memory(4); await memory(8); await foreignCallbacks(); await workerOwnership();
  if (process.platform === 'win32') { await native(mode); }
  else {
    const unavailable = await rejects(() => ManagedStore.openJournal(f.schema, 'unused-native-journal'));
    check(unavailable.message.includes('Windows'), 'Unavailable journal profile rejects before filesystem access');
    console.log(`Native Windows journal campaign unavailable on ${process.platform}; memory contracts executed`);
  }
  console.log(`Standalone native JavaScript kernel checks: ${checks}`);
}
await main();
