// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/xml_identity.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xml = rohit::serializer::xml;
namespace {
// Build one scalar record fixture without depending on an installed XML parser provider.
xml::element row(std::string_view tag, std::string_view value, std::string_view attribute = "id") {
  xml::element result{};
  result.qualified_name = {std::string{tag}, {}};
  result.attributes.push_back({{std::string{attribute}, {}}, std::string{value}});
  return result;
}
// Construct one owner-scoped key or unique projection onto a repeated row's id attribute.
xml::identity_constraint constraint(std::string_view id, std::string_view selector,
    xml::identity_kind kind = xml::identity_kind::key, std::string_view type = "string") {
  xml::identity_constraint result{};
  result.id = {std::string{id}, "urn:keys"};
  result.kind = kind;
  result.selector = xml::compile_identity_path(selector, {}, true);
  result.fields.push_back({xml::compile_identity_path("@id", {}, false), std::string{type}});
  return result;
}
// Return a normalized owner with the caller's exact child ordering.
xml::element document(std::vector<xml::element> children) {
  xml::element result{};
  result.qualified_name = {"root", {}};
  result.children = std::move(children);
  return result;
}
// Bind one local key target and a forward-reference declaration occurring before it in metadata.
xml::identity_scope referenced_scope() {
  auto reference = constraint("references", "ref", xml::identity_kind::keyref, "integer");
  reference.refer = xml::name{"ids", "urn:keys"};
  return {{"root", {}}, {reference, constraint("ids", "row", xml::identity_kind::key, "decimal")}};
}
} // namespace

// Namespace-resolved paths are prefix independent and follow XSD's empty unprefixed XPath namespace.
TEST(xml_identity, xpath_subset_resolves_expanded_names_and_rejects_other_expressions) {
  xml::element scope{};
  scope.namespace_bindings = {{"", "urn:default"}, {"a", "urn:items"}, {"b", "urn:items"}};
  const auto first = xml::compile_identity_path(".//a:row/a:item", scope, true);
  const auto second = xml::compile_identity_path(".//b:row/b:item", scope, true);
  EXPECT_EQ(first, second);
  EXPECT_TRUE(first.descendants);
  EXPECT_EQ(xml::compile_identity_path("row", scope, true).steps.front().namespace_uri, "");
  const auto field = xml::compile_identity_path("a:child/@id", scope, false);
  ASSERT_TRUE(field.attribute);
  EXPECT_TRUE(field.attribute->namespace_uri.empty());
  EXPECT_TRUE(xml::compile_identity_path(".", scope, false).steps.empty());
  for (const auto expression : {"", "/row", "row/", "row[1]", "row|item", "*", "row/@id", "row//item",
      "child::row", "text()", "..", "row/../item", "unknown:row", "row = item", "row /item"}) {
    EXPECT_THROW((void)xml::compile_identity_path(expression, scope, true), std::invalid_argument) << expression;
  }
  EXPECT_THROW((void)xml::compile_identity_path(".//row", scope, false), std::invalid_argument);
}

// Tuple equality uses exact decimal values even when spellings differ beyond binary64 precision.
TEST(xml_identity, exact_numeric_key_duplicates_and_boundaries) {
  xml::identity_scope scope{{"root", {}}, {constraint("ids", "row", xml::identity_kind::key, "decimal")}};
  EXPECT_NO_THROW(xml::validate_identity_scope(document({row("row", "9007199254740992"), row("row", "9007199254740993")}), scope));
  EXPECT_THROW(xml::validate_identity_scope(document({row("row", "+09007199254740993.00"), row("row", "9007199254740993")}), scope), std::invalid_argument);
  EXPECT_THROW(xml::validate_identity_scope(document({row("row", "-0.00"), row("row", "+000")}), scope), std::invalid_argument);
  EXPECT_NO_THROW(xml::validate_identity_scope(document({row("row", ".00000000000000000001"), row("row", ".00000000000000000002")}), scope));
}

