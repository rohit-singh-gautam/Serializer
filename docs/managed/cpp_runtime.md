# C++ managed interface

Status: implemented C++ schema generation and local snapshot history. Bare
`managed` class/member declarations generate storage, conversions, identity
traversal, and typed transaction editors. Collaboration, authorization, journal,
feature selectors, exclusions, notifications, and other-language managed runtimes
remain proposals. Unsupported selectors/backends are rejected rather than ignored.

For a step-by-step introduction using only a point with x and y, start with the
[eight point examples](../../example/managed/README.md). Each generates a managed
point from a schema and demonstrates the actual store, callbacks, generated editor,
or transaction lifecycle.

For a source-level explanation of `model_store`, its nested transaction types,
and the setter-to-candidate call path, see
[how managed transactions work](../internals/managed_transactions.md).

## Build and include

Configure with `-DSERIALIZER_BUILD_MANAGED=ON` (default OFF), link
`Serializer::managed`, and generate the application schema normally in C++20:

```cmake
set(SERIALIZER_BUILD_MANAGED ON CACHE BOOL "Build managed support")
add_subdirectory(path/to/Serializer serializer-build)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE Serializer::managed)
serializer_generate(TARGET application SCHEMAS model.serializer)
```

The optional target generates its envelope classes from
[managed_records.serializer](../../schemas/managed_records.serializer) after the
compiler builds. It requires the normal native generator/clang-format setup; a
separate cross-compilation host generator override is not provided for this target.
Installed packages built with this option export the same target and record header.
Ordinary schemas remain usable without it. A generated header containing managed
companions includes `<rohit/managed.hpp>` and therefore requires this target.

## Schema and identity

The [runnable ledger](../../example/managed/ledger/README.md) uses this schema directly:

```text
serializer version 1;
namespace ledger_example {
  class entry stable_ids managed {
    public string memo (1);
    public int64 amount_minor_units (2);
  }
  class ledger stable_ids {
    public string name (1);
    public managed map(uint64) entry entries (2);
  }
}
```

A leaf must declare `managed`. A class containing its own managed member is
eligible without repeating the class marker. An incoming managed use cannot grant
eligibility to an otherwise unmarked leaf: that schema is rejected. Managed fields
can be direct class values, arrays, or map values. They cannot be primitive fields,
map keys, or unions. The current editor generator supports public, unpacked owning
classes without inheritance or recursive ownership; these restrictions also apply to reachable ordinary
child classes. Other shapes produce generation errors.

By default the public generated class itself carries `persistent_id`, alongside
its application fields. `model_traits<ledger>::storage_type` is `ledger`, and
`store.read()->persistent_id` reads the root identity directly. There is no
`managed_ledger_data`/`managed_ledger_storage` pair in this default output.
Application fields keep their source order; the generated ID is appended, so
existing payload aggregate initializers can omit it and obtain zero (unassigned).
The compiler also generates typed editors and `model_traits`; no handwritten
adapter is required. Metadata spelling `persistent_id` remains fixed across
C++ naming profiles, while application fields and editors follow the profile.

A class qualifies through its marker or its own managed members. Participation
still follows occurrence boundaries: the store identifies the root and explicitly
`managed` descendants. An unmarked child remains an owned value under its parent.
If that child's type is managed-capable, the default single class contains an ID
slot, initially zero; that slot does not independently activate the child or its
nested managed annotations. Use a managed member for independently addressed
children. This retains the existing explicit boundary rule.

When an application actually needs ID-free ordinary values, opt into the separate
representation for all outputs sharing the model:

```ini
[managed]
separate_values = true
```

The compiler then generates ordinary `entry`/`ledger` payloads plus
`managed_entry_data`, `managed_entry_storage`, and corresponding ledger types.
The store holds wrappers with `persistent_id` and `value` members. For example,
read the ledger name through `store.read()->value.name` in this mode, versus
`store.read()->name` by default. `clone_value()` always returns a detached deep
copy: it retains identity in default mode and returns an ID-free ordinary payload
in separate-values mode. It never retains a connection to history.

Default class codecs serialize `persistent_id` at reserved field ID `1073741823`
(`0x3fffffff`), with wire name `persistent_id`. Application field IDs are unchanged;
conflicting metadata field IDs or names are rejected. Opt-in wrappers instead use
field 1 for identity and field 2 for the original payload, retaining the previous
managed representation. The modes have distinct schema bindings and cannot load
each other's saved envelopes without migration. Direct managed Protobuf output is
currently rejected; ordinary schemas and opted-in payload codecs retain their
existing behavior.

