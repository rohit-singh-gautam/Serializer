// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include "managed_changes.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rohit::managed {

// Publication origins let applications distinguish trusted replication from authoring permissions.
enum class authorization_operation { edit, history, replace, runtime, replicate };
// Content, ownership structure and permission administration are independent capabilities.
enum class authorization_permission { content, structure, policy };
enum class authorization_decision { allow, deny };
enum class authorization_scope { target, subtree };
// Explicit opt-in nearest rules permit editable exceptions beneath an explicitly read-only ancestor.
enum class authorization_precedence { deny_overrides, nearest_rule };

class authorization_error : public std::runtime_error {
public:
  // Report a denied candidate before any accepted state or journal is changed.
  authorization_error() : std::runtime_error{"Managed publication denied by authorization policy"} {}
};

// Stable entity/field addressing never treats a reference, collection index or display name as ownership.
struct authorization_target {
  std::uint64_t entity{};
  std::vector<std::uint32_t> fields{};
};

struct authorization_rule {
  authorization_target target{};
  authorization_permission permission{authorization_permission::content};
  authorization_decision decision{authorization_decision::allow};
  authorization_scope scope{authorization_scope::subtree};
};

// Immutable rules belong to one application-selected access context; authentication stays host-owned.
class authorization_policy {
  std::vector<authorization_rule> rules_{};
  authorization_precedence precedence_;
public:
  // Validate caller-authored policy metadata once; zero denotes an invalid/unbound entity.
  explicit authorization_policy(std::vector<authorization_rule> rules,
      authorization_precedence precedence = authorization_precedence::deny_overrides)
      : rules_{std::move(rules)}, precedence_{precedence} {
    if (precedence != authorization_precedence::deny_overrides &&
        precedence != authorization_precedence::nearest_rule) {
      throw std::invalid_argument{"Invalid managed authorization precedence"};
    }
    for (const auto& rule : rules_) {
      if (rule.target.entity == 0 ||
          (rule.permission != authorization_permission::content &&
           rule.permission != authorization_permission::structure &&
           rule.permission != authorization_permission::policy) ||
          (rule.decision != authorization_decision::allow && rule.decision != authorization_decision::deny) ||
          (rule.scope != authorization_scope::target && rule.scope != authorization_scope::subtree)) {
        throw std::invalid_argument{"Invalid managed authorization rule"};
      }
    }
  }

  // The caller supplies root-to-target ownership, with each ancestor's traversed field path.
  // Unmatched operations deny. Default explicit denial wins; opt-in nearest rules rank descendant
  // entities before ancestor entities, then longer field paths, and deny ties independent of rule order.
  bool permits(std::span<const authorization_target> ownership,
               authorization_permission permission) const {
    if (ownership.empty()) { return false; }
    bool granted = false;
    bool denied = false;
    bool matched = false;
    std::pair<std::size_t, std::size_t> specificity{};
    for (std::size_t index = 0; index < ownership.size(); ++index) {
      const auto& target = ownership[index];
      if (target.entity == 0) { throw std::invalid_argument{"Unbound authorization target"}; }
      for (const auto& rule : rules_) {
        if (rule.permission != permission || rule.target.entity != target.entity ||
            (index + 1 != ownership.size() && rule.scope != authorization_scope::subtree) ||
            rule.target.fields.size() > target.fields.size() ||
            !std::equal(rule.target.fields.begin(), rule.target.fields.end(), target.fields.begin())) {
          continue;
        }
        if (precedence_ == authorization_precedence::deny_overrides) {
          if (rule.decision == authorization_decision::deny) { return false; }
          granted = true;
        } else {
          const std::pair candidate_specificity{index, rule.target.fields.size()};
          if (!matched || candidate_specificity > specificity) {
            specificity = candidate_specificity;
            granted = false;
            denied = false;
            matched = true;
          }
          if (candidate_specificity == specificity) {
            granted = granted || rule.decision == authorization_decision::allow;
            denied = denied || rule.decision == authorization_decision::deny;
          }
        }
      }
    }
    return granted && !denied;
  }
};

namespace detail {
// Collect generated ownership independently from references and retain structural field boundaries.
class authorization_graph_collector {
  std::vector<std::uint32_t> scopes_{};
public:
  std::map<std::uint64_t, authorization_target> parents{};
  std::map<publication_field_address, authorization_permission> fields{};

  // Durable ordinary values are authorized at their nearest independently identified owner.
  void value(std::uint64_t entity, std::uint32_t field, const auto&) {
    if (!fields.emplace(publication_field_address{entity, scopes_, field},
                        authorization_permission::content).second) {
      throw std::invalid_argument{"Duplicate managed authorization field"};
    }
  }

  // Preserve stable ordinary-base scopes without interpreting them as independently owned entities.
  template <typename Callable>
  void with_path(std::uint32_t base, Callable&& callback) {
    scopes_.push_back(base);
    try { std::forward<Callable>(callback)(*this); }
    catch (...) { scopes_.pop_back(); throw; }
    scopes_.pop_back();
  }

