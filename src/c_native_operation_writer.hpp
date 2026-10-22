// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::native::operation_codegen {
// Preserve one complete compiler-owned C function without interpreting its schema or wire semantics.
struct c_function {
  std::string name{};
  std::string signature{};
  std::string body{};
};

// Skip C literals and comments while matching only generated punctuation.
inline std::size_t c_token_end(std::string_view text, std::size_t offset) {
  const auto quote = text[offset];
  if (quote == '"' || quote == '\'') {
    for (auto index = offset + 1U; index < text.size(); ++index) {
      if (text[index] == '\\') { ++index; }
      else if (text[index] == quote) { return index + 1U; }
    }
    throw std::logic_error{"Unterminated compiler-owned C literal"};
  }
  if (quote == '/' && offset + 1U < text.size()) {
    if (text[offset + 1U] == '/') {
      const auto end = text.find('\n', offset + 2U);
      return end == std::string_view::npos ? text.size() : end + 1U;
    }
    if (text[offset + 1U] == '*') {
      const auto end = text.find("*/", offset + 2U);
      if (end == std::string_view::npos) { throw std::logic_error{"Unterminated compiler-owned C comment"}; }
      return end + 2U;
    }
  }
  return offset + 1U;
}

// Match generated parameter/body delimiters without touching quoted native schema identities.
inline std::size_t c_close(std::string_view text, std::size_t start) {
  const auto opening = text[start];
  const auto closing = opening == '(' ? ')' : opening == '[' ? ']' : '}';
  std::size_t depth{1U};
  for (auto index = start + 1U; index < text.size();) {
    if (text[index] == '"' || text[index] == '\'' ||
        (text[index] == '/' && index + 1U < text.size() &&
         (text[index + 1U] == '/' || text[index + 1U] == '*'))) {
      index = c_token_end(text, index); continue;
    }
    if (text[index] == opening) { ++depth; }
    else if (text[index] == closing && --depth == 0U) { return index; }
    ++index;
  }
  throw std::logic_error{"Unbalanced compiler-owned C function"};
}

// Extract definitions, excluding forward declarations and every public struct layout.
inline std::vector<c_function> functions(std::string_view source) {
  std::vector<c_function> result{};
  constexpr std::string_view declaration{"static inline "};
  for (std::size_t offset{}; offset < source.size();) {
    if (source[offset] == '"' || source[offset] == '\'' ||
        (source[offset] == '/' && offset + 1U < source.size() &&
         (source[offset + 1U] == '/' || source[offset + 1U] == '*'))) {
      offset = c_token_end(source, offset); continue;
    }
    if (!source.substr(offset).starts_with(declaration)) { ++offset; continue; }
    const auto opening = source.find('(', offset + declaration.size());
    if (opening == std::string_view::npos) { break; }
    auto name_end = opening;
    while (name_end > offset && std::isspace(static_cast<unsigned char>(source[name_end - 1U]))) { --name_end; }
    auto name_start = name_end;
    while (name_start > offset && (std::isalnum(static_cast<unsigned char>(source[name_start - 1U])) || source[name_start - 1U] == '_')) { --name_start; }
    const auto parameter_end = c_close(source, opening);
    auto body_start = parameter_end + 1U;
    while (body_start < source.size() && std::isspace(static_cast<unsigned char>(source[body_start]))) { ++body_start; }
    if (body_start >= source.size()) { break; }
    if (source[body_start] != '{') { offset = body_start + 1U; continue; }
    const auto body_end = c_close(source, body_start);
    result.push_back({std::string{source.substr(name_start, name_end - name_start)},
        std::string{source.substr(offset, parameter_end + 1U - offset)},
        std::string{source.substr(body_start + 1U, body_end - body_start - 1U)}});
    offset = body_end + 1U;
  }
  return result;
}

