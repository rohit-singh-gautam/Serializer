/* Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in) */
/* SPDX-License-Identifier: 0BSD */
#include "provider.h"
#include <stdlib.h>
#include <string.h>
#include <zstd.h>
#include <zstd_errors.h>
#include <lz4frame.h>
#include <lz4.h>
enum { serializer_provider_batch_bytes = 32768, serializer_provider_zstd = 1, serializer_provider_lz4 = 2 };
struct serializer_swift_provider {
  int format;
  int encoding;
  int started;
  int ended;
  ZSTD_CCtx* zstd_encoder;
  ZSTD_DCtx* zstd_decoder;
  LZ4F_cctx* lz4_encoder;
  LZ4F_dctx* lz4_decoder;
  LZ4F_preferences_t preferences;
  uint8_t* pending;
  size_t pending_capacity_bytes;
  size_t pending_size_bytes;
  size_t pending_position_bytes;
};
/* Preserve native allocation/history categories separately from invalid frame data. */
static int serializer_swift_zstd_status(size_t result) {
  if (!ZSTD_isError(result)) { return 0; }
  const ZSTD_ErrorCode code = ZSTD_getErrorCode(result);
  if (code == ZSTD_error_memory_allocation) { return -3; }
  if (code == ZSTD_error_frameParameter_windowTooLarge) { return -2; }
  switch (code) {
    case ZSTD_error_prefix_unknown:
    case ZSTD_error_version_unsupported:
    case ZSTD_error_frameParameter_unsupported:
    case ZSTD_error_corruption_detected:
    case ZSTD_error_checksum_wrong:
    case ZSTD_error_literals_headerWrong:
    case ZSTD_error_dictionary_corrupted:
    case ZSTD_error_dictionary_wrong:
    case ZSTD_error_tableLog_tooLarge:
    case ZSTD_error_maxSymbolValue_tooLarge:
    case ZSTD_error_maxSymbolValue_tooSmall:
    case ZSTD_error_srcSize_wrong:
      return -1;
    default:
      return -4;
  }
}
/* Classify only pinned public frame-error names; unrelated provider failures retain their category. */
static int serializer_swift_lz4_status(size_t result) {
  if (!LZ4F_isError(result)) { return 0; }
  const char* name = LZ4F_getErrorName(result);
  if (strcmp(name, "ERROR_allocation_failed") == 0) { return -3; }
  static const char* const frame_errors[] = {
    "ERROR_maxBlockSize_invalid", "ERROR_blockMode_invalid", "ERROR_headerVersion_wrong",
    "ERROR_blockChecksum_invalid", "ERROR_reservedFlag_set", "ERROR_frameHeader_incomplete",
    "ERROR_frameType_unknown", "ERROR_frameSize_wrong", "ERROR_decompressionFailed",
    "ERROR_headerChecksum_invalid", "ERROR_contentChecksum_invalid"
  };
  for (size_t index = 0; index < sizeof(frame_errors)/sizeof(frame_errors[0]); ++index) {
    if (strcmp(name, frame_errors[index]) == 0) { return -1; }
  }
  return -4;
}
/* Release every successfully created native resource after any partial initialization. */
void serializer_swift_provider_close(struct serializer_swift_provider* state) {
  if (!state) { return; }
  ZSTD_freeCCtx(state->zstd_encoder);
  ZSTD_freeDCtx(state->zstd_decoder);
  if (state->lz4_encoder) { LZ4F_freeCompressionContext(state->lz4_encoder); }
  if (state->lz4_decoder) { LZ4F_freeDecompressionContext(state->lz4_decoder); }
  free(state->pending);
  free(state);
}
/* Allocate only caller-selected contexts and bounded encoder scratch. */
struct serializer_swift_provider* serializer_swift_provider_create(int format, int encoding, int level, size_t size, int window_log) {
  struct serializer_swift_provider* state = calloc(1, sizeof(*state));
  if (!state) { return NULL; }
  state->format = format; state->encoding = encoding;
  if (format == serializer_provider_zstd) {
    if (encoding) {
      state->zstd_encoder = ZSTD_createCCtx();
      if (!state->zstd_encoder || ZSTD_isError(ZSTD_CCtx_setParameter(state->zstd_encoder, ZSTD_c_compressionLevel, level)) ||
          ZSTD_isError(ZSTD_CCtx_setParameter(state->zstd_encoder, ZSTD_c_checksumFlag, 1)) ||
          ZSTD_isError(ZSTD_CCtx_setPledgedSrcSize(state->zstd_encoder, size))) { goto failure; }
    } else {
      const ZSTD_bounds bounds = ZSTD_dParam_getBounds(ZSTD_d_windowLogMax);
      if (bounds.error || window_log < bounds.lowerBound) { goto failure; }
      if (window_log > bounds.upperBound) { window_log = bounds.upperBound; }
      state->zstd_decoder = ZSTD_createDCtx();
      if (!state->zstd_decoder || ZSTD_isError(ZSTD_DCtx_setParameter(state->zstd_decoder, ZSTD_d_windowLogMax, window_log))) { goto failure; }
    }
  } else if (format == serializer_provider_lz4) {
    if (encoding) {
      state->preferences.frameInfo.blockSizeID = LZ4F_max64KB;
      state->preferences.frameInfo.blockMode = LZ4F_blockIndependent;
      state->preferences.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
      state->preferences.frameInfo.blockChecksumFlag = LZ4F_blockChecksumEnabled;
      state->preferences.frameInfo.contentSize = size;
      state->preferences.compressionLevel = level;
      const size_t bound = LZ4F_compressBound(serializer_provider_batch_bytes, &state->preferences);
      if (LZ4F_isError(bound)) { goto failure; }
      state->pending_capacity_bytes = bound < LZ4F_HEADER_SIZE_MAX ? LZ4F_HEADER_SIZE_MAX : bound;
      state->pending = malloc(state->pending_capacity_bytes);
      if (!state->pending || LZ4F_isError(LZ4F_createCompressionContext(&state->lz4_encoder, LZ4F_VERSION))) { goto failure; }
    } else if (LZ4F_isError(LZ4F_createDecompressionContext(&state->lz4_decoder, LZ4F_VERSION))) { goto failure; }
  } else { goto failure; }
  return state;
failure:
  serializer_swift_provider_close(state);
  return NULL;
}
/* Drain private frame bytes into one bounded borrowed output span. */
static void serializer_swift_pending(struct serializer_swift_provider* state, uint8_t* output, size_t* produced) {
  const size_t remaining = state->pending_size_bytes - state->pending_position_bytes;
  if (*produced > remaining) { *produced = remaining; }
  if (*produced) { memcpy(output, state->pending + state->pending_position_bytes, *produced); }
  state->pending_position_bytes += *produced;
}
/* Advance only admitted 32 KiB spans; native frame calls never receive application callbacks. */
int serializer_swift_provider_step(struct serializer_swift_provider* state, const uint8_t* input, size_t* consumed,
                                 uint8_t* output, size_t* produced, int finish) {
  if (!state || !consumed || !produced || *consumed > serializer_provider_batch_bytes ||
      *produced > serializer_provider_batch_bytes || !output || (!input && *consumed)) { return -1; }
  if (state->format == serializer_provider_zstd) {
    ZSTD_inBuffer source = {input, *consumed, 0};
    ZSTD_outBuffer destination = {output, *produced, 0};
    const size_t result = state->encoding ? ZSTD_compressStream2(state->zstd_encoder, &destination, &source, finish ? ZSTD_e_end : ZSTD_e_continue) :
                                          ZSTD_decompressStream(state->zstd_decoder, &destination, &source);
    *consumed = source.pos; *produced = destination.pos;
    const int status = serializer_swift_zstd_status(result);
    if (status) { return status; }
    return !result && (!state->encoding || finish) ? 1 : 0;
  }
  if (!state->encoding) {
    const size_t result = LZ4F_decompress(state->lz4_decoder, output, produced, input, consumed, NULL);
    const int status = serializer_swift_lz4_status(result);
    return status ? status : result == 0 ? 1 : 0;
  }
  if (state->pending_position_bytes < state->pending_size_bytes) {
    *consumed = 0; serializer_swift_pending(state, output, produced);
    return state->ended && state->pending_position_bytes == state->pending_size_bytes ? 1 : 0;
  }
  size_t result;
  if (!state->started) {
    result = LZ4F_compressBegin(state->lz4_encoder, state->pending, state->pending_capacity_bytes, &state->preferences);
    state->started = 1; *consumed = 0;
  } else if (*consumed) {
    result = LZ4F_compressUpdate(state->lz4_encoder, state->pending, state->pending_capacity_bytes, input, *consumed, NULL);
  } else if (finish && !state->ended) {
    result = LZ4F_compressEnd(state->lz4_encoder, state->pending, state->pending_capacity_bytes, NULL);
    state->ended = 1;
  } else { *produced = 0; return state->ended ? 1 : -1; }
  const int status = serializer_swift_lz4_status(result);
  if (status) { return status; }
  state->pending_position_bytes = 0; state->pending_size_bytes = result;
  serializer_swift_pending(state, output, produced);
  return state->ended && state->pending_position_bytes == state->pending_size_bytes ? 1 : 0;
}
/* Query the pinned provider rather than duplicating its public level endpoints. */
int serializer_swift_provider_minimum_level(void) { return ZSTD_minCLevel(); }
/* Query the pinned provider rather than assuming a fixed future maximum. */
int serializer_swift_provider_maximum_level(void) { return ZSTD_maxCLevel(); }

/* Query the stable public LZ4 frame API for its current maximum encoder level. */
int serializer_swift_provider_lz4_maximum_level(void) { return LZ4F_compressionLevel_max(); }

/* Return stable native version metadata without creating codec state. */
const char* serializer_swift_provider_zstd_version(void) { return ZSTD_versionString(); }
/* Return stable native version metadata without creating codec state. */
const char* serializer_swift_provider_lz4_version(void) { return LZ4_versionString(); }
