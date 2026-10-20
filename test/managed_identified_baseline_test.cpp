// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include <managed_entity_references.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
namespace serializer = rohit::serializer;
using root = entity_graph::document;
using node = entity_graph::node;
using traits = managed::model_traits<root>;
using node_traits = managed::model_traits<node>;
using store_type = managed::model_store<root>;
using baseline_storage = store_type::storage_type;
using id_type = store_type::id_type;
constexpr managed::document_id baseline_document{401U, 402U};
constexpr std::uint64_t baseline_watermark = 130U;
constexpr id_type root_id = 19U;
constexpr id_type first_id = 7U;
constexpr id_type second_id = 93U;
constexpr id_type other_id = 31U;

// Read a standalone declared token through its ordinary public codec, without a managed envelope.
template <typename Plain>
managed::managed_ref<Plain, id_type> reference(id_type id,
                                               managed::document_id document = baseline_document) {
  const auto text = std::string{"{\"document_high\":"} + std::to_string(document.high) +
                    ",\"document_low\":" + std::to_string(document.low) +
                    ",\"class_id\":" + std::to_string(managed::model_traits<Plain>::class_id) +
                    ",\"object_id\":" + std::to_string(id) + "}";
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  return serializer::deserialize_exact<managed::managed_ref<Plain, id_type>, serializer::json>(
      input);
}

// Build sparse durable storage directly, retaining required ownership and forward/cyclic tokens.
baseline_storage identified_storage() {
  root value{};
  value.nodes.resize(2U);
  value.ordinary_id = 9001U;
  auto storage = traits::make_storage(std::move(value));
  storage.persistent_id = root_id;
  auto& payload = traits::payload(storage);
  payload.nodes[0].persistent_id = first_id;
  payload.nodes[1].persistent_id = second_id;
  payload.extra.persistent_id = other_id;
  auto& first = node_traits::payload(payload.nodes[0]);
  auto& second = node_traits::payload(payload.nodes[1]);
  first.label = "first";
  second.label = "second";
  first.next = reference<node>(second_id);
  second.next = reference<node>(first_id);
  first.owner = reference<root>(root_id);
  payload.info.links = {reference<node>(first_id), reference<node>(second_id)};
  payload.info.named_links.emplace(17U, reference<node>(first_id));
  return storage;
}

// Inspect actual native bytes using the existing keyed storage codec and bounded public decoder.
template <typename Value>
std::vector<std::uint8_t> native_bytes(const Value& value) {
  rohit::full_stream_auto_alloc output{};
  serializer::serialize_to<serializer::binary_integer>(output, value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Decode only envelopes produced by a real store, never synthesize one for first initialization.
template <typename Envelope>
Envelope read_envelope(const auto& store) {
  const auto bytes = store.save();
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  return serializer::deserialize_exact<Envelope, serializer::binary_integer>(input);
}

// Observe the scoped root/control-block hook without replacing generated container allocators.
class counting_resource final : public std::pmr::memory_resource {
public:
  std::size_t allocations{};
  std::size_t deallocations{};

private:
  // Count only successfully allocated shared model holders.
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    auto* result = std::pmr::new_delete_resource()->allocate(bytes, alignment);
    ++allocations;
    return result;
  }
  // Return each holder through the same scoped resource.
  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    ++deallocations;
    std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
  }
  // Resource instances have independent counters and ownership.
  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }
};

