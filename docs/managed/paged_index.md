# Immutable paged uint64 indexes

[Documentation index](../README.md) · [Paged indexing design](../design/paged_indexing.md)
· [Native database](native_database.md)

`<rohit/paged_index.hpp>` supplies an opt-in derived unsigned 64-bit index whose
directory and entries live in fixed-size disk pages. It stores `(key, identity)`
pairs ordered numerically, with duplicate keys allowed and duplicate pairs
rejected. A bounded shared cache retains selected pages; an index can exceed its
cache substantially. This is a PG01 implementation slice, independent of a
third-party engine and of authoritative model storage.

The new manifest and source-binding records are defined in
`schemas/paged_index_records.serializer`, generated through the ordinary CMake
pipeline and installed with `Serializer::managed`. The native transport compiles
without generated-model dependencies. No existing schema, payload field identity,
managed envelope, journal frame or owning-model API changes.

## Binding, construction and lookup

A `paged_index_binding` includes the document namespace, authority epoch, source
revision and SHA-256 identities of the schema, index definition and exact source
snapshot. Supply every coordinate from the authoritative source; an arbitrary
filename or latest revision is insufficient. Opening compares the full generated
binding byte-for-byte. Foreign, stale, ahead, wrong-schema, wrong-definition and
wrong-source indexes reject before exposing any query result. A missing or
rejected derived index must be rebuilt or handled by an explicitly authorized
authoritative fallback; a failed index open is never a missing-key result.

```cpp
#include <rohit/paged_index.hpp>

namespace managed = rohit::managed;

// Populate every source_binding coordinate from the pinned authoritative source.
managed::paged_index_binding source_binding = /* checked source identity */;
managed::paged_index_options options;
options.max_cache_pages = 8;
options.max_cache_bytes = 128 * 1024;
options.max_run_entries = 1024;
options.merge_fan_in = 8;

// The producer may be unsorted. It borrows its source only for this synchronous call.
managed::paged_index::build_from("numbers.spi", source_binding,
    [](const managed::detail::paged_index_emit& emit) {
      emit({20, 7});
      emit({10, 2});
      emit({20, 4});
    }, options);

managed::paged_index index{"numbers.spi", source_binding, options};
auto cursor = index.equal(20);
while (const auto location = cursor.next()) {
  // location is an owning pair: (20,4), then (20,7).
}
```

`build` alternatively accepts a pull callback returning `bool` and filling one
`paged_index_entry`. `build_from` accepts a producer callback receiving an emit
function. These callbacks must traverse one pinned source without editing it or
performing business effects. They do not lend source pointers to retained pages.
An identity must be nonzero; a business key does not replace managed identity.

`range(lower, upper)` returns an inclusive cursor. `equal(key)` returns the same
cursor restricted to one key. Results are copied pairs, never references into
evictable pages. Cursors are move-only and own their immutable index file and
current leaf-page pin. They remain valid after the original index handle is
destroyed or the pathname is atomically replaced by another generation. Reads,
cursors and cache bookkeeping are confined to their creating thread. Recursive
queries from cancellation callbacks reject. Cancellation is cooperative and
cannot interrupt arbitrary work inside source/provider callbacks.

