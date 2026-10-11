// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybrid_construction.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

// Parse a contract fixture through the public compiler API.
auto parse_hybrid(std::string_view text) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(text.data(), text.size()));
}

// Generate spelling-preserving C++ for checking retained opaque runtime contracts.
std::string generate_hybrid(std::string_view text) {
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  options.rename_identifiers = false;
  return rohit::serializer::writer::cpp::generate(parse_hybrid(text), options);
}

// Evaluate inherited generic scalar calls against an independent construction input.
constexpr bool inherited_construction() {
  hybrid_derived_source input{};
  input.value = 27;
  return hybrid_projection{input}.value == 27;
}

} // namespace

// Explicit storage/callee contracts preserve construction and method constant evaluation.
TEST(hybrid_construction, explicit_native_qualification) {
  constexpr hybrid_value value{41};
  static_assert(value.value == 42);
  static_assert(value.doubled() == 84);
  static_assert(value.forwarded_read() == 42);
  static_assert(hybrid_value::create(7).value == 8);
  static_assert(hybrid_value::from_other(value).value == 42);
  static_assert(inherited_construction());
  static_assert(!noexcept(hybrid_value{41}));
  static_assert(!noexcept(hybrid_value::create(7)));
  static_assert(!noexcept(std::declval<const hybrid_value&>().forwarded_read()));
  EXPECT_EQ(value.unqualified_read(), 42u);
  EXPECT_THROW(hybrid_value{std::numeric_limits<std::uint32_t>::max()}, std::domain_error);
}

// Default codec construction and bytes keep the same field-only contract as ordinary values.
TEST(hybrid_construction, codec_defaults_and_wire_identity) {
  constexpr hybrid_value defaults{};
  static_assert(defaults.value == 5);
  const hybrid_value value{41};
  rohit::full_stream_auto_alloc bytes{};
  value.serialize_out<rohit::serializer::binary_none>(bytes);
  const auto input = rohit::make_constant_stream(bytes.begin(), bytes.current_offset());
  const auto decoded = rohit::serializer::deserialize_exact<hybrid_value,
      rohit::serializer::binary_none>(input);
  EXPECT_EQ(decoded.value, 42u);
  EXPECT_EQ(decoded.read(), value.read());
}

// Native readonly argument observations follow schema order and run exactly once.
TEST(hybrid_construction, call_arguments_follow_source_order) {
  hybrid_call_counter = 0;
  const hybrid_call_sequence value{3};
  EXPECT_EQ(value.value, 304u);
  EXPECT_EQ(hybrid_call_counter, 2u);
  hybrid_call_counter = 0;
  EXPECT_EQ(hybrid_call_sequence::create(7).value, 708u);
  EXPECT_EQ(hybrid_call_counter, 2u);
  hybrid_call_counter = 0;
  EXPECT_EQ(hybrid_operand_sequence{3}.value, 304u);
  EXPECT_EQ(hybrid_call_counter, 2u);
  hybrid_call_counter = 0;
  EXPECT_EQ(hybrid_operand_sequence::create(7).value, 708u);
  EXPECT_EQ(hybrid_call_counter, 2u);
}

// New native-block annotations do not consume existing keyword-named field declarations.
TEST(hybrid_construction, preserves_native_tag_and_storage_field_names) {
  const auto schema = parse_hybrid(
      "serializer version 1.8.0; class cpp {} class sample { public cpp storage (3); }");
  const auto& sample = static_cast<const rohit::serializer::class_node&>(*schema.back());
  ASSERT_EQ(sample.member_list.size(), 1u);
  EXPECT_EQ(sample.member_list.front().name, "storage");
  EXPECT_EQ(sample.member_list.front().id, 3u);
}

// Opaque C++ declarations never infer storage or callee constant-evaluation promises.
TEST(hybrid_construction, unqualified_native_contracts_remain_runtime) {
  const auto opaque = generate_hybrid(
      "serializer version 1.12.0; class sample { public uint32 value; "
      "public cpp { constexpr uint32_t read() const { return value; } } "
      "public constructor(uint32 input) { value = input; } }");
  EXPECT_NE(opaque.find("static sample create("), std::string::npos);
  EXPECT_EQ(opaque.find("static constexpr sample create("), std::string::npos);
  const auto runtime_call = generate_hybrid(
      "serializer version 1.12.0; class source { public function uint32 read() readonly cpp { return 7; }; } "
      "class sample { public uint32 value; public constructor(source input) { value = input.read(); } }");
  EXPECT_NE(runtime_call.find("static sample create("), std::string::npos);
  EXPECT_EQ(runtime_call.find("static constexpr sample create("), std::string::npos);
  const auto other_backend = generate_hybrid(
      "serializer version 1.12.0; class sample { public uint32 value; "
      "public java { int native_storage; } public constructor(uint32 input) { value = input; } }");
  EXPECT_NE(other_backend.find("static constexpr sample create("), std::string::npos);
}

// New contracts are version-gated and cannot claim mutation, dispatch or a missing native body.
TEST(hybrid_construction, rejects_invalid_qualification_and_calls) {
  for (const auto source : {
      "serializer version 1.11.0; class sample { public cpp storage(none) {} }",
      "serializer version 1.12.0; class sample { public java storage(none) {} }",
      "serializer version 1.12.0; class sample { public cpp storage(stateful) {} }",
      "serializer version 1.11.0; function uint32 read() readonly constexpr(cpp) cpp { return 1; };",
      "serializer version 1.12.0; function uint32 read() readonly constexpr(java) cpp { return 1; };",
      "serializer version 1.12.0; class sample { public function uint32 read() edit constexpr(cpp) cpp { return 1; }; }",
      "serializer version 1.12.0; class sample { public function virtual uint32 read() readonly constexpr(cpp) cpp { return 1; }; }",
      "serializer version 1.12.0; function uint32 read() readonly constexpr(cpp);",
      "serializer version 1.12.0; function uint32 read() readonly constexpr(cpp) expression(1);",
      "serializer version 1.11.0; class source { public function uint32 read() readonly expression(1); } "
          "class sample { public uint32 value; public constructor(source input) { value = input.read(); } }",
      "serializer version 1.12.0; class sample { public uint32 value; public function uint32 read() readonly expression(value); "
          "public constructor(uint32 input) { value = read(); } }",
      "serializer version 1.12.0; class source { public function uint32 read() edit cpp { return 1; }; } "
          "class sample { public uint32 value; public constructor(source input) { value = input.read(); } }",
      "serializer version 1.12.0; class result { public uint32 value; } "
          "class source { public function result read() readonly cpp { return {}; }; } "
          "class sample { public uint32 value; public constructor(source input) { value = input.read().value; } }"}) {
    EXPECT_THROW(parse_hybrid(source), std::exception) << source;
  }
}
