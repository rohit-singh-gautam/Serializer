#include "stream_test_support.hpp"

#include <person.hpp>
#include <rohit/compression.hpp>
#include <rohit/serializer.hpp>
#include <views.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace compression = codec::compression;
using person = test::test1::person;
using bytes = std::vector<std::uint8_t>;
struct compression_case {
  compression::format format;
  compression::encode_options options;
};
const std::array cases{
    compression_case{compression::format::none, compression::none_options{}},
    compression_case{compression::format::zstd, compression::zstd_options{}},
    compression_case{compression::format::lz4, compression::lz4_options{}},
    compression_case{compression::format::gzip, compression::gzip_options{}},
    compression_case{compression::format::zlib, compression::zlib_options{}},
    compression_case{compression::format::deflate, compression::deflate_options{}}};

// Create reproducible compressible or high-entropy data spanning several backend chunks.
bytes payload(std::size_t size, bool varied = false) {
  bytes result(size, 'a');
  std::uint32_t state = 12345;
  if (varied) {
    for (auto& byte : result) {
      state ^= state << 13;
      state ^= state >> 17;
      state ^= state << 5;
      byte = static_cast<std::uint8_t>(state);
    }
  }
  return result;
}

// Exercise regenerated member/static APIs and the free exact helper with each native protocol.
template <template <codec::serialize_type> class Protocol>
void check_message(const compression_case& item) {
  const person original{"Ada", 42};
  rohit::full_stream_auto_alloc output;
  original.serialize_out<Protocol>(output, item.options);
  const compression::decode_options options{.format = item.format};
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  person decoded{};
  decoded.serialize_in<Protocol>(input, {}, options);
  EXPECT_TRUE(input.full());
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);
  std::stringstream stream;
  person::serialize<Protocol>(stream, original, item.options);
  const auto fresh = person::deserialize<Protocol>(stream, {}, options);
  EXPECT_EQ(fresh.name, original.name);
  EXPECT_EQ(fresh.id, original.id);
  std::istringstream backing{stream.str()};
  std::istream& erased = backing;
  const auto exact = codec::deserialize_exact<person, Protocol>(erased, {}, options);
  EXPECT_EQ(exact.name, original.name);
  EXPECT_EQ(exact.id, original.id);
}

// A separately supplied adapter can participate without editing the built-in format list.
class identity_backend final : public compression::backend {
public:
  // Identity is a test transport whose complete bounded span is its message.
  void encode(std::span<const std::uint8_t> input,
              compression::output_sink& output) const override {
    output.append(input);
  }
  // Pass through one already bounded message; no history storage is needed.
  void decode(std::span<const std::uint8_t> input, compression::output_sink& output,
              std::size_t) const override {
    output.append(input);
  }
};
} // namespace

// Empty frames and chunk boundaries must terminate exactly for compressible and expanding inputs.
TEST(compression, frames_empty_inputs_and_chunk_boundaries) {
  for (const auto& item : cases) {
    if (!compression::available(item.format)) {
      continue;
    }
    SCOPED_TRACE(static_cast<int>(item.format));
    for (const auto size : {0U, 1U, 65535U, 65536U, 65537U, 200000U}) {
      for (const bool varied : {false, true}) {
        const auto original = payload(size, varied);
        const auto frame = compression::compress(original, item.options);
        EXPECT_EQ(compression::decompress(frame, {.format = item.format}), original);
        const compression::decode_options exact_limits{.format = item.format,
                                                       .max_compressed_bytes = frame.size(),
                                                       .max_decompressed_bytes = original.size()};
        EXPECT_EQ(compression::decompress(frame, exact_limits), original);
        if (size != 0) {
          auto limited = exact_limits;
          --limited.max_decompressed_bytes;
          EXPECT_THROW((void)compression::decompress(frame, limited), compression::error);
        }
      }
    }
  }
}

