// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_editable_ownership.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {
namespace managed = rohit::managed;
using root = editable_ownership::document;
using group = editable_ownership::group;
using child = editable_ownership::child;
using traits = managed::model_traits<root>;
using group_traits = managed::model_traits<group>;
using child_traits = managed::model_traits<child>;
using store_type = managed::model_store<root>;

// Provide two owning parents and two same-typed endpoints without preassigned identities.
root initial() {
  root value{};
  value.groups.resize(2U);
  value.groups[0].name = "first parent";
  value.groups[1].name = "second parent";
  value.groups[0].primary.resize(2U);
  value.groups[0].primary[0].label = "first";
  value.groups[0].primary[1].label = "second";
  value.groups[0].secondary.resize(1U);
  value.groups[0].secondary[0].label = "third";
  value.groups[1].primary.resize(1U);
  value.groups[1].primary[0].label = "fourth";
  return value;
}

// Resolve nested payloads without assuming direct or separate identity-bearing storage.
auto& parent(auto& storage, std::size_t index) {
  return group_traits::payload(traits::payload(storage).groups[index]);
}

// Initialize both an internal child edge and a root edge before the first accepted baseline.
void initialize_references(auto& storage, const auto& references) {
  auto& first = parent(storage, 0U).primary;
  const auto token = references.template reference<child>(first.front().persistent_id);
  child_traits::payload(first.back()).next = token;
  traits::payload(storage).links.push_back(token);
}

// Move one identity-bearing child through the existing trusted candidate-update API.
void relocate(auto& storage, bool cross_parent) {
  auto& source = parent(storage, 0U).primary;
  auto moved = std::move(source.front());
  source.erase(source.begin());
  if (cross_parent) {
    parent(storage, 1U).primary.push_back(std::move(moved));
  } else {
    parent(storage, 0U).secondary.push_back(std::move(moved));
  }
}
} // namespace

// Reordering changes collection positions while retaining identities, ownership and typed edges.
TEST(managed_editable_ownership, reorder_preserves_nested_ids_references_and_history) {
  store_type store{initial(), {1001U, 1002U}, {}, {}, initialize_references<
      store_type::storage_type, managed::reference_builder<root, traits>>};
  const auto before = store.read();
  const auto root_id = before->persistent_id;
  const auto group_id = traits::payload(*before).groups[0].persistent_id;
  const auto first = parent(*before, 0U).primary.front().persistent_id;
  const auto second = parent(*before, 0U).primary.back().persistent_id;
  const auto reference = store.registry().template reference<child>(first);
  const auto allocated = store.allocated_id();
  std::size_t publications{};
  store.set_publication_callback([&](const auto&) { ++publications; });
  auto candidate = *before;
  std::reverse(parent(candidate, 0U).primary.begin(), parent(candidate, 0U).primary.end());
  child_traits::payload(parent(candidate, 0U).primary.front()).label = "reordered";
  candidate.persistent_id = 0;
  store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(candidate));
  }).throw_if_failed();
  EXPECT_EQ(publications, 1U);
  EXPECT_EQ(store.history_cursor(), 1U);
  EXPECT_EQ(store.read()->persistent_id, root_id);
  EXPECT_EQ(traits::payload(*store.read()).groups[0].persistent_id, group_id);
  EXPECT_EQ(parent(*store.read(), 0U).primary.front().persistent_id, second);
  EXPECT_EQ(parent(*store.read(), 0U).primary.back().persistent_id, first);
  EXPECT_EQ(store.registry().resolve(reference).value().label, "first");
  EXPECT_EQ(child_traits::payload(parent(*before, 0U).primary.back()).label, "second");
  EXPECT_EQ(store.allocated_id(), allocated);
  store.undo();
  EXPECT_EQ(parent(*store.read(), 0U).primary.front().persistent_id, first);
  store.redo();
  EXPECT_EQ(parent(*store.read(), 0U).primary.front().persistent_id, second);
  EXPECT_EQ(store.registry().template find<child>(second).value().next, reference);
}

// Editable reconciliation rejects a retained identity's changed parent or owning field atomically.
TEST(managed_editable_ownership, retained_cross_endpoint_ids_reject_before_publication) {
  for (const bool cross_parent : {false, true}) {
    SCOPED_TRACE(cross_parent);
    store_type store{initial(), {1011U, 1012U}};
    const auto before = store.read();
    const auto state = store.state();
    const auto allocated = store.allocated_id();
    const auto resident = store.resident_bytes();
    std::size_t publications{};
    store.set_publication_callback([&](const auto&) { ++publications; });
    auto candidate = *before;
    relocate(candidate, cross_parent);
    const auto result = store.execute_transaction([&](auto& edit) {
      edit.root().apply_editable(std::move(candidate));
    });
    EXPECT_THROW(result.throw_if_failed(), std::invalid_argument);
    EXPECT_EQ(store.read(), before);
    EXPECT_EQ(store.state(), state);
    EXPECT_EQ(store.history_cursor(), 0U);
    EXPECT_EQ(store.allocated_id(), allocated);
    EXPECT_EQ(store.resident_bytes(), resident);
    EXPECT_EQ(publications, 0U);
  }
}

