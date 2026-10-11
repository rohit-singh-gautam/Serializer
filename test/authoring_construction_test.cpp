// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "authoring_construction.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {
// A custom C++ frame opts into the same fresh owning factory contract.
struct custom_authoring_frame {
  std::uint32_t basis{};
  std::string label{};
  // Construct independent host-owned identity placement without execution registration.
  static custom_authoring_frame identity() { return {1, "custom identity"}; }
};

// Detect whether the readonly API admits a forbidden Boolean representation.
template <typename Value>
concept supports_boolean_scale = requires(const Value& value) { value.scaled(true); };
}

// Existing compiler clients retain the positional prefix of callable aggregate initialization.
TEST(authoring_construction, preserves_callable_aggregate_initialization) {
  namespace schema = rohit::serializer;
  const schema::function_declaration method{schema::access_type::public_access,
      schema::type_name{std::string{"uint32"}, nullptr}, "read", {},
      schema::function_declaration::effect_type::readonly,
      schema::function_declaration::dispatch_type::ordinary};
  EXPECT_EQ(method.name, "read");
  EXPECT_EQ(method.effect, schema::function_declaration::effect_type::readonly);
  EXPECT_TRUE(method.generic_parameters.empty());
}

// Authoring calls validate first, register once, and construct independent factory defaults.
TEST(authoring_construction, lifecycle_and_fresh_generic_defaults) {
  authoring_host::registrations = 0;
  const authoring_placed_value codec_default{};
  EXPECT_EQ(codec_default.count, 7u);
  EXPECT_EQ(codec_default.placement.basis, 0u);
  EXPECT_EQ(authoring_host::registrations, 0u);
  auto first = authoring_placed_value{2.0};
  auto second = authoring_placed_value::create_default_1(3.0);
  EXPECT_EQ(first.count, 32u);
  EXPECT_EQ(second.placement.basis, 1u);
  first.placement.label = "changed";
  EXPECT_EQ(second.placement.label, "identity");
  EXPECT_EQ(authoring_host::registrations, 2u);
  const auto copied = second;
  EXPECT_EQ(copied.execution, second.execution);
  EXPECT_EQ(authoring_host::registrations, 2u);
  EXPECT_THROW(authoring_placed_value{-1.0}, std::domain_error);
  EXPECT_THROW(authoring_placed_value(2.0, 2), std::domain_error);
  EXPECT_THROW(authoring_placed_value(2.0, 32, authoring_frame{}), std::domain_error);
  EXPECT_THROW(authoring_placed_value{std::numeric_limits<double>::infinity()}, std::domain_error);
  EXPECT_THROW(authoring_placed_value{std::numeric_limits<double>::quiet_NaN()}, std::domain_error);
  EXPECT_EQ(authoring_host::registrations, 2u);
  rohit::full_stream_auto_alloc output{};
  second.serialize_out<rohit::serializer::binary_none>(output);
  auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
  const auto decoded = rohit::serializer::deserialize_exact<authoring_placed_value,
      rohit::serializer::binary_none>(input);
  EXPECT_EQ(decoded.dimension, second.dimension);
  EXPECT_EQ(decoded.execution, 0u);
  EXPECT_EQ(authoring_host::registrations, 2u);
}

// Open C++ models accept a custom representation only when its owning factory matches exactly.
TEST(authoring_construction, custom_frame_contract) {
  auto first = authoring_value<custom_authoring_frame>{2.0};
  const auto second = authoring_value<custom_authoring_frame>{3.0, 8};
  first.placement.label = "changed";
  EXPECT_EQ(second.placement.label, "custom identity");
  EXPECT_EQ(second.count, 8u);
}

// Mixed representations follow std::common_type while checking both operands before arithmetic.
TEST(authoring_construction, common_numeric_readonly_methods) {
  constexpr authoring_signed_numeric input{6};
  static_assert(!supports_boolean_scale<authoring_signed_numeric>);
  static_assert(std::is_same_v<decltype(input.scaled(0.5)), double>);
  static_assert(input.scaled(0.5) == 3.0);
  static_assert(input.divided(2) == 3);
  static_assert(std::is_same_v<decltype(input.echo(std::int16_t{3})), std::int16_t>);
  static_assert(input.echo(std::int16_t{3}) == 3);
  EXPECT_EQ(input.echo(std::uint64_t{42}), 42u);
  EXPECT_THROW(static_cast<void>(input.divided(4)), std::domain_error);
  EXPECT_THROW(static_cast<void>(input.divided(0)), std::domain_error);
  const authoring_signed_numeric negative{-1};
  EXPECT_THROW(static_cast<void>(negative.scaled(std::uint32_t{2})), std::domain_error);
  const authoring_wide_numeric maximum{std::numeric_limits<std::uint64_t>::max()};
  EXPECT_THROW(static_cast<void>(maximum.scaled(1.0)), std::domain_error);
  EXPECT_THROW(static_cast<void>(input.scaled(std::numeric_limits<double>::infinity())), std::domain_error);
}

