// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <optional_managed.hpp>
#include <rohit/collaboration_history.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_collaboration.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {
namespace managed = rohit::managed;
using document = optional_managed::document;
using leaf = optional_managed::leaf;
using store_type = managed::model_store<document>;

// Select the application payload for both direct and separate managed-storage profiles.
auto& payload(auto& value) { return managed::model_traits<document>::payload(value); }

// Select leaf data behind a separate storage wrapper or an ordinary/direct value.
auto& child_payload(auto& value) {
  if constexpr (requires { value.value; }) { return value.value; }
  else { return value; }
}

// Ordinary occurrences carry dormant IDs only in the direct representation.
std::uint32_t dormant_id(const auto& value) {
  if constexpr (requires { value.persistent_id; }) { return value.persistent_id; }
  else { return 0; }
}

// Construct engagement, nested collection wrappers and an ordinary dormant-identity child.
document sample() {
  document result{};
  result.scalar = 42;
  result.child = leaf{"child"};
  result.children = std::vector<leaf>{{"first"}, {"second"}};
  result.named.emplace();
  result.named->emplace("absent", std::nullopt);
  result.named->emplace("present", leaf{"named"});
  result.entries = {std::nullopt, leaf{"entry"}};
  result.value_child = leaf{"value"};
  return result;
}
} // namespace

// Engaged entity leaves receive identities in traversal order; absent/ordinary occurrences do not.
TEST(managed_optional, identities_and_wrapped_editors) {
  store_type store{sample()};
  std::vector<std::uint32_t> ids;
  managed::model_traits<document>::visit_entities(*store.read(), [&](auto id, auto) { ids.push_back(id); });
  EXPECT_EQ(ids, (std::vector<std::uint32_t>{1, 2, 3, 4, 5, 6}));
  EXPECT_EQ(dormant_id(*payload(*store.read()).value_child), 0u);
  store.execute_transaction([](auto& tx) {
    EXPECT_TRUE(tx.root().child().has_value());
    tx.root().child().edit().set_text("changed");
    tx.root().children().edit().edit(3).set_text("array");
    tx.root().named().edit().edit(5).set_text("map");
    tx.root().entries().edit(6).set_text("entry changed");
  }).throw_if_failed();
  EXPECT_EQ(child_payload(*payload(*store.read()).child).text, "changed");
  EXPECT_EQ(child_payload(payload(*store.read()).children->at(0)).text, "array");
  EXPECT_EQ(child_payload(*payload(*store.read()).named->at("present")).text, "map");
  EXPECT_EQ(child_payload(*payload(*store.read()).entries.at(1)).text, "entry changed");
}

// Reset retires live descendants while undo and reload retain identities and monotonic allocation.
TEST(managed_optional, engagement_history_and_replacement) {
  store_type store{sample()};
  const auto retained = store.read();
  store.execute_transaction([](auto& tx) { tx.root().child().reset(); }).throw_if_failed();
  EXPECT_FALSE(payload(*store.read()).child);
  EXPECT_EQ(payload(*retained).child->persistent_id, 2u);
  store.undo();
  EXPECT_EQ(payload(*store.read()).child->persistent_id, 2u);
  store.execute_transaction([](auto& tx) {
    EXPECT_EQ(tx.root().child().emplace(leaf{"replacement"}), 7u);
  }).throw_if_failed();
  EXPECT_EQ(payload(*store.read()).child->persistent_id, 7u);
  store_type restored{document{}};
  restored.load(store.save());
  EXPECT_EQ(child_payload(*payload(*restored.read()).child).text, "replacement");
  EXPECT_EQ(payload(*restored.read()).child->persistent_id, 7u);
  restored.execute_transaction([](auto& tx) {
    EXPECT_EQ(tx.root().children().edit().append(leaf{"appended"}), 8u);
    EXPECT_EQ(tx.root().named().edit().insert("new", leaf{"inserted"}), 9u);
    EXPECT_EQ(tx.root().entries().append(leaf{"entry inserted"}), 10u);
  }).throw_if_failed();
}

// Replacing engagement poisons stale handles, including an absent/present ABA in one transaction.
TEST(managed_optional, stale_engagement_and_transaction_failure) {
  store_type store{sample()};
  const auto before = store.read();
  const auto result = store.execute_transaction([](auto& tx) {
    auto retained = tx.root().child().edit();
    tx.root().child().reset();
    tx.root().child().emplace(leaf{"replacement"});
    EXPECT_THROW(retained.set_text("stale"), std::out_of_range);
  });
  EXPECT_EQ(result.status, managed::transaction_status::failed);
  EXPECT_EQ(managed::detail::encode(*store.read()), managed::detail::encode(*before));
  store.execute_transaction([](auto& tx) {
    EXPECT_EQ(tx.root().child().emplace(leaf{"after failure"}), 8u);
  }).throw_if_failed();
}

