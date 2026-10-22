// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_scalar_contract.hpp"
#include "../src/xml_schema.hpp"
#include <rohit/xml_lexical_contract.hpp>
#include <rohit/xml_tokens.hpp>

#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace binding = rohit::serializer::writer::xml_binding;
namespace xml = rohit::serializer::xml;

// Prepared list restrictions own ordered schema identities, including the empty list value.
TEST(xml_scalar_contract, compiled_qname_list_enumeration_keeps_schema_scope) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.list_item_kind = "QName";
  facets.whitespace = "collapse";
  facets.enumeration = {"schema:first local schema:second", ""};
  facets.qname_list_enumeration = {{{"first", "urn:allowed"}, {"local", ""}, {"second", "urn:allowed"}}, {}};
  const auto contract = binding::compile_scalar_contract(facets);
  facets.qname_list_enumeration.clear();
  xml::element scope{};
  scope.namespace_bindings.emplace("p", "urn:allowed");
  scope.namespace_bindings.emplace("schema", "urn:shadow");
  EXPECT_NO_THROW(contract.validate(" p:first\tlocal p:second ", scope));
  EXPECT_NO_THROW(contract.validate(" \n", scope));
  EXPECT_THROW(contract.validate("schema:first local schema:second", scope), std::invalid_argument);
  EXPECT_THROW(contract.validate("p:second local p:first", scope), std::invalid_argument);
  EXPECT_THROW(contract.validate("p:first local", scope), std::invalid_argument);
  EXPECT_THROW(contract.validate("missing:first", scope), std::invalid_argument);
  xml::operation_context context{};
  context.charge = [](auto, auto) { throw std::length_error{"list admission marker"}; };
  EXPECT_THROW(contract.validate("p:first local p:second", scope, &context), std::length_error);
}

// Schema-scope QName identities replace their lexical aliases without weakening string enumerations.
TEST(xml_scalar_contract, generated_qname_enumeration_uses_expanded_schema_identity) {
  const std::array<std::string_view, 1U> lexical{{"schema:item"}};
  const std::array<xml::name, 1U> expanded{{{"item", "urn:allowed"}}};
  xml::lexical_contract rule{};
  rule.kind = "QName";
  rule.enumeration = lexical;
  rule.qname_enumeration = expanded;
  xml::element scope{};
  scope.namespace_bindings.emplace("p", "urn:allowed");
  scope.namespace_bindings.emplace("wrong", "urn:other");
  EXPECT_NO_THROW(xml::validate_lexical_contract("p:item", rule, scope));
  EXPECT_THROW(xml::validate_lexical_contract("p:other", rule, scope), std::invalid_argument);
  EXPECT_THROW(xml::validate_lexical_contract("wrong:item", rule, scope), std::invalid_argument);
  EXPECT_THROW(xml::validate_lexical_contract("missing:item", rule, scope), std::invalid_argument);
  rule.kind = "string";
  rule.qname_enumeration = {};
  EXPECT_NO_THROW(xml::validate_lexical_contract("schema:item", rule, scope));
  EXPECT_THROW(xml::validate_lexical_contract("p:item", rule, scope), std::invalid_argument);
}