// Forward a separate operation through known direct calls; literals, comments and APIs stay exact.
inline std::string calls(std::string_view body, const std::map<std::string, std::string>& names) {
  std::string output{};
  for (std::size_t offset{}; offset < body.size();) {
    const auto ch = body[offset];
    if (ch == '"' || ch == '\'' || (ch == '/' && offset + 1U < body.size() &&
        (body[offset + 1U] == '/' || body[offset + 1U] == '*'))) {
      const auto end = c_token_end(body, offset); output.append(body.substr(offset, end - offset)); offset = end; continue;
    }
    if (!std::isalpha(static_cast<unsigned char>(ch)) && ch != '_') { output += ch; ++offset; continue; }
    auto end = offset + 1U;
    while (end < body.size() && (std::isalnum(static_cast<unsigned char>(body[end])) || body[end] == '_')) { ++end; }
    const std::string token{body.substr(offset, end - offset)};
    auto opening = end;
    while (opening < body.size() && std::isspace(static_cast<unsigned char>(body[opening]))) { ++opening; }
    const auto found = names.find(token);
    if (found == names.end() || opening >= body.size() || body[opening] != '(') {
      output += token; offset = end; continue;
    }
    const auto closing = c_close(body, opening);
    const auto arguments = calls(body.substr(opening + 1U, closing - opening - 1U), names);
    const auto admission = found->second == "srl_xml_operation_sort" || found->second.starts_with("srl_xml_operation_init_");
    output += found->second + '(' + arguments + (arguments.empty() ? "" : ",") +
        (admission ? "operation?operation->admission:NULL)" : "operation)");
    offset = closing + 1U;
  }
  return output;
}

// Derive an additive signature from the actual native codec, retaining every original entry point.
inline std::string signature(const c_function& function, const std::string& name) {
  auto result = function.signature;
  const auto opening = result.find('(');
  const auto name_start = result.rfind(function.name, opening);
  result.replace(name_start, function.name.size(), name);
  const auto parameters = result.find('(');
  if (result.substr(parameters + 1U, result.size() - parameters - 2U) == "void") {
    result.replace(parameters + 1U, 4U, "srl_native_operation* operation");
  } else { result.insert(result.size() - 1U, ",srl_native_operation* operation"); }
  return result;
}

// Charge bounded compiler-owned loop predicates without changing their native control semantics.
inline std::string loops(std::string_view body) {
  std::string output{};
  for (std::size_t offset{}; offset < body.size();) {
    if (body[offset] == '"' || body[offset] == '\'' || (body[offset] == '/' && offset + 1U < body.size() &&
        (body[offset + 1U] == '/' || body[offset + 1U] == '*'))) {
      const auto end = c_token_end(body, offset); output.append(body.substr(offset, end - offset)); offset = end; continue;
    }
    const auto token_start = offset;
    if (!std::isalpha(static_cast<unsigned char>(body[offset])) && body[offset] != '_') { output += body[offset++]; continue; }
    while (offset < body.size() && (std::isalnum(static_cast<unsigned char>(body[offset])) || body[offset] == '_')) { ++offset; }
    const auto token = body.substr(token_start, offset - token_start);
    auto opening = offset;
    while (opening < body.size() && std::isspace(static_cast<unsigned char>(body[opening]))) { ++opening; }
    if ((token != "for" && token != "while") || opening == body.size() || body[opening] != '(') {
      output.append(token); continue;
    }
    const auto closing = c_close(body, opening);
    const auto condition = body.substr(opening + 1U, closing - opening - 1U);
    std::vector<std::size_t> separators{};
    if (token == "for") {
      for (std::size_t index{}; index < condition.size();) {
        if (condition[index] == '"' || condition[index] == '\'') { index = c_token_end(condition, index); }
        else if (condition[index] == '(' || condition[index] == '[' || condition[index] == '{') { index = c_close(condition, index) + 1U; }
        else { if (condition[index] == ';') { separators.push_back(index); } ++index; }
      }
      if (separators.size() != 2U) { throw std::logic_error{"Invalid compiler-owned C for-loop"}; }
    }
    const auto start = token == "for" ? separators[0] + 1U : 0U;
    const auto end = token == "for" ? separators[1] : condition.size();
    const auto predicate = condition.substr(start, end - start);
    output.append(body.substr(token_start, opening + 1U - token_start));
    output.append(condition.substr(0U, start));
    output += "(srl_native_operation_charge(operation,srl_xml_operation_work,1u)&&(";
    output += predicate.find_first_not_of(" \r\n\t") == std::string_view::npos ? "true" : std::string{predicate};
    output += "))";
    output.append(condition.substr(end)); output += ')'; offset = closing + 1U;
  }
  return output;
}

