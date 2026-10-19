// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_schema.hpp"
#include "../src/xsd_import.hpp"

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
#include <utility>
#include <vector>

namespace {
namespace binding = rohit::serializer::writer::xml_binding;
namespace xml = rohit::serializer::xml;
using rohit::serializer::class_node;
using rohit::serializer::writer::xml_options;

// Own exclusively reserved temporary fixtures and remove only that exact directory.
class xsd_directory {
  std::filesystem::path path_{};
public:
  // Reserve a fresh directory independently of concurrent test processes.
  xsd_directory() {
    static std::atomic<unsigned> sequence{};
    path_ = std::filesystem::temp_directory_path() / ("serializer-shared-xsd-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + '-' +
        std::to_string(sequence++));
    if (!std::filesystem::create_directory(path_)) { throw std::runtime_error{"Cannot reserve XSD fixture"}; }
  }
  // Release this fixture after all test streams have closed.
  ~xsd_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  // Persist a local fixture in the reserved directory and return its absolute path.
  std::filesystem::path write(std::string_view name, std::string_view bytes) const {
    const auto path = path_ / name;
    std::ofstream output{path, std::ios::binary};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) { throw std::runtime_error{"Cannot write XSD fixture"}; }
    return path;
  }
};
// Parse portable test declarations through the real compiler frontend.
auto parse(std::string_view source) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}
// Borrow a top-level record from test declarations.
const class_node& owner(const std::vector<std::unique_ptr<rohit::serializer::syntax_node>>& model,
                        std::size_t index = 0U) {
  return static_cast<const class_node&>(*model.at(index));
}
// Select the fixed-XSD reference mode while retaining all ordinary option validation.
xml_options fixed(const std::filesystem::path& path) {
  xml_options options;
  options.enabled = true;
  options.xsd = path;
  return options;
}
// Traverse imported namespace blocks to inspect every emitted concrete record.
std::vector<const class_node*> imported_classes(const rohit::serializer::parser::parsed_schema& parsed) {
  std::vector<const class_node*> result;
  const auto visit = [&](auto&& self, const auto& nodes) -> void {
    for (const auto& node : nodes) {
      if (node->type == rohit::serializer::object_type::namespace_type) {
        self(self, static_cast<const rohit::serializer::namespace_node&>(*node).statements);
      } else if (node->type == rohit::serializer::object_type::class_type) {
        result.push_back(&static_cast<const class_node&>(*node));
      }
    }
  };
  visit(visit, parsed.statements);
  return result;
}
} // namespace

TEST(xml_xsd_contract, simple_content_derives_text_and_authoritative_attributes) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="amount"><xs:complexType><xs:simpleContent><xs:extension base="xs:unsignedInt"><xs:attribute name="unit" type="xs:string" use="required"/></xs:extension></xs:simpleContent></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class amount { public uint32 value; public string unit; }");
  const auto contract = binding::schema{fixed(path)}.bind(owner(model));
  ASSERT_EQ(contract.fields.size(), 2U);
  EXPECT_TRUE(contract.fields[0].text);
  EXPECT_EQ(contract.fields[0].facets.lexical_kind, "unsignedInt");
  EXPECT_TRUE(contract.fields[1].attribute);
  const auto incompatible = parse("serializer version 1.20.0; class amount { public string value; public string unit; }");
  EXPECT_THROW((void)binding::schema{fixed(path)}.bind(owner(incompatible)), std::invalid_argument);
  const auto ambiguous = parse("serializer version 1.20.0; class amount { public uint32 a; public uint32 b; public string unit; }");
  EXPECT_THROW((void)binding::schema{fixed(path)}.bind(owner(ambiguous)), std::invalid_argument);
}

