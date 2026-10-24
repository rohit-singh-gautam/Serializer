# Native embedded database

[Documentation index](../README.md) · [Storage requirements](../design/database_storage_requirements.md)

`<rohit/native_database.hpp>` provides `rohit::managed::native_database<Root>`:
an opt-in, bounded eager C++23 embedded engine over one complete managed root.
The engine owns an append-only native log and an exclusive writer lock. This is
the first DB02 implementation slice; it does not implement the full database,
placement, index, paging or cloud requirements in the design.

The generated root must support the ordinary C++ managed model traits and codecs.
Existing schemas and generated owning model APIs are unchanged. No
`storage(database)` annotation is required or accepted by this API. Choose the
engine explicitly when constructing the document binding.

## Prepare, accept and read

For the generated managed ledger example:

```cpp
#include <ledger.hpp>
#include <rohit/native_database.hpp>

namespace managed = rohit::managed;
using database = managed::native_database<ledger_example::ledger>;

database state{"state.sdb",
    ledger_example::ledger{"Ledger", {{10, {"One", 100}}}},
    managed::document_id{17, 23}, 42};

const std::array<std::uint8_t, 3> canonical_request{1, 2, 3};
auto prepared = state.prepare({7, 9}, state.epoch(), state.revision(), canonical_request,
    [](auto& transaction) {
      transaction.root().entries().edit(2).set_amount_minor_units(200);
      transaction.root().entries().insert(20, {"Outbox", 5});
    });
const auto outcome = state.accept(std::move(prepared));
const auto accepted = state.read();
```

Create requires an absent data file, an existing parent directory, a nonzero
document namespace and a nonzero authority epoch. All initial native identities
must be unassigned, exactly as for a new `model_store`. The engine assigns them.
Reopening uses `database state{"state.sdb"};`, restoring document namespace,
epoch, revision, native identities, allocation watermark and retained operation
outcomes. The epoch is fixed for the lifetime of this log; writer replacement
reopens the same epoch under its exclusive lock.

`snapshot(registry_options)` pins the accepted root and a bounded local catalog
together with its exact document, epoch, revision and `allocated_id()` watermark.
The watermark includes allocated IDs whose entities were subsequently deleted;
it is not the largest currently live ID. Its `read()` and `registry()`
remain immutable after later acceptance or database destruction. Snapshot creation
builds the complete eager entity catalog; it does not load pages or issue hidden
I/O. Use the database's cheaper `read()` when only a whole-root pin is needed,
or `registry()` for local schema-index queries without the revision wrapper.
Historical pins never establish current authorization.

`prepare` checks the exact base revision and request binding before invoking the
editing callback. It constructs an isolated identified `model_store`, executes
the real native managed transaction, checks generated constraints and reference
identities, runs the configured synchronous candidate invariant, and allocates
the immutable envelope and complete outcome catalog before acceptance. A
cancelled/failed preparation leaves accepted bytes and pins unchanged. Destroying
the move-only preparation cancels it without I/O. Callback editors and candidate
identities belong to the preparation and must not escape it or be used in external
effects. An accepted state keeps its original managed IDs; business keys do not
replace persistent identity.

`accept` rejects foreign, moved, consumed and stale preparations. It appends and
flushes one checksummed commit frame containing the complete managed envelope and
operation binding. Only after proven flush does a `noexcept` publication function
swap owning state/catalog pointers and assign the revision. This publication does
not allocate, call the user or execute diagnostics. Releasing the preparation and
old pins happens outside publication. Immutable pins preserve their original
state across later commits. There is no independently authoritative Serializer
journal, collaboration attachment or history navigation inside this engine.

The API is owner-thread confined. Already acquired immutable owning pins may be
retained independently. Recursive preparation or acceptance from an edit,
invariant or fault callback rejects. The optional fault callback is deterministic
test instrumentation, not a production I/O adapter or business notification API.

## Durable request identity and uncertain outcomes

