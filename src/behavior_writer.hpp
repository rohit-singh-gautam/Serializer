// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::behavior {

// Private arithmetic helper names remain distinct from all authored symbols in one output unit.
struct checked_helpers {
  std::string checked{"srl_behavior_checked"};
  std::string divide{"srl_behavior_divide"};
  std::string square_root{"srl_behavior_sqrt"};
  // Compare the complete private helper contract for reuse and guard selection.
  bool operator==(const checked_helpers&) const = default;
};

// Bind typed projections and the per-invocation predicate validity and reduction budget.
struct expression_context {
  std::function<std::string(std::string_view)> member_name{};
  std::function<std::string(const type_name&)> native_type{};
  std::function<std::string(const type_name&, std::string_view)> enum_value{};
  std::function<std::string(const behavior_expression&, const std::vector<std::string>&)> call{};
  bool predicate{};
  bool unchecked_leaf{};
  std::string valid_name{"srl_behavior_ok"};
  std::string budget_name{"srl_behavior_budget"};
};

// Recover the exact readonly declaration bound by validation rather than resolving names again.
inline const function_declaration& called_function(const behavior_expression& node) {
  if (!node.call_target) { throw std::invalid_argument{"Missing portable function call binding"}; }
  if (node.call_target->type == object_type::function_type) {
    return static_cast<const function_node&>(*node.call_target).function;
  }
  return static_cast<const class_node&>(*node.call_target).functions.at(node.call_method_index);
}

// Detect calls whose native implementation determines exception and constant-evaluation behavior.
inline bool has_call(const behavior_expression& node) {
  return node.kind == behavior_expression::operation::call ||
      std::any_of(node.operands.begin(), node.operands.end(), has_call);
}

// Assemble arguments already lowered in source order without changing their evaluation.
inline std::string call_arguments(const std::vector<std::string>& arguments, std::size_t begin = 0) {
  std::string result;
  for (std::size_t index{begin}; index < arguments.size(); ++index) {
    if (!result.empty()) { result += ", "; }
    result += arguments[index];
  }
  return result;
}

// Remove complete outer groups when a Rust assignment or call argument already delimits a value.
inline std::string ungroup_expression(std::string value) {
  while (value.starts_with('(') && value.ends_with(')')) {
    std::size_t depth{};
    bool complete = true;
    for (std::size_t index{}; index < value.size(); ++index) {
      if (value[index] == '(') { ++depth; }
      if (value[index] == ')' && --depth == 0 && index + 1 != value.size()) { complete = false; break; }
    }
    if (!complete) { break; }
    value = value.substr(1, value.size() - 2);
  }
  return value;
}

// Lower typed operations before the legacy binary64 operation cases.
inline std::optional<std::string> typed_expression(const behavior_expression& node, std::string_view language,
    const std::function<std::string(std::string_view)>& symbol, const checked_helpers& names,
    const expression_context& context);

// Lower exact numeric operations independently of the older binary64 expression contract.
inline std::optional<std::string> numeric_expression(const behavior_expression& node, std::string_view language,
    const std::function<std::string(std::string_view)>& symbol, const checked_helpers& names,
    const expression_context& context);

// Emit target-native exact numeric helpers without changing model storage or codecs.
inline std::string numeric_helpers(std::string_view language, const checked_helpers& names);

// Lower a resolved expression using backend names and explicit evaluation state.
inline std::string expression(const behavior_expression& node, std::string_view language,
    const std::function<std::string(std::string_view)>& symbol, const checked_helpers& names = {},
    const expression_context& context = {});

