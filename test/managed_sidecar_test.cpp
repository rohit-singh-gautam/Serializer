// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "../src/managed_journal_delta.hpp"
#include "managed_journal_paths.hpp"

#include <ledger.hpp>
#include <rohit/file_stream.hpp>
#include <rohit/managed.hpp>
#include <rohit/managed_journal_stream.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
using ledger = ledger_example::ledger;
using store_type = managed::model_store<ledger>;
using bytes = std::vector<std::uint8_t>;
// Explicit wire offsets let this test create a pre-differential sidecar without its writer code.
constexpr std::size_t file_header_bytes = 72;
constexpr std::size_t wire_word_bytes = 8;
constexpr std::size_t version_word = 1;
constexpr std::size_t mode_word = 2;
constexpr std::size_t kind_word = 3;
constexpr std::size_t sequence_word = 6;
constexpr std::size_t base_digest_word = 7;
constexpr std::size_t header_checksum_word = 8;
constexpr std::uint64_t legacy_version = 2;
constexpr std::uint64_t differential_version = 3;
constexpr std::uint64_t shared_version = 4;
constexpr std::uint64_t sidecar_mode = 2;
constexpr std::uint64_t companion_kind = 2;
constexpr std::uint64_t delta_magic = 0x3141544c444a5253;
constexpr std::size_t delta_header_bytes = 5 * wire_word_bytes;
constexpr std::size_t maximum_fixture_bytes = 64 * 1024 * 1024;

// Read accepted owner-thread fixture bytes even when the same thread retains its journal writer.
bytes read_bytes(const std::filesystem::path& path) {
  rohit::file_stream input{path, rohit::file_open_mode::read_shared};
  const auto length = input.size();
  if (length > maximum_fixture_bytes) {
    throw std::length_error{"Sidecar test fixture exceeded its byte budget"};
  }
  bytes result(static_cast<std::size_t>(length));
  rohit::read_stream_exact(input, result);
  return result;
}

// Prepare only a closed, private test fixture; live stores always publish through their journal API.
void write_bytes(const std::filesystem::path& path, const bytes& value) {
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.write(reinterpret_cast<const char*>(value.data()),
                 static_cast<std::streamsize>(value.size()));
  if (!output) {
    throw std::runtime_error{"Cannot write sidecar test fixture"};
  }
}

// Pin integrity after changing only a container field outside the unchanged frame checksum chain.
void refresh_header_checksum(bytes& container) {
  if (container.size() < file_header_bytes) {
    throw std::invalid_argument{"Incomplete sidecar test container"};
  }
  const auto header = std::span<const std::uint8_t>{container}.first(file_header_bytes);
  managed::detail::journal_put_word(container, header_checksum_word * wire_word_bytes,
      managed::detail::checksum(header.first(header_checksum_word * wire_word_bytes)));
}

// Resolve the exact selected generation instead of guessing among obsolete crash-recovery files.
std::filesystem::path companion_path(const std::filesystem::path& path, const bytes& main) {
  if (main.size() < file_header_bytes) {
    throw std::invalid_argument{"Incomplete sidecar test header"};
  }
  return journal_test_paths::companion(path, managed::detail::get_word(main, 4),
      managed::detail::get_word(main, 5));
}

// Make unchanged data large and nonrepetitive so a full-copy writer cannot hide behind easy runs.
std::string unchanged_payload() {
  constexpr std::size_t payload_bytes = 128 * 1024;
  constexpr std::uint64_t multiplier = 0x2545f4914f6cdd1d;
  constexpr unsigned printable_first = 33;
  constexpr unsigned printable_count = 94;
  std::uint64_t state = 0xd1b54a32d192ed03;
  std::string result(payload_bytes, '\0');
  for (auto& character : result) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    const auto random_byte = static_cast<unsigned>((state * multiplier) >> 56);
    character = static_cast<char>(printable_first + random_byte % printable_count);
  }
  return result;
}

