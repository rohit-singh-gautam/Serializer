// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <rohit/decode.hpp>
#include <rohit/serializer_creator.hpp>
#include "bit_fields.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
// Parse complete declarations through the CLI's language and member validation.
auto parse_bits(std::string_view text) {
  auto input = rohit::make_constant_stream(text.data(), text.size());
  return rohit::serializer::parser::parse(input);
}

// Compare bytes against ordinary scalars, independently of native object layout.
template <template <rohit::serializer::serialize_type, typename> class Protocol>
void verify_bit_wire() {
  bit_fields packed{};
  bit_fields_wire ordinary{};
  rohit::full_stream_auto_alloc first{}, second{};
  packed.serialize_out<Protocol>(first);
  ordinary.serialize_out<Protocol>(second);
  ASSERT_EQ(first.current_offset(), second.current_offset());
  EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(first.begin()), first.current_offset()),
      std::string_view(reinterpret_cast<const char*>(second.begin()), second.current_offset()));
  auto input = rohit::make_constant_stream(first.begin(), first.current_offset());
  const auto decoded = rohit::serializer::deserialize_exact<bit_fields, Protocol>(input);
  EXPECT_EQ(static_cast<std::uint8_t>(decoded.flags), ordinary.flags);
  EXPECT_EQ(static_cast<std::uint16_t>(decoded.count), ordinary.count);
  EXPECT_EQ(static_cast<std::uint64_t>(decoded.token), ordinary.token);
  ordinary.flags = 8;
  rohit::full_stream_auto_alloc invalid_bytes{};
  ordinary.serialize_out<Protocol>(invalid_bytes);
  auto invalid = rohit::make_constant_stream(invalid_bytes.begin(), invalid_bytes.current_offset());
  EXPECT_THROW((static_cast<void>(rohit::serializer::deserialize_exact<bit_fields, Protocol>(invalid))), std::exception);
}

// All four codecs preserve bytes and reject wider input before assigning the field.
TEST(bit_fields, native_wire_and_range_checks) {
  verify_bit_wire<rohit::serializer::binary_none>();
  verify_bit_wire<rohit::serializer::binary_integer>();
  verify_bit_wire<rohit::serializer::binary_string>();
  verify_bit_wire<rohit::serializer::json>();
}

// Malformed, overflowing and unavailable widths fail before backend publication.
TEST(bit_fields, schema_boundaries) {
  for (const auto width : {"0", "9", "65", "184467440737095516160000", "-1", "word"}) {
    EXPECT_THROW(parse_bits("serializer version 1.24.0; class value { public uint8 flags bits(" +
        std::string{width} + "); }"), std::exception);
  }
  EXPECT_THROW(parse_bits("serializer version 1.23.0; class value { public uint8 flags bits(3); }"), std::exception);
  EXPECT_THROW(parse_bits("serializer version 1.24.0; class value { public uint8 flags bits(3) {8}; }"), std::exception);
  for (const auto declaration : {"bool value {2};", "char value {'abc'};", "uint8 value {256};",
      "double value {1e400};", "string value {native_call()};"}) {
    EXPECT_THROW(parse_bits("serializer version 1.24.0; class invalid { public " +
        std::string{declaration} + " }"), std::exception);
  }
}
} // namespace
