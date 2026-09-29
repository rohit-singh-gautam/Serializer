# Journal and crash recovery

Current C++ representation: managed schema classes carry `persistent_id` directly
by default. Separate ID-free values/storage wrappers in the illustrations below
require `[managed] separate_values = true`; see the [runtime contract](cpp_runtime.md).
New documents get a namespace automatically. Root and managed descendants allocate
IDs `1, 2, 3...` within that document only; saved IDs/counters survive reload,
and undo/deletion never renumber survivors. Explicit member boundaries still apply.

Status: proposal only. `journal`, journal formats, configuration,
and storage adapters are not implemented. See [capabilities and ID rules](capabilities.md).

## Meaning and naming

`journal` durably records committed model changes between full saves, supporting
recovery through an appended section or a separate file. The name describes the
mechanism; recovery is its purpose. This feature is not an audit log or accounting
ledger. Those can require different contents, access rules, and retention periods.

```text
managed(history, journal)
managed(collaboration, authorization, journal)
managed(all except journal)
```

History retains revisions for undo/redo; journaling preserves recoverable changes
since a full save. They may share immutable data but have different retention rules.
History can run without disk storage, and a journal can recover edits without
offering undo. Changes remain unsaved from the user's perspective until a full
Save, even when their journal records are already durable. Recovery restores the
latest complete durable batches, not necessarily edits still waiting for a flush.

## Appended and sidecar storage modes

Select `appended` or `sidecar` through the journal storage adapter/runtime options,
independently of schema participation and linear/tree history mode. Both use the
same transaction, identity, durability, and replay rules.

| Mode | Physical layout | What a complete Save replaces |
| --- | --- | --- |
| `appended` | A framed base snapshot followed by a journal section in the document file. | Replace the document with a new generation; the old appended section leaves the active file with the old generation. |
| `sidecar` | The base snapshot is in the document file; subsequent changes are in a separate companion journal/patch file. | Replace the base, then retire the companion records covered by that save. Delete the old companion only when it has no remaining dependencies. |

Illustrative layouts, not finalized file formats or required suffixes:

```text
appended: document = [base framing + snapshot] [journal header] [committed batches]
sidecar:  document = [base framing + snapshot]
          document.journal = [journal header] [committed batches]
```

The appended container needs an explicit base length/journal boundary and format
version. Its reader gives the ordinary Serializer decoder only the framed base
message, then reads journal batches separately. Do not relax existing exact-message
decoding or silently treat arbitrary trailing bytes as valid Serializer payload.
A journal batch is appended in either mode; the base snapshot remains unchanged
until a full Save replaces it. Recovery-only checkpoints must retain the user's
last fully saved baseline, as described below.

Use document namespace, base generation, schema/profile/identity formats, and
sequence numbers to bind journal records to the correct base. A sidecar's filename
alone is not sufficient association. Both modes may require small manifest or
generation-publication metadata for their durability protocol; `appended` names
the change-record placement, not a promise that an adapter needs no other metadata.

## Full Save and cleanup

1. Capture a consistent state at journal sequence S and build a replacement base
   generation, including allocator state and all retained history dependencies.
2. Preserve changes after S if editing continues during Save. Before publication,
   coordinate writers and include or durably retain the complete required suffix.
3. Flush the replacement and its dependencies, then publish the new generation
   through the storage adapter's crash-safe replacement protocol. Redirect future
   appends to the new generation; no writer may keep appending to a retired handle.
4. Reclaim only the old data covered by that successful save and unneeded by any
   reader, branch, replica, or recovery path.

For `appended`, the replacement contains the new base and, if necessary, the suffix
after S in its new journal section. Replacing the old file removes its old appended
section from the active document; it is not an in-place truncation before Save.
For `sidecar`, publish the new base with a matching empty or suffix-bearing journal
generation, then delete the retired sidecar once safe. When all changes are covered
and no dependencies remain, no old patch file is needed; the next edit can create
a fresh one. Never delete a concurrently written new sidecar at the old path.

