# Optional message compression

C++ supports standard Zstandard frames, LZ4 frames, gzip members, zlib streams,
and raw DEFLATE streams through optional dependencies. Compression applies once
to the complete serialized message, including nested objects. The inner codec's
bytes, field IDs, and byte order stay unchanged. Existing calls remain uncompressed;
`format::compress` still means JSON whitespace suppression.

See the [usage examples](usage.md#compress-complete-messages) for generated APIs,
free helpers, exact decoding, and explicit resource limits.

## Build and dependency options

All options default to OFF:

| CMake option | Dependency target | Formats |
| --- | --- | --- |
| `SERIALIZER_WITH_ZSTD=ON` | zstd package: `zstd::libzstd_static` or `zstd::libzstd_shared` | Zstandard frame |
| `SERIALIZER_WITH_LZ4=ON` | lz4 package: `LZ4::lz4` | LZ4 frame |
| `SERIALIZER_WITH_ZLIB=ON` | CMake FindZLIB: `ZLIB::ZLIB` | gzip, zlib, raw DEFLATE |

Install packages supplying these targets, or use the upstream CMake builds and
add their installation prefixes to `CMAKE_PREFIX_PATH`. Set the options before
`add_subdirectory` for embedded builds. Installed Serializer packages discover
their enabled dependencies, including for static consumers. Consumer source needs
no third-party compression headers. Missing enabled dependencies fail configuration.

The optional vcpkg manifest features are `compression-zstd`, `compression-lz4`,
and `compression-zlib`. Select matching features through `VCPKG_MANIFEST_FEATURES`
as well as the CMake options; the default manifest does not acquire these libraries.
See [CMake integration](cmake_integration.md) and the
[tested versions/configurations](verification-compression-2026-09-17.md).

## Format and resource contract

Output is the standard format itself, without a Serializer envelope. gzip, zlib,
and raw DEFLATE are distinct choices. Readers select the expected format explicitly;
there is no auto-detection or silent fallback to uncompressed bytes. Schema,
inner protocol, byte order, and message boundaries are agreed separately. Schema
language version 1 identifies neither compression nor message format.

The API handles **one frame/member per message**. It rejects truncation, invalid
checksums when present, trailing bytes, concatenated frames/members, skippable
Zstandard/LZ4 frames, and legacy LZ4 frames. This is a single-member gzip profile,
not support for every valid multi-member gzip file. Raw DEFLATE has no checksum
and cannot detect every corrupt change. Built-in adapters supply no dictionaries.

Both member and fresh-value compression overloads call the inner decoder's
`finish()` after decompression. Protobuf's documented valid-message merging
behavior still applies. A buffer's unread suffix or an EOF-delimited byte source
must contain exactly one compressed message.

`encode_limits` defaults to 64 MiB of serialized input and 64 MiB of compressed
output. Allow for frame overhead and expansion when choosing output bounds.
`decode_options` separately bounds compressed input (64 MiB), actual decompressed
output (64 MiB), and backend history/block size (8 MiB). Expanded output is also
bounded by `decode_limits.max_input_bytes` before parsing. Advertised content sizes
are never trusted as allocation instructions. Zstandard's window bound rounds
down to a power of two; DEFLATE requires at least a 32 KiB budget; LZ4 checks the
advertised maximum block size before block decoding.

These are logical byte/window bounds, not total heap or CPU limits. Backend state,
scratch chunks, vector spare capacity, serialized staging, and decoded objects
occupy additional memory. Both directions currently stage bounded whole messages;
output can hold serialized and compressed bytes together. Chunked backend calls
do not make the public API an incremental decoder. Existing uncompressed calls
retain their direct path.

Compression failures occur before publishing output. Buffer output uses one append;
reservation failure preserves its previous contents and cursor. External I/O can
write a prefix. Frame-validation failures precede field decoding. Buffer input
advances after decompression succeeds, so later parsing failures can leave input
consumed. Byte-source failures can consume input earlier. Native member decoding
can still partially update fields. Fresh-value decoding returns only after full
validation; a later assignment to an existing object can itself throw.

`compression::error` carries `unavailable`, `invalid_options`, `invalid_data`, or
`resource_limit`, without payload excerpts. Allocation, stream, and inner-parser
exceptions retain their existing types. Compression storage is separate from the
inner decoder's object-allocation accounting.

## Backend options and extensibility

`zstd_options` defaults to level 3; `lz4_options` to level 0. Both enable content
checksums by default and expose a `checksum` Boolean. `gzip_options`, `zlib_options`,
and `deflate_options` default to DEFLATE level 6. Levels are validated rather than
silently clamped; LZ4 accepts 0 through its backend's maximum. There is no common
level scale. Negative LZ4 acceleration settings, dictionary configuration, and
concatenation options are not implemented.

`compression::available(format)` reports compiled-in built-in formats. Requesting
a disabled backend throws before serialization or input I/O. Include
`<rohit/compression.hpp>` and link `Serializer::serializer_lib` for standalone
`compress(span, options[, limits])` and `decompress(span, decode_options)`; both
return owned byte vectors.

Additional formats implement `compression::backend`, emitting chunks through the
provided `output_sink`. Pass `custom_options{&adapter}` for encoding, and
`decode_options{.format = format::custom, .custom_backend = &adapter}` for decoding.
The adapter is borrowed only during the call. The sink enforces output limits;
the adapter must validate exactly one complete stream, enforce its window policy,
manage its own allocations, and document thread safety. No global registration or
generator changes are needed. Brotli, XZ, bzip2, and other adapters are not bundled.

Views require stable decompressed storage: decompress first, then map the resulting
bytes. Preserve the vector's lifetime/address while views exist; edits need
recompression before transport. Java's generated APIs have no new compression
options; external standard libraries can wrap their encoded bytes.

References: [Zstandard API](https://facebook.github.io/zstd/zstd_manual.html),
[LZ4 frame format](https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md),
[zlib API](https://zlib.net/manual.html), and
[GZIP specification](https://www.rfc-editor.org/info/rfc1952/).
