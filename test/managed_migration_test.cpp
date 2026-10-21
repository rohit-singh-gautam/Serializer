// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <managed_migration.hpp>
#include <rohit/managed_collaboration.hpp>
#include <rohit/managed_migration.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
namespace managed = rohit::managed;
namespace migration = managed::migration;
using root = migration_example::document;
using traits = managed::model_traits<root>;
using storage = traits::storage_type;
using id_type = traits::id_type;

struct legacy_root;
struct legacy_storage;
struct legacy_traits {
  using storage_type = legacy_storage;
  using id_type = std::uint32_t;
  static constexpr std::string_view schema_id{"legacy-generated-model"};
};

// Build an owned graph with ordinary integers independent of its persistent identity encoding.
root initial() {
  root result{};
  result.entries.resize(2U);
  result.entries[0].amount = 0x0102030405060708ULL;
  result.entries[1].amount = std::numeric_limits<std::uint64_t>::max();
  result.ordinary = std::numeric_limits<std::uint64_t>::max();
  return result;
}

// Create a typed same-document edge only after native IDs have been allocated.
void link(auto& store) {
  const auto id = traits::payload(*store.read()).entries[1].persistent_id;
  const auto reference = store.registry().template reference<migration_example::entry>(id);
  store.execute_transaction([&](auto& edit) {
    edit.update([&](auto& value) { traits::payload(value).selected = reference; });
  }).throw_if_failed();
}

// Verify every retained state by returning the fixed result to the current native policy and loading it.
template <managed::history_mode Mode, managed::history_labels Labels>
void check_saved_history() {
  using store_type = managed::model_store<root, Mode, Labels>;
  store_type store{initial(), {41U, 42U}};
  link(store);
  store.execute_transaction([](auto& edit) {
    edit.update([](auto& value) { traits::payload(value).ordinary = 900U; });
  }).throw_if_failed();
  if constexpr (Mode != managed::history_mode::disabled) { store.undo(); }
  const auto original = store.save();
  migration::options reverse;
  reverse.source_encoding = managed::identity_encoding::compact_varint;
  reverse.target_encoding = managed::identity_encoding::fixed;
  const auto fixed = migration::migrate_save<root>(original, reverse);
  const auto converted = migration::migrate_save<root>(fixed);
  EXPECT_EQ(converted, original);
  store_type restored{initial()};
  restored.load(converted);
  EXPECT_EQ(restored.document(), store.document());
  EXPECT_EQ(restored.allocated_id(), store.allocated_id());
  EXPECT_EQ(traits::payload(*restored.read()).selected, traits::payload(*store.read()).selected);
  EXPECT_EQ(traits::payload(*restored.read()).ordinary, traits::payload(*store.read()).ordinary);
  if constexpr (Mode == managed::history_mode::linear) {
    EXPECT_EQ(restored.history_cursor(), store.history_cursor());
    restored.redo();
    EXPECT_EQ(traits::payload(*restored.read()).ordinary, 900U);
  }
}

// Preserve current values, identity and retained future across one explicit history-policy conversion.
template <managed::history_mode SourceMode, managed::history_labels SourceLabels,
          managed::history_mode TargetMode, managed::history_labels TargetLabels>
void check_history_policy_conversion() {
  using source_store = managed::model_store<root, SourceMode, SourceLabels>;
  using target_store = managed::model_store<root, TargetMode, TargetLabels>;
  source_store source{initial(), {71U, 72U}};
  link(source);
  const auto mutate = [&](std::uint64_t value) {
    const auto apply = [value](auto& edit) {
      edit.update([value](auto& candidate) { traits::payload(candidate).ordinary = value; });
    };
    if constexpr (SourceLabels == managed::history_labels::enabled) {
      source.execute_transaction("Converted action", apply).throw_if_failed();
    } else { source.execute_transaction(apply).throw_if_failed(); }
  };
  mutate(100U);
  mutate(200U);
  if constexpr (SourceMode != managed::history_mode::disabled) { source.undo(); }
  const auto original = source.save();
  migration::history_policy_options policy;
  policy.source_mode = SourceMode;
  policy.source_labels = SourceLabels;
  policy.target_mode = TargetMode;
  policy.target_labels = TargetLabels;
  if constexpr (TargetMode == managed::history_mode::disabled) {
    policy.loss = migration::history_loss_policy::discard_unselected;
  }
  const auto converted = migration::convert_history_policy<root>(original, policy);
  EXPECT_EQ(source.save(), original);
  target_store restored{initial()};
  restored.load(converted);
  EXPECT_EQ(restored.document(), source.document());
  EXPECT_EQ(restored.allocated_id(), source.allocated_id());
  EXPECT_EQ(restored.read()->persistent_id, source.read()->persistent_id);
  EXPECT_EQ(traits::payload(*restored.read()).selected, traits::payload(*source.read()).selected);
  EXPECT_EQ(traits::payload(*restored.read()).ordinary, traits::payload(*source.read()).ordinary);
  if constexpr (SourceMode != managed::history_mode::disabled && TargetMode != managed::history_mode::disabled) {
    if constexpr (TargetMode == managed::history_mode::tree) {
      const auto children = restored.redo_children();
      ASSERT_EQ(children.size(), 1U);
      restored.redo(children.front());
    } else { restored.redo(); }
    EXPECT_EQ(traits::payload(*restored.read()).ordinary, 200U);
    restored.undo();
    EXPECT_EQ(traits::payload(*restored.read()).ordinary, 100U);
    if constexpr (TargetLabels == managed::history_labels::enabled) {
      const auto expected = SourceLabels == managed::history_labels::enabled ? "Converted action" : "";
      if constexpr (TargetMode == managed::history_mode::tree) {
        EXPECT_EQ(restored.redo_label(restored.redo_children().front()), expected);
      } else { EXPECT_EQ(restored.redo_label(), expected); }
    }
  }
}

