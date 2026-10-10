// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <runtime_features.hpp>
#include <rohit/managed.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
namespace managed = rohit::managed;
using root = runtime_features::document_data;
using traits = managed::model_traits<root>;
using store_type = managed::model_store<root>;
using status = managed::transaction_status;

// Simulate a released schema fingerprint with unchanged entity type keys and an old text convention.
struct previous_traits : traits {
  static constexpr std::string_view schema_id = "serializer.test.previous.document";
};
using previous_store = managed::model_store<root, managed::history_mode::linear,
                                           managed::history_labels::disabled, previous_traits>;

// Build an ordinary text document with a runtime-only cached lookup key.
root initial_value() {
  root value;
  value.text = "First";
  value.lookup_cache = 7;
  return value;
}

// Access the generated payload in either direct or separate-value storage profiles.
const auto& payload(const auto& store) {
  return traits::payload(*store.read());
}

// Commit one durable text change through generated checked editors.
auto change_text(auto& store, std::string text) {
  return store.execute_transaction([&](auto& edit) { edit.root().set_text(std::move(text)); });
}

// Runtime publication preserves pins, bytes and allocator state; durable changes reset memory fields.
TEST(managed_runtime_features, immutable_runtime_publication_and_explicit_cloning) {
  store_type store{initial_value(), {11, 12}};
  const auto pinned = store.read();
  const auto saved = store.save();
  const auto allocated = store.allocated_id();
  const auto result = store.update_runtime([](auto& edit) { edit.root().set_lookup_cache(51); });
  result.throw_if_failed();
  EXPECT_EQ(result.status, status::committed);
  EXPECT_EQ(traits::payload(*pinned).lookup_cache, 7u);
  EXPECT_EQ(payload(store).lookup_cache, 51u);
  EXPECT_EQ(store.save(), saved);
  EXPECT_EQ(store.allocated_id(), allocated);
  EXPECT_EQ(store.history_cursor(), 0u);
  EXPECT_EQ(store.state().view_generation, 1u);
  EXPECT_EQ(store.state().durable_generation, 0u);
  EXPECT_EQ(store.clone_value().lookup_cache, 51u);
  auto copied = store.clone_value();
  auto assigned = copied;
  assigned = copied;
  EXPECT_EQ(assigned.lookup_cache, 51u);
  EXPECT_EQ(store.clone_value({managed::runtime_copy_policy::reset_to_defaults}).lookup_cache, 0u);
  EXPECT_EQ(change_text(store, "First").status, status::no_change);
  EXPECT_EQ(payload(store).lookup_cache, 51u);
  change_text(store, "Second").throw_if_failed();
  EXPECT_EQ(payload(store).lookup_cache, 0u);
  store.update_runtime([](auto& edit) { edit.root().set_lookup_cache(52); }).throw_if_failed();
  store.undo();
  EXPECT_EQ(payload(store).text, "First");
  EXPECT_EQ(payload(store).lookup_cache, 0u);
  store.redo();
  EXPECT_EQ(payload(store).text, "Second");
  EXPECT_EQ(payload(store).lookup_cache, 0u);
}

// Raw callback access cannot smuggle persistent edits; caught editor failures poison the update.
TEST(managed_runtime_features, rejected_runtime_updates_and_expired_editors) {
  store_type store{initial_value(), {11, 12}};
  const auto pinned = store.read();
  const auto before = store.state();
  const auto illegal = store.update_runtime([](auto& edit) {
    edit.update([](auto& value) { traits::payload(value).text = "Hidden durable edit"; });
  });
  EXPECT_EQ(illegal.status, status::failed);
  EXPECT_THROW(illegal.throw_if_failed(), std::invalid_argument);
  EXPECT_EQ(store.read(), pinned);
  EXPECT_EQ(store.state(), before);
  const auto failed = store.update_runtime([](auto& edit) {
    try {
      edit.update([](auto& value) {
        traits::payload(value).lookup_cache = 100;
        throw std::runtime_error{"Lookup failed"};
      });
    } catch (const std::runtime_error&) {
    }
  });
  EXPECT_EQ(failed.status, status::failed);
  EXPECT_EQ(store.read(), pinned);
  using editor_type = decltype(std::declval<store_type::runtime_edit&>().root());
  std::optional<editor_type> escaped;
  store.update_runtime([&](auto& edit) { escaped.emplace(edit.root()); }).throw_if_failed();
  EXPECT_THROW(escaped->set_lookup_cache(99), std::logic_error);
  EXPECT_EQ(payload(store).lookup_cache, 7u);
}

