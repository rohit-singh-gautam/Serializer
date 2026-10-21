// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <branch_merge.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_collaboration.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using root = branch_example::document;
using traits = managed::model_traits<root>;
using entry_traits = managed::model_traits<ledger_example::entry>;
using store_type = managed::model_store<root, managed::history_mode::tree>;
using status = managed::transaction_status;

// Seed two identified map entries and one list child, independent of the generated storage profile.
root initial() {
  root value{};
  value.count = 10;
  value.entries.emplace(10, ledger_example::entry{"one", 100});
  value.entries.emplace(20, ledger_example::entry{"two", 200});
  value.rows.emplace_back(ledger_example::entry{"row", 300});
  return value;
}

// Access only the generated public payload; private fields are edited through generated editors.
const auto& payload(const auto& store) { return traits::payload(*store.read()); }

// Read a child's stable native identity without assuming direct or separate-values storage.
auto entry_id(const auto& store, std::uint64_t key = 10) {
  return payload(store).entries.at(key).persistent_id;
}

// Encode diagnostic scalars with the public codec, independently of the merge implementation.
auto field_bytes(const auto& value) {
  rohit::full_stream_auto_alloc output;
  rohit::serializer::binary_integer<rohit::serializer::serialize_type::out,
      rohit::full_stream_auto_alloc> encoder{output};
  encoder.struct_serialize_out(value);
  return std::vector<std::uint8_t>{output.begin(), output.begin() + output.current_offset()};
}

// Own one exclusively created build-tree directory and clean it even after an unexpected exception.
struct journal_directory {
  std::filesystem::path path{std::filesystem::current_path() /
      ("journal-test-" + std::to_string(managed::make_document_id().high))};
  // Exclusive creation prevents claiming a preexisting user directory as test-owned storage.
  journal_directory() {
    if (!std::filesystem::create_directory(path)) { throw std::runtime_error{"Test directory already exists"}; }
  }
  // Only the directory successfully created by this instance is removed.
  ~journal_directory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

// Author two independent branches from one retained revision, leaving the target selected.
auto branches(auto& store, auto&& source_edit, auto&& target_edit) {
  const auto base = store.current_revision();
  store.execute_transaction(std::forward<decltype(source_edit)>(source_edit)).throw_if_failed();
  const auto source = store.current_revision();
  store.checkout(base);
  store.execute_transaction(std::forward<decltype(target_edit)>(target_edit)).throw_if_failed();
  return source;
}
} // namespace

// Independent fields of one entity merge and persist deterministic first-parent Undo/Redo ancestry.
TEST(managed_merge, independent_fields_and_ancestry_round_trip) {
  store_type store{initial()};
  const auto id = entry_id(store);
  const auto pin = store.read();
  const auto source = branches(store,
      [&](auto& tx) { tx.root().entries().edit(id).set_memo("source"); },
      [&](auto& tx) { tx.root().entries().edit(id).set_amount_minor_units(500); });
  const auto target = store.current_revision();
  EXPECT_EQ(store.merge_base(source), 1u);
  const auto result = store.merge_revision(source);
  result.throw_if_failed();
  ASSERT_EQ(result.status, status::committed);
  EXPECT_EQ(store.revision_parents(result.revision), (std::vector<std::uint64_t>{target, source}));
  EXPECT_EQ(entry_traits::payload(payload(store).entries.at(10)).memo, "source");
  EXPECT_EQ(entry_traits::payload(payload(store).entries.at(10)).amount_minor_units, 500);
  EXPECT_EQ(entry_traits::payload(traits::payload(*pin).entries.at(10)).memo, "one");
  store.undo();
  EXPECT_EQ(store.current_revision(), target);
  store.redo(result.revision);
  const auto bytes = store.save();
  store_type restored{initial()};
  restored.load(bytes);
  EXPECT_EQ(restored.save(), bytes);
  EXPECT_EQ(restored.merge_base(source), source);
  EXPECT_EQ(restored.merge_revision(source).status, status::no_change);
  EXPECT_EQ(restored.save(), bytes);
  store_type transferred{initial()};
  transferred.load_delta(bytes, restored.save_delta(bytes));
  EXPECT_EQ(transferred.save(), bytes);
}

