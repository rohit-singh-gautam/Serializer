// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <protobuf_arrays.hpp>
#include <rohit/protobuf.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;

// Decode one complete independent repeated-field fixture through the public exact-message API.
template <typename Value, template <codec::serialize_type> class Protocol>
Value decode(std::string_view text, codec::decode_limits limits = {}) {
  return codec::deserialize_exact<Value, Protocol>(
      rohit::make_constant_stream(text.data(), text.size()), limits);
}

// Every optional representation retains fixed defaults on construction and exact values on the wire.
template <template <codec::serialize_type> class Protocol>
void round_trip() {
  const protobuf_arrays::record original{};
  const auto bytes = codec::serialize_as<std::string, Protocol>(original);
  const auto copy = decode<protobuf_arrays::record, Protocol>(bytes);
  EXPECT_EQ(copy.names, original.names);
  EXPECT_EQ(copy.children[0].value, original.children[0].value);
  EXPECT_EQ(copy.children[1].value, original.children[1].value);
  EXPECT_EQ(copy.flags, original.flags);
  EXPECT_EQ(copy.omitted, original.omitted);
}
} // namespace

// No extent metadata is added: a fixed array maps to ordinary standard packed/unpacked repetitions.
TEST(protobuf_fixed_array, exact_golden_bytes_and_split_occurrences) {
  protobuf_arrays::scalar_array value{};
  value.values = {0, 150, 300};
  const std::string packed{"\x0a\x05\x00\x96\x01\xac\x02", 7};
  EXPECT_EQ((codec::serialize_as<std::string, codec::protobuf_binary>(value)), packed);
  const std::string unpacked{"\x08\x00\x08\x96\x01\x08\xac\x02", 8};
  const std::string mixed{"\x0a\x00\x08\x00\x0a\x04\x96\x01\xac\x02", 10};
  EXPECT_EQ((decode<protobuf_arrays::scalar_array, codec::protobuf_binary>(packed)).values, value.values);
  EXPECT_EQ((decode<protobuf_arrays::scalar_array, codec::protobuf_binary>(unpacked)).values, value.values);
  EXPECT_EQ((decode<protobuf_arrays::scalar_array, codec::protobuf_binary>(mixed)).values, value.values);
}

// Strings, records, Boolean packing and format-specific omission use their ordinary mappings.
TEST(protobuf_fixed_array, round_trips_all_optional_formats) {
  round_trip<codec::protobuf_binary>();
  round_trip<codec::protojson>();
  round_trip<codec::textproto>();
  const auto decoded = decode<protobuf_arrays::record, codec::protojson>(
      R"({"names":["a","b"],"children":[{},{}],"flags":[true,false]})");
  EXPECT_EQ(decoded.children[0].value, 0u);
  EXPECT_EQ(decoded.children[1].value, 0u);
  EXPECT_EQ(decoded.omitted, (std::array<std::uint32_t, 3>{1, 2, 3}));
}

// Missing/null, short and excess repetitions cannot silently replace a fixed native cardinality.
TEST(protobuf_fixed_array, rejects_wrong_cardinality_and_null_elements) {
  for (const auto text : {R"({})", R"({"values":null})", R"({"values":[]})",
                          R"({"values":[1,2]})", R"({"values":[1,2,3,4]})",
                          R"({"values":[1,null,3]})"}) {
    EXPECT_ANY_THROW((decode<protobuf_arrays::scalar_array, codec::protojson>(text)));
  }
  for (const auto text : {"", "values: [1,2]", "values: [1,2,3,4]"}) {
    EXPECT_ANY_THROW((decode<protobuf_arrays::scalar_array, codec::textproto>(text)));
  }
  EXPECT_ANY_THROW((decode<protobuf_arrays::scalar_array, codec::protobuf_binary>(std::string{"\x08\x01", 2})));
  EXPECT_ANY_THROW((decode<protobuf_arrays::scalar_array, codec::protobuf_binary>(std::string{"\x0a\x04\x01\x02\x03\x04", 6})));
}

// Inline slots spend element/work limits without charging a heap buffer; exact reads stay transactional.
TEST(protobuf_fixed_array, resource_failure_preserves_existing_destination) {
  codec::decode_limits limits;
  limits.max_collection_elements = 2;
  const std::string packed{"\x0a\x03\x01\x02\x03", 5};
  EXPECT_ANY_THROW((decode<protobuf_arrays::scalar_array, codec::protobuf_binary>(packed, limits)));
  protobuf_arrays::scalar_array retained{};
  retained.values = {7, 8, 9};
  const auto input = rohit::make_constant_stream(packed.data(), packed.size());
  codec::protobuf_binary<codec::serialize_type::in, decltype(input)> reader{input, limits};
  EXPECT_ANY_THROW(retained.serialize_in(reader));
  EXPECT_EQ(retained.values, (std::array<std::uint32_t, 3>{7, 8, 9}));
}