// Observer errors are separate from committed outcomes and callbacks cannot reenter mutation.
TEST(managed_runtime_features, accepted_notifications_and_navigation) {
  store_type store{initial_value(), {11, 12}};
  std::size_t observed = 0;
  store.set_publication_callback([&](const auto& event) {
    ++observed;
    EXPECT_EQ(event.after, store.read());
    EXPECT_EQ(event.stamp, store.state());
    EXPECT_EQ(store.update_runtime([](auto&) {}).status, status::failed);
    throw std::runtime_error{"Observer failed"};
  });
  const auto result = change_text(store, "Second");
  EXPECT_EQ(result.status, status::committed);
  EXPECT_EQ(observed, 1u);
  EXPECT_NE(store.take_notification_error(), nullptr);
  EXPECT_EQ(store.take_notification_error(), nullptr);
  EXPECT_EQ(change_text(store, "Second").status, status::no_change);
  EXPECT_EQ(observed, 1u);
  store.undo();
  EXPECT_EQ(observed, 2u);
  EXPECT_EQ(store.history_cursor(), 0u);
  using tree_store = managed::model_store<root, managed::history_mode::tree>;
  tree_store tree{initial_value(), {13, 14}};
  EXPECT_EQ(tree.current_revision(), 1u);
  const auto changed = change_text(tree, "Branch");
  EXPECT_EQ(tree.current_revision(), changed.revision);
  tree.undo();
  EXPECT_EQ(tree.current_revision(), 1u);
}

// Migration converts every unique retained payload, preserves cursor/IDs and publishes atomically.
TEST(managed_runtime_features, whole_document_migration_preserves_undo_and_rejects_bad_conversion) {
  previous_store source{initial_value(), {21, 22}};
  change_text(source, "Second").throw_if_failed();
  source.undo();
  const auto bytes = source.save();
  store_type target{initial_value(), {31, 32}};
  std::size_t converted = 0;
  target.load_migrated<root, previous_traits>(bytes, [&](const auto& previous) {
    ++converted;
    auto value = previous;
    traits::payload(value).text = "Converted: " + traits::payload(previous).text;
    return value;
  });
  EXPECT_EQ(converted, 2u);
  EXPECT_EQ(payload(target).text, "Converted: First");
  EXPECT_EQ(target.history_cursor(), 0u);
  EXPECT_EQ(target.allocated_id(), source.allocated_id());
  target.redo();
  EXPECT_EQ(payload(target).text, "Converted: Second");
  EXPECT_EQ(target.save() == bytes, false);
  const auto pin = target.read();
  const auto accepted = target.save();
  EXPECT_THROW((target.load_migrated<root, previous_traits>(bytes, [](const auto& previous) {
    auto value = previous;
    value.persistent_id = 0;
    return value;
  })), std::invalid_argument);
  EXPECT_EQ(target.read(), pin);
  EXPECT_EQ(target.save(), accepted);
  EXPECT_THROW((target.load_migrated<root, previous_traits>(bytes, [](const auto&) -> traits::storage_type {
    throw std::runtime_error{"Conversion failed"};
  })), std::runtime_error);
  EXPECT_EQ(target.save(), accepted);
}

