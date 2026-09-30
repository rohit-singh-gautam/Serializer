#include <gtest/gtest.h>
#include <managed_generated.hpp>
#include <rohit/managed.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

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
