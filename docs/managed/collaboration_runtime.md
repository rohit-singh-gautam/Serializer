# C++ collaboration runtime

Status: implemented single-authority, snapshot-based C++ collaboration. Enable
`SERIALIZER_BUILD_MANAGED=ON` (the default), link `Serializer::managed`, and include
`<rohit/managed_collaboration.hpp>`. Regenerate managed model headers to obtain
the `visit_collaboration`, `visit_collaboration_fields`, and `merge_collaboration`
traits. Existing schema
syntax, persistent IDs, model wire bindings, and save formats are unchanged.

Start with the [runnable examples](../../example/managed/collaboration/README.md).
For normal local transactions and timed synchronization, start with
[model_store integration](local_collaboration.md). It owns the client session and
outbox, reuses store transactions/journaling, and supports independent send/receive.
The authority and acknowledged-state replica primitives below remain available.
The [broader design](collaboration.md) separately describes future optimization,
delta protocols, distributed deployment, and other-language engines.

## Authority, commands, and replicas

`collaboration_authority<Root, Mode, Labels, Traits, Session, SessionTraits>` privately owns a managed
store. Its history arguments match `model_store`; collaboration also works with
disabled history. Default direct values, opt-in separate values, and uint32/uint64
model identities use the same interfaces. The appended session arguments default to
`std::uint64_t` and its built-in policy. A `collaboration_replica<Root, Traits, Session, SessionTraits>` holds
only acknowledged state, without local undo history. Proposal drafts are separate;
the authority decides whether a session may write.

Ordinary shared editing needs no handwritten command schema, action enum, decoder,
or dispatcher. Construct the authority without a handler and call `replica.propose`
with the same generated editors used for local transactions. The callback runs once
against an isolated draft; it is not transmitted or run remotely. After host delivery,
the runtime applies the resulting model snapshot through the authority's transaction, identity,
final-diff authorization, locking and journal gates.

```cpp
using authority_type = rohit::managed::collaboration_authority<ledger_example::ledger>;
authority_type authority{ledger_example::ledger{"Shared"}, 1};
authority.open_session(10);          // Host binds this ID to an actual session.
authority.open_session(20, false);   // May observe, cannot author edits or acquire locks.

rohit::managed::collaboration_replica<ledger_example::ledger> replica;
const auto binding = authority.context();
replica.synchronize(authority.snapshot(), binding);

const auto proposal = replica.propose(10, 1, [](auto& edit) {
  edit.root().set_name("Renamed ledger");
  edit.root().entries().insert(100, {"Supplies", 1500});
});
auto result = authority.submit_change(proposal, 10, monotonic_ms);
if (result->status == rohit::managed::collaboration_status::accepted) {
  replica.apply_accepted_change(*result->accepted, binding);
}
```

`propose(session, operation, callback, grants = {}, options = {})` leaves the
replica's acknowledged state unchanged. Exceptions, poisoned drafts and explicit
`revert()` return no proposal. A no-op draft may be submitted and gets `no_change`.
New IDs are provisional until acceptance; do not publish them as durable references
before that result. The payload binds the base allocator watermark as well as the
accepted sequence. If a rejected operation reserved IDs, refresh through
`snapshot`/`synchronize` even when the accepted sequence has not advanced. The stale
watermark produces `conflict`; it never reuses the reserved IDs. Authoring defaults
to the standard collaboration budgets; pass matching custom options when the host
changes them. A replica may construct bytes for any claimed session, but only the
authority's separate trusted-session and writable checks grant permission.

With default uint64 sessions, the model command uses collaboration protocol **6**, with a generated
`model_change` payload at format version **1**. The existing constructor taking a
`command_handler` uses protocol **5** for application-defined commands. The domain
binding rejects mixing these endpoints, including with older custom-command
authorities. `propose` requires a model-command baseline. Other session types/policies
use protocols **7** (custom commands) and **8** (model proposals). Replicas and lock
caches accept the two protocols corresponding to their session configuration;
custom-command callers still build their own
application records and handlers. See the
[custom-command regression fixture](../../test/resources/ledger_commands.hpp).
Use a custom handler when the server must interpret a business operation rather
than accept a proposed final model. Host policy callbacks remain authoritative in
both modes. No callback may retain mutable aliases or perform external effects on
the assumption that its candidate will be accepted.

