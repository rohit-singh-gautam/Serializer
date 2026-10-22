// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "conversion_contract.hpp"
#include <cctype>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::translator {

using type_renderer = std::function<std::string(const type_name&)>;
using name_renderer = std::function<std::string(std::string_view, access_type)>;
using default_renderer = std::function<std::string(const member&)>;

// Retain a target slot's exact type and either a source projection or a fresh leaf default.
struct slot {
  member field{};
  std::string source{};
  bool mapped{false};
  bool parent{false};
};

// Bind typed nested defaults to the projected target's actual generic arguments.
inline void bind_initializer(value_initializer& value, const type_name& target) {
  value.type = ::rohit::serializer::conversion::bound_type(std::move(value.type), target);
  for (auto& element : value.elements) { bind_initializer(element, target); }
}

// Resolve generated source navigation without evaluating or copying the projected values.
inline std::string source_path(const conversion_assignment& assignment, std::string source,
    std::string_view language, const name_renderer& name) {
  for (std::size_t index{}; index < assignment.source_path.size(); ++index) {
    if (language != "cpp") {
      for (const auto base : assignment.source_base_paths.at(index)) {
        source += "." + std::string{language == "go" || language == "csharp" ? "Base" : "base"} + std::to_string(base);
      }
    }
    source += "." + name(assignment.source_path[index], access_type::public_access);
  }
  return source;
}

// Describe all stored target slots, including fresh public parent defaults.
inline std::vector<slot> slots(const conversion_declaration& method, std::string_view language,
    const name_renderer& name) {
  const auto& target = ::rohit::serializer::conversion::record(method.target);
  std::vector<slot> result;
  for (std::size_t index{}; index < target.parents.size(); ++index) {
    member field{};
    field.name = std::string{language == "go" ? "Base" : "base"} + std::to_string(index);
    field.access = access_type::public_access;
    field.modifier = member::modifier_type::none;
    field.type_name_list.push_back(::rohit::serializer::conversion::base_type(target.parents[index], method.target));
    result.push_back({std::move(field), {}, false, true});
  }
  for (const auto& field : target.member_list) {
    if (field.wire_only) { continue; }
    auto bound = field;
    bound.type_name_list.front() = ::rohit::serializer::conversion::bound_type(field.type_name_list.front(), method.target);
    if (bound.initializer) { bind_initializer(*bound.initializer, method.target); }
    slot value{std::move(bound), {}, false};
    for (const auto& assignment : method.assignments) {
      if (assignment.field != field.name) { continue; }
      value.mapped = true;
      value.source = source_path(assignment, language == "cpp" || language == "c" ? "(*srl_source)" :
          language == "swift" ? "srl_source.pointee" : "srl_source", language, name);
      break;
    }
    result.push_back(std::move(value));
  }
  return result;
}

// Use deterministic target getters only while emitting a projected output traversal.
inline std::map<std::string, std::string> output_fields(const conversion_declaration& method,
    std::string_view language, const name_renderer& name) {
  std::map<std::string, std::string> result;
  for (const auto& value : slots(method, language, name)) {
    if (language == "c" || language == "go" || language == "swift") {
      result.emplace(value.field.name, value.mapped ?
          value.source.substr(language == "c" ? std::string{"(*srl_source)."}.size() : 0) :
          "srl_default_" + name(value.field.name, access_type::public_access));
      if (language == "c" && value.mapped) {
        result[value.field.name] = "srl_source->" + result[value.field.name];
      }
    } else {
      result.emplace(value.field.name, name("get_" + value.field.name, access_type::public_access) + "()");
    }
  }
  return result;
}

// Return a backend-safe helper name and reject collisions inside the projected API.
inline std::string view_name(std::string_view owner, const conversion_declaration& method,
    const name_renderer& name, std::string_view language) {
  const auto spelling = std::string{owner} + "_" + name(method.name, method.access) + "_translator";
  if (language == "cpp" || language == "c") { return spelling; }
  std::string result;
  bool capitalize{true};
  for (const auto character : spelling) {
    if (character == '_') { capitalize = true; continue; }
    result += capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(character))) : character;
    capitalize = false;
  }
  return result;
}

