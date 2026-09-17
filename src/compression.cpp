#include <rohit/compression.hpp>

#if SERIALIZER_WITH_ZSTD
#include <zstd.h>
#include <zstd_errors.h>
#endif
#if SERIALIZER_WITH_LZ4
#include <lz4frame.h>
#endif
#if SERIALIZER_WITH_ZLIB
#include <zlib.h>
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace rohit::serializer::compression {
namespace {
[[maybe_unused]] constexpr std::size_t chunk_bytes = 64 * 1024;
[[maybe_unused]] constexpr std::size_t deflate_window_bytes = 32 * 1024;

// Accumulate chunks with an overflow-safe limit before growing owned output storage.
class vector_sink final : public output_sink {
  const std::size_t maximum;

public:
  std::vector<std::uint8_t> bytes{};
  // Retain the caller's logical byte limit for every backend emission.
  explicit vector_sink(std::size_t maximum) : maximum{maximum} {}
  // Reject over-limit chunks without changing the current prefix.
  void append(std::span<const std::uint8_t> input) override {
    if (input.size() > maximum - bytes.size()) {
      throw error{error_code::resource_limit, "Compression output limit exceeded"};
    }
    if (!input.empty()) {
      bytes.insert(bytes.end(), input.begin(), input.end());
    }
  }
};

// Reject unsupported built-in backends before acquiring resources or consuming bytes.
void require_available(format selected) {
  if (!available(selected)) {
    throw error{error_code::unavailable, "Compression format is not enabled in this build"};
  }
}

// Require the standard frame magic, excluding legacy and skippable frame variants.
[[maybe_unused]] void require_magic(std::span<const std::uint8_t> input,
                                    std::array<std::uint8_t, 4> magic) {
  if (input.size() < magic.size() || !std::equal(magic.begin(), magic.end(), input.begin())) {
    throw error{error_code::invalid_data, "Invalid compression frame header"};
  }
}

#if SERIALIZER_WITH_ZSTD
// Translate zstd status without exposing backend messages or input bytes.
std::size_t check_zstd(std::size_t status) {
  if (ZSTD_isError(status)) {
    if (ZSTD_getErrorCode(status) == ZSTD_error_memory_allocation) {
      throw std::bad_alloc{};
    }
    if (ZSTD_getErrorCode(status) == ZSTD_error_frameParameter_windowTooLarge) {
      throw error{error_code::resource_limit, "Zstandard frame exceeds the window limit"};
    }
    throw error{error_code::invalid_data, "Invalid or unsupported Zstandard stream"};
  }
  return status;
}

// Encode one standard frame with bounded scratch chunks and explicit end-of-frame completion.
void encode_zstd(std::span<const std::uint8_t> input, output_sink& output, zstd_options options) {
  const std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context{ZSTD_createCCtx(),
                                                                     ZSTD_freeCCtx};
  if (!context) {
    throw std::bad_alloc{};
  }
  check_zstd(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, options.level));
  check_zstd(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, options.checksum));
  check_zstd(ZSTD_CCtx_setPledgedSrcSize(context.get(), input.size()));
  ZSTD_inBuffer source{input.data(), input.size(), 0};
  std::array<std::uint8_t, chunk_bytes> scratch{};
  std::size_t remaining;
  do {
    ZSTD_outBuffer destination{scratch.data(), scratch.size(), 0};
    remaining = check_zstd(ZSTD_compressStream2(context.get(), &destination, &source, ZSTD_e_end));
    output.append({scratch.data(), destination.pos});
  } while (remaining != 0);
}

// Bound the history window and actual output without trusting an advertised content size.
void decode_zstd(std::span<const std::uint8_t> input, output_sink& output,
                 const decode_options& options) {
  require_magic(input, {0x28, 0xb5, 0x2f, 0xfd});
  const auto content_size = ZSTD_getFrameContentSize(input.data(), input.size());
  if (content_size == ZSTD_CONTENTSIZE_ERROR) {
    throw error{error_code::invalid_data, "Invalid Zstandard frame header"};
  }
  if (content_size != ZSTD_CONTENTSIZE_UNKNOWN && content_size > options.max_decompressed_bytes) {
    throw error{error_code::resource_limit, "Decompressed message limit exceeded"};
  }
  const std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> context{ZSTD_createDCtx(),
                                                                     ZSTD_freeDCtx};
  if (!context) {
    throw std::bad_alloc{};
  }
  // The stable API accepts a power-of-two window bound; round down, never above policy.
  const auto window_log = static_cast<int>(std::bit_width(options.max_window_bytes) - 1);
  const auto bounds = ZSTD_dParam_getBounds(ZSTD_d_windowLogMax);
  check_zstd(bounds.error);
  if (window_log < bounds.lowerBound) {
    throw error{error_code::resource_limit, "Zstandard window limit is too small"};
  }
  check_zstd(ZSTD_DCtx_setParameter(context.get(), ZSTD_d_windowLogMax,
                                    std::min(window_log, bounds.upperBound)));
  ZSTD_inBuffer source{input.data(), input.size(), 0};
  std::array<std::uint8_t, chunk_bytes> scratch{};
  while (true) {
    const auto previous = source.pos;
    ZSTD_outBuffer destination{scratch.data(), scratch.size(), 0};
    const auto remaining = check_zstd(ZSTD_decompressStream(context.get(), &destination, &source));
    output.append({scratch.data(), destination.pos});
    if (remaining == 0) {
      if (source.pos != source.size) {
        throw error{error_code::invalid_data, "Trailing data after Zstandard frame"};
      }
      return;
    }
    if (source.pos == previous && destination.pos == 0) {
      throw error{error_code::invalid_data, "Truncated Zstandard frame"};
    }
  }
}
#endif

