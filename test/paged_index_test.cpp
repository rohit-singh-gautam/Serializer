// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <schema_indexes.hpp>
#include <rohit/native_database_index.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace managed = rohit::managed;

// Declaration capabilities remain compile-time gates even when the physical accelerator is ordered.
template <typename Descriptor>
concept database_paged_descriptor = requires {
  typename managed::native_database_index_snapshot<indexed_example::document, Descriptor>;
};

// Ordered range access must not silently appear on an unordered schema descriptor.
template <typename Snapshot>
concept database_paged_range = requires(const Snapshot& selected) { selected.range(0, 1); };

using database_key_descriptor = decltype(managed::fields<indexed_example::entry>::field_key());
using database_key_snapshot =
    managed::native_database_index_snapshot<indexed_example::document, database_key_descriptor>;
using undeclared_key_descriptor = managed::member_descriptor<indexed_example::entry,
    managed::model_traits<indexed_example::entry>::storage_type, std::uint64_t>;
using ordered_key_descriptor = managed::indexed_member_descriptor<indexed_example::entry,
    managed::model_traits<indexed_example::entry>::storage_type, std::uint64_t,
    managed::index_method::ordered, false>;
static_assert(database_paged_descriptor<database_key_descriptor>);
static_assert(!database_paged_descriptor<undeclared_key_descriptor>);
static_assert(!database_paged_range<database_key_snapshot>);
static_assert(database_paged_range<managed::native_database_index_snapshot<
    indexed_example::document, ordered_key_descriptor>>);

// Own only an exclusively created operating-system temporary child, never preexisting user files.
class page_directory {
  std::filesystem::path directory_;
public:
  // Reserve a unique directory independently of parallel tests and the CTest working directory.
  page_directory() {
    const auto id = managed::make_document_id();
    directory_ = std::filesystem::temp_directory_path() /
        ("serializer-paged-index-" + std::to_string(id.high) + "-" + std::to_string(id.low));
    if (!std::filesystem::create_directory(directory_)) {
      throw std::runtime_error{"Cannot create paged index test directory"};
    }
  }
  // Remove exactly this reserved child after its file handles and immutable pins have closed.
  ~page_directory() {
    std::error_code ignored;
    std::filesystem::remove_all(directory_, ignored);
  }
  // Locate a document/index name within this fixture's exclusive directory.
  std::filesystem::path path(std::string_view name = "index.spi") const { return directory_ / name; }
  // Count unfinished task-owned generation directories, excluding the persistent writer lock.
  std::size_t unfinished() const {
    std::size_t count{};
    for (const auto& entry : std::filesystem::directory_iterator{directory_}) {
      if (entry.is_directory()) { ++count; }
    }
    return count;
  }
};

// Bind test entries to a fully specified source, independently of the filename and build order.
managed::paged_index_binding binding(std::uint64_t revision = 1) {
  return {17, 23, 42, revision,
      rohit::make_digest<rohit::digest_algorithm::sha256>("schema"),
      rohit::make_digest<rohit::digest_algorithm::sha256>("definition:uint64:v1"),
      rohit::make_digest<rohit::digest_algorithm::sha256>(std::to_string(revision))};
}

// Force many run/merge/directory boundaries while retaining only two pages of the resulting index.
managed::paged_index_options tiny_options() {
  managed::paged_index_options options;
  options.page_bytes = 512;
  options.max_cache_pages = 2;
  options.max_cache_bytes = 16 * options.page_bytes;
  options.max_run_entries = 37;
  options.merge_fan_in = 3;
  return options;
}

// Build unique ascending keys from a stream; caller-owned test oracles do not count as builder memory.
auto build_sequential(const std::filesystem::path& path, const managed::paged_index_binding& source,
                      std::uint64_t count, std::uint64_t offset,
                      const managed::paged_index_options& options) {
  std::uint64_t next{};
  return managed::paged_index::build(path, source, [&](auto& entry) {
    if (next == count) { return false; }
    entry = {offset + next, next + 1};
    ++next;
    return true;
  }, options);
}
} // namespace

