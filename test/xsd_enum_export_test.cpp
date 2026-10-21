// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_schema.hpp"
#include "model.hpp"
#include "constraints.hpp"

#include <rohit/stream.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace binding = rohit::serializer::writer::xml_binding;
namespace codec = rohit::serializer;
using codec::writer::xml_options;

// Own uniquely reserved trusted XSD inputs and clean only that directory after each test.
class enum_schema_directory {
  std::filesystem::path root_{};
public:
  // Create a private directory without adopting any preexisting path.
  enum_schema_directory() {
    static std::atomic<unsigned> next{};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
        ("serializer-enum-export-" + std::to_string(tick) + "-" + std::to_string(next++));
    if (!std::filesystem::create_directory(root_)) { throw std::runtime_error{"Cannot reserve enum fixture directory"}; }
  }
  // Release the exact private directory; cleanup failures do not mask test failures.
  ~enum_schema_directory() { std::error_code ignored{}; std::filesystem::remove_all(root_, ignored); }
  // Write one trusted fixed schema within the private directory.
  std::filesystem::path write(std::string_view bytes) const {
    const auto path = root_ / "fixed.xsd";
    std::ofstream output{path, std::ios::binary};
    output << bytes;
    if (!output) { throw std::runtime_error{"Cannot write enum fixture schema"}; }
    return path;
  }
};

// Borrow all ordinary classes in a parsed enum fixture, preserving namespace ownership.
void collect_records(const std::vector<std::unique_ptr<codec::syntax_node>>& statements,
    std::vector<const codec::class_node*>& records) {
  for (const auto& node : statements) {
    if (node->type == codec::object_type::namespace_type) {
      collect_records(static_cast<const codec::namespace_node&>(*node).statements, records);
    } else if (node->type == codec::object_type::class_type) {
      records.push_back(static_cast<const codec::class_node*>(node.get()));
    }
  }
}

// Export one fixture through the real compiler contract while the parsed AST remains alive.
std::string export_model(std::string_view source, xml_options options = {}) {
  const auto statements = codec::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
  std::vector<const codec::class_node*> records;
  collect_records(statements, records);
  options.enabled = true;
  return binding::schema{options}.export_xsd(records);
}

// Build a string-enum input contract with independently selected attribute constraints.
std::string attribute_schema(std::string_view scalar, std::string_view attributes) {
  return "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"
      "<xs:element name='entry'><xs:complexType><xs:attribute name='value' " +
      std::string{attributes} + "><xs:simpleType><xs:restriction base='xs:string'>" +
      std::string{scalar} + "</xs:restriction></xs:simpleType></xs:attribute>"
      "</xs:complexType></xs:element></xs:schema>";
}
} // namespace

TEST(xsd_enum_export, actual_generated_enum_codecs_retain_wire_names_and_native_values) {
  constexpr std::string_view source{
      "serializer version 1.20.0; namespace tooling { xml(namespace(\"urn:tooling:enum\")); "
      "enum state { open, in_progress, closed } class entry stable_ids { "
      "public state value (37) {state::open}; public optional<state> selection (11) xml(attribute); "
      "public array state trail (83); public function bool is_open() readonly expression(value == state::open); }}"};
  const codec::xml::prepared_schema validator{export_model(source)};
  tooling::entry value{};
  EXPECT_TRUE(value.is_open());
  value.value = tooling::state::in_progress;
  value.selection = tooling::state::closed;
  value.trail = {tooling::state::open, tooling::state::closed};
  EXPECT_FALSE(value.is_open());
  const auto document = codec::xml::serialize(value);
  EXPECT_NO_THROW(validator.validate(document));
  EXPECT_NE(document.find("in_progress"), std::string::npos);
  EXPECT_EQ(document.find("InProgress"), std::string::npos);
  const auto copy = codec::xml::deserialize<tooling::entry>(document);
  EXPECT_EQ(copy.value, value.value);
  EXPECT_EQ(copy.selection, value.selection);
  EXPECT_EQ(copy.trail, value.trail);
  for (const auto name : {"InProgress", "1", "unknown", ""}) {
    const auto invalid = "<entry xmlns='urn:tooling:enum'><value>" + std::string{name} + "</value></entry>";
    EXPECT_THROW(validator.validate(invalid), std::invalid_argument);
    EXPECT_THROW((void)codec::xml::deserialize<tooling::entry>(invalid), std::invalid_argument);
  }
  value.value = static_cast<tooling::state>(99);
  EXPECT_THROW((void)codec::xml::serialize(value), std::invalid_argument);
}