// A recovered journal is upgraded into a distinct new artifact, including edits and undo cursor.
TEST(managed_runtime_features, recover_migrated_journal_preserves_recovery_and_history) {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
                         ("serializer-runtime-features-" + std::to_string(nonce));
  std::filesystem::create_directory(directory);
  struct cleanup {
    std::filesystem::path path;
    // Remove only this test's uniquely created temporary directory after all stores close.
    ~cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
  } cleanup_directory{directory};
  const auto old_path = directory / "previous.document";
  const auto new_path = directory / "current.document";
  {
    previous_store source{initial_value(), {41, 42}};
    source.create_journal(old_path);
    change_text(source, "Second").throw_if_failed();
    source.undo();
  }
  {
    store_type target{initial_value(), {43, 44}};
    target.recover_migrated_journal<root, previous_traits>(old_path, new_path,
        [](const auto& previous) {
          auto value = previous;
          traits::payload(value).text = "Converted: " + traits::payload(previous).text;
          return value;
        });
    EXPECT_EQ(payload(target).text, "Converted: First");
    target.redo();
    EXPECT_EQ(payload(target).text, "Converted: Second");
    EXPECT_TRUE(std::filesystem::exists(old_path));
  }
  store_type recovered{initial_value(), {45, 46}};
  recovered.recover_journal(new_path);
  EXPECT_EQ(payload(recovered).text, "Converted: Second");
  EXPECT_EQ(payload(recovered).lookup_cache, 0u);
}
// Patches bind exact base/target bytes and reject corrupt, stale and over-budget inputs.
TEST(managed_runtime_features, bounded_snapshot_patches_and_checkpoint_chains) {
  std::vector<std::uint8_t> base(20000, 1);
  auto target = base;
  target[10000] = 2;
  auto patch = managed::make_snapshot_patch(base, target, target.size());
  EXPECT_EQ(patch.replacement.size(), 1u);
  EXPECT_EQ(managed::apply_snapshot_patch(base, patch, target.size()), target);
  auto stale = base;
  stale.front() = 0;
  EXPECT_THROW(managed::apply_snapshot_patch(stale, patch, target.size()), std::invalid_argument);
  auto malformed = patch;
  malformed.prefix_bytes = base.size() + 1;
  EXPECT_THROW(managed::apply_snapshot_patch(base, malformed, target.size()), std::invalid_argument);
  malformed = patch;
  malformed.target_digest.front() ^= 1;
  EXPECT_THROW(managed::apply_snapshot_patch(base, malformed, target.size()), std::invalid_argument);
  EXPECT_THROW(managed::apply_snapshot_patch(base, patch, target.size() - 1), std::invalid_argument);
  managed::snapshot_delta_chain chain{base, base.size(), 2};
  chain.append(target);
  EXPECT_EQ(chain.delta_count(), 1u);
  target[10001] = 3;
  chain.append(target);
  EXPECT_EQ(chain.delta_count(), 2u);
  target[10002] = 4;
  chain.append(target);
  EXPECT_EQ(chain.delta_count(), 0u);
  EXPECT_EQ(chain.current(), target);
  const std::vector<std::uint8_t> oversized(base.size() + 1);
  EXPECT_THROW(chain.append(oversized), std::length_error);
  EXPECT_EQ(chain.current(), target);
}

// Envelope delta transfer retains complete history and uses ordinary atomic load publication.
TEST(managed_runtime_features, envelope_delta_round_trip_preserves_history) {
  store_type source{initial_value(), {51, 52}};
  const auto baseline = source.save();
  change_text(source, "Second").throw_if_failed();
  const auto patch = source.save_delta(baseline);
  const auto encoded = managed::detail::encode(patch);
  const auto decoded = managed::detail::decode<managed::records::snapshot_patch>(
      encoded, rohit::serializer::decode_limits{});
  store_type target{initial_value(), {53, 54}};
  target.load_delta(baseline, decoded);
  EXPECT_EQ(payload(target).text, "Second");
  EXPECT_EQ(target.save(), source.save());
  target.undo();
  EXPECT_EQ(payload(target).text, "First");
  const auto pinned = target.read();
  auto corrupt = decoded;
  corrupt.target_digest.front() ^= 1;
  EXPECT_THROW(target.load_delta(baseline, corrupt), std::invalid_argument);
  EXPECT_EQ(target.read(), pinned);
}