#if SERIALIZER_WITH_LZ4
// Normalize LZ4 errors to the public, payload-free failure contract.
std::size_t check_lz4(std::size_t status) {
  if (LZ4F_isError(status)) {
    throw error{error_code::invalid_data, "Invalid or unsupported LZ4 frame"};
  }
  return status;
}

// Use independent bounded blocks, an optional content checksum, and the standard frame wrapper.
void encode_lz4(std::span<const std::uint8_t> input, output_sink& output, lz4_options options) {
  LZ4F_cctx* raw{};
  check_lz4(LZ4F_createCompressionContext(&raw, LZ4F_VERSION));
  const std::unique_ptr<LZ4F_cctx, decltype(&LZ4F_freeCompressionContext)> context{
      raw, LZ4F_freeCompressionContext};
  LZ4F_preferences_t preferences{};
  preferences.frameInfo.blockSizeID = LZ4F_max64KB;
  preferences.frameInfo.blockMode = LZ4F_blockIndependent;
  preferences.frameInfo.contentChecksumFlag =
      options.checksum ? LZ4F_contentChecksumEnabled : LZ4F_noContentChecksum;
  preferences.frameInfo.contentSize = input.size();
  preferences.compressionLevel = options.level;
  preferences.autoFlush = 1;
  std::vector<std::uint8_t> scratch(std::max<std::size_t>(
      LZ4F_HEADER_SIZE_MAX, check_lz4(LZ4F_compressBound(chunk_bytes, &preferences))));
  auto written =
      check_lz4(LZ4F_compressBegin(context.get(), scratch.data(), scratch.size(), &preferences));
  output.append({scratch.data(), written});
  while (!input.empty()) {
    const auto count = std::min(input.size(), chunk_bytes);
    written = check_lz4(LZ4F_compressUpdate(context.get(), scratch.data(), scratch.size(),
                                            input.data(), count, nullptr));
    output.append({scratch.data(), written});
    input = input.subspan(count);
  }
  written = check_lz4(LZ4F_compressEnd(context.get(), scratch.data(), scratch.size(), nullptr));
  output.append({scratch.data(), written});
}

// Validate block size before LZ4 allocates block/history storage, then consume one whole frame.
void decode_lz4(std::span<const std::uint8_t> input, output_sink& output,
                const decode_options& options) {
  require_magic(input, {0x04, 0x22, 0x4d, 0x18});
  LZ4F_dctx* raw{};
  check_lz4(LZ4F_createDecompressionContext(&raw, LZ4F_VERSION));
  const std::unique_ptr<LZ4F_dctx, decltype(&LZ4F_freeDecompressionContext)> context{
      raw, LZ4F_freeDecompressionContext};
  LZ4F_frameInfo_t info{};
  auto consumed = input.size();
  check_lz4(LZ4F_getFrameInfo(context.get(), &info, input.data(), &consumed));
  std::size_t block_bytes{};
  switch (info.blockSizeID) {
  case LZ4F_max64KB:
    block_bytes = 64 * 1024;
    break;
  case LZ4F_max256KB:
    block_bytes = 256 * 1024;
    break;
  case LZ4F_max1MB:
    block_bytes = mebibyte;
    break;
  case LZ4F_max4MB:
    block_bytes = 4 * mebibyte;
    break;
  default:
    throw error{error_code::invalid_data, "Invalid LZ4 block size"};
  }
  if (block_bytes > options.max_window_bytes || info.contentSize > options.max_decompressed_bytes) {
    throw error{error_code::resource_limit, "LZ4 frame exceeds decompression limits"};
  }
  if (info.dictID != 0) {
    throw error{error_code::invalid_data, "LZ4 dictionaries are not supported"};
  }
  input = input.subspan(consumed);
  std::array<std::uint8_t, chunk_bytes> scratch{};
  while (true) {
    auto written = scratch.size();
    consumed = input.size();
    const auto remaining = check_lz4(
        LZ4F_decompress(context.get(), scratch.data(), &written, input.data(), &consumed, nullptr));
    output.append({scratch.data(), written});
    input = input.subspan(consumed);
    if (remaining == 0) {
      if (!input.empty()) {
        throw error{error_code::invalid_data, "Trailing data after LZ4 frame"};
      }
      return;
    }
    if (consumed == 0 && written == 0) {
      throw error{error_code::invalid_data, "Truncated LZ4 frame"};
    }
  }
}
#endif

