// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {

// Check one complete singleton choice against its unchanged native optional alternatives.
// Call before output and before publishing an input candidate; this function retains no AST data.
inline std::string native_choice_checks(const xml_binding::record& contract,
    const std::function<std::string(const member&)>& access, std::string_view language) {
  std::string result;
  for (std::size_t index{}; index < contract.choices.size(); ++index) {
    const auto& group = contract.choices[index];
    const auto count = std::string{language == "c" || language == "rust" ? "srl_xml_choice_count_" : "srlXmlChoiceCount"} + std::to_string(index);
    std::string sum;
    for (const auto* source : group.alternatives) {
      if (!sum.empty()) { sum += " + "; }
      const auto value = access(*source);
      if (language == "kotlin") { sum += "(if (" + value + ".isPresent) 1 else 0)"; }
      else if (language == "swift") { sum += "(" + value + " == nil ? 0 : 1)"; }
      else if (language == "rust") { sum += "usize::from(" + value + ".is_some())"; }
      else { sum += "(" + value + ".has_value ? 1u : 0u)"; }
    }
    if (sum.empty()) { sum = "0"; }
    const auto lower = std::to_string(group.minimum), upper = std::to_string(group.maximum);
    if (language == "swift") {
      result += "let " + count + " = " + sum + "\nguard " + count + " >= " + lower + " && " + count + " <= " + upper +
          " else { throw SerializerError.invalid(\"XML choice occurrence mismatch\") }\n";
    } else if (language == "kotlin") {
      result += "val " + count + " = " + sum + "\nrequire(" + count + " >= " + lower + " && " + count + " <= " + upper +
          ") { \"XML choice occurrence mismatch\" }\n";
    } else if (language == "rust") {
      result += "let " + count + ":usize = " + sum + ";\nif ";
      if (group.minimum != 0U) { result += count + " < " + lower + " || "; }
      result += count + " > " + upper + " { return Err(Error(\"XML choice occurrence mismatch\")); }\n";
    } else {
      result += "const size_t " + count + " = " + sum + ";\nif (";
      if (group.minimum != 0U) { result += count + " < " + lower + "u || "; }
      result += count + " > " + upper + "u) { return false; }\n";
    }
  }
  return result;
}

} // namespace rohit::serializer::writer::xml_codegen