TEST(xml_xsd_contract, wrapper_is_derived_and_exported_without_duplicate_configuration) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="items"><xs:complexType><xs:sequence><xs:element name="item" type="xs:unsignedInt" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class order { public array uint32 items; }");
  const binding::schema source{fixed(path)};
  const auto contract = source.bind(owner(model));
  ASSERT_TRUE(contract.fields.front().wrapper);
  EXPECT_EQ(contract.fields.front().wrapper->local, "items");
  EXPECT_EQ(contract.fields.front().qualified_name.local, "item");
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order><items><item>4</item><item>5</item></items></order>")));
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order><items/></order>")));
  EXPECT_THROW(binding::validate_content_model(contract, xml::parse("<order/>")), std::invalid_argument);
  EXPECT_THROW(binding::validate_content_model(contract, xml::parse("<order><items/><items/></order>")), std::invalid_argument);
  const auto exported = source.export_xsd({&owner(model)});
  const xml::prepared_schema oracle{exported};
  EXPECT_NO_THROW(oracle.validate("<order><items><item>3</item></items></order>"));
  EXPECT_THROW(oracle.validate("<order><item>3</item></order>"), std::invalid_argument);
  const auto reimport = files.write("exported.xsd", exported);
  const auto rebound = binding::schema{fixed(reimport)}.bind(owner(model));
  EXPECT_TRUE(binding::compare_contracts(contract, rebound).empty());
  auto conflict = fixed(path);
  conflict.classes["order"].wrappers["items"] = "different";
  EXPECT_THROW((void)binding::schema{conflict}.bind(owner(model)), std::invalid_argument);
}

TEST(xml_xsd_contract, all_models_accept_permutations_and_reject_duplicates_and_missing_children) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:all><xs:element name="amount" type="xs:unsignedInt"/><xs:element name="label" type="xs:string" minOccurs="0"/></xs:all></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class order { public uint32 amount; public optional<string> label; }");
  const binding::schema schema{fixed(path)};
  const auto contract = schema.bind(owner(model));
  ASSERT_EQ(contract.all_groups.size(), 1U);
  for (const auto xml_text : {"<order><amount>3</amount><label>x</label></order>",
                             "<order><label>x</label><amount>3</amount></order>",
                             "<order><amount>3</amount></order>"}) {
    EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse(xml_text)));
  }
  for (const auto xml_text : {"<order/>", "<order><amount>1</amount><amount>2</amount></order>",
                             "<order><unknown/></order>"}) {
    EXPECT_THROW(binding::validate_content_model(contract, xml::parse(xml_text)), std::invalid_argument);
  }
  const xml::prepared_schema oracle{schema.export_xsd({&owner(model)})};
  EXPECT_NO_THROW(oracle.validate("<order><label>x</label><amount>3</amount></order>"));
  EXPECT_THROW(oracle.validate("<order><amount>3</amount><amount>4</amount></order>"), std::invalid_argument);
  EXPECT_NO_THROW(binding::validate_backend(contract, "cpp"));
}

TEST(xml_xsd_contract, optional_all_group_can_be_absent_but_present_group_must_be_complete) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:all minOccurs="0"><xs:element name="amount" type="xs:unsignedInt"/><xs:element name="label" type="xs:string"/></xs:all></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class order { public uint32 amount; public string label; }");
  const auto contract = binding::schema{fixed(path)}.bind(owner(model));
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order/>")));
  EXPECT_THROW(binding::validate_content_model(contract, xml::parse("<order><amount>3</amount></order>")), std::invalid_argument);
}

TEST(xml_xsd_contract, singleton_choice_compiles_alternatives_and_occurrence_checks) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:choice><xs:element name="amount" type="xs:unsignedInt"/><xs:element name="label" type="xs:string"/></xs:choice></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class order { public optional<uint32> amount; public optional<string> label; }");
  const auto contract = binding::schema{fixed(path)}.bind(owner(model));
  ASSERT_EQ(contract.choices.size(), 1U);
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order><amount>3</amount></order>")));
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order><label>x</label></order>")));
  EXPECT_THROW(binding::validate_content_model(contract, xml::parse("<order/>")), std::invalid_argument);
  EXPECT_THROW(binding::validate_content_model(contract, xml::parse("<order><amount>3</amount><label>x</label></order>")), std::invalid_argument);
  const auto invalid = files.write("ambiguous.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:choice><xs:element name="amount" type="xs:unsignedInt"/><xs:element name="amount" type="xs:string"/></xs:choice></xs:complexType></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::schema{fixed(invalid)}.bind(owner(model)), std::invalid_argument);
}