// Apply a real generated editor transaction while keeping the test's persistent changes explicit.
void rename(store_type& store, const std::string& name) {
  store.execute_transaction([&](auto& transaction) { transaction.root().set_name(name); })
      .throw_if_failed();
}

struct captured_sidecar {
  bytes baseline{};
  bytes physical{};
  std::vector<bytes> patches{};
  std::vector<bytes> records{};
};

// Check physical frames and expose their reconstructed operations against the main-file dictionary.
captured_sidecar capture_sidecar(const std::filesystem::path& path) {
  captured_sidecar result;
  const auto main = read_bytes(path);
  if (main.size() < file_header_bytes) {
    throw std::invalid_argument{"Incomplete sidecar test base"};
  }
  auto sequence = managed::detail::get_word(main, sequence_word);
  const auto baseline = managed::detail::decode_frame(main, file_header_bytes, sequence, 0, {});
  if (!baseline || baseline->end_offset != main.size() ||
      baseline->digest != managed::detail::get_word(main, base_digest_word)) {
    throw std::invalid_argument{"Invalid sidecar test saved frame"};
  }
  result.baseline.assign(baseline->payload.begin(), baseline->payload.end());
  managed::detail::journal_delta_base dictionary{result.baseline};
  result.physical = read_bytes(companion_path(path, main));
  const bool differential = managed::detail::get_word(main, version_word) >= differential_version;
  auto offset = file_header_bytes;
  auto digest = baseline->digest;
  while (offset < result.physical.size()) {
    const auto record =
        managed::detail::decode_frame(result.physical, offset, ++sequence, digest, {});
    if (!record) {
      throw std::invalid_argument{"Incomplete sidecar test record"};
    }
    result.patches.emplace_back(record->payload.begin(), record->payload.end());
    result.records.push_back(differential ? dictionary.decode(record->payload, maximum_fixture_bytes)
                                           : result.patches.back());
    digest = record->digest;
    offset = record->end_offset;
  }
  return result;
}

// Convert complete version-two appended bytes into an explicit legacy sidecar fixture.
void create_legacy_sidecar(const std::filesystem::path& source,
                            const std::filesystem::path& destination) {
  auto appended = read_bytes(source);
  if (appended.size() < file_header_bytes ||
      (managed::detail::get_word(appended, version_word) != legacy_version &&
       managed::detail::get_word(appended, version_word) != shared_version) ||
      managed::detail::get_word(appended, mode_word) != 1) {
    throw std::invalid_argument{"Legacy fixture source must be a version-two appended file"};
  }
  const auto sequence = managed::detail::get_word(appended, sequence_word);
  const auto baseline = managed::detail::decode_frame(appended, file_header_bytes, sequence, 0, {});
  if (!baseline) {
    throw std::invalid_argument{"Legacy fixture source has no complete base"};
  }
  bytes main(appended.begin(), appended.begin() + static_cast<std::ptrdiff_t>(baseline->end_offset));
  managed::detail::journal_put_word(main, version_word * wire_word_bytes, legacy_version);
  managed::detail::journal_put_word(main, mode_word * wire_word_bytes, sidecar_mode);
  refresh_header_checksum(main);
  bytes companion(main.begin(), main.begin() + file_header_bytes);
  managed::detail::journal_put_word(companion, kind_word * wire_word_bytes, companion_kind);
  refresh_header_checksum(companion);
  companion.insert(companion.end(),
                      appended.begin() + static_cast<std::ptrdiff_t>(baseline->end_offset),
                      appended.end());
  write_bytes(companion_path(destination, main), companion);
  write_bytes(destination, main);
}

class managed_sidecar_test : public testing::Test {
protected:
  std::filesystem::path directory_{};
  std::filesystem::path path_{};
  bool owns_directory_{};