`stable_ids` identifies schema fields, independently of persistent object IDs.
Map keys remain application data. Root and managed descendants share **one counter
per document**, not a global object-ID allocator. A newly created document assigns
root ID `1`, then `2`, `3`, and so on in generated traversal order (schema member
order; map key order or array order within a collection). Insertions consume the
next number. Separate documents can each contain objects `1`, `2`, and `3`.

Allocation is dense/sequential at creation, not a promise of gap-free live IDs.
Deletion, undo, cancellation, and failed insertions do not renumber survivors or
reuse consumed IDs. The high-water mark survives save/load; exhaustion fails
instead of wrapping. Generated editors expose `id()` but no identity setter.

The default object-ID type is `uint32`; select `uint64` centrally:

```ini
[managed]
id_type = uint64
separate_values = false
```

Pass the file through `serializer --config project.ini` or
`serializer_generate(... CONFIG project.ini)`. CLI options `--managed.id_type`
and `--managed.separate_values true|false` override their corresponding settings.
Changing width or representation requires saved-state migration.

The store automatically creates a nonzero 128-bit **document namespace** when no
`document_id` is supplied. This happens once for a newly constructed store; managed
objects use the sequential counter, never random IDs. `make_document_id()` uses
`std::random_device` and propagates entropy-source failures. Its cross-process
uniqueness depends on the platform random source; applications can still provide
an explicit namespace for externally coordinated allocation or deterministic tests.
A document ID is not a user/session ID. External addresses combine document
namespace with object ID, while edits inside one store need only the object ID.

`load(bytes)` restores the saved namespace, object identities, history, and allocator
state rather than generating replacement identities. Loading into a freshly
constructed placeholder replaces its temporary namespace. To reopen managed saved
state, load the managed envelope; constructing a new store from a payload containing
already assigned root/entity IDs is rejected. Standalone class deserialization
preserves its ID field but does not reconstruct document/history metadata.

## All three transaction forms

```cpp
namespace managed = rohit::managed;
managed::model_store<ledger_example::ledger> store{
  ledger_example::ledger{"Draft ledger"}};
std::uint32_t entry_id{};

// Explicit commit.
managed::transaction_outcome manual;
{
  auto transaction = store.begin_transaction("Add entry", manual);
  entry_id = transaction.root().entries().insert(100, {"Supplies", 1500});
  transaction.commit();
}
manual.throw_if_failed();

// Successful normal scope exit auto-commits unless canceled or failed.
managed::transaction_outcome scoped;
{
  auto transaction = store.begin_transaction("Adjust amount", scoped);
  transaction.root().entries().edit(entry_id).set_amount_minor_units(1800);
}
scoped.throw_if_failed();

// One synchronous lambda, with a borrowed edit context.
auto result = store.execute_transaction("Describe entry", [entry_id](auto& transaction) {
  transaction.root().entries().edit(entry_id).set_memo("Office supplies");
});
result.throw_if_failed();
```

Setters across the object graph share the transaction and form one undo step.
Ordinary class children expose nested editors, such as `outer.position().set_x(10)`;
these changes belong to their nearest managed owner. Value fields have copying
`get_<field>()` and `set_<field>(value)` methods. Managed direct children expose a
child editor; managed maps provide `insert(key, value)`, `edit(id)`, and
`erase(id)`. Managed arrays provide `append(value)`, `edit(id)`, and `erase(id)`.
Insertion adopts the value (or converts an opt-in ordinary value) and assigns
new IDs to the inserted root and all managed descendants.
Application validation can be supplied through the store constructor callback.

`commit()` records completion and throws on failure. Destruction records failure
without throwing; exceptional unwinding reverts. Callback execution returns the
outcome after completion and does not retry or retain the lambda. Call `revert()`
to cancel; deleting a guard is not a separate cancellation signal. The callback
must return exactly `void`; async/coroutine results are rejected. Caught mutation
errors still poison the action, so earlier changes cannot accidentally commit.
The callback facade has no commit or ownership-transfer operation.