// Exercise every supported destination without silently omitting a mode/label combination.
template <managed::history_mode SourceMode, managed::history_labels SourceLabels>
void check_all_history_policy_targets() {
  check_history_policy_conversion<SourceMode, SourceLabels, managed::history_mode::disabled,
      managed::history_labels::disabled>();
  check_history_policy_conversion<SourceMode, SourceLabels, managed::history_mode::linear,
      managed::history_labels::disabled>();
  check_history_policy_conversion<SourceMode, SourceLabels, managed::history_mode::linear,
      managed::history_labels::enabled>();
  check_history_policy_conversion<SourceMode, SourceLabels, managed::history_mode::tree,
      managed::history_labels::disabled>();
  check_history_policy_conversion<SourceMode, SourceLabels, managed::history_mode::tree,
      managed::history_labels::enabled>();
}
} // namespace

// All five native history/label profiles convert in every direction while preserving redo and identity.
TEST(managed_migration, complete_native_history_policy_matrix) {
  check_all_history_policy_targets<managed::history_mode::disabled, managed::history_labels::disabled>();
  check_all_history_policy_targets<managed::history_mode::linear, managed::history_labels::disabled>();
  check_all_history_policy_targets<managed::history_mode::linear, managed::history_labels::enabled>();
  check_all_history_policy_targets<managed::history_mode::tree, managed::history_labels::disabled>();
  check_all_history_policy_targets<managed::history_mode::tree, managed::history_labels::enabled>();
}

// Branch loss, path choice and disabled history are explicit decisions rather than compiler guesses.
TEST(managed_migration, history_policy_conversion_requires_explicit_loss_and_path) {
  using source_store = managed::model_store<root, managed::history_mode::tree>;
  source_store source{initial(), {71U, 72U}};
  source.execute_transaction([](auto& edit) {
    edit.update([](auto& value) { traits::payload(value).ordinary = 100; });
  }).throw_if_failed();
  const auto first = source.current_revision();
  source.execute_transaction([](auto& edit) {
    edit.update([](auto& value) { traits::payload(value).ordinary = 200; });
  }).throw_if_failed();
  const auto old_branch = source.current_revision();
  source.undo();
  source.execute_transaction([](auto& edit) {
    edit.update([](auto& value) { traits::payload(value).ordinary = 300; });
  }).throw_if_failed();
  const auto new_branch = source.current_revision();
  const auto bytes = source.save();
  migration::history_policy_options policy;
  policy.source_mode = managed::history_mode::tree;
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy), std::invalid_argument);
  policy.retained_tree_path = {1, first, new_branch};
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy), std::invalid_argument);
  policy.loss = migration::history_loss_policy::discard_unselected;
  const auto converted = migration::convert_history_policy<root>(bytes, policy);
  managed::model_store<root> restored{initial()};
  restored.load(converted);
  EXPECT_EQ(traits::payload(*restored.read()).ordinary, 300U);
  restored.undo();
  EXPECT_EQ(traits::payload(*restored.read()).ordinary, 100U);
  restored.redo();
  EXPECT_EQ(traits::payload(*restored.read()).ordinary, 300U);
  policy.retained_tree_path = {1, first, old_branch};
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy), std::invalid_argument);
  policy.retained_tree_path = {1, new_branch};
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy), std::invalid_argument);
  policy.retained_tree_path.clear();
  policy.target_mode = managed::history_mode::disabled;
  policy.loss = migration::history_loss_policy::reject;
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy), std::invalid_argument);
  policy.loss = migration::history_loss_policy::discard_unselected;
  managed::model_store<root, managed::history_mode::disabled> baseline{initial()};
  baseline.load(migration::convert_history_policy<root>(bytes, policy));
  EXPECT_EQ(traits::payload(*baseline.read()).ordinary, 300U);
  EXPECT_EQ(source.save(), bytes);
}