// Durable restoration recursively resets engaged child transients while preserving engagement.
TEST(managed_optional, transient_projection) {
  auto initial = sample();
  initial.child->cache = 101;
  initial.children->at(0).cache = 102;
  initial.named->at("present")->cache = 103;
  initial.entries.at(1)->cache = 104;
  initial.value_child->cache = 105;
  store_type store{initial};
  store_type restored{document{}};
  restored.load(store.save());
  EXPECT_EQ(child_payload(*payload(*restored.read()).child).cache, 7u);
  EXPECT_EQ(child_payload(payload(*restored.read()).children->at(0)).cache, 7u);
  EXPECT_EQ(child_payload(*payload(*restored.read()).named->at("present")).cache, 7u);
  EXPECT_EQ(child_payload(*payload(*restored.read()).entries.at(1)).cache, 7u);
  EXPECT_EQ(payload(*restored.read()).value_child->cache, 7u);
  EXPECT_FALSE(payload(*restored.read()).named->at("absent"));
}

// Collaboration distinguishes optional membership from child payload and preserves unrelated edits.
TEST(managed_optional, collaboration_membership_and_selective_merge) {
  store_type store{sample()};
  auto expected = *store.read();
  auto desired = expected;
  child_payload(*payload(desired).child).text = "remote child";
  child_payload(*payload(desired).named->at("present")).text = "remote map child";
  auto current = expected;
  payload(current).scalar = 100;
  managed::detail::collaboration_history_merger merger{1024 * 1024};
  managed::model_traits<document>::merge_collaboration(current, expected, desired, merger);
  EXPECT_EQ(payload(current).scalar, 100u);
  EXPECT_EQ(child_payload(*payload(current).child).text, "remote child");
  EXPECT_EQ(child_payload(*payload(current).named->at("present")).text, "remote map child");
  managed::detail::collaboration_field_collector before{1024 * 1024, 100};
  managed::detail::collaboration_field_collector after{1024 * 1024, 100};
  managed::model_traits<document>::visit_collaboration_fields(expected, before);
  managed::model_traits<document>::visit_collaboration_fields(desired, after);
  EXPECT_EQ(before.fields.at({1, 2}), after.fields.at({1, 2}));
  EXPECT_NE(before.fields.at({2, 1}), after.fields.at({2, 1}));
  desired = expected;
  payload(desired).child.reset();
  current = expected;
  payload(current).child->persistent_id = 99;
  EXPECT_THROW(managed::model_traits<document>::merge_collaboration(current, expected, desired, merger),
               managed::detail::collaboration_history_conflict);
}

// Real authority/replica publication restores engagement selectively and protects remote leaf edits.
TEST(managed_optional, authority_replica_history) {
  managed::collaboration_authority<document> authority{sample(), 1};
  authority.open_session(10);
  authority.open_session(20);
  managed::collaboration_replica<document> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto submit = [&](const auto& proposal) {
    const auto result = authority.submit_change(proposal, proposal.session, 0);
    replica.synchronize(authority.snapshot(), authority.context());
    return result;
  };
  const auto edit = [&](auto session, auto operation, auto&& callback) {
    return submit(replica.propose(session, operation, callback));
  };
  using status = managed::collaboration_status;
  ASSERT_EQ(edit(10u, 1u, [](auto& tx) {
    tx.root().child().edit().set_text("published");
  })->status, status::accepted);
  EXPECT_EQ(child_payload(*payload(*replica.read()).child).text, "published");
  ASSERT_EQ(edit(10u, 2u, [](auto& tx) { tx.root().child().reset(); })->status, status::accepted);
  EXPECT_FALSE(payload(*authority.read()).child);
  ASSERT_EQ(edit(20u, 1u, [](auto& tx) { tx.root().set_scalar(100); })->status, status::accepted);
  ASSERT_EQ(submit(replica.undo(10, 3, 2))->status, status::accepted);
  EXPECT_EQ(payload(*authority.read()).child->persistent_id, 2u);
  EXPECT_EQ(child_payload(*payload(*authority.read()).child).text, "published");
  EXPECT_EQ(payload(*authority.read()).scalar, 100u);
  ASSERT_EQ(edit(10u, 4u, [](auto& tx) {
    tx.root().child().emplace(leaf{"new identity"});
  })->status, status::accepted);
  EXPECT_GT(payload(*authority.read()).child->persistent_id, 6u);
  ASSERT_EQ(edit(20u, 2u, [](auto& tx) {
    tx.root().child().edit().set_text("remote contribution");
  })->status, status::accepted);
  EXPECT_EQ(submit(replica.undo(10, 5, 4))->status, status::conflict);
  EXPECT_EQ(child_payload(*payload(*replica.read()).child).text, "remote contribution");
}
