#include <managed_model.hpp>
#include <rohit/managed.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed {

// Explicit adapter for generated records; automatic companion generation is a later feature.
template <>
struct model_traits<managed_test::document> {
  using storage_type = managed_test::document_record;
  using id_type = std::uint32_t;
  static constexpr std::string_view schema_id = "serializer.test.points.v1";

  // Wrap ordinary points in generated ID-bearing records without changing the payload class.
  static storage_type make_storage(managed_test::document value) {
    storage_type result;
    for (auto& point : value.points) {
      result.points.push_back({0, std::move(point)});
    }
    return result;
  }

  // Visit every owned identity, preserving mutable versus const ID references.
  static void visit_entities(auto& value, auto&& visitor) {
    visitor(value.persistent_id, "document");
    for (auto& point : value.points) {
      visitor(point.persistent_id, "point");
    }
  }

  // Materialize a detached ordinary document with no framework metadata.
  static managed_test::document clone_value(const storage_type& value) {
    managed_test::document result;
    for (const auto& point : value.points) {
      result.points.push_back(point.value);
    }
    return result;
  }

  // Reject a complete candidate violating the application's coordinate invariant.
  static void validate(const storage_type& value) {
    for (const auto& point : value.points) {
      if (point.value.x < 0 || point.value.y < 0) {
        throw std::invalid_argument{"Negative coordinate"};
      }
    }
  }
};

struct wide_traits {
  using storage_type = managed_test::document_record64;
  using id_type = std::uint64_t;
  static constexpr std::string_view schema_id = "serializer.test.wide.v1";

  // Wrap a root with a wide ID while retaining ordinary child values.
  static storage_type make_storage(managed_test::document value) {
    return {0, std::move(value)};
  }

  // The wide fixture identifies only its root.
  static void visit_entities(auto& value, auto&& visitor) {
    visitor(value.persistent_id, "document");
  }

  // Copy ordinary values independently of managed lifetime.
  static managed_test::document clone_value(const storage_type& value) {
    return value.value;
  }

  // The wide fixture adds no application constraints.
  static void validate(const storage_type&) {}
};

} // namespace rohit::managed

