# Compiler/backend qualification, 24 October 2026

This record qualifies the selected SC02, SC03, BE01, BE02, BE03 and OPC-OPT02
contracts in compiler/runtime 2.34.0, language 1.24.0 and matching editors 2.0.19.
It does not qualify unrelated pending sections or every provider/platform combination.

See the
[implemented contracts and limitations](../guides/compiler_backend_updates.md).

## Hosts and configuration

Windows x64 10.0.26300 uses MSVC 19.51 (14.51 toolset), CMake 4.4.2, C++23,
RelWithDebInfo and the existing vcpkg XML/compression providers. Native builds and
CTest suites run sequentially with one requested worker. Installed-consumer tests
retain their existing two-worker sub-builds. Restored cache paths were corrected locally;
`VCPKG_MANIFEST_INSTALL=OFF` reuses the existing dependencies. An MSVC PDB service
failure was avoided with `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` in the
ignored build cache and `/Z7` for installed consumers. No build-wrapper defaults changed.

Linux x86_64 under WSL uses GCC/libstdc++ 15.2.0 and CMake 4.2.3. The root
Makefile builds Debug with `-O0 -g0`, `JOBS=1`, tests/managed output disabled and
XML/Zstandard/LZ4/zlib disabled. Generated consumer and mapped-view probes use
`-std=c++23 -O2 -Wall -Wextra -Werror`. This independently compiles the compiler,
runtime and non-Windows signature fallback; it is not an XML-ON Linux replay.

## Executed contracts

| Requirement | Executed checks |
| --- | --- |
| SC02 | Windows core suite: strict scalar defaults, language/width gates, malformed and overflowing widths, all four native bit-field codecs matching ordinary scalar bytes, decoder range rejection and directional compatibility. Linux-generated headers compile and replay all four bit-field formats with GCC. Unsupported backend/storage/address profiles reject explicitly. |
| SC03 | Generated C++ exact uint64 calls, member calls, common arithmetic returns and independent empty/scalar/factory input defaults compile and execute with MSVC and GCC. Older language and inconsistent/untyped/constraint-invalid calls reject. Existing fixed-array Protobuf suite passes; Google's runtime independently exchanges packed/unpacked binary, ProtoJSON and TextFormat messages and rejects cardinalities 0/1/2/4 against the fixed extent 3. |
| BE01 | Actual C, Rust, Python, Swift and Kotlin SDKs compile/run separate dependency bundles, maintained construction/default consumers and native wire round trips. Depfiles enumerate current artifacts, authored string defaults survive visibility changes, and the old single-string/inline API remains available. |
| BE02 | Java stream/channel outputs match existing bytes in all four native formats, including 1 MiB payloads, nested size patches and readonly projections. Partial seven-byte channel writes, no-progress channels, original IOException identity and sink ownership pass. Fixed-scalar packed heap/direct windows preserve byte order/cursors and match positional output; readonly/short windows reject. Existing caller-owned array/NIO regression passes. |
| BE03 | Real Windows and POSIX shared mappings exercise an unaligned message, borrowed strings/arrays, readonly aliases, persisted size-preserving edits, failed length changes and input bounds. Allocation/timing results appear below. |
| OPC-OPT02 | Native and independent WindowsBase/.NET producers and validators exchange signed packages. Payload/content-type/root-relationship tampering and wrong pinned certificates reject; missing required coverage rejects. Explicit removal yields unsigned input; resigning validates independently. Linux reports unavailable and rejects verification. |

Windows `core_serializer_test` passes **636 tests in 77 suites**;
`protobuf_codec_test` passes **27 tests in four suites**. The first focused seven
CTest entries pass together in **28.80 seconds**, including Java, Python bundles,
mapped views and independent OPC signatures. The final Protobuf SDK driver checks
the cardinality diagnostic so an unrelated process failure cannot count as rejection.
Fixed arrays require exact presence/cardinality unless explicitly omitted by the
selected format; omitted defaults retain their separately tested schema contract.
An added SDK control initially expected an absent required array to retain defaults;
it correctly failed exact cardinality. The final driver checks this as a negative
case and passes **36 independent exchanges/cardinality decisions**.

`serializer_package_version`, the existing installed-package test (**195.10 seconds**)
and Java 17 replay pass. A separate installed native consumer finds 2.34.0 through
a 2.33 package request, links the new signature API and exported system dependencies,
and verifies metadata plus explicit invalid-input failure. Its local vcpkg configuration
uses `RelWithDebInfo;Release;` imported-config fallbacks and the dependency DLL directory
on PATH; an initial unmapped LZ4 debug-DLL load failure is outside the final passing run.

