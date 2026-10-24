// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Compile against actual matching declarations and execute against the generated ESM module.
import {EngineGraphModel, EngineGraphItem, ManagedDocument, ManagedStore,
  ManagedOptions, ManagedSchema, ManagedReference, ManagedToken, ManagedSnapshot,
  ManagedClient, ManagedAuthority, managedSchema, managedReference, managedToken} from './model.mjs';

type Id = @ID_TYPE@;
const schema: ManagedSchema<EngineGraphModel, Id> = managedSchema(EngineGraphModel);
const initial = new EngineGraphModel(); initial.children = [new EngineGraphItem()];
const store = new ManagedStore(schema, initial, new ManagedDocument(7n, 8n), new ManagedOptions(), edit => {
  const token: ManagedToken<EngineGraphItem, Id> = edit.token(edit.root.children[0]);
  const reference: ManagedReference<EngineGraphItem, Id> = managedReference(token);
  edit.root.selected = [reference];
});
const id: Id = store.allocatedId();
const pin: ManagedSnapshot<EngineGraphModel, Id> = store.read();
const selected = pin.copyValue().selected;
if (selected.length !== 1) throw new Error('Typed reference was not initialized');
const token: ManagedToken<EngineGraphItem, Id> = managedToken(selected[0]);
const item: EngineGraphItem = pin.resolve(token, EngineGraphItem);
if (item.persistentId === (@ZERO@) || id === (@ZERO@)) throw new Error('Typed ID round trip failed');
store.transact(edit => { edit.root.title = 'typed'; });
const client = new ManagedClient(schema, store.read(), 'typed-client');
const request = client.propose('typed-request', value => { value.title = 'typed-authority'; });
client.acknowledge(new ManagedAuthority(store).accept(request));
if (client.read().copyValue().title !== 'typed-authority') throw new Error('Typed authority round trip failed');
await store.close();
console.log('Actual generated TypeScript SDK: strict generic ownership/identity/receipt checks PASS');