// Every strict frame prefix, appended garbage, and concatenated frame is rejected.
TEST(compression, truncation_trailing_data_checksums_and_alignment) {
  const auto original = payload(512, true);
  for (const auto& item : cases) {
    if (item.format == compression::format::none || !compression::available(item.format)) {
      continue;
    }
    SCOPED_TRACE(static_cast<int>(item.format));
    const auto frame = compression::compress(original, item.options);
    const compression::decode_options options{.format = item.format};
    for (std::size_t size = 0; size < frame.size(); ++size) {
      EXPECT_THROW((void)compression::decompress(std::span{frame}.first(size), options),
                   compression::error);
    }
    auto appended = frame;
    appended.push_back(0);
    EXPECT_THROW((void)compression::decompress(appended, options), compression::error);
    appended = frame;
    appended.insert(appended.end(), frame.begin(), frame.end());
    EXPECT_THROW((void)compression::decompress(appended, options), compression::error);
    if (item.format != compression::format::deflate) {
      auto corrupt = frame;
      // gzip ends with ISIZE; the preceding four bytes are its CRC32.
      corrupt[corrupt.size() - (item.format == compression::format::gzip ? 8 : 1)] ^= 1;
      EXPECT_THROW((void)compression::decompress(corrupt, options), compression::error);
    }
    auto unaligned = bytes{0};
    unaligned.insert(unaligned.end(), frame.begin(), frame.end());
    EXPECT_EQ(compression::decompress(std::span{unaligned}.subspan(1), options), original);
  }
}

// Limits operate separately on compressed input, expanded bytes, and backend history storage.
TEST(compression, resource_limits_and_invalid_configuration) {
  const auto original = payload(10000);
  for (const auto& item : cases) {
    if (!compression::available(item.format)) {
      EXPECT_THROW((void)compression::compress(original, item.options), compression::error);
      EXPECT_THROW((void)compression::decompress(original, {.format = item.format}),
                   compression::error);
      continue;
    }
    const auto frame = compression::compress(original, item.options);
    EXPECT_THROW((void)compression::compress(original, item.options, {original.size() - 1, 100000}),
                 compression::error);
    EXPECT_THROW(
        (void)compression::compress(original, item.options, {original.size(), frame.size() - 1}),
        compression::error);
    EXPECT_THROW((void)compression::decompress(
                     frame, {.format = item.format, .max_compressed_bytes = frame.size() - 1}),
                 compression::error);
    if (item.format != compression::format::none) {
      EXPECT_THROW(
          (void)compression::decompress(frame, {.format = item.format, .max_window_bytes = 1}),
          compression::error);
    }
  }
  EXPECT_THROW(
      compression::validate(compression::zstd_options{.level = std::numeric_limits<int>::max()}),
      compression::error);
  EXPECT_THROW(compression::validate(compression::lz4_options{.level = -1}), compression::error);
  EXPECT_THROW(compression::validate(compression::gzip_options{.level = 10}), compression::error);
  EXPECT_THROW((void)compression::decompress({}, {.format = static_cast<compression::format>(999)}),
               compression::error);
}

// Generated wrappers consistently cover all native codecs and standard stream adaptation.
TEST(compression, generated_message_apis) {
  for (const auto& item : cases) {
    if (!compression::available(item.format)) {
      continue;
    }
    check_message<codec::json>(item);
    check_message<codec::binary_none>(item);
    check_message<codec::binary_integer>(item);
    check_message<codec::binary_string>(item);
  }
}

// Staging prevents compression/output-limit failures from changing an existing output prefix.
TEST(compression, output_failure_preserves_prefix_and_buffer_cursor) {
  const person value{"Ada", 42};
  for (const auto& item : cases) {
    rohit::full_stream_auto_alloc output;
    output.append("prefix");
    const auto* pointer = output.begin();
    EXPECT_ANY_THROW(value.serialize_out<codec::binary_integer>(output, item.options, {1, 1}));
    EXPECT_EQ(output.begin(), pointer);
    EXPECT_EQ(output.current_offset(), 6U);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(output.begin()), 6), "prefix");
    if (!compression::available(item.format)) {
      continue;
    }
    EXPECT_THROW(value.serialize_out<codec::binary_integer>(output, item.options, {100, 0}),
                 compression::error);
    EXPECT_EQ(output.current_offset(), 6U);
    std::array<std::uint8_t, 8> storage{};
    rohit::full_stream bounded{storage.data(), storage.size()};
    bounded.append("prefix");
    const auto saved = storage;
    EXPECT_ANY_THROW(value.serialize_out<codec::binary_integer>(bounded, item.options));
    EXPECT_EQ(bounded.current_offset(), 6U);
    EXPECT_EQ(storage, saved);
  }
}

