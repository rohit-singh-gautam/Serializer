#include <person.hpp>
#include <rohit/compression_stream.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
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
const std::array stream_cases{
    compression_case{compression::format::none, compression::none_options{}},
    compression_case{compression::format::zstd, compression::zstd_options{}},
    compression_case{compression::format::lz4, compression::lz4_options{}},
    compression_case{compression::format::gzip, compression::gzip_options{}},
    compression_case{compression::format::zlib, compression::zlib_options{}},
    compression_case{compression::format::deflate, compression::deflate_options{}}};

// Borrow arbitrary byte strings for the public span compression APIs.
std::span<const std::uint8_t> as_bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

// Generate a reproducible expanding payload with multiple backend and codec batches.
std::string stream_payload(std::size_t size) {
  std::string result(size, '\0');
  std::uint32_t state = 12345;
  for (auto& character : result) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    character = static_cast<char>(state);
  }
  return result;
}

// Compose existing whole-object codecs with the standard compression byte-sink interface.
template <template <codec::serialize_type> class Protocol>
void check_stream_object(const compression_case& item) {
  const person original{std::string(200000, 'a'), 42};
  rohit::full_stream_auto_alloc destination;
  compression::output_stream output{destination, item.options};
  static_assert(rohit::type_check::byte_output_stream<decltype(output)>);
  codec::serialize_to<Protocol>(output, original);
  EXPECT_FALSE(output.finished());
  output.finish();
  EXPECT_TRUE(output.finished());
  EXPECT_FALSE(output.fail());
  const auto size = destination.current_offset();
  output.finish();
  EXPECT_EQ(destination.current_offset(), size);

  const auto source = rohit::make_constant_full_stream(destination.begin(), size);
  compression::input_stream input{source, {.format = item.format}};
  static_assert(rohit::type_check::input_buffer<decltype(input)>);
  const auto decoded = codec::deserialize_exact<person, Protocol>(input);
  input.finish();
  EXPECT_TRUE(source.full());
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);

  std::ostringstream wrapped;
  compression::output_stream standard_output{wrapped, item.options};
  codec::serialize_to<Protocol>(standard_output, original);
  standard_output.finish();
  std::istringstream standard_source{wrapped.str()};
  compression::input_stream standard_input{standard_source, {.format = item.format}};
  const auto standard_decoded = codec::deserialize_exact<person, Protocol>(standard_input);
  EXPECT_EQ(standard_decoded.name, original.name);
  EXPECT_EQ(standard_decoded.id, original.id);
}

// Simulate an irreversible partial external write followed by a reported transport failure.
class failing_stream {
public:
  std::string prefix{};
  std::size_t calls{};
  // Record a small prefix and fail each attempt; retrying must be prevented by the wrapper.
  void write(const char* data, std::streamsize size) {
    ++calls;
    if (size > 0) {
      prefix.append(data, std::min<std::size_t>(static_cast<std::size_t>(size), 3));
    }
  }
  // Report failure as soon as the first partial write has happened.
  [[nodiscard]] bool fail() const noexcept {
    return calls != 0;
  }
};

// Keep the existing application backend's whole-span contract available through the wrapper.
class identity_stream_backend final : public compression::backend {
public:
  // Emit a whole bounded custom stream only after explicit wrapper finalization.
  void encode(std::span<const std::uint8_t> input, compression::output_sink& output) const override {
    output.append(input);
  }
  // Decode one whole bounded input without additional backend history storage.
  void decode(std::span<const std::uint8_t> input, compression::output_sink& output,
              std::size_t) const override {
    output.append(input);
  }
};
} // namespace

// Native whole-object codecs compose with all enabled compression wrappers without code changes.
TEST(compression_stream, whole_objects_all_native_codecs) {
  for (const auto& item : stream_cases) {
    if (compression::available(item.format)) {
      SCOPED_TRACE(static_cast<int>(item.format));
      check_stream_object<codec::json>(item);
      check_stream_object<codec::binary_none>(item);
      check_stream_object<codec::binary_integer>(item);
      check_stream_object<codec::binary_string>(item);
    }
  }
}

// Existing whole-span decoding interoperates with streamed output for empty and expanding input.
TEST(compression_stream, standard_frames_and_batch_boundaries) {
  for (const auto& item : stream_cases) {
    if (!compression::available(item.format)) {
      continue;
    }
    SCOPED_TRACE(static_cast<int>(item.format));
    for (const auto size : {0U, 1U, 65535U, 65536U, 65537U, 200000U}) {
      const auto original = stream_payload(size);
      std::ostringstream destination;
      compression::output_stream output{destination, item.options};
      std::size_t offset{};
      constexpr std::size_t batch_bytes = 997;
      while (offset < original.size()) {
        const auto count = std::min(original.size() - offset, batch_bytes);
        output.write(original.data() + offset, static_cast<std::streamsize>(count));
        offset += count;
      }
      output.finish();
      const auto frame = destination.str();
      const auto decoded = compression::decompress(as_bytes(frame), {.format = item.format});
      EXPECT_EQ(decoded, (bytes{as_bytes(original).begin(), as_bytes(original).end()}));

      const auto reference = compression::compress(as_bytes(original), item.options);
      const auto source = rohit::make_constant_full_stream(reference.data(), reference.size());
      compression::input_stream input{source, {.format = item.format}};
      EXPECT_EQ(input.remaining_buffer(), original.size());
      if (!original.empty()) {
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(input.curr()), input.remaining_buffer()),
                  original);
      }
    }
  }
}

