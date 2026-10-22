# Optional message compression

C++ supports standard Zstandard frames, LZ4 frames, gzip members, zlib streams,
and raw DEFLATE streams through optional dependencies. Compression applies once
to the complete serialized message, including nested objects. The inner codec's
bytes, field IDs, and byte order stay unchanged. Existing calls remain uncompressed;
`format::compress` still means JSON whitespace suppression.

See the [usage examples](../guides/usage.md#compress-complete-messages) for generated APIs,
free helpers, exact decoding, and explicit resource limits.
The [runnable compression examples](../../example/compression/README.md) cover every
built-in format, identity output, and a custom backend. Build them independently
with `SERIALIZER_BUILD_COMPRESSION_EXAMPLES=ON` or through the regular test build;
only formats whose dependencies are enabled receive executable targets.

Release **2.3.0** also provides composable C++ `compression::output_stream` and
`input_stream` wrappers. They borrow a compatible underlying stream and serialize
one complete object; byte batches are an internal detail. Output requires an
explicit fallible `finish()` before finishing underlying wrappers. Input stages
and validates one bounded complete message. See the
[stream-wrapper APIs and package composition](xml_packages.md#composable-compression-streams).
Generated Python has equivalent whole-message helpers and borrowed-stream wrappers
from release **2.14.0**; see [its API and provider contract](#generated-python-api).
Generated Java also supplies [whole-message helpers](../languages/java.md#whole-message-compression)
for identity, gzip, zlib and raw DEFLATE, plus optional Zstandard/LZ4 providers.
The table below identifies the other implemented generated helpers. Remaining
generated-language compression work is tracked per backend in
[the backlog](../pending.md#compression-tasks-for-each-language).

## Generated language availability

All listed helpers stage one bounded complete message, preserve ordinary native
codec bytes, validate exact framing and expose borrowed wrappers with explicit
finish. Provider deployment is independent of C++ CMake backend selections.

| Generated output | Core formats | Optional Zstandard/LZ4 profile | API and executed checks |
| --- | --- | --- | --- |
| Python | Identity/gzip/zlib/raw DEFLATE | Python 3.14+ stdlib Zstandard; separately installed `lz4.frame` | [API](#generated-python-api), [proof](../verification/verification-backlog-2026-10-20.md#python-compression-py-z01-through-py-z10) |
| Java | Identity/gzip/zlib/raw DEFLATE | Lazy `zstd-jni` and maintained `lz4-java`; compilation needs neither jar | [API](../languages/java.md#whole-message-compression), [proof](../verification/verification-java-compression-providers-2026-10-20.md) |
| Go | Self-contained Go 1.22+ standard library | Caller-owned per-operation adapters; pinned example requires Go 1.25+ | [API](../languages/go_compression.md), [Windows/Linux proof](../verification/verification-go-compression-providers-2026-10-20.md) |
| C# | .NET 8 identity/gzip/zlib/raw DEFLATE | Explicit caller-selected ZstdSharp.Port/K4os managed assemblies and option factories | [API](../languages/csharp_compression.md), [core proof](../verification/verification-csharp-compression-2026-10-20.md), [provider proof](../verification/verification-csharp-compression-providers-2026-10-20.md) |
| JavaScript/TypeScript | Browser identity; explicit asynchronous Node provider for gzip/zlib/raw DEFLATE | Explicit public Node Zstandard transforms; caller-supplied lz4js block module | [API](../languages/javascript_compression.md), [core proof](../verification/verification-javascript-compression-2026-10-20.md), [provider proof](../verification/verification-javascript-compression-providers-2026-10-20.md) |
| Kotlin | Standard JDK identity/gzip/zlib/raw DEFLATE; JVM 17 target | Lazy `zstd-jni` and safe-Java `lz4-java`; compilation needs neither jar | [API](../languages/kotlin_compression.md), [core proof](../verification/verification-kotlin-compression-2026-10-20.md), [provider proof](../verification/verification-kotlin-compression-providers-2026-10-20.md) |
| Swift | Standalone identity/gzip/zlib/raw DEFLATE without added provider modules | Explicit per-operation contexts, caller-owned C shim and native libraries | [API](../languages/swift_compression.md), [core proof](../verification/verification-swift-compression-2026-10-20.md), [provider proof](../verification/verification-swift-compression-providers-2026-10-20.md) |
| C11 | Standalone identity; compile-time opt-in and application zlib linkage for gzip/zlib/raw DEFLATE | Independent compile definitions and application native linkage | [API](../languages/c_compression.md), [core proof](../verification/verification-c-compression-2026-10-20.md), [provider proof](../verification/verification-c-compression-providers-2026-10-20.md) |
| Rust | Standalone standard-library identity/gzip/zlib/raw DEFLATE; typed wrapper methods | Explicit borrowed providers, pinned opt-in Cargo adapters; LZ4 fast level 0 | [API](../languages/rust_compression.md), [core proof](../verification/verification-rust-compression-2026-10-20.md), [provider proof](../verification/verification-rust-compression-providers-2026-10-20.md) |

Unavailable selections fail before native codec or borrowed transport callbacks.
JS/TS callers explicitly load `await nodeProvider()` for DEFLATE,
`await zstdProvider()` for Zstandard, or pass a block module to `lz4Provider`;
ordinary browser imports and identity never import Node. Await its wrapper
operations sequentially. Go's optional adapters introduce no imports into the
default generated core and install no global registry.

Wrapper activity is checked around policy and transport callbacks. Callback-driven
close/disposal or same-wrapper reentry fences the active operation, including a
nested failure caught by the callback. The active outer operation preserves the
first failure; subsequent inactive operations keep each backend's established
error contract. Cancellation remains cooperative, and logical byte/history caps
do not bound provider workspace, caller allocations or total process RSS.

## Build and dependency options

All three backend options default to ON. Compression remains explicit at the
message boundary; existing calls remain uncompressed. Set any backend option to
OFF to build without that dependency:

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

The repository vcpkg manifest enables `compression-zstd`, `compression-lz4`,
and `compression-zlib` as default features and acquires all three libraries. For
a reduced build, set `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON`, enable any retained
features through `VCPKG_MANIFEST_FEATURES`, and set the corresponding CMake backend
options explicitly. Cached OFF values remain OFF until reset or overridden.
See [CMake integration](../guides/cmake_integration.md) and the
[tested versions/configurations](../verification/verification-compression-2026-09-17.md).

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
advertised maximum block size before block decoding. A zlib frame also retains
its own smaller advertised history bound; a larger caller allowance does not
permit a back-reference beyond that frame contract.

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

`encode_limits::check_cancelled` and `decode_options::check_cancelled` are optional
callbacks that throw to cancel. Built-in backends invoke them at every output
boundary, including batches producing zero bytes; streaming encoders use the same
hook. The allocation-free `cancellation_callback` borrows a named callable object;
const and mutable lvalues are supported, while temporary callables are rejected.
Copies refer to the same original object. Keep that callable valid at a stable
address through the complete call or the last operation on either streaming
wrapper, including input `finish()`.
The default/empty hook is a no-op and preserves literal `constexpr` limit policies.
No synchronization is added around application callbacks. C++ byte-source staging
checks the hook around every validated borrowed read and exact-bound EOF probe;
without a hook it retains the existing direct buffered path. Output checks again
after the final external append, and fresh owning decode checks before returning
its candidate. Streaming encoder reentry fences the outer operation even when
the nested exception is swallowed. Cached input `finish()` polls its retained
hook; a cancellation/reentry failure fences later finish calls and retains the
original cause as a nested exception. A premature finish with unread bytes retains
its established recoverable precondition error. These checks do not roll back
external output or the documented in-place native field updates.
ZIP/OPC supply their shared operation-budget cancellation hook for DEFLATE.
Cancellation exceptions retain their original type, and failed streaming writers
remain fenced. This bounds polling between backend calls, not the execution time
of one backend call. Custom adapters must emit/check bounded batches themselves;
they cannot be preempted while executing arbitrary application code.

## Backend options and extensibility

`zstd_options` defaults to level 3; `lz4_options` to level 0. Both enable content
checksums by default and expose a `checksum` Boolean. `gzip_options`, `zlib_options`,
and `deflate_options` default to DEFLATE level 6. Levels are validated rather than
silently clamped; LZ4 accepts 0 through its backend's maximum. There is no common
level scale. Negative LZ4 acceleration settings, dictionary configuration, and
concatenation options are not implemented.

`compression::available(format)` reports compiled-in built-in formats. Requesting
a disabled backend throws before serialization or input I/O. Include
`<rohit/compression.hpp>` and link `Serializer::runtime` for standalone
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
recompression before transport.

## Generated Python API

Every regenerated Python module provides `__serializer_compression__`. It also
exposes `Compression` when that name is not used by a generated schema type.
Existing models named `Compression` keep their names; use the stable namespace in
reusable integration code. Ordinary generated `encode` and `decode` methods remain
uncompressed and retain their direct paths. Regeneration adds the helper source
even when unused, without eagerly importing compression providers.

```python
compression = models.__serializer_compression__
wire = compression.encode(value, models.Protocol.BINARY_INTEGER,
                          compression.Format.GZIP,
                          limits=compression.EncodeLimits(max_input_bytes=8*1024*1024,
                                                          max_output_bytes=9*1024*1024))
candidate = compression.decode(models.Record, wire, models.Protocol.BINARY_INTEGER,
                               compression.DecodeOptions(compression.Format.GZIP),
                               models.Limits(max_bytes=8*1024*1024))
```

Replace `models`, `Record` and `value` with the generated module, type and object.
`encode` requires a generated object from that same module, so its field tables
and native protocol selection stay consistent. It bounds serialized-buffer growth,
including generated size backpatches, before compression. `decode` validates one
complete frame/member, constructs a fresh native value, requires exact inner EOF,
and checks cancellation again before returning it. Invalid native protocols and
limits reject before input I/O. Use `compress(bytes, format, level=None, limits=None,
checksum=True)` and `decompress(bytes_or_source, options=None)` when working with
already encoded bytes, XML output or another serializer.

| Format | Python provider | Encoding defaults |
| --- | --- | --- |
| `NONE` | No provider | Identity bytes |
| `GZIP`, `ZLIB`, `DEFLATE` | Optional standard-library `zlib` | Level 6, validated range 0–9 |
| `ZSTD` | Optional standard-library `compression.zstd`, Python 3.14+ | Level 3, content checksum on |
| `LZ4` | Separately installed `lz4.frame` | Level 0, 64 KiB linked blocks, content checksum on |

Use `available(format)` to check importability; selecting a missing provider raises
`Error` with `code == 'unavailable'` before stream I/O or model serialization.
Pin external provider dependencies in the application. LZ4 levels accept 0–16;
Zstandard levels use the provider's advertised bounds. There is no shared level
scale. Dictionaries, auto-detection, concatenated members/frames, skippable frames
and legacy LZ4 frames are unsupported. Checksums are verified when present;
raw DEFLATE has no checksum. `checksum=False` applies only to Zstandard/LZ4.

`EncodeLimits` separately defaults to 64 MiB input/output. `DecodeOptions` takes an
explicit format (default `NONE`), 64 MiB compressed/expanded bounds and an 8 MiB
window/block bound. Native `Limits.max_bytes` also bounds expansion before object
decoding. DEFLATE requires a 32 KiB window allowance. Zstandard rounds its window
allowance down to a power of two and validates the advertised frame window before
decoding; LZ4 admits its advertised maximum block size before decoding. Content
sizes never instruct allocations. These bounds cover logical byte storage, not
total process memory, allocator overhead, decoded object allocations or peak
provider workspace. Complete input and expanded messages are staged; encoding
can retain serialized and compressed bytes together.

```python
output = compression.OutputStream(sink, compression.Format.GZIP)
output.write(value.encode(models.Protocol.JSON))
output.finish()

input = compression.InputStream(source, compression.DecodeOptions(compression.Format.GZIP))
candidate = models.Record.decode(input.read(), models.Protocol.JSON)
input.finish()
```

Sources supply `read(size)` returning contiguous bytes; sinks supply
`write(bytes_like)` returning the positive number of bytes accepted. Short reads
and writes are supported. Input validates to source EOF before exposing bytes;
`InputStream.finish()` checks that all expanded bytes were consumed. Finish nested
output wrappers from the codec outward. Neither wrapper closes, flushes or
synchronizes its borrowed stream; destruction emits no trailer. Keep models,
mutable source buffers and borrowed streams unchanged through synchronous use.
No wrapper adds thread synchronization.

`check_cancelled` on either policy is a callable that throws to cancel. It is
polled during serialized-buffer growth and between bounded provider/source/sink
calls, including batches producing no bytes and the final sink write. Input
wrappers retain a policy snapshot and continue polling during staged reads and
explicit finish. Its exception type is preserved;
it cannot preempt a provider or user I/O call. Whole-message helpers return no
partial result. External output may retain a prefix after failure, and source
input can already be consumed. Failed or finished wrappers reject subsequent
operations. Compression `Error.code` distinguishes `invalid_options`,
`unavailable`, `invalid_data` and `resource_limit`; native parser, allocation,
transport and cancellation exceptions retain their own types.

The final selected Windows CPython **3.14.3** profile passed **21** actual generated-code
tests with zlib **1.3.1.zlib-ng**, stdlib Zstandard **1.5.7**, and python-lz4
**4.4.5** using liblz4 **1.9.4**. The run includes present/absent-provider behavior,
independent standard producer/consumer exchanges and all four native protocols.
It includes callback close/reentry and first-failure regressions and records
`PY-Z01` through `PY-Z10`; other platform/provider combinations retain separate
qualification. Generated package/XML operations now share one cumulative context
across all eleven outputs; see the [package pipeline contract](xml_packages.md#generated-package-pipelines).
Selected whole-message
framing/error/limit parity is now
[qualified for all eleven outputs](../verification/verification-compression-parity-2026-10-20.md).
See the
[executed record](../verification/verification-backlog-2026-10-20.md).
The separate [performance slice](../verification/verification-python-compression-performance-2026-10-20.md)
retains reproducible 1 MiB workload timings, provider baselines, encoded/generated
sizes and traced Python allocation peaks. The final generated benchmark module is
**54,799 bytes**. Generated-SDK qualification ran concurrently; these measurements
establish no throughput threshold, RSS bound or complete XML performance result.

## Generated Java API

Release **2.14.0** adds a nested `SerializerCompression` helper to generated Java
files, with an unused suffix when an existing model already owns that name.
It provides identity, gzip, zlib, raw DEFLATE, Zstandard and LZ4 whole-message helpers and
borrowed-stream wrappers, with explicit finish, exact framing and independent
compressed/expanded/native limits. Zstandard uses optional `zstd-jni`; LZ4 uses
the optional maintained `at.yawk.lz4:lz4-java` safe pure-Java APIs, including linked
blocks. Missing selected providers report unavailable before caller I/O. The existing native codec calls remain
uncompressed. See the [Java API, examples and failure contract](../languages/java.md#whole-message-compression)
for callback signatures, dependency versions, ownership and provider bounds.
The [selected provider verification](../verification/verification-java-compression-providers-2026-10-20.md)
records actual emitted-source checks and independent/direct C++ frame exchanges.

References: [Zstandard API](https://facebook.github.io/zstd/zstd_manual.html),
[LZ4 frame format](https://github.com/lz4/lz4/blob/dev/doc/lz4_Frame_format.md),
[zlib API](https://zlib.net/manual.html), and
[GZIP specification](https://www.rfc-editor.org/info/rfc1952/).
