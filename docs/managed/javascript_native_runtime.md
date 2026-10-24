# Native JavaScript and TypeScript managed SDK

Compiler/runtime **2.34.0** provides explicit native owning JavaScript and
TypeScript SDK profiles. Windows qualification passes 384 actual generated
runtime checks across eight profiles, 96 strict TypeScript rejection controls,
eight typed SDK executions and the recorded native crash/alias campaigns. See
the [executed evidence](../verification/verification-javascript-managed-runtime-2026-10-24.md).
MG01 remains open in the [central backlog](../pending.md).

Select the ESM implementation and its matching TypeScript declarations explicitly:

```ini
[js]
managed_engine = true
[typescript]
managed_engine = true
[managed]
id_type = uint64
id_encoding = compact_varint
```

Generate matching `model.mjs` and `model.d.mts` artifacts from the same schema,
identity settings and protocol selection. For example, pass
`--js.managed_engine true` for JavaScript and
`--typescript.managed_engine true` for the declaration output. The TypeScript
target supplies a typed SDK for the paired JavaScript module; both execute the
same native ESM engine. No C++ service or runtime bridge is involved.

Ordinary generation still rejects managed declarations by default. The separate
`managed_token_codecs` switches emit only identity fields and typed wire
envelopes; they do not activate a state engine. Engine selection implies token
lowering. This profile requires direct finite concrete owning public records.
Inheritance, generic definitions, behavior, variants, transient/lifecycle fields,
indexes, caches, computed exposure, magic, XML/packages and separate-value storage
retain explicit rejection. Existing ordinary codecs remain independent.

## Ownership and editing

For the maintained [schema fixture](../../test/resources/javascript_managed_engine.serializer):

```javascript
import {EngineGraphModel, EngineGraphItem, ManagedDocument, ManagedOptions,
  ManagedStore, managedSchema, managedReference} from './model.mjs';

const initial = new EngineGraphModel();
initial.children = [new EngineGraphItem()];
const store = new ManagedStore(managedSchema(EngineGraphModel), initial,
  new ManagedDocument(17n, 18n), new ManagedOptions(), edit => {
    edit.root.selected = [managedReference(edit.token(edit.root.children[0]))];
  });
const before = store.read();
store.transact(edit => { edit.root.title = 'Accepted'; });
const detached = before.copyValue();
await store.close();
```

Accepted snapshots retain private owned canonical bytes. `bytes()` returns a
copy and `copyValue()` decodes a detached graph. Old snapshots survive edits,
history pruning and store closure. Escaped draft objects cannot change accepted
bytes, and an escaped `ManagedEdit` rejects access after its callback ends.

Callbacks are synchronous and return `undefined`. Async/generator functions,
returned promises and other returned values reject; caught nested mutations
poison the candidate. TypeScript uses an explicit `undefined` callback result,
since a `void` callback would admit async functions. Opaque handles and generated
record targets have private declaration brands, including distinct classes with
identical field layouts. Runtime ownership remains checked independently.

Object IDs are exact Numbers for uint32 and BigInts for uint64. Document halves,
class tags, epochs and revisions are BigInts. Optional references use the native
codec's empty/single-element array representation. Post-ID initialization can
mint references through `edit.token`; required references must already survive
the initial codec clone. Tokens are nonowning coordinates, and resolution checks
the selected document, class and entity membership.

IDs allocated before a later callback failure remain burned. Bounded linear
history preserves exact IDs through Undo/Redo and does not rewind the allocation
watermark. Canonical snapshots use integer-key binary for `protocols = all` and
positional binary for `protocols = binary_none`. Schema bindings include the
selected identity/protocol profile.

## Journals and authority acceptance

Call `await store.createJournal(path)` before accepting edits or authority
requests. Reopen with `await ManagedStore.openJournal(schema, path, options)`;
close with `await store.close()`. Journal attachment/release acquires native
leases asynchronously; callbacks and accepted record writes remain synchronous.
The memory module has no eager Node imports. Native journal services load only
when requested and currently require Windows Node on a local volume.

Cooperating same-host Node writers use canonical-name and native-file-object
named-pipe leases, including hard-link aliases. UNC/device paths reject before
filesystem access. A drive-letter path does not prove local media: mapped or
network volumes are unsupported and are not universally detected automatically.
No cross-host exclusion or remote authority lease is provided.

Versioned checksummed records retain bounded complete history and exact retry
outcomes. A successful append forces the file before publication. Complete-frame
corruption and invalid semantic transitions reject; an incomplete suffix may be
repaired only after all complete states validate. Uncertain writes fence the
store until reopen. File creation/directory durability and physical power-loss
guarantees require separate device qualification.

`ManagedClient` owns one immutable pending proposal. `ManagedAuthority.accept`
checks binding, expected epoch and root lifetime, and retains exact accepted or
conflict receipts. Reusing request identity with different bytes rejects;
acknowledgement clears a pending request only after its receipt validates.
These are full-state proposals, not contribution-aware collaborative Undo/Redo.
Authentication, authorization policy and network transport remain host work.

There is no tree/labeled history, native journal migration/rebaseline/compaction,
C++ Save/journal interoperability or heap/RSS ceiling in this profile. Configured
bounds apply to snapshots, entities, retained history, receipts and journal
bytes/records. Native filesystem tests do not establish cloud or power-failure
durability. Node-free VM checks are not an installed-browser qualification.
