// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "behavior_writer.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::behavior {

// Identify exact integral schema storage without treating bool or an enum as a number.
inline bool numeric_integer_type(std::string_view type) {
  return type == "char" || type == "int8" || type == "uint8" || type == "int16" ||
      type == "uint16" || type == "int32" || type == "uint32" || type == "int64" || type == "uint64";
}

// Derive bit width from the schema representation, independently of backend object layout.
inline unsigned numeric_bits(std::string_view type) {
  return type == "char" || type.ends_with("8") ? 8u : type.ends_with("16") ? 16u :
      type.ends_with("32") ? 32u : 64u;
}

// Emit exact floating endpoints with an exclusive upper bound, including unsigned 64-bit storage.
inline std::pair<std::string, std::string> numeric_float_endpoints(std::string_view type) {
  const auto bits = numeric_bits(type);
  const bool signed_value = type.starts_with("int");
  const auto exponent = signed_value ? bits - 1 : bits;
  const auto upper = exponent == 64 ? std::string{"18446744073709551616.0"} :
      std::to_string(std::uint64_t{1} << exponent) + ".0";
  return {signed_value ? "-" + upper : "0.0", upper};
}

// Emit arbitrary-precision intermediates only in targets that require them for exact full-width math.
inline std::string numeric_helpers(std::string_view language, const checked_helpers& names) {
  const auto prefix = names.checked + "_numeric";
  std::string source;
  if (language == "python") {
    source = R"CODE(
def SRL_numeric_range(value, minimum, maximum):
    if value < minimum or value > maximum: raise ValueError('Checked numeric result is outside its representation')
    return value
def SRL_numeric_integer(left, right, operation, minimum, maximum):
    if operation == 0: result = left + right
    elif operation == 1: result = left - right
    elif operation == 2: result = left * right
    elif operation == 3:
        if right == 0 or left % right != 0: raise ValueError('Checked integral division requires an exact nonzero divisor')
        result = left // right
    elif operation == 4: result = -left
    else: result = left
    return SRL_numeric_range(result, minimum, maximum)
def SRL_numeric_cast(value, integral, single, minimum, maximum):
    if integral:
        if isinstance(value, float) and (not _math.isfinite(value) or value != int(value)): raise ValueError('Checked integral binding requires an exact finite value')
        return SRL_numeric_range(int(value), minimum, maximum)
    result = float(value)
    if single: result = __import__('struct').unpack('=f', __import__('struct').pack('=f', result))[0]
    if not _math.isfinite(result) or result != value: raise ValueError('Checked floating binding requires an exact finite value')
    return result
def SRL_numeric_single(value, checked=True):
    try: result = __import__('struct').unpack('=f', __import__('struct').pack('=f', value))[0]
    except OverflowError: result = _math.copysign(float('inf'), value)
    if checked and not _math.isfinite(result): raise ValueError('Checked floating input is nonfinite')
    return result
)CODE";
  } else if (language == "js" || language == "typescript") {
    source = R"CODE(
function SRL_numeric_range(value, minimum, maximum) { if (value < minimum || value > maximum) { throw new RangeError('Checked numeric result is outside its representation'); } return value; }
function SRL_numeric_integer(left, right, operation, minimum, maximum) {
  let result;
  if (operation === 0) { result = left + right; } else if (operation === 1) { result = left - right; } else if (operation === 2) { result = left * right; }
  else if (operation === 3) { if (right === 0n || left % right !== 0n) { throw new RangeError('Checked integral division requires an exact nonzero divisor'); } result = left / right; }
  else if (operation === 4) { result = -left; } else { result = left; }
  return SRL_numeric_range(result, minimum, maximum);
}
function SRL_numeric_cast(value, integral, single, minimum, maximum) {
  if (integral) { if (typeof value === 'number' && (!Number.isFinite(value) || !Number.isInteger(value))) { throw new RangeError('Checked integral binding requires an exact finite value'); } return SRL_numeric_range(BigInt(value), minimum, maximum); }
  let result = Number(value); if (single) { result = Math.fround(result); }
  if (!Number.isFinite(result) || (typeof value === 'bigint' ? BigInt(result) !== value : result !== value)) { throw new RangeError('Checked floating binding requires an exact finite value'); }
  return result;
}
function SRL_numeric_single(value, checked = true) { const result = Math.fround(value); if (checked && !Number.isFinite(result)) { throw new RangeError('Checked floating input is nonfinite'); } return result; }
)CODE";
  } else if (language == "java") {
    source = R"CODE(
private static java.math.BigInteger SRL_numeric_range(java.math.BigInteger value, String minimum, String maximum) {
  if (value.compareTo(new java.math.BigInteger(minimum)) < 0 || value.compareTo(new java.math.BigInteger(maximum)) > 0) { throw new ArithmeticException("Checked numeric result is outside its representation"); } return value;
}
private static java.math.BigInteger SRL_numeric_integer(java.math.BigInteger left, java.math.BigInteger right, int operation, String minimum, String maximum) {
  java.math.BigInteger result;
  if (operation == 0) { result = left.add(right); } else if (operation == 1) { result = left.subtract(right); } else if (operation == 2) { result = left.multiply(right); }
  else if (operation == 3) { if (right.signum() == 0 || left.remainder(right).signum() != 0) { throw new ArithmeticException("Checked integral division requires an exact nonzero divisor"); } result = left.divide(right); }
  else if (operation == 4) { result = left.negate(); } else { result = left; }
  return SRL_numeric_range(result, minimum, maximum);
}
private static java.math.BigInteger SRL_numeric_integral(double value, String minimum, String maximum) { if (!Double.isFinite(value)) { throw new ArithmeticException("Checked numeric binding requires finite input"); } return SRL_numeric_range(new java.math.BigDecimal(value).toBigIntegerExact(), minimum, maximum); }
private static double SRL_numeric_floating(java.math.BigInteger value, boolean single) { double result = single ? (double)value.floatValue() : value.doubleValue(); if (!Double.isFinite(result) || !new java.math.BigDecimal(result).toBigIntegerExact().equals(value)) { throw new ArithmeticException("Checked floating binding is inexact"); } return result; }
private static float SRL_numeric_float(double value) { float result = (float)value; if (!Float.isFinite(result) || (double)result != value) { throw new ArithmeticException("Checked floating binding is inexact"); } return result; }
)CODE";
  } else if (language == "csharp") {
    source = R"CODE(
private static System.Numerics.BigInteger SRL_numeric_range(System.Numerics.BigInteger value, string minimum, string maximum) { if (value < System.Numerics.BigInteger.Parse(minimum, System.Globalization.CultureInfo.InvariantCulture) || value > System.Numerics.BigInteger.Parse(maximum, System.Globalization.CultureInfo.InvariantCulture)) { throw new System.ArithmeticException("Checked numeric result is outside its representation"); } return value; }
private static System.Numerics.BigInteger SRL_numeric_integer(System.Numerics.BigInteger left, System.Numerics.BigInteger right, int operation, string minimum, string maximum) { System.Numerics.BigInteger result; if (operation == 0) { result = left + right; } else if (operation == 1) { result = left - right; } else if (operation == 2) { result = left * right; } else if (operation == 3) { if (right.IsZero || left % right != 0) { throw new System.ArithmeticException("Checked integral division requires an exact nonzero divisor"); } result = left / right; } else if (operation == 4) { result = -left; } else { result = left; } return SRL_numeric_range(result, minimum, maximum); }
private static System.Numerics.BigInteger SRL_numeric_integral(double value, string minimum, string maximum) { if (!double.IsFinite(value) || System.Math.Truncate(value) != value) { throw new System.ArithmeticException("Checked integral binding is inexact"); } return SRL_numeric_range(new System.Numerics.BigInteger(value), minimum, maximum); }
private static double SRL_numeric_floating(System.Numerics.BigInteger value, bool single) { double result = single ? (double)(float)value : (double)value; if (!double.IsFinite(result) || new System.Numerics.BigInteger(result) != value) { throw new System.ArithmeticException("Checked floating binding is inexact"); } return result; }
private static float SRL_numeric_float(double value) { float result = (float)value; if (!float.IsFinite(result) || (double)result != value) { throw new System.ArithmeticException("Checked floating binding is inexact"); } return result; }
)CODE";
  } else if (language == "kotlin") {
    source = R"CODE(
private fun SRL_numeric_range(value: java.math.BigInteger, minimum: String, maximum: String): java.math.BigInteger { if (value < java.math.BigInteger(minimum) || value > java.math.BigInteger(maximum)) { throw SerializerException("Checked numeric result is outside its representation") }; return value }
private fun SRL_numeric_integer(left: java.math.BigInteger, right: java.math.BigInteger, operation: Int, minimum: String, maximum: String): java.math.BigInteger { val result = when(operation) { 0 -> left.add(right); 1 -> left.subtract(right); 2 -> left.multiply(right); 3 -> { if (right.signum() == 0 || left.remainder(right).signum() != 0) { throw SerializerException("Checked integral division requires an exact nonzero divisor") }; left.divide(right) }; 4 -> left.negate(); else -> left }; return SRL_numeric_range(result, minimum, maximum) }
private fun SRL_numeric_integral(value: Double, minimum: String, maximum: String): java.math.BigInteger { if (!value.isFinite()) { throw SerializerException("Checked numeric binding requires finite input") }; val integer = try { java.math.BigDecimal(value).toBigIntegerExact() } catch (_: ArithmeticException) { throw SerializerException("Checked integral binding is inexact") }; return SRL_numeric_range(integer, minimum, maximum) }
private fun SRL_numeric_floating(value: java.math.BigInteger, single: Boolean): Double { val result = if (single) value.toFloat().toDouble() else value.toDouble(); if (!result.isFinite() || java.math.BigDecimal(result).toBigIntegerExact() != value) { throw SerializerException("Checked floating binding is inexact") }; return result }
private fun SRL_numeric_float(value: Double): Float { val result = value.toFloat(); if (!result.isFinite() || result.toDouble() != value) { throw SerializerException("Checked floating binding is inexact") }; return result }
)CODE";
  } else if (language == "swift") {
    source = R"CODE(
private func SRL_numeric_nonzero<T: Equatable & ExpressibleByIntegerLiteral>(_ value: T) throws {
  if value == 0 { throw SerializerError.invalid("Checked numeric division requires a nonzero divisor") }
}
private func SRL_numeric_divide<T: FixedWidthInteger>(_ left: T, _ right: T) throws -> T {
  let division = left.dividedReportingOverflow(by: right)
  let remainder = left.remainderReportingOverflow(dividingBy: right)
  if division.overflow || remainder.overflow || remainder.partialValue != 0 {
    throw SerializerError.invalid("Checked integral division requires an exact representable result")
  }
  return division.partialValue
}
)CODE";
  }
  std::size_t offset{};
  constexpr std::string_view original{"SRL_numeric"};
  while ((offset = source.find(original, offset)) != std::string::npos) {
    source.replace(offset, original.size(), prefix); offset += prefix.size();
  }
  return source;
}