// Unsorted input requires multiple external passes; every range agrees with an independent numeric sort.
TEST(paged_index, external_merge_pageable_directory_and_one_page_scan) {
  page_directory directory;
  auto options = tiny_options();
  constexpr std::uint64_t count = 4096;
  std::uint64_t next{};
  std::vector<managed::paged_index_entry> expected;
  const auto statistics = managed::paged_index::build(directory.path(), binding(), [&](auto& entry) {
    if (next == count + 1) { return false; }
    const auto identity = next == count ? count : (next * 7919) % count;
    entry = {next == count ? std::numeric_limits<std::uint64_t>::max() : identity % 257,
             identity + 1};
    expected.push_back(entry);
    ++next;
    return true;
  }, options);
  std::sort(expected.begin(), expected.end(), [](auto left, auto right) {
    return left.key < right.key || (left.key == right.key && left.identity < right.identity);
  });
  EXPECT_EQ(statistics.entries, count + 1);
  EXPECT_GT(statistics.pages, options.max_cache_pages * 20);
  EXPECT_GT(statistics.initial_runs, 100u);
  EXPECT_GT(statistics.merge_passes, 1u);
  EXPECT_LE(statistics.peak_sort_entries, options.max_run_entries);
  EXPECT_LE(statistics.peak_merge_inputs, options.merge_fan_in);
  EXPECT_LE(statistics.peak_temporary_bytes, options.max_temporary_bytes);
  EXPECT_EQ(directory.unfinished(), 0u);
  options.max_cache_pages = 1;
  options.max_cache_bytes = 16 * options.page_bytes;
  managed::paged_index index{directory.path(), binding(), options};
  EXPECT_EQ(index.size(), expected.size());
  auto cursor = index.range(0, std::numeric_limits<std::uint64_t>::max());
  for (const auto entry : expected) {
    const auto actual = cursor.next();
    ASSERT_TRUE(actual);
    EXPECT_EQ(*actual, entry);
  }
  EXPECT_FALSE(cursor.next());
  EXPECT_FALSE(index.equal(900).next());
  EXPECT_FALSE(index.range(10, 9).next());
  for (const auto key : {std::uint64_t{0}, std::uint64_t{256},
                         std::numeric_limits<std::uint64_t>::max()}) {
    auto matches = index.equal(key);
    std::size_t actual{};
    while (const auto entry = matches.next()) {
      EXPECT_EQ(entry->key, key);
      ++actual;
    }
    EXPECT_EQ(actual, static_cast<std::size_t>(std::count_if(expected.begin(), expected.end(),
        [&](const auto entry) { return entry.key == key; })));
  }
  const auto cache = index.statistics();
  EXPECT_LE(cache.peak_resident_pages, 1u);
  EXPECT_LE(cache.resident_page_bytes, options.max_cache_bytes);
  EXPECT_LE(cache.peak_cache_bytes, options.max_cache_bytes);
  EXPECT_GT(cache.page_reads, index.page_count());
}

// A page pinned by a cursor is charged until released; admission never temporarily exceeds the cap.
TEST(paged_index, pinned_cursor_pressure_rejects_without_overcommit) {
  page_directory directory;
  const auto options = tiny_options();
  build_sequential(directory.path(), binding(), 200, 0, options);
  managed::paged_index index{directory.path(), binding(), options};
  std::optional<managed::paged_index::cursor> first{index.equal(0)};
  std::optional<managed::paged_index::cursor> last{index.equal(199)};
  EXPECT_EQ(index.statistics().pinned_pages, 2u);
  EXPECT_THROW((void)index.equal(80), std::length_error);
  EXPECT_EQ(index.statistics().peak_resident_pages, 2u);
  first.reset();
  auto middle = index.equal(80);
  ASSERT_TRUE(middle.next());
  EXPECT_FALSE(middle.next());
  last.reset();
  EXPECT_LE(index.statistics().resident_pages, options.max_cache_pages);
}

