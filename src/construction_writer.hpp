// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "behavior_writer.hpp"
#include "construction_qualification.hpp"
#include "construction_defaults.hpp"
#include "callable_guard.hpp"
#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::construction {

// Render the bounded schema dimension grammar without introducing host-size conversions.
inline std::string extent_text(const dimension_expression& value) {
  using operation = dimension_expression::operation;
  if (value.kind == operation::literal) { return std::to_string(value.value); }
  if (value.kind == operation::parameter) { return value.name; }
  return "(" + extent_text(value.operands.at(0)) +
      (value.kind == operation::add ? " + " : " * ") + extent_text(value.operands.at(1)) + ")";
}

// Match each backend's existing owning field representation for fixed-array inputs.
inline std::string parameter_type(const function_parameter& parameter, std::string_view language,
    const std::function<std::string(const type_name&)>& type) {
  const auto element = type(parameter.type);
  if (parameter.modifier != member::modifier_type::array) {
    return language == "cpp" && parameter.const_ref ? "const " + element + "&" : element;
  }
  if (language == "cpp") {
    const auto count = parameter.fixed_extent ? std::to_string(parameter.fixed_extent) :
        extent_text(parameter.extent_expression.at(0));
    const auto array = "::std::array<" + element + ", " + count + ">";
    return parameter.const_ref ? "const " + array + "&" : array;
  }
  if (language == "rust") { return "Vec<" + element + ">"; }
  if (language == "go") { return "[]" + element; }
  if (language == "swift") { return "[" + element + "]"; }
  if (language == "csharp") { return "System.Collections.Generic.List<" + element + ">"; }
  if (language == "kotlin") {
    return parameter.type.type == object_type::primitive && parameter.type.name != "string" &&
        !parameter.type.is_digest() && !parameter.type.application ? element + "Array" :
        "MutableList<" + element + ">";
  }
  return element + "[]";
}

// Preserve the same constraint categories and exact-type identities as class templates.
inline std::string input_constraint(const generic_constraint& value, std::string_view parameter,
    const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(std::string_view)>& callable_identifier = {}) {
  using operation = generic_constraint::operation;
  const std::string name{parameter};
  switch (value.kind) {
  case operation::integral: return "::std::integral<" + name + ">";
  case operation::floating_point: return "::std::floating_point<" + name + ">";
  case operation::stateless: return "::std::is_empty_v<" + name + ">";
  case operation::factory: return "requires { { " + name + "::" +
      (callable_identifier ? callable_identifier(value.factory_name) : value.factory_name) +
      "() } -> ::std::same_as<" + name + ">; }";
  case operation::exact_type: return "::std::same_as<" + name + ", " + type(*value.type) + ">";
  case operation::logical_not: return "!(" + input_constraint(value.operands.at(0), parameter, type, callable_identifier) + ")";
  case operation::logical_and:
  case operation::logical_or:
    return "(" + input_constraint(value.operands.at(0), parameter, type, callable_identifier) +
        (value.kind == operation::logical_and ? " && " : " || ") +
        input_constraint(value.operands.at(1), parameter, type, callable_identifier) + ")";
  }
  throw std::invalid_argument{"Unknown construction input constraint"};
}

// Emit only the method's independently constrained native C++ input parameters.
inline std::string input_template(const construction_declaration& factory,
    const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(std::string_view)>& callable_identifier = {}) {
  if (factory.generic_parameters.empty()) { return {}; }
  std::string parameters, constraints;
  for (const auto& parameter : factory.generic_parameters) {
    if (!parameters.empty()) { parameters += ", "; }
    parameters += "typename " + parameter.name;
    if (parameter.constraint) {
      if (!constraints.empty()) { constraints += " && "; }
      constraints += "(" + input_constraint(*parameter.constraint, parameter.name, type, callable_identifier) + ")";
    }
  }
  return "  template <" + parameters + ">\n" +
      (constraints.empty() ? "" : "  requires (" + constraints + ")\n");
}

// Build a typed default without interpreting native tokens or sharing owned mutable instances.
inline member parameter_default(const function_parameter& parameter) {
  member result{access_type::public_access, parameter.modifier, {parameter.type},
      parameter.name, parameter.name, 0, {}, parameter.default_value};
  result.default_factory = parameter.default_factory;
  result.fixed_extent = parameter.fixed_extent;
  result.extent_expression = parameter.extent_expression;
  result.portable_default_enabled = parameter.portable_default_enabled;
  result.initializer = parameter.initializer;
  return result;
}

