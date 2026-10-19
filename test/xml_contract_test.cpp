// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml_choice_contract.hpp>
#include <rohit/xml_lexical_contract.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace xml = rohit::serializer::xml;

// Keep each provider automaton independent of changing input and generated model instances.
bool letters(std::string_view value, xml::operation_context* context) {
  static const xml::prepared_pattern pattern{"[a-z]+"};
  return pattern.matches(value, {}, context);
}

// Accept a separate same-level pattern alternative using the real XML Schema regex grammar.
bool digits(std::string_view value, xml::operation_context* context) {
  static const xml::prepared_pattern pattern{"[0-9]+"};
  return pattern.matches(value, {}, context);
}

// Model an inherited restriction which must hold alongside an earlier OR-group.
bool three_characters(std::string_view value, xml::operation_context* context) {
  static const xml::prepared_pattern pattern{".{3}"};
  return pattern.matches(value, {}, context);
}
} // namespace

// Union alternatives remain independent and may accept an empty value without relaxing another branch.
TEST(xml_contract, unions_keep_independent_lexical_spaces_and_caller_failures) {
  std::array<xml::lexical_contract, 2U> branches{};
  branches[0].kind = "language";
  branches[0].constraints.whitespace = xml::whitespace_policy::collapse;
  branches[1].kind = "string";
  branches[1].constraints.length = 0U;
  xml::lexical_contract contract{};
  contract.union_members = branches;
  const xml::element scope{};
  EXPECT_NO_THROW(xml::validate_lexical_contract("en-US", contract, scope));
  EXPECT_NO_THROW(xml::validate_lexical_contract("", contract, scope));
  EXPECT_THROW(xml::validate_lexical_contract("en_US", contract, scope), std::invalid_argument);

  // An otherwise permissive second branch must not swallow a caller's one-shot resource failure.
  branches[1].constraints.length.reset();
  auto raised = false;
  xml::operation_context context{};
  constexpr auto normalized = std::string_view{"en"};
  context.charge = [&](xml::operation_resource resource, std::size_t amount) {
    if (!raised && resource == xml::operation_resource::work_units && amount == normalized.size()) {
      raised = true;
      throw std::invalid_argument{"caller charge failure"};
    }
  };
  try {
    xml::validate_lexical_contract(" en ", contract, scope, &context);
    FAIL() << "Caller failure must not select the permissive union branch";
  } catch (const std::invalid_argument& error) {
    EXPECT_STREQ(error.what(), "caller charge failure");
  }
  EXPECT_TRUE(raised);
}

// Same-level patterns are OR alternatives, while derived restriction groups remain conjunctive.
TEST(xml_contract, pattern_groups_preserve_xsd_restriction_semantics) {
  const std::array alternatives{xml::lexical_pattern_matcher{letters}, xml::lexical_pattern_matcher{digits}};
  const std::array derived{xml::lexical_pattern_matcher{three_characters}};
  const std::array groups{xml::lexical_pattern_group{alternatives}, xml::lexical_pattern_group{derived}};
  xml::lexical_contract contract{};
  contract.kind = "string";
  contract.pattern_groups = groups;
  const xml::element scope{};
  EXPECT_NO_THROW(xml::validate_lexical_contract("abc", contract, scope));
  EXPECT_NO_THROW(xml::validate_lexical_contract("123", contract, scope));
  EXPECT_THROW(xml::validate_lexical_contract("a1b", contract, scope), std::invalid_argument);
  EXPECT_THROW(xml::validate_lexical_contract("abcd", contract, scope), std::invalid_argument);
}

