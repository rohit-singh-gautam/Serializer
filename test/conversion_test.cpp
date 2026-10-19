// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "conversion.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
// Resolve isolated conversion declarations against the supported schema contract.
auto parse_conversion(std::string_view text) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(text.data(), text.size()));
}
// Encode the source before and after conversion to prove the readonly wire contract.
std::string encoded(const shipment_ticket& value) {
  rohit::full_stream_auto_alloc stream{};
  value.serialize_out<rohit::serializer::binary_none>(stream);
  return {reinterpret_cast<const char*>(stream.begin()), stream.current_offset()};
}
} // namespace

// Collection payloads become borrowable in 1.19 without widening the owning-copy contract.
TEST(conversion, payload_record_translator_language_contract) {
  constexpr std::string_view declarations =
      "class payload_record { public array float values (1); public array uint32 codes (2); } "
      "class target { public payload_record value (1); } "
      "class source { public payload_record data (1); public translator target as project { value=data; }; }";
  EXPECT_THROW(parse_conversion(std::string{"serializer version 1.18.0; "} + std::string{declarations}), std::exception);
  EXPECT_NO_THROW(parse_conversion(std::string{"serializer version 1.19.0; "} + std::string{declarations}));
  EXPECT_THROW(parse_conversion("serializer version 1.19.0; "
      "class payload_record { public array float values (1); } class target { public payload_record value (1); } "
      "class source { public payload_record data (1); public conversion target as copy { value=data; }; }"), std::exception);
}

// Explicit owner-bound conversions copy mapped values independently and retain fresh defaults.
TEST(conversion, explicit_owned_values_and_generic_binding) {
  constexpr auto ticket = [] { short_ticket value{}; value.value = 17; return value.to_view(); }();
  static_assert(ticket.value == 17 && ticket.revision == 3);
  constexpr auto revisioned = [] {
    conversion_versioned_source value{};
    value.value = 29;
    return static_cast<conversion_versioned_target>(value);
  }();
  static_assert(revisioned.value == 29);
  static_assert(revisioned.revision == rohit::serializer::version3{1, 2, 3});
  static_assert(noexcept(std::declval<const conversion_versioned_source&>().converted()));
  static_assert(noexcept(static_cast<conversion_versioned_target>(
      std::declval<const conversion_versioned_source&>())));
  static_assert(!noexcept(std::declval<const shipment_ticket&>().to_notice()));
  static_assert(!std::is_convertible_v<shipment_ticket, postal_address>);
  static_assert(!std::is_convertible_v<short_ticket, ticket_view<std::uint16_t>>);
  shipment_ticket source{};
  source.number = UINT64_MAX; source.contact.delivery_count = 19;
  source.region = routing_mode::express; source.priority = 7;
  const auto before = encoded(source);
  auto result = source.to_notice();
  EXPECT_EQ(result.number, UINT64_MAX);
  EXPECT_EQ(result.destination.delivery_count, 19);
  EXPECT_EQ(result.mode, routing_mode::express);
  EXPECT_EQ(result.priority, 7);
  EXPECT_TRUE(result.preview);
  result.destination.street = "Changed";
  EXPECT_EQ(source.contact.street, "Harbor Road");
  const auto address = static_cast<postal_address>(source);
  EXPECT_EQ(address.postcode, 410);
  EXPECT_EQ(address.delivery_count, 12);
  EXPECT_EQ(encoded(source), before);
}

