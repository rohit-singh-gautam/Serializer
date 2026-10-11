// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "behavior_writer.hpp"

#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::behavior {

// Return the exact declared integer range used by dynamically typed backends.
inline std::pair<std::string, std::string> integer_range(std::string_view type) {
  const auto width = type == "char" || type.ends_with("8") ? 8u : type.ends_with("16") ? 16u : type.ends_with("32") ? 32u : 64u;
  if (type.starts_with("int")) {
    if (width == 64) { return {"-9223372036854775808", "9223372036854775807"}; }
    const auto magnitude = std::uint64_t{1} << (width - 1);
    return {"-" + std::to_string(magnitude), std::to_string(magnitude - 1)};
  }
  return {"0", width == 64 ? "18446744073709551615" : std::to_string((std::uint64_t{1} << width) - 1)};
}

// Preserve integral literal bits instead of passing through a floating representation.
inline std::string integral_literal(const behavior_expression& node, std::string_view language,
                                   const expression_context& context) {
  const auto& type = node.result_type;
  const auto& value = node.value;
  if (language == "python") { return value; }
  if (language == "js" || language == "typescript") { return value + (type.ends_with("64") ? "n" : ""); }
  if (language == "java") {
    if (type == "uint64") {
      std::uint64_t bits{}; std::from_chars(value.data(), value.data() + value.size(), bits);
      char buffer[17]{}; const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), bits, 16);
      return "0x" + std::string{buffer, converted.ptr} + "L";
    }
    if (type == "int64") { return value + "L"; }
    if (type == "uint32") { return "((int)" + value + "L)"; }
    if (type == "uint16" || type == "int16") { return "((short)" + value + ")"; }
    if (type == "uint8" || type == "int8" || type == "char") { return "((byte)" + value + ")"; }
    return value;
  }
  const auto native = node.resolved_type && context.native_type ? context.native_type(*node.resolved_type) : std::string{};
  if (language == "rust") { return value + "_" + (type == "char" ? "u8" : type.starts_with("uint") ? "u" + type.substr(4) : "i" + type.substr(3)); }
  if (language == "kotlin") {
    if (type == "uint64") { return value + "uL"; }
    if (type == "int64") { return value == "-9223372036854775808" ? "Long.MIN_VALUE" : value + "L"; }
    if (type == "uint32") { return value + "u"; }
    if (type == "uint8" || type == "char") { return "(" + value + ").toUByte()"; }
    if (type == "uint16") { return "(" + value + ").toUShort()"; }
    if (type == "int8") { return "(" + value + ").toByte()"; }
    if (type == "int16") { return "(" + value + ").toShort()"; }
    return value;
  }
  if (language == "swift" || language == "go") { return native.empty() ? value : native + "(" + value + ")"; }
  if (language == "csharp") {
    if (type == "uint64") { return value + "UL"; }
    if (type == "int64") { return value == "-9223372036854775808" ? "long.MinValue" : value + "L"; }
    if (type == "uint32") { return value + "U"; }
    return native.empty() ? value : "((" + native + ")" + value + ")";
  }
  return value + (type == "uint64" ? "ULL" : type == "int64" ? "LL" : "");
}

// Read lengths without changing the collection's owning representation.
inline std::string collection_length(const behavior_expression& node, std::string value,
                                     std::string_view language) {
  if (language == "python") { return "len(" + value + ")"; }
  if (language == "rust") { return "(" + value + ").len()"; }
  if (language == "go") { return "len(" + value + ")"; }
  if (language == "java") { return "(" + value + ")" + (node.collection_modifier == member::modifier_type::array ? ".length" : ".size()"); }
  if (language == "csharp") { return "(" + value + ").Count"; }
  if (language == "swift") { return "(" + value + ").count"; }
  if (language == "kotlin") { return "(" + value + ").size"; }
  return "(" + value + ").length";
}

