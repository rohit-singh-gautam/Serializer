// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
/* Optional complete-message compression. Define selected SERIALIZER_COMPRESSION_*
   providers consistently before generated headers and link their native libraries.
   Identity needs only the C11 standard library. */
#if defined(SERIALIZER_COMPRESSION_ZLIB) && SERIALIZER_COMPRESSION_ZLIB
#include <zlib.h>
#endif
#if defined(SERIALIZER_COMPRESSION_ZSTD) && SERIALIZER_COMPRESSION_ZSTD
#include <zstd.h>
#include <zstd_errors.h>
#endif
#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4
#include <lz4frame.h>
#endif
typedef enum @c@_format { @c@_none, @c@_gzip, @c@_zlib, @c@_deflate, @c@_zstd, @c@_lz4 } @c@_format;
typedef enum @c@_code {
  @c@_ok, @c@_invalid_options, @c@_unavailable, @c@_invalid_data,
  @c@_resource_limit, @c@_cancelled, @c@_allocation, @c@_transport,
  @c@_invalid_state, @c@_codec
} @c@_code;
typedef bool (*@c@_cancel)(void* context);
typedef struct @c@_encode_options {
  @c@_format format;
  size_t max_input_bytes, max_output_bytes;
  int level;
  @c@_cancel is_cancelled;
  void* context;
} @c@_encode_options;
typedef struct @c@_decode_options {
  @c@_format format;
  size_t max_input_bytes, max_output_bytes, max_window_bytes;
  @c@_cancel is_cancelled;
  void* context;
} @c@_decode_options;
enum { @c@_batch_bytes = 32768, @c@_window_bytes = 32768 };
#if defined(SERIALIZER_COMPRESSION_ZLIB) && SERIALIZER_COMPRESSION_ZLIB
/* Validate grammar/history/extent before allocating expanded storage or native provider state. */
static inline @c@_code @c@_validate_frame(const uint8_t*, size_t, @c@_decode_options);
#endif
#if defined(SERIALIZER_COMPRESSION_ZSTD) && SERIALIZER_COMPRESSION_ZSTD
/* Run the admitted optional provider against fresh, unpublished output storage. */
static inline @c@_code @c@_zstd_compress(const uint8_t*, size_t, @c@_encode_options, srl_buffer*);
static inline @c@_code @c@_zstd_decompress(const uint8_t*, size_t, @c@_decode_options, srl_buffer*);
#endif
#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4
/* Run the admitted optional provider against fresh, unpublished output storage. */
static inline @c@_code @c@_lz4_compress(const uint8_t*, size_t, @c@_encode_options, srl_buffer*);
static inline @c@_code @c@_lz4_decompress(const uint8_t*, size_t, @c@_decode_options, srl_buffer*);
#endif