  // Reuse the ignored journal-test pattern and create a unique child of the active build directory.
  void SetUp() override {
    const auto id = managed::make_document_id();
    directory_ = std::filesystem::current_path() /
                 ("journal-test-" + std::to_string(id.high) + "-" + std::to_string(id.low));
    owns_directory_ = std::filesystem::create_directory(directory_);
    ASSERT_TRUE(owns_directory_);
    path_ = directory_ / "document";
  }

  // Delete only the successfully created unique fixture directory after local store handles close.
  void TearDown() override {
    if (!owns_directory_) {
      return;
    }
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
    EXPECT_FALSE(error) << error.message();
  }
};

// Default sidecars keep small differences across controls and more edits than the optional delta cap.
TEST_F(managed_sidecar_test, default_records_reference_large_original_and_recover_after_controls) {
  constexpr std::size_t additional_edits = 80;
  constexpr std::size_t maximum_tiny_edit_frame_bytes = 1024;
  const auto payload = unchanged_payload();
  bytes original;
  bytes expected;
  {
    store_type store{ledger{"Base", {{10, {payload, 200}}}}};
    store.create_journal(path_, managed::journal_storage_mode::sidecar);
    original = read_bytes(path_);
    EXPECT_EQ(managed::detail::get_word(original, version_word), shared_version);
    const auto companion = companion_path(path_, original);
    EXPECT_EQ(std::filesystem::file_size(companion), file_header_bytes);
    auto previous_size = std::filesystem::file_size(companion);
    rename(store, "One");
    EXPECT_LT(std::filesystem::file_size(companion) - previous_size,
                 maximum_tiny_edit_frame_bytes);
    store.undo();
    EXPECT_EQ(store.read()->name, "Base");
    store.redo();
    EXPECT_EQ(store.read()->name, "One");
    store.execute_transaction([](auto& transaction) {
      EXPECT_EQ(transaction.create_id(), 3u);
      transaction.revert();
    }).throw_if_failed();
    for (std::size_t index = 0; index < additional_edits; ++index) {
      previous_size = std::filesystem::file_size(companion);
      rename(store, "Edit " + std::to_string(index));
      EXPECT_LT(std::filesystem::file_size(companion) - previous_size,
                   maximum_tiny_edit_frame_bytes);
    }
    EXPECT_EQ(store.journal_sequence(), additional_edits + 4);
    EXPECT_TRUE(store.journal_dirty());
    EXPECT_EQ(read_bytes(path_), original);
    expected = store.save();
  }
  const auto captured = capture_sidecar(path_);
  ASSERT_EQ(captured.records.size(), additional_edits + 4);
  EXPECT_LT(captured.physical.size(), payload.size() / 2);
  EXPECT_EQ(std::search(captured.physical.begin(), captured.physical.end(),
                          payload.begin(), payload.end()), captured.physical.end());
  EXPECT_NE(std::search(original.begin(), original.end(), payload.begin(), payload.end()),
                original.end());
  for (const auto& patch : captured.patches) {
    ASSERT_GE(patch.size(), delta_header_bytes);
    EXPECT_EQ(managed::detail::get_word(patch, 0), delta_magic);
    EXPECT_EQ(managed::detail::get_word(patch, 1), captured.baseline.size());
    EXPECT_EQ(managed::detail::get_word(patch, 2), managed::detail::checksum(captured.baseline));
  }
  EXPECT_EQ(captured.records[0].front(), 1u);
  EXPECT_EQ(captured.records[1].front(), 2u);
  EXPECT_EQ(captured.records[2].front(), 2u);
  EXPECT_EQ(captured.records[3].front(), 3u);
  for (std::size_t index = 4; index < captured.records.size(); ++index) {
    EXPECT_EQ(captured.records[index].front(), 1u);
  }
  store_type recovered{ledger{}};
  recovered.recover_journal(path_);
  EXPECT_EQ(recovered.save(), expected);
  EXPECT_EQ(recovered.read()->name, "Edit 79");
  EXPECT_EQ(recovered.read()->entries.at(10).memo, payload);
  EXPECT_EQ(recovered.journal_sequence(), additional_edits + 4);
  recovered.undo();
  EXPECT_EQ(recovered.read()->name, "Edit 78");
  recovered.redo();
  EXPECT_EQ(recovered.read()->name, "Edit 79");
  EXPECT_EQ(read_bytes(path_), original);
}

