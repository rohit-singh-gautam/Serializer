// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_named_references.hpp>
#include <managed_named_required_references.hpp>
#include <rohit/managed.hpp>

#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
namespace serializer = rohit::serializer;
using root = named_reference_example::document;
using node = named_reference_example::number_node;
using sibling = named_reference_example::sibling_node;
using traits = managed::model_traits<root>;
using node_traits = managed::model_traits<node>;
using sibling_traits = managed::model_traits<sibling>;
using store_type = managed::model_store<root>;
using id_type = typename traits::id_type;
using token = managed::managed_ref<node, id_type>;
using sibling_token = managed::managed_ref<sibling, id_type>;
static_assert(std::same_as<decltype(node{}.next), std::optional<token>>);
static_assert(std::same_as<decltype(sibling{}.next), std::optional<sibling_token>>);
static_assert(!std::assignable_from<std::optional<token>&, sibling_token>);
static_assert(node_traits::class_id != sibling_traits::class_id);

// Seed ordinary values; the store assigns each named occurrence its own native identity.
root initial() {
  root value{};
  value.nodes.resize(2);
  value.nodes[0].value = 11;
  value.nodes[1].value = 22;
  value.siblings.resize(1);
  value.siblings[0].value = 33;
  return value;
}

// Inspect generated payloads without assuming direct or separate-values storage.
const auto& payload(const auto& store) { return traits::payload(*store.read()); }

// Link the first named occurrence to itself after allocation and complete-candidate scope binding.
auto link_self(auto& store) {
  const auto id = payload(store).nodes.front().persistent_id;
  const auto reference = store.registry().template reference<node>(id);
  store.execute_transaction([&](auto& tx) {
    tx.update([&](auto& storage) {
      auto& value = node_traits::payload(traits::payload(storage).nodes.front());
      value.next = reference;
      value.links = {reference};
      value.lookup.emplace(7, reference);
    });
  }).throw_if_failed();
  return reference;
}

// Exercise every ordinary codec with an exact typed token independent of container traversal.
template <template <serializer::serialize_type> class Protocol, typename Reference>
void round_trip(const Reference& value) {
  rohit::full_stream_auto_alloc output{};
  serializer::serialize_to<Protocol>(output, value);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  EXPECT_EQ((serializer::deserialize_exact<Reference, Protocol>(input)), value);
}

// Own one exclusively created journal directory and remove it after all receivers close.
struct journal_directory {
  std::filesystem::path path{std::filesystem::current_path() /
      ("journal-test-" + std::to_string(managed::make_document_id().high))};
  // Never adopt a preexisting directory as test-owned storage.
  journal_directory() {
    if (!std::filesystem::create_directory(path)) { throw std::runtime_error{"Test directory already exists"}; }
  }
  // Cleanup is restricted to the directory this instance created.
  ~journal_directory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
} // namespace

// A named generic owns its self-reference type across optional, sequence and map token positions.
TEST(managed_named_references, self_edges_resolve_in_all_codecs_and_history) {
  store_type store{initial(), {41, 42}};
  const auto before = store.read();
  const auto reference = link_self(store);
  const auto registry = store.registry();
  const auto entity = registry.resolve(reference);
  const auto& value = entity.value();
  EXPECT_EQ(registry.resolve(*value.next).id(), reference.id());
  EXPECT_EQ(registry.resolve(value.links.front()).id(), reference.id());
  EXPECT_EQ(registry.resolve(value.lookup.at(7)).id(), reference.id());
  round_trip<serializer::binary_none>(reference);
  round_trip<serializer::binary_integer>(reference);
  round_trip<serializer::binary_string>(reference);
  round_trip<serializer::json>(reference);
  const auto saved = store.save();
  store.undo();
  EXPECT_FALSE(node_traits::payload(payload(store).nodes.front()).next);
  store.redo();
  EXPECT_EQ(store.save(), saved);
  EXPECT_FALSE(node_traits::payload(traits::payload(*before).nodes.front()).next);
  store_type restored{initial(), {51, 52}};
  restored.load(saved);
  EXPECT_EQ(restored.registry().resolve(reference).value().value, 11u);
  EXPECT_EQ(restored.save(), saved);
}