// Restrict journal qualification and cleanup to one freshly created child of the CMake build tree.
struct journal_directory {
  const std::filesystem::path base{SERIALIZER_IDENTIFIED_BASELINE_TEST_DIRECTORY};
  const std::filesystem::path path{
      base / ("identified-" SERIALIZER_IDENTIFIED_BASELINE_TEST_PROFILE "-" +
              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  // Reject a reused or out-of-bound fixture before opening any document path.
  journal_directory() {
    if (!base.is_absolute() || path.parent_path() != base ||
        !std::filesystem::create_directory(path)) {
      throw std::runtime_error{"Invalid identified baseline test directory"};
    }
  }
  // Destroy only this test's freshly created child after all journal handles have closed.
  ~journal_directory() {
    std::error_code ignored;
    if (path.parent_path() == base) {
      std::filesystem::remove_all(path, ignored);
    }
  }
};

// A missing schema identity must fail before native serialization or resource callbacks.
struct missing_schema_traits : traits {
  static constexpr std::string_view schema_id{};
};
} // namespace

// Sparse IDs, every declared edge, ordinary values and burned gaps survive first admission exactly.
TEST(managed_identified_baseline, preserves_storage_tokens_namespace_and_one_linear_baseline) {
  const auto storage = identified_storage();
  const auto durable = native_bytes(storage);
  std::size_t validations{};
  store_type store{managed::identified_baseline,
                   storage,
                   baseline_document,
                   baseline_watermark,
                   {},
                   [&](const auto& accepted) {
                     ++validations;
                     EXPECT_EQ(native_bytes(accepted), durable);
                   }};
  EXPECT_EQ(validations, 1U);
  EXPECT_EQ(store.document().high, baseline_document.high);
  EXPECT_EQ(store.document().low, baseline_document.low);
  EXPECT_EQ(store.allocated_id(), baseline_watermark);
  EXPECT_EQ(store.read()->persistent_id, root_id);
  EXPECT_EQ(native_bytes(*store.read()), durable);
  EXPECT_EQ(store.registry().resolve(reference<node>(first_id)).value().label, "first");
  EXPECT_EQ(store.registry().resolve(reference<node>(second_id)).value().label, "second");
  EXPECT_EQ(store.registry().resolve(reference<root>(root_id)).id(), root_id);
  EXPECT_EQ(store.state(), managed::state_stamp{});
  EXPECT_EQ(store.history_cursor(), 0U);
  EXPECT_THROW(store.undo(), std::out_of_range);
  EXPECT_THROW(store.redo(), std::out_of_range);
  const auto envelope = read_envelope<managed::records::unlabeled_state_envelope>(store);
  ASSERT_EQ(envelope.entries.size(), 1U);
  EXPECT_EQ(envelope.cursor, 0U);
  EXPECT_EQ(envelope.allocated_id, baseline_watermark);
  EXPECT_EQ(envelope.current_snapshot, durable);
  EXPECT_EQ(envelope.entries.front().snapshot, durable);
  root empty{};
  store_type loaded{std::move(empty), {501U, 502U}};
  loaded.load(store.save());
  EXPECT_EQ(loaded.save(), store.save());
  EXPECT_EQ(loaded.registry().resolve(reference<node>(first_id)).value().label, "first");
}

// All history modes start from exactly one state; disabled histories retain no revision records.
TEST(managed_identified_baseline, tree_and_disabled_modes_keep_native_baselines) {
  using tree_store = managed::model_store<root, managed::history_mode::tree>;
  tree_store tree{managed::identified_baseline, identified_storage(), baseline_document,
                  baseline_watermark};
  const auto envelope = read_envelope<managed::records::unlabeled_envelope>(tree);
  ASSERT_EQ(envelope.revisions.size(), 1U);
  EXPECT_EQ(envelope.current_revision, 1U);
  EXPECT_EQ(envelope.revision_high_water, 1U);
  EXPECT_EQ(envelope.revisions.front().number, 1U);
  EXPECT_EQ(envelope.revisions.front().parent, 0U);
  EXPECT_EQ(tree.current_revision(), 1U);
  EXPECT_TRUE(tree.redo_children().empty());
  EXPECT_THROW(tree.undo(), std::out_of_range);
  using no_history_store =
      managed::model_store<root, managed::history_mode::disabled, managed::history_labels::disabled,
                           traits, managed::store_features::none>;
  no_history_store plain{managed::identified_baseline, identified_storage(), baseline_document,
                         baseline_watermark};
  const auto state = read_envelope<managed::records::unlabeled_state_envelope>(plain);
  EXPECT_TRUE(state.entries.empty());
  EXPECT_EQ(state.allocated_id, baseline_watermark);
  EXPECT_EQ(native_bytes(*plain.read()), native_bytes(*tree.read()));
}

// Required forward/cyclic edges are checked against the complete supplied ownership graph.
TEST(managed_identified_baseline, required_reference_cycles_admit_without_bootstrap_or_remapping) {
  using required_root = entity_graph::required_document;
  using required_node = entity_graph::required_node;
  using required_traits = managed::model_traits<required_root>;
  using required_node_traits = managed::model_traits<required_node>;
  using required_store = managed::model_store<required_root>;
  required_root value{};
  value.nodes.resize(2U);
  auto storage = required_traits::make_storage(std::move(value));
  storage.persistent_id = root_id;
  auto& nodes = required_traits::payload(storage).nodes;
  nodes[0].persistent_id = first_id;
  nodes[1].persistent_id = second_id;
  required_node_traits::payload(nodes[0]).next = reference<required_node>(second_id);
  required_node_traits::payload(nodes[1]).next = reference<required_node>(first_id);
  const auto durable = native_bytes(storage);
  required_store store{managed::identified_baseline, storage, baseline_document,
                       baseline_watermark};
  EXPECT_EQ(native_bytes(*store.read()), durable);
  EXPECT_EQ(store.registry().resolve(required_node_traits::payload(nodes[0]).next).id(), second_id);
  required_node_traits::payload(nodes[0]).next = {};
  auto resource = std::make_shared<counting_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  std::size_t validations{};
  EXPECT_THROW((required_store{managed::identified_baseline, std::move(storage), baseline_document,
                               baseline_watermark, options, [&](const auto&) { ++validations; }}),
               std::invalid_argument);
  EXPECT_EQ(validations, 0U);
  EXPECT_EQ(resource->allocations, 0U);
}

// Later allocation starts beyond the stored watermark and never reuses a gap after undo or failure.
TEST(managed_identified_baseline, preserves_burned_gaps_for_allocation_undo_and_failed_edits) {
  store_type store{managed::identified_baseline, identified_storage(), baseline_document,
                   baseline_watermark};
  const auto attach = [](auto& storage, auto incoming) {
    traits::payload(storage).nodes.push_back(std::move(incoming));
  };
  id_type accepted{}, burned{}, next{};
  store.execute_transaction([&](auto& tx) { accepted = tx.registry().adopt(node{}, attach); })
      .throw_if_failed();
  EXPECT_EQ(accepted, baseline_watermark + 1U);
  store.undo();
  EXPECT_EQ(store.allocated_id(), baseline_watermark + 1U);
  EXPECT_FALSE(store.registry().contains(accepted));
  const auto failed = store.execute_transaction([&](auto& tx) {
    burned = tx.registry().adopt(node{}, attach);
    throw std::runtime_error{"reject the edit after reserving an identity"};
  });
  EXPECT_EQ(failed.status, managed::transaction_status::failed);
  EXPECT_EQ(burned, baseline_watermark + 2U);
  EXPECT_EQ(store.allocated_id(), baseline_watermark + 2U);
  EXPECT_FALSE(store.registry().contains(burned));
  store.execute_transaction([&](auto& tx) { next = tx.registry().adopt(node{}, attach); })
      .throw_if_failed();
  EXPECT_EQ(next, baseline_watermark + 3U);
}

// Bad metadata and identities never reach application validation, root allocation or file creation.
TEST(managed_identified_baseline, rejects_metadata_and_identity_before_acceptance_effects) {
  const journal_directory directory;
  auto resource = std::make_shared<counting_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  std::size_t validations{};
  const auto validate = [&](const auto&) { ++validations; };
  const auto reject = [&](baseline_storage value, managed::document_id document,
                          std::uint64_t watermark) {
    EXPECT_THROW((store_type{managed::identified_baseline, std::move(value), document, watermark,
                             options, validate}),
                 std::invalid_argument);
    EXPECT_EQ(validations, 0U);
    EXPECT_EQ(resource->allocations, 0U);
    EXPECT_TRUE(std::filesystem::is_empty(directory.path));
  };
  reject(identified_storage(), {}, baseline_watermark);
  reject(identified_storage(), baseline_document, 0U);
  reject(identified_storage(), baseline_document, second_id - 1U);
  auto missing_root = identified_storage();
  missing_root.persistent_id = 0U;
  reject(std::move(missing_root), baseline_document, baseline_watermark);
  auto missing_child = identified_storage();
  traits::payload(missing_child).nodes[0].persistent_id = 0U;
  reject(std::move(missing_child), baseline_document, baseline_watermark);
  auto duplicate = identified_storage();
  traits::payload(duplicate).nodes[0].persistent_id = root_id;
  reject(std::move(duplicate), baseline_document, baseline_watermark);
  if constexpr (sizeof(id_type) < sizeof(std::uint64_t)) {
    reject(identified_storage(), baseline_document,
           static_cast<std::uint64_t>(std::numeric_limits<id_type>::max()) + 1U);
  }
  using missing_schema_store =
      managed::model_store<root, managed::history_mode::linear, managed::history_labels::disabled,
                           missing_schema_traits>;
  EXPECT_THROW((missing_schema_store{managed::identified_baseline, identified_storage(),
                                     baseline_document, baseline_watermark, options, validate}),
               std::invalid_argument);
  EXPECT_EQ(validations, 0U);
  EXPECT_EQ(resource->allocations, 0U);
}

// A native token remains invalid if its live target has another type, scope or membership.
TEST(managed_identified_baseline,
     rejects_reference_scope_type_and_dangling_targets_before_effects) {
  auto resource = std::make_shared<counting_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  std::size_t validations{};
  const auto reject = [&](auto token) {
    auto value = identified_storage();
    node_traits::payload(traits::payload(value).nodes[0]).next = token;
    EXPECT_THROW((store_type{managed::identified_baseline, std::move(value), baseline_document,
                             baseline_watermark, options, [&](const auto&) { ++validations; }}),
                 managed::registry_error);
    EXPECT_EQ(validations, 0U);
    EXPECT_EQ(resource->allocations, 0U);
  };
  reject(reference<node>(second_id, {601U, 602U}));
  reject(reference<node>(other_id));
  reject(reference<node>(89U));
}

// Native byte/shape limits and history/resident admission fail before the caller's resource hook.
TEST(managed_identified_baseline, enforces_decode_snapshot_history_and_resident_budgets) {
  auto resource = std::make_shared<counting_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  const auto reject = [&](managed::store_options bounds) {
    EXPECT_ANY_THROW((store_type{managed::identified_baseline, identified_storage(),
                                 baseline_document, baseline_watermark, bounds}));
    EXPECT_EQ(resource->allocations, 0U);
  };
  const auto snapshot_size = native_bytes(identified_storage()).size();
  auto bounded = options;
  bounded.max_snapshot_bytes = snapshot_size - 1U;
  reject(bounded);
  bounded = options;
  bounded.decode.max_input_bytes = snapshot_size - 1U;
  reject(bounded);
  for (const auto member : {&serializer::decode_limits::max_string_bytes,
                            &serializer::decode_limits::max_collection_elements,
                            &serializer::decode_limits::max_nesting_depth,
                            &serializer::decode_limits::max_allocation_bytes,
                            &serializer::decode_limits::max_work_units}) {
    bounded = options;
    bounded.decode.*member = 0U;
    reject(bounded);
  }
  bounded = options;
  bounded.max_revisions = 0U;
  reject(bounded);
  bounded = options;
  bounded.max_history_bytes = snapshot_size - 1U;
  reject(bounded);
  bounded = options;
  bounded.max_resident_bytes = 1U;
  reject(bounded);
  options.max_snapshot_bytes = snapshot_size;
  options.decode.max_input_bytes = snapshot_size;
  options.max_history_bytes = snapshot_size;
  options.max_revisions = 1U;
  {
    store_type exact{managed::identified_baseline, identified_storage(), baseline_document,
                     baseline_watermark, options};
    EXPECT_EQ(exact.allocated_id(), baseline_watermark);
    EXPECT_EQ(resource->allocations, 1U);
  }
  EXPECT_EQ(resource->allocations, resource->deallocations);
}

// A throwing application invariant cannot allocate an accepted holder or create a durable baseline.
TEST(managed_identified_baseline, application_rejection_has_no_resource_or_file_effects) {
  const journal_directory directory;
  auto resource = std::make_shared<counting_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  std::size_t validations{};
  EXPECT_THROW((store_type{managed::identified_baseline, identified_storage(), baseline_document,
                           baseline_watermark, options,
                           [&](const auto&) {
                             ++validations;
                             throw std::runtime_error{"application rejected the durable baseline"};
                           }}),
               std::runtime_error);
  EXPECT_EQ(validations, 1U);
  EXPECT_EQ(resource->allocations, 0U);
  EXPECT_TRUE(std::filesystem::is_empty(directory.path));
  store_type accepted{managed::identified_baseline, identified_storage(), baseline_document,
                      baseline_watermark, options};
  EXPECT_EQ(accepted.allocated_id(), baseline_watermark);
  EXPECT_EQ(resource->allocations, 1U);
}

// Exhaustion remains a checked allocator failure after accepting the largest native stored watermark.
TEST(managed_identified_baseline, full_native_watermark_is_preserved_without_wrap_or_reuse) {
  constexpr auto maximum = std::numeric_limits<id_type>::max();
  auto value = identified_storage();
  value.persistent_id = maximum;
  node_traits::payload(traits::payload(value).nodes[0]).owner = reference<root>(maximum);
  store_type store{managed::identified_baseline, std::move(value), baseline_document, maximum};
  EXPECT_EQ(store.read()->persistent_id, maximum);
  EXPECT_EQ(store.allocated_id(), maximum);
  const auto before = store.save();
  const auto failed = store.execute_transaction([&](auto& tx) {
    tx.registry().adopt(node{}, [](auto& storage, auto incoming) {
      traits::payload(storage).nodes.push_back(std::move(incoming));
    });
  });
  EXPECT_EQ(failed.status, managed::transaction_status::failed);
  EXPECT_THROW(failed.throw_if_failed(), std::overflow_error);
  EXPECT_EQ(store.save(), before);
  store_type loaded{root{}, {701U, 702U}};
  loaded.load(before);
  EXPECT_EQ(loaded.allocated_id(), maximum);
  EXPECT_EQ(loaded.read()->persistent_id, maximum);
}

// Native journal creation, Save and recovery keep exact baseline identities in both storage modes.
TEST(managed_identified_baseline,
     native_save_and_recovery_preserve_identified_baseline_and_history) {
  for (const auto mode :
       {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    const journal_directory directory;
    const auto path = directory.path / "document";
    std::vector<std::uint8_t> expected;
    {
      store_type store{managed::identified_baseline, identified_storage(), baseline_document,
                       baseline_watermark};
      store.create_journal(path, mode);
      EXPECT_FALSE(store.journal_dirty());
      EXPECT_EQ(store.journal_sequence(), 0U);
      store
          .execute_transaction([&](auto& tx) {
            tx.registry().template edit<node>(first_id).set_label("saved edit");
          })
          .throw_if_failed();
      EXPECT_TRUE(store.journal_dirty());
      store.save_journal();
      EXPECT_FALSE(store.journal_dirty());
      expected = store.save();
    }
    {
      root empty{};
      store_type recovered{std::move(empty), {801U, 802U}};
      recovered.recover_journal(path);
      EXPECT_EQ(recovered.save(), expected);
      EXPECT_EQ(recovered.allocated_id(), baseline_watermark);
      EXPECT_EQ(recovered.read()->persistent_id, root_id);
      EXPECT_EQ(recovered.document().high, baseline_document.high);
      EXPECT_EQ(recovered.document().low, baseline_document.low);
      EXPECT_FALSE(recovered.journal_dirty());
      EXPECT_EQ(recovered.registry().resolve(reference<node>(first_id)).value().label,
                "saved edit");
      recovered.undo();
      EXPECT_EQ(recovered.registry().resolve(reference<node>(first_id)).value().label, "first");
      EXPECT_EQ(recovered.history_cursor(), 0U);
      EXPECT_THROW(recovered.undo(), std::out_of_range);
      recovered.redo();
      EXPECT_EQ(recovered.save(), expected);
    }
  }
}