// Preserve copied native scalar getters while lending every non-Copy owning Rust container.
inline bool rust_getter_borrows(const type_name& type) {
  if (type.is_optional()) { return rust_getter_borrows(type.arguments.front()); }
  return type.name == "string" || type.name.starts_with("version") || type.type == object_type::class_type ||
      type.is_digest() || type.is_sequence() || type.is_associative();
}

// Emit scoped readonly projections; codecs are supplied by the backend's ordinary target traversal.
inline std::string methods(const class_node& owner, std::string_view owner_name,
    std::string_view language, const type_renderer& type, const name_renderer& name,
    const default_renderer& initial,
    const std::function<std::string(const conversion_declaration&, std::string_view)>& codecs,
    bool rust_declarations = false,
    const std::function<std::string(const conversion_declaration&)>& extra_storage = {}) {
  std::string output;
  const bool cpp = language == "cpp", c = language == "c", rust = language == "rust";
  const bool swift = language == "swift", kotlin = language == "kotlin", python = language == "python";
  const bool java = language == "java", cs = language == "csharp", go = language == "go";
  const bool js = language == "js", ts = language == "typescript";
  for (const auto& method : owner.conversions) {
    if (!method.translator) { continue; }
    const auto values = slots(method, language, name);
    const auto view = view_name(owner_name, method, name, language);
    const auto function = name(method.name, method.access);
    const auto scoped = name("with_" + method.name, method.access);
    std::set<std::string> names{"encode", "Encode", "serialize_out", "write", "Write", "srlWrite", "srl_write"};
    for (const auto& value : values) {
      if (!names.insert(name("get_" + value.field.name, access_type::public_access)).second) {
        throw std::invalid_argument{"Translator getter collides with generated output API"};
      }
    }
    if (ts) {
      std::string shape{"{ "};
      for (const auto& value : values) {
        shape += name("get_" + value.field.name, access_type::public_access) + "(): " + type(value.field.type_name_list.front()) + "; ";
      }
      const auto extra = codecs(method, view);
      shape += "encode(protocol: Protocol): Uint8Array" + (extra.empty() ? " }" : "; " + extra + " }");
      output += "  " + scoped + "<Result>(action: (view: " + shape + ") => Result): Result;\n";
      continue;
    }
    if (rust && !rust_declarations) {
      std::string build = view + " { srl_source: self";
      for (const auto& value : values) {
        if (!value.mapped) { build += ", srl_default_" + name(value.field.name, access_type::public_access) + ": " + initial(value.field); }
      }
      build += " }";
      output += "  /// Borrow readonly target projections for one synchronous callback.\n  pub fn " + scoped +
          "<Result>(&self, action: impl FnOnce(" + view + "<'_>) -> Result) -> Result { action(" + build + ") }\n";
      output += "  /// Borrow this source with a compiler-checked native lifetime.\n  pub fn " + function +
          "(&self) -> " + view + "<'_> { " + build + " }\n";
      continue;
    }
    if (cpp) {
      output += "public:\n  // Borrow target projections; the source must outlive this view and every returned reference.\n  struct " + view;
      const auto& target = ::rohit::serializer::conversion::record(method.target);
      for (std::size_t index{}; index < target.parents.size(); ++index) {
        output += std::string{index == 0 ? " : " : ", "} + "private " + type(::rohit::serializer::conversion::base_type(target.parents[index], method.target));
      }
      output += " {\n  private:\n    const " + std::string{owner_name} + "* srl_source;\n";
      output += "  public:\n    // Keep the source address without copying its owned model.\n    explicit constexpr " + view + "(const " + std::string{owner_name} + "* source) : srl_source{source} {}\n";
    } else if (rust) {
      output += "/// A readonly target projection borrowing its source without constructing a target.\npub struct " + view + "<'a> {\n  srl_source: &'a " + std::string{owner_name} + ",\n";
    } else if (go) {
      output += "// " + view + " borrows its source only for the synchronous callback.\ntype " + view + " struct {\n  srl_source *" + std::string{owner_name} + "\n";
    } else if (c) {
      output += "/* Borrowed target projections; the callback must not retain this view. */\ntypedef struct " + view + " {\n  const " + std::string{owner_name} + "* srl_source;\n";
    } else if (python) {
      output += "  class " + view + ":\n    \"\"\"Readonly target projections valid during the source callback.\"\"\"\n    def __init__(self, source):\n      \"\"\"Retain the source handle only for this synchronous projection.\"\"\"\n      self.srl_source = source\n";
    } else if (js) {
      output += "  // Readonly target projections, created for one synchronous callback.\n  static " + view + " = class {\n    // Retain the source handle without copying its owning model.\n    constructor(source) { this.srl_source = source;\n";
    } else if (java) {
      output += "  /** Readonly target projections; retain the source only for the callback. */\n  public static final class " + view + " {\n    private final " + std::string{owner_name} + " srl_source;\n    /** Retain the source handle without copying its owning model. */\n    private " + view + "(" + std::string{owner_name} + " source) { this.srl_source = source; }\n";
    } else if (cs) {
      output += "  // Readonly target projections valid during the synchronous callback.\n  public sealed class " + view + " {\n    private readonly " + std::string{owner_name} + " srl_source;\n    // Retain the source handle without copying its owning model.\n    internal " + view + "(" + std::string{owner_name} + " source) { srl_source = source; }\n";
    } else if (swift) {
      output += "  /// Readonly target projections; do not retain this scoped pointer view.\n  public struct " + view + " {\n    fileprivate let srl_source: UnsafePointer<" + std::string{owner_name} + ">\n";
    } else if (kotlin) {
      output += "  // Readonly target projections valid during the synchronous callback.\n  class " + view + " internal constructor(private val srl_source: " + std::string{owner_name} + ") {\n";
    }
    if (cpp) { output += "  private:\n"; }
    for (const auto& value : values) {
      if (value.mapped || (cpp && value.parent)) { continue; }
      const auto storage = "srl_default_" + name(value.field.name, access_type::public_access);
      const auto native_type = type(value.field.type_name_list.front());
      if (cpp) { output += "    " + native_type + " " + storage + "{" + initial(value.field) + "};\n"; }
      else if (rust) { output += "  " + storage + ": " + native_type + ",\n"; }
      else if (go) { output += "  " + storage + " " + native_type + "\n"; }
      else if (c) { output += "  " + native_type + " " + storage + ";\n"; }
      else if (python) { output += "      self." + storage + " = " + initial(value.field) + "\n"; }
      else if (js) { output += "      this." + storage + " = " + initial(value.field) + ";\n"; }
      else if (java || cs) { output += "    private " + std::string{java ? "final " : "readonly "} + native_type + " " + storage + " = " + initial(value.field) + ";\n"; }
      else if (swift) { output += "    fileprivate var " + storage + ": " + native_type + " = " + initial(value.field) + "\n"; }
      else if (kotlin) { output += "    private val " + storage + ": " + native_type + " = " + initial(value.field) + "\n"; }
    }
    if (extra_storage) { output += extra_storage(method); }
    if (js) { output += "    }\n"; }
    if (c) { output += "} " + view + ";\n"; }
    if (go) { output += "}\n"; }
    if (rust) { output += "}\nimpl<'a> " + view + "<'a> {\n"; }
    if (cpp) { output += "  public:\n"; }
    for (const auto& value : values) {
      const auto getter = name("get_" + value.field.name, access_type::public_access);
      const auto native_type = type(value.field.type_name_list.front());
      const auto storage = "srl_default_" + name(value.field.name, access_type::public_access);
      auto expression = value.mapped ? value.source : storage;
      if (python || js || java || cs) { expression = value.mapped ? "this." + expression : "this." + storage; }
      if (python) {
        if (expression.starts_with("this.")) { expression.replace(0, 5, "self."); }
        output += "    def " + getter + "(self):\n      \"\"\"Read one borrowed projection without materializing its target.\"\"\"\n      return " + expression + "\n";
      } else if (cpp) {
        if (value.parent) { expression = "static_cast<const " + native_type + "&>(*this)"; }
        output += "    // Borrow one target field from the live source or this view's fresh default.\n    [[nodiscard]] constexpr const " + native_type + "& " + getter + "() const & noexcept { return " + expression + "; }\n";
        output += "    // A temporary view cannot lend a reference to its owned default fields.\n    const " + native_type + "& " + getter + "() const && = delete;\n";
      } else if (rust) {
        const bool borrowed = rust_getter_borrows(value.field.type_name_list.front());
        output += "  /// Read one projection without copying owned source storage.\n  pub fn " + getter + "(&self) -> " + (borrowed ? "&" : "") + native_type + " { " + (borrowed ? "&" : "") + (value.mapped ? "self." + expression : "self." + storage) + " }\n";
      } else if (go) {
        output += "// " + getter + " reads one target projection.\nfunc (value " + view + ") " + getter + "() " + native_type + " { return " + (value.mapped ? "value." + expression : "value." + storage) + " }\n";
      } else if (c) {
        expression = value.mapped ? source_path(method.assignments.at(0), "(*value->srl_source)", "c", name) : "value->" + storage;
        if (value.mapped) {
          for (const auto& assignment : method.assignments) {
            if (assignment.field == value.field.name) { expression = source_path(assignment, "(*value->srl_source)", "c", name); break; }
          }
        }
        output += "/* Borrow one target field; its source must remain alive. */\nstatic inline const " + native_type + "* " + view + "_" + getter + "(const " + view + "* value) { return &" + expression + "; }\n";
      } else if (swift) {
        output += "    /// Read a target field while the source pointer remains scoped.\n    public func " + getter + "() -> " + native_type + " { return " + expression + " }\n";
      } else if (kotlin) {
        output += "    // Read one target projection.\n    fun " + getter + "(): " + native_type + " = " + expression + "\n";
      } else if (js) {
        output += "    // Read one target projection.\n    " + getter + "() { return " + expression + "; }\n";
      } else {
        output += "    // Read one target projection.\n    public " + native_type + " " + getter + "() { return " + expression + "; }\n";
      }
    }
    output += codecs(method, view);
    if (!c && !go) { output += python ? "" : js || cpp ? "  };\n" : "  }\n"; }
    if (rust) { continue; }
    if (cpp) {
      output += "  // Borrow target projections only from a live lvalue source.\n  [[nodiscard]] constexpr " + view + " " + function + "() const & { return " + view + "{this}; }\n";
      output += "  // Reject a temporary source before a borrowed translator can escape.\n  " + view + " " + function + "() const && = delete;\n";
      output += "  // Invoke the callback synchronously while every projected field remains borrowed.\n  template <typename Callback> constexpr decltype(auto) " + scoped + "(Callback&& action) const & { return ::std::forward<Callback>(action)(" + function + "()); }\n";
    } else if (python) {
      output += "  def " + scoped + "(self, action):\n    \"\"\"Run a synchronous readonly projection callback; do not retain its view.\"\"\"\n    return action(" + std::string{owner_name} + "." + view + "(self))\n";
    } else if (js) {
      output += "  // Run a synchronous readonly projection callback.\n  " + scoped + "(action) { return action(new " + std::string{owner_name} + "." + view + "(this)); }\n";
    } else if (java) {
      output += "  /** Run a synchronous readonly projection callback. */\n  public <Result> Result " + scoped + "(java.util.function.Function<" + view + ", Result> action) { return action.apply(new " + view + "(this)); }\n";
    } else if (cs) {
      output += "  // Run a synchronous readonly projection callback.\n  public Result " + scoped + "<Result>(System.Func<" + view + ", Result> action) { return action(new " + view + "(this)); }\n";
    } else if (kotlin) {
      output += "  // Run a synchronous readonly projection callback.\n  fun <Result> " + scoped + "(action: (" + view + ") -> Result): Result = action(" + view + "(this))\n";
    } else if (swift) {
      output += "  /// Borrow the source during one synchronous callback; its view must not escape.\n  public mutating func " + scoped + "<Result>(_ action: (" + view + ") throws -> Result) rethrows -> Result {\n    return try withUnsafePointer(to: &self) { pointer in try action(" + view + "(srl_source: pointer)) }\n  }\n";
    } else if (go) {
      std::string build = view + "{srl_source: value";
      for (const auto& value : values) { if (!value.mapped) { build += ", srl_default_" + name(value.field.name, access_type::public_access) + ": " + initial(value.field); } }
      output += "// " + scoped + " invokes one synchronous readonly projection callback.\nfunc (value *" + std::string{owner_name} + ") " + scoped + "(action func(" + view + ")) { action(" + build + "}) }\n";
    }
  }
  return output;
}

} // namespace rohit::serializer::writer::translator
