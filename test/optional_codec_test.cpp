// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <optional_cpp.hpp>
#include <generics.hpp>
#include <rohit/serializer.hpp>
#include <rohit/constant_binary_output.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace {
namespace codec = rohit::serializer;
using bytes = std::vector<std::uint8_t>;
static_assert(std::same_as<decltype(optional_cpp::scalar_record::value), std::optional<std::uint32_t>>);
static_assert(std::same_as<decltype(optional_cpp::nested_record::value),
                          std::optional<std::optional<std::uint32_t>>>);
static_assert(std::same_as<decltype(optional_cpp::record::numbers),
                          std::optional<std::vector<std::uint32_t>>>);
constexpr auto constant_present = codec::make_binary_none_bytes<[] {
  return std::optional<std::uint32_t>{42};
}>();
constexpr auto constant_nested_absent = codec::make_binary_none_bytes<[] {
  return std::optional<std::optional<std::uint32_t>>{std::in_place, std::nullopt};
}>();
constexpr auto constant_present_enum = codec::make_binary_none_bytes<[] {
  return std::optional<optional_cpp::color>{optional_cpp::color::green};
}>();
static_assert(constant_present == std::array<std::uint8_t, 5>{1, 42, 0, 0, 0});
static_assert(constant_nested_absent == std::array<std::uint8_t, 2>{1, 0});
static_assert(constant_present_enum == std::array<std::uint8_t, 2>{1, 1});

