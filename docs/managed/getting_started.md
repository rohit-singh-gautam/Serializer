# History, journals, collaboration, and authorization

[Back to the project overview](../../README.md)

Serializer — **State Framework** combines generated data-exchange codecs with
application-state management. Its optional C++ managed runtime adds transactions
and persistent object identity to generated models. You can then keep undo history,
recover committed edits from disk, and synchronize changes between clients.

Start with a small local model. Add persistence and collaboration when the
application needs them; each introduces a different responsibility.

Serialization-only applications can use ordinary generated models without a
managed store. Building managed support does not add identity or history to those
schemas. The source build enables managed support by default; using it in an
application remains an explicit choice.

| Feature | Question it answers | Current support |
| --- | --- | --- |
| [History](#history) | Can the user undo or redo an edit? | Disabled, linear, or tree history in C++ |
| [Journal](#journal) | Can the application recover committed work after a crash? | Synchronous appended or sidecar files in C++ |
| [Collaboration](#collaboration) | How do clients exchange and accept edits? | C++ stores and replicas coordinated by one authority |
| [Authorization](#authorization) | May this session publish this change or acquire this lock? | Authority gates and application-supplied policy hooks |

These are implemented C++ facilities. Other languages can exchange the
[managed wire records](../../example/managed/multilanguage/README.md), but do not
have native managed engines. Broader schema selectors and built-in permission
policies remain proposals.

## Start with a managed model

Managed support defaults to `SERIALIZER_BUILD_MANAGED=ON`. Link your C++ application to
`Serializer::managed`, and generate a model with supported bare `managed`
declarations. See the [build and schema setup](cpp_runtime.md#build-and-include).

Select history mode and journal/collaboration support independently at compile
time. Stores default to linear history, disabled action labels, and
`store_features::all`; disabled features omit their attachment storage and APIs.
For history-only, journal-only, or collaboration-only stores, follow
[independent store features](cpp_runtime.md#independent-store-features). Schema
feature selectors and exclusions remain proposals.

Managed classes carry `persistent_id` by default. Document namespaces and object
IDs identify the model independently of user accounts or collaboration sessions.
The [identity guide](cpp_runtime.md#schema-and-identity) explains ownership boundaries.

Create a `rohit::managed::model_store<Root>` and edit through generated transaction
editors. A successful transaction publishes its changes together. Manual commit,
scoped completion, and `execute_transaction(callback)` are supported.

Work through the [small point examples](../../example/managed/README.md) before
the [ledger example](../../example/managed/ledger/README.md). Both include runnable
code, build instructions, and expected output.

## History

History retains earlier model states so an editing action can be undone. For
example, a transaction that changes both coordinates of a point becomes one
undoable action.

Choose a history mode when declaring the store type:

| Mode | Behavior |
| --- | --- |
| `history_mode::linear` | Default. `undo()` and `redo()` move through adjacent states. A changed edit after undo discards redo. |
| `history_mode::tree` | Retains branches and offers revision-based navigation. |
| `history_mode::disabled` | Keeps identity and transactions without undo/redo storage or APIs. |

The mode is fixed at compile time. History currently stores whole-root snapshots.
Linear history evicts oldest states to satisfy limits; tree history rejects
commits that would exceed its limits.

Transaction labels are optional. Select `history_labels::enabled` if the UI needs
names such as “Move point” for undo and redo actions.

History alone does not write files. Use memory save/load for explicit persistence,
or attach a journal for durable transactions. Failed, canceled, and no-op edits
do not create ordinary history entries.

Read the [history API and retention rules](cpp_runtime.md#history-persistence-and-limitations)
and run [undo and redo one action](../../example/managed/history/README.md).

## Journal

A journal records successful edits on disk so a fresh store can recover them.
With a journal attached, a changed commit flushes its record before publishing
the new in-memory state.

The basic workflow is:

1. Call `create_journal(path, mode)` to create a new saved baseline.
2. Edit through normal transactions. The store journals committed changes.
3. Call `save_journal()` when the application performs a full Save.
4. On reopening, call `recover_journal(path)` on a fresh store.

`create_journal` refuses to overwrite an existing document. Recovery reads the
stored records; it does not run editing callbacks again.

| Storage mode | Files |
| --- | --- |
| `journal_storage_mode::appended` | The saved baseline and later records share a document file. |
| `journal_storage_mode::sidecar` | The baseline has a separate journal companion; keep both when moving the document. |

Durability and the user's Save state are separate. An edit can be recoverable
while `journal_dirty()` remains true. `save_journal()` updates the saved baseline;
`journal_sequence()` tracks durable operations, including navigation and ID reservations.

If a write reports an indeterminate outcome, stop using that store, destroy it,
and recover into a fresh instance. Do not blindly repeat the editing callback.
The [journal contract](journal.md#using-the-implemented-journal) explains failure
handling, locking, resource limits, and file-system assumptions.

Run the [journal and recovery example](../../example/managed/journal/README.md)
to exercise both layouts, reopening, undo/redo, and full Save.

## Collaboration

Collaboration lets multiple client stores exchange edits with one ordering
authority. The authority validates proposals and publishes accepted changes.
Each client keeps its own local state and pending work.

For an application with a `model_store`, begin with
[local store collaboration](local_collaboration.md):

1. Create and authenticate a session in the application.
2. Attach it with `store.collaborate(session_id)` and bind its transport.
3. Call `store.synchronize()` to join the authority's baseline before editing.
4. Use normal transactions and generated setters to edit locally.
5. Synchronize again to send pending work and receive accepted changes.

The application drives synchronization explicitly, or calls
`synchronize_if_due(now_ms)` from the store's owner-thread timer. Serializer does
not create a background network service. `send_pending()` and `receive_changes()`
are available when the application needs separate control.

Disjoint field changes can merge. Same-field and ownership conflicts remain
explicit: local drafts are retained, and `acknowledged_read()` exposes accepted
state separately for the application to resolve the conflict.

Local undo applies an inverse edit immediately. Synchronization still checks it
against accepted history, current permissions, and locks. An undo that succeeds
locally can later conflict with an unseen remote edit.

Attach a journal to persist client state and pending work together. Network
synchronization cadence is independent of the journal's per-transaction flush.
Follow the [recovery setup](local_collaboration.md#undo-identities-and-recovery)
to restore the same session and exact outstanding request.

Start with [local_sync.cpp](../../example/managed/collaboration/local_sync.cpp).
The [collaboration examples](../../example/managed/collaboration/README.md) also
cover conflicts, read-only sessions, leases, and collaborative undo/redo.

## Authorization

Authentication establishes which session a connection represents. Authorization
decides what that session may do. The application owns authentication and the
trusted connection-to-session binding.

Serializer's authority provides three implemented controls:

- `open_session(id, writable)` and `set_session_writable` allow or deny publication
  of edits at session level.
- `change_policy(session, before, candidate, affected_ids)` can reject a proposed
  model change as a whole.
- `lock_policy(session, target, scope)` can reject lock acquisition or renewal.

A read-only authority session can still have a locally edited draft; the authority
rejects publishing it. Applications should reflect permissions in their editing UI
as well as enforce them at acceptance.

Serializer does not supply user accounts, roles, inherited allow/deny rules,
automatic field permissions, read filtering, or persisted policies. A local
`model_store` is independently usable without a collaboration authorization gate.

Read the [authorization guide](authorization.md) for trusted session handling,
policy timing, and the limits of the current implementation.

## Choose only the features you need

History mode and store features are separate template choices. The fifth
`model_store` argument selects `store_features::none`, `journal`, `collaboration`,
or `all` (the default).

For example, a local undo-only tool can use linear history with `none`. A
recovery-only service can combine disabled history with `journal`. Collaboration
can run with history disabled when collaborative undo is unnecessary.

There is no standalone authorization or authentication feature selector.
Session trust and authority checks belong to the collaboration integration.
See [complete feature-selection examples](cpp_runtime.md#independent-store-features).

The [managed documentation index](README.md) links the full API contracts,
implementation walkthroughs, and clearly identified proposals.
