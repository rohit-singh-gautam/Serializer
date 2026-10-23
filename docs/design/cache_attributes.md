# Rebuildable cache attributes and storage

[Documentation index](../README.md) · [Design index](README.md)

Status: **proposal for discussion**, 2026-10-23, reviewed against compiler/runtime
2.28.2 and schema language 1.21.0. No cache attribute, persistent cache service,
priority eviction, shared `.serializer/` companion layout, or graceful-close
journal retirement is implemented by this
document. Example syntax and operation names below are illustrative, not accepted
schema syntax or existing APIs. No release or wire version changes are made.

## Intent

Add an explicit `cache` field category for values that can be rebuilt from
authoritative data. Unlike `transient`, a cache may survive loading and application
restarts. Unlike ordinary durable fields, it may be removed without losing user
work. Cache priority is optional and defaults to `0`, the lowest retention
priority: lower priorities are reclaimed first; higher priorities are reclaimed
last. The application chooses where caches live.

The requested lifecycle separates recovery from acceleration: **after a successful
graceful close, recovery journal data is deleted and cache data remains**. A cache
must therefore have a lifetime independent of its neighboring journal files,
even when the application places both in the same directory.

Typical candidates are thumbnails, parsed display data, computed statistics,
search accelerators, and expensive geometric derivations. Original media,
unsaved edits, pending collaboration operations, allocation high-water marks,
authorization policy, or the only copy of imported content are not caches.
An artifact needed for correct offline operation must be durable if its source
cannot be retained or otherwise made available for rebuilding.

## Existing behavior and the proposed distinction