TEST(xsd_enum_export, existing_scalar_restrictions_intersect_the_declared_wire_inventory) {
  enum_schema_directory files{};
  constexpr std::string_view source{
      "serializer version 1.20.0; enum state { open, in_progress, closed } "
      "class entry stable_ids { public state value (37) {state::open}; }"};
  xml_options options{};
  options.xsd = files.write(attribute_schema(
      "<xs:enumeration value='closed'/><xs:enumeration value='unknown'/>", "default='closed'"));
  const auto exported = export_model(source, options);
  EXPECT_NE(exported.find("value=\"closed\""), std::string::npos);
  EXPECT_EQ(exported.find("value=\"unknown\""), std::string::npos);
  const codec::xml::prepared_schema validator{exported};
  EXPECT_NO_THROW(validator.validate("<entry/>"));
  EXPECT_NO_THROW(validator.validate("<entry value='closed'/>"));
  EXPECT_THROW(validator.validate("<entry value='open'/>"), std::invalid_argument);
  EXPECT_THROW(validator.validate("<entry value='unknown'/>"), std::invalid_argument);
  options.xsd = files.write(attribute_schema("<xs:enumeration value='unknown'/>", ""));
  EXPECT_THROW((void)export_model(source, options), std::invalid_argument);
  options.xsd = files.write(attribute_schema("", "default='unknown'"));
  EXPECT_THROW((void)export_model(source, options), std::invalid_argument);
}

TEST(xsd_enum_export, generated_default_fixed_and_text_values_preserve_native_defaults) {
  EXPECT_EQ(enum_constraints::default_entry{}.value, enum_constraints::state::open);
  EXPECT_EQ(enum_constraints::fixed_entry{}.value, enum_constraints::state::closed);
  EXPECT_EQ(enum_constraints::text_entry{}.value, enum_constraints::state::open);
  const auto missing = codec::xml::deserialize<enum_constraints::default_entry>(
      "<default_entry xmlns='urn:tooling:enum-constraints'/>");
  EXPECT_EQ(missing.value, enum_constraints::state::in_progress);
  const auto fixed = codec::xml::deserialize<enum_constraints::fixed_entry>(
      "<fixed_entry xmlns='urn:tooling:enum-constraints'/>");
  EXPECT_EQ(fixed.value, enum_constraints::state::closed);
  const auto text = codec::xml::deserialize<enum_constraints::text_entry>(
      "<text_entry xmlns='urn:tooling:enum-constraints'/>");
  EXPECT_EQ(text.value, enum_constraints::state::in_progress);
  const auto nonempty = codec::xml::deserialize<enum_constraints::text_entry>(
      "<text_entry xmlns='urn:tooling:enum-constraints'>closed</text_entry>");
  EXPECT_EQ(nonempty.value, enum_constraints::state::closed);
  const auto fixed_text = codec::xml::deserialize<enum_constraints::fixed_text_entry>(
      "<fixed_text_entry xmlns='urn:tooling:enum-constraints'/>");
  EXPECT_EQ(fixed_text.value, enum_constraints::state::closed);
  EXPECT_THROW((void)codec::xml::deserialize<enum_constraints::fixed_text_entry>(
      "<fixed_text_entry xmlns='urn:tooling:enum-constraints'>open</fixed_text_entry>"), std::invalid_argument);
  auto invalid_text = enum_constraints::fixed_text_entry{};
  invalid_text.value = enum_constraints::state::open;
  EXPECT_THROW((void)codec::xml::serialize(invalid_text), std::invalid_argument);
  EXPECT_THROW((void)codec::xml::deserialize<enum_constraints::text_entry>(
      "<text_entry xmlns='urn:tooling:enum-constraints'><child/></text_entry>"), std::invalid_argument);
  EXPECT_THROW((void)codec::xml::deserialize<enum_constraints::fixed_entry>(
      "<fixed_entry xmlns='urn:tooling:enum-constraints' value='open'/>"), std::invalid_argument);
  auto invalid = enum_constraints::fixed_entry{};
  invalid.value = enum_constraints::state::open;
  EXPECT_THROW((void)codec::xml::serialize(invalid), std::invalid_argument);
}

TEST(xsd_enum_export, unsupported_enum_list_and_numeric_value_spaces_remain_explicit) {
  EXPECT_THROW((void)export_model(
      "serializer version 1.20.0; enum state { open, closed } class entry { "
      "public array state values xml(attribute, list); }"), std::invalid_argument);
  enum_schema_directory files{};
  xml_options options{};
  options.xsd = files.write("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"
      "<xs:element name='entry'><xs:complexType><xs:attribute name='value' type='xs:int'/>"
      "</xs:complexType></xs:element></xs:schema>");
  EXPECT_THROW((void)export_model(
      "serializer version 1.20.0; enum state { open, closed } class entry { public state value; }", options),
      std::invalid_argument);
  EXPECT_THROW((void)export_model("serializer version 1.20.0; enum state {} "
      "class entry { public state value; }"), std::invalid_argument);
  for (const auto kind : {"token", "normalizedString"}) {
    const std::string schema = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"
        "<xs:element name='entry'><xs:complexType><xs:attribute name='value' type='xs:" +
        std::string{kind} + "'/></xs:complexType></xs:element></xs:schema>";
    options.xsd = files.write(schema);
    EXPECT_THROW((void)export_model(
        "serializer version 1.20.0; enum state { open, closed } class entry { public state value; }", options),
        std::invalid_argument);
  }
}

