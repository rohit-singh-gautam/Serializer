#include <rohit/constant_binary.hpp>

#include "constant_evaluation.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace models = constant_binary_models;

// Build a transient generated model whose string necessarily exceeds usual SSO storage.
constexpr auto make_payload = [] {
  models::payload value{};
  value.sequence = 0x01020304;
  value.label = std::string(80, 'a');
  value.data = {0, 128, 255};
  return value;
};

constexpr auto payload_bytes = codec::make_binary_none_bytes<make_payload>();
static_assert(payload_bytes.size() == 90);
static_assert(payload_bytes[0] == 4 && payload_bytes[3] == 1);
static_assert(payload_bytes[4] == 0x40 && payload_bytes[5] == 80);
static_assert(payload_bytes[86] == 3 && payload_bytes[87] == 0 && payload_bytes[88] == 128 &&
              payload_bytes[89] == 255);

constexpr auto empty_bytes = codec::make_binary_none_bytes<[] { return models::empty{}; }>();
static_assert(empty_bytes.empty());

constexpr auto signed_bytes = codec::make_binary_none_bytes<[] { return std::int64_t{-2}; }>();
static_assert(signed_bytes ==
              std::array<std::uint8_t, 8>{0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
constexpr auto signed_minimum_bytes =
    codec::make_binary_none_bytes<[] { return std::numeric_limits<std::int32_t>::min(); }>();
static_assert(signed_minimum_bytes == std::array<std::uint8_t, 4>{0, 0, 0, 0x80});
constexpr auto unsigned_maximum_bytes =
    codec::make_binary_none_bytes<[] { return std::numeric_limits<std::uint64_t>::max(); }>();
static_assert(unsigned_maximum_bytes ==
              std::array<std::uint8_t, 8>{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
constexpr auto negative_zero = codec::make_binary_none_bytes<[] { return -0.0; }>();
static_assert(negative_zero == std::array<std::uint8_t, 8>{0, 0, 0, 0, 0, 0, 0, 0x80});

constexpr auto prefix_boundary = codec::make_binary_none_bytes<[] {
  return codec::detail::compact_output<true, true, std::uint32_t>{16384};
}>();
static_assert(prefix_boundary == std::array<std::uint8_t, 3>{0x80, 0x40, 0});

constexpr auto compact_bytes = codec::make_binary_none_bytes<[] {
  models::compact value{};
  value.prefix = 64;
  value.truncated = 0x40000000;
  value.variable = std::numeric_limits<std::uint64_t>::max();
  return value;
}>();
static_assert(compact_bytes == std::array<std::uint8_t, 13>{0x40, 0x40, 0, 0xff, 0xff, 0xff, 0xff,
                                                            0xff, 0xff, 0xff, 0xff, 0xff, 1});

constexpr auto union_bytes = codec::make_binary_none_bytes<[] {
  models::choice value{};
  value.data_type = models::choice::e_data::node;
  std::construct_at(std::addressof(value.data.node), models::scalar_node{0x1234, true});
  return value;
}>();
static_assert(union_bytes == std::array<std::uint8_t, 4>{2, 0x34, 0x12, 1});

constexpr auto inherited_bytes = codec::make_binary_none_bytes<[] {
  models::inherited value{};
  value.value = 0x1234;
  value.enabled = true;
  value.sequence = 0x01020304;
  return value;
}>();
static_assert(inherited_bytes == std::array<std::uint8_t, 7>{0x34, 0x12, 1, 4, 3, 2, 1});

constexpr auto magic_bytes = codec::make_binary_none_bytes<[] {
  models::magic_payload value{};
  value.value = 0x1234;
  value.omitted = 0xffffffff;
  return value;
}>();
static_assert(magic_bytes == std::array<std::uint8_t, 6>{'C', 'B', 'I', 'N', 0x34, 0x12});
constexpr auto typed_magic_bytes =
    codec::make_binary_none_bytes<[] { return models::typed_magic{0x80}; }>();
static_assert(typed_magic_bytes == std::array<std::uint8_t, 5>{4, 3, 2, 1, 0x80});

constexpr auto historical_bytes = codec::make_binary_none_bytes<[] {
  models::versioned value{};
  value.version = 1;
  value.old_value = 0x1234;
  return value;
}>();
static_assert(historical_bytes == std::array<std::uint8_t, 3>{1, 0x34, 0x12});
constexpr auto dotted_bytes = codec::make_binary_none_bytes<[] {
  models::dotted value{};
  value.value = 0x80;
  return value;
}>();
static_assert(dotted_bytes == std::array<std::uint8_t, 7>{1, 0, 2, 0, 0, 0, 0x80});

constexpr auto floating_revision_bytes = codec::make_binary_none_bytes<[] {
  models::floating_revision value{};
  value.value = 0x80;
  return value;
}>();
static_assert(floating_revision_bytes ==
              std::array<std::uint8_t, 9>{0, 0, 0, 0, 0, 0, 4, 0x40, 0x80});
constexpr auto generic_bytes = codec::make_binary_none_bytes<[] {
  models::fixed_box<3> value{};
  value.values = {0, 0x1234, 0xffff};
  return value;
}>();
static_assert(generic_bytes == std::array<std::uint8_t, 7>{3, 0, 0, 0x34, 0x12, 0xff, 0xff});

constexpr auto fixed_array_bytes =
    codec::make_binary_none_bytes<[] { return std::array<std::uint16_t, 3>{0, 0x1234, 0xffff}; }>();
static_assert(fixed_array_bytes == std::array<std::uint8_t, 7>{3, 0, 0, 0x34, 0x12, 0xff, 0xff});
constexpr auto unicode_bytes =
    codec::make_binary_none_bytes<[] { return std::string_view{"a\0\xc3\xa9", 4}; }>();
static_assert(unicode_bytes == std::array<std::uint8_t, 5>{4, 'a', 0, 0xc3, 0xa9});
constexpr auto boolean_bytes =
    codec::make_binary_none_bytes<[] { return std::array<bool, 3>{true, false, true}; }>();
static_assert(boolean_bytes == std::array<std::uint8_t, 4>{3, 1, 0, 1});

// Check exact scalar values after the existing reader consumes constant-compatible wire bytes.
template <class Value> void verify_scalar(Value value) {
  std::array<std::uint8_t, sizeof(Value)> bytes{};
  ASSERT_EQ(codec::binary_none_size(value), bytes.size());
  ASSERT_EQ(codec::serialize_binary_none_to(bytes, value), bytes.size());
  std::array<std::uint8_t, sizeof(Value)> additive_bytes{};
  codec::binary_none_output explicit_writer{additive_bytes};
  explicit_writer.serialize_out(value);
  EXPECT_EQ(explicit_writer.position_bytes(), bytes.size());
  EXPECT_EQ(additive_bytes, bytes);
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  codec::binary_none<codec::serialize_type::in> reader{input};
  Value decoded{};
  reader.serialize_in(decoded);
  reader.finish();
  if constexpr (std::floating_point<Value>) {
    EXPECT_EQ((std::bit_cast<std::array<std::uint8_t, sizeof(Value)>>(decoded)),
              (std::bit_cast<std::array<std::uint8_t, sizeof(Value)>>(value)));
  } else {
    EXPECT_EQ(decoded, value);
  }
}

// Preserve fixed-width scalar extrema and floating-point representation bits.
TEST(constant_binary, fixed_scalar_values) {
  verify_scalar(std::numeric_limits<std::int8_t>::min());
  verify_scalar(std::numeric_limits<std::uint8_t>::max());
  verify_scalar(std::numeric_limits<std::int16_t>::min());
  verify_scalar(std::numeric_limits<std::uint16_t>::max());
  verify_scalar(std::numeric_limits<std::int32_t>::min());
  verify_scalar(std::numeric_limits<std::uint32_t>::max());
  verify_scalar(std::numeric_limits<std::int64_t>::min());
  verify_scalar(std::numeric_limits<std::uint64_t>::max());
  verify_scalar(true);
  verify_scalar(false);
  verify_scalar(static_cast<char>(0xff));
  verify_scalar(1.0F);
  verify_scalar(-0.0);
  verify_scalar(std::bit_cast<float>(std::uint32_t{0x7fc12345}));
}

// Verify a generated owning value against the current optimized writer and exact runtime reader.
template <class Value> void verify_runtime_parity(const Value& value) {
  const auto count = codec::binary_none_size(value);
  std::vector<std::uint8_t> memory(count + 1, 0xcc);
  EXPECT_EQ(codec::serialize_binary_none_to(std::span<std::uint8_t>{memory}.first(count), value),
            count);
  EXPECT_EQ(memory.back(), 0xcc);
  rohit::full_stream_auto_alloc reference{};
  codec::binary_none<codec::serialize_type::out> writer{reference};
  writer.serialize_out(value);
  ASSERT_EQ(reference.current_offset(), count);
  EXPECT_TRUE(std::equal(memory.begin(), memory.begin() + count, reference.begin()));
  const auto input = rohit::make_constant_full_stream(memory.data(), count);
  Value decoded{};
  codec::binary_none<codec::serialize_type::in> reader{input};
  reader.serialize_in(decoded);
  reader.finish();
  EXPECT_EQ(codec::binary_none_size(decoded), count);
}

// Verify the selected active union member through the actual runtime decoder.
TEST(constant_binary, constant_union_decode) {
  const auto input = rohit::make_constant_full_stream(union_bytes.data(), union_bytes.size());
  codec::binary_none<codec::serialize_type::in> reader{input};
  models::choice decoded{};
  reader.serialize_in(decoded);
  reader.finish();
  EXPECT_EQ(decoded.data_type, models::choice::e_data::node);
  EXPECT_EQ(decoded.data.node.value, 0x1234);
  EXPECT_TRUE(decoded.data.node.enabled);
  models::choice number{};
  number.data_type = models::choice::e_data::number;
  number.data.number = 0x12345678;
  verify_runtime_parity(number);
  models::choice real{};
  real.data_type = models::choice::e_data::real;
  real.data.real = -0.0F;
  verify_runtime_parity(real);
}

// Verify every prefix-width boundary without allocating a payload proportional to the value.
TEST(constant_binary, compact_boundaries) {
  for (const auto value : {0U, 63U, 64U, 16383U, 16384U, 4194303U, 4194304U, 1073741823U}) {
    const codec::detail::compact_output<true, true, std::uint32_t> encoded{value};
    const auto count = codec::binary_none_size(encoded);
    EXPECT_EQ(count, codec::detail::fixed_prefix_bytes(value));
    std::array<std::uint8_t, 4> memory{};
    EXPECT_EQ(codec::serialize_binary_none_to(memory, encoded), count);
  }
  EXPECT_THROW((static_cast<void>(codec::binary_none_size(
                   codec::detail::compact_output<true, true, std::uint32_t>{0x40000000}))),
               std::out_of_range);
  EXPECT_EQ((codec::binary_none_size(
                codec::detail::compact_output<true, false, std::uint64_t>{0x40000000})),
            1U);
}

// Decode the actual constant-produced bytes, checking values rather than just their size.
TEST(constant_binary, generated_payload_decode) {
  const auto input = rohit::make_constant_full_stream(payload_bytes.data(), payload_bytes.size());
  models::payload decoded{};
  codec::binary_none<codec::serialize_type::in> reader{input};
  reader.serialize_in(decoded);
  reader.finish();
  EXPECT_EQ(decoded.sequence, 0x01020304U);
  EXPECT_EQ(decoded.label, std::string(80, 'a'));
  EXPECT_EQ(decoded.data, (std::vector<std::uint8_t>{0, 128, 255}));
  const auto value = make_payload();
  verify_runtime_parity(value);
  std::array<std::uint8_t, payload_bytes.size()> runtime_bytes{};
  EXPECT_EQ(codec::serialize_binary_none_to(runtime_bytes, value), runtime_bytes.size());
  EXPECT_EQ(runtime_bytes, payload_bytes);
}

// Cover recursively generated containers, fixed counts, Unicode and embedded NULs.
TEST(constant_binary, collections_and_maps) {
  models::containers value{};
  value.fixed = {0, 0x1234, 0xffff};
  value.labels = {"", std::string{"a\0b", 3}, "\xc3\xa9"};
  value.values = {make_payload(), models::payload{}};
  value.state = models::phase::active;
  verify_runtime_parity(value);
  models::runtime_map map{};
  map.values.emplace(255, "last");
  map.values.emplace(0, "first");
  verify_runtime_parity(map);
}

// Reject invalid text before counting or writing its prefix, matching the existing strict codec.
TEST(constant_binary, strict_validation) {
  for (const std::string_view text :
       {"\x80", "\xc0\xaf", "\xc2", "\xed\xa0\x80", "\xf4\x90\x80\x80"}) {
    EXPECT_THROW(static_cast<void>(codec::binary_none_size(text)), std::invalid_argument);
    std::array<std::uint8_t, 16> memory{};
    memory.fill(0xcc);
    EXPECT_THROW(static_cast<void>(codec::serialize_binary_none_to(memory, text)),
                 std::invalid_argument);
    EXPECT_TRUE(std::all_of(memory.begin(), memory.end(), [](auto byte) { return byte == 0xcc; }));
  }
  EXPECT_THROW(static_cast<void>(codec::binary_none_size(static_cast<models::phase>(2))),
               std::invalid_argument);
  models::choice invalid_union{};
  invalid_union.data_type = static_cast<models::choice::e_data>(3);
  EXPECT_THROW(static_cast<void>(codec::binary_none_size(invalid_union)), std::invalid_argument);
  models::floating_revision invalid_floating{};
  invalid_floating.revision = std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW(static_cast<void>(codec::binary_none_size(invalid_floating)), std::invalid_argument);
  invalid_floating.revision = std::numeric_limits<double>::infinity();
  EXPECT_THROW(static_cast<void>(codec::binary_none_size(invalid_floating)), std::invalid_argument);
  models::versioned invalid_version{};
  invalid_version.version = 3;
  EXPECT_THROW(static_cast<void>(codec::binary_none_size(invalid_version)), std::invalid_argument);
}

// Preserve exact capacity, subspan boundaries and reservation failure semantics.
TEST(constant_binary, memory_boundaries) {
  models::scalar_node value{0x1234, true};
  std::array<std::uint8_t, 5> memory{};
  memory.fill(0xcc);
  EXPECT_EQ(codec::serialize_binary_none_to(std::span<std::uint8_t>{memory}.subspan(1, 3), value),
            3U);
  EXPECT_EQ(memory, (std::array<std::uint8_t, 5>{0xcc, 0x34, 0x12, 1, 0xcc}));
  for (std::size_t capacity = 0; capacity != 3; ++capacity) {
    memory.fill(0xcc);
    EXPECT_THROW(static_cast<void>(codec::serialize_binary_none_to(
                     std::span<std::uint8_t>{memory}.first(capacity), value)),
                 rohit::exception::stream_overflow_exception);
    EXPECT_TRUE(std::all_of(memory.begin(), memory.end(), [](auto byte) { return byte == 0xcc; }));
  }
  EXPECT_EQ(codec::serialize_binary_none_to(std::span<std::uint8_t>{}, models::empty{}), 0U);
  models::long_run run{};
  EXPECT_EQ(codec::binary_none_size(run), 17U);
}

// Exercise the same checks used during constant evaluation without allocating a giant destination.
TEST(constant_binary, counting_and_constant_sink_bounds) {
  codec::detail::binary_count_sink count{};
  count.reserve_bytes(5);
  EXPECT_EQ(count.position_bytes(), 0U);
  count.commit_counted_bytes(rohit::detail::maximum_buffer_bytes);
  EXPECT_THROW(count.commit_counted_bytes(1), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(count.position_bytes(), rohit::detail::maximum_buffer_bytes);
  std::array<std::uint8_t, 2> memory{0xcc, 0xcc};
  codec::detail::binary_memory_sink sink{memory};
  codec::detail::constant_binary_out writer{sink};
  EXPECT_THROW(writer.struct_serialize_out_fixed(std::uint16_t{0x1234}, true),
               rohit::exception::stream_overflow_exception);
  EXPECT_EQ(sink.position_bytes(), 0U);
  EXPECT_EQ(memory, (std::array<std::uint8_t, 2>{0xcc, 0xcc}));
}

} // namespace
