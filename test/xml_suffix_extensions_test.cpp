// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <xml_suffix_extensions.hpp>
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;
constexpr std::string_view suffix{"<f:suffix f:attr='opaque'>unbound:opaque<f:nested/>tail</f:suffix>"};

// Author exact suffix positions independently of generated readers and writers.
std::string document(std::string_view root, std::string_view body, std::string_view attributes = "f:note='opaque'") {
  return '<' + std::string{root} + " xmlns:f='urn:foreign' " + std::string{attributes} + '>' +
      std::string{body} + "</" + std::string{root} + '>';
}

// Observe opaque content and owning values after both public XML entry points complete.
template <typename Model>
void verify_round_trip(const std::string& source) {
  for (const bool token : {false, true}) {
    const auto value = token ? xml::deserialize_tokens<Model>(source) : xml::deserialize<Model>(source);
    const auto output = token ? xml::serialize_tokens(value) : xml::serialize(value);
    const auto copy = xml::deserialize_tokens<Model>(output);
    EXPECT_EQ(xml::serialize_tokens(copy), output);
    xml::operation_context observation;
    observation.allow_mixed_content = true;
    const auto tree = xml::parse(output, {}, &observation);
    ASSERT_FALSE(tree.children.empty());
    const auto& foreign = tree.children.back();
    EXPECT_EQ(foreign.qualified_name, (xml::name{"suffix", "urn:foreign"}));
    EXPECT_EQ(foreign.text, "unbound:opaquetail");
    ASSERT_EQ(foreign.children.size(), 1U);
    EXPECT_EQ(foreign.children.front().qualified_name, (xml::name{"nested", "urn:foreign"}));
  }
}

// Failed documents never become a caller-visible model through either facade.
template <typename Model>
void reject(const std::string& source) {
  EXPECT_THROW((void)xml::deserialize<Model>(source), std::exception);
  EXPECT_THROW((void)xml::deserialize_tokens<Model>(source), std::exception);
}
} // namespace

// Unordered known members retain their grammar before one exact foreign suffix.
TEST(xml_suffix_extensions, all_group_permutations_and_boundaries) {
  for (const auto known : {"<first>A&amp;B</first><second>C</second>",
      "<second>C</second><first>A&amp;B</first>", "<first>A</first>"}) {
    verify_round_trip<suffix_models::all>(document("all", std::string{known} + std::string{suffix}));
  }
  for (const auto& body : {std::string{"<second>C</second>"} + std::string{suffix},
      std::string{"<first>A</first><first>B</first>"} + std::string{suffix},
      std::string{suffix} + "<first>A</first>",
      std::string{"<first>A</first>"} + std::string{suffix} + "<second>B</second>",
      std::string{"<first>A</first>"}, std::string{"<first>A</first>"} + std::string{suffix} + std::string{suffix},
      std::string{"<first>A</first>"} + std::string{suffix} + "late"}) {
    reject<suffix_models::all>(document("all", body));
  }
}

// Scalar prefix text preserves whitespace and cannot move text from after the suffix.
TEST(xml_suffix_extensions, scalar_prefix_preserves_exact_text_position) {
  for (const auto text : {"A&amp;B", "", " \t\n", "A<![CDATA[B]]><!--ignored--><?ignored x?>C"}) {
    const auto source = document("text", std::string{text} + std::string{suffix});
    verify_round_trip<prefix::text>(source);
    const auto value = xml::deserialize_tokens<prefix::text>(source);
    EXPECT_EQ(value.value, text == std::string_view{"A&amp;B"} ? "A&B" :
        text == std::string_view{"A<![CDATA[B]]><!--ignored--><?ignored x?>C"} ? "ABC" : text);
  }
  for (const auto& body : {std::string{"A"}, std::string{"A"} + std::string{suffix} + "late",
      std::string{"A"} + std::string{suffix} + " ", std::string{"A<unknown/>"} + std::string{suffix},
      std::string{"A"} + std::string{suffix} + std::string{suffix}}) {
    reject<prefix::text>(document("text", body));
  }
  reject<prefix::text>(document("text", std::string{"A"} + std::string{suffix}, "core='unknown'"));
}
