// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_registry.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_notifications.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using root = registry_example::document;
using entry = registry_example::entry;
using store_type = managed::model_store<root>;
using traits = managed::model_traits<root>;
using descriptors = managed::fields<entry>;
using event_type = managed::publication_event<store_type::storage_type, store_type::id_type>;

// Make a small complete ownership graph with ordinary base/value fields and managed membership.
root initial() {
  root value;
  value.title = "before";
  value.entries.resize(1);
  value.entries.front().key = "first";
  value.entries.front().count = 1;
  return value;
}

// Copy stable dependency paths into an independent immutable filter.
auto dependency(std::uint64_t entity, std::vector<std::uint32_t> path) {
  const std::array selected{managed::projection_dependency{entity, std::move(path)}};
  return managed::projection_dependencies{selected};
}
} // namespace

// Exact field paths distinguish a child payload edit from its parent's unchanged membership.
TEST(managed_notifications, precise_field_paths_and_pinned_values) {
  store_type store{initial()};
  const auto root_id = store.read()->persistent_id;
  const auto child_id = traits::payload(*store.read()).entries.front().persistent_id;
  std::optional<event_type> retained;
  store.set_publication_callback([&](const auto& event) { retained = event; }, {.field_paths = true});
  store.execute_transaction([&](auto& tx) {
    tx.registry().template edit<entry>(child_id).set_count(7);
  }).throw_if_failed();
  ASSERT_TRUE(retained.has_value());
  EXPECT_TRUE(retained->field_paths_precise);
  ASSERT_EQ(retained->changed_fields.size(), 1u);
  EXPECT_EQ(retained->changed_fields.front().entity, child_id);
  EXPECT_EQ(retained->changed_fields.front().path, (std::vector<std::uint32_t>{2}));
  EXPECT_EQ(retained->changed_fields.front().kind, managed::field_change_kind::modified);
  EXPECT_EQ(retained->changed_ids, (std::vector<store_type::id_type>{child_id}));
  EXPECT_TRUE(retained->created_ids.empty());
  EXPECT_TRUE(retained->deleted_ids.empty());
  EXPECT_TRUE(dependency(child_id, {2}).affected_by(*retained));
  EXPECT_FALSE(dependency(child_id, {1}).affected_by(*retained));
  EXPECT_FALSE(dependency(root_id, {1}).affected_by(*retained));
  const auto before = retained->before;
  const auto after = retained->after;
  store.undo();
  EXPECT_EQ(managed::model_traits<entry>::payload(traits::payload(*before).entries.front()).count, 1);
  EXPECT_EQ(managed::model_traits<entry>::payload(traits::payload(*after).entries.front()).count, 7);
  EXPECT_EQ(retained->changed_fields.front().path, (std::vector<std::uint32_t>{2}));
}

// Stable ordinary-base scopes and whole ordinary-value replacements conservatively match child paths.
TEST(managed_notifications, base_scopes_and_inline_value_dependencies) {
  store_type store{initial()};
  const auto id = traits::payload(*store.read()).entries.front().persistent_id;
  std::optional<event_type> retained;
  store.set_publication_callback([&](const auto& event) { retained = event; }, {.field_paths = true});
  constexpr std::array<std::uint32_t, 2> base_path{5, 1};
  store.execute_transaction([&](auto& tx) {
    tx.registry().template bind<entry>(id, base_path, std::string{"base changed"});
  }).throw_if_failed();
  ASSERT_EQ(retained->changed_fields.size(), 1u);
  EXPECT_EQ(retained->changed_fields.front().path, (std::vector<std::uint32_t>{5, 1}));
  EXPECT_TRUE(dependency(id, {5}).affected_by(*retained));
  EXPECT_FALSE(dependency(id, {6}).affected_by(*retained));
  store.execute_transaction([&](auto& tx) {
    tx.registry().template edit<entry>(id).info().set_text("inline changed");
  }).throw_if_failed();
  ASSERT_EQ(retained->changed_fields.size(), 1u);
  EXPECT_EQ(retained->changed_fields.front().path, (std::vector<std::uint32_t>{6}));
  EXPECT_TRUE(dependency(id, {6, 1}).affected_by(*retained));
}

// Birth/deletion identities and ownership membership remain explicit even for projections of absent fields.
TEST(managed_notifications, entity_lifecycle_and_membership) {
  store_type store{initial()};
  const auto root_id = store.read()->persistent_id;
  std::optional<event_type> retained;
  store.set_publication_callback([&](const auto& event) { retained = event; }, {.field_paths = true});
  store_type::id_type added{};
  store.execute_transaction([&](auto& tx) { added = tx.root().entries().append(entry{}); }).throw_if_failed();
  EXPECT_EQ(retained->created_ids, (std::vector<store_type::id_type>{added}));
  EXPECT_TRUE(dependency(added, {999}).affected_by(*retained));
  EXPECT_TRUE(dependency(root_id, {1}).affected_by(*retained));
  store.execute_transaction([&](auto& tx) { tx.root().entries().erase(added); }).throw_if_failed();
  EXPECT_EQ(retained->deleted_ids, (std::vector<store_type::id_type>{added}));
  EXPECT_TRUE(dependency(0, {999}).affected_by(*retained));
}

// Limits fail during preparation and preserve accepted state/history without invoking observers.
TEST(managed_notifications, bounded_preparation_rejects_before_publication) {
  for (const auto options : {managed::notification_options{.field_paths = true, .max_field_changes = 0},
           managed::notification_options{.field_paths = true, .max_field_change_bytes = 1}}) {
    store_type store{initial()};
    const auto before = store.save();
    bool notified{};
    store.set_publication_callback([&](const auto&) { notified = true; }, options);
    const auto outcome = store.execute_transaction([](auto& tx) { tx.root().set_title("reject"); });
    EXPECT_EQ(outcome.status, managed::transaction_status::failed);
    EXPECT_FALSE(notified);
    EXPECT_EQ(store.save(), before);
  }
}

// A native prepare adapter can obtain the same owning diff without executing a mutation callback.
TEST(managed_notifications, reusable_prepared_change_set_matches_callback) {
  store_type store{initial()};
  const auto before = store.read();
  store.execute_transaction([](auto& tx) { tx.root().set_title("after"); }).throw_if_failed();
  const auto after = store.read();
  const auto changes = managed::detail::prepare_change_set<traits>(before.get(), *after,
      100, 1024 * 1024, managed::notification_options{.field_paths = true});
  ASSERT_EQ(changes.changed_fields.size(), 1u);
  EXPECT_EQ(changes.changed_fields.front().path, (std::vector<std::uint32_t>{2}));
  EXPECT_EQ(changes.changed_ids, (std::vector<store_type::id_type>{before->persistent_id}));
  const auto initial_changes = managed::detail::prepare_change_set<traits>(
      static_cast<const store_type::storage_type*>(nullptr), *after,
      100, 1024 * 1024, managed::notification_options{.field_paths = true});
  EXPECT_EQ(initial_changes.created_ids.size(), 2u);
}
