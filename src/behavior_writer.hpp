// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::behavior {

// Select an authored implementation without interpreting or rewriting its native tokens.
inline const native_code_block* native_body(const function_declaration& method, std::string_view language) {
  const auto found = std::find_if(method.bodies.begin(), method.bodies.end(),
      [&](const auto& code) { return code.language == language || (language == "js" && code.language == "javascript"); });
  return found == method.bodies.end() ? nullptr : &*found;
}

// Opaque source may refer to generated identifiers only when their spelling is preserved.
inline void require_native_names(const class_node& owner, std::string_view language, bool renamed) {
  const bool blocks = std::any_of(owner.native_blocks.begin(), owner.native_blocks.end(),
      [&](const auto& code) { return code.language == language || (language == "js" && code.language == "javascript"); });
  const bool bodies = std::any_of(owner.functions.begin(), owner.functions.end(),
      [&](const auto& method) { return native_body(method, language) != nullptr; });
  if (renamed && (blocks || bodies)) {
    throw std::invalid_argument{std::string{language} + ": opaque native code requires naming = preserve"};
  }
}

// Lower already type-checked expressions using bound backend field/parameter spellings.
inline std::string expression(const behavior_expression& node, std::string_view language,
                              const std::function<std::string(std::string_view)>& symbol) {
  using operation = behavior_expression::operation;
  switch (node.kind) {
  case operation::literal:
    return node.value.find_first_of(".eE") == std::string::npos ? node.value + ".0" : node.value;
  case operation::symbol: return symbol(node.value);
  case operation::pi: return "3.141592653589793";
  case operation::to_double: {
    const auto value = expression(node.operands.at(0), language, symbol);
    if (language == "cpp") { return "static_cast<double>(" + value + ")"; }
    if (language == "java" && node.value == "uint32") { return "((double)Integer.toUnsignedLong(" + value + "))"; }
    if (language == "rust") { return "(" + value + " as f64)"; }
    if (language == "python") { return "float(" + value + ")"; }
    if (language == "go") { return "float64(" + value + ")"; }
    if (language == "swift") { return "Double(" + value + ")"; }
    if (language == "kotlin") { return "(" + value + ").toDouble()"; }
    if (language == "js" || language == "typescript") { return "Number(" + value + ")"; }
    return "((double)(" + value + "))";
  }
  case operation::positive: return std::string(language == "rust" ? "(" : "(+") + expression(node.operands.at(0), language, symbol) + ")";
  case operation::negative: return "(-" + expression(node.operands.at(0), language, symbol) + ")";
  case operation::add:
  case operation::subtract:
  case operation::multiply:
    return "(" + expression(node.operands.at(0), language, symbol) +
        (node.kind == operation::add ? " + " : node.kind == operation::subtract ? " - " : " * ") +
        expression(node.operands.at(1), language, symbol) + ")";
  }
  throw std::invalid_argument{"Unsupported portable behavior expression"};
}

// Quote source metadata safely inside a comment or a native #line string literal.
inline std::string quote_source(std::string_view text, bool compact_json = false) {
  std::string result = "\"";
  constexpr char hex[] = "0123456789abcdef";
  for (const auto byte : text) {
    const auto value = static_cast<unsigned char>(byte);
    if (byte == '\\' || byte == '"') { result += '\\'; result += byte; }
    else if (value < 32 || (compact_json && value == 32)) { result += "\\u00"; result += hex[value >> 4]; result += hex[value & 15]; }
    else { result += byte; }
  }
  return result + "\"";
}

// Span anchors remain in final output so non-#line compilers can map generated diagnostics.
inline std::string source_marker(const auto& node, std::string_view language) {
  return std::string(language == "python" ? "# " : "// ") + "serializer-source-map:{\"schema\":" +
      quote_source(node.source_path.empty() ? "<schema>" : node.source_path, true) +
      ",\"line\":" + std::to_string(node.source_line) + ",\"column\":" + std::to_string(node.source_column) +
      ",\"bytes\":[" + std::to_string(node.source_offset) + "," + std::to_string(node.source_end) +
      "],\"generated_line\":@GENERATED_LINE@}\n";
}

