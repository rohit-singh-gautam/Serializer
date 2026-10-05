#include <gtest/gtest.h>
#include <managed_generated.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <type_traits>

// Store-associated synchronization preserves wide local handles and profile-renamed generated editors.
TEST(managed_profile, local_collaboration_with_uint64_ids) {
  namespace managed = rohit::managed;
  using root = generated_managed::Workspace;
  managed::collaboration_authority<root> authority{root{}, 1};
  auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(
      authority.save_document(), {});
  envelope.allocated_id = std::numeric_limits<std::uint32_t>::max();
  authority.load_document(managed::detail::encode(envelope));
  authority.open_session(1);
  managed::collaboration_transport transport{authority, std::uint64_t{1}};
  managed::model_store<root> store{root{}};
  store.collaborate(std::uint64_t{1}).bind(transport);
  store.synchronize();
  store.execute_transaction([](auto& edit) { edit.root().Notes().Paragraphs().append({"Local"}); })
      .throw_if_failed();
  const auto id = store.read()->notes_.paragraphs_.front().persistent_id;
  EXPECT_GT(id, std::numeric_limits<std::uint32_t>::max());
  store.synchronize();
  store.undo();
  store.redo();
  store.synchronize();
  EXPECT_EQ(store.read()->notes_.paragraphs_.front().persistent_id, id);
  EXPECT_EQ(authority.read()->notes_.paragraphs_.front().text_, "Local");
  EXPECT_EQ(store.collaboration().pending_count(), 0u);
}

// Generated traversal follows naming profiles and preserves identities above the uint32 limit.
TEST(managed_profile, collaboration_with_uint64_and_disabled_history) {
  namespace managed = rohit::managed;
  using root = generated_managed::Workspace;
  using authority_type = managed::collaboration_authority<root, managed::history_mode::disabled>;
  authority_type authority{root{}, 1};
  auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(
      authority.save_document(), {});
  envelope.allocated_id = std::numeric_limits<std::uint32_t>::max();
  authority.load_document(managed::detail::encode(envelope));
  authority.open_session(1);
  managed::collaboration_replica<root> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto proposal = replica.propose(1, 1, [](auto& tx) {
    auto paragraphs = tx.root().Notes().Paragraphs();
    paragraphs.edit(paragraphs.append({"Draft"})).SetText("Shared");
  });
  const auto result = authority.submit_change(proposal, 1, 0);
  ASSERT_EQ(result->status, managed::collaboration_status::accepted);
  replica.apply_accepted_change(*result->accepted, authority.context());
  EXPECT_EQ(replica.read()->notes_.paragraphs_.front().text_, "Shared");
  EXPECT_GT(replica.read()->notes_.paragraphs_.front().persistent_id,
            std::numeric_limits<std::uint32_t>::max());
  const auto identity = replica.read()->notes_.paragraphs_.front().persistent_id;
  const auto undo = authority.submit_change(replica.undo(1, 2), 1, 0);
  EXPECT_EQ(undo->status, managed::collaboration_status::failed);
  EXPECT_EQ(authority.sequence(), 1u);
  EXPECT_EQ(authority.read()->notes_.paragraphs_.front().persistent_id, identity);
}

// Profile-renamed payloads retain fixed runtime metadata and true 64-bit persistent identities.
TEST(managed_profile, google_uint64_generation_and_round_trip) {
  namespace managed = rohit::managed;
  using value_type = generated_managed::Workspace;
  using store_type = managed::model_store<value_type, managed::history_mode::disabled>;
  static_assert(std::same_as<store_type::id_type, std::uint64_t>);
  managed::store_options options;
  store_type store{value_type{}, {5, 6}, options};
  auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(store.save(), {});
  envelope.allocated_id = std::numeric_limits<std::uint32_t>::max();
  store.load(managed::detail::encode(envelope));
  std::uint64_t shape_id{};
  const auto result = store.execute_transaction([&](auto& transaction) {
    auto shapes = transaction.root().Drawing().Shapes();
    shape_id = shapes.insert(100, {{80, 40, {1, 2}}, {80, 20, {1, 2}}});
    auto outer = shapes.edit(shape_id).Outer();
    outer.SetHeight(120);
    outer.Position().SetX(10);
    transaction.root().Cursor().Position().SetY(20);
    auto paragraphs = transaction.root().Notes().Paragraphs();
    paragraphs.edit(paragraphs.append({"Text"})).SetText("Updated");
  });
  result.throw_if_failed();
  EXPECT_GT(shape_id, std::numeric_limits<std::uint32_t>::max());
  store_type restored{value_type{}, {7, 8}, options};
  restored.load(store.save());
  EXPECT_EQ(restored.read()->drawing_.shapes_.at(100).persistent_id, shape_id);
  EXPECT_EQ(restored.clone_value().drawing_.shapes_.at(100).outer_.position_.x_, 10);
  EXPECT_EQ(restored.clone_value().cursor_.position_.y_, 20);
  EXPECT_EQ(restored.clone_value().notes_.paragraphs_.front().text_, "Updated");
}

// uint64 allocation reservations survive process-style reopen above the uint32 range.
TEST(managed_profile, journal_preserves_wide_reservations) {
  namespace managed = rohit::managed;
  using value_type = generated_managed::Workspace;
  using store_type = managed::model_store<value_type, managed::history_mode::disabled>;
  const auto namespace_id = managed::make_document_id();
  const auto directory = std::filesystem::current_path() /
      ("wide-journal-" + std::to_string(namespace_id.high));
  ASSERT_TRUE(std::filesystem::create_directory(directory));
  const auto path = directory / "document";
  {
    store_type store{value_type{}};
    auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(store.save(), {});
    envelope.allocated_id = std::numeric_limits<std::uint32_t>::max();
    store.load(managed::detail::encode(envelope));
    store.create_journal(path, managed::journal_storage_mode::sidecar);
    store.execute_transaction([](auto& tx) {
      EXPECT_EQ(tx.create_id(), std::uint64_t{std::numeric_limits<std::uint32_t>::max()} + 1);
      tx.revert();
    }).throw_if_failed();
  }
  {
    store_type store{value_type{}};
    store.recover_journal(path);
    store.execute_transaction([](auto& tx) {
      const auto id = tx.root().Notes().Paragraphs().append({"Recovered"});
      EXPECT_EQ(id, std::uint64_t{std::numeric_limits<std::uint32_t>::max()} + 2);
    }).throw_if_failed();
    store.save_journal();
  }
  {
    store_type store{value_type{}};
    store.recover_journal(path);
    EXPECT_EQ(store.read()->notes_.paragraphs_.front().text_, "Recovered");
  }
  std::error_code ignored;
  std::filesystem::remove_all(directory, ignored);
}
