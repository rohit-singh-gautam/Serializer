// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xsd_import.hpp"
#include "../src/xml_schema.hpp"

#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace binding = rohit::serializer::writer::xml_binding;
// Retain only this test's exclusively reserved files beneath the operating system temporary root.
class import_directory {
  std::filesystem::path root_;
public:
  // Reserve a unique fixture directory, never reuse a user-owned existing directory.
  import_directory() {
    static std::atomic<unsigned> sequence{};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
        ("serializer-xsd-import-" + std::to_string(tick) + '-' + std::to_string(sequence++));
    if (!std::filesystem::create_directory(root_)) { throw std::runtime_error{"Cannot create XSD import fixture"}; }
  }
  // Delete the exact directory reserved by this instance after all streams have closed.
  ~import_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(root_, ignored);
  }
  // Store one local bundle fixture and return its absolute dependency path.
  std::filesystem::path write(std::string_view name, std::string_view bytes) const {
    const auto path = root_ / name;
    std::ofstream output{path, std::ios::binary};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) { throw std::runtime_error{"Cannot write XSD import fixture"}; }
    return path;
  }
};
// Find generated records across reopened namespace blocks and ignore non-record statements.
std::vector<const rohit::serializer::class_node*> model_classes(
    const rohit::serializer::parser::parsed_schema& parsed) {
  std::vector<const rohit::serializer::class_node*> result;
  const auto visit = [&](auto&& self, const auto& nodes) -> void {
    for (const auto& node : nodes) {
      if (node->type == rohit::serializer::object_type::namespace_type) {
        self(self, static_cast<const rohit::serializer::namespace_node&>(*node).statements);
      } else if (node->type == rohit::serializer::object_type::class_type && node->name.starts_with("xsd_")) {
        result.push_back(&static_cast<const rohit::serializer::class_node&>(*node));
      }
    }
  };
  visit(visit, parsed.statements);
  if (result.empty()) { throw std::runtime_error{"Imported records are missing"}; }
  return result;
}
} // namespace

TEST(xsd_import, emits_parseable_stable_id_source_with_inline_mapping_and_pinned_dependency) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:import" targetNamespace="urn:import" elementFormDefault="qualified"><xs:complexType name="Item"><xs:attribute name="id" type="xs:unsignedInt" use="required"/></xs:complexType><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="item" type="t:Item" maxOccurs="unbounded"/></xs:sequence><xs:attribute name="label" type="xs:string"/></xs:complexType></xs:element></xs:schema>)");
  const auto result = binding::import_xsd(path, {"models", "order", "model.xsd"});
  EXPECT_NE(result.schema_text.find("serializer version 1.20.0;"), std::string::npos);
  EXPECT_NE(result.schema_text.find("using schema \"model.xsd\";"), std::string::npos);
  EXPECT_NE(result.schema_text.find("stable_ids"), std::string::npos);
  EXPECT_NE(result.schema_text.find("xml(attribute, name(\"id\")"), std::string::npos);
  EXPECT_EQ(result.bundle_identity.size(), 64U);
  ASSERT_EQ(result.dependencies.size(), 1U);
  const auto source = files.write("model.serializer", result.schema_text);
  const auto parsed = rohit::serializer::parser::parse_file(source);
  auto options = binding::effective_options(parsed.statements, {});
  EXPECT_TRUE(options.enabled);
  binding::validate_options(parsed.statements, options);
  for (const auto language : {"cpp", "java", "javascript", "typescript", "go", "csharp",
                              "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_NO_THROW(binding::validate_options(parsed.statements, options, language)) << language;
  }
  const binding::schema contract{options};
  for (const auto* owner : model_classes(parsed)) {
    EXPECT_NO_THROW((void)contract.bind(*owner));
  }
  EXPECT_EQ(contract.dependencies().size(), 1U);
}

TEST(xsd_import, singleton_choices_defaults_and_scalar_facets_survive_import) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:simpleType name="Amount"><xs:restriction base="xs:unsignedInt"><xs:minInclusive value="2"/><xs:maxInclusive value="7"/></xs:restriction></xs:simpleType><xs:element name="order"><xs:complexType><xs:choice><xs:element name="amount" type="Amount"/><xs:element name="description" type="xs:string"/></xs:choice><xs:attribute name="unit" type="xs:string" default="millimeter"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"});
  const auto source = files.write("model.serializer", imported.schema_text);
  const auto parsed = rohit::serializer::parser::parse_file(source);
  const auto options = binding::effective_options(parsed.statements, {});
  const auto& owner = *model_classes(parsed).front();
  const auto contract = binding::schema{options}.bind(owner);
  EXPECT_THROW(binding::validate_options(parsed.statements, options, "python"), std::invalid_argument);
  ASSERT_EQ(contract.choices.size(), 1U);
  EXPECT_EQ(contract.choices.front().alternatives.size(), 2U);
  ASSERT_EQ(contract.fields.size(), 3U);
  EXPECT_EQ(contract.fields[0].facets.min_inclusive, "2");
  EXPECT_EQ(contract.fields[0].facets.max_inclusive, "7");
  EXPECT_EQ(contract.fields[2].default_value, "millimeter");
}

