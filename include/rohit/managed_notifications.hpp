// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/managed_changes.hpp>
#include <rohit/managed_memory.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::managed {

// A field is the atomic codec/membership boundary, including its stable ordinary-base path.
enum class field_change_kind { inserted, modified, erased };
struct field_change {
  std::uint64_t entity{};
  std::vector<std::uint32_t> path{};
  field_change_kind kind{};
};

// Optional detailed notifications are prepared before acceptance; limits bound owned output metadata.
struct notification_options {
  bool field_paths{false};
  std::size_t max_field_changes{65536};
  std::size_t max_entity_changes{65536};
  std::size_t max_field_path_components{32};
  std::size_t max_field_change_bytes{1024U * 1024U};
  std::size_t max_catalog_bytes{16U * 1024U * 1024U};
};

// Owning immutable-by-contract preparation metadata; before/after values remain in the caller's pinned roots.
template <typename Id>
struct prepared_change_set {
  std::vector<Id> created_ids{};
  std::vector<Id> deleted_ids{};
  std::vector<Id> changed_ids{};
  std::vector<field_change> changed_fields{};
  bool namespace_replaced{};
  bool field_paths_precise{};
};

// Entity zero matches every entity; an empty path selects every durable field of that entity.
struct projection_dependency {
  std::uint64_t entity{};
  std::vector<std::uint32_t> path{};
};

// A bounded immutable dependency list filters the existing publication callback without owning a store.
class projection_dependencies {
  std::vector<projection_dependency> dependencies_;
  bool runtime_updates_;
public:
  // Copy stable IDs only; this object retains no snapshot, editor, callback or source-data aliases.
  explicit projection_dependencies(std::span<const projection_dependency> dependencies,
      bool runtime_updates = true, std::size_t max_dependencies = 1024,
      std::size_t max_path_components = 32)
      : runtime_updates_{runtime_updates} {
    if (dependencies.size() > max_dependencies) {
      throw std::length_error{"Projection dependency count limit exceeded"};
    }
    for (const auto& dependency : dependencies) {
      if (dependency.path.size() > max_path_components ||
          std::find(dependency.path.begin(), dependency.path.end(), std::uint32_t{}) != dependency.path.end()) {
        throw std::invalid_argument{"Projection dependencies require bounded nonzero stable field paths"};
      }
    }
    dependencies_.assign(dependencies.begin(), dependencies.end());
  }

  // Compare prefixes in both directions: an atomic parent replacement invalidates descendant projections.
  template <typename Event>
  bool affected_by(const Event& event) const {
    if (event.kind == decltype(event.kind)::runtime_update) { return runtime_updates_; }
    for (const auto& dependency : dependencies_) {
      // Entity birth/deletion and namespace replacement can have no fields (for example an empty record).
      if (event.namespace_replaced ||
          std::find(event.created_ids.begin(), event.created_ids.end(), dependency.entity) != event.created_ids.end() ||
          std::find(event.deleted_ids.begin(), event.deleted_ids.end(), dependency.entity) != event.deleted_ids.end()) {
        return true;
      }
      if (!event.field_paths_precise) {
        if (dependency.entity == 0 ||
            std::find(event.changed_ids.begin(), event.changed_ids.end(), dependency.entity) != event.changed_ids.end()) {
          return !event.changed_ids.empty();
        }
        continue;
      }
      if (dependency.entity == 0 && (!event.created_ids.empty() || !event.deleted_ids.empty())) { return true; }
      for (const auto& change : event.changed_fields) {
        if (dependency.entity != 0 && dependency.entity != change.entity) { continue; }
        const auto count = std::min(dependency.path.size(), change.path.size());
        if (std::equal(dependency.path.begin(), dependency.path.begin() + static_cast<std::ptrdiff_t>(count),
            change.path.begin())) { return true; }
      }
    }
    return false;
  }
};