// Validate the entire source before dropping history and bound output before a caller can publish it.
TEST(managed_migration, history_policy_conversion_rejects_corruption_and_resource_failures) {
  using source_store = managed::model_store<root, managed::history_mode::tree>;
  source_store source{initial(), {71U, 72U}};
  source.execute_transaction([](auto& edit) {
    edit.update([](auto& value) { traits::payload(value).ordinary = 500; });
  }).throw_if_failed();
  auto bytes = source.save();
  migration::history_policy_options policy;
  policy.source_mode = managed::history_mode::tree;
  policy.target_mode = managed::history_mode::disabled;
  policy.loss = migration::history_loss_policy::discard_unselected;
  auto document = managed::detail::decode<migration::detail::saved_document>(bytes, {});
  document.revisions.front().snapshot.push_back(0);
  const auto corrupt = migration::detail::encode_wire<migration::detail::saved_document,
      managed::identity_encoding::fixed, rohit::serializer::serialize_key_type::integer>(document, 65536U);
  EXPECT_THROW(migration::convert_history_policy<root>(corrupt, policy), std::exception);
  migration::options bounded;
  bounded.max_output_bytes = 1;
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy, bounded), std::exception);
  bounded = {};
  bounded.max_working_bytes = bytes.size();
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy, bounded), std::length_error);
  policy.source_mode = managed::history_mode::linear;
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy), std::invalid_argument);
  policy.source_mode = managed::history_mode::tree;
  policy.target_mode = managed::history_mode::disabled;
  policy.target_labels = managed::history_labels::enabled;
  EXPECT_THROW(migration::convert_history_policy<root>(bytes, policy), std::invalid_argument);
  EXPECT_EQ(source.save(), bytes);
}

// Changing history representation never loses authority allocation ranges or their committed cursors.
TEST(managed_migration, history_policy_conversion_preserves_native_lease_checkpoint) {
  managed::collaboration_authority<root> authority{initial(), 1, {71U, 72U}};
  authority.open_session(10);
  static_cast<void>(authority.reserve_ids(10, 12U));
  const auto bytes = authority.save_document();
  migration::history_policy_options policy;
  policy.target_mode = managed::history_mode::tree;
  policy.target_labels = managed::history_labels::enabled;
  const auto converted = migration::convert_history_policy<root>(bytes, policy);
  const auto decode_checkpoint = [](const auto& input) {
    return managed::detail::decode<managed::records::identity_allocation_checkpoint>(
        std::span<const std::uint8_t>{input}.subspan(migration::detail::allocation_magic.size()), {});
  };
  const auto before = decode_checkpoint(bytes);
  const auto after = decode_checkpoint(converted);
  ASSERT_EQ(before.leases.size(), after.leases.size());
  for (std::size_t index = 0; index < before.leases.size(); ++index) {
    EXPECT_EQ(before.leases[index].session_format, after.leases[index].session_format);
    EXPECT_EQ(before.leases[index].session, after.leases[index].session);
    EXPECT_EQ(before.leases[index].first, after.leases[index].first);
    EXPECT_EQ(before.leases[index].last, after.leases[index].last);
    EXPECT_EQ(before.leases[index].committed, after.leases[index].committed);
  }
  policy.source_mode = managed::history_mode::tree;
  policy.source_labels = managed::history_labels::enabled;
  policy.target_mode = managed::history_mode::linear;
  policy.target_labels = managed::history_labels::disabled;
  EXPECT_EQ(migration::convert_history_policy<root>(converted, policy), bytes);
}

