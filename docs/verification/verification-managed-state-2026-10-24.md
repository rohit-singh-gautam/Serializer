# Managed state and storage implementation checkpoint — 24 October 2026

Compiler/runtime **2.34.0**, schema language **1.24.0**, worktree checkpoint.
The [pending inventory](../pending.md#managed-state-and-storage) remains the
source of truth for parent status. MG07 and MG08 meet their recorded acceptance contracts; subsets of other parents
do not close those requirements. See the [completion audit](verification-managed-completions-2026-10-24.md).
The maintainer authorized deferral of database-dependent work on this date;
no unfinished implementation has been classified as will not fix.

| Parent | Implemented work and executed evidence | Remaining gate |
| --- | --- | --- |
| MG01 | Native Python: 25 runtime tests and 281 actual SDK checks across inline/included layouts. Native Java: 188 runtime and 528 actual SDK checks across eight profiles, six crash boundaries and writer exclusion including aliases. Python/Java token exchange passes 5,310/4,953 checks with independent C++ controls. JavaScript/TypeScript adds 384 actual SDK checks, 96 strict TS rejection cases, 204 Windows kernel checks and native crash/alias controls; see its [replay](verification-javascript-managed-runtime-2026-10-24.md) and the [earlier languages](verification-managed-languages-2026-10-24.md). | Native engines in Go, C#, Rust, Swift, Kotlin and C; broader non-C++ capability/profile/platform qualification. |
| MG07 | Resource-owning C++ payload containers, binary decode scopes, managed callbacks/history and native preparation integration. Combined XML/managed generated/runtime matrix: 15/15 Windows cases pass, including keyed map decoding and foreign-scope native preparations. | Completed within the documented resource-aware native profile. Additional generated Linux/installed coverage remains unclaimed; merged core 647 passes. Nine Linux standalone allocator cases already pass. |
| MG08 | Explicit owning/packed/view/recursive/inheritance matrix and early direct-managed Protobuf rejection; existing separate-value Protobuf wrappers pass eight Windows cases. Combined XML/managed/compiler-backend matrix passes all 647 core cases. | Completed support/rejection contract within the recorded Windows profile; broader generated Linux/installed coverage remains unclaimed. |
| MG09 | Guarded C++ virtual/abstract/override editor dispatch, owned/borrowed arguments and typed errors: 20 Windows generated cases pass across four identity/storage profiles. | Broader portable and native-engine method contracts. |
| MG10 | Bounded field paths, projection dependencies and immutable before/after pins: 20 Windows generated cases pass across four profiles. Compact history passes ten standalone and 24 generated Windows cases plus ten Linux kernel cases; all 24 retention/latency workloads execute. See the [measured scope](verification-compact-history-2026-10-24.md). | Generated Linux/installed replay and normalized entity-version history. Transactions still process complete roots; exported history retains complete snapshots. |
| PG01 | Immutable derived uint64 page indexes: 40 generated/runtime cases, external-run corruption checks, four process crash boundaries and three Windows file-generation tests pass. | Deferred: dirty authoritative-page mutation/writeback and incremental database recovery. |
| PG02 | Explicit eager native snapshot/index pins establish source lifetimes without hidden loading. | Deferred: lazy authoritative database values, leases, explicit I/O failure and paged selectors. |
| DB02 | Bounded eager engine, exact durable retries, prepared change details, native migration/rebaseline and immutable typed snapshots. The initial final Windows checkpoint passes all 22 selected test commands. | Deferred: declared data/index placement, integrated persistent database queries and incremental authoritative page storage. Native storage limits remain in its guide. |

See the detailed [native database](verification-native-database-2026-10-24.md),
[paged index](verification-paged-index-2026-10-24.md),
[dispatch/notifications](verification-managed-dispatch-notifications-2026-10-24.md)
and [token codec](verification-managed-token-codecs-2026-10-24.md) records for
actual commands, profiles, initial failures and qualification limits.

The initial 22-command Windows checkpoint contains 315 GTest cases: **314 pass
and one existing sidecar-only case is inapplicable**. Separate allocation-denial
and crash scripts also pass. Later source changes and generated SDK additions
require their own rebuild/replay; they are not covered merely because this
earlier checkpoint passed.

The maintained `managed_history_benchmark` executes all 24 retained-history
workloads. At 4,096 entities, measured live-history admission is 39.538 MiB by
default and 4.484 MiB with compact blocks; Save bytes are identical. This single
local run establishes no universal latency improvement. Allocation interception
excludes malloc-backed codec buffers, allocator bookkeeping and process RSS.
The compact-history record retains the complete measurements.

The later compiler/backend merge integrates `pending/compiler-backends` through
`e5a858e` in merge `dacd81e`. Managed source drafts are restored from recovery
stash `27bcf84823eee634f35e6fd4b7143c1aaa31592f`; all 58 formerly untracked
files match their preserved contents at restoration. Subsequent combined replay
passes 647 core cases, all nine language/integration targets, and the final
included-file Python regression matrix with 281 actual SDK checks. The overlapping
native build passes **22/22 CTest targets in 39.77 seconds**: 327 GTest cases,
326 completed and one existing sidecar-only case inapplicable, plus allocation,
process-crash, independent Protobuf SDK, mapped-file and OPC signature controls.
Logs and per-executable JSON are retained in `out/build/compiler-merge-native`.
