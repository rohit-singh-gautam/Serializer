# Fifty-item dependency batch — 20 October 2026

[Pending requirements](../pending.md)

This batch completes **50 existing backlog IDs** for their documented Windows
owning profiles. Broad parent requirements and unexecuted platform/vocabulary
gates stay unchecked. Each selected contract has actual SDK acceptance evidence.
Compiler/runtime development remains **2.16.0**, language **1.20.0**.

| Work stream | Existing IDs | Count |
| --- | --- | ---: |
| C# packages | `CS-P01`–`CS-P19` | 19 |
| Kotlin packages | `KT-P01`–`KT-P19` | 19 |
| Kotlin XML measurements | `KT-Q05`–`KT-Q07` | 3 |
| Rust XML measurements | `RS-Q05`–`RS-Q07` | 3 |
| Swift XML measurements | `SW-Q05`–`SW-Q07` | 3 |
| C XML measurements | `C-Q05`–`C-Q07` | 3 |

The package chains include their source/sink, metadata and shared-budget
foundation; ZIP32; DEFLATE; ZIP64 entries/directories/locators; descriptors;
bounded private spooling; complete-file Save; OPC names/media/links; transactional
typed loading; automatic codec/reference budgets; opaque policies; preservation;
and canonical metadata/order. The existing `S31`/`S32` schema/profile contracts
and the two backends' `Z07`/`Z08` compression gates are prerequisites. Fixed/default/
runtime profile selection (`P20`) and declarative application references (`P21`)
remain separately tracked.

Foundation and ZIP gates were implemented/tested before OPC and typed loading.
Shared budget/diagnostic hooks integrate with each backend's generated XML
fields and identity observers. Per-field attribution through deferred presentation
and provider boundaries pass before either `P16` is checked.
Every package acceptance gate uses the actual generated backend and independent
ZIP/XML fixtures; genuine unsupported contracts remain explicit failures.

The four measurement groups reuse deterministic owning workloads, XML-disabled
native controls, independent value checks, actual SDKs, fresh-process CPU/peak
residency and explicit allocation-counter limitations. Kotlin's final measurements
follow package integration so retained source sizes describe that implementation.

Shared compiler integration/builds followed independent source freezes. Final
regression and cleanup are recorded below.
The preceding namespace/default/package/managed slice has its
[separate evidence](verification-pending-batch-2026-10-20.md), and the earlier
five-SDK measurements retain their exact compiler/source captures
[separately](verification-portable-xml-performance-2026-10-20.md).

## Executed measurements and integration

The four measurement groups passed **72 isolated operations** and **360 warmed
wall samples**, with independent XML/value checks and exact native baselines.
The [measurement record](verification-native-mobile-xml-performance-2026-10-20.md)
retains the actual SDKs, source/output hashes and memory-counter limitations.
These twelve existing IDs are checked for their selected Windows owning scope.

Both editor packages were synchronized at **2.0.9**, rebuilt through
`./make.ps1 extension` and validated locally. The shared resolver passed all
**155 units** and **43,642 freshly generated bidirectional type/field checks**,
including five-language package facades. The real Visual Studio adapter passed
66 package cursor-boundary checks and **1,461 source/output checks** in its full
corrected replay. The first full invocation supplied a nonexistent inheritance
fixture directory; the replay used the actual `generic-inheritance` output.
The generated Kotlin member check exposed a runtime-literal masking regression;
both declaration and member indexing now use the compiler's offset-preserving
runtime/schema boundary markers. Packages were built locally without installation
or publication.

Initial actual SDK runs
passed C# **227/227** and Kotlin **227/228** decisions; the Kotlin failure exposed
a deferred identity diagnostic scope, now repaired pending refreshed execution.
Independent review also found output-buffer hold rollback, callback-driven closure,
spool allocation admission, cumulative writer expansion and escaping-root targets.
Their fixes pass adversarial regression cases before affected rows are checked.

## C# package acceptance

`CS-P01` through `CS-P19` pass for the selected synchronous Windows/.NET 8
owning profile. The frozen compiler SHA-256 is
`a21ba8b885d79434c8f9bfcdf76f291514c4d3d3f2b0a5551cf87221b88a1b42`.
Actual SDK **8.0.426** is selected from a stable working directory containing
its `global.json`, and strict `net8.0` compilation verifies that selection.
The initial net8-targeted run used SDK 10; the final proof explicitly uses SDK 8.

