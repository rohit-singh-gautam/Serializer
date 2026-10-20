# C# whole-message compression

[C# output](portable_languages.md#c) · [Compression contract](../serialization/compression.md)

Compiler **2.14.0** adds an owning compression helper to regenerated C# output.
It targets **.NET 8+**. The default profile uses the standard library without
additional package dependencies. Optional managed adapters use assemblies selected
explicitly by the caller for each operation.
Existing generated models and ordinary `Encode`/`Decode` calls keep their APIs and
uncompressed wire paths.

The nested helper normally has the public name `SerializerCompression`. If that
name belongs to a schema type or the outer output class, generation selects
`SerializerCompression_2`, `_3`, and so on. The generated source comment records
the selected name; existing schema names are preserved.

## Owning message APIs

For a generated `AccountRecord` with `AccountId` and `Label` fields:

```csharp
using static SerializerGenerated.Schema;
using C = SerializerGenerated.Schema.SerializerCompression;

var value = new AccountRecord { AccountId = 7, Label = "example" };
byte[] wire = C.Encode(() => value.Encode(Protocol.BINARY_INTEGER),
    new C.EncodeOptions(C.Format.Gzip));
AccountRecord candidate = C.Decode(wire,
    bytes => AccountRecord.Decode(bytes, Protocol.BINARY_INTEGER),
    new C.DecodeOptions(C.Format.Gzip));
```

`Compress(byte[], EncodeOptions)` and `Decompress(byte[], DecodeOptions)` operate
on complete encoded bytes, including XML or another caller-owned codec.
`Decompress(Stream, DecodeOptions)` reads a borrowed source to EOF. Results own
their byte storage. Compression wraps one complete message; it adds no Serializer
envelope or incremental record/object protocol.

`Encode` admits options/provider availability before invoking the encoder.
The callback returns its owning serialized bytes; the compression input cap is
checked after that callback returns and does not bound its internal allocations.
`Decode<T>` validates the complete frame before calling the decoder and checks
cancellation again before returning the fresh candidate. Supply a decoder that
checks exact inner consumption, as generated `Decode` does. Native decoder limits
remain separate; choose both `DecodeOptions.MaxOutputBytes` and `Limits.MaxBytes`
when the application needs a tighter native-message bound.

| Format | Provider | Encoding level |
| --- | --- | --- |
| `None` | Identity copy | Ignored after option validation |
| `Gzip`, `Zlib`, `Deflate` | Buffered `System.IO.Compression.DeflateStream`, with explicit standard wrapper framing | `CompressionLevel.Optimal` by default |
| `Zstd` | Explicit `ZstdSharp.Port` assembly | Full advertised integer range, including negative fast levels; default 3 |
| `Lz4` | Explicit `K4os.Compression.LZ4` assembly | Defined provider integers 0 and 3–12; default fast level 0 |

The original constructor's levels use .NET's actual `CompressionLevel` enum: `Optimal`, `Fastest`,
`NoCompression` and `SmallestSize`. They do not pretend to expose numeric zlib
levels 0–9. Unknown enum values reject. Formats are explicit; there is no
auto-detection, provider fallback or dictionary support. Compressed bytes may
change across provider versions while retaining the same standard framing and
uncompressed native payload.

## Optional managed providers

The generated source compiles without optional assembly references. The selected
profile uses `ZstdSharp.Port` **0.8.8** and `K4os.Compression.LZ4` **1.3.8**. Both
are managed providers; this profile needs no native compression DLL. Load them in application
setup, or pass the assembly of a provider type already referenced by the application:

```csharp
var zstd = C.Provider.ZstdSharp(
    System.Reflection.Assembly.LoadFrom("ZstdSharp.dll"));
var encode = C.EncodeOptions.WithProvider(C.Format.Zstd, zstd, level: 3);
var decode = C.DecodeOptions.WithProvider(C.Format.Zstd, zstd,
    maxWindowBytes: 8 * 1024 * 1024);
byte[] packed = C.Encode(() => value.Encode(Protocol.BINARY_INTEGER), encode);
var candidate = C.Decode(packed,
    bytes => AccountRecord.Decode(bytes, Protocol.BINARY_INTEGER), decode);
```

Use `Provider.Lz4(assembly)` for the LZ4 adapter. Providers hold immutable public
capability handles; streams and compression/decompression contexts are private to
each operation. The caller owns the assembly/load context. Provider factories
resolve only the supplied assembly and do not register providers globally.
`Available(format, provider)` checks explicit selection. The original
`Available(format)` and constructor calls retain the standalone profile, so
Zstandard/LZ4 remain unavailable without an explicitly selected provider, even
when a provider assembly is loaded elsewhere in the process.

The original five-parameter public constructors retain their signatures and
overload resolution, including calls containing an untyped `default` argument.
`WithProvider` returns the same immutable option type, preserving byte limits and
cancellation. `ProviderLevel` reports its integer level; the existing `Level`
property continues to describe BCL selectors and is unused by optional formats.
Zstandard exposes `MinimumLevel` and `MaximumLevel`; LZ4 accepts its defined
integers rather than every integer between those endpoints. Mismatched provider
selection, invalid levels or inadequate window policies and missing public capabilities fail before
codec, cancellation or borrowed transport callbacks. Application assembly-loading
errors remain the application's responsibility.

Optional outputs always include a content checksum; input accepts standard
checked and unchecked frames. Zstandard pledges the known source size. Input
independently validates its header, dictionary absence, advertised size/history,
block extents and exact EOF before provider expansion, and applies the provider's
window limit. Its ceiling rounds down to a power of two with a 1 KiB minimum.
LZ4 output uses 64 KiB independent blocks. Its parser checks descriptor, block
and content checksums, exact size/EOF, and regenerated lengths/back references
before output allocation. Linked blocks use at most 64 KiB of retained prior
output through the provider's explicit dictionary API; caller-supplied dictionaries
remain unsupported. The declared 64 KiB–4 MiB block maximum must fit the window
budget. Skippable, legacy, concatenated, truncated and trailing frames reject.

Zstandard provider buffers, borrowed transport and checksum polling use at most
32 KiB batches. LZ4 block calls use the admitted block maximum, up to 4 MiB on
input, plus the separate retained history; output blocks are 64 KiB. Full messages
are staged. The same first-failure, close/reentry, cancellation and ownership
guards apply to these adapters. Optional provider versions can change compressed
bytes. Trimming/AOT and other provider or runtime versions require their own
qualification; reflection requires the selected public APIs to remain available.
The [optional-provider qualification](../verification/verification-csharp-compression-providers-2026-10-20.md)
records Windows .NET 8 compilation and Windows/Linux runtime execution,
independent frames, native C++ interoperability, deployment profiles and lifecycle
checks.

## Exact framing and logical bounds

`EncodeOptions` defaults to 64 MiB serialized input and 64 MiB compressed output.
`DecodeOptions` independently defaults to 64 MiB compressed input, 64 MiB actual
expanded output and an 8 MiB window budget. DEFLATE admission requires at least
32 KiB; a smaller nonnegative budget is `ResourceLimit`, while negative limits
are `InvalidOptions`.

Before allocating expanded bytes, a bounded in-memory validator checks the
complete DEFLATE grammar, dynamic Huffman trees, available back-reference history
and actual expanded count. It identifies the exact raw-stream byte extent;
buffered provider read-ahead does not define the message boundary. gzip additionally
checks optional fields, FHCRC, CRC-32 and ISIZE. zlib checks its header, advertised
history window and Adler-32, and rejects dictionary-dependent input. Truncation,
trailing bytes and concatenated frames/members reject before field decoding.
Raw DEFLATE has no checksum and cannot detect every corruption.

These limits cover logical message/window storage. They do not bound total heap,
allocator spare capacity, caller codec objects, provider workspace or process RSS.
Complete messages are staged, and the validator performs an additional pass over
the compressed representation. Performance observations and the selected SDK
profile are recorded in the [qualification record](../verification/verification-csharp-compression-2026-10-20.md).

## Borrowed stream wrappers

```csharp
using var output = new C.OutputStream(sink, new C.EncodeOptions(C.Format.Gzip));
output.Write(value.Encode(Protocol.BINARY_INTEGER));
output.Finish();

using var input = new C.InputStream(source, new C.DecodeOptions(C.Format.Gzip));
using var expanded = new System.IO.MemoryStream();
input.CopyTo(expanded);
input.Finish();
AccountRecord decoded = AccountRecord.Decode(expanded.ToArray(), Protocol.BINARY_INTEGER);
```

Output stages serialized bytes until explicit fallible `Finish`, then publishes
bounded writes. Input stages and validates to source EOF before exposing any
expanded bytes; its `Finish` requires all expanded bytes to have been consumed.
Finish output wrappers from the codec outward. Neither wrapper closes, flushes
or synchronizes its borrowed transport. `Flush` does not finish output or read
input. `Dispose` releases private staging and emits no trailer. An external write
failure may retain a prefix, and failed reads may already have consumed source
bytes. Failed, finished and disposed wrappers reject subsequent data operations.
Policy and transport callbacks may not close/reenter a wrapper and then continue
its active operation: activity checks on both sides of callbacks prevent further
borrowed I/O. Reentry permanently fences the active wrapper, preserving its first
exception even if the callback catches the immediate rejection and throws later.

Policies accept an optional `Action checkCancelled`, for example
`cancellationToken.ThrowIfCancellationRequested`. It may throw the application's
own exception, which is preserved. Polling occurs during bounded validation work,
provider/source/sink batches, after final writes and after model decoding. It
cannot preempt a provider or application I/O call. A staged input retains its
policy and continues checking cancellation during reads and `Finish`.

`Error.Code` distinguishes `Unavailable`, `InvalidOptions`, `InvalidData` and
`ResourceLimit`, with no payload excerpts. Allocation, native codec, transport
and cancellation exceptions keep their own types. Fenced lifecycle operations
throw `InvalidOperationException`; later fenced operations may carry the first
failure as `InnerException`. Calls and wrappers add no thread synchronization.
Keep mutable input byte storage stable and callbacks/borrowed transports alive
throughout each synchronous operation.
