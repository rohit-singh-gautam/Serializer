# Collaborative retained-branch merging — 21 October 2026

Compiler/runtime **2.24.0**, schema language **1.21.0**. This worktree completes
the native retained-revision/authority contract in **MG04**. It does not publish
or commit a release. The [contract](../managed/branch_merging.md) documents request
fences, conflict policy, ownership and unsupported branch-import combinations.

## Executed checks

| Host | Profiles | Result |
| --- | --- | --- |
| Windows, MSVC 14.51, C++20 | direct32, separate32, direct64, separate64 | 18 GTests per profile; 72/72 pass through four CTest targets |
| WSL Linux, GCC 15.2, C++20 | direct32, separate32, direct64, separate64 | 18 GTests per profile; 72/72 pass with warnings treated as errors |

Windows configuration ran through `./make.ps1 configure`, followed by the real
CMake-generated compiler, internal record generation and native target pipeline.
The four CTests pass in 6.23 seconds. The CLI reports compiler 2.24.0 and schema
language 1.21.0. A subsequent root Windows `./make.ps1 test` build/regression
completed 279 tests: 261 passed initially; 18 needed investigation/rerun. Thirteen
passed with the required native tool/temporary-file access, and four Rust tests
passed with the already downloaded repository-local offline dependency cache.
Two core assertions were stale: newline output uses the existing `\\u000a`
spelling, and implemented C++ Protobuf fixed arrays now generate successfully.
Their corrected expectations pass with the complete core suite. All 279 tests
are accounted for across these runs; this is not a single uninterrupted pass.
Logs are `regression.log`, `rerun-failed.log` and `final-rerun.log`. Subsequent
QName generator changes have their own targeted record and are not covered by
the earlier full-regression checkpoint.

Linux configuration ran through root `make configure`. CMake rebuilt the current
runtime with XML/zlib enabled and Zstandard/LZ4 disabled. GCC compiled the current
maintained test source against that runtime and the current compiler's generated
portable C++ direct/separate headers and internal records. This is actual Linux
SDK/runtime execution, using Windows-generated headers; it is not a Linux compiler
rebuild or a full Linux regression. The first launch lacked the internal generated
header include directory; that command failure was corrected before all four
final profiles passed. GoogleTest is the retained 1.18.0 Linux dependency.

## Covered behavior

The thirteen existing native cases cover independent fields, retained immutable
pins, typed conflict diagnostics/resolvers, equal changes with durable ancestry,
identified map insertion/deletion, delete/edit conflicts, current authorization,
combined model invariants, comparison/history bounds, inherited/private fields,
lists, variant identity, labels, missing/ambiguous bases, malformed ancestry,
appended/sidecar journals, optional delta journals and interrupted durable writes.

Five new collaboration cases cover opt-in capability advertisement; exact target
revision and accepted-cursor fences; host-bound/read-only sessions; malformed
requests; accepted per-field deltas; actual replica application; exact successful
and rejected retries; changed operation payloads; selective session Undo/Redo
preserving another author's private-field edit; no-change ancestry; explicit host
resolution; callback reentry rejection; ancestry-only accepted changes; current
authority policy; mandatory lock admission; merge-work exhaustion; and fresh-epoch
recovery after each durable-write interruption.

Every storage/ID profile injects five existing journal boundaries under both
appended and sidecar publication. Before-append failures publish no decision;
uncertain writes fence the old authority. Fresh recovery selects one complete
old/new document and, when committed, both parents. A subsequent merge request
after recovery succeeds or returns `no_change` according to that durable decision.
Resolver callbacks are not replayed.

## Limits and retained evidence

Only authority-owned retained revisions are eligible. Arbitrary client-supplied
ancestry import, automatic stale-edit rebasing, schema overlays and standalone
history/identity conversion of merge-aware wrappers remain separate contracts.
Replicas retain accepted model state; the native ancestry graph stays with the
authority. Session rights, retry caches and policy administration remain host-owned
or ephemeral. No distributed deployment or cross-language native engine is claimed.

Focused build/test commands and logs are retained under ignored
`out/build/pending-active-2026-10-21/`: `build.log`, `merge-matrix.log`,
`linux-merge.log`, `merge_matrix.cmd` and `linux_merge.sh`. Their CMake outputs and
useful verification binaries are intentionally retained. Existing `.gitignore`
already covers these artifacts. The unrelated explanatory QName example remains
under `out/build/opc-r01-example/` as requested.