```powershell
python -B test/csharp_package_test.py `
  --compiler out/build/pending-qname-2026-10-20/fifty-compiler-final/serializer.exe `
  --build out/build/pending-csharp-package/final `
  --cpp-example out/build/ReleaseWindows/example/opc/serializer_opc_example.exe
```

The retained `verification.json` reports **241/241 SDK decisions**, **180
independent input rows**, **10 independent output inspections** and **two C++
exchanges**. Generated source/driver hashes and actual SDK/build/run logs remain
beside it. No temporary SDK project or owned staging/spool file remains.

| Existing IDs | Executed acceptance evidence |
| --- | --- |
| P01 | Bounded random/short/seekable/snapshot sources, borrowed sinks, close/reentry/deadline callbacks and exact/one-tight shared counters. |
| P02-P05 | Independent Stored/DEFLATE output decoding and malformed directory/local offsets, CRC/size, truncated/trailing frames. |
| P06-P10 | Independent ZIP64 local extras, central sentinels, directory/locator pointers and descriptors; sparse logical offsets below/at/above 4 GiB with under 1 MiB actual I/O; real 65,534/65,535/65,536 counts and duplicate positions; signed/unsigned 32/64-bit descriptors. |
| P11-P12 | Explicit bounded private spooling, cancellation, exclusivity and cleanup; staged Save fault boundaries preserve complete old/new files, validator atomicity and original errors even when cleanup fails. |
| P13-P15 | Names/aliases/ancestry/MIME, secure metadata encodings/MCE, scoped/dangling/escaping targets, three-part middle-failure atomicity and native exchange. |
| P16 | Shared physical/XML/scalar/namespace/reference work and allocation; deferred attribute/identity owner locations, part/member/link exhaustion, 15 cumulative OPC/XSD references, original caller/provider errors, released holds and callback-driven closure. |
| P17-P19 | Optional/required reject/skip/preserve policies, opaque bytes and Unicode external IRIs, fixed-role source/target relocation with selectors, typed QName child identities, edited-reference checks and canonical permutation equality. |

The [public API guide](../languages/csharp_kotlin_packages.md) documents logical
admission versus host heap allocation, platform array limits, selected ASCII OPC
part URIs, supported XML graph boundaries and host filesystem guarantees. Runtime
profile dispatch (`P20`), declarative application references (`P21`) and wider
vocabulary/platform qualification remain open.

## Kotlin package acceptance

`KT-P01` through `KT-P19` pass on actual Kotlin **2.2.21** and Microsoft JDK
**17.0.18**, targeting JVM 17 with warnings as errors. Final compiler SHA-256:
`78c3210e00ec48d9ee265e323d4498b8e38f3b18712cac5cd5fda5bac7c55f6c`.

```powershell
python -B test/kotlin_package_test.py `
  --compiler out/build/pending-qname-2026-10-20/fifty-compiler-qualified/serializer.exe `
  --build out/kotlin-package-qualified `
  --kotlinc out/tools/kotlin-2.2.21/kotlinc/bin/kotlinc.bat `
  --java "C:/Program Files/Microsoft/jdk-17.0.18.8-hotspot/bin/java.exe" `
  --cpp-example out/build/ReleaseWindows/example/opc/serializer_opc_example.exe
```

The retained report records **241/241 SDK cases**, **180 independent input rows**,
**11 independently inspected outputs** and **two actual C++ exchanges**. All
P01-P19 gate groups in the table above have Kotlin counterparts. Specific checks
include sparse logical offsets below/at/above `0xffffffff`, real 65K input counts
and 65,536-entry output, all descriptor forms, seven Save fault boundaries,
short-source scratch admission, private-spool ownership/cleanup, middle-part and
validator atomicity, exact/one-tight shared limits, fifteen OPC/XSD references,
deferred member/provider/identity locations and retained QName scope copies.
Windows spooling uses inherited directory ACLs: callers supply access-restricted
directories. POSIX owner-only attributes are requested where supported; no
universal Windows owner-only file ACL claim is made.

Nine model-name controls exercise native byte roundtrips, including `Throwable`,
`System`, `Nothing`, `CharArray` and `Operation`; `Operation` also passes XML.
Runtime platform types are qualified without renaming schema models. The new
XML operation hook nests under the already reserved internal `SrlXmlBudget`;
the existing `Srl` prefix rejection retains its no-output behavior.

