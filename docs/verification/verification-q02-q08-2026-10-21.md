# XML malformed-input and Windows/Linux replay, 21 October 2026

[Malformed/provider runner](../../qualification/xml_q02_qualification.py) ·
[Platform replay](../../qualification/xml_platform_replay.py) ·
[Swift packages](../languages/swift_packages.md) · [Backlog](../pending.md)

This record covers the nine Java, Kotlin, C#, Go, JavaScript, TypeScript, Rust,
Swift and C **Q02/Q08** checkpoints for compiler/runtime **2.17.0**, schema
language **1.21.0**. It records actual SDK execution; absent tools, configuration
failures and expected negative cases do not become successful SDK results.
C++ and Python checks are recorded in the
[completed batch report](verification-pending-105-2026-10-21.md).

The final compiler SHA-256 is
`20db0258097c1b5676720186c2b6d7ed2b19d3e8dc9d5a70683f7909aa82258d`.
`q02-tenth/verification.json` and both final Q08 reports use this immutable
compiler. Q08 regenerates all selected sources, rebuilds changed SDK inputs,
and replays previously executed images only when their generated inputs match
exactly. Complete source sets bind TypeScript's declarations and its shared
JavaScript runtime together. Final exact-source resumes check those sets and
execute the SDK consumers again. Retained evidence includes compiler, runner,
generated-source, output and log hashes, exact commands and exit codes.

## Q02: malformed input, explicit providers and interruption

`q02-tenth/verification.json` records **eleven successful maintained corpus
executions**: three prepared-provider groups and eight compression drivers,
completed in 355.528 seconds on Windows. JavaScript and strict TypeScript share
the generated JavaScript runtime rather than counting its decisions twice.

| Actual backend | Independent frame input decisions | Independently decoded outputs |
| --- | ---: | ---: |
| Java | 974 | 16 |
| Kotlin | 974 | 16 |
| C# | 974 | 16 |
| Go | 126 | 16 |
| JavaScript / strict TypeScript | 974 | 16 |
| Rust | 974 | 64 |
| Swift | 974 | 64 |
| C | 974 | 48 |

The standard-frame corpora use independent raw-DEFLATE, zlib and gzip producers
and consumers, checksums, malformed/truncated headers and trailers, forbidden
dictionaries, exact compressed extent, output/ratio/work limits and corrupted
payloads. The actual generated adapters also exercise bounded callbacks,
transport failures, cooperative cancellation/deadlines, reentry rejection and
terminal lifecycle behavior. Per-driver reports preserve their own exact
decision counts and configurations; Go's selected corpus has fewer cases.

Prepared XML tests separately execute real immutable provider instances:

| Backend | Executed provider |
| --- | --- |
| Java / Kotlin | JAXP schema validation through the installed JVM |
| C# | `System.Xml` prepared schema set |
| JavaScript / TypeScript | Pinned `libxml2-wasm` 0.7.1 |
| Go / Rust / Swift / C | Actual LibXml2 2.15.3 prepared-provider bridge DLL |

These tests validate generated owning input/output before publication, reject
wrong roots/namespaces, malformed messages and invalid schema values, enforce
configured message/depth/node/text bounds, and reject file/HTTP schema
include/import/redefine/override acquisition. They exercise concurrent reuse,
provider mismatch, disposal and use after close where the provider API exposes
those operations. Schema-location hints remain inert. No mocked provider or
stub validation callback substitutes for these selected provider checks.

The provider checks and cancellation checks establish their respective public
contracts. They do not establish hard provider-internal allocation ceilings,
asynchronous preemption inside provider calls, or uniform cancellation APIs
across unrelated external providers.

## Q08: actual platforms and reproducibility

Windows execution uses **Windows 11 amd64 build 26300**. Linux execution uses
actual **Linux x86-64** SDK processes under **WSL2**, kernel
`6.18.54.1-microsoft-standard-WSL2`; the Windows compiler generates the source
that those Linux SDKs compile and execute. WSL transport does not substitute
Windows binaries for Linux consumers. This selected qualification adds no
macOS, ARM, other operating-system, or performance claim.