// Namespace retention owns its strings and admits all map storage before publishing an occurrence.
TEST(xml_scalar_contract, child_namespace_storage_preserves_scopes_and_callback_failures) {
  xml::child_namespace_store retained{};
  xml::element owner{};
  owner.namespace_bindings.emplace("p", "urn:parent");
  xml::element child{};
  child.namespace_bindings.emplace("p", "urn:child");
  child.namespace_bindings.emplace("", "");
  auto& occurrences = xml::child_namespace_destination(retained, "value");
  xml::retain_child_namespace_scope(occurrences, child);
  child.namespace_bindings["p"] = "urn:edited";
  EXPECT_EQ(xml::resolve_qname("p:item", xml::child_namespace_scope(retained, "value", 0U, owner)),
      (xml::name{"item", "urn:child"}));
  EXPECT_EQ(&xml::child_namespace_scope(retained, "value", 1U, owner), &owner);
  EXPECT_EQ(&xml::child_namespace_scope(retained, "new", 0U, owner), &owner);
  xml::operation_context failed{};
  failed.charge = [](auto, auto) { throw std::invalid_argument{"caller retention failure"}; };
  EXPECT_THROW(xml::retain_child_namespace_scope(occurrences, child, &failed), std::invalid_argument);
  ASSERT_EQ(occurrences.size(), 1U);
  EXPECT_THROW((void)xml::child_namespace_destination(retained, "failed", &failed),
      std::invalid_argument);
  EXPECT_EQ(retained.size(), 1U);
}

// Final generated tag aliases cannot activate an earlier QName member of a preserved string fallback.
TEST(xml_scalar_contract, emitted_namespace_aliases_cannot_change_union_selection) {
  std::array<xml::lexical_contract, 2U> members{};
  members[0].kind = "QName";
  members[1].kind = "string";
  xml::lexical_contract rule{};
  rule.union_members = members;
  xml::element root{};
  root.qualified_name = {"root", "urn:generated"};
  xml::element child{};
  child.qualified_name = {"value", ""};
  child.text = "srl0:item";
  child.namespace_bindings.emplace("", "");
  child.namespace_contract = &rule;
  root.children.push_back(child);
  const auto replay = xml::parse(xml::write(root));
  ASSERT_EQ(replay.children.size(), 1U);
  EXPECT_FALSE(replay.children.front().namespace_bindings.contains("srl0"));
  EXPECT_EQ(replay.children.front().text, "srl0:item");
  EXPECT_THROW((void)xml::resolve_qname(replay.children.front().text, replay.children.front()), std::invalid_argument);
  EXPECT_NO_THROW(xml::validate_lexical_contract(replay.children.front().text, rule, replay.children.front()));
  xml::token_writer writer{};
  writer.open(root.qualified_name);
  writer.open(child.qualified_name, child.namespace_bindings);
  EXPECT_THROW(writer.validate_namespace_value("srl:item", child, rule), std::invalid_argument);
  EXPECT_THROW((void)writer.finish(), std::logic_error);
}

// First-member selection sees raw text; outer facets inspect that member's normalized value.
TEST(xml_scalar_contract, generated_union_whitespace_belongs_to_the_selected_member) {
  const std::array<xml::lexical_pattern_matcher, 1U> patterns{{
      +[](std::string_view value, xml::operation_context* context) {
        static const auto pattern = xml::compile_portable_pattern("p:item");
        return pattern.matches(value, {}, context);
      }}};
  const std::array<xml::lexical_pattern_group, 1U> groups{{{patterns}}};
  std::array<xml::lexical_contract, 2U> members{};
  members[0].kind = "string";
  members[0].pattern_groups = groups;
  members[1].kind = "QName";
  members[1].constraints.whitespace = xml::whitespace_policy::collapse;
  xml::lexical_contract rule{};
  rule.kind = "string";
  rule.constraints.whitespace = xml::whitespace_policy::collapse;
  rule.pattern_groups = groups;
  rule.union_members = members;
  xml::element scope{};
  scope.namespace_bindings.emplace("p", "urn:child");
  const xml::lexical_contract* selected{};
  EXPECT_NO_THROW(xml::validate_lexical_contract_member(" p:item ", rule, scope, nullptr, &selected));
  EXPECT_EQ(selected, &members[1]);
  EXPECT_NO_THROW(xml::validate_lexical_contract_member("p:item", rule, scope, nullptr, &selected));
  EXPECT_EQ(selected, &members[0]);
  if (xml::available()) {
    const xml::prepared_schema oracle{R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:q="urn:child"><xs:simpleType name="Choice"><xs:union><xs:simpleType><xs:restriction base="xs:string"><xs:pattern value="p:item"/></xs:restriction></xs:simpleType><xs:simpleType><xs:restriction base="xs:QName"/></xs:simpleType></xs:union></xs:simpleType><xs:element name="probe"><xs:simpleType><xs:restriction base="Choice"><xs:enumeration value="q:item"/></xs:restriction></xs:simpleType></xs:element></xs:schema>)"};
    EXPECT_NO_THROW(oracle.validate("<probe xmlns:p='urn:child'> p:item </probe>"));
    EXPECT_THROW(oracle.validate("<probe xmlns:p='urn:child'>p:item</probe>"), std::invalid_argument);
  }
}