TEST(xml_xsd_contract, chameleon_source_adopts_two_isolated_namespace_instances) {
  xsd_directory files;
  files.write("common.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" elementFormDefault="qualified"><xs:simpleType name="Amount"><xs:restriction base="xs:unsignedInt"/></xs:simpleType><xs:complexType name="Item"><xs:sequence><xs:element name="amount" type="Amount"/></xs:sequence></xs:complexType><xs:element name="item" type="Item"/></xs:schema>)");
  files.write("left.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:left"><xs:include schemaLocation="common.xsd"/></xs:schema>)");
  files.write("right.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:right"><xs:include schemaLocation="common.xsd"/></xs:schema>)");
  const auto root = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:import namespace="urn:left" schemaLocation="left.xsd"/><xs:import namespace="urn:right" schemaLocation="right.xsd"/></xs:schema>)");
  const auto bundle = binding::read_schema_bundle(root);
  ASSERT_EQ(bundle->documents.size(), 5U);
  const auto model = parse("serializer version 1.20.0; class left { public uint32 amount; } class right { public uint32 amount; }");
  auto options = fixed(root);
  options.classes["left"].root = "item";
  options.classes["left"].namespace_uri = "urn:left";
  options.classes["right"].root = "item";
  options.classes["right"].namespace_uri = "urn:right";
  const binding::schema schema{options};
  EXPECT_EQ(schema.bind(owner(model)).fields.front().qualified_name.namespace_uri, "urn:left");
  EXPECT_EQ(schema.bind(owner(model, 1U)).fields.front().qualified_name.namespace_uri, "urn:right");
  EXPECT_EQ(schema.dependencies().size(), 4U);
  EXPECT_EQ(bundle->identity, binding::read_schema_bundle(root)->identity);
}

TEST(xml_xsd_contract, finite_cycles_deduplicate_without_rejecting_legal_forward_declarations) {
  xsd_directory files;
  const auto root = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:cycle" targetNamespace="urn:cycle"><xs:include schemaLocation="types.xsd"/><xs:element name="order" type="t:Order"/></xs:schema>)");
  files.write("types.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:cycle"><xs:include schemaLocation="root.xsd"/><xs:complexType name="Order"><xs:attribute name="amount" type="xs:unsignedInt" use="required"/></xs:complexType></xs:schema>)");
  const auto bundle = binding::read_schema_bundle(root);
  ASSERT_EQ(bundle->documents.size(), 2U);
  const auto model = parse("serializer version 1.20.0; class order { public uint32 amount; }");
  EXPECT_TRUE(binding::schema{fixed(root)}.bind(owner(model)).fields.front().attribute);
}

TEST(xml_xsd_contract, anonymous_nested_records_bind_uniquely_and_ambiguous_names_reject) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="item"><xs:complexType><xs:attribute name="amount" type="xs:unsignedInt" use="required"/></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class item { public uint32 amount; } class order { public item item; }");
  const binding::schema schema{fixed(path)};
  EXPECT_NO_THROW((void)schema.bind(owner(model)));
  EXPECT_NO_THROW((void)schema.bind(owner(model, 1U)));
  const auto ambiguous = files.write("ambiguous.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="a"><xs:complexType><xs:sequence><xs:element name="item"><xs:complexType/></xs:element></xs:sequence></xs:complexType></xs:element><xs:element name="b"><xs:complexType><xs:sequence><xs:element name="item"><xs:complexType/></xs:element></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::schema{fixed(ambiguous)}.bind(owner(model)), std::invalid_argument);
}

