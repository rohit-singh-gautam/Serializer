// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <auto_size_constant/emission/auto_size_constant.hpp>
#include <auto_size_constant/owning/auto_size_constant.hpp>
#include <rohit/constant_binary.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace {
namespace codec = rohit::serializer;
namespace owned = sized_constant_models;
namespace borrowed = sized_constant_models::emission;

template <typename Value>
concept has_size_storage = requires(Value value) { value.encoded_size; };
static_assert(!has_size_storage<owned::leaf> && !has_size_storage<borrowed::leaf>);

// Build fresh owning storage independently in the counting and exact-byte writing traversals.
constexpr auto owning_factory = [] {
  owned::envelope value{};
  value.encoded_size = 666;
  const auto fill = [](auto& leaf) {
    leaf.label = "leaf";
    leaf.values[0] = 0x1234;
    leaf.values[1] = 0xabcd;
    leaf.values[2] = 7;
    leaf.scratch = 99;
  };
  fill(value.payload);
  for (auto& leaf : value.children) { fill(leaf); }
  return value;
};

constexpr auto owning_bytes = codec::make_binary_none_bytes<owning_factory>();
constexpr std::size_t leaf_bytes = sizeof(std::uint16_t) + sizeof(std::uint64_t) + 5 + 7;
constexpr std::size_t envelope_bytes = 1 + sizeof(std::uint32_t) + leaf_bytes + 1 + 2 * leaf_bytes;
static_assert(owning_bytes.size() == envelope_bytes);
static_assert(owning_bytes[0] == 'E' && owning_bytes[1] == envelope_bytes);
static_assert(owning_bytes[5] == 0x34 && owning_bytes[6] == 0x12);
static_assert(owning_bytes[7] == leaf_bytes - sizeof(std::uint64_t));
static_assert(owning_bytes[15] == 4 && owning_bytes[16] == 'l');
static_assert(owning_bytes[20] == 3 && owning_bytes[21] == 0x34 && owning_bytes[22] == 0x12);

constexpr std::array<std::uint16_t, 3> borrowed_values{0x1234, 0xabcd, 7};
// Bind only immutable source storage whose lifetime covers both consteval traversals.
constexpr auto borrowed_leaf = [] {
  borrowed::leaf value{};
  value.label = "leaf";
  value.values = borrowed_values;
  value.scratch = 99;
  return value;
}();
constexpr std::array borrowed_children{borrowed_leaf, borrowed_leaf};
constexpr auto borrowed_factory = [] {
  borrowed::envelope value{};
  value.encoded_size = 666;
  value.payload = borrowed_leaf;
  value.children = borrowed_children;
  return value;
};
constexpr auto emission_bytes = codec::make_binary_none_bytes<borrowed_factory>();
static_assert(emission_bytes == owning_bytes);
static_assert(codec::binary_none_size(borrowed_factory()) == envelope_bytes);

// Decode compile-time bytes with the ordinary bounded reader, including its derived-size checks.
owned::envelope decode() {
  auto input = rohit::make_constant_stream(owning_bytes.data(), owning_bytes.size());
  codec::binary<codec::serialize_type::in, codec::serialize_key_type::none,
                std::endian::little, decltype(input)> reader{input};
  owned::envelope value{};
  reader.serialize_in(value);
  reader.finish();
  return value;
}
} // namespace

// Both compile-time profiles preserve the normal record/payload wire values and source metadata.
TEST(auto_size_constant, exact_schema_bytes_and_runtime_parity) {
  const auto value = owning_factory();
  rohit::full_stream_auto_alloc output{};
  codec::binary_none<codec::serialize_type::out> writer{output};
  writer.serialize_out(value);
  ASSERT_EQ(output.current_offset(), owning_bytes.size());
  EXPECT_TRUE(std::equal(owning_bytes.begin(), owning_bytes.end(), output.begin()));
  EXPECT_EQ(value.encoded_size, 666u);
  const auto restored = decode();
  EXPECT_EQ(restored.encoded_size, owning_bytes.size());
  EXPECT_EQ(restored.payload.label, "leaf");
  EXPECT_EQ(restored.children[1].values[1], 0xabcdu);
}

// Runtime use of the same borrowed writer retains counting and bounded-buffer failure semantics.
TEST(auto_size_constant, emission_count_and_destination_bounds) {
  const auto value = borrowed_factory();
  std::array<std::uint8_t, envelope_bytes> output{};
  EXPECT_EQ(codec::serialize_binary_none_to(output, value), emission_bytes.size());
  EXPECT_EQ(output, emission_bytes);
  EXPECT_THROW(static_cast<void>(codec::serialize_binary_none_to(
                   std::span{output}.first(output.size() - 1), value)),
               rohit::exception::stream_overflow_exception);
  codec::binary_none_output counter{};
  EXPECT_EQ(counter.measure_record_size<std::uint32_t>(value), 0u);
  codec::binary_none_output writer{output};
  EXPECT_THROW(static_cast<void>(writer.measure_record_size<std::uint32_t>(value, envelope_bytes + 1)),
               std::invalid_argument);
}
