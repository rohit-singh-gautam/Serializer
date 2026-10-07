# C++ managed interface

Status: implemented C++ schema generation, local snapshot history, and synchronous
file journaling/crash recovery. Bare
`managed` class/member declarations generate storage, conversions, identity
traversal, and typed transaction editors. Built-in authorization policies,
schema feature selectors, exclusions, notifications, and other-language managed runtimes
remain proposals. Unsupported selectors/backends are rejected rather than ignored.

The [collaboration runtime](collaboration_runtime.md) now wraps this store with
authoritative snapshot acceptance, retry handling, client proposal drafts, read-only
observation, advisory presence, entity/subtree locks, and host policy hooks. Generated
editors can author ordinary shared edits without application command adapters.
Session IDs default to `uint64_t`; `collaboration_session<Session, SessionTraits>`
selects application-owned string, unsigned integer, or custom session types.
See its [runnable examples](../../example/managed/collaboration/README.md).
For immediate local editing, attach a session with `store.collaborate(session)` and
keep using the existing transaction/edit APIs. The store owns pending synchronization,
local collaborative undo and recovery state; `synchronize`, `send_pending`,
`receive_changes` and caller-driven timer ticks control exchange. See
[local collaboration](local_collaboration.md) for setup and limits.
It uses a separate header and generated ownership traversal; ordinary local stores
have no collaboration state or additional setter checks.

For a step-by-step introduction using only a point with x and y, start with the
[nine point examples](../../example/managed/README.md). Each generates a managed
point from a schema and demonstrates the actual store, callbacks, generated editor,
or transaction lifecycle.

For a source-level explanation of `model_store`, its nested transaction types,
and the setter-to-candidate call path, see
[how managed transactions work](../internals/managed_transactions.md).
For channels, callback forwarding, and collection target resolution, see the
[managed editor walkthrough](../internals/managed_editors.md).

## Build and include

Managed runtime records are built by default (`SERIALIZER_BUILD_MANAGED=ON`).
The build option exposes the managed APIs; it adds no persistent IDs, transaction
checks, or history storage to ordinary schemas. Those costs arise when a schema
uses `managed` or an application creates managed stores and uses their features.

Link `Serializer::managed`, and generate the application schema normally in C++20:

```cmake
add_subdirectory(path/to/Serializer serializer-build)
add_executable(application main.cpp)
target_link_libraries(application PRIVATE Serializer::managed)
serializer_generate(TARGET application SCHEMAS model.serializer)
```

The target generates its envelope classes from
[managed_records.serializer](../../schemas/managed_records.serializer) after the
compiler builds, even without tests or examples. Internal record generation uses
`schemas/runtime_records.ini` with formatting disabled and requires no clang-format.
A separate cross-compilation host generator override is not provided for this target.
Installed packages export the same target and both managed/collaboration record
headers. Set `SERIALIZER_BUILD_MANAGED=OFF` to omit the record-dependent APIs;
independent journal, editor, and runtime headers remain available. A generated header containing managed
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
  auto transaction = store.begin_transaction(manual);
  entry_id = transaction.root().entries().insert(100, {"Supplies", 1500});
  transaction.commit();
}
manual.throw_if_failed();

// Successful normal scope exit auto-commits unless canceled or failed.
managed::transaction_outcome scoped;
{
  auto transaction = store.begin_transaction(scoped);
  transaction.root().entries().edit(entry_id).set_amount_minor_units(1800);
}
scoped.throw_if_failed();

