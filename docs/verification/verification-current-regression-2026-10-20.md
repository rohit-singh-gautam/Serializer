# Current-source regression qualification — 2026-10-20

**G03–G08 pass for the selected profiles below.** The final Serializer 2.14.0
replay passes **213/213 Windows** and **210/210 native Linux** CTests, plus
**13/13 Windows** and **12/12 Linux** provider-free CTests. The final snapshot
and intervening failures are recorded first; the earlier combined-source record
is retained below as historical evidence.

## Final 2.14.0 replay after compression and editor repairs

The final archive contains **2,359** nonignored source files. Its Windows
aggregate SHA-256 is
`3a09ff1ef5794fcb02d81d5568a6ba7e40db665d801278bdec404600cd6f9fc2`;
the native Linux aggregate, with only `setup.sh` line-ending/mode normalization,
is `163c229a705fdee1d7a4bd84259c70c2a326c9e9fce1dad69f4aba9073cba2aa`.
Every source file has an ordered path/hash inventory in
`out/build/backlog-2026-10-20/current-regression-zstd-final/`.
The same host/tool/provider profiles described below apply. The root wrappers
build all configured native targets before running the complete CTest inventory;
the isolated overlay preserves timestamps for identical bytes. No active
worktree cleanup or WSL shutdown was used.

| Final gate/profile | Actual result | Retained evidence under `current-regression-zstd-final/` |
| --- | --- | --- |
| G03 Windows full wrapper | **213/213 passed**, 1,399.63 s CTest | `windows-full.log`, `windows-LastTest.log`, `windows-final-artifacts.json` |
| G04 native Linux full wrapper | **210/210 passed**, 473.04 s CTest; 576.31 s wrapper | `linux-full.log`, `linux-LastTest.log`, `linux-verification.json` |
| G05/G06 Windows provider-free | **13/13 passed**, 393.50 s CTest | `windows-minimal.log`, `windows-minimal-LastTest.log` |
| G05/G06 native Linux provider-free | **12/12 passed**, 118.81 s CTest; 197.89 s wrapper | `linux-minimal.log`, `linux-minimal-LastTest.log`, `linux-minimal-verification.json` |

All four native optional-provider flags are disabled, with the corresponding
`CMAKE_DISABLE_FIND_PACKAGE_*` guards, in both provider-free caches. Exact CMake
caches are retained beside the logs. Windows has four additional SDK CTests:
`serializer_compression_csharp`, `serializer_compression_java`,
`serializer_compression_javascript` and `serializer_package_java`. Native Linux
adds `stream_allocation_failure_test`, explaining the 213/210 difference.
The pre-existing opt-in formatter unit case remains skipped unless its formatting
option is enabled; the CTest totals do not imply that every underlying unit case
or every optional SDK deployment was executed.

The final G07 version/installed-consumer/component tests all pass on both hosts,
including the actual compiler-only relocated package and ABI/provider isolation
checks described below. Compiler/schema versions remain **2.14.0 / 1.20.0**.
The full Windows compiler SHA-256 is
`978a9afe1d32480063762dd9d93f4800a5bf6c9d7979f2fd3dd8428621e3d260`;
the native Linux compiler is
`3b39d98189350b0feebdf2ce59cae51b1aa05a8c6c037d7ff86ce12eb2a1ee78`.
Provider-free compilers are Windows
`a4ac2cf090baa74c522c3142a01e4a6cf363b1f4cdaeb12f44e8ec3d367ed26d`
and Linux `236a5eb80bfe952cb0f44f8b4ec776d6eadd161935c7a9499214190e5d0325a0`.
Immutable full-runtime captures include public headers, deployed provider
libraries, oracle executables and capture manifests. The final runtime artifacts
are Windows `.lib`
`efb0bc4690c9d42399d58ce49f800e0da4fcef3521adc798a5a3025917f27b59`
and Linux `.a`
`e56d05dbe41dcb4ee79b78ccac330e07c724df70daab3dbdd78f16cd0a3a9afd`.

