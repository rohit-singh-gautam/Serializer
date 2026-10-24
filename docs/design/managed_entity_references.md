# Typed managed entity references and identity-preserving operations

Status: C++ implementation and remaining qualification requirements for source
**2.7.0**, schema language **1.20.0**. Typed references, static class identity,
editable reconciliation and previews are implemented in the working source;
strict collaboration checks cover native range leases and recovery. Ordinary
nonmanaged native payloads retain their format. Generated managed persistent IDs
now default to compact varints; reference fields and lease checkpoints add the
explicit contracts described below.
See the [implemented registry](../managed/state_registry.md),
[native runtime](../managed/cpp_runtime.md), and
[local collaboration contract](../managed/local_collaboration.md).

## Ownership and present support

Serializer owns native managed identity, allocation, document scope, generated
type metadata, transactional validation and publication. An application declares
the target entity type, ownership endpoints, reference constraints, deletion
policy and the desired import operation. It supplies values and uses the native
registry; it should not need a second identity allocator or object repository.

Remaining work and qualification are tracked only in the
[central backlog, MR01–MR03 and MG05](../pending.md#managed-references-and-migration).

| Capability | Current implementation |
| --- | --- |
| Native identity | Managed occurrences carry document-local `persistent_id`; C++ `managed_ref<T>` reuses it with document/type scope. |
| Typed lookup | `registry.reference<T>(id)` and `resolve(token)` check scope/type; complete candidates validate declared references before publication. |
| Ordinary field/value references | Durable field addresses and inline borrowed/shared/weak values support ordinary owning data under their documented restrictions; they do not supply managed entity identity. |
| Collaboration | Native range reservations preserve identical document/object IDs; offline edits, exhaustion, journal restart and deleted-ID rejection are checked. No translated-ID operating mode exists. |
| Independent import | `import_storage` allocates fresh IDs and rewrites declared internal tokens; unmapped foreign external targets reject. |
| Existing-entity edits | Generated `apply_editable` retains the entity ID and reconciles owned children through ordinary native transactions. |
| Editing preview | `preview`, `working`, `cancel` and `accept_preview` share the accepted root and check the exact base. New IDs pass through the source allocator before exposure. |

The registry is a rebuildable view of a selected native snapshot, not another
allocator. Its present catalog costs and lifetime rules remain those in the
[registry guide](../managed/state_registry.md#typed-identity-and-snapshot-lookup).

| Requirement | Serializer responsibility | Application responsibility |
| --- | --- | --- |
| Same-document entity references | Typed fields/handles, native-ID reuse, document/type/liveness checks, complete-candidate validation and atomic failure. | Declare the target type, optional presence, specific-owner constraints and deletion policy. |
| Collaboration identity | Allocate collision-free identities before exposure; preserve the same document ID and object ID across replication, journals and recovery. | Select the shared document/session and transport, authenticate participants and define access rules. |
| Independent imports | Allocate fresh native IDs, rewrite declared references and attach declared owned children transactionally. | Choose what to import and provide explicit mappings or reject targets outside that import. |
| Editable-value application | Generate normal transactional setters and identity-preserving child reconciliation. | Declare editable fields and ownership endpoints; supply candidate values and domain checks. |
| Editing previews | Isolate editable values, reserve native IDs before exposure, check the exact base and accept atomically with native Undo/Redo. | Present the preview, choose accept/cancel and resolve a rejected stale proposal. |
| Static class identity | Generate and collision-check a stable discriminator with a documented compatibility contract. | Assign explicit public identities when required and preserve them through compatible class renames. |

Business rules, geometry validation and user-interface behavior remain application
work. Generic identity, allocation, reference validation, rollback and history
behavior belong in Serializer.

## Same-document typed references

Use language 1.20 and the C++ managed runtime:

```text
serializer version 1.20.0;
class item stable_ids managed class_id(101) {
  public string label (1);
}

class placement stable_ids managed {
  public managed_ref<item> target (1);
}
```

Generated typed handles expose `get_id()` carrying the existing native ID,
static class tag and document scope; legacy numeric `id()` remains available.
Assigning another target type fails at compile time. A foreign-document token
fails resolution or complete transaction validation before publication. An ordinary integer is not proof
of target identity; explicitly resolving decoded numeric input must check the
source document, existence, exact expected type and current lifetime first.
Optional references use ordinary optional presence rather than a second sentinel
identity convention.

Type compatibility permits any eligible instance of the declared type. A rule
requiring one particular owner is an additional declared equality/ownership
constraint, not another type annotation. Reuse the declared ownership relation
instead of storing its native ID again solely for that check.

Validate complete candidates after all entities are available so forward and
cyclic nonowning references do not depend on decode order. Apply the same checks
to transaction edits, import, recovery and collaboration acceptance. Target
deletion must follow a declared reject-or-unresolved policy; it must never
silently retarget a surviving reference.

`registry.reference<T>(id)` creates a token from a checked native snapshot;
`registry.resolve(token)` returns a pinned exact-type entity. Transaction
registries provide checked editor operations. Required references default invalid;
`optional<managed_ref<T>>` represents absence. For required/self/cyclic initial
graphs, the constructor's final initializer callback runs after native IDs are
assigned and before validation or the first history snapshot. Its callback-scoped
`reference_builder` supplies checked tokens; no invalid baseline is accepted.

A token writes `{document_high, document_low, class_id, object_id}` through the
four native C++ codecs, with no target pointer or ownership. Decoding checks the
envelope/type; complete document loading checks live membership after all objects
exist. Explicit [Python/Java token codec generation](../managed/token_codecs.md)
exchanges these envelopes independently of their native engine options; other
generated outputs retain their explicit capability gates.

Required references cannot be transient or omitted from a codec when resetting
that field would leave a required token invalid. Use optional presence for an
absent/reset edge. The C++ generator rejects these unsupported combinations.
Language 1.23 adds [named managed generic self binding](../schema/named_managed_references.md)
for copied direct token fields and supported container wrappers. Older named
instantiations and recursive edges inside shared nested/base records still require
explicit concrete target classes; concrete self and mutual references remain supported.

The generated `persistent_id` and token `object_id` use `compact_varint` by
default in all three native binary protocols. `[managed] id_encoding = fixed`
selects fixed-width payloads. `id_type = uint32` remains the default memory type;
`uint64` remains available. Document and class identity fields retain their
encoding. JSON keeps numeric IDs. This is an encoding choice, never an identity
translation. See the [migration guide](../guides/persistent_id_migration.md).

## Static type identity is an early check

Generated model traits and registry entries identify the exact schema type. A
stable numeric discriminator rejects mismatched types before further resolution;
it is independent of native object IDs and layout hashes.

That discriminator identifies a class, not one occurrence. Two live objects of
the same class have different native IDs. A matching class discriminator still
requires document-scope, membership, deletion/lifetime and declared constraint
checks. It cannot establish that an arbitrary numeric ID exists or belongs to
the expected document. Do not treat an RTTI name or native pointer address as a
durable schema discriminator.

The registry retains its process-local exact-type token and qualified `type_key`.
Generated `model_traits<T>::class_id` adds a stable numeric tag in reference
envelopes; `persistent_id` stays unchanged. Defaults use a frozen FNV-1a-64 hash of
qualified concrete schema identity; internal generics use canonical origin/type/
dimension arguments. Source order and compatible field additions do not change
the tag. `class_id(decimal)` freezes a tag across renaming. Distinct type-tag
collisions reject, including incompatible explicit generic tags. The changing
schema/layout identity remains a separate compatibility key.
See [registry type checks](../../include/rohit/managed_registry.hpp) and
[generated traits](../../src/cpp_managed_writer.hpp).

The implemented typed reference uses this envelope; optional presence is a
separate schema choice:

```text
managed_ref<T> = { document_id, static_type_id<T>, object_id }
```

The current envelope carries document scope on every token. Checked token
construction/resolution compares class and document scope in constant time before
membership lookup. Resolution still finds the object in the
selected snapshot; the current registry catalog uses an ordered ID map. Matching
tags do not remove that lookup or make an untrusted reference valid.

A packed `{class bits, instance bits}` ID is another possible versioned encoding,
but reduces the instance-ID space and changes allocation, persistence and wire
compatibility. It must be explicitly selected and migrated; do not silently
reinterpret existing 32-bit or 64-bit IDs. Freeze class discriminators in explicit
declarations or a reviewed manifest, reject collisions across included schemas
and concrete generic types, and define exact-type versus inheritance matching.
Source order, RTTI addresses and a changing layout hash are unsuitable sources.

## Same-document collaboration and independent import

These are separate operations with different identity requirements.

The C++ implementation uses authority-issued disjoint native ID ranges. Each
participant allocates directly from its own reserved range; acceptance and
delivery retain those exact numeric IDs. Mapping fields retained in old record
layouts are empty and do not select a translation mode. See
[the current contract](../managed/local_collaboration.md) and
[the implementation](../../include/rohit/managed_local_collaboration.hpp).

The required collaboration contract is strict: every participant editing the same
document MUST use the same document identity. One logical entity MUST retain its
identical native object ID at its source, authority and every destination replica,
including offline/concurrent creation, pending edits, acknowledgement, retries,
Undo/Redo, reconnect, Save and recovery. Serializer must prevent allocation
collisions before exposing IDs. If safe allocation is unavailable, fail without
publishing or renumbering an entity. A translation table does not satisfy this
required contract. No application option may weaken this rule or require ID
translation as the normal collaboration path.

Serializer must prevent collisions at initial allocation, using durable authority
reservations or another proven distributed allocator. It owns exhaustion, crash
recovery and nonreuse; this is not an application-side counter. Joining a document
must establish its identity and allocation authority before exposing newly created
collaborative entities. Offline creation requires a valid durable allocation
reservation; exhausted or uninitialized reservations fail before an ID is exposed.
Acceptance, acknowledgement and reconnect never rename an exposed entity.

Client checkpoints use format 2; old/mapped checkpoints reject before editing.
There is no automatic mapped-session conversion. Existing independent documents
keep their identities until an explicit independent import creates new
destination objects.

Native store clients prefetch ranges through their authority transport. A custom
transport forwards `reserve_ids`; standalone replicas explicitly call
`initialize_identity_lease(session, authority.reserve_ids(session, count))` before
creating entities. Reads remain available when a session has no writable capacity.
The allocator rejects creation before exposing an ID when its range is missing
or exhausted. Preview acceptance also checks the full captured allocation context.

Authority range issuance is durable before return when its native journal is
configured. Client journal state preserves burned/reserved capacity before
exposure. Snapshot-only use must explicitly persist a coherent authority/client
checkpoint; restoring an old snapshot cannot discover ranges issued after it.
The application selects its durability mode and supplies the native journal;
it does not implement another allocator.

Keep the steady-state path small: no local-to-remote ID maps, repeated reference
rewrites or document-wide identity scans during ordinary synchronization. A shared
identity is compared directly. Measure allocation and synchronization CPU, retained
memory and encoded size against the existing implementation; report the measured
scope rather than claiming an unqualified performance ranking.

An independent-document copy/import creates new entities in the destination.
Fresh native IDs are appropriate there. Generate a bounded mapping from source
document/entity identity to destination native identity, rewrite only declared
reference fields, attach through declared owning endpoints and validate the
complete candidate atomically. Ordinary numeric payload values remain unchanged.
An external source target needs an explicit destination mapping or rejection;
equal numeric IDs in two documents are not a mapping.

Use `registry.import_storage<Plain>(source_document, storage, attach)` for
identity-bearing source storage. It allocates fresh IDs, rewrites declared
internal tokens in two passes and invokes the owning attachment transactionally.
The overload with a typed external mapper handles explicitly selected external
targets; otherwise foreign external targets reject. Ordinary numeric values are
never rewritten. The existing
[adoption API](../managed/state_registry.md#transactional-bindings-imports-and-retirement)
continues to validate native allocation reservations.

## Cross-document access and durable addressing

A process-local pointer or leased external value can provide scoped access, but
does not establish a durable managed relationship. Persisted cross-document
addressing needs a declared document identity plus the target's existing native
ID, expected type and an explicit resolution/trust/lifetime policy. A path to a
file or a coincidentally equal numeric ID does not establish document identity.

Keep same-document `managed_ref<T>` separate from any future cross-document
reference type. The latter must specify unloaded documents, deletion,
authorization, cycles and asynchronous or unavailable resolution. Existing
[external registry leases](../managed/state_registry.md#memory-and-provider-boundaries)
and [ordinary durable field addresses](../schema/references.md) retain their
documented ownership contracts; neither automatically implements this proposal.

## Apply editable values to an existing entity

Generated managed editors provide `apply_editable(candidate)`. The candidate is
the generated identity-bearing storage type, so retained owned children carry
their existing identities in both direct and separate-value profiles. The edited
entity keeps its current ID regardless of the candidate's root ID. This uses the
normal transaction, validation, notification, journal and history path. Applications
do not copy the old root ID merely to update editable values.

For declared owned collections, identify retained children by their native IDs,
retire removed children and allocate new zero-identity values through native
adoption. Reject foreign, retired or multiply owned child identities unless an
explicit move or independent import establishes their ownership. New child IDs
must be zero for this automatic operation; preallocated imports use the checked
adoption/import API. Any failure
rolls back the complete operation. One Undo/Redo restores the corresponding
payload and membership without changing surviving identities. The operation
builds temporary identity/owning-endpoint metadata; it does not clone the old
payload just to perform these checks. The caller can move its candidate.

## Bounded native editing previews

`auto proposal = store.preview();` shares the accepted immutable root and captures
its exact document/publication and allocation context. Use
`proposal.working().execute_transaction(...)` for normal generated editing, then
`proposal.cancel()` or `store.accept_preview(proposal).throw_if_failed()`.
The source store outlives the preview and every operation stays on its owner
thread. Opening copies bounded current snapshot/index metadata, without encoding
or copying retained history; it does not promise constant-time creation. Payload
edits remain isolated from the live root and history. Creating an entity reserves
its ID through the source's native allocator and journal before exposure, using
the source client's existing range when attached. Cancellation discards private
values and invalidates workspace editors; consumed IDs remain burned. A copied
token for a canceled entity cannot later target a different live entity.

Acceptance requires the exact captured live base and allocation context, advanced
only by this preview's own reservations. Admit those already reserved IDs through
one native transaction, preserving them exactly. If an intervening live edit or allocation conflict makes that unsafe,
reject the proposal without publishing or renumbering. Return live handles only
after one ordinary native transaction succeeds. One Undo removes preview-created
entities and restores previous values; Redo restores those same native IDs.

The isolated workspace disables its own history/journal/collaboration services.
Its editors never become live-store editors; acceptance closes the proposal and
callers obtain fresh live handles. Ordinary immutable pins retain their existing
snapshot-lifetime guarantee. The normal live allocator's
flush-before-exposure, no-reuse rules remain in force; preview allocation must
not compromise them. Cancellation and failed acceptance retain consumed IDs under
the existing nonreuse contract, while the accepted model/history remain unchanged.
Journals may therefore contain reservation records for canceled previews. Value-only
previews consume no IDs. Reservation metadata is bounded and charged to the shared
resident ledger; ordinary stores have no preview callback or reservation table.

## Acceptance gates

1. Compile-time wrong-type assignments reject; decoded unknown, deleted,
   wrong-type and foreign-document IDs fail before transaction publication.
2. Optional, forward, cyclic and ownership-constrained references retain their
   meaning across edits, complete decoding, Undo/Redo, Save and fresh-process
   recovery. Retained immutable readers remain valid pins of their old snapshots.
3. Collaboration uses default settings and asserts exact document IDs, native
   object IDs and reference values at source, authority and every replica through concurrent/offline
   creation, retries, reconnect and recovery. A translated native ID fails this
   required contract's qualification. A configuration that enables translation
   fails this requirement; allocation collisions must fail before exposure.
4. Independent imports remap only declared references, validate owning endpoints
   and external mappings, and publish all or nothing. Ordinary equal-valued
   integers remain ordinary data.
5. Applying editable values preserves the existing entity ID and native history;
   child moves, retirement, allocation and failure paths respect ownership.
6. Preview cancellation leaves live values/history unchanged and never recycles
   exposed IDs. Copied tokens cannot alias later entities. Acceptance requires the
   exact base and preserves allocated IDs. Stale proposals reject; discarded
   preview handles never resolve as live objects.
7. Repeat with bounded resources, journal failures and fresh-process recovery.
   No application-owned second allocator, identity repository or history engine
   is needed. Record unexecuted checks and unsupported backend combinations.