// Restore native schema locations; the final pass computes generated reset lines after formatting.
inline std::string line_directive(const auto& node, std::string_view language) {
  if (language != "cpp" && language != "csharp") { return {}; }
  return "\n#line " + std::to_string(node.source_line) + " " +
      quote_source(node.source_path.empty() ? "<schema>" : node.source_path) + "\n";
}
// Mark generated C++ reset directives without confusing authored raw-string contents.
inline std::string reset_line(std::string_view language) {
  if (language == "cpp") { return "\n#line 1 \"<serializer-generated>\" // serializer-generated-line-reset\n"; }
  if (language == "csharp") { return "\n#line default\n"; }
  return {};
}

// Finalize physical positions only after complete output assembly and optional beautification.
inline std::string finalize_mappings(std::string source) {
  std::size_t begin{}, line{1};
  std::string output;
  while (begin < source.size()) {
    const auto end = source.find('\n', begin);
    auto text = source.substr(begin, end == std::string::npos ? end : end - begin);
    const auto nonspace = text.find_first_not_of(" \t\r");
    const bool carriage_return = !text.empty() && text.back() == '\r';
    const auto trimmed = nonspace == std::string::npos ? std::string_view{} :
        std::string_view{text}.substr(nonspace, text.size() - nonspace - (carriage_return ? 1 : 0));
    constexpr std::string_view generated_token = "\"generated_line\":@GENERATED_LINE@";
    if (trimmed.starts_with("// serializer-source-map:{") || trimmed.starts_with("# serializer-source-map:{")) {
      if (const auto position = text.find(generated_token); position != std::string::npos) {
        text.replace(position + std::string_view{"\"generated_line\":"}.size(),
            std::string_view{"@GENERATED_LINE@"}.size(), std::to_string(line + 1));
      }
    }
    if (trimmed == "#line 1 \"<serializer-generated>\" // serializer-generated-line-reset") {
      text = "#line " + std::to_string(line + 1) + " \"<serializer-generated>\"" + (carriage_return ? "\r" : "");
    }
    output += text;
    if (end == std::string::npos) { break; }
    output += '\n'; begin = end + 1; ++line;
  }
  return output;
}

// Return exact authored file hooks before generated schema declarations.
inline std::string preambles(const std::vector<std::unique_ptr<syntax_node>>& statements,
                             std::string_view language) {
  std::string output;
  for (const auto& node : statements) {
    if (node->type == object_type::native_code) {
      const auto& code = static_cast<const native_code_node&>(*node).code;
      if (code.language == language || (language == "js" && code.language == "javascript")) {
        output += source_marker(code, language) + line_directive(code, language) + code.text + reset_line(language) + "\n";
      }
    }
  }
  return output;
}

// Finite checks are explicit and do not depend on an optional external math library.
inline std::string finite(std::string_view value, std::string_view language = {}) {
  const std::string text{value};
  const auto conjunction = language == "python" ? " and " : " && ";
  return "(" + text + " == " + text + conjunction + text +
      " <= 1.7976931348623157e308" + conjunction + text + " >= -1.7976931348623157e308)";
}

// Inspect the already-resolved scalar contract for dynamic runtime range checks.
inline std::string_view symbol_type(const class_node& owner, const function_declaration& method,
                                    std::string_view name) {
  const auto parameter = std::find_if(method.parameters.begin(), method.parameters.end(),
      [&](const auto& item) { return item.name == name; });
  if (parameter != method.parameters.end()) { return parameter->type.name; }
  const auto field = std::find_if(owner.member_list.begin(), owner.member_list.end(),
      [&](const auto& item) { return item.name == name; });
  if (field == owner.member_list.end()) { throw std::invalid_argument{"Unbound behavior symbol"}; }
  return field->type_name_list.front().name;
}

// Gather only fields/parameters used by a portable body for validation before arithmetic.
inline void expression_symbols(const behavior_expression& node, std::set<std::string>& result) {
  if (node.kind == behavior_expression::operation::symbol) { result.insert(node.value); }
  for (const auto& child : node.operands) { expression_symbols(child, result); }
}

