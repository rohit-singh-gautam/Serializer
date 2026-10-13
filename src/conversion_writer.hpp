// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "conversion_contract.hpp"
#include "construction_writer.hpp"
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::conversion {

// Reject conversion APIs that collide with generated names under the selected naming profile.
inline void validate_names(const class_node& owner, std::string_view owner_name,
    const std::function<std::string(std::string_view, access_type)>& field,
    const std::function<std::string(std::string_view, access_type)>& callable) {
  std::set<std::string> names{std::string{owner_name}, "encode", "decode", "Encode", "Decode",
      "read", "write", "Read", "Write", "init", "free", "default", "new", "constructor",
      "serialize", "deserialize", "serialize_in", "serialize_out", "srlRead", "srlWrite",
      "serializer_reuses_storage", "toString", "hashCode", "equals", "getClass", "clone",
      "finalize", "wait", "notify", "notifyAll", "equals_value", "compare_value",
      "EqualsValue", "CompareValue"};
  for (const auto& item : owner.member_list) {
    if (!item.wire_only) { names.insert(field(item.name, item.access)); }
  }
  for (const auto& item : owner.functions) { names.insert(callable(item.name, item.access)); }
  for (const auto& item : owner.constructions) { names.insert(callable(item.constructor ? "create" : item.name, item.access)); }
  for (const auto& item : owner.conversions) {
    const auto name = callable(item.name, item.access);
    if (!names.insert(name).second) { throw std::invalid_argument{"Generated conversion name collision: " + name}; }
  }
}

// Emit explicit independent value conversion methods; C publishes a candidate only after success.
// An optional section callback shares access changes with the containing C++ class emitter.
inline std::string methods(const class_node& owner, std::string_view owner_name,
    std::string_view language, const std::function<std::string(const type_name&)>& type,
    const std::function<std::string(std::string_view, access_type)>& field,
    const std::function<std::string(std::string_view, access_type)>& callable = {},
    const std::function<std::string(access_type)>& cpp_access_section = {}) {
  if (owner.conversions.empty()) { return {}; }
  const auto function = callable ? callable : field;
  validate_names(owner, owner_name, field, function);
  const bool cpp = language == "cpp", c = language == "c", rust = language == "rust";
  const bool python = language == "python", go = language == "go", swift = language == "swift";
  const bool kotlin = language == "kotlin", js = language == "js", ts = language == "typescript";
  const bool csharp = language == "csharp";
  const std::string self = go ? "value" : python || swift || rust ? "self" : c ? "(*source)" : cpp ? "(*this)" : "this";
  const std::string terminator = python || swift || kotlin || go ? "\n" : ";\n";
  const auto base_member = [&](std::size_t index) {
    return std::string{go || csharp ? "Base" : "base"} + std::to_string(index);
  };
  std::string output;
  for (const auto& method : owner.conversions) {
    const auto name = function(method.name, method.access);
    const auto target = type(method.target);
    const auto fresh_name = go && target.starts_with('*') ? target.substr(1) : target;
    const auto& target_owner = ::rohit::serializer::conversion::record(method.target);
    const bool constant = cpp && construction::constexpr_construction(owner) &&
        construction::constexpr_construction(target_owner);
    const bool nothrow = cpp && construction::nothrow_construction(owner) &&
        construction::nothrow_construction(target_owner);
    const std::string visibility = method.access == access_type::public_access ? "public " :
        method.access == access_type::protected_access ? "protected " : "private ";
    if (c) {
      const auto result_type = std::string{owner_name} + "_" + name + "_result";
      output += "/* Public result alias keeps concrete generic target names out of caller code. */\ntypedef " +
          target + " " + result_type + ";\n/* Release an initialized conversion result. */\nstatic inline void " +
          result_type + "_free(" + result_type + "* value) { " + target + "_free(value); }\n";
    }
    if (cpp) {
      output += cpp_access_section ? cpp_access_section(method.access) :
          std::string{visibility}.substr(0, visibility.size() - 1) + ":\n";
    }
    output += python ? "  def " + name + "(self) -> " + target + ":\n" :
        cpp ? "  // Copy public projections into fresh independent target defaults.\n  [[nodiscard]] " + std::string{constant ? "constexpr " : ""} + target + " " + name + "() const" + std::string{nothrow ? " noexcept" : ""} + " {\n" :
        c ? "/* Copy borrowed source into initialized/zeroed result. Failure preserves both; ownership must be disjoint. */\nstatic inline srl_status " + std::string{owner_name} + "_" + name + "(" + target + "* result, const " + std::string{owner_name} + "* source) {\n" :
        rust ? "  /// Copy public projections without consuming the receiver.\n  " + std::string{method.access == access_type::public_access ? "pub " : ""} + "fn " + name + "(&self) -> " + target + " {\n" :
        go ? "// " + name + " copies into independent target defaults.\nfunc (value *" + std::string{owner_name} + ") " + name + "() " + target + " {\n" :
        swift ? "  /// Copy into independent target defaults.\n  " + std::string{method.access == access_type::public_access ? "public " : "private "} + "func " + name + "() -> " + target + " {\n" :
        kotlin ? "  // Copy into independent target defaults.\n  " + std::string{method.access == access_type::public_access ? "" : "private "} + "fun " + name + "(): " + target + " {\n" :
        ts ? "  " + std::string{visibility} + name + "(): " + target + ";\n" :
        js ? "  // Copy into independent target defaults.\n  " + name + "() {\n" :
        "  // Copy into independent target defaults.\n  " + std::string{visibility} + target + " " + name + "() {\n";
    if (ts) { continue; }
    if (c) { output += "  if (!source || !result || (const void*)source == (const void*)result) { return srl_invalid; }\n"; }
    output += python ? "    \"\"\"Return an independent value; leave the receiver unchanged.\"\"\"\n    srl_result = " + target + "()\n" :
        c ? "  " + target + " srl_result = {0}; srl_status status = " + target + "_init(&srl_result);\n" :
        cpp ? "    " + target + " srl_result{};\n" :
        rust ? "    let " + std::string{method.assignments.empty() ? "" : "mut "} + "srl_result = " + target + "::default();\n" :
        go ? "    srl_result := New" + fresh_name + "()\n" :
        swift ? "    var srl_result = " + target + "()\n" :
        kotlin ? "    val srl_result = " + target + "()\n" :
        "    " + std::string{js ? "const " : "var "} + "srl_result = new " + target + "();\n";
    const auto copy_begin = output.size();
    // Reference-based targets already construct fresh children; recursively copy their full declared value.
    std::function<void(const type_name&, const std::string&, const std::string&)> copy;
    copy = [&](const type_name& value, const std::string& destination, const std::string& source) {
      if (value.resolved_node && (value.resolved_node->type == object_type::class_type ||
          value.resolved_node->type == object_type::generic_definition) && !cpp && !rust) {
        const auto& record = ::rohit::serializer::conversion::record(value);
        for (std::size_t index = 0; index < record.parents.size(); ++index) {
          copy(::rohit::serializer::conversion::base_type(record.parents[index], value),
              destination + "." + base_member(index), source + "." + base_member(index));
        }
        for (const auto& item : record.member_list) {
          if (item.wire_only) { continue; }
          copy(::rohit::serializer::conversion::bound_type(item.type_name_list.front(), value),
              destination + "." + field(item.name, item.access), source + "." + field(item.name, item.access));
        }
      } else if (c && (value.name == "string" || value.name.starts_with("version"))) {
        output += "  if (status == srl_ok) { if (" + source + ".size && !" + source +
            ".data) { status = srl_invalid; } else { status = srl_string_set(&" + destination +
            ", " + source + ".data, " + source + ".size); } }\n";
      } else {
        output += "    " + destination + " = " + source + (rust ? ".clone()" : "") + terminator;
      }
    };
    for (const auto& assignment : method.assignments) {
      if (assignment.source_path.empty() || assignment.source_base_paths.size() != assignment.source_path.size()) {
        throw std::invalid_argument{"Conversion projection metadata is not resolved"};
      }
      auto source = self;
      for (std::size_t index = 0; index < assignment.source_path.size(); ++index) {
        if (!cpp) {
          for (const auto base : assignment.source_base_paths[index]) { source += "." + base_member(base); }
        }
        source += "." + field(assignment.source_path[index], access_type::public_access);
      }
      const auto found = std::find_if(target_owner.member_list.begin(), target_owner.member_list.end(),
          [&](const auto& item) { return item.name == assignment.field; });
      if (found == target_owner.member_list.end() || found->wire_only) {
        throw std::invalid_argument{"Conversion target field metadata is not resolved"};
      }
      copy(::rohit::serializer::conversion::bound_type(found->type_name_list.front(), method.target),
          "srl_result." + field(found->name, found->access), source);
    }
    if (swift && output.find("srl_result.", copy_begin) == std::string::npos) {
      const auto declaration = output.rfind("var srl_result = ");
      output.replace(declaration, 3, "let");
    }
    output += c ? "  if (status != srl_ok) { " + target + "_free(&srl_result); return status; }\n  " + target + "_free(result); *result = srl_result; return srl_ok;\n}\n" :
        python ? "    return srl_result\n" : rust ? "    srl_result\n  }\n" :
        "    return srl_result" + terminator + "  }\n";
    if (cpp && std::count_if(owner.conversions.begin(), owner.conversions.end(), [&](const auto& candidate) {
        return candidate.target == method.target;
      }) == 1) {
      output += "  // Explicit casts use the sole declared conversion to this target.\n  [[nodiscard]] explicit " +
          std::string{constant ? "constexpr " : ""} + "operator " + target + "() const" +
          std::string{nothrow ? " noexcept" : ""} + " { return " + name + "(); }\n";
    }
  }
  return output;
}

} // namespace rohit::serializer::writer::conversion