// Frame corruption is rejected before an object decoder can observe or mutate a destination.
TEST(compression_stream, input_truncation_trailing_checksum_and_limits) {
  const auto original = stream_payload(200);
  for (const auto& item : stream_cases) {
    if (item.format == compression::format::none || !compression::available(item.format)) {
      continue;
    }
    SCOPED_TRACE(static_cast<int>(item.format));
    std::ostringstream destination;
    compression::output_stream output{destination, item.options};
    output.write(original.data(), static_cast<std::streamsize>(original.size()));
    output.finish();
    const auto frame = destination.str();
    const compression::decode_options options{.format = item.format};
    for (std::size_t size = 0; size < frame.size(); ++size) {
      const auto input = rohit::make_constant_full_stream(frame.data(), size);
      EXPECT_THROW((compression::input_stream{input, options}), compression::error);
      EXPECT_EQ(input.remaining_buffer(), size);
    }
    auto trailing = frame + "x";
    std::istringstream trailing_source{trailing};
    EXPECT_THROW((compression::input_stream{trailing_source, options}), compression::error);
    auto concatenated = frame + frame;
    const auto concatenated_source =
        rohit::make_constant_full_stream(concatenated.data(), concatenated.size());
    EXPECT_THROW((compression::input_stream{concatenated_source, options}), compression::error);
    if (item.format != compression::format::deflate) {
      auto corrupt = frame;
      const auto trailer_offset = item.format == compression::format::gzip ? 8U : 1U;
      corrupt[corrupt.size() - trailer_offset] ^= 1;
      const auto input = rohit::make_constant_full_stream(corrupt.data(), corrupt.size());
      EXPECT_THROW((compression::input_stream{input, options}), compression::error);
    }
    const auto source = rohit::make_constant_full_stream(frame.data(), frame.size());
    auto limited = options;
    limited.max_compressed_bytes = frame.size() - 1;
    EXPECT_THROW((compression::input_stream{source, limited}), compression::error);
    limited = options;
    limited.max_decompressed_bytes = original.size() - 1;
    EXPECT_THROW((compression::input_stream{source, limited}), compression::error);
    limited = options;
    limited.max_window_bytes = 1;
    EXPECT_THROW((compression::input_stream{source, limited}), compression::error);
  }
}

// A failed sink fences writes/finalization; destruction never retries or emits missing trailers.
TEST(compression_stream, explicit_finalization_and_failure_fencing) {
  const auto original = stream_payload(200000);
  for (const auto& item : stream_cases) {
    if (!compression::available(item.format)) {
      continue;
    }
    SCOPED_TRACE(static_cast<int>(item.format));
    std::ostringstream unfinished;
    std::string before_destruction;
    {
      compression::output_stream output{unfinished, item.options};
      output.write(original.data(), static_cast<std::streamsize>(original.size()));
      before_destruction = unfinished.str();
    }
    EXPECT_EQ(unfinished.str(), before_destruction);
    if (item.format != compression::format::none) {
      EXPECT_THROW((void)compression::decompress(as_bytes(before_destruction), {.format = item.format}),
                   compression::error);
    }

    failing_stream sink;
    std::size_t failed_calls{};
    {
      compression::output_stream output{sink, item.options};
      EXPECT_ANY_THROW({
        output.write(original.data(), static_cast<std::streamsize>(original.size()));
        output.finish();
      });
      EXPECT_TRUE(output.fail());
      failed_calls = sink.calls;
      EXPECT_ANY_THROW(output.write("x", 1));
      EXPECT_ANY_THROW(output.finish());
      EXPECT_EQ(sink.calls, failed_calls);
    }
    EXPECT_EQ(sink.calls, failed_calls);

    std::ostringstream finished;
    compression::output_stream complete{finished, item.options};
    complete.finish();
    const auto empty_frame = finished.str();
    EXPECT_ANY_THROW(complete.write("x", 1));
    EXPECT_EQ(finished.str(), empty_frame);
  }
}

