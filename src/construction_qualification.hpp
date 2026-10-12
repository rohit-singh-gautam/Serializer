// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "behavior_writer.hpp"
#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace rohit::serializer::writer::construction {

// Explain an inferred constexpr restriction without claiming anything about opaque native tokens.
inline std::string constexpr_restriction(const class_node& owner,
                                        const construction_declaration& factory) {
  if (!factory.native_validations.empty() || !factory.initializers.empty()) {
    return "native authoring validation/initialization has no portable constexpr contract";
  }
  if (owner.supports_managed()) { return "managed identity and lifetime hooks require runtime state"; }
  if (std::any_of(owner.native_blocks.begin(), owner.native_blocks.end(), [](const auto& block) {
        return block.language == "cpp" && !block.no_instance_storage;
      })) { return "opaque native instance storage has no declared constexpr contract"; }
  if (std::any_of(owner.member_list.begin(), owner.member_list.end(), [](const auto& field) {
        return field.modifier == member::modifier_type::map ||
            std::any_of(field.type_name_list.begin(), field.type_name_list.end(),
                [](const auto& type) { return type.is_associative(); });
      })) { return "ordered/hash associative storage is not constexpr in C++20"; }
  if (std::any_of(owner.functions.begin(), owner.functions.end(), [](const auto& method) {
        return method.dispatch != function_declaration::dispatch_type::ordinary;
      })) { return "dispatched behavior retains runtime lifecycle requirements"; }
  return "a nested value, callable, or binding has no proven C++20 constexpr contract";
}

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

enum class default_record_qualifier { constant_evaluation, nonthrowing };

// Qualify host default construction reached by empty-record parameter defaults only.
inline bool default_record_parameters_support(const class_node& owner,
    default_record_qualifier qualifier) {
  std::set<const class_node*> active;
  std::function<bool(const class_node&)> check_owner;
  // Generic arguments and wrapped storage can contain additional owning records.
  const auto check_type = [&](const auto& self, const type_name& type) -> bool {
    if (!std::all_of(type.arguments.begin(), type.arguments.end(),
        [&](const auto& argument) { return self(self, argument); })) { return false; }
    if (type.resolved_node && (type.resolved_node->type == object_type::class_type ||
                              type.resolved_node->type == object_type::generic_definition)) {
      return check_owner(static_cast<const class_node&>(*type.resolved_node));
    }
    return true;
  };
  // A recursive storage cycle cannot establish an inferred default-construction promise.
  check_owner = [&](const class_node& value) {
    if (!active.insert(&value).second) { return false; }
    const bool external_default = std::any_of(value.constructions.begin(), value.constructions.end(),
        [&](const auto& constructor) {
          return constructor.constructor && constructor.parameters.empty() && constructor.declaration_only &&
              !constructor.defaulted && (qualifier == default_record_qualifier::nonthrowing ||
                  constructor.cpp_prefix.find("constexpr") == std::string::npos);
        });
    const bool supported = !external_default &&
        std::all_of(value.parents.begin(), value.parents.end(), [&](const auto& base) {
          return base.parent_class && check_owner(*base.parent_class);
        }) && std::all_of(value.member_list.begin(), value.member_list.end(), [&](const auto& field) {
          return field.wire_only || std::all_of(field.type_name_list.begin(), field.type_name_list.end(),
              [&](const auto& type) { return check_type(check_type, type); });
        });
    active.erase(&value);
    return supported;
  };
  // Positional record and fixed-array defaults may contain nested empty literals.
  const auto check_initializer = [&](const auto& self, const value_initializer& value) -> bool {
    if (value.kind == value_initializer::kind_type::default_record) {
      return check_type(check_type, value.type);
    }
    return std::all_of(value.elements.begin(), value.elements.end(),
        [&](const auto& element) { return self(self, element); });
  };
  return std::all_of(owner.constructions.begin(), owner.constructions.end(), [&](const auto& construction) {
    return std::all_of(construction.parameters.begin(), construction.parameters.end(), [&](const auto& parameter) {
      return !parameter.initializer || check_initializer(check_initializer, *parameter.initializer);
    });
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
      const auto& record = static_cast<const class_node&>(*type.resolved_node);
      return construction::constexpr_construction(record) &&
          construction::default_record_parameters_support(record,
              construction::default_record_qualifier::constant_evaluation);
    }
    return std::find(owner.type_parameters.begin(), owner.type_parameters.end(), type.name) != owner.type_parameters.end() ||
        std::any_of(input_types.begin(), input_types.end(), [&](const auto& input) { return input.name == type.name; });
  };
  return eligible(value, eligible);
}

// Check returned/parameter values without restricting unrelated borrowed receiver storage.
inline bool constexpr_parameters(const class_node& owner, const function_declaration& method) {
  return (method.return_type.name == "void" ||
      constexpr_type(owner, method.return_type, method.generic_parameters)) &&
      std::all_of(method.parameters.begin(), method.parameters.end(),
          [&](const auto& parameter) { return constexpr_type(owner, parameter.type, method.generic_parameters); });
}

