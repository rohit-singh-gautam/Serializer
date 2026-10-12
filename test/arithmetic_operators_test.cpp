// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "arithmetic_operators.hpp"

#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>

#include <concepts>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
namespace units = arithmetic_test;

// Test operator availability without instantiating an invalid expression in the test itself.
template <typename Left, typename Right>
concept addable = requires(Left left, Right right) { left + right; };

// Evaluate checked methods and operator forwarding through real generated owning constructors.
constexpr bool qualify_arithmetic() {
  const units::length<std::int64_t> first{12}, second{7};
  const auto negated = -first;
  const auto added = first + second;
  const auto scaled = first * std::int64_t{3};
  const auto divided = first / std::int64_t{3};
  const auto duration = -units::duration<std::int64_t>{9};
  return negated.value == -12 && added.value == 19 && scaled.value == 36 && divided.value == 4 &&
      duration.value == -9 && first.value == 12 && first.negated().value == negated.value;
}
static_assert(qualify_arithmetic());
static_assert(std::same_as<decltype(-units::length<std::int64_t>{1}), units::length<std::int64_t>>);
static_assert(std::same_as<decltype(-units::scalar_contract{}), std::int64_t>);
static_assert(!addable<units::length<std::int64_t>, units::duration<std::int64_t>>);
static_assert(!std::is_convertible_v<units::length<std::int64_t>, units::duration<std::int64_t>>);
static_assert(!noexcept(-std::declval<const units::length<std::int64_t>&>()));
static_assert(+units::native_scalar{} == 19);
static_assert(noexcept(+std::declval<const units::native_scalar&>()));

// Constructor calls preserve supplied coordinates and the generated trailing/default values.
constexpr bool qualify_constructor_calls() {
  const units::constructor_calls input{};
  const auto supplied = input.built(), defaults = input.defaults();
  return supplied.value == 12 && supplied.extra == 17 && defaults.value == 0 && defaults.extra == 17;
}
static_assert(qualify_constructor_calls());
static_assert(units::overload_calls{}.built().value == 1000);

// Operators use the shared checked body and retain its owning result rather than returning a scalar.
TEST(arithmetic_operators, constant_contracts_and_aliases) {
  EXPECT_TRUE(qualify_arithmetic());
  EXPECT_TRUE(qualify_constructor_calls());
  const units::length<std::int64_t> first{23};
  EXPECT_EQ((-first).value, -23);
  EXPECT_EQ((first + units::length<std::int64_t>{17}).value, 40);
}

// Rejected integer/floating operations propagate the existing checked exception categories.
TEST(arithmetic_operators, shared_checked_rejections) {
  const units::length<std::int64_t> maximum{std::numeric_limits<std::int64_t>::max()};
  const units::length<std::int64_t> minimum{std::numeric_limits<std::int64_t>::min()};
  EXPECT_THROW(static_cast<void>(maximum + units::length<std::int64_t>{1}), std::domain_error);
  EXPECT_THROW(static_cast<void>(-minimum), std::domain_error);
  EXPECT_THROW(static_cast<void>(units::length<std::int64_t>{3} / std::int64_t{2}), std::domain_error);
  EXPECT_THROW(static_cast<void>(minimum / std::int64_t{-1}), std::domain_error);
  EXPECT_THROW(static_cast<void>(units::length<std::int64_t>{3} / std::int64_t{0}), std::domain_error);
  EXPECT_THROW(static_cast<void>(-units::unsigned_length{1}), std::domain_error);
  EXPECT_THROW(static_cast<void>(-units::floating_length{std::numeric_limits<double>::infinity()}),
               std::domain_error);
}