// One synchronous lambda, with a borrowed edit context.
auto result = store.execute_transaction([entry_id](auto& transaction) {
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

History mode is a template argument, fixed for the lifetime of the store:

```cpp
using linear_store = rohit::managed::model_store<point>; // Linear by default.
using tree_store = rohit::managed::model_store<point, rohit::managed::history_mode::tree>;
using identity_store = rohit::managed::model_store<point, rohit::managed::history_mode::disabled>;
```

`model_store<Root, Mode, Labels, Traits, Features>` and its `managed` alias contain only the selected
history storage. Linear has a deque/cursor, tree has a revision map, and disabled
has an empty history slot. Dispatch uses `if constexpr`; there is no runtime mode
selector, variant, or pair of optional history pointers. Disabled stores retain
managed identity and transactions but have no undo/redo/checkout/reset API.
`store_options` configures limits and decoding, not history mode.
Synchronous file journals work with each history policy; see
[journal and recovery](journal.md#using-the-implemented-journal). Store-owned
[collaboration](local_collaboration.md) composes with these policies. Built-in inherited
authorization and schema capability selectors remain proposals.

### Independent store features

The fifth template argument is `store_features::all` by default. Select `none`,
`journal`, or `collaboration` to remove unused attachment storage and execution paths.
The first four argument positions and the `managed` alias remain compatible.

```cpp
namespace managed = rohit::managed;
using traits = managed::model_traits<point>;
using history_only = managed::model_store<point, managed::history_mode::linear,
    managed::history_labels::disabled, traits, managed::store_features::none>;
using journal_only = managed::model_store<point, managed::history_mode::disabled,
    managed::history_labels::disabled, traits, managed::store_features::journal>;
using collaboration_only = managed::model_store<point, managed::history_mode::disabled,
    managed::history_labels::disabled, traits, managed::store_features::collaboration>;
```

`history_only` has no journal or collaboration API, pointers, flags or saved-journal
baseline. `journal_only` has no history containers/limits or collaboration attachment.
`collaboration_only` has no journal storage or retained undo history. Pending snapshots,
identity mappings, retry bytes, conflict detection and trusted-session checks remain
necessary collaboration state. Acknowledged client transactions are removed after the
receive cursor catches up. History-disabled authorities reject inverse requests and do
not retain inverse snapshots, undo/redo stacks or contribution-version history.

An authority's seventh argument selects `store_features::collaboration` (without journal)
or `all`; it must include collaboration. The session facade exposes the same selection
as its `authority<Root, Mode, Labels, Traits, Features>` alias. Authentication and session
creation remain host responsibilities; there is no independent authentication feature.
All configurations retain model ownership, persistent identity, bounded decoding,
validation, thread confinement and transactional publication.

Tree history maintains retained-byte totals incrementally, checking each prospective
append in constant time. Load validates all retained entries and restores the counter;
failed publication does not advance it. Model envelopes and ordinary journal framing
are unchanged. See [checkpoint migration](../../migration.md#independent-managed-features)
for the new history-free collaboration checkpoint binding.

### Optional labels

The third template argument is `history_labels::disabled` by default. These stores
have no label string in transactions or history entries and emit no label wire
field. Use `execute_transaction(callback)` or `begin_transaction(outcome)`.

```cpp
using named_store = rohit::managed::model_store<
    point, rohit::managed::history_mode::linear,
    rohit::managed::history_labels::enabled>;
named_store store{point{1, 2}};
store.execute_transaction("Move point", [](auto& transaction) {
  transaction.root().set_x(10);
}).throw_if_failed();
const auto action = store.undo_label(); // "Move point"
store.undo();
const auto redo_action = store.redo_label(); // "Move point"
```

Enabled stores accept both named and unnamed forms; an unnamed action has an empty
label. `undo_label()` names the action that undo would reverse; `redo_label()` names
the next action. Tree mode uses `redo_label(revision)` and requires a direct child
of the current revision. These getters require an idle store and return string
copies, which remain valid after edits or eviction. Unavailable undo/redo throws
`std::out_of_range`. Names are metadata, not action IDs; duplicate names are valid.
Labels follow committed actions only: failed, canceled, and no-op edits add no name.
They count toward `max_history_bytes` when enabled. Enabling labels with disabled
history is a compile-time error. Custom `Traits` is now the fourth template argument.
See the [complete point example](../../example/managed/labeled_history/README.md).

### Navigation and retention

Linear stores expose `undo()` and parameterless `redo()`. Their entries contain
only snapshots by default (plus labels when enabled), and the cursor is a deque index. They have no revision
numbers, revision counters, `checkout()`, or `redo_children()`.
Tree stores expose `undo()`, `redo_children()`, `redo(revision)`, and
`checkout(revision)`; tree entries retain revision IDs and explicit parents.
Editing after undo discards the redo suffix in linear mode and preserves it in
tree mode. Failed, canceled, and no-op transactions preserve redo.

Use `Store::outcome_type` for manual guards in code supporting either policy.
Linear and disabled stores return `transaction_outcome` (status and exception).
Tree stores return `tree_transaction_outcome`, which additionally supplies
`revision` for branch selection. Persistent object IDs remain independent of history.

Linear history uses a deque of snapshots and a cursor. `max_revisions` counts all
retained states, including the current state and any redo states; it defaults to
1024. Set it to 100 for at most 99 undo steps when the cursor is at the newest state.
After removing redo, a changed commit evicts oldest states until both the count
and `max_history_bytes` limits fit. Bytes count encoded snapshots plus labels when enabled.
A single new state that cannot fit fails without changing the model or history.
A limit of one retains only the current state; zero cannot hold an enabled baseline
(use disabled mode instead). Eviction never recycles persistent object IDs.
The retained-state limit keeps its existing name `max_revisions` for both policies;
it does not imply that linear entries have revision IDs.
Tree history keeps its revision map and rejects over-budget commits, preserving
all branches until an explicit reset. The other history container is absent from the type.

Unjournaled linear commit does not copy the history container or scan every retained state;
it prepares one entry and touches only states being removed. Undo/redo select
adjacent entries in constant time, without searching revision IDs. Restoring
the full snapshot still requires decoding and validation in either mode.

`reset_history()` explicitly discards all history while retaining current state
and allocation marks. It starts a baseline in the same template-selected mode;
it cannot change a linear store into a tree store. Deleted entities need
no live graveyard: retained snapshots restore their original IDs and values.

`save()` returns bytes with document/schema identity, ID width, allocation marks,
current snapshot/cursor, mode, and retained history. Labels disabled (the default)
uses format version 3: `records::unlabeled_state_envelope` for linear/disabled,
`records::unlabeled_envelope` for tree. No label fields are emitted.
Label-enabled stores preserve `records::state_envelope` version 2 for linear and
`records::envelope` version 1 for tree. Linear entries retain order and a zero-based
cursor, with no revision IDs, parents, or revision high-water mark. Disabled
history has no entries and cursor zero.
`load(bytes)` validates a fresh candidate before atomic replacement. History mode
and label policy must match. Older saves require the corresponding enabled-label
specialization or explicit migration; no automatic converter is supplied.
Loading also rejects histories exceeding the
configured limits rather than silently evicting imported states. Invalid identity,
ancestry, cursor, schema, width, mode, or bounded decoding leaves the open document
unchanged. Linear loads validate every entry and require the selected entry's
snapshot to equal the saved current snapshot.
The compiler derives a conservative schema fingerprint from reachable field/type
contracts and the ID width; it is not a security signature. Schema changes require
explicit migration of the current state and retained history. Ordinary schema
compatibility checks describe ordinary payload codecs, not managed save migration.

Reloading an older save into the same live document preserves higher allocation
marks. Without journaling, a new process knows only allocation marks actually saved.
An attached journal durably reserves every ID before returning it, including IDs
consumed by canceled edits. Memory `save()` alone is not crash-safe filesystem saving.

### Journal and crash recovery

`create_journal(path, journal_storage_mode::appended|sidecar, options)` creates a
new durable baseline. `recover_journal(path, options)` restores and opens an
existing document into an unattached store. `save_journal()` publishes a full Save
without discarding retained history. Journal writes cover commits, navigation,
history resets, and ID reservations before live publication.
`journal_dirty()` compares current values with the last full Save;
`journal_sequence()` advances independently of the history cursor.

All three transaction forms report uncertain durable writes as
`transaction_status::indeterminate`; `throw_if_failed()` rethrows the
`journal_indeterminate_error`. When `journal_needs_recovery()` is true, destroy
the store and recover into a fresh one before further writes. Do not retry the
editing callback blindly. `load()` cannot bypass an attached journal.

The store owns a storage-independent journal sink. Its current file adapter uses
`rohit::file_stream`; shared framing accepts existing Serializer buffers and
standard streams. See [stream integration](../usage.md#file-streams-and-journal-records).
Database sinks are not implemented.

The adapter is a synchronous, single-writer snapshot journal implementation
for Windows and POSIX. It supports both representations, both ID widths, every
history mode, and optional labels. Per-record/per-file budgets, generation
binding, integrity checks, incomplete-tail repair, and durable replacement are
documented in the [journal guide](journal.md), including filesystem assumptions
and the complete version-two framing contract. Each edit writes its existing
serialized snapshot once, with 41 bytes of overhead for default linear history
(33 for unlabeled tree/disabled). Navigation/reservations are 33-byte records.
History and document/schema metadata are written in full only at base creation or
full Save. Linear commits preallocate a tail entry and publish retention changes
after the flush, without copying the history container.

Begin decodes a complete root; commit validates and encodes it. Revisions currently
retain whole-root snapshots, even when only one independently managed child changes.
Identity boundaries enable later changed-entity/delta storage but do not yet select
that optimization. Work/storage scale with root size; no benchmarked speedup is
claimed. Budgets bound snapshot bytes and retained encoded snapshots/labels, not
all decoded objects, indexes, temporary buffers, or externally pinned roots.

Feature selectors (`managed(...)`), `exclude(...)`, `transient`, notifications,
custom allocators, history-preserving policy changes, delta/checkpoint optimization,
merging, delta journals, background recovery checkpoints, built-in authorization policies,
and other-language runtimes remain
unimplemented. The compiler rejects unsupported managed syntax and backends.

## Verification

For collaboration verification and its remaining qualification work, see the
[collaboration runtime](collaboration_runtime.md#verification). Its addition passed
all 52 configured CTest checks on Windows after the collaborative undo addition;
the records below describe
earlier managed-store and journal qualifications.

Automated tests compile real managed schemas and exercise plain/managed occurrence
boundaries, nested editors, all transaction forms, atomic failure, lifetime checks,
guard moves, map/array edits, deletion/restoration, branching, save/load, and the
Google naming profile with IDs above the uint32 range. Compiler tests check target
eligibility, every C++ naming profile, central ID settings, collisions, and unsupported
backends. Runtime tests additionally cover budgets, malformed loads, reentrancy,
ID exhaustion, and canceled allocation. See `managed_store_test`,
`managed_direct_test`, `managed_profile_test`, and `core_serializer_test` in CTest.

The deque retention and optional-label changes were built on Windows with MSVC 19.51, C++20, and
warnings treated as errors. All 14 managed CTest checks passed, including the
runtime suites, nine point examples, ledger example, and isolated deque allocation
failure test. Coverage includes count/byte eviction, redo preservation on failure,
pruned save/load, same-mode resets, cross-mode load rejection, compile-time API
availability, legacy linear envelope rejection, navigation validation failure,
allocation failure before history removal with both label policies, optional label
navigation and persistence, label-free entry size, and cross-label load rejection.
No performance benchmark or other-platform qualification was run for this change.

Earlier full-project qualification on Windows with MSVC 19.51 and C++20:
35/35 configured CTest checks passed, including 226 core tests, 20 split/runtime
managed tests, four default direct-identity tests, and the generated Google/uint64
profile test. Both editor packages were rebuilt at 1.1.10; 57 shared tests and
821 fresh-generated navigation checks passed across both representations.
The default ledger example also built and ran against a locally installed package.
Interactive IDE installation, sanitizer qualification, and performance measurements
were not part of that earlier verification. Journal verification is described below.

The journal changes add native-file tests and abrupt-process-exit recovery checks
for appended and sidecar modes. The Windows MSVC build and all 47 configured CTest
checks passed, including 16 managed checks and the subprocess crash matrix.
The optimized append path is checked for exact byte growth over 100 retained edits.
Eight stream tests additionally cover generated file serialization, framing across
buffers/iostreams/files, EOF/seek/truncate, and explicit synchronization failures.
The runnable journal example verifies both physical modes and full-Save reopening.
The same 120 process-crash recovery checks passed with GCC under WSL on the
mounted workspace; the independent Python reader verifies version-two framing.
Power-cut testing, physical disk exhaustion, and network-filesystem qualification
were not performed.
