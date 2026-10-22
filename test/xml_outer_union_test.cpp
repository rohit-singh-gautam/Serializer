// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <xml_outer_union.hpp>
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>
#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;

// Render a single changed lexical value independently of the generated model and its restrictions.
std::string document(std::size_t selected, std::string_view replacement) {
  constexpr std::array<std::string_view, 6U> names{
      "boolean_value", "numeric_value", "list_value", "preserved", "fixed_value", "default_value"};
  constexpr std::array<std::string_view, 6U> values{"true", "01", "001 02", " raw ", "true", ""};
  std::string result{"<probe>"};
  for (std::size_t index{}; index < names.size(); ++index) {
    result += '<'; result += names[index]; result += '>';
    result += index == selected ? replacement : values[index];
    result += "</"; result += names[index]; result += '>';
  }
  return result + "</probe>";
}

// Normalize only known provider disagreements without changing the generated contract under test.
std::string oracle_document(std::string input) {
  auto tree = xml::parse(input);
  for (auto& field : tree.children) {
    if (field.text == "+001") { field.text = "001"; }
    if (field.qualified_name.local == "fixed_value" && field.text == "true") { field.text = "1"; }
  }
  return xml::write(tree);
}
}

// All entrypoints retain selected value identity, raw string spelling and transactional output failure.
TEST(xml_outer_union, selected_primitive_identities_match_independent_schema) {
  std::ifstream input{OUTER_UNION_XSD_PATH, std::ios::binary};
  ASSERT_TRUE(input);
  const std::string schema{std::istreambuf_iterator<char>{input}, {}};
  const xml::prepared_schema oracle{schema};
  struct probe { std::size_t field; std::string_view text; bool valid; };
  constexpr std::array cases{
      probe{0U, "true", true}, probe{0U, "1", true}, probe{0U, " true ", true},
      probe{0U, "01", false}, probe{0U, "false", false}, probe{0U, "2", false},
      probe{1U, "1", true}, probe{1U, "01", true}, probe{1U, "+001", true},
      probe{1U, "true", false}, probe{1U, "0", false}, probe{1U, "65536", false},
      probe{2U, "1 2", true}, probe{2U, "001 02", true}, probe{2U, "1\t2", true},
      probe{2U, "2 1", false}, probe{2U, "one two", false}, probe{2U, "1 2 3", false},
      probe{3U, " raw ", true}, probe{3U, "raw", false}, probe{3U, "  raw  ", false}, probe{3U, "other", false},
      probe{4U, "true", true}, probe{4U, "1", true}, probe{4U, "01", false}, probe{4U, "false", false},
      probe{5U, "", true}, probe{5U, "true", true}, probe{5U, "001", true}};
  for (const auto& selected : cases) {
    const auto source = document(selected.field, selected.text);
    SCOPED_TRACE(source);
    if (!selected.valid) {
      EXPECT_THROW(oracle.validate(oracle_document(source)), std::invalid_argument);
      EXPECT_THROW((void)xml::deserialize<outer::probe>(source), std::invalid_argument);
      EXPECT_THROW((void)xml::deserialize_tokens<outer::probe>(source), std::invalid_argument);
      continue;
    }
    EXPECT_NO_THROW(oracle.validate(oracle_document(source)));
    auto value = xml::deserialize<outer::probe>(source);
    auto token_value = xml::deserialize_tokens<outer::probe>(source);
    EXPECT_EQ(value.preserved, " raw ");
    EXPECT_EQ(token_value.preserved, " raw ");
    const auto output = xml::serialize_tokens(value);
    EXPECT_NO_THROW(oracle.validate(oracle_document(output)));
    EXPECT_EQ(xml::serialize(value), xml::serialize(token_value));
    const auto restored = xml::deserialize_tokens<outer::probe>(output);
    EXPECT_EQ(restored.boolean_value, value.boolean_value);
    EXPECT_EQ(restored.numeric_value, value.numeric_value);
    value.boolean_value = "01";
    std::string published{"previous output"};
    EXPECT_THROW(published = xml::serialize_tokens(value), std::invalid_argument);
    EXPECT_EQ(published, "previous output");
    EXPECT_THROW(published = xml::serialize(value), std::invalid_argument);
    EXPECT_EQ(published, "previous output");
  }
}