// Lower projections, bounded traversal and matching native scalar comparisons lazily.
inline std::optional<std::string> typed_expression(const behavior_expression& node, std::string_view language,
    const std::function<std::string(std::string_view)>& symbol, const checked_helpers& names,
    const expression_context& context) {
  using operation = behavior_expression::operation;
  const auto child = [&](std::size_t index) { return expression(node.operands.at(index), language, symbol, names, context); };
  const auto native = [&](const behavior_expression& value) {
    return value.resolved_type && context.native_type ? context.native_type(*value.resolved_type) :
        value.result_type == "bool" ? std::string{language == "java" ? "boolean" : "bool"} : std::string{"double"};
  };
  const auto failure = [&]() -> std::string {
    if (language == "java") { return "throw new ArithmeticException(\"Invalid portable projection or reduction\");"; }
    if (language == "csharp") { return "throw new System.ArithmeticException(\"Invalid portable projection or reduction\");"; }
    if (language == "go") { return "panic(\"Invalid portable projection or reduction\")"; }
    if (language == "swift") { return "throw SerializerError.invalid(\"Invalid portable projection or reduction\")"; }
    if (language == "kotlin") { return "throw SerializerException(\"Invalid portable projection or reduction\")"; }
    if (language == "rust") { return context.predicate ? std::string{"return false;"} : std::string{"panic!(\"Invalid portable projection or reduction\");"}; }
    return "throw new RangeError('Invalid portable projection or reduction');";
  };
  const auto closure = [&](std::string body, std::string result, bool boolean = false) {
    // Nested Java/C# closures cannot redeclare an enclosing local's name.
    for (const auto token : {"srl_items", "srl_index", "srl_result"}) {
      std::size_t offset{};
      const auto renamed = std::string{token} + "_" + std::to_string(node.source_offset);
      while ((offset = body.find(token, offset)) != std::string::npos) {
        const auto end = offset + std::string_view{token}.size();
        if (end < body.size() && (std::isalnum(static_cast<unsigned char>(body[end])) || body[end] == '_')) { offset = end; continue; }
        body.replace(offset, std::string_view{token}.size(), renamed); offset += renamed.size();
      }
    }
    if (language == "java") {
      if (result == "boolean") { result = "Boolean"; }
      else if (result == "double") { result = "Double"; }
      else if (result == "float") { result = "Float"; }
      else if (result == "long") { result = "Long"; }
      else if (result == "int") { result = "Integer"; }
      else if (result == "short") { result = "Short"; }
      else if (result == "byte") { result = "Byte"; }
      return "((java.util.function.Supplier<" + result + ">)(() -> { " + body + " })).get()";
    }
    if (language == "csharp") { return "((System.Func<" + result + ">)(() => { " + body + " }))()"; }
    if (language == "go" || language == "swift" || language == "kotlin") {
      std::size_t offset{};
      while ((offset = body.find("} ", offset)) != std::string::npos) {
        body.replace(offset, 2, "}; "); offset += 3;
      }
      if (language == "kotlin") { return "run { " + body + " }"; }
      return language == "go" ? "func() " + result + " { " + body + " }()" : "({ () throws -> " + result + " in " + body + " })()";
    }
    if (language == "swift") { return "({ () throws -> " + result + " in " + body + " })()"; }
    if (language == "kotlin") { return "run { " + body + " }"; }
    if (language == "rust") { return "{ " + body + " }"; }
    static_cast<void>(boolean);
    return "(() => { " + body + " })()";
  };
  const auto checked_leaf = [&](std::string value) {
    const auto& type = node.result_type;
    if (context.unchecked_leaf) { return value; }
    if ((language == "python" || context.predicate || node.kind == operation::member_access || node.kind == operation::index_access || node.local_symbol) &&
        (type == "double" || type == "float")) {
      if (language == "rust") { return "(" + names.checked + (context.predicate ? "_predicate" : "") + "((" + value + ") as f64" + (context.predicate ? ", &mut " + context.valid_name : "") + ") as " + (type == "float" ? "f32" : "f64") + ")"; }
      if (language == "swift") { return type == "float" ? "Float(try " + names.checked + "(Double(" + value + ")))" : names.checked + "(" + value + ")"; }
      if (language == "kotlin") { return names.checked + "(" + (type == "float" ? "(" + value + ").toDouble()" : value) + ")" + (type == "float" ? ".toFloat()" : ""); }
      if (language == "go") { return type == "float" ? "float32(" + names.checked + "(float64(" + value + ")))" : names.checked + "(" + value + ")"; }
      if (language == "java" || language == "csharp") { return (type == "float" ? "((float)" : "(") + names.checked + "(" + value + "))"; }
      return names.checked + "(" + value + ")";
    }
    if (language == "python") {
      if (type == "bool") { return names.checked + "_boolean(" + value + ")"; }
      if (type.starts_with("enum:") && node.resolved_type && context.native_type) {
        const auto maximum = static_cast<const enum_node&>(*node.resolved_type->resolved_node).enum_name_list.size() - 1;
        return names.checked + "_integer(" + value + ", 0, " + std::to_string(maximum) + ", " + context.native_type(*node.resolved_type) + ")";
      }
      if (type.starts_with("int") || type.starts_with("uint") || type == "char") {
        const auto [minimum, maximum] = integer_range(type);
        return names.checked + "_integer(" + value + ", " + minimum + ", " + maximum + ")";
      }
    }
    if (language == "js" || language == "typescript") {
      if (type == "bool") { return names.checked + "_boolean(" + value + ")"; }
      if (type.starts_with("int") || type.starts_with("uint") || type == "char" || type.starts_with("enum:")) {
        const auto [minimum, maximum] = type.starts_with("enum:") ? std::pair<std::string, std::string>{"0", std::to_string(static_cast<const enum_node&>(*node.resolved_type->resolved_node).enum_name_list.size() - 1)} : integer_range(type);
        const auto suffix = type.ends_with("64") ? "n" : "";
        return names.checked + "_integer(" + value + ", " + minimum + suffix + ", " + maximum + suffix + ")";
      }
    }
    return value;
  };
  if (node.kind == operation::integer_literal) { return integral_literal(node, language, context); }
  if (node.kind == operation::call) {
    if (!context.call) { throw std::invalid_argument{"Missing portable function call lowering"}; }
    std::vector<std::string> arguments;
    for (std::size_t index{}; index < node.operands.size(); ++index) {
      auto value = child(index);
      const auto& operand = node.operands[index];
      if (language == "rust" && (operand.result_type == "collection" || operand.result_type.starts_with("record:"))) {
        value = "(" + value + ").clone()";
      }
      if (language == "rust") { value = ungroup_expression(std::move(value)); }
      arguments.push_back(std::move(value));
    }
    return checked_leaf(context.call(node, arguments));
  }
  if (node.kind == operation::literal && node.result_type == "float") {
    auto value = node.value.find_first_of(".eE") == std::string::npos ? node.value + ".0" : node.value;
    return language == "rust" ? value + "_f32" : language == "swift" ? "Float(" + value + ")" : language == "js" || language == "typescript" || language == "python" || language == "go" ? value : value + "f";
  }
  if (node.kind == operation::symbol) {
    const auto local = language == "rust" && node.local_symbol && node.result_type != "collection" && !node.result_type.starts_with("record:") ? "(*" + node.value + ")" : node.value;
    return checked_leaf(node.local_symbol ? local : symbol(node.value));
  }
  if (node.kind == operation::enum_constant) {
    if (!node.resolved_type || !context.enum_value) { throw std::invalid_argument{"Missing portable enum expression binding"}; }
    return context.enum_value(*node.resolved_type, node.value);
  }
  if (node.kind == operation::member_access) {
    auto value = child(0);
    if (language != "cpp") {
      for (const auto base : node.member_base_path) { value += (language == "go" || language == "csharp" ? ".Base" : ".base") + std::to_string(base); }
    }
    return checked_leaf("(" + value + ")." + (context.member_name ? context.member_name(node.value) : node.value));
  }
  if (node.kind == operation::is_finite) {
    auto raw = context; raw.unchecked_leaf = true;
    const auto value = expression(node.operands.at(0), language, symbol, names, raw);
    // Python and Go finite checks inspect a value repeatedly; reached calls must run once.
    if (has_call(node.operands.front()) && language == "python") {
      return "(lambda srl_observed: type(srl_observed) in (int, float) and -1.7976931348623157e308 <= srl_observed <= 1.7976931348623157e308)(" + value + ")";
    }
    if (has_call(node.operands.front()) && language == "go") {
      return "func() bool { srlObserved := float64(" + value + "); return !math.IsNaN(srlObserved) && !math.IsInf(srlObserved, 0) }()";
    }
    if (language == "java") { return "Double.isFinite(" + value + ")"; }
    if (language == "csharp") { return "double.IsFinite(" + value + ")"; }
    if (language == "python") { return "(type(" + value + ") in (int, float) and -1.7976931348623157e308 <= (" + value + ") <= 1.7976931348623157e308)"; }
    if (language == "rust") { return "(" + value + ").is_finite()"; }
    if (language == "go") { return "(!math.IsNaN(float64(" + value + ")) && !math.IsInf(float64(" + value + "), 0))"; }
    if (language == "swift") { return "(" + value + ").isFinite"; }
    if (language == "kotlin") { return "(" + value + ").isFinite()"; }
    if (language == "cpp") { return "::std::isfinite(" + value + ")"; }
    if (language == "c") { return "isfinite(" + value + ")"; }
    return "Number.isFinite(" + value + ")";
  }
  if (node.kind == operation::collection_size) {
    const auto length = collection_length(node.operands.at(0), child(0), language);
    if (language == "rust") { return "(" + length + " as u64)"; }
    if (language == "go") { return "uint64(" + length + ")"; }
    if (language == "swift") { return "UInt64(" + length + ")"; }
    if (language == "kotlin") { return "(" + length + ").toULong()"; }
    if (language == "csharp") { return "((ulong)" + length + ")"; }
    if (language == "java") { return "((long)" + length + ")"; }
    if (language == "js" || language == "typescript") { return "BigInt(" + length + ")"; }
    return length;
  }
  if (node.kind == operation::index_access) {
    const auto container = child(0); const auto index = child(1);
    if (language == "python") { return checked_leaf(names.checked + "_index(" + container + ", " + index + ")"); }
    const auto length = collection_length(node.operands.at(0), "srl_items", language);
    const bool signed_index = node.operands.at(1).result_type.starts_with("int");
    if (language == "rust") {
      return checked_leaf("{ let srl_items = &(" + container + "); let srl_index = " + index + "; if " +
          (signed_index ? "srl_index < 0 || " : "") + "(srl_index as u128) >= (srl_items.len() as u128) { " + failure() + " } " +
          (node.result_type.starts_with("record:") || node.result_type == "collection" ? "&" : "") + "srl_items[srl_index as usize] }");
    }
    std::string setup, invalid, access;
    if (language == "swift") {
      setup = "let srl_items = " + container + "; let srl_index = " + index + "; ";
      invalid = std::string{signed_index ? "srl_index < 0 || " : ""} + "UInt64(srl_index) >= UInt64(" + length + ")";
      access = "srl_items[Int(srl_index)]";
    } else if (language == "kotlin") {
      setup = "val srl_items = " + container + "; val srl_index = " + index + "; ";
      invalid = std::string{signed_index ? "srl_index < 0 || " : ""} + "srl_index" + (node.operands.at(1).result_type == "uint64" ? "" : ".toULong()") + " >= " + length + ".toULong()";
      access = "srl_items[srl_index.toInt()]";
    } else if (language == "go") {
      setup = "srl_items := " + container + "; srl_index := " + index + "; ";
      invalid = std::string{signed_index ? "srl_index < 0 || " : ""} + "uint64(srl_index) >= uint64(" + length + ")";
      access = "srl_items[int(srl_index)]";
    } else {
      setup = std::string{language == "java" || language == "csharp" ? "var " : "const "} + "srl_items = " + container + "; " + (language == "java" || language == "csharp" ? "var " : "const ") + "srl_index = " + index + "; ";
      if (language == "java" && (node.operands.at(1).result_type == "uint32" || node.operands.at(1).result_type == "uint16" || node.operands.at(1).result_type == "uint8" || node.operands.at(1).result_type == "char")) {
        const auto convert = node.operands.at(1).result_type == "uint32" ? "Integer.toUnsignedLong" : node.operands.at(1).result_type == "uint16" ? "Short.toUnsignedInt" : "Byte.toUnsignedInt";
        setup = "var srl_items = " + container + "; var srl_index = " + convert + "(" + index + "); ";
      }
      invalid = signed_index ? "srl_index < 0 || " : "";
      if (language == "java" && node.operands.at(1).result_type == "uint64") { invalid += "Long.compareUnsigned(srl_index, (long)" + length + ") >= 0"; }
      else if (language == "js" || language == "typescript") { invalid += "srl_index >= " + std::string{node.operands.at(1).result_type.ends_with("64") ? "BigInt(" + length + ")" : length}; }
      else { invalid += std::string{language == "csharp" ? "(ulong)srl_index" : "srl_index"} + " >= " + (language == "csharp" ? "(ulong)" : "") + length; }
      access = language == "java" && node.operands.at(0).collection_modifier != member::modifier_type::array ? "srl_items.get((int)srl_index)" : "srl_items[" + std::string{language == "js" || language == "typescript" ? "Number(srl_index)" : language == "csharp" ? "(int)srl_index" : "(int)srl_index"} + "]";
    }
    const auto result = checked_leaf(closure(setup + (language == "swift" ? "if " : "if (") + invalid + (language == "swift" ? " { " : ") { ") + failure() + " } " + (language == "kotlin" ? access : "return " + access + ";"), native(node)));
    return result;
  }
  if (node.kind == operation::all || node.kind == operation::any) {
    const bool all = node.kind == operation::all;
    const auto items = child(0); const auto test = child(1); const auto bound = std::to_string(node.reduction_bound);
    if (language == "python") { return names.checked + "_reduce(" + items + ", " + bound + ", " + context.budget_name + ", lambda " + node.value + ": " + test + ", " + (all ? "True" : "False") + ")"; }
    const auto length = collection_length(node.operands.at(0), "srl_items", language);
    const auto success = all ? "true" : "false"; const auto stop = all ? "false" : "true";
    if (language == "rust") {
      auto condition = test;
      while (!all && condition.starts_with('(') && condition.ends_with(')')) {
        std::size_t depth{}; bool grouped = true;
        for (std::size_t index{}; index < condition.size(); ++index) {
          if (condition[index] == '(') { ++depth; }
          if (condition[index] == ')' && --depth == 0 && index + 1 != condition.size()) { grouped = false; break; }
        }
        if (!grouped) { break; }
        condition = condition.substr(1, condition.size() - 2);
      }
      return "{ let srl_items = &(" + items + "); if srl_items.len() > " + bound + " { " + failure() + " } let mut srl_result = " + success + "; for " + node.value + " in srl_items.iter() { if " + context.budget_name + " == 0 { " + failure() + " } " + context.budget_name + " -= 1; if " + (all ? "!(" + condition + ")" : condition) + " { srl_result = " + stop + "; break; } } srl_result }";
    }
    if (language == "kotlin") {
      return "run { val srl_items = " + items + "; if (" + length + " > " + bound + ") { " + failure() + " }; var srl_result = " + success + "; for (" + node.value + " in srl_items) { if (" + context.budget_name + " == 0) { " + failure() + " }; " + context.budget_name + "--; if (" + (all ? "!(" : "(") + test + ")) { srl_result = " + stop + "; break } }; srl_result }";
    }
    const auto setup = language == "swift" ? "let srl_items = " : language == "go" ? "srl_items := " : language == "java" || language == "csharp" ? "var srl_items = " : "const srl_items = ";
    const auto budget = context.budget_name + (language == "java" ? "[0]" : "");
    const auto guard = language == "swift" || language == "go" ? "if " : "if (";
    const auto guard_end = language == "swift" || language == "go" ? " { " : ") { ";
    const auto loop = language == "swift" ? "for " + node.value + " in srl_items { " :
        language == "go" ? "for _, " + node.value + " := range srl_items { " :
        language == "java" ? "for (var " + node.value + " : srl_items) { " :
        language == "csharp" ? "foreach (var " + node.value + " in srl_items) { " : "for (const " + node.value + " of srl_items) { ";
    const auto check_test = language == "swift" && expression_can_fail(node.operands.at(1), context.predicate) ? "try " + test : test;
    return closure(setup + items + "; " + guard + length + " > " + bound + guard_end + failure() + " } " + loop + guard + budget + " == 0" + guard_end + failure() + " } " + budget + " -= 1; " + guard + (all ? "!(" : "(") + check_test + ")" + guard_end + "return " + stop + "; } } return " + success + ";", language == "java" ? "boolean" : language == "swift" ? "Bool" : "bool", true);
  }
  if (node.kind == operation::less || node.kind == operation::less_equal || node.kind == operation::greater || node.kind == operation::greater_equal || node.kind == operation::equal || node.kind == operation::not_equal) {
    auto left = child(0), right = child(1); const auto& type = node.operands.at(0).result_type;
    const bool equality = node.kind == operation::equal || node.kind == operation::not_equal;
    const auto token = node.kind == operation::less ? " < " : node.kind == operation::less_equal ? " <= " : node.kind == operation::greater ? " > " : node.kind == operation::greater_equal ? " >= " : node.kind == operation::equal ? " == " : " != ";
    if (language == "java" && !equality && type == "uint64") { return "(Long.compareUnsigned(" + left + ", " + right + ")" + token + "0)"; }
    if (language == "java" && !equality && (type.starts_with("uint") || type == "char")) {
      const auto convert = type == "uint32" ? "Integer.toUnsignedLong" : type == "uint16" ? "Short.toUnsignedInt" : "Byte.toUnsignedInt";
      left = std::string{convert} + "(" + left + ")"; right = std::string{convert} + "(" + right + ")";
    }
    if (!equality && type.starts_with("enum:")) {
      if (language == "java" || language == "kotlin") { left = "(" + left + ").ordinal" + (language == "java" ? "()" : ""); right = "(" + right + ").ordinal" + (language == "java" ? "()" : ""); }
      if (language == "swift") { left = "(" + left + ").rawValue"; right = "(" + right + ").rawValue"; }
      if (language == "csharp") { left = "((uint)" + left + ")"; right = "((uint)" + right + ")"; }
      if (language == "rust") { left = "(" + left + " as u32)"; right = "(" + right + " as u32)"; }
    }
    const auto exact_token = (language == "js" || language == "typescript") && equality ? node.kind == operation::equal ? " === " : " !== " : token;
    return "(" + left + exact_token + right + ")";
  }
  return std::nullopt;
}

