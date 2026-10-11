// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "behavior_writer.hpp"
#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <functional>
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
  if (parameter.modifier != member::modifier_type::array) { return element; }
  if (language == "cpp") {
    const auto count = parameter.fixed_extent ? std::to_string(parameter.fixed_extent) :
        extent_text(parameter.extent_expression.at(0));
    return "::std::array<" + element + ", " + count + ">";
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
    const std::function<std::string(const type_name&)>& type) {
  using operation = generic_constraint::operation;
  const std::string name{parameter};
  switch (value.kind) {
  case operation::integral: return "::std::integral<" + name + ">";
  case operation::floating_point: return "::std::floating_point<" + name + ">";
  case operation::stateless: return "::std::is_empty_v<" + name + ">";
  case operation::exact_type: return "::std::same_as<" + name + ", " + type(*value.type) + ">";
  case operation::logical_not: return "!(" + input_constraint(value.operands.at(0), parameter, type) + ")";
  case operation::logical_and:
  case operation::logical_or:
    return "(" + input_constraint(value.operands.at(0), parameter, type) +
        (value.kind == operation::logical_and ? " && " : " || ") +
        input_constraint(value.operands.at(1), parameter, type) + ")";
  }
  throw std::invalid_argument{"Unknown construction input constraint"};
}

// Emit only the method's independently constrained native C++ input parameters.
inline std::string input_template(const construction_declaration& factory,
    const std::function<std::string(const type_name&)>& type) {
  if (factory.generic_parameters.empty()) { return {}; }
  std::string parameters, constraints;
  for (const auto& parameter : factory.generic_parameters) {
    if (!parameters.empty()) { parameters += ", "; constraints += " && "; }
    parameters += "typename " + parameter.name;
    constraints += "(" + input_constraint(*parameter.constraint, parameter.name, type) + ")";
  }
  return "  template <" + parameters + ">\n  requires (" + constraints + ")\n";
}

// Build a typed default without interpreting native tokens or sharing owned mutable instances.
inline member parameter_default(const function_parameter& parameter) {
  member result{access_type::public_access, member::modifier_type::none, {parameter.type},
      parameter.name, parameter.name, 0, {}, parameter.default_value};
  result.default_factory = parameter.default_factory;
  return result;
}

// Reject unsupported new output contracts explicitly while leaving existing construction available.
inline void validate_extended_backend(const class_node& owner, std::string_view language) {
  for (const auto& factory : owner.constructions) {
    if (language != "cpp" && !factory.generic_parameters.empty()) {
      throw std::invalid_argument{std::string{language} + ": independently generic construction inputs require C++ output"};
    }
    if (language != "cpp" && std::any_of(factory.assignments.begin(), factory.assignments.end(),
        [](const auto& assignment) { return assignment.expression.has_value(); })) {
      throw std::invalid_argument{std::string{language} + ": checked construction expressions require C++ output"};
    }
  }
}

// Prove bounded construction operations are constant-capable without importing callable emission.
inline bool constexpr_binding(const behavior_expression& expression) {
  using operation = behavior_expression::operation;
  if (expression.kind == operation::square_root || expression.kind == operation::sine ||
      expression.kind == operation::cosine || expression.kind == operation::call) { return false; }
  return std::all_of(expression.operands.begin(), expression.operands.end(), constexpr_binding);
}

