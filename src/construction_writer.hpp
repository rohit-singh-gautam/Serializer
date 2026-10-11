// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::construction {

// Prove known storage supports C++20 construction; dependent generic values defer to specialization.
inline bool constexpr_construction(const class_node& owner,
    const std::vector<function_parameter>& parameters = {}) {
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
    return std::find(scope.type_parameters.begin(), scope.type_parameters.end(), value.name) != scope.type_parameters.end();
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
    if (field.modifier != member::modifier_type::none || field.type_name_list.size() != 1 ||
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
    const std::string constant = cpp && constexpr_owner && constexpr_construction(owner, factory.parameters) ? "constexpr " : "";
    const std::string exception = cpp && nothrow_owner && nothrow_construction(owner, factory.parameters) ? " noexcept" : "";
    std::string parameters, arguments;
    for (const auto& parameter : factory.parameters) {
      const auto parameter_name = (rust ? "_srl_arg_" : "srl_arg_") + identifier(parameter.name, access_type::public_access);
      if (!parameters.empty()) { parameters += ", "; arguments += ", "; }
      arguments += cpp ? "std::move(" + parameter_name + ")" : parameter_name;
      parameters += python || js ? parameter_name :
          rust || swift || kotlin || typescript ? (swift ? "_ " : "") + parameter_name + ": " + type(parameter.type) :
          go ? parameter_name + " " + type(parameter.type) :
          std::string{cpp ? "[[maybe_unused]] " : ""} + type(parameter.type) + " " + parameter_name;
    }
    if (cpp && factory.constructor) {
      explicit_constructor = true;
      output += std::string{visibility} + ":\n  // Construct owned fields from checked schema bindings.\n  " + constant + name + "(" + parameters + ")" + exception + " : " + name + "() {\n";
      for (const auto& assignment : factory.assignments) {
        const auto found = std::find_if(owner.member_list.begin(), owner.member_list.end(), [&](const auto& field) { return field.name == assignment.field; });
        auto value = *found;
        value.default_value = assignment.value;
        value.default_factory = assignment.factory_default ? assignment.value : std::string{};
        output += "    this->" + identifier(found->name, found->access) + " = " + (assignment.parameter ? "std::move(srl_arg_" + identifier(assignment.value, access_type::public_access) + ")" : initial(value)) + ";\n";
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
        cpp ? std::string{visibility} + ":\n  // Construct a fresh owned schema value.\n  [[nodiscard]] static " + constant + name + " " + id + "(" + parameters + ")" + exception + " {\n" :
        "  // Construct a fresh owned schema value.\n  " + std::string{visibility} + "static " + name + " " + id + "(" + parameters + ") {\n";
    if (typescript) { continue; }
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
      auto assigned_value = assignment.parameter ?
          (cpp ? "std::move(" + parameter + ")" : parameter) : initial(value);
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
  if (explicit_constructor) { output += "public:\n  // Preserve default construction for codecs and containers.\n  " + std::string{constexpr_owner ? "constexpr " : ""} + name + "()" + std::string{nothrow_owner ? " noexcept" : ""} + " = default;\n"; }
  return output;
}

} // namespace rohit::serializer::writer::construction
