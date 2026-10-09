// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <type_traits>

namespace rohit {
namespace detail {
inline constexpr std::size_t maximum_buffer_bytes =
    static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
template <typename> inline constexpr bool unsupported_type = false;

// Integral byte swapping requires every object bit to participate in its value representation.
template <typename T>
concept endian_integer = std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool> &&
                         (std::numeric_limits<T>::digits + (std::is_signed_v<T> ? 1 : 0) ==
                          sizeof(T) * std::numeric_limits<unsigned char>::digits);

// Limit floating-point conversion to the supported binary IEC 559 storage widths.
template <typename T>
concept endian_floating_point =
    std::floating_point<T> && std::numeric_limits<T>::is_iec559 &&
    std::numeric_limits<T>::radix == 2 &&
    (sizeof(T) == sizeof(std::uint32_t) || sizeof(T) == sizeof(std::uint64_t));

} // namespace detail

namespace exception {
class stream_overflow_exception : public std::exception {
public:
  // Return the exception message; the pointer remains valid for this exception lifetime.
  const char* what() const noexcept override { return "Stream Overflow"; }
}; // class stream_overflow_exception

} // namespace exception
} // namespace rohit