// Decompression validates first; exact decoding never assigns a partly decoded owning candidate.
TEST(compression, input_failure_and_exact_decoding_contract) {
  for (const auto& item : cases) {
    if (!compression::available(item.format)) {
      continue;
    }
    const std::string invalid = "{\"fullname\":\"changed\"} extra";
    const auto frame = compression::compress(
        {reinterpret_cast<const std::uint8_t*>(invalid.data()), invalid.size()}, item.options);
    const auto input = rohit::make_constant_full_stream(frame.data(), frame.size());
    person destination{"original", 42};
    const compression::decode_options options{.format = item.format};
    EXPECT_THROW((destination = codec::deserialize_exact<person, codec::json>(input, {}, options)),
                 rohit::exception::base_parser);
    EXPECT_EQ(destination.name, "original");
    const auto budget_input = rohit::make_constant_full_stream(frame.data(), frame.size());
    codec::decode_limits limits{};
    limits.max_input_bytes = 1;
    EXPECT_THROW(destination.serialize_in<codec::json>(budget_input, limits, options),
                 compression::error);
    EXPECT_EQ(budget_input.curr(), frame.data());
    EXPECT_EQ(destination.name, "original");
    if (item.format != compression::format::none) {
      const auto truncated = rohit::make_constant_full_stream(frame.data(), frame.size() - 1);
      EXPECT_THROW(destination.serialize_in<codec::json>(truncated, {}, options),
                   compression::error);
      EXPECT_EQ(truncated.curr(), frame.data());
      EXPECT_EQ(destination.name, "original");
    }
  }
}

// Third-party adapters share output accounting and explicit per-call configuration.
TEST(compression, custom_backends_and_uncompressed_compatibility) {
  const identity_backend adapter{};
  const compression::encode_options options = compression::custom_options{&adapter};
  const auto original = payload(50);
  EXPECT_EQ(compression::compress(original, options), original);
  EXPECT_EQ(compression::decompress(
                original, {.format = compression::format::custom, .custom_backend = &adapter}),
            original);
  EXPECT_THROW((void)compression::compress(original, options, {50, 49}), compression::error);
  EXPECT_THROW((void)compression::decompress(original, {.format = compression::format::custom,
                                                        .max_decompressed_bytes = 49,
                                                        .custom_backend = &adapter}),
               compression::error);
  EXPECT_THROW(compression::validate(compression::custom_options{}), compression::error);
  EXPECT_THROW(
      compression::validate(compression::decode_options{.format = compression::format::custom}),
      compression::error);
  const person value{"Ada", 42};
  std::ostringstream ordinary;
  std::ostringstream explicit_none;
  value.serialize_out<codec::binary_integer>(ordinary);
  value.serialize_out<codec::binary_integer>(explicit_none, compression::none_options{});
  EXPECT_EQ(ordinary.str(), explicit_none.str());
}

// Frozen reference bytes come from zstd 1.5.7, LZ4 1.10.0, and Python gzip/zlib, not these adapters.
TEST(compression, independent_standard_format_fixtures) {
  struct fixture {
    compression::format format;
    std::string_view hex;
  };
  const std::array fixtures{
      fixture{compression::format::zstd, "28b52ffd241ff9000053657269616c697a657220636f6d70726573736"
                                         "96f6e20666978747572650a83812bcc"},
      fixture{compression::format::lz4, "04224d186440a71f00008053657269616c697a657220636f6d70726573"
                                        "73696f6e20666978747572650a00000000bf21171c"},
      fixture{compression::format::gzip, "1f8b08000000000002ff0b4e2dca4cccc9ac4a2d5248cecf2d284a2d2"
                                         "ececccf5348cbac28292d4ae502007a5180f71f000000"},
      fixture{compression::format::zlib,
              "789c0b4e2dca4cccc9ac4a2d5248cecf2d284a2d2ececccf5348cbac28292d4ae50200c5480c1e"},
      fixture{compression::format::deflate,
              "0b4e2dca4cccc9ac4a2d5248cecf2d284a2d2ececccf5348cbac28292d4ae50200"}};
  const std::string expected = "Serializer compression fixture\n";
  for (const auto& fixture : fixtures) {
    if (!compression::available(fixture.format)) {
      continue;
    }
    bytes frame;
    for (std::size_t index = 0; index < fixture.hex.size(); index += 2) {
      frame.push_back(static_cast<std::uint8_t>(
          std::stoul(std::string{fixture.hex.substr(index, 2)}, nullptr, 16)));
    }
    const auto decoded = compression::decompress(frame, {.format = fixture.format});
    EXPECT_EQ(std::string(decoded.begin(), decoded.end()), expected);
  }
}

