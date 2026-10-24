// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Import actual compiler outputs; no runtime insertion or fixture codecs.
import * as sdk from './model.mjs';
import * as fs from 'node:fs';
import * as path from 'node:path';

const {EngineGraphModel: Model, EngineGraphItem: Item, EngineGraphBranch: Branch,
  ManagedStore, ManagedOptions, ManagedDocument, ManagedError, ManagedClient,
  ManagedAuthority, ManagedReference, ManagedProposal, ManagedReceipt, managedSchema,
  managedReference, managedToken, Protocol} = sdk;
const [directory, widthText, protocolText, mode, ready] = process.argv.slice(2);
const width = Number(widthText), zero = width === 32 ? 0 : 0n, one = width === 32 ? 1 : 1n;
const protocol = Protocol[protocolText], document = new ManagedDocument(17n, 18n);
let checks = 0;
/** Keep assertions active independently of runtime optimization. */
function check(value, label) { ++checks; if (!value) throw new Error(`Generated JS check ${checks}: ${label}`); }
/** Preserve unexpected errors instead of hiding them behind a broad failure expectation. */
function reject(action, type = ManagedError) {
  let error; try { action(); } catch (caught) { error = caught; }
  check(error instanceof type, `Expected ${type.name}; got ${error?.stack}`);
}
/** Confirm owned byte equality without involving generated codecs. */
function equal(first, second) { return first.length === second.length && first.every((value, index) => value === second[index]); }
/** Construct actual generated storage spanning owning containers, ordinary boundaries and scalars. */
function initial() {
  const value = new Model(); value.title = 'baseline';
  value.children = [new Item()]; value.children[0].label = 'owned';
  value.mapped = new Map([[20, new Item()], [3, new Item()]]);
  value.ordinary.label = 'ordinary'; value.ordinaryBranch.leaves = [new Item()];
  value.branches = [new Branch()]; value.branches[0].leaves = [new Item()];
  value.fixed = ['alpha', '\u03b2']; value.note = ['note']; value.mode = sdk.EngineGraphKind.Second;
  value.precise = 7.5; value.ratio = 0.25; value.enabled = true; value.balance = -19n;
  value.fingerprint[0] = 255; return value;
}
const schema = managedSchema(Model);
/** Use the actual factory and codecs for either native identity representation. */
function store(options = new ManagedOptions()) {
  return new ManagedStore(schema, initial(), document, options, edit => {
    const reference = managedReference(edit.token(edit.root.children[0]));
    edit.root.selected = [reference]; edit.root.children[0].next = [reference];
  });
}

