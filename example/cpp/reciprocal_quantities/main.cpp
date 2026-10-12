// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "definitions.hpp"

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <iostream>
#include <type_traits>

namespace {
using radius = reciprocal_quantities::radius<double>;
using diameter = reciprocal_quantities::diameter<double>;
static_assert(radius{diameter{8.0}}.as_scalar() == 4.0);
static_assert(diameter{radius{4.0}}.value == 8.0);
static_assert(reciprocal_quantities::radius64{diameter{8.0}}.as_scalar() == 4.0);
static_assert(reciprocal_quantities::diameter64{radius{4.0}}.value == 8.0);
static_assert(!std::is_same_v<reciprocal_quantities::radius64, radius>);
static_assert(radius{}.native_scalar() == 0.0);
static_assert(std::is_default_constructible_v<radius>);

// Validate one exact native message using the generated default construction and scalar layout.
template <template <rohit::serializer::serialize_type, typename> typename Protocol>
bool round_trip(const radius& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<Protocol>(output);
  auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  return rohit::serializer::deserialize_exact<radius, Protocol>(input).value == value.value;
}
} // namespace

// Exercise reciprocal authoring, constant observers, and all four unchanged native codec paths.
int main() {
  const radius value{diameter{12.0}};
  if (!round_trip<rohit::serializer::json>(value) ||
      !round_trip<rohit::serializer::binary_none>(value) ||
      !round_trip<rohit::serializer::binary_integer>(value) ||
      !round_trip<rohit::serializer::binary_string>(value)) {
    return 1;
  }
  std::cout << "radius = " << value.as_scalar() << ", diameter = " << diameter{value}.value << '\n';
}