// Rejected conflicts retain exact state and can be retried with a synchronous explicit choice.
TEST(managed_merge, conflicts_retain_typed_address_and_retry_atomically) {
  store_type store{initial()};
  const auto source = branches(store,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_count(30); });
  const auto before = store.save();
  const auto failed = store.merge_revision(source);
  EXPECT_EQ(failed.status, status::failed);
  try {
    failed.throw_if_failed();
    FAIL();
  } catch (const managed::merge_conflict_error& error) {
    EXPECT_EQ(error.conflict.entity, store.read()->persistent_id);
    EXPECT_EQ(error.conflict.field, 2u);
    EXPECT_EQ(error.conflict.kind, managed::merge_conflict_kind::value);
    EXPECT_EQ(error.conflict.base, field_bytes(std::uint32_t{10}));
    EXPECT_EQ(error.conflict.target, field_bytes(std::uint32_t{30}));
    EXPECT_EQ(error.conflict.source, field_bytes(std::uint32_t{20}));
  }
  EXPECT_EQ(store.save(), before);
  managed::merge_options options;
  options.resolve = [&](const auto&) {
    EXPECT_THROW(store.checkout(1), std::logic_error);
    return managed::merge_resolution::source;
  };
  store.merge_revision(source, options).throw_if_failed();
  EXPECT_EQ(payload(store).count, 20u);
}

// Equal payload edits still record a merge decision, so ancestry survives a no-value-change merge.
TEST(managed_merge, equal_changes_coalesce_but_record_both_parents) {
  store_type store{initial()};
  const auto source = branches(store,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_count(20); });
  const auto target = store.current_revision();
  const auto result = store.merge_revision(source);
  result.throw_if_failed();
  EXPECT_EQ(result.status, status::committed);
  EXPECT_GT(result.revision, target);
  EXPECT_EQ(store.revision_parents(result.revision), (std::vector<std::uint64_t>{target, source}));
}

// Independent key insertions import source identities without reserving or recycling entity numbers.
TEST(managed_merge, identified_map_keys_and_deleted_entities) {
  store_type store{initial()};
  const auto old_id = entry_id(store);
  const auto source = branches(store,
      [&](auto& tx) { tx.root().entries().insert(30, {"new source", 600}); tx.root().entries().erase(old_id); },
      [&](auto& tx) { tx.root().entries().insert(40, {"new target", 700}); });
  const auto high_water = store.allocated_id();
  store.merge_revision(source).throw_if_failed();
  EXPECT_FALSE(payload(store).entries.contains(10));
  EXPECT_TRUE(payload(store).entries.contains(30));
  EXPECT_TRUE(payload(store).entries.contains(40));
  EXPECT_EQ(store.allocated_id(), high_water);
}

// A deleted key and an edited live entity conflict instead of silently discarding either decision.
TEST(managed_merge, delete_edit_conflict_has_presence_and_map_key) {
  store_type store{initial()};
  const auto id = entry_id(store);
  const auto source = branches(store,
      [&](auto& tx) { tx.root().entries().erase(id); },
      [&](auto& tx) { tx.root().entries().edit(id).set_memo("keep edit"); });
  const auto before = store.save();
  managed::merge_options options;
  options.resolve = [](const auto& conflict) {
    EXPECT_EQ(conflict.kind, managed::merge_conflict_kind::delete_edit);
    EXPECT_TRUE(conflict.base_present);
    EXPECT_TRUE(conflict.target_present);
    EXPECT_FALSE(conflict.source_present);
    EXPECT_EQ(conflict.map_key, field_bytes(std::uint64_t{10}));
    return managed::merge_resolution::target;
  };
  EXPECT_EQ(store.merge_revision(source).status, status::failed);
  EXPECT_EQ(store.save(), before);
  store.merge_revision(source, options).throw_if_failed();
  EXPECT_EQ(entry_traits::payload(payload(store).entries.at(10)).memo, "keep edit");
}