// Emit dynamic projection checks that preserve integer types and one shared traversal budget.
inline std::string helpers_typed(std::string_view language, const checked_helpers& names = {}) {
  if (language == "js") {
    return "\n// Validate exact reached integer and boolean leaves without coercion.\nfunction " + names.checked + "_integer(value, minimum, maximum) { if (typeof value !== typeof minimum || (typeof value === 'number' && !Number.isInteger(value)) || value < minimum || value > maximum) { throw new RangeError('Invalid exact portable integer'); } return value; }\nfunction " + names.checked + "_boolean(value) { if (typeof value !== 'boolean') { throw new RangeError('Invalid portable boolean'); } return value; }\n";
  }
  if (language == "python") {
    return "\ndef " + names.checked + "_index(items, index):\n    \"\"\"Check a reached index before reading its element.\"\"\"\n    if type(index) is not int or index < 0 or index >= len(items):\n        raise ValueError('Invalid portable index')\n    return items[index]\n\ndef " + names.checked + "_reduce(items, bound, budget, test, is_all):\n    \"\"\"Bound each collection and charge every visited item to the shared invocation budget.\"\"\"\n    if len(items) > bound:\n        raise ValueError('Portable collection exceeds bound')\n    for item in items:\n        if budget[0] == 0:\n            raise ValueError('Portable reduction visit budget exhausted')\n        budget[0] -= 1\n        if test(item) != is_all:\n            return not is_all\n    return is_all\n";
  }
  return {};
}

} // namespace rohit::serializer::writer::behavior
