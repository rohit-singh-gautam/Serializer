// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "quantities.hpp"

#include <cstdint>
#include <iostream>
#include <type_traits>

// Qualify distinct unit identities and constexpr forwarding before running the sample.
constexpr bool quantities_are_distinct() {
  const quantities::distance<std::int64_t> first{12}, second{7};
  return (first + second).value == 19 && (-first).value == -12 &&
      (-quantities::duration<std::int64_t>{3}).value == -3;
}
static_assert(quantities_are_distinct());
static_assert(!std::is_convertible_v<quantities::distance<std::int64_t>,
                                   quantities::duration<std::int64_t>>);

// Show explicitly declared operations without implicit unit conversions or extra model storage.
int main() {
  const quantities::distance<std::int64_t> first{12}, second{7};
  std::cout << "distance sum: " << (first + second).value << '\n'
            << "negated distance: " << (-first).value << '\n';
}