TEST(xml_xsd_contract, exported_text_enumerations_and_facets_have_independent_provider_checks) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:simpleType name="Label"><xs:restriction base="xs:string"><xs:minLength value="1"/><xs:maxLength value="2"/><xs:enumeration value="A"/><xs:enumeration value="B"/></xs:restriction></xs:simpleType><xs:element name="label"><xs:complexType><xs:simpleContent><xs:extension base="Label"><xs:attribute name="unit" type="xs:string"/></xs:extension></xs:simpleContent></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class label { public string value; public optional<string> unit; }");
  const binding::schema schema{fixed(path)};
  const auto exported = schema.export_xsd({&owner(model)});
  const xml::prepared_schema oracle{exported};
  EXPECT_NO_THROW(oracle.validate("<label unit='x'>A</label>"));
  EXPECT_THROW(oracle.validate("<label>C</label>"), std::invalid_argument);
  EXPECT_THROW(oracle.validate("<label/>"), std::invalid_argument);
  const auto exported_path = files.write("exported.xsd", exported);
  EXPECT_TRUE(binding::compare_contracts(schema.bind(owner(model)),
      binding::schema{fixed(exported_path)}.bind(owner(model))).empty());
}

TEST(xml_xsd_contract, exported_namespace_bundle_is_pinned_local_and_preserves_dependencies) {
  xsd_directory files;
  const auto model = parse(R"(serializer version 1.20.0; class detail { public uint32 amount; } class order { public detail item; public uint32 code xml(attribute, namespace("urn:metadata")); })");
  xml_options options;
  options.enabled = true;
  options.classes["detail"].namespace_uri = "urn:details";
  options.classes["order"].namespace_uri = "urn:orders";
  const binding::schema schema{options};
  const auto exported = schema.export_xsd_bundle({&owner(model), &owner(model, 1U)});
  ASSERT_EQ(exported.documents.size(), 3U);
  EXPECT_EQ(exported.identity.size(), 64U);
  for (const auto& document : exported.documents) {
    EXPECT_FALSE(document.path.is_absolute());
    files.write(document.path.generic_string(), document.bytes);
  }
  const auto entry = files.write(exported.documents.front().path.generic_string(), exported.documents.front().bytes);
  const auto reloaded = binding::read_schema_bundle(entry);
  EXPECT_EQ(reloaded->identity, exported.identity);
  options.xsd = entry;
  const binding::schema rebound{options};
  EXPECT_TRUE(binding::compare_contracts(schema.bind(owner(model)), rebound.bind(owner(model))).empty());
  EXPECT_TRUE(binding::compare_contracts(schema.bind(owner(model, 1U)), rebound.bind(owner(model, 1U))).empty());
  EXPECT_THROW((void)schema.export_xsd({&owner(model), &owner(model, 1U)}), std::invalid_argument);
  const auto reversed = schema.export_xsd_bundle({&owner(model, 1U), &owner(model)});
  EXPECT_EQ(reversed.identity, exported.identity);
}

TEST(xml_xsd_contract, scalar_attribute_and_nested_include_import_emit_compilable_bound_declarations) {
  xsd_directory files;
  files.write("types.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:import" targetNamespace="urn:import"><xs:simpleType name="Amount"><xs:restriction base="xs:unsignedInt"><xs:minInclusive value="2"/><xs:maxExclusive value="9"/></xs:restriction></xs:simpleType><xs:complexType name="Item"><xs:attribute name="amount" type="t:Amount" use="required"/></xs:complexType></xs:schema>)");
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:import" targetNamespace="urn:import" elementFormDefault="qualified"><xs:include schemaLocation="types.xsd"/><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="item" type="t:Item" minOccurs="0" maxOccurs="unbounded"/></xs:sequence><xs:attribute name="label" type="xs:string"/></xs:complexType></xs:element></xs:schema>)");
  const auto imported = binding::import_xsd(path, {"models", "order", "root.xsd"});
  const auto source = files.write("imported.serializer", imported.schema_text);
  const auto parsed = rohit::serializer::parser::parse_file(source);
  const auto options = binding::effective_options(parsed.statements, {});
  const auto classes = imported_classes(parsed);
  ASSERT_EQ(classes.size(), 2U);
  EXPECT_NO_THROW(binding::validate_options(parsed.statements, options, "cpp"));
  const binding::schema schema{options};
  std::size_t repeated{};
  std::size_t restricted{};
  for (const auto* value : classes) {
    const auto contract = schema.bind(*value);
    for (const auto& field : contract.fields) {
      repeated += field.repeated ? 1U : 0U;
      restricted += field.facets.min_inclusive ? 1U : 0U;
    }
  }
  EXPECT_EQ(repeated, 1U);
  EXPECT_EQ(restricted, 1U);
  EXPECT_EQ(schema.dependencies().size(), 2U);
}

