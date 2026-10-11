// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "behavior_writer.hpp"
#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace rohit::serializer::writer::construction {

// Prove known storage supports C++20 construction; dependent values defer to specialization.
inline bool constexpr_construction(const class_node& owner,
    const std::vector<function_parameter>& parameters = {},
    const std::vector<generic_parameter>& input_types = {}) {
  std::set<const class_node*> active;
  std::function<bool(const class_node&)> check_owner;
  const auto check_type = [&](const type_name& value, const class_node& scope, const auto& recurse) -> bool {
    if (value.common_numeric) {
      return std::all_of(value.arguments.begin(), value.arguments.end(),
          [&](const auto& argument) { return recurse(argument, scope, recurse); });
    }
    if (value.is_associative()) { return false; }
    if (value.is_optional() || value.is_sequence()) {
      return std::all_of(value.arguments.begin(), value.arguments.end(),
          [&](const auto& argument) { return recurse(argument, scope, recurse); });
    }
    if (value.type == object_type::primitive || value.type == object_type::enum_type) { return true; }
    if (value.resolved_node && (value.resolved_node->type == object_type::class_type ||
                               value.resolved_node->type == object_type::generic_definition)) {
      return check_owner(static_cast<const class_node&>(*value.resolved_node));
    }
    return std::find(scope.type_parameters.begin(), scope.type_parameters.end(), value.name) != scope.type_parameters.end() ||
        std::any_of(input_types.begin(), input_types.end(), [&](const auto& input) { return input.name == value.name; });
  };
  check_owner = [&](const class_node& value) {
    if (value.supports_managed() ||
        std::any_of(value.native_blocks.begin(), value.native_blocks.end(), [](const auto& block) {
          return block.language == "cpp" && !block.no_instance_storage;
        }) || value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        std::any_of(value.functions.begin(), value.functions.end(), [](const auto& method) {
          return method.dispatch != function_declaration::dispatch_type::ordinary;
        }) || !active.insert(&value).second) { return false; }
    // Cyclic storage still needs a runtime lifetime contract even if a container breaks the cycle.
    const bool result = std::all_of(value.parents.begin(), value.parents.end(), [&](const auto& base) {
      return base.parent_class && check_owner(*base.parent_class);
    }) && std::all_of(value.member_list.begin(), value.member_list.end(), [&](const auto& field) {
      return field.modifier != member::modifier_type::map && field.modifier != member::modifier_type::variant &&
          std::all_of(field.type_name_list.begin(), field.type_name_list.end(), [&](const auto& type) {
            return check_type(type, value, check_type);
          });
    });
    active.erase(&value);
    return result;
  };
  return check_owner(owner) && std::all_of(parameters.begin(), parameters.end(), [&](const auto& parameter) {
    return check_type(parameter.type, owner, check_type);
  });
}

// Prove concrete scalar storage/defaults cannot throw; native blocks never establish this proof.
inline bool nothrow_construction(const class_node& owner,
    const std::vector<function_parameter>& parameters = {}) {
  if (!owner.native_blocks.empty() || !constexpr_construction(owner, parameters)) { return false; }
  const auto check_type = [&](const type_name& value) -> bool {
    if (value.is_optional() || value.is_sequence() || value.is_associative()) { return false; }
    if (value.type == object_type::primitive) {
      return value.name != "string" && (!value.is_digest() || value.digest_extent != 0);
    }
    if (value.type == object_type::enum_type) { return true; }
    if (value.resolved_node && (value.resolved_node->type == object_type::class_type ||
                               value.resolved_node->type == object_type::generic_definition)) {
      return nothrow_construction(static_cast<const class_node&>(*value.resolved_node));
    }
    return false;
  };
  return std::all_of(owner.parents.begin(), owner.parents.end(), [&](const auto& base) {
    return base.parent_class && nothrow_construction(*base.parent_class);
  }) && std::all_of(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) {
    if ((field.modifier != member::modifier_type::none &&
         (field.modifier != member::modifier_type::array || field.fixed_extent == 0)) ||
        field.type_name_list.size() != 1 || !check_type(field.type_name_list.front())) { return false; }
    if (!field.default_value.empty()) {
      try {
        static_cast<void>(native::literal(field));
      } catch (const std::invalid_argument&) {
        return false; // Opaque initializers retain their authored exception contract.
      }
    }
    return true;
  }) && std::all_of(parameters.begin(), parameters.end(), [&](const auto& parameter) {
    return check_type(parameter.type);
  });
}

} // namespace rohit::serializer::writer::construction