// Compare every short branch sequence with independently recorded .NET XSD validation results.
// libxml2 admits excess choice repetitions for these schemas, so it cannot serve as this oracle.
TEST(xml_contract, repeated_choice_matches_independent_xsd_occurrences) {
  struct bounds {
    std::size_t group_minimum{}, group_maximum{}, known_minimum{}, known_maximum{},
        foreign_minimum{}, foreign_maximum{};
  };
  const std::array cases{
      bounds{0U, 2U, 1U, 2U, 1U, 3U}, bounds{1U, 2U, 0U, 2U, 1U, 2U},
      bounds{2U, 3U, 2U, 3U, 1U, 2U}, bounds{3U, 3U, 0U, 1U, 0U, 2U},
      bounds{1U, 1U, 2U, 3U, 2U, 3U}};
  // XmlSchemaSet, System.Xml 4.0.0.0: bit (2^length - 1 + word) records acceptance.
  // Each word chooses item (zero) or foreign wildcard (one), least-significant child first.
  constexpr std::array<std::string_view, cases.size()> accepted{
      "ffed446840000048000000000000004000000000000000000000000000000000",
      "ffed044800000000000000000000000000000000000000000000000000000000",
      "40c985c94d8085c9cd480080048085c885488000000000800000008004008000",
      "ff7f647c00400064000000000000004000000000000000000000000000000000",
      "c840000000000000000000000000000000000000000000000000000000000000"};
  for (std::size_t case_index{}; case_index < cases.size(); ++case_index) {
    const auto& selected = cases[case_index];
    const auto schema = std::string{
        "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' "
        "targetNamespace='urn:choice' elementFormDefault='qualified'>"
        "<xs:element name='root'><xs:complexType><xs:sequence><xs:choice minOccurs='"} +
        std::to_string(selected.group_minimum) + "' maxOccurs='" + std::to_string(selected.group_maximum) +
        "'><xs:element name='item' type='xs:string' minOccurs='" + std::to_string(selected.known_minimum) +
        "' maxOccurs='" + std::to_string(selected.known_maximum) +
        "'/><xs:any namespace='##other' processContents='lax' minOccurs='" +
        std::to_string(selected.foreign_minimum) + "' maxOccurs='" +
        std::to_string(selected.foreign_maximum) +
        "'/></xs:choice></xs:sequence></xs:complexType></xs:element></xs:schema>";
    const std::array branches{
        xml::choice_branch_contract{{"item", "urn:choice"}, false, {}, {},
            selected.known_minimum, selected.known_maximum},
        xml::choice_branch_contract{{}, true, "##other", "urn:choice",
            selected.foreign_minimum, selected.foreign_maximum}};
    constexpr std::size_t maximum_children = 7U;
    for (std::size_t count{}; count <= maximum_children; ++count) {
      for (std::size_t bits{}; bits < (std::size_t{1U} << count); ++bits) {
        auto document = std::string{"<root xmlns='urn:choice' xmlns:e='urn:foreign'>"};
        std::vector<xml::element> children;
        for (std::size_t index{}; index < count; ++index) {
          const auto foreign = (bits & (std::size_t{1U} << index)) != 0U;
          xml::element child{};
          child.qualified_name = foreign ? xml::name{"extra", "urn:foreign"} : xml::name{"item", "urn:choice"};
          children.push_back(std::move(child));
          document += foreign ? "<e:extra/>" : "<item/>";
        }
        document += "</root>";
        const auto position = (std::size_t{1U} << count) - 1U + bits;
        constexpr std::size_t bits_per_byte = 8U;
        constexpr std::string_view hex_digits{"0123456789abcdef"};
        const auto byte_offset = position / bits_per_byte * 2U;
        const auto byte = hex_digits.find(accepted[case_index][byte_offset]) * 16U +
            hex_digits.find(accepted[case_index][byte_offset + 1U]);
        const auto expected = (byte & (std::size_t{1U} << (position % bits_per_byte))) != 0U;
        bool actual = true;
        try {
          xml::validate_repeated_choice(children, branches, selected.group_minimum, selected.group_maximum);
        } catch (const std::invalid_argument&) { actual = false; }
        EXPECT_EQ(actual, expected) << "Input: " << document << "\nSchema: " << schema;
      }
    }
  }
}

// Preservation slots keep interleaving and cannot move outside their declared known-field interval.
TEST(xml_contract, repeated_choice_output_validates_retained_positions) {
  const std::array branches{
      xml::choice_branch_contract{{"item", "urn:choice"}, false, {}, {}, 1U, 1U},
      xml::choice_branch_contract{{}, true, "##other", "urn:choice", 1U, 1U}};
  xml::extension_store extensions{};
  xml::element foreign{};
  foreign.qualified_name = {"extra", "urn:foreign"};
  extensions.children.push_back({1U, foreign, 0U});
  EXPECT_NO_THROW(xml::validate_repeated_choice_output(0U, 2U, 0U, 0U, &extensions, branches, 3U, 3U));
  EXPECT_THROW(xml::validate_repeated_choice_output(0U, 2U, 0U, 0U, &extensions, branches, 1U, 2U), std::invalid_argument);
  extensions.children.front().before_child = 3U;
  EXPECT_THROW(xml::validate_repeated_choice_output(0U, 2U, 0U, 0U, &extensions, branches, 0U, {}), std::invalid_argument);
}