// Opt-in journal compression must preserve the same envelope/history/allocator semantics as full frames.
TEST(managed_runtime_features, delta_journal_preserves_history_and_bounded_replay) {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
                         ("serializer-delta-journal-" + std::to_string(nonce));
  std::filesystem::create_directory(directory);
  struct cleanup {
    std::filesystem::path path;
    // Remove only the unique qualification directory after stores release native handles.
    ~cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
  } cleanup_directory{directory};
  const auto full_path = directory / "full.document";
  const auto delta_path = directory / "delta.document";
  auto large_document = initial_value();
  large_document.text.assign(100000, 'a');
  std::vector<std::uint8_t> expected;
  const auto write = [&](const auto& path, bool deltas) {
    managed::store_options options;
    options.delta_journal = deltas;
    options.max_delta_chain = 2;
    store_type store{large_document, {81, 82}, options};
    store.create_journal(path);
    for (std::size_t index = 0; index != 5; ++index) {
      auto text = store.clone_value().text;
      text[50000 + index] = 'b';
      change_text(store, std::move(text)).throw_if_failed();
    }
    store.undo();
    store.redo();
    store.execute_transaction([](auto& edit) {
      static_cast<void>(edit.create_id());
      edit.revert();
    }).throw_if_failed();
    expected = store.save();
  };
  write(full_path, false);
  const auto full_expected = expected;
  write(delta_path, true);
  EXPECT_EQ(expected, full_expected);
  EXPECT_LT(std::filesystem::file_size(delta_path), std::filesystem::file_size(full_path));
  {
    store_type full{initial_value(), {83, 84}};
    store_type delta{initial_value(), {85, 86}};
    full.recover_journal(full_path);
    delta.recover_journal(delta_path); // Reading new frames does not require delta_journal=true.
    EXPECT_EQ(delta.save(), full.save());
    EXPECT_EQ(delta.save(), expected);
    EXPECT_EQ(delta.allocated_id(), 2u);
    delta.undo();
    EXPECT_EQ(payload(delta).text[50004], 'a');
    delta.redo();
    EXPECT_EQ(payload(delta).text[50004], 'b');
    delta.save_journal(); // A full checkpoint resets the bounded replay counter.
  }
  // A stricter receiver rejects an over-depth source before replacing its accepted root.
  // Use a separate source because the full checkpoint above deliberately removed the original chain.
  const auto strict_path = directory / "strict.document";
  write(strict_path, true);
  managed::store_options limits;
  limits.max_delta_chain = 1;
  store_type strict{initial_value(), {87, 88}, limits};
  const auto pinned = strict.read();
  EXPECT_THROW(strict.recover_journal(strict_path), std::length_error);
  EXPECT_EQ(strict.read(), pinned);
}

// Append inside runtime_features_test.cpp's anonymous namespace after review.
// Same-document migration must not reuse tree revision numbers discarded from a newer live store.
TEST(managed_runtime_features, migration_rejects_regressing_tree_revision_watermark) {
  using old_tree = managed::model_store<root, managed::history_mode::tree,
                                       managed::history_labels::disabled, previous_traits>;
  using new_tree = managed::model_store<root, managed::history_mode::tree>;
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
                         ("serializer-revision-migration-" + std::to_string(nonce));
  std::filesystem::create_directory(directory);
  struct cleanup {
    std::filesystem::path path;
    // Delete only this fixture's unique temporary directory after journals release their handles.
    ~cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
  } cleanup_directory{directory};
  const auto source_path = directory / "source.document";
  const auto destination_path = directory / "migrated.document";
  {
    old_tree source{initial_value(), {91, 92}};
    source.create_journal(source_path);
    change_text(source, "Released revision").throw_if_failed();
  }
  new_tree live{initial_value(), {91, 92}};
  change_text(live, "Live revision two").throw_if_failed();
  change_text(live, "Live revision three").throw_if_failed();
  live.undo();
  const auto pinned = live.read();
  const auto before = live.save();
  EXPECT_THROW((live.template recover_migrated_journal<root, previous_traits>(
      source_path, destination_path,
      [](const traits::storage_type& value) { return value; })), std::invalid_argument);
  EXPECT_EQ(live.read(), pinned);
  EXPECT_EQ(live.save(), before);
  EXPECT_EQ(live.current_revision(), 2u);
  EXPECT_FALSE(std::filesystem::exists(destination_path));
  change_text(live, "Live revision four").throw_if_failed();
  EXPECT_EQ(live.current_revision(), 4u);
}

