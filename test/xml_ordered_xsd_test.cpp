// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <xml_ordered_xsd.hpp>
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>
#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
namespace xml = rohit::serializer::xml;

// Render a binary word without involving the generated writer or its grammar cursor.
std::string document(std::size_t bits, std::size_t length) {
  std::string result{"<root>\n"};
  for (std::size_t index{}; index < length; ++index) {
    result += (bits & (std::size_t{1U} << index)) != 0U ? "<right id='7'/>" : "<left>A</left>";
  }
  return result + "\n</root>";
}
}

// Exhaustive short words are classified by an independent XSD provider before generated decoding.
TEST(xml_ordered_xsd, complete_trace_and_edited_output_match_independent_xsd) {
  std::ifstream schema_input{ORDERED_XSD_PATH, std::ios::binary};
  ASSERT_TRUE(schema_input);
  const std::string schema_bytes{std::istreambuf_iterator<char>{schema_input}, {}};
  const xml::prepared_schema oracle{schema_bytes};
  xml::limits bounds;
  bounds.max_tree_bytes = 0U;
  for (std::size_t length{}; length <= 8U; ++length) {
    for (std::size_t bits{}; bits < (std::size_t{1U} << length); ++bits) {
      const auto input = document(bits, length);
      bool valid{true};
      try { oracle.validate(input); } catch (const std::invalid_argument&) { valid = false; }
      if (!valid) {
        EXPECT_THROW((void)xml::deserialize_tokens<ordered_xsd::root>(input, bounds), std::invalid_argument) << input;
        continue;
      }
      auto value = xml::deserialize_tokens<ordered_xsd::root>(input, bounds);
      const auto output = xml::serialize_tokens(value, bounds);
      EXPECT_NO_THROW(oracle.validate(output));
      const auto restored = xml::deserialize_tokens<ordered_xsd::root>(output, bounds);
      EXPECT_EQ(value.items.size(), restored.items.size());
      EXPECT_EQ(xml::serialize_tokens(restored, bounds), output);
      const auto expected_children = xml::parse(input).children;
      const auto actual_children = xml::parse(output).children;
      ASSERT_EQ(expected_children.size(), actual_children.size());
      for (std::size_t index{}; index < expected_children.size(); ++index) {
        EXPECT_EQ(expected_children[index].qualified_name, actual_children[index].qualified_name);
        EXPECT_EQ(expected_children[index].text, actual_children[index].text);
      }
      // An incomplete last group must fail during output, before publication to the caller.
      value.items.pop_back();
      std::string published{"previous output"};
      EXPECT_THROW(published = xml::serialize_tokens(value, bounds), std::invalid_argument);
      EXPECT_EQ(published, "previous output");
    }
  }
}

// Formatting is free of fragment/text charges; group and fixed-array admission stay independent.
TEST(xml_ordered_xsd, occurrence_limits_and_fixed_storage_are_exact) {
  const auto input = document(5U, 4U);
  xml::limits bounds;
  bounds.max_mixed_fragments = 4U;
  bounds.max_mixed_text_bytes = 0U;
  const auto value = xml::deserialize_tokens<ordered_xsd::root>(input, bounds);
  EXPECT_NO_THROW((void)xml::serialize_tokens(value, bounds));
  --bounds.max_mixed_fragments;
  EXPECT_THROW((void)xml::deserialize_tokens<ordered_xsd::root>(input, bounds), std::length_error);
  EXPECT_THROW((void)xml::serialize_tokens(value, bounds), std::length_error);
  EXPECT_NO_THROW((void)xml::deserialize_tokens<ordered_xsd::fixed>("<fixed><left>A</left><right id='7'/><left>B</left></fixed>"));
  EXPECT_THROW((void)xml::deserialize_tokens<ordered_xsd::fixed>("<fixed><left>A</left><right id='7'/></fixed>"), std::invalid_argument);
}