// Exact decimal IR retains integers beyond binary64 precision and every inclusive/exclusive boundary.
TEST(xml_scalar_contract, numeric_boundaries_remain_exact) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "integer";
  facets.min_inclusive = "9007199254740993";
  facets.max_exclusive = "9007199254740995";
  const auto contract = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(contract.validate("9007199254740993"));
  EXPECT_NO_THROW(contract.validate("+09007199254740994"));
  EXPECT_THROW(contract.validate("9007199254740992"), std::invalid_argument);
  EXPECT_THROW(contract.validate("9007199254740995"), std::invalid_argument);
  facets.lexical_kind = "decimal";
  facets.min_inclusive.reset();
  facets.max_exclusive.reset();
  facets.min_exclusive = "-0.000000000000000000000000000001";
  facets.max_inclusive = "0.000000000000000000000000000001";
  const auto decimal = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(decimal.validate("0"));
  EXPECT_NO_THROW(decimal.validate("0.0000000000000000000000000000010"));
  EXPECT_THROW(decimal.validate("-0.000000000000000000000000000001"), std::invalid_argument);
  EXPECT_THROW(decimal.validate("0.0000000000000000000000000000011"), std::invalid_argument);
}

// Contradictory authored facets reject while compilation still owns an unpublished local candidate.
TEST(xml_scalar_contract, static_numeric_and_length_contradictions_reject) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "decimal";
  facets.min_inclusive = "2";
  facets.max_exclusive = "2.0";
  EXPECT_THROW((void)binding::compile_scalar_contract(facets), std::invalid_argument);
  facets.max_exclusive = "3";
  facets.min_inclusive = "not-numeric";
  EXPECT_THROW((void)binding::compile_scalar_contract(facets), std::invalid_argument);
  facets = {};
  facets.length = 0U;
  facets.min_length = 1U;
  EXPECT_THROW((void)binding::compile_scalar_contract(facets), std::invalid_argument);
  facets.length.reset();
  facets.max_length = 0U;
  EXPECT_THROW((void)binding::compile_scalar_contract(facets), std::invalid_argument);
}

// Float restrictions compare their actual value space, including rounding and unordered NaN limits.
TEST(xml_scalar_contract, floating_bounds_compile_in_the_selected_value_space) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "float";
  facets.min_exclusive = "16777216";
  facets.max_inclusive = "16777218";
  const auto contract = binding::compile_scalar_contract(facets);
  EXPECT_THROW(contract.validate("16777217"), std::invalid_argument);
  EXPECT_NO_THROW(contract.validate("16777218"));
  facets.max_inclusive = "16777217";
  EXPECT_THROW((void)binding::compile_scalar_contract(facets), std::invalid_argument);
  facets.min_exclusive.reset();
  facets.max_inclusive = "NaN";
  EXPECT_NO_THROW(binding::compile_scalar_contract(facets).validate("NaN"));
  facets.max_inclusive.reset();
  facets.min_exclusive = "INF";
  EXPECT_THROW(binding::compile_scalar_contract(facets).validate("INF"), std::invalid_argument);
}