namespace detail {

// Prepare bounded owning field paths using the same exact codec equality as changed entity IDs.
template <typename Traits, typename Storage>
std::vector<field_change> changed_field_paths(const Storage* before, const Storage& after,
    std::size_t max_snapshot_bytes, const notification_options& options,
    bool replaced_namespace = false) {
  publication_field_collector old{max_snapshot_bytes}, next{max_snapshot_bytes};
  if (before) { Traits::visit_collaboration_fields(*before, old); }
  Traits::visit_collaboration_fields(after, next);
  std::vector<field_change> result;
  std::size_t path_capacity_bytes{};
  const auto append = [&](const publication_field_address& address, field_change_kind kind) {
    if (result.size() >= options.max_field_changes ||
        address.scopes.size() >= options.max_field_path_components) {
      throw std::length_error{"Managed field notification count or path limit exceeded"};
    }
    std::vector<std::uint32_t> path;
    path.reserve(address.scopes.size() + 1U);
    path.insert(path.end(), address.scopes.begin(), address.scopes.end());
    path.push_back(address.field);
    add_memory_estimate(path_capacity_bytes, multiply_memory_estimate(path.capacity(), sizeof(std::uint32_t)));
    result.push_back(field_change{address.entity, std::move(path), kind});
    auto bytes = multiply_memory_estimate(result.capacity(), sizeof(field_change));
    add_memory_estimate(bytes, path_capacity_bytes);
    if (bytes > options.max_field_change_bytes) {
      throw std::length_error{"Managed field notification byte limit exceeded"};
    }
  };
  for (const auto& [address, value] : old.fields) {
    const auto found = next.fields.find(address);
    if (replaced_namespace || found == next.fields.end()) { append(address, field_change_kind::erased); }
    else if (found->second != value) { append(address, field_change_kind::modified); }
  }
  for (const auto& [address, value] : next.fields) {
    static_cast<void>(value);
    if (replaced_namespace || !old.fields.contains(address)) { append(address, field_change_kind::inserted); }
  }
  std::sort(result.begin(), result.end(), [](const field_change& left, const field_change& right) {
    if (left.entity != right.entity) { return left.entity < right.entity; }
    if (left.path != right.path) { return left.path < right.path; }
    return left.kind < right.kind;
  });
  return result;
}

// Reuse validated identity tables in model_store without building another entity catalog.
template <typename Traits, typename Storage, typename IdentityTable>
prepared_change_set<typename IdentityTable::key_type> prepare_change_set(
    const Storage* before, const Storage& after, const IdentityTable& before_ids,
    const IdentityTable& after_ids, std::size_t max_snapshot_bytes,
    const notification_options& options, bool replaced_namespace = false) {
  using id_type = typename IdentityTable::key_type;
  prepared_change_set<id_type> result;
  result.namespace_replaced = replaced_namespace;
  const auto append_entity = [&](auto& values, id_type id) {
    if (values.size() >= options.max_entity_changes) {
      throw std::length_error{"Managed change-set entity limit exceeded"};
    }
    values.push_back(id);
  };
  for (const auto& [id, type] : before_ids) {
    const auto found = after_ids.find(id);
    if (replaced_namespace || found == after_ids.end() || found->second != type) {
      append_entity(result.deleted_ids, id);
    }
  }
  for (const auto& [id, type] : after_ids) {
    const auto found = before_ids.find(id);
    if (replaced_namespace || found == before_ids.end() || found->second != type) {
      append_entity(result.created_ids, id);
    }
  }
  if constexpr (requires(const Storage& value, publication_field_collector& visitor) {
      Traits::visit_collaboration_fields(value, visitor);
    }) {
    if (options.field_paths) {
      result.changed_fields = changed_field_paths<Traits>(before, after,
          max_snapshot_bytes, options, replaced_namespace);
      result.field_paths_precise = true;
      std::set<id_type> changed{result.created_ids.begin(), result.created_ids.end()};
      changed.insert(result.deleted_ids.begin(), result.deleted_ids.end());
      for (const auto& field : result.changed_fields) {
        changed.insert(static_cast<id_type>(field.entity));
        if (changed.size() > options.max_entity_changes) {
          throw std::length_error{"Managed change-set entity limit exceeded"};
        }
      }
      result.changed_ids.assign(changed.begin(), changed.end());
    } else {
      result.changed_ids = changed_entity_ids<Traits>(before, after, before_ids,
          after_ids, max_snapshot_bytes, replaced_namespace);
    }
  } else {
    // Legacy traits expose entity-level invalidation explicitly rather than fabricating exact paths.
    std::set<id_type> changed;
    for (const auto& [id, type] : before_ids) { static_cast<void>(type); changed.insert(id); }
    for (const auto& [id, type] : after_ids) { static_cast<void>(type); changed.insert(id); }
    result.changed_ids.assign(changed.begin(), changed.end());
  }
  if (result.changed_ids.size() > options.max_entity_changes) {
    throw std::length_error{"Managed change-set entity limit exceeded"};
  }
  return result;
}

// A native acceptance adapter can prepare identical metadata directly from two immutable typed roots.
// Identity catalogs are bounded by the caller's ordinary entity ceiling; no edit callbacks execute here.
template <typename Traits, typename Storage>
auto prepare_change_set(const Storage* before, const Storage& after,
    std::size_t max_entities, std::size_t max_snapshot_bytes,
    const notification_options& options, bool replaced_namespace = false) {
  using id_type = typename Traits::id_type;
  using identity_table = std::map<id_type, std::string>;
  const auto identities = [&](const Storage& root) {
    identity_table result;
    std::size_t bytes{};
    Traits::visit_entities(root, [&](auto id, std::string_view type) {
      if (result.size() >= max_entities) {
        throw std::length_error{"Managed change-set identity catalog limit exceeded"};
      }
      constexpr auto node_bytes = sizeof(typename identity_table::value_type) + 6U * sizeof(void*);
      add_memory_estimate(bytes, node_bytes);
      add_memory_estimate(bytes, type.size());
      add_memory_estimate(bytes, 1U);
      if (bytes > options.max_catalog_bytes) {
        throw std::length_error{"Managed change-set identity catalog byte limit exceeded"};
      }
      if (id == 0 || !result.emplace(id, std::string{type}).second) {
        throw std::invalid_argument{"Managed change-set identity catalog is invalid"};
      }
    });
    return result;
  };
  const auto old = before ? identities(*before) : identity_table{};
  const auto next = identities(after);
  return prepare_change_set<Traits>(before, after, old, next,
      max_snapshot_bytes, options, replaced_namespace);
}

} // namespace detail
} // namespace rohit::managed
