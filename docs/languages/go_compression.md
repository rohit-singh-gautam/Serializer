# Generated Go whole-message compression

Compiler/runtime **2.14.0** emits owning compression helpers alongside the
existing Go codecs. Generated Go targets **Go 1.22+**, uses only the standard
library, and needs no C++ runtime, cgo or downloaded modules. Existing native
Encode and Decode calls retain their bytes and direct path.

The default provider profile supports identity, gzip, zlib and raw DEFLATE.
Zstandard and LZ4 frames use explicitly supplied optional providers. With no
matching provider, selecting either returns unavailable before native encoding
or source/sink I/O. There is no format detection or fallback.

## Owning helpers

The usual public helper prefix is SerializerCompression. If an existing schema
declaration or native helper occupies any name in that family, the generator
tries SerializerCompression_2, then _3, preserving existing declarations. The
generated comment "Generated compression helper prefix: ..." identifies the
complete selected family. The examples use the usual prefix.

    value := model.NewAccountRecord()
    value.AccountId = 42
    value.Label = "complete native message"

    encode := model.SerializerCompressionDefaultEncodeOptions(
        model.SerializerCompressionGzip)
    packed, err := model.SerializerCompressionEncode(value, model.JSON, encode)
    if err != nil {
        return err
    }

    decode := model.SerializerCompressionDefaultDecodeOptions(
        model.SerializerCompressionGzip)
    decode.Limits.MaxBytes = 1024 * 1024
    candidate, err := model.SerializerCompressionDecode(
        packed, model.JSON, model.DecodeAccountRecord, decode)
    if err != nil {
        return err
    }

The generic decode helper accepts this generated module's existing
DecodeAccountRecord function and returns its exact pointer type. It copies
Limits, returns the zero result on any failure, and checks cancellation after
the native callback before returning its candidate. Application callbacks must
preserve that native fresh-value contract; their external side effects cannot
be rolled back. Invalid or generation-disabled native protocols fail before
decoding or input I/O.

SerializerCompressionCompress and SerializerCompressionDecompress expose the
same framing for ordinary byte messages. Successful identity operations return
owned bytes. The outer format, native protocol, schema and boundary remain
separate choices.

## Borrowed streams

SerializerCompressionNewOutput accepts an io.Writer and encode options. It
returns a staged io.Writer with Encode and explicit fallible Finish methods.
No compressed bytes reach the sink until the complete frame is ready.

    var sink bytes.Buffer
    output, err := model.SerializerCompressionNewOutput(&sink, encode)
    if err != nil {
        return err
    }
    if err = output.Encode(value, model.JSON); err != nil {
        return err
    }
    if err = output.Finish(); err != nil {
        return err
    }

    input, err := model.SerializerCompressionNewInput(
        bytes.NewReader(sink.Bytes()), decode)
    if err != nil {
        return err
    }
    candidate, err := model.SerializerCompressionDecodeFrom(
        input, model.JSON, model.DecodeAccountRecord)
    if err != nil {
        return err
    }

Input borrows an EOF-delimited io.Reader, stages bounded compressed bytes,
validates the complete frame, and exposes plaintext in bounded read batches.
Plain Read users must consume all plaintext and call Finish. DecodeFrom
performs that finish check itself. A wrapper retains its first failure and
never resumes I/O afterward. Successful output finish is idempotent.
Short sink writes follow io.Writer's io.ErrShortWrite contract; zero-progress
sources eventually return io.ErrNoProgress.

The wrappers never close the borrowed source or sink. External I/O can consume
input or publish an output prefix before failing. Whole-message helpers and
fresh typed decoding return no partial owned result. Wrappers and callbacks
require application synchronization if shared between goroutines.
Callbacks must not synchronously operate on the same wrapper. Such reentry fails
with invalid_options, fences both the nested and outer operations, and prevents
later borrowed I/O or successful publication. A wrapper retains that first error
even if the callback ignores the nested failure. These guards add no concurrency
synchronization. Go wrappers expose Finish and never own a borrowed Close.

## Framing, bounds and cancellation

Input requires exactly one gzip member, one zlib stream or one raw-DEFLATE
stream. Truncation, invalid headers/checksums, dictionaries, concatenation and
trailing compressed bytes fail. Gzip optional fields and header CRC are checked
within the compressed-input budget, including strings longer than the standard
Go gzip reader's own optional-header string limit. Raw DEFLATE has no checksum
and cannot detect every corrupt bit change.

For a zlib header advertising less than 32 KiB, a bounded grammar/count pass
rejects any back-reference beyond that advertised history before the standard
decoder runs. It uses fixed scratch and cooperative cancellation, without
expanding bytes. The provider still verifies the Adler checksum. Normal 32 KiB
zlib, gzip and raw-DEFLATE decoding retain their existing provider paths.

