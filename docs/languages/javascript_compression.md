# JavaScript and TypeScript whole-message compression

[JS/TS output](portable_languages.md#javascript-and-typescript) ·
[Compression contract](../serialization/compression.md)

Compiler **2.14.0** adds an owning asynchronous compression helper to regenerated
JavaScript ES modules and their matching TypeScript declarations. Existing model
methods, module format, identifiers and ordinary native codec bytes are unchanged.
The selected Node profile uses its maintained built-in `node:zlib` provider.
An explicit optional adapter accepts a caller-supplied LZ4 block module. Default
generation, ordinary imports and the Node providers add no npm dependencies.

The exported container normally has the name `SerializerCompression`. Generation
selects `_2`, `_3`, and so on if its container or private declaration alias would
collide with a schema name. Both generated files record the selected name in a
comment. Generate JS and declarations together with the same naming settings.

## Explicit provider and owning APIs

```javascript
import {SerializerCompression as C, AccountRecord, Protocol} from './schema.mjs';

const provider = await C.nodeProvider();
const value = new AccountRecord();
value.accountId = 7;
value.label = 'example';

const packed = await C.encode(() => value.encode(Protocol.BINARY_INTEGER),
  new C.EncodeOptions({format: C.Format.Gzip, provider}));
const candidate = await C.decode(packed,
  bytes => AccountRecord.decode(bytes, Protocol.BINARY_INTEGER),
  new C.DecodeOptions({format: C.Format.Gzip, provider}));
```

`nodeProvider()` and `zstdProvider()` explicitly load `node:zlib`. Ordinary module
imports, native codecs and `Format.None` identity compression work without Node
modules, including in browsers. Compressed formats require the explicitly returned
provider in options; loading it does not change another call's policy.
`available(format, provider)` reports capability without loading modules or
invoking callbacks. Unknown format selectors reject.

| Format | Provider | Encoding level |
| --- | --- | --- |
| `None` | Dependency-free identity copy | Validated, then ignored |
| `Gzip`, `Zlib`, `Deflate` | Explicit optional Node raw-DEFLATE transform and standard wrapper framing | Integer 0–9, default 6 |
| `Zstd` | Explicit `await zstdProvider()` using Node's public Zstandard transforms | Integer -131072 through 22, default 3 |
| `Lz4` | Explicit `lz4Provider(blockModule)`; selected module is `lz4js` 0.2.0 | Fast level 0 only, default 0 |

## Optional Zstandard and LZ4

```javascript
const zstd = await C.zstdProvider();
const packed = await C.compress(serialized,
  new C.EncodeOptions({format: C.Format.Zstd, provider: zstd}));
const back = await C.decompress(packed,
  new C.DecodeOptions({format: C.Format.Zstd, provider: zstd}));

// Load the application's pinned module; bundlers can supply its block exports too.
const imported = await import('lz4js');
const lz4 = C.lz4Provider(imported.default ?? imported);
const lz4Options = new C.EncodeOptions({format: C.Format.Lz4, provider: lz4});
```

The selected Zstandard profile uses Node **22.23.1**. Public Zstandard transforms
were added in Node 22.15.0 and remain experimental in the selected release;
availability requires their actual functions and constants. A Node runtime with
only DEFLATE still supports `nodeProvider()` but rejects `zstdProvider()` as
`Unavailable`. The ordinary DEFLATE handle grants no Zstandard capability.
See the [versioned Node API](https://nodejs.org/download/release/v22.23.1/docs/api/zlib.html).

The selected [lz4js 0.2.0 module](https://github.com/Benzinga/lz4js) provides
synchronous `compressBlock` and `decompressBlock` functions. Applications own its
loading, dependency and workspace. The generated helper imports it neither
implicitly nor globally. The adapter captures the supplied block functions in a
private immutable capability handle; it works in a browser-like environment
without Node imports. Its fast block compressor supports level 0 only; higher
levels reject before policy, codec or transport callbacks. Custom modules must
implement the same synchronous block contract; promise-returning functions reject.

Optional formats require their matching explicit handle. Missing or mismatched
providers reject before cancellation, native codecs or transport calls. No global
provider registry is installed. `available(format, provider)` reports the selected
capability. Ordinary calls without providers retain their previous behavior.

TypeScript preserves `Provider`'s existing default name literal, `'node:zlib'`,
and the existing default types of option properties. New `ZstdProvider` and
`Lz4Provider` types describe their exact names. Option constructors infer the
provider type; when explicitly annotating new optional policies, use
`EncodeOptions<C.ZstdProvider>`, for example with the namespace alias `C`.
`AnyProvider` is the union accepted by operation and wrapper APIs.

## Owning bytes and models

`compress(Uint8Array, EncodeOptions)` and
`decompress(Uint8Array, DecodeOptions)` return promises of independent byte storage.
`decompressFrom(source, DecodeOptions)` reads a borrowed source to explicit EOF.
`encode` and `decode<T>` adapt complete owning native codec callbacks; those may
return immediately or return a promise. Supply a decoder that checks complete
inner consumption, as generated `decode` does. A candidate is returned only after
frame validation, native decoding and final cancellation checks.

Complete mutable input and synchronous encoder results are copied before the
first asynchronous yield. An asynchronous callback must keep its returned bytes
stable until delivery. The compression input cap is checked after an encoder
returns its bytes and does not bound that callback's internal allocations.
Native `Limits` and compression policies remain independent.

## Exact framing, bounds and cancellation

Immutable `EncodeOptions` defaults to 64 MiB serialized input and 64 MiB compressed
output. Immutable `DecodeOptions` independently defaults to 64 MiB compressed input,
64 MiB actual expanded output and an 8 MiB window budget. Limits are nonnegative
safe integers no larger than 2 GiB minus one byte. A compressed DEFLATE format
requires at least 32 KiB window admission; an insufficient nonnegative budget is
`ResourceLimit`.

A bounded in-memory RFC 1951 validator checks complete block grammar, dynamic
Huffman trees, available back-reference history, advertised zlib window and actual
expanded count before expanded output allocation. It derives the exact raw-stream
extent independently of provider read-ahead. gzip validates optional header fields,
FHCRC, CRC-32 and ISIZE; zlib validates its header and Adler-32 and rejects preset
dictionaries. Truncation, trailing bytes and concatenated frames/members reject
before model decoding. Raw DEFLATE cannot detect every corrupt change.

Zstandard independently checks one standard frame, dictionary absence, advertised
size/window, block extents and exact EOF before native expansion. The native
window ceiling rounds down to a power of two and requires at least 1 KiB.
LZ4 verifies descriptor, block and content checksums, exact size/EOF, token lengths
and back-reference history before expanded allocation. Linked blocks retain at
most 64 KiB of history. Its advertised block maximum, 64 KiB through 4 MiB, must
fit the window budget before decoding. Both optional writers emit a content
checksum; standard unchecked input is accepted. Skippable, concatenated, legacy
and externally dictionary-backed frames reject.

Private Node transforms receive at most 32 KiB per write, and output is bounded
before copying provider chunks. The helpers stage whole messages; they provide no
incremental object or record protocol. Limits cover logical bytes/window, not
total heap, provider workspace, caller objects or process RSS. No format detection,
dictionary injection, fallback provider or implicit uncompressed fallback occurs.
LZ4 output uses independent 64 KiB blocks; input block calls use the admitted
maximum plus at most 64 KiB of history. Borrowed I/O and generated checksum updates
use at most 32 KiB batches. A synchronous block-provider call cannot be preempted.

An optional synchronous `checkCancelled` callback may throw to cancel. Its original
thrown value is preserved, including application errors that resemble zlib errors.
Polling occurs during bounded memory validation, provider batches, borrowed I/O,
after final writes and after decoding. It cannot preempt an active application
callback or provider operation. A promise-returning cancellation callback rejects
as `InvalidOptions`; cancellation itself is synchronous.

`C.Error.code` is `Unavailable`, `InvalidOptions`, `InvalidData` or `ResourceLimit`
from `C.Code`, without payload excerpts. Native codec, allocation, transport and
cancellation failures retain their own types/values. Invalid wrapper lifecycle
operations reject with ordinary `Error` objects.

## Borrowed asynchronous wrappers

```javascript
const output = new C.Output({async write(bytes) {
  await transportWriteAll(bytes);
}}, new C.EncodeOptions({format: C.Format.Gzip, provider}));
try {
  await output.write(value.encode(Protocol.BINARY_INTEGER));
  await output.finish();
} finally {
  output.close();
}

const input = new C.Input({async read(maxBytes) {
  return transportReadBytesOrEof(maxBytes);
}}, new C.DecodeOptions({format: C.Format.Gzip, provider}));
```

A source returns a nonempty `Uint8Array` no larger than the request, or `null` at
actual EOF. A sink consumes the whole independent chunk or throws/rejects. Await
each operation sequentially. A Node `Readable.read()` can return `null` before EOF,
and a Node `Writable.write()` Boolean is not asynchronous completion: adapt their
completion/EOF callbacks to these contracts rather than passing either directly.
The application continues to own transport error handling.

Output stages serialized bytes until explicit fallible `finish`, then publishes
bounded writes. Input validates the complete frame and source EOF before exposing
expanded bytes; `read(maxBytes)` returns independent bytes or `null` at expanded
EOF. Its `finish` requires all expanded bytes to have been consumed. Input retains
its original cancellation policy after staging. Finish output wrappers from the
codec outward.

`flush` checks policy and lifecycle without reading, publishing or forwarding a
flush. `close` releases private staging, emits no trailer and never closes,
flushes or synchronizes the borrowed transport. Failed, finished, closed and
overlapping operations are fenced. Closing or overlapping an awaited operation
prevents subsequent borrowed calls; an already active callback can still have
effects. External writes may retain a prefix and failed reads may consume input.
Only private Node provider streams are destroyed by the helper.

The [qualification record](../verification/verification-javascript-compression-2026-10-20.md)
identifies the actual core SDK and independently authored framing cases. The
[optional-provider proof](../verification/verification-javascript-compression-providers-2026-10-20.md)
records actual Node execution, strict TypeScript compatibility, independent frames
and C++ interoperability. Broader shared-operation parity, actual browser SDKs and
performance qualification remain separate backlog items.
