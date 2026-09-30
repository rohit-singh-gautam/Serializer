# Journal and crash recovery

Current C++ representation: managed schema classes carry `persistent_id` directly
by default. Separate ID-free values/storage wrappers in the illustrations below
require `[managed] separate_values = true`; see the [runtime contract](cpp_runtime.md).
New documents get a namespace automatically. Root and managed descendants allocate
IDs `1, 2, 3...` within that document only; saved IDs/counters survive reload,
and undo/deletion never renumber survivors. Explicit member boundaries still apply.

Status: implemented synchronous C++ journaling and crash recovery for
`model_store<Root, Mode, Labels, Traits>`, with appended and sidecar native-file
adapters. Each edit appends its already serialized root snapshot once; compact
control records persist navigation, reservations, and reset. Schema capability
selectors, field/entity deltas, background checkpoints, asynchronous flushes,
and other-language journal readers remain proposals. See the
[implemented C++ runtime](cpp_runtime.md) and [broader capability design](capabilities.md).

A [runnable C++ example](../../example/managed/journal/README.md) demonstrates both
file modes, closing before full Save, replay, undo/redo, and full checkpointing.

## Using the implemented journal

Enable `SERIALIZER_BUILD_MANAGED`, link `Serializer::managed`, and use an existing
bare-`managed` schema. No generator option or schema selector is needed:

```cpp
using store_type = rohit::managed::model_store<ledger_example::ledger>;
{
  store_type store{ledger_example::ledger{"Ledger"}};
  store.create_journal("ledger.srj", rohit::managed::journal_storage_mode::sidecar);
  store.execute_transaction([](auto& transaction) {
    transaction.root().set_name("Edited ledger");
  }).throw_if_failed(); // The complete edit is flushed before committed is reported.
  const bool unsaved = store.journal_dirty(); // true, despite durable recovery data
  store.save_journal(); // Replace the base, retain history, and clear the saved baseline.
}
{
  store_type reopened{ledger_example::ledger{}};
  reopened.recover_journal("ledger.srj");
  reopened.undo(); // Navigation is itself journaled before the cursor is published.
}
```

- `create_journal(path, mode, options)` creates a new saved baseline exclusively.
  It rejects an existing document. `appended` is the default mode.
- `recover_journal(path, options)` validates the base and replays complete
  snapshot/control records, repairs only an incomplete final record, and acquires
  the writer role. It preserves the original store on validation/I/O failure.
  Recover into an unattached store; after an indeterminate operation,
  destroy the old store and recover into a new one. Recovery rejects replacing a
  same-document live store if doing so would discard higher allocation marks.
- Changed transactions, `undo`, `redo`, tree `checkout`, and `reset_history` flush
  the required snapshot or control record before nonthrowing in-memory publication. All
  three transaction forms use this boundary. No-op transactions do not append.
- Every allocated object ID has a separate durable reservation before it is
  returned. Canceled/failed edits can therefore advance `journal_sequence()`
  without changing values or dirty state. No ID returned by this writer is reused
  after recovery. Both uint32 and uint64 identities are supported.
- `save_journal()` synchronously replaces the base with the current envelope,
  including retained undo/redo branches, labels, IDs, and allocation marks. It
  does not increment the operation sequence. The thread-confined writer rejects
  concurrent/reentrant edits during Save, so no newer suffix can be lost.
- `journal_dirty()` compares current persistent values and identities with the
  last full Save. Undoing back to those values clears it; history/cursor metadata
  and allocation reservations alone do not mark values dirty. `save()` still
  returns a memory envelope and does not clear dirty state. `load()` is rejected
  while a journal is attached; import a memory envelope before creating a journal.
- `journal_sequence()` is monotonic across edits, navigation, reservations, and
  full Saves. Document namespace plus sequence identifies a durable operation,
  independently of any undo revision. Recovery never invokes editing callbacks
  or repeats external effects. After uncertainty, inspect recovered state and
  sequence before deciding whether an application action needs retrying; there
  is no automatic callback retry or external-effect deduplication service.

`journal_options` bounds each frame payload (`max_record_bytes`, default 64 MiB)
and physical file (`max_file_bytes`, default 1 GiB). Store decoding and history
budgets also apply. Recovery reads bounded files into memory and borrows record
slices; these limits are not a total process-memory budget. An over-budget append
fails before writing; a full Save can reclaim covered records.

The edit path does **not** encode an envelope, re-encode retained history, make a
byte diff, or copy the serialized snapshot into another output buffer. It sends
stack metadata, an optional borrowed label, and the existing snapshot buffer to
the retained native file handle. It computes integrity over those buffers and
flushes before publication. The writer does not reopen or seek the file per edit.
Linear history preallocates one tail entry, appends the record, then performs
nonthrowing retention/cursor changes; on failure it removes that unselected entry.
It does not copy the history deque or prior snapshots. Tree commits likewise
preallocate only the new revision node, removing it if publication fails.

