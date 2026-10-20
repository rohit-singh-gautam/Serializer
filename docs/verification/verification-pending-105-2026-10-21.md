# Package and qualification dependency batch — 21 October 2026

[Pending requirements](../pending.md) · [Application contracts](../serialization/package_application_contracts.md)

This batch selects the **105 bounded work units** that were unchecked after
`a512499`. Compiler/runtime **2.17.0** and schema language **1.21.0** preserve
previous declarations, model APIs, native wire identities and managed storage.
Both editor packages are **2.0.10**. Broad roadmap requirements remain separately
tracked; an owning-profile result does not qualify every XML graph or format.

| Work stream | Existing IDs | Count |
| --- | --- | ---: |
| C++ and Python packages | `CP-P16/P20/P21`, `PY-P16/P20/P21` | 6 |
| Java, Go, JavaScript and TypeScript packages | `P16/P20/P21` in each backend | 12 |
| C# and Kotlin packages | `CS-P20/P21`, `KT-P20/P21` | 4 |
| Rust, Swift and C packages | `RS-P01`–`RS-P21`, `SW-P01`–`SW-P21`, `C-P01`–`C-P21` | 63 |
| Malformed/resource/provider qualification | `Q02` in all eleven outputs | 11 |
| Platform and reproducibility qualification | `Q08` in Java/Kotlin/C#/Go/JS/TS/Rust/Swift/C | 9 |
| **Total** | | **105** |

The package chains depend on the checked `S31`/`S32` schema/profile contracts
and their backends' `Z07`/`Z08` compression contracts. Source/sink and physical
ZIP gates precede OPC metadata, transactional typed loading, preservation,
profile selection and generated references. The implementation uses each
backend's actual model and XML codecs; SDK tests compile freshly generated source.

## Profiles and application references

The shared application schema is
[`package_application_contract.serializer`](../../test/resources/package_application_contract.serializer).
It declares repeated target/source roles, a distinct optional target model,
fixed/default storage policies and scoped scalar selectors. Independent fixtures
exercise forward/repeated references, duplicate/missing/wrong-model keys, wrong
roots/types, equivalent namespace prefixes and conflicting physical storage.
Independent Python ZIP inspection checks emitted stored/DEFLATE methods and CRCs.

One scope has one exact primitive selector type. Model identity, rather than
role identity, determines a reference target. Declaration validation rejects
unsupported selectors before publishing compiler output. Selectors create no
payload fields or wire IDs. Complete-candidate checks precede domain callbacks
and staged Save replacement. Runtime profiles select ZIP storage while keeping
the declared model, XML root/type, content type and container immutable.

Maintained acceptance runners are `package_owning_contract_test.py` for
Python/C#/Kotlin and `package_application_contract_test.py` plus
`package_shared_codec_budget_test.py` for Java/Go/JavaScript/strict TypeScript.
The native `package_application_test` verifies C++ generated facades. Actual
SDK suites cover their supported graphs; arbitrary codec replacement remains
unsupported. Additive Go operation-policy APIs preserve existing positional
`XmlOptions` literals. Collision-selected new helper/property names preserve
existing schema names.

## Shared limits and diagnostics

Package, XML, compression, keys/references and preservation share cumulative
operation admission. C++/Python retain deferred field ownership through retained
trees, prepared attributes and provider boundaries. Package-created local XML
limit errors carry part/member/relationship locations; caller exceptions retain
their original identity. Existing ordinary XML APIs keep their failure contract.
Tests exercise exact and one-tight admission, cancellation, complete-candidate
failure and preservation of the previous final file.

Admission counters describe logical bytes/work, not hard process-memory peaks.
Cooperative cancellation does not preempt provider internals. Borrowed sinks can
retain a prefix after transport failure; full-file Save validates a staged
candidate before replacement. Filesystem durability remains host-specific.

## New package backends

The [Rust](../languages/rust_packages.md), [Swift](../languages/swift_packages.md)
and [C](../languages/c_packages.md) guides describe ownership, providers, sources,
budgets, preservation and publication. Their maintained SDK runners use
independently authored ZIP/OPC archives and independent output inspection.
C additionally injects generated allocation failures and runs AddressSanitizer.
Stored-only C output is compiled without the zlib feature macro.

C/Rust typed package graphs requiring XML document companions for QName/prefix
scope or XML extension slots reject explicitly, including nested/inherited uses.
Their ordinary native/XML document APIs retain existing support.
XSD identity constraints also have an explicit C/Rust package generation gate
until their native observers share the package operation; ordinary XML identity
and package application selectors retain their existing independent contracts.
Owning-memory sources do not imply sparse multi-gigabyte random access. Broader official
OPC/3MF corpora, markup compatibility, signatures, arbitrary graphs and hard
allocator/provider-preemption guarantees remain open.

## Qualification and final verification

The [provider/platform record](verification-q02-q08-2026-10-21.md) records actual
SDKs, enabled providers, malformed/resource/cancellation corpora, source hashes,
Windows/Linux reproducibility and cross-host exchanges. It distinguishes
executed platform profiles from untested claims.

The final Windows compiler capture has SHA-256
`20db0258097c1b5676720186c2b6d7ed2b19d3e8dc9d5a70683f7909aa82258d`.
It reports compiler/runtime **2.17.0** and language **1.21.0**. Actual SDK
acceptance includes the following independently executed checks; decision counts
describe each runner's assertions and are not interchangeable with CTest cases.

