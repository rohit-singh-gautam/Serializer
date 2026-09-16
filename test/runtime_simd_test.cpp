#include <rohit/json_text.hpp>
#include <rohit/runtime_simd.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace simd = rohit::serializer::detail;

// Build a byte-for-byte reference independently of the production escaping helpers.
std::string quote_reference(std::string_view text) {
  constexpr std::string_view hex = "0123456789abcdef";
  std::string result{"\""};
  for (const unsigned char ch : text) {
    if (ch < 0x20) {
      result += "\\u00";
      result += hex[ch >> 4];
      result += hex[ch & 0xf];
    } else {
      if (ch == '"' || ch == '\\') {
        result += '\\';
      }
      result += static_cast<char>(ch);
    }
  }
  result += '"';
  return result;
}

// Borrow exactly the bytes written, excluding spare capacity.
std::string_view written(const rohit::full_stream& output) {
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Count reservations to detect accidental per-element capacity checks.
class counting_stream : public rohit::full_stream_auto_alloc {
public:
  std::size_t reservations{};
  // Count both direct reservations and reservations made by append helpers.
  void reserve(std::size_t size) override {
    ++reservations;
    rohit::full_stream_auto_alloc::reserve(size);
  }
};

// Compare the bulk array encoder against the existing scalar representation in each protocol.
template <codec::serialize_key_type Keys, std::endian Endian, typename Value>
void compare_binary_array(const std::vector<Value>& values) {
  using protocol = codec::binary<codec::serialize_type::out, Keys, Endian>;
  counting_stream output{};
  protocol encoder{output};
  encoder.serialize_out(values);
  rohit::full_stream_auto_alloc reference{};
  protocol scalar{reference};
  scalar.serialize_out_variable(values.size());
  for (const auto& value : values) {
    scalar.serialize_out(value);
  }
  EXPECT_EQ(written(output), written(reference));
  if constexpr (rohit::detail::endian_integer<Value> ||
                rohit::detail::endian_floating_point<Value>) {
    EXPECT_EQ(output.reservations, values.empty() ? 1u : 2u);
  }
  // Decode independently encoded scalars from unpadded, possibly unaligned storage.
  for (const std::size_t offset : {0u, 1u, 7u}) {
    const auto size = reference.current_offset();
    auto storage = std::make_unique<std::uint8_t[]>(offset + size);
    std::memcpy(storage.get() + offset, reference.begin(), size);
    const auto input = rohit::make_constant_full_stream(storage.get() + offset, size);
    codec::binary<codec::serialize_type::in, Keys, Endian> decoder{input};
    std::vector<Value> decoded(values.size() + 1);
    const auto capacity = decoded.capacity();
    decoder.serialize_in(decoded);
    decoder.finish();
    ASSERT_EQ(decoded.size(), values.size());
    EXPECT_EQ(decoded.capacity(), capacity);
    if constexpr (rohit::detail::endian_integer<Value> ||
                  rohit::detail::endian_floating_point<Value>) {
      if (!values.empty()) {
        EXPECT_EQ(std::memcmp(decoded.data(), values.data(), values.size() * sizeof(Value)), 0);
      }
    } else {
      EXPECT_EQ(decoded, values);
    }
  }
}

// Exercise all key modes with both byte orders, including the SIMD swap path on either host order.
template <typename Value>
void compare_binary_protocols(const std::vector<Value>& values) {
  compare_binary_array<codec::serialize_key_type::none, std::endian::little>(values);
  compare_binary_array<codec::serialize_key_type::integer, std::endian::little>(values);
  compare_binary_array<codec::serialize_key_type::string, std::endian::little>(values);
  compare_binary_array<codec::serialize_key_type::none, std::endian::big>(values);
  compare_binary_array<codec::serialize_key_type::integer, std::endian::big>(values);
  compare_binary_array<codec::serialize_key_type::string, std::endian::big>(values);
}

// Include signed extremes and vector tails for every supported integral width.
template <typename Value>
void check_integer_arrays() {
  for (std::size_t size = 0; size <= 65; ++size) {
    std::vector<Value> values(size);
    constexpr std::array samples{Value{}, Value{1}, std::numeric_limits<Value>::max(),
                                 std::numeric_limits<Value>::lowest()};
    for (std::size_t index = 0; index < size; ++index) {
      values[index] = samples[index % samples.size()];
    }
    compare_binary_protocols(values);
  }
}

// Compare compact and both formatted JSON writers while retaining their shared Unicode validation.
template <bool Beautify>
void check_json_output(std::string_view text, const codec::write_format& format) {
  counting_stream output{};
  codec::json_out<Beautify> encoder{output, format};
  encoder.serialize_out(text);
  EXPECT_EQ(written(output), quote_reference(text));
  EXPECT_EQ(output.reservations, 1u);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  codec::json<codec::serialize_type::in> decoder{input};
  std::string decoded;
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_EQ(decoded, text);
}

} // namespace

