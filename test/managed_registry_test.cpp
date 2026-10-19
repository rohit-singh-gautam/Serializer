// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_registry.hpp>
#include <rohit/managed_collaboration.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <array>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

namespace {
namespace managed = rohit::managed;
using root = registry_example::document;
using entry = registry_example::entry;
using store_type = managed::model_store<root>;
using traits = managed::model_traits<root>;
using descriptors = managed::fields<entry>;

// Construct two entities with durable application keys distinct from their internal IDs.
root initial() {
  root value;
  value.entries.resize(2);
  value.entries[0].key = "alpha";
  value.entries[0].count = 10;
  value.entries[0].group = 4;
  value.entries[0].application_id.number = 100;
  value.entries[1].key = "beta";
  value.entries[1].count = 20;
  value.entries[1].group = 4;
  value.entries[1].application_id.number = 101;
  return value;
}

// Read the first entity ID without depending on direct versus separate-value storage.
auto first_id(const store_type& store) {
  return traits::payload(*store.read()).entries.front().persistent_id;
}

// Supply a dormant imported identity only in profiles where values contain identity metadata.
template <typename Value>
void supply_imported_identity(Value& value) {
  if constexpr (requires { value.persistent_id; }) { value.persistent_id = 999; }
}

// Own one fresh test directory within the known ignored CMake binary directory.
struct journal_directory {
  const std::filesystem::path base{SERIALIZER_REGISTRY_TEST_DIRECTORY};
  const std::filesystem::path path{base / ("registry-" SERIALIZER_REGISTRY_TEST_PROFILE "-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  // Reject a reused or out-of-bound directory before creating any test journal files.
  journal_directory() {
    if (!base.is_absolute() || path.parent_path() != base || !std::filesystem::create_directory(path)) {
      throw std::runtime_error{"Invalid registry test directory"};
    }
  }
  // Remove only this test's fresh child directory, preserving useful build artifacts.
  ~journal_directory() {
    std::error_code error;
    if (path.parent_path() == base) { std::filesystem::remove_all(path, error); }
  }
};

// A provider retains its payload separately from the shared read-lock token handed to leases.
struct external_payload {
  std::string value{"provider value"};
  std::shared_mutex mutex{};
};

// Owning this token keeps the provider storage alive and excludes writes for the lease lifetime.
struct external_read_guard {
  std::shared_ptr<external_payload> owner;
  std::shared_lock<std::shared_mutex> lock;
  // Acquire synchronization before a resolver exposes any borrowed payload reference.
  explicit external_read_guard(std::shared_ptr<external_payload> value)
      : owner{std::move(value)}, lock{owner->mutex} {}
};
} // namespace

// Pinned locators and indexes retain their exact selected values across commit and undo.
TEST(managed_registry, typed_snapshot_lookup_and_indexes) {
  store_type store{initial(), {11, 22}};
  const auto id = first_id(store);
  const auto registry = store.registry();
  EXPECT_EQ(registry.size(), 3u);
  EXPECT_EQ(registry.document().high, 11u);
  EXPECT_EQ(registry.type(id), "registry_example::entry");
  const auto pinned = registry.find<entry>(id);
  EXPECT_EQ(pinned.value().count, 10);
  EXPECT_EQ(registry.member(id, descriptors::field_count()), 10);
  EXPECT_EQ((registry.member<entry, std::int64_t>(id, 2)), 10);
  EXPECT_THROW((registry.member<entry, std::uint64_t>(id, 2)), managed::registry_error);
  EXPECT_THROW((registry.member<entry, std::int64_t>(id, 99)), managed::registry_error);
  EXPECT_THROW(registry.find<root>(id), managed::registry_error);
  EXPECT_THROW(registry.find<entry>(0), managed::registry_error);
  EXPECT_EQ(registry.ordered(descriptors::field_key(), true).at("alpha").front(), id);
  EXPECT_EQ(registry.hashed(descriptors::field_group()).at(4).size(), 2u);
  store.execute_transaction([&](auto& tx) { tx.registry().template edit<entry>(id).set_count(31); }).throw_if_failed();
  EXPECT_EQ(store.registry().find<entry>(id).value().count, 31);
  EXPECT_EQ(pinned.value().count, 10);
  EXPECT_EQ(registry.find<entry>(id).value().count, 10);
  store.undo();
  EXPECT_EQ(store.registry().find<entry>(id).value().count, 10);
  store.redo();
  EXPECT_EQ(store.registry().find<entry>(id).value().count, 31);
}

// Resolving each use prevents invalidation from vector relocation, erase, and closed transactions.
TEST(managed_registry, candidate_structural_changes_and_lifetime) {
  store_type store{initial()};
  const auto id = first_id(store);
  store_type::outcome_type outcome;
  auto tx = store.begin_transaction(outcome);
  auto registry = tx.registry();
  auto editor = registry.edit<entry>(id);
  tx.root().entries().append(entry{});
  editor.set_count(41);
  tx.commit();
  EXPECT_EQ(store.registry().find<entry>(id).value().count, 41);
  EXPECT_THROW(editor.set_count(42), std::logic_error);
  EXPECT_THROW(registry.edit<entry>(id), std::logic_error);
  const auto erased = store.execute_transaction([&](auto& edit) {
    auto stale = edit.registry().template edit<entry>(id);
    edit.root().entries().erase(id);
    EXPECT_THROW(stale.set_count(43), managed::registry_error);
  });
  EXPECT_EQ(erased.status, managed::transaction_status::failed);
  EXPECT_TRUE(store.registry().contains(id));
}

// Uniqueness is checked even for raw callback edits before any model/history publication.
TEST(managed_registry, custom_binding_and_duplicate_rollback) {
  store_type store{initial()};
  store.declare_unique_key(descriptors::field_key());
  const auto id = first_id(store);
  const auto before = store.save();
  const auto rejected = store.execute_transaction([&](auto& tx) {
    tx.registry().bind(id, descriptors::field_key(), std::string{"beta"});
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  EXPECT_EQ(store.save(), before);
  const auto raw_rejected = store.execute_transaction([](auto& tx) {
    tx.update([](auto& candidate) {
      managed::model_traits<entry>::payload(traits::payload(candidate).entries[0]).key = "beta";
    });
  });
  EXPECT_EQ(raw_rejected.status, managed::transaction_status::failed);
  store.execute_transaction([&](auto& tx) {
    tx.registry().bind(id, descriptors::field_key(), std::string{"gamma"});
  }).throw_if_failed();
  EXPECT_EQ(first_id(store), id);
  EXPECT_EQ(store.registry().ordered(descriptors::field_key(), true).at("gamma").front(), id);
  store.undo();
  EXPECT_EQ(store.registry().ordered(descriptors::field_key(), true).at("alpha").front(), id);
}

// Registry allocation is optional and obeys explicit catalog/admission budgets.
TEST(managed_registry, bounded_optional_catalog_and_external_lifetime) {
  store_type store{initial()};
  const auto before = store.resident_bytes();
  {
    const auto registry = store.registry();
    EXPECT_GT(store.resident_bytes(), before);
    EXPECT_EQ(registry.size(), 3u);
  }
  EXPECT_EQ(store.resident_bytes(), before);
  EXPECT_THROW(store.registry({2, 1024}), std::length_error);
  EXPECT_THROW(store.registry({10, 1}), std::length_error);
  EXPECT_EQ(store.resident_bytes(), before);
  auto external = std::make_shared<const std::string>("provider value");
  managed::external_registry<int, std::string> provider{[&](int key) {
    return key == 7 ? external : std::shared_ptr<const std::string>{};
  }};
  const auto pin = provider.find(7);
  external.reset();
  EXPECT_EQ(*pin, "provider value");
  EXPECT_THROW(provider.find(8), managed::registry_error);
}

// Rebinding selects a fresh version while held leases retain the old immutable payload.
TEST(managed_registry, external_leases_rebinding_and_version_selection) {
  auto payload = std::make_shared<const std::string>("first version");
  auto state = std::make_shared<managed::external_lease_state>(1);
  managed::leased_external_registry<int, std::string> provider{
      [&](int key) -> std::optional<managed::external_lease<std::string>> {
        if (key != 7) { return std::nullopt; }
        return managed::external_lease<std::string>{*payload, payload, state};
      }};
  const auto held = provider.find(7, 1);
  const auto copy = held;
  const std::weak_ptr<const std::string> previous = payload;
  store_type store{initial()};
  store.execute_transaction([](auto& tx) { tx.root().set_title("changed"); }).throw_if_failed();
  const auto saved = store.save();
  payload = std::make_shared<const std::string>("second version");
  state = std::make_shared<managed::external_lease_state>(2);
  EXPECT_FALSE(previous.expired());
  EXPECT_TRUE(held.is_valid());
  EXPECT_EQ(held.version(), 1u);
  EXPECT_EQ(copy.value(), "first version");
  auto selected = provider.find(7, 2);
  EXPECT_EQ(selected.value(), "second version");
  EXPECT_EQ(store.save(), saved);
  store.undo();
  const auto after_undo = provider.find(7, 2);
  EXPECT_EQ(after_undo.value(), "second version");
  try {
    static_cast<void>(provider.find(7, 1));
    FAIL() << "A mismatched external provider version was accepted";
  } catch (const managed::registry_error& error) {
    EXPECT_EQ(error.code(), managed::registry_error_code::wrong_version);
  }
  try {
    static_cast<void>(provider.find(8));
    FAIL() << "A missing external key was accepted";
  } catch (const managed::registry_error& error) {
    EXPECT_EQ(error.code(), managed::registry_error_code::missing_id);
  }
}

// Revocation denies checked access without reclaiming payloads guarded by an outstanding handle.
TEST(managed_registry, external_leases_invalidation_and_guard_lifetime) {
  auto payload = std::make_shared<const std::string>("retained value");
  const std::weak_ptr<const std::string> retained = payload;
  auto state = std::make_shared<managed::external_lease_state>(3);
  std::optional<managed::external_lease<std::string>> held{
      std::in_place, *payload, payload, state};
  const auto& borrowed = held->value();
  payload.reset();
  state->invalidate();
  state->invalidate();
  EXPECT_FALSE(state->is_valid());
  EXPECT_FALSE(held->is_valid());
  EXPECT_EQ(held->version(), 3u);
  EXPECT_FALSE(retained.expired());
  EXPECT_EQ(borrowed, "retained value");
  try {
    static_cast<void>(held->value());
    FAIL() << "An invalidated external lease remained readable";
  } catch (const managed::registry_error& error) {
    EXPECT_EQ(error.code(), managed::registry_error_code::expired_lease);
  }
  managed::leased_external_registry<int, std::string> provider{[&](int) { return held; }};
  try {
    static_cast<void>(provider.find(7));
    FAIL() << "A resolver returned an invalidated external lease";
  } catch (const managed::registry_error& error) {
    EXPECT_EQ(error.code(), managed::registry_error_code::expired_lease);
  }
  held.reset();
  EXPECT_TRUE(retained.expired());
}

// Shared synchronization tokens exclude provider writes until all borrowing handles are released.
TEST(managed_registry, external_leases_synchronization_and_invalid_tokens) {
  auto payload = std::make_shared<external_payload>();
  auto state = std::make_shared<managed::external_lease_state>(4);
  managed::leased_external_registry<int, std::string> provider{
      [&](int) -> std::optional<managed::external_lease<std::string>> {
        auto guard = std::make_shared<external_read_guard>(payload);
        return managed::external_lease<std::string>{payload->value, std::move(guard), state};
      }};
  auto held = std::optional{provider.find(7)};
  EXPECT_EQ(held->value(), "provider value");
  state->invalidate();
  const auto can_write = [&] {
    return std::async(std::launch::async, [&] {
      std::unique_lock<std::shared_mutex> lock{payload->mutex, std::try_to_lock};
      return lock.owns_lock();
    }).get();
  };
  EXPECT_FALSE(can_write());
  held.reset();
  EXPECT_TRUE(can_write());

  using lease_type = managed::external_lease<std::string>;
  const std::shared_ptr<const void> empty_guard;
  const std::shared_ptr<const managed::external_lease_state> empty_state;
  EXPECT_THROW((lease_type{payload->value, empty_guard, state}), std::invalid_argument);
  EXPECT_THROW((lease_type{payload->value, payload, empty_state}), std::invalid_argument);
  const std::shared_ptr<const void> ownerless_guard{empty_guard, payload.get()};
  EXPECT_THROW((lease_type{payload->value, ownerless_guard, state}), std::invalid_argument);
  const std::shared_ptr<const managed::external_lease_state> ownerless_state{
      empty_state, state.get()};
  EXPECT_THROW((lease_type{payload->value, payload, ownerless_state}), std::invalid_argument);
  EXPECT_THROW(managed::external_lease_state{0}, std::invalid_argument);
  EXPECT_THROW((managed::leased_external_registry<int, std::string>{nullptr}),
               std::invalid_argument);
  static_assert(!std::is_default_constructible_v<lease_type>);
  static_assert(!std::is_constructible_v<lease_type, const std::string&>);
  static_assert(!std::is_constructible_v<lease_type, std::string,
                std::shared_ptr<const void>,
                std::shared_ptr<const managed::external_lease_state>>);
}

// Moving transfers the tokens and prevents the source handle from exposing an unguarded reference.
TEST(managed_registry, external_leases_moved_handles) {
  const auto payload = std::make_shared<const std::string>("moved value");
  const auto state = std::make_shared<managed::external_lease_state>(5);
  managed::external_lease<std::string> source{*payload, payload, state};
  const auto destination = std::move(source);
  EXPECT_FALSE(source.is_valid());
  EXPECT_THROW(source.value(), managed::registry_error);
  EXPECT_THROW(source.version(), managed::registry_error);
  EXPECT_TRUE(destination.is_valid());
  EXPECT_EQ(destination.value(), "moved value");
}

// Stable field/base paths work across private ordinary projections and both storage layouts.
TEST(managed_registry, nested_member_paths_and_composite_keys) {
  store_type store{initial()};
  const auto id = first_id(store);
  constexpr std::array<std::uint32_t, 2> rank_path{6, 1};
  constexpr std::array<std::uint32_t, 2> note_path{5, 1};
  store.execute_transaction([&](auto& tx) {
    tx.registry().template bind<entry>(id, rank_path, std::int32_t{8});
    tx.registry().template bind<entry>(id, note_path, std::string{"owned base"});
  }).throw_if_failed();
  const auto registry = store.registry();
  EXPECT_EQ((registry.member<entry, std::int32_t>(id, rank_path)), 8);
  EXPECT_EQ((registry.member<entry, std::string>(id, note_path)), "owned base");
  const auto rank_index = registry.hashed(managed::field_path<entry, std::int32_t>({6, 1}));
  EXPECT_EQ(rank_index.at(8).front(), id);
  const auto key = registry.member(id, descriptors::field_application_id());
  store.declare_unique_key(descriptors::field_application_id());
  EXPECT_EQ(registry.ordered(descriptors::field_application_id(), true).at(key).front(), id);
}

// Imported values receive transaction-owned reservations and retirement never recycles IDs.
TEST(managed_registry, reservations_adoption_retirement_and_rejection) {
  store_type store{initial()};
  auto imported = entry{};
  imported.key = "imported";
  supply_imported_identity(imported);
  auto adopted = store_type::id_type{};
  store.execute_transaction([&](auto& tx) {
    const auto registry = tx.registry();
    adopted = registry.reserve();
    registry.adopt_reserved(adopted, imported, [](auto& candidate, auto value) {
      traits::payload(candidate).entries.push_back(std::move(value));
    });
  }).throw_if_failed();
  EXPECT_NE(adopted, 999u);
  EXPECT_EQ(store.registry().find<entry>(adopted).value().key, "imported");
  const auto high_water = store.allocated_id();
  store.execute_transaction([&](auto& tx) {
    tx.registry().template retire<entry>(adopted, [](auto& candidate, auto id) {
      auto& values = traits::payload(candidate).entries;
      std::erase_if(values, [id](const auto& value) { return value.persistent_id == id; });
    });
  }).throw_if_failed();
  EXPECT_FALSE(store.registry().contains(adopted));
  EXPECT_EQ(store.allocated_id(), high_water);
  const auto rejected = store.execute_transaction([&](auto& tx) {
    tx.registry().adopt_reserved(adopted, imported, [](auto&, auto) {});
  });
  EXPECT_EQ(rejected.status, managed::transaction_status::failed);
  EXPECT_FALSE(store.registry().contains(adopted));
  store.undo();
  EXPECT_TRUE(store.registry().contains(adopted));
  const auto same_transaction = store.execute_transaction([&](auto& tx) {
    const auto registry = tx.registry();
    const auto fresh = registry.adopt(imported, [](auto& candidate, auto value) {
      traits::payload(candidate).entries.push_back(std::move(value));
    });
    registry.template retire<entry>(fresh, [](auto& candidate, auto id) {
      auto& values = traits::payload(candidate).entries;
      std::erase_if(values, [id](const auto& value) { return value.persistent_id == id; });
    });
    registry.adopt_reserved(fresh, imported, [](auto&, auto) {});
  });
  EXPECT_EQ(same_transaction.status, managed::transaction_status::failed);
}

// Recovery, Save, and inverse navigation use the unchanged authoritative journal protocol.
TEST(managed_registry, appended_and_sidecar_recovery_and_unique_invariants) {
  journal_directory directory;
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    const auto path = directory.path / (mode == managed::journal_storage_mode::appended ? "appended" : "sidecar");
    auto id = store_type::id_type{};
    {
      store_type store{initial()};
      store.declare_unique_key(descriptors::field_key());
      id = first_id(store);
      store.create_journal(path, mode);
      const auto sequence = store.journal_sequence();
      const auto rejected = store.execute_transaction([&](auto& tx) {
        tx.registry().bind(id, descriptors::field_key(), std::string{"beta"});
      });
      EXPECT_EQ(rejected.status, managed::transaction_status::failed);
      EXPECT_EQ(store.journal_sequence(), sequence);
      store.execute_transaction([&](auto& tx) { tx.registry().template edit<entry>(id).set_count(51); }).throw_if_failed();
      store.save_journal();
      store.undo();
    }
    store_type recovered{root{}};
    recovered.declare_unique_key(descriptors::field_key());
    recovered.recover_journal(path);
    EXPECT_EQ(recovered.registry().find<entry>(id).value().count, 10);
    recovered.redo();
    EXPECT_EQ(recovered.registry().find<entry>(id).value().count, 51);
  }
}

// Tree navigation rebuilds exact selected keys and comparison callbacks cannot reenter publication.
TEST(managed_registry, tree_checkout_and_comparison_reentry) {
  managed::model_store<root, managed::history_mode::tree> tree{initial()};
  tree.declare_unique_key(descriptors::field_key());
  const auto id = traits::payload(*tree.read()).entries.front().persistent_id;
  const auto baseline = tree.current_revision();
  const auto pinned = tree.registry().find<entry>(id);
  tree.execute_transaction([&](auto& tx) {
    tx.registry().bind(id, descriptors::field_key(), std::string{"branch"});
  }).throw_if_failed();
  const auto branch = tree.current_revision();
  tree.checkout(baseline);
  EXPECT_EQ(tree.registry().ordered(descriptors::field_key(), true).at("alpha").front(), id);
  tree.checkout(branch);
  EXPECT_EQ(tree.registry().ordered(descriptors::field_key(), true).at("branch").front(), id);
  EXPECT_EQ(pinned.value().key, "alpha");

  store_type store{initial()};
  const auto before = store.save();
  bool compared{};
  store.declare_unique_key(descriptors::field_key(), [&](const auto& left, const auto& right) {
    compared = true;
    const auto nested = store.execute_transaction([](auto& tx) { tx.root().set_title("reentered"); });
    EXPECT_EQ(nested.status, managed::transaction_status::failed);
    return left < right;
  });
  EXPECT_TRUE(compared);
  EXPECT_EQ(store.save(), before);
}

// Authority registry mutation still requires trusted sessions and normal publication checks.
TEST(managed_registry, collaboration_authorization_and_exact_retries) {
  managed::collaboration_authority<root> authority{initial(), 1};
  authority.declare_unique_key(descriptors::field_key());
  authority.open_session(1);
  authority.open_session(2, false);
  managed::collaboration_replica<root> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto id = traits::payload(*authority.read()).entries.front().persistent_id;
  const auto denied = replica.propose(2, 1, [&](auto& tx) { tx.registry().template edit<entry>(id).set_count(61); });
  EXPECT_EQ(authority.submit_change(denied, 2, 0)->status, managed::collaboration_status::denied);
  EXPECT_EQ(managed::model_traits<entry>::payload(traits::payload(*authority.read()).entries.front()).count, 10);
  const auto proposal = replica.propose(1, 1, [&](auto& tx) { tx.registry().template edit<entry>(id).set_count(62); });
  const auto accepted = authority.submit_change(proposal, 1, 0);
  ASSERT_EQ(accepted->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.submit_change(proposal, 1, 0), accepted);
  replica.apply_accepted_change(*accepted->accepted, authority.context());
  EXPECT_EQ(authority.registry().find<entry>(id).value().count, 62);
  const auto duplicate = replica.propose(1, 2, [&](auto& tx) {
    tx.registry().bind(id, descriptors::field_key(), std::string{"beta"});
  });
  EXPECT_EQ(authority.submit_change(duplicate, 1, 0)->status, managed::collaboration_status::failed);
  const auto undo = authority.submit_change(replica.undo(1, 3), 1, 0);
  ASSERT_EQ(undo->status, managed::collaboration_status::accepted);
  EXPECT_EQ(managed::model_traits<entry>::payload(traits::payload(*authority.read()).entries.front()).count, 10);
}

// Canceling discards private edits while already exposed native IDs remain permanently consumed.
TEST(managed_registry, preview_cancel_preserves_snapshot_and_burns_reserved_identities) {
  store_type store{initial()};
  const auto accepted = store.read();
  const auto allocation = store.allocated_id();
  const auto state = store.state();
  auto proposal = store.preview();
  EXPECT_EQ(proposal.working().read(), accepted);
  proposal.working().execute_transaction([](auto& edit) {
    edit.root().entries().append(entry{});
    edit.root().set_title("preview");
  }).throw_if_failed();
  EXPECT_GT(proposal.working().allocated_id(), allocation);
  const auto consumed = store.allocated_id();
  EXPECT_GT(consumed, allocation);
  EXPECT_EQ(store.read(), accepted);
  proposal.cancel();
  EXPECT_FALSE(proposal.active());
  EXPECT_THROW(proposal.working(), std::logic_error);
  EXPECT_EQ(store.read(), accepted);
  EXPECT_EQ(store.state(), state);
  EXPECT_EQ(store.allocated_id(), consumed);
}

// Draft tokens cannot retarget live objects, and competing workspaces cannot expose duplicate IDs.
TEST(managed_registry, preview_tokens_never_alias_later_live_or_concurrent_preview_entities) {
  store_type store{initial()};
  auto proposal = store.preview();
  auto concurrent = store.preview();
  proposal.working().execute_transaction([](auto& edit) {
    edit.root().entries().append(entry{});
  }).throw_if_failed();
  const auto id = traits::payload(*proposal.working().read()).entries.back().persistent_id;
  const auto token = proposal.working().registry().template reference<entry>(id);
  const auto collided = concurrent.working().execute_transaction([](auto& edit) {
    edit.root().entries().append(entry{});
  });
  EXPECT_THROW(collided.throw_if_failed(), std::logic_error);
  store.execute_transaction([](auto& edit) {
    edit.root().entries().append(entry{});
  }).throw_if_failed();
  EXPECT_GT(traits::payload(*store.read()).entries.back().persistent_id, id);
  EXPECT_THROW(store.registry().resolve(token), managed::registry_error);
  proposal.cancel();
  concurrent.cancel();
  store.execute_transaction([](auto& edit) {
    edit.root().entries().append(entry{});
  }).throw_if_failed();
  EXPECT_THROW(store.registry().resolve(token), managed::registry_error);
}

// Failed edits burn their source reservations, and moving a workspace preserves its allocator binding.
TEST(managed_registry, preview_failed_edit_and_move_keep_identity_reservations) {
  store_type store{initial()};
  auto proposal = store.preview();
  const auto before = store.allocated_id();
  const auto failed = proposal.working().execute_transaction([](auto& edit) {
    edit.root().entries().append(entry{});
    throw std::runtime_error{"Discard the private edit"};
  });
  EXPECT_THROW(failed.throw_if_failed(), std::runtime_error);
  const auto burned = store.allocated_id();
  EXPECT_GT(burned, before);
  auto moved = std::move(proposal);
  moved.working().execute_transaction([](auto& edit) {
    edit.root().entries().append(entry{});
  }).throw_if_failed();
  const auto created = traits::payload(*moved.working().read()).entries.back().persistent_id;
  EXPECT_GT(created, burned);
  const auto token = moved.working().registry().template reference<entry>(created);
  store.accept_preview(moved).throw_if_failed();
  EXPECT_EQ(store.registry().resolve(token).id(), created);
}

// Previously borrowed workspace handles cannot mutate or move the proposal during acceptance callbacks.
TEST(managed_registry, preview_acceptance_fences_workspace_validation_and_publication_reentry) {
  std::function<void()> reenter;
  store_type store{initial(), managed::make_document_id(), {}, [&](const auto&) {
    if (reenter) { reenter(); }
  }};
  auto proposal = store.preview();
  auto& working = proposal.working();
  working.execute_transaction([](auto& edit) {
    edit.root().set_title("selected preview");
  }).throw_if_failed();
  auto attempts = std::size_t{};
  reenter = [&] {
    ++attempts;
    const auto result = working.execute_transaction([](auto& edit) {
      edit.root().set_title("reentrant replacement");
    });
    EXPECT_THROW(result.throw_if_failed(), std::logic_error);
    EXPECT_THROW(static_cast<void>(store_type::preview_edit{std::move(proposal)}), std::logic_error);
    EXPECT_THROW(proposal.cancel(), std::logic_error);
  };
  store.set_publication_callback([&](const auto&) { reenter(); });
  store.accept_preview(proposal).throw_if_failed();
  EXPECT_GE(attempts, 3U);
  EXPECT_EQ(traits::payload(*store.read()).title, "selected preview");
  EXPECT_FALSE(proposal.active());
}

// Exact-base acceptance transfers identities once and uses ordinary native Undo/Redo.
TEST(managed_registry, preview_accept_preserves_new_ids_and_native_history) {
  store_type store{initial()};
  const auto root_id = store.read()->persistent_id;
  auto proposal = store.preview();
  proposal.working().execute_transaction([](auto& edit) {
    entry value{};
    value.key = "preview child";
    edit.root().entries().append(value);
    edit.root().set_title("accepted preview");
  }).throw_if_failed();
  const auto child_id = traits::payload(*proposal.working().read()).entries.back().persistent_id;
  const auto allocation = proposal.working().allocated_id();
  store.accept_preview(proposal).throw_if_failed();
  EXPECT_FALSE(proposal.active());
  EXPECT_EQ(store.read()->persistent_id, root_id);
  EXPECT_EQ(store.registry().find<entry>(child_id).value().key, "preview child");
  EXPECT_EQ(store.allocated_id(), allocation);
  store.undo();
  EXPECT_FALSE(store.registry().contains(child_id));
  EXPECT_TRUE(traits::payload(*store.read()).title.empty());
  store.redo();
  EXPECT_EQ(store.registry().find<entry>(child_id).value().key, "preview child");
  EXPECT_EQ(store.allocated_id(), allocation);
}

// Allocation-only changes fence acceptance even when the accepted snapshot is unchanged.
TEST(managed_registry, preview_rejects_stale_allocation_and_foreign_store) {
  store_type store{initial()};
  auto proposal = store.preview();
  store.execute_transaction([](auto& edit) {
    static_cast<void>(edit.create_id());
    edit.revert();
  }).throw_if_failed();
  const auto before = store.save();
  EXPECT_THROW(store.accept_preview(proposal).throw_if_failed(), std::logic_error);
  EXPECT_EQ(store.save(), before);
  EXPECT_TRUE(proposal.active());
  store_type other{initial(), store.document()};
  EXPECT_THROW(other.accept_preview(proposal).throw_if_failed(), std::logic_error);
}

// The full native snapshot retains accepted preview IDs across fresh-store recovery.
TEST(managed_registry, accepted_preview_survives_native_recovery) {
  store_type store{initial()};
  auto proposal = store.preview();
  proposal.working().execute_transaction([](auto& edit) {
    edit.root().entries().append(entry{});
  }).throw_if_failed();
  const auto child_id = traits::payload(*proposal.working().read()).entries.back().persistent_id;
  store.accept_preview(proposal).throw_if_failed();
  store_type recovered{initial()};
  recovered.load(store.save());
  EXPECT_EQ(recovered.document().high, store.document().high);
  EXPECT_EQ(recovered.document().low, store.document().low);
  EXPECT_TRUE(recovered.registry().contains(child_id));
  recovered.undo();
  EXPECT_FALSE(recovered.registry().contains(child_id));
  recovered.redo();
  EXPECT_TRUE(recovered.registry().contains(child_id));
}

// Editable application preserves the root, retains surviving child IDs and allocates only new ones.
TEST(managed_registry, apply_editable_reconciles_owned_entities_in_one_action) {
  store_type store{initial()};
  const auto root_id = store.read()->persistent_id;
  const auto removed = first_id(store);
  auto candidate = *store.read();
  auto& payload = traits::payload(candidate);
  payload.entries.erase(payload.entries.begin());
  const auto retained = payload.entries.front().persistent_id;
  managed::model_traits<entry>::payload(payload.entries.front()).count = 75;
  entry added{};
  added.key = "new editable child";
  payload.entries.push_back(managed::model_traits<entry>::make_storage(added));
  payload.title = "editable";
  candidate.persistent_id = 0;
  store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(candidate));
  }).throw_if_failed();
  EXPECT_EQ(store.read()->persistent_id, root_id);
  EXPECT_FALSE(store.registry().contains(removed));
  EXPECT_EQ(store.registry().find<entry>(retained).value().count, 75);
  const auto created = traits::payload(*store.read()).entries.back().persistent_id;
  EXPECT_NE(created, removed);
  EXPECT_NE(created, retained);
  EXPECT_EQ(store.registry().find<entry>(created).value().key, "new editable child");
  store.undo();
  EXPECT_TRUE(store.registry().contains(removed));
  EXPECT_FALSE(store.registry().contains(created));
  EXPECT_EQ(store.registry().find<entry>(retained).value().count, 20);
  store.redo();
  EXPECT_EQ(store.registry().find<entry>(created).value().key, "new editable child");
}

