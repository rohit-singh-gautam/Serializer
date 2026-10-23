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
#include <variant>
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

// Divergent edits report the surviving child's field and resolve without replacing its identity.
TEST(managed_merge, variant_field_conflicts_preserve_identity_and_retry_atomically) {
  store_type store{initial()};
  const auto child_id = std::get<0>(payload(store).content).persistent_id;
  const auto source = branches(store,
      [](auto& tx) { tx.root().template edit_content<0>().set_memo("source"); },
      [](auto& tx) { tx.root().template edit_content<0>().set_memo("target"); });
  const auto before = store.save();
  const auto high_water = store.allocated_id();
  const auto failed = store.merge_revision(source);
  ASSERT_EQ(failed.status, status::failed);
  try {
    failed.throw_if_failed();
    FAIL();
  } catch (const managed::merge_conflict_error& error) {
    EXPECT_EQ(error.conflict.entity, child_id);
    EXPECT_EQ(error.conflict.field, 1u);
    EXPECT_EQ(error.conflict.kind, managed::merge_conflict_kind::value);
    EXPECT_EQ(error.conflict.base, field_bytes(std::string{}));
    EXPECT_EQ(error.conflict.target, field_bytes(std::string{"target"}));
    EXPECT_EQ(error.conflict.source, field_bytes(std::string{"source"}));
  }
  EXPECT_EQ(store.save(), before);
  managed::merge_options options;
  unsigned calls{};
  options.resolve = [&](const auto& conflict) {
    ++calls;
    EXPECT_EQ(conflict.entity, child_id);
    EXPECT_EQ(conflict.field, 1u);
    return managed::merge_resolution::source;
  };
  store.merge_revision(source, options).throw_if_failed();
  EXPECT_EQ(calls, 1u);
  const auto& child = std::get<0>(payload(store).content);
  EXPECT_EQ(child.persistent_id, child_id);
  EXPECT_EQ(entry_traits::payload(child).memo, "source");
  EXPECT_EQ(store.allocated_id(), high_water);
}

// A new entity in the same variant tag is an atomic replacement, even when its fields could merge.
TEST(managed_merge, variant_same_tag_replacement_never_merges_distinct_entities) {
  store_type store{initial()};
  const auto original = store.read();
  const auto old_id = std::get<0>(traits::payload(*original).content).persistent_id;
  const auto source = branches(store,
      [](auto& tx) { tx.root().template emplace_content<0>(ledger_example::entry{"replacement", 700}); },
      [](auto& tx) { tx.root().template edit_content<0>().set_amount_minor_units(400); });
  const auto before = store.save();
  EXPECT_EQ(store.merge_revision(source).status, status::failed);
  EXPECT_EQ(store.save(), before);
  const auto high_water = store.allocated_id();
  managed::merge_options options;
  unsigned calls{};
  options.resolve = [&](const auto& conflict) {
    ++calls;
    EXPECT_EQ(conflict.entity, original->persistent_id);
    EXPECT_EQ(conflict.field, 5u);
    EXPECT_EQ(conflict.kind, managed::merge_conflict_kind::value);
    return managed::merge_resolution::source;
  };
  store.merge_revision(source, options).throw_if_failed();
  EXPECT_EQ(calls, 1u);
  const auto& child = std::get<0>(payload(store).content);
  EXPECT_NE(child.persistent_id, old_id);
  EXPECT_EQ(entry_traits::payload(child).memo, "replacement");
  EXPECT_EQ(entry_traits::payload(child).amount_minor_units, 700);
  EXPECT_EQ(std::get<0>(traits::payload(*original).content).persistent_id, old_id);
  EXPECT_EQ(store.allocated_id(), high_water);
}

