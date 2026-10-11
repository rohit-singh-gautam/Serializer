// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "behavior_writer.hpp"
#include "construction_writer.hpp"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace rohit::serializer::writer::behavior {

// Supply backend spellings and scope operations for allocation-free native expression evaluation.
struct statement_context {
  bool cpp{};
  bool predicate{};
  checked_helpers helpers{};
  std::string prefix{"srl_behavior_value_"};
  std::string valid{"srl_behavior_ok"};
  std::string budget{"srl_behavior_budget"};
  std::function<std::string(std::string_view)> symbol;
  std::function<std::string(const behavior_expression&, std::string)> member;
  std::function<std::string(const type_name&)> native_type;
  std::function<std::string(const type_name&, std::string_view)> enum_value;
  std::function<void(const std::string&)> line;
  std::function<void(const std::string&)> open;
  std::function<void()> close;
  std::function<void()> fail;
};

// Identify bodies whose selected operations are valid during C++20 constant evaluation.
inline bool constexpr_expression(const behavior_expression& node) {
  using operation = behavior_expression::operation;
  switch (node.kind) {
  case operation::add: case operation::subtract: case operation::multiply:
  case operation::divide: case operation::square_root: case operation::sine: case operation::cosine:
  case operation::positive: case operation::negative: case operation::to_double: return false;
  default: break;
  }
  for (const auto& operand : node.operands) { if (!constexpr_expression(operand)) { return false; } }
  return true;
}

// Check explicit value parameters without constraining unrelated storage on the borrowed receiver.
inline bool constexpr_parameters(const class_node& owner, const function_declaration& method) {
  const auto eligible = [&](const type_name& type, const auto& self) -> bool {
    if (type.is_associative()) { return false; }
    if (type.is_optional() || type.is_sequence()) { return self(type.arguments.front(), self); }
    if (type.type == object_type::primitive) { return !type.name.starts_with("version"); }
    if (type.type == object_type::enum_type) { return true; }
    if (type.resolved_node && (type.resolved_node->type == object_type::class_type ||
                              type.resolved_node->type == object_type::generic_definition)) {
      return construction::constexpr_construction(static_cast<const class_node&>(*type.resolved_node));
    }
    return std::find(owner.type_parameters.begin(), owner.type_parameters.end(), type.name) != owner.type_parameters.end();
  };
  return std::all_of(method.parameters.begin(), method.parameters.end(),
      [&](const auto& parameter) { return eligible(parameter.type, eligible); });
}

// Exact scalar comparisons cannot fail on statically typed C++ values; checked indexing remains fallible.
inline bool noexcept_expression(const behavior_expression& node, bool unchecked = false) {
  using operation = behavior_expression::operation;
  if (node.kind == operation::index_access || node.kind == operation::all || node.kind == operation::any ||
      !constexpr_expression(node)) { return false; }
  if (!unchecked && (node.result_type == "float" || node.result_type == "double") &&
      node.kind != operation::literal && node.kind != operation::pi) { return false; }
  for (const auto& operand : node.operands) {
    if (!noexcept_expression(operand, unchecked || node.kind == operation::is_finite)) { return false; }
  }
  return true;
}

// Detect shared-budget use without introducing unused locals into scalar-only generated methods.
inline bool has_reduction(const behavior_expression& node) {
  if (node.kind == behavior_expression::operation::all || node.kind == behavior_expression::operation::any) { return true; }
  for (const auto& operand : node.operands) { if (has_reduction(operand)) { return true; } }
  return false;
}

// Preserve checked expression input preflight while excluding lexical binders and total finite inspections.
inline void floating_symbols(const behavior_expression& node, std::set<std::string>& symbols) {
  if (node.kind == behavior_expression::operation::is_finite) { return; }
  if (node.kind == behavior_expression::operation::symbol && !node.local_symbol &&
      (node.result_type == "float" || node.result_type == "double")) { symbols.insert(node.value); }
  for (const auto& operand : node.operands) { floating_symbols(operand, symbols); }
}

// Retain eager validation of declared scalar inputs in checked dynamic expressions, excluding total observations.
inline void scalar_symbols(const behavior_expression& node,
    std::map<std::string, const behavior_expression*>& symbols) {
  if (node.kind == behavior_expression::operation::is_finite) { return; }
  if (node.kind == behavior_expression::operation::symbol && !node.local_symbol &&
      (node.result_type == "bool" || node.result_type == "float" || node.result_type == "double" ||
       node.result_type.starts_with("int") || node.result_type.starts_with("uint") ||
       node.result_type == "char" || node.result_type.starts_with("enum:"))) {
    symbols.emplace(node.value, &node);
  }
  for (const auto& operand : node.operands) { scalar_symbols(operand, symbols); }
}

