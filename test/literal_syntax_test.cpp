// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "../src/literal_syntax.hpp"

#include <cstddef>
#include <regex>
#include <string>
#include <string_view>

namespace {
namespace syntax = rohit::serializer::literal_syntax;

// Compare bounded spellings against the previous exact regex contracts, including rejected forms.
TEST(literal_syntax, preserves_existing_grammars) {
  const std::regex integer_pattern{R"(-?(0|[1-9][0-9]*))"};
  const std::regex number_pattern{R"(-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?)"};
  const std::regex string_pattern{"\"([^\"\\\\]|\\\\[\"\\\\bfnrt])*\""};
  const std::regex identifier_pattern{"[A-Za-z][A-Za-z_0-9]*"};
  const std::regex underscore_pattern{"[A-Za-z_][A-Za-z_0-9]*"};
  constexpr std::string_view alphabet{"01a-e.+_\"\\nr\n"};
  constexpr unsigned maximum_depth = 4;
  std::string text;
  // The small fixed-depth inventory exercises grammar compatibility without unbounded regex input.
  const auto enumerate = [&](const auto& self, unsigned remaining) -> void {
    EXPECT_EQ(syntax::decimal_integer(text), std::regex_match(text, integer_pattern)) << text;
    EXPECT_EQ(syntax::decimal_number(text), std::regex_match(text, number_pattern)) << text;
    EXPECT_EQ(syntax::quoted_string(text), std::regex_match(text, string_pattern)) << text;
    EXPECT_EQ(syntax::identifier(text), std::regex_match(text, identifier_pattern)) << text;
    EXPECT_EQ(syntax::identifier(text, true), std::regex_match(text, underscore_pattern)) << text;
    if (remaining == 0) { return; }
    for (const auto value : alphabet) {
      text.push_back(value);
      self(self, remaining - 1);
      text.pop_back();
    }
  };
  enumerate(enumerate, maximum_depth);
}

// Input size affects linear work and stored bytes, never recursive recognition depth.
TEST(literal_syntax, handles_large_literals_and_identifiers) {
  constexpr std::size_t input_bytes = 262144;
  const auto digits = std::string(input_bytes, '9');
  const auto letters = std::string(input_bytes, 'a');
  const auto number = "1." + std::string(input_bytes, '0');
  EXPECT_TRUE(syntax::decimal_integer(digits));
  EXPECT_TRUE(syntax::decimal_number(number));
  EXPECT_FALSE(syntax::decimal_number(number + 'F'));
  EXPECT_TRUE(syntax::identifier(letters));
  EXPECT_FALSE(syntax::identifier(letters + '-'));
  EXPECT_TRUE(syntax::quoted_string('"' + letters + '"'));
  EXPECT_FALSE(syntax::quoted_string('"' + letters + "\\x\""));
  EXPECT_FALSE(syntax::quoted_string('"' + letters + "\\\""));
}

// Source bytes including non-ASCII and literal controls retain the previous character-class policy.
TEST(literal_syntax, preserves_quoted_bytes_and_escape_boundaries) {
  for (const std::string& value : {std::string{"\"utf8 \xc3\xa9\""},
           std::string{"\"embedded\0byte\"", 15}, std::string{"\"\n\r\t\""},
           std::string{"\"\\b\\f\\n\\r\\t\\\\\\\"\""}}) {
    EXPECT_TRUE(syntax::quoted_string(value));
  }
  for (const std::string_view value : {"", "\"", "\"unterminated", "\"\\0\"", "\"\\u0041\"",
           "\"\\/\"", "\"inner\"quote\"", "\"trailing\\\""}) {
    EXPECT_FALSE(syntax::quoted_string(value));
  }
}

} // namespace
