#include <gtest/gtest.h>
#include <managed_generated.hpp>
#include <rohit/managed.hpp>

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

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
