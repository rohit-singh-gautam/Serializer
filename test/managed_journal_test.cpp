#include <ledger.hpp>
#include <rohit/managed.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;
using mode = managed::journal_storage_mode;
using event = managed::journal_io_event;
using status = managed::transaction_status;
using bytes = std::vector<std::uint8_t>;
// Independent wire fixture offsets, pinned to the documented version-two container contract.
constexpr std::size_t file_header_bytes = 72;
constexpr std::size_t frame_header_bytes = 16;
constexpr std::size_t frame_footer_bytes = 8;

class managed_journal_test : public testing::TestWithParam<mode> {
protected:
  std::filesystem::path directory_{};
  std::filesystem::path path_{};

  // Give every test a private directory in the build tree, including companion and lock files.
  void SetUp() override {
    const auto id = managed::make_document_id();
    directory_ = std::filesystem::current_path() /
                 ("journal-test-" + std::to_string(id.high) + "-" + std::to_string(id.low));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    path_ = directory_ / "document";
  }

  // Remove only this test's freshly created directory after all store handles have closed.
  void TearDown() override {
    std::error_code ignored;
    std::filesystem::remove_all(directory_, ignored);
  }

  // Locate the one active companion in tests which have not performed generation replacement.
  std::filesystem::path append_path() const {
    if (GetParam() == mode::appended) {
      return path_;
    }
    for (const auto& item : std::filesystem::directory_iterator{directory_}) {
      if (item.path().filename().string().starts_with("document.journal-")) {
        return item.path();
      }
    }
    throw std::runtime_error{"Missing test sidecar"};
  }
};

// Read exact physical fixture bytes for truncation and corruption qualification.
bytes read_bytes(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error{"Cannot read journal test fixture"};
  }
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

// Replace only a closed test fixture; production publication always uses native durable I/O.
void write_bytes(const std::filesystem::path& path, const bytes& value) {
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.write(reinterpret_cast<const char*>(value.data()),
               static_cast<std::streamsize>(value.size()));
  if (!output) {
    throw std::runtime_error{"Cannot write journal test fixture"};
  }
}

// Change a generated field through the public callback/editor API.
auto rename(auto& store, const char* name) {
  return store.execute_transaction([&](auto& transaction) { transaction.root().set_name(name); });
}

// Copy and recapture a retained journal failure without losing its original storage cause.
void expect_retained_journal_cause(std::exception_ptr error) {
  ASSERT_TRUE(error);
  std::exception_ptr recaptured{};
  try {
    std::rethrow_exception(error);
  } catch (const managed::journal_indeterminate_error& failure) {
    EXPECT_STREQ(failure.what(), "Managed journal commit outcome is indeterminate");
    recaptured = std::current_exception();
  } catch (...) {
    FAIL() << "Retained failure lost its journal_indeterminate_error type";
  }
  ASSERT_TRUE(recaptured);
  try {
    std::rethrow_exception(recaptured);
  } catch (const managed::journal_indeterminate_error& failure) {
    const auto* nested = dynamic_cast<const std::nested_exception*>(&failure);
    ASSERT_NE(nested, nullptr);
    ASSERT_TRUE(nested->nested_ptr());
    try {
      std::rethrow_if_nested(failure);
      FAIL() << "Retained journal failure lost its nested storage cause";
    } catch (const std::system_error& cause) {
      EXPECT_EQ(cause.code(), std::make_error_code(std::errc::no_space_on_device));
    } catch (...) {
      FAIL() << "Retained journal failure changed its nested storage cause type";
    }
  } catch (...) {
    FAIL() << "Recaptured failure lost its journal_indeterminate_error type";
  }
}

