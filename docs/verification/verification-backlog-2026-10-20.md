# Backlog implementation checkpoint, 20 October 2026

[Verification index](README.md) · [Remaining requirements](../pending.md)

This record covers compiler/runtime **2.14.0**, schema language **1.20.0** and
unchanged editor packages **2.0.5**. The starting source revision is
`bd2ee2ef9239b65335367bd8de19be377ce84262`; later focused records identify baseline
`0928053389fcf237e53f682442510ae19ef5eb5c` plus working-tree changes where applicable.
Results apply to the source snapshots and named profiles in each section, not
every platform, provider or combination in the backlog. The earlier 200-test
native runs do not qualify all subsequent source changes.

## Compiler-only CMake discovery: BE04

The installed package supports `COMPONENTS compiler` without runtime dependency
discovery. The executable export is independent of the runtime export; default
discovery and `COMPONENTS runtime` retain the existing complete targets and
provider requirements. Both the traditional library directory and the neutral
`share/cmake/Serializer` directory contain relocatable package metadata.

The Windows `serializer_compiler_package` regression passes with CMake **4.4.2**.
It installs the actual package, relocates it to a prefix containing spaces,
disables its library-directory config, and discovers it through the neutral
prefix with `LANGUAGES NONE`. Poisoned provider modules/configs establish that
compiler discovery does not search XML or compression dependencies. The test
generates Python and Java source with the installed helpers and runs the imported
compiler's `--version`.

The same check verifies required/optional components, repeated compiler/runtime
lookups, older same-major and future-version requests, the C++ helper's runtime
prerequisite, and runtime pointer-width rejection. An ABI-incompatible optional
runtime leaves host compiler discovery usable; unknown optional-only names retain
default full-package discovery and its ABI checks. A mocked cross toolchain has
an empty package-only target sysroot and the opposite pointer width; explicit
`NO_CMAKE_FIND_ROOT_PATH` selects the host compiler. This is host-lookup coverage,
not execution on another CPU or operating system.

Log: `out/build/backlog-2026-10-20/be04-focused.log`.

## First identified managed initialization: D22

The [identified-baseline record](verification-managed-identified-baseline-2026-10-20.md)
qualifies the additive tagged `model_store` constructor from typed direct or
separated storage, a nonzero stored document namespace and the native allocation
watermark. IDs, declared reference tokens and burned allocation gaps remain exact;
runtime-only fields reset consistently with native load. Bounded native admission,
complete reference/type checks and application validation precede the scoped
allocation-resource hook. Acceptance establishes one initial history baseline
without allocating IDs, remapping, fabricating an envelope or attaching files.

Windows and Linux each pass **197/197 cases across 9/9 CTests**, including **81 new
D22 cases**. The eight generated profiles cover both ID widths, both ID encodings
and direct/separated storage; the ninth target checks runtime-only reset. Rejected
state, history/resource admission, all three history modes, native Save and
appended/sidecar recovery are exercised. The Windows batch takes **8.78 s** and
the Linux batch **1.11 s**. Their exact hashes/toolchains and retained logs are
recorded separately from complete release gates.

## Historical persistent-ID artifacts: MIG01

The [dedicated historical qualification](verification-managed-legacy-migration-2026-10-20.md)
passes **92/92 actual CLI migrations** from genuinely rebuilt compiler/runtime
**2.6.0**, revision `7b166e202668747f3a7e9a3545b3e4d75e853599`.
Direct uint32 and separated uint64 profiles cover all three raw binary formats,
normal/high-ID graphs and five native history/label shapes. Reverse conversions
reproduce the older fixed files exactly; current native readers verify document
and object identities, allocation nonreuse, labels, branch ancestry and cursors.
This closes MIG01 for those selected Windows profiles. Other historical layouts,
unsupported containers and Linux migration replay remain outside that result.

## C++ compression: CP-Z01 through CP-Z10

The existing C++ reference APIs pass the full-provider native/core, stream and
allocation-failure checks and the provider-free replay described below. Each of
gzip, zlib and raw DEFLATE passes **16 independent producer/consumer decisions**
through the existing `compression_interop_test.py` corpus. Zstandard and LZ4 pass
another **32 independent decisions** covering empty, textual, repeated 300,000-byte
and fixed-seed random 200,000-byte inputs, in all four encode/stream-encode and
decode/stream-decode modes. Independent decoding checks exact EOF and unused data.

The Windows native providers are Zstandard **1.5.7** and LZ4 **1.10.0**. Independent
CPython **3.14.3** uses standard-library Zstandard **1.5.7** and python-lz4
**4.4.5**, linked to LZ4 **1.9.4**. These checks qualify the selected existing
reference API/profile; they introduce no compression wire changes. Broader
shared-operation/S35 parity **CP-Z11** and other platform/provider profiles remain
separate work. The independent interchange log is
`out/build/backlog-2026-10-20/cpp-optional-compression-interop.log`.

## Python compression: PY-Z01 through PY-Z10

The actual rebuilt compiler generates normal, name-collision and binary-only
modules; the final lifecycle replay passes **21/21 tests, with no skips**, in
**0.548 s**. The selected Windows profile uses
CPython **3.14.3**, zlib **1.3.1.zlib-ng**, standard-library Zstandard **1.5.7**,
and python-lz4 **4.4.5** linked to LZ4 **1.9.4**. The optional LZ4 wheel is installed
only under ignored `out/python-compression-provider`.

The suite checks identity, gzip, zlib, raw DEFLATE, Zstandard and LZ4; independent
provider encode/decode exchanges; malformed framing/checksums, truncation,
trailing/concatenated input, dictionaries and unsupported frame profiles; exact
input/output/window limits; missing-provider admission before I/O; cancellation
and failure fencing; and borrowed short-reading/writing transports. Staged input
retains its policy and observes later read/finish cancellation; output observes
cancellation triggered by its final borrowed sink write. Generated
models retain their bytes in all four native protocols. Invalid native policies
fail before source I/O, binary-only protocol selectors remain enforced, and late
cancellation withholds the decoded candidate.

