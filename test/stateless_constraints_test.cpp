// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>

#include "stateless_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace {
namespace schema = rohit::serializer;

// Parse full documents so language gates and every generic expansion are validated.
auto parse_stateless(std::string_view source) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  return schema::parser::parse(input);
}

// Preserve transient field APIs and durable bytes while allowing native empty overlap.
TEST(stateless_constraints, owning_templates_and_wire_equivalence) {
  static_assert(std::is_empty_v<passive_layout::empty_token>);
  static_assert(sizeof(passive_layout::counter<>) == sizeof(std::uint32_t));
  static_assert(sizeof(passive_layout::compact_counter) == sizeof(std::uint32_t));
  static_assert(sizeof(passive_layout::active_counter) > sizeof(passive_layout::compact_counter));
  passive_layout::compact_counter value{};
  value.count = 73;
  passive_layout::active_counter active{};
  active.count = value.count;
  active.reference.handle = 91;
  const auto verify = [&]<template <schema::serialize_type> class Protocol>() {
    rohit::full_stream_auto_alloc compact_bytes{};
    rohit::full_stream_auto_alloc active_bytes{};
    value.template serialize_out<Protocol>(compact_bytes);
    active.template serialize_out<Protocol>(active_bytes);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(compact_bytes.begin()), compact_bytes.current_offset()),
        std::string_view(reinterpret_cast<const char*>(active_bytes.begin()), active_bytes.current_offset()));
    const auto input = rohit::make_constant_stream(compact_bytes.begin(), compact_bytes.current_offset());
    const auto recovered = schema::deserialize_exact<passive_layout::counter<>, Protocol>(input);
    EXPECT_EQ(recovered.count, value.count);
    const auto active_input = rohit::make_constant_stream(active_bytes.begin(), active_bytes.current_offset());
    const auto active_recovered = schema::deserialize_exact<passive_layout::active_counter, Protocol>(active_input);
    EXPECT_EQ(active_recovered.reference.handle, 0U);
  };
  verify.template operator()<schema::json>();
  verify.template operator()<schema::binary_none>();
  verify.template operator()<schema::binary_integer>();
  verify.template operator()<schema::binary_string>();
  passive_layout::routed_counter routed{};
  routed.value.count = 29;
  EXPECT_EQ(routed.value.count, 29U);
}

// Reject invalid concrete/default/forwarded proofs before generating any backend output.
TEST(stateless_constraints, checked_schema_contract) {
  constexpr std::string_view prelude = "serializer version 1.9.0; class empty {} "
      "class stateful { public uint32 value; } class passive<T: stateless = empty> { "
      "public transient layout(empty) T reference; } ";
  for (const auto use : {
      "instantiate bad = passive<stateful>;",
      "class bad { public passive<uint32> value; }",
      "class outer<T> { public passive<T> value; }",
      "class outer<T: stateless = stateful> {}",
      "class outer<T: stateless = uint32> {}",
      "class outer<T, U: stateless = T> {}",
      "class parent {} class child : public parent {} instantiate bad = passive<child>;",
      "class managed_token managed {} instantiate bad = passive<managed_token>;",
      "class hooked { public cpp { int hidden; } } instantiate bad = passive<hooked>;",
      "class virtual_token { public function virtual bool usable() readonly expression(true); } instantiate bad = passive<virtual_token>;",
      "class legacy<T> { public transient layout(empty) T reference; } instantiate bad = legacy<stateful>;",
      "class tag<T> {} class outer<U> { public passive<tag<passive<U>>> value; }"}) {
    SCOPED_TRACE(use);
    EXPECT_THROW(parse_stateless(std::string{prelude} + use), std::exception);
  }
  for (const auto text : {
      "serializer version 1.8.0; class bad<T: stateless> {}",
      "serializer version 1.9.0; class bad<uint64 N: stateless> {}",
      "serializer version 1.9.0; class bad<T: unknown> {}"}) {
    SCOPED_TRACE(text);
    EXPECT_THROW(parse_stateless(text), std::exception);
  }
  EXPECT_NO_THROW(parse_stateless(std::string{prelude} +
      "class outer<T: stateless, U: stateless = T> { public passive<U> value; } instantiate accepted = outer<empty>;"));
  EXPECT_NO_THROW(parse_stateless(std::string{prelude} +
      "class tag<T> {} instantiate accepted = passive<tag<stateful>>;"));
  EXPECT_NO_THROW(parse_stateless(std::string{prelude} +
      "class tag<T> {} class outer<U> { public passive<tag<U>> value; } instantiate accepted = outer<stateful>;"));
  EXPECT_NO_THROW(parse_stateless(std::string{prelude} +
      "class metadata { public wire_only uint32 size(1) auto_size(binary_none) omit(all_except(binary_none)); } "
      "instantiate accepted = passive<metadata>;"));
  for (const auto version : {"1.8.0", "1.9.0"}) {
    EXPECT_NO_THROW(parse_stateless(std::string{"serializer version "} + version +
        "; class empty {} class legacy<T> { public transient layout(empty) T reference; } "
        "instantiate accepted = legacy<empty>;"));
  }
}

} // namespace