// Check every byte at scalar/SSE2/AVX2 boundaries without allocating readable tail padding.
TEST(runtime_simd, json_scan_byte_classification) {
  constexpr std::array prefixes{0u, 7u, 8u, 15u, 16u, 23u, 24u, 31u, 32u, 39u, 40u, 63u, 64u};
  constexpr std::array alignments{0u, 1u, 7u, 15u, 31u};
  for (const auto prefix : prefixes) {
    for (const auto alignment : alignments) {
      constexpr std::size_t suffix_bytes = 64;
      const auto size = prefix + 1 + suffix_bytes;
      auto allocation = std::make_unique<std::uint8_t[]>(alignment + size);
      auto* data = allocation.get() + alignment;
      std::fill_n(data, size, 'a');
      for (unsigned byte = 0; byte <= std::numeric_limits<std::uint8_t>::max(); ++byte) {
        data[prefix] = static_cast<std::uint8_t>(byte);
        const bool ordinary = byte >= 0x20 && byte != '"' && byte != '\\';
        for (const auto kind : {simd::json_scan_kind::ascii, simd::json_scan_kind::unescaped}) {
          const auto expected =
              ordinary && (kind == simd::json_scan_kind::unescaped || byte < 0x80) ? size : prefix;
          EXPECT_EQ(simd::scan_json_baseline(data, size, kind), expected);
          EXPECT_EQ(simd::scan_json_long(data, size, kind), expected);
          const auto actual =
              kind == simd::json_scan_kind::ascii
                  ? simd::scan_json_prefix<simd::json_scan_kind::ascii>(data, size)
                  : simd::scan_json_prefix<simd::json_scan_kind::unescaped>(data, size);
          EXPECT_EQ(actual, expected);
        }
      }
    }
  }
}

// Exercise exact-size tails, including null empty input and unaligned source/destination buffers.
TEST(runtime_simd, exact_size_scans_and_byte_swaps) {
  EXPECT_EQ(simd::scan_json_prefix<simd::json_scan_kind::ascii>(nullptr, 0), 0);
  EXPECT_EQ(simd::scan_json_long(nullptr, 0, simd::json_scan_kind::ascii), 0);
  simd::copy_swapped(nullptr, nullptr, 0, 4);
  for (std::size_t size = 1; size <= 128; ++size) {
    for (std::size_t alignment = 0; alignment < 32; ++alignment) {
      auto input = std::make_unique<std::uint8_t[]>(alignment + size);
      auto output = std::make_unique<std::uint8_t[]>(alignment + size);
      auto baseline = std::make_unique<std::uint8_t[]>(size);
      auto* data = input.get() + alignment;
      auto* destination = output.get() + alignment;
      std::fill_n(data, size, 'a');
      EXPECT_EQ(simd::scan_json_prefix<simd::json_scan_kind::ascii>(data, size), size);
      EXPECT_EQ(simd::scan_json_prefix<simd::json_scan_kind::unescaped>(data, size), size);
      for (const std::size_t width : {2u, 4u, 8u}) {
        if (size % width != 0) {
          continue;
        }
        for (std::size_t index = 0; index < size; ++index) {
          data[index] = static_cast<std::uint8_t>(index);
        }
        simd::copy_swapped(destination, data, size, width);
        simd::copy_swapped_baseline(baseline.get(), data, size, width);
        for (std::size_t index = 0; index < size; ++index) {
          const auto expected =
              static_cast<std::uint8_t>((index / width) * width + width - index % width - 1);
          EXPECT_EQ(destination[index], expected);
          EXPECT_EQ(baseline[index], expected);
        }
        simd::copy_swapped(destination, destination, size, width);
        EXPECT_EQ(std::memcmp(destination, data, size), 0);
      }
    }
  }
}

// Preserve every fixed-width scalar representation and retain scalar handling of packed booleans.
TEST(runtime_simd, binary_protocols_preserve_scalar_array_bytes) {
  check_integer_arrays<char>();
  check_integer_arrays<std::uint8_t>();
  check_integer_arrays<std::int8_t>();
  check_integer_arrays<std::uint16_t>();
  check_integer_arrays<std::int16_t>();
  check_integer_arrays<std::uint32_t>();
  check_integer_arrays<std::int32_t>();
  check_integer_arrays<std::uint64_t>();
  check_integer_arrays<std::int64_t>();
  std::vector<float> floats;
  std::vector<double> doubles;
  constexpr auto float_bits =
      std::to_array<std::uint32_t>({0, 0x80000000u, 0x3f800000u, 0x7f800000u, 0x7fc01234u});
  constexpr auto double_bits =
      std::to_array<std::uint64_t>({0, 0x8000000000000000ULL, 0x3ff0000000000000ULL,
                                    0x7ff0000000000000ULL, 0x7ff8000000001234ULL});
  for (std::size_t index = 0; index <= 65; ++index) {
    compare_binary_protocols(floats);
    compare_binary_protocols(doubles);
    floats.push_back(std::bit_cast<float>(float_bits[index % float_bits.size()]));
    doubles.push_back(std::bit_cast<double>(double_bits[index % double_bits.size()]));
  }
  compare_binary_protocols(std::vector<bool>{true, false, true});
  compare_binary_protocols(std::vector<std::string>{"", std::string(80, 'a'), "\"\\\n"});
}

