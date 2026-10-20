# Swift whole-message compression

[Swift output](native_languages.md) · [Compression contract](../serialization/compression.md)

Compiler/runtime **2.14.0** adds a standalone owning compression helper to
regenerated Swift output. Identity, gzip, zlib and raw DEFLATE require no additional
module, system compression library or C++ runtime. Existing native model APIs,
identifiers and wire bytes retain their behavior. Select the outer compression
format explicitly; it does not identify the native protocol or schema.

The usual helper is the public `SerializerCompression` enum. If a generated model
already occupies that name, the generator selects `_2`, `_3` and so on. The emitted
comment records the chosen name. Existing model names are preserved.

## Owning APIs

```swift
typealias C = SerializerCompression

let packed = try C.encode({ try value.encode(.BINARY_INTEGER) },
    options: .init(format: .gzip, maxInputBytes: 8*1024*1024,
                   maxOutputBytes: 9*1024*1024))
let candidate = try C.decode(packed,
    decoder: { try AccountRecord.decode($0, .BINARY_INTEGER,
                                        Limits(maxBytes: 8*1024*1024)) },
    options: .init(format: .gzip, maxOutputBytes: 8*1024*1024))
```

Replace the model and value with generated owning types. Native callbacks execute
synchronously. Supply the generated exact-message decoder; arbitrary application
callbacks must preserve that fresh-value and complete-consumption contract.
`decode` checks cancellation again after the native callback and returns no
candidate on failure. Callback side effects cannot be rolled back.

`compress([UInt8], options:)` and `decompress([UInt8], options:)` expose the same
framing for already encoded messages. Successful results have value semantics.
Mutable models and borrowed transport state must remain stable during a call.
Compression input admission occurs after an encoder returns its bytes and does
not limit allocation inside that callback. Native object `Limits` and outer
compression bounds remain separate.

| Format | Standalone profile |
| --- | --- |
| `.none` | Identity bytes |
| `.gzip` | One gzip member, including checked optional fields/header CRC on input |
| `.zlib` | One zlib stream with checked header, advertised history and Adler-32 |
| `.deflate` | One raw RFC1951 stream; no checksum |
| `.zstd`, `.lz4` | Require an explicitly selected caller-owned provider |

`available(format)` checks the standalone profile without callbacks; optional
formats remain unavailable through that original overload. Missing formats do not
fall back to another provider or identity. There is no dictionary injection,
auto-detection, concatenation or implicit transport framing.

## Bounds and standalone codec

`EncodeOptions` defaults to 64 MiB of input and 64 MiB of output, with level 6.
`DecodeOptions` defaults to 64 MiB each of compressed input and actual expanded
output, plus an independent 8 MiB history allowance. Invalid negative policies
reject before callbacks or I/O. The DEFLATE formats require at least 32 KiB of
history allowance even when a particular frame advertises less.

Level zero emits standard stored blocks. Levels 1–9 use fixed Huffman blocks and
LZ77 matching with increasing bounded search effort and at most 32 KiB of history.
This level scale does not promise zlib's heuristics, compressed size or bytes.
All levels emit ordinary standard frames consumable by independent providers.

Input accepts stored, fixed and dynamic Huffman blocks. Before allocating expanded
storage, a bounded grammar pass validates Huffman trees/repeats/end codes,
available backward history, actual expanded count and the exact raw-stream extent.
A second pass expands only that admitted count. gzip and zlib verify their
mandatory checksums and size fields; invalid checksums, dictionary requests,
truncation, trailing bytes and concatenated frames reject. Raw DEFLATE cannot
detect every corrupt change.

Both directions stage bounded complete messages. Logical byte/history caps exclude
array spare capacity, the fixed codec workspace, native decoded objects and total
process RSS. The helper adds source and runtime code even when unused. Ordinary
model codec calls retain their direct uncompressed paths.

## Borrowed sequential wrappers

```swift
let output = try C.Output(sink: { bytes in
    try transportWriteCompleteChunk(bytes)
}, options: .init(format: .gzip))
defer { output.close() }
try output.write(value.encode(.JSON))
try output.finish()

let input = try C.Input(source: { maximum in
    try transportReadBytesOrActualEof(maximum)
}, options: .init(format: .gzip))
defer { input.close() }
var plain: [UInt8] = []
while let block = try input.read() { plain.append(contentsOf: block) }
try input.finish()
let candidate = try AccountRecord.decode(plain, .JSON)
```

The source returns a nonempty byte array no larger than its request, or `nil` at
actual EOF. Empty progress rejects. Reads and writes are bounded to 32 KiB per
borrowed call; input uses a one-byte overflow probe when its input cap is reached.
`read(0)` returns an empty array without source I/O. Returned chunks are independent
values. Input validates the complete frame and source EOF before exposing bytes;
its explicit `finish()` requires all expanded bytes to have been consumed.

