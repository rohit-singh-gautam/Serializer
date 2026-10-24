// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>
#include "generic_calls.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace {
// Parse complete schemas to exercise language gates and overload binding before emission.
auto parse_calls(std::string_view text) {
  auto input = rohit::make_constant_stream(text.data(), text.size());
  return rohit::serializer::parser::parse(input);
}

// Generated calls preserve full-width values and native independent input defaults.
TEST(generic_calls, generated_exact_calls_and_defaults) {
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  static_assert(generic_calls::exact(maximum) == maximum);
  static_assert(generic_calls::matches(maximum, maximum));
  static_assert(generic_calls::added(1.25, 2) == 3.25);
  constexpr generic_calls::value value{maximum};
  static_assert(value.read() == maximum);
  const auto zero = generic_calls::value::create<std::uint64_t>();
  EXPECT_EQ(zero.number, 0u);
  EXPECT_EQ(generic_calls::value::seven<std::uint64_t>().number, 7u);
  EXPECT_EQ(generic_calls::value::from_factory<generic_calls::token>().number, 7u);
  EXPECT_FALSE(generic_calls::matches(maximum, maximum - 1));
}

// Reject inconsistent or unconstrained inputs and retain the older call contract.
TEST(generic_calls, rejects_invalid_deduction_and_old_language) {
  const std::string declaration =
      "function T choose<T: integral && !bool>(T left, T right) readonly expression(left); ";
  EXPECT_NO_THROW(parse_calls("serializer version 1.24.0; " + declaration +
      "function uint64 probe(uint64 a) readonly expression(choose(a,a));"));
  for (const auto source : {
      "function uint64 probe(uint64 a, uint32 b) readonly expression(choose(a,b));",
      "function bool probe(bool a) readonly expression(choose(a,a));",
      "function uint64 probe() readonly expression(choose(1,2));"}) {
    EXPECT_THROW(parse_calls("serializer version 1.24.0; " + declaration + source), std::exception);
  }
  EXPECT_THROW(parse_calls("serializer version 1.23.0; " + declaration +
      "function uint64 probe(uint64 a) readonly expression(choose(a,a));"), std::exception);
}
} // namespace