// Required key tuples reject missing fields while unique constraints omit incomplete tuples.
TEST(xml_identity, incomplete_key_and_unique_tuple_rules) {
  auto missing = row("row", "x");
  missing.attributes.clear();
  xml::identity_scope scope{{"root", {}}, {constraint("ids", "row")}};
  EXPECT_THROW(xml::validate_identity_scope(document({missing}), scope), std::invalid_argument);
  scope.constraints.front().kind = xml::identity_kind::unique;
  EXPECT_NO_THROW(xml::validate_identity_scope(document({missing, missing}), scope));
  EXPECT_THROW(xml::validate_identity_scope(document({row("row", ""), row("row", "")}), scope), std::invalid_argument);
  scope.constraints.front().fields.push_back({xml::compile_identity_path("@other", {}, false), "string"});
  EXPECT_NO_THROW(xml::validate_identity_scope(document({row("row", "same"), row("row", "same")}), scope));
}

// Null simple fields and multiple selected field nodes retain their distinct XML identity meanings.
TEST(xml_identity, nil_simple_fields_and_multiple_selection) {
  xml::element value{};
  value.qualified_name = {"value", {}};
  value.attributes.push_back({{"nil", "http://www.w3.org/2001/XMLSchema-instance"}, "true"});
  xml::element item{};
  item.qualified_name = {"row", {}};
  item.children.push_back(value);
  auto key = constraint("ids", "row");
  key.fields.front().path = xml::compile_identity_path("value", {}, false);
  xml::identity_scope scope{{"root", {}}, {key}};
  EXPECT_THROW(xml::validate_identity_scope(document({item}), scope), std::invalid_argument);
  scope.constraints.front().kind = xml::identity_kind::unique;
  EXPECT_NO_THROW(xml::validate_identity_scope(document({item}), scope));
  item.children.push_back(value);
  EXPECT_THROW(xml::validate_identity_scope(document({item}), scope), std::invalid_argument);
  item.children.resize(1U);
  item.children.front().text = "forbidden";
  EXPECT_THROW(xml::validate_identity_scope(document({item}), scope), std::invalid_argument);
}

// References can precede their keys in the document and declaration sequence but must all resolve.
TEST(xml_identity, forward_keyrefs_resolve_before_success) {
  const auto scope = referenced_scope();
  EXPECT_NO_THROW(xml::validate_identity_scope(document({row("ref", "+001"), row("row", "1"), row("ref", "1")}), scope));
  EXPECT_THROW(xml::validate_identity_scope(document({row("ref", "2"), row("row", "1")}), scope), std::invalid_argument);
  auto missing = row("ref", "1");
  missing.attributes.clear();
  EXPECT_NO_THROW(xml::validate_identity_scope(document({missing, row("row", "1")}), scope));
}

// Wrong scopes, wrong target kinds, tuple arities and field value families reject before reading data.
TEST(xml_identity, keyref_targets_and_types_are_statically_checked) {
  auto scope = referenced_scope();
  scope.constraints.front().refer->namespace_uri = "urn:another-owner";
  EXPECT_THROW(xml::validate_identity_scope_contract(scope), std::invalid_argument);
  scope = referenced_scope();
  scope.constraints.front().fields.front().lexical_kind = "boolean";
  EXPECT_THROW(xml::validate_identity_scope_contract(scope), std::invalid_argument);
  scope = referenced_scope();
  scope.constraints.front().fields.push_back(scope.constraints.front().fields.front());
  EXPECT_THROW(xml::validate_identity_scope_contract(scope), std::invalid_argument);
  scope = referenced_scope();
  scope.constraints.front().refer = scope.constraints.front().id;
  EXPECT_THROW(xml::validate_identity_scope_contract(scope), std::invalid_argument);
  scope = referenced_scope();
  auto other = document({row("row", "1")});
  other.qualified_name.local = "nested";
  EXPECT_THROW(xml::validate_identity_scope(other, scope), std::invalid_argument);
}

