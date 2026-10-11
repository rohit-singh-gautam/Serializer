// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "behavior_writer.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace rohit::serializer::writer::behavior {

// Emit nonthrowing numeric checks for predicates; validity stays false after a reached failure.
inline std::string helpers_predicate(std::string_view language, const checked_helpers& names = {}) {
  std::string source;
  if (language == "cpp") {
    source = R"CODE(
#ifndef SERIALIZER_CPP_PREDICATE_HELPERS_INCLUDED
#define SERIALIZER_CPP_PREDICATE_HELPERS_INCLUDED
namespace rohit::serializer::detail {
// Test binary64 finiteness with operations available in C++20 constant evaluation.
constexpr bool srl_behavior_checked_predicate_finite(double value) noexcept {
  constexpr auto maximum = ::std::numeric_limits<double>::max();
  return value >= -maximum && value <= maximum;
}
// Preserve the reached failure flag without throwing or evaluating invalid arithmetic further.
constexpr double srl_behavior_checked_predicate(double value, bool* valid) noexcept {
  if (!srl_behavior_checked_predicate_finite(value)) { *valid = false; return 0.0; }
  return value;
}
// Reject signed zero divisors before division; constant arithmetic follows C++20 rules.
constexpr double srl_behavior_divide_predicate(double left, double right, bool* valid) noexcept {
  if (!srl_behavior_checked_predicate_finite(left) ||
      !srl_behavior_checked_predicate_finite(right) || right == 0.0) {
    *valid = false; return 0.0;
  }
  return srl_behavior_checked_predicate(left / right, valid);
}
// Keep runtime square roots total for predicates; C++20 math is not promised constant evaluation.
inline double srl_behavior_sqrt_predicate(double value, bool* valid) noexcept {
  if (!srl_behavior_checked_predicate_finite(value) || value < 0.0) {
    *valid = false; return 0.0;
  }
  return srl_behavior_checked_predicate(::std::sqrt(value), valid);
}
} // namespace rohit::serializer::detail
#endif
)CODE";
  } else if (language == "rust") {
    source = R"CODE(
// Mark reached nonfinite arithmetic as invalid without invoking a panic hook.
fn srl_behavior_checked_predicate(value: f64, valid: &mut bool) -> f64 {
    if !value.is_finite() { *valid = false; return 0.0; }
    value
}
// Reject signed zero divisors before evaluating division.
fn srl_behavior_divide_predicate(left: f64, right: f64, valid: &mut bool) -> f64 {
    if !left.is_finite() || !right.is_finite() || right == 0.0 { *valid = false; return 0.0; }
    srl_behavior_checked_predicate(left / right, valid)
}
// Invalid square roots produce a false predicate rather than a language panic.
fn srl_behavior_sqrt_predicate(value: f64, valid: &mut bool) -> f64 {
    if !value.is_finite() || value < 0.0 { *valid = false; return 0.0; }
    srl_behavior_checked_predicate(value.sqrt(), valid)
}
)CODE";
  } else if (language == "python") {
    source = R"CODE(
def srl_behavior_checked_integer(value, minimum, maximum, enum_type=None):
    """Validate a reached exact integer or declared enum without converting through binary64."""
    if enum_type is not None:
        if type(value) is not enum_type:
            raise ValueError('Portable expression requires the declared enum type')
        value = int(value)
    elif type(value) is not int:
        raise ValueError('Portable expression requires an exact integer')
    if not minimum <= value <= maximum:
        raise ValueError('Portable integer is outside the declared type')
    return value

def srl_behavior_checked_boolean(value):
    """Reject a reached dynamically malformed boolean while retaining branch laziness."""
    if type(value) is not bool:
        raise ValueError('Portable expression requires a boolean')
    return value
)CODE";
  }
  const std::array<std::pair<std::string_view, std::string>, 3> replacements{{
      {"srl_behavior_checked", names.checked}, {"srl_behavior_divide", names.divide},
      {"srl_behavior_sqrt", names.square_root}}};
  for (const auto& [original, replacement] : replacements) {
    std::size_t offset{};
    while ((offset = source.find(original, offset)) != std::string::npos) {
      source.replace(offset, original.size(), replacement);
      offset += replacement.size();
    }
  }
  return source;
}

} // namespace rohit::serializer::writer::behavior