Current [`transient` fields](../managed/cpp_runtime.md#runtime-state-and-copying)
are excluded from codecs, history equality, collaboration, identity traversal and
the durable schema fingerprint. Ordinary C++ copying retains normal member-copy
semantics. Managed runtime traits reset transient fields after changed durable
publication, history navigation and loading. Guarded `update_runtime` updates
publish isolated runtime candidates without durable history or journal changes.
The parser rejects durable identity/metadata on transient declarations.

Current [journals](../managed/journal.md#durable-decisions-and-files) store required
recovery metadata, not disposable cache data. New layouts use
`.<filename>.journal/`; legacy flat layouts remain supported. Missing required
companions fail recovery, and Save retains history dependencies. Closing the
application does not currently imply the retirement protocol proposed here.

| Property | Ordinary durable field | Existing `transient` | Proposed `cache` |
| --- | --- | --- | --- |
| Source of truth | Yes | No persistence contract | No; rebuild from retained sources |
| Ordinary document codecs | Included unless explicitly omitted | Excluded | Excluded from authoritative payload |
| May survive restart | Required by its persistence contract | No generated persistence | Optional, through a separate cache channel |
| Missing value | Follow normal field rules | Schema/runtime default | Cache miss, even if a default value exists |
| History, collaboration, dirty state | Normal durable behavior | Excluded | Excluded; use the selected source snapshot |
| Storage cleanup | Requires durable retention rules | Memory lifetime | Priority-based eviction of eligible cache entries |
| Successful graceful close | Save according to document policy | Released with the process | Keep persisted entries; do not purge on close |

`cache` should be a distinct declaration category sharing suitable runtime-field
machinery with `transient`. Do not silently make existing transient fields
persistent or reinterpret format-specific `omit(...)` as a cache declaration.

## Proposed declaration and identity

Illustrative declaration shape, deliberately without a supported language header:

```text
class article stable_ids {
  public string body (1);
  public transient uint64 session_counter {0};
  public cache bool has_links {false};
  public cache(priority = 200) uint64 estimated_word_count {0};
  public cache(priority = 20) string preview_markup;
}
```

The first value is authored content; the last three are derived. A valid cached
count of zero must be distinguishable from an absent count. Defaults initialize
storage but never prove a cache hit. Recommend an unsigned 8-bit priority,
`0..255`, with optional `priority` defaulting to `0`; these are proposed policy
values, not wire values. Bare `cache` and `cache(priority = 0)` have identical
declared retention priority. Omitting priority does not select the highest value
or give the cache automatic protection. This lets authors opt inexpensive derived
data into caching without requiring a priority choice, while explicitly raising
priority expresses a preference to retain more valuable results.

| Declaration | Declared priority | Retention preference |
| --- | --- | --- |
| `cache` | `0` | Lowest; reclaimed first among eligible valid entries |
| `cache(priority = 0)` | `0` | Same as omitting priority |
| `cache(priority = 200)` | `200` | Retained after eligible lower-priority entries |
| `cache(priority = 255)` | `255` | Highest; still disposable |

Priority may be overridden by application policy within an explicit cache domain.
Increasing it changes eviction order, not correctness or persistence guarantees.

Do not assign cache fields ordinary payload IDs or include them in the durable
schema fingerprint. Use a separate cache descriptor identity, scoped by declaring
type and concrete specialization, and a separate cache-format/recipe version.
An application-supplied stable cache key permits reuse across generated-name
changes; otherwise a renamed field may safely become a miss. Descriptor collisions
must reject, rather than alias different values. Cache declaration changes must
not renumber ordinary fields, including implicitly assigned field IDs.

Start with scalars and owning values whose entire graph is disposable. Reject
cache modifiers on magic, payload versions, managed identity, wire-only size
headers, and fields needed to establish required durable references. Do not make
cache contents independently identified managed entities. Native handles,
borrowed pointers, process-local addresses and opaque host objects need explicit
application codecs or remain memory-only. Initially reject reference-bearing
cache graphs until snapshot resolution and ownership have a qualified contract.
Durable state must never depend on a cache-owned object's continued existence.

## Validity is separate from retention

A high-priority stale result is still unusable. Before accepting an entry, match:

- Document identity and, where relevant, entity identity, declaring type and cache
  descriptor. Paths and filenames alone are not document identities.
- The exact source snapshot or a canonical dependency fingerprint. Start with
  conservative whole-durable-root invalidation; later field-level dependencies
  must cover membership, ordering, deletions, referenced objects and schema defaults.
- The application recipe version, cache payload format, parameters and relevant
  external inputs, such as renderer settings, locale or provider/model version.
- Bounded lengths, supported encoding, integrity metadata and successful payload
  validation. A checksum detects damage; it does not establish authenticity.

Use a content fingerprint or durable source identity that cannot be reused for
different states. A timestamp, process-local view generation or journal sequence
alone is insufficient across Save, recovery, branching and document replacement.
Cache payload versioning is independent of schema-language and document revisions.

The application supplies the rebuild recipe and external dependency versions;
Serializer cannot infer them from native method bodies. Persistent cache reuse
requires an explicit recipe identity. If dependencies cannot be established,
rebuild or use a process-local entry. Time-to-live may supplement invalidation,
but cannot replace exact source matching for correctness-sensitive results.

Build from a pinned immutable source. Before publishing, verify that the target
still selects the source fingerprint; discard stale asynchronous results. Undo,
redo, branch switching, import, recovery and collaboration select a different
source and therefore invalidate or reselect entries. Never attach a result to a
new state merely because the entity ID is unchanged. Cache-only publication must
not create history, document dirty state, journal records or collaboration traffic.
It may notify local views that derived data became available.

## Loading and rebuilding

Load and validate authoritative state first, including required journal recovery.
Then consult optional caches for that exact state. Missing, evicted, outdated,
unsupported or corrupt entries are misses; discard them and rebuild on demand or
according to the application's eager/background loading policy. A cache directory
must not be required to open a valid saved document.

Expose at least absent, building, ready and failed/retryable states separately
from the cached value. The first implementation may rebuild synchronously;
background scheduling is an application choice. Coalesce concurrent rebuilds for
the same key, bound work and output, support cancellation, and avoid retry loops
under disk pressure. Readiness is per entry; partially written bytes are never a hit.

Do not execute arbitrary rebuild callbacks inside codec parsing or while holding
publication/storage locks. Publish validated owning results through a guarded
runtime path with immutable read pins. A failed rebuild or cache write leaves the
durable model unchanged. The application can report an unavailable feature or
retry later; a cache miss does not make the document corrupt. Cache reads and
rebuilds still obey application authorization and resource limits.

## Application-controlled placement

Separate schema metadata (disposability and default priority), application policy
(location, quotas, rebuild timing and sharing), and storage mechanics (bounded
read/write, publication, locking and reclamation). A proposed provider boundary
should support lookup, publish, invalidate, enumerate and reclaim, with explicit
ownership, cancellation and error outcomes. These are responsibilities, not API
signatures. The application may disable disk caching entirely.

| Placement | Proposed behavior | Reclamation constraint |
| --- | --- | --- |
| In the document/container | Optional cache parts/sections alongside the authoritative payload, using an explicit future container contract | Removing entries may require an atomic document rewrite; preserve all durable sections |
| Appended to a file | Dedicated framed cache region or a separately indexed extent set, distinguished from recovery frames | Interleaved entries generally require compaction; removing an index entry does not free file bytes |
| Separate sidecar | Independently replaceable cache file(s) keyed to the source document | Whole-file entries can be removed after readers release them |
| Within a shared Serializer companion directory | Separate `journal/` and `cache/` namespaces with independent lifetimes | Retire recovery files selectively; retain caches and required metadata |
| Application-selected directory/provider | Per-document or shared cache with explicit ownership and domain quotas | Require safe identity, concurrent access and reference rules for shared entries |

## Shared companion directory naming

Recommend `.<filename>.serializer/` for the default shared companion directory.
Its name identifies the owner without tying the whole directory to one artifact's
lifetime. The application supplies the document path; Serializer would derive
this default beside it. Application-selected cache providers and locations remain
supported by the proposed policy boundary; this naming recommendation does not
require every cache to use this directory.

The proposed layout is:

```text
report.srl
.report.srl.serializer/
  writer.lock
  journal/
    <recovery files owned by the journal adapter>
  cache/
    <cache manifest and independently disposable entry files>
  state/
    <required durable metadata, only if not saved in the main document>
  staging/
    <in-progress files tracked by their owning operation>
```

`journal/` holds recovery data until successful save-and-retire; `cache/` survives
graceful close and follows cache eviction policy. Optional `state/` is authoritative,
not evictable: identities/history or other required metadata must remain there if
the main document cannot carry them. It must not become an unchecked substitute
for the self-contained-save/metadata contract discussed below. Create only needed
subdirectories. Keep lock infrastructure at the shared root so retirement of a
journal generation cannot replace the writer lock's identity. Staging files follow
their operation's recovery rules; age or a `.tmp` suffix alone does not prove that
an in-progress publication can be deleted safely.

Do not use `.tmp` for the shared directory: that name suggests that its entire
contents can be discarded. Recovery records can be the only copy of accepted work,
cache entries intentionally persist, and required durable metadata is never
temporary. A `.journal` suffix is too narrow for a directory serving all three.
Keep `.tmp` for individual temporary artifacts where their owner defines cleanup.
`.serializer` here names a directory, distinct from a `.serializer` schema file.

This is a proposed replacement for the shared folder name, not a layout supported
by the current journal writer. Do not rename existing `.<filename>.journal/`
directories manually. A future implementation must continue recognizing existing
directory and legacy flat layouts, validate document/generation binding, and
migrate under exclusive writer coordination with crash-safe selection. Old writers
must not operate on migrated documents unless qualified for the new layout; a
stable lock location alone does not make their protocols compatible. Simultaneous
old and new layouts must produce a clear conflict until an explicit recovery or
migration operation resolves ownership, rather than guessing which to delete.

On reopen, a retained companion directory containing only caches and coordination
files must not imply an active recovery journal. Determine recovery from validated
selected metadata, not the folder suffix or existence. Journal retirement must
leave cache and durable-state namespaces intact; the shared root may be removed
only when all owned contents are safely retired and writer coordination permits it.

## Container boundaries and portability

The ordinary payload decoder must continue to require its exact message slice.
Do not append unframed cache bytes to current Serializer messages, add unknown
cache fields to positional codecs, or place optional bytes in existing journal
frames. Inline/appended support requires a versioned container boundary and an
explicit reader compatibility decision. Start with an independent sidecar/provider.

Applications decide whether Save As copies validated caches or starts cold,
whether backups include them, and whether portable exports carry optional cache
parts. Rebind copied entries to validated document/source identities; never use
path similarity as proof. Regeneration after a move is an acceptable default.

## Graceful close and journal retirement

Treat graceful close as a successful save-and-retire protocol, not merely a
destructor call or normal process exit:

1. Stop admitting edits, establish the closing state and coordinate in-flight
   transactions and required collaboration acknowledgement/publication.
2. Save and durably publish every authoritative dependency needed after restart.
   Preserve document/entity identity, allocation high-water marks, retained
   history/branches and pending synchronization metadata wherever their contracts
   require them. Move such data out of retiring recovery files first.
3. Publish a durable selection indicating the document no longer depends on the
   retiring recovery generation. A crash here must leave either the old complete
   recovery path or the new self-sufficient saved state selected.
4. Delete only the retired recovery files under the storage adapter's ownership
   and writer coordination. Keep cache entries and any lock infrastructure still
   required by the locking protocol. Release the writer lock after retirement.
5. Reopen from the saved state and independently validate retained cache entries.

A failed Save, unresolved journal outcome, unavailable publication target or
unfinished required synchronization prevents successful retirement. Retain the
recovery journal and report the close outcome; do not delete the only copy of
accepted work. A crash before retirement retains recoverability. Deletion failure
after durable retirement may leave identifiable obsolete files for a later
coordinated retry, but cannot invalidate the saved document or delete cache data.

Discarding unsaved changes is a separate explicit operation and must leave a
valid saved baseline; it is not equivalent to committing them on close. An
application may deliberately omit cross-session undo history only under an
explicit history-retention policy. Never infer that policy from closing a window.

Current native sidecar bases select required companions, even empty ones;
deleting those files after existing `save_journal()` breaks recovery. External
document bytes may also lack Serializer identities/history held in their journal
metadata. The proposed retirement therefore needs a qualified self-contained save
or separate durable metadata contract and journal reattachment protocol. Do not
recommend manual deletion with today's APIs. Existing `writer.lock` files are
coordination infrastructure, not recovery payload; never unlink/replace a lock
while a writer or waiter may rely on its identity.

## Priority eviction and real space recovery

Priority applies within an application-selected storage domain, such as one
document cache or a shared directory with an explicit quota. Do not compare
unrelated documents globally unless the application selects that policy.

1. Remove invalid, expired or orphaned entries when ownership and reader rules
   permit, regardless of priority; they cannot provide a usable hit.
2. Among valid, reclaimable entries, select the lowest numeric priority first.
3. Within a priority, default to least recently used, with a stable key tie-break;
   an application may select another within-priority cost/size policy.
4. Continue until the actual reclaim target or low-water threshold is reached.
   Report reclaimed physical bytes and any shortfall, including pinned entries,
   fragmentation or failed deletions. Stop admitting new cache writes if needed.

Example: a cache with omitted priority (`0`) is evicted before a priority-20
preview, then priority-100 statistics, then a priority-200 search accelerator,
when all are valid and reclaimable in the same domain. Priority 255 is still
disposable. Priority never turns a cache into durable data, and never permits
deleting required recovery records or authoritative content.

Pins protect current readers until their handles release; leases must be bounded
or accounted for. If only high-priority entries are reclaimable, they may be
removed even while lower-priority entries remain pinned; report that exception.
Permanent retention belongs to durable storage, not an unevictable cache flag.
Memory residency and disk retention are separate budgets and policies. Removing
a disk entry must not mutate a published in-memory object held by a reader.

Account for payload, metadata, staging files, pinned retired files, compression
buffers and compaction scratch. Appended/inline deletion can make bytes reusable
without returning space to the filesystem. Compaction needs reserved headroom,
atomic replacement and a failure path that preserves durable data. Prefer
independently removable sidecars under low-space conditions. Cache writes and
compaction must not consume storage reserved for saving authoritative work.

## Failures, concurrency and trust

Publish entries by bounded staging plus atomic selection, so a crash leaves a
complete old entry, a complete new entry, or a miss. Optional cache failures must
not fence an otherwise healthy durable store. A shared-file I/O failure affecting
authoritative bytes still follows normal durable error handling. Cache writes
must not trigger an unbounded durable rewrite on ordinary edits.

Coordinate rebuilds, eviction and document close with pins and generation checks.
Deletion targets must come from validated provider-owned entry identities, never
untrusted paths embedded in cache data. Respect concurrent writers, directory
boundaries and application-selected sharing rules. Enumeration must distinguish
required journal/state artifacts from cache entries; never recursively delete the
shared companion directory as a cache-cleaning shortcut.

Treat persisted cache payloads as untrusted input: enforce cumulative decoded-size,
entry-count, nesting and decompression limits before allocation. Validate values
against the source contract. Applications requiring protection against cache
forgery need authenticated storage or independent recomputation; a source hash
alone does not prove that an arbitrary result was derived correctly.

Caches can contain sensitive derivatives. Apply appropriate permissions,
encryption and retention rules. Source deletion or access revocation can require
immediate invalidation/purge regardless of priority. Sharing a cache does not
grant access to its source or bypass authorization checks.

## Suggested implementation sequence and acceptance

Keep this design independent of the proposed [paged backend](paged_indexing.md)
and [database adapters](database_storage_requirements.md). A reconstructible
index can be a cache; a dirty page containing uncommitted or otherwise unique
authoritative data is not. No paged/database subsystem is supplied by this idea.

First implement explicit cache descriptors, miss/readiness semantics, guarded
publication and conservative source invalidation for owning C++ models. Then add
an application-selected sidecar provider with priority eviction and accounting.
Qualify graceful-close save/retirement independently before enabling it. Consider
inline/appended containers, fine-grained dependencies, shared deduplication and
other language services only after the basic contract is measured.

Before shipping, require evidence for:

- Unchanged ordinary wire bytes, IDs, durable fingerprints and public APIs for
  existing schemas; unchanged transient behavior and old-reader compatibility.
- Cold load with every cache removed; valid reuse; zero/empty hits; invalidation
  after edits, undo/redo, branching, recovery, replacement and recipe changes.
- Omitted priority matching explicit `0`, valid endpoints `0` and `255`, rejection
  of negative/out-of-range priorities, deterministic ordering and equal-priority
  ties; pinned-reader exceptions, quota accounting, partial reclaim, actual
  filesystem space recovery and low-space Save admission.
- Missing/corrupt/truncated/oversized entries, write denial, cancellation and
  failure during staging, publication, deletion and compaction.
- Graceful close deleting retired recovery data while retaining usable caches;
  reopen without the retired journal; failed Save retaining recovery; crash
  injection at each save/selection/retirement boundary in supported layouts.
- Shared `.serializer/` directory selection and migration from `.journal/` and
  legacy flat layouts; conflicting layouts, old-writer exclusion, cache-only reopen
  and namespace-specific cleanup preserving required state and writer locks.
- Both native and external-document save contracts preserving required identities,
  history, allocation state and collaboration dependencies across retirement.
- Concurrent rebuild/edit/evict/close, immutable pins, safe provider paths,
  authorization changes and bounded resource use.
- Parser version gating, generated output, compatibility metadata, editor grammar,
  diagnostics and documentation for every output that accepts cache declarations.
  Preserve previously valid schemas that use `cache` as an ordinary identifier or
  type name. Unsupported outputs/combinations must diagnose explicitly.

New grammar requires a schema-language minor release; new compiler/runtime APIs
require a compatible minor release. Synchronize both editor versions when their
support changes. Cache/container format versions remain separate. Promote accepted
implementation scope into [the central backlog](../pending.md); this discussion
does not schedule implementation or change existing deferred subsystem statuses.

Open decisions are the final declaration/key syntax, first supported value graph,
minimum source fingerprint, provider API, self-contained close-save representation,
shared-cache ownership and the application priority-override interface. The
proposed schema default is settled here as optional priority with value `0`.
The recommended initial direction is a distinct disposable
field category, conservative validity checks and application-selected sidecars,
with journal retirement as a separately verified lifecycle requirement.
