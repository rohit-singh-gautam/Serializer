// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/constant_binary_output.hpp>
#include <rohit/serializer.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace rohit::serializer {
namespace detail {
// Use the unchanged optimized runtime protocol for existing-memory output.
template <class Value>
std::size_t serialize_constant_binary_runtime(std::span<std::uint8_t> destination,
                                              const Value& value) {
  rohit::full_stream output{destination.data(), destination.size()};
  binary_none<serialize_type::out> encoder{output};
  encoder.serialize_out(value);
  return output.current_offset();
}

} // namespace detail

// Preserve optimized owning runtime output; constant evaluation uses the shared memory traversal.
template <class Value>
  requires(!detail::has_emission_binary_support<Value>() && !detail::constant_binary_span<Value>)
[[nodiscard]] constexpr std::size_t serialize_binary_none_to(std::span<std::uint8_t> destination,
                                                             const Value& value) {
  if (!std::is_constant_evaluated()) {
    return detail::serialize_constant_binary_runtime(destination, value);
  }
  return detail::serialize_constant_binary_to(destination, value);
}

} // namespace rohit::serializer