// Old readers' bytes stay writable until explicit Save selects a new differential generation.
TEST_F(managed_sidecar_test, legacy_version_two_remains_writable_and_save_upgrades_to_three) {
  const auto appended = directory_ / "legacy-appended";
  bytes expected;
  {
    store_type source{ledger{"Base"}};
    source.create_journal(appended, managed::journal_storage_mode::appended);
    rename(source, "One");
    rename(source, "Two");
    source.undo();
    expected = source.save();
  }
  create_legacy_sidecar(appended, path_);
  {
    store_type legacy{ledger{}};
    managed::journal_options legacy_options;
    legacy_options.max_replay_bytes = 1;
    legacy.recover_journal(path_, legacy_options);
    EXPECT_EQ(legacy.save(), expected);
    EXPECT_EQ(legacy.read()->name, "One");
    legacy.redo();
    EXPECT_EQ(legacy.read()->name, "Two");
    rename(legacy, "Legacy continuation");
    EXPECT_EQ(legacy.journal_sequence(), 5u);
    const auto main = read_bytes(path_);
    EXPECT_EQ(managed::detail::get_word(main, version_word), legacy_version);
    const auto physical = read_bytes(companion_path(path_, main));
    EXPECT_EQ(managed::detail::get_word(physical, version_word), legacy_version);
    expected = legacy.save();
  }
  auto captured = capture_sidecar(path_);
  ASSERT_EQ(captured.records.size(), 5u);
  EXPECT_EQ(captured.records.back().front(), 1u);
  EXPECT_EQ(captured.patches.back(), captured.records.back());
  {
    store_type upgraded{ledger{}};
    upgraded.recover_journal(path_);
    EXPECT_EQ(upgraded.save(), expected);
    EXPECT_EQ(upgraded.read()->name, "Legacy continuation");
    upgraded.save_journal();
    EXPECT_FALSE(upgraded.journal_dirty());
    EXPECT_EQ(upgraded.journal_sequence(), 5u);
    const auto main = read_bytes(path_);
    EXPECT_EQ(managed::detail::get_word(main, version_word), differential_version);
    const auto physical = read_bytes(companion_path(path_, main));
    EXPECT_EQ(managed::detail::get_word(physical, version_word), differential_version);
    EXPECT_EQ(physical.size(), file_header_bytes);
    rename(upgraded, "After upgrade");
    expected = upgraded.save();
  }
  captured = capture_sidecar(path_);
  ASSERT_EQ(captured.records.size(), 1u);
  EXPECT_EQ(managed::detail::get_word(captured.patches.front(), 0), delta_magic);
  EXPECT_EQ(captured.records.front().front(), 1u);
  store_type recovered{ledger{}};
  recovered.recover_journal(path_);
  EXPECT_EQ(recovered.save(), expected);
  EXPECT_EQ(recovered.read()->name, "After upgrade");
  EXPECT_EQ(recovered.journal_sequence(), 6u);
}

