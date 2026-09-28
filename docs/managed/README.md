# Serializer managed-state design

Status: proposal only. No managed-state grammar, generated companions, runtime,
history engine, collaboration adapter, or finalized record format is implemented.
Ordinary Serializer codecs and generated payload classes remain the existing API.
The walkthrough schema below is a generatable data-shape example using that API.

| Document | Read it for |
| --- | --- |
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
(default `uint32`, centrally configurable); normal value objects remain ID-free.
Active or required authorization cannot be bypassed by a member selector.

Collaboration coordinates opaque sessions, not user accounts. Presence and lock
tables belong to the optional store component, outside document history. The host
provides transport, trusted session binding, and authority infrastructure; Serializer
defines record exchange, validation, and atomic acceptance. Presence is informational;
exclusive locking is an optional collaboration policy with authoritative enforcement.

The proposed owning runtime is `model_store<Root, support>`; `managed<Root, support>`
can be a C++ alias for the same type. Generated `tracked_` companions are temporary
edit interfaces, not independently owning stores. Schema `managed` selects identity
boundaries within managed occurrences. Compile-time support, schema participation,
and runtime activation are separate decisions.

For one synchronous editing action, the proposed
[`execute_transaction(label, callback)`](history.md#callback-based-transaction-execution)
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