// Lower typed C/C++ expressions as statements so reached failures and lazy branches are explicit.
class statement_expression {
  statement_context context;
  std::size_t next{};
  std::map<std::string, std::string> locals{};

  // Produce a private local spelling distinct from the authored parameter namespace.
  std::string temporary() { return context.prefix + std::to_string(next++); }

  // Select an exact scalar result type rather than converting identities through floating point.
  std::string type(const behavior_expression& node) const {
    if (node.resolved_type) { return context.native_type(*node.resolved_type); }
    return node.result_type == "bool" ? "bool" : node.result_type == "float" ? "float" : "double";
  }

  // Stop evaluation before an invalid access or arithmetic result can influence an enclosing predicate.
  void reject(const std::string& condition) {
    context.open("if (" + condition + ")"); context.fail(); context.close();
  }

  // Snapshot reached scalar values, except that total finite observations accept nonfinite input.
  std::string scalar(const behavior_expression& node, const std::string& value, bool unchecked) {
    if (node.result_type == "collection" || node.result_type.starts_with("record:")) { return value; }
    const auto local = temporary(); context.line("const " + type(node) + " " + local + " = " + value + ";");
    if (!unchecked && (node.result_type == "float" || node.result_type == "double")) {
      reject(context.cpp ? "!::rohit::serializer::detail::srl_behavior_checked_predicate_finite(" + local + ")" : "!isfinite(" + local + ")");
    }
    if ((!context.cpp || context.predicate) && node.result_type.starts_with("enum:") && node.resolved_type && node.resolved_type->resolved_node) {
      const auto count = std::to_string(static_cast<const enum_node&>(*node.resolved_type->resolved_node).enum_name_list.size());
      reject(context.cpp ? "static_cast<::std::uint32_t>(" + local + ") >= " + count : local + " >= " + count);
    }
    return local;
  }

  // Check actual collection length and backing storage before traversing generated C sequences.
  std::string collection_size(const behavior_expression& node, const std::string& value) {
    const auto size = context.cpp ? "(" + value + ").size()" : "(" + value + ").size";
    if (!context.cpp) {
      reject(size + " != 0 && !(" + value + ").data");
      if (node.fixed_extent != 0) { reject(size + " != " + std::to_string(node.fixed_extent)); }
    }
    return size;
  }

public:
  // Bind callbacks; evaluation borrows schema metadata and writes only generated source text.
  explicit statement_expression(statement_context value) : context{std::move(value)} {}

