// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <reciprocal_constructors.hpp>

#include <rohit/serializer_creator.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>

// Supply visible host definitions matching the generated constexpr declarations.
template <typename T>
constexpr radius_first::radius<T>::radius(diameter<T> extent) : value{extent.value / T{2}} {}

// Preserve the reciprocal unit identity through an explicit conversion constructor.
template <typename T>
constexpr radius_first::diameter<T>::diameter(radius<T> extent) : value{extent.value * T{2}} {}

// Define the promised constant observer after both generated types are complete.
template <typename T>
constexpr T radius_first::radius<T>::as_scalar() const { return value; }

// Declaration order does not alter reciprocal constructor type resolution.
template <typename T>
constexpr diameter_first::diameter<T>::diameter(radius<T> extent) : value{extent.value * T{2}} {}

// Keep the opposite declaration order constant-evaluable as well.
template <typename T>
constexpr diameter_first::radius<T>::radius(diameter<T> extent) : value{extent.value / T{2}} {}

// Named instantiations keep distinct owning identities and their exact open counterpart signatures.
constexpr radius_first::radius64::radius64(diameter<double> extent) : value{extent.value / 2.0} {}

// Supply the named instance's separate observer promised by its generated behavior adapter.
constexpr double radius_first::radius64::as_scalar() const { return value; }

// Complete every named reciprocal declaration rather than relying on the open template definition.
constexpr radius_first::diameter64::diameter64(radius<double> extent) : value{extent.value * 2.0} {}

// Preserve named conversion definitions when diameter is declared first.
constexpr diameter_first::diameter64::diameter64(radius<double> extent) : value{extent.value * 2.0} {}

// Complete the opposite named constructor with the generated open diameter parameter.
constexpr diameter_first::radius64::radius64(diameter<double> extent) : value{extent.value / 2.0} {}

// An external default declaration requires a host definition instead of another generated default.
external_default::external_default() = default;

static_assert(radius_first::radius<double>{radius_first::diameter<double>{8.0}}.as_scalar() == 4.0);
static_assert(radius_first::diameter<double>{radius_first::radius<double>{4.0}}.value == 8.0);
static_assert(diameter_first::radius<double>{diameter_first::diameter<double>{8.0}}.value == 4.0);
static_assert(diameter_first::diameter<double>{diameter_first::radius<double>{4.0}}.value == 8.0);
static_assert(radius_first::radius64{radius_first::diameter<double>{8.0}}.as_scalar() == 4.0);
static_assert(radius_first::diameter64{radius_first::radius<double>{4.0}}.value == 8.0);
static_assert(diameter_first::radius64{diameter_first::diameter<double>{8.0}}.value == 4.0);
static_assert(diameter_first::diameter64{diameter_first::radius<double>{4.0}}.value == 8.0);
static_assert(!std::is_same_v<radius_first::radius64, radius_first::radius<double>>);
static_assert(radius_first::radius<double>{}.native_scalar() == 0.0);
static_assert(explicit_default{}.value == 7);
static_assert(inline_constructor{11}.value == 11);
static_assert(std::is_default_constructible_v<radius_first::radius64>);

namespace {
// Ordinary mutation of a local owning value remains constant-capable, including void results.
constexpr bool constant_edit_contract() {
  constant_edit value{};
  value.set_value(6);
  return value.value == 6 && value.replace_value(9) == 9 && value.value == 9;
}
static_assert(constant_edit_contract());

// Parse through the public compiler API, retaining each declaring file's language contract.
auto parse(std::string_view source) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}
} // namespace

// Constant and runtime conversions share the exact same typed owning return contract.
TEST(reciprocal_constructors, declaration_order_default_and_wire_identity) {
  radius_first::radius<double> value{radius_first::diameter<double>{10.0}};
  EXPECT_EQ(value.value, 5.0);
  EXPECT_EQ(value.native_scalar(), 5.0);
  EXPECT_EQ(external_default{}.value, 7);
  rohit::full_stream_auto_alloc encoded;
  value.serialize_out<rohit::serializer::binary_integer>(encoded);
  auto input = rohit::make_constant_full_stream(encoded.begin(), encoded.current_offset());
  const auto decoded = rohit::serializer::deserialize_exact<radius_first::radius<double>,
      rohit::serializer::binary_integer>(input);
  EXPECT_EQ(decoded.value, value.value);
}

// Callable forwards cannot weaken the finite-storage rule or older schema contracts.
TEST(reciprocal_constructors, reject_storage_cycles_and_invalid_prefixes) {
  EXPECT_ANY_THROW(parse("serializer version 1.14.0; class first { public second value; } "
      "class second { public first value; }"));
  EXPECT_ANY_THROW(parse("serializer version 1.13.0; class first { "
      "public cpp { constexpr } constructor(int32 input); }"));
  EXPECT_ANY_THROW(parse("serializer version 1.14.0; class first { "
      "public cpp { constexpr constexpr } function int32 value() readonly; }"));
  EXPECT_ANY_THROW(parse("serializer version 1.14.0; class first { "
      "public cpp { volatile } function int32 value() readonly; }"));
  EXPECT_ANY_THROW(parse("serializer version 1.14.0; class first { "
      "public constructor(int32 input) = default; }"));
}

// Generated restrictions identify the selected operation and preserve the prior native contract.
TEST(reciprocal_constructors, constant_evaluation_reasons_and_legacy_native_contract) {
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  options.rename_identifiers = false;
  const auto math = parse("serializer version 1.14.0; class value { public double number; "
      "public function double root() readonly expression(math.sqrt(number)); }");
  const auto generated = rohit::serializer::writer::cpp::generate(math, options);
  EXPECT_NE(generated.find("square root is a runtime-only C++20 math operation"), std::string::npos);
  EXPECT_EQ(generated.find("dispatched/edit behavior, runtime storage"), std::string::npos);
  const auto legacy = parse("serializer version 1.13.0; class value { public int32 number; "
      "public function void update(int32 input) edit cpp { number = input; }; }");
  const auto legacy_generated = rohit::serializer::writer::cpp::generate(legacy, options);
  EXPECT_EQ(legacy_generated.find("constexpr void update("), std::string::npos);
}