// Authored foreign IDs and duplicate ownership poison the transaction without changing live values.
TEST(managed_registry, apply_editable_rejects_foreign_and_duplicate_owned_ids) {
  store_type store{initial()};
  const auto before = store.save();
  const auto allocation = store.allocated_id();
  auto candidate = *store.read();
  traits::payload(candidate).entries.front().persistent_id =
      static_cast<store_type::id_type>(allocation + 100U);
  const auto foreign = store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(candidate));
  });
  EXPECT_THROW(foreign.throw_if_failed(), std::invalid_argument);
  EXPECT_EQ(store.allocated_id(), allocation);
  EXPECT_EQ(store.save(), before);
  candidate = *store.read();
  auto& entries = traits::payload(candidate).entries;
  entries.push_back(entries.front());
  const auto duplicate = store.execute_transaction([&](auto& edit) {
    edit.root().apply_editable(std::move(candidate));
  });
  EXPECT_THROW(duplicate.throw_if_failed(), std::invalid_argument);
  EXPECT_EQ(store.save(), before);
}

// Accepted preview reservations and the single edit replay through both native journal modes.
TEST(managed_registry, preview_accept_replays_native_journal_and_undo) {
  journal_directory directory;
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    const auto path = directory.path / (mode == managed::journal_storage_mode::appended ? "preview-appended" : "preview-sidecar");
    auto created = store_type::id_type{};
    auto allocation = std::uint64_t{};
    {
      store_type store{initial()};
      store.create_journal(path, mode);
      auto proposal = store.preview();
      proposal.working().execute_transaction([](auto& edit) {
        edit.root().entries().append(entry{});
        edit.root().set_title("journal preview");
      }).throw_if_failed();
      created = traits::payload(*proposal.working().read()).entries.back().persistent_id;
      store.accept_preview(proposal).throw_if_failed();
      allocation = store.allocated_id();
    }
    store_type recovered{root{}};
    recovered.recover_journal(path);
    EXPECT_TRUE(recovered.registry().contains(created));
    EXPECT_EQ(recovered.allocated_id(), allocation);
    recovered.undo();
    EXPECT_FALSE(recovered.registry().contains(created));
    recovered.redo();
    EXPECT_TRUE(recovered.registry().contains(created));
  }
}