// Accepted field merges retain their variant identity and both parents through every journal profile.
TEST(managed_merge, variant_field_merge_journal_recovery_preserves_identity_and_ancestry) {
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    for (const bool delta : {false, true}) {
      const journal_directory directory;
      const auto path = directory.path / "document";
      managed::store_options options;
      options.delta_journal = delta;
      std::vector<std::uint8_t> saved;
      std::uint64_t child_id{}, source{}, target{}, merged{};
      {
        store_type store{initial(), managed::make_document_id(), options};
        store.create_journal(path, mode);
        child_id = std::get<0>(payload(store).content).persistent_id;
        source = branches(store,
            [](auto& tx) { tx.root().template edit_content<0>().set_memo("source"); },
            [](auto& tx) { tx.root().template edit_content<0>().set_amount_minor_units(400); });
        target = store.current_revision();
        const auto result = store.merge_revision(source);
        result.throw_if_failed();
        merged = result.revision;
        saved = store.save();
      }
      {
        store_type restored{initial(), managed::make_document_id(), options};
        restored.recover_journal(path);
        EXPECT_EQ(restored.save(), saved);
        EXPECT_EQ(restored.revision_parents(merged), (std::vector<std::uint64_t>{target, source}));
        const auto& child = std::get<0>(payload(restored).content);
        EXPECT_EQ(child.persistent_id, child_id);
        EXPECT_EQ(entry_traits::payload(child).memo, "source");
        EXPECT_EQ(entry_traits::payload(child).amount_minor_units, 400);
        restored.undo();
        EXPECT_EQ(restored.current_revision(), target);
        EXPECT_EQ(std::get<0>(payload(restored).content).persistent_id, child_id);
        EXPECT_TRUE(entry_traits::payload(std::get<0>(payload(restored).content)).memo.empty());
        restored.redo(merged);
        restored.save_journal();
      }
      store_type compacted{initial(), managed::make_document_id(), options};
      compacted.recover_journal(path);
      EXPECT_EQ(compacted.save(), saved);
      EXPECT_EQ(std::get<0>(payload(compacted).content).persistent_id, child_id);
    }
  }
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

