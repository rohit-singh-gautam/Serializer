// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_scalar_contract.hpp"
#include "../src/xml_schema.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace binding = rohit::serializer::writer::xml_binding;
namespace xml = rohit::serializer::xml;

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

// Legacy provider patterns remain available without pretending their instructions are portable.
TEST(xml_scalar_contract, portable_opt_in_and_legacy_provider_are_distinct) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.patterns = {"\\p{L}+"};
  EXPECT_THROW((void)binding::compile_scalar_contract(facets), std::invalid_argument);
  if (xml::available()) {
    const auto legacy = binding::compile_scalar_contract(facets, false);
    EXPECT_FALSE(legacy.patterns_portable());
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

// The union's mandatory collapse policy applies before branch selection, including unbounded integers.
TEST(xml_scalar_contract, union_normalizes_before_trying_its_members) {
  binding::scalar_facets facets{};
  facets.lexical_kind = "string";
  facets.whitespace = "collapse";
  binding::scalar_facets integer{};
  integer.lexical_kind = "integer";
  facets.union_members.push_back(integer);
  const auto contract = binding::compile_scalar_contract(facets);
  EXPECT_NO_THROW(contract.validate(" \t9007199254740993\n"));
  EXPECT_THROW(contract.validate("1 2"), std::invalid_argument);
}