/** Exercise immutable ownership, logical identity, typed references, navigation and client acceptance. */
async function ownership() {
  const {EngineIdentityFirst: First, EngineIdentitySecond: Second, EngineIdentityHolder: Holder} = sdk;
  const identity = new ManagedStore(managedSchema(Holder), new Holder(), document, new ManagedOptions(), edit => {
    edit.root.selected = [managedReference(edit.token(edit.root.owned))];
  });
  const identityToken = managedToken(identity.read().copyValue().selected[0]);
  check(identity.read().resolve(identityToken, First).value === 0, 'Same-layout exact named target resolves');
  reject(() => identity.read().resolve(identityToken, Second));
  reject(() => identity.transact(edit => {
    edit.root.selected = [new ManagedReference(17n, 18n, Second.MANAGED_CLASS_ID, identityToken.objectId)];
  }));
  await identity.close();
  const input = initial(), engine = new ManagedStore(schema, input, document, new ManagedOptions(), edit => {
    const reference = managedReference(edit.token(edit.root.children[0]));
    edit.root.selected = [reference]; edit.root.children[0].next = [reference];
  });
  const baseline = engine.read(), copy = baseline.copyValue(); input.children[0].label = 'caller';
  check(copy.title === 'baseline' && copy.children[0].label === 'owned', 'Owned initial clone');
  check(copy.persistentId === one, 'Exact preorder root ID');
  check(copy.children[0].persistentId !== zero && copy.mapped.get(20).persistentId !== zero, 'Containers participate');
  check(copy.ordinary.persistentId === zero && copy.ordinaryBranch.persistentId === zero &&
    copy.ordinaryBranch.leaves[0].persistentId === zero, 'Ordinary ancestor suppresses descendants');
  check(copy.branches[0].persistentId !== zero && copy.branches[0].leaves[0].persistentId !== zero, 'Nested managed ancestry');
  check([...copy.mapped.keys()].join(',') === '3,20', 'Canonical native map ordering');
  check(copy.fixed[1] === '\u03b2' && copy.note[0] === 'note' && copy.enabled, 'Ordinary wrapper/scalar round trip');
  check(copy.precise === 7.5 && copy.ratio === 0.25 && copy.balance === -19n && copy.fingerprint[0] === 255, 'Native numeric/digest round trip');
  check(Item.MANAGED_CLASS_ID === 0xffffffffffffffffn, 'Unsigned class identity');
  const token = managedToken(copy.selected[0]); check(token.classId === Item.MANAGED_CLASS_ID, 'Typed wire conversion');
  check(baseline.resolve(token, Item).persistentId === token.objectId, 'Typed current resolution');
  check(managedToken(baseline.resolve(token, Item).next[0]).objectId === token.objectId, 'Logical self edges do not become owning cycles');
  reject(() => baseline.resolve(token, Branch));
  copy.title = 'copy'; copy.fingerprint[0] = 0; const bytes = baseline.bytes(); bytes.fill(0);
  check(baseline.copyValue().title === 'baseline' && baseline.copyValue().fingerprint[0] === 255, 'Read copies never alias accepted bytes');
  let escaped, draft;
  engine.transact(edit => { escaped = edit; draft = edit.root; draft.title = 'accepted'; }); draft.title = 'escaped';
  reject(() => escaped.root); check(engine.read().copyValue().title === 'accepted', 'Draft lifetime ends after callback');
  const pin = engine.read(); check(engine.transact(() => {}) === pin, 'No-op pin preserved');
  reject(() => engine.transact(async () => {})); reject(() => engine.transact(() => Promise.resolve()));
  reject(() => engine.transact(edit => { try { engine.transact(() => {}); } catch {} edit.root.title = 'nested'; }));
  check(engine.read() === pin, 'Nested mutation cannot publish');
  reject(() => engine.transact(edit => { edit.root.persistentId = zero; }));
  reject(() => engine.transact(edit => { edit.root.ordinary.persistentId = one; }));
  reject(() => engine.transact(edit => { edit.root.children.push(edit.root.children[0]); }));
  reject(() => engine.transact(edit => { edit.root.selected = [new ManagedReference(17n, 18n, Branch.MANAGED_CLASS_ID, token.objectId)]; }), ManagedError);
  const beforeBurn = engine.allocatedId();
  reject(() => engine.transact(edit => { edit.root.children.push(new Item()); }, () => { throw new ManagedError('burn'); }));
  check(engine.allocatedId() === beforeBurn + one && engine.read() === pin, 'Post-ID failure burns durable reservation');
  engine.transact(edit => { edit.root.children.push(new Item()); });
  const id = engine.read().copyValue().children[1].persistentId;
  check(id === beforeBurn + one + one, 'Burned ID never reused'); engine.undo();
  check(engine.read().copyValue().children.length === 1, 'Undo restores old graph'); engine.redo();
  check(engine.read().copyValue().children[1].persistentId === id, 'Redo restores exact lifetime');
  reject(() => engine.transact(edit => { edit.root.children.shift(); }));
  engine.transact(edit => { edit.root.selected = []; edit.root.children.shift(); });
  reject(() => engine.read().resolve(token, Item)); check(baseline.resolve(token, Item).label === 'owned', 'Old pin retains deleted lifetime');
  const authority = new ManagedAuthority(engine), client = new ManagedClient(schema, engine.read(), 'client');
  const other = new ManagedClient(schema, engine.read(), 'other');
  const proposal = client.propose('one', value => { value.title = 'authority'; value.children.push(new Item()); });
  const stale = other.propose('stale', value => { value.title = 'stale'; });
  const receipt = authority.accept(proposal); client.acknowledge(receipt);
  check(authority.accept(proposal) === receipt && client.read().copyValue().title === 'authority', 'Exact accepted receipt');
  const conflict = authority.accept(stale); other.acknowledge(conflict);
  check(conflict.status === 'conflict' && other.read().copyValue().title === 'authority', 'Conflict imports proven generation');
  const allocation = engine.allocatedId(), candidate = engine.read().copyValue(); candidate.persistentId = zero;
  reject(() => authority.accept(new ManagedProposal('root', 'replace', document, schema.schemaId, engine.read().epoch, candidate.encode(protocol))));
  check(engine.allocatedId() === allocation, 'Root lifetime checked before reservation');
  const pending = client.propose('receipt-root', value => { value.title = 'pending'; });
  const replaced = engine.read().copyValue(); replaced.persistentId = allocation + one;
  const imported = schema.importSnapshot(document, pending.baseEpoch + 1n, engine.read().revision + 1n, replaced.encode(protocol));
  reject(() => client.acknowledge(new ManagedReceipt(pending, 'accepted', imported)));
  check(client.retry() === pending, 'Invalid root receipt preserves exact pending request');
  client.acknowledge(authority.accept(pending)); await engine.close();
  const bounded = store(new ManagedOptions({maxHistoryEntries: 2}));
  bounded.transact(edit => { edit.root.title = 'a'; }); bounded.transact(edit => { edit.root.title = 'b'; });
  bounded.undo(); reject(() => bounded.undo()); await bounded.close();
  reject(() => managedSchema(Item, {maxEntities: 0}));
  reject(() => new ManagedStore(managedSchema(Model, {maxEntities: 2}), initial(), document));
}