// Recovery preserves values, identity, cursor, redo, and the explicit full-Save dirty baseline.
TEST_P(managed_journal_test, linear_recovery_and_full_save) {
  bytes expected;
  managed::document_id document;
  {
    store_type store{ledger{"Base"}};
    document = store.document();
    store.create_journal(path_, GetParam());
    EXPECT_FALSE(store.journal_dirty());
    rename(store, "One").throw_if_failed();
    rename(store, "Two").throw_if_failed();
    store.undo();
    EXPECT_EQ(store.journal_sequence(), 3u);
    EXPECT_TRUE(store.journal_dirty());
    expected = store.save();
    EXPECT_TRUE(store.journal_dirty());
  }
  {
    store_type store{ledger{}};
    store.recover_journal(path_);
    EXPECT_EQ(store.save(), expected);
    EXPECT_EQ(store.document().high, document.high);
    EXPECT_EQ(store.document().low, document.low);
    EXPECT_EQ(store.read()->name, "One");
    EXPECT_TRUE(store.journal_dirty());
    store.redo();
    EXPECT_EQ(store.read()->name, "Two");
    store.undo();
    store.save_journal();
    EXPECT_FALSE(store.journal_dirty());
    EXPECT_EQ(store.journal_sequence(), 5u);
  }
  {
    store_type store{ledger{}};
    store.recover_journal(path_);
    EXPECT_FALSE(store.journal_dirty());
    store.redo();
    EXPECT_EQ(store.read()->name, "Two");
    store.undo();
    EXPECT_FALSE(store.journal_dirty());
    store.undo();
    EXPECT_EQ(store.read()->name, "Base");
    EXPECT_TRUE(store.journal_dirty());
  }
}

// Tree selection, sibling branches, and optional labels remain complete after journal compaction.
TEST_P(managed_journal_test, labeled_tree_branches_and_reset) {
  using tree_store =
      managed::model_store<ledger, managed::history_mode::tree, managed::history_labels::enabled>;
  std::uint64_t first{};
  std::uint64_t second{};
  {
    tree_store store{ledger{"Base"}};
    store.create_journal(path_, GetParam());
    first =
        store.execute_transaction("First", [](auto& tx) { tx.root().set_name("One"); }).revision;
    store.undo();
    second =
        store.execute_transaction("Second", [](auto& tx) { tx.root().set_name("Two"); }).revision;
    store.checkout(first);
    EXPECT_EQ(store.undo_label(), "First");
    store.save_journal();
  }
  {
    tree_store store{ledger{}};
    store.recover_journal(path_);
    store.undo();
    EXPECT_EQ(store.redo_children(), (std::vector<std::uint64_t>{first, second}));
    EXPECT_EQ(store.redo_label(second), "Second");
    store.redo(second);
    store.reset_history();
  }
  tree_store store{ledger{}};
  store.recover_journal(path_);
  EXPECT_EQ(store.read()->name, "Two");
  EXPECT_THROW(store.undo(), std::out_of_range);
  EXPECT_TRUE(store.journal_dirty());
}

// Allocation reservations outlive cancellation and deletion even with history disabled.
TEST_P(managed_journal_test, durable_reservations_without_history) {
  using identity_store = managed::model_store<ledger, managed::history_mode::disabled>;
  {
    identity_store store{ledger{"Base"}};
    store.create_journal(path_, GetParam());
    const auto canceled = store.execute_transaction([](auto& tx) {
      EXPECT_EQ(tx.root().entries().insert(10, {"Canceled", 1}), 2u);
      tx.revert();
    });
    EXPECT_EQ(canceled.status, status::reverted);
    EXPECT_FALSE(store.journal_dirty());
    EXPECT_EQ(store.journal_sequence(), 1u);
  }
  {
    identity_store store{ledger{}};
    store.recover_journal(path_);
    EXPECT_FALSE(store.journal_dirty());
    store
        .execute_transaction(
            [](auto& tx) { EXPECT_EQ(tx.root().entries().insert(10, {"Kept", 2}), 3u); })
        .throw_if_failed();
    store.execute_transaction([](auto& tx) { tx.root().entries().erase(3); }).throw_if_failed();
    store.save_journal();
  }
  identity_store store{ledger{}};
  store.recover_journal(path_);
  store
      .execute_transaction(
          [](auto& tx) { EXPECT_EQ(tx.root().entries().insert(10, {"Next", 3}), 4u); })
      .throw_if_failed();
}

