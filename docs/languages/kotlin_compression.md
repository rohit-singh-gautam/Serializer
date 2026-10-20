# Kotlin whole-message compression

[Kotlin output](native_languages.md) ·
[Compression contract](../serialization/compression.md)

Compiler **2.14.0** adds a standalone owning compression helper to regenerated
Kotlin/JVM output. Its core uses standard JDK providers without additional dependencies;
Zstandard and LZ4 adapters are discovered lazily from the selected runtime classpath.
The qualified compression profile targets JVM 17; the linked record identifies
the actual compiler and runtime versions.
The ordinary model APIs, identifiers and native codec bytes are unchanged.

The top-level object normally has the name `SerializerCompression`. If a schema
uses that flattened name, generation selects `SerializerCompression_2`, `_3`, and
so on. The generated source comment records the selected name. The helper follows
the module's configured Kotlin package; the default package remains unnamed.

## Owning APIs

```kotlin
import SerializerCompression as C

val value = AccountRecord().apply { accountId = 7u; label = "example" }
val packed = C.encode({ value.encode(Protocol.BINARY_INTEGER) },
    C.EncodeOptions(C.Format.GZIP))
val candidate = C.decode(packed,
    { AccountRecord.decode(it, Protocol.BINARY_INTEGER) },
    C.DecodeOptions(C.Format.GZIP))
```

`compress(ByteArray, EncodeOptions)` and `decompress(ByteArray, DecodeOptions)`
operate on complete owning encoded bytes, including caller-owned XML or another
codec. `decompress(InputStream, DecodeOptions)` reads a borrowed source to EOF.
Results have independent byte storage. `encode` and `decode<T>` adapt whole-message
codec callbacks, with no Serializer compression envelope or incremental record
protocol. Generated native `decode` checks complete inner consumption.

| Format | Provider | Level |
| --- | --- | --- |
| `NONE` | Identity copy | Validated, then ignored |
| `GZIP`, `ZLIB`, `DEFLATE` | Standard `java.util.zip.Deflater`/`Inflater`, with explicit framing | Integer 0–9, default 6 |
| `ZSTD` | Optional `com.github.luben:zstd-jni` | Provider-advertised integer range, including negative fast levels; default 3 |
| `LZ4` | Optional `at.yawk.lz4:lz4-java` safe Java provider | 0 uses fast mode; 1–9 uses high compression; default 0 |

`available(format)` probes without application callbacks or borrowed I/O. Formats
are explicit; there is no auto-detection, fallback or preset dictionary support.
Missing jars or failed native deployment reject before policy, model or transport
callbacks. Provider versions can change compressed bytes
while preserving the standard framing and uncompressed native message.

## Optional providers

Generation and default compilation require no optional jars. Supply them only on
the application's runtime classpath. The selected adapters use pinned
`zstd-jni:1.5.7-16` and `lz4-java:1.12.0`; LZ4 selects the safe pure-Java APIs and
does not load a native LZ4 library. Zstandard requires the native deployment
provided by its platform jar. Reflection resolves public provider APIs lazily,
and each operation owns its native stream/context or content-hash state.

The existing `EncodeOptions(format, maxInputBytes, maxOutputBytes, level,
checkCancelled)` positional API remains unchanged. Zstandard level admission uses
the full advertised provider interval before application effects. Optional output
frames always include a content checksum; readers also accept standard unchecked
frames. No checksum field or schema syntax is added.

Zstandard output pledges the known serialized size and processes bounded provider
buffers. Input independently checks one standard frame, dictionary absence,
advertised size/window, complete block extents and exact EOF before native
expansion. Its window cap rounds down to a power of two and requires at least
1 KiB. LZ4 output uses 64 KiB independent blocks. Its input parser checks frame,
descriptor, block and content checksums, counts regenerated lengths/back
references before allocation, and supports independent or linked blocks. Linked
decoding retains at most 64 KiB and adds that history to the first literal run
for the safe block decoder. LZ4 admits the declared 64 KiB–4 MiB block maximum
against the window budget before expansion. Skippable, concatenated, dictionary,
truncated and trailing frames reject explicitly.