// Input/output logical limits are independent; failures and disabled backends never silently pass.
TEST(compression_stream, resource_bounds_options_and_custom_backends) {
  const std::string original = "entire object";
  for (const auto& item : stream_cases) {
    std::ostringstream destination;
    if (!compression::available(item.format)) {
      EXPECT_THROW((compression::output_stream{destination, item.options}), compression::error);
      EXPECT_TRUE(destination.str().empty());
      continue;
    }
    compression::output_stream input_limited{destination, item.options, {original.size() - 1, 1000}};
    EXPECT_THROW(input_limited.write(original.data(), static_cast<std::streamsize>(original.size())),
                 compression::error);
    EXPECT_TRUE(input_limited.fail());
    EXPECT_TRUE(destination.str().empty());

    std::ostringstream bounded;
    compression::output_stream output_limited{bounded, item.options, {original.size(), 0}};
    EXPECT_THROW({
      output_limited.write(original.data(), static_cast<std::streamsize>(original.size()));
      output_limited.finish();
    }, compression::error);
    EXPECT_TRUE(output_limited.fail());
    EXPECT_TRUE(bounded.str().empty());
  }

  const identity_stream_backend backend;
  std::ostringstream custom_destination;
  compression::output_stream custom{
      custom_destination, compression::custom_options{&backend}, {original.size(), original.size()}};
  custom.write(original.data(), static_cast<std::streamsize>(original.size()));
  EXPECT_TRUE(custom_destination.str().empty());
  custom.finish();
  EXPECT_EQ(custom_destination.str(), original);
  std::istringstream custom_source{custom_destination.str()};
  compression::input_stream custom_input{
      custom_source, {.format = compression::format::custom, .custom_backend = &backend}};
  EXPECT_THROW(custom_input.finish(), compression::error);
  custom_input.get_curr_and_increase_unchecked(original.size());
  EXPECT_NO_THROW(custom_input.finish());
}

// Codec reservations are bounded before allocation, including pending bytes not yet compressed.
TEST(compression_stream, codec_scratch_checks_message_bound_before_growth) {
  std::ostringstream destination;
  compression::output_stream output{
      destination, compression::none_options{}, {15, 15}};
  rohit::buffered_output_stream buffer{output};
  EXPECT_EQ(buffer.capacity(), 15U);
  buffer.append("prefix");
  const auto* original_storage = buffer.begin();
  EXPECT_ANY_THROW(buffer.reserve(1000000000));
  EXPECT_EQ(buffer.capacity(), 15U);
  EXPECT_EQ(buffer.begin(), original_storage);
  EXPECT_EQ(buffer.current_offset(), 6U);
  EXPECT_TRUE(destination.str().empty());
  EXPECT_ANY_THROW(buffer.finish());

  std::ostringstream complete;
  compression::output_stream exact{complete, compression::none_options{}, {8, 8}};
  rohit::buffered_output_stream batches{exact, 4};
  batches.append("abcd");
  batches.append("efgh");
  EXPECT_EQ(complete.str(), "abcd");
  EXPECT_EQ(batches.capacity(), 4U);
  batches.finish();
  exact.finish();
  EXPECT_EQ(complete.str(), "abcdefgh");
  std::ostringstream ordinary;
  EXPECT_THROW((rohit::buffered_output_stream{ordinary, std::numeric_limits<std::size_t>::max()}),
               rohit::exception::stream_overflow_exception);
}

// Whole-object compression filters compose as ordinary sinks and finalize in object-to-sink order.
TEST(compression_stream, nested_filters_reverse_decode_and_finalization_order) {
  const identity_stream_backend custom;
  const bool gzip_enabled = compression::available(compression::format::gzip);
  const auto outer_format = gzip_enabled ? compression::format::gzip : compression::format::custom;
  const compression::encode_options outer_options = gzip_enabled
      ? compression::encode_options{compression::gzip_options{}}
      : compression::encode_options{compression::custom_options{&custom}};
  const auto inner_format = compression::available(compression::format::zstd)
      ? compression::format::zstd : gzip_enabled ? compression::format::gzip
                                               : compression::format::none;
  const compression::encode_options inner_options = inner_format == compression::format::zstd
      ? compression::encode_options{compression::zstd_options{}}
      : gzip_enabled ? compression::encode_options{compression::gzip_options{}}
                     : compression::encode_options{compression::none_options{}};
  const compression::decode_options outer_decode{
      .format = outer_format, .custom_backend = gzip_enabled ? nullptr : &custom};
  const person original{"Nested whole object", 42};

  std::ostringstream destination;
  compression::output_stream inner{destination, inner_options};
  compression::output_stream outer{inner, outer_options};
  codec::serialize_to<codec::binary_none>(outer, original);
  outer.finish();
  inner.finish();
  std::istringstream source{destination.str()};
  compression::input_stream inner_input{source, {.format = inner_format}};
  compression::input_stream outer_input{inner_input, outer_decode};
  const auto decoded = codec::deserialize_exact<person, codec::binary_none>(outer_input);
  outer_input.finish();
  inner_input.finish();
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);

  std::ostringstream premature_destination;
  compression::output_stream premature_inner{premature_destination, inner_options};
  compression::output_stream premature_outer{premature_inner, outer_options};
  codec::serialize_to<codec::binary_none>(premature_outer, original);
  premature_inner.finish();
  EXPECT_ANY_THROW(premature_outer.finish());
  EXPECT_TRUE(premature_outer.fail());
  EXPECT_TRUE(premature_inner.fail());
  EXPECT_ANY_THROW(premature_outer.finish());
}