// Every incomplete suffix length is ignored; every fully framed operation is recovered exactly once.
TEST_P(managed_journal_test, every_truncated_final_record_and_repeat_recovery) {
  std::size_t base_size{};
  std::filesystem::path physical;
  {
    store_type store{ledger{"Base"}};
    store.create_journal(path_, GetParam());
    physical = append_path();
    base_size = static_cast<std::size_t>(std::filesystem::file_size(physical));
    rename(store, "Changed").throw_if_failed();
  }
  const auto complete = read_bytes(physical);
  for (auto size = base_size; size <= complete.size(); ++size) {
    SCOPED_TRACE(size);
    write_bytes(physical,
                bytes{complete.begin(), complete.begin() + static_cast<std::ptrdiff_t>(size)});
    {
      store_type store{ledger{}};
      store.recover_journal(path_);
      EXPECT_EQ(store.read()->name, size == complete.size() ? "Changed" : "Base");
      EXPECT_EQ(store.journal_sequence(), size == complete.size() ? 1u : 0u);
    }
    EXPECT_EQ(std::filesystem::file_size(physical), size == complete.size() ? size : base_size);
  }
  store_type store{ledger{}};
  store.recover_journal(path_);
  EXPECT_EQ(store.journal_sequence(), 1u);
  rename(store, "After recovery").throw_if_failed();
  EXPECT_EQ(store.journal_sequence(), 2u);
}

// Damage in a complete header, payload, or commit marker fails instead of silently rolling back.
TEST_P(managed_journal_test, complete_corruption_and_sequence_gaps_fail_atomically) {
  std::size_t base_size{};
  std::size_t first_end{};
  std::filesystem::path physical;
  {
    store_type store{ledger{"Base"}};
    store.create_journal(path_, GetParam());
    physical = append_path();
    base_size = static_cast<std::size_t>(std::filesystem::file_size(physical));
    rename(store, "One").throw_if_failed();
    first_end = static_cast<std::size_t>(std::filesystem::file_size(physical));
    rename(store, "Two").throw_if_failed();
  }
  const auto complete = read_bytes(physical);
  for (const auto offset :
       {base_size, base_size + frame_header_bytes, first_end - 1, complete.size() - 1}) {
    auto damaged = complete;
    damaged[offset] ^= 1;
    write_bytes(physical, damaged);
    store_type store{ledger{"Unchanged"}};
    const auto before = store.save();
    EXPECT_THROW(store.recover_journal(path_), std::invalid_argument);
    EXPECT_EQ(store.save(), before);
    EXPECT_EQ(read_bytes(physical), damaged);
  }
  auto missing = complete;
  missing.erase(missing.begin() + static_cast<std::ptrdiff_t>(base_size),
                missing.begin() + static_cast<std::ptrdiff_t>(first_end));
  write_bytes(physical, missing);
  store_type store{ledger{}};
  EXPECT_THROW(store.recover_journal(path_), std::invalid_argument);
}

