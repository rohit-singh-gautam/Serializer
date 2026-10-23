# Durable database integration requirements

[Documentation index](../README.md) · [Design index](README.md)

Status: **proposed enhancements and integration acceptance criteria**, reviewed
2026-10-23 against compiler/runtime 2.28.2, schema language 1.21.0 and commit
`ac0d3735e7c6e0c69e28fb7a78631096a9ee73a9`. This document implements no feature,
changes no wire format and provides no benchmark or cloud qualification evidence.
It describes generic database adapters and security-sensitive managed-state
workloads. It does not turn Serializer into a database server.

Use [the central pending backlog](../pending.md#managed-state-and-storage) as
the sole implementation status and completion inventory. Most storage work below
refines deferred **PG01**, **PG02**, **DB01** and **MIG03**, rather than closing or
duplicating them. Additional public API and language changes require separately
reviewed scope and release decisions before implementation. Existing native
capability activation (**MG02**) and authorization publication (**MG03**) remain
implemented within their documented boundaries; they do not establish the future
database contracts in this document.

## Baseline and ownership

Serializer already supplies schema-driven types, codecs, stable wire identities,
managed object identity, complete-candidate transactions, optional native indexes,
history, file journals, collaboration and native capability configuration. See
[the C++ runtime](../managed/cpp_runtime.md),
[capabilities](../managed/capabilities.md),
[authorization](../managed/authorization.md),
[file journaling](../managed/journal.md) and
[database integration](../guides/database_integration.md).

Current transaction begin decodes a complete root; commit validates and encodes
the complete candidate. History stores whole-root snapshots. Delta journal frames
can reduce encoded writes without making root validation, recovery or history
lazy. Snapshot indexes are not a qualified persistent database index. The
[paged-indexing proposal](paged_indexing.md#current-serializer-limits) documents
these limits. A bounded record reader or mapped file does not establish bounded
whole-model memory usage.

| Layer | Responsibility |
| --- | --- |
| Serializer | Schema/type/identity semantics, generated metadata and codecs, managed validation, immutable handles, supported publication/recovery hooks, explicit resource accounting and errors. |
| Database engine or adapter | Physical record/page layout, WAL or equivalent recovery authority, atomic publication of data and indexes, query execution, persistent indexes, durable operation outcomes, writer serialization, compaction, backups and storage admission. A Serializer-owned backend may implement part of this under PG01/PG02/DB01; an external backend must meet the same adapter contract. |
| Host application | Authentication and principal binding, security policy, audit meaning, business commands, resource ownership, credential/cryptographic key lifecycle and external effects. |
| Deployment | Cloud provider and SDK selection, volume/object-store guarantees, writer fencing and failover, provider retention/versioning, encryption key service, monitoring and recovery objectives. |

The [application responsibility contract](../project/application_responsibilities.md)
continues to apply. Database page placement, a particular B+ tree or LSM design,
cloud consensus, authorization policy, an identity provider and audit archival
policy must not become mandatory Serializer core dependencies. Missing runtime
hooks remain library work, rather than being relabeled as application policy.

## C++23 and one qualified dependency source

The inspected CMake baseline is **C++20**, preserving a consumer's newer mode.
Compiling one consumer in C++23 does not upgrade the library's supported minimum.
The proposed upgrade must cover compiler, runtime, managed runtime, generated C++
and installed targets with C++23 as the minimum, accepting newer standard modes.
Require explicit standard-library/toolchain checks, synchronized Windows/Linux
wrappers and matching package/exported target requirements. Preserve public API
names, schema identities, generated names and bytes unless a reviewed migration
explicitly changes them. Follow [CMake integration](../guides/cmake_integration.md)
and [wire compatibility](../serialization/wire_format.md).

Raising the minimum removes C++20 build compatibility. Decide the release and
migration policy with the maintainer before publishing it; the existing
minor/patch compatibility rules cannot silently waive that loss. A toolchain
upgrade alone does not require a schema-language or payload revision bump. New
schema syntax does require the language's own versioning and editor qualification.
This design does not bump any release or select a future version.

A combined application build must select one Serializer source revision and one
compatible runtime/generator/generated-header set, including dependencies used by
diagnostic providers. Reuse existing exported targets or a supported source-dir
override; detect divergent duplicate runtimes rather than linking incompatible
copies. Record source commits, build options, compiler/standard library and
generator provenance in qualification artifacts. Independent checkouts are not
automatically synchronized.

## Prepare, durable acceptance and publication

Expose a supported database boundary with the following semantics. The phase
names describe a proposed contract, not existing C++ symbols or fault hooks.

1. **Prepare:** validate a bounded candidate and all required dependencies against
   an exact base revision; produce immutable change/identity metadata and reserve
   all resources needed for publication. Failure has no externally accepted
   database effect. The prepared state pins its inputs and has explicit ownership,
   cancellation and lifetime rules.
2. **Accept durably:** the database atomically persists the authoritative data
   transition, index mutation facts or sufficient reconstruction references,
   identity high-water marks and operation outcome under its commit protocol.
   Derived index pages may be materialized later; gate queries on a matching
   indexed-through revision or use a correct authoritative fallback until then.
   This does not require flushing physical index pages with every commit. Include
   expected base revision and writer epoch. A flush or conditional publication
   must satisfy the selected durability profile before reporting success. No
   second independent Serializer journal may claim a conflicting authoritative
   commit decision.
3. **Publish:** after proven acceptance, install the prepared immutable state
   without allocation, user callbacks or throwing work. Observers and diagnostics
   run outside this critical publication step. A publication-process crash is
   resolved from durable state, not by rolling back an accepted transition.
4. **Recover or reconcile:** restore the exact accepted state and outcome without
   replaying authentication callbacks, business effects, audit sends or diagnostic
   callbacks. Reapply the required live capability/policy configuration before
   admitting requests. Validate namespace, schema/profile, generation, epoch and
   sequence bindings before publishing recovered state.

The internal `journal_sink` and bounded frame helpers are useful foundations,
not a supported general database prepare/accept API. The
[prepared/confirmed Save contract](../managed/document_origin.md) addresses
external document Save and must not be assumed to implement arbitrary database
transactions. Define an explicit supported adapter surface and ownership model.
Never implement production I/O through test fault hooks or make a diagnostic sink
part of the durable commit decision.

An uncertain write outcome must fence incompatible mutation until the database
resolves it. A durable operation ID binds the document namespace, authority epoch,
caller request digest and original outcome. Exact retry returns that outcome;
ID reuse with a different request rejects. Host-visible idempotency keys may need
longer retention or cross-epoch reconciliation than internal operation IDs.
Specify both horizons and reject expired retries that cannot be proven safe.
Lost acknowledgements, process restart and replacement-writer recovery must not
accept a duplicate logical transition.

## Structured changes and index metadata

Expose a versioned, immutable structured change feed at the prepared transaction
boundary: created/deleted entities, changed fields, collection operations, old/new
values or bounded accessors, object versions, namespace, base/result revision,
schema identity and operation ID. Define reference lifetimes, maximum output
bytes, ordering, coalescing and cancellation. Host adapters need not reverse
engineer full snapshot byte differences. Change feeds are not durable audit logs;
the host defines audit events and persists them in its authoritative transaction.

Propose generated metadata for stable field paths, type/optionality, comparator
semantics and index projection dependencies. A future index declaration grammar
must explicitly cover unique/nonunique keys, compound ordering, absent/null
handling, collections and schema evolution. **No new index annotation, generator
flag or `managed(...)` schema selector is introduced here.** Review grammar,
language version, old-schema compatibility and every affected editor/backend
before implementation. Native `state_registry` metadata and application keys do
not automatically define persistent database indexes.

The database owns physical indexes and query plans. A prepared transition must
allow it to validate uniqueness and update data/index roots under one atomic
acceptance boundary. Comparator versions, index definition hashes, source
generation and indexed-through revision must prevent reuse of a stale or
incompatible sidecar. Rebuildable indexes must remain distinct from authoritative
recovery records, as required by [PG01's design](paged_indexing.md).

## Incremental managed storage

PG02 needs immutable per-object records or an equivalent storage abstraction,
per-object/contribution versions, lazy typed handles, pageable collections and
bounded cursors. Reads must pin the exact snapshot and keep object addresses,
borrowed values, references and iterators valid for documented lifetimes. A
bounded cache may evict unpinned pages; a live handle cannot silently observe a
new revision after eviction/reload.

Change validation must retain the current complete-candidate semantics. Build an
explicit dependency set for identities, ownership, required references, affected
invariants and index constraints. Incremental validation must be equivalent to
the full validator for the supported schema subset; if an invariant requires an
unbounded scan, declare the cost or reject the bounded profile. Do not skip a
global invariant merely because its objects are not resident. Verify dependencies
against the acceptance base under the writer's serialization boundary.

Keep whole-root operation as the compatible fallback. Paging only secondary
indexes or applying delta journal encoding does not satisfy this requirement.
Measure decoded bytes, touched objects/pages, allocated and resident bytes,
write amplification and physical storage as total model/history size grows while
the modified set stays fixed. Bound eager fallback explicitly.

## Security-sensitive profiles and current authority

Support a documented integration profile using native disabled history and
required policy configuration. Exclude ordinary Undo/Redo, tree checkout, branch
merge and unreviewed history conversion for security-sensitive authoritative
state. Configure and validate the profile before initial exposure and after
recovery; persisted content must not grant itself permission to weaken it.
Existing native capability selection is available, but the database adapter's
reopen, restore and migration enforcement still needs qualification.

Revocation, consumed credentials, allocated IDs, settlement facts and durable
idempotency outcomes must survive business reversals and compaction. The host
expresses reversals as explicit commands preserving those facts. Disabled history
alone cannot prevent an administrator from restoring an obsolete backup. Expose
namespace/epoch/high-water metadata so the host can enforce its restore gate,
invalidate credentials or reconcile independently retained evidence. Never claim
that lost post-backup facts can be inferred from old payloads.

Expose exact revision and authority-epoch tokens on immutable snapshots and a
supported way to acquire/validate the current accepted head in the selected
adapter. Distinguish historical content reads from admission against current
authority. The database establishes ordering with concurrent mutations and writer
fencing; the host decides authorization and binds principal/context. An old valid
snapshot must not be represented as proof that a new request sees current policy.
Replay/recovery does not reauthorize previously committed operations, and must
not admit new ones until current authority is established.

## Recovery, retention and erasure

Specify bounded checkpoint/replay batches, cancellation, malformed-record checks,
maximum startup/recovery work and resumable verification for a selected storage
profile. Recovery must not eagerly materialize all retained history while claiming
a lazy memory ceiling. Checkpoint changes must retain the durable operation/ID
facts needed after restart; a derived index can be rebuilt without changing them.

Snapshot pins, reader cursors, prepared candidates, replicas, backups and retained
history need explicit retention leases or an equivalent ownership contract. Report
which roots prevent reclamation, and bound pinned bytes, lease count/age and
compaction workspace. Reject admission or require explicit host action when
configured limits are reached. Never reclaim a page still reachable by a valid
handle. Define a safe cursor-expiry response rather than silently skipping data.

Expose a storage dependency inventory: live and obsolete records, journal and
checkpoint generations, retained revisions, snapshots, diagnostic payloads and
external object references. This lets an adapter report physical copies that may
contain deleted material. Logical deletion, no-history configuration and payload
compression do not erase old blocks, backups or provider object versions.

The host/deployment owns erasure policy, cryptographic key destruction, backups,
cloud retention, credential secrecy and audit archival. Serializer should expose
bounded reclamation and inventory hooks, avoid retaining unneeded secret copies,
and document zeroization limits; it must not promise physical-media erasure or
cryptographic deletion without a qualified owner/key protocol. Host audit/outbox
watermarks and archival receipts must be persisted before pruning protected data.

## Diagnostics and measurement

Provide optional structured hooks or error/event adapters for transaction phases,
recovery, rejected limits, cache/lease pressure and uncertain outcomes. Keep them
bounded, redact payload/credential material and document thread/reentry rules.
The consuming application chooses its diagnostic library. Do not introduce a
mandatory dependency cycle when that library itself uses Serializer.

No hook may change acceptance, throw through nonthrowing publication or block
indefinitely on an exhausted queue. Report dropped events through bounded
counters or another documented policy. Measure disabled, enabled and saturated
diagnostics with real producer-thread/queue limits. Operational diagnostics,
transactional audit and database recovery records have different owners and
durability semantics.

## Acceptance and incremental delivery

These are design acceptance criteria linked to existing work, not a second
completion backlog. A future implementation must record exact versions, executed
commands, measured limits and known exclusions in verification records.

| Contract | Required evidence | Existing work / boundary |
| --- | --- | --- |
| C++23 and dependency coherence | Windows/MSVC and Linux/GCC or Clang generation/build/install/consumer tests, newer mode retained, unsupported toolchains rejected, legacy API/wire fixtures unchanged, one runtime/generator across a combined dependency build. | Cross-cutting release decision; no upgrade implemented here. |
| Prepare/accept/publish | Faults before/after every durable boundary, allocation failure during prepare, publication with allocation/callback guards, crash after acceptance before publication, recovery to precisely accepted revision. | DB01; integrate native managed validation rather than bypassing it. |
| Operation identity | Lost acknowledgements and exact retries before/after restart, request mismatch, epoch changes, dedup expiry and uncertain-outcome fencing. | DB01/MIG03 and host idempotency policy. |
| Structured changes/index metadata | Feed equals full before/after semantics, bounded lifetime/bytes, uniqueness races rejected atomically, stale comparator/schema/generation indexes rebuilt or rejected. | PG01/DB01; proposed metadata grammar separately reviewed. |
| Incremental objects/collections | Lazy read/edit/reopen with data larger than cache, evictions under pinned handles, reference/invariant parity with full validation, fixed-change-set scaling and explicit fallback limits. | PG02, depending on PG01 lifetime/recovery design. |
| Current authority and security profile | Historical-reader rejection for current admission, revocation ordering races, all navigation entry points denied, no ID/consumption/settlement resurrection after reopen/compaction/restore. | Existing MG02/MG03 plus DB01 and host restore/authorization policy. |
| Recovery/retention/erasure | Interrupted checkpoints/compaction, corrupt and truncated records, replay/peak-memory bounds, expired/stalled pins and replicas, retained-copy inventory, pruning only after safe watermarks. | PG01/PG02/MIG03; physical erasure/key/provider claims qualified by their owners. |
| Diagnostics/resources | Queue exhaustion, first-event registration, sink failure, shutdown, redaction and disabled/enabled timing; complete storage and peak compaction accounting. | Optional adapter hooks and consuming application's diagnostic integration. |

Deliver a C++23-compatible build and a small durable adapter vertical slice first:
prepare one bounded transaction, atomically store the host's state/audit/outbox/
idempotency records, accept durably, interrupt, reopen and safely retry. This
establishes correctness using explicit whole-root size limits, not incremental
performance. Then expose structured changes and derived index bindings, implement
paged/lazy objects with validation parity, and measure retention growth and
diagnostic saturation. Qualify the selected database/deployment failure profile
only after those mechanisms exist. Optional editing history and collaboration can
be qualified separately from security-sensitive durable state.
