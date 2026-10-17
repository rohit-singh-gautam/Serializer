// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/compression_stream.hpp>

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
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::serializer::compression {
namespace detail {
namespace {
[[maybe_unused]] constexpr std::size_t chunk_bytes = 64 * 1024;
}

// Share logical bounds, failure fencing, and explicit finalization across every backend.
class encoding_state : protected output_sink {
  output_sink& destination_;
  const encode_limits limits_;
  std::size_t input_bytes{};
  std::size_t output_bytes{};
  bool failed_{};
  bool finished_{};

protected:
  // Encode another range from the same message without adding a record boundary.
  virtual void encode_bytes(std::span<const std::uint8_t> bytes) = 0;
  // Produce the backend trailer exactly once, including an empty-message frame if necessary.
  virtual void encode_end() = 0;
  // Check every emission before invoking the borrowed destination.
  void append(std::span<const std::uint8_t> bytes) override {
    if (bytes.size() > limits_.max_output_bytes - output_bytes) {
      throw error{error_code::resource_limit, "Compression output limit exceeded"};
    }
    if (!bytes.empty()) {
      destination_.append(bytes);
      output_bytes += bytes.size();
    }
  }

public:
  // Borrow the caller's sink and copy its immutable logical message bounds.
  encoding_state(output_sink& destination, encode_limits limits)
      : destination_{destination}, limits_{limits} {}
  // Release only owned backend state; never finish implicitly.
  ~encoding_state() override = default;
  // Reject invalid lifecycle transitions and check input bounds before touching the backend.
  void write(std::span<const std::uint8_t> bytes) {
    if (failed_ || finished_) {
      throw std::logic_error{"Compression encoder is closed"};
    }
    try {
      if (bytes.size() > limits_.max_input_bytes - input_bytes) {
        throw error{error_code::resource_limit, "Compression input limit exceeded"};
      }
      encode_bytes(bytes);
      input_bytes += bytes.size();
    } catch (...) {
      failed_ = true;
      throw;
    }
  }
  // Fence failed streams; idempotence never emits a second frame or duplicate trailer.
  void finish() {
    if (failed_) {
      throw std::logic_error{"Compression encoder has failed"};
    }
    if (!finished_) {
      try {
        encode_end();
        finished_ = true;
      } catch (...) {
        failed_ = true;
        throw;
      }
    }
  }
  // Report failure without invoking a borrowed resource.
  [[nodiscard]] bool failed() const noexcept {
    return failed_;
  }
  // Report completion without invoking a borrowed resource.
  [[nodiscard]] bool finished() const noexcept {
    return finished_;
  }
  // Report bytes still available to codec batches not yet passed through this encoder.
  [[nodiscard]] std::size_t remaining_input_bytes() const noexcept {
    return limits_.max_input_bytes - input_bytes;
  }
};

namespace {
// Preserve ordinary byte transport when no compression was selected.
class identity_state final : public encoding_state {
  // Forward the same message bytes immediately, preserving their original ordering.
  void encode_bytes(std::span<const std::uint8_t> bytes) override {
    append(bytes);
  }
  // Identity transport has no trailer.
  void encode_end() override {}

public:
  using encoding_state::encoding_state;
};

// Existing custom backends accept whole spans; bounded staging preserves that public contract.
class custom_state final : public encoding_state {
  const backend& implementation;
  std::vector<std::uint8_t> storage{};

  // Accumulate only bytes already admitted by the shared input bound.
  void encode_bytes(std::span<const std::uint8_t> bytes) override {
    storage.insert(storage.end(), bytes.begin(), bytes.end());
  }
  // Let the custom backend write a single standard/custom stream under the same output bound.
  void encode_end() override {
    implementation.encode(storage, *this);
    storage.clear();
  }

public:
  // Borrow the explicitly supplied custom backend for this encoding session.
  custom_state(output_sink& output, encode_limits limits, const backend& implementation)
      : encoding_state{output, limits}, implementation{implementation} {}
};

#if SERIALIZER_WITH_ZSTD
// Translate zstd failures without including input bytes or backend-dependent diagnostic details.
std::size_t check_zstd_stream(std::size_t status) {
  if (ZSTD_isError(status)) {
    if (ZSTD_getErrorCode(status) == ZSTD_error_memory_allocation) {
      throw std::bad_alloc{};
    }
    throw error{error_code::invalid_data, "Unable to encode Zstandard stream"};
  }
  return status;
}

// Keep one Zstandard frame open while ordinary codec batches arrive.
class zstd_state final : public encoding_state {
  std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context;
  std::array<std::uint8_t, chunk_bytes> scratch{};

  // Drain bounded output until the supplied batch is consumed or finalization is complete.
  void pump(std::span<const std::uint8_t> bytes, ZSTD_EndDirective directive) {
    ZSTD_inBuffer source{bytes.data(), bytes.size(), 0};
    std::size_t remaining{};
    do {
      ZSTD_outBuffer destination{scratch.data(), scratch.size(), 0};
      remaining = check_zstd_stream(
          ZSTD_compressStream2(context.get(), &destination, &source, directive));
      append({scratch.data(), destination.pos});
    } while (source.pos != source.size || (directive == ZSTD_e_end && remaining != 0));
  }
  // Compress the next ordinary codec batch without creating another frame.
  void encode_bytes(std::span<const std::uint8_t> bytes) override {
    pump(bytes, ZSTD_e_continue);
  }
  // Flush pending blocks and the optional frame checksum.
  void encode_end() override {
    pump({}, ZSTD_e_end);
  }

public:
  // Configure the backend before publishing any frame bytes.
  zstd_state(output_sink& output, encode_limits limits, zstd_options options)
      : encoding_state{output, limits}, context{ZSTD_createCCtx(), ZSTD_freeCCtx} {
    if (!context) {
      throw std::bad_alloc{};
    }
    check_zstd_stream(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, options.level));
    check_zstd_stream(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, options.checksum));
  }
};
#endif

#if SERIALIZER_WITH_LZ4
// Normalize LZ4 failures to the stable compression error contract.
std::size_t check_lz4_stream(std::size_t status) {
  if (LZ4F_isError(status)) {
    throw error{error_code::invalid_data, "Unable to encode LZ4 frame"};
  }
  return status;
}

// Produce one standard LZ4 frame using independent 64 KiB blocks.
class lz4_state final : public encoding_state {
  std::unique_ptr<LZ4F_cctx, decltype(&LZ4F_freeCompressionContext)> context;
  LZ4F_preferences_t preferences{};
  std::vector<std::uint8_t> scratch{};
  bool started{};

  // Emit the header lazily so construction never performs fallible destination I/O.
  void begin() {
    if (!started) {
      const auto count = check_lz4_stream(
          LZ4F_compressBegin(context.get(), scratch.data(), scratch.size(), &preferences));
      append({scratch.data(), count});
      started = true;
    }
  }
  // Split large contiguous codec batches into bounded compression calls.
  void encode_bytes(std::span<const std::uint8_t> bytes) override {
    begin();
    while (!bytes.empty()) {
      const auto input_count = std::min(bytes.size(), chunk_bytes);
      const auto output_count = check_lz4_stream(LZ4F_compressUpdate(
          context.get(), scratch.data(), scratch.size(), bytes.data(), input_count, nullptr));
      append({scratch.data(), output_count});
      bytes = bytes.subspan(input_count);
    }
  }
  // Complete the frame even when the whole serialized message was empty.
  void encode_end() override {
    begin();
    const auto count = check_lz4_stream(
        LZ4F_compressEnd(context.get(), scratch.data(), scratch.size(), nullptr));
    append({scratch.data(), count});
  }

public:
  // Allocate one backend context and bounded output scratch without writing to the sink.
  lz4_state(output_sink& output, encode_limits limits, lz4_options options)
      : encoding_state{output, limits}, context{nullptr, LZ4F_freeCompressionContext} {
    LZ4F_cctx* raw{};
    check_lz4_stream(LZ4F_createCompressionContext(&raw, LZ4F_VERSION));
    context.reset(raw);
    preferences.frameInfo.blockSizeID = LZ4F_max64KB;
    preferences.frameInfo.blockMode = LZ4F_blockIndependent;
    preferences.frameInfo.contentChecksumFlag =
        options.checksum ? LZ4F_contentChecksumEnabled : LZ4F_noContentChecksum;
    preferences.compressionLevel = options.level;
    preferences.autoFlush = 1;
    scratch.resize(std::max<std::size_t>(
        LZ4F_HEADER_SIZE_MAX, check_lz4_stream(LZ4F_compressBound(chunk_bytes, &preferences))));
  }
};
#endif

#if SERIALIZER_WITH_ZLIB
// Select the exact requested wrapper, without format auto-detection.
int deflate_window_bits(format selected) {
  constexpr int window_bits = 15;
  constexpr int gzip_wrapper = 16;
  if (selected == format::gzip) {
    return window_bits + gzip_wrapper;
  }
  return selected == format::deflate ? -window_bits : window_bits;
}

// Reuse one zlib context until explicit whole-message completion.
class deflate_state final : public encoding_state {
  z_stream stream{};
  std::array<std::uint8_t, chunk_bytes> scratch{};

  // Publish a backend chunk, distinguishing memory failures from malformed state.
  int pump(int flush) {
    stream.next_out = scratch.data();
    stream.avail_out = static_cast<uInt>(scratch.size());
    const auto status = deflate(&stream, flush);
    if (status == Z_MEM_ERROR) {
      throw std::bad_alloc{};
    }
    if (status != Z_OK && status != Z_STREAM_END) {
      throw error{error_code::invalid_data, "Unable to encode DEFLATE stream"};
    }
    append({scratch.data(), scratch.size() - stream.avail_out});
    return status;
  }
  // Never narrow a size_t range into zlib's uInt counters without bounding it first.
  void encode_bytes(std::span<const std::uint8_t> bytes) override {
    while (!bytes.empty()) {
      const auto count = std::min(bytes.size(), chunk_bytes);
      stream.next_in = const_cast<Bytef*>(bytes.data());
      stream.avail_in = static_cast<uInt>(count);
      do {
        pump(Z_NO_FLUSH);
      } while (stream.avail_in != 0);
      bytes = bytes.subspan(count);
    }
  }
  // Drain all pending data and its wrapper trailer exactly once.
  void encode_end() override {
    stream.next_in = nullptr;
    stream.avail_in = 0;
    while (pump(Z_FINISH) != Z_STREAM_END) {}
  }

public:
  // Prepare a fixed gzip, zlib, or raw DEFLATE stream without emitting a header.
  deflate_state(output_sink& output, encode_limits limits, format selected, int level)
      : encoding_state{output, limits} {
    static_assert(chunk_bytes <= std::numeric_limits<uInt>::max());
    constexpr int memory_level = 8;
    const auto status = deflateInit2(&stream, level, Z_DEFLATED, deflate_window_bits(selected),
                                     memory_level, Z_DEFAULT_STRATEGY);
    if (status == Z_MEM_ERROR) {
      throw std::bad_alloc{};
    }
    if (status != Z_OK) {
      throw error{error_code::invalid_options, "Unable to initialize DEFLATE stream"};
    }
  }
  // Release backend memory even after a sink failure; never emit pending bytes.
  ~deflate_state() override {
    deflateEnd(&stream);
  }
};
#endif

// Construct only an explicitly enabled backend after shared option validation succeeds.
std::unique_ptr<encoding_state> make_encoding_state(output_sink& output,
                                                  const encode_options& options,
                                                  encode_limits limits) {
  validate(options);
  return std::visit(
      [&](const auto& option) -> std::unique_ptr<encoding_state> {
        using Options = std::decay_t<decltype(option)>;
        if constexpr (std::is_same_v<Options, none_options>) {
          return std::make_unique<identity_state>(output, limits);
        } else if constexpr (std::is_same_v<Options, custom_options>) {
          return std::make_unique<custom_state>(output, limits, *option.implementation);
#if SERIALIZER_WITH_ZSTD
        } else if constexpr (std::is_same_v<Options, zstd_options>) {
          return std::make_unique<zstd_state>(output, limits, option);
#endif
#if SERIALIZER_WITH_LZ4
        } else if constexpr (std::is_same_v<Options, lz4_options>) {
          return std::make_unique<lz4_state>(output, limits, option);
#endif
#if SERIALIZER_WITH_ZLIB
        } else if constexpr (std::is_same_v<Options, gzip_options>) {
          return std::make_unique<deflate_state>(output, limits, format::gzip, option.level);
        } else if constexpr (std::is_same_v<Options, zlib_options>) {
          return std::make_unique<deflate_state>(output, limits, format::zlib, option.level);
        } else if constexpr (std::is_same_v<Options, deflate_options>) {
          return std::make_unique<deflate_state>(output, limits, format::deflate, option.level);
#endif
        } else {
          throw error{error_code::unavailable, "Compression format is not enabled in this build"};
        }
      },
      options);
}
} // namespace
} // namespace detail

// Backend selection and allocations complete before any destination bytes are produced.
stream_encoder::stream_encoder(output_sink& output, const encode_options& options,
                               encode_limits limits)
    : state{detail::make_encoding_state(output, options, limits)} {}

// Destroying owned state cannot touch a closed or already destroyed destination.
stream_encoder::~stream_encoder() = default;

// Apply the shared lifecycle and bounds contract to the backend's next ordinary codec batch.
void stream_encoder::write(std::span<const std::uint8_t> bytes) {
  state->write(bytes);
}

// Complete the backend frame without closing or flushing the borrowed sink.
void stream_encoder::finish() {
  state->finish();
}

// Expose failure fencing without allocating or touching the destination.
bool stream_encoder::failed() const noexcept {
  return state->failed();
}

// Expose successful explicit completion without allocating or touching the destination.
bool stream_encoder::finished() const noexcept {
  return state->finished();
}

// Let contiguous codec adapters enforce the same whole-message bound before allocation.
std::size_t stream_encoder::remaining_input_bytes() const noexcept {
  return state->remaining_input_bytes();
}
} // namespace rohit::serializer::compression