An operation ID has two unsigned 64-bit components and is scoped to the persisted
document and authority epoch. The engine binds it to SHA-256 of the exact request
bytes, the expected base revision and the original accepted result revision.
The host must provide a canonical request representation covering every meaningful
command parameter, principal/policy binding where applicable and business intent.
The digest is an equality/integrity binding, not authentication or encryption.

An exact retry returns the original outcome without invoking the edit callback,
even when later operations advanced the document, and after close/reopen. Reusing
the ID with different bytes or a different base rejects; a different epoch rejects.
Unchanged model edits still commit one operation outcome and advance the database
revision so the command has a durable retry result. No background expiry discards
these records: deduplication lasts for this log's lifetime. At the configured
operation/file limit, new commands fail before writes. Exact retained retries
remain available. There is no compaction, deduplication expiry or cross-epoch
reconciliation in this profile.

Any failure after issuing a write may have accepted it. The engine fences further
preparation/acceptance and keeps reads at its last proven in-process state.
Destroy it and reopen under the exclusive lock. Recovery verifies frame magic,
length checksum, complete frame checksum, commit marker, record version, epoch,
namespace, exact revision sequence, duplicate operation IDs and nondecreasing
identity watermark. Every retained model envelope goes through native managed
schema/identity/reference validation. A complete recovered frame is flushed before
its outcome is reported; an incomplete final suffix is truncated and flushed only
after all complete records and models validate. Corrupt complete frames reject
without tail repair. A process interruption after acceptance before publication is
resolved from the accepted log, never by rerunning the business callback.

Recovery does not invoke edit, authentication, business-effect or diagnostic
callbacks. The optional validator passed on reopen is saved for future candidate
validation; it is not run over recovered records. The host must establish current
policy/capability configuration before admitting commands and put application
audit/outbox/idempotency data inside the same root when those facts must share one
durable decision. Sending audit messages and outbox effects remains host work.

## Prepared change details

Each new preparation exposes `changes()`, `before()` and `after()` before any
durable write. The roots are immutable pins for that exact source/candidate;
they remain valid after acceptance or database destruction. `changes()` owns
created/deleted/changed IDs. Enable `options.notifications.field_paths` for
stable field paths and field/membership changes. Notification limits reject
preparation before append. Exact retries carry no newly prepared changes or
roots and never rerun callbacks. Consuming these details does not acknowledge
acceptance; only `accept()` establishes its durable outcome.

## Native migration and rebaseline

The managed/storage redesign may introduce breaking formats or APIs when required.
Version each changed contract and provide an explicit qualified conversion or a
clear unsupported-version error. The schema language changes only when its syntax
or semantics change. A recoverable source generation protects interrupted
publication; retaining that safety copy does not require permanent readers for
every historical storage format.

The native migration and interruption matrix passes on the recorded Windows
profile; see the [verification record](../verification/verification-native-database-2026-10-24.md).
This is separate from managed-save conversion and does not close the broader
MIG03 journal/history and upgrade requirements.

`source.migrate<DestinationRoot>(next_epoch, archive_path, converter, validator)`
prepares a complete identified destination under the original writer lock.
The synchronous converter receives the immutable current storage and returns
destination storage. Every live entity ID and exact type key, the document
namespace must remain unchanged. The allocation watermark cannot decrease. A different
identity width, deleted/retyped entity or invalid destination rejects. The
destination's ordinary schema/reference/index validation runs before I/O;
the optional validator applies destination application invariants.

The destination epoch must be strictly greater. Its accepted revision starts at
zero with an empty operation horizon. Old-epoch requests reject; their outcomes
remain in the retained original log. The returned `unique_ptr` owns the new
database and original stable writer lock. The source becomes `retired()` and
rejects writes while existing immutable source pins remain usable.

The engine first syncs a bounded adjacent staging file and a create-only archive
of the complete old log, then atomically replaces the current file. It never
overwrites the requested archive or prunes retained copies. A failure before
replacement leaves the source writable; failure after replacement fences it
until reopen. Failures while attempting replacement conservatively fence even
when the old name survives. Recovery selects the complete generation at the original path
without invoking the converter. Archives remain explicit retained recovery
artifacts; reopening one independently does not constitute an authorized
authority rollback. Coordinate restoration and choose a fresh epoch.

