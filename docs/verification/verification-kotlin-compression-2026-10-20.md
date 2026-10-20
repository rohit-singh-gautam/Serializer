# Kotlin compression qualification, 20 October 2026

[Kotlin compression API](../languages/kotlin_compression.md) ·
[Compression contract](../serialization/compression.md) ·
[Remaining work](../pending.md) · [Verification index](README.md)

This record closes **KT-Z01–KT-Z08** for the selected standalone Kotlin/JVM
profile in compiler/runtime **2.14.0**. Identity, gzip, zlib and raw DEFLATE use
standard JDK providers. Zstandard and LZ4 were explicitly unavailable in this
original snapshot. Their subsequent implementation and actual SDK qualification
are covered by the [optional-provider follow-up](verification-kotlin-compression-providers-2026-10-20.md).
Broader **KT-Z11** parity and performance gates remain open.

## Actual SDK execution

The maintained [runner](../../test/kotlin_compression_test.py) invokes the actual
rebuilt C++ compiler and compiles/executes its emitted source with
**kotlinc-jvm 2.2.21**, **Microsoft OpenJDK 17.0.18+8-LTS**,
`-Werror -jvm-target 17 -include-runtime`. The final complete run exits zero.
It uses the standard Kotlin/JDK runtime without package downloads or additional
compression dependencies.

| Qualification | Actual result |
| --- | --- |
| Independent input decisions | **974/974** expected valid/invalid frames agree |
| Independent output checks | **16/16** Python zlib/identity checks preserve native bytes and exact EOF |
| Native protocols | JSON, BINARY_NONE, BINARY_INTEGER and BINARY_STRING pass owning callback roundtrips and unread inner-suffix rejection |
| Actual compiled/executed profiles | **4/4** pass: normal, helper/model collision, new provider-type name collisions, binary-only |
| Borrowed wrappers | Exact bounds/EOF/finish, short reads, failure fencing, cancellation and callback close/reentry pass |

The independent Python-provider corpus supplies 957 gzip/zlib/raw decisions,
including empty/single-byte/binary/random/repeated payloads, short-message
truncations, trailing/concatenated input, optional gzip fields/FHCRC/checksums
and zlib dictionary framing. Seventeen separately authored wire-bit cases add
stored-length complements, reserved symbols, history underflow, dynamic Huffman
alphabet/tree/repeat/EOB failures, empty-distance valid controls and a
distance-257 control under valid/insufficient zlib advertised windows. Handcrafted
valid controls are checked by Python's independent zlib before Kotlin receives
them. Every nonempty valid input also receives exact and one-tight bounds.

The output oracle independently decodes all four formats for each native
protocol, checks standard EOF with no unused data, and compares the exact
uncompressed native bytes. The ordinary JSON field identity is separately
inspected. Native decoder failure and late cancellation do not publish a fresh
candidate.

Additional checks cover provider absence before policy/codec/transport callbacks,
negative limits and levels, mandatory 32 KiB admission, exact/one-tight byte caps,
levels 0/1/6/9, empty messages, cancellation exception identity across observed
provider boundaries, seven-byte short reads, bounded source/sink batches,
one-byte overflow probing, zero-progress rejection, unread expanded input,
explicit finish, abandoned close, borrowed close/flush ownership and transport
failure identity. Wrapper policies remain active after source staging.

All four formats check callback-driven close and swallowed reentrant-operation
failures. Private lifecycle guards check activity before and after policy hooks
and after transport callbacks. Closing or failing an active wrapper prevents
subsequent borrowed calls or successful completion, while preserving the first
original error. No Kotlin production repair was needed after the first actual
SDK execution. The collision harness initially expected a naming-preserve suffix;
its corrected expectation follows Kotlin's existing Pascal naming, and the final
consumer also instantiates/encodes/decodes the preserved model types. That
test-only strengthening followed the complete passing suite; the exact final
collision smoke then compiled and executed in a focused replay, with its separate
log retained. The rest of the runner's qualified checks were unchanged.

## Provider and scope boundary