// Complete-candidate invariants, authorization and comparison budgets fail before publication.
TEST(managed_merge, invariant_authorization_work_and_history_failures) {
  bool reject = false;
  store_type store{initial(), managed::make_document_id(), {}, [&](const auto& value) {
    if (reject && traits::payload(value).count == 20) { throw std::invalid_argument{"combined invariant"}; }
  }};
  const auto source = branches(store,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_title("target"); });
  const auto before = store.save();
  managed::merge_options options;
  options.max_work_bytes = 1;
  EXPECT_EQ(store.merge_revision(source, options).status, status::failed);
  reject = true;
  EXPECT_EQ(store.merge_revision(source).status, status::failed);
  reject = false;
  store.set_authorization_policy([](auto, const auto&, const auto&, bool) { return false; });
  EXPECT_EQ(store.merge_revision(source).status, status::failed);
  EXPECT_EQ(store.save(), before);
  store.set_authorization_policy({});
  store.merge_revision(source).throw_if_failed();
}

// Transient caches reset only after a valid merge; resolver exceptions preserve them.
TEST(managed_merge, private_inherited_fields_lists_and_runtime_caches) {
  store_type store{initial()};
  const auto row = payload(store).rows.front().persistent_id;
  const auto source = branches(store,
      [&](auto& tx) { tx.root().set_title("source"); tx.root().rows().edit(row).set_memo("source row"); },
      [&](auto& tx) { tx.root().rows().edit(row).set_amount_minor_units(800); });
  store.update_runtime([](auto& tx) { tx.root().set_cache(42); }).throw_if_failed();
  store.merge_revision(source).throw_if_failed();
  EXPECT_EQ(payload(store).cache, 0u);
  EXPECT_EQ(entry_traits::payload(payload(store).rows.front()).memo, "source row");
  EXPECT_EQ(entry_traits::payload(payload(store).rows.front()).amount_minor_units, 800);
}

// A retained alternative merges its entity fields; replacing its tag still conflicts with live edits.
TEST(managed_merge, variant_fields_merge_without_replacing_surviving_identity) {
  store_type store{initial()};
  const auto id = std::get<0>(payload(store).content).persistent_id;
  const auto source = branches(store,
      [](auto& tx) { tx.root().template edit_content<0>().set_memo("source"); },
      [](auto& tx) { tx.root().template edit_content<0>().set_amount_minor_units(400); });
  store.merge_revision(source).throw_if_failed();
  const auto& child = std::get<0>(payload(store).content);
  EXPECT_EQ(child.persistent_id, id);
  EXPECT_EQ(entry_traits::payload(child).memo, "source");
  EXPECT_EQ(entry_traits::payload(child).amount_minor_units, 400);
  const auto switch_source = branches(store,
      [](auto& tx) { tx.root().template emplace_content<1>(branch_example::document_leaf{}); },
      [](auto& tx) { tx.root().template edit_content<0>().set_memo("target"); });
  const auto before = store.save();
  EXPECT_EQ(store.merge_revision(switch_source).status, status::failed);
  EXPECT_EQ(store.save(), before);
}

