// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include "unicode_license.hpp"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer {

// Recognize extra line terminators accepted by supported target-language comment lexers.
inline std::size_t notice_line_separator_bytes(std::string_view contents) {
  constexpr std::string_view next_line_utf8{"\xc2\x85"};
  constexpr std::string_view line_separator_utf8{"\xe2\x80\xa8"};
  constexpr std::string_view paragraph_separator_utf8{"\xe2\x80\xa9"};
  return contents.starts_with('\r') ? std::size_t{1}
      : contents.starts_with(next_line_utf8) ? next_line_utf8.size()
      : contents.starts_with(line_separator_utf8) ? line_separator_utf8.size()
      : contents.starts_with(paragraph_separator_utf8) ? paragraph_separator_utf8.size()
      : std::size_t{};
}

// Detect a backslash at the end of a target line, including compiler-tolerated trailing whitespace.
inline bool notice_continuation_backslash(std::string_view following) {
  const auto remaining = following.find_first_not_of(" \t\v\f");
  return remaining == std::string_view::npos ||
         notice_line_separator_bytes(following.substr(remaining)) != 0;
}

// Keep target-language line terminators and Java Unicode translation inside source comments.
inline void append_notice_line(std::string& output, std::string_view contents,
                               std::string_view comment_prefix, bool escape_java_unicode) {
  output += comment_prefix;
  output += ' ';
  std::size_t offset{};
  while (offset < contents.size()) {
    const auto remainder = contents.substr(offset);
    const auto newline_bytes = notice_line_separator_bytes(remainder);
    if (newline_bytes != 0) {
      // Schema line comments end at LF, but several target languages recognize more terminators.
      output += '\n';
      output += comment_prefix;
      output += ' ';
      offset += newline_bytes;
      continue;
    }
    if (contents[offset] == '\\' && (escape_java_unicode ||
        (comment_prefix == "//" && notice_continuation_backslash(remainder.substr(1))))) {
      // Java translates this once; other targets retain the safe escaped representation in comments.
      output += "\\u005c";
    } else {
      output += contents[offset];
    }
    ++offset;
  }
  output += '\n';
}

// Render preserved source text as comments, escaping control sequences without assigning a license.
inline std::string generated_source_notices(
    const std::shared_ptr<const std::vector<std::string>>& notices,
    std::string_view comment_prefix = "//", bool escape_java_unicode = false) {
  std::string result;
  if (notices && !notices->empty()) {
    for (const auto& notice : *notices) {
      std::size_t offset{};
      while (offset < notice.size()) {
        const auto end = notice.find('\n', offset);
        append_notice_line(result, std::string_view{notice}.substr(offset,
            end == std::string::npos ? end : end - offset), comment_prefix, escape_java_unicode);
        if (end == std::string::npos) { break; }
        offset = end + 1;
      }
      // End any line-continuation backslash in source text before generated code begins.
      result += '\n';
    }
  }
  return result;
}

// Preserve owner notices before minimal compiler identification without assigning an output license.
inline std::string generated_license_notice(
    const std::vector<std::unique_ptr<syntax_node>>& statements,
    std::string_view comment_prefix = "//", bool escape_java_unicode = false,
    bool includes_xml_support = false) {
  std::string result;
  const auto line = [&](std::string_view contents) {
    append_notice_line(result, contents, comment_prefix, escape_java_unicode);
  };
  std::set<std::string_view> emitted;
  std::set<const std::vector<std::string>*> visited;
  const auto visit = [&](const auto& self, const auto& values) -> void {
    for (const auto& value : values) {
      if (value->source_notices && visited.insert(value->source_notices.get()).second) {
        for (const auto& notice : *value->source_notices) {
          if (emitted.contains(notice)) {
            continue;
          }
          emitted.insert(notice);
          if (!result.empty()) { result += '\n'; }
          std::size_t offset{};
          while (offset < notice.size()) {
            const auto end = notice.find('\n', offset);
            line(std::string_view{notice}.substr(offset,
                end == std::string::npos ? end : end - offset));
            if (end == std::string::npos) {
              break;
            }
            offset = end + 1;
          }
        }
      }
      if (value->type == object_type::namespace_type) {
        self(self, static_cast<const namespace_node&>(*value).statements);
      }
    }
  };
  visit(visit, statements);
  if (!result.empty()) { result += '\n'; }
  line("Generated by Serializer. Do not edit this file manually.");
  line("https://github.com/rohit-singh-gautam/Serializer");
  if (includes_xml_support) {
    line("Third-party notice for Unicode-derived XML pattern character-range data only:");
    std::size_t offset{};
    while (offset < unicode_data_license.size()) {
      const auto end = unicode_data_license.find('\n', offset);
      line(unicode_data_license.substr(offset, end == std::string_view::npos ? end : end - offset));
      if (end == std::string_view::npos) { break; }
      offset = end + 1U;
    }
  }
  // A blank line also prevents a source notice ending in a backslash from consuming code.
  result += '\n';
  return result;
}

} // namespace rohit::serializer::writer