The final compression repair scope is independently observable in **10/10 named
native regression cases** on both hosts: cancellation after the last native
decode/sink operation; advertised small-zlib history; three Zstandard header/
window/dictionary controls; swallowed sink reentry; first-source cancellation;
and cached-input finish cancellation/fencing. Windows also passes the complete
three-test focused selection in 9.66 s. Exact names/result lines are retained in
`windows-ten-regressions/verification.json` and `linux-ten-regressions.json`.
The shared history corpus is **44 unique frames / 64 policy decisions**, SHA-256
`0363f3b735a4525d8ed2cb9d35f871d5bd93d2186bf10483fd50b1f568fd3b34`.
The final C++ Zstandard admission delta is separately qualified against the
[all-language compression parity proof](verification-compression-parity-2026-10-20.md).

G08 is finally qualified with synchronized **2.0.8** local packages. A fresh-process
inherited-field request had reproduced an actual Jint allocation failure at the
original 256 MiB limit; request-local prepared-mask reuse repairs it without
raising the limit. The final checks pass **154/154 Node unit tests**, **43,422**
fresh generated navigation checks, **1,437** actual .NET/Jint checks, and a
separate **12/12** fresh inherited-field control. The exact Visual Studio DLL
extracted from the built VSIX independently passes both the 12-check control and
the complete 1,437-check suite under the original limits. See the
[editor field-navigation proof](verification-editor-field-navigation-2026-10-20.md)
for before/after evidence, offsets, package contents and SDK commands.

| Final local package | Bytes | SHA-256 |
| --- | ---: | --- |
| `serializer-vscode-2.0.8.vsix` | 87,526 | `cc1e2c444448651aa619f98e1087dc35e587347e5f4eb77cdbcdf310bbc95246` |
| `serializer-visual-studio-2.0.8.vsix` | 1,171,520 | `8cd578d8a3d87b4de651e724ad72c5e4abaa42569d8b5ffdb3b3f1d5a13a7df8` |

Both packages were rebuilt through `./make.ps1 extension` and validated locally;
installation and Marketplace publication are not claimed. The exact 63-file
editor/source-guide inventory has aggregate SHA-256
`90eec4bf69007001f4c25498dc40e568d61854884e1bc09720bf05c8cc2a573c`.
Later 2.15.0 package-backend integration is a separate source batch; none of its
staged or subsequently promoted behavior is attributed to these 2.14.0 gates.

### Retained intervening failures and successful snapshots

The 2,342-file pre-repair Windows run passed **212/213** CTests in 918.71 s.
`serializer_compiler_cli` alone failed with `ios_base::failbit` while rewriting
an existing output. A repeat probe also reproduced the error with the older,
pre-publication compiler: failure at call 313 of 360, followed immediately by an
independent Win32 `CreateFileW` check returning **ERROR_SHARING_VIOLATION (32)**.
The external handle owner was not identified. No compiler retry or I/O semantics
change was inferred or introduced. The old/current compiler probes and original
failed full logs remain in `compiler-io-probe/` and `current-regression-final/`.
The final complete 213-test replay passes the unchanged CLI test.

The corresponding pre-repair native Linux scope passed **210/210** in 391.92 s.
The later 2,349-file history/cancellation snapshot passed **213/213 Windows** in
1,996.44 s and **210/210 Linux** in 486.04 s, plus provider-free **13/13** in
335.34 s and **12/12** in 271.29 s. Its compiler/runtime/source manifests remain
under `current-regression-compression-final/`. The independent all-language
matrix subsequently exposed the C++ Zstandard one-tight advertised-window bug;
that snapshot is retained as pre-fix evidence. The 2,359-file results above
include the narrow repair and three new native tests. The final full and minimal
logs supersede these intermediate runs for current selected-profile gates.

## Historical 2,291-file combined-source replay