// anyURI is an independent XSD primitive, while token and other string-derived types share strings.
TEST(xml_identity, uri_keyref_value_space_is_distinct_from_string) {
  auto scope = referenced_scope();
  scope.constraints.front().fields.front().lexical_kind = "anyURI";
  scope.constraints.back().fields.front().lexical_kind = "anyURI";
  EXPECT_NO_THROW(xml::validate_identity_scope_contract(scope));
  EXPECT_NO_THROW(xml::validate_identity_scope(
      document({row("ref", " urn:example:value "), row("row", "urn:example:value")}), scope));
  scope.constraints.back().fields.front().lexical_kind = "string";
  EXPECT_THROW(xml::validate_identity_scope_contract(scope), std::invalid_argument);
  scope.constraints.front().fields.front().lexical_kind = "string";
  scope.constraints.back().fields.front().lexical_kind = "anyURI";
  EXPECT_THROW(xml::validate_identity_scope_contract(scope), std::invalid_argument);
  scope.constraints.front().fields.front().lexical_kind = "token";
  scope.constraints.back().fields.front().lexical_kind = "string";
  EXPECT_NO_THROW(xml::validate_identity_scope(
      document({row("ref", " same "), row("row", "same")}), scope));
}

// QName values compare expanded names in their own field scope rather than authored prefixes.
TEST(xml_identity, qname_boolean_and_whitespace_value_spaces) {
  auto first = row("row", "a:item");
  first.namespace_bindings = {{"a", "urn:value"}};
  auto second = row("row", "b:item");
  second.namespace_bindings = {{"b", "urn:value"}};
  xml::identity_scope scope{{"root", {}}, {constraint("ids", "row", xml::identity_kind::key, "QName")}};
  EXPECT_THROW(xml::validate_identity_scope(document({first, second}), scope), std::invalid_argument);
  second.namespace_bindings["b"] = "urn:other";
  EXPECT_NO_THROW(xml::validate_identity_scope(document({first, second}), scope));
  scope.constraints.front().fields.front().lexical_kind = "boolean";
  EXPECT_THROW(xml::validate_identity_scope(document({row("row", "true"), row("row", "1")}), scope), std::invalid_argument);
  scope.constraints.front().fields.front().lexical_kind = "token";
  EXPECT_THROW(xml::validate_identity_scope(document({row("row", "two words"), row("row", " two\twords ")}), scope), std::invalid_argument);
}

// Descendant selectors cover nested owning records while ordinary child selectors stay local.
TEST(xml_identity, descendant_and_child_scope_selection) {
  xml::element nested{};
  nested.qualified_name = {"nested", {}};
  nested.children = {row("row", "same")};
  const auto input = document({row("row", "same"), nested});
  xml::identity_scope scope{{"root", {}}, {constraint("ids", "row")}};
  EXPECT_NO_THROW(xml::validate_identity_scope(input, scope));
  scope.constraints.front().selector = xml::compile_identity_path(".//row", {}, true);
  EXPECT_THROW(xml::validate_identity_scope(input, scope), std::invalid_argument);
}

// A leaf at the exact descendant depth boundary must not be charged for nonexistent child edges.
TEST(xml_identity, descendant_depth_boundary_counts_only_existing_edges) {
  xml::identity_scope scope{{"root", {}}, {constraint("ids", ".//row")}};
  xml::identity_limits limits{};
  limits.max_depth = 1U;
  EXPECT_NO_THROW(xml::validate_identity_scope(document({row("row", "id")}), scope, limits));
  auto nested = row("nested", "unused");
  nested.children.push_back(row("row", "id"));
  EXPECT_THROW(xml::validate_identity_scope(document({nested}), scope, limits), std::length_error);
}

// Cardinality, text retention, path, depth and work limits fail explicitly without partial indexes.
TEST(xml_identity, resource_limits_and_caller_failures_are_enforced) {
  const xml::identity_scope scope{{"root", {}}, {constraint("ids", "row")}};
  const auto input = document({row("row", "aa"), row("row", "bb")});
  xml::identity_limits limits{};
  limits.max_tuples = 1U;
  EXPECT_THROW(xml::validate_identity_scope(input, scope, limits), std::length_error);
  limits = {};
  limits.max_tuple_bytes = 3U;
  EXPECT_THROW(xml::validate_identity_scope(input, scope, limits), std::length_error);
  limits = {};
  limits.max_selected_nodes = 1U;
  EXPECT_THROW(xml::validate_identity_scope(input, scope, limits), std::length_error);
  limits = {};
  limits.max_work_units = 2U;
  EXPECT_THROW(xml::validate_identity_scope(input, scope, limits), std::length_error);
  limits = {};
  limits.max_xpath_bytes = 2U;
  EXPECT_THROW((void)xml::compile_identity_path("row", {}, true, limits), std::length_error);
  xml::operation_context context{};
  context.charge = [](auto, auto) { throw std::runtime_error{"caller budget"}; };
  EXPECT_THROW(xml::validate_identity_scope(input, scope, {}, &context), std::runtime_error);
}