`encode_collaboration_record` and
`decode_collaboration_record<T>` use bounded `binary_integer` codecs; the latter
requires exact consumption and accepts `serializer::decode_limits`. Apply input
limits at the transport boundary before constructing an untrusted record. Record
methods on default uint64 records also work with the normal generated codecs.
Typed session records use these collaboration helpers to encode/decode their
generated envelope and inner payload together.

The second `submit_change` argument is the **host's trusted session binding**,
independent of the claimed session in the decoded message. The final argument is
host monotonic milliseconds, never a client timestamp. The runtime checks both
bindings but cannot authenticate a transport or determine whether a host supplied
the truth. The host likewise authenticates the authority before calling replica
methods; passing a decoded record's own context as the trusted argument provides
no authentication. Domain bindings include the selected protocol version, model schema ID,
entity-ID width, document namespace, and a nonzero authority epoch.

Each proposal uses the document's exact accepted `base_sequence`. Concurrent
proposals from the same base conflict after the first accepted change, including
edits to unrelated entities. Reconcile against acknowledged state, then issue a
new operation ID. There is no automatic merge, callback retry, offline acceptance,
or per-entity version conflict resolution in this version. Newly inserted entity
IDs are allocated at the authority; rejected reservations are never reused.

## Application-owned sessions

Applications decide how sessions are created, stored, associated with connections,
and retired. The runtime never allocates session IDs, creates accounts, or imposes
a mapping to uint64 handles. It retains only collaboration participation, rights,
retry keys, locks and presence required to coordinate registered IDs.

Existing `collaboration_authority<Root>` and `collaboration_replica<Root>` use
`std::uint64_t`. For another ID type, select the whole API together:

```cpp
using sessions = rohit::managed::collaboration_session<std::string>;
sessions::authority<ledger_example::ledger> authority{ledger_example::ledger{"Shared"}, 1};
sessions::replica<ledger_example::ledger> replica;
sessions::lock_cache locks;

// Application-provided ID, already bound to a connection by the host.
const std::string session = "desktop-session";
authority.open_session(session);
replica.synchronize(authority.snapshot(), authority.context());
auto proposal = replica.propose(session, 1, [](auto& edit) {
  edit.root().set_name("Updated ledger");
});
const auto bytes = rohit::managed::encode_collaboration_record(proposal);
auto received = rohit::managed::decode_collaboration_record<
    sessions::records::change_proposal>(bytes);
// Supply the ID from the trusted connection binding, not received.session.
auto result = authority.submit_change(received, session, 0);
```

The session type is preserved in proposals, accepted origins, lock owners, presence,
retry keys, trusted-session arguments, and authorization hooks. Choose `std::uint32_t`
the same way. Unsigned integer policies and `std::string` are built in; the string
policy compares complete case-sensitive UTF-8 values and bounds each encoded ID to 4096
bytes. There is no hashing or numeric alias allocation. The [session example](../../example/managed/collaboration/sessions.cpp)
round-trips both string and uint32 sessions through the actual codecs.

For an application type, pass
`collaboration_session<app_session, app_session_policy>`, or specialize
`collaboration_session_traits<app_session>`. The type must be default-constructible
and copyable. The policy contract is:

| Member | Required behavior |
| --- | --- |
| `wire_name` | Stable, nonempty format name agreed by peers; change it when encoding/meaning changes. |
| `max_encoded_bytes` | Maximum encoded size of one session ID. |
| `valid(const Session&) -> bool` | Decide which application IDs may participate. |
| `less(const Session&, const Session&) -> bool` | Stable strict weak ordering; equivalent values represent the same identity. |
| `encode(const Session&) -> vector<uint8_t>` | Deterministic, bounded, canonical encoding of the complete identity. |
| `decode(span<const uint8_t>, decode_limits) -> Session` | Exact bounded decoding, rejecting malformed/trailing bytes. |

Equivalent IDs must encode identically; distinct IDs must have distinct encodings.
Validation and comparison should be pure and nonthrowing. Session values retained
by the runtime must own their identity data; avoid dangling pointers or views.
The decoder checks format names, per-ID bounds and canonical re-encoding. The
[custom policy test](../../test/managed_collaboration_session_test.cpp) shows a type
without comparison operators or numeric conversion, including an application that
permits an empty ID. Default policies reject numeric zero and empty strings.
Join snapshots use operation 0 with a default-constructed session and carry no
authored-edit origin; custom policies need not reserve that value from participation.