// Malformed, unavailable, ambiguous or unsupported conversions diagnose instead of coercing/sharing.
TEST(conversion, rejected_contracts) {
  for (const auto source : {
      "serializer version 1.8.0; class a {} class b { public conversion a as convert {}; }",
      "class b { public conversion a as convert {}; } class a {}",
      "class a { public uint32 x; } class b { public uint64 x; public conversion a as convert { x=x; }; }",
      "class a { public uint32 x; } class b { private uint32 x; public conversion a as convert { x=x; }; }",
      "class a { private uint32 x; } class b { public uint32 x; public conversion a as convert { x=x; }; }",
      "class a { public uint32 x; } class b { public uint32 x; public conversion a as convert { missing=x; }; }",
      "class a { public uint32 x; } class b { public uint32 x; public conversion a as convert { x=x; x=x; }; }",
      "class a { public uint32 x; } class b { public uint32 x; public conversion a as convert { x=x+1; }; }",
      "class a { public uint32 x; } class b { public uint32 x; public conversion a as convert { x=x; } }",
      "class a { public uint32 x; } class b { public uint32 x; public conversion a as x { x=x; }; }",
      "class a { public array<uint32> x; } class b { public array<uint32> x; public conversion a as convert { x=x; }; }",
      "class p { public string text; private uint32 secret; } class a { public p x; } class b { public p x; public conversion a as convert { x=x; }; }",
      "class a managed { public uint32 x; } class b { public uint32 x; public conversion a as convert { x=x; }; }",
      "class a { public cpp { int native{}; } } class b { public conversion a as convert {}; }",
      "class a<T> { public T x; } class b<T> { public T x; public conversion a<T> as convert { x=x; }; } instantiate bad=b<optional<uint32>>;",
      "class a<T> { public T x; } class b<T> { public uint32 x; public conversion a<T> as convert { x=x; }; }"}) {
    EXPECT_THROW(parse_conversion(source), std::exception) << source;
  }
}

// Existing names and exact generic inherited projections retain their lexical meaning.
TEST(conversion, names_and_dependent_projections) {
  const auto old_names = parse_conversion("serializer version 1.0.0; class conversion {} class sample { public conversion value; }");
  EXPECT_EQ(old_names.size(), 2);
  const auto schema = parse_conversion(
      "class target<T> { public T value; } class parent_value<U> { public U value; } "
      "class source<T> : public parent_value<T> { public conversion target<T> as converted { value=value; }; } "
      "instantiate concrete=source<uint64>;");
  namespace writer = rohit::serializer::writer;
  writer::cpp_options options; options.format = false; options.rename_identifiers = false;
  const auto output = writer::cpp::generate(schema, options);
  EXPECT_NE(output.find("converted() const"), std::string::npos);
  EXPECT_NE(output.find("explicit constexpr operator"), std::string::npos);
}

// Every target emits the opted-in API and rejects names colliding with generated lifecycle methods.
TEST(conversion, portable_generation_and_names) {
  const auto source = "class a { public uint32 x; } class b { public uint32 x; public conversion a as convert { x=x; }; }";
  const auto schema = parse_conversion(source);
  namespace writer = rohit::serializer::writer;
  writer::java_options java;
  EXPECT_NE(writer::java::generate(schema, "Schema", java).find("convert()"), std::string::npos);
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    writer::portable_options options; options.package_name = "main";
    EXPECT_NE(writer::portable::generate(schema, language, "Schema", options).find(
        std::string_view{language} == "go" || std::string_view{language} == "csharp" ? "Convert(" : "convert("), std::string::npos);
  }
  const auto collision = parse_conversion(
      "class a {} class b { public conversion a as encode {}; }");
  EXPECT_THROW(writer::java::generate(collision, "Schema", java), std::invalid_argument);
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    writer::portable_options options;
    EXPECT_THROW(writer::portable::generate(collision, language, "Schema", options), std::invalid_argument) << language;
  }
}

// Computed wire-only metadata is neither a source value nor a stored nested-copy component.
TEST(conversion, wire_metadata_boundaries) {
  const std::string wire = "public wire_only uint32 bytes (1) auto_size(binary_none) omit(all_except(binary_none)); ";
  EXPECT_THROW(parse_conversion("class a { public uint32 x; } class b { " + wire +
      "public conversion a as converted { x=bytes; }; }"), std::exception);
  EXPECT_THROW(parse_conversion("class a { " + wire + "} class b { public uint32 x; "
      "public conversion a as converted { bytes=x; }; }"), std::exception);
  EXPECT_THROW(parse_conversion("class a { " + wire +
      "public function bool valid() readonly predicate(bytes==0); }"), std::exception);
  const auto schema = parse_conversion("class p { " + wire + "public uint32 value (2); } "
      "class target { public p copied; } class origin { public p stored; "
      "public conversion target as converted { copied=stored; }; }");
  namespace writer = rohit::serializer::writer;
  writer::cpp_options options; options.format = false;
  const auto generated = writer::cpp::generate(schema, options);
  EXPECT_NE(generated.find("converted() const"), std::string::npos);
  EXPECT_EQ(generated.find("srl_result.copied.bytes"), std::string::npos);
}

