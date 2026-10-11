// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "extended_construction.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace {
// Resolve a bounded construction fixture with its explicit declared language contract.
auto parse_extended_construction(std::string_view text) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(text.data(), text.size()));
}
}

// Native defaults initialize fresh values while codec default construction retains schema defaults.
TEST(extended_construction, defaults_preserve_codec_construction) {
  constexpr construction_defaults defaults{};
  static_assert(defaults.x == 1 && defaults.y == 2);
  constexpr construction_defaults supplied{5};
  static_assert(supplied.x == 5 && supplied.y == 9);
  static_assert(construction_defaults::create().x == 7);
  static_assert(construction_defaults::create_default_0().y == 9);
  static_assert(construction_defaults::create_default_1(4).x == 4);
  static_assert(!std::is_convertible_v<std::uint32_t, construction_defaults>);
  EXPECT_EQ(construction_defaults::create(3, 8).y, 8u);
}

// Dimensioned bindings retain exact native array storage and positional wire cardinality.
TEST(extended_construction, fixed_array_bindings) {
  constexpr construction_triplet value{std::array<std::int32_t, 3>{1, 2, 3}};
  static_assert(value.values[2] == 3);
  static_assert(std::is_same_v<decltype(value.values), std::array<std::int32_t, 3>>);
  constexpr construction_fixed_vector<double, 6> generic{std::array<double, 6>{1, 2, 3, 4, 5, 6}};
  static_assert(generic.values[5] == 6);
  rohit::full_stream_auto_alloc encoded{};
  value.serialize_out<rohit::serializer::binary_none>(encoded);
  auto input = rohit::make_constant_stream(encoded.begin(), encoded.current_offset());
  const auto decoded = rohit::serializer::deserialize_exact<construction_triplet,
      rohit::serializer::binary_none>(input);
  EXPECT_EQ(decoded.values, value.values);
}

// Checked construction preserves the full integer range and rejects every reached narrowing failure.
TEST(extended_construction, separately_constrained_checked_inputs) {
  constexpr construction_small_measure small{std::uint32_t{65535}};
  static_assert(small.value == std::numeric_limits<std::uint16_t>::max());
  constexpr construction_unsigned_measure maximum{std::numeric_limits<std::uint64_t>::max()};
  static_assert(maximum.value == std::numeric_limits<std::uint64_t>::max());
  constexpr construction_signed_measure minimum{std::numeric_limits<std::int64_t>::min()};
  static_assert(minimum.value == std::numeric_limits<std::int64_t>::min());
  static_assert(!std::is_convertible_v<std::uint32_t, construction_small_measure>);
  static_assert(!std::is_constructible_v<construction_small_measure, bool>);
  static_assert(!std::is_constructible_v<construction_small_measure, construction_signed_measure>);
  EXPECT_THROW(construction_small_measure{std::uint32_t{65536}}, std::domain_error);
  EXPECT_THROW(construction_unsigned_measure{-1}, std::domain_error);
  EXPECT_THROW(construction_signed_measure{1.5}, std::domain_error);
  EXPECT_THROW(construction_floating_measure{std::numeric_limits<std::uint64_t>::max()}, std::domain_error);
}

// Scalar projections and structured defaults use checked bindings without arbitrary constructor code.
TEST(extended_construction, checked_projection_and_structured_defaults) {
  const construction_wide_value input{std::int64_t{42}};
  EXPECT_EQ(construction_projected_value{input}.value, 42);
  EXPECT_THROW(construction_projected_value{construction_wide_value{
      std::numeric_limits<std::int64_t>::max()}}, std::domain_error);
  EXPECT_EQ(construction_projected_value::half(12).value, 6);
  EXPECT_THROW(static_cast<void>(construction_projected_value::half(11)), std::domain_error);
  constexpr construction_checked_default defaults{};
  static_assert(defaults.value.value == 0);
}

// New syntax is gated by the declaring language contract and retains exact field/type checks.
TEST(extended_construction, rejects_invalid_bindings) {
  const std::string_view rejected[]{
      "serializer version 1.10.0; class p { public uint32 x; public constructor(uint32 input = 1) { x = input; } }",
      "serializer version 1.10.0; class p { public uint32 x; public constructor(uint32 input) explicit { x = input; } }",
      "serializer version 1.11.0; class p { public uint32 x; public constructor(uint32 a = 1, uint32 b) { x = a; } }",
      "serializer version 1.11.0; class p { public array[3] uint32 x; public constructor(array[2] uint32 input) { x = input; } }",
      "serializer version 1.11.0; class p { public array[3] uint32 x; public factory bad() { x = 1; } }",
      "serializer version 1.11.0; class p { public array[0] uint32 x; public constructor(array[0] uint32 input) { x = input; } }",
      "serializer version 1.11.0; class p { public uint8 x; public constructor(uint8 input = 256) { x = input; } }",
      "serializer version 1.11.0; class p { public uint32 x; public constructor(uint64 input) { x = input; } }",
      "serializer version 1.11.0; class p { public uint32 x; public constructor(uint64 input) { x = checked<uint64>(input); } }",
      "serializer version 1.11.0; class p { public uint32 x; public constructor(uint32 input) { x = x + input; } }",
  };
  for (const auto schema : rejected) { EXPECT_THROW(parse_extended_construction(schema), std::exception); }
}

// Backend qualification is explicit: portable owning defaults/arrays do not imply method input templates.
TEST(extended_construction, diagnoses_unavailable_checked_backends) {
  namespace writer = rohit::serializer::writer;
  writer::cpp_options options;
  options.format = false;
  for (const auto source : {
      "serializer version 1.11.0; class p { public uint32 x; public constructor(uint64 input) explicit { x = checked<uint32>(input); } }",
      "serializer version 1.11.0; class p { public uint32 x; public constructor<Input: integral && !bool>(Input input) explicit { x = checked<uint32>(input); } }"}) {
    const auto schema = parse_extended_construction(source);
    EXPECT_NO_THROW(writer::cpp::generate(schema, options));
    EXPECT_THROW(writer::java::generate(schema, "Schema", {}), std::invalid_argument);
    for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
      EXPECT_THROW(writer::portable::generate(schema, language, "Schema", {}), std::invalid_argument) << language;
    }
  }
}

// Shortened member APIs and owner-prefixed C factory APIs reserve names before emission.
TEST(extended_construction, reserves_default_overload_symbols) {
  namespace writer = rohit::serializer::writer;
  writer::cpp_options options;
  options.format = false;
  options.rename_identifiers = false;
  const auto member_collision = parse_extended_construction(
      "serializer version 1.11.0; class p { public uint32 x; public factory make(uint32 input = 1) { x = input; } "
      "public function uint32 make_default_0() readonly expression(0); }");
  EXPECT_THROW(writer::cpp::generate(member_collision, options), std::invalid_argument);
  const auto c_collision = parse_extended_construction(
      "serializer version 1.11.0; class p { public uint32 x; public factory make(uint32 input = 1) { x = input; } } "
      "class p_make {}");
  EXPECT_THROW(writer::portable::generate(c_collision, "c", "Schema", {}), std::invalid_argument);
}