// Previously supported single NaN facets compile and preserve XSD 1.0 inclusive/exclusive semantics.
TEST(xml_scalar_contract, nan_bound_facets_preserve_existing_provider_contracts) {
  using facet_member = std::optional<std::string> binding::scalar_facets::*;
  const std::pair<std::string_view, facet_member> facets[]{
      {"minInclusive", &binding::scalar_facets::min_inclusive},
      {"maxInclusive", &binding::scalar_facets::max_inclusive},
      {"minExclusive", &binding::scalar_facets::min_exclusive},
      {"maxExclusive", &binding::scalar_facets::max_exclusive}};
  for (const auto kind : {"float", "double"}) {
    for (const auto& [name, member] : facets) {
      SCOPED_TRACE(std::string{kind} + ':' + std::string{name});
      binding::scalar_facets restrictions{};
      restrictions.lexical_kind = kind;
      restrictions.*member = "NaN";
      const auto contract = binding::compile_scalar_contract(restrictions);
      const bool inclusive = name == "minInclusive" || name == "maxInclusive";
      if (inclusive) { EXPECT_NO_THROW(contract.validate("NaN")); }
      else { EXPECT_THROW(contract.validate("NaN"), std::invalid_argument); }
      EXPECT_THROW(contract.validate("0"), std::invalid_argument);
      if (xml::available()) {
        const auto xsd = std::string{"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='v'><xs:simpleType><xs:restriction base='xs:"} +
            kind + "'><xs:" + std::string{name} + " value='NaN'/></xs:restriction></xs:simpleType></xs:element></xs:schema>";
        const xml::prepared_schema reference{xsd};
        if (inclusive) { EXPECT_NO_THROW(reference.validate("<v>NaN</v>")); }
        else { EXPECT_THROW(reference.validate("<v>NaN</v>"), std::invalid_argument); }
        // Some provider releases accept finite values against an incomparable NaN bound.
        // Preserve the established generated XSD 1.0 checks above instead of inheriting
        // that provider discrepancy; this oracle checks schema acceptance and NaN itself.
      }
    }
  }
}

// Independent schema compilation checks supported numeric restriction consistency against libxml2.
TEST(xml_scalar_contract, numeric_facet_compilation_agrees_with_xsd_provider) {
  if (xml::available()) {
    for (const auto& limits : {std::pair{"2", "3"}, std::pair{"3", "2"}, std::pair{"2", "2"},
        std::pair{"INF", "INF"}, std::pair{"NaN", "3"}}) {
      binding::scalar_facets facets{};
      facets.lexical_kind = "float";
      facets.min_exclusive = limits.first;
      facets.max_inclusive = limits.second;
      const auto xsd = std::string{"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='v'><xs:simpleType><xs:restriction base='xs:float'><xs:minExclusive value='"} +
          limits.first + "'/><xs:maxInclusive value='" + limits.second + "'/></xs:restriction></xs:simpleType></xs:element></xs:schema>";
      bool provider_valid{true}, compiled_valid{true};
      try { const xml::prepared_schema reference{xsd}; } catch (const std::invalid_argument&) { provider_valid = false; }
      try { (void)binding::compile_scalar_contract(facets); } catch (const std::invalid_argument&) { compiled_valid = false; }
      EXPECT_EQ(compiled_valid, provider_valid) << limits.first << ": " << limits.second;
    }
  }
}

// Length counts XML Unicode scalar values, retaining empty strings and combining sequences literally.
TEST(xml_scalar_contract, unicode_length_and_empty_values) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.length = 1U;
  const auto contract = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(contract.validate("\xc3\xa9"));
  EXPECT_NO_THROW(contract.validate("\xf0\x9f\x98\x80"));
  EXPECT_THROW(contract.validate(""), std::invalid_argument);
  EXPECT_THROW(contract.validate("e\xcc\x81"), std::invalid_argument);
  facets.length = 0U;
  EXPECT_NO_THROW(binding::compile_scalar_contract(facets).validate(""));
}