The final callback regressions reject same-wrapper reentry even if the callback
ignores its nested error. Active outer calls preserve the first original failure;
later inactive calls keep `RuntimeError` and retain that failure as their cause.
The immutable compiler SHA-256 is
`0d318c2e3b6ac19e0cea1b8561bcc2e38986f61f60025e2d00bec30c10856a94`;
the Python runtime template is
`568dd0f891573a5169aebcb0ce5f9db5a84aebf8f4a0e199bb3c791b72dff030`.

`__serializer_compression__` is the stable namespace. The optional `Compression`
alias preserves an existing model with that name. Provider internals and process
RSS are outside logical byte bounds. Broader shared-operation/S35 parity
**PY-Z11**, other Python/platform profiles and performance qualification remain
separate requirements.

The original 18-case log is `python-compression-final.log`; the final 21-case log
is `out/build/backlog-2026-10-20/python-reentry-final.log`.

The separate [performance slice](verification-python-compression-performance-2026-10-20.md)
records fixed 1 MiB workloads, independent provider baselines, encoded/generated
sizes and traced Python allocations. The final lifecycle helper produces a
**54,799-byte** module, SHA-256
`0258cd157cc4703fdbc34eb9a7b370c8ed63342c4241852030a4948d413eb1d4`.
That refreshed benchmark uses immutable compiler `4a4f8604…d8d813` and five measured
iterations after warmup. Concurrent SDK qualification and excluded native
workspace/RSS prevent treating those measurements as general performance gates.

## Java compression: JV-Z01 through JV-Z10

The actual generated SDK passes **957 independent input decisions**, independent
Python consumption of Java output, all four native model protocols, exact limits
and levels, zero-length inputs, missing-provider admission, cancellation, I/O
exception identity and stream failure fencing. Random frame exchanges and
schema-helper/output-class name collisions are also checked. Compilation uses
JDK **26**, `--release 17 -encoding UTF-8 -Xlint:all -Werror`; execution uses Java
**26** (build **26+35-2893**). This is not a Java 17 runtime qualification.