// View mapping borrows the owned expanded vector, and edited bytes must be recompressed explicitly.
TEST(compression, views_use_owned_decompressed_storage) {
  using owning = view_test::owning_mutable<codec::storage_mode::owning>;
  using view_type = view_test::owning_mutable<codec::storage_mode::mutable_view>;
  owning original{};
  original.value = 42;
  for (const auto& item : cases) {
    if (!compression::available(item.format)) {
      continue;
    }
    rohit::full_stream_auto_alloc frame;
    original.serialize_out<codec::binary_none>(frame, item.options);
    auto storage =
        compression::decompress({frame.begin(), frame.current_offset()}, {.format = item.format});
    auto view = view_type::map(std::span{storage});
    EXPECT_EQ(view.get_value(), 42U);
    view.set_value(73);
    std::stringstream edited;
    codec::serialize_to<codec::binary_none>(edited, view, item.options);
    const auto decoded = codec::deserialize_exact<owning, codec::binary_none>(
        edited, {}, compression::decode_options{.format = item.format});
    EXPECT_EQ(decoded.value, 73U);
  }
}

namespace {
bool* stop_after_native_decode{};

// Keep ordinary JSON semantics while arming cancellation after native candidate construction.
template <codec::serialize_type Direction>
class cancelling_json : public codec::json<Direction> {
  using base = codec::json<Direction>;

public:
  using base::base;
  // Simulate an application decoder that requests cancellation after producing its native value.
  template <typename Value>
  void serialize_in(Value& value) {
    base::serialize_in(value);
    *stop_after_native_decode = true;
  }
};
} // namespace

// Fresh decoded values must not publish after native code requests cancellation.
TEST(compression, final_native_decode_cancellation_withholds_candidate) {
  const person value{"new candidate", 42};
  for (const auto& item : cases) {
    if (!compression::available(item.format)) {
      continue;
    }
    SCOPED_TRACE(static_cast<int>(item.format));
    rohit::full_stream_auto_alloc frame;
    codec::serialize_to<codec::json>(frame, value, item.options);
    bool stopped{};
    stop_after_native_decode = &stopped;
    int original_failure{};
    auto check = [&] {
      if (stopped) {
        throw &original_failure;
      }
    };
    const compression::decode_options options{.format = item.format, .check_cancelled = check};
    person destination{"unchanged", 7};
    const auto input = rohit::make_constant_full_stream(frame.begin(), frame.current_offset());
    bool rejected{};
    try {
      destination = codec::deserialize_exact<person, cancelling_json>(input, {}, options);
    } catch (int* failure) {
      EXPECT_EQ(failure, &original_failure);
      rejected = true;
    }
    EXPECT_TRUE(rejected);
    EXPECT_EQ(destination.name, "unchanged");
    EXPECT_EQ(destination.id, 7U);
    stopped = false;
    const auto inplace = rohit::make_constant_full_stream(frame.begin(), frame.current_offset());
    rejected = false;
    try {
      codec::serialize_from<cancelling_json>(inplace, destination, {}, options);
    } catch (int* failure) {
      EXPECT_EQ(failure, &original_failure);
      rejected = true;
    }
    EXPECT_TRUE(rejected);
    // In-place decoding retains its established partial-update semantics.
    EXPECT_EQ(destination.name, value.name);
    stop_after_native_decode = nullptr;
  }
}