// Reject unsupported new output contracts explicitly while leaving existing construction available.
inline void validate_extended_backend(const class_node& owner, std::string_view language) {
  if (language != "cpp" && std::any_of(owner.constructions.begin(), owner.constructions.end(),
      [](const auto& factory) {
        return factory.delegating || std::any_of(factory.parameters.begin(), factory.parameters.end(),
            [](const auto& parameter) { return parameter.const_ref; });
      })) {
    throw std::invalid_argument{std::string{language} + ": delegating or borrowed constructors require C++ output"};
  }
  if (language != "cpp" && std::count_if(owner.constructions.begin(), owner.constructions.end(),
      [](const auto& factory) { return factory.constructor; }) > 1) {
    throw std::invalid_argument{std::string{language} + ": constructor overloads require C++ output"};
  }
  for (const auto& factory : owner.constructions) {
    if (factory.guard && factory.guard->available == false) { continue; }
    if (language != "cpp" && factory.guard && !factory.guard->available) {
      throw std::invalid_argument{std::string{language} + ": dependent callable availability requires a concrete specialization"};
    }
    if (language != "cpp" && std::any_of(factory.assignments.begin(), factory.assignments.end(),
        [](const auto& assignment) {
          return !assignment.generator_indices.empty() || std::any_of(assignment.elements.begin(),
              assignment.elements.end(), [](const auto& element) { return element.expression.has_value(); });
        })) {
      throw std::invalid_argument{std::string{language} + ": checked array generation requires C++ output"};
    }
    if (factory.declaration_only || factory.defaulted) {
      if (language != "cpp") {
        throw std::invalid_argument{std::string{language} + ": external/defaulted constructor declarations require C++ output"};
      }
    }
    if (language != "cpp" && (!factory.validations.empty() || !factory.native_validations.empty() ||
        !factory.initializers.empty())) {
      throw std::invalid_argument{std::string{language} + ": authoring validation and initialization require C++ output"};
    }
    if (language != "cpp" && !factory.generic_parameters.empty()) {
      throw std::invalid_argument{std::string{language} + ": independently generic construction inputs require C++ output"};
    }
    const auto native_default = [&](const auto& self, const value_initializer& value) -> bool {
      return value.kind == value_initializer::kind_type::default_owner ||
          std::any_of(value.elements.begin(), value.elements.end(),
              [&](const auto& element) { return self(self, element); });
    };
    if (language != "cpp" && std::any_of(factory.parameters.begin(), factory.parameters.end(),
        [&](const auto& parameter) {
          return parameter.initializer && native_default(native_default, *parameter.initializer);
        })) {
      throw std::invalid_argument{std::string{language} +
          ": empty owner-parameter defaults require a concrete supported owning record or C++ output"};
    }
    if (language != "cpp" && std::any_of(factory.assignments.begin(), factory.assignments.end(),
        [](const auto& assignment) { return assignment.expression.has_value(); })) {
      throw std::invalid_argument{std::string{language} + ": checked construction expressions require C++ output"};
    }
  }
}

// Reject new factory symbols that collide after the backend's naming and visibility conversion.
inline void validate_names(const class_node& owner, std::string_view owner_name,
    const std::function<std::string(std::string_view, access_type)>& identifier,
    const std::function<std::string(std::string_view)>& callable_identifier = {},
    std::string_view language = {}) {
  if (owner.constructions.empty()) { return; }
  std::set<std::string> symbols{
      "encode", "decode", "read", "write", "init", "free", "default", "new", "constructor",
      "Encode", "Decode", "Read", "Write", "serialize_in", "serialize_out", "serialize",
      "deserialize", "srlRead", "srlWrite", "serializer_reuses_storage", std::string{owner_name}};
  if (language == "java" || language == "kotlin") {
    symbols.insert({"toString", "hashCode", "equals", "getClass", "clone", "finalize",
                    "wait", "notify", "notifyAll"});
  }
  for (const auto& field : owner.member_list) {
    if (!field.wire_only) { symbols.insert(identifier(field.name, field.access)); }
  }
  for (const auto& method : owner.functions) {
    symbols.insert(callable_identifier ? callable_identifier(method.name) : identifier(method.name, method.access));
  }
  std::set<std::string> constructor_names;
  for (const auto& factory : owner.constructions) {
    if (factory.guard && factory.guard->available == false) { continue; }
    const auto source_name = factory.constructor ? std::string_view{"create"} : std::string_view{factory.name};
    const auto name = callable_identifier ? callable_identifier(source_name) : identifier(source_name, factory.access);
    const bool constructor_overload = language == "cpp" && factory.constructor &&
        factory.overload_contract_enabled && constructor_names.contains(name);
    if (!constructor_overload && !symbols.insert(name).second) {
      throw std::invalid_argument{"Generated construction name collision: " + name};
    }
    if (factory.constructor) { constructor_names.insert(name); }
    for (std::size_t arity{}; arity < factory.parameters.size(); ++arity) {
      const auto& parameter = factory.parameters[arity];
      if (parameter.default_value.empty() && parameter.default_factory.empty()) { continue; }
      const auto default_name = name + "_default_" + std::to_string(arity);
      const bool default_overload = language == "cpp" && factory.constructor &&
          factory.overload_contract_enabled && constructor_names.contains(default_name);
      if (!default_overload && !symbols.insert(default_name).second) {
        throw std::invalid_argument{"Generated construction default overload name collision"};
      }
      if (factory.constructor) { constructor_names.insert(default_name); }
    }
    std::set<std::string> parameters;
    for (const auto& parameter : factory.parameters) {
      const auto parameter_name = "srl_arg_" + identifier(parameter.name, access_type::public_access);
      if (!parameters.insert(parameter_name).second) {
        throw std::invalid_argument{"Generated construction parameter collision: " + parameter_name};
      }
    }
  }
}

