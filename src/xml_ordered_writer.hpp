// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_content_contract.hpp"

#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Keep static transition metadata shared by dynamic runtimes, without per-operation allocations.
inline std::string ordered_table(const xml_binding::mixed_contract& mixed) {
  if (!mixed.grammar) { return "[]"; }
  std::string result{"["};
  for (const auto& row : mixed.grammar->transitions) {
    result += '[';
    for (const auto target : row) { result += std::to_string(target) + ','; }
    result += "],";
  }
  return result + ']';
}

// List accepting state identities using syntax shared by Python and JavaScript metadata.
inline std::string ordered_accepting(const xml_binding::mixed_contract& mixed) {
  std::string result{"["};
  if (mixed.grammar) {
    for (std::size_t index{}; index < mixed.grammar->accepting.size(); ++index) {
      if (mixed.grammar->accepting[index]) { result += std::to_string(index) + ','; }
    }
  }
  return result + ']';
}

// Declare one constant-size cursor only for fixed-XSD ordered content.
inline std::string ordered_start(const xml_binding::mixed_contract& mixed, std::string_view language) {
  if (!mixed.grammar) { return {}; }
  if (language == "rust") { return "let mut srl_ordered_state=1usize;\n"; }
  if (language == "kotlin" || language == "swift") { return "var srl_ordered_state = 1\n"; }
  if (language == "go") { return "srl_ordered_state := 1\n"; }
  return "int srl_ordered_state=1;\n";
}

// Advance a statically compiled cursor; a failed transition returns through the ordinary codec path.
inline std::string ordered_step(const xml_binding::mixed_contract& mixed, std::string_view symbol,
    std::string_view language, std::string_view failure) {
  if (!mixed.grammar) { return {}; }
  const auto width = mixed.children.size();
  const auto key = "srl_ordered_state * " + std::to_string(width) + " + " + std::string{symbol};
  const bool rust = language == "rust", kotlin = language == "kotlin", swift = language == "swift";
  const bool go = language == "go";
  std::string result = rust ? "srl_ordered_state=match " + key + " {\n" :
      kotlin ? "srl_ordered_state = when (" + key + ") {\n" :
      swift || go ? "switch " + key + " {\n" : "switch (" + key + ") {\n";
  for (std::size_t state{}; state < mixed.grammar->transitions.size(); ++state) {
    const auto& row = mixed.grammar->transitions[state];
    for (std::size_t index{}; index < row.size(); ++index) {
      if (row[index] == 0U) { continue; }
      const auto selector = std::to_string(state * width + index);
      const auto target = std::to_string(row[index]);
      result += rust ? selector + " => " + target + ",\n" :
          kotlin ? selector + " -> " + target + '\n' :
          "case " + selector + ": srl_ordered_state=" + target +
              (swift || go ? "\n" : "; break;\n");
    }
  }
  result += rust ? "_ => {" + std::string{failure} + "} };\n" :
      kotlin ? "else -> {" + std::string{failure} + "}\n}\n" :
      "default: " + std::string{failure} + "\n}\n";
  return result;
}

// Require an accepting cursor at the publication boundary, including empty-content minimums.
inline std::string ordered_finish(const xml_binding::mixed_contract& mixed,
    std::string_view language, std::string_view failure) {
  if (!mixed.grammar) { return {}; }
  std::string invalid;
  for (std::size_t index{}; index < mixed.grammar->accepting.size(); ++index) {
    if (!mixed.grammar->accepting[index]) { continue; }
    if (!invalid.empty()) { invalid += " && "; }
    invalid += "srl_ordered_state != " + std::to_string(index);
  }
  if (invalid.empty()) { return std::string{failure} + '\n'; }
  return "if " + std::string{language == "swift" || language == "go" || language == "rust" ? "" : "("} +
      invalid + std::string{language == "swift" || language == "go" || language == "rust" ? "" : ")"} +
      " { " + std::string{failure} + " }\n";
}
} // namespace rohit::serializer::writer::xml_codegen