// Enumerations compare numeric values and normalized strings rather than UTF-8 byte length or spelling.
TEST(xml_scalar_contract, enumeration_value_spaces_and_whitespace) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "decimal";
  facets.enumeration = {"9007199254740993.00"};
  const auto numeric = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(numeric.validate("+09007199254740993.0"));
  EXPECT_THROW(numeric.validate("9007199254740992.999999"), std::invalid_argument);
  facets = {};
  facets.lexical_kind = "string";
  facets.whitespace = "collapse";
  facets.enumeration = {"", "\xc3\xa9", "two words"};
  const auto text = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(text.validate(" \t\n"));
  EXPECT_NO_THROW(text.validate("two\t words"));
  EXPECT_NO_THROW(text.validate("\xc3\xa9"));
  EXPECT_THROW(text.validate("e\xcc\x81"), std::invalid_argument);
}

// Restrictions intersect pattern groups while alternatives within a single restriction form a union.
TEST(xml_scalar_contract, portable_pattern_group_composition) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.pattern_groups = {{"a.*", "b.*"}};
  facets.patterns = {".*[0-9]{2}"};
  const auto contract = binding::compile_scalar_contract(facets);
  EXPECT_TRUE(contract.patterns_portable());
  EXPECT_NO_THROW(contract.validate("a12"));
  EXPECT_NO_THROW(contract.validate("b34"));
  EXPECT_THROW(contract.validate("c12"), std::invalid_argument);
  EXPECT_THROW(contract.validate("a1"), std::invalid_argument);
}

// Both prepared paths retain Unicode category semantics and explicit provider selection.
TEST(xml_scalar_contract, portable_and_legacy_unicode_properties) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.patterns = {"\\p{L}+"};
  const auto portable = binding::compile_scalar_contract(facets);
  EXPECT_TRUE(portable.patterns_portable());
  EXPECT_NO_THROW(portable.validate("\xc3\xa9"));
  EXPECT_THROW(portable.validate("123"), std::invalid_argument);
  if (xml::available()) {
    const auto legacy = binding::compile_scalar_contract(facets, false);
    EXPECT_TRUE(legacy.patterns_portable());
    EXPECT_NO_THROW(legacy.validate("\xc3\xa9"));
    EXPECT_THROW(legacy.validate("123"), std::invalid_argument);
  }
}

// A previously valid large provider repeat must not inherit the portable compiler's tighter caps.
TEST(xml_scalar_contract, legacy_provider_preserves_patterns_exceeding_portable_compilation_limits) {
  const auto repeat_count = xml::pattern_limits{}.max_repetitions + 1U;
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.patterns = {"a{" + std::to_string(repeat_count) + '}'};
  EXPECT_THROW((void)binding::compile_scalar_contract(facets), std::length_error);
  if (xml::available()) {
    const auto legacy = binding::compile_scalar_contract(facets, false);
    EXPECT_FALSE(legacy.patterns_portable());
    EXPECT_NO_THROW(legacy.validate(std::string(repeat_count, 'a')));
    EXPECT_THROW(legacy.validate(std::string(repeat_count - 1U, 'a')), std::invalid_argument);
    facets.patterns = {"aa"};
    xml::pattern_limits tight{};
    tight.max_expression_bytes = 1U;
    EXPECT_THROW((void)binding::compile_scalar_contract(facets, true, tight), std::length_error);
    const auto expression_fallback = binding::compile_scalar_contract(facets, false, tight);
    EXPECT_FALSE(expression_fallback.patterns_portable());
    EXPECT_NO_THROW(expression_fallback.validate("aa"));
  } else {
    EXPECT_THROW((void)binding::compile_scalar_contract(facets, false), std::runtime_error);
  }
}

// Prepared metadata owns its authored strings and survives destruction of the input facets.
TEST(xml_scalar_contract, metadata_owns_bounds_and_patterns) {
  const auto contract = [] {
    binding::scalar_facets facets{};
    facets.lexical_kind = "integer";
    facets.min_inclusive = "10";
    facets.patterns = {"[0-9]{2}"};
    return binding::compile_scalar_contract(facets);
  }();
  EXPECT_NO_THROW(contract.validate("12"));
  EXPECT_THROW(contract.validate("09"), std::invalid_argument);
}

