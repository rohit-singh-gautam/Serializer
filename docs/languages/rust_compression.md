# Rust whole-message compression

[Rust output](native_languages.md) · [Compression contract](../serialization/compression.md)

Compiler/runtime **2.14.0** adds a standalone compression module to regenerated
Rust 2021 output. Identity, gzip, zlib and raw DEFLATE require only the standard
library. The default generated profile needs no Cargo dependency, native
compression FFI or Serializer runtime library. Explicit caller-owned providers
add optional Zstandard and LZ4 formats.
Ordinary generated models, identifiers, codecs and native wire bytes retain
their existing behavior.

The module is normally `serializer_compression`. The generator chooses `_2`,
`_3` and so on when normalized schema declarations occupy that helper family,
preserving their existing generated names. An emitted comment records the
selected module. Compression format selection remains independent of the native
protocol and schema.

## Owning operations

```rust
use schema::serializer_compression as compression;
use schema::{AccountRecord, Limits, Protocol};

let packed = compression::encode(
    || value.encode(Protocol::BinaryInteger).map_err(compression::into_error),
    compression::EncodeOptions {
        format: compression::Format::Gzip,
        max_input_bytes: 8 * 1024 * 1024,
        max_output_bytes: 9 * 1024 * 1024,
        ..Default::default()
    },
)?;
let candidate = compression::decode(
    &packed,
    |bytes| AccountRecord::decode_with_limits(
        bytes, Protocol::BinaryInteger,
        Limits { max_bytes: 8 * 1024 * 1024, ..Default::default() },
    ).map_err(compression::into_error),
    compression::DecodeOptions {
        format: compression::Format::Gzip,
        max_output_bytes: 8 * 1024 * 1024,
        ..Default::default()
    },
)?;
```

Replace the model and value with generated owning types. `compress(&[u8], options)`
and `decompress(&[u8], options)` provide the same framing for existing native
bytes. `encode` calls an owning encoder after option/provider admission. Its
callback's allocations remain outside compression byte caps. `decode` validates
the entire outer frame, then invokes a fresh native decoder and polls cancellation
before returning the candidate. Application callback side effects cannot be
rolled back.

The helper cannot inspect arbitrary native closures. Select a supported native
protocol and validate application policies before borrowing an input source;
supply an exact-message generated decoder with separate native `Limits`.

| Format | Standard-library profile |
| --- | --- |
| `None` | Identity bytes |
| `Gzip` | One member, including checked optional fields/header CRC on input |
| `Zlib` | One stream, checked header/advertised history/Adler-32 |
| `Deflate` | One raw RFC1951 stream; no checksum |
| `Zstd`, `Lz4` | Explicitly unavailable before native, cancellation or transport callbacks |

`available(format)` performs no callbacks or loading. Missing formats never fall
back to identity or another provider. There is no auto-detection, dictionary
injection, concatenation or implicit transport framing.

## Explicit optional providers

The original `EncodeOptions` and `DecodeOptions` fields remain unchanged,
including exhaustive struct literals. Existing functions and constructors keep
the standard-library profile; linking an adapter does not change
`available(Format::Zstd)` or make an ordinary `compress` call select it.
There is no global provider registry or fallback.

Use `compress_with_provider`, `decompress_with_provider`, `encode_with_provider`,
`decode_with_provider` and `decompress_from_with_provider` with an explicitly
borrowed `&dyn Provider`. Options precede the provider argument; owning callbacks
retain the same position as in the original APIs. `Output::with_provider` and
`Input::with_provider` take transport, options and provider in that order.
The provider must outlive its wrappers. `available_with_provider(format,
Some(&provider))` reports a matching explicit capability; option-specific
admission still occurs before application callbacks or borrowed I/O.

The [optional adapter crate](../../qualification/rust_compression_providers/README.md)
provides hygienic macros that implement
the generated trait for the caller's actual helper module:

