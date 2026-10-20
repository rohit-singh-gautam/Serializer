# Kotlin optional compression providers, 20 October 2026

[Kotlin compression API](../languages/kotlin_compression.md) ·
[Core SDK proof](verification-kotlin-compression-2026-10-20.md) ·
[Compression contract](../serialization/compression.md)

This record qualifies **KT-Z09/KT-Z10** for the selected optional-provider profiles
in compiler/runtime **2.14.0**, schema language **1.20.0**, on baseline
`0928053389fcf237e53f682442510ae19ef5eb5c` plus working-tree changes. Actual
Windows-generated Kotlin targets JVM 17 and runs on Windows JDK 17 and native
Linux JRE 25. Broader **KT-Z11** operation-context parity, other platforms and
performance gates remain separate.

## Providers and compatibility

Generation and default compilation require no optional jars. Reflection resolves
the selected runtime classpath lazily; no provider or native library is installed
globally. The pinned public APIs and source jars are the same independently
qualified providers used by the [Java adapters](verification-java-compression-providers-2026-10-20.md).

| Provider | Pinned runtime artifact | SHA-256 |
| --- | --- | --- |
| Zstandard | `com.github.luben:zstd-jni:1.5.7-16` | `d84360d39f57a40dc1e26ea2d4bb66da0cb88057ba117edae67431a719057edc` |
| LZ4 | `at.yawk.lz4:lz4-java:1.12.0` | `31c287041eab41f2459e93659d49162eff015a0eb858877e1c5e9dcce1a10c26` |