// Every source/definition coordinate participates in the generated manifest binding.
TEST(paged_index, foreign_stale_ahead_schema_definition_and_source_bindings_reject) {
  page_directory directory;
  const auto options = tiny_options();
  const auto expected = binding();
  build_sequential(directory.path(), expected, 80, 0, options);
  auto wrong = expected;
  wrong.document_low++;
  EXPECT_THROW((void)managed::paged_index(directory.path(), wrong, options), std::invalid_argument);
  wrong = expected; wrong.authority_epoch++;
  EXPECT_THROW((void)managed::paged_index(directory.path(), wrong, options), std::invalid_argument);
  wrong = expected; wrong.source_revision--;
  EXPECT_THROW((void)managed::paged_index(directory.path(), wrong, options), std::invalid_argument);
  wrong = expected; wrong.source_revision++;
  EXPECT_THROW((void)managed::paged_index(directory.path(), wrong, options), std::invalid_argument);
  wrong = expected; wrong.schema_digest.front() ^= 1;
  EXPECT_THROW((void)managed::paged_index(directory.path(), wrong, options), std::invalid_argument);
  wrong = expected; wrong.definition_digest.front() ^= 1;
  EXPECT_THROW((void)managed::paged_index(directory.path(), wrong, options), std::invalid_argument);
  wrong = expected; wrong.source_digest.front() ^= 1;
  EXPECT_THROW((void)managed::paged_index(directory.path(), wrong, options), std::invalid_argument);
}

// Before replacement, every build failure preserves the old path and already pinned readers.
TEST(paged_index, failed_staging_and_atomic_generation_replacement_preserve_readers) {
  for (const auto boundary : {managed::paged_index_io_event::before_page_write,
                              managed::paged_index_io_event::after_flush,
                              managed::paged_index_io_event::before_publish,
                              managed::paged_index_io_event::after_publish}) {
    page_directory directory;
    auto options = tiny_options();
    build_sequential(directory.path(), binding(1), 80, 0, options);
    managed::paged_index old{directory.path(), binding(1), options};
    auto old_cursor = old.range(0, 79);
    options.fault_injector = [boundary](auto event) {
      if (event == boundary) { throw std::runtime_error{"Injected index interruption"}; }
    };
    EXPECT_THROW((void)build_sequential(directory.path(), binding(2), 80, 1000, options),
                 std::runtime_error);
    options.fault_injector = {};
    if (boundary == managed::paged_index_io_event::after_publish) {
      managed::paged_index current{directory.path(), binding(2), options};
      ASSERT_TRUE(current.equal(1000).next());
      EXPECT_THROW((void)managed::paged_index(directory.path(), binding(1), options), std::invalid_argument);
    } else {
      managed::paged_index current{directory.path(), binding(1), options};
      ASSERT_TRUE(current.equal(0).next());
      EXPECT_THROW((void)managed::paged_index(directory.path(), binding(2), options), std::invalid_argument);
    }
    for (std::uint64_t key = 0; key < 80; ++key) {
      const auto entry = old_cursor.next();
      ASSERT_TRUE(entry);
      EXPECT_EQ(entry->key, key);
    }
    EXPECT_FALSE(old_cursor.next());
    EXPECT_EQ(directory.unfinished(), 0u);
  }
}

// Corrupt/truncated pages report errors rather than a missing identity or silent scan fallback.
TEST(paged_index, corruption_and_truncation_reject_without_repairing_source_bytes) {
  page_directory directory;
  const auto options = tiny_options();
  build_sequential(directory.path(), binding(), 80, 0, options);
  {
    std::fstream corrupt{directory.path(), std::ios::in | std::ios::out | std::ios::binary};
    corrupt.seekp(static_cast<std::streamoff>(options.page_bytes + 48));
    corrupt.put('\xff');
  }
  const auto bytes = std::filesystem::file_size(directory.path());
  {
    managed::paged_index index{directory.path(), binding(), options};
    EXPECT_THROW((void)index.equal(0), std::invalid_argument);
  }
  EXPECT_EQ(std::filesystem::file_size(directory.path()), bytes);
  build_sequential(directory.path(), binding(), 80, 0, options);
  std::filesystem::resize_file(directory.path(), bytes - 1);
  EXPECT_THROW((void)managed::paged_index(directory.path(), binding(), options), std::invalid_argument);
  EXPECT_EQ(std::filesystem::file_size(directory.path()), bytes - 1);
  build_sequential(directory.path(), binding(), 80, 0, options);
  {
    std::fstream corrupt{directory.path(), std::ios::in | std::ios::out | std::ios::binary};
    corrupt.seekp(24);
    corrupt.put('\xff');
  }
  EXPECT_THROW((void)managed::paged_index(directory.path(), binding(), options), std::invalid_argument);
}

