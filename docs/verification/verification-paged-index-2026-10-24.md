# Immutable paged uint64 index verification, 2026-10-24

[Documentation index](../README.md) · [Paged index API](../managed/paged_index.md)

Scope: the first PG01 immutable derived uint64 index and its explicit eager
native-database snapshot integration. This record does not close PG01/PG02 or
qualify lazy authoritative objects/history, automatic persistent placement,
incremental dirty-page mutation or cloud storage.

The generated durable manifest/source-binding schema is
`schemas/paged_index_records.serializer`; the compiler-independent implementation
is `src/paged_index.cpp`. Public wrappers are `paged_index.hpp` and
`native_database_index.hpp`. All previous user payload, schema and journal wire
identities remain unchanged.

## Implemented qualification matrix

`test/paged_index_test.cpp` supplies ten tests:

- An unsorted 4,097-entry source requires over 100 initial runs, several bounded
  merge passes and a multi-level pageable directory. Full numeric ordering,
  duplicate keys, zero/high-bit boundary keys, equality/missing/range results and
  empty ranges are compared with an independent in-memory test oracle. A complete
  scan uses at most one resident page; the test oracle is not builder memory.
- Two independent cursors pin distinct leaf pages. A third traversal fails
  admission at the two-page cap; releasing a cursor makes progress possible.
- Namespace, epoch, stale/ahead revision, schema, definition and exact source
  digest mismatches reject before query results.
- Interruption before page writes, after file flush, before replacement and after
  replacement preserves either the complete old or complete new named generation.
  Previously pinned readers continue traversing their original generation.
- Corrupt manifest/leaf bytes and truncated files reject without file repair or
  false missing-identity results.
- A temporary-run identity bit flip preserves pair ordering but must fail the
  block checksum before new generation publication; the old index stays usable.
- Duplicate pairs across external runs, source-entry limits, temporary-disk
  limits, final-file limits and cancellation fail without replacing the old
  complete index; ordinary failures remove their exclusively created staging
  directories.
- An empty generation has one valid empty leaf. Cross-thread query access rejects.
- Cursor/control-block metadata exhausts the same controlled byte budget as pages;
  release restores admission. Cancellation callbacks cannot recursively query.
- Actual generated uint64 descriptors traverse real native database identities.
  Index queries and explicit bounded identity loading remain bound to the pinned
  eager source across a durable commit; a new source rejects the old derived
  generation and can rebuild it without changing the accepted database revision.
  Bound cursors preserve their own source after the wrapper is destroyed; ordinary
  descriptors reject and unordered declarations do not gain range capabilities.

The same source is registered by the root task with all four generated managed
representation/identity-width profiles. Cache tests inspect actual controlled
allocation requests for state/control blocks, PMR buffers, map nodes, cursors and
permanent manifest capacity, separately from resident page count and process RSS.

`paged_index_crash_helper.cpp` and `paged_index_crash_test.py` additionally terminate
fresh helper processes without C++ unwinding at each page/flush/replacement
boundary. Two subsequent processes query the exact expected complete generation
using a one-page cache. Unselected staging files never become readable merely
because their writes or flushes completed.

## Execution status and limits

Windows x64 MSVC 19.51 C++23 Release passed all **40/40 paged-index tests**
(ten under each direct32/direct64/separate32/separate64 profile), together with
56/56 standalone schema-index tests and the four-boundary subprocess crash matrix.
Three native file-generation tests also passed, including 32 varying UTF-16 path
lengths with old reader pins and an open replacement writer. The passing focused
run contains 22/22 CTest targets, 314 passing unit cases and one existing skip.

Builds and CTest used one worker. Full log:
`out/build/managed-contracts-windows-passing.log`; machine-readable counts:
`out/build/managed-contracts-windows-summary.json`.

Initial runs exposed Windows pinned-file replacement and path-termination defects,
plus an incorrect include-test exception expectation. Those were corrected before
the passing run. The complete earlier logs remain
`out/build/managed-contracts-first-pass.log` and
`out/build/managed-contracts-second-pass.log`.

No Linux run, allocator-failure campaign, physical power failure, adversarial
checksum rewriting, peak process-RSS benchmark, large-file ZIP/provider test,
incremental update/catch-up, automatic source-file retirement, backup/compaction,
string/composite key support or lazy authoritative record/history engine is
claimed. The sort/merge/cache limits bound controlled library resources and do
not turn the existing eager owning root into a larger-than-RAM document.
