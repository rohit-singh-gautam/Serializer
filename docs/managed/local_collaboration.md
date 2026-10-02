# Local editing and synchronization

Status: implemented C++ store integration with one ordering authority. Include
`<rohit/managed_collaboration.hpp>` and link `Serializer::managed`. Regenerate
managed model headers for the field traversal and merge helpers. Start with the
[runnable example](../../example/managed/collaboration/local_sync.cpp).

## Use the existing store

```cpp
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
constexpr std::uint64_t session_id = 10;

// The host owns sessions and authenticates each connection.
managed::collaboration_authority<ledger> authority{ledger{"Shared"}, 1};
authority.open_session(session_id);
managed::collaboration_transport connection{authority, session_id};

managed::model_store<ledger> store{ledger{}};
managed::collaboration_client_options options;
options.sync_interval_ms = 60'000;
auto& session = store.collaborate(session_id, options);
session.bind(connection);
store.synchronize(); // First call joins the authority's baseline.
store.create_journal("client.srj", managed::journal_storage_mode::sidecar);

store.execute_transaction([](auto& edit) {
  edit.root().set_name("Office expenses");
}).throw_if_failed();
// store.read() now sees Office expenses; the authority still sees Shared.

store.synchronize(); // Explicitly flush pending work and receive remote changes.
store.undo();        // Immediately apply a local inverse through a normal transaction.
store.synchronize(); // The authority validates the inverse like any other change.
```

`model_store` owns the typed session, local history, outbox, acknowledged cursor,
identity mapping and journal attachment. Application session creation, authentication,
transport and connection lifetime remain host responsibilities. Use a fresh placeholder
store for initial join: the baseline replaces its constructor value. Attach before
editing or creating the client journal. Transactions before join are rejected.
An existing ordinary document journal is not converted by attaching collaboration.

All three existing transaction forms work: explicit `commit`, scoped completion,
and `execute_transaction`. Generated setters edit locally inside the transaction;
successful commit publishes the complete local document and queues its change.
Callbacks are never rerun during synchronization or recovery. No-op, reverted and
failed transactions add no outgoing change; allocated IDs remain reserved.

