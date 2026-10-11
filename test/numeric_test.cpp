// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/numeric.hpp>
#include <rohit/serializer_creator.hpp>
#include "numeric_values.hpp"
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;
constexpr auto signed_minimum = std::numeric_limits<std::int64_t>::min();
constexpr auto signed_maximum = std::numeric_limits<std::int64_t>::max();
constexpr auto unsigned_maximum = std::numeric_limits<std::uint64_t>::max();

// Qualify valid values and nonthrowing invalid results during C++20 constant evaluation.
constexpr bool qualify_numeric_values() {
  numeric_values::quantity<std::int64_t> value{};
  value.value = 6;
  return value.added(2) == 8 && value.half() == 3 && value.bound(42) == 42 &&
      codec::checked_numeric_cast<double>(signed_minimum) == -9223372036854775808.0 &&
      !codec::try_numeric_cast<double>(unsigned_maximum) &&
      !codec::try_checked_add(signed_maximum, std::int64_t{1}) &&
      !codec::try_checked_divide(std::int64_t{3}, std::int64_t{2}) &&
      !numeric_values::scalar{}.exact_half(3) && numeric_values::scalar{}.exact_half(4) &&
      numeric_values::scalar{}.float_equal(static_cast<float>(16777217)) &&
      numeric_values::scalar{}.float_nonfinite(std::numeric_limits<float>::infinity());
}
static_assert(qualify_numeric_values());

// Exercise both 64-bit endpoints without constructing an overflowing intermediate.
TEST(numeric, exact_full_width_bindings) {
  EXPECT_EQ(codec::checked_numeric_cast<std::int64_t>(signed_minimum), signed_minimum);
  EXPECT_EQ(codec::checked_numeric_cast<std::uint64_t>(unsigned_maximum), unsigned_maximum);
  EXPECT_THROW((void)codec::checked_numeric_cast<std::uint64_t>(std::int64_t{-1}), std::domain_error);
  EXPECT_THROW((void)codec::checked_numeric_cast<std::int64_t>(unsigned_maximum), std::domain_error);
  EXPECT_EQ(codec::checked_numeric_cast<double>(signed_minimum), -9223372036854775808.0);
  EXPECT_EQ(codec::checked_numeric_cast<double>(std::uint64_t{1} << 63), 9223372036854775808.0);
  EXPECT_THROW((void)codec::checked_numeric_cast<double>(signed_maximum), std::domain_error);
  EXPECT_THROW((void)codec::checked_numeric_cast<double>(unsigned_maximum), std::domain_error);
  EXPECT_EQ(codec::checked_numeric_cast<float>(16777216), 16777216.0f);
  EXPECT_THROW((void)codec::checked_numeric_cast<float>(16777217), std::domain_error);
  EXPECT_EQ(codec::checked_numeric_cast<std::uint64_t>(std::nextafter(18446744073709551616.0, 0.0)),
      unsigned_maximum - 2047);
  EXPECT_THROW((void)codec::checked_numeric_cast<std::uint64_t>(18446744073709551616.0), std::domain_error);
  EXPECT_THROW((void)codec::checked_numeric_cast<std::int64_t>(9223372036854775808.0), std::domain_error);
  EXPECT_EQ(codec::checked_numeric_cast<std::int64_t>(-9223372036854775808.0), signed_minimum);
  EXPECT_THROW((void)codec::checked_numeric_cast<std::int64_t>(1.5), std::domain_error);
  EXPECT_THROW((void)codec::checked_numeric_cast<float>(0.1), std::domain_error);
  EXPECT_EQ(codec::checked_numeric_cast<float>(0.5), 0.5f);
  EXPECT_TRUE(std::signbit(codec::checked_numeric_cast<float>(-0.0)));
  EXPECT_THROW((void)codec::checked_numeric_cast<double>(std::numeric_limits<double>::infinity()), std::domain_error);
  EXPECT_THROW((void)codec::checked_numeric_cast<std::int64_t>(std::numeric_limits<double>::quiet_NaN()), std::domain_error);
}