When restoring an older archive, supply a host-established epoch greater than
every previously exposed authority epoch and an allocation floor at least as
large as every previously exposed ID. The database and snapshot `allocated_id()`
accessors expose the exact retained watermark for backup metadata. Pass the floor
with the optional fifth argument, for example
`database_migration_options{.minimum_allocated_id = known_watermark}`. Migration
retains the greater of this floor and the source watermark, validates it against
the selected identity width before invoking the converter, and persists it in
the new baseline. This prevents subsequent allocation from recycling IDs absent
from an older archive. The engine cannot discover epochs or IDs from lost later
generations; coordinating that restoration metadata remains a host responsibility.

Rebaseline uses the same operation with an identity-preserving converter and the
same root type. It preserves current IDs and values while moving previous retry
outcomes into the archive. This API does not translate old log entries into a new
schema, migrate attached model-store journals/history/collaboration, perform
rolling upgrades, or decide archive retention policy. After validating the new
generation, the host may explicitly remove an archive when its recovery, retry
and retention requirements allow. Temporary files from
abrupt process termination can remain beside the log and require coordinated
cleanup after writer recovery.

## Physical layout and bounds

The data file contains version-one frames with explicit little-endian integers,
independently checksummed fixed headers, full SHA-256 frame checksums and commit
markers. Each payload carries a versioned engine metadata prefix, authority epoch,
operation ID, base/result revision, request digest and managed envelope. No native
C++ object layout is persisted. A persistent `<data-file>.writer.lock` companion
provides the stable single-writer lock; preserve it alongside the data file and
never remove it while any process may own the document. Destruction closes both
handles without implicit writes or saves. File deletion, backups and namespace
publication are explicit host operations after coordinating readers/writers.

`database_options` bounds record bytes, total log bytes, retained operation count,
request bytes, and delegates snapshot/decode/resident limits to `model`. Admission
checks arithmetic before allocating/issuing writes. Each preparation owns its
complete candidate, snapshot and copied outcome catalog. The host controls the
number/lifetime of outstanding preparations and pins; limits are per managed
candidate, not a shared process-wide allocator ceiling. Recovery reads the bounded
whole log and retains complete record payloads before decoding them, so memory is
proportional to the configured file limit as well as current model size. Choose
file/record/model budgets for that eager working set.

This is local native file durability using `file_stream::sync` and the underlying
volume's guarantees. It does not establish protection against volume loss, cloud
durability, physical power failure on unqualified devices or malicious rewriting.
There is one serialized writer; multi-process concurrent readers, writable
replicas, remote authority leases and failover epochs are not exposed.

Pinned generation replacement uses `replace_file_generation`: same-filesystem
rename on POSIX, and the Windows extended rename operation with POSIX replacement
semantics and a write-through native handle. Existing pins retain their old file
object while future opens select the replacement. Unsupported Windows/volume
capabilities fail explicitly; no copying or reader invalidation substitutes for
atomic replacement. See Microsoft's [rename flags](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/4217551b-d2c0-42cb-9dc1-69a716cf6d0c)
and [write-through metadata contract](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-createfile2_extended_parameters).
The existing general `publish_file` API keeps its original behavior.

## Remaining design requirements

This slice supplies the bounded whole-root acceptance/reopen/exact-retry path,
real managed validation, identity persistence and atomic host records. It does not
complete DB02's general adapter negotiation, declared persistent database
indexes, database query facade, backup,
compaction or operational qualification. It does not implement PG01 bounded
page/object mutation or PG02 lazy reference leases and paging. It does not implement
MIG03 journal/history migration, rolling upgrades or provider capabilities.
These requirements stay in the central pending backlog; none is implied by a
mapped file, bounded log reader or immutable whole-root pin.

See the [native database verification record](../verification/verification-native-database-2026-10-24.md)
for executed test evidence and qualification limits.
