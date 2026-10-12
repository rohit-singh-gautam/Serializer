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
#include <vector>

namespace rohit::serializer::writer::behavior {

// Supply backend spellings and scope operations for allocation-free native expression evaluation.
struct statement_context {
  bool cpp{};
  bool predicate{};
  checked_helpers helpers{};
  std::string prefix{"srl_behavior_value_"};
  std::string valid{"srl_behavior_ok"};
  std::string budget{"srl_behavior_budget"};
  std::function<std::string(std::string_view)> symbol{};
  std::function<std::string(const behavior_expression&, std::string)> member{};
  std::function<std::string(const type_name&)> native_type{};
  std::function<std::string(const type_name&, std::string_view)> enum_value{};
  std::function<std::string(const behavior_expression&, const std::vector<std::string>&)> call{};
  std::function<void(const std::string&)> line{};
  std::function<void(const std::string&)> open{};
  std::function<void()> close{};
  std::function<void()> fail{};
};

// Prove native operations use concrete scalar values rather than opaque conversions or comparators.
inline bool native_scalar_expression(const behavior_expression& node) {
  if (node.kind == behavior_expression::operation::call) { return false; }
  const auto& type = node.result_type;
  const bool scalar = type == "bool" || type == "char" || type == "float" || type == "double" ||
      type == "int8" || type == "uint8" || type == "int16" || type == "uint16" ||
      type == "int32" || type == "uint32" || type == "int64" || type == "uint64" ||
      type.starts_with("enum:");
  bool borrowed = false;
  if ((type == "collection" || type.starts_with("record:")) && node.resolved_type &&
      (!node.resolved_type->resolved_node ||
       node.resolved_type->resolved_node->type != object_type::generic_definition)) {
    // Concrete passive collection/record projections borrow storage; they do not copy a result.
    static const class_node known_types{object_type::class_type, std::string{}, nullptr,
        class_attributes::none, {}};
    borrowed = constexpr_type(known_types, *node.resolved_type);
  }
  return (scalar || borrowed) && std::all_of(node.operands.begin(), node.operands.end(), [](const auto& operand) {
    return native_scalar_expression(operand);
  });
}

// Exact scalar conversions/comparisons cannot fail on C++ values; checked indexing remains fallible.
inline bool noexcept_expression(const behavior_expression& node, bool unchecked = false) {
  using operation = behavior_expression::operation;
  // Owning record selection can allocate while copying the selected branch.
  if (node.kind == operation::conditional && node.result_type.starts_with("record:")) { return false; }
  if (!native_scalar_expression(node)) { return false; }
  // Constant-capable arithmetic still reports runtime domain failures through exceptions.
  if (node.kind == operation::add || node.kind == operation::subtract ||
      node.kind == operation::multiply || node.kind == operation::divide ||
      node.kind == operation::positive || node.kind == operation::negative ||
      node.kind == operation::checked_cast) { return false; }
  if (node.kind == operation::to_double) {
    return std::all_of(node.operands.begin(), node.operands.end(), [&](const auto& operand) {
      return noexcept_expression(operand, unchecked);
    });
  }
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

  // Emit C numeric checks before a cast, overflowing operation or invalid integral remainder.
  std::string numeric_c(const behavior_expression& node, const std::string& left,
                        const std::string& right = {}) {
    using operation = behavior_expression::operation;
    const bool integral = numeric_integer_type(node.result_type);
    const auto wire_literal = [](const std::string& value) {
      return value == "-9223372036854775808" ? std::string{"(-9223372036854775807LL - 1LL)"} :
          value + (value == "18446744073709551615" ? "ULL" : "LL");
    };
    const auto [low, high] = integral ? integer_range(node.result_type) : std::pair<std::string, std::string>{"0", "0"};
    const auto minimum = wire_literal(low), maximum = wire_literal(high);
    const bool signed_value = node.result_type.starts_with("int");
    const auto cast = [&](const std::string& native, const std::string& value) {
      return "((" + native + ")(" + value + "))";
    };
    if (node.kind == operation::checked_cast) {
      const auto& source = node.operands.front();
      const bool source_integral = numeric_integer_type(source.result_type);
      if (integral && source_integral) {
        auto condition = cast("uint64_t", left) + " > " + maximum;
        if (source.result_type.starts_with("int")) {
          condition = "(" + left + " < 0 && " + (signed_value ? cast("int64_t", left) + " < " + minimum : "true") +
              ") || (" + left + " >= 0 && " + condition + ")";
        }
        reject(condition);
        return cast(type(node), left);
      }
      if (integral) {
        const auto [lower, upper] = numeric_float_endpoints(node.result_type);
        reject("!isfinite(" + left + ") || " + left + " < " + lower + " || " + left + " >= " + upper);
      } else if (node.result_type == "float" && source.result_type == "double") {
        // C requires a representable floating result before performing a narrowing conversion.
        constexpr std::string_view single_maximum{"3.4028234663852885981170418348451692544e38"};
        reject("!isfinite(" + left + ") || " + left + " < -" + std::string{single_maximum} +
            " || " + left + " > " + std::string{single_maximum});
      }
      const auto result = temporary();
      context.line("const " + type(node) + " " + result + " = " + cast(type(node), left) + ";");
      if (source_integral) {
        const auto [lower, upper] = numeric_float_endpoints(source.result_type);
        reject("!isfinite(" + result + ") || " + result + " < " + lower + " || " + result + " >= " + upper);
      } else if (!integral) { reject("!isfinite(" + result + ")"); }
      reject(cast(type(source), result) + " != " + left);
      return result;
    }
    if (integral) {
      if (node.kind == operation::add) {
        reject(signed_value ? "(" + right + " > 0 && " + left + " > " + maximum + " - " + right +
            ") || (" + right + " < 0 && " + left + " < " + minimum + " - " + right + ")" :
            left + " > " + maximum + " - " + right);
      } else if (node.kind == operation::subtract) {
        reject(signed_value ? "(" + right + " > 0 && " + left + " < " + minimum + " + " + right +
            ") || (" + right + " < 0 && " + left + " > " + maximum + " + " + right + ")" : left + " < " + right);
      } else if (node.kind == operation::multiply) {
        reject(signed_value ? "(" + left + " > 0 && ((" + right + " > 0 && " + left + " > " + maximum + " / " + right +
            ") || (" + right + " < 0 && " + right + " < " + minimum + " / " + left + "))) || (" + left +
            " < 0 && ((" + right + " > 0 && " + left + " < " + minimum + " / " + right + ") || (" + right +
            " < 0 && " + left + " < " + maximum + " / " + right + ")))" : right + " != 0 && " + left + " > " + maximum + " / " + right);
      } else if (node.kind == operation::negative) {
        reject(signed_value ? left + " == " + minimum : left + " != 0");
      }
    }
    if (node.kind == operation::divide) {
      reject(right + " == 0" + (integral && signed_value ? " || (" + left + " == " + minimum + " && " + right + " == -1)" : ""));
      if (integral) { reject(left + " % " + right + " != 0"); }
    }
    const auto value = node.kind == operation::positive ? left : node.kind == operation::negative ? "(-" + left + ")" :
        "(" + left + (node.kind == operation::add ? " + " : node.kind == operation::subtract ? " - " : node.kind == operation::multiply ? " * " : " / ") + right + ")";
    return scalar(node, cast(type(node), value), false);
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
    case operation::call: {
      if (!context.call) { throw std::invalid_argument{"Missing native function call lowering"}; }
      std::vector<std::string> arguments;
      for (std::size_t index{}; index < node.operands.size(); ++index) {
        const auto& operand = node.operands[index];
        const auto value = emit(operand, unchecked), argument = temporary();
        context.line(std::string{context.cpp && node.call_constructor ? "" : "const "} + type(operand) +
            (context.cpp && node.call_receiver && index == 0 ? "& " : " ") + argument + " = " + value + ";");
        arguments.push_back(argument);
      }
      return scalar(node, context.call(node, arguments), unchecked);
    }
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
      const auto condition = child(0), result = temporary();
      if (context.cpp && node.result_type.starts_with("record:")) {
        // Avoid constructing an unselected record default, which can itself run a factory.
        context.line("::std::optional<" + type(node) + "> " + result + ";");
        context.open("if (" + condition + ")");
        const auto yes = child(1); context.line(result + ".emplace(" + yes + ");"); context.close();
        context.open("else");
        const auto no = child(2); context.line(result + ".emplace(" + no + ");"); context.close();
        return "::std::move(*" + result + ")";
      }
      context.line(type(node) + " " + result + " = " +
          (context.cpp ? "{}" : node.result_type.starts_with("record:") ? "{0}" : "0") + ";");
      context.open("if (" + condition + ")"); const auto yes = child(1); context.line(result + " = " + yes + ";"); context.close();
      context.open("else"); const auto no = child(2); context.line(result + " = " + no + ";"); context.close(); return result;
    }
    case operation::logical_not: return "(!" + child(0) + ")";
    case operation::checked_cast: {
      const auto source = child(0);
      if (!context.cpp) { return numeric_c(node, source); }
      const auto result = temporary();
      context.line("const auto " + result + " = ::rohit::serializer::try_numeric_cast<" + type(node) + ">(" + source + ");");
      reject("!" + result);
      return "(*" + result + ")";
    }
    case operation::to_double: return context.cpp ? "static_cast<double>(" + child(0) + ")" : "((double)(" + child(0) + "))";
    case operation::less: case operation::less_equal: case operation::greater: case operation::greater_equal: case operation::equal: case operation::not_equal: {
      const auto left = child(0), right = child(1);
      const auto token = node.kind == operation::less ? " < " : node.kind == operation::less_equal ? " <= " : node.kind == operation::greater ? " > " : node.kind == operation::greater_equal ? " >= " : node.kind == operation::equal ? " == " : " != ";
      return "(" + left + token + right + ")";
    }
    default: break;
    }
    if (node.numeric_contract_enabled && node.result_type != "double" &&
        (node.kind == operation::positive || node.kind == operation::negative ||
         node.kind == operation::add || node.kind == operation::subtract ||
         node.kind == operation::multiply || node.kind == operation::divide)) {
      const auto left = child(0);
      if (!context.cpp) { return numeric_c(node, left, node.operands.size() == 2 ? child(1) : ""); }
      const auto result = temporary();
      const auto function = node.kind == operation::positive ? "try_numeric_cast" :
          node.kind == operation::negative ? "try_checked_negate" :
          node.kind == operation::add ? "try_checked_add" :
          node.kind == operation::subtract ? "try_checked_subtract" :
          node.kind == operation::multiply ? "try_checked_multiply" : "try_checked_divide";
      auto arguments = left;
      if (node.operands.size() == 2) { arguments += ", " + child(1); }
      context.line("const auto " + result + " = ::rohit::serializer::" + function + "<" + type(node) + ">(" + arguments + ");");
      reject("!" + result);
      return "(*" + result + ")";
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