// Follow typed callees and explicit native contracts without interpreting their C++ tokens.
inline bool constexpr_expression(const behavior_expression& node,
    std::set<const void*>& active) {
  using operation = behavior_expression::operation;
  switch (node.kind) {
  case operation::square_root: case operation::sine: case operation::cosine: return false;
  case operation::call: {
    if (node.call_constructor) {
      if (!node.call_target) { return false; }
      const auto& target = static_cast<const class_node&>(*node.call_target);
      if (!construction::constexpr_construction(target) ||
          !construction::default_record_parameters_support(target,
              construction::default_record_qualifier::constant_evaluation)) { return false; }
      if (node.call_method_index != std::numeric_limits<std::size_t>::max()) {
        const auto& constructor = target.constructions.at(node.call_method_index);
        if (constructor.declaration_only && !constructor.defaulted &&
            constructor.cpp_prefix.find("constexpr") == std::string::npos) { return false; }
        if (!active.insert(&constructor).second) { return false; }
        const bool eligible = constructor.native_validations.empty() && constructor.initializers.empty() &&
            std::all_of(constructor.assignments.begin(), constructor.assignments.end(), [&](const auto& assignment) {
              return !assignment.expression || constexpr_expression(*assignment.expression, active);
            }) && std::all_of(constructor.validations.begin(), constructor.validations.end(), [&](const auto& expression) {
              return constexpr_expression(expression, active);
            });
        active.erase(&constructor);
        if (!eligible) { return false; }
      }
      break;
    }
    const auto& method = called_function(node);
    if (method.dispatch != function_declaration::dispatch_type::ordinary ||
        method.effect != function_declaration::effect_type::readonly) { return false; }
    static const class_node free_owner{object_type::class_type, std::string{}, nullptr,
        class_attributes::none, {}};
    const auto& owner = node.call_target->type == object_type::function_type ? free_owner :
        static_cast<const class_node&>(*node.call_target);
    if (!constexpr_parameters(owner, method)) { return false; }
    if (native_body(method, "cpp") || !method.expression) {
      if (!method.cpp_constexpr && method.cpp_prefix.find("constexpr") == std::string::npos &&
          !(method.automatic_constexpr_enabled && native_body(method, "cpp") &&
            construction::constexpr_construction(owner))) { return false; }
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
  std::set<const void*> active;
  return constexpr_expression(node, active);
}

// Identify the first operation or selected callee that prevents constant evaluation.
inline std::string constexpr_expression_restriction(const behavior_expression& node) {
  using operation = behavior_expression::operation;
  switch (node.kind) {
  case operation::square_root: return "square root is a runtime-only C++20 math operation";
  case operation::sine: return "sine is a runtime-only C++20 math operation";
  case operation::cosine: return "cosine is a runtime-only C++20 math operation";
  case operation::call:
    if (!constexpr_expression(node)) {
      if (node.call_constructor && node.call_target) {
        return "constructor '" + node.call_target->get_full_name() +
            "' has no proven C++20 constexpr construction contract";
      }
      return "callable '" + called_function(node).name +
          "' has no proven C++20 constexpr definition";
    }
    break;
  default: break;
  }
  for (const auto& operand : node.operands) {
    if (!constexpr_expression(operand)) { return constexpr_expression_restriction(operand); }
  }
  return "the expression has no proven C++20 constexpr contract";
}

// Report the selected receiver, value, or expression restriction rather than unrelated alternatives.
inline std::string constexpr_method_restriction(const class_node* owner,
    const function_declaration& method, bool editor_receiver) {
  if (editor_receiver) { return "managed editor guards require runtime state"; }
  if (method.dispatch != function_declaration::dispatch_type::ordinary) {
    return "dispatched behavior retains runtime lifecycle requirements";
  }
  static const class_node free_owner{object_type::class_type, std::string{}, nullptr,
      class_attributes::none, {}};
  const auto& scope = owner ? *owner : free_owner;
  if (method.return_type.name != "void" &&
      !constexpr_type(scope, method.return_type, method.generic_parameters)) {
    return "return type '" + method.return_type.get_full_name() +
        "' has no proven C++20 constexpr value lifetime";
  }
  for (const auto& parameter : method.parameters) {
    if (!constexpr_type(scope, parameter.type, method.generic_parameters)) {
      return "parameter type '" + parameter.type.get_full_name() +
          "' has no proven C++20 constexpr value lifetime";
    }
  }
  if (native_body(method, "cpp") && owner && !construction::constexpr_construction(*owner)) {
    return construction::constexpr_restriction(*owner, {});
  }
  if (method.expression) { return constexpr_expression_restriction(*method.expression); }
  return "the native C++ body has no declared constexpr contract";
}

} // namespace rohit::serializer::writer::behavior

namespace rohit::serializer::writer::construction {

// Share the same callee/operation proof as ordinary readonly generated methods.
inline bool constexpr_binding(const behavior_expression& expression) {
  return behavior::constexpr_expression(expression);
}

} // namespace rohit::serializer::writer::construction