| Tool | Windows execution | Linux execution |
| --- | --- | --- |
| Java | JDK 26 | OpenJDK 25.0.4.1 |
| Kotlin | 2.2.21, Microsoft JRE 17.0.18 | 2.2.21, OpenJDK 25.0.4.1 |
| C# | .NET SDK 10.0.402, `net8.0` | .NET SDK 8.0.426, `net8.0` |
| Go | 1.26.4 | 1.26.0 |
| JavaScript / TypeScript | Node 22.23.1, npm 11.18.0 | Node 24.15.0, npm 11.12.1 |
| TypeScript compiler | 5.9.3 | 5.9.3 |
| Rust / Cargo | 1.99.0 | 1.93.1 |
| Swift | 6.4, Windows x86-64 | 6.1.3, Linux x86-64 |
| C | MSVC 19.51.36260 | GCC 15.2.0 |
| XML provider for C | LibXml2 2.15.3 | LibXml2 2.15.2 |

The maintained XML example runs both its configured mapping and its fixed
XSD-derived mapping. Every produced/consumed document is independently checked
against expanded names, attributes, exact escaped Unicode values, child order
and repeated values in the checked-in manifest. Each backend also executes
twice with the same input and must produce identical bytes. Corresponding
Windows/Linux generated source and output bytes must match before cross-host
exchange starts.

| Selected mapping | Windows same-host pairs | Linux same-host pairs | Bidirectional cross-host pairs | Repeated-byte checks |
| --- | ---: | ---: | ---: | ---: |
| Configured XML | 81 | 81 | 162 | 36 |
| Fixed XSD mapping | 81 | 81 | 162 | 36 |
| Total | 162 | 162 | 324 | 72 |

All **648 producer/consumer exchanges** and **72 repeated-byte checks** passed.
Including each producer's initial output, this checkpoint independently checks
**756 XML documents**. Both platforms execute all nine SDKs for both mappings;
no backend is skipped. The Windows and Linux reports retain their initial
actual builds, final source refresh, complete-source verification and replay.
Cross-host consumers run in both directions, including different producer and
consumer languages. The table describes one complete final matrix; additional
historical and provenance repetitions do not inflate those counts.

This is functional interoperability and repeatability of the selected owning
profiles. It does not measure throughput, peak memory, constant-memory
streaming, provider-internal resource isolation or every optional provider
combination.

## Swift package acceptance

The [Swift package runner](../../test/swift_package_test.py) strictly compiled
three complete generated applications using **Swift 6.4 on Windows x86-64**,
with `-O -warnings-as-errors`. `swift-package-ninth/verification.json` retains
those actual SDK commands and successful executions. Fresh final-compiler
generation reproduces all three source files byte-for-byte; exact commands
and SHA-256 comparisons are retained in
`swift-package-tenth-generation/verification.json`. No hand-edited generated
source substitutes for the compiler output.

Acceptance includes 32 independently authored ZIP fixtures, 14 invalid OPC
fixtures, 15 shared application fixtures, eight independently inspected ZIP
output forms, collision-selected helper names and per-role storage selection.
It exercises source/sink faults, CRC/extent checks, ZIP64 entries and directories,
all descriptor forms, spooling cleanup, staged Save, unknown-part preservation,
matching/equivalent-prefix/whitespace `xsi:type`, immutable root/type dispatch,
and same-model references across roles. Canonically equivalent Unicode scope,
root and type spellings remain distinct when their UTF-8 identities differ.

Shared-limit controls assert the exact six physical/key/request admissions in
the application graph and fifteen physical/XSD-tuple admissions in the identity
graph. Exact and one-tight reference/work/allocation limits, final-resolution
work, authored part/member/link locations, local XML quotas and original caller
errors are checked before publication. A missing native XSD keyref rejects the
whole aggregate. These are logical admission contracts rather than measured
process-memory ceilings.

This package acceptance is a Windows SDK result. The separate Linux Q08 results
cover the selected owning XML profiles, without asserting a Linux or macOS
package acceptance run.

## Reproduction and retained evidence

Final retained report identities are:

| Report under `out/build/pending-105-2026-10-20` | SHA-256 |
| --- | --- |
| `q02-tenth/verification.json` | `11af450d79bcc5e1da030b8f13899a8926da84cacd0200c9ec9f2ade5b485022` |
| `q08-windows/verification.json` | `14d8e15c758b30b2284b205e71aadc722fc4c5eeee4a91bb5dbd027260c10011` |
| `q08-linux/verification.json` | `d689e1c3fd715e5a2a55ba5cbacc9bc7739f96e43afe981f0262425b4341d56e` |
| `swift-package-ninth/verification.json` | `d42f47a6e82c74f54416652c5e6d1903461f17fbeda4055aa695feed4cff469e` |
| `swift-package-tenth-generation/verification.json` | `1bf23576ae6ae6999d7deea5f66bf1a59951a1fc34e588d481bfce80e61e150b` |