#if SERIALIZER_WITH_ZLIB
// Own zlib state and release it on all paths, including sink exceptions.
struct zlib_context {
  z_stream stream{};
  const bool encoding;
  // Select an exact wrapper; never enable gzip/zlib auto-detection.
  zlib_context(bool encoding, int window_bits, int level = Z_DEFAULT_COMPRESSION)
      : encoding{encoding} {
    constexpr int memory_level = 8;
    const auto result = encoding ? deflateInit2(&stream, level, Z_DEFLATED, window_bits,
                                                memory_level, Z_DEFAULT_STRATEGY)
                                 : inflateInit2(&stream, window_bits);
    if (result == Z_MEM_ERROR) {
      throw std::bad_alloc{};
    }
    if (result != Z_OK) {
      throw error{error_code::invalid_options, "Unable to initialize zlib"};
    }
  }
  zlib_context(const zlib_context&) = delete;
  zlib_context& operator=(const zlib_context&) = delete;
  // Destruction frees state and never emits bytes or throws.
  ~zlib_context() {
    if (encoding) {
      deflateEnd(&stream);
    } else {
      inflateEnd(&stream);
    }
  }
};

// Map distinct wire wrappers to zlib's documented windowBits values.
int zlib_window_bits(format selected) {
  constexpr int window_bits = 15;
  constexpr int gzip_wrapper = 16;
  if (selected == format::gzip) {
    return window_bits + gzip_wrapper;
  }
  return selected == format::deflate ? -window_bits : window_bits;
}

// Feed bounded chunks without truncating size_t into zlib's uInt input/output counters.
void run_zlib(std::span<const std::uint8_t> input, output_sink& output, bool encoding,
              format selected, int level = Z_DEFAULT_COMPRESSION) {
  zlib_context context{encoding, zlib_window_bits(selected), level};
  auto& stream = context.stream;
  std::array<std::uint8_t, chunk_bytes> scratch{};
  static_assert(chunk_bytes <= std::numeric_limits<uInt>::max());
  while (true) {
    if (stream.avail_in == 0 && !input.empty()) {
      const auto count = std::min(input.size(), chunk_bytes);
      stream.next_in = const_cast<Bytef*>(input.data());
      stream.avail_in = static_cast<uInt>(count);
      input = input.subspan(count);
    }
    stream.next_out = scratch.data();
    stream.avail_out = static_cast<uInt>(scratch.size());
    const auto before = stream.avail_in;
    const auto status = encoding ? deflate(&stream, input.empty() ? Z_FINISH : Z_NO_FLUSH)
                                 : inflate(&stream, Z_NO_FLUSH);
    if (status == Z_MEM_ERROR) {
      throw std::bad_alloc{};
    }
    if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) {
      throw error{error_code::invalid_data, "Invalid or unsupported DEFLATE stream"};
    }
    const auto written = scratch.size() - stream.avail_out;
    output.append({scratch.data(), written});
    if (status == Z_STREAM_END) {
      if (stream.avail_in != 0 || !input.empty()) {
        throw error{error_code::invalid_data, "Trailing data after compressed stream"};
      }
      return;
    }
    if (before == stream.avail_in && written == 0) {
      throw error{error_code::invalid_data, "Truncated or stalled DEFLATE stream"};
    }
  }
}
#endif
} // namespace

// Keep capability discovery independent of any installed third-party headers in consumers.
bool available(format selected) noexcept {
  switch (selected) {
  case format::none:
    return true;
  case format::zstd:
    return SERIALIZER_WITH_ZSTD != 0;
  case format::lz4:
    return SERIALIZER_WITH_LZ4 != 0;
  case format::gzip:
  case format::zlib:
  case format::deflate:
    return SERIALIZER_WITH_ZLIB != 0;
  default:
    return false;
  }
}

