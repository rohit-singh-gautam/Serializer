# Collaboration sessions, presence, and edit locks

Status: design proposal only. These records, interfaces, lock policies, and runtime
components are not implemented. See the [managed design index](README.md) and
[capability contract](capabilities.md). Locking and editing presence are optional
collaboration policies, not additional `managed(...)` keywords.

## Scope and session identity

Serializer coordinates opaque collaboration sessions. It does not require users,
accounts, display names, invitations, or a login mechanism. An application may map
a session to a person, device, service, or automation. Two sessions belonging to
one person remain separate lock owners. A replica identity can survive reconnects;
a session identifies an active participation lifetime and is a different concept.

The host binds incoming requests to a trusted session context. A claimed session
ID in a decoded record is not proof of ownership or permission. Authorization
remains a separate provider-backed gate; the host may associate a session with
whatever access context its policy needs. Session identifiers and grant counters
do not inherit the configurable `uint32` managed entity ID width.

The collaboration domain identifies a document and the shared editing context,
such as its live branch. Separate private branches need not share locks. A merge
into the shared branch must pass the target domain's current lock and permission
checks. Persistent entity IDs identify targets within that domain.

| Serializer responsibility | Application/adapter responsibility |
| --- | --- |
| Typed records, bounded decoding, validation, deduplication, and explicit outcomes | Transport, connections, routing, retries, and authenticated session binding |
| Atomic candidate validation/publication and lock state transitions | Hosting the authority; leader election or consensus when deployment requires it |
| Presence and lock tables, ordered updates, snapshots, and expiry transitions | Driving timers and delivery through supplied interfaces; choosing expiry policy |
| Change/presence/lock sinks and local notifications | UI, identity management, and external service administration |

Sinks carry records; successful delivery is not authoritative acceptance. Serializer
does not open network connections or implement a distributed consensus service.
These rules govern managed APIs, not arbitrary writes to public objects or memory.

## Three independent record streams

| Stream | Meaning | Lifetime |
| --- | --- | --- |
| Accepted change batch | An atomic, validated model transaction | Retained as required by synchronization, history, or journal policy |
| Editing presence | A session reports what it is editing or previewing | Temporary; never grants edit permission or blocks another session |
| Lock state | The authority grants, releases, revokes, or expires exclusive editing rights | Temporary coordination state, outside document undo |

Conceptual record fields below are not finalized wire layouts. Define concrete
data classes in `.serializer` and generate all language representations when the
protocol is implemented. Runtime engines, indexes, providers, and guards wrap
those generated records; do not present handwritten runtime classes as compiler
output. The current [walkthrough schema](walkthrough.serializer) does not yet
include these collaboration records.

An envelope carries document/domain identity, protocol/schema/profile version,
and an authority epoch where applicable. Domain identity need not be repeated on
every entry inside a batch.

```text
change_proposal
  operation_id + session_id + expected_base_versions
  atomic entity insert/update/delete records + stable field paths
  proposed values or resolvable version references + relevant grant references

accepted_change_batch
  operation_id + authority_sequence + resulting_versions
  accepted changes + provenance + explicit resolution decisions

editing_presence
  session_id + presence_id + update_sequence
  target_entity_id + optional stable field path
  begin/update/end + bounded optional preview metadata

lock_grant
  target_entity_id + scope(entity or owned_subtree)
  owner_session_id + grant_generation + lease_id

lease_record
  lease_id + owner_session_id + authority_monotonic_deadline

lock_update
  authority_epoch + lock_sequence + grant/release/revoke/expire
  affected grant reference + grant data when applicable
```

Authority-local monotonic deadlines are runtime values, not portable timestamps.
Recipients may receive a remaining lease duration as a hint; they cannot use a
local wall clock to declare authoritative expiry. A grant reference binds the
domain, epoch, target, and generation. Sequence/generation exhaustion must fail
or rotate through an explicit protocol transition, never silently wrap.

## Proposed interface responsibilities

These are language-neutral operation names, not existing APIs or synchronous
network calls. Remote outcomes can be pending until the host delivers a response.