// Whole-object publication checks cancellation after the external destination accepts its final bytes.
TEST(compression, final_whole_object_sink_cancellation_is_reported) {
  class cancelling_sink {
    bool& stopped_;

  public:
    std::size_t calls{};
    // Borrow state only for this synchronous publication.
    explicit cancelling_sink(bool& stopped) : stopped_{stopped} {}
    // Simulate a completed external write that requests cancellation before the call returns.
    void write(const char*, std::streamsize) {
      ++calls;
      stopped_ = true;
    }
    // The transport itself succeeds; the separate cancellation policy reports failure.
    bool fail() const noexcept {
      return false;
    }
  };
  const person value{"publication", 42};
  for (const auto& item : cases) {
    if (!compression::available(item.format)) {
      continue;
    }
    SCOPED_TRACE(static_cast<int>(item.format));
    bool stopped{};
    cancelling_sink output{stopped};
    int original_failure{};
    auto check = [&] {
      if (stopped) {
        throw &original_failure;
      }
    };
    compression::encode_limits limits{};
    limits.check_cancelled = check;
    bool rejected{};
    try {
      codec::serialize_to<codec::binary_none>(output, value, item.options, limits);
    } catch (int* failure) {
      EXPECT_EQ(failure, &original_failure);
      rejected = true;
    }
    EXPECT_TRUE(rejected);
    EXPECT_EQ(output.calls, 1U);
  }
}

// RFC1950's advertised window limits distances even when the native provider ignores CINFO.
TEST(compression, zlib_advertised_history_rejects_one_beyond_window) {
  if (!compression::available(compression::format::zlib)) {
    GTEST_SKIP() << "The zlib provider is disabled";
  }
  // A nonfinal stored block emits 257 literals; a final fixed block copies distance 257.
  bytes frame{0x08, 0x1d, 0x00, 0x01, 0x01, 0xfe, 0xfe};
  frame.insert(frame.end(), 257, 'A');
  constexpr auto trailer =
      std::to_array<std::uint8_t>({0x03, 0x06, 0x00, 0x00, 0xa9, 0xfd, 0x42, 0x05});
  frame.insert(frame.end(), trailer.begin(), trailer.end());
  const compression::decode_options options{.format = compression::format::zlib};
  try {
    static_cast<void>(compression::decompress(frame, options));
    FAIL() << "A distance beyond the advertised 256-byte history was accepted";
  } catch (const compression::error& failure) {
    EXPECT_EQ(failure.code, compression::error_code::invalid_data);
  }
  // The identical raw stream and checksum are valid under the ordinary 32 KiB header.
  frame[0] = 0x78;
  frame[1] = 0x01;
  EXPECT_EQ(compression::decompress(frame, options), bytes(260, 'A'));
}

// Every legal single-segment content-size width advertises the complete decoded extent as history.
TEST(compression, zstd_single_segment_window_is_admitted_before_expansion) {
  if (!compression::available(compression::format::zstd)) {
    GTEST_SKIP() << "The Zstandard provider is disabled";
  }
  constexpr std::size_t payload_bytes{65536};
  const std::array headers{
      bytes{0x28, 0xb5, 0x2f, 0xfd, 0x60, 0x00, 0xff},
      bytes{0x28, 0xb5, 0x2f, 0xfd, 0xa0, 0x00, 0x00, 0x01, 0x00},
      bytes{0x28, 0xb5, 0x2f, 0xfd, 0xe0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}};
  for (const auto& header : headers) {
    SCOPED_TRACE(header.size());
    auto frame = header;
    // A final raw block contains exactly 65,536 independent literal bytes.
    frame.insert(frame.end(), {0x01, 0x00, 0x08});
    frame.insert(frame.end(), payload_bytes, 'W');
    EXPECT_EQ(compression::decompress(
                  frame, {.format = compression::format::zstd, .max_window_bytes = payload_bytes}),
              bytes(payload_bytes, 'W'));
    EXPECT_EQ(compression::decompress(frame, {.format = compression::format::zstd,
                                              .max_window_bytes = payload_bytes + 1}),
              bytes(payload_bytes, 'W'));
    for (const auto maximum : {std::size_t{}, payload_bytes - 1}) {
      bool output_boundary_reached{};
      int original_failure{};
      auto check = [&] {
        output_boundary_reached = true;
        throw &original_failure;
      };
      try {
        static_cast<void>(compression::decompress(frame, {.format = compression::format::zstd,
                                                          .max_window_bytes = maximum,
                                                          .check_cancelled = check}));
        FAIL() << "A single-segment frame exceeded its history budget";
      } catch (const compression::error& failure) {
        EXPECT_EQ(failure.code, compression::error_code::resource_limit);
      }
      EXPECT_FALSE(output_boundary_reached);
    }
    int original_failure{};
    auto check = [&] { throw &original_failure; };
    try {
      static_cast<void>(compression::decompress(frame, {.format = compression::format::zstd,
                                                        .max_window_bytes = payload_bytes,
                                                        .check_cancelled = check}));
      FAIL() << "An admitted frame swallowed the application's output-boundary failure";
    } catch (int* failure) {
      EXPECT_EQ(failure, &original_failure);
    }
  }
}