namespace {
namespace managed = rohit::managed;
using store_type = managed::model_store<managed_test::document>;
using tree_store = managed::model_store<managed_test::document, managed::history_mode::tree>;
using labeled_store = managed::model_store<managed_test::document, managed::history_mode::linear,
                                           managed::history_labels::enabled>;
using labeled_tree_store = managed::model_store<managed_test::document, managed::history_mode::tree,
                                                managed::history_labels::enabled>;
using disabled_store = managed::model_store<managed_test::document, managed::history_mode::disabled>;
using status = managed::transaction_status;

// Construct one ordinary point for all transaction and snapshot tests.
managed_test::document initial_value() {
  return {{{1, 2}}};
}

// Change both coordinates in one transaction using the explicit storage adapter.
auto move_point(auto& store, int x, int y) {
  return store.execute_transaction([x, y](auto& transaction) {
    transaction.update([x, y](auto& document) {
      document.points.front().value.x = x;
      document.points.front().value.y = y;
    });
  });
}

// Verify the three public completion forms share atomic publication and stable identities.
TEST(managed_store, manual_scoped_and_callback_completion) {
  store_type store{initial_value(), {1, 2}};
  const auto pinned = store.read();
  const auto point_id = pinned->points.front().persistent_id;
  managed::transaction_outcome manual;
  {
    auto transaction = store.begin_transaction(manual);
    transaction.update([](auto& document) { document.points.front().value.x = 5; });
    EXPECT_EQ(store.read()->points.front().value.x, 1);
    transaction.commit();
    EXPECT_EQ(manual.status, status::committed);
  }
  managed::transaction_outcome automatic;
  {
    auto transaction = store.begin_transaction(automatic);
    transaction.update([](auto& document) { document.points.front().value.y = 6; });
  }
  EXPECT_EQ(automatic.status, status::committed);
  const auto callback = move_point(store, 10, 20);
  callback.throw_if_failed();
  EXPECT_EQ(callback.status, status::committed);
  EXPECT_EQ(store.read()->points.front().persistent_id, point_id);
  EXPECT_EQ(pinned->points.front().value.x, 1);
  EXPECT_EQ(store.clone_value().points.front().y, 20);
  store.undo();
  EXPECT_EQ(store.read()->points.front().value.x, 5);
  EXPECT_EQ(store.read()->points.front().value.y, 6);
}

// Explicit cancellation, caught edit errors, and callback exceptions all preserve the old root.
TEST(managed_store, cancellation_and_poisoned_edits) {
  store_type store{initial_value(), {1, 2}};
  auto canceled = store.execute_transaction([](auto& transaction) {
    transaction.update([](auto& value) { value.points.clear(); });
    transaction.revert();
  });
  EXPECT_EQ(canceled.status, status::reverted);
  auto failed = store.execute_transaction([](auto& transaction) {
    transaction.update([](auto& value) { value.points.clear(); });
    throw std::runtime_error{"Application failure"};
  });
  EXPECT_EQ(failed.status, status::failed);
  EXPECT_THROW(failed.throw_if_failed(), std::runtime_error);
  auto caught = store.execute_transaction([](auto& transaction) {
    try {
      transaction.update([](auto& value) {
        value.points.clear();
        throw std::runtime_error{"Edit failure"};
      });
    } catch (const std::runtime_error&) {
    }
  });
  EXPECT_EQ(caught.status, status::failed);
  EXPECT_EQ(store.read()->points.size(), 1u);
  EXPECT_EQ(move_point(store, 3, 4).status, status::committed);
}

// Exceptional guard unwinding reverts; normal destructor validation failure remains observable.
TEST(managed_store, destructor_failure_unwinding_and_move) {
  store_type store{initial_value(), {1, 2}};
  managed::transaction_outcome outcome;
  try {
    auto transaction = store.begin_transaction(outcome);
    transaction.update([](auto& value) { value.points.clear(); });
    throw std::runtime_error{"Stop"};
  } catch (const std::runtime_error&) {
  }
  EXPECT_EQ(outcome.status, status::reverted);
  {
    auto transaction = store.begin_transaction(outcome);
    auto moved = std::move(transaction);
    moved.update([](auto& value) { value.points.front().value.x = -1; });
  }
  EXPECT_EQ(outcome.status, status::failed);
  EXPECT_THROW(outcome.throw_if_failed(), std::invalid_argument);
  EXPECT_EQ(store.read()->points.front().value.x, 1);
  EXPECT_EQ(move_point(store, 4, 5).status, status::committed);
}

// A nested begin is rejected without invoking its callback or consuming the outer writer slot.
TEST(managed_store, nested_and_noop_transactions) {
  store_type store{initial_value(), {1, 2}};
  auto outer = store.execute_transaction([&](auto&) {
    bool called = false;
    const auto inner = store.execute_transaction([&](auto&) { called = true; });
    EXPECT_FALSE(called);
    EXPECT_EQ(inner.status, status::failed);
  });
  EXPECT_EQ(outer.status, status::no_change);
  move_point(store, 3, 4).throw_if_failed();
  store.undo();
  EXPECT_EQ(move_point(store, 1, 2).status, status::no_change);
  store.redo();
  EXPECT_EQ(store.read()->points.front().value.x, 3);
}

// Branching preserves abandoned futures; linear commits explicitly remove the redo suffix.
TEST(managed_store, linear_and_tree_history) {
  const auto check = []<managed::history_mode Mode>() {
    managed::model_store<managed_test::document, Mode> store{initial_value(), {1, 2}};
    const auto first = move_point(store, 3, 4);
    const auto second = move_point(store, 5, 6);
    store.undo();
    const auto alternate = move_point(store, 7, 8);
    store.undo();
    if constexpr (Mode == managed::history_mode::tree) {
      EXPECT_EQ(store.redo_children(), (std::vector<std::uint64_t>{second.revision, alternate.revision}));
      store.redo(second.revision);
      EXPECT_EQ(store.read()->points.front().value.x, 5);
      store.checkout(first.revision);
      store.redo(alternate.revision);
    } else {
      first.throw_if_failed();
      second.throw_if_failed();
      alternate.throw_if_failed();
      store.redo();
      EXPECT_THROW(store.redo(), std::out_of_range);
    }
    EXPECT_EQ(store.read()->points.front().value.y, 8);
  };
  check.template operator()<managed::history_mode::linear>();
  check.template operator()<managed::history_mode::tree>();
}

// Deletion retains historical values and IDs; cancellation and pruning never recycle IDs.
TEST(managed_store, deletion_restoration_and_consumed_ids) {
  store_type store{initial_value(), {1, 2}};
  const auto original_id = store.read()->points.front().persistent_id;
  std::uint32_t canceled_id{};
  const auto cancellation = store.execute_transaction([&](auto& transaction) {
    canceled_id = transaction.create_id();
    transaction.revert();
  });
  EXPECT_EQ(cancellation.status, status::reverted);
  auto deletion = store.execute_transaction([](auto& transaction) {
    transaction.update([](auto& document) { document.points.clear(); });
  });
  EXPECT_EQ(deletion.status, status::committed);
  EXPECT_TRUE(store.read()->points.empty());
  store.undo();
  EXPECT_EQ(store.read()->points.front().persistent_id, original_id);
  std::uint32_t new_id{};
  auto insertion = store.execute_transaction([&](auto& transaction) {
    new_id = transaction.create_id();
    transaction.update([&](auto& document) { document.points.push_back({new_id, {9, 10}}); });
  });
  EXPECT_EQ(insertion.status, status::committed);
  EXPECT_GT(new_id, canceled_id);
}

// Identity and budget validation happen before publication and release the writer on failure.
TEST(managed_store, identity_and_budget_failures_are_atomic) {
  managed::store_options options;
  options.max_revisions = 1;
  tree_store store{initial_value(), {1, 2}, options};
  auto limited = move_point(store, 3, 4);
  EXPECT_EQ(limited.status, status::failed);
  EXPECT_EQ(store.read()->points.front().value.x, 1);
  auto duplicate = store.execute_transaction([](auto& transaction) {
    transaction.update(
        [](auto& document) { document.points.front().persistent_id = document.persistent_id; });
  });
  EXPECT_EQ(duplicate.status, status::failed);
  EXPECT_EQ(store.read()->points.front().persistent_id, 2u);
  auto root = store.execute_transaction([](auto& transaction) {
    const auto id = transaction.create_id();
    transaction.update([id](auto& document) { document.persistent_id = id; });
  });
  EXPECT_EQ(root.status, status::failed);
}

// The retention limit counts states; deque position alone selects undo and redo targets.
TEST(managed_store, linear_history_evicts_oldest_states) {
  managed::store_options options;
  constexpr std::size_t retained_states = 100;
  options.max_revisions = retained_states;
  store_type store{initial_value(), {1, 2}, options};
  const auto pinned = store.read();
  constexpr int edits = 120;
  for (int index = 1; index <= edits; ++index) {
    ASSERT_EQ(move_point(store, index + 1, 2).status, status::committed);
  }
  const auto saved = store.save();
  const auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(saved, {});
  ASSERT_EQ(envelope.entries.size(), retained_states);
  EXPECT_EQ(envelope.cursor, retained_states - 1);
  EXPECT_EQ(pinned->points.front().value.x, 1);
  for (std::size_t index = 1; index < retained_states; ++index) {
    store.undo();
  }
  EXPECT_EQ(store.read()->points.front().value.x, edits + 2 - retained_states);
  EXPECT_THROW(store.undo(), std::out_of_range);
  for (std::size_t index = 1; index < retained_states; ++index) {
    store.redo();
  }
  EXPECT_THROW(store.redo(), std::out_of_range);
  EXPECT_EQ(store.save(), saved);
}

// One state permits editing without undo; zero is valid only for a disabled history store.
TEST(managed_store, linear_history_minimum_capacity) {
  managed::store_options options;
  options.max_revisions = 1;
  store_type store{initial_value(), {1, 2}, options};
  ASSERT_EQ(move_point(store, 3, 4).status, status::committed);
  EXPECT_THROW(store.undo(), std::out_of_range);
  EXPECT_THROW(store.redo(), std::out_of_range);
  const auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(store.save(), {});
  ASSERT_EQ(envelope.entries.size(), 1u);
  EXPECT_EQ(envelope.cursor, 0u);
  options.max_revisions = 0;
  EXPECT_THROW((store_type{initial_value(), {1, 2}, options}), std::length_error);
  EXPECT_THROW((tree_store{initial_value(), {1, 2}, options}), std::length_error);
  disabled_store disabled{initial_value(), {1, 2}, options};
  EXPECT_EQ(move_point(disabled, 3, 4).status, status::committed);
}

// Byte eviction includes labels; failed, canceled, and unchanged edits preserve the redo suffix.
TEST(managed_store, linear_history_byte_retention_and_atomic_failure) {
  labeled_store probe{initial_value(), {1, 2}};
  move_point(probe, 3, 4).throw_if_failed();
  const auto sample = managed::detail::decode<managed::records::state_envelope>(probe.save(), {});
  const auto entry_bytes = sample.entries.back().snapshot.size() + sample.entries.back().label.size();
  managed::store_options options;
  options.max_history_bytes = 2 * entry_bytes;
  labeled_store store{initial_value(), {1, 2}, options};
  move_point(store, 3, 4).throw_if_failed();
  move_point(store, 5, 6).throw_if_failed();
  store.undo();
  EXPECT_THROW(store.undo(), std::out_of_range);
  const auto before = store.save();
  const auto too_large = store.execute_transaction(
      std::string(options.max_history_bytes, 'a'), [](auto& transaction) {
        transaction.update([](auto& document) { document.points.front().value.x = 7; });
      });
  EXPECT_EQ(too_large.status, status::failed);
  EXPECT_THROW(too_large.throw_if_failed(), std::length_error);
  EXPECT_EQ(move_point(store, 3, 4).status, status::no_change);
  EXPECT_EQ(move_point(store, -1, 4).status, status::failed);
  const auto canceled = store.execute_transaction([](auto& transaction) {
    transaction.update([](auto& document) { document.points.front().value.x = 8; });
    transaction.revert();
  });
  EXPECT_EQ(canceled.status, status::reverted);
  EXPECT_EQ(store.save(), before);
  store.redo();
  EXPECT_EQ(store.read()->points.front().value.x, 5);
  store.undo();
  move_point(store, 7, 8).throw_if_failed();
  store.undo();
  EXPECT_THROW(store.undo(), std::out_of_range);
  store.redo();
  EXPECT_EQ(store.read()->points.front().value.x, 7);
  EXPECT_THROW(store.redo(), std::out_of_range);
  store.undo();
  const auto snapshot_bytes = sample.entries.back().snapshot.size();
  const auto large = store.execute_transaction(
      std::string(options.max_history_bytes - snapshot_bytes, 'b'), [](auto& transaction) {
        transaction.update([](auto& document) { document.points.front().value.x = 9; });
      });
  ASSERT_EQ(large.status, status::committed);
  const auto retained = managed::detail::decode<managed::records::state_envelope>(store.save(), {});
  ASSERT_EQ(retained.entries.size(), 1u);
  EXPECT_EQ(retained.cursor, 0u);
  EXPECT_THROW(store.undo(), std::out_of_range);
}

// Ordered entries and an interior cursor survive reload; malformed or oversized data is atomic.
TEST(managed_store, linear_history_round_trip_and_validation) {
  managed::store_options options;
  options.max_revisions = 3;
  store_type store{initial_value(), {1, 2}, options};
  move_point(store, 3, 4).throw_if_failed();
  move_point(store, 5, 6).throw_if_failed();
  store.undo();
  move_point(store, 7, 8).throw_if_failed();
  move_point(store, 9, 10).throw_if_failed();
  store.undo();
  const auto saved = store.save();
  store_type loaded{initial_value(), {3, 4}, options};
  loaded.load(saved);
  EXPECT_EQ(loaded.save(), saved);
  loaded.redo();
  EXPECT_EQ(loaded.read()->points.front().value.x, 9);
  EXPECT_THROW(loaded.redo(), std::out_of_range);
  loaded.undo();
  loaded.undo();
  EXPECT_EQ(loaded.read()->points.front().value.x, 3);
  EXPECT_THROW(loaded.undo(), std::out_of_range);
  loaded.redo();
  EXPECT_EQ(loaded.read()->points.front().value.x, 7);
  EXPECT_EQ(loaded.save(), saved);
  for (int corruption = 0; corruption < 6; ++corruption) {
    auto invalid = managed::detail::decode<managed::records::unlabeled_state_envelope>(saved, {});
    switch (corruption) {
    case 0: invalid.cursor = invalid.entries.size(); break;
    case 1: invalid.cursor = std::numeric_limits<std::uint64_t>::max(); break;
    case 2: invalid.entries.clear(); break;
    case 3: invalid.current_snapshot = invalid.entries.front().snapshot; break;
    case 4: invalid.format_version = 1; break;
    case 5: invalid.entries.front().snapshot = {0}; break;
    }
    EXPECT_THROW(loaded.load(managed::detail::encode(invalid)), std::exception);
    EXPECT_EQ(loaded.save(), saved);
  }
  options.max_revisions = 2;
  store_type smaller{initial_value(), {3, 4}, options};
  const auto before = smaller.save();
  EXPECT_THROW(smaller.load(saved), std::length_error);
  EXPECT_EQ(smaller.save(), before);
}

// Navigation validation cannot move the linear cursor or publish a partially restored state.
TEST(managed_store, linear_navigation_validation_is_atomic) {
  bool reject = false;
  store_type store{initial_value(), {1, 2}, {}, [&](const auto&) {
    if (reject) {
      throw std::invalid_argument{"Rejected navigation"};
    }
  }};
  move_point(store, 3, 4).throw_if_failed();
  const auto edited = store.save();
  reject = true;
  EXPECT_THROW(store.undo(), std::invalid_argument);
  EXPECT_EQ(store.save(), edited);
  reject = false;
  store.undo();
  const auto undone = store.save();
  reject = true;
  EXPECT_THROW(store.redo(), std::invalid_argument);
  EXPECT_EQ(store.save(), undone);
  reject = false;
  store.redo();
  EXPECT_EQ(store.save(), edited);
}

// Version-one linear envelopes cannot be silently interpreted as ordered version-two entries.
TEST(managed_store, rejects_legacy_linear_envelope) {
  store_type store{initial_value(), {1, 2}};
  const auto before = store.save();
  const auto current = managed::detail::decode<managed::records::unlabeled_state_envelope>(before, {});
  managed::records::envelope legacy;
  legacy.format_version = 1;
  legacy.schema_id = current.schema_id;
  legacy.id_bits = current.id_bits;
  legacy.document_high = current.document_high;
  legacy.document_low = current.document_low;
  legacy.allocated_id = current.allocated_id;
  legacy.revision_high_water = legacy.current_revision = 1;
  legacy.mode = current.mode;
  legacy.current_snapshot = current.current_snapshot;
  managed::records::revision baseline;
  baseline.number = 1;
  baseline.snapshot = current.current_snapshot;
  legacy.revisions.push_back(std::move(baseline));
  EXPECT_THROW(store.load(managed::detail::encode(legacy)), std::exception);
  EXPECT_EQ(store.save(), before);
}

// Generated envelopes preserve branching state and identity; malformed input cannot partially load.
TEST(managed_store, save_reload_and_corruption) {
  managed::store_options options;
  tree_store original{initial_value(), {3, 4}, options};
  const auto first = move_point(original, 3, 4);
  const auto second = move_point(original, 5, 6);
  original.undo();
  move_point(original, 7, 8).throw_if_failed();
  auto bytes = original.save();
  tree_store loaded{initial_value(), {8, 9}, options};
  loaded.load(bytes);
  EXPECT_EQ(loaded.document().high, 3u);
  EXPECT_EQ(loaded.read()->points.front().value.x, 7);
  loaded.checkout(first.revision);
  loaded.redo(second.revision);
  EXPECT_EQ(loaded.read()->points.front().value.y, 6);
  const auto before = loaded.save();
  bytes.pop_back();
  EXPECT_THROW(loaded.load(bytes), std::exception);
  EXPECT_EQ(loaded.save(), before);
  auto envelope = managed::detail::decode<managed::records::unlabeled_envelope>(before, {});
  envelope.revisions.back().parent = envelope.revisions.back().number;
  EXPECT_THROW(loaded.load(managed::detail::encode(envelope)), std::invalid_argument);
  EXPECT_EQ(loaded.save(), before);
}

// No-history stores have no revision container and support a separate uint64 storage profile.
TEST(managed_store, disabled_history_and_wide_identity) {
  managed::store_options options;
  using wide_store = managed::model_store<managed_test::document,
                                          managed::history_mode::disabled, managed::history_labels::disabled, managed::wide_traits>;
  wide_store store{initial_value(), {1, 2}, options};
  auto result = store.execute_transaction([](auto& transaction) {
    transaction.update([](auto& document) { document.value.points.front().x = 42; });
  });
  EXPECT_EQ(result.status, status::committed);
  const auto saved = store.save();
  auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(saved, {});
  EXPECT_EQ(envelope.id_bits, 64u);
  EXPECT_TRUE(envelope.entries.empty());
  store.load(saved);
  EXPECT_EQ(store.clone_value().points.front().x, 42);
  auto wide_root =
      managed::detail::decode<managed_test::document_record64>(envelope.current_snapshot, {});
  const auto wide_id = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1;
  wide_root.persistent_id = wide_id;
  envelope.document_high = 9;
  envelope.allocated_id = wide_id;
  envelope.current_snapshot = managed::detail::encode(wide_root);
  store.load(managed::detail::encode(envelope));
  EXPECT_EQ(store.read()->persistent_id, wide_id);
  std::uint64_t next_id{};
  const auto reserved = store.execute_transaction([&](auto& transaction) {
    next_id = transaction.create_id();
    transaction.revert();
  });
  EXPECT_EQ(reserved.status, status::reverted);
  EXPECT_EQ(next_id, wide_id + 1);
}

// Reloading an older save in a live document cannot recycle previously reserved IDs or revisions.
TEST(managed_store, reload_preserves_live_high_water_marks) {
  tree_store store{initial_value(), {1, 2}};
  const auto older = store.save();
  std::uint32_t consumed{};
  auto canceled = store.execute_transaction([&](auto& transaction) {
    consumed = transaction.create_id();
    transaction.revert();
  });
  EXPECT_EQ(canceled.status, status::reverted);
  const auto newer = move_point(store, 3, 4);
  store.load(older);
  std::uint32_t next{};
  auto inserted = store.execute_transaction([&](auto& transaction) {
    next = transaction.create_id();
    transaction.update([next](auto& document) { document.points.push_back({next, {8, 9}}); });
  });
  EXPECT_GT(next, consumed);
  EXPECT_GT(inserted.revision, newer.revision);
}

// Illegal reentrancy poisons the action but must not free a candidate borrowed by the callback.
TEST(managed_store, caught_reentrant_edit_failure) {
  store_type store{initial_value(), {1, 2}};
  const auto failed = store.execute_transaction([](auto& transaction) {
    try {
      transaction.update([&](auto& document) {
        try {
          transaction.revert();
        } catch (const std::logic_error&) {
          document.points.front().value.x = 12;
        }
      });
    } catch (const std::logic_error&) {
    }
  });
  EXPECT_EQ(failed.status, status::failed);
  EXPECT_EQ(store.read()->points.front().value.x, 1);
  EXPECT_EQ(move_point(store, 4, 5).status, status::committed);
}

// Reset clears history in either policy while preserving the current model and selected mode.
TEST(managed_store, history_reset_preserves_selected_mode) {
  const auto check = []<managed::history_mode Mode>() {
    managed::model_store<managed_test::document, Mode> store{initial_value(), {1, 2}};
    move_point(store, 3, 4).throw_if_failed();
    move_point(store, 5, 6).throw_if_failed();
    store.reset_history();
    const auto baseline = store.save();
    EXPECT_THROW(store.undo(), std::out_of_range);
    move_point(store, 7, 8).throw_if_failed();
    store.undo();
    EXPECT_EQ(store.read()->points.front().value.x, 5);
    store.load(baseline);
    if constexpr (Mode == managed::history_mode::linear) {
      EXPECT_THROW(store.redo(), std::out_of_range);
    } else {
      EXPECT_TRUE(store.redo_children().empty());
    }
    EXPECT_THROW(store.undo(), std::out_of_range);
  };
  check.template operator()<managed::history_mode::linear>();
  check.template operator()<managed::history_mode::tree>();
}

// A saved mode must match the receiving store specialization; loading cannot switch storage types.
TEST(managed_store, load_rejects_other_history_specializations) {
  store_type linear{initial_value(), {1, 2}};
  tree_store tree{initial_value(), {1, 2}};
  disabled_store disabled{initial_value(), {1, 2}};
  const auto linear_bytes = linear.save();
  const auto tree_bytes = tree.save();
  const auto disabled_bytes = disabled.save();
  EXPECT_THROW(linear.load(tree_bytes), std::exception);
  EXPECT_THROW(linear.load(disabled_bytes), std::exception);
  EXPECT_THROW(tree.load(linear_bytes), std::exception);
  EXPECT_THROW(tree.load(disabled_bytes), std::exception);
  EXPECT_THROW(disabled.load(linear_bytes), std::exception);
  EXPECT_THROW(disabled.load(tree_bytes), std::exception);
  EXPECT_EQ(linear.save(), linear_bytes);
  EXPECT_EQ(tree.save(), tree_bytes);
  EXPECT_EQ(disabled.save(), disabled_bytes);
}

// Public history interfaces are selected at compile time, with no revision API on linear stores.
template <typename Store>
concept has_basic_history = requires(Store& store) { store.undo(); store.reset_history(); };

// Only linear stores can redo without selecting a branch.
template <typename Store>
concept has_linear_redo = requires(Store& store) { store.redo(); };

// Revision-addressed navigation is restricted to tree history.
template <typename Store>
concept has_tree_navigation = requires(Store& store) {
  store.redo(1); store.checkout(1); store.redo_children();
};

// Linear and disabled outcomes expose completion and failure without revision identity.
template <typename Outcome>
concept has_revision = requires(Outcome outcome) { outcome.revision; };

static_assert(has_basic_history<store_type> && has_basic_history<tree_store>);
static_assert(!has_basic_history<disabled_store>);
static_assert(has_linear_redo<store_type> && !has_linear_redo<tree_store>);
static_assert(!has_linear_redo<disabled_store>);
static_assert(!has_tree_navigation<store_type> && has_tree_navigation<tree_store>);
static_assert(!has_tree_navigation<disabled_store>);
static_assert(!has_revision<store_type::outcome_type>);
static_assert(!has_revision<disabled_store::outcome_type>);
static_assert(has_revision<tree_store::outcome_type>);
static_assert(std::is_empty_v<managed::detail::history_storage<managed::history_mode::disabled>>);

// Finite ID space exhaustion is reported without wrapping or leaving a writer active.
TEST(managed_store, id_exhaustion) {
  store_type store{initial_value(), {1, 2}};
  auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(store.save(), {});
  envelope.allocated_id = std::numeric_limits<std::uint32_t>::max();
  store.load(managed::detail::encode(envelope));
  const auto exhausted = store.execute_transaction([](auto& transaction) { static_cast<void>(transaction.create_id()); });
  EXPECT_EQ(exhausted.status, status::failed);
  EXPECT_THROW(exhausted.throw_if_failed(), std::overflow_error);
  EXPECT_EQ(move_point(store, 4, 5).status, status::committed);
}

// Export budgets and foreign profiles fail explicitly without damaging the current document.
TEST(managed_store, load_validation_and_encoding_budget) {
  store_type store{initial_value(), {1, 2}};
  const auto saved = store.save();
  auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(saved, {});
  envelope.id_bits = 64;
  EXPECT_THROW(store.load(managed::detail::encode(envelope)), std::invalid_argument);
  envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(saved, {});
  envelope.cursor = 1;
  EXPECT_THROW(store.load(managed::detail::encode(envelope)), std::invalid_argument);
  EXPECT_EQ(store.save(), saved);
  managed::store_options options;
  options.max_snapshot_bytes = 1;
  EXPECT_THROW((store_type{initial_value(), {1, 2}, options}), std::exception);
}

// Application preparation failures must not publish candidate or history state.
TEST(managed_store, preparation_failure_releases_writer) {
  bool fail = false;
  store_type store{initial_value(), {1, 2}, {}, [&](const auto&) {
                     if (fail) {
                       throw std::bad_alloc{};
                     }
                   }};
  fail = true;
  const auto failed = move_point(store, 3, 4);
  EXPECT_EQ(failed.status, status::failed);
  EXPECT_THROW(failed.throw_if_failed(), std::bad_alloc);
  EXPECT_EQ(store.read()->points.front().value.x, 1);
  EXPECT_THROW(store.redo(), std::out_of_range);
  fail = false;
  EXPECT_EQ(move_point(store, 5, 6).status, status::committed);
}

// Opt-in names follow committed actions through undo, redo, moves, and persistence.
TEST(managed_store, optional_linear_labels) {
  labeled_store store{initial_value(), {1, 2}};
  EXPECT_THROW(store.undo_label(), std::out_of_range);
  EXPECT_THROW(store.redo_label(), std::out_of_range);
  managed::transaction_outcome outcome;
  {
    auto original = store.begin_transaction("Move point", outcome);
    auto transaction = std::move(original);
    transaction.update([](auto& document) { document.points.front().value.x = 3; });
    transaction.commit();
  }
  ASSERT_EQ(outcome.status, status::committed);
  EXPECT_EQ(store.undo_label(), "Move point");
  store.undo();
  EXPECT_EQ(store.redo_label(), "Move point");
  EXPECT_EQ(store.execute_transaction("No change", [](auto&) {}).status, status::no_change);
  EXPECT_EQ(store.execute_transaction("Cancel", [](auto& transaction) {
    transaction.revert();
  }).status, status::reverted);
  EXPECT_EQ(store.execute_transaction("Fail", [](auto&) {
    throw std::runtime_error{"Failed edit"};
  }).status, status::failed);
  EXPECT_EQ(store.redo_label(), "Move point");
  const auto saved = store.save();
  EXPECT_EQ((managed::detail::decode<managed::records::state_envelope>(saved, {}).format_version), 2u);
  labeled_store loaded{initial_value()};
  loaded.load(saved);
  EXPECT_EQ(loaded.redo_label(), "Move point");
  loaded.redo();
  EXPECT_EQ(loaded.undo_label(), "Move point");
  move_point(loaded, 5, 6).throw_if_failed();
  EXPECT_EQ(loaded.undo_label(), "");
  loaded.undo();
  EXPECT_EQ(loaded.redo_label(), "");
}

// Tree names belong to each branch, and enabled tree saves retain their original format.
TEST(managed_store, optional_tree_labels) {
  labeled_tree_store store{initial_value(), {1, 2}};
  const auto first = store.execute_transaction("First branch", [](auto& transaction) {
    transaction.update([](auto& document) { document.points.front().value.x = 3; });
  });
  first.throw_if_failed();
  store.undo();
  const auto second = store.execute_transaction("Second branch", [](auto& transaction) {
    transaction.update([](auto& document) { document.points.front().value.x = 5; });
  });
  second.throw_if_failed();
  EXPECT_EQ(store.undo_label(), "Second branch");
  EXPECT_THROW(store.redo_label(first.revision), std::out_of_range);
  store.undo();
  EXPECT_THROW(store.undo_label(), std::out_of_range);
  EXPECT_EQ(store.redo_label(first.revision), "First branch");
  EXPECT_EQ(store.redo_label(second.revision), "Second branch");
  const auto saved = store.save();
  EXPECT_EQ((managed::detail::decode<managed::records::envelope>(saved, {}).format_version), 1u);
  labeled_tree_store loaded{initial_value()};
  loaded.load(saved);
  EXPECT_EQ(loaded.redo_label(first.revision), "First branch");
  loaded.redo(first.revision);
  EXPECT_EQ(loaded.undo_label(), "First branch");
  loaded.checkout(second.revision);
  EXPECT_EQ(loaded.undo_label(), "Second branch");
}

// Default saves omit label fields; crossing label policies fails without changing live state.
TEST(managed_store, label_policy_persistence) {
  // Compare each topology independently because history mode must also match on load.
  const auto check = []<typename Plain, typename Labeled>() {
    Plain plain{initial_value(), {1, 2}};
    Labeled labeled{initial_value(), {1, 2}};
    move_point(plain, 3, 4).throw_if_failed();
    move_point(labeled, 3, 4).throw_if_failed();
    const auto plain_bytes = plain.save();
    const auto labeled_bytes = labeled.save();
    EXPECT_LT(plain_bytes.size(), labeled_bytes.size());
    EXPECT_THROW(plain.load(labeled_bytes), std::exception);
    EXPECT_THROW(labeled.load(plain_bytes), std::exception);
    EXPECT_EQ(plain.save(), plain_bytes);
    EXPECT_EQ(labeled.save(), labeled_bytes);
  };
  check.operator()<store_type, labeled_store>();
  check.operator()<tree_store, labeled_tree_store>();
  store_type plain{initial_value()};
  const auto envelope = managed::detail::decode<managed::records::unlabeled_state_envelope>(plain.save(), {});
  EXPECT_EQ(envelope.format_version, 3u);
}

// Disabled metadata has no string member or per-entry storage overhead.
template <typename Entry>
concept contains_label = requires(Entry entry) { entry.label; };
static_assert(!contains_label<managed::detail::history_entry<>>);
static_assert(!contains_label<managed::records::unlabeled_history_entry>);
static_assert(!contains_label<managed::records::unlabeled_revision>);
static_assert(sizeof(managed::detail::history_entry<>) == sizeof(std::vector<std::uint8_t>));
static_assert(contains_label<managed::detail::history_entry<managed::history_labels::enabled>>);

// Naming APIs are unavailable unless the store's label policy enables them.
template <typename Store>
concept accepts_names = requires(Store& store, typename Store::outcome_type& outcome) {
  store.begin_transaction("Move point", outcome);
  store.execute_transaction("Move point", [](auto&) {});
  store.undo_label();
};
static_assert(!accepts_names<store_type>);
static_assert(!accepts_names<tree_store>);
static_assert(!accepts_names<disabled_store>);
static_assert(accepts_names<labeled_store>);
static_assert(accepts_names<labeled_tree_store>);

// Synchronous callbacks must not silently discard a false/error/async return value.
template <typename Store, typename Callback>
concept accepted_callback =
    requires(Store& store, Callback callback) { store.execute_transaction(callback); };

static_assert(!accepted_callback<store_type, decltype([](auto&) { return false; })>);
static_assert(accepted_callback<store_type, decltype([](auto&) {})>);

} // namespace
