// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "scalar_defaults.hpp"

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

// Instantiate checked generated defaults and preserve their values through every native wire format.
TEST(scalar_default, generated_native_values_and_wire_round_trips) {
  namespace codec = rohit::serializer;
  constexpr scalar_default_record source{};
  static_assert(source.single == std::numeric_limits<float>::max());
  static_assert(source.extended == std::numeric_limits<double>::max());
  static_assert(source.selection == scalar_default_kind::second);
  static_assert(source.enabled);
  EXPECT_TRUE(std::signbit(source.negative_zero));
  const auto round_trip = [&]<template <codec::serialize_type> class Protocol>() {
    rohit::full_stream_auto_alloc output{};
    source.serialize_out<Protocol>(output);
    const auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
    const auto decoded = codec::deserialize_exact<scalar_default_record, Protocol>(input);
    EXPECT_EQ(decoded.single, source.single);
    EXPECT_EQ(decoded.extended, source.extended);
    EXPECT_TRUE(std::signbit(decoded.negative_zero));
    EXPECT_EQ(decoded.selection, source.selection);
    EXPECT_EQ(decoded.enabled, source.enabled);
  };
  round_trip.template operator()<codec::binary_none>();
  round_trip.template operator()<codec::binary_integer>();
  round_trip.template operator()<codec::binary_string>();
  round_trip.template operator()<codec::json>();
}
