// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <transaction_collections.hpp>
#include <rohit/managed.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
namespace model = transaction_collections;
namespace managed = rohit::managed;
using store_type = managed::model_store<model::document>;
using traits = managed::model_traits<model::document>;

// Read the root payload in either direct or separate-value generated storage mode.
const auto& payload(const store_type& store) {
  return traits::payload(*store.read());
}
} // namespace

// Ordinary collection edits remain one owner action and survive navigation and managed Save/load.
TEST(transaction_collection, ordinary_elements_history_and_save) {
  store_type store{model::document{}};
  const auto root_id = store.read()->persistent_id;
  store.execute_transaction([](auto& tx) {
    auto points = tx.root().edit_points();
    points.append(model::point{1, 2});
    points.insert(std::size_t{0}, model::point{3, 4});
    points.update_element(std::size_t{1}, [](auto& point) { point.x = 9; });
    EXPECT_EQ(points.size(), 2u);
    EXPECT_TRUE(points.with_element(std::size_t{1}, [](const auto& point) {
      EXPECT_EQ(point.x, 9);
      EXPECT_EQ(point.y, 2);
    }));
    EXPECT_FALSE(points.with_element(std::size_t{9}, [](const auto&) {}));
    EXPECT_TRUE(points.erase(std::size_t{0}));
    EXPECT_FALSE(points.erase(std::size_t{9}));
    auto scores = tx.root().edit_scores();
    scores.insert(std::string{"score"}, 2.0);
    scores.update_element(std::string{"score"}, [](auto& value) { value += 3; });
    EXPECT_TRUE(scores.with_element(std::string{"score"}, [](const auto& value) {
      EXPECT_EQ(value, 5);
    }));
    scores.insert(std::string{"removed"}, 1.0);
    EXPECT_TRUE(scores.erase(std::string{"removed"}));
    EXPECT_FALSE(scores.erase(std::string{"missing"}));
    tx.root().edit_coefficients().update_element(std::size_t{2}, [](auto& value) { value = 7; });
    auto flags = tx.root().edit_flags();
    flags.append(false);
    flags.update_element(std::size_t{0}, [](auto& value) { value = true; });
    EXPECT_TRUE(flags.with_element(std::size_t{0}, [](const auto& value) { EXPECT_TRUE(value); }));
  }).throw_if_failed();
  EXPECT_EQ(store.read()->persistent_id, root_id);
  ASSERT_EQ(payload(store).points.size(), 1u);
  EXPECT_EQ(payload(store).points.front().x, 9);
  EXPECT_EQ(payload(store).scores.at("score"), 5);
  EXPECT_EQ(payload(store).coefficients[2], 7);
  store.undo();
  EXPECT_TRUE(payload(store).points.empty());
  EXPECT_TRUE(payload(store).scores.empty());
  store.redo();
  store_type reopened{model::document{}};
  reopened.load(store.save());
  EXPECT_EQ(reopened.read()->persistent_id, root_id);
  EXPECT_EQ(payload(reopened).points.front().x, 9);
  EXPECT_EQ(payload(reopened).scores.at("score"), 5);
  EXPECT_EQ(payload(reopened).coefficients[2], 7);
  EXPECT_TRUE(payload(reopened).flags.front());
}

// Collection callbacks and invalid selectors poison the isolated action even when caught.
TEST(transaction_collection, failure_and_closed_handle_boundaries) {
  store_type store{model::document{}};
  store_type::outcome_type outcome;
  auto transaction = store.begin_transaction(outcome);
  auto points = transaction.root().edit_points();
  points.append(model::point{1, 2});
  transaction.commit();
  EXPECT_THROW(points.size(), std::logic_error);
  const auto before = store.read();
  const auto failed = store.execute_transaction([](auto& tx) {
    auto points = tx.root().edit_points();
    points.append(model::point{3, 4});
    EXPECT_THROW(points.update_element(std::size_t{99}, [](auto&) {}), std::out_of_range);
  });
  EXPECT_EQ(failed.status, managed::transaction_status::failed);
  EXPECT_EQ(store.read(), before);
  const auto callback_failure = store.execute_transaction([](auto& tx) {
    auto points = tx.root().edit_points();
    EXPECT_THROW(points.with_element(std::size_t{0}, [](const auto&) {
      throw std::runtime_error{"Failed borrowed observer"};
    }), std::runtime_error);
  });
  EXPECT_EQ(callback_failure.status, managed::transaction_status::failed);
  EXPECT_EQ(store.read(), before);
}

