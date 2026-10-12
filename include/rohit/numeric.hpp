// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace rohit::serializer {

// A numeric representation excludes Boolean state and accepts ordinary arithmetic values.
template <typename T>
concept numeric_representation = (std::integral<T> && !std::same_as<T, bool>) || std::floating_point<T>;

namespace detail {
// Check finiteness without depending on C++23 constexpr math-library support.
template <std::floating_point T>
constexpr bool numeric_finite(T value) noexcept {
  return value >= -std::numeric_limits<T>::max() && value <= std::numeric_limits<T>::max();
}

// Obtain the full unsigned magnitude without negating a signed minimum.
template <std::integral T>
constexpr std::make_unsigned_t<T> numeric_magnitude(T value) noexcept {
  using UInt = std::make_unsigned_t<T>;
  const auto bits = static_cast<UInt>(value);
  if constexpr (std::signed_integral<T>) {
    return value < 0 ? UInt{0} - bits : bits;
  } else {
    return bits;
  }
}

// Calculate an exact power-of-two endpoint used before a floating-to-integral cast.
template <std::floating_point T>
constexpr T numeric_power_of_two(unsigned exponent) noexcept {
  T value{1};
  for (unsigned index{}; index < exponent; ++index) { value *= T{2}; }
  return value;
}
} // namespace detail

// Return an exact conversion or nullopt without evaluating an out-of-range cast.
// NaN and infinity are always rejected; no allocation or exception occurs.
template <numeric_representation To, numeric_representation From>
[[nodiscard]] constexpr std::optional<To> try_numeric_cast(From value) noexcept {
  if constexpr (std::integral<To> && std::integral<From>) {
    if constexpr (std::signed_integral<From>) {
      if (value < 0) {
        if constexpr (std::unsigned_integral<To>) {
          return std::nullopt;
        } else if constexpr (std::numeric_limits<To>::digits < std::numeric_limits<From>::digits) {
          if (value < static_cast<From>(std::numeric_limits<To>::min())) {
            return std::nullopt;
          }
        }
        return static_cast<To>(value);
      }
    }
    const auto magnitude = detail::numeric_magnitude(value);
    if constexpr (std::numeric_limits<To>::digits < std::numeric_limits<From>::digits ||
        (std::signed_integral<To> && std::unsigned_integral<From>)) {
      if (magnitude > static_cast<std::make_unsigned_t<From>>(std::numeric_limits<To>::max())) {
        return std::nullopt;
      }
    }
    return static_cast<To>(value);
  } else if constexpr (std::floating_point<To> && std::integral<From>) {
    static_assert(std::numeric_limits<To>::radix == 2, "Exact numeric binding requires binary floating point");
    const auto magnitude = detail::numeric_magnitude(value);
    const auto width = std::bit_width(magnitude);
    if (width > std::numeric_limits<To>::digits &&
        std::countr_zero(magnitude) < width - std::numeric_limits<To>::digits) {
      return std::nullopt;
    }
    const auto result = static_cast<To>(value);
    if (!detail::numeric_finite(result)) {
      return std::nullopt;
    }
    return result;
  } else if constexpr (std::integral<To> && std::floating_point<From>) {
    const auto upper = detail::numeric_power_of_two<From>(std::numeric_limits<To>::digits);
    const auto lower = std::signed_integral<To> ? -upper : From{0};
    if (!detail::numeric_finite(value) || value < lower || value >= upper) {
      return std::nullopt;
    }
    const auto result = static_cast<To>(value);
    if (static_cast<From>(result) != value) {
      return std::nullopt;
    }
    return result;
  } else {
    if (!detail::numeric_finite(value)) {
      return std::nullopt;
    }
    if constexpr (std::numeric_limits<To>::max_exponent < std::numeric_limits<From>::max_exponent) {
      if (value < -static_cast<From>(std::numeric_limits<To>::max()) ||
          value > static_cast<From>(std::numeric_limits<To>::max())) {
        return std::nullopt;
      }
    }
    const auto result = static_cast<To>(value);
    if (!detail::numeric_finite(result) || static_cast<From>(result) != value) {
      return std::nullopt;
    }
    return result;
  }
}

// Add matching representations without integral overflow or nonfinite floating results.
template <numeric_representation T>
[[nodiscard]] constexpr std::optional<T> try_checked_add(T left, T right) noexcept {
  if constexpr (std::integral<T>) {
    if constexpr (std::unsigned_integral<T>) {
      if (left > std::numeric_limits<T>::max() - right) {
        return std::nullopt;
      }
    } else if ((right > 0 && left > std::numeric_limits<T>::max() - right) ||
               (right < 0 && left < std::numeric_limits<T>::min() - right)) {
      return std::nullopt;
    }
    return static_cast<T>(left + right);
  } else {
    if (!try_numeric_cast<T>(left) || !try_numeric_cast<T>(right)) { return std::nullopt; }
    return try_numeric_cast<T>(static_cast<T>(left + right));
  }
}

