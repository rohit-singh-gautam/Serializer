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

#include <rohit/binary_scalar.hpp>
#include <rohit/versioning.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace rohit::serializer {
enum class serialize_key_type { none, integer, string };

// Unchecked binary text requires the caller to guarantee valid UTF-8 at the boundary.
enum class binary_text_validation { strict, unchecked };

namespace constants {
// The first byte stores a two-bit length tag and six payload bits.
inline constexpr std::uint32_t variable_tag_mask = 0xc0U;
inline constexpr unsigned variable_length_tag_shift = 6;
inline constexpr std::uint32_t variable_payload_mask = 0x3fU;
inline constexpr std::uint32_t variable_two_byte_tag = 0x40U;
inline constexpr std::uint32_t variable_three_byte_tag = 0x80U;
inline constexpr std::uint32_t variable_four_byte_tag = 0xc0U;
// Keep threshold types consistent with the original integral comparisons.
inline constexpr int variable_one_byte_max = 0x3f;
inline constexpr int variable_two_byte_max = 0x3fff;
inline constexpr int variable_three_byte_max = 0x3fffff;
inline constexpr int variable_four_byte_max = 0x3fffffff;
// Integer-key objects end with ID zero; string-key objects use a zero-length name.
inline constexpr std::uint32_t binary_object_end_id = 0;
inline constexpr std::uint32_t wire_byte_mask = 0xffU;
inline constexpr unsigned wire_byte_bits = 8;
// Unsigned LEB128 uses seven payload bits and one continuation bit per byte.
inline constexpr unsigned varint_payload_bits = 7;
inline constexpr std::uint8_t varint_payload_mask = 0x7f;
inline constexpr std::uint8_t varint_continuation_mask = 0x80;
inline constexpr int decimal_radix = 10;
inline constexpr std::string_view map_key_name = "key";
inline constexpr std::string_view map_value_name = "value";
// Bound generated unrolling and scalar snapshots; longer runs are split into separate batches.
inline constexpr std::size_t maximum_fixed_batch_fields = 16;
static_assert(variable_payload_mask == variable_one_byte_max);
static_assert((variable_tag_mask & variable_payload_mask) == 0);
} // namespace constants

namespace detail {
// Keep public scalar storage and JSON values independent of the native binary encoding.
template <bool Prefix, bool Strict, std::unsigned_integral T> struct compact_output {
  T value;

  // Select the binary compact hook; other protocols retain their ordinary scalar mapping.
  template <typename Protocol> constexpr void serialize_out(Protocol& protocol) const {
    if constexpr (requires { protocol.template serialize_out_compact<Prefix, Strict>(value); }) {
      protocol.template serialize_out_compact<Prefix, Strict>(value);
    } else {
      protocol.serialize_out(value);
    }
  }
};

// Only padding-free fixed-width scalars can share bulk binary input/output paths.
template <typename T>
concept binary_array_scalar =
    (rohit::detail::endian_integer<T> || rohit::detail::endian_floating_point<T>) &&
    (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

// Booleans occupy one validated wire byte; all other batch members have padding-free scalar bits.
template <typename T>
concept binary_fixed_scalar = std::same_as<T, bool> || binary_array_scalar<T>;

template <binary_fixed_scalar T>
inline constexpr std::size_t binary_fixed_bytes = std::same_as<T, bool> ? 1 : sizeof(T);

// Return the canonical prefix width for an already validated 30-bit value.
constexpr std::size_t fixed_prefix_bytes(std::uint32_t value) noexcept {
  return value <= constants::variable_one_byte_max     ? 1
         : value <= constants::variable_two_byte_max   ? 2
         : value <= constants::variable_three_byte_max ? 3
                                                       : 4;
}

static_assert(constants::maximum_fixed_batch_fields <=
              rohit::detail::maximum_buffer_bytes /
                  (sizeof(std::uint64_t) + fixed_prefix_bytes(constants::variable_four_byte_max)));

// Encode an already validated compact ID or length into a reserved span.
constexpr void write_fixed_prefix(std::uint8_t*& output, std::uint32_t value) noexcept {
  const auto following_bytes = fixed_prefix_bytes(value) - 1;
  *output++ = static_cast<std::uint8_t>((value >> (following_bytes * constants::wire_byte_bits)) |
                                        (following_bytes << constants::variable_length_tag_shift));
  for (auto remaining = following_bytes; remaining != 0; --remaining) {
    *output++ = static_cast<std::uint8_t>(value >> ((remaining - 1) * constants::wire_byte_bits));
  }
}

// Keep named-enum lookup visible to the shared typed-magic template; the runtime header defines it.
template <typename T> constexpr std::string_view enum_name(T value);

// Emit a typed header through the same scalar or named-enum codec as payload fields.
template <typename Protocol, typename T>
constexpr void write_typed_magic(Protocol& protocol, const T& expected) {
  if constexpr (std::is_enum_v<T> && Protocol::key_type == decltype(Protocol::key_type)::string) {
    protocol.serialize_out(enum_name(expected));
  } else {
    protocol.serialize_out(expected);
  }
}

// Evaluate schema dimension arithmetic at compile time without unsigned wraparound.
consteval std::uint64_t dimension_add(std::uint64_t left, std::uint64_t right) {
  if (right > std::numeric_limits<std::uint64_t>::max() - left) {
    throw "Dimension addition overflow";
  }
  return left + right;
}

// Reject intermediate overflow before multiplying a dependent fixed-array extent.
consteval std::uint64_t dimension_multiply(std::uint64_t left, std::uint64_t right) {
  if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
    throw "Dimension multiplication overflow";
  }
  return left * right;
}

// Bound native template storage before instantiating std::array.
consteval std::size_t fixed_extent(std::uint64_t value) {
  constexpr std::uint64_t maximum_fixed_elements = 65536;
  if (value == 0 || value > maximum_fixed_elements || value > std::numeric_limits<std::size_t>::max()) {
    throw "Invalid or excessive fixed array extent (1..65536)";
  }
  return static_cast<std::size_t>(value);
}

} // namespace detail

namespace type_check {
template <typename T, typename J>
concept serializer_out_enabled_ptr = (!requires(T cls, J& serialize_protocol) {
                                       {
                                         cls.template serialize_out<J>(serialize_protocol)
                                       } -> std::same_as<void>;
                                     }) && requires(T cls, J& serialize_protocol) {
  { cls->template serialize_out<J>(serialize_protocol) } -> std::same_as<void>;
};

template <typename T, typename J>
concept serializer_out_enabled = requires(T cls, J& serialize_protocol) {
  { cls.template serialize_out<J>(serialize_protocol) } -> std::same_as<void>;
};

template <typename T>
concept vector = requires(T t) {
  typename T::value_type;
  requires std::is_same_v<T, std::vector<typename T::value_type>>;
};

template <typename T>
concept fixed_array = requires {
  typename T::value_type;
  requires std::is_same_v<T, std::array<typename T::value_type, std::tuple_size<T>::value>>;
};

template <typename T>
concept map = requires(T t) {
  typename T::key_type;
  typename T::mapped_type;
  requires std::is_same_v<T, std::map<typename T::key_type, typename T::mapped_type>>;
};

} // namespace type_check
} // namespace rohit::serializer