// Canceled draft identities remain burned after recovery through either native journal layout.
TEST(managed_registry, preview_cancel_recovery_never_retargets_a_draft_token) {
  journal_directory directory;
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    const auto path = directory.path / (mode == managed::journal_storage_mode::appended ? "canceled-appended" : "canceled-sidecar");
    managed::managed_ref<entry, store_type::id_type> token;
    auto consumed = std::uint64_t{};
    {
      store_type store{initial()};
      store.create_journal(path, mode);
      auto proposal = store.preview();
      proposal.working().execute_transaction([](auto& edit) {
        edit.root().entries().append(entry{});
      }).throw_if_failed();
      const auto created = traits::payload(*proposal.working().read()).entries.back().persistent_id;
      token = proposal.working().registry().template reference<entry>(created);
      consumed = store.allocated_id();
      proposal.cancel();
    }
    store_type recovered{root{}};
    recovered.recover_journal(path);
    EXPECT_EQ(recovered.allocated_id(), consumed);
    recovered.execute_transaction([](auto& edit) {
      edit.root().entries().append(entry{});
    }).throw_if_failed();
    EXPECT_GT(traits::payload(*recovered.read()).entries.back().persistent_id, consumed);
    EXPECT_THROW(recovered.registry().resolve(token), managed::registry_error);
  }
}