// Simulated disk errors preserve live state, classify uncertainty, and retain their nested cause.
TEST_P(managed_journal_test, append_faults_fence_and_recover_the_durable_decision) {
  for (const auto point : {event::before_append, event::after_header, event::after_payload,
                           event::after_commit, event::after_flush}) {
    const auto path = directory_ / std::to_string(static_cast<int>(point));
    bool armed = false;
    std::exception_ptr retained_error{};
    managed::journal_options options;
    options.fault_injector = [&](auto actual) {
      if (armed && actual == point) {
        throw std::system_error{std::make_error_code(std::errc::no_space_on_device)};
      }
    };
    {
      store_type store{ledger{"Base"}};
      store.create_journal(path, GetParam(), options);
      const auto pinned = store.read();
      armed = true;
      const auto result = rename(store, "Changed");
      retained_error = result.error;
      const bool uncertain = point != event::before_append;
      EXPECT_EQ(result.status, uncertain ? status::indeterminate : status::failed);
      if (uncertain) {
        EXPECT_THROW(result.throw_if_failed(), managed::journal_indeterminate_error);
      } else {
        EXPECT_THROW(result.throw_if_failed(), std::system_error);
      }
      EXPECT_EQ(store.journal_needs_recovery(), uncertain);
      EXPECT_EQ(store.read(), pinned);
      EXPECT_EQ(store.journal_sequence(), 0u);
      if (uncertain) {
        EXPECT_THROW(store.save_journal(), managed::journal_indeterminate_error);
        EXPECT_EQ(rename(store, "Blocked").status, status::indeterminate);
      }
    }
    if (point != event::before_append) {
      expect_retained_journal_cause(retained_error);
    }
    store_type recovered{ledger{}};
    recovered.recover_journal(path);
    const bool committed = point == event::after_commit || point == event::after_flush;
    EXPECT_EQ(recovered.read()->name, committed ? "Changed" : "Base");
    EXPECT_EQ(recovered.journal_sequence(), committed ? 1u : 0u);
    rename(recovered, "Next").throw_if_failed();
  }
}

// Save faults select a complete old or new generation; cleanup never changes the committed outcome.
TEST_P(managed_journal_test, save_faults_keep_matching_generations_and_dirty_baseline) {
  for (const auto point :
       {event::after_sidecar_flush, event::after_base_flush, event::before_replace,
        event::after_replace, event::after_publish, event::before_cleanup}) {
    if (point == event::after_sidecar_flush && GetParam() == mode::appended) {
      continue;
    }
    const auto path = directory_ / std::to_string(static_cast<int>(point));
    bool armed = false;
    managed::journal_options options;
    options.fault_injector = [&](auto actual) {
      if (armed && point == actual) {
        throw std::runtime_error{"Injected Save interruption"};
      }
    };
    {
      store_type store{ledger{"Base"}};
      store.create_journal(path, GetParam(), options);
      rename(store, "Changed").throw_if_failed();
      armed = true;
      if (point == event::before_cleanup) {
        EXPECT_NO_THROW(store.save_journal());
        EXPECT_FALSE(store.journal_dirty());
      } else {
        EXPECT_ANY_THROW(store.save_journal());
        EXPECT_TRUE(store.journal_dirty());
      }
      EXPECT_EQ(store.read()->name, "Changed");
      EXPECT_EQ(store.journal_needs_recovery(),
                point == event::after_replace || point == event::after_publish);
    }
    store_type recovered{ledger{}};
    recovered.recover_journal(path);
    EXPECT_EQ(recovered.read()->name, "Changed");
    EXPECT_EQ(recovered.journal_sequence(), 1u);
    const bool saved = point == event::after_replace || point == event::after_publish ||
                       point == event::before_cleanup;
    EXPECT_EQ(recovered.journal_dirty(), !saved);
    recovered.undo();
    EXPECT_EQ(recovered.read()->name, "Base");
    recovered.redo();
    EXPECT_EQ(recovered.read()->name, "Changed");
  }
}

// Store APIs cannot bypass journaling, and independent writers cannot acquire the same document.
TEST_P(managed_journal_test, exclusive_writer_noop_cancel_and_memory_load_guard) {
  store_type first{ledger{"Base"}};
  const auto memory = first.save();
  first.create_journal(path_, GetParam());
  store_type second{ledger{"Other"}};
  EXPECT_ANY_THROW(second.recover_journal(path_));
  EXPECT_ANY_THROW(second.create_journal(path_, GetParam()));
  EXPECT_THROW(first.load(memory), std::logic_error);
  EXPECT_EQ(rename(first, "Base").status, status::no_change);
  auto canceled = first.execute_transaction([](auto& tx) {
    tx.root().set_name("Discard");
    tx.revert();
  });
  EXPECT_EQ(canceled.status, status::reverted);
  EXPECT_EQ(first.journal_sequence(), 0u);
  EXPECT_FALSE(first.journal_dirty());
}

