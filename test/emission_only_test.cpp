// Copyright (c) 2026 Rohit Jairaj Singh. All rights reserved.
#include <emission_only/emission/emission_only.hpp>
#include <emission_only/owning/emission_only.hpp>
#include <gtest/gtest.h>
#include <rohit/constant_binary.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace borrowed = emission_models::emission;
namespace owned = emission_models;

using payload_output_method = void (borrowed::payload::*)(codec::binary_none_output&) const;
static_assert(std::same_as<decltype(&borrowed::payload::serialize_out), payload_output_method>);
static_assert(std::same_as<decltype(borrowed::payload::label), std::string_view>);
static_assert(std::same_as<decltype(borrowed::payload::data), std::span<const std::uint8_t>>);
static_assert(
    std::same_as<decltype(borrowed::fixed_packet::values), std::span<const std::uint16_t>>);
static_assert(std::is_trivially_copyable_v<borrowed::payload>);

// Own inline construction data and create borrowed generated fields only at traversal time.
struct fixed_payload_owner {
  static constexpr bool serializer_emission_only = true;
  static constexpr bool serializer_constant_evaluation = true;
  using serializer_constant_evaluation_type = fixed_payload_owner;

  std::array<char, 80> label{};
  std::array<std::uint8_t, 3> data{0, 128, 255};

  // Initialize the inline text without any string/vector allocation.
  constexpr fixed_payload_owner() {
    label.fill('a');
  }

  // Serialize a local view while all referenced owner buffers remain alive.
  constexpr void serialize_out(codec::binary_none_output& writer) const {
    borrowed::payload value{};
    value.sequence = 0x01020304;
    value.label = {label.data(), label.size()};
    value.data = data;
    value.serialize_out(writer);
  }
};

static_assert(fixed_payload_owner::serializer_constant_evaluation);

// Reconstruct inline source storage independently for counting and writing.
constexpr auto fixed_factory = [] { return fixed_payload_owner{}; };
constexpr auto fixed_bytes = codec::make_binary_none_bytes<fixed_factory>();
static_assert(fixed_bytes.size() == 90);
static_assert(fixed_bytes[0] == 4 && fixed_bytes[3] == 1);
static_assert(fixed_bytes[4] == 0x40 && fixed_bytes[5] == 80);
static_assert(fixed_bytes[86] == 3);
static_assert(fixed_bytes[87] == 0 && fixed_bytes[88] == 128 && fixed_bytes[89] == 255);

// Check the borrowed scalar collection wire order independently of native array layout.
constexpr auto span_factory = [] {
  std::array<std::uint16_t, 2> values{0x1234, 0xabcd};
  std::array<std::uint8_t, 5> bytes{};
  const auto written =
      codec::serialize_binary_none_to(bytes, std::span<const std::uint16_t>{values});
  if (written != bytes.size()) {
    throw "span size mismatch";
  }
  return bytes;
};
static_assert(span_factory() == std::array<std::uint8_t, 5>{2, 0x34, 0x12, 0xcd, 0xab});

// Establish the active class union lifetime before the non-template writer reads it.
constexpr auto union_factory = [] {
  borrowed::choice value{};
  value.data_type = borrowed::choice::e_data::node;
  std::construct_at(&value.data.node, borrowed::scalar_node{0x1234, true});
  return value;
};
constexpr auto union_bytes = codec::make_binary_none_bytes<union_factory>();
static_assert(union_bytes == std::array<std::uint8_t, 4>{1, 0x34, 0x12, 1});

// Decode exactly one generated owning model from bytes produced by the emission profile.
template <class Model, std::size_t Bytes>
Model decode(const std::array<std::uint8_t, Bytes>& bytes) {
  auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  codec::binary_none<codec::serialize_type::in, rohit::stream,
                     codec::binary_text_validation::strict, codec::read_policy::compatible>
      reader{input};
  Model result{};
  reader.serialize_in(result);
  reader.finish();
  return result;
}

// Compare emission output to the unchanged owning runtime writer for the decoded value.
template <class Model, std::size_t Bytes>
void verify_owning_parity(const Model& value, const std::array<std::uint8_t, Bytes>& bytes) {
  rohit::full_stream_auto_alloc output{};
  value.template serialize_out<codec::binary_none>(output);
  ASSERT_EQ(output.current_offset(), bytes.size());
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), output.begin()));
}

// Verify exact constant bytes are consumed by the existing owning reader and writer.
TEST(emission_only, inline_owner_and_runtime_reader) {
  const auto value = decode<owned::payload>(fixed_bytes);
  EXPECT_EQ(value.sequence, 0x01020304U);
  EXPECT_EQ(value.label, std::string(80, 'a'));
  EXPECT_EQ(value.data, (std::vector<std::uint8_t>{0, 128, 255}));
  verify_owning_parity(value, fixed_bytes);
  const fixed_payload_owner owner{};
  EXPECT_EQ(codec::binary_none_size(owner), fixed_bytes.size());
  std::array<std::uint8_t, fixed_bytes.size()> memory{};
  EXPECT_EQ(codec::serialize_binary_none_to(memory, owner), memory.size());
  EXPECT_EQ(memory, fixed_bytes);
}