// Subtract matching representations after checking both integral endpoints.
template <numeric_representation T>
[[nodiscard]] constexpr std::optional<T> try_checked_subtract(T left, T right) noexcept {
  if constexpr (std::integral<T>) {
    if constexpr (std::unsigned_integral<T>) {
      if (left < right) { return std::nullopt; }
    } else if ((right > 0 && left < std::numeric_limits<T>::min() + right) ||
               (right < 0 && left > std::numeric_limits<T>::max() + right)) {
      return std::nullopt;
    }
    return static_cast<T>(left - right);
  } else {
    if (!try_numeric_cast<T>(left) || !try_numeric_cast<T>(right)) { return std::nullopt; }
    return try_numeric_cast<T>(static_cast<T>(left - right));
  }
}

// Multiply after division-based bounds checks; no overflowing intermediate is evaluated.
template <numeric_representation T>
[[nodiscard]] constexpr std::optional<T> try_checked_multiply(T left, T right) noexcept {
  if constexpr (std::integral<T>) {
    bool overflow{};
    if constexpr (std::unsigned_integral<T>) {
      overflow = right != 0 && left > std::numeric_limits<T>::max() / right;
    } else if (left > 0) {
      overflow = right > 0 ? left > std::numeric_limits<T>::max() / right :
          right < std::numeric_limits<T>::min() / left;
    } else if (left < 0) {
      overflow = right > 0 ? left < std::numeric_limits<T>::min() / right :
          right < 0 && left < std::numeric_limits<T>::max() / right;
    }
    if (overflow) { return std::nullopt; }
    return static_cast<T>(left * right);
  } else {
    if (!try_numeric_cast<T>(left) || !try_numeric_cast<T>(right)) { return std::nullopt; }
    return try_numeric_cast<T>(static_cast<T>(left * right));
  }
}

// Divide matching representations; integral results must be exact and divisors nonzero.
template <numeric_representation T>
[[nodiscard]] constexpr std::optional<T> try_checked_divide(T left, T right) noexcept {
  if (right == T{0}) { return std::nullopt; }
  if constexpr (std::integral<T>) {
    if constexpr (std::signed_integral<T>) {
      if (left == std::numeric_limits<T>::min() && right == T{-1}) {
        return std::nullopt;
      }
    }
    if (left % right != 0) { return std::nullopt; }
    return static_cast<T>(left / right);
  } else {
    if (!try_numeric_cast<T>(left) || !try_numeric_cast<T>(right)) { return std::nullopt; }
    return try_numeric_cast<T>(static_cast<T>(left / right));
  }
}

// Negate while rejecting signed minimum and nonzero unsigned values.
template <numeric_representation T>
[[nodiscard]] constexpr std::optional<T> try_checked_negate(T value) noexcept {
  if constexpr (std::integral<T>) {
    if constexpr (std::unsigned_integral<T>) {
      if (value != 0) { return std::nullopt; }
      return T{};
    } else {
      if (value == std::numeric_limits<T>::min()) { return std::nullopt; }
      return static_cast<T>(-value);
    }
  } else {
    const auto valid = try_numeric_cast<T>(value);
    return valid ? std::optional<T>{-value} : std::nullopt;
  }
}

// Convert exactly or report an invalid numeric value with std::domain_error.
template <numeric_representation To, numeric_representation From>
[[nodiscard]] constexpr To checked_numeric_cast(From value) {
  const auto result = try_numeric_cast<To>(value);
  if (!result) { throw std::domain_error{"Numeric value is not exactly representable"}; }
  return *result;
}

// Add matching representations or report overflow/nonfinite results.
template <numeric_representation T>
[[nodiscard]] constexpr T checked_add(T left, T right) {
  const auto result = try_checked_add(left, right);
  if (!result) { throw std::domain_error{"Invalid checked numeric add"}; }
  return *result;
}

// Subtract matching representations or report overflow/nonfinite results.
template <numeric_representation T>
[[nodiscard]] constexpr T checked_subtract(T left, T right) {
  const auto result = try_checked_subtract(left, right);
  if (!result) { throw std::domain_error{"Invalid checked numeric subtract"}; }
  return *result;
}

// Multiply matching representations or report overflow/nonfinite results.
template <numeric_representation T>
[[nodiscard]] constexpr T checked_multiply(T left, T right) {
  const auto result = try_checked_multiply(left, right);
  if (!result) { throw std::domain_error{"Invalid checked numeric multiply"}; }
  return *result;
}

// Divide exactly for integral values or report an invalid divisor/result.
template <numeric_representation T>
[[nodiscard]] constexpr T checked_divide(T left, T right) {
  const auto result = try_checked_divide(left, right);
  if (!result) { throw std::domain_error{"Invalid checked numeric divide"}; }
  return *result;
}

// Negate the original representation or report an unrepresentable result.
template <numeric_representation T>
[[nodiscard]] constexpr T checked_negate(T value) {
  const auto result = try_checked_negate(value);
  if (!result) { throw std::domain_error{"Invalid checked numeric negate"}; }
  return *result;
}

} // namespace rohit::serializer
