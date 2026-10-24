# JavaScript and TypeScript managed SDK verification, 24 October 2026

Compiler/runtime **2.34.0**, language **1.24.0**, Windows x64. Actual SDKs:
Node **22.23.1** and TypeScript **7.0.2**. Compiler/core: MSVC 19.51, C++23
Release with formatter enabled. All compiler and SDK processes run serially.

## Executed scope

- **652/652 compiler/core cases pass**, with zero skipped cases. Four new
  generator controls and one option control cover explicit selection, ordinary
  boundaries, deterministic metadata, exact token instances, synthetic ID
  widths/compact encoding, nominal declarations and unsupported profiles.
- The independent production kernel passes **204 Windows checks**, **128
  Node-free VM memory checks**, six externally killed process-crash boundaries,
  and cross-process canonical-path/hard-link writer exclusion. The rejected
  contender does not change journal bytes; OS leases release after process death.
- Actual generated engines pass **384 runtime checks across eight profiles**:
  uint32/uint64 IDs, fixed/compact encoding and `all`/`binary_none` codec
  selection. `all` uses integer-key canonical snapshots. Tests cover owning
  clones, immutable read copies, managed/ordinary ancestry, nested containers,
  canonical map order, typed references, native scalar/digest fields, callback
  lifetime/reentrancy, burned IDs, Undo/Redo, exact authority/client receipts,
  native journal reopening and rejected invalid transitions.
- **Eight actual TypeScript SDK programs execute** against the paired ESM
  modules. **96 strict negative contract cases reject** in eight serial TypeScript
  compilations, including async callbacks,
  wrong native ID types, mutable snapshot metadata, structurally forged opaque
  handles and three distinct named-target checks for identical field layouts.
  Private declaration brands preserve named identity independently of field shape.
- Actual generated modules pass **14 Node-free VM checks in two profiles** and
  **two externally killed generated-writer recovery cases**. These VM checks
  forbid Node imports; they are not installed-browser execution.
- Token-only JavaScript/TypeScript passes **5,309 checks**, four independent C++
  exchange profiles and **64 actual exchanged messages**. All four native codecs
  preserve existing C++ envelope bytes and validate raw metadata before coercion.
  Token-only output contains no state engine.
- All **10 existing Java/Python/default-JavaScript controls pass in 193.88
  seconds**, including their actual generated SDKs, token exchanges, Java sinks
  and buffer windows, and ordinary JavaScript codec controls.

The TypeScript output is a strict declaration SDK for the matching generated
JavaScript ESM implementation. Both execute the native engine without a C++
service bridge. Generated profiles and unsupported combinations are described
in the [usage guide](../managed/javascript_native_runtime.md).

## Evidence and corrections

Logs remain under `out/build`: `javascript-managed-kernel-async-final.log`,
`javascript-managed-core-final.json/.log`, `javascript-managed-sdk-engine-rerun.log`,
`javascript-managed-sdk-first-run.log` and `javascript-managed-existing-controls.log`. Generated
profiles and C++ exchange executables remain in the selected CMake test build
directories. Initial failures are retained, including the cross-realm rejected
Promise regression and the first SDK diagnostic-harness mismatch.

The kernel now handles rejected promises from another realm through the
intrinsic Promise brand. Generator controls use the currently unsupported Go
backend for their original unsupported-output negatives. The TypeScript INI
section is admitted explicitly. The wrong-root TypeScript rejection expects
its actual `TS2740` diagnostic; it does not accept arbitrary compiler failures.
The final kernel also proves that proven reads remain available while an async
journal lease is acquired: caught mutation/close attempts poison acquisition,
write no frames, release leases and leave the in-memory engine usable.

## Limits

Native journals require Windows Node, a local volume and cooperating same-host
writers. UNC/device paths reject before access; drive-letter syntax alone does
not detect a mapped network volume. No distributed writer exclusion, Linux/macOS
native journal, device power-loss or directory-publication proof is claimed.
Other-host runner branches preserve explicit unsupported-journal controls but
have not been executed in this checkpoint.

The engine has finite concrete public owning records, callback edits, bounded
linear history, full-state native journals and full-state authority/client
proposals. Tree/labeled history, contribution-aware collaborative Undo/Redo,
inheritance/generic/behavior/cache/index/variant profiles, native migration,
rebaseline and compaction are not implemented here. Byte/count admission does
not establish a process heap/RSS ceiling. C++ Save/journal interoperability and
mixed-language collaboration are not claimed by token exchange.

MG01 remains open for Go, C#, Rust, Swift, Kotlin and C native engines and the
broader capability contracts. MR03 retains its remaining codec/selection work.
