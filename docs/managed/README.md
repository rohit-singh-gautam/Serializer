# Serializer managed-state design

Current C++ representation: managed schema classes carry `persistent_id` directly
by default. Separate ID-free values/storage wrappers in the illustrations below
require `[managed] separate_values = true`; see the [runtime contract](cpp_runtime.md).
New documents get a namespace automatically. Root and managed descendants allocate
IDs `1, 2, 3...` within that document only; saved IDs/counters survive reload,
and undo/deletion never renumber survivors. Explicit member boundaries still apply.

Status: design collection with an implemented C++ managed interface. Read the
[implemented C++ API](cpp_runtime.md) for schema generation, typed editors, identity,
all three transaction forms, snapshot history, synchronous file journals, and exact limitations. Selectors,
exclusions, built-in authorization policies, and other-language runtimes remain
proposals. ID-free payload layouts/codecs are retained in opt-in separate-values mode. The walkthrough schema
remains a separate illustrative data model.

| Document | Read it for |
| --- | --- |
| [Implemented C++ runtime](cpp_runtime.md) | Optional build target, generated storage and typed editors, all three transaction forms, snapshot history, bounded save/load, and remaining work. |
| [Local store collaboration](local_collaboration.md) | Normal store transactions, store-owned sessions/outbox, immediate local undo, separate send/receive, periodic synchronization and client recovery. |
| [Implemented C++ collaboration](collaboration_runtime.md) | Authority/replica APIs, application-owned session types, generated change records, conditional undo/redo, retries, conflicts, presence, entity/subtree leases, host policy hooks, limits, and five runnable examples. |
| [Transaction implementation walkthrough](../internals/managed_transactions.md) | Current C++ store and transaction ownership, callback flow, commit, and cleanup, illustrated with a point. |
| [Editor implementation walkthrough](../internals/managed_editors.md) | Channel lifetime, callback forwarding, target resolution, and map/array editor behavior. |
| [Capabilities and identity](capabilities.md) | Four managed features, mandatory uint32 IDs, central configuration, collaboration allocation, and additional uses. |
| [Collaboration sessions and locks](collaboration.md) | Session-based synchronization, advisory presence, authoritative entity/subtree locks, replica caches, leases, failure handling, and efficiency targets. |
| [Journal](journal.md) | Appended/sidecar modes, full Save replacement and cleanup, undo cursors, durability, and crash recovery of unsaved changes. |
| [Data structures](data_structures.md) | Logical records and C++ excerpts generated from a schema: store ownership, identity/version tables, transactions, revision graphs, snapshots/deltas, deleted entities, and persistence. |
| [Walkthrough schema](walkthrough.serializer) | Source for every data class in the C++ walkthrough; ordinary supported syntax, without managed runtime behavior. |
| [Language bindings](language_bindings.md) | C++ capability templates and conditional storage, plus proposed representations in every other supported language. |
| [History and lifetime contract](history.md) | Schema participation, plain/managed objects, RAII completion, allocation, undo/redo, exclusions, and failure rules. |
| [Managed-state features](managed_state.md) | Feature profiles, authorization, merging, collaboration, distributed transactions, and external effects. |
| [Application examples](managed_examples.md) | A cylinder with a hole, accounting, wordpad, and other domains. |

The four proposed optional features are `history`, `collaboration`, `authorization`,
and `journal`. Every managed object has a generated persistent ID
(default `uint32`, centrally configurable); separate normal value objects are an opt-in representation.
Active or required authorization cannot be bypassed by a member selector.

Collaboration coordinates opaque sessions, not user accounts. Presence and lock
tables belong to the optional store component, outside document history. The host
provides transport, trusted session binding, and authority infrastructure; Serializer
defines record exchange, validation, and atomic acceptance. Presence is informational;
exclusive locking is an optional collaboration policy with authoritative enforcement.

The owning C++ runtime is `model_store<Root, Mode, Labels, Traits>`; `managed` is an alias
with the same arguments. `Mode` selects disabled, linear (default), or tree history
at compile time; each specialization contains only its selected storage. `Labels`
defaults to `history_labels::disabled`; `history_labels::enabled` opts into names.
Implemented `<type>_editor<Access>` handles
are temporary edit interfaces, not independently owning stores. Broader sketches
use illustrative `tracked_` names. Schema `managed` selects identity
boundaries within managed occurrences. Broader capability combinations remain
proposals; runtime journaling and the collaboration wrapper are independent of history mode.

For one synchronous editing action, the C++
[`execute_transaction(callback)`](cpp_runtime.md#all-three-transaction-forms)
passes a borrowed transaction edit context and returns its outcome after completion.
The wrapper uses the same RAII engine as caller-controlled `begin_transaction`.
Normal callback return attempts commit; revert or failure prevents publication.
All three forms remain supported: begin with explicit commit, begin with automatic
completion on normal scope exit, and callback execution. Explicit completion closes
once, so later destruction cannot commit again; `revert()` means cancellation.

History has three independent choices: whether it is compiled/available, whether
recording is active in disabled/linear/tree mode, and whether records contain root
snapshots, changed-entity snapshots, or reversible deltas. In tree mode, undo then
edit preserves the abandoned future as a sibling branch until explicit pruning.
Deleted entities leave the live model but remain reconstructable from retained
history records; there is no required repository of live deleted objects.

The detailed records are logical contracts, not native memory layouts or a finalized
wire format. Feature-specific state may be omitted where the backend supports it;
identity, ownership validation, transaction atomicity, and enforced permissions
remain part of the managed foundation. See the
[current usage guide](../usage.md) for implemented Serializer functionality.