// Keep JSON escaping bytes identical across long spans, control characters, and Unicode boundaries.
TEST(runtime_simd, json_output_and_input_preserve_strings) {
  for (std::size_t prefix = 0; prefix <= 65; ++prefix) {
    std::string text(prefix, 'a');
    for (int ch = 0; ch < 0x80; ++ch) {
      text.push_back(static_cast<char>(ch));
    }
    text += "\xc2\xa3\xe2\x82\xac\xf0\x9f\x98\x80";
    text += std::string(prefix, 'z');
    check_json_output<false>(text, codec::format::compress);
    check_json_output<true>(text, codec::format::beautify);
    check_json_output<true>(text, codec::format::beautify_vertical);
    check_json_output<false>(std::string(prefix, 'a'), codec::format::compress);
  }
}

// A rejected string or insufficient destination must not expose a partial quoted payload.
TEST(runtime_simd, json_failures_leave_output_bytes_unchanged) {
  for (const std::string_view invalid :
       {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"}) {
    rohit::full_stream_auto_alloc output{};
    output.append('x');
    codec::json_out<false> encoder{output};
    const std::string text = std::string(64, 'a') + std::string{invalid};
    EXPECT_THROW(encoder.serialize_out(text), std::invalid_argument);
    EXPECT_EQ(written(output), "x");
  }
  std::array<std::uint8_t, 7> buffer{};
  buffer.fill(0xab);
  rohit::full_stream output{buffer.data(), buffer.size()};
  codec::json_out<false> encoder{output};
  EXPECT_THROW(encoder.serialize_out(std::string_view{"\n"}),
               rohit::exception::stream_overflow_exception);
  EXPECT_EQ(output.current_offset(), 0);
  EXPECT_TRUE(std::all_of(buffer.begin(), buffer.end(), [](auto ch) { return ch == 0xab; }));
}

// Source aliases survive growth and expanding writes that overlap the unread source.
TEST(runtime_simd, direct_json_output_preserves_aliased_input) {
  for (const std::string_view suffix : {"", "\n\"\\"}) {
    const std::string text = std::string(64, 'a') + std::string{suffix};
    rohit::full_stream_auto_alloc growing{text.size()};
    growing.append(text);
    codec::json_out<false> append{growing};
    append.serialize_out(
        std::string_view{reinterpret_cast<const char*>(growing.begin()), text.size()});
    EXPECT_EQ(written(growing), text + quote_reference(text));

    std::array<std::uint8_t, 256> bytes{};
    std::memcpy(bytes.data() + 1, text.data(), text.size());
    rohit::full_stream overlapping{bytes.data(), bytes.size()};
    codec::json_out<false> replace{overlapping};
    replace.serialize_out(
        std::string_view{reinterpret_cast<const char*>(bytes.data() + 1), text.size()});
    EXPECT_EQ(written(overlapping), quote_reference(text));
  }
}

// Bulk reservation failure retains the completed count and never writes a partial array payload.
TEST(runtime_simd, binary_payload_reservation_is_bounded) {
  std::array<std::uint8_t, 4> bytes{};
  bytes.fill(0xab);
  rohit::full_stream output{bytes.data(), bytes.size()};
  codec::binary<codec::serialize_type::out, codec::serialize_key_type::none, std::endian::big>
      encoder{output};
  const std::vector<std::uint32_t> values(9, 0x12345678u);
  EXPECT_THROW(encoder.serialize_out(values), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(output.current_offset(), 1);
  EXPECT_EQ(bytes[0], values.size());
  EXPECT_TRUE(std::all_of(bytes.begin() + 1, bytes.end(), [](auto ch) { return ch == 0xab; }));
}

// Public transformed writes reject impossible ranges before allocation or callback execution.
TEST(runtime_simd, transformed_append_rejects_invalid_sizes) {
  rohit::full_stream_auto_alloc output{};
  bool invoked{};
  const auto writer = [&invoked](std::uint8_t*, const std::uint8_t*, std::size_t) noexcept {
    invoked = true;
  };
  EXPECT_THROW(output.append_transformed(nullptr, 1, 1, writer), std::invalid_argument);
  EXPECT_THROW(
      output.append_transformed(nullptr, 0, rohit::detail::maximum_buffer_bytes + 1, writer),
      rohit::exception::stream_overflow_exception);
  EXPECT_FALSE(invoked);
  EXPECT_EQ(output.current_offset(), 0);
  // An inaccurate allocation hint must not become an unchecked destination bound.
  EXPECT_EQ(simd::escape_json_string("\n", 0), "\\u000a");
}
