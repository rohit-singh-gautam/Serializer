#include <managed_model.hpp>
#include <rohit/managed.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
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
using status = managed::transaction_status;

// Construct one ordinary point for all transaction and snapshot tests.
managed_test::document initial_value() {
  return {{{1, 2}}};
}

// Change both coordinates in one transaction using the explicit storage adapter.
managed::transaction_outcome move_point(store_type& store, int x, int y) {
  return store.execute_transaction("Move point", [x, y](auto& transaction) {
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
    auto transaction = store.begin_transaction("Manual", manual);
    transaction.update([](auto& document) { document.points.front().value.x = 5; });
    EXPECT_EQ(store.read()->points.front().value.x, 1);
    transaction.commit();
    EXPECT_EQ(manual.status, status::committed);
  }
  managed::transaction_outcome automatic;
  {
    auto transaction = store.begin_transaction("Scoped", automatic);
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
  auto canceled = store.execute_transaction("Cancel", [](auto& transaction) {
    transaction.update([](auto& value) { value.points.clear(); });
    transaction.revert();
  });
  EXPECT_EQ(canceled.status, status::reverted);
  auto failed = store.execute_transaction("Fail", [](auto& transaction) {
    transaction.update([](auto& value) { value.points.clear(); });
    throw std::runtime_error{"Application failure"};
  });
  EXPECT_EQ(failed.status, status::failed);
  EXPECT_THROW(failed.throw_if_failed(), std::runtime_error);
  auto caught = store.execute_transaction("Caught", [](auto& transaction) {
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
    auto transaction = store.begin_transaction("Unwind", outcome);
    transaction.update([](auto& value) { value.points.clear(); });
    throw std::runtime_error{"Stop"};
  } catch (const std::runtime_error&) {
  }
  EXPECT_EQ(outcome.status, status::reverted);
  {
    auto transaction = store.begin_transaction("Invalid", outcome);
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
  auto outer = store.execute_transaction("Outer", [&](auto&) {
    bool called = false;
    const auto inner = store.execute_transaction("Inner", [&](auto&) { called = true; });
    EXPECT_FALSE(called);
    EXPECT_EQ(inner.status, status::failed);
  });
  EXPECT_EQ(outer.status, status::no_change);
  const auto first = move_point(store, 3, 4);
  store.undo();
  EXPECT_EQ(move_point(store, 1, 2).status, status::no_change);
  EXPECT_EQ(store.redo_children(), (std::vector<std::uint64_t>{first.revision}));
}

// Branching preserves abandoned futures; linear commits explicitly remove the redo suffix.
TEST(managed_store, linear_and_tree_history) {
  for (auto mode : {managed::history_mode::linear, managed::history_mode::tree}) {
    managed::store_options options;
    options.mode = mode;
    store_type store{initial_value(), {1, 2}, options};
    const auto first = move_point(store, 3, 4);
    const auto second = move_point(store, 5, 6);
    store.undo();
    const auto alternate = move_point(store, 7, 8);
    store.undo();
    auto children = store.redo_children();
    if (mode == managed::history_mode::tree) {
      EXPECT_EQ(children, (std::vector<std::uint64_t>{second.revision, alternate.revision}));
      store.redo(second.revision);
      EXPECT_EQ(store.read()->points.front().value.x, 5);
      store.checkout(first.revision);
    } else {
      EXPECT_EQ(children, (std::vector<std::uint64_t>{alternate.revision}));
      EXPECT_THROW(store.checkout(second.revision), std::out_of_range);
    }
    store.redo(alternate.revision);
    EXPECT_EQ(store.read()->points.front().value.y, 8);
  }
}

// Deletion retains historical values and IDs; cancellation and pruning never recycle IDs.
TEST(managed_store, deletion_restoration_and_consumed_ids) {
  store_type store{initial_value(), {1, 2}};
  const auto original_id = store.read()->points.front().persistent_id;
  std::uint32_t canceled_id{};
  const auto cancellation = store.execute_transaction("Reserve", [&](auto& transaction) {
    canceled_id = transaction.create_id();
    transaction.revert();
  });
  EXPECT_EQ(cancellation.status, status::reverted);
  auto deletion = store.execute_transaction("Delete", [](auto& transaction) {
    transaction.update([](auto& document) { document.points.clear(); });
  });
  EXPECT_EQ(deletion.status, status::committed);
  EXPECT_TRUE(store.read()->points.empty());
  store.undo();
  EXPECT_EQ(store.read()->points.front().persistent_id, original_id);
  std::uint32_t new_id{};
  auto insertion = store.execute_transaction("Insert", [&](auto& transaction) {
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
  store_type store{initial_value(), {1, 2}, options};
  auto limited = move_point(store, 3, 4);
  EXPECT_EQ(limited.status, status::failed);
  EXPECT_EQ(store.read()->points.front().value.x, 1);
  auto duplicate = store.execute_transaction("Duplicate", [](auto& transaction) {
    transaction.update(
        [](auto& document) { document.points.front().persistent_id = document.persistent_id; });
  });
  EXPECT_EQ(duplicate.status, status::failed);
  EXPECT_EQ(store.read()->points.front().persistent_id, 2u);
  auto root = store.execute_transaction("Root", [](auto& transaction) {
    const auto id = transaction.create_id();
    transaction.update([id](auto& document) { document.persistent_id = id; });
  });
  EXPECT_EQ(root.status, status::failed);
}

// Generated envelopes preserve branching state and identity; malformed input cannot partially load.
TEST(managed_store, save_reload_and_corruption) {
  managed::store_options options;
  options.mode = managed::history_mode::tree;
  store_type original{initial_value(), {3, 4}, options};
  const auto first = move_point(original, 3, 4);
  const auto second = move_point(original, 5, 6);
  original.undo();
  move_point(original, 7, 8).throw_if_failed();
  auto bytes = original.save();
  store_type loaded{initial_value(), {8, 9}, options};
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
  auto envelope = managed::detail::decode<managed::records::envelope>(before, {});
  envelope.revisions.back().parent = envelope.revisions.back().number;
  EXPECT_THROW(loaded.load(managed::detail::encode(envelope)), std::invalid_argument);
  EXPECT_EQ(loaded.save(), before);
}

// No-history stores have no revision container and support a separate uint64 storage profile.
TEST(managed_store, disabled_history_and_wide_identity) {
  managed::store_options options;
  options.mode = managed::history_mode::disabled;
  using wide_store = managed::model_store<managed_test::document,
                                          managed::supported_mechanism::none, managed::wide_traits>;
  wide_store store{initial_value(), {1, 2}, options};
  auto result = store.execute_transaction("Wide", [](auto& transaction) {
    transaction.update([](auto& document) { document.value.points.front().x = 42; });
  });
  EXPECT_EQ(result.status, status::committed);
  const auto saved = store.save();
  auto envelope = managed::detail::decode<managed::records::envelope>(saved, {});
  EXPECT_EQ(envelope.id_bits, 64u);
  EXPECT_TRUE(envelope.revisions.empty());
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
  const auto reserved = store.execute_transaction("Reserve wide ID", [&](auto& transaction) {
    next_id = transaction.create_id();
    transaction.revert();
  });
  EXPECT_EQ(reserved.status, status::reverted);
  EXPECT_EQ(next_id, wide_id + 1);
}

// Reloading an older save in a live document cannot recycle previously reserved IDs or revisions.
TEST(managed_store, reload_preserves_live_high_water_marks) {
  store_type store{initial_value(), {1, 2}};
  const auto older = store.save();
  std::uint32_t consumed{};
  auto canceled = store.execute_transaction("Reserve", [&](auto& transaction) {
    consumed = transaction.create_id();
    transaction.revert();
  });
  EXPECT_EQ(canceled.status, status::reverted);
  const auto newer = move_point(store, 3, 4);
  store.load(older);
  std::uint32_t next{};
  auto inserted = store.execute_transaction("Insert", [&](auto& transaction) {
    next = transaction.create_id();
    transaction.update([next](auto& document) { document.points.push_back({next, {8, 9}}); });
  });
  EXPECT_GT(next, consumed);
  EXPECT_GT(inserted.revision, newer.revision);
}

// Illegal reentrancy poisons the action but must not free a candidate borrowed by the callback.
TEST(managed_store, caught_reentrant_edit_failure) {
  store_type store{initial_value(), {1, 2}};
  const auto failed = store.execute_transaction("Reentrant", [](auto& transaction) {
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

// Explicit history reset drops old branches, while disabled edits form the next enabled baseline.
TEST(managed_store, runtime_history_reset) {
  store_type store{initial_value(), {1, 2}};
  const auto revision = move_point(store, 3, 4).revision;
  store.reset_history(managed::history_mode::disabled);
  EXPECT_THROW(store.undo(), std::out_of_range);
  move_point(store, 5, 6).throw_if_failed();
  store.reset_history(managed::history_mode::tree);
  move_point(store, 7, 8).throw_if_failed();
  store.undo();
  EXPECT_EQ(store.read()->points.front().value.x, 5);
  EXPECT_THROW(store.checkout(revision), std::out_of_range);
}

// Finite ID space exhaustion is reported without wrapping or leaving a writer active.
TEST(managed_store, id_exhaustion) {
  store_type store{initial_value(), {1, 2}};
  auto envelope = managed::detail::decode<managed::records::envelope>(store.save(), {});
  envelope.allocated_id = std::numeric_limits<std::uint32_t>::max();
  store.load(managed::detail::encode(envelope));
  const auto exhausted = store.execute_transaction(
      "Exhaust", [](auto& transaction) { static_cast<void>(transaction.create_id()); });
  EXPECT_EQ(exhausted.status, status::failed);
  EXPECT_THROW(exhausted.throw_if_failed(), std::overflow_error);
  EXPECT_EQ(move_point(store, 4, 5).status, status::committed);
}

// Export budgets and foreign profiles fail explicitly without damaging the current document.
TEST(managed_store, load_validation_and_encoding_budget) {
  store_type store{initial_value(), {1, 2}};
  const auto saved = store.save();
  auto envelope = managed::detail::decode<managed::records::envelope>(saved, {});
  envelope.id_bits = 64;
  EXPECT_THROW(store.load(managed::detail::encode(envelope)), std::invalid_argument);
  envelope = managed::detail::decode<managed::records::envelope>(saved, {});
  envelope.current_revision = 0;
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
  EXPECT_TRUE(store.redo_children().empty());
  fail = false;
  EXPECT_EQ(move_point(store, 5, 6).status, status::committed);
}

// Synchronous callbacks must not silently discard a false/error/async return value.
template <typename Store, typename Callback>
concept accepted_callback =
    requires(Store& store, Callback callback) { store.execute_transaction("Check", callback); };

static_assert(!accepted_callback<store_type, decltype([](auto&) { return false; })>);
static_assert(accepted_callback<store_type, decltype([](auto&) {})>);

} // namespace
