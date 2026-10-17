// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>
#include <rohit/xml.hpp>
#include <xml_fixture.hpp>
#include <xml_xsd_fixture.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;

// Create an owning nested fixture with escaped content and both optional/array cardinalities.
xml_fixture::order sample() {
  xml_fixture::order value{};
  value.id = 7U;
  value.customer = "R&D <Customer>";
  value.items.push_back(xml_fixture::item{"Widget", 3});
  value.note = "line 1\r\nline 2";
  value.status = xml_fixture::state::closed;
  value.codes = {4U, 5U};
  return value;
}
// Emit a schema in memory using the production generator and explicit XML options.
std::string generate(std::string_view source, rohit::serializer::writer::cpp_options options) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input);
  options.format = false;
  return rohit::serializer::writer::cpp::generate(statements, options);
}
// Exercise the production portable dispatcher with XML enabled or disabled.
std::string generate_portable(std::string_view source, std::string_view language, bool xml_enabled) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input);
  rohit::serializer::writer::portable_options options{};
  options.xml.enabled = xml_enabled;
  return rohit::serializer::writer::portable::generate(statements, language, "Schema", options);
}
} // namespace

TEST(xml, owning_round_trip_and_prefix_independence) {
  const auto original = sample();
  const auto bytes = xml::serialize(original);
  EXPECT_NE(bytes.find(" id=\"7\""), std::string::npos);
  EXPECT_NE(bytes.find("R&amp;D &lt;Customer&gt;"), std::string::npos);
  const auto result = xml::deserialize<xml_fixture::order>(bytes);
  EXPECT_EQ(result.id, original.id);
  EXPECT_EQ(result.customer, original.customer);
  ASSERT_EQ(result.items.size(), 1U);
  EXPECT_EQ(result.items.front().count, 3);
  EXPECT_EQ(result.note, original.note);
  EXPECT_EQ(result.status, original.status);
  EXPECT_EQ(result.codes, original.codes);
  const auto aliased = xml::deserialize<xml_fixture::order>(
      "<order xmlns='urn:serializer:order' id='8'><customer>A</customer>"
      "<status>open</status><code>1</code><code>2</code></order>");
  EXPECT_EQ(aliased.id, 8U);
  EXPECT_FALSE(aliased.note);
}

TEST(xml, rejects_wrong_shape_cardinality_scalar_and_trailing_input) {
  const std::string_view documents[]{
      "<order id='7'><customer>A</customer></order>",
      "<order xmlns='urn:serializer:order'><customer>A</customer><status>open</status><code>1</code><code>2</code></order>",
      "<order xmlns='urn:serializer:order' id='-1'><customer>A</customer><status>open</status><code>1</code><code>2</code></order>",
      "<order xmlns='urn:serializer:order' id='7' extra='x'><customer>A</customer><status>open</status><code>1</code><code>2</code></order>",
      "<order xmlns='urn:serializer:order' id='7'><customer>A</customer><status>invalid</status><code>1</code><code>2</code></order>",
      "<order xmlns='urn:serializer:order' id='7'><customer>A</customer><status>open</status><code>1</code></order>",
      "<order xmlns='urn:serializer:order' id='7'><status>open</status><customer>A</customer><code>1</code><code>2</code></order>",
      "<order xmlns='urn:serializer:order' id='7'><customer>A</customer><status>open</status><code>1</code><code>2</code><code>3</code></order>",
      "<order xmlns='urn:serializer:order' id='7'><customer>A</customer><status>open</status><code>1</code><code>2</code></order><extra/>"};
  for (const auto document : documents) {
    EXPECT_THROW((void)(xml::deserialize<xml_fixture::order>(document)), std::invalid_argument) << document;
  }
}