Sessions default to `std::uint64_t`. Pass a value of the intended type, for example
`store.collaborate(std::string{"client-a"})`, or select an application policy with
`collaborate<Session, Policy>(session)`. Retrieve it with
`store.collaboration<Session>()`. Authority and transport must use the same session
policy; see [session policies](collaboration_runtime.md#application-owned-sessions).
Use one active client writer per application session in an authority epoch.

## Journal cadence and synchronization cadence

The journal flushes every committed transaction before publishing it. ID reservations
and local undo/redo also pass through the existing durable boundary. It does not wait
a minute, and `save_journal()` is a full checkpoint, not the first persistence of edits.
Without a journal or explicitly saved checkpoint, pending work is memory-only.

Synchronization is independent of journal writes:

| Operation | Behavior |
| --- | --- |
| `store.send_pending()` | Send at most one queued transaction or retry its exact outstanding request. Persist its acknowledgement separately from receiving the ordered stream. |
| `store.receive_changes()` | Receive and reconcile accepted changes without sending new local work. Join/reconnect baselines are also handled here. |
| `store.synchronize()` | Receive, send FIFO work and receive acknowledgements, bounded by `max_flush_operations`. |
| `store.synchronize_if_due(now_ms)` | Run synchronization when the configured interval has elapsed; return whether it ran. |

Call the timer method from the store's owner-thread event loop using monotonic
milliseconds. The first tick establishes the timer origin. The default interval is
5000 ms; `session.set_sync_interval(60'000)` selects one minute, and zero selects
explicit synchronization only. There is no worker thread or automatic network timer.
A reconnect can bind a replacement connection and immediately call `synchronize()`.

Separate send/receive applications must receive an accepted transaction before
sending its successor. A lost reply retains an outstanding request: retry it with
`send_pending()` or `synchronize()` before calling `receive_changes()`. The latter
rejects unresolved delivery instead of guessing which local identities were accepted.
The returned send acknowledgement clears that pending item; the receive cursor can
still lag. An empty outbox therefore does not assert that remote changes were received.

Each client both sends and receives. The authority orders accepted transactions and
checks permissions/leases; its accepted stream distributes one client's changes to
all clients, including the originator. The supplied `collaboration_transport` is an
in-process adapter for examples and tests. A network adapter is application code with
these synchronous methods:

```cpp
collaboration::domain context(); // Trusted endpoint binding, not an untrusted message.
accepted_type snapshot();
std::vector<std::shared_ptr<const accepted_type>> accepted_since(std::uint64_t cursor);
std::shared_ptr<const result_type> submit_change(const proposal_type& request);
```

Use the session's `accepted_type`, `proposal_type` and `result_type` aliases.
`accepted_since` must supply the complete contiguous accepted history since the cursor;
snapshot-only polling cannot detect intervening write-away/write-back conflicts.
The transport must outlive its binding. All callbacks run synchronously on the store's
owner thread; reentrant edits and synchronization are rejected. The direct adapter's
`set_time(now_ms)` supplies authority lease time; the host drives that clock separately.

## Reconciliation and rejected work

Independent field changes are merged using generated typed three-way traversal.
The client records before/after snapshots and rebases those changes onto received
state. Intervening writes to touched fields or ownership invalidate the local
intention, including changes that later return to the same value. Container membership
and order are one field: concurrent insertions into the same container can conflict.
This is not a CRDT or arbitrary application-specific conflict resolver.

`session.pending_count()` and `session.pending_status()` expose the oldest pending
outcome. `session.state()` pins all records, including rejected and archived drafts.
Known outcomes are `queued`, `accepted`, `conflict`, `denied`, `obsolete_grant`,
`failed`, `uncertain` and `discarded`.

On conflict or rejection, the complete local working view remains available through
`store.read()`. `session.acknowledged_read()` exposes received authority state separately.
While blocked, even unrelated received values may remain visible only in that
acknowledged view. No partial overwrite silently loses local edits.

After fixing permission or lease failures, update grants with `session.set_grants(...)`
and explicitly call `session.retry_pending()`. Intervening conflicting changes prevent
that retry. For a conflict, inspect both views, then explicitly choose a resolution:

```cpp
const auto local = session.discard_pending(); // Returns a pin; archives every discarded draft.
store.execute_transaction([&](auto& edit) {
  edit.root().set_name(local->name + " (resolved)");
}).throw_if_failed();
store.synchronize();
```

Discarding all pending work is an explicit application decision. It is forbidden while
delivery to the current epoch is unresolved. Same-epoch reconnect retries exactly
the persisted request with the same operation ID. After an authority restart/new epoch,
pending work is marked `uncertain` and retained for inspection; it is never blindly
replayed. The authority does not persist deduplication or contribution history across
epochs. See [authority durability](collaboration_runtime.md#durability-limits-and-current-costs).

## Undo, identities and recovery

`store.undo()` and linear `store.redo()` create local inverse transactions and queue
them. An offline edit followed by undo remains two ordered transactions. The authority
checks undo against its accepted history, current authorization and grants. Unrelated
remote fields survive; conflicting remote contributions invalidate undo. Transaction
labels survive synchronization and recovery. Native tree stores support editing and
undo; `store.collaboration().undo(true)` requests collaborative redo. Arbitrary tree
checkout and `reset_history` are rejected while attached. Disabled native history
still records pending synchronization; native undo APIs remain unavailable.

The [store undo example](../../example/managed/collaboration/undo.cpp) uses
`execute_transaction`, `undo`, and `redo` for all document modifications. An undo
whose target was invalidated by an already received remote change throws before
publishing a local inverse. If the client has not received that change yet, undo
can commit locally; subsequent synchronization detects the conflict, retains the
local inverse, and exposes the remote state through `acknowledged_read()`.

Offline creation uses the existing local ID allocator. Local handles never renumber;
a persisted mapping translates generated `persistent_id` fields into authoritative
IDs. Concurrent clients can allocate the same numeric local ID without representing
the same entity. `session.remote_id(local)` and `session.local_id(remote)` return zero
when no mapping is known. Use authoritative IDs for server locks/presence and translate
at the UI boundary. Ordinary integer fields, map keys and application foreign-key
references are **not** interpreted as persistent IDs or automatically rewritten.

The existing store journal pointer wraps the native file adapter. One durable frame
pairs each native transaction/reservation with its client state. Synchronization
metadata and exact outgoing bytes are also persisted before sending. Appended and
sidecar framing, writer locks, CRC checks, tail repair and indeterminate-write fencing
are reused. This stores full client metadata/snapshots; it is not a compact delta log.

Applications keep using `model_store::create_journal`, `save_journal`, and
`recover_journal` for ordinary edits and undo/redo. The internal adapter adds a
checkpoint envelope containing session identity, local history, pending changes,
acknowledgements, identity mappings and exact retry bytes. A model-only snapshot
cannot tell recovery whether an edit still needs sending or was already accepted.
Pairing those records atomically prevents a recovered document from losing its
pending send or retrying an accepted operation under a new identity. The envelope
changes the stored payload; it reuses the existing journal file and durability
mechanism. The application does not construct another store or journal wrapper.

```cpp
managed::model_store<ledger> recovered{ledger{}};
recovered.collaborate(session_id).bind(connection);
recovered.recover_journal("client.srj"); // Before first synchronize/join.
recovered.synchronize();
```

`save()`/`load()` checkpoint the complete model, session, local history and outbox.
Load/recover into a fresh attached store with the same session policy, ID, model and
history specialization; live replacement is rejected. Client wrapper format 1 has
its own binding and is not readable as an ordinary model save. `save_journal()`
preserves synchronization state, including unsent work. `journal_dirty()` still tracks
document values relative to Save; it does not mean the outbox is empty.

## Host coordination and operation IDs

Presence and lease acquisition/renewal/release remain host coordination APIs; all
document edits still use `model_store` transactions. Translate local entity handles
with `session.remote_id(local)` before using them in authority lock/presence calls,
and pass acquired `{target, generation}` grants to `session.set_grants(...)`.
After recovery, refresh host lease state and supply grants for new edits again;
the journal preserves grants on retained transactions, not a live lease for future edits.
See the [store locks example](../../example/managed/collaboration/locks.cpp).

Lock requests and document submissions share one operation namespace per session.
After joining, use `session.reserve_operation_id()` on the store owner thread for
each new host lock request. It advances the same counter as synchronization and
persists the reservation through the attached journal before returning. Unused IDs
remain reserved. A pre-join call throws; exhaustion and persistence failures also
throw. Without a journal or saved checkpoint, reservations are memory-only.

Reservation does not send or enqueue a lock request or preserve its payload for
retry; the host owns that request and must reuse its exact bytes and reserved ID
for retries. The store continues to manage exact retries of document changes.

## Resource limits

Client options bound retained transactions (4096 by default), encoded state (256 MiB)
and submissions per synchronization (256). Accepted and archived records count too;
there is no automatic compaction. Size limits reject the local commit atomically.
Store decoder limits and authority command/history/entity budgets also apply. Every
checkpoint is checked against its recovery decoder before publication. Provision both sides for the
workload. Current payloads contain full model snapshots, and retained field projections
and history add CPU/memory costs. Multi-authority peer-to-peer ordering, automatic
transport reconnect, durable authority history across epochs and other-language
collaboration runtimes remain outside this implementation.
