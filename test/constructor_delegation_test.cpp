// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "constructor_delegation.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {
namespace fixture = constructor_delegation_fixture;
namespace records = constructor_delegation;

// Reset host observations so failure injection and registrations remain isolated per test.
void reset_observations() {
  fixture::validations = 0;
  fixture::registrations = 0;
  fixture::default_frames = 0;
  fixture::reject_registration = false;
  fixture::reject_default = false;
}

class constructor_delegation_test : public ::testing::Test {
protected:
  // Start with clean host counters and no injected failure.
  void SetUp() override { reset_observations(); }
  // Restore host state even when an assertion or constructor fails.
  void TearDown() override { reset_observations(); }
};

// Parse explicit language contracts for malformed delegation and compatibility boundaries.
auto parse(std::string_view source) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}

// Encode one owned value using the actual generated codec.
template<template<rohit::serializer::serialize_type> class Protocol, class Value>
std::string encode(const Value& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}
} // namespace

TEST_F(constructor_delegation_test, borrowed_adapter_runs_target_hooks_once_and_preserves_copies) {
  records::inputs input;
  input.first = 11;
  input.second = 19;
  const records::authored direct{input};
  EXPECT_EQ(direct.first, input.first);
  EXPECT_EQ(direct.second, input.second);
  EXPECT_EQ(fixture::validations, 1u);
  EXPECT_EQ(fixture::registrations, 1u);
  const auto copied = direct;
  EXPECT_EQ(copied.second, 19u);
  EXPECT_EQ(fixture::registrations, 1u);
  const auto created = records::authored::create(input);
  EXPECT_EQ(created.first, 11u);
  EXPECT_EQ(fixture::validations, 2u);
  EXPECT_EQ(fixture::registrations, 2u);
  const auto bytes = encode<rohit::serializer::binary_integer>(created);
  const auto decoded = rohit::serializer::deserialize_exact<records::authored, rohit::serializer::binary_integer>(
      rohit::make_constant_stream(bytes.data(), bytes.size()));
  EXPECT_EQ(decoded.first, 11u);
  EXPECT_EQ(fixture::registrations, 2u);
}

TEST_F(constructor_delegation_test, validation_and_host_failures_preserve_borrowed_inputs) {
  records::inputs input;
  input.second = 23;
  EXPECT_THROW(static_cast<void>(records::authored{input}), std::domain_error);
  EXPECT_EQ(fixture::validations, 1u);
  EXPECT_EQ(fixture::registrations, 0u);
  input.first = 7;
  fixture::reject_registration = true;
  EXPECT_THROW(static_cast<void>(records::authored::create(input)), fixture::registration_failure);
  EXPECT_EQ(fixture::validations, 2u);
  EXPECT_EQ(fixture::registrations, 1u);
  EXPECT_EQ(input.first, 7u);
  EXPECT_EQ(input.second, 23u);
}

TEST_F(constructor_delegation_test, independent_phantom_input_types_preserve_the_generic_cpp_api) {
  using source = records::generic_authored<std::uint32_t, fixture::nondefault_tag, fixture::other_tag>;
  using target = records::generic_authored<std::uint32_t, fixture::other_tag, fixture::nondefault_tag>;
  const source input{3, 29};
  reset_observations();
  static_assert(std::is_constructible_v<target, const source&>);
  static_assert(!std::is_convertible_v<const source&, target>);
  const target result{input};
  EXPECT_EQ(result.first, 3u);
  EXPECT_EQ(result.second, 29u);
  EXPECT_EQ(fixture::validations, 1u);
  EXPECT_EQ(fixture::registrations, 1u);
  const auto same_type_copy = result;
  EXPECT_EQ(same_type_copy.first, 3u);
  EXPECT_EQ(fixture::registrations, 1u);
}

TEST_F(constructor_delegation_test, borrowed_copy_only_values_do_not_require_default_or_assignment) {
  using value = fixture::copy_only_value;
  static_assert(!std::is_default_constructible_v<value>);
  static_assert(!std::is_move_constructible_v<value>);
  static_assert(!std::is_copy_assignable_v<value>);
  constexpr value marker{31};
  constexpr records::wrapped<value> input{marker};
  constexpr records::forwarded<value> direct{input};
  static_assert(direct.value.marker == 31);
  constexpr auto created = records::forwarded<value>::create(input);
  static_assert(created.value.marker == 31);
  EXPECT_EQ(input.value.marker, 31);
}