TEST(xml, security_and_complete_document_budgets) {
  EXPECT_EQ(xml::parse("<a><![CDATA[literal <!DOCTYPE sample>]]></a>").text, "literal <!DOCTYPE sample>");
  EXPECT_THROW((void)xml::parse("<!DOCTYPE x [<!ENTITY local 'expanded'>]><x>&local;</x>"), std::invalid_argument);
  EXPECT_THROW((void)xml::parse("<!DOCTYPE x SYSTEM 'file:///not-readable'><x/>"), std::invalid_argument);
  EXPECT_THROW((void)xml::parse("<a>before<b/>after</a>"), std::invalid_argument);
  EXPECT_THROW((void)xml::parse("<a x='1' x='2'/>"), std::invalid_argument);
  EXPECT_THROW((void)xml::parse("<a><b><c/></b></a>", {.max_depth = 2U}), std::length_error);
  EXPECT_THROW((void)xml::parse("<a><b/><c/></a>", {.max_nodes = 2U}), std::length_error);
  EXPECT_THROW((void)xml::parse("<a x='123'>456</a>", {.max_text_bytes = 5U}), std::length_error);
  EXPECT_THROW((void)xml::parse("<a x='1' y='2'/>", {.max_attributes = 1U}), std::length_error);
  EXPECT_THROW((void)xml::parse("<a/>", {.max_input_bytes = 3U}), std::length_error);
  EXPECT_THROW((void)xml::parse("<?xml version='1.1'?><a/>"), std::invalid_argument);
  EXPECT_THROW((void)xml::parse("<?xml version='1.0' encoding='ISO-8859-1'?><a/>"), std::invalid_argument);
  EXPECT_NO_THROW((void)xml::parse("<?xml version='1.0' encoding='utf-8'?><a/>"));
  EXPECT_THROW((void)xml::parse("<a xmlns:p='urn:p'><b><c/></b></a>", {.max_namespace_bindings = 2U}), std::length_error);
  EXPECT_THROW((void)xml::parse("<a xmlns:p='urn:long-name'/>", {.max_name_bytes = 5U}), std::length_error);
}

TEST(xml, retained_namespace_bindings_preserve_qname_valued_metadata) {
  const auto original = xml::parse("<root xmlns:t='urn:type' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:type='t:Thing'/>");
  const auto written = xml::write(original);
  const auto reread = xml::parse(written);
  EXPECT_EQ(reread.namespace_bindings.at("t"), "urn:type");
  ASSERT_EQ(reread.attributes.size(), 1U);
  EXPECT_EQ(reread.attributes.front().value, "t:Thing");
}

TEST(xml, numeric_lexical_forms_are_exact) {
  std::int32_t integer{};
  double real{};
  std::uint32_t unsigned_value{1U};
  xml::detail::read_scalar("-000", unsigned_value);
  EXPECT_EQ(unsigned_value, 0U);
  EXPECT_THROW((void)xml::detail::read_scalar("+-1", integer), std::invalid_argument);
  EXPECT_THROW((void)xml::detail::read_scalar("++1", integer), std::invalid_argument);
  for (const auto text : {"nan", "inf", "infinity", "nan(payload)", "1e", ".", "--1", "+-1"}) {
    EXPECT_THROW((void)xml::detail::read_scalar(text, real), std::invalid_argument) << text;
  }
  xml::detail::read_scalar("+1.25e-2", real);
  EXPECT_DOUBLE_EQ(real, 0.0125);
  xml::detail::read_scalar("INF", real);
  EXPECT_EQ(real, std::numeric_limits<double>::infinity());
}

TEST(xml, generated_output_enforces_budgets_before_copying_model_strings_or_collections) {
  auto value = sample();
  EXPECT_THROW((void)xml::serialize(value, {.max_nodes = 2U}), std::length_error);
  EXPECT_THROW((void)xml::serialize(value, {.max_text_bytes = 2U}), std::length_error);
  EXPECT_THROW((void)xml::serialize(value, {.max_depth = 1U}), std::length_error);
}

TEST(xml, common_whole_object_stream_api_and_compression_are_composable) {
  const auto original = sample();
  rohit::full_stream_auto_alloc output{};
  rohit::serializer::serialize_to<xml::codec>(output, original);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  const auto result = rohit::serializer::deserialize_exact<xml_fixture::order, xml::codec>(input);
  EXPECT_EQ(result.customer, original.customer);
  EXPECT_EQ(result.codes, original.codes);
  EXPECT_TRUE(input.full());
  namespace compression = rohit::serializer::compression;
  if (compression::available(compression::format::gzip)) {
    rohit::full_stream_auto_alloc compressed{};
    rohit::serializer::serialize_to<xml::codec>(compressed, original, compression::gzip_options{});
    const auto frame = rohit::make_constant_full_stream(compressed.begin(), compressed.current_offset());
    const auto decoded = rohit::serializer::deserialize_exact<xml_fixture::order, xml::codec>(
        frame, {}, compression::decode_options{.format = compression::format::gzip});
    EXPECT_EQ(decoded.customer, original.customer);
  }
}