Windows and native Linux passed
their complete current combined-source suites. The Windows replay includes the
qualified test-only temporary-file visibility and explicit UTF-8 repairs
described below. The independent corpus audit also supplies selected landed-subset
G01/G02 evidence, with its execution limits stated explicitly.

This record qualifies the selected Serializer **2.14.0**, schema language
**1.20.0** source/configurations for pending gates **G03–G08**. The native
snapshot includes the Java package backend, Kotlin optional compression,
checked identified managed baselines, external-document example/recovery
tests, companion admission before the external payload decoder, Go ZIP, optional
C#/JavaScript compression providers, compatible TypeScript declarations and
unchanged-output publication. Unreferenced staging is not qualified by this run.

## Source identity and host profiles

The retained source archive contains **2,291** nonignored source files, including
existing index/worktree changes and new maintained files. Ordered relative paths
and per-file SHA-256 values are retained in
`out/build/backlog-2026-10-20/current-regression/source-inventory.json`.

| Inventory | Aggregate SHA-256 |
| --- | --- |
| Windows source bytes | `44d60dbb71662bf29beb1237f06030c6c4cc3047237b53141c4cd7683d1ed29c` |
| Native Linux source bytes | `bdb3ae5bfea8234e445e9172f1531e444383c3ae526bdc90d210704638abaf48` |

The earlier 2,280-file archive/inventory remain under the `c7fb6d10` suffix.
After the combined archive, one Windows test-only overlay gives all implicit text
operations in `test/go_compression_test.py` explicit UTF-8. Its final SHA-256 is
`c03ebf9399742c5411bcaa9001d56b45f250a70a84cf9ecddceaa39d69c9b81d`.
The native replay used the archived runner with Linux's existing UTF-8 default;
no generated runtime or compiler source differs for this repair.

WSL briefly returned `CreateInstance/CreateVm/ERROR_TIMEOUT`. A bounded read-only
readiness probe recovered normally, and the complete combined native build and
tests then passed. No VM restart or shutdown was performed.

The Linux archive normalizes only `setup.sh` line endings and executable mode.
The overlay checks every file hash and preserves timestamps when bytes match;
source-only stale files are checked against the expected inventory. Builds and
dependencies remain in separate directories. Later documentation, editor-only
refinements, and unreferenced Go staging are recorded separately in
`live-source-delta.json`; they are not silently attributed to this native snapshot.

| Host | Native build profile | Optional providers |
| --- | --- | --- |
| Windows x64 | MSVC 19.51.36260.0, CMake 4.4.2, Ninja, Release, Python 3.14.3 | LibXml2 2.15.3, zlib 1.3.2, Zstandard 1.5.7, LZ4 1.10.0, all enabled |
| Native Linux x86-64 | GCC 15.2.0, CMake 4.2.3, Ninja, Release, Python 3.14.4 | LibXml2 2.15.2, zlib 1.3.1, Zstandard 1.5.7, LZ4 1.10.0, all enabled |

Windows uses the Visual Studio developer environment, JDK 17 and installed Swift
6.4 toolchain/runtime paths plus matching `SDKROOT`. Tool-generated temporary files
use distinct workspace-local directories. Linux uses a Linux-local source/cache,
native paths/tools, and no inherited Windows toolchain or provider search paths.
Its current cache has a Java runtime without `javac`. Compared with the Windows
inventory, Linux omits the Java compression/package, C# compression and strict
JavaScript/TypeScript compression SDK tests, and adds `stream_allocation_failure_test`.
The omitted SDK tests are covered on Windows and in their separate backend records.

## Complete and provider-free regression

| Gate/profile | Actual result | Primary retained log |
| --- | --- | --- |
| G03 Windows root wrapper | **212/212 passed**, 728.06 s CTest | `windows-current-combined-utf8-final.log` |
| G04 Linux root wrapper | **209/209 passed**, 401.22 s CTest; 660.72 s complete wrapper | `current-regression/linux-full.log` |
| G05/G06 Windows, all four providers disabled | **13/13 passed**, 355.34 s CTest | `windows-current-minimal-combined.log` |
| G05/G06 Linux, all four providers disabled | **12/12 passed**, 71.03 s CTest; 243.31 s complete wrapper | `current-regression/linux-minimal.log` |

