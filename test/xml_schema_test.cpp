// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_schema.hpp"

#include <rohit/stream.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace binding = rohit::serializer::writer::xml_binding;
using rohit::serializer::writer::xml_options;
// Own only uniquely created test inputs and remove them after every test, including failure paths.
class schema_directory {
  std::filesystem::path root_{};
public:
  // Reserve a new temporary directory without reusing any preexisting user path.
  schema_directory() {
    static std::atomic<unsigned> next{};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
        ("serializer-xsd-test-" + std::to_string(tick) + "-" + std::to_string(next++));
    if (!std::filesystem::create_directory(root_)) { throw std::runtime_error{"Cannot reserve XSD test directory"}; }
  }
  // Delete the exact directory reserved by this instance; never follow another computed target.
  ~schema_directory() { std::error_code ignored{}; std::filesystem::remove_all(root_, ignored); }
  // Write a fixture inside this instance's reserved directory.
  std::filesystem::path write(std::string_view filename, std::string_view content) const {
    const auto path = root_ / filename;
    std::ofstream output{path, std::ios::binary};
    output << content;
    if (!output) { throw std::runtime_error{"Cannot write XSD test input"}; }
    return path;
  }
};
// Parse one ordinary record with an exact unsigned scalar representation.
auto scalar_model() {
  constexpr std::string_view source{"serializer version 1; class order { public uint32 amount; }"};
  return rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}
// Borrow the requested top-level test record.
const rohit::serializer::class_node& record_at(
    const std::vector<std::unique_ptr<rohit::serializer::syntax_node>>& statements, std::size_t index) {
  return static_cast<const rohit::serializer::class_node&>(*statements.at(index));
}
} // namespace

TEST(xml_schema, imports_are_local_bounded_identified_and_derive_expanded_names) {
  schema_directory files{};
  files.write("types.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:types" targetNamespace="urn:types"><xs:simpleType name="Amount"><xs:restriction base="xs:unsignedInt"/></xs:simpleType><xs:element name="amount" type="t:Amount"/></xs:schema>)");
  const auto root = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:types" targetNamespace="urn:orders"><xs:import namespace="urn:types" schemaLocation="types.xsd"/><xs:element name="order"><xs:complexType><xs:sequence><xs:element ref="t:amount"/></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  xml_options options{}; options.enabled = true; options.xsd = root;
  const binding::schema schema{options};
  EXPECT_EQ(schema.dependencies().size(), 2U);
  EXPECT_EQ(schema.bundle_identity().size(), 64U);
  const auto model = scalar_model();
  const auto contract = schema.bind(record_at(model, 0U));
  EXPECT_EQ(contract.root.namespace_uri, "urn:orders");
  ASSERT_EQ(contract.fields.size(), 1U);
  EXPECT_EQ(contract.fields.front().qualified_name.namespace_uri, "urn:types");
  options.bundle_sha256 = schema.bundle_identity();
  EXPECT_NO_THROW((void)binding::schema{options});
  files.write("types.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:types" targetNamespace="urn:types"><xs:simpleType name="Amount"><xs:restriction base="xs:unsignedInt"/></xs:simpleType><xs:element name="amount" type="t:Amount"/></xs:schema> )");
  EXPECT_THROW((void)binding::schema{options}, std::invalid_argument);
}

TEST(xml_schema, includes_and_named_nested_records_keep_type_document_defaults) {
  schema_directory files{};
  files.write("types.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:orders" targetNamespace="urn:orders" elementFormDefault="qualified"><xs:complexType name="Item"><xs:sequence><xs:sequence><xs:element name="amount" type="xs:unsignedInt"/></xs:sequence></xs:sequence></xs:complexType><xs:element name="item" type="t:Item"/></xs:schema>)");
  const auto root = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:orders" targetNamespace="urn:orders"><xs:include schemaLocation="types.xsd"/><xs:element name="order"><xs:complexType><xs:sequence><xs:element name="entry" type="t:Item"/></xs:sequence></xs:complexType></xs:element></xs:schema>)");
  constexpr std::string_view source{"serializer version 1; class item { public uint32 amount; } class order { public item entry; }"};
  const auto model = rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
  xml_options options{}; options.enabled = true; options.xsd = root;
  const binding::schema schema{options};
  const auto item = schema.bind(record_at(model, 0U));
  const auto order = schema.bind(record_at(model, 1U));
  EXPECT_EQ(item.fields.front().qualified_name.namespace_uri, "urn:orders");
  EXPECT_TRUE(order.fields.front().qualified_name.namespace_uri.empty());
}

