// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/xml_pattern.hpp>

#include <gtest/gtest.h>

#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xml = rohit::serializer::xml;

// Compare finite ASCII language corpora against an independent standard-library regex implementation.
TEST(xml_pattern, exhaustive_ascii_corpus_agrees_with_independent_regex) {
  const std::vector<std::string> expressions{"", "a", "a|b", "(a|b)*", "a?b+", "[ab01]{1,3}",
      "[^ab]{0,2}", "(ab|a){2,}", "a.*b", "(a?)*", "[0-1]+", "(a|){0,3}"};
  std::vector<std::string> values{std::string{}};
  std::vector<std::string> level{std::string{}};
  for (std::size_t length{}; length < 5U; ++length) {
    std::vector<std::string> next{};
    for (const auto& prefix : level) {
      for (const auto letter : std::string_view{"ab01"}) { next.push_back(prefix + letter); }
    }
    values.insert(values.end(), next.begin(), next.end());
    level = std::move(next);
  }
  for (const auto& expression : expressions) {
    const auto portable = xml::compile_portable_pattern(expression);
    const std::regex reference{expression, std::regex_constants::ECMAScript};
    for (const auto& value : values) {
      EXPECT_EQ(portable.matches(value), std::regex_match(value, reference)) << expression << ": " << value;
    }
  }
}

// Exercise Unicode code points, XML-specific anchors and dot semantics against libxml2 when available.
TEST(xml_pattern, unicode_and_xsd_semantics_agree_with_provider) {
  const std::vector<std::pair<std::string, std::vector<std::string>>> corpus{
      {"[\xc3\xa9-\xc3\xab]{1,2}", {"\xc3\xa9", "\xc3\xaa\xc3\xab", "e", ""}},
      {".", {"\xf0\x9f\x98\x80", "\xc3\xa9", "a", "\n", "\r", ""}},
      {"^a$", {"^a$", "a", "x^a$"}},
      {"[^a]{1,2}", {"\xc3\xa9", "\n", "\xf0\x9f\x98\x80", "a"}},
      {"\\[a\\]", {"[a]", "a"}},
      {"(a?)*", {"", "a", "aaaa", "b"}}};
  for (const auto& [expression, values] : corpus) {
    const auto portable = xml::compile_portable_pattern(expression);
    if (xml::available()) {
      const xml::prepared_pattern reference{expression};
      for (const auto& value : values) { EXPECT_EQ(portable.matches(value), reference.matches(value)) << expression; }
    }
  }
  EXPECT_TRUE(xml::compile_portable_pattern(".").matches("\xf0\x9f\x98\x80"));
  EXPECT_FALSE(xml::compile_portable_pattern(".").matches("\n"));
  EXPECT_TRUE(xml::compile_portable_pattern("^a$").matches("^a$"));
  EXPECT_FALSE(xml::compile_portable_pattern("^a$").matches("a"));
}

// Unsupported dialect constructs fail at compilation rather than receiving different host meanings.
TEST(xml_pattern, unsupported_and_malformed_expressions_reject) {
  for (const auto expression : {"\\p{L}", "\\d", "\\i", "\\w", "\\1", "(?=a)", "(?:a)",
      "[a-z-[aeiou]]", "[a", "[]", "[z-a]", "(", "a)", "*a", "a**", "a{2,1}", "a{,2}", "a{2", "a\\"}) {
    EXPECT_THROW((void)xml::compile_portable_pattern(expression), std::invalid_argument) << expression;
  }
}

// Bounded and nullable repetitions preserve exact cardinality without recursive backtracking.
TEST(xml_pattern, repetition_intervals_and_empty_branches) {
  EXPECT_TRUE(xml::compile_portable_pattern("a{0}").matches(""));
  EXPECT_FALSE(xml::compile_portable_pattern("a{0}").matches("a"));
  const auto bounded = xml::compile_portable_pattern("(ab){2,3}");
  EXPECT_FALSE(bounded.matches("ab"));
  EXPECT_TRUE(bounded.matches("abab"));
  EXPECT_TRUE(bounded.matches("ababab"));
  EXPECT_FALSE(bounded.matches("abababab"));
  EXPECT_TRUE(xml::compile_portable_pattern("(a?|b*)*").matches("aaabbbab"));
  EXPECT_TRUE(xml::compile_portable_pattern("|a").matches(""));
}

// Reject invalid XML/UTF-8 even when an earlier input character already made matching impossible.
TEST(xml_pattern, invalid_unicode_never_becomes_a_plain_mismatch) {
  const auto pattern = xml::compile_portable_pattern("a");
  for (const auto& value : {std::string{"b\xc0\xaf"}, std::string{"\xed\xa0\x80"},
      std::string{"\xf4\x90\x80\x80"}, std::string{"\xe2\x82"}, std::string{"\0", 1U}}) {
    EXPECT_THROW((void)pattern.matches(value), std::invalid_argument);
    EXPECT_THROW((void)xml::compile_portable_pattern(value), std::invalid_argument);
  }
}

// Every configurable resource boundary fails before unchecked compilation or matching growth.
TEST(xml_pattern, compilation_and_match_limits_are_enforced) {
  xml::pattern_limits limits{};
  limits.max_expression_bytes = 1U;
  EXPECT_THROW((void)xml::compile_portable_pattern("aa", limits), std::length_error);
  limits = {};
  limits.max_nesting_depth = 1U;
  EXPECT_THROW((void)xml::compile_portable_pattern("((a))", limits), std::length_error);
  limits = {};
  limits.max_repetitions = 3U;
  EXPECT_THROW((void)xml::compile_portable_pattern("a{4}", limits), std::length_error);
  EXPECT_THROW((void)xml::compile_portable_pattern("a{999999999999999999999999}", limits), std::length_error);
  limits = {};
  limits.max_states = 3U;
  EXPECT_THROW((void)xml::compile_portable_pattern("(ab){3}", limits), std::length_error);
  limits = {};
  limits.max_input_bytes = 1U;
  const auto pattern = xml::compile_portable_pattern("a*");
  EXPECT_THROW((void)pattern.matches("aa", limits), std::length_error);
  limits = {};
  limits.max_work_units = 2U;
  EXPECT_THROW((void)pattern.matches("aaaa", limits), std::length_error);
}

// Tight source/group limits do not count implementation-only empty or repetition AST levels.
TEST(xml_pattern, tight_source_and_group_limits_accept_valid_programs) {
  xml::pattern_limits limits{};
  limits.max_expression_bytes = 0U;
  limits.max_nesting_depth = 0U;
  EXPECT_TRUE(xml::compile_portable_pattern("", limits).matches(""));
  limits.max_expression_bytes = 2U;
  EXPECT_TRUE(xml::compile_portable_pattern("a*", limits).matches("aaa"));
  EXPECT_THROW((void)xml::compile_portable_pattern("(a)", limits), std::length_error);
}

// Caller interruption and failures propagate through both APIs without becoming regex mismatches.
TEST(xml_pattern, caller_budget_exceptions_and_cancellation_propagate) {
  xml::operation_context context{};
  context.charge = [](auto, auto) { throw std::runtime_error{"caller budget"}; };
  EXPECT_THROW((void)xml::compile_portable_pattern("a", {}, &context), std::runtime_error);
  const auto pattern = xml::compile_portable_pattern("a");
  EXPECT_THROW((void)pattern.matches("a", {}, &context), std::runtime_error);
  context.charge = {};
  context.is_cancelled = [] { return true; };
  EXPECT_THROW((void)pattern.matches("a", {}, &context), std::runtime_error);
}
