# Differential sidecar journal verification, 20 October 2026

Compiler/runtime 2.13.0 adds differential sidecar storage and caller-supplied
document codecs. Schema language remains 1.20.0. The separate C++23 XML fix was
committed first as `26b0332`, compiler/runtime 2.12.1; journal changes were left
uncommitted for review.

The Windows Release build used MSVC 14.51 and Ninja with the repository wrapper
for configuration, then built the affected native targets through CMake:

```powershell
./make.ps1 configure -BuildDirectory out/build/opc-implementation -Configuration Release
cmake --build out/build/opc-implementation --target managed_direct_test managed_store_test managed_collaboration_test managed_journal_crash_helper serializer xml_cpp23_test --parallel 4
ctest --test-dir out/build/opc-implementation -C Release -R '^(managed_direct_test|managed_store_test|managed_collaboration_test|managed_journal_crash_test|xml_cpp23_test|serializer_package_version)$' --output-on-failure
```

The configured vcpkg toolchain and MSVC developer environment were retained.
`TEMP` and `TMP` selected an existing ignored build-tree temporary directory for
the sandboxed test process. Test-created document directories were removed.

All six selected CTest targets passed in 37.23 seconds: 256 C++ test cases passed,
one existing sidecar-specific case was intentionally skipped in appended mode,
240 abrupt-process crash recovery checks passed, and CMake package compatibility
passed. `serializer --version` reported compiler 2.13.0 and schema language 1.20.0.
The C++23 regression used `/std:c++latest /W4 /WX` and instantiated float, double
and long double.

Executed journal cases cover:

- Sparse and shifted edits, repeated new data, independent patches beyond the old
  delta-chain limit, bounded indexes, malformed instructions and expansion limits.
- Native version-three sidecars with a large unchanged payload, controls and
  eighty further edits; each tested edit frame stayed below 1 KiB.
- Legacy version-two sidecar recovery and continued writing, followed by explicit
  Save upgrading the generation to version three.
- Existing JSON attachment without rewriting bytes, application-selected binary
  documents, stale/foreign document rejection, lossy callback failure, and discard
  preserving original formatting and allocation marks.
- Interrupted document/manifest publication, old/new generation selection and
  metadata-only Saves with identical document bytes.
- Collaboration pending work and exact lost-reply retries surviving Save/recovery.
- Separate-storage runtime fields leaving document, manifest and companion bytes
  unchanged, with transient defaults restored after recovery.
- Cumulative expanded replay limits rejecting before append I/O; lower reader
  limits preserving the live destination; full Save resetting the replay budget.

The three changed journal implementation sources also passed Linux GCC C++20
syntax checks with `-Wall -Wextra -Wpedantic -Werror` through WSL. This was not a
full Linux build or runtime test run. The bounded JSON documentation example
passed an MSVC C++20 warnings-as-errors syntax check.

Useful local logs are retained under ignored `out/build/`, including
`journal-verification-windows.log`, `journal-document-tests.log`,
`journal-runtime-transient-tests.log`, and `journal-doc-check/`.

Qualification does not include automatic XML/OPC document profile adapters,
all schema/format/provider combinations, physical power-loss testing, a complete
default CTest suite, or throughput/peak-memory benchmarks. Generic adapters
require exact application-owned encode/decode callbacks and their external-format
resource bounds. The broader [document-state bridge backlog](../pending.md#c-document-state-bridge-tasks)
retains its independent completion gates.
