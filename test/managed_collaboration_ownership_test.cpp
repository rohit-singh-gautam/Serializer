#include <collaboration_model.hpp>
#include <gtest/gtest.h>
#include <rohit/managed_collaboration.hpp>
#include <utility>

namespace managed = rohit::managed;
namespace records = managed::collaboration;

// Parent deletion and cross-parent relocation cannot hide protected descendants inside a snapshot.
TEST(managed_collaboration, nested_moves_and_parent_deletion_check_descendants) {
  using root = collaboration_test::workspace;
  using authority_type = managed::collaboration_authority<root>;
  bool erase_parent = false;
  root initial{{{1, {"First", {{10, {"One", 100}}}}}, {2, {"Second"}}}};
  authority_type authority{initial, 1, [&](auto& tx, auto) {
                             tx.update([&](auto& value) {
                               if (erase_parent) {
                                 value.ledgers.erase(1);
                               } else {
                                 auto node = value.ledgers.at(1).entries.extract(10);
                                 value.ledgers.at(2).entries.insert(std::move(node));
                               }
                             });
                           }};
  authority.open_session(10);
  authority.open_session(20);
  const auto child_id = authority.read()->ledgers.at(1).entries.at(10).persistent_id;
  records::lock_request request{authority.context(), 10, 1, 1, child_id, 1, 0, 100};
  ASSERT_EQ(authority.change_lock(request, 10, 0)->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.submit_change({authority.context(), 20, 1, 0, {}, {}}, 20, 0)->status,
            managed::collaboration_status::conflict);
  erase_parent = true;
  EXPECT_EQ(authority.submit_change({authority.context(), 20, 2, 0, {}, {}}, 20, 0)->status,
            managed::collaboration_status::conflict);
  EXPECT_EQ(authority.read()->ledgers.size(), 2u);
  EXPECT_EQ(authority.read()->ledgers.at(1).entries.size(), 1u);
}

// A same-owner move must not create overlap between separately granted entity and subtree rights.
TEST(managed_collaboration, relocation_preserves_nonoverlapping_grants) {
  using root = collaboration_test::workspace;
  using authority_type = managed::collaboration_authority<root>;
  root initial{{{1, {"First", {{10, {"One", 100}}}}}, {2, {"Second"}}}};
  authority_type authority{initial, 1, [](auto& tx, auto) {
                             tx.update([](auto& value) {
                               auto node = value.ledgers.at(1).entries.extract(10);
                               value.ledgers.at(2).entries.insert(std::move(node));
                             });
                           }};
  authority.open_session(10);
  const auto child = authority.read()->ledgers.at(1).entries.at(10).persistent_id;
  const auto destination = authority.read()->ledgers.at(2).persistent_id;
  const auto first =
      authority.change_lock({authority.context(), 10, 1, 1, child, 1, 0, 100}, 10, 0);
  const auto second =
      authority.change_lock({authority.context(), 10, 2, 1, destination, 2, 0, 100}, 10, 0);
  ASSERT_EQ(first->status, managed::collaboration_status::accepted);
  ASSERT_EQ(second->status, managed::collaboration_status::accepted);
  records::change_proposal proposal{
      authority.context(),
      10,
      3,
      0,
      {},
      {{child, first->grant.generation}, {destination, second->grant.generation}}};
  EXPECT_EQ(authority.submit_change(proposal, 10, 0)->status,
            managed::collaboration_status::conflict);
  EXPECT_EQ(authority.read()->ledgers.at(1).entries.size(), 1u);
  EXPECT_TRUE(authority.read()->ledgers.at(2).entries.empty());
}