| Operation | Required result or behavior |
| --- | --- |
| `submit_change(proposal, trusted_context)` | Accepted, conflict, denied, obsolete grant, or pending/indeterminate until reconciled |
| `apply_accepted_change(batch, trusted_authority_context)` | Validate origin, ordering, schema, dependencies, and atomic application; deduplicate repeats |
| `subscribe_accepted_changes(sink)` | Emit accepted batches; expose delivery failure separately from commit outcome |
| `publish_editing_presence(record, sink)` / `receive_editing_presence(record, context)` | Maintain temporary session state and ignore obsolete updates |
| `acquire_edit_lock(target, scope, context)` | Grant, conflict, denied, or pending; no implicit success on send |
| `renew_edit_lock(grant, context)` / `release_edit_lock(grant, context)` | Match the current grant; stale release cannot remove a newer grant |
| `receive_lock_update(record, authority_context)` | Apply ordered authoritative updates or request resynchronization on gaps |
| `advance_expiry(now)` | Authority processes due leases; host supplies a monotonic clock/timer contract |

Mutating requests need operation IDs and defined retry/deduplication semantics.
Repeating an ID with a different payload is an error. A duplicate acquisition
returns the recorded outcome, not an additional lock or extended lease; callers
must still establish that a returned grant remains current. Queuing a release
locally does not prove the authority received it.

Local optimistic candidates stay separate from acknowledged state. Rejected or
uncertain submissions must reconcile by operation ID before a caller claims
success. Applying a trusted accepted batch is distinct from authorizing a new
local edit: a read-only session must still receive permitted replicated changes.
Validate the trusted acceptance and any local replication policy, rather than
requiring the observing session to have authored-edit permission. A replica does
not reject an earlier accepted batch merely because its author's lock later
expired. Do not bypass authority checks by labeling an untrusted proposal accepted.

## Editing presence is advisory state

Advisory means informational and non-exclusive. Serializer manages and exchanges
the state; the application chooses how to use it. Presence does not mutate the
design, enter undo history, or establish lock ownership. An end event, session
termination, or receiver expiry removes the corresponding active presence entry.

Order updates per session/presence lifetime. Retain sufficient sequence watermarks
or retired-lifetime markers to prevent delayed begin/update messages resurrecting
an ended or expired entry; reclaim them only under a defined session/protocol
boundary. Reconnect starts a fresh lifetime or explicitly resumes one through the
host. Coalesce frequent updates, bound preview payloads and table size, and allow
presence to be dropped under backpressure without dropping committed model data.
Presence referring to deleted targets is cleared or reported as unavailable.

## One logical lock authority

Start with one logical authority for each collaboration domain. The authority
serializes lock acquisition/release/expiry and model publication within one
ordering boundary. Lock validation and accepted publication are atomic relative
to those transitions. A separate check followed by an unprotected write is unsafe.

Example, using sessions rather than user identities:

1. Session A requests an owned-subtree lock on entity 42.
2. The authority validates permission and overlap, and grants generation 17.
3. It emits a sequenced lock update; replicas cache the grant.
4. Session B's conflicting edit is rejected even if its cache missed the update.
5. Session A submits a transaction referring to grant 17. The authority validates
   the current grant, base versions, complete affected scopes, and domain rules.
6. Release or expiry ends grant 17. A delayed edit or release using it cannot
   affect a subsequent grant.

An entity lock covers that entity's ordinary values, including unmarked children.
An owned-subtree lock additionally covers managed descendants and future insertions
under the locked subtree. Arbitrary reference edges do not imply ownership.
Replacing/deleting a parent affects its descendants; moves check source and
destination scopes. Undo, redo, checkout, and merge check their complete effective
changes. `exclude(history)` or per-member feature selectors cannot bypass an
active lock or required authorization on an affected protected scope.

Choose an explicit domain policy: edits may be permitted where no conflicting
grant exists, or every edit may require a covering exclusive grant. Initial
acquisition is fail-fast, with no waiting queue, upgrades, or overlapping grants
even for one session. A holder can reuse a covering grant; exact acquisition
retries are idempotent. A subtree acquisition conflicts with locks anywhere in its
subtree or a covering ancestor. An entity acquisition conflicts with a lock on
that entity or a covering ancestor subtree; structural edits can affect more.

