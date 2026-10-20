# Managed reference failure and recovery qualification, 21 October 2026

[Backlog](../pending.md) · [Managed interface](../managed/cpp_runtime.md) ·
[Previous package batch](verification-pending-105-2026-10-21.md)

Compiler/runtime **2.17.1**, schema language **1.21.0**. This batch selects
`MR01` and `MR02` and reconciles the package API parity parents `OPC-R07` and
`XML-B17` against the completed eleven-output package evidence. The parent
reconciliation adds no new package graph, provider or platform claim.

## Initial collaboration join

A fresh client previously checked the authority's incoming typed references
against its placeholder document namespace. The first synchronization therefore
rejected an otherwise valid graph before adopting the authority baseline.
Replacement validation now uses the staged authority namespace while retaining
the original client's application policy and reentrancy guard. Publication still
follows complete validation and resource/durable admission.

The focused direct32 regression passes a valid cyclic graph, rejects a foreign
typed token before policy, rejects the original caller policy without replacing
the placeholder root/document/history, then retries successfully. The original
failing isolated probe and final strict MSVC probe are retained. Native IDs,
model APIs, generated names, wire values, journal framing and client checkpoint
formats are unchanged.

## Selected native matrix

The existing eight profiles combine direct/separate storage, fixed/compact IDs,
and uint32/uint64 IDs. Existing corruption, import mapping and lifecycle checks
remain part of the same generated executables. New suites exercise:

- Preview root/control-block allocation, reservation metadata growth, snapshot,
  resident, collection and history admission; exact and one-tight limits;
  exposed-token nonreuse; original caller error state and acceptance retry.
- Nested editable reordering, retirement, cross-parent/field identity rejection
  and supported explicit moves, with typed edges, pinned snapshots, Undo/Redo
  and Save/load.
- Typed optional/forward/cyclic graph reservations, rejected imports, deletion,
  navigation and full-Save failure decisions in appended and sidecar journals;
  invalid dangling deletion rejects before journal I/O.
- Actual subprocess termination and fresh-process recovery, independently
  checked journal framing, incomplete tails, reservation versus model-publication
  records and complete old/new generations.
- Leased concurrent creation, conflict/burn, exact retries, epoch/reconnect
  rules, malformed typed payloads, receiver allocation failures, lost replies,
  checkpoint recovery, notifications and collaborative Undo/Redo.

## Windows results

MSVC 19.51, C++20, RelWithDebInfo, `/W4 /WX` builds all native targets. The
eight-profile checks pass with zero skipped or disabled cases:

| Suite | Tests per profile | Profile executions |
| --- | ---: | ---: |
| Existing reference/identified baseline | 23 | 184 |
| Preview resource and acceptance | 10 | 80 |
| Nested editable ownership | 5 | 40 |
| Typed-reference journal faults | 6 | 48 |
| Typed-reference collaboration | 5 | 40 |
| **Total** | | **392** |

The journal suites cover **520** mode/boundary scenarios. The separate driver
passes **696** crash scenarios, **1,896** semantic/framing checks and **3,768**
fresh helper processes. It distinguishes reservation records from subsequent
model publication, validates existing CRC64/footer/chain framing independently,
and checks complete recovered graph/history decisions. Abrupt process termination
does not qualify power-loss or storage-hardware durability.

The broader managed/core regression accounts for **48 CTest cases**. Its first
invocation passes 42; six cases encounter access denials while canonicalizing
the execution environment's system-temporary paths. All six pass with `TEMP`
and `TMP` set to the verified task-local build directory. The original 34.38-second
invocation and 9.38-second replay are retained. The 184 baseline reference tests
above are executed separately from each new-suite filter.

Compiler/runtime **2.17.1** and schema **1.21.0** version checks pass. The four
version/package checks are accounted for: three pass initially; the installed
consumer requires the MSVC developer environment and passes after it is loaded.
Its first invocation lacked the standard-library include environment. This is
recorded as a configuration failure rather than a successful first invocation.

Evidence beneath ignored `out/build/pending-managed-2026-10-21/` includes
`preview-ownership-windows/verification.json`,
`final-reference-windows/verification.json`,
`collaboration-matrix-windows/verification.json`,
`existing-reference-windows/verification.json`,
`broader-managed-windows-test.log`, `broader-managed-windows-temp-replay.log`
and `installed-package-devshell.log`. XML/log pairs, exact commands, source,
schema/config, generated-header and executable hashes identify the tested images.

## Package parent reconciliation

The [105-item batch](verification-pending-105-2026-10-21.md) implements the API
categories named by `OPC-R07` and `XML-B17` in all eleven outputs: ZIP/OPC
source/sink, metadata, reader/writer, shared operation limits, whole-part/link
preservation and staged full-file Save, with independent producers and consumers.
C/Rust private-memory spooling is a bounded source strategy. Their recursive
document-companion/XSD-identity generation gates are explicit typed-graph limits;
the ordinary package categories remain implemented and tested.

These two parent closures do not close caller-selected shared-context codec
composition (`XML-B20`), broader OPC/MCE/UTF-16 conformance, sparse large sources,
canonical-path retargeting in every backend, provider-internal peak/preemption
bounds or universal filesystem durability. The preexisting distinct C XML-header
coexistence limitation remains documented separately.

## Linux results

GCC 15.2.0, C++20, Release and `-Wall -Wextra -Wpedantic -Werror` build all
native targets under WSL2. The root `make all` build was interrupted deliberately
after verifying its owned Ninja process, then resumed against the same unchanged
cache with eight jobs; the remaining 220 actions complete successfully. LibXML2
2.15.2 and zlib 1.3.1 are enabled. Zstd/LZ4 development headers are unavailable
in this Linux configuration, so no new qualification of those providers is made.

The same **392 GTests**, **520** journal scenarios, **696** crash scenarios,
**1,896** crash semantic/framing checks and **3,768** fresh helper processes pass
on Linux. The new profile matrix has zero skips or disabled cases. Crash fixtures
use private POSIX temporary directories; final reports are copied into the build
tree and the owned temporary directories are removed. Generated reference
headers match the Windows headers byte for byte.

The broader 48-case managed/core invocation passes 47 cases on the Windows-mounted
build filesystem. `managed_direct_test` encounters seven existing appended-journal
discard failures there. Replaying its unchanged ELF in a private POSIX directory
passes 111 tests, with the existing one mode-inapplicable sidecar test skipped.
The first invocation and the full-executable replay remain separate evidence;
this does not qualify appended-journal discard on the Windows-mounted filesystem.

Linux evidence includes `preview-ownership-linux/verification-combined.json`
(80 preview and 40 ownership tests, without rerunning the earlier preview scope),
`final-reference-linux/verification.json`,
`collaboration-matrix-linux/verification.json`,
`existing-reference-linux/verification.json`, `broader-managed-linux-test.log`
and `managed-direct-linux-posix.{json,log,xml}` beneath the same ignored batch
directory. `integrated-verification.json` records report/source hashes and
checks all selected counts against the final files.

## Repository hygiene

All generated headers, binaries, verification adapters, logs and hash manifests
remain beneath ignored build directories as useful verification evidence.
`.gitignore` already covers these paths. Test-owned transient directories are
removed; the Windows system-temporary failure paths were checked and none
remained. No schema language, editor package, public identity or wire revision
changes in this patch. Marketplace publication and installation are separate.