namespace rohit::serializer::writer::behavior {

// Prove known value types are C++20 constant-capable; open template types defer to specialization.
inline bool constexpr_type(const class_node& owner, const type_name& value,
    const std::vector<generic_parameter>& input_types = {}) {
  const auto eligible = [&](const type_name& type, const auto& self) -> bool {
    if (type.common_numeric) {
      return std::all_of(type.arguments.begin(), type.arguments.end(),
          [&](const auto& argument) { return self(argument, self); });
    }
    if (type.is_associative()) { return false; }
    if (type.is_optional() || type.is_sequence()) { return self(type.arguments.front(), self); }
    if (type.type == object_type::primitive || type.type == object_type::enum_type) { return true; }
    if (type.resolved_node && (type.resolved_node->type == object_type::class_type ||
                              type.resolved_node->type == object_type::generic_definition)) {
      return construction::constexpr_construction(static_cast<const class_node&>(*type.resolved_node));
    }
    return std::find(owner.type_parameters.begin(), owner.type_parameters.end(), type.name) != owner.type_parameters.end() ||
        std::any_of(input_types.begin(), input_types.end(), [&](const auto& input) { return input.name == type.name; });
  };
  return eligible(value, eligible);
}

// Check returned/parameter values without restricting unrelated borrowed receiver storage.
inline bool constexpr_parameters(const class_node& owner, const function_declaration& method) {
  return constexpr_type(owner, method.return_type, method.generic_parameters) &&
      std::all_of(method.parameters.begin(), method.parameters.end(),
          [&](const auto& parameter) { return constexpr_type(owner, parameter.type, method.generic_parameters); });
}

// Follow typed callees and explicit native contracts without interpreting their C++ tokens.
inline bool constexpr_expression(const behavior_expression& node,
    std::set<const function_declaration*>& active) {
  using operation = behavior_expression::operation;
  switch (node.kind) {
  case operation::square_root: case operation::sine: case operation::cosine: return false;
  case operation::call: {
    const auto& method = called_function(node);
    if (method.dispatch != function_declaration::dispatch_type::ordinary ||
        method.effect != function_declaration::effect_type::readonly) { return false; }
    static const class_node free_owner{object_type::class_type, std::string{}, nullptr,
        class_attributes::none, {}};
    const auto& owner = node.call_target->type == object_type::function_type ? free_owner :
        static_cast<const class_node&>(*node.call_target);
    if (!constexpr_parameters(owner, method)) { return false; }
    if (native_body(method, "cpp")) {
      if (!method.cpp_constexpr) { return false; }
    } else {
      if (!method.expression || !active.insert(&method).second) { return false; }
      const bool eligible = constexpr_expression(*method.expression, active);
      active.erase(&method);
      if (!eligible) { return false; }
    }
    break;
  }
  default: break;
  }
  return std::all_of(node.operands.begin(), node.operands.end(), [&](const auto& operand) {
    return constexpr_expression(operand, active);
  });
}

// Start a fresh walk for repeated callees and recursive callable metadata.
inline bool constexpr_expression(const behavior_expression& node) {
  std::set<const function_declaration*> active;
  return constexpr_expression(node, active);
}

} // namespace rohit::serializer::writer::behavior

namespace rohit::serializer::writer::construction {

// Share the same callee/operation proof as ordinary readonly generated methods.
inline bool constexpr_binding(const behavior_expression& expression) {
  return behavior::constexpr_expression(expression);
}

} // namespace rohit::serializer::writer::construction
