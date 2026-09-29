# Managed draft ledger example

This runnable C++ example uses the [schema-driven managed interface](../../../docs/managed/cpp_runtime.md).
[ledger.serializer](ledger.serializer) declares a managed entry and a ledger with
a managed entries map. [managed_example.cpp](managed_example.cpp) keeps comments
brief; this walkthrough explains each operation. No handwritten storage adapter
is required.

The default generated `ledger` and `entry` classes now contain `persistent_id`
directly. A new store automatically creates its document namespace and allocates
object IDs sequentially within that document. This is a draft-entry demonstration,
not a complete accounting model or durable journal.

[design.serializer](../design/design.serializer) models a hollow cylinder as a difference
between two managed cylinders, including ordinary and independently managed points.
[wordpad.serializer](../wordpad/wordpad.serializer) models managed paragraphs in an array.
Both are generated and exercised by the integration tests.

## What the schema generates

`entry stable_ids managed` makes the leaf eligible for managed use. `ledger`
qualifies through its `managed map(uint64) entry entries` member without repeating
a class-level marker. Omitting generated serialization methods, default output is:

```cpp
class entry {
public:
  std::string memo{};
  std::int64_t amount_minor_units{};
  std::uint32_t persistent_id{};
};

class ledger {
public:
  std::string name{};
  std::map<std::uint64_t, entry> entries{};
  std::uint32_t persistent_id{};
};
```

The ID follows application fields so aggregate initializers such as
`entry{"Supplies", 1500}` still work. Its initial zero means unassigned. A store
assigns IDs to its root and each explicitly managed descendant. Constructing a
standalone value does not allocate an ID because it has no document allocator.

Generated `model_traits<ledger>` uses `ledger` itself as storage. The compiler
also generates editors which perform checked changes inside transactions.
`read()` exposes an immutable ledger; editors do not provide an identity setter.

`stable_ids` concerns schema field numbers, not object instances. `memo (1)` and
`amount_minor_units (2)` identify entry fields. The ledger has its own field
numbers, so `name (1)` can use the same number as entry `memo (1)`.
The generated ID uses reserved wire key `0x3fffffff` and name `persistent_id`,
leaving those application field numbers unchanged.

## Document identity versus object identity

| Identity | Purpose | Allocation |
| --- | --- | --- |
| `store.document()` | Namespace for this entire document | Automatic when creating a store, or explicitly supplied |
| Root `persistent_id` | Identify the ledger within its document | `1` in a new document |
| `entry_id` | Identify one managed entry within that document | Next counter value; `2` for this example's first entry |
| `draft_key` | Application map lookup key | Application constant `100` here |
| Schema `(1)` and `(2)` | Identify fields within a class | Schema author |

A store owns one root ledger, not multiple independent documents. Another store
for a different document can also use object IDs `1`, `2`, and `3`: persistent
IDs are unique only within a document. The pair `(document_id, persistent_id)` is
needed only when addressing objects outside that document's context.

The document namespace is a `document_id` with two uint64 words, `high` and `low`.
Together they distinguish documents; neither word is a user/session identifier
or revision number. The store obtains a nonzero namespace from `make_document_id()`
when the constructor's document argument is omitted. This uses the platform
random source once for document creation, **not for object IDs**. Applications
can supply a namespace explicitly for coordination or deterministic tests.

Object allocation starts at `1` and increments across the entire managed tree.
Deletion, undo, or transaction cancellation does not renumber survivors or reuse
consumed IDs. Thus a newly created tree is densely numbered, but later live trees
may have gaps. Saved envelopes retain the namespace and allocator high-water mark
so reopening preserves identity and subsequent allocation.

`ledger_store::id_type` follows `[managed] id_type` (`uint32` by default, optionally
`uint64`), independently of the schema's uint64 map key and document namespace.
`--managed.id_type uint64` can override the configuration file.

## Walkthrough in source order

### Includes and initialization

`<ledger.hpp>` is generated from the schema during the build. It supplies the
classes, traits, codecs, and editors. `<rohit/managed.hpp>` supplies the store,
transaction guards, outcomes, and document namespace generation. `<cstdint>`
supplies `std::uint64_t`; `<iostream>` supplies `std::cout`.

