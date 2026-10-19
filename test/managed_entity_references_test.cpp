// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <managed_entity_references.hpp>
#include <gtest/gtest.h>

#include <concepts>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
namespace managed = rohit::managed;
namespace serializer = rohit::serializer;
using root = entity_graph::document;
using node = entity_graph::node;
using other = entity_graph::other;
using traits = managed::model_traits<root>;
using node_traits = managed::model_traits<node>;
using store_type = managed::model_store<root>;
using token = managed::managed_ref<node, typename traits::id_type>;
using other_token = managed::managed_ref<other, typename traits::id_type>;
static_assert(!std::constructible_from<token, typename traits::id_type>);
static_assert(!std::assignable_from<token&, other_token>);
static_assert(!std::assignable_from<std::optional<token>&, other_token>);
static_assert(std::same_as<decltype(token{}.id()), typename traits::id_type>);
static_assert(node_traits::class_id == 101U);
static_assert(!managed::detail::edit_channel<typename managed::model_traits<other>::storage_type,
    typename traits::id_type>::has_reference_scope);

// Start with absent optional edges; linking occurs only against assigned managed identities.
root initial() {
  root result{};
  result.nodes.resize(2U);
  result.nodes[0].label = "first";
  result.nodes[1].label = "second";
  result.ordinary_id = 1234U;
  return result;
}

// Exercise the same four public native codecs through the token's additive metadata envelope.
template <template <serializer::serialize_type> class Protocol>
void round_trip(const token& reference) {
  rohit::full_stream_auto_alloc output{};
  serializer::serialize_to<Protocol>(output, reference);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  EXPECT_EQ((serializer::deserialize_exact<token, Protocol>(input)), reference);
}

// Match generated policy metadata while retaining support for old handwritten fixed-width traits.
template <typename ModelTraits>
constexpr bool compact_ids() {
  if constexpr (requires { ModelTraits::id_encoding; }) {
    using encoding_type = std::remove_cv_t<decltype(ModelTraits::id_encoding)>;
    return ModelTraits::id_encoding == encoding_type::compact_varint;
  } else { return false; }
}

// Verify the real synthesized identity field, including compact boundaries and full-width values.
template <template <serializer::serialize_type> class Protocol>
void persistent_id_boundaries() {
  using other_traits = managed::model_traits<other>;
  using storage_type = typename other_traits::storage_type;
  using id_type = typename other_traits::id_type;
  storage_type zero{};
  rohit::full_stream_auto_alloc baseline{};
  serializer::serialize_to<Protocol>(baseline, zero);
  const auto base_id_bytes = compact_ids<other_traits>() ? 1U : sizeof(id_type);
  const auto check = [&](id_type id) {
    storage_type value{};
    value.persistent_id = id;
    rohit::full_stream_auto_alloc output{};
    serializer::serialize_to<Protocol>(output, value);
    constexpr auto varint_payload_bits = 7U;
    const auto bit_count = std::bit_width(id);
    const auto compact_bytes = id == 0U ? 1U : (bit_count + varint_payload_bits - 1U) / varint_payload_bits;
    const auto id_bytes = compact_ids<other_traits>() ? compact_bytes : sizeof(id_type);
    EXPECT_EQ(output.current_offset(), baseline.current_offset() - base_id_bytes + id_bytes);
    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    const auto decoded = serializer::deserialize_exact<storage_type, Protocol>(input);
    EXPECT_EQ(decoded.persistent_id, id);
  };
  const std::array<id_type, 6> values{0U, 127U, 128U, 16383U, 16384U, std::numeric_limits<id_type>::max()};
  for (const auto id : values) { check(id); }
  if constexpr (sizeof(id_type) > sizeof(std::uint32_t)) {
    check(static_cast<id_type>(std::numeric_limits<std::uint32_t>::max()) + id_type{1U});
  }
}
} // namespace