## Limits, framing and cancellation

Immutable `EncodeOptions` defaults to 64 MiB serialized input and 64 MiB compressed
output. Immutable `DecodeOptions` independently defaults to 64 MiB compressed
input, 64 MiB actual expanded output and an 8 MiB window budget. A compressed
DEFLATE format requires at least 32 KiB; an insufficient nonnegative window is
`RESOURCE_LIMIT`, while negative limits or invalid levels are `INVALID_OPTIONS`.

Before expanded output allocation, a bounded in-memory validator checks complete
DEFLATE block/Huffman grammar, actual history/distance availability, advertised
zlib window and actual expanded count. It establishes the exact stream byte
extent independently of provider buffering. gzip additionally validates optional
headers, FHCRC, CRC-32 and ISIZE; zlib checks its header and Adler-32. Truncation,
trailing data and concatenated streams/members reject before model decoding.
Raw DEFLATE cannot detect every corruption.

Borrowed transport requests, Zstandard provider buffers and streaming checksum
updates use at most 32 KiB batches. LZ4 block calls use the admitted frame block
maximum, up to 4 MiB on input plus at most 64 KiB of retained prefix history;
output blocks are 64 KiB. A source
overflow check requests at most one byte beyond the remaining compressed input
budget. Whole messages are staged; logical limits exclude provider workspace,
allocator capacity, caller codec allocations and total process RSS. Native
`Limits` remain independent: select both compression and native limits when a
smaller decoded-object budget is required. The complete encoder callback returns
its owning bytes before the compression input cap can check their size.

The optional synchronous `checkCancelled: (() -> Unit)?` callback may throw the
application's own exception, which is preserved. It is polled during bounded
validation work and provider/source/sink batches, after final writes and after
fresh model decoding. It cannot preempt a blocking callback or provider call.
Wrappers remain sequential; immutable provider-capability caches synchronize only
their lazy resolution. Keep mutable input byte storage stable and
callbacks/transports alive throughout each synchronous operation.

`Failure.code` distinguishes `UNAVAILABLE`, `INVALID_OPTIONS`, `INVALID_DATA`
and `RESOURCE_LIMIT`, without payload excerpts. Native codec, allocation,
transport and cancellation exceptions keep their own types. Invalid lifecycle
operations throw `IllegalStateException`.

## Borrowed stream wrappers

```kotlin
val output = C.Output(sink, C.EncodeOptions(C.Format.GZIP))
try {
    output.write(value.encode(Protocol.BINARY_INTEGER))
    output.finish()
} finally {
    output.close()
}

val input = C.Input(source, C.DecodeOptions(C.Format.GZIP))
val bytes = try {
    input.readBytes().also { input.finish() }
} finally {
    input.close()
}
val decoded = AccountRecord.decode(bytes, Protocol.BINARY_INTEGER)
```

Output stages codec bytes until explicit fallible `finish`, then publishes bounded
writes. Input reads and validates the complete frame and borrowed source EOF
before exposing any expanded bytes; its `finish` requires all expanded bytes to
have been consumed. Zero-length input reads return zero without reading the source.
Input retains its original cancellation policy after staging. Finish output
wrappers from the codec outward.

`flush` does not finish or publish output and does not flush the borrowed sink.
`close` releases private staging without trailers, borrowed close/flush or further
I/O. Failed, finished, closed and reentrant operations are fenced. Activity checks
on both sides of policy callbacks and after transport calls prevent callback-driven
close/reentry from triggering later borrowed operations or successful completion.
A swallowed nested rejection still aborts its active outer operation with the
first exception. External writes may retain a prefix; failed reads may consume
input. Use each wrapper sequentially on one thread.

The [qualification record](../verification/verification-kotlin-compression-2026-10-20.md)
identifies the actual core Kotlin/JVM 17 SDK profile and independent fixtures.
The [optional-provider proof](../verification/verification-kotlin-compression-providers-2026-10-20.md)
records actual Windows JDK 17 compilation/execution and isolated Linux JRE 25
execution of those JVM 17 classes. Broader shared-operation parity and performance
qualification remain separate work.
