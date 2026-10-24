// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <ledger.hpp>
#include <rohit/native_database.hpp>

#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <thread>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using database = managed::native_database<ledger>;
// Model a distinct destination schema binding while retaining this fixture's exact entity types.
struct migrated_ledger_traits : managed::model_traits<ledger> {
  static constexpr std::string_view schema_id{"ledger-native-migration-v2"};
};
constexpr managed::document_id document_namespace{17, 23};
constexpr std::uint64_t epoch = 42;
constexpr std::array<std::uint8_t, 3> request{1, 2, 3};

// Keep native artifacts under a unique operating-system temporary directory, removed by RAII.
class database_directory {
  std::filesystem::path directory_;
public:
  // Create a path independent of the CTest working directory and other concurrently running tests.
  database_directory() {
    const auto identity = managed::make_document_id();
    directory_ = std::filesystem::temp_directory_path() /
        ("serializer-native-db-" + std::to_string(identity.high) + "-" +
         std::to_string(identity.low));
    if (!std::filesystem::create_directory(directory_)) {
      throw std::runtime_error{"Cannot create database test directory"};
    }
  }
  // Remove only this exact, task-created temporary child after writer resources have been released.
  ~database_directory() {
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }
  // Return the single document path owned by this fixture.
  std::filesystem::path path() const { return directory_ / "state.sdb"; }
};

// Set an ordinary generated managed entry through the checked transactional editor.
void edit_amount(auto& transaction) {
  transaction.root().entries().edit(2).set_amount_minor_units(200);
}

// Forge a fully checksummed transport frame to exercise semantic recovery validation independently.
void append_envelope(const std::filesystem::path& path, const std::vector<std::uint8_t>& envelope) {
  constexpr std::uint64_t record_version = 1;
  constexpr std::uint64_t previous_revision = 1;
  constexpr std::uint64_t forged_revision = previous_revision + 1;
  std::vector<std::uint8_t> record;
  for (const auto word : {record_version, epoch, std::uint64_t{1}, std::uint64_t{2},
                          previous_revision, forged_revision, static_cast<std::uint64_t>(envelope.size())}) {
    managed::detail::database_put_word(record, word);
  }
  const auto digest = rohit::make_digest<rohit::digest_algorithm::sha256>(request);
  record.insert(record.end(), digest.begin(), digest.end());
  record.insert(record.end(), envelope.begin(), envelope.end());
  managed::detail::native_database_file transport{path, false, {}};
  static_cast<void>(transport.recover());
  transport.finish_recovery();
  transport.append(record);
}
} // namespace

// Accepted root data, host records and identity allocation share exactly one durable outcome.
TEST(native_database, prepare_accept_reopen_and_exact_retry) {
  database_directory directory;
  managed::database_outcome accepted;
  std::uint32_t original_id{};
  {
    database store{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace, epoch};
    const auto original = store.read();
    original_id = original->entries.at(10).persistent_id;
    auto prepared = store.prepare({7, 1}, epoch, 0, request, [](auto& transaction) {
      edit_amount(transaction);
      transaction.root().entries().insert(20, {"Outbox", 5});
    });
    EXPECT_EQ(store.revision(), 0u);
    EXPECT_EQ(store.read()->entries.size(), 1u);
    accepted = store.accept(std::move(prepared));
    EXPECT_EQ(accepted.revision, 1u);
    EXPECT_EQ(store.read()->entries.at(10).amount_minor_units, 200);
    EXPECT_EQ(store.read()->entries.at(20).persistent_id, 3u);
    EXPECT_EQ(original->entries.at(10).amount_minor_units, 100);
    EXPECT_EQ(original->entries.size(), 1u);
    bool called{};
    auto retry = store.prepare({7, 1}, epoch, 0, request, [&](auto&) { called = true; });
    EXPECT_TRUE(retry.is_retry());
    EXPECT_FALSE(called);
    EXPECT_EQ(store.accept(std::move(retry)), accepted);
  }
  database reopened{directory.path()};
  EXPECT_EQ(reopened.document(), document_namespace);
  EXPECT_EQ(reopened.epoch(), epoch);
  EXPECT_EQ(reopened.read()->entries.at(10).persistent_id, original_id);
  EXPECT_EQ(reopened.read()->entries.at(20).persistent_id, 3u);
  bool called{};
  auto retry = reopened.prepare({7, 1}, epoch, 0, request, [&](auto&) { called = true; });
  EXPECT_EQ(reopened.accept(std::move(retry)), accepted);
  EXPECT_FALSE(called);
  EXPECT_EQ(reopened.revision(), 1u);
}