// A typed token resolves one selected snapshot and retains the store's native object-ID width.
TEST(managed_entity_references, typed_envelopes_and_complete_candidate_cycles) {
  store_type store{initial(), {41U, 42U}};
  const auto& payload = traits::payload(*store.read());
  const auto first = payload.nodes[0].persistent_id;
  const auto second = payload.nodes[1].persistent_id;
  const auto catalog = store.registry();
  const auto first_ref = catalog.reference<node>(first);
  const auto second_ref = catalog.reference<node>(second);
  EXPECT_EQ(catalog.resolve(first_ref).value().label, "first");
  EXPECT_EQ(catalog.find<node>(first).get_id(), first_ref);
  EXPECT_THROW((void)catalog.reference<other>(first), managed::registry_error);
  round_trip<serializer::json>(first_ref);
  round_trip<serializer::binary_none>(first_ref);
  round_trip<serializer::binary_integer>(first_ref);
  round_trip<serializer::binary_string>(first_ref);
  store.execute_transaction([&](auto& tx) {
    const auto registry = tx.registry();
    EXPECT_EQ(registry.template edit<node>(first).get_id(), first_ref);
    const auto owner = registry.template reference<root>(store.read()->persistent_id);
    tx.update([&](auto& storage) {
      auto& value = traits::payload(storage);
      node_traits::payload(value.nodes[0]).next = second_ref;
      node_traits::payload(value.nodes[1]).next = first_ref;
      node_traits::payload(value.nodes[0]).owner = owner;
      value.info.links = {first_ref, second_ref};
      value.info.named_links.emplace(7U, first_ref);
    });
  }).throw_if_failed();
  EXPECT_EQ(store.registry().resolve(*node_traits::payload(traits::payload(*store.read()).nodes[0]).next).id(), second);
  EXPECT_EQ(traits::payload(*store.read()).ordinary_id, 1234U);
  const auto bytes = store.save();
  store_type loaded{initial(), {51U, 52U}};
  loaded.load(bytes);
  EXPECT_EQ(loaded.registry().resolve(first_ref).value().label, "first");
  store.undo();
  EXPECT_FALSE(node_traits::payload(traits::payload(*store.read()).nodes[0]).next);
  store.redo();
  EXPECT_EQ(store.registry().resolve(second_ref).value().label, "second");
}

// Foreign/deleted targets reject before publication, including assignments through raw storage.
TEST(managed_entity_references, foreign_and_dangling_edges_leave_published_state_intact) {
  store_type store{initial(), {61U, 62U}};
  store_type foreign{initial(), {71U, 72U}};
  const auto first = traits::payload(*store.read()).nodes[0].persistent_id;
  const auto foreign_first = traits::payload(*foreign.read()).nodes[0].persistent_id;
  const auto reference = store.registry().reference<node>(first);
  const auto foreign_ref = foreign.registry().reference<node>(foreign_first);
  EXPECT_THROW((void)store.registry().resolve(foreign_ref), managed::registry_error);
  const auto before = store.save();
  const auto foreign_outcome = store.execute_transaction([&](auto& tx) {
    tx.update([&](auto& storage) { traits::payload(storage).info.links.push_back(foreign_ref); });
  });
  EXPECT_EQ(foreign_outcome.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), before);
  const auto deleted_outcome = store.execute_transaction([&](auto& tx) {
    tx.update([&](auto& storage) {
      auto& value = traits::payload(storage);
      value.info.links.push_back(reference);
      value.nodes.erase(value.nodes.begin());
    });
  });
  EXPECT_EQ(deleted_outcome.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), before);
}

// Required reciprocal references can be initialized against all assigned IDs before the first snapshot.
TEST(managed_entity_references, required_reference_bootstrap_and_budget) {
  using required_root = entity_graph::required_document;
  using required_node = entity_graph::required_node;
  using required_traits = managed::model_traits<required_root>;
  using required_node_traits = managed::model_traits<required_node>;
  using required_store = managed::model_store<required_root>;
  required_root value{};
  value.nodes.resize(2U);
  EXPECT_THROW((void)required_store(value, {81U, 82U}), std::invalid_argument);
  required_store initialized{value, {81U, 82U}, {}, {}, [](auto& storage, const auto& references) {
    auto& nodes = required_traits::payload(storage).nodes;
    required_node_traits::payload(nodes[0]).next = references.template reference<required_node>(nodes[1].persistent_id);
    required_node_traits::payload(nodes[1]).next = references.template reference<required_node>(nodes[0].persistent_id);
  }};
  const auto& nodes = required_traits::payload(*initialized.read()).nodes;
  EXPECT_EQ(initialized.registry().resolve(required_node_traits::payload(nodes[0]).next).id(), nodes[1].persistent_id);
  managed::store_options limits{};
  limits.decode.max_work_units = 1U;
  EXPECT_THROW((void)store_type(initial(), {91U, 92U}, limits), std::length_error);
}