Commands use the synchronized repository wrappers: `./make.ps1 test` with the
selected Release cache and four build jobs; native `make test BUILD_DIR=... JOBS=4`.
No repository cleanup/rebuild target runs in the active worktree. Each full run
builds all configured native targets before its complete CTest inventory.

The separate provider-free caches set `SERIALIZER_WITH_XML`, `_ZLIB`, `_ZSTD`,
and `_LZ4` to `OFF`, and enable all four `CMAKE_DISABLE_FIND_PACKAGE_*` guards.
The compiler, managed records library, core tests, compression tests, ZIP tests,
interchange helper and XML event tests build. Their selected CTests include
identity/stored behavior, explicit unavailable-provider results, installed consumers,
compiler-only package discovery and version checks. The core suites execute
592 Windows / 591 Linux cases; package tests execute 44 cases on each host.
Windows additionally runs the generated Java compression test in this selection.
`dumpbin /DEPENDENTS` and native `ldd` outputs confirm the checked compiler and
package-test binaries have no optional provider library dependencies.

Provider-free compiler hashes are Windows
`58672538ea2429cdad1b49b2ed5cf6ddb23406a7b6b5e20b7d5e84c72249846e`
and Linux
`754a03a971ae1d80eb9243555fa5c46bee30056cea9ae0fb076a4f8da763984a`.
Full compiler hashes are Windows
`3533d50e7b9ac33bae04e969080c63ff80402bca69332381d1044f129262804e`
and native Linux
`b4dddfa3bc20be82c42faba3a2473c698ada780d4512882d2ee054a346790629`.
Immutable compiler snapshots retain the deployed Windows DLLs or native SDK
probes and their capture/source manifests.

G07 is exercised by actual `serializer --version`, generated version metadata,
`serializer_package_version`, `serializer_installed_package` and
`serializer_compiler_package`. These cover exact/current and previous compatible
minor/patch requests, rejection of future/different-major releases, real installed
consumer compilation/linking, relocated prefixes containing spaces, language-neutral
compiler discovery with forbidden runtime providers, component requirements,
optional components and runtime ABI admission. Both CLI binaries report compiler
2.14.0 and supported schema language 1.20.0.

## Independent XML/XSD and ZIP/OPC corpus audit

G01/G02 evidence is selected implementation evidence, rather than complete XML,
XSD, Office or OPC specification conformance. The complete Linux replay and the
combined Windows replay execute the following registered checks; every listed
check passes. Their names/output are retained in `oracle-execution-audit.json`
and the complete `LastTest.log` files.

| Selected independent oracle | Actual executed scope |
| --- | --- |
| `xml_xsd_contract_test` | 21 native cases, including **374** repeated/grouped content traces compared directly with the independent LibXml2 XSD validator |
| `xml_schema_test`, `xsd_import_test`, `xml_operation_test` | 11, 23 and 19 native cases respectively; exported schemas compile, reimport and independently accept/reject controls, with occurrence and cancellation/resource checks |
| `html_interop_test` | Six fragments compared with Python `HTMLParser` and lxml HTML traces |
| `package_test`, `python_package_test`, `opc_profile_test` | 44 native, 30 actual generated Python and six typed native cases; shared independent malformed/descriptor/content-type/relationship fixtures |
| `package_zip64_test` | **56 cases / 100 native-and-generated-Python decisions**, zero failures; independent `zipfile` controls, 65,534/65,535/65,536 entries, real 65,536-entry native output and segmented offsets around 4 GiB |
| `package_independent_interop` | Exact canonical stored ZIP bytes, three archives consumed by Python `zipfile`, and independently parsed OPC metadata/payloads |
| `serializer_opc_exchange` | Four C++/Python producer-consumer checks and two independent ZIP/XML inspections |
| `serializer_package_go` | 66 independent inputs, 54 API checks, seven independently consumed outputs and 11 native interchange controls; ZIP foundation only, with unsupported OPC generation rejected |
| Windows `serializer_package_java` | Actual JDK 17, 224 decisions, nine independently inspected outputs and collision/outer-name profiles; its CTest invocation has no optional native-exchange argument |

