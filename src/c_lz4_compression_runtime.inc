// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
#if defined(SERIALIZER_COMPRESSION_LZ4) && SERIALIZER_COMPRESSION_LZ4
/* Read bounded unsigned little-endian metadata independently of the native object layout. */
static inline uint64_t @c@_lz4_word(const uint8_t* bytes, size_t count) {
  uint64_t result = 0;
  for (size_t index = 0; index < count; ++index) { result |= (uint64_t)bytes[index] << (index*8); }
  return result;
}
/* Check exact frame extent, dictionary/block admission and advertised size before provider allocation. */
static inline @c@_code @c@_lz4_frame(const uint8_t* bytes, size_t size,
                                     @c@_decode_options options) {
  if (size < 7 || bytes[0] != 4 || bytes[1] != 0x22 || bytes[2] != 0x4d || bytes[3] != 0x18) {
    return @c@_invalid_data;
  }
  const unsigned flags = bytes[4], descriptor = bytes[5];
  if ((flags >> 6) != 1 || (flags & 2) || (descriptor & 0x8f)) { return @c@_invalid_data; }
  const unsigned block_code = (descriptor >> 4) & 7;
  if (block_code < 4) { return @c@_invalid_data; }
  const size_t block_bytes = (size_t)1 << (8+block_code*2);
  if (block_bytes > options.max_window_bytes) { return @c@_resource_limit; }
  size_t position = 6;
  if (flags & 8) {
    if (size-position < 8) { return @c@_invalid_data; }
    const uint64_t content = @c@_lz4_word(bytes+position, 8); position += 8;
    if (content > options.max_output_bytes) { return @c@_resource_limit; }
  }
  if (flags & 1) {
    if (size-position < 4) { return @c@_invalid_data; }
    if (@c@_lz4_word(bytes+position, 4)) { return @c@_invalid_data; } position += 4;
  }
  if (position == size) { return @c@_invalid_data; }
  ++position;
  for (;;) {
    const @c@_code status = @c@_poll(options.is_cancelled, options.context);
    if (status != @c@_ok) { return status; }
    if (size-position < 4) { return @c@_invalid_data; }
    const uint32_t block = (uint32_t)@c@_lz4_word(bytes+position, 4); position += 4;
    if (!block) { break; }
    const size_t count = block & 0x7fffffffU;
    if (count > block_bytes || count > size-position) { return @c@_invalid_data; }
    position += count;
    if (flags & 16) {
      if (size-position < 4) { return @c@_invalid_data; } position += 4;
    }
  }
  const size_t checksum_bytes = flags & 4 ? 4 : 0;
  if (checksum_bytes != size-position) { return @c@_invalid_data; }
  return @c@_poll(options.is_cancelled, options.context);
}
/* Emit one checked 64-KiB-independent-block frame through bounded provider input batches. */
static inline @c@_code @c@_lz4_compress(const uint8_t* bytes, size_t size,
                                        @c@_encode_options options, srl_buffer* output) {
  LZ4F_preferences_t preferences; memset(&preferences, 0, sizeof(preferences));
  preferences.frameInfo.blockSizeID = LZ4F_max64KB;
  preferences.frameInfo.blockMode = LZ4F_blockIndependent;
  preferences.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
  preferences.frameInfo.blockChecksumFlag = LZ4F_blockChecksumEnabled;
  preferences.frameInfo.contentSize = size;
  preferences.compressionLevel = options.level;
  const size_t bound = LZ4F_compressBound(@c@_batch_bytes, &preferences);
  if (LZ4F_isError(bound)) { return @c@_invalid_options; }
  const size_t capacity = bound < LZ4F_HEADER_SIZE_MAX ? LZ4F_HEADER_SIZE_MAX : bound;
  uint8_t* scratch = (uint8_t*)malloc(capacity);
  if (!scratch) { return @c@_allocation; }
  LZ4F_cctx* context = NULL;
  size_t step = LZ4F_createCompressionContext(&context, LZ4F_VERSION);
  @c@_code status = LZ4F_isError(step) ? @c@_allocation : @c@_ok;
  if (status == @c@_ok) {
    step = LZ4F_compressBegin(context, scratch, capacity, &preferences);
    status = LZ4F_isError(step) ? @c@_invalid_data : @c@_append(output, scratch, step, options.max_output_bytes);
  }
  for (size_t position = 0; status == @c@_ok && position < size;) {
    status = @c@_poll(options.is_cancelled, options.context);
    if (status != @c@_ok) { break; }
    const size_t remaining = size-position;
    const size_t count = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
    step = LZ4F_compressUpdate(context, scratch, capacity, bytes+position, count, NULL);
    status = LZ4F_isError(step) ? @c@_invalid_data : @c@_poll(options.is_cancelled, options.context);
    if (status == @c@_ok) { status = @c@_append(output, scratch, step, options.max_output_bytes); }
    position += count;
  }
  if (status == @c@_ok) {
    status = @c@_poll(options.is_cancelled, options.context);
    if (status == @c@_ok) {
      step = LZ4F_compressEnd(context, scratch, capacity, NULL);
      status = LZ4F_isError(step) ? @c@_invalid_data : @c@_poll(options.is_cancelled, options.context);
      if (status == @c@_ok) { status = @c@_append(output, scratch, step, options.max_output_bytes); }
    }
  }
  if (context) { LZ4F_freeCompressionContext(context); }
  free(scratch); return status;
}
/* Validate metadata before creating provider state and preserve linked-block history privately. */
static inline @c@_code @c@_lz4_decompress(const uint8_t* bytes, size_t size,
                                          @c@_decode_options options, srl_buffer* output) {
  @c@_code status = @c@_lz4_frame(bytes, size, options);
  if (status != @c@_ok) { return status; }
  LZ4F_dctx* context = NULL;
  size_t step = LZ4F_createDecompressionContext(&context, LZ4F_VERSION);
  if (LZ4F_isError(step)) { return @c@_allocation; }
  LZ4F_frameInfo_t info; memset(&info, 0, sizeof(info));
  size_t position = size;
  step = LZ4F_getFrameInfo(context, &info, bytes, &position);
  if (LZ4F_isError(step)) { status = @c@_invalid_data; }
  uint8_t scratch[@c@_batch_bytes];
  while (status == @c@_ok) {
    status = @c@_poll(options.is_cancelled, options.context);
    if (status != @c@_ok) { break; }
    const size_t remaining = size-position;
    size_t consumed = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
    size_t produced = sizeof(scratch);
    step = LZ4F_decompress(context, scratch, &produced, bytes+position, &consumed, NULL);
    status = LZ4F_isError(step) ? @c@_invalid_data : @c@_poll(options.is_cancelled, options.context);
    if (status != @c@_ok) { break; }
    status = @c@_append(output, scratch, produced, options.max_output_bytes);
    if (status != @c@_ok) { break; }
    position += consumed;
    if (!step) {
      if (position != size) { status = @c@_invalid_data; }
      break;
    }
    if (!consumed && !produced) { status = @c@_invalid_data; }
  }
  LZ4F_freeDecompressionContext(context); return status;
}
#endif
)serializer"
