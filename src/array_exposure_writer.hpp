// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::array_exposure {

// Forward native access without copies; logical helpers additionally check every coordinate before flattening.
// An optional section callback shares access changes with the containing C++ class emitter.
inline std::string methods(const member& field, std::string_view storage_type,
    std::string_view element_type, std::string_view field_name,
    bool value_constexpr,
    const std::function<std::string(const dimension_expression&)>& dimension,
    const std::function<std::string(std::string_view)>& callable,
    const std::function<std::string(access_type)>& cpp_access_section = {}) {
  if (!field.array_exposure) { return {}; }
  const auto& options = *field.array_exposure;
  const std::string target = "this->" + std::string{field_name};
  const std::string element{element_type}, storage{storage_type};
  if (field.extent_expression.empty() && element == "bool") {
    throw std::invalid_argument{"C++ array exposure does not support noncontiguous vector<bool> storage"};
  }
  const auto constant_result = options.const_value ? element : "const " + element + "&";
  const auto constant_qualifier = !options.const_value || value_constexpr ? "constexpr " : "";
  const auto visibility = options.access == access_type::public_access ? "public" :
      options.access == access_type::protected_access ? "protected" : "private";
  std::string output = "\n" + (cpp_access_section ? cpp_access_section(options.access) :
      std::string{visibility} + ":\n") +
      "  // Borrow original storage; the owner must outlive references, pointers and iterators.\n";
  if (field.extent_expression.empty()) {
    // vector<bool> has no contiguous data operation; no reference/proxy approximation is permitted.
    output += "  static_assert(requires(" + storage + "& srl_array) { srl_array.data(); }, "
        "\"Array exposure requires native contiguous element storage\");\n";
  }
  if (!options.dimensions.empty()) {
    std::string product = dimension(options.dimensions.front());
    for (std::size_t index{1}; index < options.dimensions.size(); ++index) {
      product = "::rohit::serializer::detail::dimension_multiply(" + product + ", " +
          dimension(options.dimensions[index]) + ")";
    }
    output += "  static_assert(" + product + " == ::std::tuple_size_v<" + storage +
        ">, \"Array dimensions must match fixed backing storage\");\n";
  }
  if (!options.readonly) {
    output += "  // Index one element with the native container's unchecked precondition.\n"
        "  constexpr " + element + "& operator[](::std::size_t index) { return " + target + "[index]; }\n";
  }
  output += "  // Preserve the selected const reference or explicit const-value indexing policy.\n"
      "  " + std::string{constant_qualifier} + constant_result + " operator[](::std::size_t index) const { return " + target + "[index]; }\n";
  std::string coordinates, offset, invalid;
  if (options.dimensions.empty()) {
    coordinates = "::std::size_t index";
    offset = "index";
    invalid = "index >= " + target + ".size()";
  } else {
    for (std::size_t index{}; index < options.dimensions.size(); ++index) {
      const auto argument = "srl_index_" + std::to_string(index);
      if (!coordinates.empty()) { coordinates += ", "; invalid += " || "; }
      coordinates += "::std::size_t " + argument;
      invalid += argument + " >= " + dimension(options.dimensions[index]);
      offset = offset.empty() ? argument : "(" + offset + " * " +
          dimension(options.dimensions[index]) + " + " + argument + ")";
    }
    if (!options.readonly) {
      output += "  // Flatten explicitly declared coordinates in row-major order; indices must be in range.\n"
          "  constexpr " + element + "& operator()(" + coordinates + ") { return " + target + "[" + offset + "]; }\n";
    }
    output += "  // Preserve native unchecked multi-index access and the selected const return policy.\n"
        "  " + std::string{constant_qualifier} + constant_result + " operator()(" + coordinates + ") const { return " + target + "[" + offset + "]; }\n";
  }
  const auto check = "    if (" + invalid + ") { throw ::std::out_of_range{\"Array index out of range\"}; }\n";
  if (!options.readonly) {
    output += "  // Borrow one mutable element after checking every logical coordinate.\n"
        "  constexpr " + element + "& " + callable("get") + "(" + coordinates + ") {\n" + check +
        "    return " + target + "[" + offset + "];\n  }\n";
  }
  output += "  // Observe one element after checking every coordinate; const_value deliberately copies it.\n"
      "  " + std::string{constant_qualifier} + constant_result + " " + callable("get") + "(" + coordinates + ") const {\n" + check +
      "    return " + target + "[" + offset + "];\n  }\n";
  if (!options.readonly) {
    output += "  // Replace a checked element using its native move assignment or available copy assignment.\n"
        "  " + std::string{value_constexpr ? "constexpr " : ""} + "void " + callable("set") + "(" + coordinates + ", " + element + " srl_value) {\n" + check +
        "    if constexpr (::std::is_move_assignable_v<" + element + ">) {\n"
        "      " + target + "[" + offset + "] = ::std::move(srl_value);\n"
        "    } else {\n      " + target + "[" + offset + "] = ::std::as_const(srl_value);\n    }\n  }\n";
  }
  for (const std::string_view method : {"begin", "end", "data"}) {
    if (!options.readonly) {
      output += "  // Borrow the native mutable range endpoint or contiguous storage.\n"
          "  constexpr auto " + std::string{method} + "() { return " + target + "." + std::string{method} + "(); }\n";
    }
    output += "  // Borrow the native const range endpoint or contiguous storage.\n"
        "  constexpr auto " + std::string{method} + "() const { return " + target + "." + std::string{method} + "(); }\n";
  }
  output += "  // Observe the native logical length without constructing or copying elements.\n"
      "  constexpr ::std::size_t size() const { return " + target + ".size(); }\n"
      "  // Borrow a const iteration range over the original container; writes can invalidate it.\n"
      "  constexpr const " + storage + "& " + callable("values") + "() const { return " + target + "; }\n"
      "  // Preserve the implicit const-container borrowing conversion and ordinary temporary-owner preconditions.\n"
      "  constexpr operator const " + storage + "&() const { return " + target + "; }\n";
  return output;
}

} // namespace rohit::serializer::writer::array_exposure
