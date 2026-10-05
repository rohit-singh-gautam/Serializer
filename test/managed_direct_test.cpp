#include <ledger.hpp>
#include <managed_generated.hpp>
#include <rohit/managed.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>
#include <vector>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;
static_assert(std::same_as<store_type::storage_type, ledger>);
static_assert(std::same_as<decltype(ledger::persistent_id), std::uint32_t>);
} // namespace

// New documents have independent namespaces and independently start their object counters at one.
TEST(managed_direct, automatic_document_namespace_and_local_sequence) {
  store_type first{ledger{"First", {{10, {"One", 100}}, {20, {"Two", 200}}}}};
  store_type second{ledger{"Second", {{10, {"Other", 300}}}}};
  EXPECT_TRUE(first.document().high != 0 || first.document().low != 0);
  EXPECT_TRUE(first.document().high != second.document().high ||
              first.document().low != second.document().low);
  EXPECT_EQ(first.read()->persistent_id, 1u);
  EXPECT_EQ(first.read()->entries.at(10).persistent_id, 2u);
  EXPECT_EQ(first.read()->entries.at(20).persistent_id, 3u);
  EXPECT_EQ(second.read()->persistent_id, 1u);
  EXPECT_EQ(second.read()->entries.at(10).persistent_id, 2u);
}

// Nested identified occurrences use one preorder sequence; ordinary occurrences stay owner-managed.
TEST(managed_direct, nested_sequence_and_owned_values) {
  generated_managed::workspace initial;
  initial.drawing.shapes.emplace(10,
                                 design_example::difference{{80, 40, {1, 2}}, {80, 20, {1, 2}}});
  initial.notes.paragraphs = {{"First"}, {"Second"}};
  managed::model_store<generated_managed::workspace> store{initial};
  std::vector<std::uint32_t> ids;
  managed::model_traits<generated_managed::workspace>::visit_entities(
      *store.read(), [&](auto id, auto) { ids.push_back(id); });
  ASSERT_EQ(ids.size(), 11u);
  for (std::size_t index = 0; index < ids.size(); ++index) {
    EXPECT_EQ(ids[index], index + 1);
  }
  EXPECT_EQ(store.read()->template_design.persistent_id, 0u);
  EXPECT_EQ(store.read()->drawing.shapes.at(10).outer.position.persistent_id, 0u);
  EXPECT_NE(store.read()->cursor.position.persistent_id, 0u);
}

// Reload restores namespace, history, and allocator state; deletion/undo never renumber live IDs.
TEST(managed_direct, edit_undo_reload_and_monotonic_allocation) {
  store_type store{ledger{"Ledger", {{10, {"One", 100}}}}};
  const auto original = store.read();
  auto changed = store.execute_transaction([](auto& transaction) {
    auto entry = transaction.root().entries().edit(2);
    entry.set_memo("Updated");
    entry.set_amount_minor_units(200);
  });
  changed.throw_if_failed();
  EXPECT_EQ(store.read()->entries.at(10).persistent_id, 2u);
  EXPECT_EQ(store.read()->entries.at(10).memo, "Updated");
  EXPECT_EQ(original->entries.at(10).memo, "One");
  store.undo();
  EXPECT_EQ(store.read()->entries.at(10).amount_minor_units, 100);
  auto canceled = store.execute_transaction([](auto& transaction) {
    EXPECT_EQ(transaction.root().entries().insert(20, {"Canceled", 300}), 3u);
    transaction.revert();
  });
  EXPECT_EQ(canceled.status, managed::transaction_status::reverted);
  store_type restored{ledger{}};
  restored.load(store.save());
  EXPECT_EQ(restored.document().high, store.document().high);
  EXPECT_EQ(restored.document().low, store.document().low);
  EXPECT_EQ(restored.read()->persistent_id, 1u);
  EXPECT_EQ(restored.clone_value().entries.at(10).persistent_id, 2u);
  auto inserted = restored.execute_transaction([](auto& transaction) {
    EXPECT_EQ(transaction.root().entries().insert(20, {"New", 400}), 4u);
  });
  inserted.throw_if_failed();
  auto deleted = restored.execute_transaction([](auto& transaction) { transaction.root().entries().erase(2); });
  deleted.throw_if_failed();
  EXPECT_EQ(restored.read()->entries.at(20).persistent_id, 4u);
  restored.undo();
  EXPECT_EQ(restored.read()->entries.at(10).persistent_id, 2u);
}

// Direct class codecs include the identity; external ID-free payload codecs remain an explicit opt-in.
TEST(managed_direct, direct_codec_preserves_identity) {
  store_type store{ledger{"Ledger", {{10, {"One", 100}}}}};
  auto copied = managed::detail::decode<ledger>(managed::detail::encode(*store.read()), {});
  EXPECT_EQ(copied.persistent_id, 1u);
  EXPECT_EQ(copied.entries.at(10).persistent_id, 2u);
  EXPECT_EQ(copied.entries.at(10).memo, "One");
}
