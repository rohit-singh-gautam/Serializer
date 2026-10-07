//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

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
