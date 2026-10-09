// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/stream_concepts.hpp>

#include <cstdint>
#include <filesystem>
#include <ios>
#include <memory>
#include <optional>
#include <string>

namespace rohit {

// create is exclusive; update preserves existing data; lock owns a stable single-writer lock.
enum class file_open_mode { read, update, create, lock };

// An owning, unbuffered byte stream with explicit durability and recovery capabilities.
// Thread-confined. Reads/writes complete the requested range or report EOF/throw; errors latch.
// Destruction closes the handle, releasing any lock, and never implicitly syncs or throws.
class file_stream {
  struct implementation;
  std::unique_ptr<implementation> state_;
  std::streamsize count_{};
  bool eof_{};
  bool failed_{};
  bool bad_{};
  std::optional<unsigned char> lookahead_{};

public:
  // Open a file or acquire its nonblocking exclusive lock without transferring a host handle.
  explicit file_stream(const std::filesystem::path& path, file_open_mode mode);
  file_stream(const file_stream&) = delete;
  file_stream& operator=(const file_stream&) = delete;
  // Release the native resource; applications must explicitly sync before acknowledging durability.
  ~file_stream();

  // Read up to size bytes; a short EOF read sets eof/fail and gcount reports the delivered prefix.
  void read(char* bytes, std::streamsize size);
  // Write all bytes or throw and set bad/fail; a failed external write may have delivered a prefix.
  void write(const char* bytes, std::streamsize size);
  // Inspect one byte without consuming it logically; EOF sets eof, and native errors throw.
  std::char_traits<char>::int_type peek();
  // Return the number of bytes delivered by the last read.
  std::streamsize gcount() const noexcept {
    return count_;
  }
  // Report EOF independently of native I/O failure.
  bool eof() const noexcept {
    return eof_;
  }
  // Report a short read or a failed operation, matching the standard byte-stream concepts.
  bool fail() const noexcept {
    return failed_;
  }
  // Distinguish native failure from ordinary end of input.
  bool bad() const noexcept {
    return bad_;
  }
  // Return the physical byte length without changing the logical cursor.
  std::uint64_t size() const;
  // Set an absolute byte position and clear read/EOF state on success; never reopens the file.
  void seek(std::uint64_t offset_bytes);
  // Resize and position at the new end; caller must validate the recovery boundary and then sync.
  void truncate(std::uint64_t size_bytes);
  // Flush file data and metadata to the platform's durability boundary or throw and latch failure.
  void sync();
};

static_assert(type_check::input_stream<file_stream>);
static_assert(type_check::durable_output_stream<file_stream>);
static_assert(type_check::seekable_stream<file_stream>);
static_assert(type_check::truncatable_stream<file_stream>);
static_assert(type_check::sized_stream<file_stream>);

// Persist the parent directory after publication on POSIX; Windows uses write-through publication.
void sync_parent_directory(const std::filesystem::path& path);
// Atomically publish a fully synced same-directory file; never fall back to copying file contents.
// replace=false must not overwrite an existing destination. Caller syncs the parent afterwards.
void publish_file(const std::filesystem::path& source, const std::filesystem::path& target,
                  bool replace);

} // namespace rohit