The core formats are identity, gzip, zlib and raw DEFLATE. The separate
[optional-provider record](verification-java-compression-providers-2026-10-20.md)
qualifies **JV-Z09/JV-Z10** against actual generated output: Zstandard passes
**136 independent input cases** and four native model round trips; LZ4 passes
**179 independent input cases** and four native model round trips. The LZ4-only
classpath also passes independently of the Zstandard jar. Direct C++ frame
exchanges, provider absence, checksums, linked blocks and bounded failure paths
are recorded there. Broader shared-operation/S35 parity **JV-Z11** remains open.
The final provider-allocation cleanup ordering
follow-up also passes the repeated suite against the refreshed compiler; emitted
scratch/CRC allocation precedes both native provider constructions.
The final callback lifecycle replay uses immutable compiler
`0d318c2e…856a94`. The 957-case core suite, both optional providers, independent
LZ4-only classpath and direct C++ exchanges all pass again. Callback close/reentry
now prevents further borrowed I/O or successful completion, even if the nested
failure is swallowed. The active operation preserves its first checked I/O,
runtime or `Error` object; later inactive calls keep `IOException` with that cause.
The decoder also checks cancellation after the native callback before publication.
These regressions and the replayed shared corpus are recorded in the
[provider record's follow-up](verification-java-compression-providers-2026-10-20.md#subsequent-callback-lifecycle-regression-checks).

Logs include the original `java-compression-final.log` and final
`java-reentrancy-final.log`, `java-optional-lifecycle-final.log` and
`java-lz4-only-lifecycle-final.log` under `out/build/backlog-2026-10-20/`.

## Go compression: GO-Z01 through GO-Z10

The [Go verification record](verification-go-compression-2026-10-20.md) qualifies
actual generated standard-library identity/gzip/zlib/raw DEFLATE helpers on
Windows Go **1.26.4** and Linux Go **1.26.0**. Each selected profile passes
**82 independent input decisions**, **16 independent output checks** and seven
top-level test groups, with separate collision, binary-only and XML-independent
modules. Borrowed stream finish/failure behavior, native protocols, exact bounds
and cancellation are exercised. A strict GCC **15.2** Linux compiler refresh
passes **5/5 focused CTests** in **9.66 s**. The default generated core remains
self-contained standard-library Go **1.22+**; these executed SDKs do not establish
an actual Go 1.22 runtime replay.

The [optional-provider proof](verification-go-compression-providers-2026-10-20.md)
adds caller-owned per-operation adapters pinning klauspost/compress **1.20.3**
and pierrec/lz4/v4 **4.1.33**, with module baseline **Go 1.25**. On both Windows
Go **1.26.4** and isolated Linux Go **1.26.0**, **136 Zstandard** and **181 LZ4**
decisions, **16** checksum/native-protocol round trips, **32** C++ consumer checks
and **6** C++ producers pass. Two linked-LZ4 controls independently require prior
block history. The isolated Linux replay executes unchanged generated Go from the
Windows compiler and fresh native Linux C++ helpers; it does not claim a new
Linux compiler-source build.

The final immutable `0d318c2e…856a94` compiler also passes **32** default-format
and **16** optional-provider callback-reentry boundaries. The latest default and
optional suites pass eight and nine test groups respectively. An ignored nested
failure still fences the outer operation and later I/O. Complete cumulative
shared-operation/performance qualification remains under **GO-Z11**.

## C# compression: CS-Z01 through CS-Z08

The [C# verification record](verification-csharp-compression-2026-10-20.md)
qualifies actual generated output with .NET SDK **8.0.426**, runtime **8.0.32**,
and compiler SHA-256
`0d318c2e3b6ac19e0cea1b8561bcc2e38986f61f60025e2d00bec30c10856a94`.
All **974 independent input decisions**, **16 independent output checks**, four
native model protocols and five warning-free profile/collision builds pass.
The tested formats are identity/gzip/zlib/raw DEFLATE. Frame/tree/history/window
validation, exact caps, late cancellation, sticky failures and borrowed stream
completion are exercised. The later optional-provider qualification below adds
selected Zstandard/LZ4 adapters; full shared-operation parity remains open.

The maintained callback regression first fails against the old emitted helper.
The final complete SDK replay passes four-format callback disposal/reentry,
source/sink publication fencing and first-exception identity checks. Later
inactive calls preserve their established lifecycle exception type with the first
failure as cause. See the [final regression detail](verification-csharp-compression-2026-10-20.md#final-callback-lifecycle-regression).

## JavaScript/TypeScript compression: JS/TS-Z01 through Z08

The [JS/TS record](verification-javascript-compression-2026-10-20.md) qualifies
actual emitted ESM on **Node 22.23.1**, zlib **1.3.1-e00f703**, and matching strict
**TypeScript 7.0.2** consumers. **974/974** input decisions, **16/16** independent
outputs and **4/4** declaration/profile consumers pass against immutable compiler
`0d318c2e…856a94`. Browser-like isolation checks ordinary imports, native codecs
and identity without Node imports. `await nodeProvider()` explicitly loads the
optional gzip/zlib/raw-DEFLATE provider for each operation's options.

The before-fix callback regression fails; the repaired suite passes **102/102**
callback-close boundaries plus asynchronous overlap, owned-input, first-failure
and borrowed-transport checks. TypeScript describes the same asynchronous JS
runtime. The later provider proof below qualifies selected Zstandard/LZ4
adapters; actual browser SDKs and broader JS-Z11/TS-Z11 context/performance
gates remain open.

## Kotlin compression: KT-Z01 through KT-Z08

The [Kotlin record](verification-kotlin-compression-2026-10-20.md) qualifies
actual emitted code with **Kotlin 2.2.21**, **OpenJDK 17.0.18+8-LTS**,
`-Werror -jvm-target 17`. Standard JDK identity/gzip/zlib/raw-DEFLATE helpers pass
**974/974** input decisions, **16/16** independent outputs and **4/4** actual
compiled profiles against immutable compiler `4a4f8604…d8d813`. Exact native
consumption, late publication cancellation, callback close/reentry and borrowed
stream lifecycle checks pass. The final strengthened collision smoke separately
compiles/runs both preserved native model names beside the selected helper.
The subsequent optional-provider proof below extends this core qualification;
broader KT-Z11/performance qualification remains open.

## Swift compression: SW-Z01 through SW-Z08

The [Swift proof](verification-swift-compression-2026-10-20.md) qualifies actual
emitted standalone code on native Linux Swift **6.1.3** and Windows Swift **6.4**,
with warnings as errors.
It passes **974/974** independent input decisions, **64/64** independent output
checks and **3/3** generated SDK profiles on each host. Exact native decoding,
bounded history, short transport I/O, close/reentry and sticky first failures
pass. The Linux SDK receives Windows-generated source in a Linux-local directory;
the Windows replay imports the matching SDK/runtime and Visual Studio developer
environment. It required approved SDK-helper launch permission. Neither direct
SDK proof claims a new Linux compiler rebuild or a complete native CTest result.
Kotlin's representative output remains byte-identical. Optional providers and
broader shared-operation/performance gates remain open.

## C compression: C-Z01 through C-Z08

The [C proof](verification-c-compression-2026-10-20.md) passes **974/974**
independent input decisions, **48/48** independent output checks and **5/5**
generated SDK profiles on both Windows MSVC **19.51** and native Linux GCC
**15.2**. The optional application zlib versions are **1.3.2** and **1.3.1**
respectively; standalone identity also compiles/runs without zlib. Typed native
publication, separate codec statuses, strict framing, bounded callbacks and
first-failure lifecycle fencing pass. The selected Windows compiler emits both
SDK inputs. Optional Zstandard/LZ4 and broader shared-operation/performance gates
remain open.

## Shared compression exchange: selected Z11 slice

The separate [shared compression qualification](verification-compression-exchange-2026-10-20.md)
passes **144/144 actual generated-model exchanges** across C++, Python and Java,
four native protocols and identity/gzip/zlib/raw DEFLATE. Each adapter replays the
same **83 framing/error/limit cases**, producing **249/249 matching decisions**.
Independent zlib checks validate all **48 producer frames** and **12 exact-bound
output frames**. Explicitly missing SDKs fail the run rather than reducing scope.
The record identifies the exact compiler/runtime, generated sources and consumers.
This is a shared core-format slice; optional providers, other backends and complete
cumulative operation-context contracts remain under Z11.
The final lifecycle follow-up replays the same **144 exchanges, 249 decisions,
48 producer checks and 12 exact-output checks** successfully against immutable
compiler `0d318c2e…856a94`; its source hashes and
`compression-exchange-lifecycle.json` are recorded in the
[Java follow-up](verification-java-compression-providers-2026-10-20.md#subsequent-callback-lifecycle-regression-checks).

## Python OPC Unicode authority slice: OPC-R08

The actual generated package facade passes **14/14 tests**, with no skips.
Independent ZIP/XML fixtures preserve Unicode authority characters that resemble
ASCII delimiters, relationship types and external targets without normalization.
Malformed IP literals, zone identifiers, escapes and ports reject. Internal
target tests preserve query/fragment handling and reject schemes/authorities.
Failed Save tests preserve existing final bytes.

The baseline run reproduces the former Unicode authority rejection and IPv6 zone
overacceptance. The selected OPC edition remains **ECMA-376 Part 2, fifth edition,
December 2021**. Full Annex B/pass-through and official 3MF application/corpus
qualification remain open; this slice does not close all of OPC-R08.

Logs: `out/build/backlog-2026-10-20/python-authorities-baseline.log` and
`out/build/backlog-2026-10-20/python-authorities-final.log`.

## ZIP/OPC reference qualification

The [package reference record](verification-package-reference-2026-10-20.md)
closes selected **CP-P01–CP-P08, CP-P13, CP-P14, CP-P19** and
**PY-P03–PY-P08, PY-P10, PY-P13, PY-P14** instances. It combines independent
existing exchanges with **41 content-type decisions per backend** and ten shared
malformed ZIP fixtures. The narrow Python repair reports truncated central-field
ranges as `invalid_archive`; three explicit unsupported-feature probes retain
their prior category. Fresh Python package tests pass **17/17**; both Windows
and Linux focused native/generated selections pass **5/5**. The record distinguishes
their exact test selections and retains the before-fix failure and source hashes.
Remaining package/profile/reference and full OPC conformance work stays open.

The [descriptor and package-lifecycle proof](verification-package-lifecycle-2026-10-20.md)
adds signed/unsigned 32/64-bit descriptors, exact byte-view/I/O accounting, borrowed
transport bounds, spool cleanup, Save publication fault injection and **36**
canonical permutations. The initial generated Python replay passes **25/25**;
the later native spool matrix passes **6/6 focused CTests** on Windows (**6.28 s**)
and Linux (**2.81 s**), including **44** native package cases and **6** generated
C++ profile cases. The Linux follow-up also replays **28/28** Python tests.
Interrupted private spools are removed, and Linux checks their live private
permissions. These are controlled fault checks, not actual power-loss evidence.

The [typed profile/preservation proof](verification-package-profiles-2026-10-20.md)
qualifies first/middle/last typed-part failures before aggregate publication,
**21** reject/skip/preserve/required decisions per backend and edited dangling-link
rejection before Save replacement. Windows and Linux each pass **6/6 focused
CTests**, including **28/28** generated Python tests; Windows takes **1.37 s**
and Linux **1.00 s** for those source snapshots. Full field/budget diagnostics,
automatic payload-key/reference generation and complete OPC/3MF conformance remain
separate work.

## Published distribution: REL01 selected profile

The [distribution qualification](verification-distribution-2026-10-20.md)
checks the actual published **2.0.0** VSIX archives and a byte-matched upstream
**2.0.1** port through vcpkg's x64-windows build/install pipeline. All **9/9
installed-consumer CTests** pass, including code generation, managed examples
and twelve compression round trips. Its installed compiler reports schema
language **1.17.0**. This closes the selected REL01 profile; other platforms,
editor hosts and publication retain QA01, QA04 and REL02 respectively.

## Earlier native build snapshots and subsequent focused gates

The earlier Windows root `./make.ps1 test` snapshot passes **200/200 tests** in **134.38 s**
of CTest time, using MSVC, XML/zlib/Zstandard/LZ4 and managed support enabled,
eight build jobs and four concurrent tests. This includes installed headers,
ordinary native codecs, managed recovery/collaboration, package/compression
interchange and generated Python/Java compression. The first interrupted run's
default temporary directory had sandbox access failures; the passing run uses
an ignored workspace temporary directory. No result is claimed for the interrupted
run.

After the full run, the compiler is refreshed for the final Java provider-cleanup
ordering, behavior-preserving GCC fixes and optional-component metadata.
The **9/9 focused CTests pass** in **28.55 s**: compiler-only installation,
package version, compiler CLI, Python and Java compression, Python packages,
XSD bundle/import CLI and XML schema tools. Logs are
`out/build/backlog-2026-10-20/windows-native-final.log`,
`windows-compiler-final.log` and `windows-source-final-checks.log`.
An earlier five-test focused replay also passes.
The subsequent cancellation repairs pass **18/18 Python cases** with all six
providers and **1/1 Java CTest** in **19.38 s**, using another refreshed compiler.
The equivalent malformed journal-metadata fixture construction also passes the
rebuilt **1/1 managed-direct CTest** in **14.25 s**. These follow-ups are recorded
in `windows-cancellation-compiler.log`, `python-compression-final.log`,
`windows-cancellation-java.log` and `windows-managed-final.log`.

The earlier Linux root `make test` snapshot passes **200/200 CTests** in **94.77 s**,
with XML, zlib, Zstandard, LZ4 and managed support enabled. Tools/providers are
GCC **15.2.0**, CMake **4.2.3**, Ninja **1.13.2**, LibXml2 **2.15.2**, zlib
**1.3.1**, static Zstandard **1.5.7** and LZ4 **1.10.0**. Native provider builds
and a strict independent find/link/frame-roundtrip consumer also pass.
Installed compiler-only discovery passes with the intentionally nested
`lib/serializer-test-arch` layout, exercising neutral package discovery on Linux.
The CLI reports compiler **2.14.0**, schema language **1.20.0**.

Linux CPython **3.14.4** runs all **18 compression tests**, with one explicit
`lz4.frame`-specific skip: its Python providers are identity, gzip, zlib, raw
DEFLATE and stdlib Zstandard. Native C++ LZ4 remains enabled and tested. The Linux
Java compiler is absent, so Java SDK evidence remains the Windows execution above.

The mounted `/mnt/c` filesystem stalled in 9P I/O while Linux-local operations
remained responsive. Verification moved to a byte-streamed, inventoried snapshot
under `/var/tmp/serializer-backlog-20261020-4ef8773fa8`; inherited Windows PATH
entries were excluded from native Linux tool discovery. A WSL restart cleared an
earlier `/tmp` attempt. The final **2,139-file** snapshot has aggregate SHA-256
`8644031f37e00d5d2f20a0a026ac97004a5ed38231478c2eab5733ba42f0bfed`.
At capture time its comparison to the live tree found no existing native
source/build-input drift. Later managed admission, package tests and generated
compression/lifecycle changes have their own focused proofs above; they are not
covered by that 200-test source snapshot.

`-Weffc++` ownership diagnostics are fixed through deleted private state copy/move
members. An explicit Boolean grouping in `src/xml_schema.cpp`
preserves the expression while satisfying GCC's parentheses warning. The private
native XML emitter also declares its existing noncopyable/nonmovable state
explicitly for GCC; callbacks keep the same borrowed owner and state address.
The malformed journal fixture now allocates its extra-byte candidate before
copying the metadata prefix, preserving the original bytes while avoiding GCC's
optimized vector-insertion diagnostic. No warnings are suppressed.
Final logs and the hash inventory are `linux-local-rerun-3.log`,
`linux-final-LastTest.log`, `linux-final-evidence-manifest.json` and
`linux-live-source-comparison.json` under `out/build/backlog-2026-10-20/`.
The retained evidence archive has SHA-256
`798a6709e8a2e4875718394d979a05a5f49ce1258a04335940022dd9aab15ced`.
An earlier full-provider replay also passes 200/200 before the final cancellation
overlay; the later 200-test snapshot above supersedes that earlier run only.

## CP-P09/PY-P09: ZIP64 input boundaries

The [ZIP64 proof](verification-package-zip64-input-2026-10-20.md) passes **56 cases /
100 backend decisions** on each of Windows and Linux, plus **30/30** generated
Python package tests. Independent sparse inputs exercise offsets above 4 GiB
with about 66 KiB actual reads; real 65,534/65,535/65,536-entry archives qualify
count transitions and duplicate-name controls. Exact representable classic
fields and optional ZIP64 metadata are corrected. Checked seekable Python
sources preserve their borrowed ownership and current-position base. Native
indexing reuses existing admitted scratch; measured 65K indexing drops from
roughly 10–13 s to 0.04–0.06 s. This is the focused package snapshot in that proof,
not qualification of later language backends or physical multi-gigabyte files.

## CP-P12: native full-Save fault boundaries

The [native Save proof](verification-package-save-faults-2026-10-20.md) executes
the production ZIP and OPC Save paths with native creation, partial-write,
file-sync and replacement failures. Windows passes **2/2 cases**; Linux passes
**3/3**, adding postpublication directory-sync uncertainty. Old files remain
unchanged before publication; successful or uncertain published files validate
all payloads. Every branch releases owned native handles and removes staging.
This is a static-runtime error-injection qualification, not a power-loss or
document/sidecar crash-recovery claim.

## Provider-free Windows replay: G05/G06 selected slice

A separate isolated build has XML, zlib, Zstandard and LZ4 explicitly **OFF** and
all four corresponding `CMAKE_DISABLE_FIND_PACKAGE_*` guards **ON**, without a
vcpkg toolchain. Configure and the selected 297-step native build pass. The
**10/10 selected CTests** cover core codecs, compression streams/allocation,
packages/stored ZIP interchange, package-version compatibility, installed public
headers/examples, relocated compiler-only discovery and Python/Java helpers.
The batch takes **188.44 s** of CTest time.

Three additional XML event/C++23/provider-boundary CTests pass. The XML reader's
one unavailable-provider case passes; **eight provider-dependent cases explicitly
skip**. A separate **1/1 installed runtime probe** confirms XML and all five
built-in compressed formats are unavailable, identity bytes round-trip, and
normal runtime discovery retains the complete targets while all provider lookups
are disabled. The compiler reports **2.14.0**, schema language **1.20.0**; `dumpbin`
shows Windows CRT/kernel dependencies and no XML/compression provider DLLs.

Tools are MSVC **19.51**, CMake **4.4.2**, Ninja **1.13.2**, clang-format **22.1.3**
and GoogleTest **1.18.0**. Logs are
`out/build/backlog-2026-10-20/windows-minimal-{configure,build,tests,xml-build,xml-tests,runtime-probe,version,binary-dependencies,metadata}.log`.
These selected Windows runs do not close other-language/platform release gates.

The Linux-local native cache, provider builds and final verification artifacts
are retained for useful incremental replays. Broader platforms and the rest of
the backlog remain open.

## Rust compression: RS-Z01 through RS-Z08

The [Rust proof](verification-rust-compression-2026-10-20.md) qualifies
standalone Rust 2021 identity/gzip/zlib/raw DEFLATE on actual Windows Rust
**1.99.0** and native Linux Rust **1.93.1**. Each final SDK run passes **974**
independent input decisions, **64** independently decoded outputs and three
strict generated profiles, with two expected borrow-checker reentry failures.
The strict GCC native Linux compiler rebuild emits all three profiles byte for
byte identically to the frozen Windows compiler. Existing generated native
source is preserved as an exact prefix.

Wrappers take adapters by value; mutable references retain caller ownership.
Typed wrapper methods preserve error identity and require explicit finish;
standard `Read`/`Write` trait nesting requires an application adapter. Optional
Zstandard/LZ4, broader cumulative contexts and performance gates remain open.
The proof records exact source/compiler/SDK hashes and the selected Linux
overlay inventory; it does not qualify later unrelated backend changes.

## JavaScript/TypeScript packages: selected P01–P15 and P17–P19

The [JS/TS package proof](verification-javascript-packages-2026-10-20.md)
qualifies actual Node **22.23.1**, strict TypeScript **5.9.3** and pinned XML
dependencies against immutable compiler `97148980…`. The final run passes
**175 independent inputs**, **14 independent outputs**, **five C++ exchanges**,
**56 policy-close boundaries** and three strict declaration/collision profiles.
Browser import and stored ZIP require no Node builtin loads. Exact/tight shared
budgets, mid-spool cancellation, awaited transport closure, terminal private
staging release and five atomic Save fault stages pass.

The facades use typed generated XML and complete fresh-aggregate validation.
Explicit Node compression/filesystem providers enable DEFLATE, spooling and Save.
P16/P20/P21 remain pending: this profile does not establish automatic nested
cumulative contexts, arbitrary runtime native-codec selection or generated
application key/reference rules. Filesystem fault execution is Windows-specific.

## Optional C compression: C-Z09 and C-Z10

The [C provider proof](verification-c-compression-providers-2026-10-20.md)
qualifies actual MSVC/GCC C11 with native Zstandard **1.5.7** and LZ4 **1.10.0**.
Each host passes **317 input decisions** (136 Zstandard/181 LZ4), **24 independent
outputs**, **48 C++ consumers**, **six native producers** and six strict
deployment/collision/protocol profiles. Provider/level/history admission runs
before callbacks or borrowed I/O; linked history, dictionaries, checksums,
exact frames and fresh typed native publication are independently checked.

The existing C identity/zlib suite was replayed on both SDKs after integration:
**974 inputs**, **48 outputs** and five generated profiles pass on each host.
The unchanged default history is 32 KiB; optional callers explicitly choose a
sufficient window. Native provider workspace/process memory, broad cumulative
contexts and C-Z11 remain outside this qualification. Generated Windows output
was executed by the native Linux SDK in private temporary storage, without a
claim of a Linux C++ compiler rebuild for this particular proof.

## Java packages: selected JV-P01 through JV-P15 and JV-P17 through JV-P19

The [Java package proof](verification-java-packages-2026-10-20.md) qualifies
actual Microsoft OpenJDK **17.0.18+8** strict compilation and execution with
**224 decisions**, **nine independent outputs**, **two native C++ exchanges**
and helper/outer-name collision profiles. The core uses only Java 17 standard
APIs. An isolated native Linux **JRE 25.0.4.1** replay executes the same
Java 17-targeted classfiles and 224 decisions; it does not claim Linux javac.

The actual final compiler is `0e494607dd15d6fd12cdabe96271ee51c31647ae5a9a70192e5628cd821d7efa`.
The corpus covers descriptors/ZIP64 boundaries, MCE, MIME whitespace,
ASCII candidate versus unsupported Unicode admission, typed fresh-aggregate
validation, complete preserved extensions, callback reentry fencing, exact
spool I/O admission and publication faults. P16/P20/P21 and larger-than-array
output remain open; fixed-role canonical writer names require explicit
preserved-link rebinding when loaded names differ. The dated proof separates
Windows SDK/filesystem checks from Linux runtime-only qualification.

## Kotlin optional compression: KT-Z09 and KT-Z10

The [Kotlin provider proof](verification-kotlin-compression-providers-2026-10-20.md)
qualifies actual Kotlin **2.2.21** JVM17 compilation without provider jars and
Windows **JDK17.0.18+8** execution with pinned lazy Zstandard/LZ4 runtime jars.
All **317 input decisions**, **24 independent outputs**, **48 C++ consumers**,
**six C++ producers**, **four compiled profiles** and **five deployment cases**
pass against immutable compiler `d25df05e…`. The fifth deployment has the
Zstandard jar present with failed native loading while safe-Java LZ4 remains
available. Admission precedes application callbacks and borrowed I/O.

A separate native Linux **JRE25.0.4.1** replay executes those exact Windows JVM17
classfiles, all 317 decisions, four profiles and five deployments. Its 24 returned
frames also pass independent Python and 48 Windows C++ consumers. This does not
claim a Linux Kotlin compiler or Linux JDK 17. The unchanged core suite passes
974 inputs, 16 outputs and four profiles again after optional-provider integration.
Positional options, ordinary native APIs and wire bytes remain compatible;
provider workspace/RSS, broader cumulative contexts and KT-Z11 remain open.

## External documents and native recovery: selected D01 through D21

The [document-bridge proof](verification-managed-document-bridge-2026-10-20.md)
qualifies the selected C++ native/external-JSON profile on both hosts. It covers
exact baseline binding, IDs and burned allocation marks, labeled tree/history
cursors, coherent publication, session/outbox retention, legacy native behavior
and the new runnable bounded JSON example. Independent main-file inspection
finds only native model projection keys with zero identity placeholders.

A new before/after regression exposed decoder calls on foreign companion
generations and corrupt/truncated first baselines. The localized repair admits
those physical fences before invoking the application decoder, preserving APIs
and bytes. Three focused CTests pass on Windows in 132.10 seconds and native Linux
in 180.25 seconds, including the expanded 364-check abrupt-exit matrix. The Linux
repair snapshot has 2,280 files, aggregate `c7fb6d10…`, compiler `7282fcb1…`. The proof
retains exact hashes and source/configuration scopes. Broader automatic XML/OPC
adapters, universal power-loss guarantees and other native managed engines remain
outside this selected qualification.

## C# optional compression: CS-Z09 and CS-Z10

The [managed-provider proof](verification-csharp-compression-providers-2026-10-20.md)
qualifies actual SDK8.0.426/C#12/net8.0 compilation and Windows .NET8.0.32 execution:
**317 input decisions**, **24 independent outputs**, **48 C++ consumers**, six
native producers, nine warning-free SDK profiles and four deployment cases.
Exact Windows-generated IL also executes on private Linux .NET8.0.32, with all
317 decisions/profiles/deployments and 24 returned frames passing independent
Python and 48 Windows C++ consumers. No Linux SDK compilation is claimed.

Providers use explicitly selected ZstdSharp.Port0.8.8/K4os1.3.8 managed assemblies.
Compatibility-preserving static option factories avoid constructor ambiguity;
original untyped-default calls remain valid. The unchanged core suite repeats
974 inputs, 16 outputs and five strict profiles against actual combined compiler
`3533d50e...`. Broader operation contexts, AOT and CS-Z11 remain open.

## JavaScript/TypeScript optional compression: JS/TS-Z09 and Z10

The [provider proof](verification-javascript-compression-providers-2026-10-20.md)
qualifies explicit Node Zstandard transforms and caller-supplied lz4js0.2.0 blocks
against actual combined compiler `3533d50e...`. Windows Node22.23.1 and strict
TypeScript7.0.2 pass **317 input decisions**, **18 independent outputs**, **36
C++ consumers**, six native producers and four strict profiles. Callback-close
and provider-cancellation sweeps cover 170 and 168 boundaries respectively.

Native Linux Node24.15.0 replays those exact generated sources and fixtures in
private Linux-local storage, passing the same decisions and runtime profiles.
All 18 newly produced frames independently pass Python and 36 Windows C++
consumers. Generated source bytes are unchanged; consumer import URIs are the
only platform adaptation. Browser-like no-Node LZ4 and older DEFLATE-only Node
controls pass. Original TypeScript literal types and option-property defaults
are preserved. The complete unchanged core974/16/four-profile suite also passes.
JS-Z11/TS-Z11 and broad browser/performance gates remain open.

## Go physical ZIP: selected GO-P01 through GO-P12

The [Go ZIP proof](verification-go-packages-2026-10-20.md) qualifies actual
Go1.26.4 execution with the Go1.22 language baseline and only standard packages.
Selected Windows Go1.26.4 and native Linux Go1.26.0 profiles pass 66 archive
decisions, 54 API/resource/lifecycle
cases, seven independently inspected outputs and eleven overlapping C++ exchange
controls. Large-offset/count boundaries, all descriptor forms, private spooling,
first-failure semantics and staged Save are covered. Collision, binary-only and
XML-independent modules pass; ten other language outputs retain their bytes.
OPC metadata, typed package profiles and automatic nested contexts remain separate.

## Compiler publication preserves unchanged files

The [publication proof](verification-generated-output-publication-2026-10-20.md)
compares the same maintained runner against actual before/after compiler binaries:
five of six tests fail before, and six of six pass afterward with no skips.
Equal prepared outputs and depfiles retain their timestamps across all eleven
languages, including separate C++ children. Actual Ninja restat prunes dependent
compilation after compiler-only regeneration; a schema change still rebuilds.
Source/alias/formatter protections and changed-file write semantics are preserved.
The combined Windows build subsequently completed 390 steps with no C++ object
recompilation. This is no claim of new atomic publication or crash durability.

## Compact scalar design prerequisite: SC01

The [selected contract](../design/compact_scalar_contract.md) defines all five
proposed payloads, including direct nearest/ties-to-even rounding, subnormals,
checked finite overflow, canonical NaN, signed zero and exact scaled parameters.
The [reference proof](verification-compact-scalar-contract-2026-10-20.md) passes
76 vectors, all 131,072 binary16/bfloat16 patterns, 20000 independent conversion
or overflow checks, signed/malformed boundaries and actual compiler rejection
of all five proposed keywords without replacing existing output. This closes
only the numeric-design prerequisite. Current schema 1.20.0 and generated codecs
remain unchanged; syntax and backend implementations require a later feature
release and complete qualification.

The current combined Linux suite also passes all six publication checks without
skips and all Go physical ZIP cases. The Go modules and seven outputs have the
same hashes as the selected Windows run. Actual native Linux generation separately
produces byte-identical JS/TS sources/declarations across all four optional-provider
profiles. The dedicated records retain exact runtime/compiler and source identities.

## Current combined regression gates: selected G01 through G08

The [combined native record](verification-current-regression-2026-10-20.md)
qualifies exact 2,291-file snapshots, actual Windows compiler 3533d50e and native
Linux compiler b4dddfa3. The complete final Windows wrapper passes **212/212** in
728.06 seconds; native Linux passes **209/209** in 401.22 seconds. Provider-free
Windows passes **13/13**, and provider-free native Linux **12/12**. The Windows
final runner adds explicit UTF-8 text handling; no further runtime fix was needed.
Selected independent XML/XSD/ZIP/OPC oracles, version/discovery compatibility and
synchronized validated editor 2.0.7 packages qualify G01 through G08. This does not
claim every 121 language-pair exchange, full OPC conformance or unexecuted SDK
combinations. Broader gates stay open. Later optional Rust, Go OPC and Swift work
must qualify its own source deltas rather than inherit these frozen results.

## Rust optional compression: RS-Z09 and RS-Z10

The [provider proof](verification-rust-compression-providers-2026-10-20.md)
qualifies actual compiler cdaa775d emission with Windows Rust/Cargo 1.99 and native
Linux Rust/Cargo 1.93.1 offline compilation/execution. Each passes 317 input decisions,
18 independent outputs, 36 C++ consumers, six native producers, five SDK profiles,
four feature deployments and six compile-failure lifetime controls. Original
exhaustive options/default constructors remain compatible; ordinary helpers need
only rustc/std. Both hosts repeat the unchanged 974/64/three-profile core suite.

Borrowed per-operation providers create checked progress contexts; generated code
owns exact framing, output caps and callback identity. The pinned LZ4 adapter uses
safe blocks and fast level 0; Zstandard supports its full advertised level interval.
Native code/prefixes remain unchanged. The exact 708-file Linux inventory and
private Cargo-only tool deployment are retained in the dedicated proof. Broader
RS-Z11, other providers and performance gates remain separate.

## Swift optional compression: SW-Z09 and SW-Z10

The [provider proof](verification-swift-compression-providers-2026-10-20.md)
qualifies actual compiler e4ef04ca output on Windows Swift 6.4 and native Linux
Swift 6.1.3/GCC 15.2. Each host passes 317 shared frame decisions, 24 independently
checked outputs, 48 C++ consumers, five profiles, 62 cancellation and 72 callback/
close boundaries. The unchanged standalone core runner also passes 974 inputs,
64 outputs and three profiles against the new actual compiler on Windows.

Explicit providers create private checked contexts without changing original
initializer signatures or default availability. The reference C adapter uses
Zstandard 1.5.7 and LZ4 1.10.0. Reentrant cleanup waits for the active native step;
its error implementation stays inside the collision-selected helper namespace.
The exact 650-file Linux SDK replay input and native library identities are
retained; its private workspace was removed. Other integration and performance
gates remain separate.

## Python XML measurement gates: selected PY-Q05 through PY-Q07

The [measurement record](verification-python-xml-performance-2026-10-20.md)
qualifies actual compiler 309eb574 with Python 3.14.3/Expat 2.7.3 on Windows.
Three fixed owning workloads retain five wall/CPU samples per operation, separate
traced allocation, exact XML output hashes, independent ElementTree exchanges,
all four native protocol baselines and generated-source sizes with XML on/off.
The largest selected XML output is 1,619,122 bytes; its encode/decode traced peaks
are 44,446,919 and 4,772,109 bytes. Native provider workspace/RSS is excluded.
These measured observations do not close broad performance/platform gates or
assert an isolated machine regression threshold.

The same runner also passes actual final combined compiler 99380733. All XML
and ordinary native output hashes and all traced peaks are unchanged. The bounded
zlib-history helper adds 7,514 source bytes to both profiles, preserving the
165,341-byte XML support difference. The dedicated record retains final source
hashes and all timing samples; concurrent compilation explains the larger
wall/process-CPU separation.

## C++ XML measurement gates: selected CP-Q05 through CP-Q07

The [C++ measurement record](verification-cpp-xml-performance-2026-10-20.md)
qualifies actual final compiler 99380733 with MSVC 19.51 C++20 and LibXml2 2.15.3.
Three owning workloads pass 54 isolated operation processes and retain 270
wall/CPU samples, exact source/encoded sizes, independent ElementTree checks and
identical native bytes with XML enabled/disabled. The native-only executable
imports no XML DLL. Generated headers contain 25,539/19,088 bytes respectively.

Windows OS process peak residency is recorded before warmup and after measurement;
it includes setup and native libraries and does not isolate operation allocation.
The largest selected process peak is 34,267,136 bytes. Complete owning models and
concurrent compilation remain explicit limits; broad performance/platform gates
remain open.

## Eleven-language XML exchange and native independence

The [exchange record](verification-xml-exchange-2026-10-20.md) qualifies selected
Q01/Q03/Q04 for all eleven language prefixes. Actual compiler 99380733 and
frozen native helpers pass 121 configured and 121 fixed-XSD producer/consumer
exchanges, with 264 independently checked expanded-name XML documents. All
eleven XML-disabled SDK profiles pass four native protocols and reject the
selected unsupported future version. C/C++ import no XML DLL; Java/Kotlin's
native executions use only `java.base`. Missing-SDK controls fail explicitly.

The report retains 350 commands, exact tools/sources/outputs and locked offline
dependencies. This is a small selected document under two mappings; uniform
Q02 malformed/resource/cancellation gates, broader vocabulary, other platforms
and unmeasured performance remain open.

## Go OPC: selected GO-P13 through GO-P15 and GO-P17 through GO-P19

The [OPC proof](verification-go-opc-packages-2026-10-20.md) qualifies actual
Windows compiler 309eb574 and native Linux compiler df7b20c8 with Go 1.26 SDKs
under the Go 1.22 language baseline. Each passes 143 independent fixtures at
both metadata and typed/policy layers, 12 additional SDK groups/29 named
boundaries, eight independent outputs and two typed C++ exchanges. Generated
profile and archive hashes match across hosts. Meaningful before-controls
reproduce two MIME/MCE scratch-admission defects; both pass after repair.

Typed loading is transactional, unknown capability policies are explicit,
edited preserved graphs are revalidated and canonical ordering survives input
permutations. Save fault controls cover all seven exposed boundaries. Native
XML still has independently narrowed limits; GO-P16/P20/P21 remain open.
The frozen pre-C++-repair native Linux full suite also passes 210/210 in
391.92 seconds, separately from these selected SDK results.

## Final shared compression and editor follow-up

The [all-output compression record](verification-compression-parity-2026-10-20.md)
supersedes earlier selected-checkpoint Z11 limitations in this log. All eleven
outputs pass the literal shared framing/error/limit corpus: **15,400 decisions**,
**2,904 owning exchanges**, **264 independently decoded frames** and **792 encode
bound checks**. Seventeen maintained core/optional SDK suites pass. Ten generated
SDK executions use the sealed `99380733` compiler snapshot. The separately
captured repaired C++ `978a9afe` CLI and matching runtime pass 1,400 decisions,
264 exchanges and 25 lifecycle controls; all 24 C++ producer frames are unchanged.
The refresh verifies unchanged ten-SDK emissions, complete baseline source,
resource/provider deployment seals and the actual refreshed native library,
headers and reachable provider DLLs. The original sole Zstandard window-limit
failure is retained. These close selected `CP-Z11` through `C-Z11` and `XML-B16`;
automatic nested XML/OPC ledgers, arbitrary provider/platform combinations and
performance remain separately scoped tasks.

The [editor record](verification-editor-field-navigation-2026-10-20.md) qualifies
both **2.0.8** packages and the actual native resolver extracted from the VSIX.
Request-local prepared source masking fixes inherited-field resolution that
exceeded the existing 256 MiB Jint limit; the limit remains unchanged. The
packaged resolver passes the complete 1,437 native controls, including fresh
inherited-field and package checks. The shared Node suite passes 43,422 checks
and the editor unit suite passes 154 tests. Package validation and synchronized
manifests, listing payloads and changelogs close the latest `G08` follow-up.
No package was installed or published.