// Detailed changes and pinned roots are fully prepared before durability and survive publication.
TEST(native_database, prepared_changes_pin_exact_source_and_candidate_before_acceptance) {
  database_directory directory;
  managed::database_options options;
  options.notifications.field_paths = true;
  database store{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}},
                 document_namespace, epoch, options};
  const auto bytes_before = std::filesystem::file_size(directory.path());
  auto prepared = store.prepare({7, 9}, epoch, 0, request, [](auto& transaction) {
    edit_amount(transaction);
    transaction.root().entries().insert(20, {"Outbox", 5});
  });
  const auto before = prepared.before();
  const auto after = prepared.after();
  const auto& changes = prepared.changes();
  EXPECT_EQ(std::filesystem::file_size(directory.path()), bytes_before);
  EXPECT_TRUE(changes.field_paths_precise);
  EXPECT_FALSE(changes.namespace_replaced);
  EXPECT_EQ(changes.created_ids, std::vector<std::uint32_t>{3});
  EXPECT_TRUE(changes.deleted_ids.empty());
  EXPECT_NE(std::find(changes.changed_ids.begin(), changes.changed_ids.end(), 2U),
            changes.changed_ids.end());
  EXPECT_FALSE(changes.changed_fields.empty());
  EXPECT_EQ(before->entries.at(10).amount_minor_units, 100);
  EXPECT_EQ(after->entries.at(10).amount_minor_units, 200);
  store.accept(std::move(prepared));
  EXPECT_EQ(prepared.before(), before);
  EXPECT_EQ(prepared.after(), after);
  EXPECT_EQ(store.read(), after);
  auto retry = store.prepare({7, 9}, epoch, 0, request, [](auto&) { FAIL(); });
  EXPECT_TRUE(retry.is_retry());
  EXPECT_FALSE(retry.before());
  EXPECT_FALSE(retry.after());
  EXPECT_TRUE(retry.changes().changed_ids.empty());
}

// Feed admission is transactional: a rejected metadata budget cannot append or advance the root.
TEST(native_database, prepared_change_budget_failure_preserves_durable_base) {
  database_directory directory;
  managed::database_options options;
  options.notifications.max_entity_changes = 0;
  database store{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}},
                 document_namespace, epoch, options};
  const auto before = store.read();
  const auto bytes_before = std::filesystem::file_size(directory.path());
  EXPECT_THROW(store.prepare({7, 9}, epoch, 0, request, [](auto& transaction) {
    edit_amount(transaction);
  }), std::length_error);
  EXPECT_EQ(store.read(), before);
  EXPECT_EQ(store.revision(), 0U);
  EXPECT_EQ(std::filesystem::file_size(directory.path()), bytes_before);
  EXPECT_FALSE(store.needs_recovery());
}