Checking ownership inside the protected transaction, with lease-based release,
has a public precedent in the [etcd concurrency API](https://etcd.io/docs/v3.5/dev-guide/api_concurrency_reference_v3/).
This is a design reference, not a required Serializer dependency.

## Replica lock caches and failure handling

A joining replica receives an authoritative lock snapshot at sequence S and then
updates after S. Register/capture that boundary atomically or buffer updates so
joining cannot miss a transition. Ignore duplicates, detect gaps, and resynchronize
when replay is unavailable. Track whether the cache is complete; absence from an
incomplete or stale table does not prove a target is unlocked. Local cached checks
provide fast feedback; only authoritative acceptance establishes a shared edit.

Several grants may share one lease so one renewal covers an editing session.
Authority expiry, revocation, and session shutdown release its indexed grants.
Renewal after expiry cannot revive an old generation. C++ guards can enqueue
best-effort release on destruction without waiting for network I/O; explicit
release reports completion. Lease expiry handles crashes and failed delivery.

Authority recovery must either safely restore grants with a valid lease contract
or invalidate them under a fresh epoch. An epoch alone does not prevent two
authorities from running: the deployment must fence the previous authority so
its new publications are no longer accepted. Consensus, durable epoch allocation,
and leader replacement are host/adapter responsibilities when required. Serializer
checks supplied epochs and grants at acceptance; clients cannot establish these
guarantees by broadcasting lock announcements to one another.

During a partition, a disconnected replica may retain a private draft. It cannot
independently grant globally exclusive locks or declare its edits accepted by the
shared authority. Global exclusivity and unrestricted disconnected acceptance are
not simultaneous promises of this design. Reconnect requires state reconciliation
and valid grants, rather than trusting the old local lock table.

## Storage, indexes, and performance targets

The optional store-owned `collaboration_state` holds grants, leases, presence,
pending proposals, acceptance/deduplication results, stream cursors, and derived
indexes. Do not embed session or lock objects in each generated payload. Document
history and the [document journal](journal.md) do not restore live presence or
editing rights. Any durable authority coordination log is a separate contract.

| Concern | Initial implementation target |
| --- | --- |
| Direct entity lookup | Active-grant hash index: expected constant-time lookup, not a worst-case guarantee |
| Covering subtree lock | Walk the existing ownership parent index: work proportional to depth |
| Descendant conflict | Maintain descendant-lock counts on relevant ancestors; reject overlap without scanning the whole subtree |
| Lease release/expiry | Index grants by lease; use a deadline queue rather than scan every entity |
| Frequent setters | Reuse transaction session/grant context; no network call or new lock allocation per setter |
| Acceptance | Deduplicate affected entities/scopes; atomically revalidate current grants and policy at publication |
| Communication | Compact IDs within a domain envelope, batch changes/renewals, coalesce presence |
| Memory | Reuse buffers and pool records; allocate feature tables only when required |

Descendant counts accelerate existence checks, not identification of every
conflicting grant. Counters and ownership indexes must be updated transactionally
on lock changes, deletion, and moves; tree relocation has additional cost and
cannot claim unconditional constant time. Guard arithmetic against overflow and
retain correctness on allocation failure. The simplest implementation may reject
a structural operation that cannot preserve these invariants.

Validate any cached grant result against relevant lock, ownership, and policy
generations; setter-level checks never eliminate final acceptance checks. Lock
coordination does not replace transaction isolation, domain validation, or thread
safety inside a process. Disabled collaboration should impose no per-setter network
or lock-table work; exact object-size overhead depends on the language backend.

Measure setter/commit latency separately from network acceptance latency. Benchmark
bytes per active grant/session, allocations per transaction, update bandwidth,
deep and wide trees, contention, moves/deletes, and p50/p95/p99 latency. Compare
disabled collaboration against the ordinary managed baseline. These are acceptance
targets, not measured speedups or a claim of universal maximum efficiency.

## Verification required before implementation claims

- Competing sessions cannot obtain conflicting grants, including two sessions
  mapped by the application to the same person.
- Forged session/grant metadata fails trusted-context checks; read-only replicas
  still receive valid accepted changes without acquiring authorship permission.
- Acquisition, expiry/revocation, and publication races preserve one atomic order.
- Old grants, releases, authority epochs, and reordered/duplicate requests cannot
  revoke a newer grant, revive expiry, or apply a transaction twice.
- Snapshot-plus-update handoff detects gaps; partitions never turn cached absence
  into authority. Reconnect, authority replacement, and lease loss reconcile.
- Ancestor/descendant locks cover insert, delete, replace, move, undo, and merge;
  allocation failure leaves indexes, counts, and ownership consistent.
- Presence expiry/end and delayed updates preserve lifecycle ordering; presence
  traffic can be coalesced without losing accepted changes or lock transitions.
- Save/reload and journal/history recovery never resurrect editing rights.
- RAII cleanup is nonblocking and nonthrowing; explicit completion and failed
  release are observable, with expiry providing the crash fallback.
- All generated-language adapters preserve the same decisions and identifier
  widths, with bounded decode/table growth and measured performance budgets.