A failed Save leaves the last valid base and journal recoverable. A crash after
publication but before cleanup may leave obsolete files; the committed manifest
selects the matching generation, and later cleanup removes proven obsolete data.
Never attach an old sidecar to a new base solely because their filenames match.
Clear the UI's dirty state only through S; changes after S remain unsaved even if
journaled. Saving the current values must not silently discard retained undo/redo
branches; copy their dependencies into retained storage before journal cleanup.

## Base snapshot plus journal

Keep a validated base snapshot and a logically separate append-only journal of
subsequent durable changes, using either physical mode. A manifest binds their
document namespace, identity type/format, schema/profile versions, and generations.
Recovery loads the matching base and applies complete committed journal batches
after its checkpoint sequence.

This is an architectural proposal, not a requirement to use a database engine.
SQLite's documented [WAL design](https://www.sqlite.org/wal.html) provides a useful
precedent for a base file plus appended changes and checkpointing. Its
[atomic-commit discussion](https://www.sqlite.org/atomiccommit.html) also explains
why flush ordering and filesystem assumptions matter. Serializer needs its own
versioned record format and platform-specific durability adapter.

```text
base_snapshot
  document_id + identity_type + identity_format_version
  schema/profile/record_format versions
  base_generation + last_durable_sequence
  managed records with persistent_id + ordinary persistent fields
  allocator reservations/high-water marks
  optional retained history and its complete recovery dependencies

journal_header
  storage_mode: appended | sidecar
  document_id + identity_type + identity_format_version
  base_generation + first_sequence + schema/profile/record_format versions

journal_batch
  monotonic sequence + transaction/operation ID
  expected state token + resulting state token
  entity creations/updates/deletions and ownership changes
  persisted excluded-value changes and required identity bindings
  history additions or cursor selection, if history is retained
  allocator reservation updates when required
  bounded framing + integrity checks + commit marker

manifest
  generation + selected base/journal locations or sections + integrity metadata
  last_full_save_state_token + optional recovery_checkpoint_sequence
```

These are logical records, not new hand-written C++ persistence classes. Define
the concrete records in `.serializer` and generate their language classes when
the journal format is designed. Native file handles, callbacks, locks, and
transaction guards remain runtime state. A persistent ID identifies an object;
it does not specify which version of it to replay. Validate base/result tokens.

Recovery does not restore session presence, live lock grants, or permission to
publish shared changes. Reconnect reconciles pending operations with the authority
and reacquires valid grants under the [collaboration contract](collaboration.md).
An authority may need a separate durable coordination log and fencing protocol;
the document journal does not implement that distributed service. A recovered
local draft is not automatically an accepted collaboration transaction.

History and recovery use different projections. `exclude(history)` values still
go into recovery when they are persistent. `transient` caches do not. Structural
changes and all affected owners/children form one atomic recovery batch. Recovery
must not rerun UI callbacks or resend network/file effects. External intents have
their own durable, idempotent delivery protocol.

## Commit and recovery boundaries

1. Prepare and validate an isolated candidate plus a complete journal batch.
2. Append bounded records and their commit marker in the adapter's required order.
3. In a durable-commit profile, complete required flushes before acknowledging
   durable success. Prepare publication so the remaining in-memory switch cannot
   fail after durable commit; crash recovery resolves the durable decision.
4. In an asynchronous-save profile, report local commit and durable save separately.
   A crash may lose locally committed edits after the last durable sequence.
5. Recover from the matching base and validated complete batches. An incomplete
   final batch is not published. Mid-file corruption, missing dependencies, sequence
   gaps, or mismatched base generations fail recovery rather than skipping damage.

Checksum/framing does not authenticate hostile data or make a write durable.
Distinguish process termination from power loss and document each storage adapter's
guarantees. RAII transaction completion must report failures through its existing
outcome contract; it cannot quietly promise durability after a failed flush.
An I/O failure after a commit record was issued may leave the durable decision
uncertain. Report an indeterminate outcome, stop further writes until recovery
resolves it, and use operation IDs for retry deduplication. Do not claim that a
local failure proves the journal contains no committed operation.
The initial adapter uses one coordinated journal writer. Concurrent replicas submit
validated operations rather than concurrently appending uncoordinated file bytes.

## Undo and moving a file position backward

Separate three positions: the selected history revision, the journal's durable
sequence, and the physical append offset. The latter two advance when an undo is
durably recorded even though the selected revision moves backward.

For `R0 -> R1 -> R2 -> R3`, undo to R1 can append a small `select_revision(R1)`
record. Recovery reconstructs R1 from retained dependencies and then applies the
latest non-undoable persistent overlay. R2/R3 remain available for redo or a sibling
branch. A new R4 names R1 as its parent; it need not overwrite R2's file bytes.
This optimization requires the complete referenced history to be durably available.
If only the current model is being retained, record the resulting undo changes or
a recoverable snapshot instead of a dangling cursor reference.

Simply seeking backward changes a file handle's position, not the saved document's
meaning. Overwriting or truncating the tail on every undo can destroy redo branches,
reader/replication dependencies, allocator reservations, and more recent
`exclude(history)` data. It also introduces a crash window before the new state
is safely published. An appended cursor record is the proposed default optimization.

A logical recovery endpoint may be recorded in an atomically replaced manifest
for a restricted linear profile, but only if no desired later data is lost and
all independent persistent state/allocation metadata is retained. Tail reclamation
is a separate operation: first publish a durable replacement state/dependency set,
then reclaim exclusively unreferenced bytes with no readers, branches, replication
or recovery dependencies. Never reclaim allocator reservations by moving a cursor.

## Checkpointing and storage optimization

Distinguish full Save from background recovery checkpointing. A recovery checkpoint
may compact replay data but must retain the last user-saved baseline and the dirty
state; it does not silently perform Save. Keep that baseline as a retained
generation until an explicit full Save replaces it. Applications that intentionally
autosave the user document must expose that as a separate product policy.

Write a new base generation from a consistent snapshot, including its exact journal
sequence and allocator state. Preserve required history versions/branches or leave
them in referenced segments; saving just current values cannot preserve undo.
Flush the new base and the required journal suffix, publish the manifest through
the storage adapter's crash-safe replace protocol, and only then retire the old
generation when no recovery path or reader needs it. Do not overwrite the sole valid
base and then assume the journal can be discarded.

Candidate optimizations:

- Coalesce repeated setters within a transaction into its first-before/final-after
  state. Coalesce across transactions only when retained history, observers,
  collaboration acknowledgements, and the advertised durability boundary allow it.
- Store changed-entity snapshots or typed deltas according to measured encoded size
  and replay cost; use dictionaries to compress repeated IDs without losing identity.
- Share immutable blobs across recovery and history where projections match;
  maintain separate dependencies where excluded fields differ.
- Batch flushes under an explicit durability/latency policy and checkpoint by byte
  budget, replay time, or idle periods. Bound journal growth and recovery work.
- Maintain rebuildable entity/revision-to-offset indexes. File offsets are locations,
  not stable entity IDs; compaction can change every offset without changing identity.

## Verification required before implementation claims

Inject interruption at every append/flush/manifest-replace/cleanup boundary;
exercise incomplete final records, corruption, disk-full, mismatched generations,
and repeated recovery. Check undo/redo/branch recovery, persistent values excluded
from history, deletion restoration, ID exhaustion and durable range reservations.
Test a crash after durable commit but before live publication/acknowledgement,
including duplicate operation retry. Verify both ID widths and all backend readers
against the same record fixtures. No journal runtime or recovery testing is supplied
by the current ordinary-class walkthrough.

Exercise both storage modes: frame boundaries in an appended file, stale or missing
sidecars, interrupted full Save, edits arriving during Save, and a crash after new
generation publication but before old-file cleanup. Reopening must recover the
selected generation exactly once and preserve the newer unsaved journal suffix.