// Rebaseline changes epoch explicitly, keeps old pins/outcomes and transfers the one writer lock.
TEST(native_database, native_rebaseline_retains_previous_generation_and_retires_source_writer) {
  database_directory directory;
  auto archive = directory.path();
  archive += ".epoch-42";
  const auto identity = [](const auto& value) { return value; };
  {
    database source{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace, epoch};
    auto accepted = source.prepare({7, 1}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
    source.accept(std::move(accepted));
    const auto original = source.read();
    auto migrated = source.migrate(epoch + 1, archive, [](const auto& value) {
      auto converted = value;
      converted.name = "Migrated";
      return converted;
    });
    EXPECT_TRUE(source.retired());
    EXPECT_FALSE(migrated->retired());
    EXPECT_EQ(source.read(), original);
    EXPECT_EQ(migrated->read()->name, "Migrated");
    EXPECT_EQ(migrated->read()->entries.at(10).persistent_id, original->entries.at(10).persistent_id);
    EXPECT_EQ(migrated->document(), document_namespace);
    EXPECT_EQ(migrated->epoch(), epoch + 1);
    EXPECT_EQ(migrated->revision(), 0U);
    EXPECT_THROW(source.prepare({7, 2}, epoch, 1, request, [](auto&) {}), std::logic_error);
    EXPECT_THROW(migrated->prepare({7, 1}, epoch, 0, request, [](auto&) {}), std::invalid_argument);
    EXPECT_THROW(migrated->migrate(epoch, directory.path(), identity), std::invalid_argument);
    EXPECT_THROW(database(directory.path()), std::exception);
  }
  database reopened{directory.path()};
  EXPECT_EQ(reopened.epoch(), epoch + 1);
  EXPECT_EQ(reopened.read()->name, "Migrated");
  database previous{archive};
  EXPECT_EQ(previous.epoch(), epoch);
  EXPECT_EQ(previous.revision(), 1U);
  bool called{};
  auto retry = previous.prepare({7, 1}, epoch, 0, request, [&](auto&) { called = true; });
  EXPECT_TRUE(retry.is_retry());
  EXPECT_FALSE(called);
  EXPECT_EQ(previous.accept(std::move(retry)).revision, 1U);
}

// Native envelopes select the new schema explicitly; old compilers cannot silently reopen it.
TEST(native_database, native_schema_migration_binds_destination_and_keeps_original_schema_archive) {
  database_directory directory;
  auto archive = directory.path();
  archive += ".old-schema";
  using destination_database = managed::native_database<ledger, migrated_ledger_traits>;
  {
    database source{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace, epoch};
    const auto migrated = source.migrate<ledger, migrated_ledger_traits>(epoch + 1, archive,
        [](const auto& value) {
          auto converted = value;
          converted.name = "New schema";
          return converted;
        });
    EXPECT_EQ(migrated->read()->name, "New schema");
  }
  EXPECT_THROW(database(directory.path()), std::invalid_argument);
  destination_database reopened{directory.path()};
  EXPECT_EQ(reopened.read()->name, "New schema");
  EXPECT_EQ(reopened.epoch(), epoch + 1);
  database original{archive};
  EXPECT_EQ(original.read()->name, "Ledger");
}

// Restore supplies externally known allocation reservations so newer exposed IDs cannot be recycled.
TEST(native_database, native_restore_raises_watermark_before_exposing_new_identifiers) {
  database_directory directory;
  auto archive = directory.path();
  archive += ".before-restore";
  constexpr std::uint64_t restored_watermark = 100;
  {
    database source{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace, epoch};
    const auto pinned = source.snapshot();
    EXPECT_EQ(pinned.allocated_id(), 2U);
    bool called{};
    constexpr auto excessive_watermark =
        static_cast<std::uint64_t>(std::numeric_limits<database::store_type::id_type>::max()) + 1U;
    EXPECT_THROW(source.migrate(epoch + 1, archive, [&](const auto& value) {
      called = true;
      return value;
    }, {}, {.minimum_allocated_id = excessive_watermark}), std::invalid_argument);
    EXPECT_FALSE(called);
    EXPECT_FALSE(std::filesystem::exists(archive));
    auto restored = source.migrate(epoch + 1, archive, [](const auto& value) { return value; },
        {}, {.minimum_allocated_id = restored_watermark});
    EXPECT_EQ(restored->allocated_id(), restored_watermark);
    EXPECT_EQ(restored->snapshot().allocated_id(), restored_watermark);
    EXPECT_EQ(pinned.allocated_id(), 2U);
    auto prepared = restored->prepare({9, 1}, epoch + 1, 0, request, [](auto& edit) {
      edit.root().entries().insert(20, {"After restore", 1});
    });
    restored->accept(std::move(prepared));
    EXPECT_EQ(restored->read()->entries.at(20).persistent_id, restored_watermark + 1);
  }
  database reopened{directory.path()};
  EXPECT_EQ(reopened.allocated_id(), restored_watermark + 1);
  database original{archive};
  EXPECT_EQ(original.allocated_id(), 2U);
}

// Invalid conversions and pre-replacement failures leave original authority and durable bytes usable.
TEST(native_database, native_migration_rejects_identity_changes_and_preserves_old_generation_on_fault) {
  for (const auto boundary : {managed::database_io_event::after_rebaseline_archive,
                              managed::database_io_event::before_rebaseline_replace}) {
    database_directory directory;
    auto archive = directory.path();
    archive += ".old";
    managed::database_options options;
    options.fault_injector = [boundary](auto event) {
      if (event == boundary) { throw std::runtime_error{"Injected rebaseline failure"}; }
    };
    database source{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}},
                    document_namespace, epoch, options};
    const auto original = source.read();
    const auto bytes_before = std::filesystem::file_size(directory.path());
    EXPECT_THROW(source.migrate(epoch + 1, archive, [](const auto& value) {
      auto invalid = value;
      invalid.entries.clear();
      return invalid;
    }), std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(archive));
    EXPECT_THROW(source.migrate(epoch + 1, archive, [](const auto& value) { return value; }),
                 std::runtime_error);
    EXPECT_FALSE(source.retired());
    EXPECT_FALSE(source.needs_recovery());
    EXPECT_EQ(source.read(), original);
    EXPECT_EQ(std::filesystem::file_size(directory.path()), bytes_before);
    database previous{archive};
    EXPECT_EQ(previous.read()->name, original->name);
    auto prepared = source.prepare({9, 1}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
    EXPECT_EQ(source.accept(std::move(prepared)).revision, 1U);
  }
}

