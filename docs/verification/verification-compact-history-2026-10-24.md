# Compact managed history: 24 October 2026

Compiler/runtime 2.34.0, schema language 1.24.0; Windows x64 Release with MSVC
19.51.36260.0 as recorded by CMake. The combined XML/managed native build uses
one compiler worker. This evidence qualifies the explicit
[`compact_history` runtime policy](../managed/compact_history.md), not normalized
entity-version storage or changed-entity-proportional transaction CPU.

## Executed checks

- Ten standalone block/table cases pass: exact materialization, byte/chunk bounds,
  resource-domain isolation, pruning, replacement/deletion, every selected PMR
  allocation failure, resident-admission rollback and cross-thread final release.
- Twenty-four generated cases pass: six each under direct32, direct64, separate32
  and separate64. They cover exact default/compact Save bytes, Undo/Redo/load,
  nested ownership and ID restoration/nonreuse, tree branch merging, runtime fields,
  pruning/pins, allocation rollback and post-flush allocation denial/recovery.
- All 16 selected native CTest commands pass in 30.94 seconds: 935 GTest cases,
  with 934 completed and one existing sidecar-only case inapplicable. The selection
  includes core 639/639, payload allocation 15/15, migration 22/22, managed stores,
  file generations, XML import, document profiles and restricted HTML controls.
- Linux Ubuntu GCC 15.2.0 passes the standalone snapshot ten, payload allocator
  nine and file-generation three cases: three CTest commands, 22 cases, 1.26
  seconds. Root `make configure` and single-worker CMake builds use the existing
  provider prefix. Its library directory must also be in `LIBRARY_PATH` and
  `LD_LIBRARY_PATH` for the extracted xxHash/LZ4 dependencies. Strict member-init
  warnings in the paged-index source are corrected before this passing run.

The first builds exposed an obsolete history-entry size assertion and a strict
MSVC unreachable-code warning in the labeled tree comparison. Both are fixed
before the passing run. Initial independent language replay separately reproduced
three Python root-identity failures; its subsequent fix and SDK replay have their
own logs and do not change this C++ history result.

## Retention and latency measurements

`managed_history_benchmark` completes all 24 workloads: 64/512/4,096 entities,
256-byte text payloads, 32 committed edits, default/compact retention, and four
live/deleted/pruned/pinned scenarios. It checks deletion Undo identity and
immutable pins. The table shows live retained history before deletion:

| Entities | Resident admission MiB, default / compact | Live C++ new requests MiB, default / compact | Median edit microseconds, default / compact | Save bytes, both modes |
| --- | --- | --- | --- | --- |
| 64 | 0.634 / 0.224 | 0.618 / 0.203 | 74.4 / 49.4 | 614,892 |
| 512 | 4.955 / 0.698 | 4.926 / 0.662 | 507.2 / 546.6 | 4,923,440 |
| 4,096 | 39.538 / 4.484 | 39.399 / 4.330 | 6,399.1 / 4,462.1 | 39,408,688 |

For 4,096 entities, deleting one entity retains 40.643/5.632 MiB of admission
in default/compact mode. Resetting history reduces those values to 4.148/4.192
MiB. A retained pre-deletion root adds its independently owned model in both
modes; releasing history never invalidates that pin. Compact tables have their
own overhead, including when little history remains.

Interception counts requested C++ `new` bytes. It excludes malloc-backed codec
streams, allocator metadata/retention and process RSS. The live-history
4,096-entity peak is 114.567/79.498 MiB; exporting complete Save envelopes still
materializes substantial temporary memory. Timings are one local run under a
shared machine workload, with median/p95 across 32 edits. They establish no
universal speedup; the 512-entity compact median is slower in this run.

## Evidence and remaining scope

Retained evidence: `out/build/managed-xml-integration/native.log`, per-executable
GTest JSON, `history-windows.csv`, `hashes.log` and `version.log`. The generator
capture is SHA-256
`686902ab47cca5552b73797a4e2f8a7f12fb54f4a8d63d472a65e1ccd6af3a38`.
Later Python-only runtime fixes have a separate generated-SDK replay. Build the
named tests/benchmark through CMake after root-wrapper configuration and run
CTest with `--parallel 1`; use ordinary native filesystem semantics.

Fixed positional blocks can lose sharing after length-changing insertions.
Exported history remains complete snapshots; transactions still decode, encode
and validate complete candidates. Generated Linux stores/installed consumers, normalized entity
records, lazy database authority and other native engines are separate gates.