```rust
serializer_compression_providers::zstd_provider!(
    zstd, crate::schema::serializer_compression
);

let provider = zstd::Zstd;
let options = compression::EncodeOptions {
    format: compression::Format::Zstd,
    level: 3,
    ..Default::default()
};
let packed = compression::encode_with_provider(
    || value.encode(Protocol::BinaryInteger).map_err(compression::into_error),
    options,
    &provider,
)?;
let candidate = compression::decode_with_provider(
    &packed,
    |bytes| AccountRecord::decode(bytes, Protocol::BinaryInteger)
        .map_err(compression::into_error),
    compression::DecodeOptions {
        format: compression::Format::Zstd,
        ..Default::default()
    },
    &provider,
)?;
```

Enable the crate's explicit `zstd` or `lz4` feature; its default feature set is
empty. `lz4_provider!(module, helper_path)` defines `module::Lz4`. Supply the
actual suffixed helper path when the schema occupies the usual module name.
The pinned adapters use `zstd` **0.14.0** and safe `lz4_flex` **0.14.0** block
APIs. Zstandard builds its bundled native **1.5.7** implementation and needs a
matching native C build toolchain for the selected Cargo feature. LZ4 uses safe
Rust without native code. These are caller-selected dependencies; default
generated Rust still compiles directly with `rustc`.

Zstandard admits its advertised integer interval, including negative fast
levels; the selected provider reports **−131072 through 22**. Choose level 3 for
ordinary Zstandard use. The original options default remains level 6. The pinned
LZ4 fast encoder supports exactly **level 0**: set it explicitly, because other
integers, including the original default, reject before effects. This adapter
does not expose high-compression levels. Another compatible provider can add
them without changing schema syntax or the options struct.

`Provider` admits format, level and window policies, then creates an owned
`Encoder` or `Decoder` context for each operation. Their `process` methods receive
bounded input/output slices and return checked `Progress { bytes_read,
bytes_written, finished }`. Generated code rejects overreported extents,
zero progress and premature completion. Providers receive no application
cancellation or transport callback; generated code polls on both sides of their
calls, preserving the original shared error handle. Custom providers are trusted
application code and must validate availability before admission, retain no
borrowed call slices and keep their capabilities stable through the borrow.

Generated Zstandard validation independently checks one standard frame, dictionary
absence, content size, block extents, checksum position, exact EOF and advertised
history before expansion. The history ceiling rounds down to a power of two,
with a 1 KiB minimum, and is also passed to the native context. Output pledges
the complete known size and includes a content checksum; input accepts checked
and unchecked standard frames. Input/output context calls use at most 32 KiB
slices, with a bounded overflow probe when expanded capacity is exhausted.

Generated LZ4 framing checks descriptor, optional block and content checksums,
exact size/EOF, token expansion and back-reference history before allocation.
Linked blocks receive at most 64 KiB of explicit prior output; independent blocks
reset history. Legal empty raw blocks do not terminate a frame. The declared
64 KiB–4 MiB block maximum must fit the window budget. Decoder calls use an
expanded slice bounded by the admitted block size. Output uses independent
64 KiB blocks with block/content checksums and 128 KiB of private encoder
scratch. Skippable, legacy, external-dictionary, concatenated and trailing frames
reject. Logical caps exclude provider workspace, allocator spare capacity and
process RSS; no provider call is preempted.
The [provider proof](../verification/verification-rust-compression-providers-2026-10-20.md)
records actual Windows/Linux compilation, independent frames, native C++
interoperability, feature deployments, borrow checks and lifecycle controls.

## Bounds and codec

`EncodeOptions` defaults to 64 MiB each of input/output and level 6.
`DecodeOptions` defaults to 64 MiB each of compressed/expanded bytes and an
independent 8 MiB history allowance. Byte limits use `usize`; invalid negative
values cannot be constructed. Standard-library levels are checked, including
identity; levels outside 0–9 reject before callbacks. DEFLATE formats require at least
32 KiB of history admission even for streams advertising a smaller history.