// Budget errors happen before durable publication and leave the writer usable for a full Save.
TEST_P(managed_journal_test, bounded_append_and_recovery) {
  {
    store_type store{ledger{"Base"}};
    managed::journal_options options;
    const auto baseline_bytes = store.save().size();
    options.max_file_bytes =
        file_header_bytes + frame_header_bytes + frame_footer_bytes + baseline_bytes;
    options.max_record_bytes = baseline_bytes;
    store.create_journal(path_, GetParam(), options);
    const std::string oversized(baseline_bytes * 2, 'x');
    const auto result = rename(store, oversized.c_str());
    EXPECT_EQ(result.status, status::failed);
    EXPECT_THROW(result.throw_if_failed(), std::length_error);
    EXPECT_EQ(store.read()->name, "Base");
    EXPECT_FALSE(store.journal_needs_recovery());
    store.save_journal();
  }
  managed::journal_options options;
  options.max_record_bytes = 1;
  store_type store{ledger{"Untouched"}};
  EXPECT_THROW(store.recover_journal(path_, options), std::length_error);
  EXPECT_EQ(store.read()->name, "Untouched");
}

// Reentrant storage hooks cannot publish or mutate the active store during durability work.
TEST_P(managed_journal_test, reentrant_hook_is_rejected) {
  store_type store{ledger{"Base"}};
  bool armed = false;
  managed::journal_options options;
  options.fault_injector = [&](auto point) {
    if (armed && point == event::before_append) {
      EXPECT_THROW(store.save_journal(), std::logic_error);
      EXPECT_EQ(rename(store, "Nested").status, status::failed);
    }
  };
  store.create_journal(path_, GetParam(), options);
  armed = true;
  rename(store, "Outer").throw_if_failed();
  EXPECT_EQ(store.read()->name, "Outer");
}

// Missing or substituted sidecars cannot be guessed from names or attached to a different base.
TEST_P(managed_journal_test, sidecar_binding_and_missing_dependency) {
  if (GetParam() != mode::sidecar) {
    GTEST_SKIP() << "Sidecar-specific binding";
  }
  {
    store_type store{ledger{"Base"}};
    store.create_journal(path_, GetParam());
    rename(store, "Changed").throw_if_failed();
  }
  const auto physical = append_path();
  const auto original = read_bytes(physical);
  const auto other_path = directory_ / "other";
  {
    store_type other{ledger{"Other"}};
    other.create_journal(other_path, mode::sidecar);
  }
  for (const auto& item : std::filesystem::directory_iterator{directory_}) {
    if (item.path().filename().string().starts_with("other.journal-")) {
      write_bytes(physical, read_bytes(item.path()));
      break;
    }
  }
  {
    store_type store{ledger{}};
    EXPECT_THROW(store.recover_journal(path_), std::invalid_argument);
  }
  std::filesystem::remove(physical);
  {
    store_type store{ledger{}};
    EXPECT_THROW(store.recover_journal(path_), std::system_error);
  }
  write_bytes(physical, original);
  store_type store{ledger{}};
  store.recover_journal(path_);
  EXPECT_EQ(store.read()->name, "Changed");
}

// Failed initial publication must fence the store even before its adapter has been returned.
TEST_P(managed_journal_test, indeterminate_creation_fences_unattached_store) {
  {
    store_type store{ledger{"Base"}};
    managed::journal_options options;
    options.fault_injector = [](auto point) {
      if (point == event::after_publish) {
        throw std::runtime_error{"Crash before adapter attachment"};
      }
    };
    EXPECT_THROW(store.create_journal(path_, GetParam(), options),
                 managed::journal_indeterminate_error);
    EXPECT_TRUE(store.journal_needs_recovery());
    EXPECT_EQ(rename(store, "Blocked").status, status::indeterminate);
    EXPECT_EQ(store.read()->name, "Base");
  }
  store_type store{ledger{}};
  store.recover_journal(path_);
  EXPECT_FALSE(store.journal_dirty());
  EXPECT_EQ(store.read()->name, "Base");
}