The helper validates complete DEFLATE grammar, dynamic trees, available history,
actual expanded count and exact byte extent before allocating expanded storage.
gzip and zlib framing/checksums are explicit. The selected raw provider is
[`Inflater(true)`](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/util/zip/Inflater.html);
its private final lookahead byte follows the documented nowrap contract and is
not accepted as an extra frame byte. Private `Inflater` and
[`Deflater`](https://docs.oracle.com/en/java/javase/17/docs/api/java.base/java/util/zip/Deflater.html)
instances are ended on success and failure. The byte extent is established
independently of provider buffering, and dictionaries are unsupported.

Logical input/output/window caps do not bound allocator capacity, caller codec
objects, provider workspace or total process RSS. Whole messages are staged.
Cancellation is cooperative between bounded validation/provider/transport work;
it cannot preempt a blocking call. Wrappers are sequential and unsynchronized.
This record does not qualify other JVM/platform versions, optional compression
providers or a hard CPU/peak-memory/performance gate.

## Provenance and reproduction

The immutable actual compiler snapshot SHA-256 is
`4a4f8604bb76361f2e5f33e20e1b7e5176d6916a1b3ed56d98f7b79675d8d813`.
Frozen source hashes are:

| Input | SHA-256 |
| --- | --- |
| `src/kotlin_compression_runtime.inc` | `92a02da6c3a8fa3e437ab86bdb4a7a0a5a9eb7b36e0dddeeb27cfd71f9f6281a` |
| `src/mobile_writer.cpp` | `22e400a1067a9e8df7493d21111e55bc4d375a72205f8af3b40a27a5035c59c9` |
| `test/kotlin_compression_test.py` | `5e5751858014348c7bbc7c55992a8564aa4d6a880a3edf69e424996a05228e58` |

The normal emitted fixture is **61,369 bytes**, SHA-256
`773b6595b47bb039f0980450f556cd8f4a27fd31d264987cc4951462dd701140`.
Runtime raw literals pass the MSVC 16,380-byte token check. For local SDKs:

```powershell
python -B test/kotlin_compression_test.py `
  --compiler out/build/opc-implementation/serializer.exe `
  --build out/build/backlog-2026-10-20/kotlin-compression-final `
  --kotlinc path/to/kotlinc.bat --java path/to/jdk17/bin/java.exe
```

The runner honors `JAVA_HOME` before PATH, requires actual JDK 17 for this
baseline, and never substitutes a newer runtime qualification. CTest registers
`serializer_compression_kotlin` when `kotlinc` is discoverable; this record reports
the direct complete SDK run, with aggregate CTest replays kept separate.

Retained evidence under ignored `out/build/backlog-2026-10-20/`:

- `windows-kotlin-compiler.log`
- `kotlin-compression-final.log`
- `kotlin-compression-final/verification.json`
- `kotlin-collision-preservation.log` and its focused SDK output directory
- `kotlin-compiler-4a4f8604/`, the immutable compiler/app-local dependency snapshot

Generated projects/fixtures are automatically removed by the runner. Final logs,
reports, SDKs and useful build outputs remain; obsolete direct probes are removed.

## Subsequent optional-provider integration replay

The unchanged core runner also passes against actual compiler
`d25df05ef2775704995c219c8516a53369520787c1a5112c46d5e74eae635be6`
after the additive optional-provider integration: **974/974** independent inputs,
**16/16** independent outputs and **4/4** generated profiles on the same Kotlin
2.2.21/Microsoft JDK 17 SDK baseline, without optional jars. The normal emitted
module is now **90,877 bytes**, SHA-256
`f6d4d698f0699befcb38836121718d437a598de989842005c72e83b04f28e27c`.
The helper adds source while preserving the ordinary model APIs and native bytes.
Retained evidence is `kotlin-core-provider-update.log` and
`kotlin-core-provider-update/verification.json` beneath the same ignored task root.
The [separate provider proof](verification-kotlin-compression-providers-2026-10-20.md)
records the optional deployment and Windows/Linux runtime scope.