Native SDKs: MSVC C, Rust 1.99.0, Windows Python 3.14.3, Swift 6.4 (Windows x64),
Kotlin/JVM 2.2.21 on Microsoft OpenJDK 17.0.18+8. Java streaming is compiled with
`--release 17 -Xlint:all -Werror` and replayed on JDK 17.0.18; an initial SDK check
also passed javac/JVM 26. Independent Protobuf uses Google `protobuf` 5.29.6.
Node 22.23.1 runs the editor checks with one requested test worker.
The final Windows/Linux Python replay (Linux Python 3.14.4) also imports the entry through a containing package;
its runtime/dependency modules resolve within that package without changing sys.path
to the entry module's own directory.

## Mapped workload and measurements

The workload has a 64 KiB label and 16,384 uint32 values, producing **131,082
bytes**. Each measurement performs 100 generated view map/traversals or 100
owning exact decode/traversals. Opening/closing the OS mapping is outside the
timed region. A zero-allocation assertion covers ordinary current-thread heap
allocations in generated mapping/traversal, not OS resources or process peaks.

| Host | Mapped time, microseconds | Owning time, microseconds | Mapped allocations | Owning allocations | Owning requested bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Windows MSVC | 11,484 | 8,358 | 0 | 200 | 13,116,600 |
| Linux GCC | 7,041 | 1,822 | 0 | 200 | 13,107,300 |

Both report a 120-byte view object and checksum 13,420,953,600. Owning decoding is
faster for this workload. These single-run observations establish no universal
speedup, throughput threshold or process peak. Owners must outlive borrowed views;
unsupported formats/endianness/platforms remain outside the mapping contract.

## Editor and package scope

The shared grammar/navigation suite passes **123 tests**. Focused fresh generation
passes **80 bidirectional native bundle boundaries** with only entry outputs in
the workspace inventory and transitive schema includes, plus **1,943 ordinary
type/field checks across all eleven output languages**. Visual Studio's actual
Jint adapter passes **30 forward/reverse native bundle checks** under its existing
256 MiB interpreter limit, including captured unsaved schema contents. The checks
found and fixed discovery of compiler-named fragments absent from basename inventory.

Both editor packages are rebuilt through `./make.ps1 extension`; Visual Studio's
package validator checks embedded assets and identity. Package contents retain
the matching 2.0.19 versions. They are not installed or published. Interactive
editor-host qualification remains QA04 in the separate qualification branch.
The broad existing generated-navigation driver was interrupted during unrelated
package-application checks; the maintained `--compiler-backends-only` replay above
passes. Do not infer completion of that broader matrix.

## Reproduction and retained output

Use the Windows root configure wrapper with the local cache options above, then
build `serializer core_serializer_test protobuf_codec_test protobuf_sdk_probe
mapped_view_platform_test opc_signature_probe` with `--parallel 1`. Run the
matching focused CTest entries, `serializer_java_output_buffer`,
`serializer_package_version` and `serializer_installed_package` sequentially.
The maintained native bundle driver accepts `--languages c,rust,python,swift,kotlin`;
set `SERIALIZER_KOTLINC` to the installed Kotlin compiler. Swift uses `-j 1` and
JVM processes use `-XX:ActiveProcessorCount=1`.

Run `node editors/vscode/test/generated_navigation.cjs --compiler-backends-only`
with `SERIALIZER_COMPILER` set. Build the Visual Studio navigation test project
and pass repository root, generated ordinary output directory and
`--compiler-backends-only`. No extension host or external communication is needed.

Final logs, useful generated consumers, local SDK dependencies and editor packages
remain under ignored `out/build/compiler-backends*`, `out/extension-tests` and
`out/extensions`. Linux build/probe output remains under
`/tmp/serializer-compiler-backends-qualification`. Private test PFX files are
removed by the signature driver. macOS/ARM, other signature algorithms/trust
policies, XML-ON Linux signatures and broad provider/resource/performance matrices
are not qualified by this record.

Final package SHA-256:

- `serializer-vscode-2.0.19.vsix`: `101cbaca76b920859198815154b8361b7730f8c9115564fcbbdd2f0ab2b309b3`
- `serializer-visual-studio-2.0.19.vsix`: `8b5a096f13fa1eb26316a151b88d25296ac6ff2155d36771dea0b6cc16ac3ad8`