// Identity overrides propagate through owned collections and typed tokens for all three binary codecs.
TEST(managed_migration, raw_nested_values_and_independent_binary_formats) {
  managed::model_store<root> store{initial(), {41U, 42U}};
  link(store);
  for (const auto source : {migration::payload_codec::binary_none, migration::payload_codec::binary_integer,
                            migration::payload_codec::binary_string}) {
    const auto fixed = migration::detail::encode_value(*store.read(), managed::identity_encoding::fixed, source, 65536U);
    using fixed_policy = migration::detail::identity_policy<managed::identity_encoding::fixed>;
    if (source == migration::payload_codec::binary_integer) {
      EXPECT_EQ((rohit::serializer::binary_integer_size<storage, fixed_policy>(*store.read())), fixed.size());
    } else if (source == migration::payload_codec::binary_string) {
      EXPECT_EQ((rohit::serializer::binary_string_size<storage, fixed_policy>(*store.read())), fixed.size());
    }
    for (const auto target : {migration::payload_codec::binary_none, migration::payload_codec::binary_integer,
                              migration::payload_codec::binary_string}) {
      migration::options selected;
      selected.source_codec = source;
      selected.target_codec = target;
      const auto converted = migration::migrate_raw<root>(fixed, selected);
      const auto decoded = migration::detail::decode_value<storage>(converted, managed::identity_encoding::compact_varint, target, {});
      EXPECT_EQ(traits::payload(decoded).ordinary, std::numeric_limits<std::uint64_t>::max());
      EXPECT_EQ(traits::payload(decoded).selected, traits::payload(*store.read()).selected);
      EXPECT_EQ(traits::payload(decoded).entries[1].persistent_id,
                traits::payload(*store.read()).entries[1].persistent_id);
      EXPECT_EQ(managed::model_traits<migration_example::entry>::payload(traits::payload(decoded).entries[0]).amount,
                0x0102030405060708ULL);
    }
  }
}

// Explicit policy conversion retains the full unsigned ID domain, including growth beyond fixed width.
TEST(managed_migration, raw_identity_boundaries_do_not_narrow_or_reassign_values) {
  using legacy = migration_example::legacy_value;
  using legacy_traits = managed::model_traits<legacy>;
  for (const auto id : {id_type{0U}, id_type{127U}, id_type{128U}, id_type{16384U}, std::numeric_limits<id_type>::max()}) {
    typename legacy_traits::storage_type value{};
    value.persistent_id = id;
    legacy_traits::payload(value).amount = std::numeric_limits<std::uint64_t>::max();
    migration::options selected;
    selected.source_codec = selected.target_codec = migration::payload_codec::binary_none;
    const auto fixed = migration::detail::encode_value(value, managed::identity_encoding::fixed, selected.source_codec, 65536U);
    const auto output = migration::migrate_raw<legacy>(fixed, selected);
    const auto decoded = migration::detail::decode_value<typename legacy_traits::storage_type>(output,
        managed::identity_encoding::compact_varint, selected.target_codec, {});
    EXPECT_EQ(decoded.persistent_id, id);
    EXPECT_EQ(legacy_traits::payload(decoded).amount, std::numeric_limits<std::uint64_t>::max());
  }
}

// Every currently emitted native history/label shape is preserved, including redo and tree ancestry.
TEST(managed_migration, all_native_save_history_shapes) {
  check_saved_history<managed::history_mode::disabled, managed::history_labels::disabled>();
  check_saved_history<managed::history_mode::linear, managed::history_labels::disabled>();
  check_saved_history<managed::history_mode::linear, managed::history_labels::enabled>();
  check_saved_history<managed::history_mode::tree, managed::history_labels::disabled>();
  check_saved_history<managed::history_mode::tree, managed::history_labels::enabled>();
}

// A saved authority's burned allocation ranges remain available to recovery without renumbering entities.
TEST(managed_migration, preserves_native_authority_lease_ledger) {
  managed::collaboration_authority<root> authority{initial(), 1, {41U, 42U}};
  authority.open_session(10);
  const auto lease = authority.reserve_ids(10, 12U);
  const auto original = authority.save_document();
  migration::options reverse;
  reverse.source_encoding = managed::identity_encoding::compact_varint;
  reverse.target_encoding = managed::identity_encoding::fixed;
  const auto fixed = migration::migrate_save<root>(original, reverse);
  const auto converted = migration::migrate_save<root>(fixed);
  EXPECT_EQ(converted, original);
  managed::collaboration_authority<root> restored{initial(), 2};
  restored.load_document(converted);
  restored.open_session(10);
  EXPECT_GT(restored.reserve_ids(10, 1U).first, lease.last);
}