// Share foreign/QName traversal bodies with actual operation allocations and bounded loop polling.
inline std::string extensions(std::string_view source) {
  const auto declarations = functions(source);
  std::map<std::string, std::string> names{{"malloc", "srl_native_operation_malloc"},
      {"realloc", "srl_native_operation_realloc"}, {"calloc", "srl_native_operation_calloc"},
      {"srl_string_set", "srl_string_set_with_operation"}, {"srl_string_set_cstr", "srl_string_set_cstr_with_operation"},
      {"srl_reserve", "srl_reserve_with_operation"}, {"srl_put", "srl_put_with_operation"},
      {"srl_write_varint", "srl_write_varint_with_operation"},
      {"srl_xml_input_init", "srl_extension_operation_input_init"},
      {"srl_xml_input_free", "srl_extension_operation_input_free"}};
  for (const auto& function : declarations) {
    if (!function.name.ends_with("_free")) { names.emplace(function.name, "srl_extension_operation_" + function.name); }
  }
  std::string output{
#include "c_xml_extension_operation_runtime.inc"
  };
  for (const auto& function : declarations) {
    if (const auto found = names.find(function.name); found != names.end()) { output += signature(function, found->second) + ";\n"; }
  }
  for (const auto& function : declarations) {
    if (const auto found = names.find(function.name); found != names.end()) {
      const auto type_start = std::string_view{"static inline "}.size();
      const auto return_type = function.signature.substr(type_start, function.signature.find(function.name) - type_start);
      const auto guard = function.signature.starts_with("static inline void ") ? std::string{"return;"} :
          function.signature.starts_with("static inline srl_status ") ? std::string{"return srl_invalid;"} :
          "return (" + return_type + "){0};";
      auto body = function.body;
      if (function.name == "srl_ext_charge") {
        const std::string marker{"state->preserved_bytes+=size;"};
        const auto position = body.find(marker);
        if (position == std::string::npos) { throw std::logic_error{"Missing XML extension preservation admission"}; }
        body.insert(position, "if(!srl_xml_operation_preserve(operation?operation->admission:NULL,size)){return srl_invalid;}");
      }
      if (function.name == "srl_ext_head_read") {
        const std::string marker{"if (input->kind!=XML_READER_TYPE_ELEMENT) { return srl_invalid; }"};
        const auto position = body.find(marker);
        if (position == std::string::npos) { throw std::logic_error{"Missing XML extension head boundary"}; }
        body.insert(position + marker.size(), "\nconst bool package_root=!parent&&operation&&operation->admission&&operation->admission->package_contract;"
            "if(package_root&&!srl_xml_package_capabilities(input,operation->admission)){return srl_invalid;}\n");
        const std::string attribute{"} else { srl_ext_attribute attribute="};
        const auto selected = body.find(attribute);
        if (selected == std::string::npos) { throw std::logic_error{"Missing XML extension attribute boundary"}; }
        body.replace(selected, attribute.size(), "} else if(package_root&&xmlStrEqual(xmlTextReaderConstNamespaceUri(input->reader),BAD_CAST \"http://schemas.openxmlformats.org/markup-compatibility/2006\")&&xmlStrEqual(xmlTextReaderConstLocalName(input->reader),BAD_CAST \"MustUnderstand\")){/* Only the authorized root transport attribute is consumed. */} else { srl_ext_attribute attribute=");
      }
      output += "/* Retain XML companion behavior while admitting real traversal and allocation. */\n" +
          signature(function, found->second) + "{if(!srl_native_operation_charge(operation,srl_xml_operation_work,1u)){" + guard + "}\n" +
          calls(loops(body), names) + "}\n";
    }
  }
  return output;
}