// Append inside runtime_features_test.cpp's anonymous namespace after independent review.
// A pinned old owning model consumes admission budget until its last reader releases it.
TEST(managed_runtime_features, resident_budget_tracks_pinned_models_and_rejects_atomically) {
  using disabled_store = managed::model_store<root, managed::history_mode::disabled>;
  auto value = initial_value();
  value.text.assign(4000, 'a');
  disabled_store probe{value, {101, 102}};
  const auto baseline = probe.resident_bytes();
  const auto model_cost = traits::estimate_memory(*probe.read()) +
      sizeof(managed::detail::resident_storage<traits::storage_type>) -
      sizeof(traits::storage_type) + 8 * sizeof(void*);
  managed::store_options limits;
  limits.max_resident_bytes = baseline + model_cost + 128;
  disabled_store store{value, {103, 104}, limits};
  auto pinned = store.read();
  change_text(store, std::string(4000, 'b')).throw_if_failed();
  EXPECT_EQ(traits::payload(*pinned).text.front(), 'a');
  EXPECT_GE(store.resident_bytes(), baseline + model_cost);
  const auto accepted = store.read();
  const auto stamp = store.state();
  auto failure = change_text(store, std::string(4000, 'c'));
  EXPECT_EQ(failure.status, status::failed);
  EXPECT_THROW(failure.throw_if_failed(), std::length_error);
  EXPECT_EQ(store.read(), accepted);
  EXPECT_EQ(store.state(), stamp);
  pinned.reset();
  EXPECT_LT(store.resident_bytes(), baseline + model_cost);
  change_text(store, std::string(4000, 'c')).throw_if_failed();
  EXPECT_EQ(payload(store).text.front(), 'c');
}

// Serialized and owning-size limits are independent; an oversized candidate cannot publish.
TEST(managed_runtime_features, resident_budget_rejects_large_edits_load_and_tiny_construction) {
  managed::store_options tiny;
  tiny.max_resident_bytes = 1;
  EXPECT_THROW((store_type{initial_value(), {111, 112}, tiny}), std::length_error);
  managed::store_options limits;
  limits.max_resident_bytes = 8192;
  store_type store{initial_value(), {113, 114}, limits};
  const auto pinned = store.read();
  const auto before = store.save();
  const auto cursor = store.history_cursor();
  const auto failure = change_text(store, std::string(20000, 'x'));
  EXPECT_EQ(failure.status, status::failed);
  EXPECT_THROW(failure.throw_if_failed(), std::length_error);
  EXPECT_EQ(store.read(), pinned);
  EXPECT_EQ(store.save(), before);
  EXPECT_EQ(store.history_cursor(), cursor);
  auto large = initial_value();
  large.text.assign(20000, 'x');
  store_type source{large, {115, 116}};
  EXPECT_THROW(store.load(source.save()), std::length_error);
  EXPECT_EQ(store.read(), pinned);
  EXPECT_EQ(store.save(), before);
}

// The scoped resource hook owns root/control-block allocations through store and pin destruction.
TEST(managed_runtime_features, resident_allocation_resource_lifetime_and_balanced_release) {
  struct counts { std::size_t allocations{}, deallocations{}, outstanding{}; bool destroyed{}; };
  struct resource final : std::pmr::memory_resource {
    std::shared_ptr<counts> state;
    // Keep qualification counters alive independently of the resource itself.
    explicit resource(std::shared_ptr<counts> value) : state{std::move(value)} {}
    // Record resource lifetime only after every shared control block has returned its allocation.
    ~resource() override { state->destroyed = true; }
    // Forward real allocations through the standard PMR resource while counting root/control blocks.
    void* do_allocate(std::size_t size, std::size_t alignment) override {
      auto pointer = std::pmr::new_delete_resource()->allocate(size, alignment);
      ++state->allocations; state->outstanding += size; return pointer;
    }
    // Count release after the exact aligned allocation is returned.
    void do_deallocate(void* pointer, std::size_t size, std::size_t alignment) override {
      std::pmr::new_delete_resource()->deallocate(pointer, size, alignment);
      ++state->deallocations; state->outstanding -= size;
    }
    // Separate resource instances never share allocation ownership.
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
      return this == &other;
    }
  };
  auto state = std::make_shared<counts>();
  auto allocation = std::make_shared<resource>(state);
  std::weak_ptr<std::pmr::memory_resource> lifetime = allocation;
  std::shared_ptr<const traits::storage_type> pinned;
  std::weak_ptr<const traits::storage_type> weak;
  {
    managed::store_options options;
    options.allocation_resource = allocation;
    store_type store{initial_value(), {121, 122}, options};
    pinned = store.read();
    weak = pinned;
    allocation.reset();
    change_text(store, "Second").throw_if_failed();
    EXPECT_GT(state->allocations, 1u);
    EXPECT_GT(state->outstanding, 0u);
  }
  EXPECT_FALSE(lifetime.expired());
  EXPECT_EQ(traits::payload(*pinned).text, "First");
  pinned.reset();
  EXPECT_FALSE(lifetime.expired()); // A weak shared-control-block reference also retains the resource.
  weak.reset();
  EXPECT_TRUE(lifetime.expired());
  EXPECT_TRUE(state->destroyed);
  EXPECT_EQ(state->outstanding, 0u);
  EXPECT_EQ(state->allocations, state->deallocations);
}