TEST_F(constructor_delegation_test, omitted_target_defaults_are_fresh_and_throw_without_noexcept) {
  using owner = records::default_target<fixture::counted_frame>;
  static_assert(!noexcept(owner{true}));
  const owner first{true};
  EXPECT_EQ(first.id, 5u);
  EXPECT_EQ(first.placement.marker, 1u);
  EXPECT_EQ(fixture::default_frames, 1u);
  const auto second = owner::create(true);
  EXPECT_EQ(second.placement.marker, 2u);
  EXPECT_EQ(fixture::default_frames, 2u);
  fixture::reject_default = true;
  EXPECT_THROW(static_cast<void>(owner{true}), fixture::default_failure);
}

TEST_F(constructor_delegation_test, schema_default_bindings_preserve_fresh_decode_and_four_wire_layouts) {
  constexpr records::default_bound value{};
  static_assert(value.value == 17);
  static_assert(records::default_bound::create().value == 17);
  records::plain_bound plain;
  plain.value = value.value;
  const auto check = [&]<template<rohit::serializer::serialize_type> class Protocol>() {
    const auto bytes = encode<Protocol>(value);
    EXPECT_EQ(bytes, encode<Protocol>(plain));
    rohit::serializer::decode_limits limits;
    limits.max_input_bytes = bytes.size();
    limits.max_allocation_bytes = 256;
    limits.max_work_units = 4096;
    limits.max_nesting_depth = 8;
    const auto decoded = rohit::serializer::deserialize_exact<records::default_bound, Protocol>(
        rohit::make_constant_stream(bytes.data(), bytes.size()), limits);
    EXPECT_EQ(decoded.value, 17u);
  };
  check.template operator()<rohit::serializer::binary_none>();
  check.template operator()<rohit::serializer::binary_integer>();
  check.template operator()<rohit::serializer::binary_string>();
  check.template operator()<rohit::serializer::json>();
  const std::string_view empty = "{}";
  const auto fresh = rohit::serializer::deserialize_exact<records::default_bound, rohit::serializer::json>(
      rohit::make_constant_stream(empty.data(), empty.size()));
  EXPECT_EQ(fresh.value, 17u);
}

TEST_F(constructor_delegation_test, rejects_cycles_ambiguous_targets_and_older_language_contracts) {
  for (const auto source : {
      "serializer version 1.17.0; class a { public uint32 value; public constructor(uint32 input) delegates(input); }",
      "serializer version 1.17.0; class a { public uint32 value; public constructor(uint32 input) delegates(true); public constructor(bool input) delegates(1); }",
      "serializer version 1.17.0; class a { public uint32 value; public constructor(float input) {value=0;} public constructor(int32 input) {value=0;} public constructor(bool input) delegates(1); }",
      "serializer version 1.17.0; class a { public uint32 value; public constructor(uint32 input) {value=input;} public constructor(bool input) delegates(false, false); }",
      "serializer version 1.17.0; class a { public uint32 value; public constructor<Input>(Input input) {value=0;} }",
      "serializer version 1.17.0; class a { public uint32 value; public factory build(uint32 input) delegates(input); }",
      "serializer version 1.16.0; class a { public uint32 value; public constructor(uint32 input) delegates(input); }",
      "serializer version 1.16.0; class a { public uint32 value; public constructor(uint32 input const_ref) {value=input;} }",
      "serializer version 1.16.0; class a { public uint32 value; public constructor() {value=17;} }",
      "serializer version 1.17.0; class a { public uint32 value; public constructor() {value=17; initialize cpp { value=18; };} }",
      "serializer version 1.17.0; class a { public uint32 value; public constructor(uint32 input) {value=input; initialize cpp { value=18; };} public constructor() delegates(17); }"}) {
    EXPECT_THROW(parse(source), std::exception) << source;
  }
}

TEST_F(constructor_delegation_test, unsupported_portable_delegation_is_diagnosed_explicitly) {
  const auto schema = parse("serializer version 1.17.0; class inputs { public uint32 value; } "
      "class result { public uint32 value; public constructor(uint32 input) {value=input;} "
      "public constructor(inputs input const_ref) delegates(input.value); }");
  EXPECT_THROW(rohit::serializer::writer::java::generate(schema, "Schema", {}), std::exception);
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_THROW(rohit::serializer::writer::portable::generate(schema, language, "Schema", {}), std::exception) << language;
  }
}