// Compare every small signed pair against a wider oracle, including exact division and overflow.
TEST(numeric, exhaustive_signed_byte_arithmetic) {
  constexpr auto minimum = std::numeric_limits<std::int8_t>::min();
  constexpr auto maximum = std::numeric_limits<std::int8_t>::max();
  for (int left = minimum; left <= maximum; ++left) {
    for (int right = minimum; right <= maximum; ++right) {
      const auto a = static_cast<std::int8_t>(left), b = static_cast<std::int8_t>(right);
      const auto check = [&](const auto& result, int expected) {
        ASSERT_EQ(result.has_value(), expected >= minimum && expected <= maximum);
        if (result) { EXPECT_EQ(*result, expected); }
      };
      check(codec::try_checked_add(a, b), left + right);
      check(codec::try_checked_subtract(a, b), left - right);
      check(codec::try_checked_multiply(a, b), left * right);
      const auto division = codec::try_checked_divide(a, b);
      const bool valid = right != 0 && left % right == 0 && left / right >= minimum && left / right <= maximum;
      ASSERT_EQ(division.has_value(), valid);
      if (division) { EXPECT_EQ(*division, left / right); }
    }
  }
}

// Generated expressions preserve their representation and predicate failures at every boundary.
TEST(numeric, generated_checked_expressions) {
  const numeric_values::scalar value{};
  EXPECT_EQ(value.add(signed_maximum, 0), signed_maximum);
  EXPECT_THROW((void)value.add(signed_maximum, 1), std::domain_error);
  EXPECT_THROW((void)value.subtract(signed_minimum, 1), std::domain_error);
  EXPECT_THROW((void)value.multiply(signed_minimum, -1), std::domain_error);
  EXPECT_EQ(value.multiply(signed_minimum, 1), signed_minimum);
  EXPECT_THROW((void)value.divide(signed_minimum, -1), std::domain_error);
  EXPECT_THROW((void)value.divide(3, 2), std::domain_error);
  EXPECT_THROW((void)value.divide(0, 0), std::domain_error);
  EXPECT_THROW((void)value.negate(signed_minimum), std::domain_error);
  EXPECT_THROW((void)value.unsigned_add(unsigned_maximum, 1), std::domain_error);
  EXPECT_THROW((void)value.unsigned_subtract(0, 1), std::domain_error);
  EXPECT_THROW((void)value.unsigned_multiply(unsigned_maximum, 2), std::domain_error);
  EXPECT_THROW((void)value.inexact_float_literal(0.0f), std::domain_error);
  EXPECT_THROW((void)value.inexact_double_literal(0.0), std::domain_error);
  EXPECT_FALSE(value.exact_half(3));
  EXPECT_FALSE(value.exact_unsigned(-1.0));
  EXPECT_FALSE(value.exact_unsigned(18446744073709551616.0));
  EXPECT_TRUE(value.exact_unsigned(42.0));
  EXPECT_TRUE(value.float_equal(static_cast<float>(16777217)));
  EXPECT_TRUE(value.float_nonfinite(std::numeric_limits<float>::infinity()));
  EXPECT_TRUE(value.float_nonfinite(std::numeric_limits<float>::quiet_NaN()));
}

// Parse schema contracts through the normal frontend, including older and mixed representation rejection.
TEST(numeric, schema_contract_and_explicit_representation) {
  const auto parse = [](std::string_view version, std::string_view body) {
    const auto source = "serializer version " + std::string{version} + "; class sample { " + std::string{body} + " }";
    auto input = rohit::make_constant_full_stream(source.data(), source.size());
    return codec::parser::parse(input);
  };
  EXPECT_NO_THROW(parse("1.11.0", "public function int64 add(int64 left, int64 right) readonly expression(left + right);"));
  EXPECT_THROW(parse("1.10.0", "public function int64 add(int64 left, int64 right) readonly expression(left + right);"), std::exception);
  EXPECT_THROW(parse("1.10.0", "public function double value(uint64 input) readonly expression(checked<double>(input));"), std::exception);
  EXPECT_THROW(parse("1.11.0", "public function int64 add(int64 left, uint64 right) readonly expression(left + right);"), std::exception);
  EXPECT_NO_THROW(parse("1.11.0", "public function int64 add(int64 left, uint64 right) readonly expression(left + checked<int64>(right));"));
  EXPECT_THROW(parse("1.11.0", "public function int64 value(bool input) readonly expression(checked<int64>(input));"), std::exception);
  EXPECT_THROW(parse("1.11.0", "public function int64 value(int64 input) readonly expression(checked<bool>(input));"), std::exception);
}
} // namespace