// Union branch selection never masks invalid_argument failures from a caller's resource callback.
TEST(xml_scalar_contract, union_preserves_caller_failure) {
  binding::scalar_facets facets{};
  binding::scalar_facets integer{};
  integer.lexical_kind = "integer";
  binding::scalar_facets text{};
  text.lexical_kind = "string";
  facets.union_members = {integer, text};
  const auto contract = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(contract.validate("hello"));
  xml::operation_context context{};
  context.charge = [](auto, auto) { throw std::invalid_argument{"caller failure"}; };
  EXPECT_THROW(contract.validate("hello", {}, &context), std::invalid_argument);
}

// Member-specific whitespace normalization follows raw first-member selection.
TEST(xml_scalar_contract, union_normalizes_inside_its_selected_member) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.whitespace = "collapse";
  binding::scalar_facets integer{};
  integer.lexical_kind = "integer";
  integer.whitespace = "collapse";
  facets.union_members.push_back(integer);
  const auto contract = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(contract.validate(" \t9007199254740993\n"));
  EXPECT_THROW(contract.validate("1 2"), std::invalid_argument);
}

// Outer enumeration distinguishes selected primitive spaces and exact numeric aliases.
TEST(xml_scalar_contract, outer_union_enumeration_compares_selected_value_identities) {
  binding::scalar_facets boolean{};
  boolean.lexical_kind = "boolean";
  boolean.whitespace = "collapse";
  binding::scalar_facets integer{};
  integer.lexical_kind = "unsignedShort";
  integer.whitespace = "collapse";
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.whitespace = "collapse";
  facets.union_members = {boolean, integer};
  facets.enumeration = {"1"};
  const auto boolean_first = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(boolean_first.validate(" true "));
  EXPECT_THROW(boolean_first.validate("01"), std::invalid_argument);
  EXPECT_THROW(boolean_first.validate("false"), std::invalid_argument);
  facets.union_members = {integer, boolean};
  const auto number_first = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(number_first.validate("+001"));
  EXPECT_THROW(number_first.validate("true"), std::invalid_argument);
  binding::scalar_facets preserve{};
  preserve.lexical_kind = "string";
  preserve.patterns = {" raw "};
  binding::scalar_facets token{};
  token.lexical_kind = "token";
  token.whitespace = "collapse";
  facets.union_members = {preserve, token};
  facets.enumeration = {" raw "};
  const auto string_first = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(string_first.validate(" raw "));
  EXPECT_THROW(string_first.validate("raw"), std::invalid_argument);
}

// The generated static runtime shares primitive identity rules with the compiler's prepared contract.
TEST(xml_scalar_contract, static_outer_union_enumeration_preserves_raw_member_selection) {
  std::array<xml::lexical_contract, 2U> members{};
  members[0].kind = "boolean";
  members[1].kind = "unsignedShort";
  for (auto& member : members) { member.constraints.whitespace = xml::whitespace_policy::collapse; }
  constexpr std::array<std::string_view, 1U> enumeration{{"1"}};
  xml::lexical_contract rule{};
  rule.union_members = members;
  rule.enumeration = enumeration;
  EXPECT_NO_THROW(xml::validate_lexical_contract("true", rule, {}));
  EXPECT_THROW(xml::validate_lexical_contract("01", rule, {}), std::invalid_argument);
  EXPECT_TRUE(xml::lexical_union_equal("true", "1", rule, {}));
  EXPECT_FALSE(xml::lexical_union_equal("01", "1", rule, {}));
}