// Manual and destructor-driven commits preserve the same uncertain completion as callback commits.
TEST_P(managed_journal_test, manual_and_scope_completion_report_indeterminate) {
  for (const bool explicit_commit : {false, true}) {
    const auto path = directory_ / (explicit_commit ? "manual" : "scoped");
    bool armed = false;
    managed::journal_options options;
    options.fault_injector = [&](auto point) {
      if (armed && point == event::after_flush) {
        throw std::runtime_error{"Durable but unacknowledged"};
      }
    };
    {
      store_type store{ledger{"Base"}};
      store.create_journal(path, GetParam(), options);
      armed = true;
      managed::transaction_outcome outcome;
      {
        auto tx = store.begin_transaction(outcome);
        tx.root().set_name("Changed");
        if (explicit_commit) {
          EXPECT_THROW(tx.commit(), managed::journal_indeterminate_error);
        }
      }
      EXPECT_EQ(outcome.status, status::indeterminate);
      EXPECT_EQ(store.read()->name, "Base");
    }
    store_type store{ledger{}};
    store.recover_journal(path);
    EXPECT_EQ(store.read()->name, "Changed");
  }
}

// Model validation must precede repairing the file, leaving both file and live receiver unchanged.
TEST_P(managed_journal_test, rejected_recovery_does_not_repair_tail) {
  std::filesystem::path physical;
  {
    store_type store{ledger{"Base"}};
    store.create_journal(path_, GetParam());
    physical = append_path();
    rename(store, "Changed").throw_if_failed();
  }
  auto incomplete = read_bytes(physical);
  incomplete.pop_back();
  write_bytes(physical, incomplete);
  store_type store{ledger{"Untouched"}, managed::make_document_id(), {}, [](const auto& value) {
                     if (value.name == "Base") {
                       throw std::invalid_argument{"Application rejects restored value"};
                     }
                   }};
  EXPECT_THROW(store.recover_journal(path_), std::invalid_argument);
  EXPECT_EQ(store.read()->name, "Untouched");
  EXPECT_EQ(read_bytes(physical), incomplete);
}

// A same-document live receiver must not discard IDs issued after the recoverable baseline.
TEST_P(managed_journal_test, recovery_rejects_lost_live_reservations) {
  bytes memory;
  {
    store_type store{ledger{"Base"}};
    memory = store.save();
    store.create_journal(path_, GetParam());
  }
  store_type store{ledger{}};
  store.load(memory);
  store
      .execute_transaction([](auto& tx) {
        tx.create_id();
        tx.revert();
      })
      .throw_if_failed();
  EXPECT_THROW(store.recover_journal(path_), std::invalid_argument);
  store.execute_transaction([](auto& tx) { EXPECT_EQ(tx.create_id(), 3u); }).throw_if_failed();
}

// Allocator exhaustion remains permanent through recovery, without overflow or ID reuse.
TEST_P(managed_journal_test, last_uint32_reservation_survives_cancellation) {
  {
    store_type store{ledger{"Base"}};
    auto envelope =
        managed::detail::decode<managed::records::unlabeled_state_envelope>(store.save(), {});
    envelope.allocated_id = std::numeric_limits<std::uint32_t>::max() - 1;
    store.load(managed::detail::encode(envelope));
    store.create_journal(path_, GetParam());
    store
        .execute_transaction([](auto& tx) {
          EXPECT_EQ(tx.create_id(), std::numeric_limits<std::uint32_t>::max());
          tx.revert();
        })
        .throw_if_failed();
  }
  store_type store{ledger{}};
  store.recover_journal(path_);
  const auto failed = store.execute_transaction([](auto& tx) { tx.create_id(); });
  EXPECT_THROW(failed.throw_if_failed(), std::overflow_error);
  EXPECT_EQ(failed.status, status::failed);
  EXPECT_EQ(store.journal_sequence(), 1u);
}