// Static metadata checking and document traversal consume one exact local work ceiling together.
TEST(xml_identity, contract_and_document_share_one_work_ledger) {
  const xml::identity_scope scope{{"root", {}}, {constraint("ids", "row")}};
  const auto input = document({});
  std::size_t charged{};
  xml::operation_context context{};
  context.charge = [&](const auto resource, const auto amount) {
    if (resource == xml::operation_resource::work_units) { charged += amount; }
  };
  xml::validate_identity_scope_contract(scope, {}, &context);
  const auto static_work = charged;
  charged = 0U;
  xml::validate_identity_scope(input, scope, {}, &context);
  const auto total_work = charged;
  ASSERT_GT(static_work, 0U);
  ASSERT_GT(total_work, static_work + 1U);
  xml::identity_limits limits{};
  limits.max_work_units = total_work;
  EXPECT_NO_THROW(xml::validate_identity_scope(input, scope, limits));
  limits.max_work_units = total_work - 1U;
  EXPECT_THROW(xml::validate_identity_scope(input, scope, limits), std::length_error);
}

// An ultimately incomplete unique tuple cannot build an oversized temporary candidate first.
TEST(xml_identity, incomplete_candidate_tuple_storage_is_bounded) {
  auto unique = constraint("ids", "row", xml::identity_kind::unique);
  for (const auto field : {"@second", "@third", "@missing"}) {
    unique.fields.push_back({xml::compile_identity_path(field, {}, false), "string"});
  }
  const xml::identity_scope scope{{"root", {}}, {unique}};
  auto value = row("row", "aa");
  value.attributes.push_back({{"second", {}}, "bb"});
  value.attributes.push_back({{"third", {}}, "cc"});
  xml::identity_limits limits{};
  limits.max_tuple_bytes = 6U;
  EXPECT_NO_THROW(xml::validate_identity_scope(document({value}), scope, limits));
  limits.max_tuple_bytes = 5U;
  EXPECT_THROW(xml::validate_identity_scope(document({value}), scope, limits), std::length_error);
}

// Compare a complete validity corpus against an independent prepared XSD provider when available.
TEST(xml_identity, key_and_forward_reference_corpus_agrees_with_xsd_provider) {
  if (xml::available()) {
    const std::string xsd = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
      <xs:element name="root"><xs:complexType><xs:choice minOccurs="0" maxOccurs="unbounded">
        <xs:element name="row"><xs:complexType><xs:attribute name="id" type="xs:decimal" use="required"/></xs:complexType></xs:element>
        <xs:element name="ref"><xs:complexType><xs:attribute name="id" type="xs:decimal"/></xs:complexType></xs:element>
      </xs:choice></xs:complexType><xs:key name="ids"><xs:selector xpath="row"/><xs:field xpath="@id"/></xs:key>
      <xs:keyref name="references" refer="ids"><xs:selector xpath="ref"/><xs:field xpath="@id"/></xs:keyref></xs:element>
      </xs:schema>)";
    const xml::prepared_schema reference{xsd};
    auto scope = referenced_scope();
    scope.constraints.front().fields.front().lexical_kind = "decimal";
    const std::vector<std::pair<std::string, bool>> corpus{
        {"<root/>", true}, {"<root><ref id='01.0'/><row id='1'/></root>", true},
        {"<root><row id='1'/><row id='1.00'/></root>", false},
        {"<root><ref id='2'/><row id='1'/></root>", false},
        {"<root><ref/><row id='1'/></root>", true}};
    for (const auto& [bytes, expected] : corpus) {
      bool provider_valid{true}, portable_valid{true};
      try { reference.validate(bytes); } catch (const std::invalid_argument&) { provider_valid = false; }
      try { xml::validate_identity_scope(xml::parse(bytes), scope); } catch (const std::invalid_argument&) { portable_valid = false; }
      EXPECT_EQ(provider_valid, expected) << bytes;
      EXPECT_EQ(portable_valid, provider_valid) << bytes;
    }
  }
}