// Admission arithmetic must reject overflow and release reservations without unsigned wrapping.
TEST(managed_runtime_features, resident_estimation_and_ledger_overflow_are_checked) {
  std::size_t amount = std::numeric_limits<std::size_t>::max();
  EXPECT_THROW(managed::detail::add_memory_estimate(amount, 1), std::length_error);
  EXPECT_THROW(managed::detail::multiply_memory_estimate(amount, 2), std::length_error);
  auto ledger = std::make_shared<managed::detail::resident_ledger>(100);
  {
    managed::detail::resident_charge first{ledger, 70};
    EXPECT_THROW((managed::detail::resident_charge{ledger, 31}), std::length_error);
    EXPECT_EQ(ledger->bytes(), 70u);
    first.resize(50);
    managed::detail::resident_charge second{ledger, 50};
    EXPECT_EQ(ledger->bytes(), 100u);
  }
  EXPECT_EQ(ledger->bytes(), 0u);
}

// Resource failures and callback growth must be rejected before any durable journal append.
TEST(managed_runtime_features, resident_failures_precede_journal_io_and_runtime_acceptance) {
  struct failing_resource final : std::pmr::memory_resource {
    bool fail{};
    // Qualification switches the scoped allocation hook only after the initial document is durable.
    void* do_allocate(std::size_t size, std::size_t alignment) override {
      if (fail) { throw std::bad_alloc{}; }
      return std::pmr::new_delete_resource()->allocate(size, alignment);
    }
    // Matching releases remain valid even after subsequent allocation attempts are configured to fail.
    void do_deallocate(void* pointer, std::size_t size, std::size_t alignment) override {
      std::pmr::new_delete_resource()->deallocate(pointer, size, alignment);
    }
    // The resource never redirects deallocation to a distinct instance.
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
      return this == &other;
    }
  };
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
                         ("serializer-resident-journal-" + std::to_string(nonce));
  std::filesystem::create_directory(directory);
  struct cleanup {
    std::filesystem::path path;
    // Delete only the unique qualification folder after the journal owner releases native handles.
    ~cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
  } cleanup_directory{directory};
  auto resource = std::make_shared<failing_resource>();
  managed::store_options limits;
  limits.max_resident_bytes = 8192;
  limits.allocation_resource = resource;
  store_type store{initial_value(), {131, 132}, limits};
  const auto path = directory / "document.journal";
  store.create_journal(path);
  const auto size = std::filesystem::file_size(path);
  const auto pinned = store.read();
  const auto before = store.save();
  const auto stamp = store.state();
  bool called{};
  resource->fail = true;
  const auto failed_allocation = store.execute_transaction([&](auto&) { called = true; });
  EXPECT_EQ(failed_allocation.status, status::failed);
  EXPECT_THROW(failed_allocation.throw_if_failed(), std::bad_alloc);
  EXPECT_FALSE(called);
  resource->fail = false;
  const auto failed_durable = change_text(store, std::string(20000, 'x'));
  EXPECT_EQ(failed_durable.status, status::failed);
  EXPECT_THROW(failed_durable.throw_if_failed(), std::length_error);
  const auto failed_runtime = store.update_runtime([](auto& edit) {
    edit.update([](auto& value) { traits::payload(value).lookup_buffer.resize(20000); });
  });
  EXPECT_EQ(failed_runtime.status, status::failed);
  EXPECT_THROW(failed_runtime.throw_if_failed(), std::length_error);
  EXPECT_EQ(store.read(), pinned);
  EXPECT_EQ(store.save(), before);
  EXPECT_EQ(store.state(), stamp);
  EXPECT_EQ(std::filesystem::file_size(path), size);
  EXPECT_TRUE(payload(store).lookup_buffer.empty());
  change_text(store, "Second").throw_if_failed();
  EXPECT_GT(std::filesystem::file_size(path), size);
}