TEST(xml_xsd_contract, compatibility_is_directional_for_added_retired_required_fields_and_models) {
  const auto old_model = parse("serializer version 1.20.0; class order { public uint32 amount; }");
  const auto optional_model = parse("serializer version 1.20.0; class order { public uint32 amount; public optional<string> label; }");
  const auto required_model = parse("serializer version 1.20.0; class order { public uint32 amount; public string label; }");
  xml_options options;
  options.enabled = true;
  const binding::schema schema{options};
  const auto old_contract = schema.bind(owner(old_model));
  const auto optional_contract = schema.bind(owner(optional_model));
  const auto required_contract = schema.bind(owner(required_model));
  EXPECT_TRUE(binding::compare_contracts(old_contract, optional_contract).empty());
  EXPECT_FALSE(binding::compare_contracts(optional_contract, old_contract).empty());
  EXPECT_FALSE(binding::compare_contracts(old_contract, required_contract).empty());
  auto all_contract = optional_contract;
  all_contract.all_groups.push_back({{all_contract.fields[0].source, all_contract.fields[1].source}, 1U});
  EXPECT_TRUE(binding::compare_contracts(optional_contract, all_contract).empty());
  EXPECT_FALSE(binding::compare_contracts(all_contract, optional_contract).empty());
}

TEST(xml_xsd_contract, identity_constraints_bind_export_and_validate_against_independent_schema) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="item" minOccurs="0" maxOccurs="unbounded"><xs:complexType><xs:attribute name="id" type="xs:unsignedInt" use="required"/></xs:complexType></xs:element><xs:element name="link" minOccurs="0" maxOccurs="unbounded"><xs:complexType><xs:attribute name="target" type="xs:unsignedInt" use="required"/></xs:complexType></xs:element></xs:sequence></xs:complexType><xs:key name="ids"><xs:selector xpath="item"/><xs:field xpath="@id"/></xs:key><xs:keyref name="links" refer="ids"><xs:selector xpath="link"/><xs:field xpath="@target"/></xs:keyref></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class item { public uint32 id; } class link { public uint32 target; } class order { public array item item; public array link link; }");
  const binding::schema schema{fixed(path)};
  const auto contract = schema.bind(owner(model, 2U));
  ASSERT_TRUE(contract.identity);
  ASSERT_EQ(contract.identity->constraints.size(), 2U);
  EXPECT_EQ(contract.identity->constraints.front().fields.front().lexical_kind, "unsignedInt");
  const auto good = "<order><item id='7'/><link target='007'/></order>";
  const auto duplicate = "<order><item id='7'/><item id='007'/></order>";
  const auto dangling = "<order><item id='7'/><link target='8'/></order>";
  EXPECT_NO_THROW(xml::validate_identity_scope(xml::parse(good), *contract.identity));
  EXPECT_THROW(xml::validate_identity_scope(xml::parse(duplicate), *contract.identity), std::invalid_argument);
  EXPECT_THROW(xml::validate_identity_scope(xml::parse(dangling), *contract.identity), std::invalid_argument);
  const auto exported = schema.export_xsd({&owner(model), &owner(model, 1U), &owner(model, 2U)});
  const xml::prepared_schema oracle{exported};
  EXPECT_NO_THROW(oracle.validate(good));
  EXPECT_THROW(oracle.validate(duplicate), std::invalid_argument);
  EXPECT_THROW(oracle.validate(dangling), std::invalid_argument);
  const auto rebound = binding::schema{fixed(files.write("exported.xsd", exported))}.bind(owner(model, 2U));
  EXPECT_EQ(rebound.identity, contract.identity);
  EXPECT_NO_THROW(binding::validate_backend(contract, "cpp"));
}