// Prove known storage supports C++20 construction; dependent generic values defer to specialization.
inline bool constexpr_construction(const class_node& owner,
    const std::vector<function_parameter>& parameters = {},
    const std::vector<generic_parameter>& input_types = {}) {
  std::set<const class_node*> active;
  std::function<bool(const class_node&)> check_owner;
  const auto check_type = [&](const type_name& value, const class_node& scope, const auto& recurse) -> bool {
    if (value.is_associative()) { return false; }
    if (value.is_optional() || value.is_sequence()) {
      return std::all_of(value.arguments.begin(), value.arguments.end(),
          [&](const auto& argument) { return recurse(argument, scope, recurse); });
    }
    if (value.type == object_type::primitive) { return true; }
    if (value.type == object_type::enum_type) { return true; }
    if (value.resolved_node && (value.resolved_node->type == object_type::class_type ||
                               value.resolved_node->type == object_type::generic_definition)) {
      return check_owner(static_cast<const class_node&>(*value.resolved_node));
    }
    return std::find(scope.type_parameters.begin(), scope.type_parameters.end(), value.name) != scope.type_parameters.end() ||
        std::any_of(input_types.begin(), input_types.end(), [&](const auto& input) { return input.name == value.name; });
  };
  check_owner = [&](const class_node& value) {
    if (value.supports_managed() || !value.native_blocks.empty() ||
        value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        std::any_of(value.functions.begin(), value.functions.end(), [](const auto& method) {
          return method.dispatch != function_declaration::dispatch_type::ordinary;
        }) || !active.insert(&value).second) { return false; }
    // Cyclic storage needs a runtime lifetime contract even when an empty container breaks the cycle.
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

// Prove concrete scalar storage and checked defaults cannot throw during construction or copying.
inline bool nothrow_construction(const class_node& owner,
    const std::vector<function_parameter>& parameters = {}) {
  if (!constexpr_construction(owner, parameters)) { return false; }
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
    return false; // Concrete generic arguments must establish their own exception guarantees.
  };
  return std::all_of(owner.parents.begin(), owner.parents.end(), [&](const auto& base) {
    return base.parent_class && nothrow_construction(*base.parent_class);
  }) && std::all_of(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) {
    if ((field.modifier != member::modifier_type::none &&
         (field.modifier != member::modifier_type::array || field.fixed_extent == 0)) || field.type_name_list.size() != 1 ||
        !check_type(field.type_name_list.front())) { return false; }
    if (!field.default_value.empty()) {
      try {
        static_cast<void>(native::literal(field));
      } catch (const std::invalid_argument&) {
        return false; // Preserve native C++ initializers without promising their exception behavior.
      }
    }
    return true;
  }) && std::all_of(parameters.begin(), parameters.end(), [&](const auto& parameter) {
    return check_type(parameter.type);
  });
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
  for (const auto& factory : owner.constructions) {
    const auto source_name = factory.constructor ? std::string_view{"create"} : std::string_view{factory.name};
    const auto name = callable_identifier ? callable_identifier(source_name) : identifier(source_name, factory.access);
    if (!symbols.insert(name).second) {
      throw std::invalid_argument{"Generated construction name collision: " + name};
    }
    for (std::size_t arity{}; arity < factory.parameters.size(); ++arity) {
      const auto& parameter = factory.parameters[arity];
      if (parameter.default_value.empty() && parameter.default_factory.empty()) { continue; }
      if (!symbols.insert(name + "_default_" + std::to_string(arity)).second) {
        throw std::invalid_argument{"Generated construction default overload name collision"};
      }
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

// Emit typed value factories; default decoding continues to construct ordinary schema defaults.
inline std::string factories(const class_node& owner, std::string_view owner_name,
    std::string_view language, const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(std::string_view, access_type)>& identifier,
    const std::function<std::string(const member&)>& initial,
    const std::function<std::string(std::string_view)>& callable_identifier = {}) {
  validate_names(owner, owner_name, identifier, callable_identifier, language);
  validate_extended_backend(owner, language);
  std::string output;
  const std::string name{owner_name};
  const bool cpp = language == "cpp", python = language == "python", rust = language == "rust";
  const bool go = language == "go", swift = language == "swift", kotlin = language == "kotlin";
  const bool typescript = language == "typescript", js = language == "js";
  const bool constexpr_owner = cpp && constexpr_construction(owner);
  const bool nothrow_owner = cpp && nothrow_construction(owner);
  bool explicit_constructor{};
  for (const auto& factory : owner.constructions) {
    const auto id = callable_identifier ? callable_identifier(factory.constructor ? "create" : factory.name) :
        identifier(factory.constructor ? "create" : factory.name, factory.access);
    auto go_owner = name;
    // Go exports a free function by its first character, including the owner prefix.
    if (go && factory.access != access_type::public_access && !go_owner.empty() &&
        go_owner.front() >= 'A' && go_owner.front() <= 'Z') {
      go_owner.front() = static_cast<char>(go_owner.front() + ('a' - 'A'));
    }
    const auto visibility = factory.access == access_type::public_access ? "public " :
        factory.access == access_type::protected_access && (cpp || language == "java" || language == "csharp") ?
            "protected " : "private ";
    const bool constexpr_bindings = std::all_of(factory.assignments.begin(), factory.assignments.end(),
        [](const auto& assignment) { return !assignment.expression || constexpr_binding(*assignment.expression); });
    const std::string constant = cpp && constexpr_owner && constexpr_bindings &&
        constexpr_construction(owner, factory.parameters, factory.generic_parameters) ? "constexpr " : "";
    const bool checked_binding = std::any_of(factory.assignments.begin(), factory.assignments.end(),
        [](const auto& assignment) { return assignment.expression.has_value(); });
    const bool default_factory = std::any_of(factory.parameters.begin(), factory.parameters.end(),
        [](const auto& parameter) { return !parameter.default_factory.empty(); });
    const std::string exception = cpp && !checked_binding && !default_factory && nothrow_owner && nothrow_construction(owner, factory.parameters) ? " noexcept" : "";
    const auto template_declaration = cpp ? input_template(factory, type) : std::string{};
    std::string parameters, constructor_parameters, arguments;
    std::vector<std::string> parameter_declarations, parameter_names, default_values;
    for (const auto& parameter : factory.parameters) {
      const auto parameter_name = (rust ? "_srl_arg_" : "srl_arg_") + identifier(parameter.name, access_type::public_access);
      if (!parameters.empty()) { parameters += ", "; constructor_parameters += ", "; arguments += ", "; }
      arguments += cpp ? "std::move(" + parameter_name + ")" : parameter_name;
      auto declaration = python || js ? parameter_name :
          rust || swift || kotlin || typescript ? (swift ? "_ " : "") + parameter_name + ": " + parameter_type(parameter, language, type) :
          go ? parameter_name + " " + parameter_type(parameter, language, type) :
          std::string{cpp ? "[[maybe_unused]] " : ""} + parameter_type(parameter, language, type) + " " + parameter_name;
      parameter_declarations.push_back(declaration);
      parameter_names.push_back(parameter_name);
      const auto default_value = parameter.default_value.empty() && parameter.default_factory.empty() ?
          std::string{} : initial(parameter_default(parameter));
      default_values.push_back(default_value);
      constructor_parameters += declaration;
      if (cpp && !default_value.empty() && parameter_names.size() > 1) { constructor_parameters += " = " + default_value; }
      if ((cpp || (python && !default_factory) || js || swift || kotlin) && !default_value.empty()) { declaration += " = " + default_value; }
      if (typescript && !default_value.empty()) {
        declaration = parameter_name + "?: " + parameter_type(parameter, language, type);
      }
      parameters += declaration;
    }
    if (cpp && factory.constructor) {
      explicit_constructor = true;
      output += std::string{visibility} + ":\n  // Construct owned fields from checked schema bindings.\n" + template_declaration + "  " +
          (factory.explicit_constructor ? "explicit " : "") + constant + name + "(" + constructor_parameters + ")" + exception + " : " + name + "() {\n";
      for (const auto& assignment : factory.assignments) {
        const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) { return field.name == assignment.field; });
        auto value = *found;
        value.default_value = assignment.value;
        value.default_factory = assignment.factory_default ? assignment.value : std::string{};
        behavior::expression_context context;
        context.native_type = type;
        context.member_name = [&](auto field) { return identifier(field, access_type::public_access); };
        const auto bound = assignment.expression ? behavior::expression(*assignment.expression, "cpp",
            [&](auto symbol) { return "srl_arg_" + identifier(symbol, access_type::public_access); }, {}, context) :
            assignment.parameter ? "std::move(srl_arg_" + identifier(assignment.value, access_type::public_access) + ")" : initial(value);
        output += "    this->" + identifier(found->name, found->access) + " = " + bound + ";\n";
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
        cpp ? std::string{visibility} + ":\n  // Construct a fresh owned schema value.\n" + template_declaration + "  [[nodiscard]] static " + constant + name + " " + id + "(" + parameters + ")" + exception + " {\n" :
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
    output += python ? "    srl_result = " + name + "()\n" :
        rust ? "    let " + std::string{factory.assignments.empty() ? "" : "mut "} + "srl_result = Self::default();\n" : go ? "  srl_result := New" + name + "()\n" :
        swift ? "    " + std::string{factory.assignments.empty() ? "let" : "var"} + " srl_result = " + name + "()\n" : kotlin ? "    val srl_result = " + name + "()\n" :
        js ? "    const srl_result = new " + name + "();\n" : cpp ? "    " + name + " srl_result{};\n" :
        "    " + name + " srl_result = new " + name + "();\n";
    for (const auto& assignment : factory.assignments) {
      const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) { return field.name == assignment.field; });
      if (found == owner.member_list.end()) { throw std::invalid_argument{"Unbound construction field"}; }
      auto value = *found;
      value.default_value = assignment.value;
      value.default_factory = assignment.factory_default ? assignment.value : std::string{};
      const auto parameter = assignment.parameter ?
          (rust ? "_srl_arg_" : "srl_arg_") + identifier(assignment.value, access_type::public_access) : std::string{};
      auto assigned_value = assignment.expression ? std::string{} : assignment.parameter ?
          (cpp ? "std::move(" + parameter + ")" : parameter) : initial(value);
      if (assignment.expression) {
        behavior::expression_context context;
        context.native_type = type;
        context.member_name = [&](auto field) { return identifier(field, access_type::public_access); };
        assigned_value = behavior::expression(*assignment.expression, language,
            [&](auto symbol) { return "srl_arg_" + identifier(symbol, access_type::public_access); }, {}, context);
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
        inputs += index < arity ? (cpp ? "std::move(" + parameter_names[index] + ")" : parameter_names[index]) : default_values[index];
      }
      const auto wrapper = id + "_default_" + std::to_string(arity);
      const auto target = cpp || rust ? id : go ? go_owner + "_" + id : name + "." + id;
      output += python ? "  @staticmethod\n  def " + wrapper + "(" + declarations + "):\n    \"\"\"Construct with fresh defaults for omitted trailing arguments.\"\"\"\n    return " + target + "(" + inputs + ")\n" :
          rust ? "  /// Construct with fresh defaults for omitted trailing arguments.\n  " + std::string(factory.access == access_type::public_access ? "pub " : "") + "fn " + wrapper + "(" + declarations + ") -> Self { Self::" + target + "(" + inputs + ") }\n" :
          go ? "// Construct with fresh defaults for omitted trailing arguments.\nfunc " + go_owner + "_" + wrapper + "(" + declarations + ") *" + name + " { return " + target + "(" + inputs + ") }\n" :
          swift ? "  /// Construct with fresh defaults for omitted trailing arguments.\n  " + std::string{visibility} + "static func " + wrapper + "(" + declarations + ") -> " + name + " { return " + target + "(" + inputs + ") }\n" :
          kotlin ? "  // Construct with fresh defaults for omitted trailing arguments.\n  " + std::string{visibility} + "fun " + wrapper + "(" + declarations + "): " + name + " { return " + target + "(" + inputs + ") }\n" :
          cpp ? std::string{visibility} + ":\n  // Construct with fresh defaults for omitted trailing arguments.\n  [[nodiscard]] static " + constant + name + " " + wrapper + "(" + declarations + ")" + exception + " { return " + target + "(" + inputs + "); }\n" :
          js ? "  // Construct with fresh defaults for omitted trailing arguments.\n  static " + wrapper + "(" + declarations + ") { return " + target + "(" + inputs + "); }\n" :
          "  // Construct with fresh defaults for omitted trailing arguments.\n  " + std::string{visibility} + "static " + name + " " + wrapper + "(" + declarations + ") { return " + target + "(" + inputs + "); }\n";
    }
  }
  if (explicit_constructor) { output += "public:\n  // Preserve default construction for codecs and containers.\n  " + std::string{constexpr_owner ? "constexpr " : ""} + name + "()" + std::string{nothrow_owner ? " noexcept" : ""} + " = default;\n"; }
  return output;
}

} // namespace rohit::serializer::writer::construction