// Selective collaboration Undo/Redo retains another author's independent field in the same alternative.
TEST(managed_merge, collaboration_variant_undo_preserves_other_field_contributions) {
  managed::collaboration_authority<root> authority{initial(), 1};
  managed::collaboration_replica<root> replica;
  authority.open_session(10);
  authority.open_session(20);
  replica.synchronize(authority.snapshot(), authority.context());
  // Every transport submission authenticates the session separately and refreshes the replica.
  const auto submit = [&](const auto& proposal) {
    auto result = authority.submit_change(proposal, proposal.session, 0);
    replica.synchronize(authority.snapshot(), authority.context());
    return result;
  };
  replica.initialize_identity_lease(10, authority.reserve_ids(10));
  const auto first = submit(replica.propose(10, 1, [](auto& tx) {
    tx.root().template edit_content<0>().set_memo("first author");
  }));
  ASSERT_EQ(first->status, managed::collaboration_status::accepted);
  replica.initialize_identity_lease(20, authority.reserve_ids(20));
  const auto second = submit(replica.propose(20, 1, [](auto& tx) {
    tx.root().template edit_content<0>().set_amount_minor_units(400);
  }));
  ASSERT_EQ(second->status, managed::collaboration_status::accepted);
  const auto undone = submit(replica.undo(10, 2, 1));
  ASSERT_EQ(undone->status, managed::collaboration_status::accepted);
  const auto& value = entry_traits::payload(std::get<0>(traits::payload(*authority.read()).content));
  EXPECT_TRUE(value.memo.empty());
  EXPECT_EQ(value.amount_minor_units, 400);
  ASSERT_EQ(submit(replica.redo(10, 3, 2))->status, managed::collaboration_status::accepted);
  const auto& restored = entry_traits::payload(std::get<0>(traits::payload(*authority.read()).content));
  EXPECT_EQ(restored.memo, "first author");
  EXPECT_EQ(restored.amount_minor_units, 400);
}

// Labels are independent from the additive ancestry wrapper and remain first-parent action labels.
TEST(managed_merge, labels_and_missing_or_ambiguous_bases) {
  managed::model_store<root, managed::history_mode::tree, managed::history_labels::enabled> store{initial()};
  const auto a = branches(store,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_title("target"); });
  const auto b = store.current_revision();
  EXPECT_THROW(store.merge_base(999), std::out_of_range);
  const auto first = store.merge_revision(a, "first merge");
  first.throw_if_failed();
  EXPECT_EQ(store.undo_label(), "first merge");
  store.checkout(a);
  const auto second = store.merge_revision(b, "opposite merge");
  second.throw_if_failed();
  EXPECT_EQ(store.merge_revision(first.revision).status, status::failed);
  EXPECT_THROW(store.merge_base(first.revision), std::invalid_argument);
}

// Forged secondary parents cannot become cycles, duplicate edges or silently downgraded legacy trees.
TEST(managed_merge, malformed_ancestry_and_revision_limits_preserve_old_state) {
  store_type store{initial()};
  const auto legacy = store.save();
  EXPECT_EQ(managed::detail::decode<managed::records::unlabeled_envelope>(legacy, {}).format_version, 3u);
  const auto source = branches(store,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_title("target"); });
  store.merge_revision(source).throw_if_failed();
  const auto bytes = store.save();
  constexpr std::array<std::uint8_t, 8> magic{0xff, 'S', 'R', 'M', 'R', 'G', 1, 0};
  ASSERT_TRUE(std::equal(magic.begin(), magic.end(), bytes.begin()));
  const auto original = managed::detail::decode<managed::records::merge_checkpoint>(
      std::span{bytes}.subspan(magic.size()), {});
  for (unsigned attack = 0; attack != 7; ++attack) {
    auto record = original;
    if (attack == 0) { record.parents.front().source = record.parents.front().number; }
    if (attack == 1) { record.parents.front().source = 999; }
    if (attack == 2) { record.parents.front().number = 999; }
    if (attack == 3) { record.parents.push_back(record.parents.front()); }
    if (attack == 4) { record.parents.front().source = 0; }
    if (attack == 5) { record.parents.clear(); }
    if (attack == 6) { record.format_version = 2; }
    auto forged = managed::detail::encode(record);
    forged.insert(forged.begin(), magic.begin(), magic.end());
    EXPECT_THROW(store.load(forged), std::invalid_argument);
    EXPECT_EQ(store.save(), bytes);
  }
  EXPECT_ANY_THROW((managed::detail::decode<managed::records::unlabeled_envelope>(bytes, {})));
  managed::model_store<root> linear{initial()};
  EXPECT_THROW(linear.load(bytes), std::invalid_argument);
  managed::store_options options;
  options.max_revisions = 3;
  store_type bounded{initial(), managed::make_document_id(), options};
  const auto bounded_source = branches(bounded,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_title("target"); });
  const auto before = bounded.save();
  const auto failed = bounded.merge_revision(bounded_source);
  EXPECT_THROW(failed.throw_if_failed(), std::length_error);
  EXPECT_EQ(bounded.save(), before);
}