TEST(xml, deterministic_writer_preserves_attribute_whitespace_and_checks_characters) {
  xml::element node{};
  node.qualified_name = {"root", "urn:r"};
  node.attributes.push_back({{"value", ""}, "a\t\n\rb\"&<"});
  node.text = "\r<&>";
  const auto bytes = xml::write(node);
  EXPECT_EQ(bytes, xml::write(node));
  const auto result = xml::parse(bytes);
  ASSERT_EQ(result.attributes.size(), 1U);
  EXPECT_EQ(result.attributes.front().value, node.attributes.front().value);
  EXPECT_EQ(result.text, node.text);
  EXPECT_THROW((void)xml::write(node, {.max_output_bytes = 10U}), std::length_error);
  node.text.assign(1U, '\0');
  EXPECT_THROW((void)xml::write(node), std::invalid_argument);
  node.text = std::string{"\xc0\x80", 2U};
  EXPECT_THROW((void)xml::write(node), std::invalid_argument);
}

TEST(xml, xsd_authority_is_compiled_into_mapping_and_occurrence_checks) {
  xml_xsd_fixture::order value{};
  value.id = 9U;
  value.customer = "Customer";
  value.values = {1, 2};
  const auto bytes = xml::serialize(value);
  const auto tree = xml::parse(bytes);
  EXPECT_EQ(tree.qualified_name.namespace_uri, "urn:serializer:xsd");
  ASSERT_EQ(tree.attributes.size(), 1U);
  EXPECT_EQ(tree.attributes.front().qualified_name.local, "id");
  const auto result = xml::deserialize<xml_xsd_fixture::order>(bytes);
  EXPECT_EQ(result.values, value.values);
  value.values.clear();
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  value.values = {1, 2, 3, 4};
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  EXPECT_THROW((void)(xml::deserialize<xml_xsd_fixture::order>(
      "<order xmlns='urn:serializer:xsd' id='1'><customer>A</customer></order>")), std::invalid_argument);
}

TEST(xml, runtime_xsd_is_explicit_prepared_and_bounded) {
  constexpr std::string_view xsd{
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"
      "<xs:element name='count'><xs:simpleType><xs:restriction base='xs:int'>"
      "<xs:minInclusive value='1'/><xs:maxInclusive value='10'/>"
      "</xs:restriction></xs:simpleType></xs:element></xs:schema>"};
  const xml::prepared_schema schema{xsd};
  EXPECT_NO_THROW(schema.validate("<count>7</count>"));
  EXPECT_THROW(schema.validate("<count>11</count>"), std::invalid_argument);
  EXPECT_THROW(schema.validate("<count>7</count><count>8</count>"), std::invalid_argument);
  EXPECT_THROW(schema.validate("<count>7</count>", {.max_input_bytes = 3U}), std::length_error);
  EXPECT_THROW((void)(xml::prepared_schema{
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"
      "<xs:include schemaLocation='https://invalid.example/schema.xsd'/></xs:schema>"}), std::invalid_argument);
}

TEST(xml, generator_diagnoses_unsupported_shapes_and_retains_other_codecs) {
  rohit::serializer::writer::cpp_options options{};
  options.xml.enabled = true;
  const auto generated = generate("serializer version 1; class value { public int32 count; }", options);
  EXPECT_NE(generated.find("serialize_xml_out"), std::string::npos);
  EXPECT_NE(generated.find("serialize_out(SerializeOutProtocol"), std::string::npos);
  EXPECT_THROW(generate("serializer version 1; class value { public map (string) int32 values; }", options), std::invalid_argument);
  options.xml.classes["value"].attributes = {"missing"};
  EXPECT_THROW(generate("serializer version 1; class value { public int32 count; }", options), std::invalid_argument);
}

TEST(xml, direct_generation_validates_xml_configuration_for_every_backend) {
  namespace writer = rohit::serializer::writer;
  constexpr std::string_view source{"serializer version 1.17.0; class value { public int32 count; }"};
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input);
  const std::initializer_list<std::string_view> languages{
      "cpp", "java", "js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"};
  const auto emit = [&](std::string_view language, const writer::xml_options& xml_options) {
    if (language == "cpp") {
      writer::cpp_options options{};
      options.format = false;
      options.xml = xml_options;
      return writer::cpp::generate(statements, options);
    }
    if (language == "java") {
      writer::java_options options{};
      options.xml = xml_options;
      return writer::java::generate(statements, "Schema", options);
    }
    writer::portable_options options{};
    options.xml = xml_options;
    return writer::portable::generate(statements, language, "Schema", options);
  };
  const auto reject = [&](const writer::xml_options& options) {
    for (const auto language : languages) {
      EXPECT_THROW((void)emit(language, options), std::invalid_argument) << language;
    }
  };
  for (const auto language : languages) {
    EXPECT_NO_THROW((void)emit(language, {})) << language;
  }
  writer::xml_options options{};
  options.runtime_xsd = true;
  reject(options);
  options = {};
  options.xsd = "unused-build-input.xsd";
  reject(options);
  options = {};
  options.classes["value"].root = "root";
  reject(options);
  options = {};
  options.validation_scope = "invalid";
  reject(options);
  options.enabled = true;
  reject(options);
  options = {};
  options.enabled = true;
  options.classes["missing"].root = "root";
  reject(options);
}

