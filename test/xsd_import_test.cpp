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
  EXPECT_NO_THROW(binding::validate_options(parsed.statements, options, "python"));
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
  EXPECT_NO_THROW(binding::validate_backend(ordinary, "python"));
  EXPECT_NO_THROW(binding::validate_backend(ordinary, "cpp"));
  ordinary.unknown.reset();
  ordinary.wildcards.emplace_back();
  EXPECT_THROW(binding::validate_backend(ordinary, "java"), std::invalid_argument);
}

TEST(xsd_import, union_members_and_inherited_pattern_groups_remain_independent) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:simpleType name="Letters"><xs:restriction base="xs:string"><xs:pattern value="[a-z]+"/><xs:pattern value="[A-Z]+"/></xs:restriction></xs:simpleType><xs:simpleType name="ShortLetters"><xs:restriction base="Letters"><xs:pattern value=".{1,4}"/></xs:restriction></xs:simpleType><xs:simpleType name="LanguageOrEmpty"><xs:union memberTypes="xs:language"><xs:simpleType><xs:restriction base="xs:string"><xs:enumeration value=""/></xs:restriction></xs:simpleType></xs:union></xs:simpleType><xs:element name="metadata"><xs:complexType><xs:attribute name="lang" type="LanguageOrEmpty"/><xs:attribute name="code" type="ShortLetters"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto contract = binding::schema{binding::effective_options(parsed.statements, {})}.bind(
      *model_classes(parsed).front());
  ASSERT_EQ(contract.fields.size(), 2U);
  const auto& language = contract.fields[0].facets;
  ASSERT_EQ(language.union_members.size(), 2U);
  EXPECT_EQ(language.union_members[0].lexical_kind, "language");
  EXPECT_EQ(language.union_members[1].enumeration, std::vector<std::string>{""});
  EXPECT_TRUE(language.enumeration.empty());
  const auto& code = contract.fields[1].facets;
  EXPECT_EQ(code.patterns, std::vector<std::string>{".{1,4}"});
  ASSERT_EQ(code.pattern_groups.size(), 1U);
  EXPECT_EQ(code.pattern_groups.front(), (std::vector<std::string>{"[a-z]+", "[A-Z]+"}));
}

TEST(xsd_import, static_content_expansion_preserves_imported_attribute_group_provenance) {
  import_directory files;
  files.write("attributes.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:attributes" attributeFormDefault="qualified"><xs:attributeGroup name="Fields"><xs:attribute name="lang" type="xs:language"/></xs:attributeGroup></xs:schema>)");
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:a="urn:attributes"><xs:import namespace="urn:attributes" schemaLocation="attributes.xsd"/><xs:complexType name="Base"><xs:sequence><xs:element name="name" type="xs:string"/></xs:sequence></xs:complexType><xs:complexType name="Derived"><xs:complexContent><xs:extension base="Base"><xs:attributeGroup ref="a:Fields"/><xs:attribute name="id" type="xs:unsignedInt"/></xs:extension></xs:complexContent></xs:complexType><xs:element name="model" type="Derived"/></xs:schema>)");
  const auto bundle = binding::read_schema_bundle(path);
  const binding::schema_bundle::document* owner{};
  const rohit::serializer::xml::element* derived{};
  for (const auto& document : bundle->documents) {
    for (const auto& node : document.root.children) {
      if (node.qualified_name.local == "complexType" && std::any_of(node.attributes.begin(),
          node.attributes.end(), [](const auto& value) { return value.value == "Derived"; })) {
        owner = &document;
        derived = &node;
      }
    }
  }
  ASSERT_NE(owner, nullptr);
  ASSERT_NE(derived, nullptr);
  const auto content = binding::expand_complex_type(*bundle, *owner, *derived);
  ASSERT_EQ(content.children.size(), 3U);
  EXPECT_EQ(content.children[0].node->qualified_name.local, "sequence");
  EXPECT_EQ(content.children[1].owner->target, "urn:attributes");
  EXPECT_EQ(content.children[1].node->qualified_name.local, "attribute");
  EXPECT_EQ(content.children[2].owner, owner);
  const auto imported = binding::import_xsd(path, {"models", "model", "model.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto contract = binding::schema{binding::effective_options(parsed.statements, {})}.bind(
      *model_classes(parsed).front());
  ASSERT_EQ(contract.fields.size(), 3U);
  EXPECT_EQ(contract.fields[1].qualified_name.namespace_uri, "urn:attributes");
}

