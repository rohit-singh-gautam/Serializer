// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "overloaded_construction.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {
// Parse each source with its declaring language contract and bounded input ownership.
auto parse_overloaded_construction(std::string_view source) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}
}

// Numeric and record input overloads preserve native copying, strong types and constant evaluation.
TEST(overloaded_construction, checked_record_and_numeric_inputs) {
  constexpr overloaded_quantity<std::uint32_t> numeric{std::uint16_t{42}};
  constexpr overloaded_quantity<double> converted{numeric};
  constexpr overloaded_byte compact{numeric};
  constexpr auto created = overloaded_byte::create(numeric);
  static_assert(converted.value == 42.0 && compact.value == 42 && created.value == 42);
  constexpr overloaded_byte copied{compact};
  static_assert(copied.value == compact.value);
  static_assert(overloaded_byte{}.value == 0);
  static_assert(!std::is_convertible_v<decltype(numeric), overloaded_byte>);
  static_assert(!std::is_constructible_v<overloaded_byte, bool>);
  static_assert(!std::is_constructible_v<overloaded_byte, overloaded_angle<double>>);
  EXPECT_THROW(overloaded_byte{overloaded_quantity<std::uint32_t>{std::uint32_t{256}}}, std::domain_error);
  EXPECT_EQ(overloaded_wide::create(std::uint32_t{43}).value, 43u);
}

// Shortened overloads retain their selected trailing defaults and fresh field initialization.
TEST(overloaded_construction, distinct_default_argument_lists) {
  const overloaded_options numbered{std::uint32_t{5}};
  const overloaded_options named{std::string{"named"}};
  EXPECT_EQ(numbered.count, 5u);
  EXPECT_EQ(numbered.label, "initial");
  EXPECT_EQ(named.count, 1u);
  EXPECT_EQ(named.label, "named");
  EXPECT_EQ(overloaded_options::create_default_1(std::uint32_t{7}).count, 7u);
  EXPECT_EQ(overloaded_options::create_default_1(std::string{"other"}).label, "other");
}

// Dependent dimensions and digest storage extents produce genuinely different native overloads.
TEST(overloaded_construction, preserves_distinct_dimension_and_digest_storage) {
  constexpr overloaded_dimensions triplet{overloaded_sized_input<std::uint32_t, 3>{}};
  constexpr overloaded_dimensions quadruplet{overloaded_sized_input<std::uint32_t, 4>{}};
  constexpr overloaded_digests narrow{std::array<std::uint8_t, 16>{}};
  constexpr overloaded_digests wide{std::array<std::uint8_t, 32>{}};
  static_assert(triplet.size == 3 && quadruplet.size == 4);
  static_assert(narrow.size == 16 && wide.size == 32);
  EXPECT_EQ(overloaded_digests::create(std::array<std::uint8_t, 32>{}).size, 32u);
}

// Declaration validation rejects old language syntax, duplicate shapes and conflicting default arities.
TEST(overloaded_construction, rejects_ambiguous_or_legacy_declarations) {
  const std::string_view rejected[]{
      "serializer version 1.11.0; class value { public uint32 x; public constructor(uint32 a) { x = a; } public constructor(uint32 a, uint32 b) { x = a; } }",
      "serializer version 1.12.0; class value { public uint32 x; public constructor(uint32 a) { x = a; } public constructor(uint32 b) { x = b; } }",
      "serializer version 1.12.0; class value { public uint32 x; public constructor(uint32 a) { x = a; } public constructor(uint32 a, uint32 b = 1) { x = a; } }",
      "serializer version 1.12.0; class value { public uint32 x; public constructor(uint32 a = 1) { x = a; } public constructor(double b = 2.0) { x = 3; } }",
      "serializer version 1.12.0; class value { public uint32 x; public constructor<Input: integral && !bool>(Input a) { x = checked<uint32>(a); } public constructor<Value: integral && !bool>(Value b) { x = checked<uint32>(b); } }",
      "serializer version 1.12.0; class value { public uint32 x; public constructor<First: integral, Second: integral>(First a, Second b) { x = 1; } public constructor<Second: integral, First: integral>(First a, Second b) { x = 2; } }",
      "serializer version 1.12.0; class source<T, uint64 N> { public array[N] T values; } class value { public uint32 x; public constructor<Input: integral>(source<Input, 3> a) { x = 1; } public constructor<Value: integral>(source<Value, 1 + 2> b) { x = 2; } }",
      "serializer version 1.12.0; class value { public uint32 x; public constructor(digest(md5) a) { x = 1; } public constructor(digest[16] b) { x = 2; } }",
      "serializer version 1.12.0; class value { public uint32 x; public constructor(digest[16] a) { x = 1; } public constructor(array[16] uint8 b) { x = 2; } }",
      "serializer version 1.12.0; class value { public uint32 x; public constructor(digest a) { x = 1; } public constructor(array<uint8> b) { x = 2; } }",
      "serializer version 1.12.0; class value<T: integral && !bool> { public T x; public constructor(value<T> other) { x = other.x; } }",
      "serializer version 1.11.0; class source<T> { public T x; } class value { public uint32 x; public constructor<Input: integral && !bool>(source<Input> other) { x = checked<uint32>(other.x); } }",
      "serializer version 1.12.0; class source<T: floating_point> { public T x; } class value { public uint32 x; public constructor(source<uint32> other) { x = checked<uint32>(other.x); } }",
  };
  for (const auto source : rejected) { EXPECT_THROW(parse_overloaded_construction(source), std::exception); }
}

// New native and numeric-policy syntax preserves previously valid user-defined type spellings.
TEST(overloaded_construction, preserves_legacy_keyword_named_fields) {
  for (const auto source : {
      "serializer version 1.8.0; class cpp {} class value { public cpp storage; }",
      "serializer version 1.8.0; class cpp {} class value { public cpp storage (3); }",
      "serializer version 1.11.0; class common_type<T, U> { public T first; public U second; } class value { public common_type<uint32, uint64> stored; }"}) {
    EXPECT_NO_THROW(parse_overloaded_construction(source));
  }
  for (const auto source : {
      "serializer version 1.12.0; class value { public common_type<uint32, double> stored; }",
      "serializer version 1.12.0; class value<T: integral> { public common_type<T, double> stored; }",
      "serializer version 1.12.0; class value { public array<common_type<uint32, double>> stored; }"}) {
    EXPECT_THROW(parse_overloaded_construction(source), std::exception);
  }
}

// Backends without the native overload contract fail before emitting a partially supported API.
TEST(overloaded_construction, diagnoses_other_backends) {
  const auto schema = parse_overloaded_construction(
      "serializer version 1.12.0; class value { public uint32 x; public constructor(uint32 a) { x = a; } public constructor(uint32 a, uint32 b) { x = b; } }");
  namespace writer = rohit::serializer::writer;
  writer::cpp_options options;
  options.format = false;
  EXPECT_NO_THROW(writer::cpp::generate(schema, options));
  EXPECT_THROW(writer::java::generate(schema, "Schema", {}), std::invalid_argument);
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_THROW(writer::portable::generate(schema, language, "Schema", {}), std::invalid_argument);
  }
}