// Scoped translators select a new language contract while keeping old user type names available.
TEST(conversion, translator_contract_and_portable_generation) {
  EXPECT_NO_THROW(parse_conversion("serializer version 1; class translator {} class owner { public translator value; }"));
  EXPECT_THROW(parse_conversion("serializer version 1.17.0; class target {} class source { public translator target as project {}; }"), std::exception);
  EXPECT_THROW(parse_conversion("serializer version 1.18.0; class target {} class source { private translator target as project {}; }"), std::exception);
  EXPECT_THROW(parse_conversion("serializer version 1.18.0; class target {} class source { protected translator target as project {}; }"), std::exception);
  EXPECT_NO_THROW(parse_conversion("serializer version 1.18.0; class target { public uint32 value; public constructor() { value=1; } }"));
  EXPECT_THROW(parse_conversion("serializer version 1.18.0; class target { public uint32 value; public constructor() { value=1; } } "
      "class source { public translator target as project {}; }"), std::exception);
  EXPECT_THROW(parse_conversion("serializer version 1.18.0; class child { public uint32 value; public factory fresh() { value=1; } } "
      "class target { public child value default(fresh); } class source { public translator target as project {}; }"), std::exception);
  const auto schema = parse_conversion("serializer version 1.18.0; class target { public string label; public uint32 count {7}; } "
      "class source { public string text; public translator target as project { label=text; }; }");
  const auto& source = static_cast<const rohit::serializer::class_node&>(*schema.back());
  ASSERT_EQ(source.conversions.size(), 1);
  EXPECT_TRUE(source.conversions.front().translator);
  namespace writer = rohit::serializer::writer;
  writer::cpp_options cpp; cpp.format = false; cpp.rename_identifiers = false;
  const auto output = writer::cpp::generate(schema, cpp);
  EXPECT_NE(output.find("with_project"), std::string::npos);
  EXPECT_NE(output.find("this->get_label()"), std::string::npos);
  EXPECT_NE(output.find("const && = delete"), std::string::npos);
  EXPECT_EQ(output.find("srl_result"), std::string::npos);
  const auto collision = parse_conversion("serializer version 1.18.0; class target {} "
      "class source { public uint32 with_project; public translator target as project {}; }");
  EXPECT_THROW(writer::cpp::generate(collision, cpp), std::invalid_argument);
  auto constant_profile = cpp;
  constant_profile.constant_evaluation = true;
  constant_profile.protocols = writer::cpp_protocols::binary_none;
  EXPECT_THROW(writer::cpp::generate(schema, constant_profile), std::invalid_argument);
  auto protobuf_profile = cpp;
  protobuf_profile.protobuf = true;
  EXPECT_THROW(writer::cpp::generate(schema, protobuf_profile), std::invalid_argument);
  writer::java_options java;
  EXPECT_NE(writer::java::generate(schema, "Schema", java).find("withProject"), std::string::npos);
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    writer::portable_options options; options.package_name = "main";
    const auto generated = writer::portable::generate(schema, language, "Schema", options);
    const std::string_view selected{language};
    const bool snake = selected == "rust" || selected == "python" || selected == "c";
    const bool exported = selected == "go" || selected == "csharp";
    EXPECT_NE(generated.find(snake ? "with_project" : exported ? "WithProject" : "withProject"), std::string::npos) << language;
    EXPECT_NE(generated.find(snake ? "get_label" : exported ? "GetLabel" : "getLabel"), std::string::npos) << language;
    EXPECT_EQ(generated.find("srl_result"), std::string::npos) << language;
  }
}
