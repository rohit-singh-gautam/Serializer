# C# compression qualification, 20 October 2026

[C# compression API](../languages/csharp_compression.md) ·
[Compression contract](../serialization/compression.md) ·
[Remaining work](../pending.md) · [Verification index](README.md)

This record closes **CS-Z01–CS-Z08** for the selected Windows x64 .NET profile
below. Compiler/runtime release is **2.14.0**, schema language **1.20.0**. The
starting Git revision is `bd2ee2ef9239b65335367bd8de19be377ce84262`; changes are
uncommitted. This original snapshot covers the core profile. The subsequent
[optional-provider proof](verification-csharp-compression-providers-2026-10-20.md)
qualifies **CS-Z09/CS-Z10** and reruns this unchanged core suite. Broader
shared-operation parity **CS-Z11**, other platforms and general performance gates
remain separate.

## Actual generated-code checks

The [maintained runner](../../test/csharp_compression_test.py) invokes the rebuilt
compiler and compiles its actual emitted C# with **SDK 8.0.426**, target
**net8.0**, C# **12**, nullable reference checks and warnings as errors. Execution
uses **Microsoft.NETCore.App 8.0.32**. Five SDK builds complete with zero warnings
and zero errors: normal output, existing helper-name collisions, binary-only
protocol selection, an outer output class named `SerializerCompression`, and an
outer class named `Format`. `global.json` pins SDK 8; package sources are cleared
and no additional NuGet dependencies are used.

All **974 independent input decisions** pass. The shared Java/Python-provider
corpus supplies 957 decisions, including empty/single-byte/binary/random/repeated
payloads, every short-message truncation, trailing data, concatenated frames,
checksums, optional gzip fields/FHCRC and zlib dictionaries. Seventeen additional
independently authored cases exercise reserved block/length/distance codes,
stored-block length complements, distance before available history, invalid
dynamic alphabets, over/incomplete trees, invalid length repeats, missing end
codes, valid empty distance alphabets, fixed Huffman output and zlib advertised
history-window admission. Valid handcrafted raw controls are checked with
Python's independent zlib decoder before the C# consumer receives them.

Generated output passes **16 independent Python output decisions**: each of
identity, gzip, zlib and raw DEFLATE for all four ordinary model protocols.
Expanded bytes equal the original uncompressed generated model bytes. The
ordinary JSON field identity is independently inspected. The whole-model
functional adapters retain disabled-protocol checks and exact native consumption;
late cancellation withholds the fresh decoded candidate.

The suite also checks exact and one-tight input/output caps, all four actual
`CompressionLevel` values, empty output, invalid enum/negative policy admission,
32 KiB history admission before callbacks/source reads, unavailable Zstandard/LZ4
before callbacks or I/O, exception-object identity, failure fencing, borrowed
seven-byte short reads, bounded transport requests, EOF/overflow probes, unread
expanded input, explicit finish, abandoned disposal, sink/source failures and
late source/sink cancellation. An application `InvalidDataException` used for
cancellation survives every observed decoding callback boundary unchanged.

## Exact framing and provider boundary

A preliminary probe of the actual .NET 8.0.32 `GZipStream`, `ZLibStream` and
`DeflateStream` accepts one-byte-truncated frames and trailing bytes. Its source
position reaches the entire supplied input in those cases. gzip joins two
concatenated members; zlib/raw return the first payload while reading ahead over
both frames. This confirms that provider EOF or underlying stream position alone
cannot enforce this project's single-frame contract. The
[official .NET 8 implementation](https://github.com/dotnet/runtime/blob/v8.0.32/src/libraries/System.IO.Compression/src/System/IO/Compression/DeflateZLib/DeflateStream.cs)
also exposes its buffered read and concatenated-gzip behavior.

The generated helper first validates the DEFLATE bitstream in staged memory,
including canonical Huffman tree validity, real backward-distance availability,
overflow-safe actual output counts and the exact consumed byte extent. Expanded
storage is allocated only after that count passes the configured cap. The
buffered raw BCL provider then receives precisely the validated body; explicit
gzip/zlib wrapper checks reject invalid checksums, dictionaries, trailing bytes
and concatenation. The grammar/header checks follow
[RFC 1951](https://www.rfc-editor.org/rfc/rfc1951) and
[RFC 1952](https://www.rfc-editor.org/rfc/rfc1952); no external implementation
source is copied into the helper.

Cancellation is polled between bounded validator work and provider/transport
batches. It cannot interrupt a native provider call. Logical byte/window caps
do not bound provider workspace, allocator capacity, process RSS or allocations
inside a caller's complete encoder callback. Ordinary generated model code and
uncompressed wire paths retain their existing implementations.

## Final callback lifecycle regression

The additional maintained callback-reentrancy regression first failed against the
old actual generated .NET 8 helper: `Finish` could report success after the
application's cancellation hook disposed its wrapper. The final combined compiler
above was rebuilt and the complete suite passed again, including all 974 input
decisions, 16 independent output checks and five warning-free SDK/profile builds.

All four formats now exercise callback-driven disposal during write, flush,
source admission/read, cached empty-input finish and output finish. Disposing from
a source/sink callback prevents later borrowed calls or candidate publication.
Private activity checks surround policy callbacks and provider/transport
boundaries. Reentry is fenced even if its immediate rejection is swallowed by
the callback; the active operation preserves the exact first exception object,
including when the callback subsequently throws a different error. Later generic
lifecycle exceptions retain first-cause metadata. The before-failure log and
final SDK log are retained separately.

## Bounded performance observation

The actual emitted test performs two warmups and five measured decodes per
1 MiB workload, comparing raw BCL inflation with exact validation plus buffered
inflation. These observations include managed output allocation and byte equality
checks; tiered JIT and concurrent host work are not controlled general benchmarks.

| Workload | Compressed bytes | BCL mean ms | Validated mean ms | Borrowed source reads |
| --- | ---: | ---: | ---: | ---: |
| Fixed-seed random | 1,048,896 | 0.527 | 0.576 | 34 |
| Repeated byte | 1,034 | 0.587 | 1.617 | 2 |

Requests never exceed 32 KiB. The implementation uses no per-byte borrowed
transport calls; additional scalar validation work remains visible for highly
compressed input. These selected measurements do not close **CS-Q05** or a hard
CPU/peak-memory gate.

## Provenance and reproduction

The actual compiler is rebuilt with MSVC through the root Windows configuration
and CMake pipeline. Its SHA-256 is
`0d318c2e3b6ac19e0cea1b8561bcc2e38986f61f60025e2d00bec30c10856a94`.
The C# runtime template hash is
`00aa6a7d87d67b91cf3296933f3ba85aaa17344f574dba99ed478176a0425fa3`;
the portable generator hash is
`b358abb91e73cba258e1239e009ac2c80c03f21474f6f6e43e868efc66767f97`.
The test source hash is
`30bfbbf839190c3e03b737911b3a2a53562d42c7f31f2eae7ba492da780080e7`.
The normal generated C# file is **72,916 bytes** in this fixture. The four
template raw literals remain below the MSVC 16,380-byte token limit, and the
repository's embedded-literal check passes.

From a workspace with an available .NET 8 SDK/runtime and matching compiler:

```powershell
python -B test/csharp_compression_test.py `
  --compiler out/build/opc-implementation/serializer.exe `
  --build out/build/backlog-2026-10-20/csharp-compression/final
```

CTest registers `serializer_compression_csharp` when `dotnet` is discoverable.
The runner fails clearly if SDK 8 is missing rather than substituting a newer
SDK-only qualification. This checkpoint's direct actual-SDK execution passes;
an aggregate CTest replay after later source changes is a separate release check.

Retained evidence:

- `out/build/backlog-2026-10-20/windows-csharp-compiler.log`
- `out/build/backlog-2026-10-20/windows-lifecycle-compiler.log`
- `out/build/backlog-2026-10-20/csharp-reentrancy-before.log`
- `out/build/backlog-2026-10-20/csharp-compression-final.log`
- `out/build/backlog-2026-10-20/csharp-compression/final/verification.json`
- `out/build/backlog-2026-10-20/csharp-compression/bcl-framing-probe.log`

The runner removes generated projects/fixtures automatically. Useful SDK caches,
the selected build, final logs and provenance remain under ignored `out/build/`.

## Optional-provider follow-up

After the additive optional adapters, actual compiler SHA-256
`3533d50e7b9ac33bae04e969080c63ff80402bca69332381d1044f129262804e`
passes the unchanged core runner on SDK **8.0.426** and runtime **8.0.32**:
**974/974** independent inputs, **16/16** outputs and **5/5** strict SDK profiles.
No optional assembly references are needed, and default optional-format absence
admission remains intact. The fixture's normal generated source is now
**104,742 bytes** because it includes the additional helper API. Earlier source
sizes, hashes and performance observations above retain their original scope.
See the [provider record](verification-csharp-compression-providers-2026-10-20.md)
for final source hashes, explicit assembly selection and the separate
**317/24/48** optional input/output/native-consumer proof.