// Choose helper spellings after backend conversion without reserving otherwise-valid schema names.
inline checked_helpers select_helpers(const std::vector<const syntax_node*>& nodes,
    const std::function<std::string(const syntax_node&)>& declaration,
    const std::function<std::string(std::string_view)>& identifier) {
  std::set<std::string> symbols;
  for (const auto* node : nodes) {
    symbols.insert(declaration(*node));
    if (node->type == object_type::function_type) {
      for (const auto& parameter : static_cast<const function_node&>(*node).function.parameters) {
        symbols.insert(identifier(parameter.name));
      }
    }
    if (node->type != object_type::class_type) { continue; }
    const auto& owner = static_cast<const class_node&>(*node);
    for (const auto& field : owner.member_list) { symbols.insert(identifier(field.name)); }
    for (const auto& method : owner.functions) {
      const auto name = identifier(method.name);
      symbols.insert(name);
      // C and Go publish owner-prefixed free functions; other backends use member names.
      symbols.insert(declaration(*node) + "_" + name);
      for (const auto& parameter : method.parameters) { symbols.insert(identifier(parameter.name)); }
    }
    for (const auto& factory : owner.constructions) {
      symbols.insert(identifier(factory.constructor ? "create" : factory.name));
      for (const auto& parameter : factory.parameters) { symbols.insert(identifier(parameter.name)); }
    }
  }
  checked_helpers result{identifier("portable_arithmetic_checked"), identifier("portable_arithmetic_divide"),
                         identifier("portable_arithmetic_sqrt")};
  const auto collides = [&](const auto& helper) {
    return symbols.contains(helper) || symbols.contains(helper + "_predicate") ||
        symbols.contains(helper + "_integer") || symbols.contains(helper + "_boolean") ||
        symbols.contains(helper + "_index") || symbols.contains(helper + "_reduce") ||
        symbols.contains(helper + "_numeric_range") || symbols.contains(helper + "_numeric_integer") ||
        symbols.contains(helper + "_numeric_cast") || symbols.contains(helper + "_numeric_integral") ||
        symbols.contains(helper + "_numeric_floating") || symbols.contains(helper + "_numeric_float") ||
        symbols.contains(helper + "_numeric_nonzero") || symbols.contains(helper + "_numeric_divide") ||
        symbols.contains(helper + "_numeric_single");
  };
  while (collides(result.checked) || collides(result.divide) || collides(result.square_root)) {
    result.checked += '_'; result.divide += '_'; result.square_root += '_';
  }
  return result;
}

// Select an authored implementation without interpreting or rewriting its native tokens.
inline const native_code_block* native_body(const function_declaration& method, std::string_view language) {
  const auto common_type = [&](const auto& self, const type_name& type) -> bool {
    return type.common_numeric || std::any_of(type.arguments.begin(), type.arguments.end(),
        [&](const auto& argument) { return self(self, argument); });
  };
  if (language != "cpp" && (!method.generic_parameters.empty() || common_type(common_type, method.return_type) ||
      std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& parameter) {
        return common_type(common_type, parameter.type);
      }))) {
    throw std::invalid_argument{std::string{language} + ": generic readonly inputs and common_type require C++ output"};
  }
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
  const bool hooks = std::any_of(owner.constructions.begin(), owner.constructions.end(),
      [&](const auto& factory) { return std::any_of(factory.native_validations.begin(), factory.native_validations.end(),
          [&](const auto& block) { return block.language == language; }) ||
          std::any_of(factory.initializers.begin(), factory.initializers.end(),
              [&](const auto& block) { return block.language == language; }); });
  if (renamed && (blocks || bodies || hooks)) {
    throw std::invalid_argument{std::string{language} + ": opaque native code requires naming = preserve"};
  }
}

