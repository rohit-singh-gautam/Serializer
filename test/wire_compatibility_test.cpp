#include <rohit/serializer.hpp>
#include <wire_compatibility.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;

// Frozen protocol fixtures, specified independently of Serializer's encoder.
// Big endian captures the earlier default contract; little endian captures the current contract.
// Compact ID 64, counts, strings, union tags, and terminators keep their own encoding in both.
constexpr std::string_view positional_big =
    "abfffe123456783f800000c000000000000000014102010203040506000708090a";
constexpr std::string_view positional_little =
    "abfeff785634120000803f00000000000000c0014102020104030605000a090807";
constexpr std::string_view integer_big =
    "01ab02fffe404012345678033f80000004c000000000000000050141060201020304070105060008000708090a00";
constexpr std::string_view integer_little =
    "01ab02feff404078563412030000803f0400000000000000c0050141060202010403070106050008000a09080700";
constexpr std::string_view string_big =
    "066d61726b6572ab0564656c7461fffe0873657175656e63651234567805726174696f3f800000"
    "0772656164696e67c000000000000000056c6162656c01410676616c7565730201020304"
    "066e657374656404636f64650506000e7061796c6f61643a6e756d6265720708090a00";
constexpr std::string_view string_little =
    "066d61726b6572ab0564656c7461feff0873657175656e63657856341205726174696f0000803f"
    "0772656164696e6700000000000000c0056c6162656c01410676616c7565730202010403"
    "066e657374656404636f64650605000e7061796c6f61643a6e756d6265720a09080700";

// Reject a mistyped fixture instead of allowing invalid hex to weaken the byte comparison.
std::uint8_t hex_digit(char value) {
  constexpr std::string_view digits = "0123456789abcdef";
  const auto index = digits.find(value);
  if (index == std::string_view::npos) {
    throw std::invalid_argument{"Invalid wire fixture hex digit"};
  }
  return static_cast<std::uint8_t>(index);
}

// Materialize fixed literal bytes without invoking either codec direction.
std::vector<std::uint8_t> fixture_bytes(std::string_view hex) {
  if (hex.size() % 2 != 0) {
    throw std::invalid_argument{"Odd wire fixture hex length"};
  }
  std::vector<std::uint8_t> result;
  result.reserve(hex.size() / 2);
  for (std::size_t index = 0; index < hex.size(); index += 2) {
    result.push_back(
        static_cast<std::uint8_t>((hex_digit(hex[index]) << 4) | hex_digit(hex[index + 1])));
  }
  return result;
}

// Use values whose byte-reversed representations differ observably, including nested storage.
wire_compatibility::record sample() {
  wire_compatibility::record value{};
  value.marker = 0xab;
  value.delta = -2;
  value.sequence = 0x12345678;
  value.ratio = 1.0F;
  value.reading = -2.0;
  value.label = "A";
  value.values = {0x0102, 0x0304};
  value.nested.code = 0x0506;
  value.payload.number = 0x0708090a;
  return value;
}

// Check decoding against semantic values and encoding against independent frozen bytes.
template <codec::serialize_key_type Keys, std::endian Endian>
void check_fixture(std::string_view hex) {
  const auto bytes = fixture_bytes(hex);
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  codec::binary<codec::serialize_type::in, Keys, Endian> decoder{input};
  wire_compatibility::record decoded{};
  decoder.serialize_in(decoded);
  decoder.finish();
  const auto expected = sample();
  EXPECT_EQ(decoded.marker, expected.marker);
  EXPECT_EQ(decoded.delta, expected.delta);
  EXPECT_EQ(decoded.sequence, expected.sequence);
  EXPECT_EQ(std::bit_cast<std::uint32_t>(decoded.ratio),
            std::bit_cast<std::uint32_t>(expected.ratio));
  EXPECT_EQ(std::bit_cast<std::uint64_t>(decoded.reading),
            std::bit_cast<std::uint64_t>(expected.reading));
  EXPECT_EQ(decoded.label, expected.label);
  EXPECT_EQ(decoded.values, expected.values);
  EXPECT_EQ(decoded.nested.code, expected.nested.code);
  EXPECT_EQ(decoded.payload_type, expected.payload_type);
  ASSERT_EQ(decoded.payload_type, wire_compatibility::record::e_payload::number);
  EXPECT_EQ(decoded.payload.number, expected.payload.number);

  rohit::full_stream_auto_alloc output{};
  codec::binary<codec::serialize_type::out, Keys, Endian> encoder{output};
  encoder.serialize_out(expected);
  ASSERT_EQ(output.current_offset(), bytes.size());
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), output.begin()));
}
} // namespace

// Preserve explicit access to the legacy fixed-width byte order in every native binary key mode.
TEST(wire_compatibility, legacy_big_endian_fixtures) {
  check_fixture<codec::serialize_key_type::none, std::endian::big>(positional_big);
  check_fixture<codec::serialize_key_type::integer, std::endian::big>(integer_big);
  check_fixture<codec::serialize_key_type::string, std::endian::big>(string_big);
}

// Keep the new default pinned independently from any matching encoder/decoder mistake.
TEST(wire_compatibility, current_little_endian_fixtures) {
  static_assert(codec::binary_none<codec::serialize_type::in>::wire_endian == std::endian::little);
  static_assert(codec::binary_integer<codec::serialize_type::in>::wire_endian ==
                std::endian::little);
  static_assert(codec::binary_string<codec::serialize_type::in>::wire_endian ==
                std::endian::little);
  check_fixture<codec::serialize_key_type::none, std::endian::little>(positional_little);
  check_fixture<codec::serialize_key_type::integer, std::endian::little>(integer_little);
  check_fixture<codec::serialize_key_type::string, std::endian::little>(string_little);
}

// A message has no endian marker: the wrong selection may parse successfully with different values.
TEST(wire_compatibility, byte_order_requires_explicit_agreement) {
  const auto bytes = fixture_bytes(positional_big);
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  codec::binary_none<codec::serialize_type::in> decoder{input};
  wire_compatibility::record decoded{};
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_EQ(decoded.sequence, 0x78563412U);
  EXPECT_NE(decoded.sequence, sample().sequence);
  EXPECT_EQ(decoded.label, sample().label);
  EXPECT_EQ(decoded.values.size(), sample().values.size());
}
