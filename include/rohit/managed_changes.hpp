// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include "serializer.hpp"
#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rohit::managed {
template <typename Root> struct model_traits;
namespace detail {

// An entity owns local field keys; ordinary base scopes retain their full stable prefix.
struct publication_field_address {
  std::uint64_t entity{};
  std::vector<std::uint32_t> scopes{};
  std::uint32_t field{};
  auto operator<=>(const publication_field_address&) const = default;
};

// Capture bounded durable fields independently of collaboration's history and wire records.
class publication_field_collector {
  std::size_t remaining_;
  std::vector<std::uint32_t> scopes_{};

  // Restore the enclosing ordinary-base scope even if nested child traversal throws.
  template <typename Callable>
  void entity_scope(Callable&& callback) {
    auto parent = std::move(scopes_);
    scopes_.clear();
    try { std::forward<Callable>(callback)(); }
    catch (...) { scopes_ = std::move(parent); throw; }
    scopes_ = std::move(parent);
  }

public:
  std::map<publication_field_address, std::vector<std::uint8_t>> fields{};
  // Use the store's owning snapshot budget for each temporary durable field traversal.
  explicit publication_field_collector(std::size_t max_bytes) : remaining_{max_bytes} {}

  // Preserve exact codec equality, including floating-point bit patterns and nested ordinary data.
  void value(std::uint64_t entity, std::uint32_t field, const auto& input) {
    if (remaining_ == 0) { throw std::length_error{"Managed notification comparison budget exceeded"}; }
    const stream_limits limits{std::min(stream_limits{}.min_read_buffer_bytes, remaining_), remaining_};
    full_stream_auto_alloc_limits stream{&limits};
    serializer::binary_integer<serializer::serialize_type::out, full_stream_auto_alloc_limits>
        encoder{stream};
    if constexpr (requires { input.valueless_by_exception(); input.index(); }) {
      if (input.valueless_by_exception()) { throw std::invalid_argument{"Valueless managed variant"}; }
      encoder.serialize_out_variable(input.index());
      std::visit([&](const auto& active) { encoder.serialize_out(active); }, input);
    } else { encoder.struct_serialize_out(input); }
    std::vector<std::uint8_t> bytes{stream.begin(), stream.begin() + stream.current_offset()};
    remaining_ -= bytes.size();
    if (!fields.emplace(publication_field_address{entity, scopes_, field}, std::move(bytes)).second) {
      throw std::invalid_argument{"Duplicate managed notification field address"};
    }
  }

  // Ordinary inherited data remains part of its enclosing entity under a stable base scope.
  template <typename Callable>
  void with_path(std::uint32_t base, Callable&& callback) {
    scopes_.push_back(base);
    try { std::forward<Callable>(callback)(*this); }
    catch (...) { scopes_.pop_back(); throw; }
    scopes_.pop_back();
  }

  // Separate active variant membership from the independently identified child's durable payload.
  void owned_variant(std::uint64_t entity, std::uint32_t field, const auto& input) {
    if (input.valueless_by_exception()) { throw std::invalid_argument{"Valueless managed variant"}; }
    std::vector<std::uint64_t> membership{static_cast<std::uint64_t>(input.index())};
    std::visit([&](const auto& child) { membership.push_back(child.persistent_id); }, input);
    value(entity, field, membership);
    entity_scope([&] {
      std::visit([&](const auto& child) {
        model_traits<std::remove_cvref_t<decltype(child)>>::visit_collaboration_fields(child, *this);
      }, input);
    });
  }

  // Parent membership identifies relocations/births/deletions without reporting every child payload edit.
  template <typename ChildTraits>
  void owned(std::uint64_t entity, std::uint32_t field, const auto& input) {
    if constexpr (requires { input.persistent_id; }) {
      value(entity, field, input.persistent_id);
      entity_scope([&] { ChildTraits::visit_collaboration_fields(input, *this); });
    } else if constexpr (requires { typename std::remove_cvref_t<decltype(input)>::mapped_type; }) {
      std::map<typename std::remove_cvref_t<decltype(input)>::key_type, std::uint64_t> membership;
      for (const auto& [key, child] : input) { membership.emplace(key, child.persistent_id); }
      value(entity, field, membership);
      entity_scope([&] {
        for (const auto& [key, child] : input) {
          static_cast<void>(key);
          ChildTraits::visit_collaboration_fields(child, *this);
        }
      });
    } else {
      std::vector<std::uint64_t> membership;
      membership.reserve(input.size());
      for (const auto& child : input) { membership.push_back(child.persistent_id); }
      value(entity, field, membership);
      entity_scope([&] {
        for (const auto& child : input) { ChildTraits::visit_collaboration_fields(child, *this); }
      });
    }
  }
};

// Compare complete entity-local durable field/membership encodings and identity presence/type.
template <typename Traits, typename Storage, typename IdentityTable>
auto changed_entity_ids(const Storage* before, const Storage& after,
                        const IdentityTable& before_ids, const IdentityTable& after_ids,
                        std::size_t max_bytes, bool replaced_namespace) {
  using id_type = typename IdentityTable::key_type;
  std::set<id_type> changed;
  if (!before || replaced_namespace) {
    for (const auto& [id, type] : before_ids) { static_cast<void>(type); changed.insert(id); }
    for (const auto& [id, type] : after_ids) { static_cast<void>(type); changed.insert(id); }
  } else {
    publication_field_collector old{max_bytes}, next{max_bytes};
    Traits::visit_collaboration_fields(*before, old);
    Traits::visit_collaboration_fields(after, next);
    for (const auto& [address, bytes] : old.fields) {
      const auto found = next.fields.find(address);
      if (found == next.fields.end() || found->second != bytes) {
        changed.insert(static_cast<id_type>(address.entity));
      }
    }
    for (const auto& [address, bytes] : next.fields) {
      static_cast<void>(bytes);
      if (!old.fields.contains(address)) { changed.insert(static_cast<id_type>(address.entity)); }
    }
    for (const auto& [id, type] : before_ids) {
      const auto found = after_ids.find(id);
      if (found == after_ids.end() || found->second != type) { changed.insert(id); }
    }
    for (const auto& [id, type] : after_ids) {
      static_cast<void>(type);
      if (!before_ids.contains(id)) { changed.insert(id); }
    }
  }
  return std::vector<id_type>{changed.begin(), changed.end()};
}
} // namespace detail
} // namespace rohit::managed