/* Construct an explicit identity policy; other formats require caller selection. */
static inline @c@_encode_options @c@_default_encode_options(void) {
  const @c@_encode_options result = {@c@_none, 64u*1024u*1024u, 256u*1024u*1024u, 6, NULL, NULL};
  return result;
}
/* Keep compressed, expanded and provider history bounds independent. */
static inline @c@_decode_options @c@_default_decode_options(void) {
  const @c@_decode_options result = {@c@_none, 64u*1024u*1024u, 256u*1024u*1024u, @c@_window_bytes, NULL, NULL};
  return result;
}
/* Inspect this compilation unit's provider profile without touching callbacks or I/O. */
static inline bool @c@_available(@c@_format format) {
  if (format == @c@_none) { return true; }
#if defined(SERIALIZER_COMPRESSION_ZLIB) && SERIALIZER_COMPRESSION_ZLIB
  if (format == @c@_gzip || format == @c@_zlib || format == @c@_deflate) { return true; }
#endif
#if defined(SERIALIZER_COMPRESSION_ZSTD) && SERIALIZER_COMPRESSION_ZSTD
  if (format == @c@_zstd) { return true; }
#endif
#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4
  if (format == @c@_lz4) { return true; }
#endif
  return false;
}
/* Validate a selector before checking the optional provider or invoking application code. */
static inline @c@_code @c@_admit(@c@_format format) {
  if (format < @c@_none || format > @c@_lz4) { return @c@_invalid_options; }
  return @c@_available(format) ? @c@_ok : @c@_unavailable;
}
/* Validate provider-specific levels before native codecs, callbacks or transport operations. */
static inline @c@_code @c@_admit_encode(@c@_encode_options options) {
  const @c@_code status = @c@_admit(options.format);
  if (status != @c@_ok) { return status; }
#if defined(SERIALIZER_COMPRESSION_ZSTD) && SERIALIZER_COMPRESSION_ZSTD
  if (options.format == @c@_zstd) {
    return options.level < ZSTD_minCLevel() || options.level > ZSTD_maxCLevel() ? @c@_invalid_options : @c@_ok;
  }
#endif
#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4
  if (options.format == @c@_lz4) {
    return options.level < 0 || options.level > LZ4F_compressionLevel_max() ? @c@_invalid_options : @c@_ok;
  }
#endif
  return options.level < 0 || options.level > 9 ? @c@_invalid_options : @c@_ok;
}
/* Reject impossible provider history policies before touching application state. */
static inline @c@_code @c@_admit_decode(@c@_decode_options options) {
  const @c@_code status = @c@_admit(options.format);
  if (status != @c@_ok) { return status; }
  const size_t minimum = options.format == @c@_none ? 0 :
      options.format == @c@_zstd ? 1024 : options.format == @c@_lz4 ? 65536 : @c@_window_bytes;
  return options.max_window_bytes < minimum ? @c@_resource_limit : @c@_ok;
}
/* Observe cooperative cancellation between bounded transport/provider operations. */
static inline @c@_code @c@_poll(@c@_cancel callback, void* context) {
  return callback && callback(context) ? @c@_cancelled : @c@_ok;
}
/* Retain at most the admitted output extent, including allocator capacity. */
static inline @c@_code @c@_append(srl_buffer* target, const uint8_t* bytes,
                                  size_t size, size_t maximum) {
  if (target->size > maximum || size > maximum-target->size) { return @c@_resource_limit; }
  const size_t required = target->size+size;
  if (required > target->capacity) {
    size_t capacity = target->capacity ? target->capacity : 256;
    if (capacity > maximum) { capacity = maximum; }
    while (capacity < required) {
      capacity = capacity > maximum/2 ? maximum : capacity*2;
    }
    void* allocation = realloc(target->data, capacity);
    if (!allocation) { return @c@_allocation; }
    target->data = (uint8_t*)allocation; target->capacity = capacity;
  }
  if (size) { memcpy(target->data+target->size, bytes, size); }
  target->size = required; return @c@_ok;
}
/* Copy identity bytes in bounded batches while retaining cooperative policy checks. */
static inline @c@_code @c@_copy(srl_buffer* target, const uint8_t* bytes, size_t size,
                                size_t maximum, @c@_cancel callback, void* context) {
  if (size > maximum) { return @c@_resource_limit; }
  for (size_t offset = 0; offset < size;) {
    @c@_code status = @c@_poll(callback, context);
    if (status != @c@_ok) { return status; }
    const size_t remaining = size-offset;
    const size_t count = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
    status = @c@_append(target, bytes+offset, count, maximum);
    if (status != @c@_ok) { return status; }
    offset += count;
  }
  return @c@_poll(callback, context);
}
/* Compress owning bytes transactionally; result must be zeroed or initialized srl_buffer. */
static inline @c@_code @c@_compress(const uint8_t* bytes, size_t size,
                                    @c@_encode_options options, srl_buffer* result) {
  @c@_code status = @c@_admit_encode(options);
  if (status != @c@_ok) { return status; }
  if (!result || (size && !bytes)) { return @c@_invalid_options; }
  if (size > options.max_input_bytes) { return @c@_resource_limit; }
  status = @c@_poll(options.is_cancelled, options.context);
  if (status != @c@_ok) { return status; }
  srl_buffer fresh = {0};
  if (options.format == @c@_none) {
    status = @c@_copy(&fresh, bytes, size, options.max_output_bytes, options.is_cancelled, options.context);
  }
#if defined(SERIALIZER_COMPRESSION_ZLIB) && SERIALIZER_COMPRESSION_ZLIB
  else if (options.format == @c@_gzip || options.format == @c@_zlib || options.format == @c@_deflate) {
    z_stream stream; memset(&stream, 0, sizeof(stream));
    const int window = options.format == @c@_gzip ? 31 : options.format == @c@_zlib ? 15 : -15;
    const int initialized = deflateInit2(&stream, options.level, Z_DEFLATED, window, 8, Z_DEFAULT_STRATEGY);
    if (initialized != Z_OK) { return initialized == Z_MEM_ERROR ? @c@_allocation : @c@_invalid_options; }
    uint8_t output[@c@_batch_bytes]; size_t supplied = 0;
    for (;;) {
      status = @c@_poll(options.is_cancelled, options.context);
      if (status != @c@_ok) { break; }
      if (!stream.avail_in && supplied < size) {
        const size_t remaining = size-supplied;
        const size_t count = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
        stream.next_in = (Bytef*)(bytes+supplied); stream.avail_in = (uInt)count; supplied += count;
      }
      const uInt before = stream.avail_in;
      stream.next_out = output; stream.avail_out = (uInt)sizeof(output);
      const int step = deflate(&stream, supplied == size ? Z_FINISH : Z_NO_FLUSH);
      status = @c@_poll(options.is_cancelled, options.context);
      if (status != @c@_ok) { break; }
      const size_t produced = sizeof(output)-stream.avail_out;
      status = @c@_append(&fresh, output, produced, options.max_output_bytes);
      if (status != @c@_ok || step == Z_STREAM_END) { break; }
      if (step != Z_OK || (before == stream.avail_in && !produced)) {
        status = step == Z_MEM_ERROR ? @c@_allocation : @c@_invalid_data; break;
      }
    }
    deflateEnd(&stream);
  }
#endif
#if defined(SERIALIZER_COMPRESSION_ZSTD) && SERIALIZER_COMPRESSION_ZSTD
  else if (options.format == @c@_zstd) { status = @c@_zstd_compress(bytes, size, options, &fresh); }
#endif
#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4
  else if (options.format == @c@_lz4) { status = @c@_lz4_compress(bytes, size, options, &fresh); }
#endif
  if (status == @c@_ok) { status = @c@_poll(options.is_cancelled, options.context); }
  if (status == @c@_ok) { srl_buffer_free(result); *result = fresh; }
  else { srl_buffer_free(&fresh); }
  return status;
}
/* Decode exactly one frame; prefixes, concatenated streams and dictionaries are rejected. */
static inline @c@_code @c@_decompress(const uint8_t* bytes, size_t size,
                                      @c@_decode_options options, srl_buffer* result) {
  @c@_code status = @c@_admit_decode(options);
  if (status != @c@_ok) { return status; }
  if (!result || (size && !bytes)) { return @c@_invalid_options; }
  if (size > options.max_input_bytes) { return @c@_resource_limit; }
  status = @c@_poll(options.is_cancelled, options.context);
  if (status != @c@_ok) { return status; }
  srl_buffer fresh = {0};
  if (options.format == @c@_none) {
    status = @c@_copy(&fresh, bytes, size, options.max_output_bytes, options.is_cancelled, options.context);
  }
#if defined(SERIALIZER_COMPRESSION_ZLIB) && SERIALIZER_COMPRESSION_ZLIB
  else if (options.format == @c@_gzip || options.format == @c@_zlib || options.format == @c@_deflate) {
    z_stream stream; memset(&stream, 0, sizeof(stream));
    const int window = options.format == @c@_gzip ? 31 : options.format == @c@_zlib ? 15 : -15;
    status = @c@_validate_frame(bytes, size, options);
    if (status != @c@_ok) { return status; }
    const int initialized = inflateInit2(&stream, window);
    if (initialized != Z_OK) { return initialized == Z_MEM_ERROR ? @c@_allocation : @c@_invalid_options; }
    uint8_t output[@c@_batch_bytes]; size_t supplied = 0;
    for (;;) {
      status = @c@_poll(options.is_cancelled, options.context);
      if (status != @c@_ok) { break; }
      if (!stream.avail_in && supplied < size) {
        const size_t remaining = size-supplied;
        const size_t count = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
        stream.next_in = (Bytef*)(bytes+supplied); stream.avail_in = (uInt)count; supplied += count;
      }
      const uInt before = stream.avail_in;
      stream.next_out = output; stream.avail_out = (uInt)sizeof(output);
      const int step = inflate(&stream, Z_NO_FLUSH);
      status = @c@_poll(options.is_cancelled, options.context);
      if (status != @c@_ok) { break; }
      const size_t produced = sizeof(output)-stream.avail_out;
      status = @c@_append(&fresh, output, produced, options.max_output_bytes);
      if (status != @c@_ok) { break; }
      if (step == Z_STREAM_END) {
        if (stream.avail_in || supplied != size) { status = @c@_invalid_data; }
        break;
      }
      if (step != Z_OK || (before == stream.avail_in && !produced)) {
        status = step == Z_MEM_ERROR ? @c@_allocation : @c@_invalid_data; break;
      }
    }
    inflateEnd(&stream);
  }
#endif
#if defined(SERIALIZER_COMPRESSION_ZSTD) && SERIALIZER_COMPRESSION_ZSTD
  else if (options.format == @c@_zstd) { status = @c@_zstd_decompress(bytes, size, options, &fresh); }
#endif
#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4
  else if (options.format == @c@_lz4) { status = @c@_lz4_decompress(bytes, size, options, &fresh); }
#endif
  if (status == @c@_ok) { status = @c@_poll(options.is_cancelled, options.context); }
  if (status == @c@_ok) { srl_buffer_free(result); *result = fresh; }
  else { srl_buffer_free(&fresh); }
  return status;
}
)serializer"
