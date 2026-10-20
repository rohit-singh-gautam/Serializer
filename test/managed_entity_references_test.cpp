// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <managed_entity_references.hpp>
#include <rohit/binary_scalar.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
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
using id_type = typename traits::id_type;
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

// Decode a well-formed typed envelope independently of target membership for rejection probes.
token decoded_node_token(managed::document_id document, id_type id) {
  const auto text = std::string{"{\"document_high\":"} + std::to_string(document.high) +
      ",\"document_low\":" + std::to_string(document.low) + ",\"class_id\":" +
      std::to_string(node_traits::class_id) + ",\"object_id\":" + std::to_string(id) + "}";
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  return serializer::deserialize_exact<token, serializer::json>(input);
}

// Fail only the documented scoped root/control-block hook, retaining independent allocation accounting.
class failing_resource final : public std::pmr::memory_resource {
public:
  bool reject{};
  std::size_t attempts{}, live_bytes{};
private:
  // Admit a scoped allocation before delegating to the ordinary owning resource.
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    ++attempts;
    if (reject) { throw std::bad_alloc{}; }
    auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
    live_bytes += bytes;
    return result;
  }
  // Balance each successful scoped allocation, including candidates discarded after failure.
  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    live_bytes -= bytes;
    std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
  }
  // Resource identity remains local to this deterministic fault injector.
  bool do_is_equal(const std::pmr::memory_resource& value) const noexcept override {
    return this == &value;
  }
};