// Lower exact operations with single evaluation of operands and full-width intermediate storage.
inline std::optional<std::string> numeric_expression(const behavior_expression& node, std::string_view language,
    const std::function<std::string(std::string_view)>& symbol, const checked_helpers& names,
    const expression_context& context) {
  using operation = behavior_expression::operation;
  const bool cast = node.kind == operation::checked_cast;
  const bool arithmetic = node.kind == operation::add || node.kind == operation::subtract ||
      node.kind == operation::multiply || node.kind == operation::divide ||
      node.kind == operation::positive || node.kind == operation::negative;
  const auto prefix = names.checked + "_numeric";
  if (node.numeric_contract_enabled && node.result_type == "float" && !cast && !arithmetic &&
      (language == "python" || language == "js" || language == "typescript")) {
    // Restore f32 leaves for comparisons and lazy guards; finite inspection deliberately remains raw.
    auto raw = node;
    raw.numeric_contract_enabled = false;
    return prefix + "_single(" + expression(raw, language, symbol, names, context) +
        (context.unchecked_leaf ? (language == "python" ? ", False)" : ", false)") : ")");
  }
  if (!cast && (!arithmetic || !node.numeric_contract_enabled || node.result_type == "double")) { return {}; }
  const auto child = [&](std::size_t index) { return expression(node.operands.at(index), language, symbol, names, context); };
  const auto native = node.resolved_type && context.native_type ? context.native_type(*node.resolved_type) : std::string{};
  const auto kind = node.kind == operation::add ? 0 : node.kind == operation::subtract ? 1 :
      node.kind == operation::multiply ? 2 : node.kind == operation::divide ? 3 :
      node.kind == operation::negative ? 4 : 5;
  auto left = child(0), right = node.operands.size() == 2 ? child(1) : std::string{};
  if (language == "cpp") {
    const auto function = cast || node.kind == operation::positive ? "checked_numeric_cast" :
        node.kind == operation::negative ? "checked_negate" : node.kind == operation::add ? "checked_add" :
        node.kind == operation::subtract ? "checked_subtract" : node.kind == operation::multiply ? "checked_multiply" : "checked_divide";
    return "::rohit::serializer::" + std::string{function} + "<" + native + ">(" + left +
        (right.empty() ? "" : ", " + right) + ")";
  }
  const auto target_integer = numeric_integer_type(node.result_type);
  const auto source_type = node.operands.front().result_type;
  const auto source_integer = numeric_integer_type(source_type);
  if (language == "python" || language == "js" || language == "typescript") {
    // Dynamic targets use binary64 storage; restore the declared f32 operand before evaluation.
    if (source_type == "float") { left = prefix + "_single(" + left + ")"; }
    if (!right.empty() && node.operands.at(1).result_type == "float") { right = prefix + "_single(" + right + ")"; }
  }
  const auto [minimum, maximum] = target_integer ? integer_range(node.result_type) : std::pair<std::string, std::string>{"0", "0"};
  const auto limits = language == "python" ? minimum + ", " + maximum :
      language == "js" || language == "typescript" ? minimum + "n, " + maximum + "n" :
      "\"" + minimum + "\", \"" + maximum + "\"";
  const auto boolean = [&](bool value) { return language == "python" ? (value ? "True" : "False") : (value ? "true" : "false"); };
  // Floating arithmetic rounds once in its own representation and then checks the finite result.
  if (!cast && node.result_type == "float") {
    const auto token = node.kind == operation::add ? " + " : node.kind == operation::subtract ? " - " : " * ";
    const auto value = node.kind == operation::positive ? left : node.kind == operation::negative ? "(-" + left + ")" :
        node.kind == operation::divide ? names.divide + "(" + left + ", " + right + ")" : "(" + left + token + right + ")";
    if (language == "python") { return names.checked + "(__import__('struct').unpack('=f', __import__('struct').pack('=f', " + value + "))[0])"; }
    if (language == "js" || language == "typescript") { return names.checked + "(Math.fround(" + value + "))"; }
    if (language == "java") { return "((float)" + names.checked + "(" +
        (node.kind == operation::divide ? "((float)(" + value + "))" : value) + "))"; }
    if (language == "csharp") { return "((float)" + names.checked + "((float)(" + value + ")))"; }
    if (language == "kotlin") {
      const auto native_value = node.kind == operation::divide ? names.divide + "((" + left +
          ").toDouble(), (" + right + ").toDouble()).toFloat()" : value;
      return names.checked + "((" + native_value + ").toDouble()).toFloat()";
    }
  }
  if (language == "python") {
    if (cast) { return prefix + "_cast(" + left + ", " + boolean(target_integer) + ", " + boolean(node.result_type == "float") + ", " + limits + ")"; }
    if (target_integer) { return prefix + "_integer(" + left + ", " + (right.empty() ? "0" : right) + ", " + std::to_string(kind) + ", " + limits + ")"; }
  }
  if (language == "js" || language == "typescript") {
    const auto result = [&](std::string value) { return target_integer && !node.result_type.ends_with("64") ? "Number(" + value + ")" : value; };
    if (cast) { return result(prefix + "_cast(" + left + ", " + boolean(target_integer) + ", " + boolean(node.result_type == "float") + ", " + limits + ")"); }
    if (target_integer) { return result(prefix + "_integer(BigInt(" + left + "), " + (right.empty() ? "0n" : "BigInt(" + right + ")") + ", " + std::to_string(kind) + ", " + limits + ")"); }
  }
  if (language == "java" || language == "csharp" || language == "kotlin") {
    const auto big = [&](const std::string& value, const std::string& type) {
      if (language == "csharp") { return "new System.Numerics.BigInteger(" + value + ")"; }
      if (language == "kotlin") { return "java.math.BigInteger((" + value + ").toString())"; }
      if (type == "uint64") { return "new java.math.BigInteger(Long.toUnsignedString(" + value + "))"; }
      if (type == "uint32") { return "java.math.BigInteger.valueOf(Integer.toUnsignedLong(" + value + "))"; }
      if (type == "uint16") { return "java.math.BigInteger.valueOf(Short.toUnsignedInt(" + value + "))"; }
      if (type == "uint8" || type == "char") { return "java.math.BigInteger.valueOf(Byte.toUnsignedInt(" + value + "))"; }
      return "java.math.BigInteger.valueOf(" + value + ")";
    };
    const auto unbig = [&](std::string value) {
      if (language == "csharp") { return "unchecked((" + native + ")(" + value + "))"; }
      const auto bits = numeric_bits(node.result_type);
      const auto suffix = bits == 64 ? "Long" : bits == 32 ? "Int" : bits == 16 ? "Short" : "Byte";
      if (language == "java") { return "(" + value + ")." + (bits == 64 ? "long" : bits == 32 ? "int" : bits == 16 ? "short" : "byte") + "Value()"; }
      return "(" + value + ").to" + suffix + "()" + (node.result_type.starts_with("uint") || node.result_type == "char" ? ".toU" + std::string{suffix} + "()" : "");
    };
    if (cast) {
      if (target_integer) {
        return unbig(source_integer ? prefix + "_range(" + big(left, source_type) + ", " + limits + ")" :
            prefix + "_integral(" + (language == "kotlin" && source_type == "float" ? "(" + left + ").toDouble()" : left) + ", " + limits + ")");
      }
      if (source_integer) {
        auto result = prefix + "_floating(" + big(left, source_type) + ", " + boolean(node.result_type == "float") + ")";
        return node.result_type == "float" ? (language == "kotlin" ? "(" + result + ").toFloat()" : "((float)(" + result + "))") : result;
      }
      if (node.result_type == "float") { return prefix + "_float(" + (language == "kotlin" && source_type == "float" ? "(" + left + ").toDouble()" : left) + ")"; }
      return language == "kotlin" ? names.checked + "((" + left + ").toDouble())" : names.checked + "(" + left + ")";
    }
    if (target_integer) {
      return unbig(prefix + "_integer(" + big(left, source_type) + ", " + (right.empty() ?
          (language == "csharp" ? "System.Numerics.BigInteger.Zero" : "java.math.BigInteger.ZERO") :
          big(right, node.operands.at(1).result_type)) + ", " + std::to_string(kind) + ", " + limits + ")");
    }
  }
  if (language == "go" || language == "rust" || language == "swift") {
    const auto source_native = node.operands.front().resolved_type && context.native_type ?
        context.native_type(*node.operands.front().resolved_type) : std::string{};
    auto a = "srl_numeric_left_" + std::to_string(node.source_offset);
    auto b = "srl_numeric_right_" + std::to_string(node.source_offset);
    auto result = "srl_numeric_result_" + std::to_string(node.source_offset);
    while (left.find(a) != std::string::npos || right.find(a) != std::string::npos) { a += '_'; }
    while (left.find(b) != std::string::npos || right.find(b) != std::string::npos) { b += '_'; }
    while (left.find(result) != std::string::npos || right.find(result) != std::string::npos) { result += '_'; }
    const auto declare = [&](const std::string& name, const std::string& value) {
      return language == "go" ? name + " := " + value + "; " : "let " + name + " = " +
          (language == "rust" ? ungroup_expression(value) : value) + "; ";
    };
    const auto convert = [&](const std::string& type, const std::string& value) {
      return language == "rust" ? value + " as " + type : type + "(" + value + ")";
    };
    const auto failure = language == "rust" && context.predicate ? "return false; " :
        language == "go" ? "panic(\"Invalid checked numeric expression\"); " :
        language == "swift" ? "throw SerializerError.invalid(\"Invalid checked numeric expression\"); " :
        "panic!(\"Invalid checked numeric expression\"); ";
    const auto finish = [&](std::string body, const std::string& value) {
      if (language == "go") { return "func() " + native + " { " + body + "return " + value + " }()"; }
      if (language == "swift") { return "({ () throws -> " + native + " in " + body + "return " + value + " })()"; }
      return "{ " + body + value + " }";
    };
    const auto guard = [&](const std::string& condition) {
      return "if " + std::string{language == "swift" ? "(" : ""} + condition +
          (language == "swift" ? ")" : "") + " { " + failure + "}; ";
    };
    const bool typed_float = !cast && node.result_type == "float" && language == "go";
    const auto throwing_child = [&](std::size_t index, const std::string& value) {
      return language == "swift" && expression_can_fail(node.operands.at(index), context.predicate) ? "try " + value : value;
    };
    auto body = declare(a, typed_float ? convert(native, left) : throwing_child(0, left));
    if (!right.empty()) { body += declare(b, typed_float ? convert(native, right) : throwing_child(1, right)); }
    if (cast && language == "swift") {
      body += "guard let " + result + " = " + native + "(exactly: " + a + ") else { " + failure + "}; ";
      if (!target_integer) { body += guard("!" + result + ".isFinite"); }
      return finish(body, result);
    }
    if (cast && target_integer && source_integer && language == "rust") {
      const auto conversion = native + "::try_from(" + a + ")";
      return finish(body, context.predicate ? "match " + conversion + " { Ok(value) => value, Err(_) => { " + context.valid_name + " = false; 0 } }" :
          conversion + ".expect(\"Checked numeric value is outside its representation\")");
    }
    if (cast && target_integer && source_integer) {
      const auto source_signed = source_type.starts_with("int");
      auto condition = convert("uint64", a) + " > " + maximum;
      if (source_signed) {
        condition = "(" + a + " < 0 && " + (node.result_type.starts_with("int") ?
            convert("int64", a) + " < " + minimum : "true") + ") || (" + a + " >= 0 && " + condition + ")";
      }
      body += guard(condition);
      return finish(body, convert(native, a));
    }
    if (cast) {
      body += declare(result, convert(native, a));
      const auto finite = [&](const std::string& value) {
        return language == "go" ? "!math.IsNaN(float64(" + value + ")) && !math.IsInf(float64(" + value + "), 0)" :
            "(" + value + ").is_finite()";
      };
      std::string condition;
      if (source_integer) {
        const auto [lower, upper] = numeric_float_endpoints(source_type);
        condition = "!(" + finite(result) + ") || " + result + " < " + lower + " || " + result + " >= " + upper;
        // A back conversion is safe only after the exclusive integer endpoints pass.
        body += guard(condition);
        condition = convert(source_native, result) + " != " + a;
      } else if (target_integer) {
        const auto [lower, upper] = numeric_float_endpoints(node.result_type);
        condition = "!(" + finite(a) + ") || " + a + " < " + lower + " || " + a + " >= " + upper;
        // Perform the integer conversion after validating its source, never before it.
        body = declare(a, left) + guard(condition) + declare(result, convert(native, a));
        condition = convert(source_native, result) + " != " + a;
      } else {
        condition = "!(" + finite(result) + ") || " + convert(source_native, result) + " != " + a;
      }
      if (language == "rust" && context.predicate) {
        return finish(body, "if " + condition + " { " + context.valid_name + " = false; 0 as " + native + " } else { " + result + " }");
      }
      body += guard(condition);
      return finish(body, result);
    }
    if (node.result_type == "float") {
      if (node.kind == operation::divide) {
        body += language == "swift" ? "try " + prefix + "_nonzero(" + b + "); " : guard(b + " == 0.0");
      }
      const auto value = node.kind == operation::positive ? a : node.kind == operation::negative ? "(-" + a + ")" :
          "(" + a + (node.kind == operation::add ? " + " : node.kind == operation::subtract ? " - " : node.kind == operation::multiply ? " * " : " / ") + b + ")";
      body += declare(result, value);
      const auto checked = language == "go" ? native + "(" + names.checked + "(float64(" + result + ")))" :
          language == "swift" ? native + "(try " + names.checked + "(Double(" + result + ")))" :
          names.checked + (context.predicate ? "_predicate" : "") + "(" + result + " as f64" +
          (context.predicate ? ", &mut " + context.valid_name : "") + ") as " + native;
      return finish(body, checked);
    }
    if (language == "rust") {
      const auto method = kind == 0 ? "checked_add" : kind == 1 ? "checked_sub" : kind == 2 ? "checked_mul" : kind == 3 ? "checked_div" : "checked_neg";
      auto value = kind == 5 ? "Some(" + a + ")" : a + "." + method + "(" + (kind == 4 ? "" : b) + ")";
      if (kind == 3) { value += ".and_then(|quotient| if " + a + ".checked_rem(" + b + ") == Some(0) { Some(quotient) } else { None })"; }
      return finish(body, context.predicate ? "match " + value + " { Some(value) => value, None => { " + context.valid_name + " = false; 0 } }" :
          value + ".expect(\"Invalid checked integral expression\")");
    }
    if (language == "swift") {
      if (kind == 5) { return finish(body, a); }
      if (kind == 3) {
        return finish(body, "try " + prefix + "_divide(" + a + ", " + b + ")");
      }
      const auto method = kind == 0 ? "addingReportingOverflow" : kind == 1 || kind == 4 ? "subtractingReportingOverflow" : "multipliedReportingOverflow";
      const auto invocation = kind == 4 ? native + "(0)." + method + "(" + a + ")" :
          a + "." + method + "(" + (kind == 2 ? "by: " : "") + b + ")";
      body += declare(result, invocation) + guard(result + ".overflow");
      return finish(body, result + ".partialValue");
    }
    const bool signed_value = node.result_type.starts_with("int");
    std::string invalid;
    if (kind == 0) { invalid = signed_value ? "(" + b + " > 0 && " + a + " > " + maximum + " - " + b + ") || (" + b + " < 0 && " + a + " < " + minimum + " - " + b + ")" : a + " > " + maximum + " - " + b; }
    if (kind == 1) { invalid = signed_value ? "(" + b + " > 0 && " + a + " < " + minimum + " + " + b + ") || (" + b + " < 0 && " + a + " > " + maximum + " + " + b + ")" : a + " < " + b; }
    if (kind == 2) {
      invalid = signed_value ? "(" + a + " > 0 && ((" + b + " > 0 && " + a + " > " + maximum + " / " + b + ") || (" + b + " < 0 && " + b + " < " + minimum + " / " + a + "))) || (" + a + " < 0 && ((" + b + " > 0 && " + a + " < " + minimum + " / " + b + ") || (" + b + " < 0 && " + a + " < " + maximum + " / " + b + ")))" : b + " != 0 && " + a + " > " + maximum + " / " + b;
    }
    if (kind == 3) {
      invalid = b + " == 0" + (signed_value ? " || (" + a + " == " + minimum + " && " + b + " == -1)" : "");
      body += guard(invalid) + guard(a + " % " + b + " != 0"); invalid.clear();
    }
    if (kind == 4) { invalid = signed_value ? a + " == " + minimum : a + " != 0"; }
    if (!invalid.empty()) { body += guard(invalid); }
    const auto value = kind == 5 ? a : kind == 4 ? "(-" + a + ")" : "(" + a +
        (kind == 0 ? " + " : kind == 1 ? " - " : kind == 2 ? " * " : " / ") + b + ")";
    return finish(body, value);
  }
  throw std::invalid_argument{std::string{language} + ": missing checked numeric expression lowering"};
}
} // namespace rohit::serializer::writer::behavior