// Label-enabled deque pruning must persist its final cursor, labels, and surviving undo states.
TEST_P(managed_journal_test, labeled_linear_eviction_and_redo_replacement) {
  using labeled_store =
      managed::model_store<ledger, managed::history_mode::linear, managed::history_labels::enabled>;
  managed::store_options options;
  options.max_revisions = 3;
  {
    labeled_store store{ledger{"Base"}, managed::make_document_id(), options};
    store.create_journal(path_, GetParam());
    store.execute_transaction("One", [](auto& tx) { tx.root().set_name("One"); }).throw_if_failed();
    store.execute_transaction("Two", [](auto& tx) { tx.root().set_name("Two"); }).throw_if_failed();
    store.execute_transaction("Three", [](auto& tx) { tx.root().set_name("Three"); })
        .throw_if_failed();
    store.undo();
    store.execute_transaction("Four", [](auto& tx) { tx.root().set_name("Four"); })
        .throw_if_failed();
  }
  labeled_store store{ledger{}, managed::make_document_id(), options};
  store.recover_journal(path_);
  EXPECT_EQ(store.undo_label(), "Four");
  EXPECT_THROW(store.redo(), std::out_of_range);
  store.undo();
  EXPECT_EQ(store.read()->name, "Two");
  EXPECT_EQ(store.redo_label(), "Four");
  store.undo();
  EXPECT_EQ(store.read()->name, "One");
  EXPECT_THROW(store.undo(), std::out_of_range);
}

// Unlabeled tree envelopes use their distinct wire type while preserving sibling branches.
TEST_P(managed_journal_test, unlabeled_tree_recovery) {
  using tree_store = managed::model_store<ledger, managed::history_mode::tree>;
  {
    tree_store store{ledger{"Base"}};
    store.create_journal(path_, GetParam());
    rename(store, "One").throw_if_failed();
    store.undo();
    rename(store, "Two").throw_if_failed();
    store.undo();
  }
  tree_store store{ledger{}};
  store.recover_journal(path_);
  const auto children = store.redo_children();
  ASSERT_EQ(children.size(), 2u);
  store.redo(children.front());
  EXPECT_EQ(store.read()->name, "One");
  store.checkout(children.back());
  EXPECT_EQ(store.read()->name, "Two");
}

// Each edit writes the existing snapshot once, with constant framing independent of retained history.
TEST_P(managed_journal_test, append_size_is_snapshot_plus_constant_metadata) {
  store_type store{ledger{"Base"}};
  store.create_journal(path_, GetParam());
  const auto physical = append_path();
  constexpr std::size_t edit_metadata_bytes = 1 + 8 + 8; // Kind, evictions, allocation boundary.
  constexpr std::size_t control_bytes = 1 + 8;           // Kind plus selection/reservation.
  for (int index = 0; index < 100; ++index) {
    const auto before = std::filesystem::file_size(physical);
    const auto name = "Snapshot " + std::to_string(index);
    rename(store, name.c_str()).throw_if_failed();
    const auto snapshot = managed::detail::encode(*store.read());
    EXPECT_EQ(std::filesystem::file_size(physical) - before,
              snapshot.size() + edit_metadata_bytes + frame_header_bytes + frame_footer_bytes);
  }
  auto before = std::filesystem::file_size(physical);
  store.undo();
  EXPECT_EQ(std::filesystem::file_size(physical) - before,
            control_bytes + frame_header_bytes + frame_footer_bytes);
  before = std::filesystem::file_size(physical);
  store
      .execute_transaction([](auto& tx) {
        tx.create_id();
        tx.revert();
      })
      .throw_if_failed();
  EXPECT_EQ(std::filesystem::file_size(physical) - before,
            control_bytes + frame_header_bytes + frame_footer_bytes);
}

INSTANTIATE_TEST_SUITE_P(storage_modes, managed_journal_test,
                         testing::Values(mode::appended, mode::sidecar));
} // namespace
