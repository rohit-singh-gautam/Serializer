# Go physical ZIP qualification, 20 October 2026

[Go ZIP API](../languages/go_packages.md) ·
[Package contract](../serialization/xml_packages.md) ·
[Verification index](README.md)

This record qualifies **GO-P01 through GO-P12** for the selected standard-library
Go and Windows/Linux filesystem profiles in compiler/runtime **2.14.0**, schema language
**1.20.0**. OPC names/content types, relationships, transactional typed aggregate
loading, unknown-part policies and generated package profiles are outside this
physical ZIP record. The subsequent [Go OPC qualification](verification-go-opc-packages-2026-10-20.md)
records their separately executed scope; it does not expand the original ZIP
compiler/source capture.

## Actual compiler and SDK execution

The maintained [runner](../../test/go_package_test.py) emits actual generated Go
source using an immutable rebuilt compiler, then runs an offline, cgo-free
module with **Go 1.26.4 windows/amd64** and the **`go 1.22` language baseline**.
This is not a claim of execution with the Go 1.22 SDK. The final run exits zero
in **59.82 seconds**, including **8.068 seconds** of ordinary SDK test execution;
its `staged_runtime` result is **false**.

| Executed check | Result |
| --- | --- |
| ZIP input decisions | **66/66**: 62 independently authored or Python-standard-library controls plus 4 actual C++ producer controls |
| API/resource/lifecycle/publication subcases | **54/54**, including exact/one-tight admission, short I/O, cancellation, reentry, panic fencing and Save boundaries |
| Independent output checks | **7/7**: ZIP32/forced-ZIP64 crossed with absent/signed/unsigned descriptors, plus a real 65,536-entry writer result |
| Native interoperability | **11/11** overlapping controls: 4 C++ inputs above and all 7 Go archives consumed by the native C++ ZIP64 probe |
| Additional actual generated profiles | Collision, binary-only and XML-independent modules compile and execute |
| Unsupported OPC generation | Rejects before replacing the preexisting output sentinel |

The compiler SHA-256 is
`9dc380cc5b0ecb479e7a533ad97da8ef274cc435f44087899650e936829400fb`,
retained with deployed DLLs and `source-manifest.json` in ignored
`out/build/backlog-2026-10-20/compiler-go-package-9dc380cc/`.
The final outputs, SDK logs, native producer/consumer logs and
`verification.json` are retained in `go-package-windows-final/` under the same
task directory. Binary hashes are checked before and after the run.

The maintained Python runner SHA-256 is
`2879e4670b122ce1f7c9681ca8b31a3cadd16dff6fe2f2114ae228d3d2b7c85a`;
the [Go SDK test driver](../../test/resources/package_go_main_test.go) SHA-256 is
`14a93a000d2c2cbd4c699fa20f7ae5c697f42b08c4631ec18bcd3fd782d3ae02`.
The ordinary generated module SHA-256 is
`da69fc802ec2c33ee3f76bde06c34d2d8401e5465b732c8a4add670d21958eb0`.
The native producer and ZIP64 probe hashes are respectively
`b09ccaafdee4b7d87bfc32501b366894c41236d220af5fc83964c1bcf8cadd1c` and
`5c9cbd4aa5e449255d5b58b0051896b340875e957aa871107474e501ec54b4f5`.

## Independent framing and behavioral controls

Separate wire producers and Python `zipfile` cover ZIP32/ZIP64 directory and local
records, optional locator consistency, signed/unsigned descriptors in both
widths, CRC equal to the descriptor signature, malformed extents, overlap,
unsupported versions and compression, exact raw-DEFLATE EOF, size and CRC.
Actual 65,534/65,535/65,536-entry inputs and early/middle/late duplicate controls
exercise count and indexed-name boundaries. Segmented borrowed readers expose
logical offsets around `UINT32_MAX` while bounding actual reads to at most 1 MiB.
Classic full-range fields are accepted without a ZIP64 locator; an actual
locator independently selects and cross-checks wide records.

