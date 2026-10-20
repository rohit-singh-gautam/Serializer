# Low-memory paged indexing

Implementation work is tracked as **PG01–PG02** in
[the central pending backlog](../pending.md#managed-state-and-storage).

Status: research and proposed design, reviewed on **2026-10-11**. Serializer does
not implement a paged index, lazy managed storage, or a database adapter. The
candidate systems below implement disk-backed indexing themselves; they are not
qualified Serializer integrations. No performance improvement or memory limit is
claimed for an implementation that has not been built and measured.

Serializer can keep most of an index on disk and read only the pages needed for
each lookup. This fits its State Framework goals when index entries remain
derived from schema-owned records, stable object identity, transactions, history,
and recovery. It must preserve those contracts rather than introduce another
independently writable model.

**Indexing must preserve journal, history, authorization, and collaboration
behavior.** Keep the eager backend and its defaults unchanged. Indexes are derived
access accelerators, never the authority for model values, permissions, accepted
operations, or recovery. Every lookup belongs to a particular source snapshot;
using a faster lookup cannot skip the existing validation or host policy checks.

See the [current feature status](../project/feature_status.md),
[managed runtime](../managed/cpp_runtime.md), [journal contract](../managed/journal.md),
[database integration guidance](../guides/database_integration.md), and
[wire-format contract](../serialization/wire_format.md).

## Existing systems and candidate approaches

| System | Existing disk-backed indexing | Relevance and limitations for Serializer |
| --- | --- | --- |
| SQLite | Tables and indexes use separate disk B-trees; its pager reads, writes, caches, and commits fixed-size pages. [Architecture](https://sqlite.org/arch.html). | A candidate embedded adapter when a database dependency is acceptable. Its default page cache honors the configured cache target, but the target is not a process memory bound. [Cache configuration](https://sqlite.org/pragma.html#pragma_cache_size). |
| LMDB / OpenLDAP | A mapped tree with copy-on-write pages and reader snapshots; OpenLDAP's MDB backend is an existing application. [Developer paper](https://www.openldap.org/pub/hyc/20111010LDAPCon-MDB.html). | A candidate for read-heavy workloads with OS-managed page residency. Clean mapped pages are reclaimable; mapping size does not limit resident memory. Long-lived readers delay page reuse. [Memory and reader behavior](https://lmdb.readthedocs.io/en/release/#memory-usage). |
| RocksDB | Partitioned index/filter blocks use a smaller top-level index and load required partitions into block cache on demand. [Partitioned indexes](https://rocksdb.org/blog/2017/05/12/partitioned-index-filter.html). | A candidate for write-heavy key/value workloads. Budget memtables, index/filter blocks, iterator pins, and other allocations as well as the block cache; compaction needs separate qualification. [Memory accounting](https://github.com/facebook/rocksdb/wiki/Memory-usage-in-RocksDB). |

These sources establish feasibility, not a winner for an unmeasured workload.
A SQLite prototype offers existing indexing and transactional storage. A native
Serializer sidecar avoids a mandatory database dependency and can preserve the
existing document/journal containers. LMDB and RocksDB are alternatives to
evaluate against actual access patterns, write rates, platform requirements, and
dependency policy.

## What low memory means

A disk-backed index persists every required index entry on disk and caches a
bounded working set. Index size can exceed cache size. A cache miss costs I/O;
small caches can cause repeated reads, especially with random access or scans
that evict frequently used pages. Disk backing does not make lookup cost free.

Distinguish these measurements:

| Measurement | What it describes |
| --- | --- |
| Index cache bytes | Explicitly allocated page buffers and their metadata. |
| Other index working memory | Pinned pages, cursor state, pending updates, sorting buffers, and I/O scratch. |
| Decoded model/history bytes | Application objects, transaction candidates, retained snapshots, and model pins. |
| Virtual mapping bytes | Reserved address space; it does not imply that every mapped page is resident. |
| Process resident memory | Pages currently resident for the process, including resident file mappings; its precise accounting is platform-specific. |
| OS file cache | Reclaimable cached file pages managed by the OS, including buffered-I/O effects. |

Memory mapping lets the OS bring file pages into RAM when accessed. It does not
provide a hard resident-memory ceiling. Clean mapped pages can be reclaimed and
faulted back later, as the [LMDB memory documentation](https://lmdb.readthedocs.io/en/release/#memory-usage)
explains. Explicit buffered reads also interact with the OS file cache.

SQLite can disable mapped I/O with `PRAGMA mmap_size=0`. Its
`sqlite3_hard_heap_limit64()` bounds tracked SQLite heap allocations across the
process's connections, subject to the documented allocator/page-cache exceptions;
it does not bound Serializer objects or the whole process.
See [mapped I/O configuration](https://sqlite.org/pragma.html#pragma_mmap_size)
and the [heap-limit contract](https://sqlite.org/c3ref/hard_heap_limit64.html).

A future native index should begin with explicit page I/O and a budgeted cache.
Report its controlled allocations separately from process residency and OS cache.
An application requiring an enforced process limit also needs an appropriate
host/platform policy and must handle allocation failure. Such enforcement is
outside this index proposal.

## Current Serializer limits

The managed runtime's transaction begin decodes a complete root; commit validates
and encodes the complete candidate. History retains whole-root snapshots. Undo,
redo, and tree checkout restore a complete snapshot. Loading validates retained
history, and journal recovery reconstructs the managed state and retained history;
a record reader's bounded payload handling does not make recovery a lazy model
loader. See [history, persistence, and limitations](../managed/cpp_runtime.md#history-persistence-and-limitations).

The implemented `max_resident_bytes` admission estimate covers model/candidate/
history storage and outstanding immutable pins. Generated container allocations,
decoder temporaries, allocator metadata, and native resources prevent it from
being a hard allocator peak guarantee. Opt-in verified delta journal records
reduce selected encoded writes; they do not provide entity-delta history or lazy
root validation.

Paging only an index can reduce index memory. It cannot make an eager managed
model larger than available RAM usable. A genuinely larger-than-RAM model needs
a later payload/history backend and a compatible access contract.

## Recommended first phase: rebuildable sidecar index

Add an opt-in derived index alongside the existing authoritative document and
journal. This index sidecar is different from the required journal companion in
`journal_storage_mode::sidecar`: a missing journal companion currently fails
recovery; a missing derived index must be safely rebuildable.

Start with journal-sequence-to-record and retained-history-to-snapshot locations.
An optional entity index can map a document-scoped persistent ID to a validated
path within a containing root snapshot. The current snapshot format does not
provide independently stored entity payload records; locating the root does not
remove its decoding/validation cost. Add entity-to-record locations only when an
entity-record backend actually exists. Default linear history has a cursor and
ordered entries, not tree revision IDs; use generation-scoped entry ordinals
there and genuine revision IDs only for tree history.

Use fixed-size on-disk pages with a hierarchical sorted directory or B+tree.
Cache only a budgeted subset. The directory must itself be pageable: retaining a
complete key table or growing top-level directory in RAM would defeat the goal.
Keep a small root descriptor and stream range results through cursors.

### Durable metadata and portable ordering

Define new durable metadata in Serializer schemas and generate its codecs through
the normal CMake pipeline. The final format must declare its magic, payload
version, stable field identities, and compatibility policy before implementation.
This proposal assigns no new wire values and changes no existing journal frame.

| Index metadata | Required meaning |
| --- | --- |
| Format and comparator versions | Select the exact metadata layout, key encoding, and ordering contract. |
| Document namespace and schema binding | Prevent indexes from another document, schema, storage profile, or ID width from being reused. |
| Source generation | Bind offsets to the exact base and journal generation/files they address. |
| Indexed-through sequence | Identify the last completely indexed durable journal sequence, independently of undo position. |
| Root page, page size, and counts | Bound traversal and locate the index without loading its directory. |
| Page integrity and structural metadata | Detect malformed/truncated pages, invalid ordering, impossible counts, and invalid child references. |
| Record location | Source-file identity, checked byte offset/length, and containing-record identity; never a persistent object identity. |

Preserve generated `persistent_id` fields and the document namespace. IDs are
scoped, not globally unique; deletion, undo, compaction, and indexing cannot
renumber or recycle them. A file offset changes when storage is rewritten and
must never become an entity ID. Durable allocation high-water marks/reservations
remain authoritative model/journal metadata.

Specify numeric comparison for unsigned IDs/sequences without signed narrowing.
Either decode canonical keys for comparison or specify an order-preserving key
encoding independently of the payload codec. Native byte order, struct layout,
`size_t`, locale-dependent string comparison, and raw comparison of variable-length
encoded integers cannot define the durable ordering contract. Do not assume
ordinary Serializer binary byte order is lexicographic numeric order.

Validate offsets and lengths against the bound source file before arithmetic or
allocation. Bound key size, page size, tree depth, cursor count, and index file
bytes. Integrity checks detect damage; they do not authenticate an untrusted
index or permit skipping authoritative record validation.

### Recovery and publication

The document/journal owns durable truth. Treat the index as a projection with a
known freshness boundary:

1. Recover and validate the authoritative selected generation using the existing
   single-writer lock, frame integrity checks, incomplete-tail rules, schema
   binding, allocation marks, and uncertain-outcome fencing.
2. Open a candidate index only after validating its format, namespace, schema,
   generation, ordering, bounds, and indexed-through sequence. An index ahead of
   the recovered journal, pointing into a discarded tail, or bound to another
   generation is rejected. A missing or damaged index is rebuilt from validated
   records.
3. Catch up a valid stale index from its last complete sequence, including
   navigation, history resets, and allocation controls. Reads must either use an
   index known current for their pinned source snapshot or take a correct
   authoritative fallback. Never treat a stale index miss as proof of absence.
4. Flush the authoritative transaction before publishing the model, preserving
   the existing journal contract. Index publication may follow it. Failure of a
   derived-index update after a successful durable model commit marks the index
   stale/unavailable; it must not misreport that model commit as failed or cause
   an editing callback to be repeated.
5. Build replacement index pages as a new immutable generation. Flush them before
   publishing a small index manifest through the platform's durable replacement
   protocol. Recovery may discard any unfinished index generation and rebuild.

Pin the source generation and index generation together for every reader/cursor.
Index entries must not outlive the files they reference. Reclaim pages/files only
when no readers, retained history branches, journal recovery path, or replication
dependency needs them. A full Save may rewrite every offset: invalidate/rebuild
the old projection and publish a matching index generation. If reader pins are
introduced, extend source-file retirement accordingly; the current file adapter
does not already supply such a page-backed reader contract.

Bind each lookup to the exact transaction/read snapshot, not merely the latest
index generation. Tree revision ownership, linear history position, local pending
collaboration state, and accepted authority state are separate coordinates. Keep
accepted sequence advancement, exact operation retries/deduplication, ID mapping,
undo contribution checks, and authorization/lease evaluation on their existing
paths. An index may locate data for those paths; it cannot cache a permission grant
or decide acceptance. Index rebuilding cannot change observers' edit semantics or
cause transaction callbacks to run again.

Keep the existing base file's generation selection authoritative; do not make an
optional index manifest select the journal. Preserve the last explicit user-Save
baseline and `journal_dirty()` semantics. Rebuilding or checkpointing an index
cannot silently perform Save. Existing platform flush/filesystem assumptions
still apply; a process-crash test does not qualify every device for power loss.

### Bounded construction, pins, and transactions

Scan authoritative records with bounded frame/page readers. Accumulate at most a
configured run budget of checked key/location entries, sort a run, and spill it
to temporary disk storage. Merge sorted runs with a bounded fan-in and fixed
per-run input buffers; perform multiple merge passes when needed. Stream the
final ordered entries into leaf and directory pages with bounded builder state.
Deduplication must choose the specified last valid source sequence/presence, not
input-file enumeration order.

Do not retain all extracted entries, run names, merge cursors, or directory pages
in RAM. Store the run catalog on disk and read it in bounded groups. Count file
handles, metadata, decompression scratch, oversized keys, and output buffers in
the build budget. Enforce a separate temporary-disk budget. Cancellation or
disk-full removes only verified task-owned temporary generations and preserves
the last usable index and authoritative data. Rebuilding uses this same bounded
path; it does not first construct an O(N) in-memory map.

Include page buffers, metadata, and pinned pages in cache admission. A lookup
reserves its required pages before taking pins. If the budget cannot hold the
required traversal/cursor working set, return a documented resource failure
before taking excess pins or publishing state. Do not temporarily exceed the
budget because pinned pages cannot be evicted. Use scoped page guards; copied
locations or borrowed bytes with an explicit guard cannot become dangling
references after eviction.

Bound dirty pages/pending entries and transaction scratch independently. An
over-budget transaction must fail before authoritative publication, or use a
specified bounded spill protocol whose unpublished files remain recoverable or
discardable. Eviction must never write uncommitted index state into a published
generation. A commit cannot silently weaken the journal flush boundary to meet
the cache budget.

## Later phase: lazy model and history storage

A subsequent opt-in backend could own immutable per-entity payload records,
containment links, and pageable history metadata. Access would materialize only
the requested bounded working set, with changed entities prepared in an isolated
transaction. Its ownership must be authoritative; do not keep both normalized
records and an eagerly writable root as competing models.

Before claiming larger-than-RAM editing, design validation over unloaded entities,
stable identity, structural moves/deletions, undo/redo branches, current-state and
history projections, reader lifetimes, durable ID reservations, recovery, and
collaboration consistency. Full-root `read()` pointers cannot transparently become
lazy values without an explicit compatible API/profile. Keep the current eager
backend available and define migrations for every new durable binding. Paging
the index alone is not completion of this phase.

## Capability selection

The selection below records capabilities used by this proposal, not newly
implemented behavior. Check the pinned dependency before implementation.

| Capability | Selection and reason |
| --- | --- |
| Schema-generated durable records, payload versions, and bounded codecs | Used: future index metadata follows the established schema/CMake/codec workflow; bound exact-message reads and preserve existing wire contracts. |
| Document namespaces, persistent IDs, and allocation reservations | Used: index keys preserve the existing scoped identity and non-reuse contract. |
| Managed transactions and typed editors | Used: authoritative changes keep current isolation/publication semantics; index maintenance is derived work. |
| Linear/tree history and independent feature selection | Used: preserve the selected history mode, cursor/branches, labels, and compile-time absence of unused journal/collaboration support. |
| File streams and synchronous appended/sidecar journals | Used: authoritative recovery, durability, generation binding, and indeterminate-error handling. |
| Resident admission accounting and decode/file budgets | Used: existing limits still apply; additional index/builder budgets need explicit implementation and cannot be called a hard process-memory limit. |
| Collaboration and host policy hooks | Used when attached: index publication must match accepted local/authoritative state and cannot bypass policy or pending-work durability. No new transport or account system is needed for indexing. |
| Optional delta journal frames | Not required for phase one: they reduce some writes but do not provide lazy entity storage; existing opt-in use must remain supported. |
| Portable behavior, construction, generics, comparisons, borrowed output, compact integers, digests, compression, views, and constant evaluation | Not applicable to the research artifact itself: it adds no schema, codec, or application model. Evaluate relevant choices when defining real index records; none supplies a disk pager automatically. |
| Paged cache/index, external merge builder, and generation pinning | Unavailable in the current implementation: phase one requires the optional index backend and its tests. |
| Lazy entities/history, allocator-wide hard limits, and lazy recovery | Unavailable: require the later storage/access/validation backend; current whole-root operations remain. |
| Database adapters and native managed runtimes outside C++ | Unavailable: provider capability and portable record codecs do not qualify these Serializer backends. |

## Validation required before implementation claims

| Area | Required cases and evidence |
| --- | --- |
| Correctness and ordering | Point/range/missing-key queries; both ID widths; equal numeric prefixes; boundary values; deletion/restoration; namespace/schema/profile mismatch; platform-independent key fixtures. |
| Bounded working set | Index substantially larger than cache; tiny cache; random/scan workloads; pinned readers; cursor exhaustion; oversized keys; dirty-page limits; allocation failures; measured cache/metadata/builder peaks separated from model memory and residency. |
| Bounded build/rebuild | Multiple external merge passes; bounded run catalog/fan-in; duplicate keys and tombstones; cancellation; file-handle pressure; temporary disk exhaustion; no O(N) heap structures. |
| Staleness and recovery | Missing/corrupt optional index; stale/ahead sequence; generation mismatch; complete authoritative commit with failed index publication; incomplete journal tail; repeated catch-up/recovery; correct fallback misses. |
| Crash boundaries | Interrupt every record write/flush, index page flush, manifest replacement, and retirement step; recover exactly the authoritative state without retrying callbacks. Distinguish process interruption from separately qualified power-loss behavior. |
| Lifetime and compaction | Eviction with active guards; generation switches with readers; full Save rewriting offsets; retained branches; source-file retirement; no use-after-free or deletion of pinned files. |
| Serializer compatibility | Eager backend defaults and Save/journal bytes unchanged; cross-product of disabled/linear/tree history, labels where valid, journal feature/mode, collaboration feature/attachment, and both ID widths; ID reservations; indeterminate-write fencing; schema migration; local pending work. |
| Authorization and collaboration isolation | Same allow/deny and lease decisions with indexing enabled/disabled/stale; per-snapshot lookups; disjoint/conflicting edits; accepted sequence and exact retries unchanged; deduplication/ID mapping; conditional undo/redo; no policy bypass, duplicate callback, or mistaken post-commit failure. |
| Performance and platforms | Windows/POSIX implementation builds and real-file tests; lookup/rebuild/commit latency, I/O counts, write amplification, disk growth, cache hit rate, and resident-memory measurements on documented hardware/workloads. |

Research verification completed: the linked provider documentation was reviewed,
and the proposal was checked against the current runtime, journal, database, and
wire guides. No paged-index prototype, database integration test, memory benchmark,
or performance qualification was executed for this document.
