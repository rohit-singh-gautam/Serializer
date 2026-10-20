// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
#if defined(SERIALIZER_COMPRESSION_ZSTD) && SERIALIZER_COMPRESSION_ZSTD
/* Preserve allocation/window categories independently of native provider message text. */
static inline @c@_code @c@_zstd_status(size_t status) {
  if (!ZSTD_isError(status)) { return @c@_ok; }
  const ZSTD_ErrorCode code = ZSTD_getErrorCode(status);
  return code == ZSTD_error_memory_allocation ? @c@_allocation :
      code == ZSTD_error_frameParameter_windowTooLarge ? @c@_resource_limit : @c@_invalid_data;
}
/* Read an explicitly bounded little-endian unsigned wire value. */
static inline uint64_t @c@_zstd_word(const uint8_t* bytes, size_t count) {
  uint64_t result = 0;
  for (size_t index = 0; index < count; ++index) { result |= (uint64_t)bytes[index] << (index*8); }
  return result;
}
/* Validate one standard frame's metadata and block extent before provider workspace admission. */
static inline @c@_code @c@_zstd_frame(const uint8_t* bytes, size_t size,
                                      @c@_decode_options options, int* window_log) {
  if (size < 5 || bytes[0] != 0x28 || bytes[1] != 0xb5 || bytes[2] != 0x2f || bytes[3] != 0xfd) {
    return @c@_invalid_data;
  }
  const unsigned descriptor = bytes[4];
  if (descriptor & 8) { return @c@_invalid_data; }
  const bool single = (descriptor & 32) != 0;
  const unsigned size_flag = descriptor >> 6;
  const unsigned dictionary_flag = descriptor & 3;
  const size_t dictionary_bytes = dictionary_flag == 3 ? 4 : dictionary_flag;
  const size_t content_bytes = size_flag ? (size_t)1 << size_flag : single ? 1 : 0;
  const size_t header_bytes = 5+(single ? 0u : 1u)+dictionary_bytes+content_bytes;
  if (header_bytes > size) { return @c@_invalid_data; }
  size_t position = 5; uint64_t window = 0;
  if (!single) {
    const unsigned value = bytes[position++];
    const uint64_t base = (uint64_t)1 << (10+(value >> 3));
    window = base+(base >> 3)*(value & 7);
  }
  if (dictionary_bytes && @c@_zstd_word(bytes+position, dictionary_bytes)) { return @c@_invalid_data; }
  position += dictionary_bytes;
  uint64_t content = 0;
  if (content_bytes) {
    content = @c@_zstd_word(bytes+position, content_bytes);
    if (content_bytes == 2) { content += 256; }
    if (content > options.max_output_bytes) { return @c@_resource_limit; }
    if (single) { window = content; }
    position += content_bytes;
  }
  int logarithm = 0;
  for (size_t remaining = options.max_window_bytes; remaining > 1; remaining >>= 1) { ++logarithm; }
  const ZSTD_bounds bounds = ZSTD_dParam_getBounds(ZSTD_d_windowLogMax);
  @c@_code status = @c@_zstd_status(bounds.error);
  if (status != @c@_ok) { return status; }
  if (logarithm < bounds.lowerBound) { return @c@_resource_limit; }
  if (logarithm > bounds.upperBound) { logarithm = bounds.upperBound; }
  if (window > ((uint64_t)1 << logarithm)) { return @c@_resource_limit; }
  *window_log = logarithm;
  const uint64_t maximum_block_bytes = window < 128u*1024u ? window : 128u*1024u;
  bool last = false;
  do {
    status = @c@_poll(options.is_cancelled, options.context);
    if (status != @c@_ok) { return status; }
    if (size-position < 3) { return @c@_invalid_data; }
    const uint32_t block = (uint32_t)@c@_zstd_word(bytes+position, 3); position += 3;
    last = (block & 1) != 0;
    const unsigned kind = (block >> 1) & 3;
    const size_t block_bytes = block >> 3;
    if (kind == 3 || block_bytes > maximum_block_bytes) { return @c@_invalid_data; }
    const size_t stored = kind == 1 ? 1 : block_bytes;
    if (stored > size-position) { return @c@_invalid_data; }
    position += stored;
  } while (!last);
  const size_t checksum_bytes = descriptor & 4 ? 4 : 0;
  if (checksum_bytes != size-position) { return @c@_invalid_data; }
  return @c@_poll(options.is_cancelled, options.context);
}
/* Emit bounded standard Zstandard chunks with a pledged content size and checksum. */
static inline @c@_code @c@_zstd_compress(const uint8_t* bytes, size_t size,
                                         @c@_encode_options options, srl_buffer* output) {
  ZSTD_CCtx* context = ZSTD_createCCtx();
  if (!context) { return @c@_allocation; }
  @c@_code status = @c@_zstd_status(ZSTD_CCtx_setParameter(context, ZSTD_c_compressionLevel, options.level));
  if (status == @c@_ok) { status = @c@_zstd_status(ZSTD_CCtx_setParameter(context, ZSTD_c_checksumFlag, 1)); }
  if (status == @c@_ok) { status = @c@_zstd_status(ZSTD_CCtx_setPledgedSrcSize(context, size)); }
  uint8_t scratch[@c@_batch_bytes]; size_t supplied = 0;
  ZSTD_inBuffer source = {NULL, 0, 0};
  while (status == @c@_ok) {
    status = @c@_poll(options.is_cancelled, options.context);
    if (status != @c@_ok) { break; }
    if (source.pos == source.size && supplied < size) {
      const size_t remaining = size-supplied;
      const size_t count = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
      source.src = bytes+supplied; source.size = count; source.pos = 0; supplied += count;
    }
    const size_t before = source.pos;
    ZSTD_outBuffer destination = {scratch, sizeof(scratch), 0};
    const size_t remaining = ZSTD_compressStream2(context, &destination, &source,
                                                  supplied == size ? ZSTD_e_end : ZSTD_e_continue);
    status = @c@_zstd_status(remaining);
    if (status != @c@_ok) { break; }
    status = @c@_poll(options.is_cancelled, options.context);
    if (status == @c@_ok) { status = @c@_append(output, scratch, destination.pos, options.max_output_bytes); }
    if (status != @c@_ok || (!remaining && supplied == size && source.pos == source.size)) { break; }
    if (source.pos == before && !destination.pos) { status = @c@_invalid_data; }
  }
  ZSTD_freeCCtx(context); return status;
}
/* Admit exact framing/history first, then expand with bounded input/output provider calls. */
static inline @c@_code @c@_zstd_decompress(const uint8_t* bytes, size_t size,
                                           @c@_decode_options options, srl_buffer* output) {
  int window_log = 0;
  @c@_code status = @c@_zstd_frame(bytes, size, options, &window_log);
  if (status != @c@_ok) { return status; }
  ZSTD_DCtx* context = ZSTD_createDCtx();
  if (!context) { return @c@_allocation; }
  status = @c@_zstd_status(ZSTD_DCtx_setParameter(context, ZSTD_d_windowLogMax, window_log));
  uint8_t scratch[@c@_batch_bytes]; size_t supplied = 0;
  ZSTD_inBuffer source = {NULL, 0, 0};
  while (status == @c@_ok) {
    status = @c@_poll(options.is_cancelled, options.context);
    if (status != @c@_ok) { break; }
    if (source.pos == source.size && supplied < size) {
      const size_t remaining = size-supplied;
      const size_t count = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
      source.src = bytes+supplied; source.size = count; source.pos = 0; supplied += count;
    }
    const size_t before = source.pos;
    ZSTD_outBuffer destination = {scratch, sizeof(scratch), 0};
    const size_t remaining = ZSTD_decompressStream(context, &destination, &source);
    status = @c@_zstd_status(remaining);
    if (status != @c@_ok) { break; }
    status = @c@_poll(options.is_cancelled, options.context);
    if (status == @c@_ok) { status = @c@_append(output, scratch, destination.pos, options.max_output_bytes); }
    if (status != @c@_ok) { break; }
    if (!remaining) {
      if (source.pos != source.size || supplied != size) { status = @c@_invalid_data; }
      break;
    }
    if (source.pos == before && !destination.pos) { status = @c@_invalid_data; }
  }
  ZSTD_freeDCtx(context); return status;
}
#endif
)serializer"