TEST(xml_xsd_contract, fixed_xsd_rejects_authored_placement_and_import_namespace_conflicts) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="amount" type="xs:unsignedInt"/></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse(R"(serializer version 1.20.0; class order { public uint32 amount xml(attribute); })");
  EXPECT_THROW((void)binding::schema{fixed(path)}.bind(owner(model)), std::invalid_argument);
  files.write("common.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:simpleType name="Amount"><xs:restriction base="xs:unsignedInt"/></xs:simpleType></xs:schema>)");
  files.write("left.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:left"><xs:include schemaLocation="common.xsd"/></xs:schema>)");
  const auto invalid = files.write("invalid.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:import namespace="urn:left" schemaLocation="left.xsd"/><xs:import namespace="urn:left" schemaLocation="common.xsd"/></xs:schema>)");
  EXPECT_THROW((void)binding::read_schema_bundle(invalid), std::invalid_argument);
}

TEST(xml_xsd_contract, text_only_mixed_schema_derives_one_unambiguous_string_member) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="label"><xs:complexType mixed="true"><xs:attribute name="unit" type="xs:string"/></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class label { public string value; public optional<string> unit; }");
  const auto contract = binding::schema{fixed(path)}.bind(owner(model));
  ASSERT_EQ(contract.fields.size(), 2U);
  EXPECT_TRUE(contract.fields[0].text);
  EXPECT_TRUE(contract.fields[1].attribute);
  EXPECT_EQ(contract.fields[0].facets.lexical_kind, "string");
}

TEST(xml_xsd_contract, exported_numeric_exclusive_bounds_and_unicode_lengths_keep_exact_value_spaces) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="amount"><xs:simpleType><xs:restriction base="xs:unsignedLong"><xs:minInclusive value="9007199254740992"/><xs:maxExclusive value="9007199254740994"/></xs:restriction></xs:simpleType></xs:element><xs:element name="label"><xs:simpleType><xs:restriction base="xs:string"><xs:length value="1"/><xs:enumeration value="&#x1F600;"/><xs:enumeration value="A"/></xs:restriction></xs:simpleType></xs:element></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class order { public uint64 amount; public string label; }");
  const binding::schema schema{fixed(path)};
  const auto exported = schema.export_xsd({&owner(model)});
  const xml::prepared_schema oracle{exported};
  EXPECT_NO_THROW(oracle.validate("<order><amount>9007199254740992</amount><label>&#x1F600;</label></order>"));
  EXPECT_NO_THROW(oracle.validate("<order><amount>9007199254740993</amount><label>A</label></order>"));
  EXPECT_THROW(oracle.validate("<order><amount>9007199254740994</amount><label>A</label></order>"), std::invalid_argument);
  EXPECT_THROW(oracle.validate("<order><amount>9007199254740991</amount><label>A</label></order>"), std::invalid_argument);
  EXPECT_THROW(oracle.validate("<order><amount>9007199254740992</amount><label>AA</label></order>"), std::invalid_argument);
  const auto rebound = binding::schema{fixed(files.write("exported.xsd", exported))}.bind(owner(model));
  EXPECT_TRUE(binding::compare_contracts(schema.bind(owner(model)), rebound).empty());
}