// Bind a structured default to the backend's owner-qualified factory name.
inline std::string factory_call(std::string_view owner, std::string_view name, std::string_view language) {
  if (language == "go" || language == "c") { return std::string{owner} + "_" + std::string{name} + "()"; }
  return std::string{owner} + (language == "cpp" || language == "rust" ? "::" : ".") + std::string{name} + "()";
}

// Move an owned input whenever its representation supports moving; explicit copy-only types keep copying.
inline std::string transfer_owned(std::string value, bool direct_members, std::string_view local_name) {
  if (!direct_members) { return "std::move(" + value + ")"; }
  // The generic lambda adds no storage or runtime dispatch and cannot collide with owner members.
  const std::string local{local_name};
  return "[](auto& " + local + ") noexcept -> decltype(auto) { "
      "if constexpr (::std::is_move_constructible_v<::std::remove_reference_t<decltype(" + local + ")>>) { "
      "return ::std::move(" + local + "); } else { return ::std::as_const(" + local + "); } }(" + value + ")";
}

// Emit typed value factories; default decoding continues to construct ordinary schema defaults.
inline std::string factories(const class_node& owner, std::string_view owner_name,
    std::string_view language, const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(std::string_view, access_type)>& identifier,
    const std::function<std::string(const member&)>& initial,
    const std::function<std::string(std::string_view)>& callable_identifier = {},
    const std::function<std::string(const behavior_expression&, const std::vector<std::string>&)>& call = {},
    const std::function<std::string(const behavior_expression&,
        const std::function<std::string(std::string_view)>&)>& checked_expression = {}) {
  validate_extended_backend(owner, language);
  validate_names(owner, owner_name, identifier, callable_identifier, language);
  std::string output;
  const std::string name{owner_name};
  const bool cpp = language == "cpp", python = language == "python", rust = language == "rust";
  const bool go = language == "go", swift = language == "swift", kotlin = language == "kotlin";
  const bool typescript = language == "typescript", js = language == "js";
  const bool constexpr_owner = cpp && constexpr_construction(owner) &&
      default_record_parameters_support(owner, default_record_qualifier::constant_evaluation);
  const bool nothrow_owner = cpp && nothrow_construction(owner) &&
      default_record_parameters_support(owner, default_record_qualifier::nonthrowing);
  // Calls require ordered statement evaluation; retain existing lowering for other bindings.
  const auto expression_text = [&](const behavior_expression& expression,
      const std::function<std::string(std::string_view)>& symbol,
      const behavior::expression_context& context) {
    if (cpp && behavior::has_call(expression) && checked_expression) {
      return checked_expression(expression, symbol);
    }
    return behavior::expression(expression, language, symbol, {}, context);
  };
  // Construct fixed arrays directly; each owned input is bound once in source order.
  const auto array_binding = [&](const construction_assignment& assignment, const member& field,
      bool direct_members, const std::string& transfer_name) {
    const auto& element_type = field.type_name_list.front();
    std::string arguments;
    behavior::expression_context context;
    context.call = call;
    context.native_type = type;
    context.member_name = [&](auto member_name) { return identifier(member_name, access_type::public_access); };
    if (!assignment.generator_indices.empty()) {
      if (!cpp) { throw std::invalid_argument{"Checked array generation requires C++ output"}; }
      const auto dimensions = field.array_exposure && !field.array_exposure->dimensions.empty() ?
          field.array_exposure->dimensions : field.extent_expression;
      const auto count = field.fixed_extent ? std::to_string(field.fixed_extent) : extent_text(field.extent_expression.at(0));
      std::set<std::string> locals{name, owner.name};
      locals.insert(owner.type_parameters.begin(), owner.type_parameters.end());
      for (const auto& construction : owner.constructions) {
        for (const auto& parameter : construction.generic_parameters) { locals.insert(parameter.name); }
        for (const auto& parameter : construction.parameters) {
          locals.insert("srl_arg_" + identifier(parameter.name, access_type::public_access));
        }
      }
      const auto fresh_local = [&](const std::string& base) {
        auto result = base;
        std::size_t suffix{};
        while (!locals.insert(result).second) { result = base + "_" + std::to_string(++suffix); }
        return result;
      };
      const auto array_name = fresh_local("srl_array");
      std::vector<std::string> indices;
      for (const auto& index : assignment.generator_indices) { indices.push_back(fresh_local("srl_index_" + index)); }
      const auto symbol = [&](std::string_view name) {
        const auto found = std::find(assignment.generator_indices.begin(), assignment.generator_indices.end(), name);
        return found != assignment.generator_indices.end() ? indices[static_cast<std::size_t>(found - assignment.generator_indices.begin())] :
            "srl_arg_" + identifier(name, access_type::public_access);
      };
      std::string result = "[&]() constexpr { ::std::array<" + type(element_type) + ", " + count + "> " + array_name + "{}; ";
      std::string offset = indices.front();
      for (std::size_t axis{}; axis < dimensions.size(); ++axis) {
        const auto& index = indices[axis];
        result += "for (::std::uint64_t " + index + "{}; " + index + " < " + extent_text(dimensions[axis]) + "; ++" + index + ") { ";
        if (axis) { offset = "(" + offset + " * " + extent_text(dimensions[axis]) + " + " + index + ")"; }
      }
      result += array_name + "[" + offset + "] = " + expression_text(*assignment.expression, symbol, context) + "; ";
      for (std::size_t axis{}; axis < dimensions.size(); ++axis) { result += "} "; }
      return result + "return " + array_name + "; }()";
    }
    for (const auto& element : assignment.elements) {
      if (!arguments.empty()) { arguments += ", "; }
      if (element.parameter) {
        const auto argument = (rust ? "_srl_arg_" : "srl_arg_") + identifier(element.value, access_type::public_access);
        arguments += cpp ? transfer_owned(argument, direct_members, transfer_name) : argument;
      } else if (element.expression) {
        arguments += expression_text(*element.expression,
            [&](auto symbol) { return "srl_arg_" + identifier(symbol, access_type::public_access); }, context);
      } else {
        auto value = field;
        value.modifier = member::modifier_type::none;
        value.extent_expression.clear(); value.fixed_extent = 0;
        value.initializer.reset(); value.default_factory.clear(); value.default_value = element.value;
        arguments += initial(value);
      }
    }
    if (cpp) { return "::std::array<" + type(element_type) + ", " + std::to_string(assignment.elements.size()) + ">{" + arguments + "}"; }
    if (language == "java") { return "new " + type(element_type) + "[]{" + arguments + "}"; }
    if (rust) { return "vec![" + arguments + "]"; }
    if (go) { return "[]" + type(element_type) + "{" + arguments + "}"; }
    if (language == "csharp") { return "new System.Collections.Generic.List<" + type(element_type) + ">{" + arguments + "}"; }
    if (kotlin) {
      auto kind = type(element_type);
      std::transform(kind.begin(), kind.end(), kind.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
      return (element_type.type == object_type::primitive && element_type.name != "string" ? kind + "ArrayOf" : "mutableListOf") + "(" + arguments + ")";
    }
    return "[" + arguments + "]";
  };
  bool explicit_constructor{};
  bool authored_default_constructor{};
  std::set<std::string> generated_tags;
  for (const auto& factory : owner.constructions) {
    if (factory.guard && factory.guard->available == false) { continue; }
    const auto id = callable_identifier ? callable_identifier(factory.constructor ? "create" : factory.name) :
        identifier(factory.constructor ? "create" : factory.name, factory.access);
    const bool direct_members = cpp && factory.member_initialization_enabled && !owner.supports_managed();
    auto transfer_name = std::string{"srl_owned_value"};
    std::size_t transfer_suffix{};
    while (transfer_name == name ||
        std::find(owner.type_parameters.begin(), owner.type_parameters.end(), transfer_name) != owner.type_parameters.end() ||
        std::any_of(factory.generic_parameters.begin(), factory.generic_parameters.end(),
            [&](const auto& parameter) { return parameter.name == transfer_name; })) {
      transfer_name = "srl_owned_value_" + std::to_string(++transfer_suffix);
    }
    auto go_owner = name;
    // Go exports a free function by its first character, including the owner prefix.
    if (go && factory.access != access_type::public_access && !go_owner.empty() &&
        go_owner.front() >= 'A' && go_owner.front() <= 'Z') {
      go_owner.front() = static_cast<char>(go_owner.front() + ('a' - 'A'));
    }
    const auto visibility = factory.access == access_type::public_access ? "public " :
        factory.access == access_type::protected_access && (cpp || language == "java" || language == "csharp") ?
            "protected " : "private ";
    const auto constant_binding = [&](const auto& self, const construction_assignment& assignment) -> bool {
      return (!assignment.expression || constexpr_binding(*assignment.expression)) &&
          std::all_of(assignment.elements.begin(), assignment.elements.end(),
              [&](const auto& element) { return self(self, element); });
    };
    const bool constexpr_bindings = std::all_of(factory.assignments.begin(), factory.assignments.end(),
        [&](const auto& assignment) { return constant_binding(constant_binding, assignment); });
    const bool authoring = !factory.validations.empty() || !factory.native_validations.empty() || !factory.initializers.empty();
    // A delegated call inherits its selected constructor's constant-evaluation eligibility.
    const auto constant_delegation = [&](const auto& self, const construction_declaration& item) -> bool {
      if (!item.delegating) { return true; }
      if (item.delegation_target == std::numeric_limits<std::size_t>::max()) { return constexpr_owner; }
      const auto& target = owner.constructions.at(item.delegation_target);
      if (!self(self, target)) { return false; }
      behavior_expression call_expression;
      call_expression.kind = behavior_expression::operation::call;
      call_expression.call_constructor = true;
      call_expression.call_target = &owner;
      call_expression.call_method_index = item.delegation_target;
      call_expression.operands = item.delegation;
      return constexpr_binding(call_expression);
    };
    const bool inferred_constexpr = cpp && constexpr_owner && constexpr_bindings &&
        constant_delegation(constant_delegation, factory) &&
        factory.native_validations.empty() && factory.initializers.empty() &&
        std::all_of(factory.validations.begin(), factory.validations.end(), constexpr_binding) &&
        constexpr_construction(owner, factory.parameters, factory.generic_parameters);
    auto constant = cpp ? factory.cpp_prefix : std::string{};
    if (inferred_constexpr && (!factory.declaration_only || factory.defaulted) &&
        factory.cpp_prefix.find("constexpr") == std::string::npos) {
      constant = "constexpr" + (constant.empty() ? "" : " " + constant);
    }
    if (!constant.empty()) { constant += ' '; }
    const auto checked_assignment = [&](const auto& self, const construction_assignment& assignment) -> bool {
      return assignment.expression.has_value() || std::any_of(assignment.elements.begin(), assignment.elements.end(),
          [&](const auto& element) { return self(self, element); });
    };
    const bool checked_binding = std::any_of(factory.assignments.begin(), factory.assignments.end(),
        [&](const auto& assignment) { return checked_assignment(checked_assignment, assignment); });
    const bool default_factory = std::any_of(factory.parameters.begin(), factory.parameters.end(),
        [](const auto& parameter) { return !parameter.default_factory.empty(); });
    const std::string exception = cpp && !factory.delegating && !authoring && !checked_binding && !default_factory && nothrow_owner && nothrow_construction(owner, factory.parameters) ? " noexcept" : "";
    const auto constexpr_comment = cpp && constant.find("constexpr") == std::string::npos && !factory.declaration_only && !factory.defaulted ?
        "  // C++ constexpr unavailable: " + constexpr_restriction(owner, factory) + ".\n" : std::string{};
    const auto template_declaration = cpp ? input_template(factory, type, callable_identifier) : std::string{};
    const auto guard_clause = cpp && factory.guard && !factory.guard->available.has_value() ?
        " requires (" + callable_availability::cpp(*factory.guard, type) + ")" : std::string{};
    std::string parameters, constructor_parameters, arguments;
    std::vector<std::string> parameter_declarations, parameter_names, default_values;
    for (const auto& parameter : factory.parameters) {
      const auto parameter_name = (rust ? "_srl_arg_" : "srl_arg_") + identifier(parameter.name, access_type::public_access);
      if (!parameters.empty()) { parameters += ", "; constructor_parameters += ", "; arguments += ", "; }
      arguments += cpp && !parameter.const_ref ? transfer_owned(parameter_name, direct_members, transfer_name) : parameter_name;
      auto declaration = python || js ? parameter_name :
          rust || swift || kotlin || typescript ? (swift ? "_ " : "") + parameter_name + ": " + parameter_type(parameter, language, type) :
          go ? parameter_name + " " + parameter_type(parameter, language, type) :
          std::string{cpp ? "[[maybe_unused]] " : ""} + parameter_type(parameter, language, type) + " " + parameter_name;
      parameter_declarations.push_back(declaration);
      parameter_names.push_back(parameter_name);
      auto default_value = parameter.default_value.empty() && parameter.default_factory.empty() ?
          std::string{} : initial(parameter_default(parameter));
      if (rust) { default_value = behavior::ungroup_expression(default_value); }
      default_values.push_back(default_value);
      constructor_parameters += declaration;
      if (cpp && !default_value.empty() && parameter_names.size() > 1) { constructor_parameters += " = " + default_value; }
      const bool owning_default = parameter.initializer &&
          parameter.initializer->kind != value_initializer::kind_type::scalar;
      if ((cpp || (python && !default_factory) || js || swift || kotlin) && !default_value.empty()) {
        declaration += " = " + (python && owning_default ? "None" : default_value);
      }
      if (typescript && !default_value.empty()) {
        declaration = parameter_name + "?: " + parameter_type(parameter, language, type);
      }
      parameters += declaration;
    }
    if (cpp && (factory.declaration_only || factory.defaulted)) {
      explicit_constructor = true;
      authored_default_constructor = authored_default_constructor || factory.parameters.empty();
      output += std::string{visibility} + ":\n  // Declare a constructor supplied by the C++ host.\n" +
          template_declaration + "  " + (factory.explicit_constructor ? "explicit " : "") +
          (factory.defaulted ? constant : factory.cpp_prefix +
              (factory.cpp_prefix.empty() ? "" : " ")) + name + "(" + constructor_parameters + ")" +
          guard_clause + (factory.defaulted ? " = default;\n" : ";\n");
      continue;
    }
    // Private tag identities cannot become an open generic representation or shadow emitted members.
    std::set<std::string> tag_names{name};
    tag_names.insert(owner.type_parameters.begin(), owner.type_parameters.end());
    for (const auto& parameter : factory.generic_parameters) { tag_names.insert(parameter.name); }
    const auto callable_name = [&](std::string_view value) {
      return callable_identifier ? callable_identifier(value) : identifier(value, access_type::public_access);
    };
    for (const auto& field : owner.member_list) {
      tag_names.insert(identifier(field.name, field.access));
      if (!field.readonly_getter.empty()) { tag_names.insert(callable_name(field.readonly_getter)); }
    }
    for (const auto& method : owner.functions) { tag_names.insert(callable_name(method.name)); }
    for (const auto& conversion : owner.conversions) { tag_names.insert(callable_name(conversion.name)); }
    for (const auto& construction : owner.constructions) {
      tag_names.insert(callable_name(construction.constructor ? "create" : construction.name));
    }
    const auto tag_base = "srl_factory_tag_" + id;
    auto factory_tag = tag_base;
    std::size_t tag_suffix{};
    while (tag_names.contains(factory_tag) || generated_tags.contains(factory_tag)) {
      factory_tag = tag_base + "_" + std::to_string(++tag_suffix);
    }
    if (cpp && factory.delegating) {
      explicit_constructor = true;
      authored_default_constructor = authored_default_constructor || factory.parameters.empty();
      std::string delegated_arguments;
      behavior::expression_context context;
      context.call = call;
      context.native_type = type;
      context.member_name = [&](auto field) { return identifier(field, access_type::public_access); };
      for (const auto& argument : factory.delegation) {
        if (!delegated_arguments.empty()) { delegated_arguments += ", "; }
        delegated_arguments += expression_text(argument,
            [&](auto symbol) { return "srl_arg_" + identifier(symbol, access_type::public_access); }, context);
      }
      if (factory.delegation_target != std::numeric_limits<std::size_t>::max()) {
        const auto& target = owner.constructions.at(factory.delegation_target);
        // Spell every omitted argument so delegation cannot select the independent codec default.
        for (std::size_t index = factory.delegation.size(); index < target.parameters.size(); ++index) {
          if (!delegated_arguments.empty()) { delegated_arguments += ", "; }
          delegated_arguments += initial(parameter_default(target.parameters[index]));
        }
      }
      output += std::string{visibility} + ":\n  // Delegate once to the selected owning constructor and its authoring hooks.\n" +
          constexpr_comment + template_declaration + "  " + (factory.explicit_constructor ? "explicit " : "") +
          constant + name + "(" + constructor_parameters + ")" + guard_clause + " : " + name + "(" + delegated_arguments + ") {}\n";
    } else if (cpp && (factory.constructor || direct_members)) {
      explicit_constructor = true;
      authored_default_constructor = authored_default_constructor || (factory.constructor && factory.parameters.empty());
      if (!factory.constructor) {
        generated_tags.insert(factory_tag);
        output += "private:\n  struct " + factory_tag + " {};\n";
      }
      output += std::string{factory.constructor ? visibility : "private "} + ":\n  // Construct owned fields from checked schema bindings.\n" + constexpr_comment + template_declaration + "  " +
          (factory.explicit_constructor ? "explicit " : "") + constant + name + "(" +
          (factory.constructor ? constructor_parameters : factory_tag +
              (parameters.empty() ? "" : ", " + constructor_parameters)) + ")" + exception + guard_clause;
      if (!direct_members) { output += " : " + name + "()"; }
      // Declaration order matches the C++ initialization order; unbound members keep schema defaults.
      const auto& assignments = factory.assignments;
      std::string initializers;
      for (const auto& field : owner.member_list) {
        const auto binding = std::find_if(assignments.begin(), assignments.end(),
            [&](const auto& assignment) { return assignment.field == field.name; });
        if (binding == assignments.end()) { continue; }
        const auto& assignment = *binding;
        auto value = field;
        value.initializer.reset();
        value.default_value = assignment.value;
        value.default_factory = assignment.factory_default ? assignment.value : std::string{};
        behavior::expression_context context;
        context.call = call;
        context.native_type = type;
        context.member_name = [&](auto member_name) { return identifier(member_name, access_type::public_access); };
        const auto bound = assignment.array_binding ? array_binding(assignment, field, direct_members, transfer_name) : assignment.expression ? expression_text(*assignment.expression,
            [&](auto symbol) { return "srl_arg_" + identifier(symbol, access_type::public_access); }, context) :
            assignment.parameter ? transfer_owned("srl_arg_" + identifier(assignment.value, access_type::public_access), direct_members, transfer_name) : initial(value);
        if (direct_members) {
          if (!initializers.empty()) { initializers += ", "; }
          initializers += identifier(field.name, field.access) + "{" + bound + "}";
        }
      }
      if (direct_members && !initializers.empty()) { output += " : " + initializers; }
      output += " {\n";
      for (const auto& assignment : factory.assignments) {
        if (direct_members) { break; }
        const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) { return field.name == assignment.field; });
        auto value = *found;
        value.initializer.reset();
        value.default_value = assignment.value;
        value.default_factory = assignment.factory_default ? assignment.value : std::string{};
        behavior::expression_context context;
        context.call = call;
        context.native_type = type;
        context.member_name = [&](auto field) { return identifier(field, access_type::public_access); };
        const auto bound = assignment.array_binding ? array_binding(assignment, *found, direct_members, transfer_name) : assignment.expression ? expression_text(*assignment.expression,
            [&](auto symbol) { return "srl_arg_" + identifier(symbol, access_type::public_access); }, context) :
            assignment.parameter ? "std::move(srl_arg_" + identifier(assignment.value, access_type::public_access) + ")" : initial(value);
        output += "    this->" + identifier(found->name, found->access) + " = " + bound + ";\n";
      }
      for (const auto& validation : factory.validations) {
        behavior::expression_context context;
        context.call = call;
        context.native_type = type;
        context.member_name = [&](auto field) { return identifier(field, access_type::public_access); };
        const auto condition = expression_text(validation, [&](auto field) {
          const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(),
              [&](const auto& member) { return member.name == field; });
          if (found == owner.member_list.end()) { throw std::invalid_argument{"Unbound authoring validation field"}; }
          return "this->" + identifier(field, found->access);
        }, context);
        output += "    if (!(" + condition + ")) { throw ::std::domain_error{\"Construction validation failed\"}; }\n";
      }
      for (const auto& block : factory.native_validations) {
        output += "// clang-format off\n" + behavior::line_directive(block, "cpp") + block.text +
            behavior::reset_line("cpp") + "// clang-format on\n";
      }
      for (const auto& block : factory.initializers) {
        output += "// clang-format off\n" + behavior::line_directive(block, "cpp") + block.text +
            behavior::reset_line("cpp") + "// clang-format on\n";
      }
      output += "  }\n";
    }
    output += python ? "  @staticmethod\n  def " + id + "(" + parameters + "):\n    \"\"\"Construct a fresh owned schema value.\"\"\"\n" :
        rust ? "  /// Construct a fresh owned schema value.\n  " + std::string(factory.access == access_type::public_access ? "pub " : "") + "fn " + id + "(" + parameters + ") -> Self {\n" :
        go ? "// Construct a fresh owned schema value.\nfunc " + go_owner + "_" + id + "(" + parameters + ") *" + name + " {\n" :
        swift ? "  /// Construct a fresh owned schema value.\n  " + std::string{visibility} + "static func " + id + "(" + parameters + ") -> " + name + " {\n" :
        kotlin ? "  // Construct a fresh owned schema value.\n  @Suppress(\"UNUSED_PARAMETER\")\n  " + std::string{visibility} + "fun " + id + "(" + parameters + "): " + name + " {\n" :
        typescript ? "  " + std::string{visibility} + "static " + id + "(" + parameters + "): " + name + ";\n" :
        js ? "  // Construct a fresh owned schema value.\n  static " + id + "(" + parameters + ") {\n" :
        cpp ? std::string{visibility} + ":\n  // Construct a fresh owned schema value.\n" + constexpr_comment + template_declaration + "  [[nodiscard]] static " + constant + name + " " + id + "(" + parameters + ")" + exception + guard_clause + " {\n" :
        "  // Construct a fresh owned schema value.\n  " + std::string{visibility} + "static " + name + " " + id + "(" + parameters + ") {\n";
    if (typescript) {
      for (std::size_t arity{}; arity < factory.parameters.size(); ++arity) {
        if (default_values[arity].empty()) { continue; }
        std::string declarations;
        for (std::size_t index{}; index < arity; ++index) {
          if (!declarations.empty()) { declarations += ", "; }
          declarations += parameter_declarations[index];
        }
        output += "  " + std::string{visibility} + "static " + id + "_default_" + std::to_string(arity) +
            "(" + declarations + "): " + name + ";\n";
      }
      continue;
    }
    if (python) {
      for (std::size_t index{}; index < factory.parameters.size(); ++index) {
        const auto& parameter = factory.parameters[index];
        if (parameter.initializer && parameter.initializer->kind != value_initializer::kind_type::scalar) {
          output += "    if " + parameter_names[index] + " is None:\n      " + parameter_names[index] +
              " = " + default_values[index] + "\n";
        }
      }
    }
    for (std::size_t index{}; index < factory.parameters.size(); ++index) {
      const auto& parameter = factory.parameters[index];
      if (parameter.modifier != member::modifier_type::array || cpp) { continue; }
      const auto count = std::to_string(parameter.fixed_extent);
      const auto& argument = parameter_names[index];
      output += python ? "    if len(" + argument + ") != " + count + ":\n      raise ValueError('Construction fixed-array cardinality mismatch')\n" :
          rust ? "    assert!(" + argument + ".len() == " + count + ", \"Construction fixed-array cardinality mismatch\");\n" :
          go ? "  if len(" + argument + ") != " + count + " { panic(\"Construction fixed-array cardinality mismatch\") }\n" :
          swift ? "    precondition(" + argument + ".count == " + count + ", \"Construction fixed-array cardinality mismatch\")\n" :
          kotlin ? "    require(" + argument + ".size == " + count + ") { \"Construction fixed-array cardinality mismatch\" }\n" :
          "    if (" + argument + (language == "csharp" ? ".Count" : ".length") + " != " + count + ") { throw new " +
              (language == "java" ? "IllegalArgumentException" : language == "csharp" ? "System.ArgumentException" : "Error") +
              "(\"Construction fixed-array cardinality mismatch\"); }\n";
    }
    if (cpp && (authoring || direct_members)) {
      output += "    return " + name + "{" +
          (factory.constructor ? "" : factory_tag + "{}" + (arguments.empty() ? "" : ", ")) +
          arguments + "};\n  }\n";
    } else {
      output += python ? "    srl_result = " + name + "()\n" :
          rust ? "    let " + std::string{factory.assignments.empty() ? "" : "mut "} + "srl_result = Self::default();\n" : go ? "  srl_result := New" + name + "()\n" :
          swift ? "    " + std::string{factory.assignments.empty() ? "let" : "var"} + " srl_result = " + name + "()\n" : kotlin ? "    val srl_result = " + name + "()\n" :
          js ? "    const srl_result = new " + name + "();\n" : cpp ? "    " + name + " srl_result{};\n" :
          "    " + name + " srl_result = new " + name + "();\n";
      for (const auto& assignment : factory.assignments) {
        const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) { return field.name == assignment.field; });
        if (found == owner.member_list.end()) { throw std::invalid_argument{"Unbound construction field"}; }
        auto value = *found;
        value.initializer.reset();
        value.default_value = assignment.value;
        value.default_factory = assignment.factory_default ? assignment.value : std::string{};
        const auto parameter = assignment.parameter ?
            (rust ? "_srl_arg_" : "srl_arg_") + identifier(assignment.value, access_type::public_access) : std::string{};
        auto assigned_value = assignment.array_binding ? array_binding(assignment, *found, false, transfer_name) : assignment.expression ? std::string{} : assignment.parameter ?
            (cpp ? "std::move(" + parameter + ")" : parameter) : initial(value);
        if (assignment.expression && !assignment.array_binding) {
          behavior::expression_context context;
          context.call = call;
          context.native_type = type;
          context.member_name = [&](auto field) { return identifier(field, access_type::public_access); };
          assigned_value = expression_text(*assignment.expression,
              [&](auto symbol) { return "srl_arg_" + identifier(symbol, access_type::public_access); }, context);
        }
        // Rust's float initializer casts need no grouping when they form the entire assignment RHS.
        const auto& assigned_type = found->type_name_list.front();
        if (rust && !assignment.parameter &&
            (assigned_type.name == "float" || assigned_type.name == "double") &&
            assigned_value.starts_with('(') && assigned_value.ends_with(')')) {
          assigned_value = assigned_value.substr(1, assigned_value.size() - 2);
        }
        output += "    srl_result." + identifier(found->name, found->access) + " = " +
            assigned_value +
            (python || swift || kotlin || go ? "\n" : ";\n");
      }
      output += python ? "    return srl_result\n" : go || swift || kotlin ? "    return srl_result\n  }\n" :
          rust ? "    srl_result\n  }\n" : "    return srl_result;\n  }\n";
    }
    if (!factory.generic_parameters.empty()) { continue; }
    for (std::size_t arity{}; arity < factory.parameters.size(); ++arity) {
      if (default_values[arity].empty()) { continue; }
      std::string declarations, inputs;
      for (std::size_t index{}; index < factory.parameters.size(); ++index) {
        if (index < arity) {
          if (!declarations.empty()) { declarations += ", "; }
          declarations += parameter_declarations[index];
        }
        if (!inputs.empty()) { inputs += ", "; }
        inputs += index < arity ? (cpp ? transfer_owned(parameter_names[index], direct_members, transfer_name) : parameter_names[index]) : default_values[index];
      }
      const auto wrapper = id + "_default_" + std::to_string(arity);
      const auto target = cpp || rust ? id : go ? go_owner + "_" + id : name + "." + id;
      output += python ? "  @staticmethod\n  def " + wrapper + "(" + declarations + "):\n    \"\"\"Construct with fresh defaults for omitted trailing arguments.\"\"\"\n    return " + target + "(" + inputs + ")\n" :
          rust ? "  /// Construct with fresh defaults for omitted trailing arguments.\n  " + std::string(factory.access == access_type::public_access ? "pub " : "") + "fn " + wrapper + "(" + declarations + ") -> Self { Self::" + target + "(" + inputs + ") }\n" :
          go ? "// Construct with fresh defaults for omitted trailing arguments.\nfunc " + go_owner + "_" + wrapper + "(" + declarations + ") *" + name + " { return " + target + "(" + inputs + ") }\n" :
          swift ? "  /// Construct with fresh defaults for omitted trailing arguments.\n  " + std::string{visibility} + "static func " + wrapper + "(" + declarations + ") -> " + name + " { return " + target + "(" + inputs + ") }\n" :
          kotlin ? "  // Construct with fresh defaults for omitted trailing arguments.\n  " + std::string{visibility} + "fun " + wrapper + "(" + declarations + "): " + name + " { return " + target + "(" + inputs + ") }\n" :
          cpp ? std::string{visibility} + ":\n  // Construct with fresh defaults for omitted trailing arguments.\n  [[nodiscard]] static " + constant + name + " " + wrapper + "(" + declarations + ")" + exception + guard_clause + " { return " + target + "(" + inputs + "); }\n" :
          js ? "  // Construct with fresh defaults for omitted trailing arguments.\n  static " + wrapper + "(" + declarations + ") { return " + target + "(" + inputs + "); }\n" :
          "  // Construct with fresh defaults for omitted trailing arguments.\n  " + std::string{visibility} + "static " + name + " " + wrapper + "(" + declarations + ") { return " + target + "(" + inputs + "); }\n";
    }
  }
  if (explicit_constructor && !authored_default_constructor) { output += "public:\n  // Preserve default construction for codecs and containers.\n  " + std::string{constexpr_owner ? "constexpr " : ""} + name + "()" + std::string{nothrow_owner ? " noexcept" : ""} + " = default;\n"; }
  return output;
}

} // namespace rohit::serializer::writer::construction