TEST(xsd_import, official_core_1_4_imports_without_changing_its_grammar) {
  import_directory files;
  const auto path = std::filesystem::path{__FILE__}.parent_path() / "resources" / "3mf_core_1_4" / "core.xsd";
  const auto imported = binding::import_xsd(path, {"core_model", "model", path.generic_string()});
  ASSERT_EQ(imported.dependencies.size(), 2U);
  const auto parsed = rohit::serializer::parser::parse_file(files.write("core.serializer", imported.schema_text));
  const binding::schema bindings{binding::effective_options(parsed.statements, {})};
  bool metadata_found{}, resources_found{}, language_found{};
  for (const auto* owner : model_classes(parsed)) {
    const auto contract = bindings.bind(*owner);
    if (contract.root.local == "CT_Metadata") {
      metadata_found = true;
      EXPECT_TRUE(contract.fields.front().text);
    }
    if (contract.root.local == "CT_Resources") {
      resources_found = true;
      ASSERT_EQ(contract.choices.size(), 1U);
      const auto& choice = contract.choices.front();
      EXPECT_TRUE(choice.repeated);
      EXPECT_EQ(choice.minimum, 0U);
      EXPECT_EQ(choice.max_repetitions, 2147483647U);
      ASSERT_EQ(choice.branches.size(), 2U);
      EXPECT_NE(choice.branches[0].source, nullptr);
      ASSERT_TRUE(choice.branches[1].wildcard_index);
      EXPECT_EQ(contract.wildcards[*choice.branches[1].wildcard_index].before_field, choice.before_field);
    }
    if (contract.root.local == "model") {
      const auto language = std::find_if(contract.fields.begin(), contract.fields.end(), [](const auto& field) {
        return field.qualified_name.namespace_uri == "http://www.w3.org/XML/1998/namespace";
      });
      ASSERT_NE(language, contract.fields.end());
      language_found = true;
      EXPECT_EQ(language->qualified_name.local, "lang");
      EXPECT_EQ(language->facets.union_members.size(), 2U);
    }
  }
  EXPECT_TRUE(metadata_found);
  EXPECT_TRUE(resources_found);
  EXPECT_TRUE(language_found);
}

