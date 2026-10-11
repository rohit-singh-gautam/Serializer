// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "construction.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <type_traits>

namespace {
// Resolve each review fixture with the current supported language contract.
auto parse_construction(std::string_view source) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}
} // namespace

// Qualify owner-bound construction, fresh structured defaults, and transparent empty layout.
TEST(construction, owning_values_and_layout) {
  const construction_point direct{5.0, 7.0};
  EXPECT_EQ(direct.x, 5.0);
  EXPECT_EQ(direct.y, 7.0);
  const auto created = construction_point::create(8.0, 9.0);
  EXPECT_EQ(created.x, 8.0);
  construction_record first{};
  construction_record second{};
  EXPECT_EQ(first.position.x, 2.0);
  first.position.x = 20.0;
  EXPECT_EQ(second.position.x, 2.0);
  const auto positioned = construction_record::positioned(direct);
  EXPECT_EQ(positioned.position.x, 5.0);
  const construction_byte_identity identity{std::uint8_t{42}};
  EXPECT_EQ(identity.value, 42);
  static_assert(std::is_same_v<decltype(construction_byte_identity::from_value(std::uint8_t{1})), construction_byte_identity>);
  static_assert(std::is_empty_v<construction_empty>);
  EXPECT_EQ(sizeof(construction_record), sizeof(construction_point) + sizeof(std::uint64_t));
  for (const auto& value : {first, second, positioned}) {
    rohit::full_stream_auto_alloc bytes{};
    value.serialize_out<rohit::serializer::binary_none>(bytes);
    const auto input = rohit::make_constant_stream(bytes.begin(), bytes.current_offset());
    auto decoded = rohit::serializer::deserialize_exact<construction_record, rohit::serializer::binary_none>(input);
    EXPECT_EQ(decoded.position.x, value.position.x);
    EXPECT_EQ(decoded.tag, value.tag);
  }
}

// Portable declarations preserve scalar, generic, literal and structured-default constant construction.
TEST(construction, constexpr_owned_construction) {
  constexpr construction_point point{5.0, 7.0};
  static_assert(point.x == 5.0 && point.y == 7.0);
  static_assert(construction_point::create(8.0, 9.0).y == 9.0);
  static_assert(construction_point::origin().x == 2.0);
  static_assert(construction_record::noop(1.0).position.y == 3.0);
  static_assert(construction_record::positioned(point).position.x == 5.0);
  static_assert(construction_byte_identity::create(std::uint8_t{42}).value == 42);
  static_assert(construction_identity<std::uint8_t>::from_value(std::uint8_t{11}).value == 11);
  EXPECT_EQ(point.y, 7.0);
}

// Known nonconstexpr storage retains ordinary APIs rather than introducing invalid declarations.
TEST(construction, constexpr_eligibility_preserves_runtime_shapes) {
  namespace writer = rohit::serializer::writer;
  writer::cpp_options options; options.format = false; options.rename_identifiers = false;
  for (const auto text : {
      "class sample { public map<string, double> values; public factory make() {} }",
      "class sample { public array sample children; public factory make() {} }",
      "class sample { public cpp { int native_storage{}; } public factory make() {} }"}) {
    const auto schema = parse_construction(text);
    const auto generated = writer::cpp::generate(schema, options);
    EXPECT_NE(generated.find("static sample make("), std::string::npos);
    EXPECT_EQ(generated.find("static constexpr sample make("), std::string::npos);
  }
  const auto strings = parse_construction(
      "class sample { public string text; public factory make(string input) { text = input; } }");
  EXPECT_NE(writer::cpp::generate(strings, options).find("static constexpr sample make("), std::string::npos);
}

// Direct and separate managed payload lowering must retain declared owned construction APIs.
TEST(construction, managed_lowering_preserves_scalar_construction) {
  namespace writer = rohit::serializer::writer;
  const auto schema = parse_construction(
      "class managed_sample stable_ids managed { public double value (1); "
      "public constructor(double input) { value = input; } "
      "public factory origin() { value = 2.0; } }");
  writer::cpp_options options; options.format = false; options.rename_identifiers = false;
  for (const bool separate : {false, true}) {
    options.managed_separate_values = separate;
    const auto generated = writer::cpp::generate(schema, options);
    EXPECT_NE(generated.find("static managed_sample create("), std::string::npos);
    EXPECT_NE(generated.find("static managed_sample origin("), std::string::npos);
    EXPECT_NE(generated.find("managed_sample([[maybe_unused]] double srl_arg_input)"), std::string::npos);
    if (separate) {
      EXPECT_NE(generated.find("static managed_managed_sample_data create("), std::string::npos);
      EXPECT_NE(generated.find("static managed_managed_sample_data origin("), std::string::npos);
    }
  }
}

// Reject ill-typed bindings, invalid default factories, stateful hints, and old declarations.
TEST(construction, checked_schema_contract) {
  using rohit::serializer::parser::parse;
  for (const auto text : {
      "serializer version 1.7.0; class p { public constructor(double x) { } }",
      "class p { public double x; public constructor(int32 input) { x = input; } }",
      "class p { public double x; public factory make() { missing = 1; } }",
      "class p { public double x; } class q { public p value default(missing); }",
      "class p { public double x; } class q { public transient layout(empty) p value; }",
      "class p { public constructor() {} }",
      "class p { public double x; public double y; public factory make(double v) { x = v; y = v; } }",
      "class p { public constructor(p value) {} }",
      "class p { public double create; public double x; public constructor(double v) { x = v; } }",
      "class p { public double x; public factory p() { x = 1.0; } }",
      "class p view readonly { public double x; public factory make() { x = 1.0; } }",
      "class borrowed view readonly { public double x; } class p { public factory use(borrowed input) {} }",
      "class borrowed view readonly { public double x; } class p { public factory use(optional<borrowed> input) {} }",
      "class borrowed view readonly { public double x; } class p { public factory use(array<borrowed> input) {} }",
      "class borrowed view readonly { public double x; } class p { public factory use(map<string, borrowed> input) {} }",
      "class p { public p value default(make); public factory make() {} }",
      "class p { public p value; public factory make() { value = default(make); } }"}) {
    const std::string_view source{text};
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(parse(input), std::exception) << text;
  }
  EXPECT_NO_THROW(parse_construction("class p { public factory from_self(p input) {} }"));
}

