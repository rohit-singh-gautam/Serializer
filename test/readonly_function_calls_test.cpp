// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "readonly_function_calls.hpp"
#include "readonly_function_overloads.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
constexpr auto maximum_identity = std::numeric_limits<std::uint64_t>::max();

// Evaluate actual forward-bound namespace and member calls with exact full-width scalar returns.
[[nodiscard]] constexpr bool qualify_calls() {
  call_probe value{};
  return readonly_helpers::forwarded(41.0) == 42.0 && value.sum(2.0) == 14.0 &&
      value.composed(2.0) == 15.0 && value.forwarded() == 13.0 &&
      value.echoed(maximum_identity) == maximum_identity && value.matches(maximum_identity) &&
      !value.matches(maximum_identity - 1) && value.divided(2.0) == 6.0 &&
      value.positive(2.0) && value.guarded(false, 0.0);
}

// Keep native overload resolution consistent with the schema's exact bound parameter types.
[[nodiscard]] constexpr bool qualify_overloads() {
  overload_probe value{};
  return overloaded_helpers::identity(41.0) == 42.0 &&
      overloaded_helpers::identity(maximum_identity) == maximum_identity &&
      value.scalar() == 13.0 && value.exact() == maximum_identity;
}

static_assert(qualify_calls());
static_assert(qualify_overloads());
static_assert(!noexcept(std::declval<const call_probe&>().positive(0.0)));

// Direct checked calls preserve callee failures, including when reached through a predicate caller.
TEST(readonly_function_calls, runtime_dispatch_and_failures) {
  EXPECT_TRUE(qualify_calls());
  call_probe value{};
  EXPECT_THROW(static_cast<void>(value.divided(0.0)), std::domain_error);
  EXPECT_THROW(static_cast<void>(value.divided(-0.0)), std::domain_error);
  EXPECT_THROW(static_cast<void>(value.positive(0.0)), std::domain_error);
  EXPECT_THROW(static_cast<void>(value.divided(std::numeric_limits<double>::infinity())),
      std::domain_error);
  EXPECT_TRUE(value.guarded(false, std::numeric_limits<double>::quiet_NaN()));
}

// Selected overloads keep floating behavior separate from exact unsigned integer identity.
TEST(readonly_function_calls, namespace_overloads) {
  EXPECT_TRUE(qualify_overloads());
  overload_probe value{};
  value.amount = 3.0;
  value.token = maximum_identity - 1;
  EXPECT_EQ(value.scalar(), 4.0);
  EXPECT_EQ(value.exact(), maximum_identity - 1);
}

} // namespace