// New contracts require explicit factory promises and cannot silently alter older schema meaning.
TEST(authoring_construction, rejects_unqualified_contracts) {
  const std::string_view rejected[]{
      "serializer version 1.11.0; class p<T: factory(identity)> {}",
      "serializer version 1.12.0; class p<T> { public T value; public constructor(T input = default(identity)) { value = input; } }",
      "serializer version 1.12.0; class p<T: factory(identity) || integral> { public T value; public constructor(T input = default(identity)) { value = input; } }",
      "serializer version 1.12.0; class p { public uint32 x; public constructor(uint32 input) { x = input; validate expression(x); } }",
      "serializer version 1.12.0; class p { public uint32 x; public constructor(uint32 input) { validate expression(true); x = input; } }",
      "serializer version 1.12.0; class p { public function common_type<string, double> bad() readonly cpp { return 0; }; }",
      "serializer version 1.12.0; class p { public function common_type<bool, double> bad() readonly cpp { return 0; }; }",
      "serializer version 1.12.0; class p { public function common_type<Input, double> bad<Input: integral>(Input input) readonly cpp { return input; }; }",
      "serializer version 1.12.0; class p<T: integral> { public function common_type<T, double> bad(T input) readonly cpp { return input; }; }",
      "serializer version 1.12.0; class p { public function common_type<Input, double> bad<Input: integral || !bool>(Input input) readonly cpp { return input; }; }",
      "serializer version 1.12.0; class p { public function uint32 bad(common_type<string, double> input) readonly cpp { return 0; }; }",
      "serializer version 1.12.0; function uint32 bad(common_type<bool, double> input) readonly;",
      "serializer version 1.12.0; class p<T: integral> { public function uint32 bad(common_type<T, double> input) readonly expression(0); }",
      "serializer version 1.12.0; class wrapper<T> { public T value; } class p { public function wrapper<common_type<uint32, double>> bad() readonly cpp { return {}; }; }",
      "serializer version 1.12.0; class wrapper<T> { public T value; } class p { public function uint32 bad(wrapper<common_type<uint32, double>> input) readonly cpp { return 0; }; }",
      "serializer version 1.12.0; class p { public function uint32 bad(array<common_type<uint32, double>> input) readonly cpp { return 0; }; }",
      "serializer version 1.12.0; class wrapper<T> { public T value; } instantiate p = wrapper<common_type<uint32, double>>;",
      "serializer version 1.12.0; class wrapper<T = common_type<uint32, double>> { public T value; }",
      "serializer version 1.12.0; class wrapper<T: common_type<uint32, double>> { public T value; }",
      "serializer version 1.11.0; class p { public function double f<Input: integral>(Input input) readonly expression(0.0); }"
  };
  for (const auto schema : rejected) {
    SCOPED_TRACE(schema);
    const auto input = rohit::make_constant_stream(schema.data(), schema.size());
    EXPECT_THROW(rohit::serializer::parser::parse(input), std::exception);
  }
}

// Boolean exclusion belongs to the common result policy, preserving ordinary numeric bindings.
TEST(authoring_construction, accepts_proven_numeric_result_constraints) {
  const std::string_view accepted[]{
      "serializer version 1.12.0; class p { public function common_type<Input, double> f<Input: (integral || floating_point) && !bool>(Input input) readonly cpp { return input; }; }",
      "serializer version 1.12.0; class p { public function common_type<Input, double> f<Input: floating_point>(Input input) readonly cpp { return input; }; }",
      "serializer version 1.12.0; class p { public function common_type<Input, double> f<Input: int32 || double>(Input input) readonly cpp { return input; }; }",
      "serializer version 1.12.0; class p { public function uint32 f(common_type<uint32, double> input) readonly cpp { return 0; }; }",
      "serializer version 1.12.0; function uint32 f(common_type<uint32, double> input) readonly cpp { return 0; };",
      "serializer version 1.12.0; class p { public function Input f<Input: integral>(Input input) readonly expression(checked<Input>(input)); }"
  };
  for (const auto schema : accepted) {
    SCOPED_TRACE(schema);
    const auto input = rohit::make_constant_stream(schema.data(), schema.size());
    EXPECT_NO_THROW(rohit::serializer::parser::parse(input));
  }
}

// Independent generic deduction is a host C++ API contract, with an explicit schema-call diagnostic.
TEST(authoring_construction, diagnoses_independently_generic_expression_calls) {
  const std::string_view rejected[]{
      "serializer version 1.12.0; function Input helper<Input: integral>(Input input) readonly expression(input); class p { public function int32 f(int32 input) readonly expression(helper(input)); }",
      "serializer version 1.12.0; class p { public function Input helper<Input: integral>(Input input) readonly expression(input); public function int32 f(int32 input) readonly expression(helper(input)); }"
  };
  for (const auto schema : rejected) {
    SCOPED_TRACE(schema);
    const auto input = rohit::make_constant_stream(schema.data(), schema.size());
    try {
      rohit::serializer::parser::parse(input);
      FAIL() << "Independently generic expression call was accepted";
    } catch (const std::exception& error) {
      EXPECT_NE(std::string_view{error.what()}.find("independently generic functions are unsupported"), std::string_view::npos);
    }
  }
}
