// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once
#include "versioning.hpp"
#include <concepts>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <variant>
namespace rohit::managed {
template <typename Value> struct runtime_traits;
namespace detail {
// Add a conservative estimate without wrapping size arithmetic.
inline void add_memory_estimate(std::size_t& result, std::size_t amount) {
  if (amount > std::numeric_limits<std::size_t>::max() - result) {
    throw std::length_error{"Managed memory estimate overflow"};
  }
  result += amount;
}
// Check capacity multiplication before estimating owned storage.
inline std::size_t multiply_memory_estimate(std::size_t count, std::size_t bytes) {
  if (bytes != 0 && count > std::numeric_limits<std::size_t>::max() / bytes) {
    throw std::length_error{"Managed memory estimate overflow"};
  }
  return count * bytes;
}
// Count owned dynamic capacity; generated traits count inline sizeof separately.
// Map overhead is conservative for qualified libraries, not a portable ABI guarantee.
template <typename Value>
std::size_t estimate_dynamic_memory(const Value& value) {
  using type = std::remove_cvref_t<Value>;
  std::size_t result = 0;
  if constexpr (std::is_arithmetic_v<type> || std::is_enum_v<type> || std::is_pointer_v<type> ||
                std::same_as<type, ::rohit::serializer::version2> ||
                std::same_as<type, ::rohit::serializer::version3> ||
                std::same_as<type, ::rohit::serializer::version4>) {
    result = 0;
  } else if constexpr (requires { typename type::traits_type; value.capacity(); }) {
    result = multiply_memory_estimate(value.capacity(), sizeof(typename type::value_type));
    add_memory_estimate(result, sizeof(typename type::value_type));
  } else if constexpr (requires { typename type::mapped_type; value.size(); }) {
    constexpr auto node_overhead_bytes = 6 * sizeof(void*);
    result = multiply_memory_estimate(value.size(), sizeof(typename type::value_type) + node_overhead_bytes);
    if constexpr (requires { value.bucket_count(); }) {
      add_memory_estimate(result, multiply_memory_estimate(value.bucket_count(), sizeof(void*)));
    }
    for (const auto& [key, mapped] : value) {
      add_memory_estimate(result, estimate_dynamic_memory(key));
      add_memory_estimate(result, estimate_dynamic_memory(mapped));
    }
  } else if constexpr (requires { value.capacity(); typename type::value_type; }) {
    if constexpr (std::same_as<typename type::value_type, bool>) {
      result = value.capacity() / 8 + (value.capacity() % 8 != 0 ? 1 : 0);
    } else {
      result = multiply_memory_estimate(value.capacity(), sizeof(typename type::value_type));
      for (const auto& element : value) {
        add_memory_estimate(result, estimate_dynamic_memory(element));
      }
    }
  } else if constexpr (requires { value.valueless_by_exception(); value.index(); }) {
    if (value.valueless_by_exception()) {
      throw std::invalid_argument{"Cannot estimate a valueless managed variant"};
    }
    std::visit([&](const auto& active) { result = estimate_dynamic_memory(active); }, value);
  } else if constexpr (requires { value.data(); value.size(); typename type::value_type; }) {
    for (const auto& element : value) {
      add_memory_estimate(result, estimate_dynamic_memory(element));
    }
  } else if constexpr (requires { runtime_traits<type>::estimate_memory(value); }) {
    const auto total = runtime_traits<type>::estimate_memory(value);
    if (total < sizeof(type)) {
      throw std::invalid_argument{"Managed memory estimator omits inline storage"};
    }
    result = total - sizeof(type);
  } else {
    static_assert(std::is_arithmetic_v<type>, "Managed memory estimation requires explicit type traits");
  }
  return result;
}
} // namespace detail
} // namespace rohit::managed