// Every existing durable-write fault selects a complete old or new graph and allows fresh recovery.
TEST(managed_merge, interrupted_merge_flush_recovers_one_complete_decision) {
  using event = managed::journal_io_event;
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    for (const auto point : {event::before_append, event::after_header, event::after_payload,
                             event::after_commit, event::after_flush}) {
      const journal_directory directory;
      const auto path = directory.path / "document";
      bool armed{};
      managed::journal_options fault;
      fault.fault_injector = [&](auto actual) {
        if (armed && actual == point) { throw std::runtime_error{"merge interruption"}; }
      };
      std::uint64_t source{}, target{}, sequence{};
      {
        store_type store{initial()};
        store.create_journal(path, mode, fault);
        source = branches(store,
            [](auto& tx) { tx.root().set_count(20); },
            [](auto& tx) { tx.root().set_title("target"); });
        target = store.current_revision();
        sequence = store.journal_sequence();
        const auto pin = store.read();
        armed = true;
        const auto result = store.merge_revision(source);
        EXPECT_EQ(result.status, point == event::before_append ? status::failed : status::indeterminate);
        EXPECT_EQ(store.read(), pin);
        EXPECT_EQ(store.journal_sequence(), sequence);
        EXPECT_EQ(store.journal_needs_recovery(), point != event::before_append);
      }
      armed = false;
      store_type restored{initial()};
      restored.recover_journal(path);
      const bool committed = point == event::after_commit || point == event::after_flush;
      EXPECT_EQ(restored.current_revision(), committed ? target + 1 : target);
      EXPECT_EQ(restored.journal_sequence(), sequence + (committed ? 1 : 0));
      EXPECT_EQ(payload(restored).count, committed ? 20u : 10u);
      if (committed) {
        EXPECT_EQ(restored.revision_parents(restored.current_revision()),
                  (std::vector<std::uint64_t>{target, source}));
      } else { restored.merge_revision(source).throw_if_failed(); }
      restored.execute_transaction([](auto& tx) { tx.root().set_count(40); }).throw_if_failed();
    }
  }
}

// Journal replay reconstructs accepted merge bytes and ancestry without invoking a conflict resolver.
TEST(managed_merge, appended_and_sidecar_journal_recovery) {
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    for (const bool delta : {false, true}) {
      const journal_directory directory;
      const auto path = directory.path / "document";
      managed::store_options options;
      options.delta_journal = delta;
      std::vector<std::uint8_t> saved;
      std::uint64_t merged{};
      {
        store_type store{initial(), managed::make_document_id(), options};
        store.create_journal(path, mode);
        const auto source = branches(store,
            [](auto& tx) { tx.root().set_count(20); },
            [](auto& tx) { tx.root().set_title("target"); });
        auto result = store.merge_revision(source);
        result.throw_if_failed();
        merged = result.revision;
        saved = store.save();
      }
      {
        store_type restored{initial(), managed::make_document_id(), options};
        restored.recover_journal(path);
        EXPECT_EQ(restored.save(), saved);
        EXPECT_EQ(restored.revision_parents(merged).size(), 2u);
        restored.save_journal();
        restored.undo();
        restored.redo(merged);
      }
      {
        store_type restored{initial(), managed::make_document_id(), options};
        restored.recover_journal(path);
        EXPECT_EQ(restored.save(), saved);
      }
    }
  }
}