// A borrowing emission representation cannot expose APIs that claim owned construction.
TEST(construction, rejects_borrowing_emission_constructions) {
  namespace writer = rohit::serializer::writer;
  const auto schema = parse_construction(
      "class sample { public double x; public factory make(double x) { x = x; } }");
  writer::cpp_options options;
  options.format = false;
  options.constant_evaluation = true;
  options.emission_only = true;
  options.protocols = writer::cpp_protocols::binary_none;
  EXPECT_THROW(writer::cpp::generate(schema, options), std::invalid_argument);
}

// Literal validation must preserve the source form needed by every target's typed lowerer.
TEST(construction, quoted_char_and_qualified_enum_factory_literals) {
  namespace writer = rohit::serializer::writer;
  const auto schema = parse_construction(
      "serializer version 1.8.0; enum state { idle, active } class sample { "
      "public char marker; public state status; public string text; "
      "public factory make() { marker = '\\n'; status = state::active; text = \"owned\\ntext\"; } }");
  const auto& sample = static_cast<const rohit::serializer::class_node&>(*schema.back());
  ASSERT_EQ(sample.constructions.size(), 1u);
  EXPECT_EQ(sample.constructions.front().assignments.front().value, "'\\n'");
  EXPECT_EQ(sample.constructions.front().assignments[1].value, "state::active");
  writer::cpp_options cpp;
  cpp.format = false;
  cpp.rename_identifiers = false;
  const auto generated = writer::cpp::generate(schema, cpp);
  EXPECT_NE(generated.find("state::active"), std::string::npos);
  EXPECT_FALSE(writer::java::generate(schema, "Schema", {}).empty());
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_FALSE(writer::portable::generate(schema, language, "Schema", {}).empty()) << language;
  }
}

// Typed layout lookahead must leave existing user type names and numeric field identities intact.
TEST(construction, layout_lookahead_preserves_existing_names) {
  const auto schema = parse_construction(
      "serializer version 1.8.0; class constructor {} class factory {} class transient {} "
      "class layout {} class sample { public constructor first; public factory second; "
      "public transient layout (3); public transient layout runtime_value; }");
  const auto& sample = static_cast<const rohit::serializer::class_node&>(*schema.back());
  ASSERT_EQ(sample.member_list.size(), 4u);
  EXPECT_EQ(sample.member_list[2].type_name_list.front().name, "transient");
  EXPECT_EQ(sample.member_list[2].name, "layout");
  EXPECT_EQ(sample.member_list[2].id, 3u);
  EXPECT_FALSE(sample.member_list[2].transient);
  EXPECT_TRUE(sample.member_list[3].transient);
}

// Generated construction symbols must not collide with codec APIs or profile-converted names.
TEST(construction, rejects_generated_and_renamed_factory_collisions) {
  namespace writer = rohit::serializer::writer;
  writer::cpp_options cpp;
  cpp.format = false;
  const auto reserved = parse_construction("class sample { public factory serialize_out() {} }");
  EXPECT_THROW(writer::cpp::generate(reserved, cpp), std::invalid_argument);
  const auto renamed = parse_construction(
      "class sample { public factory some_name() {} public factory SomeName() {} }");
  EXPECT_THROW(writer::cpp::generate(renamed, cpp), std::invalid_argument);
  const auto duplicate_parameters = parse_construction(
      "class sample { public factory make(double some_name, double SomeName) {} }");
  EXPECT_THROW(writer::cpp::generate(duplicate_parameters, cpp), std::invalid_argument);
  const auto codec_collision = parse_construction("class sample { public factory decode() {} }");
  for (const auto language : {"js", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_THROW(writer::portable::generate(codec_collision, language, "Schema", {}), std::invalid_argument) << language;
  }
}

// Preserve schema access when a factory is an owner-prefixed Go function or a TS declaration.
TEST(construction, nonpublic_portable_factory_visibility) {
  namespace writer = rohit::serializer::writer;
  const auto schema = parse_construction(
      "class sample { public uint32 value; private factory secret() { value = 3; } "
      "protected constructor(uint32 input) { value = input; } }");
  const auto go = writer::portable::generate(schema, "go", "Schema", {});
  EXPECT_NE(go.find("func sample_secret("), std::string::npos);
  EXPECT_NE(go.find("func sample_create("), std::string::npos);
  EXPECT_EQ(go.find("func Sample_secret("), std::string::npos);
  const auto typescript = writer::portable::generate(schema, "typescript", "Schema", {});
  EXPECT_NE(typescript.find("private static secret("), std::string::npos);
  EXPECT_NE(typescript.find("private static create("), std::string::npos);
  const auto javascript = writer::portable::generate(schema, "js", "Schema", {});
  EXPECT_NE(javascript.find("static #secret("), std::string::npos);
  EXPECT_NE(javascript.find("static #create("), std::string::npos);
}