// Exercise the concrete writer directly in both modes without generated method templates.
TEST(emission_only, concrete_output_and_spans) {
  const std::array<std::uint16_t, 3> values{1, 0x1234, 0xffff};
  borrowed::fixed_packet value{};
  value.values = values;
  codec::binary_none_output count{};
  EXPECT_TRUE(count.counting());
  value.serialize_out(count);
  EXPECT_EQ(count.position_bytes(), 7U);
  std::array<std::uint8_t, 7> memory{};
  codec::binary_none_output writer{memory};
  EXPECT_FALSE(writer.counting());
  value.serialize_out(writer);
  EXPECT_EQ(writer.position_bytes(), count.position_bytes());
  EXPECT_EQ(memory, (std::array<std::uint8_t, 7>{3, 1, 0, 0x34, 0x12, 0xff, 0xff}));
  const auto decoded = decode<owned::fixed_packet>(memory);
  EXPECT_EQ(decoded.values, values);
  verify_owning_parity(decoded, memory);
  value.values = std::span<const std::uint16_t>{values}.first(2);
  EXPECT_THROW(static_cast<void>(codec::binary_none_size(value)), std::invalid_argument);
  std::array<std::uint8_t, 4> short_memory{};
  short_memory.fill(0xcc);
  EXPECT_THROW(static_cast<void>(codec::serialize_binary_none_to(
                   short_memory, std::span<const std::uint16_t>{values})),
               rohit::exception::stream_overflow_exception);
  EXPECT_EQ(short_memory, (std::array<std::uint8_t, 4>{3, 0xcc, 0xcc, 0xcc}));
}

// Preserve selected union and nested borrowed string/object collection framing.
TEST(emission_only, unions_and_nested_collections) {
  const auto union_value = decode<owned::choice>(union_bytes);
  EXPECT_EQ(union_value.data_type, owned::choice::e_data::node);
  EXPECT_EQ(union_value.data.node.value, 0x1234U);
  EXPECT_TRUE(union_value.data.node.enabled);
  verify_owning_parity(union_value, union_bytes);
  const std::array<std::uint8_t, 2> data{4, 5};
  borrowed::payload inner{};
  inner.sequence = 9;
  inner.label = "inner";
  inner.data = data;
  const std::array items{inner};
  const std::array<std::string_view, 2> labels{"first", "second"};
  borrowed::nested_packet value{};
  value.inner = inner;
  value.items = items;
  value.labels = labels;
  std::array<std::uint8_t, 128> memory{};
  const auto count = codec::binary_none_size(value);
  const auto written = codec::serialize_binary_none_to(memory, value);
  ASSERT_EQ(written, count);
  auto input = rohit::make_constant_full_stream(memory.data(), written);
  codec::binary_none<codec::serialize_type::in> reader{input};
  owned::nested_packet decoded{};
  reader.serialize_in(decoded);
  reader.finish();
  ASSERT_EQ(decoded.items.size(), 1U);
  EXPECT_EQ(decoded.inner.label, "inner");
  EXPECT_EQ(decoded.items[0].data, (std::vector<std::uint8_t>{4, 5}));
  EXPECT_EQ(decoded.labels, (std::vector<std::string>{"first", "second"}));
}

// Apply magic, historical version selection and compact policies to the concrete protocol.
TEST(emission_only, magic_version_and_compact_fields) {
  borrowed::special_packet value{};
  value.version = 1;
  value.old_value = 0x1234;
  value.prefix = 64;
  value.variable = 128;
  std::array<std::uint8_t, 12> memory{};
  ASSERT_EQ(codec::binary_none_size(value), memory.size());
  ASSERT_EQ(codec::serialize_binary_none_to(memory, value), memory.size());
  const auto decoded = decode<owned::special_packet>(memory);
  EXPECT_EQ(decoded.version, 1U);
  EXPECT_EQ(decoded.old_value, 0x1234U);
  EXPECT_EQ(decoded.prefix, 64U);
  EXPECT_EQ(decoded.variable, 128U);
  verify_owning_parity(decoded, memory);
}
// Byte pools preserve exact wire bytes and reserve their whole payload before any copy.
TEST(emission_only, contiguous_octet_pool_bounds_and_parity) {
  std::array<std::uint8_t, 65> values{};
  for (std::size_t index = 0; index < values.size(); ++index) {
    values[index] = static_cast<std::uint8_t>(index * 7U);
  }
  const auto source = std::span<const std::uint8_t>{values};
  std::array<std::uint8_t, 69> guarded{};
  guarded.fill(0xcc);
  const auto destination = std::span<std::uint8_t>{guarded}.subspan(1, 67);
  ASSERT_EQ(codec::serialize_binary_none_to(destination, source), destination.size());
  EXPECT_EQ(codec::binary_none_size(source), destination.size());
  EXPECT_EQ(guarded.front(), 0xcc);
  EXPECT_EQ(guarded.back(), 0xcc);
  EXPECT_EQ(destination[0], 0x40);
  EXPECT_EQ(destination[1], values.size());
  EXPECT_TRUE(std::equal(values.begin(), values.end(), destination.begin() + 2));
  rohit::full_stream_auto_alloc reference{};
  codec::binary_none<codec::serialize_type::out> original{reference};
  original.serialize_out(std::vector<std::uint8_t>{values.begin(), values.end()});
  ASSERT_EQ(reference.current_offset(), destination.size());
  EXPECT_TRUE(std::equal(destination.begin(), destination.end(), reference.begin()));
  std::array<std::uint8_t, 4> short_memory{};
  short_memory.fill(0xcc);
  EXPECT_THROW(static_cast<void>(codec::serialize_binary_none_to(short_memory, source)),
               rohit::exception::stream_overflow_exception);
  EXPECT_EQ(short_memory, (std::array<std::uint8_t, 4>{0x40, 65, 0xcc, 0xcc}));
  std::array<std::uint8_t, 1> empty_bytes{0xcc};
  EXPECT_EQ(codec::serialize_binary_none_to(empty_bytes, std::span<const std::uint8_t>{}), 1U);
  EXPECT_EQ(empty_bytes[0], 0U);
}

} // namespace