// Reject invalid levels instead of silently clamping backend-specific settings.
void validate(const encode_options& options) {
  std::visit(
      [](const auto& option) {
        using T = std::decay_t<decltype(option)>;
        if constexpr (std::is_same_v<T, zstd_options>) {
          require_available(format::zstd);
#if SERIALIZER_WITH_ZSTD
          if (option.level < ZSTD_minCLevel() || option.level > ZSTD_maxCLevel()) {
            throw error{error_code::invalid_options, "Invalid Zstandard compression level"};
          }
#endif
        } else if constexpr (std::is_same_v<T, lz4_options>) {
          require_available(format::lz4);
#if SERIALIZER_WITH_LZ4
          if (option.level < 0 || option.level > LZ4F_compressionLevel_max()) {
            throw error{error_code::invalid_options, "Invalid LZ4 compression level"};
          }
#endif
        } else if constexpr (std::is_same_v<T, gzip_options> || std::is_same_v<T, zlib_options> ||
                             std::is_same_v<T, deflate_options>) {
          require_available(format::gzip);
          if (option.level < -1 || option.level > 9) {
            throw error{error_code::invalid_options, "Invalid DEFLATE compression level"};
          }
        } else if constexpr (std::is_same_v<T, custom_options>) {
          if (option.implementation == nullptr) {
            throw error{error_code::invalid_options, "Missing custom compression backend"};
          }
        }
      },
      options);
}

// Built-in formats never consult a custom adapter and always enforce an explicit window policy.
void validate(const decode_options& options) {
  if (options.format == format::custom) {
    if (options.custom_backend == nullptr) {
      throw error{error_code::invalid_options, "Missing custom compression backend"};
    }
  } else {
    require_available(options.format);
    if (options.custom_backend != nullptr) {
      throw error{error_code::invalid_options, "Custom backend requires the custom format"};
    }
  }
  if (options.format != format::none && options.max_window_bytes == 0) {
    throw error{error_code::resource_limit, "Decompression window limit is zero"};
  }
}

// Centralize input bounds and output accounting around all built-in and external adapters.
std::vector<std::uint8_t> compress(std::span<const std::uint8_t> input,
                                   const encode_options& options, encode_limits limits) {
  validate(options);
  if (input.size() > limits.max_input_bytes) {
    throw error{error_code::resource_limit, "Compression input limit exceeded"};
  }
  vector_sink output{limits.max_output_bytes};
  std::visit(
      [&](const auto& option) {
        using T = std::decay_t<decltype(option)>;
        if constexpr (std::is_same_v<T, none_options>) {
          output.append(input);
        } else if constexpr (std::is_same_v<T, custom_options>) {
          option.implementation->encode(input, output);
#if SERIALIZER_WITH_ZSTD
        } else if constexpr (std::is_same_v<T, zstd_options>) {
          encode_zstd(input, output, option);
#endif
#if SERIALIZER_WITH_LZ4
        } else if constexpr (std::is_same_v<T, lz4_options>) {
          encode_lz4(input, output, option);
#endif
#if SERIALIZER_WITH_ZLIB
        } else if constexpr (std::is_same_v<T, gzip_options>) {
          run_zlib(input, output, true, format::gzip, option.level);
        } else if constexpr (std::is_same_v<T, zlib_options>) {
          run_zlib(input, output, true, format::zlib, option.level);
        } else if constexpr (std::is_same_v<T, deflate_options>) {
          run_zlib(input, output, true, format::deflate, option.level);
#endif
        }
      },
      options);
  return std::move(output.bytes);
}

// Single-frame decoding never accepts a second member, a skippable frame, or unexplained suffix.
std::vector<std::uint8_t> decompress(std::span<const std::uint8_t> input,
                                     const decode_options& options) {
  validate(options);
  if (input.size() > options.max_compressed_bytes) {
    throw error{error_code::resource_limit, "Compressed input limit exceeded"};
  }
  vector_sink output{options.max_decompressed_bytes};
  switch (options.format) {
  case format::none:
    output.append(input);
    break;
#if SERIALIZER_WITH_ZSTD
  case format::zstd:
    decode_zstd(input, output, options);
    break;
#endif
#if SERIALIZER_WITH_LZ4
  case format::lz4:
    decode_lz4(input, output, options);
    break;
#endif
#if SERIALIZER_WITH_ZLIB
  case format::gzip:
  case format::zlib:
  case format::deflate:
    if (options.max_window_bytes < deflate_window_bytes) {
      throw error{error_code::resource_limit, "DEFLATE requires a 32 KiB window budget"};
    }
    run_zlib(input, output, false, options.format);
    break;
#endif
  case format::custom:
    options.custom_backend->decode(input, output, options.max_window_bytes);
    break;
  default:
    throw error{error_code::unavailable, "Compression format is not enabled in this build"};
  }
  return std::move(output.bytes);
}
} // namespace rohit::serializer::compression
