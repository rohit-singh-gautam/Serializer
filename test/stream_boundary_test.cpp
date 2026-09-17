#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

// Invalid compact values must preserve an existing prefix, cursor, allocation, and spare bytes.
TEST(binary_serializer, rejects_out_of_range_compact_values_without_writing) {
  std::array<std::uint8_t, 8> bytes{};
  bytes.fill(0xa5);
  rohit::full_stream output{bytes.data(), bytes.size()};
  output.append(std::uint8_t{0x5a});
  rohit::serializer::binary_none<rohit::serializer::serialize_type::out> encoder{output};
  const auto expected = bytes;
  const auto* const cursor = output.curr();
  EXPECT_THROW(encoder.serialize_out_variable(-1), std::out_of_range);
  EXPECT_EQ(bytes, expected);
  EXPECT_EQ(output.curr(), cursor);
  EXPECT_THROW(encoder.serialize_out_variable(0x40000000U), std::out_of_range);
  EXPECT_EQ(bytes, expected);
  EXPECT_EQ(output.curr(), cursor);
  EXPECT_THROW(encoder.serialize_out_variable(std::numeric_limits<std::uint64_t>::max()),
               std::out_of_range);
  EXPECT_EQ(bytes, expected);
  EXPECT_EQ(output.curr(), cursor);
  EXPECT_EQ(output.begin(), bytes.data());
  EXPECT_EQ(output.capacity(), bytes.size());
}

// Accept exact-fit byte and character writes while preserving adjacent guards.
TEST(stream_boundaries, single_byte_writes_use_exact_capacity) {
  constexpr auto byte_values = std::to_array<std::uint8_t>({0, 1, 127, 255});
  for (const auto value : byte_values) {
    std::array<std::uint8_t, 3> bytes{0xa5, 0, 0x5a};
    rohit::full_stream output{bytes.data() + 1, 1};
    EXPECT_NO_THROW(output.append(value));
    EXPECT_EQ(output.current_offset(), 1U);
    EXPECT_EQ(bytes, (std::array<std::uint8_t, 3>{0xa5, value, 0x5a}));
    const auto expected = bytes;
    EXPECT_THROW(output.append(std::uint8_t{0}), rohit::exception::stream_overflow_exception);
    EXPECT_EQ(output.current_offset(), 1U);
    EXPECT_EQ(bytes, expected);

    output.reset();
    EXPECT_NO_THROW(output.append(static_cast<char>(value)));
    EXPECT_EQ(output.current_offset(), 1U);
    EXPECT_EQ(bytes, expected);
    EXPECT_THROW(output.append(char{0}), rohit::exception::stream_overflow_exception);
    EXPECT_EQ(output.current_offset(), 1U);
    EXPECT_EQ(bytes, expected);
  }
}
