// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace rohit::managed {

// Fault boundaries are qualification instrumentation, never storage adapters or notifications.
enum class database_io_event {
  before_write, after_write, after_flush, before_publish,
  after_rebaseline_archive, before_rebaseline_replace, after_rebaseline_replace
};

namespace detail {

// Keep native transport independent of generated managed records and the compiler build graph.
struct native_database_file_options {
  static constexpr std::size_t mebibyte = 1024 * 1024;
  std::size_t max_record_bytes{64 * mebibyte};
  std::size_t max_file_bytes{1024 * mebibyte};
  std::size_t max_operations{4096};
  std::function<void(database_io_event)> fault_injector{};
};

// Own the native writer lock; append is durable only after the operating-system flush succeeds.
class native_database_file {
public:
  // Open exclusively for this writer. Creation requires an absent file and an existing directory.
  native_database_file(const std::filesystem::path& path, bool create,
                       const native_database_file_options& options);
  // Release the writer without saving or changing accepted bytes.
  ~native_database_file();
  native_database_file(const native_database_file&) = delete;
  native_database_file& operator=(const native_database_file&) = delete;
  // Read and verify the bounded committed frames; a truncated final frame is repaired on success.
  std::vector<std::vector<std::uint8_t>> recover();
  // Repair a verified incomplete suffix only after native model validation has completed.
  void finish_recovery();
  // Append one checksummed record and flush it. Any issued write failure fences the file.
  void append(std::span<const std::uint8_t> payload);
  // Retain an exclusive immutable archive, then atomically replace the current log with one
  // baseline. Failure after replacement fences the writer; the archived generation is preserved.
  void rebaseline(std::span<const std::uint8_t> payload, const std::filesystem::path& archive);
  // Report an unresolved durable decision without performing I/O.
  bool needs_recovery() const noexcept;

private:
  struct implementation;
  std::unique_ptr<implementation> state_;
};

} // namespace detail
} // namespace rohit::managed