  // Traverse each independently owned child once and reset its entity-local field scope.
  template <typename ChildTraits, typename Child>
  void child(std::uint64_t owner, const std::vector<std::uint32_t>& path, const Child& input) {
    if (input.persistent_id == 0 ||
        !parents.emplace(input.persistent_id, authorization_target{owner, path}).second) {
      throw std::invalid_argument{"Invalid managed authorization ownership"};
    }
    auto parent_scopes = std::move(scopes_);
    scopes_.clear();
    try { ChildTraits::visit_collaboration_fields(input, *this); }
    catch (...) { scopes_ = std::move(parent_scopes); throw; }
    scopes_ = std::move(parent_scopes);
  }

  // Membership and collection keys are structural; child payloads retain separate content permissions.
  template <typename ChildTraits>
  void owned(std::uint64_t entity, std::uint32_t field, const auto& input) {
    value(entity, field, input);
    fields.at(publication_field_address{entity, scopes_, field}) = authorization_permission::structure;
    auto path = scopes_;
    path.push_back(field);
    visit_owned_children(input, [&](const auto& item) { child<ChildTraits>(entity, path, item); });
  }

  // Variant alternatives are structural membership, and only the live alternative owns descendants.
  void owned_variant(std::uint64_t entity, std::uint32_t field, const auto& input) {
    value(entity, field, input);
    fields.at(publication_field_address{entity, scopes_, field}) = authorization_permission::structure;
    auto path = scopes_;
    path.push_back(field);
    std::visit([&](const auto& item) {
      child<model_traits<std::remove_cvref_t<decltype(item)>>>(entity, path, item);
    }, input);
  }

  // Build the actual source/candidate ownership chain; cycles and detached nodes fail closed.
  std::vector<authorization_target> ownership(authorization_target target,
                                             std::uint64_t root) const {
    std::vector<authorization_target> result;
    auto entity = target.entity;
    result.push_back(std::move(target));
    while (entity != root) {
      const auto parent = parents.find(entity);
      if (parent == parents.end() || result.size() > parents.size()) {
        throw std::invalid_argument{"Detached or cyclic managed authorization ownership"};
      }
      result.push_back(parent->second);
      entity = parent->second.entity;
    }
    std::reverse(result.begin(), result.end());
    return result;
  }

  // Resolve a durable field through the same complete ownership chain used for entity moves.
  std::vector<authorization_target> ownership(const publication_field_address& address,
                                             std::uint64_t root) const {
    auto path = address.scopes;
    path.push_back(address.field);
    return ownership(authorization_target{address.entity, std::move(path)}, root);
  }
};
} // namespace detail

// Authorize the complete generated durable difference under both actual ownership trees.
// Moves check every moved field in both trees; new/deleted fields also require content grants.
// Each encoding traversal is bounded by max_snapshot_bytes; policies are not serialized.
template <typename Traits, typename Storage>
bool authorize_managed_candidate(const authorization_policy& policy,
                                 const Storage& before, const Storage& candidate,
                                 std::size_t max_snapshot_bytes) {
  if (before.persistent_id != candidate.persistent_id) { return false; }
  detail::publication_field_collector old{max_snapshot_bytes}, next{max_snapshot_bytes};
  detail::authorization_graph_collector old_graph, next_graph;
  Traits::visit_collaboration_fields(before, old);
  Traits::visit_collaboration_fields(candidate, next);
  Traits::visit_collaboration_fields(before, old_graph);
  Traits::visit_collaboration_fields(candidate, next_graph);
  const auto same_ownership = [](const auto& left, const auto& right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
        [](const auto& first, const auto& second) {
          return first.entity == second.entity && first.fields == second.fields;
        });
  };
  // Identity-only entities still need permission for deletion/moves, even with no durable fields.
  const auto check_entities = [&](const auto& source, const auto& destination) {
    for (const auto& [entity, parent] : source.parents) {
      static_cast<void>(parent);
      const auto ownership = source.ownership(authorization_target{entity, {}}, before.persistent_id);
      if ((!destination.parents.contains(entity) || !same_ownership(ownership,
              destination.ownership(authorization_target{entity, {}}, before.persistent_id))) &&
          !policy.permits(ownership, authorization_permission::content)) {
        return false;
      }
    }
    return true;
  };
  if (!check_entities(old_graph, next_graph) || !check_entities(next_graph, old_graph)) { return false; }
  // Compare ownership as well as bytes, preventing unchanged payloads from bypassing subtree denial.
  const auto check = [&](const auto& source, const auto& destination,
                         const auto& source_graph, const auto& destination_graph) {
    for (const auto& [address, bytes] : source.fields) {
      const auto found = destination.fields.find(address);
      const auto ownership = source_graph.ownership(address, before.persistent_id);
      bool moved = false;
      if (found != destination.fields.end()) {
        const auto other = destination_graph.ownership(address, before.persistent_id);
        moved = !same_ownership(ownership, other);
      }
      if ((found == destination.fields.end() || found->second != bytes || moved) &&
          !policy.permits(ownership, source_graph.fields.at(address))) {
        return false;
      }
    }
    return true;
  };
  return check(old, next, old_graph, next_graph) && check(next, old, next_graph, old_graph);
}
} // namespace rohit::managed