TEST(xsd_import, grouped_sequences_expand_but_ambiguous_ordered_storage_is_rejected) {
  import_directory files;
  const auto grouped = files.write("grouped.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:group name="Fields"><xs:sequence><xs:element name="value" type="xs:string"/></xs:sequence></xs:group><xs:element name="model"><xs:complexType><xs:group ref="Fields"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(grouped, {"models", {}, "grouped.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("grouped.serializer", imported.schema_text));
  EXPECT_NO_THROW((void)binding::schema{binding::effective_options(parsed.statements, {})}.bind(
      *model_classes(parsed).front()));
  const auto ambiguous = files.write("choice.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType><xs:choice maxOccurs="unbounded"><xs:element name="a" type="xs:string"/><xs:element name="b" type="xs:string"/></xs:choice></xs:complexType></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(ambiguous), std::invalid_argument);
  const auto mixed = files.write("mixed.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType mixed="true"><xs:sequence><xs:element name="value" type="xs:string"/></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(mixed), std::invalid_argument);
}

TEST(xsd_import, qualified_attributes_bind_by_expanded_name_when_local_names_collide) {
  import_directory files;
  files.write("a.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:a"><xs:attribute name="name" type="xs:string"/></xs:schema>)");
  files.write("b.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:b"><xs:attribute name="name" type="xs:string"/></xs:schema>)");
  files.write("c.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:c"><xs:attribute name="name" type="xs:string"/></xs:schema>)");
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:a="urn:a" xmlns:b="urn:b" xmlns:c="urn:c"><xs:import namespace="urn:a" schemaLocation="a.xsd"/><xs:import namespace="urn:b" schemaLocation="b.xsd"/><xs:import namespace="urn:c" schemaLocation="c.xsd"/><xs:element name="model"><xs:complexType><xs:attribute ref="a:name"/><xs:attribute ref="b:name"/><xs:attribute ref="c:name"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto contract = binding::schema{binding::effective_options(parsed.statements, {})}.bind(
      *model_classes(parsed).front());
  ASSERT_EQ(contract.fields.size(), 3U);
  EXPECT_EQ(contract.fields[0].qualified_name.namespace_uri, "urn:a");
  EXPECT_EQ(contract.fields[1].qualified_name.namespace_uri, "urn:b");
  EXPECT_EQ(contract.fields[2].qualified_name.namespace_uri, "urn:c");
}

TEST(xsd_import, boolean_metadata_and_inherited_final_defaults_remain_schema_constraints) {
  import_directory files;
  const auto mixed = files.write("mixed.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType mixed=" 1 "><xs:attribute name="name" type="xs:string"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(mixed, {"models", {}, "mixed.xsd"});
  EXPECT_NE(imported.schema_text.find("public string xsd_value"), std::string::npos);
  const auto abstract_path = files.write("abstract.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:complexType name="Abstract" abstract=" 1 "/><xs:element name="model" type="Abstract"/></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(abstract_path), std::invalid_argument);
  const auto final_path = files.write("final.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" finalDefault="#all"><xs:complexType name="Base"/><xs:complexType name="Derived"><xs:complexContent><xs:extension base="Base"/></xs:complexContent></xs:complexType><xs:element name="model" type="Derived"/></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(final_path), std::invalid_argument);
  const auto permitted = files.write("permitted.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" finalDefault="list union"><xs:complexType name="Base"/><xs:complexType name="Derived"><xs:complexContent><xs:extension base="Base"/></xs:complexContent></xs:complexType><xs:element name="model" type="Derived"/></xs:schema>)");
  EXPECT_NO_THROW((void)binding::import_xsd(permitted));
  for (const auto spelling : {"true", "false", "1", "0", " \ttrue\n ", " \t0\n "}) {
    const auto boolean_path = files.write("boolean.xsd", std::string{
        "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"><xs:element name=\"model\"><xs:complexType mixed=\""} +
        spelling + "\"/></xs:element></xs:schema>");
    EXPECT_NO_THROW((void)binding::import_xsd(boolean_path)) << spelling;
  }
  for (const auto spelling : {"yes", "2", "true false", ""}) {
    const auto boolean_path = files.write("boolean.xsd", std::string{
        "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"><xs:element name=\"model\"><xs:complexType mixed=\""} +
        spelling + "\"/></xs:element></xs:schema>");
    EXPECT_THROW((void)binding::import_xsd(boolean_path), std::invalid_argument) << spelling;
  }
}

TEST(xsd_import, provider_large_maximum_projection_keeps_exact_native_bounds) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType><xs:sequence><xs:element name="entry" type="xs:string" minOccurs="3" maxOccurs="2147483647"/></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto contract = binding::schema{binding::effective_options(parsed.statements, {})}.bind(
      *model_classes(parsed).front());
  ASSERT_EQ(contract.fields.size(), 1U);
  EXPECT_EQ(contract.fields.front().min_occurs, 3U);
  EXPECT_EQ(contract.fields.front().max_occurs, 2147483647U);
  const auto reversed = files.write("reversed.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType><xs:sequence><xs:element name="entry" type="xs:string" minOccurs="2147483647" maxOccurs="2147483646"/></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(reversed), std::invalid_argument);
  const auto malformed = files.write("malformed.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType><xs:sequence><xs:element name="entry" type="xs:string" maxOccurs="2147483647junk"/></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(malformed), std::invalid_argument);
}

TEST(xsd_import, union_alias_fanout_is_bounded_before_provider_preflight) {
  import_directory files;
  std::string source{"<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"><xs:simpleType name=\"u0\"><xs:restriction base=\"xs:string\"/></xs:simpleType>"};
  constexpr unsigned union_levels{12U};
  for (unsigned index{1U}; index <= union_levels; ++index) {
    const auto previous = "u" + std::to_string(index - 1U);
    source += "<xs:simpleType name=\"u" + std::to_string(index) + "\"><xs:union memberTypes=\"" +
        previous + ' ' + previous + "\"/></xs:simpleType>";
  }
  source += "<xs:element name=\"model\"><xs:complexType><xs:attribute name=\"value\" type=\"u" +
      std::to_string(union_levels) + "\"/></xs:complexType></xs:element></xs:schema>";
  EXPECT_THROW((void)binding::import_xsd(files.write("fanout.xsd", source)), std::length_error);
}

TEST(xsd_import, builtin_nmtokens_supports_lexical_string_and_typed_list_storage) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType><xs:attribute name="tokens" type="xs:NMTOKENS" use="required"/></xs:complexType></xs:element></xs:schema>)");
  auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"}).schema_text;
  const auto typed = rohit::serializer::parser::parse_file(files.write("typed.serializer", imported));
  const auto typed_contract = binding::schema{binding::effective_options(typed.statements, {})}.bind(
      *model_classes(typed).front());
  ASSERT_EQ(typed_contract.fields.size(), 1U);
  EXPECT_TRUE(typed_contract.fields.front().list);
  EXPECT_EQ(typed_contract.fields.front().facets.list_item_kind, "NMTOKEN");
  EXPECT_EQ(typed_contract.fields.front().facets.min_length, 1U);
  const auto declaration = imported.find("array string xsd_tokens");
  ASSERT_NE(declaration, std::string::npos);
  imported.replace(declaration, std::string_view{"array string"}.size(), "string");
  const auto list = imported.find(", list");
  ASSERT_NE(list, std::string::npos);
  imported.erase(list, std::string_view{", list"}.size());
  const auto scalar = rohit::serializer::parser::parse_file(files.write("scalar.serializer", imported));
  const auto scalar_contract = binding::schema{binding::effective_options(scalar.statements, {})}.bind(
      *model_classes(scalar).front());
  ASSERT_EQ(scalar_contract.fields.size(), 1U);
  EXPECT_FALSE(scalar_contract.fields.front().list);
  EXPECT_EQ(scalar_contract.fields.front().facets.list_item_kind, "NMTOKEN");
  EXPECT_EQ(scalar_contract.fields.front().facets.min_length, 1U);
}

TEST(xsd_import, annotation_mixed_markup_does_not_relax_structural_schema_text) {
  import_directory files;
  const auto documented = files.write("documented.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:annotation><xs:documentation>Leading <em xmlns="urn:documentation">marked</em> trailing text.</xs:documentation></xs:annotation><xs:element name="model"><xs:complexType><xs:attribute name="id" type="xs:unsignedInt"/></xs:complexType></xs:element></xs:schema>)");
  EXPECT_NO_THROW((void)binding::import_xsd(documented));
  const auto nested = files.write("nested.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:annotation><xs:documentation>Before <em xmlns="urn:documentation">marked</em> after.</xs:documentation></xs:annotation><xs:complexType><xs:annotation><xs:documentation>Nested <em xmlns="urn:documentation">markup</em> text.</xs:documentation></xs:annotation><xs:attribute name="id" type="xs:unsignedInt"/></xs:complexType></xs:element></xs:schema>)");
  EXPECT_NO_THROW((void)binding::import_xsd(nested));
  const auto bundle = binding::read_schema_bundle(nested);
  ASSERT_EQ(bundle->documents.size(), 1U);
  const auto& root_element = bundle->documents.front().root.children.front();
  const auto& documentation = root_element.children.front().children.front();
  ASSERT_EQ(documentation.children.size(), 1U);
  EXPECT_EQ(documentation.text, "Before  after.");
  ASSERT_EQ(documentation.text_segments.size(), 2U);
  EXPECT_EQ(documentation.text_segments.back().before_child, 1U);
  const auto mixed_grammar = files.write("mixed-grammar.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType>not schema grammar<xs:attribute name="id" type="xs:unsignedInt"/></xs:complexType></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(mixed_grammar), std::invalid_argument);
  const auto scalar_grammar = files.write("scalar-grammar.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType>not schema grammar</xs:complexType></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::import_xsd(scalar_grammar), std::invalid_argument);
}

TEST(xsd_import, global_root_namespace_is_independent_of_its_imported_complex_type) {
  import_directory files;
  files.write("types.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:types"><xs:complexType name="Body"><xs:attribute name="id" type="xs:unsignedInt" use="required"/></xs:complexType></xs:schema>)");
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:types" targetNamespace="urn:document"><xs:import namespace="urn:types" schemaLocation="types.xsd"/><xs:element name="document" type="t:Body"/></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", "document", "model.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto contract = binding::schema{binding::effective_options(parsed.statements, {})}.bind(
      *model_classes(parsed).front());
  EXPECT_EQ(contract.root.local, "document");
  EXPECT_EQ(contract.root.namespace_uri, "urn:document");
}

TEST(xsd_import, inherited_and_grouped_wildcards_keep_their_defining_target_namespace) {
  import_directory files;
  files.write("base.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:b="urn:base" targetNamespace="urn:base"><xs:attributeGroup name="Extra"><xs:anyAttribute namespace="##other" processContents="lax"/></xs:attributeGroup><xs:group name="Tail"><xs:sequence><xs:any namespace="##targetNamespace" processContents="lax" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:group><xs:complexType name="Base"><xs:attribute name="id" type="xs:unsignedInt" use="required"/><xs:attributeGroup ref="b:Extra"/></xs:complexType></xs:schema>)");
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:b="urn:base" xmlns:d="urn:document" targetNamespace="urn:document"><xs:import namespace="urn:base" schemaLocation="base.xsd"/><xs:complexType name="Derived"><xs:complexContent><xs:extension base="b:Base"><xs:group ref="b:Tail"/></xs:extension></xs:complexContent></xs:complexType><xs:element name="document" type="d:Derived"/></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", "document", "model.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto contract = binding::schema{binding::effective_options(parsed.statements, {})}.bind(
      *model_classes(parsed).front());
  EXPECT_EQ(contract.root.namespace_uri, "urn:document");
  ASSERT_EQ(contract.wildcards.size(), 2U);
  EXPECT_TRUE(contract.wildcards[0].attribute);
  EXPECT_EQ(contract.wildcards[0].namespace_rule, "##other");
  EXPECT_EQ(contract.wildcards[0].target_namespace, "urn:base");
  EXPECT_FALSE(contract.wildcards[1].attribute);
  EXPECT_EQ(contract.wildcards[1].namespace_rule, "##targetNamespace");
  EXPECT_EQ(contract.wildcards[1].target_namespace, "urn:base");
}

TEST(xsd_import, outer_union_value_constraints_reject_without_selected_member_identity) {
  import_directory files;
  for (const auto members : {"xs:int xs:string", "xs:string xs:int"}) {
    const auto prefix = std::string{"<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"><xs:simpleType name=\"Value\"><xs:union memberTypes=\""} +
        members + "\"/></xs:simpleType>";
    const auto enumeration_path = files.write("enumeration.xsd", prefix +
        "<xs:simpleType name=\"Restricted\"><xs:restriction base=\"Value\"><xs:enumeration value=\"01\"/></xs:restriction></xs:simpleType><xs:element name=\"model\"><xs:complexType><xs:attribute name=\"value\" type=\"Restricted\"/></xs:complexType></xs:element></xs:schema>");
    const auto enumeration = binding::import_xsd(enumeration_path, {"models", {}, "enumeration.xsd"});
    const auto enumeration_parsed = rohit::serializer::parser::parse_file(files.write("enumeration.serializer", enumeration.schema_text));
    const auto enumeration_options = binding::effective_options(enumeration_parsed.statements, {});
    const auto enumeration_contract = binding::schema{enumeration_options}.bind(*model_classes(enumeration_parsed).front());
    const auto fixed_path = files.write("fixed.xsd", prefix +
        "<xs:element name=\"model\"><xs:complexType><xs:attribute name=\"value\" type=\"Value\" fixed=\"01\"/></xs:complexType></xs:element></xs:schema>");
    const auto fixed = binding::import_xsd(fixed_path, {"models", {}, "fixed.xsd"});
    const auto fixed_parsed = rohit::serializer::parser::parse_file(files.write("fixed.serializer", fixed.schema_text));
    const auto fixed_options = binding::effective_options(fixed_parsed.statements, {});
    const auto fixed_contract = binding::schema{fixed_options}.bind(*model_classes(fixed_parsed).front());
    const auto plain_path = files.write("plain.xsd", prefix +
        "<xs:element name=\"model\"><xs:complexType><xs:attribute name=\"value\" type=\"Value\"/></xs:complexType></xs:element></xs:schema>");
    const auto plain = binding::import_xsd(plain_path, {"models", {}, "plain.xsd"});
    const auto plain_parsed = rohit::serializer::parser::parse_file(files.write("plain.serializer", plain.schema_text));
    const auto plain_options = binding::effective_options(plain_parsed.statements, {});
    for (const auto language : {"cpp", "python"}) {
      EXPECT_THROW(binding::validate_backend(enumeration_contract, language), std::invalid_argument) << members;
      EXPECT_THROW(binding::validate_backend(fixed_contract, language), std::invalid_argument) << members;
      EXPECT_NO_THROW(binding::validate_options(plain_parsed.statements, plain_options, language)) << members;
    }
  }
}

// QName list enumeration values need schema-scope item identities even inside a union member.
TEST(xsd_import, nested_qname_list_enumeration_rejects_without_expanded_item_identities) {
  import_directory files;
  const auto path = files.write("model.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:q="urn:allowed"><xs:simpleType name="Names"><xs:list itemType="xs:QName"/></xs:simpleType><xs:simpleType name="Allowed"><xs:restriction base="Names"><xs:enumeration value="q:item"/></xs:restriction></xs:simpleType><xs:simpleType name="Value"><xs:union memberTypes="Allowed xs:int"/></xs:simpleType><xs:element name="model"><xs:complexType><xs:attribute name="value" type="Value"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
  const auto options = binding::effective_options(parsed.statements, {});
  const auto contract = binding::schema{options}.bind(*model_classes(parsed).front());
  for (const auto language : {"cpp", "python"}) {
    EXPECT_THROW(binding::validate_backend(contract, language), std::invalid_argument);
  }
}

// Retain legacy QName child generation while rejecting newly added union-child scope loss.
TEST(xsd_import, cpp_union_qname_children_reject_without_changing_legacy_qname_generation) {
  import_directory files;
  for (const auto type : {"xs:QName", "Value"}) {
    const auto schema = std::string{
        "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"><xs:simpleType name=\"Value\"><xs:union memberTypes=\"xs:QName xs:int\"/></xs:simpleType><xs:element name=\"model\"><xs:complexType><xs:sequence><xs:element name=\"value\" type=\""} +
        type + "\"/></xs:sequence></xs:complexType></xs:element></xs:schema>";
    const auto path = files.write("child.xsd", schema);
    const auto imported = binding::import_xsd(path, {"models", {}, "child.xsd"});
    const auto parsed = rohit::serializer::parser::parse_file(files.write("child.serializer", imported.schema_text));
    const auto options = binding::effective_options(parsed.statements, {});
    const auto contract = binding::schema{options}.bind(*model_classes(parsed).front());
    if (std::string_view{type} == "xs:QName") {
      EXPECT_NO_THROW(binding::validate_backend(contract, "cpp"));
    } else {
      EXPECT_THROW(binding::validate_backend(contract, "cpp"), std::invalid_argument);
    }
  }
  const auto path = files.write("attribute.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="model"><xs:complexType><xs:attribute name="value" type="xs:QName"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", {}, "attribute.xsd"});
  const auto parsed = rohit::serializer::parser::parse_file(files.write("attribute.serializer", imported.schema_text));
  const auto options = binding::effective_options(parsed.statements, {});
  EXPECT_NO_THROW(binding::validate_options(parsed.statements, options, "cpp"));
}

// Union defaults require selected schema-scope QName identities; ordinary QName defaults retain them.
TEST(xsd_import, qname_union_defaults_reject_without_affecting_ordinary_qname_defaults) {
  import_directory files;
  for (const auto type : {"Value", "xs:QName"}) {
    const auto source = std::string{
        "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:q=\"urn:allowed\"><xs:simpleType name=\"Value\"><xs:union memberTypes=\"xs:QName xs:int\"/></xs:simpleType><xs:element name=\"model\"><xs:complexType><xs:attribute name=\"value\" type=\""} +
        type + "\" default=\"q:item\"/></xs:complexType></xs:element></xs:schema>";
    const auto path = files.write("model.xsd", source);
    const auto imported = binding::import_xsd(path, {"models", {}, "model.xsd"});
    const auto parsed = rohit::serializer::parser::parse_file(files.write("model.serializer", imported.schema_text));
    const auto options = binding::effective_options(parsed.statements, {});
    const auto contract = binding::schema{options}.bind(*model_classes(parsed).front());
    ASSERT_EQ(contract.fields.size(), 1U);
    if (std::string_view{type} == "xs:QName") {
      ASSERT_TRUE(contract.fields.front().qname_default);
      EXPECT_EQ(contract.fields.front().qname_default->namespace_uri, "urn:allowed");
      for (const auto language : {"cpp", "python"}) {
        EXPECT_NO_THROW(binding::validate_backend(contract, language));
      }
    } else {
      for (const auto language : {"cpp", "python"}) {
        EXPECT_THROW(binding::validate_backend(contract, language), std::invalid_argument);
      }
    }
  }
}