The [official Zstandard provider](https://github.com/luben/zstd-jni/tree/v1.5.7-16)
supplies native contexts, pledged sizes, checksums, streaming buffers and history
limits. The [maintained LZ4 provider](https://github.com/yawkat/lz4-java/tree/v1.12.0)
supplies safe pure-Java block and XXHash APIs; no native LZ4 deployment is needed.
Zstandard requires its jar's platform-native deployment.

The existing five-field positional `EncodeOptions(format, maxInputBytes,
maxOutputBytes, level, checkCancelled)` and `DecodeOptions` APIs remain compatible.
Zstandard defaults to level 3 and accepts its full advertised integer interval,
including negative fast levels. LZ4 defaults to fast level 0; levels 1–9 select
high compression. Existing DEFLATE defaults remain level 6. Optional outputs
always include a content checksum; readers also accept standard unchecked frames.
The ordinary model names, native APIs, protocols and uncompressed bytes are unchanged.

## Actual Windows SDK execution

The maintained [optional runner](../../test/kotlin_compression_optional_test.py)
invokes immutable actual compiler SHA-256
`d25df05ef2775704995c219c8516a53369520787c1a5112c46d5e74eae635be6`,
then compiles its emitted modules with **kotlinc-jvm 2.2.21**, using
`-Werror -jvm-target 17 -include-runtime`, without provider jars on the compiler
classpath. Runtime execution uses **Microsoft OpenJDK 17.0.18+8-LTS** on Windows.
The final complete maintained replay exits zero with these results:

| Check | Actual result |
| --- | --- |
| Independent input decisions | **317/317**: 136 Zstandard and 181 LZ4 |
| Independent output checks | **24/24** Python-provider decisions preserve complete input and exact EOF |
| Native interoperability | **6** real C++ producer fixtures and **48** C++ whole-message/stream consumer checks pass |
| Native protocols | JSON, BINARY_NONE, BINARY_INTEGER and BINARY_STRING pass owning callbacks and inner-suffix rejection |
| Generated SDK profiles | **4/4**: normal, preserved helper/model collision, provider-type name collisions and binary-only |
| Deployments | **5/5**: both absent, Zstandard only, LZ4 only, both present and Zstandard jar with failed native deployment |
| Borrowed lifecycle | Bounds, staging, finish/close, cancellation, callback close/reentry and first-error preservation pass |

A separate JVM invocation with the Zstandard jar present and
`-DZstdNativePath=<nonexistent task file>` also passes unavailable admission before
policy, codec, source or sink callbacks, while LZ4 remains available. The final
maintained replay includes this fifth deployment check. Its source SHA-256 is
`b992f939e7da9294b010091b7b0e018e384914a752e42830c5cc910235bd7b07`.

Inputs include standard checked/unchecked frames, empty and multichunk data,
truncation, concatenation, trailing bytes, dictionaries, skippable/legacy framing,
size/checksum failures, exact and one-tight byte/window limits, all LZ4 block
maxima and independent/linked raw or compressed blocks. Zstandard controls include
full level-range admission, descriptor bits, content-size forms and conservative
non-power-of-two window ceilings. LZ4 controls include eleven independently
validated history-prefix boundaries and native linked-block producers.

The runner reuses actual owning model and borrowed lifecycle controls for both
formats. It polls cancellation at every observed optional-provider boundary and
requires the exact original checked exception object, including when a provider
wraps it. No candidate is published after late decoder cancellation. Independent
outputs cover all four native protocols and empty/multichunk data at levels
0/1/6/9. Python **3.14.3** uses stdlib Zstandard **1.5.7** and python-lz4 **4.4.5**
with liblz4 **1.9.4**. C++ interoperability uses the existing MSVC Release helpers
with Zstandard **1.5.7** and LZ4 **1.10.0**.

The unchanged [core runner](../../test/kotlin_compression_test.py) also passes
against this compiler without optional jars: **974/974** input decisions,
**16/16** independent outputs and **4/4** generated profiles, including ordinary
model, bounds, lifecycle and cancellation controls.

## Native Linux JRE replay

The four actual Windows-produced Java 17 classfile jars, fixture bytes and exact
provider jars were hash-checked and streamed into a fresh isolated Linux directory.
No Windows mount or shared native build cache is used. **OpenJDK 25.0.4.1** on
Ubuntu 26.04 passes **317/317** decisions, the normal and three additional SDK
profiles, and all five deployment cases, including missing native Zstandard.
The 24 Linux-produced frames pass independent Python checks and **48** Windows
C++ whole-message/stream consumers after being streamed back.

This qualifies Linux runtime execution of Windows-generated JVM 17 classes;
Linux Kotlin compilation and JDK 17 execution on Linux are not claimed.
The 737-file input inventory aggregate is
`2572c0e5d15683cea6086bd0a6566a066fdc356e236af9d6d368c3f66d1f3f60`.
The private Linux staging directory is removed after returning logs and outputs.
All classfile entries in the four replayed jars and their `Schema.kt`/`Main.kt`
sources are byte-identical to the final Windows replay. The retained class-entry
comparison distinguishes immutable Linux inputs from jar container timestamps.

## Framing and resource boundary

Zstandard output pledges the complete known input size. Input independently
checks one standard frame, header, dictionary absence, content size, window,
block extents and exact EOF before native expansion. The native history cap rounds
down to a power of two, with a 1 KiB minimum. Owned native contexts close on every
outcome; known frame/window errors receive the corresponding compression category,
while unrelated provider and application failures retain their types.

LZ4 output uses 64 KiB independent blocks. Input verifies descriptor, block and
content checksums, counts regenerated lengths/back references before allocating
output, and admits the declared 64 KiB–4 MiB block maximum against the window cap.
Linked decoding retains at most 64 KiB and merges that history into the first
literal run for the safe block decoder. Streaming content-hash states are closed
on success and failure.

Borrowed transport, Zstandard provider buffers and streaming checksum updates use
at most 32 KiB batches. LZ4 block calls use the admitted block maximum plus at most
64 KiB of history. Complete messages are staged. Logical input/output/history caps
exclude provider workspace, allocator capacity, caller codec allocations and RSS;
this proof makes no hard heap, CPU or performance claim. Cooperative cancellation
cannot preempt a blocking provider or application callback. Wrappers remain sequential.

## Provenance and reproduction

| Input | SHA-256 |
| --- | --- |
| `src/kotlin_compression_runtime.inc` | `6c2c54dcafe3d593207f133c5751c0166c8103226b1f22e92234d297644e9154` |
| `src/kotlin_zstd_compression_runtime.inc` | `55e84a6719c36d06b3722ea37247fafd621046061cb2470b8c9d902e6da4d392` |
| `src/kotlin_lz4_compression_runtime.inc` | `c5a30c344b98c49c4079a914470cb7502a36031ca236e4cb2bb2e05de0b1ec6c` |
| `src/mobile_writer.cpp` | `7022cdaf8bfc88cf539b31644797f21903fe7dce5cde4918ad71b2f1adfa90df` |

The normal emitted Kotlin fixture is **90,877 bytes**, SHA-256
`f6d4d698f0699befcb38836121718d437a598de989842005c72e83b04f28e27c`.
The source grows because the additive helper is emitted even when unused. The
representative Swift module at the same input/output paths remains byte-identical
before/after this Kotlin-only integration: **66,461 bytes**, SHA-256
`cf60046c56a7ae805027aa515e95726ae2584ee2d3ef2dbbdd04f9cd93b098b8`.
That comparison qualifies one owning Swift profile rather than every output.
The global MSVC runtime-literal limit check passes.

```powershell
python -B test/kotlin_compression_optional_test.py `
  --compiler path/to/serializer.exe --build out/kotlin-optional `
  --kotlinc path/to/kotlinc.bat --java path/to/jdk17/bin/java.exe `
  --zstd-jar path/to/zstd-jni-1.5.7-16.jar `
  --lz4-jar path/to/lz4-java-1.12.0.jar `
  --provider-path out/python-compression-provider `
  --cpp-helper path/to/compression_interop.exe `
  --cpp-linked-helper path/to/linked_lz4_helper.exe
```

Explicitly requested SDKs, providers and native helpers are mandatory; their
absence fails rather than reducing the selected checks. Retained evidence under
ignored `out/build/backlog-2026-10-20/` includes `kotlin-optional-actual/`,
`kotlin-optional-final.log`, `kotlin-optional-native-failure.log`,
`kotlin-core-provider-update/verification.json`, `kotlin-core-provider-update.log`,
`kotlin-optional-linux-final/`, `kotlin-optional-linux-final.log` and the immutable
`compiler-editor-kotlin-d25df05e/` snapshot. The Linux report and inventory retain
the exact classfile inputs separately from the maintained Windows runner source;
`final_class_equivalence.json` confirms the final Windows class-entry equality.
The earlier complete report is preserved as
`kotlin-optional-actual/verification-before-native-admission.json` rather than
being substituted for the final missing-native-inclusive result.