Native replacement uses the [pinned-generation file contract](native_database.md#physical-layout-and-bounds).
It requires a filesystem supporting atomic replacement while readers pin the old
generation. Unsupported capabilities fail without copying or discarding pins.

## Real database snapshot integration

`<rohit/native_database_index.hpp>` provides
`native_database_index_snapshot<Root, Descriptor>`. Its descriptor must be a
generated schema-declared indexed field descriptor with exact native value type
`std::uint64_t`; ordinary/transient/version descriptors do not gain this API.
Equality is available for ordered and unordered declarations; range queries require
an ordered declaration. The snapshot pins the accepted eager model, obtains its document/epoch/revision,
hashes the native root bytes and schema, and binds the descriptor's stable owning
class/field identity plus an explicit extraction-definition string. Rebuilding
traverses generated ownership once and emits actual persistent IDs directly into
the external builder, without a second in-memory key/object directory.

```cpp
const auto descriptor = rohit::managed::fields<model::entry>::field_key();
rohit::managed::native_database_index_snapshot<model::document, decltype(descriptor)>
    snapshot{database, descriptor, "entry.key.v1"};
snapshot.rebuild("entries.spi");
snapshot.open("entries.spi");
auto matches = snapshot.equal(10);
if (const auto location = matches.next()) {
  const auto entity = snapshot.load(location->identity, 10000);
  // entity pins the exact typed occurrence from snapshot's source, even after later commits.
}
```

The selected population is the exact managed entity type within this complete
root, including all its owned collection occurrences. No collection-specific,
cross-document or database-global uniqueness is implied. Snapshot and bound-cursor
`load` explicitly traverse the already owning model under a work cap; they perform
no I/O and are not lazy record storage. Returned cursors retain the exact source
pin and binding alongside the immutable index generation, even if their snapshot
wrapper is destroyed. Copied bare locations still need their binding/source pin
for later resolution. Accepted database changes do not mutate old source
pins. Capturing a newer snapshot and opening an older derived file rejects;
rebuild the new snapshot explicitly. A failed derived rebuild never changes a
successful database commit or reruns its edit callback.

## Bounded external construction

Construction accepts unsorted input and stores at most `max_run_entries` pairs
in its sort buffer. Completed sorted runs spill to task-owned files. Numeric
pass/ordinal names replace an unbounded in-memory run catalog. Each merge opens
at most `merge_fan_in` inputs, each with one page-sized read buffer, a bounded
head heap and one output buffer. Each temporary block has a checked count and
SHA-256 integrity binding; a bit flip cannot silently become a valid final page
merely because its modified pairs remain sorted. Block overhead is charged to
the same temporary-byte budget. Multiple passes handle arbitrarily many runs
within entry/file/disk limits. Duplicate pairs reject both within and across runs.
No initial complete in-memory map or whole-source key table is built.

The final sorted stream creates leaf pages and a disk catalog of page maxima.
Each directory level is built by streaming that catalog into branch pages and
another catalog. The directory itself remains pageable; construction retains no
complete upper-level key table. Empty indexes have a valid empty leaf. Every
page/file offset and size is checked before arithmetic, indexing or I/O.

`max_entries`, `max_file_bytes`, `max_depth`, sort-run size, merge fan-in and
`max_temporary_bytes` are independent limits. Temporary-byte admission includes
buffered pending output and live runs/catalogs/staged pages; obsolete runs are
removed only after their consumers close. `paged_index_build_statistics` reports
entry/page counts, initial runs, merge passes, peak sort entries, peak open merge
inputs and peak charged temporary bytes. Source callback allocations, standard
sort stack, filesystem cache and allocator implementation overhead remain
separate from controlled buffer/disk admission.

## Cache, pins and exact controlled allocation charges

`max_cache_pages` bounds resident page objects, including every cursor-pinned
page. `max_cache_bytes` independently admits actual allocation requests for the
owning state/control block, PMR page buffers, shared page control blocks, map
nodes, cursor state/control blocks and permanent generated-manifest capacity.
The allocator checks before forwarding an allocation. Unpinned pages can be
evicted; pinned pages cannot. If sufficient space cannot be reserved, the query
or cursor step fails with a resource error without temporarily exceeding either
limit. Releasing cursors/pins makes that capacity available again. A one-page
cache can traverse the directory and scan leaves by releasing each prior page
before loading the next; concurrent cursor pressure may require more pages.

`statistics()` separates resident page bytes, total controlled cache bytes,
their measured peaks, pinned pages, live cursors, physical page reads and cache
hits. These are library allocation/accounting quantities, not process RSS, OS
file cache or a whole-application allocator ceiling. The small allocator-manager
object, native file resources, caller-owned callback state and temporary manifest
decode/header buffers are outside the persistent cache domain and remain bounded
separately by the selected page/record limits and host lifecycle.

## Integrity and publication

The physical file uses a generated versioned manifest, explicit little-endian
numeric page headers/pairs, fixed page sizes and SHA-256 page integrity checks.
The comparator version selects unsigned `(key, identity)` ordering independently
of host byte order or variable-length payload encoding. Reads verify checksums,
page identity/kind/count, numeric order, child/leaf bounds, monotone leaf links,
directory maxima and declared depth before exposing results. Corruption and
truncation reject; the reader never repairs authoritative source bytes or
silently substitutes an incomplete scan. Checksums detect damage and do not
authenticate maliciously rewritten indexes.

A stable `<index>.writer.lock` serializes builders. All runs and output pages are
created in an exclusively reserved sibling generation directory. The complete
file is flushed before atomic pathname replacement; both affected directories
are synchronized under the native `file_stream` durability contract. Before
replacement, interruption preserves the previous named generation. After
replacement, the named file is the complete new generation. Existing readers
retain their old file handle and pages until their final pin closes. Replacement
does not mutate cached old pages, rewrite the source journal or select a model
generation. Normal exceptions clean only the task-created scratch directory.
Abrupt process termination can leave an unselected staging directory; it never
becomes a ready index merely by existing. Coordinate removal after stopping its
builder and verifying the directory is an unused generated sibling.

Fault hooks exist solely for deterministic qualification after initial run writes, before page writes,
after file flush, before replacement and after replacement. A post-replacement
failure may have published the new derived generation; reopen against the exact
source binding to resolve readiness. It never makes the authoritative model
commit fail. Power-loss guarantees remain specific to the qualified volume.

## Scope still pending

This implementation supplies immutable complete index generations, bounded
external construction, pageable numeric directories, budgeted caches/pins,
corruption rejection and explicit eager database snapshot integration. It does
not automatically provision schema-declared persistent placement, incrementally
catch up source journals, mutate dirty pages in place, index strings/composite
keys, or implement lazy authoritative objects/history. PG01 remains open for
those broader contracts and operational/platform qualification. PG02's paged
payload/reference leases, unloaded-object validation and larger-than-RAM editing
are not established by paging this derived index.