`int main()` starts the executable. `namespace managed = rohit::managed` and
`using ledger_store = managed::model_store<ledger_example::ledger>` shorten names;
they do not allocate or construct objects. Omitted store options select history
support in linear mode. History currently stores whole-root snapshots, not diffs.

`static constexpr std::uint64_t draft_key = 100` defines the application map key.
It is independent of entry identity: `.at(draft_key)` is map lookup, whereas
`.edit(entry_id)` selects a managed object by persistent ID.

`ledger_store store{ledger_example::ledger{"Draft ledger"}}` constructs a
ledger with a name. Omitted members use their default member initializers: the
entries map is empty and the root ID is zero/unassigned. The store generates
its document namespace, assigns root ID `1`, and records the initial history
baseline. No manual `document_id{high, low}` is needed.

`ledger_store::id_type entry_id{}` initializes a local ID variable to zero. A
successful insertion will replace it with the allocated entry ID.

### Manual transaction

`managed::transaction_outcome inserted` creates a caller-owned pending outcome.
It precedes the inner block so it outlives the transaction guard.

`store.begin_transaction("Add draft entry", inserted)` creates an owning RAII
guard with an isolated candidate and reserves the store's single active writer.
The string labels the action in history; it does not write a log or create a UI.

| Expression | Meaning |
| --- | --- |
| `transaction.root()` | Obtain the generated ledger editor |
| `.entries()` | Obtain its managed map editor |
| `.insert(draft_key, {"Supplies", 1500})` | Insert a value at key `100`, allocate its identity, and return that ID |

The initializer contains memo and amount; omitted identity starts at zero. `1500`
is an integer number of minor units. The application chooses the currency/scale;
the schema does not imply 100 minor units per major unit.

`entry_id = ...` records the returned ID for later editing. The store's committed
snapshot remains unchanged until `transaction.commit()` publishes the entire
action as one undo step and fills `inserted`. Explicit commit failures throw.
The closing brace destroys the completed guard without committing a second time.

`inserted.throw_if_failed()` rethrows a recorded failure. An outcome contains
status, revision, and an optional exception. Status may be pending, committed,
no_change, reverted, or failed; the helper does not treat cancellation or no change
as an error. Application variables such as `entry_id` are outside rollback: use an
inserted ID as a published identity only after successful completion.

### Automatic scope completion

`adjusted` is another outcome declared outside its transaction scope.
`begin_transaction("Adjust draft amount", adjusted)` begins the second action.
`entries().edit(entry_id)` selects the same entry, and
`set_amount_minor_units(1800)` changes only the candidate.

The closing brace destroys the guard. Healthy normal exit commits automatically;
exception unwinding or an explicit `transaction.revert()` cancels. The destructor
cannot throw commit failures, so `adjusted.throw_if_failed()` checks them **after**
the block. Store and outcome must outlive the guard; transactions stay on the
store's owning thread. RAII releases resources without a manual `delete`.
Several setters in the block would still form one undo step.

### Callback transaction

`execute_transaction("Describe draft", [entry_id](auto& transaction) { ... })`
creates and completes the guard internally. `[entry_id]` copies the scalar ID,
not the object. `auto& transaction` receives a borrowed transaction-edit facade.
The callback runs synchronously once, returns void, and must not retain its facade
or editors. It sets the memo to `"Office supplies"`.

Normal return commits before the wrapper returns its outcome. Calling `revert()`
inside the callback would cancel; the borrowed facade does not expose early commit.
Callback/commit exceptions are recorded in the returned `const auto described`;
`described.throw_if_failed()` surfaces them. This is not asynchronous execution.

### Undo

`store.undo()` moves to the parent revision after all guards have closed. It undoes
only the description action; the amount adjustment and object identity survive.
Undo at the baseline would throw because there is no preceding revision.

