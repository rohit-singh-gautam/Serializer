// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// This dependency-isolation proof uses Node's VM; it is not a browser SDK certification.
import {SourceTextModule, createContext} from 'node:vm';
import {readFileSync} from 'node:fs';
import {TextEncoder, TextDecoder} from 'node:util';

const source = readFileSync(process.argv[2], 'utf8');
const context = createContext({TextEncoder, TextDecoder, Uint8Array, console});
const exercise = `
let checks = 0;
function assert(value) { ++checks; if (!value) throw new Error('Generated Node-free memory check ' + checks); }
assert(typeof process === 'undefined' && typeof Buffer === 'undefined' && typeof require === 'undefined');
const schema = managedSchema(EngineGraphModel), initial = new EngineGraphModel();
initial.children = [new EngineGraphItem()];
const store = new ManagedStore(schema, initial, new ManagedDocument(5n, 6n), new ManagedOptions(), edit => {
  edit.root.selected = [managedReference(edit.token(edit.root.children[0]))];
});
const baseline = store.read(), copy = baseline.copyValue();
copy.title = 'outside'; assert(baseline.copyValue().title === '');
assert(baseline.resolve(managedToken(copy.selected[0]), EngineGraphItem).persistentId === copy.children[0].persistentId);
store.transact(edit => { edit.root.title = 'accepted'; });
assert(store.read().copyValue().title === 'accepted' && baseline.copyValue().title === '');
store.undo(); assert(store.read().copyValue().title === '');
store.redo(); assert(store.read().copyValue().title === 'accepted');
const client = new ManagedClient(schema, store.read(), 'isolated');
const request = client.propose('one', value => { value.title = 'authority'; });
client.acknowledge(new ManagedAuthority(store).accept(request));
assert(client.read().copyValue().title === 'authority');
await store.close();
console.log('Actual generated Node-free VM memory checks: ' + checks);
`;
const module = new SourceTextModule(source + exercise, {
  context,
  /** Every import would violate the explicitly selected memory-only module contract. */
  importModuleDynamically() { throw new Error('Generated memory module attempted a host import'); }
});
await module.link(() => { throw new Error('Generated memory module attempted a static import'); });
await module.evaluate();