The sink consumes the complete supplied chunk or throws. Adapt short-write APIs
inside the callback. Output stages native bytes until explicit fallible `finish()`.
Its `flush()` checks activity and policy without publishing bytes or forwarding a
flush. `close()` only abandons private staging; it emits no trailer and never
closes or flushes the borrowed transport. Finish nested output wrappers from the
codec outward. An external failure may leave consumed input or an output prefix.

Both option types accept a synchronous throwing `checkCancelled` closure. Polling
occurs between bounded codec/validation work and around transport callbacks,
including final writes and later reads/finish after staging. It cannot preempt a
running application callback or uninterrupted work interval.

Wrappers are sequential and unsynchronized. Closing or reentering the same active
wrapper fences both the nested and outer operation, even if the callback catches
the immediate nested failure. No subsequent borrowed call or successful
publication follows that failure. The active outer operation preserves its first
original error; later inactive operations throw `Failure(.invalidOptions, ...)`
with that error in `cause`. Native, cancellation and transport failures retain
their original values and types. Compression failures expose `.unavailable`,
`.invalidOptions`, `.invalidData` or `.resourceLimit`, without payload excerpts.

See the [selected SDK verification](../verification/verification-swift-compression-2026-10-20.md)
for the actual Linux Swift 6.1.3 and Windows Swift 6.4 profiles, independent
framing decisions and scope. Windows requires a matching Swift SDK and runtime
environment in addition to the compiler executable.
## Explicit optional providers

Regenerated helpers expose `Provider`, `Context` and `Progress` inside their
collision-selected enum. Pass a provider through the separate option initializer
with required `provider:`. The original initializers retain their exact argument
lists, defaults and function-value types. Linking a provider never changes the
original `available(format)` result or installs a global registry.

```swift
// nativeProvider is supplied by the application-owned reference adapter below.
let provider = C.nativeProvider(.zstd)
precondition(C.available(.zstd, provider: provider))
let packed = try C.compress(serialized,
    options: .init(format: .zstd, provider: provider, level: 3))
let plain = try C.decompress(packed,
    options: .init(format: .zstd, provider: provider,
                   maxWindowBytes: 8*1024*1024))
```

The [caller-owned reference adapter](../../qualification/swift_compression_providers/README.md)
uses stable Zstandard **1.5.7** and LZ4 **1.10.0** C APIs. Copy its shim, module map
and Swift adapter into the application's build; replace the helper placeholder
with the actual generated name and link the selected native libraries. Ordinary
generated Swift requires none of these files or libraries. The adapter extends
the helper namespace, including its implementation error type, so ordinary models
named `Provider`, `Context`, `Progress` or `NativeProviderFailure` remain usable.

Provider-specific encode initializers default to Zstandard level **3** or LZ4
level **0**. The original initializer still defaults to level **6**. The reference
Zstandard adapter admits the linked library's full advertised level interval;
LZ4 admits **0** through its native maximum. Zstandard emits checksummed frames
with pledged content size. LZ4 emits independent 64 KiB blocks with block and
content checksums. Empty frames and all four native protocols are supported.

Each operation obtains a private `Context`. Its synchronous step accepts a
borrowed input slice and fixed output storage, then reports consumed/produced
counts and completion. Input and output spans are at most **32 KiB**. Generated
code checks counts, storage size, progress and exact completion before publishing
bytes. Context callbacks must not retain the buffers, resize output storage or
share mutable codec state. Factory, context and cancellation errors keep their
original identity; a later policy callback cannot replace a prior provider error.

Generated preflight checks one standard frame, dictionary absence, advertised
size/window, block extents and exact EOF before creating a decoder context.
Zstandard's history ceiling rounds down to a power of two with a 1 KiB minimum.
LZ4 accepts advertised block maxima from 64 KiB through 4 MiB, linked history and
legal empty raw blocks. Native providers validate compressed grammar and all
checksums. Only known frame errors map to `.invalidData`; unrelated native failures
retain the adapter's own error type.

Contexts close once on success or failure. Reuse and reentry are fenced. Cleanup
requested during an active step waits until that step returns, preserving the
first failure and preventing release of an active native context. Providers and
wrappers are sequential and unsynchronized; keep their configuration and borrowed
state stable through each call. Cancellation is cooperative around native calls.
The 32 KiB API span does not bound native workspace: LZ4 may buffer or process an
admitted block of up to 4 MiB. Logical limits exclude provider workspace and RSS.

The [Windows/Linux provider proof](../verification/verification-swift-compression-providers-2026-10-20.md)
records 317 shared input decisions, 24 independently checked outputs, 48 C++
consumers, five compiled profiles and lifecycle controls on each host. Broader
operation-context integration and performance qualification remain separate work.