// Append inside runtime_features_test.cpp's anonymous namespace after independent review.
// Runtime generation and cache resets do not masquerade as durable entity changes.
TEST(managed_runtime_features, precise_notifications_exclude_runtime_edits_and_durable_noops) {
  store_type store{initial_value(), {141, 142}};
  const auto root_id = store.read()->persistent_id;
  std::size_t notifications{};
  std::vector<store_type::id_type> changed, invalidated;
  bool precise{};
  store.set_publication_callback([&](const auto& event) {
    ++notifications;
    changed = event.changed_ids;
    invalidated = event.invalidated_ids;
    precise = event.changed_ids_precise;
    EXPECT_EQ(event.after, store.read());
  });
  store.update_runtime([](auto& edit) { edit.root().set_lookup_cache(77); }).throw_if_failed();
  EXPECT_TRUE(changed.empty());
  EXPECT_TRUE(invalidated.empty());
  EXPECT_TRUE(precise);
  change_text(store, "Second").throw_if_failed();
  EXPECT_EQ(changed, (std::vector<store_type::id_type>{root_id}));
  EXPECT_EQ(invalidated, changed);
  EXPECT_TRUE(precise);
  const auto accepted_notifications = notifications;
  change_text(store, "Second").throw_if_failed();
  EXPECT_EQ(notifications, accepted_notifications);
  store.undo();
  EXPECT_EQ(changed, (std::vector<store_type::id_type>{root_id}));
  store.redo();
  EXPECT_EQ(changed, (std::vector<store_type::id_type>{root_id}));
  auto same_values = initial_value();
  same_values.text = "Second";
  store_type different_document{same_values, {143, 144}};
  store.load(different_document.save());
  EXPECT_EQ(changed, (std::vector<store_type::id_type>{root_id}));
}

// Hide traversal to simulate older handwritten model traits without durable-field metadata.
struct notification_legacy_traits : traits {
  template <typename Value, typename Visitor>
  static void visit_collaboration_fields(const Value&, Visitor&) = delete;
};
// Legacy handwritten traits must disclose conservative precision instead of claiming exact entity changes.
TEST(managed_runtime_features, notification_fallback_discloses_conservative_changed_ids) {
  using legacy_store = managed::model_store<root, managed::history_mode::linear,
                                           managed::history_labels::disabled, notification_legacy_traits>;
  legacy_store store{initial_value(), {151, 152}};
  bool called{};
  store.set_publication_callback([&](const auto& event) {
    called = true;
    EXPECT_FALSE(event.changed_ids_precise);
    EXPECT_EQ(event.changed_ids, event.invalidated_ids);
  });
  change_text(store, "Second").throw_if_failed();
  EXPECT_TRUE(called);
}

// A failed durable comparison must leave an attached journal and accepted state untouched.
struct notification_failing_traits : traits {
  // Simulate pre-publication generated traversal failure without issuing journal bytes.
  template <typename Value, typename Visitor>
  static void visit_collaboration_fields(const Value&, Visitor&) {
    throw std::runtime_error{"Qualification durable comparison failure"};
  }
};
TEST(managed_runtime_features, comparison_failure_precedes_journal_acceptance) {
  using failing_store = managed::model_store<root, managed::history_mode::linear,
      managed::history_labels::disabled, notification_failing_traits>;
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path() /
                         ("serializer-comparison-journal-" + std::to_string(nonce));
  std::filesystem::create_directory(directory);
  struct cleanup {
    std::filesystem::path path;
    // Delete only this fixture's unique folder after the journal owner releases its handles.
    ~cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
  } cleanup_directory{directory};
  failing_store store{initial_value(), {161, 162}};
  const auto path = directory / "document.journal";
  store.create_journal(path);
  const auto bytes = std::filesystem::file_size(path);
  const auto pinned = store.read();
  const auto before = store.save();
  bool notified{};
  store.set_publication_callback([&](const auto&) { notified = true; });
  const auto failed = change_text(store, "Second");
  EXPECT_EQ(failed.status, status::failed);
  EXPECT_THROW(failed.throw_if_failed(), std::runtime_error);
  EXPECT_FALSE(notified);
  EXPECT_EQ(store.read(), pinned);
  EXPECT_EQ(store.save(), before);
  EXPECT_EQ(std::filesystem::file_size(path), bytes);
  store.set_publication_callback({});
  change_text(store, "Second").throw_if_failed();
  EXPECT_GT(std::filesystem::file_size(path), bytes);
}

} // namespace