/** Native generated replay preserves original acceptance and reservation outcomes under actual OS leases. */
async function durable() {
  const journalPath = path.join(directory, 'generated.srjsmg'), engine = store(); await engine.createJournal(journalPath);
  const client = new ManagedClient(schema, engine.read(), 'durable');
  const proposal = client.propose('exact', value => { value.title = 'durable'; });
  const receipt = new ManagedAuthority(engine).accept(proposal); client.acknowledge(receipt);
  reject(() => engine.transact(edit => { edit.root.children.push(new Item()); }, () => { throw new ManagedError('burn'); }));
  const burned = engine.allocatedId(), bytes = engine.read().bytes(); await engine.close();
  fs.appendFileSync(journalPath, Uint8Array.of(8, 9));
  const recovered = await ManagedStore.openJournal(schema, journalPath);
  check(equal(recovered.read().bytes(), bytes) && recovered.allocatedId() === burned, 'Native graph/reservation replay');
  const originalReceipt = new ManagedAuthority(recovered).accept(proposal);
  check(equal(originalReceipt.snapshot.bytes(), receipt.snapshot.bytes()), 'Exact retry survives native recovery');
  recovered.transact(edit => { edit.root.title = 'next'; }); await recovered.close();
}

if (mode === '--hold') {
  const engine = store(); await engine.createJournal(path.join(directory, 'child.srjsmg'));
  engine.transact(edit => { edit.root.title = 'child-accepted'; }); fs.writeFileSync(ready, 'ready');
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0);
} else if (mode === '--recover-child') {
  const engine = await ManagedStore.openJournal(schema, path.join(directory, 'child.srjsmg'));
  check(engine.read().copyValue().title === 'child-accepted', 'Actual generated journal recovers after process death'); await engine.close();
} else {
  await ownership();
  if (process.platform === 'win32') { await durable(); }
  else {
    let unavailable;
    try { await ManagedStore.openJournal(schema, 'unused-native-journal'); } catch (error) { unavailable = error; }
    check(unavailable instanceof ManagedError && unavailable.message.includes('Windows'), 'Explicit unavailable journal profile');
    console.log(`Native Windows journal profile unavailable on ${process.platform}; owning memory controls executed`);
  }
  console.log(`Actual generated JavaScript engine checks: ${checks}`);
}
