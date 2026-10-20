# C whole-message compression

[Native C output](native_languages.md) · [Compression contract](../serialization/compression.md)

Compiler/runtime **2.14.0** adds standalone C11 compression helpers to regenerated
headers. Existing model functions and native wire bytes retain their behavior.
The usual prefix is `serializer_compression`; a collision with an emitted model
or callable selects an unused numeric suffix. The generated comment identifies
the actual prefix. The examples below use the usual prefix and an `account_record`
model with its existing C lifecycle functions.

Identity uses only the standard library. To enable gzip, zlib and raw DEFLATE,
define `SERIALIZER_COMPRESSION_ZLIB=1` before including generated headers and
link the application's [zlib provider](https://www.zlib.net/manual.html).
Use the same definition throughout the application. The compiler's C++ CMake
provider options do not configure generated C consumers. Optional Zstandard
and LZ4 use `SERIALIZER_COMPRESSION_ZSTD=1` and `SERIALIZER_COMPRESSION_LZ4=1`
with application linkage to `zstd` and `lz4` respectively. Each disabled provider
reports `serializer_compression_unavailable` before callbacks or I/O; headers
and native symbols for disabled providers are not required.

```c
#define SERIALIZER_COMPRESSION_ZLIB 1
#include "model.h"

serializer_compression_encode_options encode =
    serializer_compression_default_encode_options();
encode.format = serializer_compression_gzip;
encode.max_input_bytes = 8u * 1024u * 1024u;

srl_buffer packed = {0};
srl_status native_status = srl_ok;
serializer_compression_code status = serializer_compression_account_record_encode(
    &value, srl_json, encode, &packed, &native_status);

/* Check status before using packed. destination is already initialized. */
serializer_compression_decode_options decode =
    serializer_compression_default_decode_options();
decode.format = serializer_compression_gzip;
decode.max_output_bytes = 8u * 1024u * 1024u;
srl_limits native_limits = srl_default_limits();
if (status == serializer_compression_ok) {
  status = serializer_compression_account_record_decode(
      &destination, packed.data, packed.size, srl_json, &native_limits,
      decode, &native_status);
}
srl_buffer_free(&packed);
```

Each generated class has prefixed typed encode/decode helpers. A native codec
failure returns `serializer_compression_codec`; the optional `native_status`
output retains its exact `srl_status`. Decode uses a fresh initialized candidate
and publishes it only after exact native consumption and a final cancellation
check. Failure leaves the prior destination unchanged. Initialize/free models
with their existing functions; initialize output buffers with `{0}` and release
them with `srl_buffer_free`.

For already encoded native/XML bytes, use `serializer_compression_compress` and
`serializer_compression_decompress`. Both replace an initialized `srl_buffer`
only on success. Choose the outer format and native protocol independently;
there is no format guessing, dictionary negotiation or concatenated-stream API.

Defaults admit 64 MiB input, 256 MiB output, level 6 and a 32 KiB decode-history
budget. Identity and DEFLATE formats accept levels 0–9. DEFLATE input requires
at least 32 KiB history admission. A fixed-scratch grammar pass checks stored/fixed/dynamic
DEFLATE, actual expanded count, available/declared history and exact byte extent
before creating provider state or allocating expanded output. zlib verifies
wrapper checksums/header CRC and completes exactly one frame. Tiny zlib header
windows are enforced by the grammar pass independently of provider permissiveness.

## Optional Zstandard and LZ4

```c
#define SERIALIZER_COMPRESSION_ZSTD 1
#define SERIALIZER_COMPRESSION_LZ4 1
#include "model.h"
/* Link zstd and lz4; select either format independently on each operation. */
```

Zstandard accepts the linked provider's `ZSTD_minCLevel()` through
`ZSTD_maxCLevel()`, including negative levels. LZ4 accepts zero through
`LZ4F_compressionLevel_max()`. Invalid levels reject before native model encoding,
cancellation callbacks or transport access. Output uses standard frames with
content checksums enabled; LZ4 also enables block checksums and independent
64 KiB blocks. Zstandard pledges the exact content length.

Set `decode.max_window_bytes` explicitly for optional formats: the unchanged
32 KiB default is often insufficient. Zstandard requires at least 1 KiB and
rounds the admitted maximum down to a power of two before configuring the native
decoder. LZ4 requires at least 64 KiB and admits the actual declared block size
(64 KiB, 256 KiB, 1 MiB or 4 MiB). An 8 MiB allowance covers the selected test
profiles; choose application-specific bounds. These are logical history limits,
separate from native provider workspace and total process memory.

An independent metadata pass checks standard magic, exact frame/block extent,
dictionary identifiers, advertised content size and history before creating
decoder state. Nonzero dictionary IDs, skippable/legacy frames, concatenated
frames and trailing bytes reject. Zero dictionary identifiers and Zstandard's
unused descriptor bit remain accepted. LZ4 input supports independent and linked
blocks; the provider validates frame/block/content checksums and history. Calls
feed and receive bounded chunks with cancellation between provider operations.
The [Windows/Linux provider proof](../verification/verification-c-compression-providers-2026-10-20.md)
records actual native versions, six deployment profiles and independent exchanges.

Native model encoding allocates its ordinary native representation before the
compressed input-size check; keep model sizes bounded independently. Byte caps
do not bound model allocations, provider workspace, allocator overhead or total
process RSS. Whole messages are staged. Cancellation uses `bool (*)(void*)` in
the options, with the caller's `context`, and is cooperative between bounded
copy/validation/provider/transport calls. It cannot preempt a blocking callback.

## Borrowed source and sink wrappers

`serializer_compression_output` and `serializer_compression_input` are sequential,
thread-confined objects. Initialize zeroed storage once with `output_init` or
`input_init`, then call the corresponding `close` function before discarding it.
Do not copy active wrappers, mutate their fields/policies, or alias supplied
spans with their owned storage.

The source callback receives `(context, destination, capacity, actual_count)`;
return a compression status and set the count. Short reads are accepted and zero
means real EOF. Input initialization stages the bounded source to EOF, using at
most one byte beyond the admitted input cap to distinguish overflow. It then
validates the frame before exposing expanded bytes through `input_read`.
`input_finish` requires every expanded byte to have been consumed.

The sink callback receives `(context, bytes, count, actual_count)`. Return a
compression status and the complete supplied count; short/zero/oversized writes
are transport failures. `output_write` stages serialized bytes and
`output_finish` creates one frame and delivers bounded chunks. Call finish
explicitly: close abandons output without generating a trailer. Neither wrapper
owns, closes or flushes the transport. An external sink may retain a prefix after
failure.

Every failed active operation fences the wrapper and retains its first status.
Later calls cannot retry a provider or borrowed transport. Same-wrapper callback
reentry and close are detected; deferred close releases owned buffers only after
active code stops borrowing them. Do not initialize the same active object again
from a callback. Keep caller buffers, models and transports stable through calls.
These C status contracts do not add C++ exception handling or `longjmp` cleanup.
