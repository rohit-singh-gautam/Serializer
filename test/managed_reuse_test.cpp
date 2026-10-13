// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <managed_reuse_first.hpp>
#include <managed_reuse_second.hpp>
#include <rohit/managed.hpp>

#include <cstdint>
#include <type_traits>

// Shared generated headers reuse managed editors and traits without duplicate definitions.
TEST(managed_reuse, shared_companions_and_values) {
  using namespace reusable_managed;
  using traits = rohit::managed::model_traits<shared_state>;
  static_assert(std::is_same_v<typename traits::storage_type, shared_state>);
  first_contract first{};
  first.state.value = 17;
  // Shared managed definitions must remain interchangeable through every native protocol.
  const auto round_trip = [&]<template <rohit::serializer::serialize_type> class Protocol>() {
    rohit::full_stream_auto_alloc output{};
    first.serialize_out<Protocol>(output);
    const auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
    const auto second = rohit::serializer::deserialize_exact<second_contract, Protocol>(input);
    EXPECT_EQ(second.state.value, std::uint32_t{17});
  };
  round_trip.template operator()<rohit::serializer::binary_none>();
  round_trip.template operator()<rohit::serializer::binary_integer>();
  round_trip.template operator()<rohit::serializer::binary_string>();
  round_trip.template operator()<rohit::serializer::json>();
}
