// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <quantities.hpp>

namespace reciprocal_quantities {
// Define the declared conversion after both owning generated types are complete.
template <typename Rep>
constexpr radius<Rep>::radius(diameter<Rep> extent) : value{extent.value / Rep{2}} {}

// Keep conversion explicit in the typed constructor while preserving each quantity's identity.
template <typename Rep>
constexpr diameter<Rep>::diameter(radius<Rep> extent) : value{extent.value * Rep{2}} {}

// Match the declaration's qualifier and retain this definition where constant evaluation uses it.
template <typename Rep>
constexpr Rep radius<Rep>::as_scalar() const { return value; }

// Named instantiations own distinct contracts and accept the exact open counterpart from the schema.
constexpr radius64::radius64(diameter<double> extent) : value{extent.value / 2.0} {}

// Define the named observer as well as the open template observer required by constant consumers.
constexpr double radius64::as_scalar() const { return value; }

// Complete the second named conversion without changing either generated schema identity.
constexpr diameter64::diameter64(radius<double> extent) : value{extent.value * 2.0} {}
} // namespace reciprocal_quantities