TEST(xsd_import, simple_text_and_foreign_wildcard_contracts_are_retained) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="metadata"><xs:complexType><xs:simpleContent><xs:extension base="xs:string"><xs:attribute name="name" type="xs:QName" use="required"/><xs:anyAttribute namespace="##other" processContents="lax"/></xs:extension></xs:simpleContent></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto options = binding::effective_options(parsed.statements, {});
  const auto& owner = *model_classes(parsed).front();
  const auto contract = binding::schema{options}.bind(owner);
  ASSERT_EQ(contract.fields.size(), 2U);
  EXPECT_TRUE(contract.fields.front().text);
  EXPECT_EQ(contract.fields.back().facets.lexical_kind, "QName");
  ASSERT_EQ(contract.wildcards.size(), 1U);
  EXPECT_TRUE(contract.wildcards.front().attribute);
  EXPECT_EQ(contract.wildcards.front().namespace_rule, "##other");
}

TEST(xsd_import, unsupported_recursive_and_remote_contracts_fail_without_output) {
  import_directory files;
  const auto remote = files.write("remote.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:include schemaLocation="https://invalid.example/types.xsd"/></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(remote), std::invalid_argument);
  const auto recursive = files.write("recursive.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:complexType name="Node"><xs:sequence><xs:element name="child" type="Node" minOccurs="0"/></xs:sequence></xs:complexType><xs:element name="root" type="Node"/></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(recursive), std::invalid_argument);
}

TEST(xsd_import, fixed_lexical_lists_import_as_typed_arrays_without_repeated_xml_occurrences) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:simpleType name="Transform"><xs:restriction><xs:simpleType><xs:list itemType="xs:double"/></xs:simpleType><xs:length value="12"/></xs:restriction></xs:simpleType><xs:element name="item"><xs:complexType><xs:attribute name="transform" type="Transform" use="required"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"});
  EXPECT_NE(imported.schema_text.find("array[12] double"), std::string::npos);
  EXPECT_NE(imported.schema_text.find("xml(attribute, list,"), std::string::npos);
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto options = binding::effective_options(parsed.statements, {});
  const auto& owner = *model_classes(parsed).front();
  const auto contract = binding::schema{options}.bind(owner);
  ASSERT_EQ(contract.fields.size(), 1U);
  EXPECT_TRUE(contract.fields.front().list);
  EXPECT_FALSE(contract.fields.front().repeated);
  EXPECT_EQ(contract.fields.front().facets.length, 12U);
  EXPECT_EQ(contract.fields.front().facets.list_item_kind, "double");
}

TEST(xsd_import, adding_an_earlier_field_does_not_renumber_existing_native_identities) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType><xs:attribute name="id" type="xs:unsignedInt" use="required"/></xs:complexType></xs:element></xs:schema>)");
  const auto first = binding::import_xsd(path).schema_text;
  files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType><xs:attribute name="added" type="xs:string"/><xs:attribute name="id" type="xs:unsignedInt" use="required"/></xs:complexType></xs:element></xs:schema>)");
  const auto second = binding::import_xsd(path).schema_text;
  const auto begin = first.find("public uint32 xsd_id (");
  ASSERT_NE(begin, std::string::npos);
  const auto end = first.find('\n', begin);
  EXPECT_NE(second.find(first.substr(begin, end - begin)), std::string::npos);
}

TEST(xsd_import, backend_guards_allow_existing_primitives_but_reject_new_semantics) {
  binding::record ordinary;
  EXPECT_NO_THROW(binding::validate_backend(ordinary, "python"));
  ordinary.unknown = rohit::serializer::xml_unknown_policy::preserve;
  EXPECT_THROW(binding::validate_backend(ordinary, "python"), std::invalid_argument);
  EXPECT_NO_THROW(binding::validate_backend(ordinary, "cpp"));
  ordinary.unknown.reset();
  ordinary.wildcards.emplace_back();
  EXPECT_THROW(binding::validate_backend(ordinary, "java"), std::invalid_argument);
}
