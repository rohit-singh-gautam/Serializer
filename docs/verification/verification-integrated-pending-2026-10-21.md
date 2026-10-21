# Integrated pending implementation batch

Compiler/runtime **2.21.0**, schema language **1.21.0**, 21 October 2026.

## Implemented scope and accounting

The batch assigned all **55 initially active parent requirements** concurrently:
fourteen managed/storage, sixteen XML, twenty-three package/compiler and two
root qualification requirements. `DB01` and `MIG03` remain held at the user's
request. Assignment and audit do not mean an unfinished parent is implemented.

Three checklist rows close: `OPC-OPT01` adds the reusable C++ fifth-edition
core-property adapter; `XML-B06` completes the selected eleven-SDK ordinary
native baseline; `XML-B12` reconciles an already completed authoring/evolution
contract with `OPC-R13`. The checked inventory is **793 of 847**, leaving
**54 unchecked: 52 active and two held**.

Additional implemented slices include complete-candidate native-store
authorization, inherited entity/field rules, capability requirement validation,
bounded routed PMR requests, Python QName-list schema identities and corrected
Java/C#/Kotlin/Swift namespace admission. Their broader parent gates remain open.

| Evidence | Executed scope and provenance |
| --- | --- |
| [Core properties](verification-core-properties-2026-10-21.md) | All fifteen fifth-edition properties, mixed localized keywords, normative date union, Windows provider-enabled/disabled tests and independent Windows/Linux official-schema/OPC checks. Contains the twenty-three-row audit. |
| [Managed batch](verification-managed-authorization-2026-10-21.md) | Native publication authorization, capability and allocation tests; separate Windows/Linux source and runtime provenance. Contains the fourteen-row audit. |
| [QName lists](verification-qname-lists-2026-10-21.md) | Actual 2.21.0 compiler, nineteen Python tests, forty-two native contract/import tests and CLI checks; explicit provider differences and remaining namespace/export gates. Contains the sixteen-row audit. |
| [Namespace admission](verification-namespace-budgets-2026-10-21.md) | Immutable 2.20.1 checkpoint: 8,256 exact boundary decisions in four SDKs, mobile variants and actual prepared providers across eleven selected outputs. |
| [Native baseline](verification-native-codec-baseline-2026-10-21.md) | Same immutable 2.20.1 checkpoint: 132 native operation profiles, 660 samples and exact shared bytes across eleven actual Windows SDKs. Memory categories and measurement limits remain distinct. |

## Final integrated Windows regression

The normal repository wrapper completed successfully:

```powershell
./make.ps1 test -BuildDirectory out/build/ReleaseWindows `
  -Configuration RelWithDebInfo -Jobs 4
```

The MSVC developer shell used toolset **14.51**, CMake **4.4.2** and the existing
vcpkg provider installation. XML, zlib, Zstandard, LZ4 and native managed builds
were enabled. `CTEST_PARALLEL_LEVEL=4` selected four test workers. The wrapper's
native build passed, followed by **271/271 CTests passing**, exit code **0**;
CTest wall time was **461.75 seconds**. This includes installed-package consumers,
same-major package-version compatibility, generated-code/native wire controls,
managed crash/recovery and selected SDK package/compression checks.

The final CTest log contains these actual native subtest counts; they are part
of the 271 targets, not additional CTest targets:

| Native target | GTests passed |
| --- | ---: |
| `managed_collaboration_test` | 113 |
| `managed_document_features_test` | 22 |
| `package_test` | 60 |
| `xml_scalar_contract_test` | 18 |
| `xsd_import_test` | 24 |

The final executable reports `Serializer compiler 2.21.0` and supported schema
language `1.21.0`; its generated public version header agrees. Executable SHA-256:
`25c11e5a0dc84612271df349ec054d6debc070cb42451d8cf747b437ede149d8`.

`SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES=OFF` in this integrated configuration.
The new opt-in namespace-boundary CTests therefore are not included in 271;
their actual four-SDK execution and the eleven-output provider/baseline matrices
are recorded separately above. Earlier checkpoint measurements are not relabeled
as fresh 2.21.0 executions. Linux evidence is limited to the exact slices and
toolchains in the linked records. Actual editor-host qualification and release
publication remain open; this batch changes no extension package or schema grammar.

## Earlier failures and retained artifacts

An earlier serial 2.20.1 checkpoint run passed 266/271 targets in 2,205.96 seconds.
Four Rust tests failed because that invocation selected a Cargo home without the
required offline `xmlparser` cache. JavaScript relocation encountered a Windows
`EPERM` replacement failure. The final full run selected the maintained Cargo
cache, set `CARGO_NET_OFFLINE=true`, supplied the existing Python/Kotlin tools,
and passed all five previously failing targets. No initial failure is counted
as a pass or omitted from the history.

The private core-property build's earlier Debug/RelWithDebInfo dependency ABI
mismatch and fractional-date loop failure are described in its linked record,
with the configuration/source corrections and final passing runs. Windows native
children in the final launch inherited `SetErrorMode(0x8003)` to suppress error
dialogs while retaining exit status and captured diagnostics.

Useful builds, SDK sources, official schema inputs, benchmark reports and logs
remain intentionally under ignored `out/build/pending-*-2026-10-21/` directories.
The final wrapper log is
`out/build/pending-namespace-budget-2026-10-21/integrated-2.21.log`; the earlier
failed checkpoint is retained as `native-regression.log` beside it. CTest detail
remains in `out/build/ReleaseWindows/Testing/Temporary/LastTest.log`.

Automatic approval review rejected removal of the earlier C# trial directory
`out/build/pending-namespace-budget-2026-10-21/first/`, because generated build
outputs could still be useful. It remains retained; no alternative deletion was
attempted after that rejection. Repository status and ignored artifact roots
were inspected. No commits were created and the existing staged pending-list
changes were preserved.