// Failure after assigning new children burns only their reservations and preserves the full old graph.
TEST(managed_editable_ownership, endpoint_failure_after_allocation_never_reuses_exposed_ids) {
  store_type store{initial(), {1021U, 1022U}};
  const auto before = store.read();
  const auto state = store.state();
  const auto allocated = store.allocated_id();
  std::size_t publications{};
  store.set_publication_callback([&](const auto&) { ++publications; });
  auto candidate = *before;
  parent(candidate, 1U).secondary.push_back(child_traits::make_storage(child{}));
  relocate(candidate, true);
  const auto result = store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(candidate));
  });
  EXPECT_THROW(result.throw_if_failed(), std::invalid_argument);
  EXPECT_EQ(store.read(), before);
  EXPECT_EQ(store.state(), state);
  EXPECT_EQ(store.history_cursor(), 0U);
  EXPECT_EQ(publications, 0U);
  EXPECT_GT(store.allocated_id(), allocated);
  const auto burned = store.allocated_id();
  auto valid = *before;
  parent(valid, 1U).secondary.push_back(child_traits::make_storage(child{}));
  store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(valid));
  }).throw_if_failed();
  EXPECT_GT(parent(*store.read(), 1U).secondary.back().persistent_id, burned);
  EXPECT_EQ(publications, 1U);
}

// A trusted explicit candidate move retains IDs and references through both ownership changes.
TEST(managed_editable_ownership, explicit_moves_preserve_ids_references_undo_and_recovery) {
  for (const bool cross_parent : {false, true}) {
    SCOPED_TRACE(cross_parent);
    store_type store{initial(), {1031U, 1032U}, {}, {}, initialize_references<
        store_type::storage_type, managed::reference_builder<root, traits>>};
    const auto before = store.read();
    const auto moved = parent(*before, 0U).primary.front().persistent_id;
    const auto token = store.registry().template reference<child>(moved);
    const auto allocated = store.allocated_id();
    store.execute_transaction([&](auto& edit) {
      edit.update([&](auto& storage) { relocate(storage, cross_parent); });
    }).throw_if_failed();
    EXPECT_EQ(store.registry().resolve(token).id(), moved);
    EXPECT_EQ(store.allocated_id(), allocated);
    EXPECT_EQ(parent(*before, 0U).primary.front().persistent_id, moved);
    if (cross_parent) {
      EXPECT_EQ(parent(*store.read(), 1U).primary.back().persistent_id, moved);
    } else {
      EXPECT_EQ(parent(*store.read(), 0U).secondary.back().persistent_id, moved);
    }
    const auto saved = store.save();
    store.undo();
    EXPECT_EQ(parent(*store.read(), 0U).primary.front().persistent_id, moved);
    store.redo();
    EXPECT_EQ(store.save(), saved);
    store_type recovered{root{}};
    recovered.load(saved);
    EXPECT_EQ(recovered.registry().resolve(token).id(), moved);
    recovered.undo();
    EXPECT_EQ(parent(*recovered.read(), 0U).primary.front().persistent_id, moved);
    recovered.redo();
    EXPECT_EQ(recovered.save(), saved);
  }
}

// Retirement requires clearing all incoming edges and never permits a removed child ID to return.
TEST(managed_editable_ownership, retirement_checks_edges_and_retired_identity_reuse) {
  store_type store{initial(), {1041U, 1042U}, {}, {}, initialize_references<
      store_type::storage_type, managed::reference_builder<root, traits>>};
  const auto before = store.read();
  const auto removed = parent(*before, 0U).primary.front().persistent_id;
  const auto token = store.registry().template reference<child>(removed);
  const auto pinned = store.registry();
  auto candidate = *before;
  parent(candidate, 0U).primary.erase(parent(candidate, 0U).primary.begin());
  const auto rejected = store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(candidate));
  });
  EXPECT_THROW(rejected.throw_if_failed(), managed::registry_error);
  EXPECT_EQ(store.read(), before);
  EXPECT_EQ(store.history_cursor(), 0U);
  candidate = *before;
  parent(candidate, 0U).primary.erase(parent(candidate, 0U).primary.begin());
  child_traits::payload(parent(candidate, 0U).primary.front()).next.reset();
  traits::payload(candidate).links.clear();
  store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(candidate));
  }).throw_if_failed();
  EXPECT_FALSE(store.registry().contains(removed));
  EXPECT_EQ(pinned.resolve(token).value().label, "first");
  const auto retired = store.read();
  candidate = *retired;
  parent(candidate, 0U).primary.push_back(parent(*before, 0U).primary.front());
  const auto reuse = store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(candidate));
  });
  EXPECT_THROW(reuse.throw_if_failed(), std::invalid_argument);
  EXPECT_EQ(store.read(), retired);
  store.undo();
  EXPECT_EQ(store.registry().resolve(token).id(), removed);
  store.redo();
  EXPECT_FALSE(store.registry().contains(removed));
}