TEST(xml_xsd_contract, repeated_heterogeneous_choice_preserves_order_and_exact_group_bounds) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:choice minOccurs="2" maxOccurs="3"><xs:element name="amount" type="xs:unsignedInt"/><xs:element name="label" type="xs:string"/></xs:choice></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class order { public array uint32 amount; public array string label; }");
  const auto contract = binding::schema{fixed(path)}.bind(owner(model));
  ASSERT_TRUE(contract.particles);
  EXPECT_TRUE(contract.particles->advanced);
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order><amount>1</amount><label>x</label><amount>2</amount></order>")));
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order><label>x</label><amount>2</amount></order>")));
  EXPECT_THROW(binding::validate_content_model(contract, xml::parse("<order><label>x</label></order>")), std::invalid_argument);
  EXPECT_THROW(binding::validate_content_model(contract, xml::parse("<order><amount>1</amount><label>x</label><amount>2</amount><label>y</label></order>")), std::invalid_argument);
  EXPECT_THROW(binding::validate_backend(contract, "cpp"), std::invalid_argument);
}

TEST(xml_xsd_contract, repeated_grouped_choice_keeps_branch_pairing_and_per_branch_sequence) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:choice minOccurs="2" maxOccurs="3"><xs:sequence><xs:element name="amount" type="xs:unsignedInt"/><xs:element name="label" type="xs:string"/></xs:sequence><xs:sequence><xs:element name="code" type="xs:unsignedInt"/><xs:element name="description" type="xs:string"/></xs:sequence></xs:choice></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse("serializer version 1.20.0; class order { public array uint32 amount; public array string label; public array uint32 code; public array string description; }");
  const binding::schema schema{fixed(path)};
  const auto contract = schema.bind(owner(model));
  ASSERT_TRUE(contract.particles);
  EXPECT_TRUE(contract.particles->advanced);
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order><amount>1</amount><label>a</label><code>2</code><description>b</description></order>")));
  EXPECT_NO_THROW(binding::validate_content_model(contract, xml::parse("<order><code>2</code><description>b</description><amount>1</amount><label>a</label></order>")));
  for (const auto invalid : {
      "<order><amount>1</amount><label>a</label></order>",
      "<order><amount>1</amount><code>2</code><label>a</label><description>b</description></order>",
      "<order><amount>1</amount><label>a</label><code>2</code></order>"}) {
    EXPECT_THROW(binding::validate_content_model(contract, xml::parse(invalid)), std::invalid_argument);
  }
  binding::particle_limits limits;
  limits.max_work_units = 1U;
  const std::vector<xml::name> trace{{"amount", ""}, {"label", ""}, {"code", ""}, {"description", ""}};
  EXPECT_THROW(binding::validate_particle_trace(*contract.particles, trace, limits), std::length_error);
  xml::operation_context context;
  context.is_cancelled = [] { return true; };
  EXPECT_THROW(binding::validate_particle_trace(*contract.particles, trace, {}, &context), std::exception);
  EXPECT_THROW((void)schema.export_xsd({&owner(model)}), std::invalid_argument);
}

TEST(xml_xsd_contract, particle_compiler_diagnoses_overlapping_branches_and_nullable_boundaries) {
  for (const auto body : {
      "<xs:choice><xs:sequence><xs:element name='a' type='xs:string'/><xs:element name='b' type='xs:string'/></xs:sequence><xs:sequence><xs:element name='a' type='xs:string'/><xs:element name='c' type='xs:string'/></xs:sequence></xs:choice>",
      "<xs:sequence><xs:element name='a' type='xs:string' minOccurs='0'/><xs:element name='a' type='xs:string'/></xs:sequence>"}) {
    const auto bytes = std::string{"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:complexType name='Order'>"} + body + "</xs:complexType></xs:schema>";
    binding::schema_bundle bundle;
    bundle.documents.push_back({{}, bytes, {}, xml::parse(bytes)});
    const auto& document = bundle.documents.front();
    const auto content = binding::expand_complex_type(bundle, document, document.root.children.front());
    EXPECT_THROW((void)binding::compile_particle_contract(bundle, content.children), std::invalid_argument);
  }
}

