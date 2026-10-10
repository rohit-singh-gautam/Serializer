#include <gtest/gtest.h>
#include <managed_document_features.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

namespace {
namespace model = managed_document_features;
namespace managed = rohit::managed;
using traits = managed::model_traits<model::document_data>;
using author_traits = managed::model_traits<model::author_data>;
using review_traits = managed::model_traits<model::review_data>;
using store_type = managed::model_store<model::document_data>;

// Expose the separated payload only through its generated immutable model trait.
const auto& payload(const store_type& store) { return traits::payload(*store.read()); }
} // namespace

// Active base occurrences use their own separated wrapper and preserve IDs across restoration.
TEST(managed_document_separate, base_identity_and_variant_conversion) {
  store_type store{model::document_data{}};
  const auto root = store.read()->persistent_id;
  const auto author = static_cast<const author_traits::storage_type&>(payload(store)).persistent_id;
  const auto review = static_cast<const review_traits::storage_type&>(payload(store)).persistent_id;
  EXPECT_NE(root, author);
  EXPECT_NE(root, review);
  EXPECT_NE(author, review);
  store.execute_transaction([](auto& tx) {
    tx.root().author().set_name("Ada");
    tx.root().review().set_status("accepted");
    tx.root().template edit_content<0>().set_text("Reviewed text");
  }).throw_if_failed();
  EXPECT_EQ(store.clone_value().title, "");
  EXPECT_EQ(static_cast<const model::review_data&>(store.clone_value()).status, "accepted");
  EXPECT_EQ(std::get<0>(store.clone_value().content).text, "Reviewed text");
  store.execute_transaction([](auto& tx) {
    model::attachment_data file{};
    file.filename = "review.txt";
    tx.root().template emplace_content<1>(std::move(file));
  }).throw_if_failed();
  const auto file_id = std::get<1>(payload(store).content).persistent_id;
  EXPECT_EQ(std::get<1>(store.clone_value().content).filename, "review.txt");
  store.undo();
  EXPECT_EQ(payload(store).content.index(), 0u);
  store.redo();
  EXPECT_EQ(std::get<1>(payload(store).content).persistent_id, file_id);
  store_type loaded{model::document_data{}};
  loaded.load(store.save());
  EXPECT_EQ(static_cast<const author_traits::storage_type&>(payload(loaded)).persistent_id, author);
  EXPECT_EQ(static_cast<const review_traits::storage_type&>(payload(loaded)).persistent_id, review);
}

// Memory-only projection crosses separated bases and active alternative wrappers without wire changes.
TEST(managed_document_separate, transient_fields_in_bases_and_variants) {
  store_type store{model::document_data{}};
  const auto saved = store.save();
  store.update_runtime([](auto& runtime) {
    runtime.root().set_preview_cache(17);
    runtime.root().author().set_lookup_cache(19);
    runtime.root().template edit_content<0>().set_word_cache(23);
  }).throw_if_failed();
  EXPECT_EQ(store.save(), saved);
  auto copied = store.clone_value();
  EXPECT_EQ(copied.preview_cache, 17u);
  EXPECT_EQ(static_cast<const model::author_data&>(copied).lookup_cache, 19u);
  EXPECT_EQ(std::get<0>(copied.content).word_cache, 23u);
  auto reset = store.clone_value({managed::runtime_copy_policy::reset_to_defaults});
  EXPECT_EQ(reset.preview_cache, 0u);
  EXPECT_EQ(static_cast<const model::author_data&>(reset).lookup_cache, 0u);
  EXPECT_EQ(std::get<0>(reset.content).word_cache, 0u);
  store.execute_transaction([](auto& tx) { tx.root().set_title("Published"); }).throw_if_failed();
  EXPECT_EQ(payload(store).preview_cache, 0u);
}

// Deep separated storage conversion preserves the fixed extent and ordinary clone payloads.
TEST(managed_document_separate, fixed_managed_array_conversion) {
  using root = model::fixed_review_document;
  managed::model_store<root> store{root{}};
  const auto child_id = store.read()->value.paragraphs[1].persistent_id;
  store.execute_transaction([&](auto& tx) {
    tx.root().paragraphs().edit(child_id).set_text("Fixed child value");
  }).throw_if_failed();
  EXPECT_EQ(store.clone_value().paragraphs[1].text, "Fixed child value");
  managed::model_store<root> reopened{root{}};
  reopened.load(store.save());
  EXPECT_EQ(reopened.read()->value.paragraphs.size(), 2u);
  EXPECT_EQ(reopened.read()->value.paragraphs[1].persistent_id, child_id);
}

// A managed-capable generic argument remains a value unless its occurrence is managed.
TEST(managed_document_separate, managed_capable_generic_argument) {
  using root = model::text_annotation;
  managed::model_store<root> store{root{}};
  store.execute_transaction([](auto& tx) {
    tx.root().note().set_text("An annotated paragraph");
  }).throw_if_failed();
  const auto saved = store.save();
  store.update_runtime([](auto& runtime) {
    runtime.root().note().set_word_cache(31);
  }).throw_if_failed();
  EXPECT_EQ(store.save(), saved);
  EXPECT_EQ(store.clone_value().note.word_cache, 31u);
  store.undo();
  EXPECT_TRUE(store.clone_value().note.text.empty());
}

// Separated wrappers apply payload lifetimes to child identities and active alternative editors.
TEST(managed_document_separate, versioned_children_and_alternatives) {
  using root = model::versioned_entity_document;
  managed::model_store<root> store{root{}};
  EXPECT_EQ(store.read()->value.retired_text.persistent_id, 0u);
  const auto current_id = store.read()->value.current_text.persistent_id;
  EXPECT_NE(current_id, 0u);
  const auto failed = store.execute_transaction([](auto& tx) {
    tx.root().retired_text();
  });
  EXPECT_EQ(failed.status, managed::transaction_status::failed);
  store.execute_transaction([](auto& tx) {
    tx.root().current_text().set_text("Live separated child");
  }).throw_if_failed();
  managed::model_store<root> reopened{root{}};
  reopened.load(store.save());
  EXPECT_EQ(reopened.read()->value.current_text.persistent_id, current_id);
  EXPECT_EQ(reopened.clone_value().current_text.text, "Live separated child");
  reopened.undo();
  EXPECT_TRUE(reopened.clone_value().current_text.text.empty());

  using choice = model::versioned_variant_document;
  managed::model_store<choice> choices{choice{}};
  EXPECT_EQ(std::get<0>(choices.read()->value.retired_content).persistent_id, 0u);
  EXPECT_NE(std::get<0>(choices.read()->value.current_content).persistent_id, 0u);
  const auto rejected = choices.execute_transaction([](auto& tx) {
    tx.root().template edit_retired_content<0>();
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  choices.execute_transaction([](auto& tx) {
    tx.root().template edit_current_content<0>().set_text("Live separated alternative");
  }).throw_if_failed();
  EXPECT_EQ(std::get<0>(choices.clone_value().current_content).text, "Live separated alternative");
}
