// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>

#include "generic_constraints.hpp"

#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>

namespace {
namespace schema = rohit::serializer;

// Ask the generated open template to enforce the same numeric contract as schema applications.
template <class T>
concept accepts_number = requires { typename constrained_generics::number<T>; };

// Check exact-type disjunctions independently of the broader arithmetic categories.
template <class T>
concept accepts_allowed = requires { typename constrained_generics::allowed<T>; };

// Resolve qualified generated records and enums in the native exact-type predicate.
template <class T>
concept accepts_selected = requires { typename constrained_generics::selected<T>; };

static_assert(accepts_number<std::int32_t> && accepts_number<double>);
static_assert(!accepts_number<bool> && !accepts_number<std::string>);
static_assert(accepts_allowed<std::int32_t> && accepts_allowed<double>);
static_assert(!accepts_allowed<std::uint32_t> && !accepts_allowed<float>);
static_assert(accepts_selected<constrained_generics::empty>);
static_assert(accepts_selected<constrained_generics::marker>);
static_assert(accepts_selected<constrained_generics::state>);
static_assert(!accepts_selected<std::int32_t>);
static_assert(std::is_same_v<decltype(constrained_generics::number<>::value), std::int32_t>);
static_assert(std::is_same_v<decltype(constrained_generics::nested<>::value),
    constrained_generics::number<double>>);

// Parse whole documents so defaults, forwarding and concrete substitutions share the CLI checker.
auto parse_constraint(std::string_view source) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  return schema::parser::parse(input);
}

// Generic eligibility carries no wire data and preserves the ordinary protocol traversal.
TEST(generic_constraints, open_templates_and_wire_equivalence) {
  constrained_generics::number<> value{};
  value.value = -73;
  constrained_generics::plain_number plain{};
  plain.value = value.value;
  const auto verify = [&]<template <schema::serialize_type> class Protocol>() {
    rohit::full_stream_auto_alloc constrained_bytes{};
    rohit::full_stream_auto_alloc plain_bytes{};
    value.template serialize_out<Protocol>(constrained_bytes);
    plain.template serialize_out<Protocol>(plain_bytes);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(constrained_bytes.begin()),
        constrained_bytes.current_offset()),
        std::string_view(reinterpret_cast<const char*>(plain_bytes.begin()),
            plain_bytes.current_offset()));
    const auto input = rohit::make_constant_stream(constrained_bytes.begin(),
        constrained_bytes.current_offset());
    const auto recovered =
        schema::deserialize_exact<constrained_generics::integer_number, Protocol>(input);
    EXPECT_EQ(recovered.value, value.value);
  };
  verify.template operator()<schema::json>();
  verify.template operator()<schema::binary_none>();
  verify.template operator()<schema::binary_integer>();
  verify.template operator()<schema::binary_string>();
  constrained_generics::nested<double> nested{};
  nested.value.value = 12.5;
  EXPECT_EQ(nested.value.value, 12.5);
}

// Reject invalid concrete types and recheck dependent arguments after their substitution.
TEST(generic_constraints, checked_schema_contract) {
  constexpr std::string_view prelude =
      "serializer version 1.10.0; "
      "class number<T: (integral || floating_point) && !bool = int32> { public T value; } ";
  for (const auto use : {
      "instantiate bad = number<bool>;",
      "class bad { public number<string> value; }",
      "class outer<T> { public number<T> value; } instantiate bad = outer<string>;",
      "class outer<T: integral> { public number<T> value; } instantiate bad = outer<bool>;",
      "class outer<T: (integral || floating_point) && !bool = bool> {}",
      "class outer<T: (integral || floating_point) && !bool = string> {}",
      "class outer<T, U: (integral || floating_point) && !bool = T> {} "
          "instantiate bad = outer<bool>;"}) {
    SCOPED_TRACE(use);
    EXPECT_THROW(parse_constraint(std::string{prelude} + use), std::exception);
  }
  for (const auto use : {
      "instantiate accepted = number<int32>;",
      "instantiate accepted = number<double>;",
      "class outer<T> { public number<T> value; } instantiate accepted = outer<double>;",
      "class outer<T: (integral || floating_point) && !bool> { public number<T> value; } "
          "instantiate accepted = outer<double>;",
      "class outer<T: integral && !bool> { public number<T> value; } "
          "instantiate accepted = outer<int32>;",
      "class outer<T: int32 || double> { public number<T> value; } "
          "instantiate accepted = outer<double>;",
      "class outer<T: (integral || floating_point) && !bool, "
          "U: (integral || floating_point) && !bool = T> { public number<U> value; } "
          "instantiate accepted = outer<int32>;",
      "class outer<T, U: (integral || floating_point) && !bool = T> {} "
          "instantiate accepted = outer<double>;"}) {
    SCOPED_TRACE(use);
    EXPECT_NO_THROW(parse_constraint(std::string{prelude} + use));
  }
}

// Exact atoms compose an allowed list and preserve declaration namespace binding.
TEST(generic_constraints, exact_type_alternatives) {
  constexpr std::string_view prelude =
      "serializer version 1.10.0; namespace tags { class empty {} "
      "class record { public uint32 id; } enum state { idle, active } } "
      "class selected<T: tags::empty || tags::record || tags::state> { public T value; } ";
  for (const auto type : {"tags::empty", "tags::record", "tags::state"}) {
    SCOPED_TRACE(type);
    EXPECT_NO_THROW(parse_constraint(std::string{prelude} +
        "instantiate accepted = selected<" + type + ">;"));
  }
  EXPECT_THROW(parse_constraint(std::string{prelude} +
      "instantiate bad = selected<uint32>;"), std::exception);
  constexpr std::string_view types = "serializer version 1.10.0; "
      "class selected<T: int32 || double = int32> { public T value; } ";
  EXPECT_NO_THROW(parse_constraint(std::string{types} + "instantiate accepted = selected<>;"));
  EXPECT_THROW(parse_constraint(std::string{types} + "instantiate bad = selected<uint32>;"),
      std::exception);
}

// New Boolean constraints require their language contract while the original stateless form stays valid.
TEST(generic_constraints, language_gate_and_diagnostics) {
  for (const auto source : {
      "serializer version 1.9.0; class bad<T: integral> {}",
      "serializer version 1.9.0; class bad<T: stateless || int32> {}",
      "serializer version 1.10.0; class bad<uint64 N: integral> {}",
      "serializer version 1.10.0; class bad<T: unknown> {}",
      "serializer version 1.10.0; class bad<T: integral &&> {}",
      "serializer version 1.10.0; class bad<T: (integral || floating_point> {}",
      "serializer version 1.10.0; class bad<T: !> {}"}) {
    SCOPED_TRACE(source);
    EXPECT_THROW(parse_constraint(source), std::exception);
  }
  EXPECT_NO_THROW(parse_constraint("serializer version 1.9.0; class empty {} "
      "class passive<T: stateless = empty> {} instantiate accepted = passive<>;"));
  EXPECT_NO_THROW(parse_constraint("serializer version 1.10.0; class empty {} "
      "class selected<T: stateless || int32 = empty> {} "
      "instantiate accepted = selected<int32>;"));
}

} // namespace