// Parsing an invalid explicit mapping fails without adding field/wire metadata.
TEST(arithmetic_operators, diagnose_contract_errors) {
  constexpr std::string_view invalid_bodies[]{
      "class value { public int32 number; arithmetic(unary(-) -> missing); }",
      "class value { public function int32 negate(int32 input) readonly expression(input); arithmetic(unary(-) -> negate); }",
      "class value { public function int32 negate() edit cpp { return 0; }; arithmetic(unary(-) -> negate); }",
      "class value { public function void negate() readonly; arithmetic(unary(-) -> negate); }",
      "class value { public function int32 negate() readonly expression(0); arithmetic(unary(-) -> negate); arithmetic(unary(-) -> negate); }",
      "class value { public function int32 add(int32 input) readonly expression(input); public function int64 add(int64 input) readonly expression(input); arithmetic(binary(+) -> add); }",
      "class value { public int32 number; arithmetic(binary(+) -> expression(value(number))); }",
      "class value { public int32 number; arithmetic(unary(-) -> expression(-number)); }",
      "class value { public int32 number; public constructor(int32 input) { number = input; } public function int64 bad() readonly expression(value(number)); }",
      "class value { public int32 number; public constructor(int32 input) { number = build(input); } public function int32 build(int32 input) readonly expression(value(input).number); }",
      "class value view readonly { public int32 number; public function int32 negate() readonly expression(-number); arithmetic(unary(-) -> negate); }"};
  for (const auto body : invalid_bodies) {
    const auto source = "serializer version 1.14.0; " + std::string{body};
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_ANY_THROW(rohit::serializer::parser::parse(input, true)) << body;
  }
  const std::string legacy{"serializer version 1.13.0; class value { public int32 number; public function int32 negate() readonly expression(-number); arithmetic(unary(-) -> negate); }"};
  const auto input = rohit::make_constant_stream(legacy.data(), legacy.size());
  EXPECT_ANY_THROW(rohit::serializer::parser::parse(input, true));
}

// Older language contracts keep ordinary calls even when a record has the same name.
TEST(arithmetic_operators, legacy_calls_keep_method_binding_when_a_record_shares_the_name) {
  const std::string source{
      "serializer version 1.13.0; class result { public int32 number (1); } "
      "class request { public function int32 result() readonly expression(12); "
      "public function int32 value() readonly expression(result()); }"};
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input, true);
  const auto& owner = static_cast<const rohit::serializer::class_node&>(*statements.back());
  ASSERT_EQ(owner.functions.size(), 2U);
  ASSERT_TRUE(owner.functions.back().expression);
  EXPECT_FALSE(owner.functions.back().expression->call_constructor);
  EXPECT_EQ(owner.functions.back().expression->call_target, &owner);
}

// Inline private helper names avoid later authored fields and do not consume serialized identities.
TEST(arithmetic_operators, helpers_preserve_wire_fields_and_avoid_authored_names) {
  const std::string source{
      "serializer version 1.14.0; class projection { public int32 number (1); } "
      "class value arithmetic(unary(-) -> expression(value(-number))) { "
      "public int32 serializer_arithmetic_0 (1); "
      "private int32 number (2) expose readonly(public) as serializer_arithmetic_0_1; "
      "public constructor(int32 input) { number = input; } "
      "public conversion projection as serializer_arithmetic_0_2 { number = serializer_arithmetic_0; }; }"};
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input, true);
  const auto& owner = static_cast<const rohit::serializer::class_node&>(*statements.back());
  ASSERT_EQ(owner.member_list.size(), 2U);
  EXPECT_EQ(owner.member_list[0].id, 1U);
  EXPECT_EQ(owner.member_list[1].id, 2U);
  ASSERT_EQ(owner.arithmetic.size(), 1U);
  EXPECT_NE(owner.functions[owner.arithmetic.front().method_index].name, "serializer_arithmetic_0");
  EXPECT_NE(owner.functions[owner.arithmetic.front().method_index].name, "serializer_arithmetic_0_1");
  EXPECT_NE(owner.functions[owner.arithmetic.front().method_index].name, "serializer_arithmetic_0_2");
}
} // namespace