// Selective Undo cannot erase a later same-field contribution; ordered author Undo/Redo remains valid.
TEST(managed_merge, collaboration_variant_undo_conflict_preserves_later_author_and_retries) {
  managed::collaboration_authority<root> authority{initial(), 1};
  managed::collaboration_replica<root> replica;
  authority.open_session(10);
  authority.open_session(20);
  replica.synchronize(authority.snapshot(), authority.context());
  const auto child_id = std::get<0>(traits::payload(*authority.read()).content).persistent_id;
  // Authenticate each proposal and refresh the client's exact accepted base after every decision.
  const auto submit = [&](const auto& proposal) {
    auto result = authority.submit_change(proposal, proposal.session, 0);
    replica.synchronize(authority.snapshot(), authority.context());
    return result;
  };
  replica.initialize_identity_lease(10, authority.reserve_ids(10));
  ASSERT_EQ(submit(replica.propose(10, 1, [](auto& tx) {
    tx.root().template edit_content<0>().set_memo("first author");
  }))->status, managed::collaboration_status::accepted);
  replica.initialize_identity_lease(20, authority.reserve_ids(20));
  ASSERT_EQ(submit(replica.propose(20, 1, [](auto& tx) {
    tx.root().template edit_content<0>().set_memo("second author");
  }))->status, managed::collaboration_status::accepted);
  const auto sequence = authority.sequence();
  const auto request = replica.undo(10, 2, 1);
  const auto rejected = submit(request);
  ASSERT_EQ(rejected->status, managed::collaboration_status::conflict);
  EXPECT_EQ(authority.sequence(), sequence);
  EXPECT_EQ(submit(request), rejected);
  EXPECT_EQ(entry_traits::payload(std::get<0>(payload(authority).content)).memo, "second author");
  ASSERT_EQ(submit(replica.undo(20, 2, 1))->status, managed::collaboration_status::accepted);
  EXPECT_EQ(entry_traits::payload(std::get<0>(payload(authority).content)).memo, "first author");
  ASSERT_EQ(submit(replica.undo(10, 3, 1))->status, managed::collaboration_status::accepted);
  EXPECT_TRUE(entry_traits::payload(std::get<0>(payload(authority).content)).memo.empty());
  ASSERT_EQ(submit(replica.redo(10, 4, 3))->status, managed::collaboration_status::accepted);
  EXPECT_EQ(entry_traits::payload(std::get<0>(payload(authority).content)).memo, "first author");
  ASSERT_EQ(submit(replica.redo(20, 3, 2))->status, managed::collaboration_status::accepted);
  EXPECT_EQ(entry_traits::payload(std::get<0>(payload(authority).content)).memo, "second author");
  EXPECT_EQ(std::get<0>(payload(authority).content).persistent_id, child_id);
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

// A merge contributes accepted field deltas, native ancestry and selective session Undo/Redo.
TEST(managed_merge, collaboration_merge_replication_retry_and_selective_history) {
  using authority_type = managed::collaboration_authority<root, managed::history_mode::tree>;
  store_type seed{initial()};
  const auto source = branches(seed,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_title("target"); });
  const auto target = seed.current_revision();
  managed::collaboration_options options;
  options.allow_branch_merges = true;
  authority_type authority{initial(), 1, seed.document(), options};
  authority.load_document(seed.save());
  authority.open_session(10);
  authority.open_session(20);
  ASSERT_TRUE(authority.branch_merges_enabled());
  managed::collaboration_replica<root> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  auto request = replica.merge_request(10, 1, source, authority.current_revision());
  request = managed::decode_collaboration_record<decltype(request)>(
      managed::encode_collaboration_record(request));
  const auto accepted = authority.submit_change(request, 10, 0);
  ASSERT_EQ(accepted->status, managed::collaboration_status::accepted);
  EXPECT_EQ(authority.submit_change(request, 10, 1), accepted);
  ASSERT_EQ(accepted->accepted->history.fields.size(), 1u);
  EXPECT_EQ(accepted->accepted->history.fields.front().field, 2u);
  EXPECT_EQ(replica.apply_accepted_change(*accepted->accepted, authority.context()),
      managed::replication_status::applied);
  EXPECT_EQ(payload(replica).count, 20u);
  store_type restored{initial()};
  restored.load(authority.save_document());
  EXPECT_EQ(restored.revision_parents(restored.current_revision()),
      (std::vector<std::uint64_t>{target, source}));
  const auto edit = replica.propose(20, 1, [](auto& tx) { tx.root().set_title("later"); });
  const auto changed = authority.submit_change(edit, 20, 1);
  ASSERT_EQ(changed->status, managed::collaboration_status::accepted);
  replica.apply_accepted_change(*changed->accepted, authority.context());
  const auto undo = authority.submit_change(replica.undo(10, 2), 10, 2);
  ASSERT_EQ(undo->status, managed::collaboration_status::accepted);
  EXPECT_EQ(payload(authority).count, 10u);
  EXPECT_NE(rohit::serializer::serialize_as(*authority.read()).find("\"title\":\"later\""), std::string::npos);
  replica.apply_accepted_change(*undo->accepted, authority.context());
  const auto redo = authority.submit_change(replica.redo(10, 3), 10, 3);
  ASSERT_EQ(redo->status, managed::collaboration_status::accepted);
  EXPECT_EQ(payload(authority).count, 20u);
  EXPECT_NE(rohit::serializer::serialize_as(*authority.read()).find("\"title\":\"later\""), std::string::npos);
  replica.apply_accepted_change(*redo->accepted, authority.context());
  const auto noop = replica.merge_request(10, 4, source, authority.current_revision());
  EXPECT_EQ(authority.submit_change(noop, 10, 4)->status, managed::collaboration_status::no_change);
  auto reused = request;
  reused.base_sequence = 1;
  EXPECT_THROW(authority.submit_change(reused, 10, 4), std::invalid_argument);
}

