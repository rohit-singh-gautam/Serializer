// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <rohit/managed_editor.hpp>

#include <cstdint>
#include <concepts>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace rohit::managed::detail {

// Retain only identity/type/owning endpoint metadata; reconciliation copies no old payload.
struct editable_entity_location {
  std::uint64_t parent{};
  std::uint32_t field{};
  std::string_view type{};
};

// Reuse generated ownership traversal while ignoring ordinary values and collection positions.
class editable_ownership_collector {
  std::uint32_t field_{};
public:
  std::map<std::uint64_t, editable_entity_location> entities{};

  // Require unique assigned identities before the caller can reconcile ownership.
  void begin_entity(std::uint64_t id, std::uint64_t parent) {
    if (id == 0 || !entities.emplace(id, editable_entity_location{parent, field_, {}}).second) {
      throw std::invalid_argument{"Duplicate or unassigned editable entity identity"};
    }
  }
  // Ordinary values do not participate in native identity reconciliation.
  void value(const auto&) const noexcept {}
  // Complete one generated entity without retaining its payload.
  void end_entity() const noexcept {}
  // Keep the owning field stable while permitting reordering within that collection.
  void edge(std::uint32_t field, const auto&) noexcept { field_ = field; }
};

// Build an ownership/type catalog only for an explicitly requested editable operation.
template <typename Traits, typename Storage>
auto editable_locations(const Storage& storage) {
  editable_ownership_collector collector;
  Traits::visit_collaboration(storage, typename Traits::id_type{}, collector);
  Traits::visit_entities(storage, [&](auto id, std::string_view type) {
    const auto found = collector.entities.find(id);
    if (found == collector.entities.end()) {
      throw std::logic_error{"Generated editable ownership omits an entity"};
    }
    found->second.type = type;
  });
  return std::move(collector.entities);
}

} // namespace rohit::managed::detail

namespace rohit::managed {

// Apply editable storage through the existing transaction failure boundary.
// The root keeps its ID. Retained children keep ID and owning endpoint; zero child IDs
// allocate normally, omitted children retire, and foreign/retired identities reject.
template <typename Plain, typename Access, typename Candidate>
void apply_editable(const Access& access, Candidate candidate) {
  using traits = model_traits<Plain>;
  static_assert(Access::is_managed, "Editable entity application requires a managed editor");
  static_assert(std::same_as<Candidate, typename traits::storage_type>,
      "Editable child reconciliation requires the generated identity-bearing storage type");
  access.guard([&] {
    candidate.persistent_id = access.id();
    std::map<std::uint64_t, detail::editable_entity_location> previous;
    access.with_storage([&](const auto& storage) { previous = detail::editable_locations<traits>(storage); });
    // Reject authored foreign IDs before allocating any newly requested occurrences.
    traits::visit_entities(candidate, [&](auto id, std::string_view type) {
      if (id == 0) { return; }
      const auto found = previous.find(id);
      if (found == previous.end() || found->second.type != type) {
        throw std::invalid_argument{"Editable entity has a foreign, retired or retyped identity"};
      }
    });
    // Native allocation occurs outside a borrowed candidate callback, before exposing a new ID.
    traits::visit_entities(candidate, [&](auto& id, std::string_view) {
      if (id == 0) { id = access.create_id(); }
    });
    const auto desired = detail::editable_locations<traits>(candidate);
    for (const auto& [id, location] : desired) {
      const auto found = previous.find(id);
      if (found != previous.end() && (location.parent != found->second.parent ||
          location.field != found->second.field || location.type != found->second.type)) {
        throw std::invalid_argument{"Editable entity changes an existing owning endpoint or type"};
      }
    }
    access.update([&](auto& target) { target = std::move(traits::payload(candidate)); });
  });
}

} // namespace rohit::managed
