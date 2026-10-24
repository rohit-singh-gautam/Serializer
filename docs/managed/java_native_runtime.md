# Native Java managed runtime

Compiler/runtime **2.34.0** provides an explicit concrete owning Java managed
engine profile. It is independent of `java.managed_token_codecs`: token-only
generation supplies wire records, while engine selection adds ownership,
transactions, immutable snapshot pins, identities, bounded linear history,
journals and authority/client acceptance.

```ini
[java]
managed_engine = true
[managed]
id_type = uint64
id_encoding = compact_varint
```

Default Java output remains ordinary owning codecs. Engine selection implies
token lowering and requires direct, unpacked, finite concrete owning records.
Inheritance, generic definitions, behavior, indexes, caches, lifecycle fields,
computed/exposed helpers, XML, packages and separated-value storage require their
own contracts and reject in this profile. Even unrelated packed classes reject
when the engine is selected; ordinary Java packed output remains available
independently. Required reference fields need a wire-valid initial value before
the first codec clone; default zero envelopes cannot be initialized afterward.
Optional references support post-identity initialization.

## Ownership and editing

The generated enclosing class supplies `managedSchema(Root.class)` and
`ManagedStore<Root>`. For the maintained
[generated SDK fixture](../../test/resources/java_managed_engine.serializer):

```java
var schema = EngineSdk.managedSchema(EngineSdk.EngineGraph.Model.class);
try (var store = new EngineSdk.ManagedStore<>(schema,
    new EngineSdk.EngineGraph.Model(), new EngineSdk.ManagedDocument(17, 18),
    EngineSdk.ManagedOptions.defaults())) {
  var before = store.read();
  store.transact(edit -> edit.root().title = "Accepted");
  var detached = before.copyValue();
}
```

An accepted snapshot owns private canonical bytes. `bytes()` returns a copy;
`copyValue()` decodes a detached mutable graph. Neither operation lends accepted
mutable fields. Old pins survive edits, history pruning and store closure.
Accepted maps normalize through the generated decoder before their final encode,
including caller-supplied custom comparators.

The store owns its creating `Thread`. Mutations, journal operations and close
check that exact thread; synchronous callbacks reject reentrancy. Drafts belong
to the callback. Retaining an ordinary draft cannot mutate accepted bytes, and
an escaped `ManagedEdit` rejects after callback completion. Exceptions preserve
the accepted state; caught nested-operation failures poison the candidate.

The engine assigns monotonic IDs only to independently managed occurrences.
Ordinary ancestor boundaries suppress participation below them. IDs are never
derived from map keys and are not reused after failed reservations, deletion or
Undo/Redo. The root identity remains fixed for its document namespace. Unsigned
uint32 IDs use zero-extended `long` values at runtime; uint64 IDs retain all bits
in Java `long` and use unsigned comparisons where required.

Use the optional initializer of `ManagedStore` or `transact` to populate new
references after IDs are assigned. `edit.token(value)` works only in that phase.
Typed resolution checks document, exact class tag and live identity, then returns
an entity in a detached decoded graph. Tokens establish no authorization.

## Journals, history and collaboration

`undo()` and `redo()` navigate bounded linear history. `ManagedOptions` bounds
history entries/bytes, retained receipts, frame bytes, total journal bytes and
record count. `managedSchema(Root.class, maxSnapshotBytes, maxEntities)` bounds
snapshot encoding and ownership traversal. These are library admission limits,
not a hard JVM heap or process-RSS ceiling. Operations process complete roots.

`createJournal(path)` requires a new file; `ManagedStore.openJournal(...)`
reopens an existing generation. A stable companion lock serializes the name;
an additional native data-file lock excludes writers through hard-link aliases.
Existing names resolve to their real paths. Process termination releases locks.
The checksummed, bounded journal is a Java engine storage format, separate from
C++ and Python journals. Writes are forced before publication. Uncertain writes
fence mutation until reopen; complete recovered generations are forced before
their outcomes become visible. Recovery validates schema, identities, root
lifetime, history, receipt semantics and canonical bytes before tail repair.
Corrupt complete records reject without truncating the file.

`ManagedAuthority` accepts exact full-state `ManagedProposal` requests against
document, schema and acceptance epoch. Accepted and stale-base conflict receipts
persist for exact retries. `ManagedClient` retains one confirmed immutable pin
and at most one pending request. Retrying returns that exact request without
rerunning edits. Invalid receipts leave the pending request and confirmed pin
unchanged. Conflicts require explicit subsequent editing; no transport,
authentication, automatic merge or rebase is supplied. A client proposal cannot
reserve authoritative IDs or create forward references to unassigned entities.

Compaction, native journal migration/rebaseline, branching history and qualified
machine/power-failure durability remain outside this profile. File creation is
qualified for process-crash recovery; broader filesystem/provider behavior and
non-Windows execution require separate replay.

## Executed scope

The combined compiler/backend Windows replay passes **647 core cases**,
**188 standalone Java runtime checks**, six separate-JVM crash/reopen boundaries,
writer exclusion through ordinary and hard-link paths, and **528 actual generated
SDK checks**. The generated matrix combines uint32/uint64, fixed/compact identity
encoding, and canonical positional/integer-key binary: eight profiles, 66 checks
each. Java 17 compilation uses `--release 17 -Xlint:all -Werror`.

Separate token exchange passes 4,953 checks with four independent C++ profiles.
Java stream/packed and caller-owned buffer controls also pass. These results
qualify this concrete profile, not completion of MG01 across every non-C++ output.
See the [language verification record](../verification/verification-managed-languages-2026-10-24.md).