// A replacement whose acknowledgement fails exposes no in-process new root and requires recovery.
TEST(native_database, native_migration_uncertain_publication_recovers_new_epoch_and_keeps_archive) {
  database_directory directory;
  auto archive = directory.path();
  archive += ".old";
  {
    managed::database_options options;
    options.fault_injector = [](auto event) {
      if (event == managed::database_io_event::after_rebaseline_replace) {
        throw std::runtime_error{"Injected lost migration acknowledgement"};
      }
    };
    database source{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}},
                    document_namespace, epoch, options};
    const auto original = source.read();
    EXPECT_THROW(source.migrate(epoch + 1, archive, [](const auto& value) {
      auto converted = value;
      converted.name = "Migrated";
      return converted;
    }), managed::journal_indeterminate_error);
    EXPECT_TRUE(source.needs_recovery());
    EXPECT_EQ(source.read(), original);
    EXPECT_THROW(source.prepare({9, 1}, epoch, 0, request, [](auto&) {}),
                 managed::journal_indeterminate_error);
  }
  database reopened{directory.path()};
  EXPECT_EQ(reopened.epoch(), epoch + 1);
  EXPECT_EQ(reopened.read()->name, "Migrated");
  database previous{archive};
  EXPECT_EQ(previous.epoch(), epoch);
  EXPECT_EQ(previous.read()->name, "Ledger");
}

