# Rebuildable cache attributes and storage

[Documentation index](../README.md) | [Design index](README.md)

Compiler/runtime **2.30.0**, schema language **1.22.0**, editor packages **2.0.12**.
This document records the implemented owning C++ cache contract and the storage
lifecycle that grew from the original proposal. See the
[qualification record](../verification/verification-cache-2026-10-23.md) for actual
checks and platform limits. Existing payloads, field IDs and transient behavior
remain unchanged; new journal containers have explicitly versioned headers.
Native Windows and native Linux storage are qualified. WSL mounted-Windows
journal replacement has observed permission failures; see the dated record.

## Intent and field categories

A `cache` value can be rebuilt from authoritative data. It may survive a restart,
but its loss must not lose user work. Priority is optional: **omitted priority is
`0`, the lowest retention preference**. Higher values are reclaimed later within
the same application-selected cache domain. Even priority `255` is disposable.

A successful graceful close saves all required state and retires recovery journal
data while caches remain. A failed or uncertain Save retains recovery. The shared
folder is not temporary storage and must never be recursively deleted on close.

| Property | Durable field | `transient` | `cache` |
| --- | --- | --- | --- |
| Authoritative document data | Yes | No | No |
| Ordinary codecs and payload IDs | Normal contract | Excluded | Excluded |
| History, collaboration and dirty state | Normal contract | Excluded | Excluded |
| Survives restart | Through document persistence | No generated persistence | Optional cache provider |
| Missing/zero/empty value | Normal field rules | Runtime default | Readiness is separate from the value |
| Durable publication or navigation | Normal contract | Reset to defaults | Reset to defaults; select/rebuild for the new source |
| Graceful close | Save required dependencies | Process lifetime | Retain persisted entries |

