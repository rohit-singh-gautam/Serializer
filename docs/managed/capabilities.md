# Managed capabilities and mandatory identity

Current C++ representation: managed schema classes carry `persistent_id` directly
by default. Separate ID-free values/storage wrappers in the illustrations below
require `[managed] separate_values = true`; see the [runtime contract](cpp_runtime.md).
New documents get a namespace automatically. Root and managed descendants allocate
IDs `1, 2, 3...` within that document only; saved IDs/counters survive reload,
and undo/deletion never renumber survivors. Explicit member boundaries still apply.

Status: revised design proposal with implemented C++ identity/history and
[synchronous file journaling](journal.md), and [snapshot collaboration with conditional per-session undo/redo](collaboration_runtime.md).
The [C++ interface](cpp_runtime.md) accepts bare `managed` declarations and centrally
configured uint32/uint64 IDs. Selectors and the complete four-capability runtime
remain unimplemented. This document defines their shared contracts and supersedes
earlier suggestions that map keys alone provide identity or uint64 is the default.

## Four managed features

| Selector | Responsibility | Dependencies and boundaries |
| --- | --- | --- |
| `history` | Grouped undo/redo, linear or branching revisions, retained versions. | Does not imply durable saving. |
| `collaboration` | Exchange atomic changes between sessions, manage editing presence, and optionally enforce entity/subtree edit locks. | Requires a synchronization adapter and explicit acceptance authority; user undo history is optional. |
| `authorization` | Decide which entities, subtrees, fields, and operations a supplied principal may edit. | Can be used locally or with collaboration; authentication/invitations remain application responsibilities. |
| `journal` | Durably record committed changes between full saves, using an appended section or sidecar for crash recovery. | Requires a storage adapter; works without undo history or collaboration. |

Use singular `authorization` and the short mechanism name `journal` consistently
in schema selectors and native capability names. For example:

```text
managed(history)
managed(collaboration, authorization)
managed(history, journal)
managed(all except collaboration)
managed(all except journal)
```

`journal` describes change recording, while recovery describes its purpose.
It is distinct from audit/accounting journals and from automatically marking the
document saved. The [journal contract](journal.md) defines `appended` and `sidecar`
storage, full Save replacement, cleanup, and recovery of durable unsaved changes.

`all` means the features in an explicitly selected versioned profile. Expanding
an old two-feature profile to these four features requires a new profile version;
a compiler upgrade must not silently activate new behavior. Runtime options and
provided adapters determine activation. Identity, ownership validation, isolated
edits, and atomic local publication are always part of managed operation.

Full-document recovery must cover all persistent fields and ownership dependencies.
A member selector cannot silently omit data while the store promises complete crash
recovery: reject that configuration or provide a documented full-snapshot fallback.
A partial recovery domain needs an explicit scope and its own durability result.

Authorization is a selectable store capability. When active or required by a
document/application policy, it is an unavoidable gate on every affected object,
including ordinary child values, history navigation, and incoming remote changes.
Per-member selectors cannot exempt a protected object. Reject configurations that
omit required authorization support, or activate it without a policy provider.
A collaboration adapter may require authorization even though the two features
are conceptually separate. This revises the earlier classification of authorization
as only foundation machinery, while retaining mandatory enforcement once enabled.

Incoming proposals are authorized as edits at the authority. Receiving a trusted
accepted batch is a replication operation: validate its acceptance and applicable
replication policy without requiring the observing session to have edit permission.
Read-only collaborators must still be able to receive accepted model changes.

See [collaboration sessions and locks](collaboration.md) for the session contract,
three record streams, authority/replica roles, leases, and performance targets.
Presence is informational; locks are enforced coordination. Neither supplies user
identity or authorization. They are collaboration policies, not new selectors.
Session, replica, operation, lease, and grant IDs are distinct from entity IDs.

## Persistent ID on every managed object

Every managed instance, including the root, has a generated, persisted
`persistent_id` field. Its scalar type defaults to `uint32`. It belongs to the
managed storage representation, is assigned by the store, and is exposed read-only
through tracked editors. Separate normal value classes are ID-free only when explicitly requested.
A class-level `managed` marker requests managed capability and its ID-bearing
representation; it does not activate management in every ordinary occurrence.

Generated managed storage must carry the field even when history is disabled or
only authorization/journaling is selected. A sidecar index can speed
lookup, but cannot substitute for the mandatory managed ID. A map key remains
application data unless the application explicitly opts into an ID-key binding;
even then the managed record retains its authoritative ID and validates equality.