// Malformed input, wrong source policy, unsupported containers and resource failures publish no result.
TEST(managed_migration, rejects_mismatched_or_unbounded_inputs) {
  managed::model_store<root> store{initial(), {41U, 42U}};
  const auto bytes = store.save();
  EXPECT_THROW((void)migration::migrate_save<root>(bytes), std::invalid_argument);
  migration::options selected;
  selected.source_encoding = managed::identity_encoding::compact_varint;
  auto corrupt = bytes;
  corrupt.pop_back();
  EXPECT_ANY_THROW((void)migration::migrate_save<root>(corrupt, selected));
  selected.max_input_bytes = 1U;
  EXPECT_THROW((void)migration::migrate_save<root>(bytes, selected), std::length_error);
  selected = {};
  selected.source_encoding = managed::identity_encoding::compact_varint;
  selected.max_output_bytes = 1U;
  EXPECT_ANY_THROW((void)migration::migrate_save<root>(bytes, selected));
  selected = {};
  selected.source_encoding = managed::identity_encoding::compact_varint;
  selected.max_working_bytes = 1U;
  EXPECT_THROW((void)migration::migrate_save<root>(bytes, selected), std::length_error);
  migration::options wrong_format;
  wrong_format.source_codec = migration::payload_codec::binary_none;
  EXPECT_THROW((void)migration::migrate_save<root>(bytes, wrong_format), std::invalid_argument);
}

// Older generated functions cannot honor policy overrides; reject before instantiating their model codecs.
TEST(managed_migration, rejects_stale_managed_headers_before_model_instantiation) {
  const auto require_regeneration = [](auto migrate) {
    try {
      migrate();
      FAIL() << "Stale generated managed headers must be rejected";
    } catch (const std::invalid_argument& error) {
      EXPECT_NE(std::string_view{error.what()}.find("regenerate with the current compiler"), std::string_view::npos);
    }
  };
  require_regeneration([] { (void)migration::migrate_raw<legacy_root, legacy_traits>({}); });
  require_regeneration([] { (void)migration::migrate_save<legacy_root, legacy_traits>({}); });
}

// Ordinary generated values can use the reusable format converter without managed model traits.
TEST(managed_migration, ordinary_raw_model_and_explicit_fixed_wire_fixture) {
  migration_example::ordinary_value ordinary{17U, "plain"};
  const auto input = migration::detail::encode_value(ordinary, managed::identity_encoding::fixed,
      migration::payload_codec::binary_none, 4096U);
  migration::options selected;
  selected.source_codec = migration::payload_codec::binary_none;
  selected.target_codec = migration::payload_codec::binary_string;
  const auto output = migration::migrate_raw<migration_example::ordinary_value>(input, selected);
  const auto decoded = migration::detail::decode_value<migration_example::ordinary_value>(output,
      managed::identity_encoding::compact_varint, selected.target_codec, {});
  EXPECT_EQ(decoded.amount, 17U);
  EXPECT_EQ(decoded.name, "plain");
  EXPECT_THROW((void)migration::migrate_save<migration_example::ordinary_value>(input), std::invalid_argument);

  // Independent direct32 fixed-wire fixture: key1, uint64 amount, reserved ID key, uint32 ID, terminator.
  if constexpr (sizeof(id_type) == sizeof(std::uint32_t) && std::same_as<storage, root>) {
    const std::array<std::uint8_t, 18> fixed{1U, 8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U,
        255U, 255U, 255U, 255U, 128U, 0U, 0U, 0U, 0U};
    const auto converted = migration::migrate_raw<migration_example::legacy_value>(fixed);
    const auto value = migration::detail::decode_value<managed::model_traits<migration_example::legacy_value>::storage_type>(
        converted, managed::identity_encoding::compact_varint, migration::payload_codec::binary_integer, {});
    EXPECT_EQ(value.persistent_id, 128U);
    EXPECT_EQ(managed::model_traits<migration_example::legacy_value>::payload(value).amount, 0x0102030405060708ULL);
    managed::records::unlabeled_state_envelope old_save;
    old_save.format_version = 3U;
    old_save.schema_id = migration::detail::schema_binding<managed::model_traits<migration_example::legacy_value>>(managed::identity_encoding::fixed);
    old_save.id_bits = 32U;
    old_save.document_high = 41U;
    old_save.document_low = 42U;
    old_save.allocated_id = 128U;
    old_save.mode = static_cast<std::uint32_t>(managed::history_mode::disabled);
    old_save.current_snapshot.assign(fixed.begin(), fixed.end());
    const auto migrated_save = migration::migrate_save<migration_example::legacy_value>(managed::detail::encode(old_save));
    managed::model_store<migration_example::legacy_value, managed::history_mode::disabled> restored{migration_example::legacy_value{}};
    restored.load(migrated_save);
    EXPECT_EQ(restored.document(), (managed::document_id{41U, 42U}));
    EXPECT_EQ(restored.allocated_id(), 128U);
    EXPECT_EQ(restored.read()->persistent_id, 128U);
  }
}