// A temporary pair can remain numerically ordered after corruption; block integrity must catch it.
TEST(paged_index, temporary_run_corruption_rejects_before_new_generation_publication) {
  page_directory directory;
  auto options = tiny_options();
  build_sequential(directory.path(), binding(), 80, 0, options);
  bool corrupted{};
  options.fault_injector = [&](auto event) {
    if (corrupted || event != managed::paged_index_io_event::after_run_write) { return; }
    for (const auto& entry : std::filesystem::directory_iterator{directory.path().parent_path()}) {
      if (!entry.is_directory()) { continue; }
      std::fstream stream{entry.path() / "run-0-0", std::ios::in | std::ios::out | std::ios::binary};
      ASSERT_TRUE(stream);
      // One uint64 block count, followed by the first key, then its nonzero identity.
      constexpr std::streamoff first_identity_offset_bytes = 16;
      stream.seekp(first_identity_offset_bytes);
      stream.put('\x7f');
      ASSERT_TRUE(stream);
      corrupted = true;
      break;
    }
  };
  EXPECT_THROW((void)build_sequential(directory.path(), binding(2), 200, 1000, options),
               std::invalid_argument);
  EXPECT_TRUE(corrupted);
  options.fault_injector = {};
  managed::paged_index old{directory.path(), binding(), options};
  EXPECT_TRUE(old.equal(0).next());
  EXPECT_EQ(directory.unfinished(), 0u);
}

// External run boundaries, temporary disk, source count, cancellation and final pages all fail closed.
TEST(paged_index, duplicate_and_resource_failures_leave_old_generation_usable) {
  page_directory directory;
  const auto baseline = tiny_options();
  build_sequential(directory.path(), binding(), 80, 0, baseline);
  {
    auto options = baseline;
    options.max_run_entries = 3;
    options.merge_fan_in = 2;
    std::uint64_t next{};
    EXPECT_THROW((void)managed::paged_index::build(directory.path(), binding(2), [&](auto& entry) {
      if (next == 20) { return false; }
      entry = {next % 4, next % 4 + 1};
      ++next;
      return true;
    }, options), std::invalid_argument);
  }
  for (unsigned scenario = 0; scenario != 4; ++scenario) {
    auto options = baseline;
    if (scenario == 0) { options.max_entries = 3; }
    if (scenario == 1) { options.max_temporary_bytes = 16; }
    if (scenario == 2) { options.max_file_bytes = 2 * options.page_bytes; }
    if (scenario == 3) { options.is_cancelled = [] { return true; }; }
    EXPECT_THROW((void)build_sequential(directory.path(), binding(2), 200, 1000, options), std::exception);
    managed::paged_index old{directory.path(), binding(), baseline};
    ASSERT_TRUE(old.equal(0).next());
    EXPECT_EQ(directory.unfinished(), 0u);
  }
}

// Empty generation is a valid complete index, and cache state remains owner-thread confined.
TEST(paged_index, empty_index_and_owner_thread_contract) {
  page_directory directory;
  const auto options = tiny_options();
  build_sequential(directory.path(), binding(), 0, 0, options);
  managed::paged_index index{directory.path(), binding(), options};
  EXPECT_EQ(index.size(), 0u);
  EXPECT_EQ(index.page_count(), 1u);
  EXPECT_FALSE(index.equal(0).next());
  bool rejected{};
  std::thread other{[&] {
    try { static_cast<void>(index.equal(0)); }
    catch (const std::logic_error&) { rejected = true; }
  }};
  other.join();
  EXPECT_TRUE(rejected);
}