// Explicit header admission also observes the provider's conservative power-of-two window profile.
TEST(compression, zstd_window_descriptor_is_admitted_before_provider_expansion) {
  if (!compression::available(compression::format::zstd)) {
    GTEST_SKIP() << "The Zstandard provider is disabled";
  }
  // Window descriptor 0x31 advertises 65,536 + 8,192 = 73,728 bytes; FCS is absent.
  const bytes frame{0x28, 0xb5, 0x2f, 0xfd, 0x00, 0x31, 0x09, 0x00, 0x00, 'A'};
  EXPECT_EQ(compression::decompress(
                frame, {.format = compression::format::zstd, .max_window_bytes = 131072}),
            bytes{'A'});
  for (const auto maximum : {std::size_t{65536}, std::size_t{80000}}) {
    try {
      static_cast<void>(compression::decompress(
          frame, {.format = compression::format::zstd, .max_window_bytes = maximum}));
      FAIL() << "The advertised window exceeded the conservative provider budget";
    } catch (const compression::error& failure) {
      EXPECT_EQ(failure.code, compression::error_code::resource_limit);
    }
  }
  // A 256 MiB advertised window rejects under the default budget before output.
  const bytes huge{0x28, 0xb5, 0x2f, 0xfd, 0x00, 0x90, 0x01, 0x00, 0x00};
  try {
    static_cast<void>(compression::decompress(huge, {.format = compression::format::zstd}));
    FAIL() << "A huge advertised history was accepted";
  } catch (const compression::error& failure) {
    EXPECT_EQ(failure.code, compression::error_code::resource_limit);
  }
}

// The admission check preserves native framing validation and accepts the specification's unused bit.
TEST(compression, zstd_window_admission_preserves_header_and_dictionary_contracts) {
  if (!compression::available(compression::format::zstd)) {
    GTEST_SKIP() << "The Zstandard provider is disabled";
  }
  const std::array valid{bytes{0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x01, 0x09, 0x00, 0x00, 'A'},
                         bytes{0x28, 0xb5, 0x2f, 0xfd, 0x30, 0x01, 0x09, 0x00, 0x00, 'A'},
                         bytes{0x28, 0xb5, 0x2f, 0xfd, 0x21, 0x00, 0x01, 0x09, 0x00, 0x00, 'A'}};
  for (const auto& frame : valid) {
    EXPECT_EQ(compression::decompress(frame, {.format = compression::format::zstd}), bytes{'A'});
    const auto header_bytes = frame.size() - 4;
    for (std::size_t size{}; size < header_bytes; ++size) {
      const auto truncated = std::span<const std::uint8_t>{frame}.first(size);
      try {
        static_cast<void>(
            compression::decompress(truncated, {.format = compression::format::zstd}));
        FAIL() << "A truncated header was accepted";
      } catch (const compression::error& failure) {
        EXPECT_EQ(failure.code, compression::error_code::invalid_data);
      }
    }
  }
  auto reserved = valid.front();
  reserved[4] |= 0x08;
  auto dictionary = valid.back();
  dictionary[5] = 1;
  // Native header validation keeps its existing error for an unsupported maximum exponent.
  const bytes unsupported_window{0x28, 0xb5, 0x2f, 0xfd, 0x00, 0xff, 0x01, 0x00, 0x00};
  for (const auto& frame : {reserved, dictionary, unsupported_window}) {
    try {
      static_cast<void>(compression::decompress(frame, {.format = compression::format::zstd}));
      FAIL() << "An unsupported header or dictionary was accepted";
    } catch (const compression::error& failure) {
      EXPECT_EQ(failure.code, compression::error_code::invalid_data);
    }
  }
}