TEST(xml_schema, bundle_resolver_rejects_remote_escaping_and_namespace_mismatch_but_deduplicates_cycles) {
  schema_directory files{};
  xml_options options{}; options.enabled = true;
  for (const auto location : {"https://example.invalid/schema.xsd", "file:///schema.xsd", "../outside.xsd", "/etc/schema.xsd", "types%2exsd"}) {
    options.xsd = files.write("root.xsd", std::string{"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:include schemaLocation='"} + location + "'/></xs:schema>");
    EXPECT_THROW((void)binding::schema{options}, std::exception) << location;
  }
  files.write("types.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:wrong'/>");
  options.xsd = files.write("root.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:include schemaLocation='types.xsd'/></xs:schema>");
  EXPECT_THROW((void)binding::schema{options}, std::invalid_argument);
  options.xsd = files.write("root.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:include schemaLocation='root.xsd'/></xs:schema>");
  EXPECT_NO_THROW((void)binding::schema{options});
}

TEST(xml_schema, supported_facets_are_retained_and_identity_constraints_still_reject) {
  schema_directory files{};
  xml_options options{}; options.enabled = true;
  options.xsd = files.write("root.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:simpleType name='Amount'><xs:restriction base='xs:unsignedInt'><xs:minInclusive value='2'/></xs:restriction></xs:simpleType><xs:element name='order'><xs:complexType><xs:sequence><xs:element name='amount' type='Amount'/></xs:sequence></xs:complexType></xs:element></xs:schema>");
  const auto model = scalar_model();
  const auto contract = binding::schema{options}.bind(record_at(model, 0U));
  ASSERT_EQ(contract.fields.size(), 1U);
  EXPECT_EQ(contract.fields.front().facets.min_inclusive, "2");
  options.xsd = files.write("root.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='order'><xs:complexType/><xs:key name='identity'><xs:selector xpath='item'/><xs:field xpath='@id'/></xs:key></xs:element></xs:schema>");
  EXPECT_THROW((void)binding::schema{options}, std::invalid_argument);
}

TEST(xml_schema, closed_projection_preserves_particle_collisions_and_original_name_validation) {
  schema_directory files{};
  xml_options options{}; options.enabled = true;
  options.xsd = files.write("root.xsd", R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:test" targetNamespace="urn:test" elementFormDefault="qualified"><xs:element name="amount" type="xs:unsignedInt"/><xs:complexType name="unused"><xs:sequence><xs:element ref="t:amount" minOccurs="0"/><xs:element name="amount" type="xs:unsignedInt"/></xs:sequence></xs:complexType></xs:schema>)");
  EXPECT_THROW((void)binding::schema{options}, std::invalid_argument);
  options.xsd = files.write("root.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='1invalid' type='xs:string'/></xs:schema>");
  EXPECT_THROW((void)binding::schema{options}, std::invalid_argument);
  options.xsd = files.write("root.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' elementFormDefault='invalid'/>");
  EXPECT_THROW((void)binding::schema{options}, std::invalid_argument);
}

TEST(xml_schema, each_referencing_document_requires_its_own_namespace_import) {
  schema_directory files{};
  files.write("foreign.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:foreign'><xs:simpleType name='Amount'><xs:restriction base='xs:unsignedInt'/></xs:simpleType></xs:schema>");
  files.write("types.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' xmlns:f='urn:foreign' targetNamespace='urn:test'><xs:element name='amount' type='f:Amount'/></xs:schema>");
  xml_options options{}; options.enabled = true;
  options.xsd = files.write("root.xsd", "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:test'><xs:import namespace='urn:foreign' schemaLocation='foreign.xsd'/><xs:include schemaLocation='types.xsd'/></xs:schema>");
  EXPECT_THROW((void)binding::schema{options}, std::invalid_argument);
}

