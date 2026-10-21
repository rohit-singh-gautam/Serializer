// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rohit::managed {
template <typename Value> struct model_traits;
template <typename Value> struct runtime_traits;
namespace detail {

// Visit one unchanged alternative/identity across three snapshots without a Cartesian variant dispatch.
// Switching tags or replacing the identified occurrence returns false for the caller's atomic policy.
template <typename Variant, typename Visitor>
bool visit_matching_owned_variant(Variant& current, const Variant& base, const Variant& desired,
                                   Visitor&& visitor) {
  if (current.valueless_by_exception() || base.valueless_by_exception() || desired.valueless_by_exception()) {
    throw std::invalid_argument{"Valueless managed variant"};
  }
  if (current.index() != base.index() || current.index() != desired.index()) { return false; }
  bool matched{};
  // Index dispatch also supports two schema alternatives with the same native payload type.
  const auto visit = [&](auto index) {
    constexpr auto selected = decltype(index)::value;
    if (current.index() != selected) { return; }
    auto& target = std::get<selected>(current);
    const auto& old = std::get<selected>(base);
    const auto& source = std::get<selected>(desired);
    if (target.persistent_id != old.persistent_id || target.persistent_id != source.persistent_id) { return; }
    std::forward<Visitor>(visitor)(target, old, source);
    matched = true;
  };
  [&]<std::size_t... Index>(std::index_sequence<Index...>) {
    (visit(std::integral_constant<std::size_t, Index>{}), ...);
  }(std::make_index_sequence<std::variant_size_v<Variant>>{});
  return matched;
}

// Rebind standard owning wrappers to the generated storage type at their identified leaves.
template <typename Plain>
struct owned_value_traits {
  using leaf_plain = Plain;
  using storage_type = typename model_traits<Plain>::storage_type;
  static constexpr bool is_entity = true;
};
template <typename Plain>
struct owned_value_traits<std::optional<Plain>> {
  using leaf_plain = typename owned_value_traits<Plain>::leaf_plain;
  using storage_type = std::optional<typename owned_value_traits<Plain>::storage_type>;
  static constexpr bool is_entity = false;
};
template <typename Plain, typename Allocator>
struct owned_value_traits<std::vector<Plain, Allocator>> {
  using leaf_plain = typename owned_value_traits<Plain>::leaf_plain;
  using storage_type = std::vector<typename owned_value_traits<Plain>::storage_type>;
  static constexpr bool is_entity = false;
};
template <typename Key, typename Plain, typename Compare, typename Allocator>
struct owned_value_traits<std::map<Key, Plain, Compare, Allocator>> {
  using leaf_plain = typename owned_value_traits<Plain>::leaf_plain;
  using storage_type = std::map<Key, typename owned_value_traits<Plain>::storage_type, Compare>;
  static constexpr bool is_entity = false;
};
template <typename Plain, std::size_t N>
struct owned_value_traits<std::array<Plain, N>> {
  using leaf_plain = typename owned_value_traits<Plain>::leaf_plain;
  using storage_type = std::array<typename owned_value_traits<Plain>::storage_type, N>;
  static constexpr bool is_entity = false;
};

// Visit only engaged independently identified descendants in arbitrarily nested owning wrappers.
template <typename Value, typename Visitor>
void visit_owned_children(Value& value, Visitor&& visitor) {
  if constexpr (requires { value.persistent_id; }) {
    visitor(value);
  } else if constexpr (requires { value.has_value(); value.value(); }) {
    if (value) { visit_owned_children(*value, visitor); }
  } else if constexpr (requires { typename std::remove_cvref_t<Value>::mapped_type; }) {
    for (auto& [key, child] : value) {
      static_cast<void>(key);
      visit_owned_children(child, visitor);
    }
  } else {
    for (auto& child : value) { visit_owned_children(child, visitor); }
  }
}

// Locate a live leaf by immutable identity without retaining a container pointer across edits.
template <typename Child, typename Value, typename Id>
Child* find_owned_child(Value& value, Id id) {
  Child* result{};
  visit_owned_children(value, [&](auto& child) {
    if (child.persistent_id == id) { result = &child; }
  });
  return result;
}

// Return the first descendant identity, or zero for a wrapper containing no engaged entities.
template <typename Value>
std::uint64_t first_owned_id(Value& value) {
  std::uint64_t result{};
  visit_owned_children(value, [&](const auto& child) {
    if (result == 0) { result = child.persistent_id; }
  });
  return result;
}

// Retain wrapper shape and optional engagement while replacing identified payloads with IDs.
template <typename Value>
auto owned_membership(const Value& value) {
  using type = std::remove_cvref_t<Value>;
  if constexpr (requires { value.persistent_id; }) {
    return value.persistent_id;
  } else if constexpr (requires { value.has_value(); value.value(); }) {
    using member_type = decltype(owned_membership(*value));
    std::optional<member_type> result;
    if (value) { result.emplace(owned_membership(*value)); }
    return result;
  } else if constexpr (requires { typename type::mapped_type; }) {
    using child_type = typename type::mapped_type;
    using member_type = std::conditional_t<requires (child_type child) { child.persistent_id; },
        std::uint64_t, decltype(owned_membership(std::declval<const child_type&>()))>;
    std::map<typename type::key_type, member_type> result;
    for (const auto& [key, child] : value) { result.emplace(key, owned_membership(child)); }
    return result;
  } else {
    using child_type = typename type::value_type;
    using member_type = std::conditional_t<requires (child_type child) { child.persistent_id; },
        std::uint64_t, decltype(owned_membership(std::declval<const child_type&>()))>;
    std::vector<member_type> result;
    result.reserve(value.size());
    for (const auto& child : value) { result.push_back(owned_membership(child)); }
    return result;
  }
}

// Rebuild transient projections without changing wrapper engagement or durable values.
template <typename Value>
void reset_owned_runtime(Value& value) {
  using type = std::remove_cvref_t<Value>;
  if constexpr (std::is_arithmetic_v<type> || std::is_enum_v<type> ||
                requires { typename type::traits_type; }) {
    static_cast<void>(value);
  } else if constexpr (requires { value.has_value(); value.value(); }) {
    if (value) { reset_owned_runtime(*value); }
  } else if constexpr (requires { typename type::mapped_type; }) {
    for (auto& [key, child] : value) { static_cast<void>(key); reset_owned_runtime(child); }
  } else if constexpr (requires { value.begin(); value.end(); }) {
    for (auto&& child : value) { reset_owned_runtime(child); }
  } else if constexpr (requires { runtime_traits<type>::reset(value); }) {
    runtime_traits<type>::reset(value);
  }
}

// Carry transient state only across matching engaged wrappers and retained collection positions.
template <typename Value>
void copy_owned_runtime(Value& value, const Value& source) {
  using type = std::remove_cvref_t<Value>;
  if constexpr (std::is_arithmetic_v<type> || std::is_enum_v<type> ||
                requires { typename type::traits_type; }) {
    static_cast<void>(value); static_cast<void>(source);
  } else if constexpr (requires { value.has_value(); value.value(); }) {
    if (value && source) { copy_owned_runtime(*value, *source); }
  } else if constexpr (requires { typename type::mapped_type; }) {
    for (auto& [key, child] : value) {
      const auto found = source.find(key);
      if (found != source.end()) { copy_owned_runtime(child, found->second); }
    }
  } else if constexpr (requires { value.begin(); value.end(); }) {
    auto old = source.begin();
    for (auto&& child : value) {
      if (old == source.end()) { break; }
      copy_owned_runtime(child, *old++);
    }
  } else if constexpr (requires { runtime_traits<type>::copy(value, source); }) {
    runtime_traits<type>::copy(value, source);
  }
}

// Convert matching wrappers deeply; generated entity traits preserve each terminal contract.
template <typename Destination, typename Source>
Destination convert_owned_value(Source&& source) {
  using source_type = std::remove_cvref_t<Source>;
  if constexpr (requires { source.has_value(); source.value(); }) {
    Destination result{};
    if (source) {
      result.emplace(convert_owned_value<typename Destination::value_type>(
          std::forward<Source>(source).value()));
    }
    return result;
  } else if constexpr (requires { typename source_type::mapped_type; }) {
    Destination result{};
    for (auto& [key, child] : source) {
      if constexpr (std::is_const_v<std::remove_reference_t<Source>>) {
        result.emplace(key, convert_owned_value<typename Destination::mapped_type>(child));
      } else {
        result.emplace(key, convert_owned_value<typename Destination::mapped_type>(std::move(child)));
      }
    }
    return result;
  } else if constexpr (requires { source.begin(); source.end(); }) {
    Destination result{};
    std::size_t index{};
    if constexpr (requires { result.reserve(source.size()); }) { result.reserve(source.size()); }
    for (auto& child : source) {
      if constexpr (std::is_const_v<std::remove_reference_t<Source>>) {
        auto converted = convert_owned_value<typename Destination::value_type>(child);
        if constexpr (requires { result.push_back(std::move(converted)); }) {
          result.push_back(std::move(converted));
        } else { result[index++] = std::move(converted); }
      } else {
        auto converted = convert_owned_value<typename Destination::value_type>(std::move(child));
        if constexpr (requires { result.push_back(std::move(converted)); }) {
          result.push_back(std::move(converted));
        } else { result[index++] = std::move(converted); }
      }
    }
    return result;
  } else if constexpr (std::is_const_v<std::remove_reference_t<Source>>) {
    return model_traits<Destination>::clone_value(source);
  } else {
    return model_traits<source_type>::make_storage(std::forward<Source>(source));
  }
}
} // namespace detail
} // namespace rohit::managed