TEST(xsd_enum_export, equivalent_exported_names_compare_without_hiding_narrower_contracts) {
  enum_schema_directory files{};
  constexpr std::string_view source{
      "serializer version 1.20.0; enum state { open, in_progress, closed } "
      "class entry stable_ids { public state value (37) {state::open}; }"};
  const auto statements = codec::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
  std::vector<const codec::class_node*> records;
  collect_records(statements, records);
  ASSERT_EQ(records.size(), 1U);
  xml_options options{};
  options.enabled = true;
  const binding::schema original{options};
  const auto before = original.bind(*records.front());
  options.xsd = files.write(original.export_xsd(records));
  const auto rebound = binding::schema{options}.bind(*records.front());
  EXPECT_TRUE(binding::compare_contracts(before, rebound).empty());
  EXPECT_TRUE(binding::compare_contracts(rebound, before).empty());
  auto narrower = rebound;
  narrower.fields.front().facets.enumeration = {"closed"};
  narrower.fields.front().compiled_facets = binding::compile_scalar_contract(narrower.fields.front().facets);
  EXPECT_FALSE(binding::compare_contracts(before, narrower).empty());
  auto patterned = rebound;
  patterned.fields.front().facets.patterns = {"[a-z_]+"};
  patterned.fields.front().compiled_facets = binding::compile_scalar_contract(patterned.fields.front().facets);
  EXPECT_FALSE(binding::compare_contracts(before, patterned).empty());
  auto fixed = rebound;
  fixed.fields.front().fixed_value = "closed";
  EXPECT_FALSE(binding::compare_contracts(before, fixed).empty());
  auto defaulted = rebound;
  defaulted.fields.front().default_value = "closed";
  EXPECT_FALSE(binding::compare_contracts(before, defaulted).empty());
}

TEST(xsd_enum_export, binding_rejects_unrepresentable_enum_constraints_before_generation) {
  enum_schema_directory files{};
  for (const bool text : {false, true}) {
    const auto source = std::string{
        "serializer version 1.20.0; enum state { open, closed } "
        "class entry stable_ids { public state value (37) {state::open}"} +
        (text ? " xml(text); }" : "; }");
    const auto statements = codec::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
    std::vector<const codec::class_node*> records;
    collect_records(statements, records);
    ASSERT_EQ(records.size(), 1U);
    for (const auto constraint : {"default", "fixed", "intersection"}) {
      SCOPED_TRACE(std::string{text ? "text-" : "attribute-"} + constraint);
      const bool intersection = std::string_view{constraint} == "intersection";
      const auto setting = intersection ? (text ? std::string{} : std::string{" use='required'"}) :
          " " + std::string{constraint} + "='unknown'";
      const auto scalar = std::string{
          "<xs:simpleType name='state'><xs:restriction base='xs:string'>"} +
          (intersection ? "<xs:enumeration value='unknown'/>" : "") +
          "</xs:restriction></xs:simpleType>";
      const auto element = text ?
          "<xs:element name='entry'" + setting + "><xs:complexType><xs:simpleContent>"
          "<xs:extension base='state'/></xs:simpleContent></xs:complexType></xs:element>" :
          "<xs:element name='entry'><xs:complexType><xs:attribute name='value' type='state'" +
          setting + "/></xs:complexType></xs:element>";
      auto document = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>" + scalar +
          element + "</xs:schema>";
      xml_options options{};
      options.enabled = true;
      options.xsd = files.write(document);
      const auto diagnostic = intersection ? "XSD native enum binding has no permitted wire names" :
          "XSD native enum default/fixed value is not a permitted wire name";
      try {
        (void)binding::schema{options}.bind(*records.front());
        FAIL() << "Unrepresentable native enum contract was bound";
      } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string_view{error.what()}, diagnostic);
      }
      // The same shape with a real native name remains supported, including narrowed XML-only defaults.
      const auto bad_name = document.find("unknown");
      ASSERT_NE(bad_name, std::string::npos);
      document.replace(bad_name, std::string_view{"unknown"}.size(), "closed");
      options.xsd = files.write(document);
      EXPECT_NO_THROW((void)binding::schema{options}.bind(*records.front()));
    }
  }
}