// Capability, exact target/cursor, session binding and malformed requests cannot publish ancestry.
TEST(managed_merge, collaboration_merge_capability_and_request_fences) {
  using authority_type = managed::collaboration_authority<root, managed::history_mode::tree>;
  store_type seed{initial()};
  const auto source = branches(seed,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_title("target"); });
  for (const bool enabled : {false, true}) {
    managed::collaboration_options options;
    options.allow_branch_merges = enabled;
    authority_type authority{initial(), 1, seed.document(), options};
    authority.load_document(seed.save());
    authority.open_session(10);
    authority.open_session(20, false);
    managed::collaboration_replica<root> replica;
    replica.synchronize(authority.snapshot(), authority.context());
    const auto before = authority.save_document();
    const auto target = authority.current_revision();
    EXPECT_EQ(authority.branch_merges_enabled(), enabled);
    const auto request = replica.merge_request(10, 1, source, target);
    EXPECT_THROW(authority.submit_change(request, 20, 0), std::invalid_argument);
    EXPECT_EQ(authority.submit_change(replica.merge_request(20, 1, source, target), 20, 0)->status,
        managed::collaboration_status::denied);
    const auto stale = replica.merge_request(10, 2, source, target - 1);
    EXPECT_EQ(authority.submit_change(stale, 10, 0)->status, enabled
        ? managed::collaboration_status::conflict : managed::collaboration_status::denied);
    auto malformed = replica.merge_request(10, 3, source, target);
    auto change = managed::decode_collaboration_record<managed::collaboration::model_change>(malformed.command);
    change.allocated_id = 1;
    malformed.command = managed::encode_collaboration_record(change);
    EXPECT_EQ(authority.submit_change(malformed, 10, 0)->status, enabled
        ? managed::collaboration_status::failed : managed::collaboration_status::denied);
    auto ahead = replica.merge_request(10, 4, source, target);
    ++ahead.base_sequence;
    EXPECT_EQ(authority.submit_change(ahead, 10, 0)->status, managed::collaboration_status::conflict);
    EXPECT_EQ(authority.save_document(), before);
    const auto result = authority.submit_change(request, 10, 0);
    EXPECT_EQ(result->status, enabled
        ? managed::collaboration_status::accepted : managed::collaboration_status::denied);
  }
}

// Conflict, current policy, work admission and lock ownership retain the complete old decision.
TEST(managed_merge, collaboration_merge_conflict_policy_quota_and_locks) {
  using authority_type = managed::collaboration_authority<root, managed::history_mode::tree>;
  for (unsigned failure{}; failure < 4; ++failure) {
    store_type seed{initial()};
    const auto source = branches(seed,
        [](auto& tx) { tx.root().set_count(20); },
        [failure](auto& tx) {
          if (failure == 0) { tx.root().set_count(30); }
          else { tx.root().set_title("target"); }
        });
    managed::collaboration_options options;
    options.allow_branch_merges = true;
    if (failure == 2) { options.max_merge_work_bytes = 1; }
    if (failure == 3) { options.require_edit_lock = true; }
    authority_type authority{initial(), 1, seed.document(), options, {},
        [failure](const auto&, const auto&, const auto&, const auto&) { return failure != 1; }};
    authority.load_document(seed.save());
    authority.open_session(10);
    managed::collaboration_replica<root> replica;
    replica.synchronize(authority.snapshot(), authority.context());
    const auto before = authority.save_document();
    const auto request = replica.merge_request(10, 1, source, authority.current_revision());
    const auto result = authority.submit_change(request, 10, 0);
    const auto expected = failure == 0 ? managed::collaboration_status::conflict :
        failure == 2 ? managed::collaboration_status::failed : managed::collaboration_status::denied;
    EXPECT_EQ(result->status, expected);
    EXPECT_EQ(authority.submit_change(request, 10, 1), result);
    EXPECT_EQ(authority.save_document(), before);
    EXPECT_EQ(authority.snapshot().sequence, 0u);
  }
}