| Operation | Bytes appended, including framing |
| --- | --- |
| Edit, default unlabeled linear history | Serialized root snapshot size + **41** |
| Edit, unlabeled tree or disabled history | Serialized root snapshot size + **33** |
| Labels enabled | Add 8 bytes for label length plus the label bytes to an edit |
| Undo/redo/checkout | **33** |
| ID reservation | **33** |
| History reset | **25** |

The complete envelope, including retained history and schema/document metadata,
is written only when creating the base or performing a full Save. Snapshot size
still scales with the root being serialized; changed-entity/field deltas are not
implemented. The tests assert exact append sizes across 100 retained edits, so
per-edit disk growth cannot silently become proportional to accumulated history.
No end-to-end throughput benchmark or latency guarantee is claimed.

## Stream and file adapter boundary

`managed_journal.hpp` defines storage-independent journal options, errors, and the
internal `journal_sink` commit/recovery interface. The managed store owns that sink.
`managed_journal_stream.hpp` handles frame encoding and bounded record reading through
existing `input_stream`/`output_stream` concepts. Serializer memory/custom buffers,
standard narrow iostreams, and `file_stream` use identical version-two frame bytes.
Buffer recovery borrows payload slices; byte-stream recovery owns each bounded payload.

`file_stream.hpp` is a reusable owning file stream for ordinary serialization as well
as journals. Native read/write/sync/seek/truncate operations and atomic file
publication live in this stream layer. `managed_file_journal.cpp` is the concrete
file adapter: it binds streams to paths, owns the writer lock, selects generations,
rotates sidecars, and enforces uncertain-outcome fencing. The existing path-based
managed APIs remain convenience entrypoints for this backend. There is no database
adapter; database journaling is deferred until Serializer implements and tests that
backend. The low-level frame helpers alone do not provide a complete managed document
or claim persistent durability for memory/plain iostream sinks.

