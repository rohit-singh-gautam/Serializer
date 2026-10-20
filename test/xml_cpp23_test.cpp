// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

// Instantiate XML floating parsing under C++23 without deprecated numeric_limits members.
template <typename Float> void check_floating_range() {
  Float value{1};
  rohit::serializer::xml::detail::read_scalar("-1e-99999", value);
  EXPECT_EQ(value, Float{});
  EXPECT_TRUE(std::signbit(value));
  rohit::serializer::xml::detail::read_scalar("1e-99999", value);
  EXPECT_EQ(value, Float{});
  EXPECT_FALSE(std::signbit(value));
  EXPECT_THROW(rohit::serializer::xml::detail::read_scalar("1e99999", value), std::invalid_argument);
  EXPECT_THROW(rohit::serializer::xml::detail::read_scalar("-1e99999", value), std::invalid_argument);
  constexpr auto minimum_positive = std::numeric_limits<Float>::denorm_min();
  rohit::serializer::xml::detail::read_scalar(
      rohit::serializer::xml::detail::scalar_text(minimum_positive), value);
  EXPECT_EQ(value, minimum_positive);
}

// Cover each standard floating type with warnings treated as errors by this target.
TEST(xml_cpp23, floating_underflow_and_overflow) {
  check_floating_range<float>();
  check_floating_range<double>();
  check_floating_range<long double>();
}

} // namespace