Call `open_session` once for each participation lifetime and `close_session` when
it ends. An ID cannot be reused within the same authority epoch after closure;
choose a new ID (or include an application lifetime/generation in your custom ID)
to prevent delayed requests from reviving an old session. Rebinding a still-active
session on reconnect is the host's decision and requires retaining its operation
IDs and pending proposals; the runtime provides no automatic reconnect service.
Shared user/account IDs need a distinct participation component when devices or
tabs must hold independent locks and retry histories.

Session format changes use new collaboration envelopes but do not change managed
model IDs, snapshot formats or journals. The default uint64 policy keeps protocol
5/6 records without a session wrapper. Other policies use protocol 7/8 and a generated
`session_envelope` with a format tag, the original record payload with numeric
session placeholders zeroed, and the encoded session values. See the
[wire contract](../wire_format.md#collaboration-envelopes). Both endpoints must use
the same session policy; cross-format messages are rejected rather than coerced.
Typed envelopes add framing and temporary encoding buffers; the default uint64
path still uses the original generated records without this overhead.

## Outcomes and reliable delivery

`submit_change` and `change_lock` return a shared immutable `collaboration_result`:

| Status | Meaning |
| --- | --- |
| `accepted` | Published change (with `accepted` batch), or completed lock request (with `grant`). |
| `no_change` | Successful command made no model change; no accepted cursor advance. |
| `conflict` | Stale document base, overlapping acquisition, or another session's covering lock. |
| `denied` | Read-only session, required lock missing, or host policy denial. |
| `obsolete_grant` | Missing/stale grant reference, expired lease, or wrong owner/generation. |
| `invalid` | Invalid lock action, scope, or target. |
| `reverted` | Handler explicitly canceled the transaction. |
| `failed` | Command/decode/validation/allocation failure; `error` retains the exception. |
| `indeterminate` | Journal durability is uncertain; the authority is fenced until destruction and fresh-epoch recovery. |

Malformed/untrusted contexts, zero operation IDs, changed-payload ID reuse, and
admission-limit failures throw before running a command. Timers may have applied
earlier independent expiry transitions when a request subsequently fails.

Operation IDs are nonzero uint64 values scoped to a session and shared across
change and lock requests. Exact retries return the retained original outcome,
including denial/conflict/failure; they never reexecute the handler or renew a
lease. Reusing an ID with another payload or operation kind throws. A retried
successful acquisition may refer to an already expired grant: an old result is
not a claim that the grant is still current. Session IDs cannot be reused within
an epoch, even after `close_session`.

The authority retains accepted records separately from delivery. Feed
`accepted_since(cursor)` to any host sink/queue; sink failure does not change the
commit outcome. Retrying the proposal or replaying this outbox reconciles a lost
reply. Replicas apply only the next sequence, ignore older repeats, reject
conflicting content at their current sequence, and return `resync_required` for
gaps. Snapshot decoding and application are atomic. Model snapshot and cursor
capture happen together on the owner thread; retain/replay batches after that
cursor for joining replicas. `synchronize` explicitly supports a host-fenced epoch
change for the same document. Sequence and allocation regressions are rejected.

## Locks and ownership

`change_lock` consumes a generated `lock_request`; use `edit_lock_action` and
`edit_lock_scope` when constructing its numeric wire fields. Acquisition is
fail-fast and does not allow overlapping grants, even within one session.
An entity lock protects its ordinary values and owned collection membership;
an owned-subtree lock additionally covers managed descendants and new insertions.
An entity lock on a parent does not prevent an unrelated value edit on a child.
Moves that would create overlapping grants are rejected, including same-owner grants.

Generated traversal records each entity's ordinary values, child IDs, and
ownership edge (parent, field ID, and key/position). Commit compares the old and
new projections. Deletion checks every deleted descendant; relocation checks
changed ancestry, including moves between keys/positions under one parent. An
array insertion/reorder conservatively checks siblings whose positions change.
Reference edges do not convey ownership. Whole-candidate updates cannot bypass
the checks by omitting an affected-ID list from a request.

The default permits unlocked edits while rejecting conflicting grants. Set
`collaboration_options::require_edit_lock` to require coverage for every affected
entity. Supply the holder's `{target, generation}` references in the proposal's
`grants`; ownership of a lock without its current reference is insufficient.

Every grant has one authority-local lease deadline. Pass monotonic milliseconds
to requests and drive `advance_expiry(now)` during idle periods. Expiry occurs at
`now >= deadline`; clocks moving backwards and arithmetic overflow are rejected.
Renewal cannot revive an expired generation. Release matches owner, scope,
target, epoch, and generation, so a delayed release cannot remove a replacement
grant. `close_session` revokes its grants and retires presence. A deleted target's
grant remains fenced until explicit release, expiry, or session closure; IDs are
not reused. Grouped leases and an RAII/network release queue are not supplied.

`lock_snapshot()` captures the complete table at a cursor. Deliver
`locks_since(cursor)` to `collaboration_lock_cache::receive_lock_update`.
On a gap, the cache remains incomplete until `synchronize` replaces it. Missing
cached locks never grant editing rights; only the authority accepts edits. Prune
acknowledged lock events explicitly with `prune_lock_updates`; old cursors then
require a new snapshot. Lease renewals do not emit events because portable cached
grants contain no deadline and receivers cannot declare authoritative expiry.

## Presence

`receive_presence(record, trusted_session, now)` tracks advisory state independently
of model/history/locks. `editing_presence()` copies current entries for UI or a
host sink. Read-only sessions may publish presence. The generated record contains
a session, nonzero presence lifetime ID, monotonically increasing update sequence,
entity target, action, and bounded preview string. Duplicate/older updates are
ignored. An end, timeout, target deletion, or session closure is terminal for that
lifetime: a delayed update cannot resurrect it; use a new presence ID.

The authority retains retired lifetime markers until a fresh epoch. There is no
presence-change outbox; hosts may coalesce/drop intermediate snapshots. Presence
does not imply a lock, permission, or a document change. Field-specific previews
and independent peer presence caches are future extensions.

## Host authorization hooks

`open_session(id, writable)` and `set_session_writable` provide a coarse host gate.
An optional `change_policy(session, before, candidate, affected_ids)` runs on the
actual diff inside the acceptance boundary. Returning false denies the entire
candidate; an exception fails it closed. An optional
`lock_policy(session, target, scope)` gates acquisitions and renewals. Release is
still allowed after edit permission is revoked. Policy callbacks cannot reenter
the authority. The host must synchronize external policy changes with this owner
thread; a remote lookup performed earlier is not a publication-time policy check.

These hooks do **not** implement the broader [authorization design](managed_state.md#authorization-on-the-design-hierarchy):
there is no built-in inherited allow/deny policy, role model, field-path permission,
operation classification, read filtering, or policy persistence. The ordinary local
`model_store` remains independently usable without this collaboration gate.

## Conditional collaborative undo and redo

Generated model endpoints support per-session undo/redo of accepted transactions.
The authority records each accepted transaction in `accepted_change.history`;
`accepted_since` returns these records with the existing snapshots. The outer
domain, session, operation and sequence identify the document, author, grouped
transaction and accepted ordering. Custom-command endpoints also produce records,
but do not interpret the generic history requests.

Each `field_change` identifies an entity and a schema field ID, before/after
presence and codec values, and before/after contribution versions. An ordinary
field is atomic, including an unmanaged nested object, map or array. Managed
ownership fields encode only IDs and keys/order; child payload fields have their
own entity addresses. Ownership dependencies cover deletion and relocation,
including ancestry changes. Unchanged descendant fields are retained as guards
when reversing a move could discard their later edits.

Contribution versions identify the currently active edit, not a hash of its value.
Normal edits use their accepted sequence. Undo restores the earlier contribution
version, allowing multiple consecutive undos and redos on the same field. A normal
write away and back still has a different version and conflicts. The accepted
sequence always increases, including for undo/redo.

```cpp
auto request = replica.undo(session, next_operation); // Optional target operation and grants follow.
auto result = authority.submit_change(request, trusted_session, monotonic_ms);
if (result->status == rohit::managed::collaboration_status::accepted) {
  replica.apply_accepted_change(*result->accepted, trusted_authority);
}
// redo(session, next_operation[, target_operation, grants]) follows the same delivery path.
```

The inverse is computed from the authority's retained transaction; the client never
supplies inverse values or permission to resurrect IDs. `undo_operation(session)`
and `redo_operation(session)` expose the corresponding stack tip, or zero when
empty. They are inspection APIs, not permission or success guarantees. An optional
nonzero target must match that tip; arbitrary historical selection is unsupported.
A zero target uses the current tip, which must already be visible at the request's
base sequence. An unseen newer tip causes a conflict; it does not select an older edit.

Undo/redo is a new transaction with a fresh operation ID, current policy and lock
checks, journal flush, and ordered replication. Unrelated intervening changes may
remain even when the request's document base sequence is stale. Changed field
versions, ownership dependencies, and structural membership must still match.
For example, Alice can undo an entry memo while retaining Bob's amount edit, but
cannot undo creation of an entry after Bob edited it. Restoring a deleted object
retains its original identity without lowering the allocator watermark. Reusing
the old key for a new object conflicts. Map membership and array ordering are
currently checked at the containing field level, so even disjoint membership
changes can block structural undo; this is not a general merge engine.

One conflict rejects the complete transaction. Denied, failed, reverted and no-op
operations leave history unchanged. Successful normal edits clear only that
session's redo stack. Undo pushes its accepted inverse onto redo; redo reverses
that inverse and pushes onto undo. Exact retries return the original result, even
if permissions later change. Uncertain journal writes fence the authority. See
the [store undo example](../../example/managed/collaboration/undo.cpp), which invokes
this acceptance path through `model_store::undo`, `redo`, and `synchronize`.

Replicas remain acknowledged-state receivers: these methods build requests and
do not immediately edit a local view. Per-session stacks, retained before snapshots
and version tombstones last only for the bounded authority epoch. Journals recover
the resulting document, not these authority collaboration records. The separate
[store client](local_collaboration.md) implements immediate local undo, a durable
outgoing queue and caller-driven periodic sync. It persists client history across
client restarts; authority history and deduplication still do not span epochs.

## Durability, limits, and current costs

`save_document`/`load_document` preserve the ordinary managed history envelope.
`create_journal`, `save_journal`, and `recover_journal` compose with synchronous
appended/sidecar document journaling. Accepted outcomes follow the existing journal
flush. Restore/recover into a fresh authority before opening sessions, using a new
host-fenced epoch. Recovery never reruns editing callbacks or restores sessions,
presence, grants, retry results, collaborative undo stacks, contribution versions,
or collaboration cursors. A lost reply spanning
an authority crash therefore requires snapshot/application reconciliation: this
version does not promise durable exactly-once command processing across epochs.

The authority is confined to its creating thread. All operations and host policy
changes run there; reentrant mutation fails. Replicas are also thread-confined;
serialize access to the separate lock cache externally. Lease expiry and publication
have one order rather than a check-then-write race. A new epoch is not leader
election: the deployment must fence any previous authority and its publications.

`collaboration_options` bounds sessions (including closed IDs), retry records and
retained payload bytes, commands, projected entities, active locks, lock outbox,
presence lifetimes, previews, and lease durations. Retry outcomes/accepted snapshots
are retained for the epoch: reaching a limit rejects new work without evicting
deduplication evidence. Provision budgets for the workload and deliberately replace
the authority under a new fenced epoch before capacity is exhausted. There is no
acknowledged operation-compaction protocol yet. Presence tombstones likewise count
toward their lifetime budget. A full lock outbox must be pruned before further
transitions; earlier expiry transitions can already have completed.

The authority's optional seventh `Features` argument defaults to `store_features::all`;
`store_features::collaboration` compiles out its journal storage and API. Authentication
and authorization checks remain part of collaboration. With `history_mode::disabled`,
undo/redo APIs, per-session stacks, inverse snapshots and contribution-version tables
are absent. Accepted snapshots and compact changed-field/ownership addresses remain
necessary for ordered synchronization, conflict detection and retry deduplication.
Inverse requests are rejected. History-enabled authorities preserve their existing behavior.

Each candidate is encoded once by the store. The authority reuses those prepared bytes,
and baseline polling copies the existing committed snapshot without re-encoding it.
Accepted batches carry the complete current model snapshot and transaction metadata.
Retained before snapshots and encoded field metadata count against `max_retained_bytes`;
`max_tracked_fields` (default 100000) bounds projection and version tombstones.
These limits reject new work rather than discarding deduplication/history dependencies.
History preparation adds field projection, retained snapshots, and version-table copies;
these are bounded correctness-first costs, not a compact delta journal.
Default proposals also carry the complete candidate model and create a private
history-disabled draft for authoring. This removes application boilerplate, but is
not a compact operation/delta protocol. `max_created_ids` (default 4096) bounds
per-proposal identity reservations on both authoring and authority application;
`max_command_bytes` also bounds the encoded candidate payload (default 1 MiB).
Budget for the complete model or use custom intent commands for larger documents.
Ordinary value projection is linear in encoded model size; ancestry comparison
adds work proportional to entity depth. Lock lookup uses a target map, overlap
checks scan active locks, and expiry scans active locks/presence. Ownership relocation
additionally checks grant pairs for newly introduced overlap. These bounded initial
algorithms do not provide the hash indexes, descendant counters, pooled allocation,
deadline queues, or measured latency targets in the broader design. Disabled
collaboration adds no tables or setter work to an ordinary `model_store`.

Store clients merge disjoint fields before submitting a rebased snapshot. General
merging, arbitrary historical selection, conflict-free replicated types,
per-field deltas/exclusions, automatic reconnect transport, distributed consensus,
and other-language collaboration engines remain future work. Custom compensation
can be an explicit new application command passing the same acceptance checks.

## Verification

The [latest four-toolchain verification](../verification-toolchains-2026-10-07.md)
includes the complete default collaboration suites on Windows x64 (MSVC and
clang-cl) and Ubuntu WSL2 x64 (GCC and Clang), using Release with a C++20 baseline
and managed records, SIMD, and all compression backends enabled. The results below
describe earlier collaboration-specific checks.

Earlier Windows Release builds with MSVC 19.51/C++20 and warnings as errors passed all 53
configured CTest checks, including collaboration examples, managed history/profile
checks, and the existing journal crash matrix. The full test run had access to installed toolchain executables for formatter
discovery. A final serial build retry cleared transient generated/output file-access
failures. One
appended-mode instance of the sidecar-only journal test is intentionally skipped.
Focused collaboration tests cover
retry identity, trusted contexts, conflicts, replica gaps and malformed snapshots,
entity/subtree locks, relocation/deletion, same-owner grant overlap, expiry,
presence tombstones, policy denial, resource limits, thread/reentry rejection,
document recovery, and indeterminate journal acknowledgement. Default-proposal
tests additionally cover compound generated edits, isolated drafts, callback failure,
revert/no-op behavior, expired editors, protocol separation, allocation conflicts
after rejected creation, malformed payloads, retired IDs, and payload/ID budgets.
Generated tests exercise separated storage with tree history/labels and Google-profile
uint64 model identities with disabled history. Session tests cover strings, uint32,
smaller unsigned boundaries, a custom type without operators, application validation,
trusted bindings, lifecycle fencing, policy hooks, locks/presence, malformed envelopes,
codec limits, and protocol-version fencing. All five examples built and ran against
an installed package. The shared editor suite passed 66 tests; this runtime addition
uses existing schema syntax and requires no extension source/package change. Fresh
compiler output passed 6007 bidirectional navigation checks across all 11 languages
and all C++ managed naming profiles; the Visual Studio .NET resolver passed 42 checks.

The 21 new history cases cover field metadata round trips, unrelated remote edits,
compound atomic conflicts, equal-value ABA writes, consecutive undo/redo, stale own
views, creation/deletion and ID restoration, reused keys, parent deletion and moves,
redo lifetime, trusted sessions, policy/lease checks, resource limits, malformed
requests and legacy protocols, restoration isolation after denial, both journal
layouts, and failed/indeterminate writes. Existing generated-model tests also exercise
undo/redo with separated storage, tree history/labels, and Google-profile uint64 IDs.

The collaboration target passes 73 cases, including 22 new store-integration cases:
manual/scoped/callback edits; separate send/receive and interval scheduling; independent
field rebase and ABA conflicts; denial/retry and explicit draft retention; offline
identity collisions; immediate undo/redo and labels; disabled/tree native history;
string sessions; memory checkpoints and both journal layouts; lost acknowledgements
and exact retry; new-epoch uncertainty; thread/reentry rejection; persistence failure
boundaries; and encode/recovery budgets. Generated-model tests also exercise local
synchronization with separated storage and Google-profile uint64 IDs.

Collaboration-specific sanitizer qualification, allocation-fault injection,
network/partition integration, cross-language engines, and performance measurements
remain outstanding. The runtime remains independent of a transport; these local
tests do not establish distributed failover or network interoperability guarantees.