The Linux Go report pins compiler `b4dddfa3`, generated/output hashes and Go
1.26.0; the runner takes 37.404 s (37.67 s CTest). Its generated/output hashes
match the Windows proof. The registered publication test separately passes all
six checks without skips (0.920 s runner, 1.09 s CTest), including preserved
timestamps for equal output and transactional failure controls.

`xml_fixture_manifest_test` passes seven inventory tests covering the 11-language,
121-pair matrix and mandatory missing-SDK behavior. It does **not** execute all
121 exchanges. `SERIALIZER_BUILD_ALL_LANGUAGE_EXAMPLES` is `OFF` in these full
profiles, so their default inventories do not execute the opt-in all-language XML
SDK matrix or JavaScript package runner. The earlier actual JS/TS package proof
separately records 175 independent inputs, 14 outputs, five native exchanges and
three strict TypeScript profiles; the Java standalone proof separately supplies
two native typed exchanges. See the [JS/TS package proof](verification-javascript-packages-2026-10-20.md)
and [Java package proof](verification-java-packages-2026-10-20.md).
P16/P20/P21 declarative gaps and unsupported backend/profile combinations remain
separate requirements.

## Editor behavior and synchronized packages

G08 found and repaired three actual navigation issues: JavaScript package factory
constants were classified as enums, TypeScript package namespaces were not indexed,
and backticks inside ordinary strings could hide later declarations. Generated
Java runtime markers now let the editor skip compiler-owned runtime declarations
while retaining exact UTF-16 offsets and authored model/package containers. Literal
and comment masking operates on line spans to reduce interpreter allocations.
Unmarked legacy generated files retain the existing indexing path. Public model
names, serialized bytes, schema syntax and extension identities remain unchanged.

Final actual checks pass **154/154** unit tests, **43,422** freshly generated
bidirectional checks across all 11 ordinary output languages, and **1,437** source/
output checks through the real Visual Studio .NET/Jint adapter. The latter includes
42 fresh Java/JavaScript/TypeScript package declaration/source boundary checks;
the unchanged per-request limits remain 256 MiB and 30 seconds. Live edits,
transitive includes, member/type references, naming profiles, cursor/selection
boundaries, cancellation and unrelated enum identity remain covered.
The complete Node and .NET/Jint checks were rerun against the combined `3533d50e`
compiler; the verified editor packages and payload hashes remain unchanged.

Both manifests, npm lock metadata, changelogs, READMEs, listing payloads and output
filenames use **2.0.7**. `./make.ps1 extension` builds both packages with locked
offline SDK caches. The Visual Studio package validator passes. Independent ZIP
inspection verifies the VS Code identity/version and exact canonical grammar,
snippets, language configuration and compiled payload bytes.

| Package | Bytes | SHA-256 |
| --- | ---: | --- |
| `out/extensions/serializer-vscode-2.0.7.vsix` | 87,208 | `bd53a32444042c93878079dab91ae2ca46b629d7cbe8f4d17eebd2123ff3a094` |
| `out/extensions/serializer-visual-studio-2.0.7.vsix` | 1,171,453 | `7ceeed57617bec57b20ed04349e922823069e3e5be442b783d8f5b3997585838` |

These are local built/validated artifacts; no installation or Marketplace
publication is claimed. Superseded task-created 2.0.6 package files were removed.
The 60-file editor/source-interface inventory is retained in
`current-regression/editor-source-inventory.json`, aggregate SHA-256
`15570497d12e18af989073e0f80db5b3cd5e00e683f0bff0354803b6f4df9971`.

## Failure evidence and replay boundaries

