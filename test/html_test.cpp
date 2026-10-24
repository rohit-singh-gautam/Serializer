// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <gtest/gtest.h>
#include <rohit/html.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace html = rohit::serializer::html;

// Subscript and superscript tags retain nesting, escaping and ordinary case folding.
TEST(html, subscript_and_superscript_use_the_closed_phrasing_contract) {
  const auto events = html::parse_fragment(
      "<P>H<SUB title='index'>2</SUB>O; x<SUP>2<em>nd</em></SUP> &amp; y<sub><sup>n</sup></sub></P>");
  EXPECT_EQ(html::write_fragment(events),
      "<p>H<sub title=\"index\">2</sub>O; x<sup>2<em>nd</em></sup> &amp; y<sub><sup>n</sup></sub></p>");
  for (const auto* invalid : {"<sub><div>x</div></sub>", "<sup><p>x</p></sup>", "<sub/>", "<sup>unclosed"}) {
    EXPECT_THROW((void)html::parse_fragment(invalid), std::invalid_argument);
  }
}

// HTML case-folding, unquoted/boolean attributes and void tags differ from XML by design.
TEST(html, ordinary_html_subset_round_trips) {
  const auto events = html::parse_fragment(
      "<DIV hidden class=card><P>A&nbsp;&amp; B<br><IMG src='x.png' alt=picture></P></DIV>");
  EXPECT_EQ(html::write_fragment(events), "<div hidden class=\"card\"><p>A\xc2\xa0&amp; B<br><img "
                                          "src=\"x.png\" alt=\"picture\"></p></div>");
  EXPECT_FALSE(html::supported_subset().empty());
}

// HTML numeric entities and CRLF handling preserve semantic character values through canonical output.
TEST(html, unicode_entities_and_comments) {
  const auto events =
      html::parse_fragment("<!-- ignored --><span title=\"&#x1F642;\">x\r\ny&#13;&lt;</span>");
  const auto output = html::write_fragment(events);
  EXPECT_NE(output.find("x\ny&#13;&lt;"), std::string::npos);
  EXPECT_EQ(html::write_fragment(html::parse_fragment(output)), output);
}

// Browser recovery, foreign namespaces, raw text and XML-only syntax remain outside this HTML subset.
TEST(html, unsupported_browser_recovery_and_xml_declarations_reject) {
  for (const auto* input :
       {"<p>one<p>two", "<p><div>x</div></p>", "<ul><div>x</div></ul>", "<li>x</li>",
        "<a><a>x</a></a>", "<span/>", "<br></br>", "<!DOCTYPE html><p>x</p>",
        "<?xml version='1.0'?><p>x</p>", "<svg/>", "<script>x</script>", "<p onclick=x>x</p>",
        "<div ID=x id=y></div>", "<p>&unknown;</p>", "<p>&#0;</p>", "<p>&#128;</p>"}) {
    EXPECT_THROW((void)html::parse_fragment(input), std::invalid_argument) << input;
  }
}

// Input/output/depth/event bounds and shared cancellation fail before returning any candidate.
TEST(html, resource_bounds_and_interruption) {
  html::limits bounds{};
  bounds.max_input_bytes = 3U;
  EXPECT_THROW((void)html::parse_fragment("<br>", bounds), std::length_error);
  bounds = {};
  bounds.max_depth = 1U;
  EXPECT_THROW((void)html::parse_fragment("<div><span>x</span></div>", bounds), std::length_error);
  bounds = {};
  bounds.max_events = 2U;
  EXPECT_THROW((void)html::parse_fragment("x", bounds), std::length_error);
  const auto events = html::parse_fragment("<p>text</p>");
  bounds = {};
  bounds.max_output_bytes = 1U;
  EXPECT_THROW((void)html::write_fragment(events, bounds), std::length_error);
  auto attributed = events;
  attributed[1].attributes.push_back({"title", std::string(64U, 'x'), false});
  attributed[1].attributes.front().value.front() = '\xc0';
  bounds.max_output_bytes = 20U;
  // The impossible output size is rejected before inspecting even malformed attribute bytes.
  EXPECT_THROW((void)html::write_fragment(attributed, bounds), std::length_error);
  rohit::serializer::xml::operation_context cancelled{};
  cancelled.is_cancelled = [] { return true; };
  EXPECT_THROW((void)html::parse_fragment("<br>", {}, &cancelled), std::exception);
}

// Hand-authored event traces cannot bypass structural and attribute/content validation.
TEST(html, malformed_event_traces_reject) {
  auto events = html::parse_fragment("<div>text</div>");
  events[3].name = "span";
  EXPECT_THROW((void)html::write_fragment(events), std::invalid_argument);
  EXPECT_THROW((void)html::parse_fragment(std::string{"\xc0\x80", 2U}), std::invalid_argument);
  EXPECT_THROW((void)html::parse_fragment(std::string{"\0", 1U}), std::invalid_argument);
}