// Required named self and mutual edges initialize only after native allocation and before first publication.
TEST(managed_named_references, required_self_edges_use_the_existing_reference_initializer) {
  using required_root = named_required_reference_example::document;
  using required_node = named_required_reference_example::number_node;
  using required_traits = managed::model_traits<required_root>;
  using required_node_traits = managed::model_traits<required_node>;
  using required_store = managed::model_store<required_root>;
  required_root initial{};
  initial.nodes.resize(3);
  EXPECT_THROW((void)required_store(initial), std::invalid_argument);
  required_store store{initial, {151, 152}, {}, {}, [](auto& storage, const auto& references) {
    auto& nodes = required_traits::payload(storage).nodes;
    required_node_traits::payload(nodes[0]).next = references.template reference<required_node>(nodes[0].persistent_id);
    required_node_traits::payload(nodes[1]).next = references.template reference<required_node>(nodes[2].persistent_id);
    required_node_traits::payload(nodes[2]).next = references.template reference<required_node>(nodes[1].persistent_id);
  }};
  const auto registry = store.registry();
  const auto view = store.read();
  const auto& nodes = required_traits::payload(*view).nodes;
  const auto reference = registry.template reference<required_node>(nodes[0].persistent_id);
  EXPECT_EQ(registry.resolve(registry.resolve(reference).value().next).id(), reference.id());
  const auto second = registry.template reference<required_node>(nodes[1].persistent_id);
  EXPECT_EQ(registry.resolve(registry.resolve(second).value().next).id(), nodes[2].persistent_id);
  round_trip<serializer::binary_none>(reference);
  round_trip<serializer::binary_integer>(reference);
  round_trip<serializer::binary_string>(reference);
  round_trip<serializer::json>(reference);
}

// Equal generic arguments never make independently named records interchangeable token targets.
TEST(managed_named_references, distinct_named_class_tags_reject_forged_envelopes) {
  store_type store{initial(), {61, 62}};
  const auto sibling_id = payload(store).siblings.front().persistent_id;
  const auto sibling_reference = store.registry().reference<sibling>(sibling_id);
  rohit::full_stream_auto_alloc output{};
  serializer::serialize_to<serializer::json>(output, sibling_reference);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  EXPECT_THROW((void)(serializer::deserialize_exact<token, serializer::json>(input)), std::invalid_argument);
  EXPECT_THROW((void)store.registry().reference<node>(sibling_id), managed::registry_error);
}

// Complete publication rejects foreign or dangling self edges without changing the accepted Save.
TEST(managed_named_references, foreign_and_deleted_targets_preserve_publication) {
  store_type store{initial(), {71, 72}};
  store_type foreign{initial(), {81, 82}};
  const auto reference = link_self(store);
  const auto foreign_reference = link_self(foreign);
  const auto saved = store.save();
  const auto rejected = store.execute_transaction([&](auto& tx) {
    tx.root().nodes().edit(reference.id()).set_next(foreign_reference);
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
  const auto dangling = store.execute_transaction([](auto& tx) {
    tx.update([](auto& storage) {
      auto& nodes = traits::payload(storage).nodes;
      node_traits::payload(nodes[1]).next = node_traits::payload(nodes.front()).next;
      nodes.erase(nodes.begin());
    });
  });
  EXPECT_EQ(dangling.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), saved);
}

// Independent import rewrites named self edges into the destination document and newly allocated ID.
TEST(managed_named_references, import_rewrites_named_self_targets) {
  store_type source{initial(), {91, 92}};
  store_type destination{initial(), {101, 102}};
  const auto original = link_self(source);
  auto incoming = source.registry().resolve(original).storage();
  id_type imported{};
  destination.execute_transaction([&](auto& tx) {
    imported = tx.registry().template import_storage<node>(source.document(), incoming,
        [](auto& storage, auto value) { traits::payload(storage).nodes.push_back(std::move(value)); });
  }).throw_if_failed();
  EXPECT_NE(imported, original.id());
  const auto registry = destination.registry();
  const auto reference = registry.reference<node>(imported);
  const auto entity = registry.resolve(reference);
  const auto& value = entity.value();
  EXPECT_EQ(*value.next, reference);
  EXPECT_EQ(value.links.front(), reference);
  EXPECT_EQ(value.lookup.at(7), reference);
  EXPECT_THROW((void)registry.resolve(original), managed::registry_error);
}

// Journal framing and replay preserve the named target's exact class/identity contract in both layouts.
TEST(managed_named_references, full_and_delta_journals_recover_named_self_edges) {
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    for (const bool delta : {false, true}) {
      const journal_directory directory;
      const auto path = directory.path / "document";
      managed::store_options options;
      options.delta_journal = delta;
      std::vector<std::uint8_t> saved;
      token reference;
      {
        store_type store{initial(), {111, 112}, options};
        store.create_journal(path, mode);
        reference = link_self(store);
        saved = store.save();
      }
      store_type restored{initial(), {121, 122}, options};
      restored.recover_journal(path);
      EXPECT_EQ(restored.save(), saved);
      const auto registry = restored.registry();
      EXPECT_EQ(*registry.resolve(reference).value().next, reference);
      restored.save_journal();
    }
  }
}