// Cursor/control-block metadata consumes the same byte domain as pages and cannot grow unbounded.
TEST(paged_index, cursor_byte_admission_and_cancellation_callback_reentrancy) {
  page_directory directory;
  auto options = tiny_options();
  build_sequential(directory.path(), binding(), 80, 0, options);
  managed::paged_index* active_index{};
  bool inspect_callback{};
  options.is_cancelled = [&] {
    if (inspect_callback) {
      EXPECT_THROW((void)active_index->equal(10), std::logic_error);
    }
    return false;
  };
  managed::paged_index index{directory.path(), binding(), options};
  active_index = &index;
  inspect_callback = true;
  EXPECT_TRUE(index.equal(10).next());
  inspect_callback = false;
  std::vector<managed::paged_index::cursor> cursors;
  bool exhausted{};
  for (unsigned ordinal = 0; ordinal < 10000; ++ordinal) {
    try { cursors.push_back(index.range(10, 9)); }
    catch (const std::length_error&) { exhausted = true; break; }
  }
  EXPECT_TRUE(exhausted);
  EXPECT_EQ(index.statistics().cursors, cursors.size());
  EXPECT_LE(index.statistics().peak_cache_bytes, options.max_cache_bytes);
  cursors.clear();
  EXPECT_EQ(index.statistics().cursors, 0u);
  EXPECT_TRUE(index.equal(10).next());
}

// Real generated descriptors stream stable managed IDs and preserve eager source pins across commits.
TEST(paged_index, native_database_snapshot_binding_real_descriptor_and_explicit_load) {
  using root = indexed_example::document;
  using entry = indexed_example::entry;
  const auto descriptor = managed::fields<entry>::field_key();
  using snapshot = managed::native_database_index_snapshot<root, decltype(descriptor)>;
  page_directory directory;
  root value;
  value.entries.resize(2);
  value.entries[0].key = 10;
  value.entries[1].key = 20;
  managed::value_fields<entry>::field_sequence().write(value.entries[0], 1);
  managed::value_fields<entry>::field_sequence().write(value.entries[1], 2);
  managed::native_database<root> database{directory.path("source.sdb"), value, {17, 23}, 42};
  const auto options = tiny_options();
  snapshot previous{database, descriptor, "entry.key.v1"};
  previous.rebuild(directory.path(), options);
  previous.open(directory.path(), options);
  std::optional<typename snapshot::cursor> retained;
  {
    snapshot temporary{database, descriptor, "entry.key.v1"};
    temporary.open(directory.path(), options);
    retained.emplace(temporary.equal(10));
  }
  const auto match = previous.equal(10).next();
  ASSERT_TRUE(match);
  EXPECT_EQ(previous.load(match->identity, 3).value().key, 10u);
  const auto retained_match = retained->next();
  ASSERT_TRUE(retained_match);
  EXPECT_EQ(retained->load(retained_match->identity, 3).value().key, 10u);
  EXPECT_EQ(retained->binding().source_revision, 0u);
  EXPECT_THROW((void)previous.load(match->identity, 1), std::length_error);
  EXPECT_THROW((void)previous.load(999, 3), managed::registry_error);
  const std::array<std::uint8_t, 1> request{1};
  auto prepared = database.prepare({1, 1}, 42, 0, request, [](auto& transaction) {
    transaction.root().entries().edit(2).set_key(30);
  });
  database.accept(std::move(prepared));
  EXPECT_EQ(descriptor.read(managed::model_traits<root>::payload(*database.read()).entries.front()), 30u);
  EXPECT_EQ(previous.load(match->identity, 3).value().key, 10u);
  snapshot current{database, descriptor, "entry.key.v1"};
  EXPECT_THROW(current.open(directory.path(), options), std::invalid_argument);
  current.rebuild(directory.path(), options);
  current.open(directory.path(), options);
  EXPECT_FALSE(current.equal(10).next());
  ASSERT_TRUE(current.equal(30).next());
  EXPECT_EQ(previous.equal(10).next()->identity, match->identity);
  EXPECT_EQ(database.revision(), 1u);
}
