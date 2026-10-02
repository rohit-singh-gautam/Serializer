#include <gtest/gtest.h>
#include <managed_generated.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_collaboration.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using root = generated_managed::workspace;
using store_type = managed::model_store<root>;

// Detect metadata without requiring invalid member access in a non-dependent expression.
template <typename Value>
concept has_persistent_id = requires(Value value) { value.persistent_id; };
static_assert(!has_persistent_id<design_example::point>);
static_assert(!has_persistent_id<design_example::cylinder>);
static_assert(!has_persistent_id<design_example::design>);
static_assert(has_persistent_id<managed::model_traits<design_example::point>::storage_type>);
static_assert(std::same_as<store_type::id_type, std::uint32_t>);

// Build a graph containing managed and ordinary occurrences of the same eligible types.
root initial_value() {
  root value;
  value.drawing.shapes.emplace(100, design_example::difference{{80, 40, {1, 2}}, {80, 20, {1, 2}}});
  value.template_design = value.drawing;
  value.cursor.position = {5, 6};
  value.notes = {"Notes", {{"First"}, {"Second"}}};
  return value;
}
} // namespace

// Separate-value wrappers preserve ownership boundaries and replicate generated nested edits.
TEST(managed_generated, collaboration_with_separate_values_and_tree_labels) {
  using authority_type = managed::collaboration_authority<root, managed::history_mode::tree,
                                                          managed::history_labels::enabled>;
  authority_type authority{initial_value(), 1};
  authority.open_session(1);
  managed::collaboration_replica<root> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto proposal = replica.propose(1, 1, [](auto& tx) {
    tx.root().cursor().position().set_x(99);
    auto paragraphs = tx.root().notes().paragraphs();
    const auto id = paragraphs.append({"Draft"});
    paragraphs.edit(id).set_text("Shared");
  });
  const auto result = authority.submit_change(proposal, 1, 0);
  ASSERT_EQ(result->status, managed::collaboration_status::accepted);
  EXPECT_EQ(replica.apply_accepted_change(*result->accepted, authority.context()),
            managed::replication_status::applied);
  EXPECT_EQ(replica.read()->value.cursor.value.position.value.x, 99);
  EXPECT_EQ(replica.read()->value.notes.value.paragraphs.back().value.text, "Shared");
  EXPECT_EQ(replica.read()->value.template_design.shapes.at(100).outer.position.x, 1);
  const auto undo = authority.submit_change(replica.undo(1, 2), 1, 0);
  ASSERT_EQ(undo->status, managed::collaboration_status::accepted);
  replica.apply_accepted_change(*undo->accepted, authority.context());
  EXPECT_EQ(replica.read()->value.cursor.value.position.value.x, 5);
  EXPECT_EQ(replica.read()->value.notes.value.paragraphs.size(), 2u);
  const auto redo = authority.submit_change(replica.redo(1, 3), 1, 0);
  ASSERT_EQ(redo->status, managed::collaboration_status::accepted);
  replica.apply_accepted_change(*redo->accepted, authority.context());
  EXPECT_EQ(replica.read()->value.notes.value.paragraphs.back().value.text, "Shared");
}

// Attached collaboration uses the same generated separated-value editors and normal store transactions.
TEST(managed_generated, local_collaboration_with_separate_values) {
  managed::collaboration_authority<root> authority{initial_value(), 1};
  authority.open_session(1);
  managed::collaboration_transport transport{authority, std::uint64_t{1}};
  store_type store{root{}};
  store.collaborate(std::uint64_t{1}).bind(transport);
  store.synchronize();
  store
      .execute_transaction([](auto& edit) {
        edit.root().cursor().position().set_x(99);
        edit.root().notes().paragraphs().append({"Local"});
      })
      .throw_if_failed();
  EXPECT_EQ(store.read()->value.cursor.value.position.value.x, 99);
  store.undo();
  store.redo();
  store.synchronize();
  EXPECT_EQ(authority.read()->value.cursor.value.position.value.x, 99);
  EXPECT_EQ(authority.read()->value.notes.value.paragraphs.back().value.text, "Local");
}