// Check the precise registry failure recorded after the transaction has released its candidate.
void expect_registry_error(const managed::transaction_outcome& outcome, managed::registry_error_code code) {
  EXPECT_EQ(outcome.status, managed::transaction_status::failed);
  try {
    outcome.throw_if_failed();
    FAIL() << "Expected a recorded registry error";
  } catch (const managed::registry_error& error) {
    EXPECT_EQ(error.code(), code);
  }
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

// Mapping callbacks cannot smuggle malformed, foreign, absent or differently typed external targets.
TEST(managed_entity_references, import_external_mapping_failures_preserve_history_and_burn_reservations) {
  constexpr managed::document_id source_document{151U, 152U};
  constexpr managed::document_id destination_document{161U, 162U};
  store_type source{initial(), source_document};
  const auto source_first = traits::payload(*source.read()).nodes[0].persistent_id;
  const auto source_second = traits::payload(*source.read()).nodes[1].persistent_id;
  const auto external = source.registry().reference<node>(source_second);
  source.execute_transaction([&](auto& tx) {
    tx.registry().template edit<node>(source_first).set_next(external);
  }).throw_if_failed();
  const auto source_snapshot = source.read();
  const auto imported = traits::payload(*source_snapshot).nodes[0];
  store_type destination{initial(), destination_document};
  const auto accepted = destination.read();
  const auto state = destination.state();
  const auto existing = traits::payload(*accepted).nodes[0].persistent_id;
  const auto different_type = traits::payload(*accepted).extra.persistent_id;
  const auto attach = [](auto& storage, auto incoming) {
    traits::payload(storage).nodes.push_back(std::move(incoming));
  };
  enum class mapping_failure { omitted, empty, foreign, missing, wrong_type, callback };
  const std::array failures{mapping_failure::omitted, mapping_failure::empty, mapping_failure::foreign,
      mapping_failure::missing, mapping_failure::wrong_type, mapping_failure::callback};
  for (const auto failure : failures) {
    SCOPED_TRACE(static_cast<int>(failure));
    const auto allocation = destination.allocated_id();
    std::size_t mapper_calls{};
    const auto outcome = destination.execute_transaction([&](auto& tx) {
      if (failure == mapping_failure::omitted) {
        tx.registry().template import_storage<node>(source_document, imported, attach);
      } else {
        tx.registry().template import_storage<node>(source_document, imported, attach,
            [&](const auto& reference) -> std::remove_cvref_t<decltype(reference)> {
              using target_type = typename std::remove_cvref_t<decltype(reference)>::target_type;
              ++mapper_calls;
              if constexpr (std::same_as<target_type, node>) {
                switch (failure) {
                case mapping_failure::empty: return {};
                case mapping_failure::foreign: return reference;
                case mapping_failure::missing:
                  return decoded_node_token(destination_document, std::numeric_limits<id_type>::max());
                case mapping_failure::wrong_type: return decoded_node_token(destination_document, different_type);
                case mapping_failure::callback: throw std::runtime_error{"mapping failed"};
                default: return destination.registry().template reference<node>(existing);
                }
              } else {
                return destination.registry().template reference<root>(accepted->persistent_id);
              }
            });
      }
    });
    EXPECT_EQ(outcome.status, managed::transaction_status::failed);
    if (failure == mapping_failure::empty) {
      EXPECT_THROW(outcome.throw_if_failed(), std::invalid_argument);
    } else if (failure == mapping_failure::callback) {
      EXPECT_THROW(outcome.throw_if_failed(), std::runtime_error);
    } else {
      const auto code = failure == mapping_failure::missing ? managed::registry_error_code::missing_id :
          failure == mapping_failure::wrong_type ? managed::registry_error_code::wrong_type :
          managed::registry_error_code::foreign_document;
      expect_registry_error(outcome, code);
    }
    EXPECT_EQ(mapper_calls, failure == mapping_failure::omitted ? 0U : 1U);
    EXPECT_EQ(destination.read(), accepted);
    EXPECT_EQ(destination.state(), state);
    EXPECT_EQ(destination.history_cursor(), 0U);
    EXPECT_GT(destination.allocated_id(), allocation);
    EXPECT_EQ(source.read(), source_snapshot);
    EXPECT_EQ(source.registry().resolve(external).id(), source_second);
  }
  const auto burned = destination.allocated_id();
  id_type imported_id{};
  destination.execute_transaction([&](auto& tx) {
    imported_id = tx.registry().template import_storage<node>(source_document, imported, attach,
        [&](const auto& reference) -> std::remove_cvref_t<decltype(reference)> {
          using target_type = typename std::remove_cvref_t<decltype(reference)>::target_type;
          if constexpr (std::same_as<target_type, node>) {
            return destination.registry().template reference<node>(existing);
          } else {
            return destination.registry().template reference<root>(accepted->persistent_id);
          }
        });
  }).throw_if_failed();
  EXPECT_GT(imported_id, burned);
  EXPECT_EQ(destination.registry().find<node>(imported_id).value().next->id(), existing);
  EXPECT_EQ(source.registry().find<node>(source_first).value().next->id(), source_second);
  destination.undo();
  EXPECT_FALSE(destination.registry().contains(imported_id));
  EXPECT_EQ(destination.history_cursor(), 0U);
  destination.redo();
  store_type loaded{initial(), {171U, 172U}};
  loaded.load(destination.save());
  EXPECT_EQ(loaded.allocated_id(), destination.allocated_id());
  EXPECT_EQ(loaded.registry().find<node>(imported_id).value().next->id(), existing);
}

// The attachment must establish exactly one owned occurrence; all callback failures remain private.
TEST(managed_entity_references, import_attachment_and_source_identity_failures_leave_no_partial_ownership) {
  constexpr managed::document_id source_document{181U, 182U};
  store_type source{initial(), source_document};
  const auto imported = traits::payload(*source.read()).nodes[0];
  store_type destination{initial(), {191U, 192U}};
  const auto accepted = destination.read();
  const auto state = destination.state();
  const auto existing = traits::payload(*accepted).nodes[0].persistent_id;
  enum class attachment_failure { dropped, duplicated, substituted, before_attachment, after_attachment };
  const std::array failures{attachment_failure::dropped, attachment_failure::duplicated,
      attachment_failure::substituted, attachment_failure::before_attachment, attachment_failure::after_attachment};
  for (const auto failure : failures) {
    SCOPED_TRACE(static_cast<int>(failure));
    const auto allocation = destination.allocated_id();
    bool attached{};
    const auto outcome = destination.execute_transaction([&](auto& tx) {
      tx.registry().template import_storage<node>(source_document, imported, [&](auto& storage, auto incoming) {
        attached = true;
        auto& nodes = traits::payload(storage).nodes;
        switch (failure) {
        case attachment_failure::dropped: return;
        case attachment_failure::duplicated:
          nodes.push_back(incoming);
          nodes.push_back(std::move(incoming));
          return;
        case attachment_failure::substituted:
          incoming.persistent_id = existing;
          nodes.push_back(std::move(incoming));
          return;
        case attachment_failure::before_attachment: throw std::runtime_error{"attachment failed before insertion"};
        case attachment_failure::after_attachment:
          nodes.push_back(std::move(incoming));
          throw std::runtime_error{"attachment failed after insertion"};
        }
      });
    });
    EXPECT_TRUE(attached);
    EXPECT_EQ(outcome.status, managed::transaction_status::failed);
    if (failure == attachment_failure::before_attachment || failure == attachment_failure::after_attachment) {
      EXPECT_THROW(outcome.throw_if_failed(), std::runtime_error);
    } else {
      expect_registry_error(outcome, failure == attachment_failure::duplicated ?
          managed::registry_error_code::invalid_identity : managed::registry_error_code::missing_id);
    }
    EXPECT_EQ(destination.read(), accepted);
    EXPECT_EQ(destination.state(), state);
    EXPECT_EQ(destination.history_cursor(), 0U);
    EXPECT_GT(destination.allocated_id(), allocation);
  }
  for (const bool duplicate : {false, true}) {
    auto invalid_source = *source.read();
    if (duplicate) {
      traits::payload(invalid_source).nodes[1].persistent_id = traits::payload(invalid_source).nodes[0].persistent_id;
    } else {
      traits::payload(invalid_source).nodes[0].persistent_id = 0U;
    }
    const auto allocation = destination.allocated_id();
    bool attached{};
    const auto outcome = destination.execute_transaction([&](auto& tx) {
      tx.registry().template import_storage<root>(source_document, invalid_source,
          [&](auto&, auto) { attached = true; });
    });
    expect_registry_error(outcome, managed::registry_error_code::invalid_identity);
    EXPECT_FALSE(attached);
    EXPECT_EQ(destination.read(), accepted);
    EXPECT_EQ(destination.state(), state);
    EXPECT_GT(destination.allocated_id(), allocation);
  }
  const auto burned = destination.allocated_id();
  id_type accepted_id{};
  destination.execute_transaction([&](auto& tx) {
    accepted_id = tx.registry().template import_storage<node>(source_document, imported,
        [](auto& storage, auto incoming) { traits::payload(storage).nodes.push_back(std::move(incoming)); });
  }).throw_if_failed();
  EXPECT_GT(accepted_id, burned);
  EXPECT_EQ(destination.registry().find<node>(accepted_id).value().label, "first");
  EXPECT_EQ(traits::payload(*destination.read()).nodes.size(), 3U);
  destination.undo();
  EXPECT_EQ(traits::payload(*destination.read()).nodes.size(), 2U);
  EXPECT_FALSE(destination.registry().contains(accepted_id));
}

// Scoped allocation rejection occurs before import callbacks, releases its candidate and permits recovery.
TEST(managed_entity_references, import_scoped_candidate_allocation_failure_is_atomic) {
  auto resource = std::make_shared<failing_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  {
    store_type source{initial(), {201U, 202U}};
    const auto imported = traits::payload(*source.read()).nodes[0];
    store_type destination{initial(), {211U, 212U}, options};
    const auto accepted = destination.read();
    const auto state = destination.state();
    const auto allocation = destination.allocated_id();
    const auto resident_bytes = destination.resident_bytes();
    const auto live_bytes = resource->live_bytes;
    const auto attempts = resource->attempts;
    bool attached{};
    resource->reject = true;
    const auto failed = destination.execute_transaction([&](auto& tx) {
      tx.registry().template import_storage<node>({201U, 202U}, imported,
          [&](auto& storage, auto incoming) {
            attached = true;
            traits::payload(storage).nodes.push_back(std::move(incoming));
          });
    });
    resource->reject = false;
    EXPECT_THROW(failed.throw_if_failed(), std::bad_alloc);
    EXPECT_FALSE(attached);
    EXPECT_EQ(destination.read(), accepted);
    EXPECT_EQ(destination.state(), state);
    EXPECT_EQ(destination.allocated_id(), allocation);
    EXPECT_EQ(destination.resident_bytes(), resident_bytes);
    EXPECT_GT(resource->attempts, attempts);
    EXPECT_EQ(resource->live_bytes, live_bytes);
    id_type imported_id{};
    destination.execute_transaction([&](auto& tx) {
      imported_id = tx.registry().template import_storage<node>({201U, 202U}, imported,
          [](auto& storage, auto incoming) { traits::payload(storage).nodes.push_back(std::move(incoming)); });
    }).throw_if_failed();
    EXPECT_EQ(imported_id, allocation + 1U);
    EXPECT_EQ(destination.registry().find<node>(imported_id).value().label, "first");
  }
  EXPECT_EQ(resource->live_bytes, 0U);
}

// Post-import snapshot admission rolls back ownership while retaining every consumed reservation.
TEST(managed_entity_references, import_snapshot_limit_failure_keeps_only_allocator_progress) {
  store_type baseline{initial(), {221U, 222U}};
  rohit::full_stream_auto_alloc encoded{};
  serializer::serialize_to<serializer::binary_integer>(encoded, *baseline.read());
  managed::store_options options{};
  options.max_snapshot_bytes = encoded.current_offset();
  store_type destination{initial(), {231U, 232U}, options};
  store_type source{initial(), {241U, 242U}};
  const auto imported = traits::payload(*source.read()).nodes[0];
  const auto accepted = destination.read();
  const auto state = destination.state();
  const auto allocation = destination.allocated_id();
  const auto resident_bytes = destination.resident_bytes();
  bool attached{};
  const auto failed = destination.execute_transaction([&](auto& tx) {
    tx.registry().template import_storage<node>({241U, 242U}, imported,
        [&](auto& storage, auto incoming) {
          attached = true;
          traits::payload(storage).nodes.push_back(std::move(incoming));
        });
  });
  EXPECT_TRUE(attached);
  EXPECT_EQ(failed.status, managed::transaction_status::failed);
  EXPECT_THROW(failed.throw_if_failed(), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(destination.read(), accepted);
  EXPECT_EQ(destination.state(), state);
  EXPECT_GT(destination.allocated_id(), allocation);
  EXPECT_EQ(destination.resident_bytes(), resident_bytes);
  const auto burned = destination.allocated_id();
  store_type loaded{initial(), {251U, 252U}};
  loaded.load(destination.save());
  EXPECT_EQ(loaded.allocated_id(), burned);
  EXPECT_EQ(traits::payload(*loaded.read()).nodes.size(), 2U);
  id_type imported_id{};
  destination.execute_transaction([&](auto& tx) {
    imported_id = tx.registry().template import_storage<node>({241U, 242U}, imported,
        [](auto& storage, auto incoming) {
          node_traits::payload(incoming).label.clear();
          traits::payload(storage).nodes[0] = std::move(incoming);
        });
  }).throw_if_failed();
  EXPECT_GT(imported_id, burned);
  EXPECT_TRUE(destination.registry().find<node>(imported_id).value().label.empty());
  destination.undo();
  EXPECT_FALSE(destination.registry().contains(imported_id));
  destination.redo();
  EXPECT_TRUE(destination.registry().contains(imported_id));
}

// Cyclic entities retire as one complete candidate; pinned registries and history retain exact identities.
TEST(managed_entity_references, cyclic_component_retirement_requires_edge_cleanup_and_survives_history) {
  store_type store{initial(), {261U, 262U}};
  const auto first = traits::payload(*store.read()).nodes[0].persistent_id;
  const auto second = traits::payload(*store.read()).nodes[1].persistent_id;
  const auto first_ref = store.registry().reference<node>(first);
  const auto second_ref = store.registry().reference<node>(second);
  store.execute_transaction([&](auto& tx) {
    tx.update([&](auto& storage) {
      auto& value = traits::payload(storage);
      node_traits::payload(value.nodes[0]).next = second_ref;
      node_traits::payload(value.nodes[1]).next = first_ref;
      value.info.links = {first_ref, second_ref};
      value.info.named_links.emplace(7U, second_ref);
    });
  }).throw_if_failed();
  const auto linked = store.read();
  const auto pinned = store.registry();
  const auto state = store.state();
  const auto allocation = store.allocated_id();
  const auto remove = [](auto& storage, auto id) {
    auto& values = traits::payload(storage).nodes;
    std::erase_if(values, [id](const auto& value) { return value.persistent_id == id; });
  };
  const auto rejected = store.execute_transaction([&](auto& tx) {
    tx.registry().template retire<node>(first, remove);
  });
  expect_registry_error(rejected, managed::registry_error_code::missing_id);
  EXPECT_EQ(store.read(), linked);
  EXPECT_EQ(store.state(), state);
  EXPECT_EQ(store.allocated_id(), allocation);
  store.execute_transaction([&](auto& tx) {
    tx.update([](auto& storage) {
      auto& value = traits::payload(storage);
      value.info.links.clear();
      value.info.named_links.clear();
    });
    const auto registry = tx.registry();
    registry.template retire<node>(first, remove);
    registry.template retire<node>(second, remove);
  }).throw_if_failed();
  EXPECT_FALSE(store.registry().contains(first));
  EXPECT_FALSE(store.registry().contains(second));
  EXPECT_EQ(pinned.resolve(first_ref).value().next, second_ref);
  EXPECT_EQ(pinned.resolve(second_ref).value().next, first_ref);
  EXPECT_EQ(store.allocated_id(), allocation);
  const auto deleted = store.save();
  store.undo();
  EXPECT_EQ(store.registry().resolve(first_ref).value().next, second_ref);
  EXPECT_EQ(store.registry().resolve(second_ref).value().next, first_ref);
  EXPECT_EQ(traits::payload(*store.read()).info.named_links.at(7U), second_ref);
  store.redo();
  EXPECT_EQ(store.save(), deleted);
  store_type loaded{initial(), {271U, 272U}};
  loaded.load(deleted);
  EXPECT_FALSE(loaded.registry().contains(first));
  EXPECT_FALSE(loaded.registry().contains(second));
  loaded.undo();
  EXPECT_EQ(loaded.registry().resolve(first_ref).value().next, second_ref);
  EXPECT_EQ(loaded.registry().resolve(second_ref).value().next, first_ref);
  loaded.redo();
  EXPECT_FALSE(loaded.registry().contains(first));
  id_type created{};
  loaded.execute_transaction([&](auto& tx) {
    created = tx.registry().adopt(node{}, [](auto& storage, auto incoming) {
      traits::payload(storage).nodes.push_back(std::move(incoming));
    });
  }).throw_if_failed();
  EXPECT_GT(created, allocation);
  EXPECT_EQ(pinned.resolve(first_ref).value().label, "first");
  EXPECT_FALSE(loaded.registry().contains(first));
}