// Lower already type-checked expressions using bound backend field/parameter spellings.
inline std::string expression(const behavior_expression& node, std::string_view language,
                              const std::function<std::string(std::string_view)>& symbol,
                              const checked_helpers& names, const expression_context& context) {
  using operation = behavior_expression::operation;
  if (const auto numeric = numeric_expression(node, language, symbol, names, context)) { return *numeric; }
  if (const auto typed = typed_expression(node, language, symbol, names, context)) { return *typed; }
  const auto argument = [&](std::string value) {
    // Native Rust rejects redundant grouping around helper arguments and condition values.
    while (language == "rust" && value.starts_with('(') && value.ends_with(')')) {
      std::size_t nesting{};
      bool complete_group = true;
      for (std::size_t index{}; index < value.size(); ++index) {
        if (value[index] == '(') { ++nesting; }
        if (value[index] == ')' && --nesting == 0 && index + 1 != value.size()) {
          complete_group = false; break;
        }
      }
      if (!complete_group) { break; }
      value = value.substr(1, value.size() - 2);
    }
    return value;
  };
  const auto helper_prefix = language == "cpp" ? "::rohit::serializer::detail::" : "";
  const auto checked = [&](std::string value) {
    if (context.predicate && language == "rust") {
      return names.checked + "_predicate(" + argument(std::move(value)) + ", &mut " + context.valid_name + ")";
    }
    return std::string{helper_prefix} + names.checked + "(" + argument(std::move(value)) + (language == "c" ? ", &srl_behavior_ok)" : ")");
  };
  const auto child = [&](std::size_t index) { return expression(node.operands.at(index), language, symbol, names, context); };
  switch (node.kind) {
  case operation::literal:
    return (node.value.find_first_of(".eE") == std::string::npos ? node.value + ".0" : node.value) + (language == "rust" ? "_f64" : "");
  case operation::symbol: return symbol(node.value);
  case operation::boolean: return language == "python" ? (node.value == "true" ? "True" : "False") : node.value;
  case operation::pi: return language == "rust" ? "3.141592653589793_f64" : "3.141592653589793";
  case operation::to_double: {
    const auto value = child(0);
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
  case operation::positive: return std::string(language == "rust" ? "(" : "(+") + child(0) + ")";
  case operation::negative: return "(-" + child(0) + ")";
  case operation::divide: return std::string{helper_prefix} + names.divide + (context.predicate && language == "rust" ? "_predicate" : "") + "(" + argument(child(0)) + ", " + argument(child(1)) + (context.predicate && language == "rust" ? ", &mut " + context.valid_name + ")" : language == "c" ? ", &srl_behavior_ok)" : ")");
  case operation::square_root: return std::string{helper_prefix} + names.square_root + (context.predicate && language == "rust" ? "_predicate" : "") + "(" + argument(child(0)) + (context.predicate && language == "rust" ? ", &mut " + context.valid_name + ")" : language == "c" ? ", &srl_behavior_ok)" : ")");
  case operation::sine:
  case operation::cosine: {
    const auto function = node.kind == operation::sine ? "sin" : "cos";
    const auto value = child(0);
    if (language == "rust") { return checked("(" + value + ")." + function + "()"); }
    const auto prefix = language == "cpp" ? "::std::" : language == "csharp" ? "System.Math." : language == "java" || language == "js" || language == "typescript" ? "Math." : language == "python" ? "_math." : language == "go" ? "math." : language == "kotlin" ? "kotlin.math." : "";
    const auto spelling = language == "go" || language == "csharp" ? (node.kind == operation::sine ? "Sin" : "Cos") : function;
    return checked(std::string{prefix} + spelling + "(" + value + ")");
  }
  case operation::logical_not: return std::string{language == "python" ? "(not " : "(!"} + child(0) + ")";
  case operation::logical_and:
  case operation::logical_or:
    return "(" + child(0) + (node.kind == operation::logical_and ? (language == "python" ? " and " : " && ") : (language == "python" ? " or " : " || ")) + child(1) + ")";
  case operation::conditional:
    if (language == "python") { return "(" + child(1) + " if " + child(0) + " else " + child(2) + ")"; }
    if (language == "rust") { return "(if " + argument(child(0)) + " { " + argument(child(1)) + " } else { " + argument(child(2)) + " })"; }
    if (language == "kotlin") { return "(if (" + child(0) + ") " + child(1) + " else " + child(2) + ")"; }
    if (language == "go") { return "func() " + (node.resolved_type && context.native_type ? context.native_type(*node.resolved_type) : std::string{node.result_type == "bool" ? "bool" : "float64"}) + " { if " + child(0) + " { return " + child(1) + " }; return " + child(2) + " }()"; }
    return "(" + child(0) + " ? " + child(1) + " : " + child(2) + ")";
  case operation::less:
  case operation::less_equal:
  case operation::greater:
  case operation::greater_equal:
  case operation::equal:
  case operation::not_equal: {
    const auto token = node.kind == operation::less ? " < " : node.kind == operation::less_equal ? " <= " : node.kind == operation::greater ? " > " : node.kind == operation::greater_equal ? " >= " : node.kind == operation::equal ? " == " : " != ";
    return "(" + child(0) + token + child(1) + ")";
  }
  case operation::add:
  case operation::subtract:
  case operation::multiply:
    return checked(child(0) +
        (node.kind == operation::add ? " + " : node.kind == operation::subtract ? " - " : " * ") +
        child(1));
  default: break;
  }
  throw std::invalid_argument{"Unsupported portable behavior expression"};
}

// Checked helper calls preserve branch laziness and reject invalid numeric intermediate values.
inline std::string helpers_base(std::string_view language, const checked_helpers& names = {}) {
  if (names != checked_helpers{}) {
    auto source = helpers_base(language);
    const std::array<std::pair<std::string_view, std::string>, 3> replacements{{
        {"srl_behavior_checked", names.checked}, {"srl_behavior_divide", names.divide},
        {"srl_behavior_sqrt", names.square_root}}};
    for (const auto& [before, after] : replacements) {
      std::size_t offset{};
      while ((offset = source.find(before, offset)) != std::string::npos) {
        source.replace(offset, before.size(), after);
        offset += after.size();
      }
    }
    if (language == "c") {
      const std::string_view original_guard{"SRL_C_BEHAVIOR_HELPERS_INCLUDED"};
      const auto guard = std::string{original_guard} + "_" + names.checked;
      std::size_t offset{};
      while ((offset = source.find(original_guard, offset)) != std::string::npos) {
        source.replace(offset, original_guard.size(), guard);
        offset += guard.size();
      }
    }
    return source;
  }
  if (language == "cpp") { return R"CODE(
#ifndef SERIALIZER_CPP_BEHAVIOR_HELPERS_INCLUDED
#define SERIALIZER_CPP_BEHAVIOR_HELPERS_INCLUDED
namespace rohit::serializer::detail {
// Reject invalid intermediate values before a comparison can conceal arithmetic failure.
constexpr double srl_behavior_checked(double value) {
  constexpr auto maximum = ::std::numeric_limits<double>::max();
  const bool finite = ::std::is_constant_evaluated() ?
      value >= -maximum && value <= maximum : ::std::isfinite(value);
  if (!finite) { throw ::std::domain_error{"Non-finite portable expression"}; }
  return value;
}
// Both signed zero divisors are outside the portable division domain.
constexpr double srl_behavior_divide(double left, double right) {
  if (right == 0.0) { throw ::std::domain_error{"Portable division by zero"}; }
  return srl_behavior_checked(left / right);
}
// Square root accepts finite nonnegative inputs, including negative zero.
inline double srl_behavior_sqrt(double value) {
  if (value < 0.0) { throw ::std::domain_error{"Portable square root requires nonnegative input"}; }
  return srl_behavior_checked(::std::sqrt(value));
}
} // namespace rohit::serializer::detail
#endif
)CODE"; }
  if (language == "c") { return R"CODE(
#ifndef SRL_C_BEHAVIOR_HELPERS_INCLUDED
#define SRL_C_BEHAVIOR_HELPERS_INCLUDED
/* Retain any numeric failure even when a surrounding comparison produces a boolean. */
static inline double srl_behavior_checked(double value, bool* valid) {
  if (!isfinite(value)) { *valid = false; }
  return value;
}
/* Division rejects both signed zero divisors without publishing a result. */
static inline double srl_behavior_divide(double left, double right, bool* valid) {
  if (right == 0.0) { *valid = false; return NAN; }
  return srl_behavior_checked(left / right, valid);
}
/* Negative square roots fail while negative zero remains a valid input. */
static inline double srl_behavior_sqrt(double value, bool* valid) {
  if (value < 0.0) { *valid = false; return NAN; }
  return srl_behavior_checked(sqrt(value), valid);
}
#endif
)CODE"; }
  if (language == "python") { return R"CODE(
def srl_behavior_checked(value):
    """Reject non-finite numeric intermediates before surrounding boolean comparisons."""
    if type(value) not in (int, float):
        raise ValueError('Portable expression requires a numeric scalar')
    try:
        value = float(value)
    except OverflowError as error:
        raise ValueError('Non-finite portable expression') from error
    if not _math.isfinite(value):
        raise ValueError('Non-finite portable expression')
    return value

def srl_behavior_divide(left, right):
    """Reject both signed zero divisors using the portable arithmetic failure contract."""
    if right == 0.0:
        raise ValueError('Portable division by zero')
    return srl_behavior_checked(left / right)

def srl_behavior_sqrt(value):
    """Accept nonnegative finite input, including negative zero."""
    if value < 0.0:
        raise ValueError('Portable square root requires nonnegative input')
    return srl_behavior_checked(_math.sqrt(value))
)CODE"; }
  if (language == "rust") { return R"CODE(
// Reject invalid numeric intermediates before a comparison can conceal arithmetic failure.
fn srl_behavior_checked(value: f64) -> f64 { assert!(value.is_finite(), "Non-finite portable expression"); value }
// Division excludes both signed zero divisors.
fn srl_behavior_divide(left: f64, right: f64) -> f64 { assert!(right != 0.0, "Portable division by zero"); srl_behavior_checked(left / right) }
// Square root includes negative zero and excludes negative finite inputs.
fn srl_behavior_sqrt(value: f64) -> f64 { assert!(value >= 0.0, "Portable square root requires nonnegative input"); srl_behavior_checked(value.sqrt()) }
)CODE"; }
  if (language == "go") { return R"CODE(
// Reject invalid numeric intermediates before surrounding boolean comparisons.
func srl_behavior_checked(value float64) float64 { if math.IsNaN(value) || math.IsInf(value, 0) { panic("Non-finite portable expression") }; return value }
// Division excludes both signed zero divisors.
func srl_behavior_divide(left, right float64) float64 { if right == 0 { panic("Portable division by zero") }; return srl_behavior_checked(left / right) }
// Square root accepts nonnegative inputs including negative zero.
func srl_behavior_sqrt(value float64) float64 { if value < 0 { panic("Portable square root requires nonnegative input") }; return srl_behavior_checked(math.Sqrt(value)) }
)CODE"; }
  if (language == "swift") { return R"CODE(
// Reject non-finite numeric intermediates before surrounding boolean comparisons.
private func srl_behavior_checked(_ value: Double) throws -> Double {
  if !value.isFinite { throw SerializerError.invalid("Non-finite portable expression") }
  return value
}
// Division excludes both signed zero divisors.
private func srl_behavior_divide(_ left: Double, _ right: Double) throws -> Double {
  if right == 0 { throw SerializerError.invalid("Portable division by zero") }
  return try srl_behavior_checked(left / right)
}
// Square root accepts nonnegative inputs, including negative zero.
private func srl_behavior_sqrt(_ value: Double) throws -> Double {
  if value < 0 { throw SerializerError.invalid("Portable square root requires nonnegative input") }
  return try srl_behavior_checked(sqrt(value))
}
)CODE"; }
  if (language == "kotlin") { return R"CODE(
// Reject invalid intermediates before surrounding boolean comparisons.
private fun srl_behavior_checked(value: Double): Double {
  if (!value.isFinite()) { throw SerializerException("Non-finite portable expression") }
  return value
}
// Division excludes both signed zero divisors.
private fun srl_behavior_divide(left: Double, right: Double): Double {
  if (right == 0.0) { throw SerializerException("Portable division by zero") }
  return srl_behavior_checked(left / right)
}
// Square root accepts nonnegative inputs, including negative zero.
private fun srl_behavior_sqrt(value: Double): Double {
  if (value < 0.0) { throw SerializerException("Portable square root requires nonnegative input") }
  return srl_behavior_checked(kotlin.math.sqrt(value))
}
)CODE"; }
  if (language == "java" || language == "csharp") {
    const auto real = language == "java" ? "Double" : "double";
    const auto finite_name = language == "java" ? ".isFinite" : ".IsFinite";
    const auto error = language == "java" ? "ArithmeticException" : "System.ArithmeticException";
    const auto sqrt_name = language == "java" ? "Math.sqrt" : "System.Math.Sqrt";
    return "\n// Reject invalid intermediates before surrounding boolean comparisons.\nprivate static double srl_behavior_checked(double value) { if (!" + std::string{real} + finite_name + "(value)) { throw new " + error + "(\"Non-finite portable expression\"); } return value; }\n" +
      "// Division excludes both signed zero divisors.\nprivate static double srl_behavior_divide(double left, double right) { if (right == 0.0) { throw new " + error + "(\"Portable division by zero\"); } return srl_behavior_checked(left / right); }\n" +
      "// Square root accepts nonnegative inputs, including negative zero.\nprivate static double srl_behavior_sqrt(double value) { if (value < 0.0) { throw new " + error + "(\"Portable square root requires nonnegative input\"); } return srl_behavior_checked(" + sqrt_name + "(value)); }\n";
  }
  if (language == "js") { return R"CODE(
// Reject invalid numeric intermediates before surrounding boolean comparisons.
function srl_behavior_checked(value) { if (!Number.isFinite(value)) { throw new RangeError('Non-finite portable expression'); } return value; }
// Division excludes both signed zero divisors.
function srl_behavior_divide(left, right) { if (right === 0) { throw new RangeError('Portable division by zero'); } return srl_behavior_checked(left / right); }
// Square root accepts nonnegative inputs, including negative zero.
function srl_behavior_sqrt(value) { if (value < 0) { throw new RangeError('Portable square root requires nonnegative input'); } return srl_behavior_checked(Math.sqrt(value)); }
)CODE"; }
  return {};
}

// Include exact numeric support with the existing private helper-name collision policy.
inline std::string helpers(std::string_view language, const checked_helpers& names = {}) {
  return helpers_base(language, names) + numeric_helpers(language, names);
}

// Swift requires try only when an evaluated subtree contains a throwing numeric helper.
inline bool expression_can_fail(const behavior_expression& node, bool predicate = false, bool unchecked_leaf = false) {
  using operation = behavior_expression::operation;
  switch (node.kind) {
  case operation::add:
  case operation::subtract:
  case operation::multiply:
  case operation::divide:
  case operation::square_root:
  case operation::sine:
  case operation::cosine: return true;
  case operation::checked_cast: return true;
  case operation::positive:
  case operation::negative:
    if (node.numeric_contract_enabled && node.result_type != "double") { return true; }
    return std::any_of(node.operands.begin(), node.operands.end(), [&](const auto& child) {
      return expression_can_fail(child, predicate, unchecked_leaf);
    });
  case operation::call: {
    const auto& method = called_function(node);
    if (method.expression && !method.predicate_body) { return true; }
    if (!unchecked_leaf && (predicate || node.result_type == "float") &&
        (node.result_type == "float" || node.result_type == "double")) { return true; }
    return std::any_of(node.operands.begin(), node.operands.end(), [&](const auto& child) {
      return expression_can_fail(child, predicate, unchecked_leaf);
    });
  }
  case operation::index_access:
  case operation::all:
  case operation::any: return true;
  case operation::is_finite:
    return std::any_of(node.operands.begin(), node.operands.end(), [&](const auto& child) { return expression_can_fail(child, predicate, true); });
  case operation::symbol:
  case operation::member_access:
    if (!unchecked_leaf && (predicate || node.local_symbol || node.kind == operation::member_access) &&
        (node.result_type == "double" || node.result_type == "float")) { return true; }
    [[fallthrough]];
  default:
    return std::any_of(node.operands.begin(), node.operands.end(), [&](const auto& child) { return expression_can_fail(child, predicate, unchecked_leaf); });
  }
}

// Detect reductions that share one bounded visit budget across nested evaluations.
inline bool expression_has_reduction(const behavior_expression& node) {
  using operation = behavior_expression::operation;
  return node.kind == operation::all || node.kind == operation::any ||
      std::any_of(node.operands.begin(), node.operands.end(), expression_has_reduction);
}

// Emit the shared checked body of a standalone function using the selected language's scopes.
inline void free_expression_body(const function_declaration& method, std::string_view language,
    const std::string& result_type, const std::function<std::string(std::string_view)>& symbol,
    const checked_helpers& helpers, expression_context context,
    const std::function<void(const std::string&)>& line,
    const std::function<void(const std::string&)>& open, const std::function<void()>& close) {
  const bool predicate = method.predicate_body;
  context.predicate = predicate;
  while (std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& parameter) {
    return symbol(parameter.name) == context.valid_name || symbol(parameter.name) == context.budget_name;
  })) { context.valid_name += '_'; context.budget_name += '_'; }
  const auto suffix = language == "python" || language == "go" || language == "swift" || language == "kotlin" ? "" : ";";
  const auto statement = [&](const std::string& text) { line(text + suffix); };
  if (predicate && language == "rust") {
    statement("let mut " + context.valid_name + " = true");
    statement("let _ = &mut " + context.valid_name);
  }
  if (expression_has_reduction(*method.expression)) {
    const auto budget = std::to_string(behavior_expression::maximum_reduction_visits);
    statement(language == "python" ? context.budget_name + " = [" + budget + "]" :
        language == "java" ? "long[] " + context.budget_name + " = {" + budget + "}" :
        language == "rust" ? "let mut " + context.budget_name + ": usize = " + budget :
        language == "go" ? context.budget_name + " := " + budget :
        language == "swift" || language == "kotlin" ? "var " + context.budget_name + " = " + budget :
        language == "csharp" ? "int " + context.budget_name + " = " + budget :
        "let " + context.budget_name + " = " + budget);
  }
  if (!predicate) {
    std::set<std::string> inputs;
    const auto collect = [&](const behavior_expression& node, const auto& self) -> void {
      if (node.kind == behavior_expression::operation::is_finite) { return; }
      if (node.kind == behavior_expression::operation::symbol && !node.local_symbol &&
          (node.result_type == "float" || node.result_type == "double")) { inputs.insert(node.value); }
      for (const auto& operand : node.operands) { self(operand, self); }
    };
    collect(*method.expression, collect);
    for (const auto& input : inputs) {
      const auto value = symbol(input);
      const auto parameter = std::find_if(method.parameters.begin(), method.parameters.end(),
          [&](const auto& argument) { return argument.name == input; });
      const bool single_precision = parameter != method.parameters.end() && parameter->type.name == "float";
      statement(language == "python" ? helpers.checked + "(" + value + ")" :
          language == "rust" ? "assert!((" + value + " as f64).is_finite(), \"Non-finite portable input\")" :
          language == "go" ? helpers.checked + "(float64(" + value + "))" :
          language == "swift" ? "_ = try " + helpers.checked + "(Double(" + value + "))" :
          language == "kotlin" ? helpers.checked + "(" + (single_precision ? "(" + value + ").toDouble()" : value) + ")" :
          helpers.checked + "(" + value + ")");
    }
  }
  if (predicate && language == "go") {
    line("defer func() { if recover() != nil { srlPredicateResult = false } }()");
  }
  const bool catches = predicate && language != "rust" && language != "go";
  if (catches) { open(language == "swift" ? "do" : "try"); }
  auto value = expression(*method.expression, language, symbol, helpers, context);
  if (language == "rust") { value = ungroup_expression(std::move(value)); }
  const std::string result = language == "python" ? "_srl_result" : language == "rust" ? "srl_behavior_result" : "srlBehaviorResult";
  statement((language == "python" ? std::string{} : language == "rust" ? "let " :
      language == "go" ? "" : language == "swift" ? "let " : language == "kotlin" ? "val " :
      language == "js" ? "const " : result_type + " ") + result +
      (language == "rust" ? ": " + result_type : "") +
      (language == "go" ? " := " : " = ") +
      (language == "swift" && expression_can_fail(*method.expression, predicate) ? "try " : "") + value);
  const bool floating = method.return_type.name == "float" || method.return_type.name == "double";
  if (floating) {
    statement(language == "rust" ? "assert!(" + result + ".is_finite(), \"Non-finite portable result\")" :
        language == "swift" ? "_ = try " + helpers.checked + "(Double(" + result + "))" :
        language == "kotlin" ? helpers.checked + "(" + (method.return_type.name == "float" ? "(" + result + ").toDouble()" : result) + ")" :
        language == "go" ? helpers.checked + "(float64(" + result + "))" : helpers.checked + "(" + result + ")");
  }
  if (language == "rust") { line((predicate ? context.valid_name + " && " : "") + result); }
  else { statement("return " + result); }
  if (catches) {
    close();
    open(language == "python" ? "except (ArithmeticError, ValueError, TypeError, AttributeError, IndexError)" :
          language == "java" ? "catch (RuntimeException srlFailure)" :
          language == "csharp" ? "catch (System.Exception srlFailure) when (srlFailure is System.ArithmeticException || srlFailure is System.NullReferenceException || srlFailure is System.ArgumentException || srlFailure is System.IndexOutOfRangeException)" :
          language == "swift" ? "catch" : language == "kotlin" ? "catch (_: SerializerException)" : "catch (srlFailure)");
    statement(language == "python" ? "return False" : "return false"); close();
  }
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
  if (node.kind == behavior_expression::operation::is_finite) { return; }
  if (node.kind == behavior_expression::operation::symbol && !node.local_symbol) { result.insert(node.value); }
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

// Reject overloads that lose parameter identity or collide after target naming conversion.
inline void require_unique_free_names(const std::vector<const syntax_node*>& nodes,
    std::string_view language, const std::function<std::string(const syntax_node&)>& spelling,
    const std::function<std::string(const type_name&)>& parameter_type = {}) {
  std::set<std::string> names;
  for (const auto* node : nodes) {
    if (node->type != object_type::function_type) { continue; }
    auto key = spelling(*node);
    if (parameter_type) {
      key += "(";
      for (const auto& parameter : static_cast<const function_node&>(*node).function.parameters) {
        key += parameter_type(parameter.type) + ",";
      }
      key += ")";
    }
    if (!names.insert(key).second) {
      throw std::invalid_argument{std::string{language} + ": standalone overload/name erasure collision: " + key};
    }
  }
}

// Dispatch contracts are emitted as separate adapters; owning codec data stays concrete.
inline void require_ordinary_dispatch(const function_declaration&, std::string_view) {}

} // namespace rohit::serializer::writer::behavior

#include "typed_expression_writer.hpp"
#include "numeric_expression_writer.hpp"