TEST(xml, separate_cpp_includes_validate_class_selectors_against_the_complete_schema) {
  namespace writer = rohit::serializer::writer;
  const auto source = std::filesystem::path{__FILE__}.parent_path() / "resources/xml_includes/root.serializer";
  const auto parsed = rohit::serializer::parser::parse_file(source);
  writer::cpp_options options{};
  options.format = false;
  options.xml.enabled = true;
  options.xml.classes["xml_included::child"].attributes = {"count"};
  options.xml.classes["root"].root = "document";
  const auto files = writer::cpp::generate_files(parsed, "xml-includes.hpp", options, writer::include_mode::separate);
  ASSERT_EQ(files.size(), 2U);
  for (const auto& file : files) {
    EXPECT_NE(file.contents.find("serialize_xml_out"), std::string::npos) << file.path;
  }
}

TEST(xml, generated_cpp_api_names_are_reserved_only_when_xml_is_enabled) {
  rohit::serializer::writer::cpp_options options{};
  constexpr std::string_view sources[]{
      "serializer version 1.17.0; class value { public int32 xml_root_name; }",
      "serializer version 1.17.0; class value { private int32 number expose readonly(public) as xml_runtime_xsd; }",
      "serializer version 1.17.0; class value { public function int32 serialize_xml_out() readonly expression(1); }",
      "serializer version 1.17.0; class value { public factory serialize_xml_in() {} }",
      "serializer version 1.17.0; class target {} class value { public conversion target as xml_validate_input {}; }"};
  for (const auto source : sources) {
    options.xml.enabled = false;
    EXPECT_NO_THROW((void)generate(source, options)) << source;
    options.xml.enabled = true;
    EXPECT_THROW((void)generate(source, options), std::invalid_argument) << source;
  }
}

TEST(xml, generated_native_api_and_runtime_names_are_reserved_without_changing_disabled_output) {
  constexpr std::string_view callable{
      "serializer version 1.17.0; class value { public factory encode_xml() {} }"};
  constexpr std::string_view mobile_field{
      "serializer version 1.17.0; class value { public int32 encode_xml; }"};
  constexpr std::string_view runtime_type{
      "serializer version 1.17.0; class xml_limits { public int32 count; }"};
  for (const auto language : {"c", "swift", "kotlin"}) {
    EXPECT_NO_THROW((void)generate_portable(callable, language, false)) << language;
    EXPECT_THROW((void)generate_portable(callable, language, true), std::invalid_argument) << language;
  }
  for (const auto language : {"swift", "kotlin"}) {
    EXPECT_NO_THROW((void)generate_portable(mobile_field, language, false)) << language;
    EXPECT_THROW((void)generate_portable(mobile_field, language, true), std::invalid_argument) << language;
    EXPECT_NO_THROW((void)generate_portable(runtime_type, language, false)) << language;
    EXPECT_THROW((void)generate_portable(runtime_type, language, true), std::invalid_argument) << language;
  }
  constexpr std::string_view go_field{
      "serializer version 1.17.0; class value { public int32 EncodeXML; }"};
  EXPECT_NO_THROW((void)generate_portable(go_field, "go", false));
  EXPECT_THROW((void)generate_portable(go_field, "go", true), std::invalid_argument);
}

TEST(xml, swift_xml_provider_types_cannot_be_shadowed_by_generated_models) {
  for (const auto name : {"XMLParser", "XMLParserDelegate", "NSObject", "CharacterSet"}) {
    const auto source = std::string{"serializer version 1.17.0; class "} + name + " { public int32 count; }";
    EXPECT_NO_THROW((void)generate_portable(source, "swift", false)) << name;
    EXPECT_THROW((void)generate_portable(source, "swift", true), std::invalid_argument) << name;
  }
  constexpr std::string_view callable{
      "serializer version 1.17.0; function int32 XmlLimits() readonly expression(1); class value { public int32 count; }"};
  EXPECT_NO_THROW((void)generate_portable(callable, "swift", true));
}