The framework metadata envelope/wrapper is separate from ordinary payload field
numbering. Do not silently inject a new numbered member into the existing plain
class or renumber its `stable_ids` fields. Generated names must diagnose collisions.
The walkthrough's explicit wrappers illustrate this separation. The implemented
C++ generator emits `managed_<type>_storage` wrappers and `<type>_editor` handles;
see [the runtime guide](cpp_runtime.md) for exact names and supported shapes.

An ordinary child point has no independent ID: changes use its nearest managed
owner's ID plus a stable field path. A managed child point gets its own ID and
can be independently referenced by any of the four features. Field IDs, entity
IDs, revision IDs, and operation IDs remain different concepts.

## Central configuration and compatibility

Implemented central C++ configuration:

```ini
[managed]
id_type = uint32
```

Implemented compiler override:

```text
serializer --input model.serializer --output model.hpp --managed.id_type uint64
```

Resolve `CLI override > project configuration > uint32 default`. Apply the effective
type consistently to generated managed IDs, reference adapters, descriptors, and
persisted format metadata across all generated languages and included schemas.
Initially specify `uint32` and `uint64`; other encodings need separately versioned
profiles. Reject incompatible linked/imported generated models rather than mixing
widths silently. A running store cannot change its generated scalar type.

Save the ID type and identity-format version in the envelope and journal headers.
Widening is an explicit migration of references, checkpoints, and recovery data;
narrowing additionally requires every live/retained/reserved ID to fit. Migration
must preserve numeric identity and document namespace or record a complete remap.
Schema member IDs stay unchanged. Revision/version/operation counters and file
offsets do not automatically become 32-bit because entity IDs do.

## Scope, allocation, and collaboration

The scalar ID is unique within a document, across all managed entity types and
collections. External addresses combine `document_id` with `persistent_id`.
`uint32` alone is not universally unique. Reserve zero for an unbound/not-yet-created
instance and allocate `1..2^32-1`; validate before publication. Exhaustion fails
without wrapping. Configure a wider type for workloads exceeding that lifetime
allocation budget, including deleted objects and reserved/offline ranges.

Preserve IDs on edit, move, save/reload, undo restoration, and crash recovery.
Duplication creates new IDs and remaps declared internal references. Never reuse
IDs consumed by aborted transactions, deletion, history pruning, or undo. Keep
allocator high-water marks outside undoable state, with a representation that can
express exhaustion even for the largest configured scalar.

For one writer, a monotonic allocator suffices. Multiple writers in the same
document need an authority or disjoint, durably reserved ranges. Offline peers
must reserve ranges in advance or negotiate a different identity format; separate
local counters collide even when each peer has a different user account. A replica
label alone cannot distinguish equal scalar IDs in this format.

Before exposing IDs outside a recoverable process, durably reserve their range or
use an equivalent durable allocation service. Recovery preserves allocation state
even when an edit or history cursor is rolled back. Opening independently edited
backup copies requires an allocation/namespace policy before they can synchronize.
This invariant also applies when local journaling is disabled but a
collaboration service accepts externally visible IDs.

## More uses of identified differential changes

| Scenario | Why identity and change records help |
| --- | --- |
| Audit and compliance records | Attribute changes to exact entities; use a separately retained audit log, not prunable undo history. |
| Offline synchronization and selective replication | Exchange only changed entities/subtrees and reconcile reconnecting replicas. |
| Incremental computation and rendering | Invalidate geometry, totals, layouts, caches, and indexes only for affected entities/dependencies. |
| Selective loading and large documents | Fetch an entity and its version without rebuilding the entire model. |
| Review, patches, and branch comparison | Describe edits to the same object across moves and accept/reject a change set. |
| External references and integrations | Keep annotations, bookmarks, selections, links, and database associations stable after reordering. |
| Distributed workflows | Correlate participant changes, deduplicate requests, and compensate earlier actions. |
| Incremental backup and migration | Transfer/version only affected records while preserving reference mappings. |

These are extension scenarios, not additional accepted `managed(...)` keywords.
Identity alone does not supply conflict resolution, audit retention, access checks,
or exactly-once external effects. Differential records also need typed paths,
insert/update/delete semantics, transaction/operation identity, exact base/result
versions, and schema/format versions. See [recovery](journal.md).