See [file stream usage](../usage.md#file-streams-and-journal-records) for generated
serialization, bounded reads, optional capability concepts, and explicit host-policy
adaptation of an iostream. Frame emission does not flush; managed file commits call
`file_stream::sync()` before publishing the live state. Full Save retains its existing
complete-envelope behavior; this stream refactor does not change history retention.

## Durable decisions and files

One store owns the document through its stable `<path>.lock` file. Windows uses an
exclusive native handle; POSIX uses nonblocking `flock`. The lock is released by
process exit and the lock file deliberately remains. All writers must cooperate
and use the same canonical path; hard-link aliases and network filesystem locking
are outside this contract. Do not remove/replace the lock file while a writer may
exist. External file modifications are unsupported.

In appended mode, `<path>` contains the base header and base envelope frame,
followed by committed snapshot/control frames. In sidecar mode, `<path>` contains only the
base header and frame. Its generation selects
`<path>.journal-<generation_high>-<generation_low>`, which contains a matching
header and subsequent frames. A missing sidecar fails recovery, including an empty
one; a stale or substituted sidecar cannot be attached by filename alone.

Full Save writes and flushes a new generation first, creating the matching
companion first in sidecar mode. It then atomically publishes the base through a
same-directory rename/replacement. That base is the generation-selection manifest;
there is no separately mutable manifest. Only after publication does the writer
switch destinations and best-effort remove the known old companion. Failure before
publication leaves the previous generation recoverable. Failure after publication
may leave obsolete companions or `.pending-<generation>` files; recovery uses only
the generation selected by the base. Automatic scanning/deletion of crash leftovers
is not implemented. Cleanup failure cannot invalidate a successful Save.

Windows issues `FlushFileBuffers` and `MoveFileExW` with `MOVEFILE_WRITE_THROUGH`;
POSIX uses `fsync`, atomic `rename` (exclusive initial creation uses `link`), and
parent-directory `fsync`. Durability depends on the filesystem/device honoring those
operations. Process-interruption tests do not certify power-loss behavior on every
filesystem, network share, or device. See the platform contracts for
[FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers),
[MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw),
[fsync](https://man7.org/linux/man-pages/man2/fsync.2.html), and
[rename](https://man7.org/linux/man-pages/man2/rename.2.html).

A failure after issuing an append or attempting base publication throws
`journal_indeterminate_error` (with the underlying exception nested). Transaction
outcomes report `transaction_status::indeterminate`; `throw_if_failed()` rethrows
it. The live candidate stays unpublished and `journal_needs_recovery()` is true.
Further transactions, navigation, memory export, and full Save are blocked. Reads
can still inspect the old live state, which may differ from disk. Recovery replays
complete records, validates the final application state, flushes recovered bytes,
and resumes at the next sequence. Validation, budget, and pre-write preparation
failures remain ordinary failures and leave a healthy writer usable.

`journal_options::fault_injector` is a qualification hook, not a change-notification
API. It may throw or terminate at `journal_io_event` boundaries. It must not reenter
the store or perform application effects. Exceptions from best-effort cleanup are
ignored because publication has already succeeded.

## Version-two file contract

Framing integers are unsigned 64-bit little-endian wire words, independent of
native object layout. CRCs use CRC-64/ECMA-182 (polynomial
`0x42f0e1eba9ea3693`, initial value and final XOR zero, non-reflected). Checksums
are integrity checks, not authentication.

The 72-byte file header contains, in order:

1. Magic `0x00314c4e4a5a5253`.
2. Container version **2** (the superseded full-envelope journal format is rejected).
3. Storage mode: appended `1`, sidecar `2`.
4. File kind: base `1`, companion `2`.
5. Generation high word.
6. Generation low word (the pair must be nonzero).
7. Base operation sequence (initially zero).
8. Base frame CRC.
9. CRC of the preceding 64 header bytes.

Each frame has just a **16-byte header** (payload length, header CRC), the payload,
and an **8-byte commit CRC**. Sequence and previous CRC are implicit integrity
inputs, avoiding repeated fields. Let `seed` be CRC over the little-endian sequence
followed by the previous frame CRC. The header CRC extends `seed` over the 8-byte
length. The commit CRC extends `seed` over the complete header and payload.
The base uses the file-header sequence and previous CRC zero; subsequent records
advance sequence by exactly one, starting from the base frame CRC in either mode.
This detects reordered, duplicated, or missing frames without storing sequence or
chain words in each record. Sequence overflow fails.

The base payload is the unchanged managed envelope, binding document namespace,
schema/profile fingerprint, ID width, history mode, label policy, allocation marks,
and retained snapshots. Its existing versions 1/2/3 remain unchanged. Subsequent
payloads start with one operation byte:

| Tag | Following payload |
| --- | --- |
| Edit `1` | Linear-only uint64 front-eviction count; uint64 transaction allocation boundary; enabled-label-only uint64 label length and label bytes; remainder is the existing serialized root snapshot. |
| Select `2` | uint64 linear cursor index or tree revision ID. |
| Reserve `3` | uint64 next allocated object ID, required to advance by one. |
| Reset `4` | No additional bytes. |

An edit drops a linear redo suffix, applies exactly the writer's front-eviction
count, and appends the new snapshot. Tree edits allocate the next revision with
the selected revision as parent. The base's mode and label policy determine the
record layout. Recovery rejects malformed operations, identity changes, impossible
cursor/eviction decisions, and budgets exceeded at any replay point; it never
silently prunes to different reader limits. It invokes schema validation while
replaying and application validation on the final state, without editing callbacks.

Ordinary Serializer decoders receive only exact envelope/snapshot slices; journal
bytes are not accepted as trailing ordinary-message data. Base/companion headers
must match except for file kind. Complete integrity failures, missing dependencies,
unsupported versions, and trailing bytes in a sidecar base are errors. A short
final record is ignored and durably truncated only after the reconstructed model
validates. A complete damaged final record is an error, not a rollback instruction.

## Verification

Tests cover both physical modes, byte-by-byte final-frame truncation, complete
corruption, sequence gaps, stale/missing sidecars, writer exclusion, bounded
recovery, dirty-state restoration, undo/redo and branch retention, reset, labels,
direct and separate-values representations, and uint64 reservations. Injected
append/Save failures exercise uncertain outcomes and transactional live state.
`managed_journal_crash_test` launches fresh processes and exits without destructors
at append, reservation, undo, Save, and tail-repair boundaries, then checks repeated
recovery. These are process-crash and injected-I/O tests, not physical disk-full or
power-cut qualification. See the runtime guide for the tested platforms.

## Further design and optimizations

The sections below preserve the broader design. Selector syntax, typed deltas,
background checkpoints, asynchronous acknowledgements,
replication, and persistent excluded-value overlays are not implemented. The current
adapter realizes their core durability/identity rules using a full base plus
framed snapshots/control records and one synchronous writer.

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

The architecture does not require a database engine.
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
against the same record fixtures. The ordinary-class walkthrough does not supply
these broader distributed/exclusion features; current runtime tests are listed above.

Exercise both storage modes: frame boundaries in an appended file, stale or missing
sidecars, interrupted full Save, edits arriving during Save, and a crash after new
generation publication but before old-file cleanup. Reopening must recover the
selected generation exactly once and preserve the newer unsaved journal suffix.
