// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "construction_qualification.hpp"
#include <rohit/serializer_creator.hpp>
#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::arithmetic {

// Forward one declared operation through its shared typed method; forwarding adds no runtime dispatch.
inline std::string operators(const class_node& owner,
    const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(std::string_view)>& identifier) {
  std::string result;
  for (const auto& mapping : owner.arithmetic) {
    const auto& method = owner.functions.at(mapping.method_index);
    const auto* native = behavior::native_body(method, "cpp");
    const bool constant = behavior::constexpr_parameters(owner, method) &&
        (native || !method.expression ? method.cpp_constexpr ||
            method.cpp_prefix.find("constexpr") != std::string::npos ||
            (native && method.automatic_constexpr_enabled && construction::constexpr_construction(owner)) :
         behavior::constexpr_expression(*method.expression));
    std::string parameter, arguments;
    if (!mapping.unary) {
      parameter = type(method.parameters.front().type) + " serializer_arithmetic_rhs";
      arguments = "::std::move(serializer_arithmetic_rhs)";
    }
    const auto call = "this->" + identifier(method.name) + "(" + arguments + ")";
    result += "\npublic:\n  // Apply the explicitly mapped readonly operation with its exact result type.\n"
        "  [[nodiscard]] inline " + std::string{constant ? "constexpr " : ""} +
        type(method.return_type) + " operator" + mapping.symbol + "(" + parameter +
        ") const noexcept(noexcept(" + call + ")) { return " + call + "; }\n";
    if (!constant) {
      result += "  // Constant evaluation follows the mapped method's declared C++ body/storage contract.\n";
    }
  }
  return result;
}
} // namespace rohit::serializer::writer::arithmetic
