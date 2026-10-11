// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "behavior_writer.hpp"
#include "construction_qualification.hpp"
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
    if (!parameters.empty()) { parameters += ", "; constraints += " && "; }
    parameters += "typename " + parameter.name;
    constraints += "(" + input_constraint(*parameter.constraint, parameter.name, type, callable_identifier) + ")";
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
  if (language != "cpp" && std::count_if(owner.constructions.begin(), owner.constructions.end(),
      [](const auto& factory) { return factory.constructor; }) > 1) {
    throw std::invalid_argument{std::string{language} + ": constructor overloads require C++ output"};
  }
  for (const auto& factory : owner.constructions) {
    if (language != "cpp" && (!factory.validations.empty() || !factory.native_validations.empty() ||
        !factory.initializers.empty())) {
      throw std::invalid_argument{std::string{language} + ": authoring validation and initialization require C++ output"};
    }
    if (language != "cpp" && !factory.generic_parameters.empty()) {
      throw std::invalid_argument{std::string{language} + ": independently generic construction inputs require C++ output"};
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
  const bool constexpr_owner = cpp && constexpr_construction(owner);
  const bool nothrow_owner = cpp && nothrow_construction(owner);
  // Calls require ordered statement evaluation; retain existing lowering for other bindings.
  const auto expression_text = [&](const behavior_expression& expression,
      const std::function<std::string(std::string_view)>& symbol,
      const behavior::expression_context& context) {
    if (cpp && behavior::has_call(expression) && checked_expression) {
      return checked_expression(expression, symbol);
    }
    return behavior::expression(expression, language, symbol, {}, context);
  };
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
    const bool authoring = !factory.validations.empty() || !factory.native_validations.empty() || !factory.initializers.empty();
    const std::string constant = cpp && constexpr_owner && constexpr_bindings &&
        factory.native_validations.empty() && factory.initializers.empty() &&
        std::all_of(factory.validations.begin(), factory.validations.end(), constexpr_binding) &&
        constexpr_construction(owner, factory.parameters, factory.generic_parameters) ? "constexpr " : "";
    const bool checked_binding = std::any_of(factory.assignments.begin(), factory.assignments.end(),
        [](const auto& assignment) { return assignment.expression.has_value(); });
    const bool default_factory = std::any_of(factory.parameters.begin(), factory.parameters.end(),
        [](const auto& parameter) { return !parameter.default_factory.empty(); });
    const std::string exception = cpp && !authoring && !checked_binding && !default_factory && nothrow_owner && nothrow_construction(owner, factory.parameters) ? " noexcept" : "";
    const auto template_declaration = cpp ? input_template(factory, type, callable_identifier) : std::string{};
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
        context.call = call;
        context.native_type = type;
        context.member_name = [&](auto field) { return identifier(field, access_type::public_access); };
        const auto bound = assignment.expression ? expression_text(*assignment.expression,
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
    if (cpp && authoring) {
      output += "    return " + name + "{" + arguments + "};\n  }\n";
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
        value.default_value = assignment.value;
        value.default_factory = assignment.factory_default ? assignment.value : std::string{};
        const auto parameter = assignment.parameter ?
            (rust ? "_srl_arg_" : "srl_arg_") + identifier(assignment.value, access_type::public_access) : std::string{};
        auto assigned_value = assignment.expression ? std::string{} : assignment.parameter ?
            (cpp ? "std::move(" + parameter + ")" : parameter) : initial(value);
        if (assignment.expression) {
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