// Cross-document import changes only owning IDs and declared internal tokens in both storage profiles.
TEST(managed_entity_references, import_rewrites_self_edges_and_rejects_unmapped_external_targets) {
  store_type source{initial(), {111U, 112U}};
  const auto first = traits::payload(*source.read()).nodes[0].persistent_id;
  const auto second = traits::payload(*source.read()).nodes[1].persistent_id;
  const auto first_ref = source.registry().reference<node>(first);
  source.execute_transaction([&](auto& tx) {
    tx.update([&](auto& storage) {
      node_traits::payload(traits::payload(storage).nodes[0]).next = first_ref;
    });
  }).throw_if_failed();
  store_type destination{initial(), {121U, 122U}};
  typename traits::id_type imported_id{};
  destination.execute_transaction([&](auto& tx) {
    auto storage = traits::payload(*source.read()).nodes[0];
    imported_id = tx.registry().template import_storage<node>({111U, 112U}, std::move(storage),
        [](auto& candidate, auto imported) { traits::payload(candidate).nodes.push_back(std::move(imported)); });
  }).throw_if_failed();
  const auto imported = destination.registry().find<node>(imported_id);
  ASSERT_TRUE(imported.value().next);
  EXPECT_EQ(imported.value().next->document().high, 121U);
  EXPECT_EQ(imported.value().next->id(), imported_id);
  EXPECT_EQ(destination.registry().resolve(*imported.value().next).id(), imported_id);
  const auto external_ref = source.registry().reference<node>(second);
  source.execute_transaction([&](auto& tx) {
    tx.update([&](auto& storage) {
      node_traits::payload(traits::payload(storage).nodes[0]).next = external_ref;
    });
  }).throw_if_failed();
  const auto before = destination.read();
  const auto rejected = destination.execute_transaction([&](auto& tx) {
    tx.registry().template import_storage<node>({111U, 112U}, traits::payload(*source.read()).nodes[0],
        [](auto& candidate, auto incoming) { traits::payload(candidate).nodes.push_back(std::move(incoming)); });
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  EXPECT_EQ(destination.read(), before);
  const auto destination_id = traits::payload(*destination.read()).nodes[0].persistent_id;
  typename traits::id_type mapped_id{};
  destination.execute_transaction([&](auto& tx) {
    mapped_id = tx.registry().template import_storage<node>({111U, 112U}, traits::payload(*source.read()).nodes[0],
        [](auto& candidate, auto incoming) { traits::payload(candidate).nodes.push_back(std::move(incoming)); },
        [&](const auto& reference) {
          using target_type = typename std::remove_cvref_t<decltype(reference)>::target_type;
          if constexpr (std::same_as<target_type, node>) {
            return destination.registry().template reference<node>(destination_id);
          } else {
            return destination.registry().template reference<root>(destination.read()->persistent_id);
          }
        });
  }).throw_if_failed();
  EXPECT_EQ(destination.registry().find<node>(mapped_id).value().next->id(), destination_id);
}

// Metadata validation precedes any membership lookup and cannot be bypassed through keyed input.
TEST(managed_entity_references, malformed_and_wrong_class_envelopes_reject) {
  for (const std::string_view text : {
      R"({"document_high":41,"document_low":42,"class_id":102,"object_id":2})",
      R"({"document_high":41,"document_low":42,"class_id":101})",
      R"({"document_high":41,"document_low":42,"class_id":101,"object_id":2,"object_id":3})",
      R"({"document_high":0,"document_low":0,"class_id":101,"object_id":2})"}) {
    const auto input = rohit::make_constant_full_stream(text.data(), text.size());
    EXPECT_THROW((void)(serializer::deserialize_exact<token, serializer::json>(input)), std::invalid_argument);
  }
}

// Token object IDs use the exact selected managed encoding while preserving the full native range.
TEST(managed_entity_references, token_id_encoding_keeps_small_values_compact_and_full_width_range) {
  store_type store{initial(), {131U, 132U}};
  const auto id = traits::payload(*store.read()).nodes[0].persistent_id;
  const auto reference = store.registry().reference<node>(id);
  rohit::full_stream_auto_alloc small{};
  serializer::serialize_to<serializer::binary_none>(small, reference);
  constexpr auto metadata_bytes = 3U * sizeof(std::uint64_t);
  const auto small_id_bytes = compact_ids<node_traits>() ? 1U : sizeof(typename traits::id_type);
  EXPECT_EQ(small.current_offset(), metadata_bytes + small_id_bytes);
  const auto text = std::string{"{\"document_high\":131,\"document_low\":132,\"class_id\":101,\"object_id\":"} +
      std::to_string(std::numeric_limits<typename traits::id_type>::max()) + "}";
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  const auto maximum = serializer::deserialize_exact<token, serializer::json>(input);
  rohit::full_stream_auto_alloc wide{};
  serializer::serialize_to<serializer::binary_none>(wide, maximum);
  constexpr auto varint_payload_bits = 7U;
  constexpr auto maximum_varint_bytes =
      (std::numeric_limits<typename traits::id_type>::digits + varint_payload_bits - 1U) / varint_payload_bits;
  const auto maximum_id_bytes = compact_ids<node_traits>() ? maximum_varint_bytes : sizeof(typename traits::id_type);
  EXPECT_EQ(wide.current_offset(), metadata_bytes + maximum_id_bytes);
  round_trip<serializer::binary_none>(maximum);
  round_trip<serializer::binary_integer>(maximum);
  round_trip<serializer::binary_string>(maximum);
}

// Managed identity values retain every uint32/uint64 bit across all three binary key modes.
TEST(managed_entity_references, generated_persistent_ids_follow_selected_encoding_at_boundaries) {
  persistent_id_boundaries<serializer::binary_none>();
  persistent_id_boundaries<serializer::binary_integer>();
  persistent_id_boundaries<serializer::binary_string>();
}

// New required edges may be linked after adoption, while incomplete candidates never publish.
TEST(managed_entity_references, staged_adoption_links_required_self_and_mutual_edges_before_publication) {
  using required_root = entity_graph::required_document;
  using required_node = entity_graph::required_node;
  using required_traits = managed::model_traits<required_root>;
  using required_store = managed::model_store<required_root>;
  using id_type = typename required_traits::id_type;
  required_store store{required_root{}, {141U, 142U}};
  const auto attach = [](auto& storage, auto incoming) {
    required_traits::payload(storage).nodes.push_back(std::move(incoming));
  };
  const auto before = store.read();
  const auto rejected = store.execute_transaction([&](auto& tx) {
    tx.registry().adopt(required_node{}, attach);
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  EXPECT_EQ(store.read(), before);
  id_type self{}, first{}, second{};
  store.execute_transaction([&](auto& tx) {
    const auto registry = tx.registry();
    self = registry.adopt(required_node{}, attach);
    first = registry.adopt(required_node{}, attach);
    second = registry.adopt(required_node{}, attach);
    registry.template edit<required_node>(self).set_next(registry.template reference<required_node>(self));
    registry.template edit<required_node>(first).set_next(registry.template reference<required_node>(second));
    registry.template edit<required_node>(second).set_next(registry.template reference<required_node>(first));
  }).throw_if_failed();
  const auto registry = store.registry();
  EXPECT_EQ(registry.resolve(registry.template find<required_node>(self).value().next).id(), self);
  EXPECT_EQ(registry.resolve(registry.template find<required_node>(first).value().next).id(), second);
  EXPECT_EQ(registry.resolve(registry.template find<required_node>(second).value().next).id(), first);
  auto incomplete_saved = registry.template find<required_node>(self).storage();
  managed::model_traits<required_node>::payload(incomplete_saved).next = {};
  const auto accepted = store.read();
  const auto imported = store.execute_transaction([&](auto& tx) {
    tx.registry().template import_storage<required_node>({141U, 142U}, std::move(incomplete_saved), attach);
  });
  EXPECT_EQ(imported.status, managed::transaction_status::failed);
  EXPECT_EQ(store.read(), accepted);
}