// Expanded replay limits cover tiny physical patches and reset only after a successful full Save.
TEST_F(managed_sidecar_test, cumulative_replay_budget_rejects_before_io_and_resets_after_save) {
  constexpr std::size_t edit_metadata_bytes = 1 + 2 * wire_word_bytes;
  managed::store_options store_options;
  store_options.max_revisions = 1; // Keep the checkpoint size stable while exercising ordinary edits.
  managed::journal_options options;
  std::size_t before_append_calls = 0;
  options.fault_injector = [&](auto point) {
    if (point == managed::journal_io_event::before_append) {
      ++before_append_calls;
    }
  };
  bytes expected;
  bytes main_before_recovery;
  bytes companion_before_recovery;
  std::size_t edit_bytes = 0;
  std::size_t baseline_bytes = 0;
  {
    store_type writer{ledger{"Base", {{10, {unchanged_payload(), 200}}}},
                        managed::make_document_id(), store_options};
    baseline_bytes = writer.save().size();
    edit_bytes = managed::detail::encode(*writer.read()).size() + edit_metadata_bytes;
    options.max_replay_bytes = baseline_bytes + 2 * edit_bytes;
    writer.create_journal(path_, managed::journal_storage_mode::sidecar, options);
    rename(writer, "AAAA");
    rename(writer, "BBBB");
    EXPECT_EQ(writer.journal_sequence(), 2u);
    EXPECT_EQ(before_append_calls, 2u);
    main_before_recovery = read_bytes(path_);
    const auto companion = companion_path(path_, main_before_recovery);
    companion_before_recovery = read_bytes(companion);
    for (const auto* rejected_name : {"CCCC", "DDDD"}) {
      const auto result = writer.execute_transaction([&](auto& transaction) {
        transaction.root().set_name(rejected_name);
      });
      EXPECT_EQ(result.status, managed::transaction_status::failed);
      EXPECT_THROW(result.throw_if_failed(), std::length_error);
      EXPECT_FALSE(writer.journal_needs_recovery());
      EXPECT_EQ(writer.journal_sequence(), 2u);
      EXPECT_EQ(writer.read()->name, "BBBB");
      EXPECT_EQ(read_bytes(path_), main_before_recovery);
      EXPECT_EQ(read_bytes(companion), companion_before_recovery);
      EXPECT_EQ(before_append_calls, 2u);
    }
    expected = writer.save();
  }
  {
    auto lower = options;
    lower.max_replay_bytes = baseline_bytes + edit_bytes;
    store_type unchanged{ledger{"Untouched"}, managed::make_document_id(), store_options};
    const auto pinned = unchanged.read();
    const auto original = unchanged.save();
    EXPECT_THROW(unchanged.recover_journal(path_, lower), std::length_error);
    EXPECT_EQ(unchanged.read(), pinned);
    EXPECT_EQ(unchanged.save(), original);
    EXPECT_FALSE(unchanged.journal_needs_recovery());
    EXPECT_EQ(read_bytes(path_), main_before_recovery);
    EXPECT_EQ(read_bytes(companion_path(path_, main_before_recovery)), companion_before_recovery);
    rename(unchanged, "Healthy");
  }
  {
    store_type checkpointed{ledger{}, managed::make_document_id(), store_options};
    checkpointed.recover_journal(path_, options);
    EXPECT_EQ(checkpointed.save(), expected);
    checkpointed.save_journal();
    EXPECT_FALSE(checkpointed.journal_dirty());
    EXPECT_EQ(checkpointed.journal_sequence(), 2u);
    EXPECT_EQ(checkpointed.save().size(), baseline_bytes);
    rename(checkpointed, "CCCC");
    rename(checkpointed, "DDDD");
    EXPECT_EQ(checkpointed.journal_sequence(), 4u);
    EXPECT_EQ(before_append_calls, 4u);
    const auto rejected = checkpointed.execute_transaction([](auto& transaction) {
      transaction.root().set_name("EEEE");
    });
    EXPECT_THROW(rejected.throw_if_failed(), std::length_error);
    EXPECT_FALSE(checkpointed.journal_needs_recovery());
    EXPECT_EQ(checkpointed.read()->name, "DDDD");
    EXPECT_EQ(before_append_calls, 4u);
    expected = checkpointed.save();
  }
  auto larger = options;
  larger.max_replay_bytes += edit_bytes;
  store_type recovered{ledger{}, managed::make_document_id(), store_options};
  recovered.recover_journal(path_, larger);
  EXPECT_EQ(recovered.save(), expected);
  EXPECT_EQ(recovered.read()->name, "DDDD");
  EXPECT_EQ(recovered.journal_sequence(), 4u);
}

} // namespace