// Resolution is a fresh host-authorized decision; exact conflict retries retain their old outcome.
TEST(managed_merge, collaboration_merge_resolution_and_ancestry_only_acceptance) {
  using authority_type = managed::collaboration_authority<root, managed::history_mode::tree>;
  store_type seed{initial()};
  const auto source = branches(seed,
      [](auto& tx) { tx.root().set_count(20); },
      [](auto& tx) { tx.root().set_count(30); });
  managed::collaboration_options options;
  options.allow_branch_merges = true;
  authority_type authority{initial(), 1, seed.document(), options};
  authority.load_document(seed.save());
  authority.open_session(10);
  managed::collaboration_replica<root> replica;
  replica.synchronize(authority.snapshot(), authority.context());
  const auto rejected = replica.merge_request(10, 1, source, authority.current_revision());
  const auto conflict = authority.submit_change(rejected, 10, 0);
  ASSERT_EQ(conflict->status, managed::collaboration_status::conflict);
  unsigned calls{};
  authority.set_branch_merge_resolver([&](const auto& value) {
    ++calls;
    EXPECT_EQ(value.field, 2u);
    EXPECT_THROW(authority.snapshot(), std::logic_error);
    return managed::merge_resolution::target;
  });
  EXPECT_EQ(authority.submit_change(rejected, 10, 0), conflict);
  EXPECT_EQ(calls, 0u);
  const auto before_revision = authority.current_revision();
  const auto request = replica.merge_request(10, 2, source, before_revision);
  const auto result = authority.submit_change(request, 10, 0);
  ASSERT_EQ(result->status, managed::collaboration_status::accepted);
  EXPECT_EQ(calls, 1u);
  EXPECT_EQ(payload(authority).count, 30u);
  EXPECT_TRUE(result->accepted->history.fields.empty());
  EXPECT_GT(authority.current_revision(), before_revision);
  EXPECT_EQ(authority.submit_change(request, 10, 0), result);
  EXPECT_EQ(calls, 1u);
  EXPECT_EQ(replica.apply_accepted_change(*result->accepted, authority.context()),
      managed::replication_status::applied);
}

// Durable merge faults fence the authority exactly as edits do and recover both native parents.
TEST(managed_merge, collaboration_merge_journal_fault_recovery) {
  using authority_type = managed::collaboration_authority<root, managed::history_mode::tree>;
  using event = managed::journal_io_event;
  for (const auto mode : {managed::journal_storage_mode::appended, managed::journal_storage_mode::sidecar}) {
    for (const auto point : {event::before_append, event::after_header, event::after_payload,
                             event::after_commit, event::after_flush}) {
      const journal_directory directory;
      const auto path = directory.path / "document";
      store_type seed{initial()};
      const auto source = branches(seed,
          [](auto& tx) { tx.root().set_count(20); },
          [](auto& tx) { tx.root().set_title("target"); });
      const auto target = seed.current_revision();
      managed::collaboration_options options;
      options.allow_branch_merges = true;
      bool armed{};
      managed::journal_options fault;
      fault.fault_injector = [&](auto actual) {
        if (armed && actual == point) { throw std::runtime_error{"collaborative merge interruption"}; }
      };
      {
        authority_type authority{initial(), 1, seed.document(), options};
        authority.load_document(seed.save());
        authority.create_journal(path, mode, fault);
        authority.open_session(10);
        managed::collaboration_replica<root> replica;
        replica.synchronize(authority.snapshot(), authority.context());
        const auto request = replica.merge_request(10, 1, source, target);
        armed = true;
        const auto result = authority.submit_change(request, 10, 0);
        EXPECT_EQ(result->status, point == event::before_append
            ? managed::collaboration_status::failed : managed::collaboration_status::indeterminate);
        EXPECT_FALSE(result->accepted);
        if (point != event::before_append) {
          EXPECT_THROW(authority.snapshot(), managed::journal_indeterminate_error);
        }
      }
      armed = false;
      authority_type recovered{initial(), 2, seed.document(), options};
      recovered.recover_journal(path);
      const bool committed = point == event::after_commit || point == event::after_flush;
      EXPECT_EQ(payload(recovered).count, committed ? 20u : 10u);
      store_type restored{initial()};
      restored.load(recovered.save_document());
      if (committed) {
        EXPECT_EQ(restored.revision_parents(restored.current_revision()),
            (std::vector<std::uint64_t>{target, source}));
      }
      recovered.open_session(10);
      managed::collaboration_replica<root> replica;
      replica.synchronize(recovered.snapshot(), recovered.context());
      const auto request = replica.merge_request(10, 1, source, recovered.current_revision());
      EXPECT_EQ(recovered.submit_change(request, 10, 0)->status, committed
          ? managed::collaboration_status::no_change : managed::collaboration_status::accepted);
    }
  }
}