// Encode through each actual generated/native protocol with no hand-maintained field dispatch.
template <template <codec::serialize_type> class Protocol, typename Value>
bytes encode(const Value& value) {
  rohit::full_stream_auto_alloc output{};
  Protocol<codec::serialize_type::out> encoder{output};
  encoder.serialize_out(value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Decode into the supplied candidate and require exact-message completion.
template <template <codec::serialize_type> class Protocol, typename Value>
void decode(const bytes& source, Value& value, codec::decode_limits limits = {}) {
  const auto input = rohit::make_constant_full_stream(source.data(), source.size());
  Protocol<codec::serialize_type::in> decoder{input, limits};
  decoder.serialize_in(value);
  decoder.finish();
}

// Read JSON text through the same bounded exact native decode boundary.
template <typename Value>
void decode_json(std::string_view text, Value& value, codec::decode_limits limits = {}) {
  decode<codec::json>(bytes{text.begin(), text.end()}, value, limits);
}

// Populate every supported owning optional shape with distinguishable nested states.
optional_cpp::record sample() {
  optional_cpp::record result{};
  result.scalar = 42;
  result.child = optional_cpp::value_record{"child", {1, 2}};
  result.numbers = {3, 4};
  result.labels.emplace();
  result.labels->emplace("absent", std::nullopt);
  result.labels->emplace("present", 8);
  result.entries = {std::nullopt, 9};
  result.lookup.emplace("absent", std::nullopt);
  result.lookup.emplace("present", optional_cpp::value_record{"lookup", {5}});
  result.generic.value.emplace(std::nullopt);
  result.emplace_choice<optional_cpp::record::e_choice::child>(
      optional_cpp::value_record{"variant", {6}});
  return result;
}

// Qualify replacement, truncation, trailing bytes and nesting limits in all four native codecs.
template <template <codec::serialize_type> class Protocol>
void round_trip() {
  auto destination = sample();
  const auto source = encode<Protocol>(destination);
  decode<Protocol>(source, destination);
  EXPECT_EQ(encode<Protocol>(destination), source);
  ASSERT_TRUE(destination.generic.value);
  EXPECT_FALSE(*destination.generic.value);
  ASSERT_TRUE(destination.labels);
  EXPECT_FALSE(destination.labels->at("absent"));
  EXPECT_EQ(destination.labels->at("present"), 8u);
  ASSERT_TRUE(destination.lookup.at("present"));
  EXPECT_EQ(destination.lookup.at("present")->text, "lookup");
  for (std::size_t size = 0; size < source.size(); ++size) {
    SCOPED_TRACE(size);
    optional_cpp::record candidate{};
    EXPECT_THROW(decode<Protocol>(bytes{source.begin(), source.begin() + size}, candidate),
                 rohit::exception::base_parser);
  }
  auto trailing = source;
  trailing.push_back(0xff);
  EXPECT_THROW(decode<Protocol>(trailing, destination), rohit::exception::base_parser);
  const auto preserved = encode<Protocol>(destination);
  const auto exact_input = rohit::make_constant_full_stream(trailing.data(), trailing.size());
  EXPECT_THROW((destination = codec::deserialize_exact<optional_cpp::record, Protocol>(exact_input)),
               rohit::exception::base_parser);
  EXPECT_EQ(encode<Protocol>(destination), preserved);
  const optional_cpp::record absent{};
  decode<Protocol>(encode<Protocol>(absent), destination);
  EXPECT_FALSE(destination.scalar);
  EXPECT_FALSE(destination.child);
  EXPECT_FALSE(destination.numbers);
  EXPECT_FALSE(destination.labels);
  EXPECT_TRUE(destination.entries.empty());
  EXPECT_TRUE(destination.lookup.empty());
  codec::decode_limits limits{};
  limits.max_nesting_depth = 1;
  EXPECT_THROW(decode<Protocol>(source, destination, limits), codec::exception::resource_limit);
}
} // namespace

// Exact standard storage supports values, records, nested wrappers and generic instantiations.
TEST(optional_codec, native_round_trips) {
  round_trip<codec::json>();
  round_trip<codec::binary_none>();
  round_trip<codec::binary_integer>();
  round_trip<codec::binary_string>();
}

// Every optional level has a canonical zero-or-one JSON array; null never silently means absence.
TEST(optional_codec, canonical_json_and_rejection) {
  optional_cpp::nested_record value{};
  EXPECT_EQ(encode<codec::json>(value), (bytes{'{', '"', 'v', 'a', 'l', 'u', 'e', '"', ':', '[', ']', '}'}));
  decode_json("{\"value\":[[]]}", value);
  ASSERT_TRUE(value.value);
  EXPECT_FALSE(*value.value);
  decode_json("{\"value\":[[12]]}", value);
  ASSERT_TRUE(value.value && *value.value);
  EXPECT_EQ(**value.value, 12u);
  for (const auto text : {"{\"value\":null}", "{\"value\":[null]}",
                          "{\"value\":[[1],[2]]}", "{\"value\":[[1],]}",
                          "{\"value\":[[1,2]]}", "{\"value\":12}"}) {
    SCOPED_TRACE(text);
    EXPECT_THROW(decode_json(text, value), rohit::exception::base_parser);
    ASSERT_TRUE(value.value && *value.value);
    EXPECT_EQ(**value.value, 12u);
  }
  decode_json("{\"value\":[]}", value);
  EXPECT_FALSE(value.value);
}

// Runtime-only optional JSON retains its existing null/value representation outside a schema scope.
TEST(optional_codec, standalone_runtime_json_compatibility) {
  std::optional<std::uint32_t> value = 12;
  EXPECT_EQ(encode<codec::json>(value), (bytes{'1', '2'}));
  decode_json("null", value);
  EXPECT_FALSE(value);
  EXPECT_EQ(encode<codec::json>(value), (bytes{'n', 'u', 'l', 'l'}));
  native_generics::result<std::optional<std::uint32_t>> legacy{};
  const auto legacy_bytes = encode<codec::json>(legacy);
  EXPECT_EQ(std::string(legacy_bytes.begin(), legacy_bytes.end()),
            "{\"value\":null,\"success\":false}");
  decode_json("{\"value\":17,\"success\":true}", legacy);
  EXPECT_EQ(legacy.value, 17u);
}

// Anonymous schema applications retain canonical presence when serialized outside their parent.
TEST(optional_codec, standalone_schema_generic_application) {
  using plain_type = decltype(optional_cpp::record::plain_generic);
  plain_type value{};
  EXPECT_EQ(encode<codec::json>(value), (bytes{'{', '"', 'v', 'a', 'l', 'u', 'e', '"', ':', '[', ']', '}'}));
  decode_json("{\"value\":[17]}", value);
  EXPECT_EQ(value.value, 17u);
  using collection_type = decltype(optional_cpp::record::collection_generic);
  collection_type collection{};
  decode_json("{\"value\":[[],[17]]}", collection);
  ASSERT_EQ(collection.value.size(), 2u);
  EXPECT_FALSE(collection.value[0]);
  EXPECT_EQ(collection.value[1], 17u);
  optional_cpp::default_passthrough<> default_value{};
  const auto default_bytes = encode<codec::json>(default_value);
  EXPECT_EQ(std::string(default_bytes.begin(), default_bytes.end()), "{\"value\":[]}");
  decode_json("{\"value\":[23]}", default_value);
  EXPECT_EQ(default_value.value, 23u);
}

// String-key scalar enum spelling survives optional levels while collection elements stay ordinal.
TEST(optional_codec, optional_enum_field_context) {
  using color = optional_cpp::color;
  EXPECT_EQ(encode<codec::binary_none>(std::optional<color>{color::green}),
            (bytes{constant_present_enum.begin(), constant_present_enum.end()}));
  optional_cpp::enum_record value{};
  value.scalar = color::green;
  value.nested.emplace(color::blue);
  value.colors = {color::red, std::nullopt, color::blue};
  value.generic.value = color::green;
  value.plain_generic.value = color::blue;
  value.emplace_choice<optional_cpp::enum_record::e_choice::nested>(color::green);
  const bytes expected{
      6, 's', 'c', 'a', 'l', 'a', 'r', 1, 5, 'g', 'r', 'e', 'e', 'n',
      6, 'n', 'e', 's', 't', 'e', 'd', 1, 1, 4, 'b', 'l', 'u', 'e',
      6, 'c', 'o', 'l', 'o', 'r', 's', 3, 1, 0, 0, 1, 2,
      7, 'g', 'e', 'n', 'e', 'r', 'i', 'c', 5, 'v', 'a', 'l', 'u', 'e',
      1, 5, 'g', 'r', 'e', 'e', 'n', 0,
      13, 'p', 'l', 'a', 'i', 'n', '_', 'g', 'e', 'n', 'e', 'r', 'i', 'c',
      5, 'v', 'a', 'l', 'u', 'e', 1, 4, 'b', 'l', 'u', 'e', 0,
      13, 'c', 'h', 'o', 'i', 'c', 'e', ':', 'n', 'e', 's', 't', 'e', 'd',
      1, 1, 1, 0};
  EXPECT_EQ(encode<codec::binary_string>(value), expected);
  optional_cpp::enum_record destination{};
  decode<codec::binary_string>(expected, destination);
  EXPECT_EQ(encode<codec::binary_string>(destination), expected);
  for (const bytes& malformed : {bytes{1, 3, 'b', 'a', 'd'}, bytes{1, 5, 'g'}, bytes{2}}) {
    const auto input = rohit::make_constant_full_stream(malformed.data(), malformed.size());
    codec::binary_string<codec::serialize_type::in> reader{input};
    [[maybe_unused]] auto input_scope = reader.schema_values();
    EXPECT_THROW(reader.serialize_in_named_optional(value.scalar), rohit::exception::base_parser);
    EXPECT_EQ(value.scalar, color::green);
  }
}

// Invalid/truncated binary presence leaves an engaged destination unchanged at the optional boundary.
TEST(optional_codec, binary_tags_and_boundaries) {
  std::optional<std::uint32_t> value = 9;
  EXPECT_THROW(decode<codec::binary_none>(bytes{2}, value), codec::exception::bad_input_data);
  EXPECT_EQ(value, 9u);
  EXPECT_THROW(decode<codec::binary_none>(bytes{1, 0}, value), codec::exception::bad_input_data);
  EXPECT_EQ(value, 9u);
  decode<codec::binary_none>(bytes{0}, value);
  EXPECT_FALSE(value);
  EXPECT_EQ(encode<codec::binary_none>(std::optional<std::uint32_t>{42}), (bytes{1, 42, 0, 0, 0}));
}

// Presence and nesting consume the shared collection/allocation/work/input resource budgets.
TEST(optional_codec, bounded_presence) {
  optional_cpp::scalar_record value{};
  codec::decode_limits limits{};
  limits.max_allocation_bytes = sizeof(std::uint32_t) - 1;
  EXPECT_THROW(decode_json("{\"value\":[1]}", value, limits), codec::exception::resource_limit);
  decode_json("{\"value\":[]}", value, limits);
  limits = {};
  limits.max_work_units = 0;
  EXPECT_THROW(decode_json("{\"value\":[]}", value, limits), codec::exception::resource_limit);
  limits = {};
  limits.max_input_bytes = 1;
  EXPECT_THROW(decode_json("{\"value\":[]}", value, limits), codec::exception::resource_limit);
}