// Identity follows managed occurrence boundaries, not mere eligibility or application map keys.
TEST(managed_generated, identity_boundaries_and_value_editing) {
  store_type store{initial_value(), {1, 2}};
  const auto before = store.read();
  std::size_t entities{};
  managed::model_traits<root>::visit_entities(*before, [&](auto, auto) { ++entities; });
  EXPECT_EQ(entities, 11u);
  static_assert(!has_persistent_id<decltype(before->value.template_design.shapes.begin()->second)>);
  const auto shape_id = before->value.drawing.value.shapes.at(100).persistent_id;
  const auto cylinder_id = before->value.drawing.value.shapes.at(100).value.outer.persistent_id;
  const auto point_id = before->value.cursor.value.position.persistent_id;
  auto result =
      store.execute_transaction([shape_id, point_id](auto& transaction) {
        auto shape = transaction.root().drawing().shapes().edit(shape_id);
        auto outer = shape.outer();
        outer.set_height(120);
        outer.position().set_x(10);
        outer.position().set_y(20);
        shape.hole().set_diameter(25);
        EXPECT_EQ(transaction.root().cursor().position().id(), point_id);
        transaction.root().cursor().position().set_x(7);
        // This ordinary design contains dormant managed annotations: replace it as a value.
        auto plain = transaction.root().get_template_design();
        plain.shapes.at(100).outer.height = 9;
        transaction.root().set_template_design(std::move(plain));
      });
  result.throw_if_failed();
  EXPECT_EQ(result.status, managed::transaction_status::committed);
  EXPECT_EQ(store.read()->value.drawing.value.shapes.at(100).value.outer.persistent_id,
            cylinder_id);
  EXPECT_EQ(store.clone_value().drawing.shapes.at(100).outer.position.x, 10);
  EXPECT_EQ(before->value.drawing.value.shapes.at(100).value.outer.value.height, 80);
  store.undo();
  EXPECT_EQ(store.clone_value().drawing.shapes.at(100).outer.position.x, 1);
  EXPECT_EQ(store.clone_value().template_design.shapes.at(100).outer.height, 80);
  EXPECT_EQ(store.clone_value().cursor.position.x, 5);
}

// Editor handles survive guard moves but detect completion and destruction without dangling access.
TEST(managed_generated, editor_lifetime_and_move) {
  store_type store{initial_value(), {1, 2}};
  managed::transaction_outcome outcome;
  using editor_type = decltype(std::declval<store_type::transaction&>().root());
  std::optional<editor_type> saved;
  {
    auto first = store.begin_transaction(outcome);
    saved.emplace(first.root());
    auto second = std::move(first);
    saved->notes().set_title("Changed");
    second.commit();
    EXPECT_THROW(saved->notes().set_title("Too late"), std::logic_error);
  }
  EXPECT_THROW(saved->notes().set_title("Destroyed"), std::logic_error);
  outcome.throw_if_failed();
  EXPECT_EQ(store.clone_value().notes.title, "Changed");
}

// Array editors resolve by ID across reallocations; erased handles poison an entire transaction.
TEST(managed_generated, array_insert_erase_and_stale_editor) {
  store_type store{initial_value(), {1, 2}};
  const auto first_id = store.read()->value.notes.value.paragraphs.front().persistent_id;
  std::uint32_t added_id{};
  auto result = store.execute_transaction([&](auto& transaction) {
    auto paragraphs = transaction.root().notes().paragraphs();
    auto first = paragraphs.edit(first_id);
    added_id = paragraphs.append({"Third"});
    first.set_text("Updated first");
    paragraphs.edit(added_id).set_text("Updated third");
  });
  result.throw_if_failed();
  EXPECT_EQ(store.clone_value().notes.paragraphs.back().text, "Updated third");
  managed::transaction_outcome failed;
  {
    auto transaction = store.begin_transaction(failed);
    auto paragraphs = transaction.root().notes().paragraphs();
    auto first = paragraphs.edit(first_id);
    paragraphs.erase(first_id);
    EXPECT_THROW(first.set_text("Removed"), std::out_of_range);
  }
  EXPECT_EQ(failed.status, managed::transaction_status::failed);
  EXPECT_EQ(store.clone_value().notes.paragraphs.size(), 3u);
}