// A retry binds the epoch, expected acceptance base and every canonical command parameter.
TEST(native_database, reject_mismatched_retry_stale_candidate_and_epoch) {
  database_directory directory;
  database store{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace, epoch};
  auto first = store.prepare({1, 1}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
  auto duplicate = store.prepare({1, 1}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
  const std::array<std::uint8_t, 1> different{4};
  auto mismatch = store.prepare({1, 1}, epoch, 0, different, [](auto& edit) { edit_amount(edit); });
  auto stale = store.prepare({1, 2}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
  store.accept(std::move(first));
  EXPECT_EQ(store.accept(std::move(duplicate)).revision, 1u);
  EXPECT_THROW(store.accept(std::move(mismatch)), std::invalid_argument);
  EXPECT_THROW(store.accept(std::move(first)), std::invalid_argument);
  EXPECT_THROW(store.accept(std::move(stale)), std::invalid_argument);
  EXPECT_THROW(store.prepare({1, 1}, epoch, 0, different, [](auto&) {}), std::invalid_argument);
  EXPECT_THROW(store.prepare({1, 1}, epoch, 1, request, [](auto&) {}), std::invalid_argument);
  EXPECT_THROW(store.prepare({1, 1}, epoch + 1, 0, request, [](auto&) {}), std::invalid_argument);
  EXPECT_THROW(store.prepare({1, 3}, epoch, 0, request, [](auto&) {}), std::invalid_argument);
  EXPECT_EQ(store.revision(), 1u);
}

// Complete candidate validation and publication-resource admission precede durable file writes.
TEST(native_database, validation_cancellation_and_prewrite_fault_leave_base_mutable) {
  database_directory directory;
  bool fail_write{};
  managed::database_options options;
  options.fault_injector = [&](auto event) {
    if (fail_write && event == managed::database_io_event::before_write) {
      throw std::runtime_error{"Injected before write"};
    }
  };
  auto invariant = [](const auto& root) {
    if (root.entries.at(10).amount_minor_units < 0) {
      throw std::invalid_argument{"Negative amount"};
    }
  };
  database store{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace,
                 epoch, options, invariant};
  const auto initial_bytes = std::filesystem::file_size(directory.path());
  EXPECT_THROW(store.prepare({1, 1}, epoch, 0, request, [](auto& edit) {
    edit.root().entries().edit(2).set_amount_minor_units(-1);
  }), std::invalid_argument);
  EXPECT_THROW(store.prepare({1, 1}, epoch, 0, request, [](auto& edit) {
    edit.revert();
  }), std::invalid_argument);
  EXPECT_EQ(std::filesystem::file_size(directory.path()), initial_bytes);
  auto prepared = store.prepare({1, 1}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
  fail_write = true;
  EXPECT_THROW(store.accept(std::move(prepared)), std::runtime_error);
  EXPECT_FALSE(store.needs_recovery());
  EXPECT_EQ(store.revision(), 0u);
  fail_write = false;
  EXPECT_EQ(store.accept(std::move(prepared)).revision, 1u);
}

// Lost acknowledgements at each issued-write boundary fence mutation until authoritative reopen.
TEST(native_database, uncertain_write_and_accepted_unpublished_recover_exactly_once) {
  for (const auto boundary : {managed::database_io_event::after_write,
                              managed::database_io_event::after_flush,
                              managed::database_io_event::before_publish}) {
    database_directory directory;
    {
      bool inject{};
      managed::database_options options;
      options.fault_injector = [&](auto event) {
        if (inject && event == boundary) {
          throw std::runtime_error{"Injected lost acknowledgement"};
        }
      };
      database store{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace,
                     epoch, options};
      auto prepared = store.prepare({3, 8}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
      inject = true;
      EXPECT_THROW(store.accept(std::move(prepared)), std::runtime_error);
      EXPECT_TRUE(store.needs_recovery());
      EXPECT_EQ(store.revision(), 0u);
      EXPECT_EQ(store.read()->entries.at(10).amount_minor_units, 100);
      EXPECT_THROW(store.prepare({3, 9}, epoch, 0, request, [](auto&) {}),
                   managed::journal_indeterminate_error);
    }
    database reopened{directory.path()};
    EXPECT_EQ(reopened.revision(), 1u);
    EXPECT_EQ(reopened.read()->entries.at(10).amount_minor_units, 200);
    bool called{};
    auto retry = reopened.prepare({3, 8}, epoch, 0, request, [&](auto&) { called = true; });
    EXPECT_FALSE(called);
    EXPECT_EQ(reopened.accept(std::move(retry)).revision, 1u);
  }
}

// Exclusive ownership covers every engine and validates corruption before recovery tail repair.
TEST(native_database, writer_lock_truncated_tail_repair_and_corruption_rejection) {
  database_directory directory;
  std::uintmax_t committed_bytes{};
  {
    database store{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace, epoch};
    EXPECT_THROW(database second{directory.path()}, std::exception);
    auto prepared = store.prepare({9, 1}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
    store.accept(std::move(prepared));
    committed_bytes = std::filesystem::file_size(directory.path());
  }
  {
    std::ofstream append{directory.path(), std::ios::binary | std::ios::app};
    append.write("partial", 7);
  }
  {
    database reopened{directory.path()};
    EXPECT_EQ(reopened.revision(), 1u);
    EXPECT_EQ(std::filesystem::file_size(directory.path()), committed_bytes);
  }
  {
    std::fstream corrupt{directory.path(), std::ios::binary | std::ios::in | std::ios::out};
    corrupt.seekp(24);
    corrupt.put('\xff');
  }
  const auto corrupt_bytes = std::filesystem::file_size(directory.path());
  EXPECT_THROW(database reopened{directory.path()}, std::invalid_argument);
  EXPECT_EQ(std::filesystem::file_size(directory.path()), corrupt_bytes);
}

// Retention fails closed rather than forgetting a committed host operation and executing it twice.
TEST(native_database, bounded_retention_and_request_admission) {
  database_directory directory;
  managed::database_options options;
  options.max_operations = 1;
  options.max_request_bytes = request.size();
  database store{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace,
                 epoch, options};
  const std::array<std::uint8_t, 4> oversized_request{1, 2, 3, 4};
  EXPECT_THROW(store.prepare({1, 1}, epoch, 0, oversized_request, [](auto&) {}),
               std::length_error);
  auto prepared = store.prepare({1, 1}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
  store.accept(std::move(prepared));
  EXPECT_THROW(store.prepare({1, 2}, epoch, 1, request, [](auto&) {}), std::length_error);
  bool called{};
  auto retry = store.prepare({1, 1}, epoch, 0, request, [&](auto&) { called = true; });
  EXPECT_EQ(store.accept(std::move(retry)).revision, 1u);
  EXPECT_FALSE(called);
}

// File-capacity exhaustion is a known rejection before writes, not an uncertain accepted outcome.
TEST(native_database, file_limit_rejects_before_write_without_fencing) {
  database_directory directory;
  {
    database initial{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace, epoch};
  }
  const auto baseline_bytes = std::filesystem::file_size(directory.path());
  managed::database_options options;
  options.max_record_bytes = static_cast<std::size_t>(baseline_bytes);
  options.max_file_bytes = static_cast<std::size_t>(baseline_bytes);
  database store{directory.path(), options};
  auto prepared = store.prepare({1, 1}, epoch, 0, request, [](auto& edit) { edit_amount(edit); });
  EXPECT_THROW(store.accept(std::move(prepared)), std::length_error);
  EXPECT_FALSE(store.needs_recovery());
  EXPECT_EQ(store.revision(), 0u);
  EXPECT_EQ(std::filesystem::file_size(directory.path()), baseline_bytes);
}

// The exclusive preparation token cannot cross owners, outlive consumption or retain moved state.
TEST(native_database, preparation_ownership_thread_and_callback_barriers) {
  database_directory first_directory;
  database_directory second_directory;
  database* active{};
  bool check_fault{};
  managed::database_options options;
  options.fault_injector = [&](auto event) {
    if (check_fault && event == managed::database_io_event::before_write) {
      EXPECT_THROW(active->prepare({90, 1}, epoch, 0, request, [](auto&) {}), std::logic_error);
      EXPECT_EQ(active->revision(), 0u);
    }
  };
  database first{first_directory.path(), ledger{"Ledger", {{10, {"One", 100}}}}, document_namespace,
                 epoch, options};
  active = &first;
  database second{second_directory.path(), ledger{"Other", {{10, {"Other", 100}}}},
                  managed::document_id{18, 24}, epoch};
  auto prepared = first.prepare({1, 1}, epoch, 0, request, [&](auto& edit) {
    EXPECT_THROW(first.prepare({90, 2}, epoch, 0, request, [](auto&) {}), std::logic_error);
    edit_amount(edit);
  });
  EXPECT_THROW(second.accept(std::move(prepared)), std::invalid_argument);
  auto moved = std::move(prepared);
  EXPECT_THROW(first.accept(std::move(prepared)), std::invalid_argument);
  check_fault = true;
  EXPECT_EQ(first.accept(std::move(moved)).revision, 1u);
  EXPECT_THROW(first.accept(std::move(moved)), std::invalid_argument);
  bool rejected{};
  std::thread other{[&] {
    try {
      static_cast<void>(first.read());
    } catch (const std::logic_error&) {
      rejected = true;
    }
  }};
  other.join();
  EXPECT_TRUE(rejected);
  auto retry = first.prepare({1, 1}, epoch, 0, request, [](auto&) {});
  auto moved_retry = std::move(retry);
  EXPECT_THROW(first.accept(std::move(retry)), std::invalid_argument);
  EXPECT_EQ(first.accept(std::move(moved_retry)).revision, 1u);
  EXPECT_THROW(first.accept(std::move(moved_retry)), std::invalid_argument);
}

// Integrity-valid records still cannot resurrect a deleted identity or retype a surviving entity.
TEST(native_database, recovery_rejects_identity_reuse_and_retyping) {
  for (const bool retype : {false, true}) {
    database_directory directory;
    std::shared_ptr<const database::storage_type> original;
    {
      database state{directory.path(), ledger{"Ledger", {{10, {"One", 100}}}},
                     document_namespace, epoch};
      original = state.read();
      auto prepared = state.prepare({1, 1}, epoch, 0, request, [&](auto& edit) {
        if (!retype) { edit.root().entries().erase(2); }
      });
      state.accept(std::move(prepared));
    }
    auto invalid = *original;
    if (retype) { std::swap(invalid.persistent_id, invalid.entries.at(10).persistent_id); }
    database::store_type forged{managed::identified_baseline, std::move(invalid),
                                document_namespace, 2};
    append_envelope(directory.path(), forged.save());
    const auto original_bytes = std::filesystem::file_size(directory.path());
    EXPECT_THROW(database reopened{directory.path()}, std::invalid_argument);
    EXPECT_EQ(std::filesystem::file_size(directory.path()), original_bytes);
  }
}