// Add bounded traversal charges at the production runtime's real read/write boundaries.
inline std::string runtime_body(const c_function& function) {
  auto body = function.body;
  const auto charge = [](std::string_view amount, std::string_view status, std::string_view failure) {
    return "if(!srl_native_operation_charge(operation,srl_xml_operation_work," + std::string{amount} + ")){" +
        std::string{status} + "=srl_invalid;return" + std::string{failure} + ";}\n";
  };
  if (function.name == "srl_put") { body.insert(0U, "if(out->status!=srl_ok){return;}if(operation&&(out->buffer.size>operation->max_output_bytes||size>operation->max_output_bytes-out->buffer.size)){out->status=srl_xml_operation_fail(operation->admission,srl_limit);return;}" + charge("size", "out->status", "")); }
  else if (function.name == "srl_require") { body.insert(0U, "if(in->status!=srl_ok){return false;}" + charge("count", "in->status", " false")); }
  else if (function.name == "srl_token") {
    const auto marker = body.find("char* value =");
    if (marker == std::string::npos) { throw std::logic_error{"Missing native token allocation boundary"}; }
    body.insert(marker, charge("size", "in->status", " NULL"));
  } else if (function.name == "srl_write_begin" || function.name == "srl_write_element" || function.name == "srl_write_array") {
    body.insert(0U, "if(out->status!=srl_ok){return;}" + charge("1u", "out->status", ""));
  } else if (function.name == "srl_read_begin") { body.insert(0U, "if(in->status!=srl_ok){return;}" + charge("1u", "in->status", "")); }
  else if (function.name == "srl_read_element") { body.insert(0U, "if(in->status!=srl_ok){return false;}" + charge("1u", "in->status", " false")); }
  return body;
}

// Emit operation-aware twins of compiler-owned native runtime definitions, with checked actual allocation.
inline std::string runtime(std::string_view source) {
  const auto declarations = functions(source);
  std::map<std::string, std::string> names{{"malloc", "srl_native_operation_malloc"},
      {"realloc", "srl_native_operation_realloc"}, {"calloc", "srl_native_operation_calloc"}};
  for (const auto& function : declarations) {
    if (function.signature.find("srl_reader*") != std::string::npos ||
        function.signature.find("srl_writer*") != std::string::npos ||
        function.name == "srl_reserve" || function.name == "srl_string_set" ||
        function.name == "srl_string_set_cstr" || function.name == "srl_digest_init") {
      names.emplace(function.name, function.name + "_with_operation");
    }
  }
  std::string output{
#include "c_native_operation_runtime.inc"
  };
  for (const auto& function : declarations) {
    if (const auto found = names.find(function.name); found != names.end()) {
      output += signature(function, found->second) + ";\n";
    }
  }
  for (const auto& function : declarations) {
    if (const auto found = names.find(function.name); found != names.end()) {
      output += "/* Share native wire logic while admitting this operation's actual work and ownership. */\n" +
          signature(function, found->second) + "{(void)operation;\n" + calls(runtime_body(function), names) + "}\n";
    }
  }
  return "\n#ifndef SRL_NATIVE_OPERATION_INCLUDED\n#define SRL_NATIVE_OPERATION_INCLUDED\n" + output + "\n#endif\n";
}

// Share actual schema codec bodies without payload staging or mutable global operation state.
inline std::string models(std::string_view source, std::string_view runtime_source,
    const std::map<std::string, std::string>& initializers) {
  const auto declarations = functions(source);
  std::map<std::string, std::string> names = initializers;
  names.emplace("malloc", "srl_native_operation_malloc");
  names.emplace("calloc", "srl_native_operation_calloc");
  names.emplace("realloc", "srl_native_operation_realloc");
  names.emplace("qsort", "srl_xml_operation_sort");
  for (const auto& function : functions(runtime_source)) {
    if (function.signature.find("srl_reader*") != std::string::npos || function.signature.find("srl_writer*") != std::string::npos ||
        function.name == "srl_reserve" || function.name == "srl_string_set" || function.name == "srl_string_set_cstr" || function.name == "srl_digest_init") {
      names.emplace(function.name, function.name + "_with_operation");
    }
  }
  for (const auto& function : declarations) {
    if ((function.name.find("_srl_") != std::string::npos &&
        (function.signature.find("srl_reader*") != std::string::npos || function.signature.find("srl_writer*") != std::string::npos)) || function.name.ends_with("_normalize")) {
      names.emplace(function.name, "srl_native_operation_" + function.name);
    }
  }
  std::string output{};
  for (const auto& function : declarations) {
    if (const auto found = names.find(function.name); found != names.end() && !initializers.contains(function.name)) {
      output += signature(function, found->second) + ";\n";
    }
  }
  for (const auto& function : declarations) {
    if (const auto found = names.find(function.name); found != names.end() && !initializers.contains(function.name)) {
      output += "/* Traverse the original native model with one separately borrowed admission context. */\n" +
          signature(function, found->second) + "{(void)operation;\n" + calls(function.body, names) + "}\n";
    }
  }
  return output;
}
} // namespace rohit::serializer::writer::native::operation_codegen