// Borrowed entity reads complement identity-safe generated element editors without copying maps.
TEST(transaction_collection, managed_identity_lookup_and_erasure) {
  store_type store{model::document{}};
  store_type::id_type entry_id{};
  store_type::id_type keyed_id{};
  store.execute_transaction([&](auto& tx) {
    entry_id = tx.root().entries().append(model::entry{"first"});
    keyed_id = tx.root().keyed_entries().insert("key", model::entry{"second"});
    tx.root().entries().edit(entry_id).set_text("edited");
    EXPECT_TRUE(tx.root().entries().with_element(entry_id, [](const auto& value) {
      EXPECT_EQ(value.text, "edited");
    }));
    EXPECT_TRUE(tx.root().keyed_entries().with_element(keyed_id, [](const auto& value) {
      EXPECT_EQ(value.text, "second");
    }));
    EXPECT_FALSE(tx.root().entries().with_element(store_type::id_type{}, [](const auto&) {}));
  }).throw_if_failed();
  const auto before = store.read();
  store.execute_transaction([&](auto& tx) {
    tx.root().entries().erase(entry_id);
    tx.root().keyed_entries().erase(keyed_id);
    EXPECT_FALSE(tx.root().entries().with_element(entry_id, [](const auto&) {}));
  }).throw_if_failed();
  EXPECT_TRUE(payload(store).entries.empty());
  store.undo();
  // Navigation decodes a fresh snapshot; durable payload and persistent IDs must match exactly.
  const auto restored = store.read();
  EXPECT_EQ(managed::detail::encode(*restored), managed::detail::encode(*before));
  const auto saved_before_lookup = store.save();
  std::size_t publications{};
  store.set_publication_callback([&](const auto&) { ++publications; });
  const auto lookup = store.execute_transaction([&](auto& tx) {
    EXPECT_EQ(tx.root().entries().edit(entry_id).id(), entry_id);
    EXPECT_EQ(tx.root().keyed_entries().edit(keyed_id).id(), keyed_id);
    EXPECT_TRUE(tx.root().entries().with_element(entry_id, [](const auto& value) {
      EXPECT_EQ(value.text, "edited");
    }));
    EXPECT_TRUE(tx.root().keyed_entries().with_element(keyed_id, [](const auto& value) {
      EXPECT_EQ(value.text, "second");
    }));
    EXPECT_FALSE(tx.root().edit_points().with_element(std::size_t{0}, [](const auto&) {}));
  });
  lookup.throw_if_failed();
  EXPECT_EQ(lookup.status, managed::transaction_status::no_change);
  EXPECT_EQ(publications, 0u);
  EXPECT_EQ(store.read(), restored);
  EXPECT_EQ(store.save(), saved_before_lookup);
  store.redo();
  EXPECT_TRUE(payload(store).entries.empty());
  EXPECT_TRUE(payload(store).keyed_entries.empty());
}

// Transient collection access remains confined to runtime edits and excluded from durable saves.
TEST(transaction_collection, transient_collection_projection) {
  store_type store{model::document{}};
  store.update_runtime([](auto& edit) {
    edit.root().edit_scratch().append(std::uint32_t{17});
    edit.root().edit_scratch().update_element(std::size_t{0}, [](auto& value) { value = 23; });
  }).throw_if_failed();
  ASSERT_EQ(payload(store).scratch.size(), 1u);
  EXPECT_EQ(payload(store).scratch.front(), 23u);
  store_type reopened{model::document{}};
  reopened.load(store.save());
  EXPECT_TRUE(payload(reopened).scratch.empty());
}

// Publication identifies the owner and the same collection action replays from a durable journal.
TEST(transaction_collection, notifications_and_journal_recovery) {
  const auto document_id = managed::make_document_id();
  const auto directory = std::filesystem::current_path() /
      ("journal-test-" + std::to_string(document_id.high) + "-" + std::to_string(document_id.low));
  ASSERT_TRUE(std::filesystem::create_directory(directory));
  const auto path = directory / "collections";
  {
    store_type store{model::document{}};
    const auto root_id = store.read()->persistent_id;
    std::size_t publications{};
    store.set_publication_callback([&](const auto& event) {
      EXPECT_EQ(event.kind, managed::publication_kind::durable_edit);
      EXPECT_EQ(event.changed_ids, (std::vector<store_type::id_type>{root_id}));
      ++publications;
    });
    store.create_journal(path, managed::journal_storage_mode::appended);
    store.execute_transaction([](auto& tx) {
      tx.root().edit_points().append(model::point{11, 12});
      tx.root().edit_scores().insert(std::string{"score"}, 13.0);
    }).throw_if_failed();
    EXPECT_EQ(publications, 1u);
  }
  {
    store_type recovered{model::document{}};
    recovered.recover_journal(path);
    ASSERT_EQ(payload(recovered).points.size(), 1u);
    EXPECT_EQ(payload(recovered).points.front().x, 11);
    EXPECT_EQ(payload(recovered).scores.at("score"), 13);
    recovered.undo();
    EXPECT_TRUE(payload(recovered).points.empty());
    recovered.redo();
    EXPECT_EQ(payload(recovered).points.front().y, 12);
  }
  std::error_code cleanup_error;
  std::filesystem::remove_all(directory, cleanup_error);
  EXPECT_FALSE(cleanup_error);
}