| Profile | Final acceptance |
| --- | --- |
| C++ | Core: 609 passed, one optional formatter check skipped; 13 OPC-profile and five application-contract GTests passed. |
| Python | 39 package tests and 24 application decisions passed on Windows and Linux; application fixtures include 15 independent inputs and two independently inspected outputs. |
| Java | 57 application decisions and 12 shared-codec decisions passed. |
| Go | 55 application decisions and 12 shared-codec decisions passed, including legacy positional options and additive operation-policy APIs. |
| JavaScript / strict TypeScript | 64 application decisions and 12 shared-codec decisions passed using the actual generated runtime and strict declaration checks. |
| C# | 29 application decisions passed, including four local XML quotas, original caller exception identity and legacy role APIs. |
| Kotlin | 30 application decisions passed with warnings as errors, including old two-argument operation calls and nine-field role constructor/copy compatibility. |
| C | 64 independent ZIP/OPC fixtures and 16 application fixtures passed under C11 and AddressSanitizer; all 66 encode and 84 load allocation-failure positions passed. |
| Rust | 64 independent ZIP/OPC fixtures and 16 application fixtures passed with the actual Rust SDK. |
| Swift | Three optimized, warnings-as-errors SDK builds passed: 32 ZIP and 14 malformed OPC fixtures, 15 application inputs, eight inspected outputs, helper/role collisions and exact shared XSD-identity admission. |

The Java/Go/JS application fixtures each have 15 independent inputs and two
independently inspected outputs; each shared-codec fixture has two inputs and
one inspected output. The shared application ledger admits exactly six
references. Where native XSD identity composition is supported, the selected
identity fixture admits exactly fifteen physical/key/unique/keyref references.
Equal requests count before deduplication. Exact and one-tight limits retain the
actual loaded part, model field and relationship ID.

The original Kotlin two-argument operation call sites also pass the unchanged
baseline: 241 checks, 180 corpus decisions, eleven independently inspected
outputs and two native C++ exchanges. Schema extension occurrence failures are tested
separately from configured item quotas; the earlier compiler's failing negative
control is retained alongside the final Java/Go/JS passes. Matching and
equivalent-prefix root `xsi:type` fixtures exercise the private C/Rust package
decode bridges while ordinary XML entry points retain their behavior.

Final generation proves all three Swift package SDK inputs, C#'s application
input and the C++/Python
application, profile and identity fixtures byte-identical to the source used by
their passing SDK/native tests. These comparisons cover complete source bytes,
not selected API names. Generated sources, exact commands, compiler/source
hashes and `verification.json` reports are retained under ignored
`out/build/pending-105-2026-10-20/`, `out/build/pending-c-package/` and
`out/build/pending-rust-package/`.

The actual Linux GCC 15.2/CMake 4.2 build passed **36/36** selected native,
XML/provider, ZIP/OPC and generated application CTest cases. It uses LibXml2
2.15.2 and zlib 1.3.1; Zstandard and LZ4 development providers are absent in this
native Linux configuration. Python's private-file tests run in a POSIX temporary
directory, because the Windows filesystem mount does not implement the tested
permission boundary. The 39 package tests and 24 application decisions passed;
reports and generated application source were archived before removing that
task-owned temporary directory.

A separate final Windows build with XML, zlib, Zstandard and LZ4 disabled passes
the compiler version check, all four native-format Python round trips including
the maximum uint64 identity, and explicit transactional rejection of XML output.
This provider-free check does not replace the enabled-provider qualification.

Both editor VSIX packages are built and verified at **2.0.10**. Verification
includes 158 unit checks, 546 packaged grammar checks, 45,292 generated navigation
checks across eleven languages and 153 Visual Studio interpreter navigation
checks. Packages were not installed or published as part of this batch.

The Windows wrapper builds the complete enabled configuration and executes
**230 CTest cases**. Its original complete invocation passed 222 cases in
1,243.34 seconds. Eight cases exposed the subsequently corrected Kotlin
two-argument call compatibility, extension occurrence/quota classification,
JavaScript parser dependency setup and matching C/Rust root-type bridges.
After rebuilding the main compiler, **all eight affected cases passed** in
104.11 seconds. All 230 cases are therefore accounted for; this is not a claim
of one failure-free full invocation. The retained logs are
`full-windows-ninth-test.log`, `main-final-compiler-build.log` and
`full-windows-affected-final.log`. The final main-build compiler SHA-256 is
`44033dddddd2632385efb7e11263cffa74bd7b14e267e572d37a31d44e3bcb3a`;
the separate immutable SDK capture above was built from the same final sources.
Package-version compatibility and installed compiler/runtime package tests pass
in the complete invocation.

The raw runtime-literal size guard and Git whitespace checks pass. `.gitignore`
already covers the generated SDKs, local tool distributions, editor packages,
build trees and caches; no new ignore rule is needed. The unused dependency-cache
directories from the first provider-free configuration attempt and the explicit
diagnostic bytecode file were removed after checking their workspace boundaries.
Twelve unused publication-test probe directories from this batch's interrupted
and complete runs were removed; older artifacts were preserved.
Useful generated sources, SDK executables, packages, exact command logs and
verification reports remain intentionally under ignored build/tool directories.
The temporary POSIX Python test directory was removed after archiving evidence.
No release, Marketplace listing, package-port update or remote Git push is
included in this batch.
