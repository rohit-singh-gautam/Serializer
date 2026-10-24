// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace rohit::managed {

// Canonical unsigned numeric ordering is (key, identity); duplicate keys are permitted.
struct paged_index_entry {
  std::uint64_t key{};
  std::uint64_t identity{};
  bool operator==(const paged_index_entry&) const = default;
};

// Deterministic test instrumentation; publication faults never change authoritative source data.
enum class paged_index_io_event {
  after_run_write, before_page_write, after_flush, before_publish, after_publish
};

struct paged_index_options {
  static constexpr std::size_t mebibyte = 1024 * 1024;
  std::size_t page_bytes{4096};
  std::size_t max_cache_pages{64};
  std::size_t max_cache_bytes{4 * mebibyte};
  std::size_t max_run_entries{4096};
  std::size_t merge_fan_in{8};
  std::size_t max_depth{32};
  std::uint64_t max_entries{10000000};
  std::uint64_t max_file_bytes{1024 * mebibyte};
  std::uint64_t max_temporary_bytes{2048ULL * mebibyte};
  std::function<bool()> is_cancelled{};
  std::function<void(paged_index_io_event)> fault_injector{};
};

// Controlled page-buffer admission is independent of OS cache and native owning model memory.
struct paged_index_statistics {
  std::size_t resident_pages{};
  std::size_t resident_page_bytes{};
  std::size_t pinned_pages{};
  std::size_t peak_resident_pages{};
  std::size_t resident_cache_bytes{};
  std::size_t peak_cache_bytes{};
  std::size_t cursors{};
  std::uint64_t page_reads{};
  std::uint64_t cache_hits{};
};

// Construction accounting excludes callback-owned extraction memory and filesystem cache.
struct paged_index_build_statistics {
  std::uint64_t entries{};
  std::uint64_t pages{};
  std::uint64_t initial_runs{};
  std::uint64_t merge_passes{};
  std::size_t peak_sort_entries{};
  std::size_t peak_merge_inputs{};
  std::uint64_t peak_temporary_bytes{};
};

namespace detail {

// In-memory transport metadata; persistence uses generated record callbacks, never object layout.
struct paged_index_metadata {
  std::uint64_t page_bytes{};
  std::uint64_t page_count{};
  std::uint64_t root_page{};
  std::uint64_t depth{};
  std::uint64_t entries{};
  std::uint64_t generation_high{};
  std::uint64_t generation_low{};
  std::vector<std::uint8_t> binding{};
};

using paged_index_source = std::function<bool(paged_index_entry&)>;
using paged_index_emit = std::function<void(paged_index_entry)>;
using paged_index_producer = std::function<void(const paged_index_emit&)>;
using paged_index_encode_metadata =
    std::function<std::vector<std::uint8_t>(const paged_index_metadata&)>;
using paged_index_decode_metadata =
    std::function<paged_index_metadata(std::span<const std::uint8_t>)>;

// Bulk-build sorted immutable pages from an unsorted bounded stream, then publish one file.
paged_index_build_statistics build_paged_index(const std::filesystem::path& path,
    std::span<const std::uint8_t> binding, const paged_index_source& source,
    const paged_index_encode_metadata& encode, const paged_index_options& options);

// Traverse generated ownership once and stream extracted pairs directly into the bounded builder.
paged_index_build_statistics build_paged_index_from(const std::filesystem::path& path,
    std::span<const std::uint8_t> binding, const paged_index_producer& source,
    const paged_index_encode_metadata& encode, const paged_index_options& options);

// A read-only pinned file generation with an explicitly budgeted shared page cache.
class paged_index_file {
  struct implementation;
  std::shared_ptr<implementation> state_;
public:
  // Cursor state owns its source file/cache generation; only copied key/identity pairs escape.
  class cursor {
    struct implementation;
    std::shared_ptr<implementation> state_;
    friend class paged_index_file;
    // Construct a cursor already bound to one exact immutable index generation.
    explicit cursor(std::shared_ptr<implementation> state);
  public:
    // Release the current page pin and its generation when the last owner goes away.
    ~cursor();
    cursor(const cursor&) = delete;
    cursor& operator=(const cursor&) = delete;
    // Transfer a cursor without allocating or changing its pinned page.
    cursor(cursor&&) noexcept;
    // Release an old cursor and transfer the selected generation/page pin.
    cursor& operator=(cursor&&) noexcept;
    // Return one copied result; failures expose no partial pair and can be retried for I/O admission.
    std::optional<paged_index_entry> next();
  };

  // Verify the bounded generated manifest and exact expected binding before exposing queries.
  paged_index_file(const std::filesystem::path& path, std::span<const std::uint8_t> binding,
      const paged_index_decode_metadata& decode, const paged_index_options& options);
  // Release this owner; cursors keep the original immutable generation alive independently.
  ~paged_index_file();
  paged_index_file(const paged_index_file&) = delete;
  paged_index_file& operator=(const paged_index_file&) = delete;
  // Pin one file generation in a movable owning query handle.
  paged_index_file(paged_index_file&&) noexcept;
  // Replace this query handle while preserving separately retained cursor generations.
  paged_index_file& operator=(paged_index_file&&) noexcept;
  // Create a bounded inclusive numeric range cursor; a miss never scans another source generation.
  cursor range(std::uint64_t lower, std::uint64_t upper) const;
  // Inspect controlled cache usage and I/O separately from process memory and OS file cache.
  paged_index_statistics statistics() const;
  // Return verified structural metadata, including the immutable generation and source binding.
  const paged_index_metadata& metadata() const;
};

} // namespace detail
} // namespace rohit::managed
