#include <compact_fields.hpp>
#include <rohit/serializer.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
// Encode a generated value through a complete native protocol.
template <template <rohit::serializer::serialize_type> class Protocol, typename T>
std::string encode(const T& value) {
  rohit::string_stream output;
  rohit::serializer::serialize_to<Protocol>(output, value);
  return std::move(output).str();
}

// Decode a fresh value only after the exact message has been validated.
template <template <rohit::serializer::serialize_type> class Protocol, typename T>
T decode(std::string_view bytes) {
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  return rohit::serializer::deserialize_exact<T, Protocol>(input);
}

// Exercise compact payloads in positional, keyed and JSON protocols.
template <typename T>
void round_trip(const T& value) {
  EXPECT_EQ((decode<rohit::serializer::binary_none, T>(encode<rohit::serializer::binary_none>(value))).value, value.value);
  EXPECT_EQ((decode<rohit::serializer::binary_integer, T>(encode<rohit::serializer::binary_integer>(value))).value, value.value);
  EXPECT_EQ((decode<rohit::serializer::binary_string, T>(encode<rohit::serializer::binary_string>(value))).value, value.value);
  EXPECT_EQ((decode<rohit::serializer::json, T>(encode<rohit::serializer::json>(value))).value, value.value);
}

template <rohit::serializer::serialize_type Direction>
using big_endian = rohit::serializer::binary<Direction, rohit::serializer::serialize_key_type::none, std::endian::big>;
} // namespace

// Freeze all prefix width transitions independently of the generated writer.
TEST(compact_fields, prefix_boundaries_and_key_framing) {
  const std::array<std::pair<std::uint32_t, std::string>, 8> cases{{
      {0, std::string("\x00", 1)}, {63, "\x3f"}, {64, "\x40\x40"},
      {16383, "\x7f\xff"}, {16384, std::string("\x80\x40\x00", 3)},
      {4194303, "\xbf\xff\xff"}, {4194304, std::string("\xc0\x40\x00\x00", 4)},
      {1073741823, "\xff\xff\xff\xff"}}};
  for (const auto& [number, expected] : cases) {
    compact_test::prefix_record value{};
    value.value = number;
    EXPECT_EQ(encode<rohit::serializer::binary_none>(value), expected);
    EXPECT_EQ(encode<big_endian>(value), expected);
    EXPECT_EQ(encode<rohit::serializer::binary_integer>(value), std::string("\x03", 1) + expected + '\0');
    round_trip(value);
  }
}

// Require a strict failure before this payload and deterministic truncation in lenient output.
TEST(compact_fields, strict_and_lenient_overflow_policies) {
  compact_test::prefix_record value{};
  value.value = 1073741824;
  rohit::string_stream output;
  rohit::serializer::binary_none<rohit::serializer::serialize_type::out, rohit::string_stream> writer{output};
  EXPECT_THROW((writer.serialize_out_compact<true, true>(value.value)), std::out_of_range);
  EXPECT_EQ(output.current_offset(), 0);
  EXPECT_THROW(encode<rohit::serializer::binary_none>(value), std::out_of_range);
  EXPECT_EQ(encode<rohit::serializer::json>(value), "{\"value\":1073741824}");
  compact_test::lenient_record unchecked{};
  EXPECT_EQ((decode<rohit::serializer::binary_none, compact_test::lenient_record>(encode<rohit::serializer::binary_none>(unchecked))).value, 1073741823);
  EXPECT_EQ(encode<rohit::serializer::json>(unchecked), "{\"value\":4294967295}");
  unchecked.value = 1073741824;
  EXPECT_EQ(encode<rohit::serializer::binary_none>(unchecked), std::string(1, '\0'));
}

// Freeze varint boundaries through the full unsigned 64-bit range.
TEST(compact_fields, varint_boundaries_and_full_width) {
  static_assert(std::is_same_v<decltype(compact_test::varint_record::value), std::uint64_t>);
  const std::array<std::pair<std::uint64_t, std::string>, 7> cases{{
      {0, std::string(1, '\0')}, {127, "\x7f"}, {128, "\x80\x01"},
      {16383, "\xff\x7f"}, {16384, "\x80\x80\x01"},
      {4294967295, "\xff\xff\xff\xff\x0f"},
      {std::numeric_limits<std::uint64_t>::max(), std::string(9, static_cast<char>(0xff)) + '\x01'}}};
  for (const auto& [number, expected] : cases) {
    compact_test::varint_record value{};
    value.value = number;
    EXPECT_EQ(encode<rohit::serializer::binary_none>(value), expected);
    EXPECT_EQ(encode<big_endian>(value), expected);
    round_trip(value);
  }
}

// Reject truncation, redundant groups, unused high bits and excessive continuation groups.
TEST(compact_fields, malformed_varints_and_narrow_scalars) {
  using value_type = compact_test::varint_record;
  for (const auto& bytes : {std::string(), std::string("\x80", 1), std::string("\x80\x00", 2),
                           std::string(10, static_cast<char>(0x80)),
                           std::string(9, static_cast<char>(0xff)) + '\x02',
                           std::string("\x01\x00", 2)}) {
    EXPECT_THROW((decode<rohit::serializer::binary_none, value_type>(bytes)), std::exception);
  }
  EXPECT_THROW((decode<rohit::serializer::binary_none, compact_test::narrow_record>("\x80\x02")), std::exception);
  EXPECT_THROW((decode<rohit::serializer::binary_none, compact_test::prefix_narrow_record>(std::string("\x41\x00", 2))), std::exception);
  compact_test::narrow_record narrow{};
  narrow.value = 255;
  round_trip(narrow);
  EXPECT_EQ(encode<rohit::serializer::binary_none>(narrow), "\xff\x01");
}

// Compact fields end fixed-width batches without changing adjacent field bytes or defaults.
TEST(compact_fields, mixed_fixed_and_compact_layout) {
  const compact_test::mixed_record value{};
  const auto bytes = encode<rohit::serializer::binary_none>(value);
  EXPECT_EQ(bytes, std::string("\x01\x00\x00\x00\x80\x01\x02\x00\x00\x00\x03\x00", 12));
  const auto restored = decode<rohit::serializer::binary_none, compact_test::mixed_record>(bytes);
  EXPECT_EQ(restored.before, value.before);
  EXPECT_EQ(restored.value, value.value);
  EXPECT_EQ(restored.after, value.after);
  EXPECT_EQ(restored.tail, value.tail);
}