The independent output inspector checks exact directory/end/locator boundaries,
narrow/wide counts, local records and descriptor positions, CRCs, fixed metadata,
canonical entry ordering and absence of unaccounted gaps. Python `zipfile` also
opens and validates each archive. Reordering entry additions produces identical
bytes. The generated collision module retains four preexisting declarations and
independently selects `SerializerPackage_3` and `SerializerCompression_3`.

API controls exercise snapshotted input/policy, borrowed ownership, legal short
reads, invalid zero-progress/short-write results, exact application errors,
synchronous callback reentry and closure, shared cumulative budgets, private
spool admission/cleanup and abandoned panic state. Reproduced development
controls exposed recursive budget failure replacement and late cancellation
masking an already returned I/O-contract failure. The final implementation keeps
the first failure; later operations remain fenced and expose that cause.

Save controls validate every payload before filesystem effects. Injected
pre-replacement failures retain original bytes and remove staging. Post-replace
failures leave a complete readable new archive and report
`publication_uncertain` with the original cause. This qualifies the selected
filesystem behavior, not universal Windows atomicity: portable `os.Rename` has
no such general guarantee, and the Windows path has no portable directory-sync
step. Logical buffer/cumulative-allocation estimates do not establish a hard
Go heap, compression-workspace or process-RSS ceiling.

## Unrelated generated-output preservation

The same representative schema was generated before and after the Go-only
integration using immutable compiler hashes `7f13e0e4…` and `9dc380cc…`.
**10/10 non-Go outputs are byte-identical**: C++, Java, JavaScript, TypeScript,
C#, Rust, Python, Swift, Kotlin and C. This comparison completed before the
separate C#/JavaScript optional-provider integrations. The exact schema,
per-language output hashes and before/after files are retained in
`go-package-output-preservation/`. The schema SHA-256 is
`d2749a264b7e204e38dbb82c1d2c8b78639e79d32b1fe86cde8bbd2ea858ac4f`.

## Actual Linux compiler and SDK replay

The combined native Linux root-wrapper replay strictly rebuilds the compiler with
**GCC 15.2.0**, CMake 4.2.3 and Ninja, then executes the maintained registered
`serializer_package_go` test using **Go 1.26.0 linux/amd64**, offline and cgo-free
with the same `go 1.22` language baseline. It passes **66/66 input decisions,
54/54 API subcases, 7/7 independent output checks and 11/11 overlapping native
interchange controls**, plus all three generated profiles and unsupported-OPC
prepublication rejection. The runner takes **37.404 seconds**, CTest **37.67
seconds**. The ordinary and all three additional generated-module hashes exactly
match the Windows run; all seven output ZIP hashes also match.

The actual Linux compiler SHA-256 is
`b4dddfa3bc20be82c42faba3a2473c698ada780d4512882d2ee054a346790629`.
The native producer and ZIP64 probe hashes are respectively
`6cb40887fbbab13335f86404c7123a8f8daf22353783675f1b770e426983db06` and
`9c10d98b276aa6ff43dc5afcf4ac6fffe061fad9d6a85f9eafb5b9c254730e47`.
This is part of a **209/209** passing native CTest replay on the 2,291-file source
snapshot with aggregate
`bdb3ae5bfea8234e445e9172f1531e444383c3ae526bdc90d210704638abaf48`.
It includes the POSIX Save directory-sync path and does not imply another
filesystem's durability guarantees. Detailed output and the source/compiler
summary are retained in ignored `current-regression/linux-LastTest.log` and
`current-regression/linux-verification.json`; the immutable Linux compiler/native
tool capture resides in the workflow's `compiler-current-combined-b4dddfa3/`.

An earlier focused attempt stopped at
`Wsl/Service/CreateInstance/CreateVm/ERROR_TIMEOUT` before configuration or SDK
execution. WSL recovered without a restart before the successful combined
replay. That earlier failed attempt and the pre-Go 207-test snapshot are not
used as qualification of the Go delta.
