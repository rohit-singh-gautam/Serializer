// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace rohit::serializer::callable_availability {

// Validate schema-only availability and substitute class arguments without inspecting native code.
inline void bind(callable_guard& guard, const class_node& owner,
    const std::unordered_map<std::string, type_name>& bindings = {}) {
  using operation = callable_guard::operation;
  const auto parameter = [&](std::string_view name, generic_argument_kind kind) {
    return std::any_of(owner.generic_parameters.begin(), owner.generic_parameters.end(),
        [&](const auto& value) { return value.name == name && value.kind == kind; });
  };
  const auto integer = [&](const auto& self, callable_guard& value) -> std::optional<std::uint64_t> {
    if (value.kind == operation::integer) { return value.value; }
    if (value.kind != operation::symbol) {
      throw std::invalid_argument{"Availability comparisons require class dimensions or unsigned literals"};
    }
    if (const auto found = bindings.find(value.name); found != bindings.end()) {
      if (found->second.kind != generic_argument_kind::dimension) {
        throw std::invalid_argument{"Availability requires a resolved dimension argument"};
      }
      value.kind = operation::integer;
      value.value = found->second.dimension;
      value.name.clear();
      return value.value;
    }
    if (!parameter(value.name, generic_argument_kind::dimension)) {
      throw std::invalid_argument{"Availability comparisons require class dimensions or unsigned literals"};
    }
    static_cast<void>(self);
    return {};
  };
  const auto type = [&](type_name& value) -> std::optional<std::string> {
    bool substituted{};
    if (const auto found = bindings.find(value.name); found != bindings.end()) {
      value = found->second;
      substituted = true;
    }
    const auto identity = [&](const auto& self, const type_name& argument) -> std::string {
      if (argument.kind == generic_argument_kind::dimension) { return std::to_string(argument.dimension); }
      // Digest policies share their native byte container; fixed extents still distinguish C++ types.
      if (argument.is_digest() || (!argument.resolved_node && !argument.application && argument.name == "digest")) {
        return argument.digest_extent ? "fixed_bytes<" + std::to_string(argument.digest_extent) + ">" :
            "array<" + serializer::get_cpp_type_or_empty("uint8") + ",>";
      }
      if (!argument.resolved_node && !argument.application &&
          !serializer::get_cpp_type_or_empty(argument.name).empty()) {
        return serializer::get_cpp_type_or_empty(argument.name);
      }
      auto result = argument.application && !argument.resolved_node ? argument.name : argument.get_full_name();
      if (argument.application) {
        result += '<';
        for (const auto& item : argument.arguments) { result += self(self, item) + ','; }
        result += '>';
      }
      return result;
    };
    if (value.resolved_node || value.application || value.is_digest()) { return identity(identity, value); }
    if (!substituted && parameter(value.name, generic_argument_kind::type)) { return {}; }
    if (!value.application && !serializer::get_cpp_type_or_empty(value.name).empty()) { return identity(identity, value); }
    if (substituted) { return identity(identity, value); }
    throw std::invalid_argument{"same_type requires a class type parameter or a primitive type"};
  };
  const auto boolean = [&](const auto& self, callable_guard& value) -> std::optional<bool> {
    std::optional<bool> result;
    if (value.kind == operation::type_equal) {
      const auto left = type(value.left_type), right = type(value.right_type);
      if (left && right) { result = *left == *right; }
    } else if (value.kind == operation::logical_not) {
      const auto operand = self(self, value.operands.at(0));
      if (operand) { result = !*operand; }
    } else if (value.kind == operation::logical_and || value.kind == operation::logical_or) {
      const auto left = self(self, value.operands.at(0)), right = self(self, value.operands.at(1));
      if (left && right) { result = value.kind == operation::logical_and ? *left && *right : *left || *right; }
      else if (value.kind == operation::logical_and && ((left && !*left) || (right && !*right))) { result = false; }
      else if (value.kind == operation::logical_or && ((left && *left) || (right && *right))) { result = true; }
    } else {
      const auto left = integer(integer, value.operands.at(0)), right = integer(integer, value.operands.at(1));
      if (left && right) {
        switch (value.kind) {
        case operation::equal: result = *left == *right; break;
        case operation::not_equal: result = *left != *right; break;
        case operation::less: result = *left < *right; break;
        case operation::less_equal: result = *left <= *right; break;
        case operation::greater: result = *left > *right; break;
        case operation::greater_equal: result = *left >= *right; break;
        default: throw std::invalid_argument{"Availability requires a Boolean comparison"};
        }
      }
    }
    value.available = result;
    return result;
  };
  static_cast<void>(boolean(boolean, guard));
}

// Emit dependent C++ constraints; closed false declarations are omitted by their writers.
inline std::string cpp(const callable_guard& value,
    const std::function<std::string(const type_name&)>& type) {
  using operation = callable_guard::operation;
  if (value.kind == operation::integer) { return std::to_string(value.value); }
  if (value.kind == operation::symbol) { return value.name; }
  if (value.kind == operation::type_equal) {
    return "::std::same_as<" + type(value.left_type) + ", " + type(value.right_type) + ">";
  }
  if (value.kind == operation::logical_not) { return "!(" + cpp(value.operands.at(0), type) + ")"; }
  const auto token = value.kind == operation::equal ? " == " : value.kind == operation::not_equal ? " != " :
      value.kind == operation::less ? " < " : value.kind == operation::less_equal ? " <= " :
      value.kind == operation::greater ? " > " : value.kind == operation::greater_equal ? " >= " :
      value.kind == operation::logical_and ? " && " : " || ";
  return "(" + cpp(value.operands.at(0), type) + token + cpp(value.operands.at(1), type) + ")";
}

} // namespace rohit::serializer::callable_availability