TEST(xml_schema, bundle_limits_cover_aggregate_bytes_documents_and_reference_depth) {
  schema_directory files{};
  constexpr std::string_view opening{"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"};
  constexpr std::string_view closing{"</xs:schema>"};
  constexpr std::size_t supported_document_count{64U};
  constexpr std::size_t supported_reference_depth{32U};
  constexpr std::size_t supported_bundle_bytes{16U * 1024U * 1024U};
  xml_options options{}; options.enabled = true;
  std::string root{opening};
  for (std::size_t index{}; index < supported_document_count; ++index) {
    const auto name = "part" + std::to_string(index) + ".xsd";
    files.write(name, std::string{opening} + std::string{closing});
    root += "<xs:include schemaLocation='" + name + "'/>";
  }
  options.xsd = files.write("root.xsd", root + std::string{closing});
  EXPECT_THROW((void)binding::schema{options}, std::length_error);
  for (std::size_t index{}; index <= supported_reference_depth + 1U; ++index) {
    const auto reference = index == supported_reference_depth + 1U ? std::string{} :
        "<xs:include schemaLocation='chain" + std::to_string(index + 1U) + ".xsd'/>";
    files.write("chain" + std::to_string(index) + ".xsd", std::string{opening} + reference + std::string{closing});
  }
  options.xsd = files.write("root.xsd", std::string{opening} + "<xs:include schemaLocation='chain0.xsd'/>" + std::string{closing});
  EXPECT_THROW((void)binding::schema{options}, std::length_error);
  files.write("large.xsd", std::string(supported_bundle_bytes, ' '));
  options.xsd = files.write("root.xsd", std::string{opening} + "<xs:include schemaLocation='large.xsd'/>" + std::string{closing});
  EXPECT_THROW((void)binding::schema{options}, std::length_error);
}

TEST(xml_schema, exported_contract_compiles_reimports_and_validates_independently) {
  schema_directory files{};
  const auto model = scalar_model();
  xml_options options{}; options.enabled = true;
  options.classes["order"].namespace_uri = "urn:orders";
  options.classes["order"].attributes.push_back("amount");
  const binding::schema original{options};
  const auto exported = original.export_xsd({&record_at(model, 0U)});
  const rohit::serializer::xml::prepared_schema validator{exported};
  EXPECT_NO_THROW(validator.validate("<order xmlns='urn:orders' amount='42'/>"));
  EXPECT_THROW(validator.validate("<order xmlns='urn:orders' amount='-1'/>"), std::invalid_argument);
  options.xsd = files.write("export.xsd", exported);
  const binding::schema imported{options};
  EXPECT_TRUE(binding::compare_contracts(original.bind(record_at(model, 0U)), imported.bind(record_at(model, 0U))).empty());
}

TEST(xml_schema, compatibility_reports_order_names_and_narrowed_occurrences) {
  const auto model = scalar_model();
  xml_options options{}; options.enabled = true;
  const binding::schema schema{options};
  const auto previous = schema.bind(record_at(model, 0U));
  auto current = previous;
  current.root.namespace_uri = "urn:new";
  current.fields.front().qualified_name.local = "renamed";
  current.fields.front().max_occurs = 0U;
  EXPECT_EQ(binding::compare_contracts(previous, current).size(), 3U);
}

TEST(xml_schema, exported_fixed_arrays_preserve_exact_generated_cardinality) {
  constexpr std::string_view source{"serializer version 1.7.0; class order { public array[2] uint32 amount; }"};
  const auto model = rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
  xml_options options{}; options.enabled = true;
  const binding::schema schema{options};
  const rohit::serializer::xml::prepared_schema validator{schema.export_xsd({&record_at(model, 0U)})};
  EXPECT_NO_THROW(validator.validate("<order><amount>1</amount><amount>2</amount></order>"));
  EXPECT_THROW(validator.validate("<order><amount>1</amount></order>"), std::invalid_argument);
  EXPECT_THROW(validator.validate("<order><amount>1</amount><amount>2</amount><amount>3</amount></order>"), std::invalid_argument);
}

TEST(xml_schema, text_and_wrapper_bindings_reject_conflicts_and_unknown_members) {
  const auto model = scalar_model();
  xml_options options{}; options.enabled = true;
  options.classes["order"].text = "amount";
  EXPECT_TRUE(binding::schema{options}.bind(record_at(model, 0U)).fields.front().text);
  options.classes["order"].attributes.push_back("amount");
  EXPECT_THROW((void)binding::schema{options}.bind(record_at(model, 0U)), std::invalid_argument);
  options.classes["order"].attributes.clear();
  options.classes["order"].text.clear();
  options.classes["order"].wrappers["amount"] = "amounts";
  EXPECT_THROW((void)binding::schema{options}.bind(record_at(model, 0U)), std::invalid_argument);
  options.classes["order"].wrappers.clear();
  options.classes["order"].text = "missing";
  EXPECT_THROW((void)binding::schema{options}.bind(record_at(model, 0U)), std::invalid_argument);
}
