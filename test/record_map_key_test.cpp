// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "record_maps.hpp"
#include <gtest/gtest.h>
#include <rohit/serializer.hpp>

#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

namespace {
namespace codec = rohit::serializer;

// Both schema spellings keep the same native key type, map cardinality and encoded bytes.
template <template <codec::serialize_type> class Protocol>
void verify_record_keys() {
  using legacy = record_maps::legacy_table<std::uint64_t>;
  using generic = record_maps::generic_table<std::uint64_t>;
  static_assert(std::is_same_v<decltype(legacy::entries), decltype(generic::entries)>);
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  legacy value{};
  value.entries.emplace(record_maps::key<std::uint64_t>{maximum}, maximum);
  value.entries.emplace(record_maps::key<std::uint64_t>{1}, 7);
  rohit::full_stream_auto_alloc encoded{};
  value.template serialize_out<Protocol>(encoded);
  const auto input = rohit::make_constant_stream(encoded.begin(), encoded.current_offset());
  const auto restored = codec::deserialize_exact<generic, Protocol>(input);
  ASSERT_EQ(restored.entries.size(), value.entries.size());
  EXPECT_EQ(restored.entries.at(record_maps::key<std::uint64_t>{maximum}), maximum);
  EXPECT_EQ(restored.entries.at(record_maps::key<std::uint64_t>{1}), 7u);
  rohit::full_stream_auto_alloc reproduced{};
  restored.template serialize_out<Protocol>(reproduced);
  EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(encoded.begin()), encoded.current_offset()),
      std::string_view(reinterpret_cast<const char*>(reproduced.begin()), reproduced.current_offset()));
}

// Full-width record keys survive ordinary exact decode and unchanged legacy-map wire framing.
TEST(record_map_keys, native_templates_and_wire_equivalence) {
  verify_record_keys<codec::binary_none>();
  verify_record_keys<codec::binary_integer>();
  verify_record_keys<codec::binary_string>();
  verify_record_keys<codec::json>();
}
} // namespace
