// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <xml_ordered_extensions.hpp>
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>
#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;
constexpr std::string_view content{"<left>A</left><right id='7'/><left>B</left>"};
constexpr std::string_view suffix{"<v:suffix xmlns:v='urn:foreign'>opaque:Text</v:suffix>"};

// Author the same suffix around independent native content, without using a generated writer.
std::string document(std::string_view root, std::string_view known, std::string_view foreign) {
  return '<' + std::string{root} + '>' + std::string{known} + std::string{foreign} +
      "</" + std::string{root} + '>';
}

// Observe complete expanded child identities and mixed runs independently of native fragment storage.
template <typename Model>
void expect_round_trip(std::string_view source, std::size_t fragments) {
  xml::limits bounds;
  bounds.max_tree_bytes = 0U;
  bounds.max_mixed_fragments = fragments;
  const auto value = xml::deserialize_tokens<Model>(source, bounds);
  EXPECT_EQ(value.items.size(), fragments);
  const auto output = xml::serialize_tokens(value, bounds);
  xml::operation_context observation;
  observation.allow_mixed_content = true;
  const auto before = xml::parse(source, {}, &observation), after = xml::parse(output, {}, &observation);
  EXPECT_EQ(before.text, after.text);
  ASSERT_EQ(before.text_segments.size(), after.text_segments.size());
  for (std::size_t index{}; index < before.text_segments.size(); ++index) {
    EXPECT_EQ(before.text_segments[index].before_child, after.text_segments[index].before_child);
    EXPECT_EQ(before.text_segments[index].value, after.text_segments[index].value);
  }
  ASSERT_EQ(before.children.size(), after.children.size());
  for (std::size_t index{}; index < before.children.size(); ++index) {
    EXPECT_EQ(before.children[index].qualified_name, after.children[index].qualified_name);
    EXPECT_EQ(before.children[index].text, after.children[index].text);
  }
  EXPECT_EQ(xml::serialize_tokens(xml::deserialize_tokens<Model>(output, bounds), bounds), output);
  auto tight = bounds;
  tight.max_output_bytes = output.size() - 1U;
  std::string published{"previous"};
  EXPECT_THROW(published = xml::serialize_tokens(value, tight), std::length_error);
  EXPECT_EQ(published, "previous");
  EXPECT_EQ(xml::serialize_tokens(value, bounds), output);
}
}

// A required foreign suffix consumes no native fragment or native mixed-text allowance.
TEST(xml_ordered_extensions, suffix_keeps_native_cardinality_and_mixed_order) {
  expect_round_trip<ordered_extensions::root>(document("root", content, suffix), 3U);
  expect_round_trip<ordered_extensions::root>(document("root", "", suffix), 0U);
  expect_round_trip<ordered_extensions::fixed>(document("fixed", content, suffix), 3U);
  expect_round_trip<ordered_extensions::mixed>(document("mixed",
      "before<left>A</left>between<right id='7'/>after", suffix), 5U);
}

// End slots cannot move ahead of later known values or absorb text after foreign content.
TEST(xml_ordered_extensions, exact_positions_and_counts_reject_without_publication) {
  for (const auto& source : {document("root", content, ""),
      document("root", suffix, content),
      document("root", content, std::string{suffix} + "late"),
      document("root", content, "<unknown/>")}) {
    EXPECT_THROW((void)xml::deserialize_tokens<ordered_extensions::root>(source), std::invalid_argument);
  }
  EXPECT_THROW((void)xml::deserialize_tokens<ordered_extensions::root>(
      document("root", content, std::string{suffix} + std::string{suffix})), std::length_error);
  EXPECT_THROW((void)xml::deserialize_tokens<ordered_extensions::mixed>(
      document("mixed", "before<left>A</left>", std::string{suffix} + " ")), std::invalid_argument);
  ordered_extensions::root native{};
  std::string published{"previous"};
  EXPECT_THROW(published = xml::serialize_tokens(native), std::invalid_argument);
  EXPECT_EQ(published, "previous");
}