Level zero emits stored blocks. Levels 1–9 emit fixed Huffman blocks with bounded
LZ77 search and at most 32 KiB of history. These effort levels do not promise
zlib's heuristics, compressed sizes or byte sequences. Input accepts stored,
fixed and dynamic Huffman blocks. A grammar pass checks code trees/repeats/end
codes, backward history, actual expanded count and exact frame extent before
reserving expanded output. A second pass expands only that admitted count.
gzip/zlib verify their mandatory checksums and size fields. Truncation, trailing
bytes, concatenation and dictionaries reject. Raw DEFLATE cannot detect every
corrupt change.

Logical limits exclude allocator spare capacity, fixed codec workspace, native
objects and total process RSS. Actual fallible vector reservation failures report
a resource error. The helper stages complete messages and adds generated code
even when unused. Direct native codecs retain their uncompressed paths.

## Borrowed wrappers and errors

```rust
let mut output = compression::Output::new(
    &mut transport,
    compression::EncodeOptions { format: compression::Format::Gzip, ..Default::default() },
)?;
output.encode(|| value.encode(Protocol::Json).map_err(compression::into_error))?;
output.finish()?;
output.close();

let mut input = compression::Input::new(
    &mut source,
    compression::DecodeOptions { format: compression::Format::Gzip, ..Default::default() },
)?;
let candidate = input.decode(
    |bytes| AccountRecord::decode(bytes, Protocol::Json).map_err(compression::into_error),
)?;
```

The wrappers accept standard `Read`/`Write` values; passing mutable references
retains caller ownership. Passing an owned `File` or another owned adapter
intentionally transfers that value into the wrapper; dropping it can then close
that owned transport. Their public methods return compression results and
do not themselves implement `std::io::Read` or `std::io::Write`. Standard-trait
nesting requires an explicit adapter and is outside this initial profile.

`Output.write` and `Output.encode` stage private bytes. Explicit fallible `finish`
prepares one complete frame, then publishes through bounded writes. Legal short
writes make progress; a zero write fails. `flush` only checks state and
cancellation. `close` and destruction never finish or flush the borrowed sink.
An external transport failure can retain a written prefix.

`Input.read(&mut destination)` validates the complete source/frame before
copying expanded bytes. An empty destination performs no source I/O. Standard
`Read` zero means actual EOF; legal short reads are accepted. Each borrowed call
requests at most 32 KiB, with a one-byte overflow probe when the input cap is
reached. `finish` requires all expanded bytes to have been consumed. `Input.decode`
requires unconsumed input and finishes only after its fresh candidate and final
cancellation pass. `decompress_from(&mut source, options)` uses the same lifecycle.
Neither input wrapper operation closes caller-owned transport.

Both options accept a borrowed `Fn() -> Result<()>` cancellation hook, checked
between bounded codec work and around transport/native callbacks, including
cached input and final writes. It cannot preempt running application code.
Methods take exclusive `&mut self` access; safe Rust prevents direct same-wrapper
close/reentry from callbacks. Callbacks must respect that active exclusive borrow.
Interior-mutability containers enforce their own runtime borrow rules, which
the helper does not intercept. A panic that unwinds through an operation leaves
it inactive; panic payloads propagate normally and are not converted into errors.

The module's `Error` is `Arc<dyn std::error::Error + Send + Sync>`.
`into_error` adapts a concrete native/transport error once, retaining its value
and concrete type. Cancellation hooks can return an existing shared handle.
Compression errors downcast to `Failure`, whose `code` is `Unavailable`,
`InvalidOptions`, `InvalidData` or `ResourceLimit`. An active wrapper preserves
its first original error handle; later operations return an inactive `Failure`
with that same handle in `cause`, without further borrowed I/O. Wrappers are
sequential and add no thread synchronization.

The [core SDK qualification](../verification/verification-rust-compression-2026-10-20.md)
records actual Windows/Linux compilers, independent framing and lifecycle checks,
and preserved native output. Broader cumulative operation contexts,
standard-trait nesting, additional provider profiles and performance qualification
remain separate.