| After operation | Root ID | Entry ID | Memo | Amount |
| --- | --- | --- | --- | --- |
| Construct store | `1` | Absent | ? | ? |
| Insert and commit | `1` | `2` | `Supplies` | `1500` |
| Scope-exit adjustment | `1` | `2` | `Supplies` | `1800` |
| Callback description | `1` | `2` | `Office supplies` | `1800` |
| Undo | `1` | `2` | `Supplies` | `1800` |
| Save and load | `1` | `2` | `Supplies` | `1800` |

Use the returned ID rather than hard-coding the example's allocation number.
The undone action remains available for redo. In linear mode a new edit after
undo discards that redo branch; tree mode retains alternatives. Here no new edit
is made before saving.

### Save, load, copy, and verify

`const auto bytes = store.save()` creates an owned byte vector containing the
versioned managed envelope: namespace, schema binding, ID width, current snapshot,
allocator/revision high-water marks, history mode, retained revisions, and cursor.
The undone description revision is retained for redo. This is memory serialization;
it does not write/flush a file or provide durable journaling or network management.

`ledger_store restored{ledger_example::ledger{"Temporary"}}` constructs a
placeholder store with a newly generated namespace. `restored.load(bytes)` checks
compatibility and validates the envelope before replacing it with the saved
document. The saved namespace and IDs are restored, not newly assigned. The name
becomes `"Draft ledger"`. These two in-memory stores now represent the same logical
document but are not automatically synchronized collaborators.

`const auto value = restored.clone_value()` deep-copies the identified ledger.
It retains persistent IDs in default mode but no connection to the store/history.
Changing a mutable copy would not change the store. This particular copy is const.
To restore a store, use its envelope: standalone class decoding preserves ID
fields but does not restore namespace/history/allocator metadata.

The final condition joins three checks with `||`:

1. `restored.read()->entries.at(draft_key).persistent_id != entry_id` checks entry
   identity. `read()` pins an immutable ledger with a shared pointer; there is no
   `.value` wrapper. The temporary pin remains alive through the expression.
2. `value.entries.at(draft_key).memo != "Supplies"` checks the undo result.
3. `value.entries.at(draft_key).amount_minor_units != 1800` checks that the earlier
   adjustment survived.

Any mismatch returns exit code `1`; a missing key instead throws from `at()`.
This small example leaves exceptions uncaught. A real application handles them at
its application boundary. The assertions check entry identity, memo, and amount,
not every envelope field. `std::cout` prints the success message and newline;
reaching the end of `main` returns `0` and releases locals through C++ lifetimes.

## Opt into ID-free ordinary values

If separate normal and managed types are needed, select them explicitly:

```ini
[managed]
separate_values = true
```

Or pass `--managed.separate_values true` to the compiler. The public `ledger`
then stays ID-free, while `managed_ledger_storage` holds its `persistent_id` and
`value`. Store reads in that representation require `read()->value.entries`, and
`clone_value()` returns an ID-free ordinary value. This example source uses the
default direct representation; changing the setting requires adjusting those
accesses. The split representation has separate integration tests.

Explicit member participation is unchanged: an unmarked child belongs to its
nearest managed owner. If its type is managed-capable, default output has an
unused zero ID slot; the opt-in separate representation removes that slot from
ordinary occurrences. Use `managed` on the member to give it independent identity.

See the [runtime guide](../../../docs/managed/cpp_runtime.md) for limitations,
representation/wire migration, and proposed capabilities beyond implemented history.

## Build and run

In the repository, configure `SERIALIZER_BUILD_MANAGED=ON`, build `managed_example`,
and run it. It prints a confirmation after checking the restored values and ID.
With tests enabled, CTest also registers `managed_example`.

Against an installed package built with managed support:

```sh
cmake -S example/managed/ledger -B out/managed-ledger-consumer -DCMAKE_PREFIX_PATH=/path/to/serializer
cmake --build out/managed-ledger-consumer --config Release
```

Use the platform's C++ developer environment and configure clang-format as described
in [CMake integration](../../../docs/cmake_integration.md). Run the resulting executable
from the standalone build directory (or its `Release` subdirectory for multi-config generators).
In a repository build, it is under `example/managed/ledger/` in the build directory.

Return to the [managed example index](../README.md).