Encode options default to 64 MiB each for MaxInputBytes and MaxOutputBytes.
Decode options independently default to 64 MiB each for compressed input and
expanded output, plus an 8 MiB MaxWindowBytes budget. DEFLATE-family formats
require at least 32 KiB of history budget for the selected standard-library
provider, including frames advertising a smaller logical window. Fresh native
decoding also caps expanded bytes by Limits.MaxBytes and preserves native string,
element, depth and read-policy checks.

DEFLATE-family output uses level 6 by default and accepts levels 0 through 9.
Identity options use level zero to represent the absence of a compression level.
All format-specific options and native decode limits are validated before I/O.

## Optional Zstandard and LZ4 providers

EncodeOptions and DecodeOptions each accept a Provider implementing the selected
family's Provider interface. This is a per-operation value; there is no global
registry. The provider must remain immutable during the operation. Available
continues to report the default standard-library profile. AvailableWithProvider
also checks an explicitly supplied provider's format; option/deployment admission
still occurs through ValidateEncode or ValidateDecode before application work.

The [maintained adapter example](../../qualification/go_compression_providers/providers.go)
uses pure-Go klauspost/compress **1.20.3** for Zstandard and pierrec/lz4/v4
**4.1.33** for LZ4. Its [module and lockfile](../../qualification/go_compression_providers/go.mod)
pin these optional imports. The combined example requires **Go 1.25+**; the
generated core remains standard-library-only **Go 1.22+**. Adapt the example's
package and helper prefix to the generated module before using it.

    encode := model.SerializerCompressionDefaultEncodeOptions(
        model.SerializerCompressionZstd)
    encode.Provider = model.ZstdProvider{}
    encode.Checksum = true
    packed, err := model.SerializerCompressionEncode(value, model.JSON, encode)

    decode := model.SerializerCompressionDefaultDecodeOptions(
        model.SerializerCompressionZstd)
    decode.Provider = model.ZstdProvider{}
    candidate, err := model.SerializerCompressionDecode(
        packed, model.JSON, model.DecodeAccountRecord, decode)

Use Lz4Provider with the Lz4 format in the same way. Optional formats default to
Checksum true. The adapter admits Zstandard levels 1–22 and maps them through the
provider's four speed tiers; these are not exact native-libzstd level settings.
LZ4 accepts level 0 for fast compression and levels 1–9 for its corresponding
high-compression modes. Supplying an external provider or Checksum true for a
standard-library format is invalid_options.

Before expansion, the generated core independently admits one complete standard
frame. It rejects legacy/skippable frames, unsupported dictionaries, reserved
fields, truncation, concatenation and trailing data. It checks LZ4 descriptor and
block checksums, known content sizes and advertised history before calling the
provider, then checks known expanded size and content integrity before returning
plaintext. Output frames must match the requested format and checksum policy.
The selected LZ4 adapter accepts both independent and linked input blocks.

Zstandard rounds MaxWindowBytes down to a power of two, matching the native
reference profile, and requires at least 1 KiB. LZ4 admits its advertised maximum
block against MaxWindowBytes and requires at least 64 KiB. The adapters use one
worker, bounded read/write batches and provider history limits; Zstandard's eager
whole-buffer decode path is disabled. These settings bound codec work units and
frame history without promising a process-wide memory ceiling.

Providers are trusted application code. Their validation methods must report
unavailable deployment or invalid configuration before any native callback or
borrowed I/O; their codec methods must stop on sink errors, poll cancellation
between bounded batches and release their private resources. Arbitrary provider
side effects and uninterrupted internal calls cannot be rolled back or preempted.

CheckCancelled is an optional function returning an application error; it can
be set to context.Context.Err. Its original error is returned unchanged.
Polling occurs between bounded batches and after application source/sink/native
callbacks, including the final sink call and later input reads/finish. One
provider call or application callback cannot be preempted by this policy.

Compression failures use the selected family's Error type and one of
unavailable, invalid_options, invalid_data or resource_limit. Messages contain
no payload excerpts. Source/sink errors, native codec errors and cancellation
errors retain their original types and identity.

These are logical byte/history bounds. Both directions stage whole messages;
provider state, slice capacity, copies and decoded objects use additional memory.
Native encoding retains its existing allocation contract before its staged bytes
are checked for compression. Full shared-operation accounting, hard peak memory
and preemption guarantees remain pending.

The [Go verification record](../verification/verification-go-compression-2026-10-20.md)
lists executed SDKs and independent fixtures. See the
[advertised-window repair verification](../verification/verification-go-zlib-window-2026-10-20.md)
for the selected zlib grammar checks, the
[optional-provider verification record](../verification/verification-go-compression-providers-2026-10-20.md)
for the selected external module profile and the
[common compression contract](../serialization/compression.md) for other backends.
