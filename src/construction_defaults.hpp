// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::construction {

// Render already checked scalar/record/array defaults without interpreting host expressions.
inline std::string initializer(const value_initializer& value, std::string_view language,
    const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(const member&)>& scalar,
    const std::function<std::string(std::string_view)>& callable = {}) {
  if (value.kind == value_initializer::kind_type::scalar) {
    member field{access_type::public_access, member::modifier_type::none, {value.type},
        "serializer_default", "serializer_default", 0, {}, value.value};
    field.magic = value.type.name == "char" && !value.value.empty() && value.value.front() != '\'';
    auto literal = scalar(field);
    if (language == "cpp") {
      return field.magic ? "static_cast<char>(" + literal + ")" : type(value.type) + "{" + literal + "}";
    }
    if (language == "rust" && literal.starts_with('(') && literal.ends_with(')')) {
      literal = literal.substr(1, literal.size() - 2);
    }
    return literal;
  }
  std::string arguments;
  for (const auto& element : value.elements) {
    if (!arguments.empty()) { arguments += ", "; }
    arguments += initializer(element, language, type, scalar, callable);
  }
  if (value.kind == value_initializer::kind_type::record) {
    const auto name = callable ? callable("serializer_make_default") : "serializer_make_default";
    auto owner = type(value.type);
    if (language == "go" && owner.starts_with('*')) { owner.erase(0, 1); }
    return owner + (language == "go" || language == "c" ? "_" :
        language == "cpp" || language == "rust" ? "::" : ".") + name + "(" + arguments + ")";
  }
  if (language == "cpp") {
    return "::std::array<" + type(value.type) + ", " + std::to_string(value.elements.size()) + ">{" + arguments + "}";
  }
  if (language == "java") { return "new " + type(value.type) + "[]{" + arguments + "}"; }
  if (language == "rust") { return "vec![" + arguments + "]"; }
  if (language == "go") { return "[]" + type(value.type) + "{" + arguments + "}"; }
  if (language == "csharp") { return "new System.Collections.Generic.List<" + type(value.type) + ">{" + arguments + "}"; }
  if (language == "kotlin") {
    const bool primitive = value.type.type == object_type::primitive && value.type.name != "string" && !value.type.is_digest();
    const auto kind = type(value.type);
    auto constructor = kind;
    std::transform(constructor.begin(), constructor.end(), constructor.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    constructor = primitive ? constructor + "ArrayOf" : "mutableListOf";
    return constructor + "(" + arguments + ")";
  }
  return "[" + arguments + "]";
}
} // namespace rohit::serializer::writer::construction