TEST(xml_xsd_contract, repeated_and_grouped_particle_corpora_match_independent_xsd_provider) {
  xsd_directory files;
  constexpr std::size_t maximum_exhaustive_trace_names{4U};
  const std::vector<std::string_view> schemas{
      R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:choice minOccurs="2" maxOccurs="3"><xs:element name="amount" type="xs:unsignedInt"/><xs:element name="label" type="xs:string"/></xs:choice></xs:complexType></xs:element></xs:schema>)",
      R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="order"><xs:complexType><xs:choice minOccurs="2" maxOccurs="3"><xs:sequence><xs:element name="amount" type="xs:unsignedInt"/><xs:element name="label" type="xs:string"/></xs:sequence><xs:sequence><xs:element name="code" type="xs:unsignedInt"/><xs:element name="description" type="xs:string"/></xs:sequence></xs:choice></xs:complexType></xs:element></xs:schema>)"};
  const std::vector<std::string_view> models{
      "serializer version 1.20.0; class order { public array uint32 amount; public array string label; }",
      "serializer version 1.20.0; class order { public array uint32 amount; public array string label; public array uint32 code; public array string description; }"};
  const std::vector<std::vector<std::string>> alphabets{{"amount", "label"}, {"amount", "label", "code", "description"}};
  std::size_t checked{};
  std::size_t expected{};
  for (std::size_t shape{}; shape < schemas.size(); ++shape) {
    const auto path = files.write("shape_" + std::to_string(shape) + ".xsd", schemas[shape]);
    const auto model = parse(models[shape]);
    const auto contract = binding::schema{fixed(path)}.bind(owner(model));
    const xml::prepared_schema oracle{schemas[shape]};
    const auto compare = [&](const std::vector<std::string>& trace) {
      std::string text{"<order>"};
      for (const auto& name : trace) { text += '<' + name + ">2</" + name + '>'; }
      text += "</order>";
      bool provider_accepts = true;
      try { oracle.validate(text); }
      catch (const std::invalid_argument&) { provider_accepts = false; }
      bool shared_accepts = true;
      try { binding::validate_content_model(contract, xml::parse(text)); }
      catch (const std::invalid_argument&) { shared_accepts = false; }
      EXPECT_EQ(shared_accepts, provider_accepts) << text;
      ++checked;
    };
    std::vector<std::vector<std::string>> level{{}};
    for (std::size_t depth{}; depth <= maximum_exhaustive_trace_names; ++depth) {
      expected += level.size();
      for (const auto& trace : level) { compare(trace); }
      if (depth == maximum_exhaustive_trace_names) { break; }
      std::vector<std::vector<std::string>> next;
      for (const auto& trace : level) {
        for (const auto& name : alphabets[shape]) {
          auto extended = trace;
          extended.push_back(name);
          next.push_back(std::move(extended));
        }
      }
      level = std::move(next);
    }
    if (shape != 0U) {
      // The exhaustive corpus reaches two grouped repetitions. Check the exact upper
      // boundary and its first overflow without expanding the four-name alphabet further.
      compare({"amount", "label", "code", "description", "amount", "label"});
      compare({"amount", "label", "code", "description", "amount", "label", "code", "description"});
      expected += 2U;
    }
  }
  EXPECT_EQ(checked, expected);
}

TEST(xml_xsd_contract, bounded_array_of_records_is_not_inferred_as_its_records_inner_wrapper) {
  xsd_directory files;
  const auto path = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:complexType name="Item"><xs:sequence><xs:element name="value" type="xs:unsignedInt" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="item" type="Item"/></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  const auto model = parse(R"(serializer version 1.20.0; class item { xml(root("Item")); public array uint32 value; } class order { public array item item; })");
  const auto contract = binding::schema{fixed(path)}.bind(owner(model, 1U));
  ASSERT_EQ(contract.fields.size(), 1U);
  EXPECT_FALSE(contract.fields.front().wrapper);
  EXPECT_TRUE(contract.fields.front().repeated);
  EXPECT_EQ(contract.fields.front().qualified_name.local, "item");
  EXPECT_EQ(contract.fields.front().max_occurs, 1U);
}
