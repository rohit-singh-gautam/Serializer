// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace rohit::serializer::literal_syntax {

// Recognize ASCII digits independently of locale and signed char representation.
[[nodiscard]] constexpr bool digit(char value) noexcept {
  return value >= '0' && value <= '9';
}

// Recognize the ASCII letters supported by schema/native identifiers.
[[nodiscard]] constexpr bool letter(char value) noexcept {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

// Check the backend's existing identifier alphabet without recursive regex state.
[[nodiscard]] constexpr bool identifier(std::string_view text, bool leading_underscore = false) noexcept {
  if (text.empty() || !(letter(text.front()) || (leading_underscore && text.front() == '_'))) {
    return false;
  }
  return std::all_of(text.begin() + 1, text.end(), [](char value) {
    return letter(value) || digit(value) || value == '_';
  });
}

// Recognize the existing unsuffixed decimal integer grammar with constant stack use.
[[nodiscard]] constexpr bool decimal_integer(std::string_view text) noexcept {
  if (text.starts_with('-')) { text.remove_prefix(1); }
  if (text.empty()) { return false; }
  if (text.front() == '0') { return text.size() == 1; }
  return text.front() >= '1' && text.front() <= '9' &&
      std::all_of(text.begin(), text.end(), digit);
}

// Recognize canonical decimal numbers, including optional fraction/exponent, in linear work.
[[nodiscard]] constexpr bool decimal_number(std::string_view text) noexcept {
  std::size_t offset = text.starts_with('-') ? 1U : 0U;
  if (offset == text.size() || !digit(text[offset])) { return false; }
  if (text[offset] == '0') { ++offset; }
  else { while (offset < text.size() && digit(text[offset])) { ++offset; } }
  if (offset < text.size() && text[offset] == '.') {
    ++offset;
    const auto start = offset;
    while (offset < text.size() && digit(text[offset])) { ++offset; }
    if (offset == start) { return false; }
  }
  if (offset < text.size() && (text[offset] == 'e' || text[offset] == 'E')) {
    ++offset;
    if (offset < text.size() && (text[offset] == '-' || text[offset] == '+')) { ++offset; }
    const auto start = offset;
    while (offset < text.size() && digit(text[offset])) { ++offset; }
    if (offset == start) { return false; }
  }
  return offset == text.size();
}

// Check the portable quoted-string escape subset while preserving every source byte.
[[nodiscard]] constexpr bool quoted_string(std::string_view text) noexcept {
  if (text.size() < 2 || text.front() != '"' || text.back() != '"') { return false; }
  const auto end = text.size() - 1;
  for (std::size_t offset = 1; offset < end; ++offset) {
    if (text[offset] == '"') { return false; }
    if (text[offset] == '\\') {
      ++offset;
      if (offset == end || std::string_view{"\"\\bfnrt"}.find(text[offset]) == std::string_view::npos) {
        return false;
      }
    }
  }
  return true;
}

} // namespace rohit::serializer::literal_syntax