// Tree history retains deleted objects and abandoned futures with their original identities.
TEST(managed_generated, deleted_entities_tree_reload_and_scoped_commit) {
  managed::store_options options;
  using tree_store = managed::model_store<root, managed::history_mode::tree>;
  tree_store store{initial_value(), {1, 2}, options};
  const auto old_id = store.read()->value.drawing.value.shapes.at(100).persistent_id;
  tree_store::outcome_type removed;
  {
    auto transaction = store.begin_transaction(removed);
    transaction.root().drawing().shapes().erase(old_id);
  }
  removed.throw_if_failed();
  EXPECT_TRUE(store.clone_value().drawing.shapes.empty());
  store.undo();
  std::uint32_t replacement_id{};
  const auto branched = store.execute_transaction([&](auto& transaction) {
    auto shapes = transaction.root().drawing().shapes();
    replacement_id = shapes.insert(200, {{60, 30, {0, 0}}, {60, 10, {0, 0}}});
  });
  branched.throw_if_failed();
  EXPECT_GT(replacement_id, old_id);
  tree_store restored{root{}, {3, 4}, options};
  restored.load(store.save());
  EXPECT_EQ(restored.read()->value.drawing.value.shapes.at(100).persistent_id, old_id);
  restored.undo();
  EXPECT_EQ(restored.redo_children().size(), 2u);
  restored.redo(removed.revision);
  EXPECT_TRUE(restored.clone_value().drawing.shapes.empty());
}

// Catching an invalid collection operation cannot commit earlier edits from the same action.
TEST(managed_generated, invalid_map_operations_are_atomic) {
  store_type store{initial_value(), {1, 2}};
  const auto id = store.read()->value.drawing.value.shapes.at(100).persistent_id;
  auto result = store.execute_transaction([&](auto& transaction) {
    auto shapes = transaction.root().drawing().shapes();
    shapes.edit(id).outer().set_height(7);
    EXPECT_THROW(shapes.insert(100, {}), std::invalid_argument);
  });
  EXPECT_EQ(result.status, managed::transaction_status::failed);
  EXPECT_EQ(store.clone_value().drawing.shapes.at(100).outer.height, 80);
  result = store.execute_transaction([](auto& transaction) {
    EXPECT_THROW(transaction.root().notes().paragraphs().edit(1), std::out_of_range);
  });
  EXPECT_EQ(result.status, managed::transaction_status::failed);
}

// The opt-in values/storage representation journals ownership edits and complete retained snapshots.
TEST(managed_generated, separate_values_journal_round_trip) {
  const auto id = managed::make_document_id();
  const auto directory = std::filesystem::current_path() / ("split-journal-" + std::to_string(id.high));
  ASSERT_TRUE(std::filesystem::create_directory(directory));
  const auto path = directory / "document";
  std::vector<std::uint8_t> expected;
  {
    store_type store{initial_value()};
    store.create_journal(path);
    store.execute_transaction([](auto& tx) {
      tx.root().notes().paragraphs().append({"Journaled paragraph"});
      tx.root().cursor().position().set_x(42);
    }).throw_if_failed();
    expected = store.save();
  }
  {
    store_type store{root{}};
    store.recover_journal(path);
    EXPECT_EQ(store.save(), expected);
    EXPECT_EQ(store.clone_value().cursor.position.x, 42);
    store.undo();
    EXPECT_EQ(store.clone_value().cursor.position.x, 5);
    EXPECT_FALSE(store.journal_dirty());
    store.redo();
    EXPECT_EQ(store.clone_value().notes.paragraphs.back().text, "Journaled paragraph");
  }
  std::error_code ignored;
  std::filesystem::remove_all(directory, ignored);
}