Run these commands from the repository root in an MSVC x64 developer
environment. The paths below identify the actual retained task evidence;
another checkout can supply its own compiler, provider prefix and SDK paths.
The task directory retains its 20 October creation date across the overnight
execution. All generated sources, caches and logs remain beneath ignored
`out/`; the runners do not normalize or rewrite the tracked source tree.

```powershell
$task = 'out/build/pending-105-2026-10-20'
$compiler = "$task/compiler-tenth/serializer.exe"
$prefix = 'out/build/ReleaseWindows/vcpkg_installed/x64-windows'
$kotlin = 'out/tools/kotlin-2.2.21/kotlinc/bin/kotlinc.bat'
$cargoCache = 'out/build/pending-qname-2026-10-20/native-namespaces/cargo-home'

python -B qualification/xml_q02_qualification.py --compiler $compiler `
  --build "$task/q02-tenth" --group all `
  --bridge out/build/ReleaseWindows/test/xml_provider_bridge.dll `
  --libxml2-prefix $prefix --kotlinc $kotlin `
  --node-dependencies "$task/q02-js-provider/node_modules" `
  --cargo-home $cargoCache --jobs 3

python -B qualification/xml_platform_replay.py --compiler $compiler `
  --build "$task/q08-windows" --host windows `
  --libxml2-prefix $prefix --kotlinc $kotlin --cargo-home $cargoCache `
  --npm-cache "$task/q02-js-provider/npm-cache"

python -B qualification/xml_platform_replay.py --compiler $compiler `
  --build "$task/q08-linux" --host linux --kotlinc $kotlin `
  --cargo-home $cargoCache `
  --linux-cargo out/tools/linux-cargo/cargo-1.93.1-x86_64-unknown-linux-gnu/cargo/bin/cargo `
  --linux-dotnet out/tools/linux-dotnet/dotnet `
  --linux-javac out/tools/linux-jdk/sdk/usr/lib/jvm/java-25-openjdk-amd64/bin/javac `
  --compare "$task/q08-windows/verification.json" --jobs 4

python -B test/swift_package_test.py --compiler $compiler `
  --build "$task/swift-package-reproduction"
```

An existing passed same-host run accepts `--refresh` to regenerate with a new
compiler. It reuses a previously executed image only when the complete emitted
source is byte-identical; changed source is rebuilt. `--resume` instead requires
the exact same compiler and verifies the retained source/output hashes before
executing repetitions and any requested cross-host consumers again. Complete
TypeScript identity includes both `schema.d.mts` and `schema.mjs`; an older
TS-only report without a runtime hash cannot silently reuse its image. Failed
or partial reports cannot be resumed as passed builds.

The Linux host initially lacked standalone Cargo, a .NET SDK and the JDK's
`javac` launcher/`ct.sym`. Actual matching tool distributions were unpacked
locally under ignored `out/tools`, without system installation or PATH changes:
official Cargo 1.93.1 and .NET SDK 8.0.426 archives, plus Ubuntu's
`openjdk-25-jdk-headless` 25.0.4.1 package. The Java wrapper supplies that extracted
JDK's `java.home`; it compiles the real `--release 17` consumer against its own
`ct.sym`. The platform runner selects these tools explicitly and isolates
mutable SDK caches. Missing tools were configuration failures until those
actual SDKs were available.

The pinned JS parser packages are `@xmldom/xmldom` 0.9.12, `saxes` 6.0.0 and
`xmlchars` 2.2.0. Rust uses the maintained locked `roxmltree` 0.21.1,
`xmlparser` 0.13.6 and `memchr` 2.8.3 caches. No provider dependency was removed
to make the replay pass. Useful local tool archives, generated SDK builds,
command logs and `verification.json` reports are intentionally retained under
the ignored build/tool directories. Unused diagnostic probes and temporary
source overlays were removed; useful generated builds and verification evidence
remain intentionally retained.
