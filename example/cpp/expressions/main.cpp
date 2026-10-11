// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
// Create an owning value whose pure generated behavior can run during C++20 constant evaluation.
[[nodiscard]] constexpr expressions::metrics sample() noexcept {
  expressions::metrics value{};
  value.identity = std::numeric_limits<std::uint64_t>::max();
  value.readings = {2.0, 4.0, 6.0};
  return value;
}

constexpr auto constant_value = sample();
static_assert(expression_math::total(12.5, 4) == 50.0);
static_assert(constant_value.subtotal() == 50.0);
static_assert(constant_value.adjusted(5.0, 2.0) == 47.0);
static_assert(constant_value.unit_ratio(2.5) == 5.0);
static_assert(constant_value.matches(std::numeric_limits<std::uint64_t>::max()));
static_assert(constant_value.within_budget(50.0));
static_assert(!constant_value.within_budget(49.0));
static_assert(constant_value.ratio_within(2.5, 5.0));
static_assert(!constant_value.ratio_within(0.0, 5.0));
static_assert(constant_value.all_nonnegative());
static_assert(constant_value.reading_matches(2, 6.0));
static_assert(!constant_value.reading_matches(3, 6.0));
static_assert(noexcept(std::declval<const expressions::metrics&>().matches(0)));
static_assert(noexcept(std::declval<const expressions::metrics&>().ratio_within(0.0, 1.0)));
static_assert(!noexcept(std::declval<const expressions::metrics&>().unit_ratio(0.0)));
} // namespace

// Exercise the same generated arithmetic at runtime and distinguish checked expressions from predicates.
int main() {
  auto value = sample();
  value.unit_price = 18.0;
  value.count = 3;
  if (value.subtotal() != 54.0 || value.adjusted(4.0, 2.0) != 52.0 ||
      value.unit_ratio(3.0) != 6.0 || !value.within_budget(54.0) ||
      value.within_budget(53.0) || !value.reading_matches(2, 6.0) ||
      value.reading_matches(3, 6.0)) {
    return 1;
  }

  bool rejected_zero{};
  try {
    static_cast<void>(value.unit_ratio(0.0));
  } catch (const std::domain_error&) {
    rejected_zero = true;
  }
  bool rejected_nonfinite{};
  try {
    static_cast<void>(value.adjusted(std::numeric_limits<double>::infinity(), 2.0));
  } catch (const std::domain_error&) {
    rejected_nonfinite = true;
  }
  if (!rejected_zero || !rejected_nonfinite || value.ratio_within(0.0, 6.0) ||
      value.ratio_within(std::numeric_limits<double>::infinity(), 6.0)) {
    return 2;
  }
  value.readings[1] = -1.0;
  if (value.all_nonnegative()) { return 3; }

  std::cout << "subtotal: " << value.subtotal() << '\n'
            << "adjusted: " << value.adjusted(4.0, 2.0) << '\n'
            << "invalid expressions: rejected\n"
            << "invalid predicates: false\n";
  return 0;
}