  // Emit in evaluation order and return the scalar temporary or borrowed native lvalue.
  std::string emit(const behavior_expression& node, bool unchecked = false) {
    using operation = behavior_expression::operation;
    const auto child = [&](std::size_t index) { return emit(node.operands.at(index), unchecked); };
    switch (node.kind) {
    case operation::integer_literal: {
      auto literal = node.value;
      if (literal == "-9223372036854775808") { literal = "(-9223372036854775807LL - 1LL)"; }
      else if (node.result_type == "uint64") { literal += "ULL"; }
      return context.cpp ? "static_cast<" + type(node) + ">(" + literal + ")" : "((" + type(node) + ")(" + literal + "))";
    }
    case operation::literal: return (node.value.find_first_of(".eE") == std::string::npos ? node.value + ".0" : node.value) + (node.result_type == "float" ? "f" : "");
    case operation::boolean: return node.value;
    case operation::pi: return "3.141592653589793";
    case operation::enum_constant: return context.enum_value(*node.resolved_type, node.value);
    case operation::symbol: return scalar(node, node.local_symbol ? locals.at(node.value) : context.symbol(node.value), unchecked);
    case operation::member_access: return scalar(node, context.member(node, child(0)), unchecked);
    case operation::collection_size: {
      const auto value = child(0), size = collection_size(node.operands.front(), value);
      return context.cpp ? "static_cast<::std::uint64_t>(" + size + ")" : "((uint64_t)(" + size + "))";
    }
    case operation::index_access: {
      const auto container = child(0), index = child(1), size = collection_size(node.operands.front(), container);
      if (node.operands.at(1).result_type.starts_with("int")) { reject(index + " < 0"); }
      const auto cast = context.cpp ? "static_cast<::std::uint64_t>(" + index + ")" : "((uint64_t)(" + index + "))";
      reject(cast + " >= " + size);
      return scalar(node, "(" + container + ")" + (context.cpp ? "[" : ".data[") + index + "]", unchecked);
    }
    case operation::is_finite: {
      const auto value = emit(node.operands.front(), true);
      return context.cpp ? "::rohit::serializer::detail::srl_behavior_checked_predicate_finite(" + value + ")" : "isfinite(" + value + ")";
    }
    case operation::all: case operation::any: {
      const auto container = child(0), size = collection_size(node.operands.front(), container);
      reject(size + " > " + std::to_string(node.reduction_bound));
      const auto result = temporary(), index = temporary();
      context.line("bool " + result + " = " + (node.kind == operation::all ? "true" : "false") + ";");
      context.open("for (" + std::string{context.cpp ? "::std::size_t" : "size_t"} + " " + index + " = 0; " + index + " < " + size + "; ++" + index + ")");
      reject(context.budget + " == 0"); context.line("--" + context.budget + ";");
      locals.emplace(node.value, "(" + container + ")" + (context.cpp ? "[" : ".data[") + index + "]");
      const auto item = emit(node.operands.at(1)); locals.erase(node.value);
      context.open("if (" + std::string{node.kind == operation::all ? "!" : ""} + "(" + item + "))");
      context.line(result + " = " + (node.kind == operation::all ? "false" : "true") + ";"); context.line("break;"); context.close(); context.close(); return result;
    }
    case operation::logical_and: case operation::logical_or: {
      const auto left = child(0), result = temporary(); context.line("bool " + result + " = " + left + ";");
      context.open("if (" + std::string{node.kind == operation::logical_or ? "!" : ""} + result + ")");
      const auto right = child(1); context.line(result + " = " + right + ";"); context.close(); return result;
    }
    case operation::conditional: {
      const auto condition = child(0), result = temporary(); context.line(type(node) + " " + result + " = " + (context.cpp ? "{}" : "0") + ";");
      context.open("if (" + condition + ")"); const auto yes = child(1); context.line(result + " = " + yes + ";"); context.close();
      context.open("else"); const auto no = child(2); context.line(result + " = " + no + ";"); context.close(); return result;
    }
    case operation::logical_not: return "(!" + child(0) + ")";
    case operation::to_double: return context.cpp ? "static_cast<double>(" + child(0) + ")" : "((double)(" + child(0) + "))";
    case operation::less: case operation::less_equal: case operation::greater: case operation::greater_equal: case operation::equal: case operation::not_equal: {
      const auto left = child(0), right = child(1);
      const auto token = node.kind == operation::less ? " < " : node.kind == operation::less_equal ? " <= " : node.kind == operation::greater ? " > " : node.kind == operation::greater_equal ? " >= " : node.kind == operation::equal ? " == " : " != ";
      return "(" + left + token + right + ")";
    }
    default: break;
    }
    const auto helper_prefix = std::string{context.cpp ? "::rohit::serializer::detail::" : ""};
    const auto suffix = context.cpp && context.predicate ? "_predicate" : "";
    const auto flag = context.cpp && !context.predicate ? ")" : ", &" + context.valid + ")";
    std::string arithmetic;
    if (node.kind == operation::divide) {
      const auto left = child(0), right = child(1);
      arithmetic = helper_prefix + context.helpers.divide + suffix + "(" + left + ", " + right + flag;
    } else if (node.kind == operation::square_root) {
      arithmetic = helper_prefix + context.helpers.square_root + suffix + "(" + child(0) + flag;
    } else {
      std::string value;
      if (node.kind == operation::positive || node.kind == operation::negative) {
        value = "(" + std::string{node.kind == operation::positive ? "+" : "-"} + child(0) + ")";
      } else if (node.kind == operation::sine || node.kind == operation::cosine) {
        value = std::string{context.cpp ? "::std::" : ""} + (node.kind == operation::sine ? "sin" : "cos") + "(" + child(0) + ")";
      } else {
        const auto left = child(0), right = child(1);
        value = left + (node.kind == operation::add ? " + " : node.kind == operation::subtract ? " - " : " * ") + right;
      }
      arithmetic = helper_prefix + context.helpers.checked + suffix + "(" + value + flag;
    }
    const auto result = temporary(); context.line("const double " + result + " = " + arithmetic + ";"); reject("!" + context.valid); return result;
  }
};

} // namespace rohit::serializer::writer::behavior