// Outer literals resolve in their schema scope, while instances retain unrelated aliases and shadowing.
TEST(xml_scalar_contract, prepared_outer_union_names_retain_schema_owned_identities) {
  binding::scalar_facets qname{};
  qname.lexical_kind = "QName";
  qname.whitespace = "collapse";
  binding::scalar_facets text{};
  text.lexical_kind = "string";
  binding::scalar_facets selection{};
  selection.union_members = {qname, text};
  xml::element schema{};
  schema.namespace_bindings.emplace("schema", "urn:allowed");
  binding::scalar_facets restriction = selection;
  restriction.enumeration = {"schema:item"};
  restriction.union_enumeration = {binding::compile_scalar_contract(selection).identify("schema:item", schema)};
  const auto contract = binding::compile_scalar_contract(restriction);
  xml::element instance{};
  instance.namespace_bindings.emplace("p", "urn:allowed");
  instance.namespace_bindings.emplace("schema", "urn:shadowed");
  EXPECT_NO_THROW(contract.validate(" p:item ", instance));
  EXPECT_THROW(contract.validate("schema:item", instance), std::invalid_argument);
  EXPECT_THROW(contract.validate("missing:item", instance), std::invalid_argument);
  selection.union_members = {text, qname};
  restriction = selection;
  restriction.enumeration = {"schema:item"};
  restriction.union_enumeration = {binding::compile_scalar_contract(selection).identify("schema:item", schema)};
  const auto string_first = binding::compile_scalar_contract(restriction);
  EXPECT_NO_THROW(string_first.validate("schema:item", instance));
  EXPECT_THROW(string_first.validate("p:item", instance), std::invalid_argument);
}

// Item restrictions select primitive identities before outer list enumeration and cardinality checks.
TEST(xml_scalar_contract, restricted_union_list_items_keep_primitive_families) {
  binding::scalar_facets boolean{};
  boolean.lexical_kind = "boolean";
  boolean.whitespace = "collapse";
  binding::scalar_facets integer{};
  integer.lexical_kind = "unsignedShort";
  integer.whitespace = "collapse";
  integer.max_inclusive = "9";
  binding::scalar_facets item{};
  item.union_members = {boolean, integer};
  binding::scalar_facets list{};
  list.lexical_kind = "string";
  list.list_item_kind = "string";
  list.whitespace = "collapse";
  list.list_item_facets = {item};
  list.length = 2U;
  list.enumeration = {"1 2"};
  const auto rule = binding::compile_scalar_contract(list);
  EXPECT_NO_THROW(rule.validate("true 002"));
  EXPECT_THROW(rule.validate("01 2"), std::invalid_argument);
  EXPECT_THROW(rule.validate("true 10"), std::invalid_argument);
  EXPECT_THROW(rule.validate("true 2 2"), std::invalid_argument);
  const auto identity = rule.identify("true 002");
  ASSERT_EQ(identity.items.size(), 2U);
  EXPECT_EQ(identity.items[0].lexical_kind, "boolean");
  EXPECT_EQ(identity.items[1].lexical_kind, "unsignedShort");
}

// The public static comparison resolves each selected item rather than comparing owning list strings.
TEST(xml_scalar_contract, static_restricted_list_comparison_uses_atomic_rules) {
  std::array<xml::lexical_contract, 2U> branches{};
  branches[0].kind = "QName";
  branches[1].kind = "string";
  xml::lexical_contract item{};
  item.union_members = branches;
  xml::lexical_contract list{};
  list.kind = "string";
  list.list_item_kind = "string";
  list.list_item = &item;
  xml::element scope{};
  scope.namespace_bindings.emplace("p", "urn:allowed");
  scope.namespace_bindings.emplace("q", "urn:allowed");
  EXPECT_TRUE(xml::lexical_union_equal("p:item missing:item", "q:item missing:item", list, scope));
  EXPECT_FALSE(xml::lexical_union_equal("p:item missing:item", "q:other missing:item", list, scope));
  EXPECT_FALSE(xml::lexical_union_equal("p:item missing:item", "q:item missing:other", list, scope));
}
