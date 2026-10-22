# Native document origin and confirmed Save

Compiler/runtime **2.26.0**, C++ external-document sidecars. This adds native
opaque provenance and an explicit prepare/confirm protocol to the
[document bridge](journal.md#external-main-file-formats). It does not change the
authored schema, model IDs, native envelope or differential-record format.

Require `find_package(Serializer 2.26 CONFIG REQUIRED)` and link
`Serializer::managed` when consuming these APIs from an installed package.

## Application storage choice

The application chooses the journal's main path:

| Source lifetime | Application action | Native recovery depends on |
| --- | --- | --- |
| Temporary/download/capture | Durably publish a complete application-owned main, then attach its normal sidecar | Owned main and its journal directory |
| Durable original | Attach the normal sidecar directly to that original | Original and its journal directory |

Serializer never infers storage policy from a locator. An owned main is a real
complete file, not another full snapshot per edit. Once it is published, recovery
has no dependency on a separate external original or shared capture cache.
Deleting those sources remains application work. Do not prune Serializer-owned
generations or staging files independently.

The [runnable example](../../example/managed/external_document/README.md) exercises
both choices with synthetic JSON. Its host code stages, flushes and publishes
the owned copy before journal attachment. This contract introduces neither a
shared immutable-base resolver nor shared-cache dependency inventory.

## Opaque origin binding

Set `journal_options::origin` on creation, and set `max_origin_bytes` to an
application-appropriate finite bound (default 64 KiB). The record is arbitrary
bytes; Serializer does not parse its locator, cache ID, policy or authentication
data. It is native generation metadata outside the authored root.

`store.journal_origin()` returns an owning, checked copy of the selected base's
record. Recovery uses stored provenance; creation-only `options.origin` cannot
replace it. Edits, Undo/Redo, native full Save, retained native history/session
checkpoints and cold recovery preserve the binding. It shares the manifest's
integrity binding to the exact main size/SHA-256, codec, native generation and
checkpoint, which in turn carries the admitted document namespace and IDs.

`store.save_journal(new_origin)` replaces provenance through ordinary full Save,
including the current retained checkpoint. It marks that current document saved.
Preparation/budget failures leave the accepted binding intact. Uncertain native
publication fences the writer until recovery. This is an integrity contract;
host authentication and protection against an attacker rewriting all native
files remain host responsibilities.

Metadata input is bounded before allocation independently of the large document
limit. `max_file_bytes` additionally caps every physical file; `max_record_bytes`
and `max_replay_bytes` retain their checkpoint/expansion limits. Oversized,
truncated, corrupt or generation-mismatched input is rejected before admitting
recovered store state. No IDs are reserved merely to read metadata.

## Prepare, publish, confirm

```cpp
// Existing C++ model_store attached with an external-document codec.
auto prepared = store.prepare_journal_save(new_origin);
// Or prepare_journal_save() to retain the selected origin.

// Host code runs here, outside Serializer callbacks:
// publish prepared.document; independently verify the published bytes.
rohit::managed::journal_publication_evidence evidence{
    published.size(),
    rohit::make_digest<rohit::digest_algorithm::sha256>(published)};
bool activated = store.confirm_journal_save(prepared.token, evidence);
```

Preparation durably stages one proposed external document, its native
differential checkpoint and an unresolved native intent. The returned owning
`journal_prepared_save` contains bounded `token`, `document` and `origin` buffers.
The fixed 184-byte opaque token binds the exact old saved generation, acknowledged
operation sequence/checksum, proposed generation/metadata and proposed main
size/SHA-256. This pins the accepted model, namespace, allocator watermark,
retained history and any paired client session/outbox. Preparation does not
change the selected main, saved dictionary or origin.

Persistent edits, allocation reservations, history navigation and ordinary Save
reject while preparation is unresolved. Store-owned collaboration operations
also reject before transport, so synchronization cannot change the pinned
session. Read-only state and preparation inspection remain available. This
profile deliberately requires completing reconciliation and confirmation before
resuming writes; it has no implicit timeout or automatic abandonment operation.
`journal_dirty()` still compares authored values with the saved baseline; it can
be false for a metadata-only unresolved Save. Inspect `prepared_journal_save()`
for publication status independently of that dirty flag.

The host performs copying/replacement, authentication, idempotency and external
reconciliation. Publication evidence is the host's attestation, supplied after
independent verification; Serializer does not access that service or establish
that a remote side effect happened. Fault-injection callbacks remain test hooks.
This preserves the [MG06 application boundary](../project/application_responsibilities.md).

Host publication must not replace Serializer's managed main or companions behind
its writer lock. The prepared protocol coordinates a separate external target
and the owned native recovery base. When a durable original is itself the native
main, ordinary `save_journal()` already publishes that main through Serializer;
do not replace it independently and then try to confirm the stale old binding.

Confirmation verifies the exact token, expected old activation, staged main,
proposed checkpoint and independent size/digest evidence before local publication.
It reuses ordinary native pending-manifest/main/manifest publication. Only that
durable selection installs the new differential dictionary. Known covered native
files may be retired afterward; cleanup failure does not undo successful Save.
The accepted model, history/Undo, watermark and pending collaboration are retained.

`confirm_journal_save` returns `true` for activation. An exact retry with matching
evidence returns `false` without changing state or marking later edits saved.
The last confirmation receipt survives restart and ordinary full Save. A later
confirmed preparation replaces it; older tokens then reject. Wrong, stale,
foreign, truncated, tampered or evidence-mismatched confirmations reject without
adopting another base or retiring the old recovery state.
Retain the opaque token with the host's durable publication intent for exact
retry after acknowledgement loss. Serializer retains and checks the native
receipt; it does not implement the host's business-intent storage. An application
can also include its publication-intent identifier in the opaque proposed origin
record and read the selected record through `journal_origin()` after recovery.

## Restart and uncertainty

After reopening through `recover_journal`, call `prepared_journal_save()`.
An optional preparation means external publication remains unresolved. It exposes
the same validated token, proposed bytes and provenance for host reconciliation.
Neither a filename nor a missing acknowledgement proves external success/failure.
If host publication failed or is uncertain, the preparation remains durable and
writes remain blocked; the host can reconcile and retry publishing those bytes.

Interruption before durable preparation leaves the old selected base and journal.
Interruption after preparation preserves an unresolved intent. During native
confirmation, recovery selects a coherent old unresolved or new confirmed base
using the existing exact-byte publication decision. If confirmation became
durable but its acknowledgement was lost, the selected receipt supports an exact
retry. For equal old/new document bytes, the durable confirmation decision may
select the proposed metadata/checkpoint before a redundant main replacement.

After uncertain I/O, destroy the fenced store and recover afresh. Recovery
validates intent/staging before publishing accepted state or repairing the old
tail. It never automatically performs host publication. Crash leftovers that are
not selected remain inert; directory scanning is not permission to evict them.

## Files and compatibility

New journals keep all auxiliaries in `.<filename>.journal/`:

```text
document.json
.document.json.journal/
  writer.lock
  manifest
  changes-<generation>
  save.prepared                 # only while unresolved / cleanup pending
  pending-<generation>           # proposed main during prepared Save
  pending-<generation>.manifest  # publication staging
  manifest.pending              # native confirmation/Save decision
```

The writer uses a native exclusive Windows handle or POSIX `flock`. A separate
stable lock remains necessary because Save replaces the main and manifest; it
must survive those replacements. A lock file remaining on disk is not a held
lock. Do not delete/replace it while a writer may exist.

Existing flat-layout journals still recover and Save in their original layout.
The runtime rejects simultaneous legacy and new-layout metadata instead of
guessing. Move the main with its entire journal directory when closed. Older
binaries do not understand the new directory/lock protocol; do not mix old and
new writers on a new-layout document.

Default no-origin/no-confirmation manifests remain byte-compatible version one.
Version two adds bounded provenance and a confirmation receipt; current readers
accept both. Older readers reject version-two manifests. Native journal versions
two/three, schema language 1.21.0 and authored payload bytes are unchanged.
Origin/prepared Save are supported for external-document sidecars; ordinary
native containers reject these additions, and custom sinks explicitly opt in.

See [wire details](../serialization/wire_format.md#external-document-origin-metadata),
[migration](../../migration.md), and the
[qualification record](../verification/verification-document-origin-2026-10-22.md).