// Reindent Python code lines while leaving multiline literal contents unchanged.
inline std::string python_block(std::string_view text, std::size_t indentation) {
  std::size_t common = std::string_view::npos;
  std::size_t begin{};
  std::string triple;
  std::vector<std::pair<std::string_view, bool>> lines;
  while (begin < text.size()) {
    const auto end = text.find('\n', begin);
    const auto line = text.substr(begin, end == std::string_view::npos ? end : end - begin);
    const bool protected_line = !triple.empty();
    lines.push_back({line, protected_line});
    const auto leading = line.find_first_not_of(" \t\r");
    if (!protected_line && leading != std::string_view::npos) { common = std::min(common, leading); }
    for (std::size_t index = 0; index + 2 < line.size(); ++index) {
      const auto token = line.substr(index, 3);
      if (triple.empty() && (token == "\"\"\"" || token == std::string(3, char{39}))) {
        triple = token; index += 2;
      } else if (!triple.empty() && token == triple) { triple.clear(); index += 2; }
      else if (triple.empty() && line[index] == '#') { break; }
    }
    if (end == std::string_view::npos) { break; }
    begin = end + 1;
  }
  if (common == std::string_view::npos) { return {}; }
  std::string output;
  for (const auto& [line, protected_line] : lines) {
    if (protected_line) { output += line; }
    else if (line.find_first_not_of(" \t\r") != std::string_view::npos) {
      output.append(indentation, ' '); output += line.substr(std::min(common, line.size()));
    }
    output += '\n';
  }
  return output;
}

// Preserve relative authored behavior/native order; durable fields keep their existing backend layout.
inline std::vector<class_body_item> ordered_body(const class_node& owner) {
  if (!owner.body_order.empty()) { return owner.body_order; }
  std::vector<class_body_item> result;
  for (std::size_t index{}; index < owner.native_blocks.size(); ++index) {
    result.push_back({class_body_item::kind_type::native_code, index});
  }
  for (std::size_t index{}; index < owner.functions.size(); ++index) {
    result.push_back({class_body_item::kind_type::function, index});
  }
  return result;
}

// Compare schema signatures without treating a return type or dispatch modifier as an overload.
inline std::string signature(const function_declaration& method) {
  std::string result = method.name + "(";
  for (const auto& argument : method.parameters) { result += argument.type.get_full_name() + ","; }
  return result + ")";
}

// Flatten behavior contracts for adapters while retaining the nearest implementation signature.
inline std::vector<const function_declaration*> contracts(const class_node& owner) {
  std::map<std::string, const function_declaration*> methods;
  for (const auto& base : owner.parents) {
    for (const auto* method : contracts(*base.parent_class)) { methods.emplace(signature(*method), method); }
  }
  for (const auto& method : owner.functions) {
    const auto key = signature(method);
    if (method.dispatch != function_declaration::dispatch_type::ordinary || methods.contains(key)) {
      methods[key] = &method;
    }
  }
  std::vector<const function_declaration*> result;
  for (const auto& [key, method] : methods) { static_cast<void>(key); result.push_back(method); }
  return result;
}

// Native C++ overrides remain on data only when a concrete virtual base member exists.
inline bool concrete_virtual_base(const class_node& owner, const function_declaration& method) {
  for (const auto& base : owner.parents) {
    for (const auto& candidate : base.parent_class->functions) {
      if (signature(candidate) == signature(method) &&
          candidate.dispatch != function_declaration::dispatch_type::ordinary &&
          candidate.dispatch != function_declaration::dispatch_type::abstract_method) { return true; }
    }
    if (concrete_virtual_base(*base.parent_class, method)) { return true; }
  }
  return false;
}

// Erasure-free languages require one generated callable name per behavior signature.
inline void require_unique_callable_names(const class_node& owner, std::string_view language,
    const std::function<std::string(std::string_view)>& spelling,
    const std::function<std::string(const type_name&)>& parameter_type = {}) {
  std::map<std::string, std::string> names;
  for (const auto& method : owner.functions) {
    auto key = spelling(method.name);
    if (parameter_type) {
      key += "(";
      for (const auto& argument : method.parameters) { key += parameter_type(argument.type) + ","; }
      key += ")";
    }
    const auto [position, inserted] = names.emplace(key, signature(method));
    if (!inserted) {
      throw std::invalid_argument{std::string{language} + ": behavior overload/name erasure collision: " + key};
    }
    static_cast<void>(position);
  }
}

// Dispatch contracts are emitted as separate adapters; owning codec data stays concrete.
inline void require_ordinary_dispatch(const function_declaration&, std::string_view) {}

} // namespace rohit::serializer::writer::behavior