Existing [`transient`](../managed/cpp_runtime.md#runtime-state-and-copying)
declarations remain memory-only. Normal C++ copies retain ordinary member-copy
semantics. Cache operations share the guarded runtime projection, but cache
metadata and persistence are separate. Format-specific `omit(...)` is not a cache.

Thumbnails, previews, statistics and reconstructible indexes are candidates.
Original content, unsaved changes, pending collaboration requests, allocation
high-water marks, required history and the sole copy of imported data are durable.
If correct offline operation requires an unavailable source, retain that source
or its required result durably rather than declaring the result a cache.

## Schema and generated metadata

```text
serializer version 1.22.0;
class article stable_ids managed {
  public string body (1);
  public transient uint64 session_counter {0};
  public cache bool has_links {false};
  public cache(priority = 200) uint64 estimated_word_count {0};
  public cache(priority = 20) string preview_markup;
}
```

`cache` and `cache(priority = 0)` have the same retention preference. The integer
range is `0..255`; negative, overflowing, fractional, unknown and repeated options
reject. Every declaring file, including includes, needs language 1.22.0 or newer.
Previously valid types named `cache` keep their ordinary type interpretation.

Cache fields consume no payload IDs, do not renumber later implicit IDs, and do
not enter the durable schema fingerprint. They cannot declare wire IDs, durable
lifecycle metadata or output omissions. They are runtime fields, not separately
identified managed entities. Defaults initialize model storage; they do not prove
that a cache hit exists.

The C++ generator emits `rohit::cache::fields<Owner>` with one accessor per cache
field, including nonpublic fields. An accessor returns a typed `cache::member`
with a `descriptor` (`identity`, `priority`), a lifetime-bound `read(owner)` and
`assign(owner, value)` for application-owned values. Managed stores publish through
runtime editors, not by mutating an immutable read pin. Generated API names follow
the selected C++ naming profile; descriptor identities use the declaring schema
identity, concrete specialization and original field name.

Scalar and inspectable owning value graphs are supported. References, independently
managed cache objects, hidden native storage, raw unions and borrowed/view/packed
cache records reject. Cache declarations on open generic owners also diagnose;
declare them on a concrete owning record (whose cache value may use concrete
owning generic types). Unsupported non-C++ outputs diagnose cache declarations
rather than dropping their semantics. Native handles and resources remain transient
or use an explicit application-owned provider/codec; they are never persisted as
pointer bytes.

Applications may supply a stable descriptor identity to retain caches across field
renames. An automatic rename becoming a miss is safe. Scope nested occurrences by
managed entity ID, or include a stable relative field/container path in parameters
for ordinary owned values; a type/field descriptor alone does not identify two
instances of the same ordinary child type.

## Validity and immutable publication

`cache::key` binds document namespace, entity/scope, declaration identity, exact
source fingerprint, explicit application recipe identity, and parameter digest.
All key components are encoded with fixed widths and SHA-256 bindings. Paths,
file timestamps, view generations and journal sequence numbers are not source
identities. Priority is retention policy rather than source validity.

`model_store::cache_source()` hashes the exact durable snapshot with its schema
identity. `cache_key(descriptor, recipe, entity, parameters)` adds the document
namespace. This conservative whole-root policy covers membership, order, deletion,
references and schema changes. External inputs such as renderer settings, locale,
provider version and recipe changes must appear in the recipe/parameters supplied
by the application. Recipe identity is mandatory for persistent reuse.

`publish_cache(key, callback)` rejects a stale or foreign document source before
executing a guarded runtime update. It cannot change durable bytes or identities,
add undo history, dirty the document or create journal/collaboration operations.
Existing read pins stay immutable. Undo, redo, branch selection, import, recovery
and collaboration naturally select different source fingerprints; an identical
retained source can reuse its own entry.

Load/recover authoritative state before consulting caches. Validate a cache against
its key and its decoded value constraints. Corrupt, absent, evicted, unsupported
or outdated data is a miss. A checksum detects damage; it is not authentication
and cannot prove that a forged result was computed correctly. Authenticity-sensitive
applications require authenticated storage or independent recomputation.

## Rebuilding, readiness and resource limits

`cache::service` exposes absent, building, ready and failed states. Its synchronous
`get_or_rebuild` operation coalesces callers for one exact key; a same-thread
recursive rebuild fails rather than deadlocking. Applications choose eager, lazy
or background scheduling. No rebuild callback runs while parsing a document or
under a service/provider storage lock.

A result owns an immutable `entry` pin and separates rebuild errors from optional
`storage_error`. A failed cache write can still return a ready rebuilt value.
Failed or canceled rebuilds never become ready, and there is no automatic retry
loop. A later explicit call may retry. Validators, cancellation checks and optional
`source_is_current` checks run outside locks. For managed models, the final
`publish_cache` check must run on the store's owning thread even if rebuilding
happened in a background task.

`request_options` bounds rebuilt value bytes and supplies priority override,
validation, cancellation and source checks. The service bounds slots and resident
entry bytes, including outstanding invalidated pins; callbacks must bound their
own intermediate allocations. `cache::encode` and `cache::decode` use the bounded
native keyed codec and exact-message validation, not object layout. Pass normal
decoder limits for collections, allocation and nesting when decoding cache values.

## Application-controlled providers and placement

The `cache::provider` interface separates exact-key lookup, atomic publication,
invalidation, bounded enumeration and reclamation from the schema. Applications
may disable persistence by constructing a service without a provider, select a
per-document or shared cache domain, or implement another storage boundary.
Sharing a cache never grants source access or bypasses authorization.

The built-in `directory_provider` stores independently replaceable `.srcache`
entries in the chosen directory. It owns one stable `cache.lock`; share one provider
instance across local threads. A competing process/provider fails to acquire that
lock instead of racing publication and cleanup. This is serialized domain ownership,
not a distributed cache service. All operations remain synchronous.

Directory options bound entry size, total stored/staging admission, entry count,
scan count and free-space reserve. Publication stages and syncs a complete record,
then atomically replaces its selected filename. Lookup validates the entire key,
record version, length and digest before returning a hit. Entry names derive from
fixed key hashes; untrusted payloads never supply deletion paths. Roots/ancestors
and entry aliases must be ordinary directories/files. Only the provider's reserved
staging spelling is cleaned after acquiring its exclusive lock on reopen.

A ready pin owns bytes independently of the file. Ordinary eviction respects disk
lookup pins; explicit invalidation may remove disk data immediately while existing
owning pins stay readable. The application must check current authorization before
using a previously acquired value after revocation.

| Placement | Contract |
| --- | --- |
| Separate cache sidecar directory | Built-in `directory_provider`; independent file deletion and publication |
| Shared Serializer companion folder | Select `cache::companion_directory(document) / "cache"` |
| Application-selected/shared directory | Same provider with an explicit domain, quota and exclusive ownership |
| Inline/container or appended cache sections | Application-supplied provider with an explicit container boundary and atomic publication/compaction contract |

No universal inline/appended cache adapter is inferred for arbitrary application
formats. Such a provider must preserve exact authoritative slices and framing;
plain Serializer decoders still reject trailing bytes. Do not append unframed data
to an existing message or put optional data into required journal frames. The
provider interface permits application-specific placement without requiring a
paged store, database adapter or new mandatory dependency.

Save As may copy validated caches or start cold. Backups and portable exports may
omit caches. Rebind copied entries using document/source identity, never filenames.
Required external-document state metadata must accompany the main document.

## Shared companion layout and compatibility

New journals use `.<filename>.serializer/`, derived by Serializer from the document
path. For example:

```text
report.srl
.report.srl.serializer/
  writer.lock
  journal/
    changes-<generation>       # Required recovery data while selected
  cache/
    cache.lock
    <key>.srcache              # Optional, survives graceful close
  state/
    manifest                  # External-document binding, when needed
    snapshot-<generation>      # Retained closed external state, when selected
    save.prepared              # Unresolved host publication, when present
  staging/
    pending-<generation>       # Owned in-progress publication
```

The main native container or external manifest selects the exact generation;
folder existence never means recovery is active. Native container version 4 uses
the shared layout. External retired metadata uses version 5 and an immutable
state snapshot with no trailing operation records. The frame payload and existing
version-2/3 readers in this runtime remain compatible; older compilers/runtimes do
not understand the new container versions and must not write them.

Existing `.<filename>.journal/` and legacy flat layouts continue in their existing
locations until `migrate_journal_layout()` explicitly publishes a complete shared
checkpoint. Never manually rename live journal folders. Migration holds old and
new lock identities, publishes matching portable migration receipts before
selection, and retains legacy coordination guards. Unexplained simultaneous
layouts reject. Move/copy migrated documents with their shared directory **and
retained legacy guards**; do not discard those guards as cache or crash debris.

Use `.serializer`, not `.tmp`, for the shared folder: caches persist and required
state is authoritative. `.tmp` only names individually owned temporary artifacts.
A suffix or age alone cannot prove that recovery/publication data is disposable.
Unknown files deliberately prevent removing a nonempty journal folder.

## Graceful close and recovery retirement

`model_store::close_journal()` is an explicit synchronous, fallible save-and-retire
operation. Destructors release resources without implicitly saving. The application
stops admitting work and coordinates any required synchronization/external effects
before invoking close; Serializer preserves pending native session/outbox metadata
in the complete supplied checkpoint rather than silently losing it.

1. Require an idle, unfenced writer and reject unresolved prepared publication.
2. Encode complete current state: values, identities, allocator high-water marks,
   history/branches and applicable collaboration dependencies.
3. Migrate a legacy layout under both locks when necessary.
4. Durably publish a self-contained native saved baseline, or an external document
   plus selected durable `state/` checkpoint and manifest.
5. Retire unselected owned recovery files, retain caches/state/coordination, release
   writer ownership and detach the journal on success.

Native close folds the recovery suffix into an appended-container baseline and
requires no selected companion. External close publishes the current application
document and retains provenance with a version-5 state checkpoint; reopening validates it before
creating a fresh active journal generation. An external state snapshot is durable
metadata, not an evictable cache. Reopening preserves IDs/history and never reruns
application editing callbacks.

A failure before durable selection leaves the previous recovery path valid.
An uncertain replacement fences the live store and requires reopening. Cleanup
failure after selection cannot invalidate a successful Save: recognized obsolete
files may remain for later coordinated cleanup. Never recursively delete the
shared folder or unlink a writer lock while another writer/waiter can rely on its
identity. Discarding unsaved changes remains a separate explicit operation.

## Priority and actual reclamation

Within the selected domain, remove invalid/unusable unpinned entries first.
Among valid eligible entries, evict the lowest priority, then least recently used,
then stable key order. An omitted-priority entry (`0`) precedes priority 20, 100,
200 and 255. Pins may make a lower-priority entry temporarily ineligible; report
that exception rather than claiming the ordering was absolute.

`reclaim(target_bytes)` reports removed file bytes, observed available filesystem
bytes, pinned bytes, removed entries, shortfall and any error. Available space is
an observation and can change because of unrelated filesystem activity. Failed
removal and pinned entries can prevent reaching the target. Admission then rejects
new cache writes rather than consuming the configured reserve for durable saves.
Memory retention and disk retention are independently bounded.

For a custom inline/appended provider, logical removal need not release filesystem
space. Compaction must reserve scratch space and atomically preserve authoritative
sections. Prefer independent sidecars when space is already constrained.

## Ownership, trust and qualification

Applications own rebuild algorithms, recipe/dependency identity, security policy,
credentials, encryption, background scheduling, document-copy policy and external
business effects. Serializer owns declared field semantics, supported codecs,
guarded publication, bounded provider mechanics and documented journal decisions.
Dirty database pages or required recovery records cannot become caches merely
because a page cache or index is involved. Paged indexing/database adapters remain
separate subsystems.

Invalidate/purge sensitive derivatives when source access is revoked or source
retention policy requires it, regardless of priority. Bound decoded allocations
and authenticate storage where appropriate; a high priority is never a security
or durability guarantee.

Qualification covers actual generated output, numeric/version/identifier cases,
old wire identity, rebuild/corruption/priority/pinning, source invalidation,
close/reopen and failure boundaries, legacy migration, and editor behavior. See
[the dated record](../verification/verification-cache-2026-10-23.md) for toolchains,
executed counts, and any checks that remain outside the measured configuration.