Initial failures remain in retained logs rather than being overwritten:

- Strict GCC exposed an uninitialized Java emitter member under `-Weffc++`;
  value initialization repairs it without changing emitted runtime semantics.
- The provider-free descriptor fixture originally assumed DEFLATE availability.
  It now always checks ZIP admission/CRC and expects explicit `unavailable` when
  payload DEFLATE is disabled. Both independent provider-free replays pass.
- Windows first encountered generated temporary-file access and later one
  verified zero-byte object (`windows-empty-objects.json`). Separate owned temp
  directories and regeneration of that single empty object resolve the build
  artifacts; source diagnostics were not suppressed.
- The earlier Windows full run had a separate32 baseline Save failure. The same
  binary/case subsequently passed 20/20 isolated repeats. It passes in the complete current Windows replay; no managed-runtime repair is inferred from that earlier isolated failure.
- Actual Jint reproduced the Java-package memory failure before masking repairs;
  package-only and complete adapter checks now pass under the original limits.
- The final Windows run passed 29/30 Python package cases but observed a temporary
  directory entry after the owned rolled spool had already closed. A controlled
  standard-library shared observer reproduces this Windows delete-pending behavior.
  The test retains immediate closed/rolled/borrowed-ownership checks and allows up
  to one second only for Windows directory visibility; persistent leftovers fail.
  Both unchanged and repaired complete 30-case suites pass 20/20 repetitions each
  (1,200 case executions), and the repaired maintained cleanup test passes with a
  controlled observer releasing its handle 50 ms after close. No runtime change
  is inferred. Both subsequent complete package suites pass this check.
- The first combined Windows replay passed 211/212 in 1324.44 s. Its sole failure
  was the Go compression test runner decoding valid generated UTF-8 with the
  Windows CP1252 default. Explicit UTF-8 fixes the maintained test; the actual
  combined-compiler standalone replay passes 82 independent inputs, 16 outputs
  and all model/collision/binary/XML-independent and lifecycle profiles. The final
  complete wrapper replay with this test-only repair passes all 212 tests.

The earlier Linux snapshots passed 207/207 in 571.42 s and 228.97 s; their logs and
inventories are retained separately. The table above uses the later combined
209-test replay rather than substituting either older run.
The final D14 repair separately passes `managed_direct_test`,
`managed_journal_crash_test` and `managed_external_document_example`: Windows
3/3 in 132.10 s and native Linux 3/3 in 180.25 s. Its three before-control cases
proved the unwanted decoder calls on the old runtime; the after-controls make
zero decoder calls on invalid companion/baseline admission. See the
[document bridge qualification](verification-managed-document-bridge-2026-10-20.md).

Final logs, exact inventories, CMake caches, LastTest output, compiler snapshots,
package metadata and reusable replay helpers are intentionally retained below
`out/build/backlog-2026-10-20/`. Optional provider SDK proofs and future source
deltas retain their own scope; this record does not close unsupported declarative
package policies or broader backend contracts merely from a successful build.


## Checkpoint hygiene

Before staging this 2.14 checkpoint, all **880 compiled inputs** selected by the
final source manifest were compared again with the live worktree: no mismatch.
The README, usage guide, migration notes and canonical integration skill match
the implemented scope; both current editor versions are 2.0.8. Skill validation,
Markdown link validation, whitespace checks and the public confidentiality scan
pass. Checklist IDs are unique and use only task tables with explicit completion
markers. Required wire fixtures, manifests and locks remain source files.

The existing `out/` ignore rule covers qualification logs, immutable tool captures,
SDK caches and preserved unfinished work. Those useful local artifacts are kept
outside the checkpoint. No new ignore rule is required. Unqualified future
implementations remain unchecked and are excluded from the source checkpoint.

The staged whitespace check also removed indentation from two otherwise empty
lines in the C# compression source template. This changes only generated-source
whitespace; the compiled-input manifest and native results above identify the
preceding tested snapshot. No code, signature or encoded format changed.