Eight emitted ZIP/OPC envelopes additionally pass an independent APPNOTE directory,
ZIP64 extra/locator and descriptor inspector, with four wrong-size mutations
rejected. C# retains its zero-placeholder writer policy; Kotlin's inspector also
admits known local ZIP64 sizes only when they match the central directory and
descriptor. This separates backend canonical policy from structural inspection;
see [PKWARE APPNOTE 6.3.10, sections 4.4.8-9 and 4.5.3](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT).
The successful SDK compilation/execution was reused for this test-oracle-only
inspection update; the final report retains the actual JAR/source hashes and
fresh native exchanges. No writer change was needed.

The final compiler also passes **10/10** actual Kotlin namespace probes through
`test/mobile_xml_namespace_test.py --language kotlin`, including independent
expanded QName identity, Unicode names, shadowing/default absence and native
byte invariance. `out/kotlin-namespace-qualified.log` retains the result.

## Integrated regression and final scope

`./make.ps1 test` completed the full MSVC RelWithDebInfo native build and **217
registered tests**: **215 passed**, with two Kotlin failures. The compression
builtin-name fixture found a real unqualified `Throwable` regression; the package
test had captured the earlier sparse-allocation/provider-bound fixtures during
their correction. Both are resolved and explicitly replayed on the final
compiler: registered Kotlin compression **1/1** (114.44 s), and the complete
Kotlin package command above **241/241**, including the strengthened collision,
archive-inspection and native-exchange controls. This is full-suite accounting
with two successful replacements, rather than a claim of one failure-free
217-test invocation. The full run retained package-version discovery, installed
packages, ordinary C# compression/XML and existing native/managed regressions.

Logs remain under `out/build/pending-qname-2026-10-20/`, including
`fifty-integrated-final.log`, `fifty-kotlin-compression-replay.log` and the frozen
compiler builds. Final C#/Kotlin SDK proofs and measured-source equivalence checks
retain their exact compiler snapshots. The later Kotlin repair leaves the two
measured C# sources and six C/Rust/Swift sources byte-identical; Kotlin's complete
18-operation measurement is refreshed against the final compiler.

All **50 selected existing IDs** are checked with evidence in `docs/pending.md`.
No broader OPC parent, P20/P21, untested platform or new vocabulary gate is closed
by these results.

## Final staged hygiene capture

The staged diff check exposed trailing blank-line spaces in the four new C#
runtime templates and their test driver, which had been untracked during earlier
diff checks. Removing only end-of-line whitespace is independently confirmed by
`git diff --ignore-space-at-eol --exit-code`. The clean compiler is
`out/build/pending-qname-2026-10-20/fifty-compiler-clean/serializer.exe`, SHA-256
`ef49b32f4a37abb493b877275eaf263960e761de9c91b981ee603cfdfae6b6a1`.

The complete C# suite is replayed against this compiler: actual SDK **8.0.426**,
**241/241** cases, **180** independent inputs, **10** independent outputs and
**two C++ exchanges**, in **11.772 s**. Evidence is retained separately in
`out/build/pending-csharp-package/clean/verification.json`; earlier captures
remain intact. `native-mobile-clean-source-check/verification.json` confirms
all eight C/Rust/Kotlin/Swift sources remain byte-identical to their selected
verified captures. C# generated-byte counts and measurements receive their own
clean refresh in the [portable measurement record](verification-portable-xml-performance-2026-10-20.md).
The global runtime-literal guard and staged whitespace check pass.

## Repository hygiene

Reviewed `.gitignore`; existing build/editor/bytecode patterns cover this work.
Removed obsolete measurement probes (719 files, about 181 MiB), three older C#
scratch trees, 24 superseded publication-test trees, task bytecode and 121 unused
temporary files/directories. Retained final SDK/generated-source/archive evidence,
measurement audits, current build outputs, editor packages and reusable tool caches.
The newest publication diagnostic tree for each test is intentionally retained.

Windows denied access to seven older scratch directories: six publication-test
trees and the task's `temp/hsperfdata_rohit`. They remain preserved; their exact
paths/errors are recorded in `publication-scratch-cleanup.json` and
`final-scratch-cleanup.json` under the task build directory.