Editors resolve their paths on every use and reject removed entities. Their shared
lifetime channel is invalidated on transaction completion/destruction, so an
escaped editor rejects later use. Guard moves preserve active editor handles.
Map `edit(id)` initially scans for its key; subsequent edits use key lookup plus an
ID check. Array editors search by ID so reallocation cannot retarget a handle.
These are correctness guarantees, not a claim of constant-time ID lookup.

`read()` returns an immutable shared pin of storage (for example,
`store.read()->entries.at(key).persistent_id` by default, or
`store.read()->value.entries.at(key).persistent_id` in separate-values mode).
`clone_value()` returns a detached deep copy with IDs by default; only the opt-in
separated representation removes them. Stores are thread-confined and allow
one writer. The store and caller-owned outcome must outlive a manual guard.
Cross-thread guard destruction terminates; store copy/move is disabled.

Low-level `model_traits` adapters and `transaction.update(callback)`/`create_id()`
remain available for existing explicit storage integrations. They are trusted escape
hatches: do not leak mutable aliases or reassign IDs. Generated application code
should use `root()` and its editors. Raw same-type ID swaps cannot be distinguished
from ownership changes by snapshot comparison alone.

## History, persistence, and limitations

`store_options.mode` selects `disabled`, `linear` (default), or `tree` at runtime.
`model_store<Root, supported_mechanism::none>` with disabled mode omits the history
container while retaining managed identity. Only `none` and `history` are implemented
capability values; bare `managed` uses this available local profile.

`undo()` follows the parent; `redo_children()` lists alternatives; `redo(revision)`
requires a direct child; `checkout(revision)` restores a retained revision. Editing
after undo discards the redo suffix in linear mode and preserves it in tree mode.
No-op transactions preserve redo. `reset_history(mode)` explicitly discards all
history while retaining current state and allocation marks. Deleted entities need
no live graveyard: retained snapshots restore their original IDs and values.

`save()` returns bytes with document/schema identity, ID width, allocation marks,
current snapshot/cursor, mode, and retained revisions. `load(bytes)` validates a
fresh candidate before atomic replacement. Invalid identity, ancestry, cursor,
schema, width, mode, or bounded decoding leaves the open document unchanged.
The compiler derives a conservative schema fingerprint from reachable field/type
contracts and the ID width; it is not a security signature. Schema changes require
explicit migration of the current state and retained history. Ordinary schema
compatibility checks describe ordinary payload codecs, not managed save migration.

Reloading an older save into the same live document preserves higher allocation
marks. A new process knows only reservations actually saved; durable allocation
reservations and journal/file publication remain future work. Memory save/load is
not crash-safe filesystem saving.

Begin decodes a complete root; commit validates and encodes it. Revisions currently
retain whole-root snapshots, even when only one independently managed child changes.
Identity boundaries enable later changed-entity/delta storage but do not yet select
that optimization. Work/storage scale with root size; no benchmarked speedup is
claimed. Budgets bound snapshot bytes and retained encoded snapshots/labels, not
all decoded objects, indexes, temporary buffers, or externally pinned roots.

Feature selectors (`managed(...)`), `exclude(...)`, `transient`, notifications,
custom allocators, history-preserving policy changes, delta/checkpoint optimization,
merging, journals, authorization, collaboration, and other-language runtimes remain
unimplemented. The compiler rejects unsupported managed syntax and backends.

## Verification

Automated tests compile real managed schemas and exercise plain/managed occurrence
boundaries, nested editors, all transaction forms, atomic failure, lifetime checks,
guard moves, map/array edits, deletion/restoration, branching, save/load, and the
Google naming profile with IDs above the uint32 range. Compiler tests check target
eligibility, every C++ naming profile, central ID settings, collisions, and unsupported
backends. Runtime tests additionally cover budgets, malformed loads, reentrancy,
ID exhaustion, and canceled allocation. See `managed_store_test`,
`managed_direct_test`, `managed_profile_test`, and `core_serializer_test` in CTest.

Verified on Windows with MSVC 19.51, C++20, and warnings treated as errors:
35/35 configured CTest checks passed, including 226 core tests, 20 split/runtime
managed tests, four default direct-identity tests, and the generated Google/uint64
profile test. Both editor packages were rebuilt at 1.1.10; 57 shared tests and
821 fresh-generated navigation checks passed across both representations.
The default ledger example also built and ran against a locally installed package.
Interactive IDE installation, sanitizer qualification, durable-file recovery,
and performance measurements were not part of this verification.
