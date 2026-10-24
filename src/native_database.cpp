// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/native_database_file.hpp>

#include <rohit/digest.hpp>
#include <rohit/file_stream.hpp>
#include <rohit/managed_journal.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <ios>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

namespace rohit::managed::detail {
namespace {
constexpr std::uint64_t frame_magic = 0x3142444e5253;
constexpr std::uint64_t frame_commit = 0x3154494d4d4f43;
constexpr std::size_t word_bytes = 8;
constexpr std::size_t digest_bytes = digest_size(digest_algorithm::sha256);
constexpr std::size_t header_prefix_bytes = 2 * word_bytes;
constexpr std::size_t header_bytes = header_prefix_bytes + digest_bytes;
constexpr std::size_t trailer_bytes = digest_bytes + word_bytes;
constexpr std::size_t frame_overhead_bytes = header_bytes + trailer_bytes;

// Append the transport's fixed little-endian wire integer without native object-layout coupling.
void database_put_word(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) {
    bytes.push_back(static_cast<std::uint8_t>(value >> shift));
  }
}

// Read a fixed wire integer from a caller-verified complete transport header/trailer.
std::uint64_t database_get_word(std::span<const std::uint8_t> bytes, std::size_t& offset) {
  if (offset > bytes.size() || bytes.size() - offset < word_bytes) {
    throw std::invalid_argument{"Truncated native database transport word"};
  }
  std::uint64_t result{};
  for (unsigned shift = 0; shift < 64; shift += 8) {
    result |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
  }
  return result;
}

// Keep an immutable lock name beside the log, independent of recovered tail length.
std::filesystem::path writer_path(const std::filesystem::path& path) {
  auto result = path;
  result += ".writer.lock";
  return result;
}

// Convert a bounded byte extent to the byte-stream interface without narrowing overflow.
std::streamsize stream_size(std::size_t size) {
  if (size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
    throw std::length_error{"Native database byte-stream extent exceeded"};
  }
  return static_cast<std::streamsize>(size);
}

// Prepare complete checksummed transport bytes before opening a durable decision boundary.
std::vector<std::uint8_t> encode_frame(std::span<const std::uint8_t> payload) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(payload.size() + frame_overhead_bytes);
  database_put_word(bytes, frame_magic);
  database_put_word(bytes, payload.size());
  const auto header_digest = make_digest<digest_algorithm::sha256>(bytes);
  bytes.insert(bytes.end(), header_digest.begin(), header_digest.end());
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  const auto digest = make_digest<digest_algorithm::sha256>(bytes);
  bytes.insert(bytes.end(), digest.begin(), digest.end());
  database_put_word(bytes, frame_commit);
  return bytes;
}

// Own only a freshly created adjacent staging directory; never remove an existing user path.
class rebaseline_directory {
  std::filesystem::path path_;
public:
  // Exclusive directory creation prevents sharing temporary files with another operation.
  explicit rebaseline_directory(const std::filesystem::path& source) : path_{source} {
    std::random_device random;
    path_ += ".rebaseline-" + std::to_string(random()) + "-" + std::to_string(random());
    if (!std::filesystem::create_directory(path_)) {
      throw std::runtime_error{"Native database staging directory already exists"};
    }
  }
  // Remove task-owned staging leftovers only; retained archives are outside this directory.
  ~rebaseline_directory() {
    try {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    } catch (...) {
      // Best-effort staging cleanup must not obscure a durable result or terminate on bad_alloc.
    }
  }
  // Return the one unpublished baseline path owned by this operation.
  std::filesystem::path baseline() const { return path_ / "baseline.sdb"; }
};
} // namespace

struct native_database_file::implementation {
  native_database_file_options options;
  file_stream lock;
  std::unique_ptr<file_stream> data;
  std::filesystem::path data_path;
  std::uint64_t valid_bytes{};
  bool uncertain{};
  bool repair_pending{};

  // Acquire the stable writer lock before opening, validating or modifying the data file.
  implementation(const std::filesystem::path& path, bool create,
                 const native_database_file_options& configuration)
      : options{configuration}, lock{writer_path(path), file_open_mode::lock},
        data{std::make_unique<file_stream>(path, create ? file_open_mode::create : file_open_mode::update)},
        data_path{path} {
    if (create) {
      data->sync();
      sync_parent_directory(path);
    }
  }

  // Run deterministic fault instrumentation outside all publication steps.
  void fault(database_io_event event) const {
    if (options.fault_injector) {
      options.fault_injector(event);
    }
  }
};

// Construct native resources only after the caller has prepared the baseline in memory.
native_database_file::native_database_file(const std::filesystem::path& path, bool create,
                                         const native_database_file_options& options)
    : state_{std::make_unique<implementation>(path, create, options)} {}

// Let owning streams release locks and handles without implicit persistence.
native_database_file::~native_database_file() = default;

// Verify each length/hash/commit marker under the writer lock; defer tail truncation to the caller.
std::vector<std::vector<std::uint8_t>> native_database_file::recover() {
  const auto file_bytes = state_->data->size();
  if (file_bytes > state_->options.max_file_bytes ||
      file_bytes > std::numeric_limits<std::size_t>::max()) {
    throw std::length_error{"Native database file budget exceeded"};
  }
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_bytes));
  state_->data->seek(0);
  state_->data->read(reinterpret_cast<char*>(bytes.data()), stream_size(bytes.size()));
  if (state_->data->gcount() != stream_size(bytes.size())) {
    throw std::runtime_error{"Native database changed while exclusively locked"};
  }
  std::vector<std::vector<std::uint8_t>> records;
  std::size_t offset{};
  while (offset != bytes.size()) {
    const auto remaining = bytes.size() - offset;
    if (remaining < header_bytes) {
      break;
    }
    auto cursor = offset;
    if (database_get_word(bytes, cursor) != frame_magic) {
      throw std::invalid_argument{"Invalid native database frame magic"};
    }
    const auto payload_bytes = database_get_word(bytes, cursor);
    const auto header_digest = make_digest<digest_algorithm::sha256>(
        std::span<const std::uint8_t>{bytes}.subspan(offset, header_prefix_bytes));
    if (!std::equal(header_digest.begin(), header_digest.end(), bytes.begin() +
                    static_cast<std::ptrdiff_t>(cursor))) {
      throw std::invalid_argument{"Native database frame header checksum mismatch"};
    }
    cursor += digest_bytes;
    if (payload_bytes > state_->options.max_record_bytes ||
        payload_bytes > std::numeric_limits<std::size_t>::max() - frame_overhead_bytes) {
      throw std::length_error{"Native database record budget exceeded"};
    }
    const auto frame_bytes = static_cast<std::size_t>(payload_bytes) + frame_overhead_bytes;
    if (frame_bytes > remaining) {
      break;
    }
    const auto payload_end = cursor + static_cast<std::size_t>(payload_bytes);
    const auto digest = make_digest<digest_algorithm::sha256>(
        std::span<const std::uint8_t>{bytes}.subspan(offset, header_bytes +
                                                    static_cast<std::size_t>(payload_bytes)));
    if (!std::equal(digest.begin(), digest.end(), bytes.begin() +
                    static_cast<std::ptrdiff_t>(payload_end))) {
      throw std::invalid_argument{"Native database frame checksum mismatch"};
    }
    auto commit_offset = payload_end + digest_bytes;
    if (database_get_word(bytes, commit_offset) != frame_commit) {
      throw std::invalid_argument{"Invalid native database commit marker"};
    }
    if (records.size() > state_->options.max_operations) {
      throw std::length_error{"Native database operation retention budget exceeded"};
    }
    records.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                         bytes.begin() + static_cast<std::ptrdiff_t>(payload_end));
    offset += frame_bytes;
  }
  state_->valid_bytes = offset;
  state_->repair_pending = offset != bytes.size();
  return records;
}

// Truncate only after the engine validates all committed native states; sync before admitting work.
void native_database_file::finish_recovery() {
  if (state_->repair_pending) {
    state_->data->truncate(state_->valid_bytes);
    state_->repair_pending = false;
  } else {
    state_->data->seek(state_->valid_bytes);
  }
  // A complete frame may have survived an unacknowledged pre-flush write. Recovery promotes
  // it to accepted durable truth before any exact retry can report its stored outcome.
  state_->data->sync();
}

// Allocate/check the entire frame before the first write; every later failure is uncertain.
void native_database_file::append(std::span<const std::uint8_t> payload) {
  if (state_->uncertain || state_->repair_pending) {
    throw journal_indeterminate_error{"Native database file requires recovery"};
  }
  if (payload.size() > state_->options.max_record_bytes ||
      payload.size() > std::numeric_limits<std::size_t>::max() - frame_overhead_bytes) {
    throw std::length_error{"Native database record budget exceeded"};
  }
  const auto frame_bytes = payload.size() + frame_overhead_bytes;
  if (state_->valid_bytes > state_->options.max_file_bytes ||
      frame_bytes > state_->options.max_file_bytes - state_->valid_bytes) {
    throw std::length_error{"Native database file budget exceeded"};
  }
  const auto bytes = encode_frame(payload);
  state_->fault(database_io_event::before_write);
  state_->data->seek(state_->valid_bytes);
  state_->uncertain = true;
  try {
    state_->data->write(reinterpret_cast<const char*>(bytes.data()), stream_size(bytes.size()));
    state_->fault(database_io_event::after_write);
    state_->data->sync();
    state_->fault(database_io_event::after_flush);
  } catch (...) {
    std::throw_with_nested(journal_indeterminate_error{"Native database append outcome is uncertain"});
  }
  state_->valid_bytes += bytes.size();
  state_->uncertain = false;
}

// Stage a fully synced replacement while the original stable writer lock covers both generations.
void native_database_file::rebaseline(std::span<const std::uint8_t> payload,
                                     const std::filesystem::path& archive) {
  if (state_->uncertain || state_->repair_pending) {
    throw journal_indeterminate_error{"Native database requires recovery before rebaseline"};
  }
  if (payload.size() > state_->options.max_record_bytes ||
      payload.size() > std::numeric_limits<std::size_t>::max() - frame_overhead_bytes ||
      payload.size() + frame_overhead_bytes > state_->options.max_file_bytes) {
    throw std::length_error{"Native database rebaseline budget exceeded"};
  }
  const auto bytes = encode_frame(payload);
  const rebaseline_directory staging{state_->data_path};
  const auto stage_path = staging.baseline();
  auto replacement = std::make_unique<file_stream>(stage_path, file_open_mode::create);
  replacement->write(reinterpret_cast<const char*>(bytes.data()), stream_size(bytes.size()));
  replacement->sync();
  // The archive is deliberately create-only. A failed or interrupted attempt never overwrites
  // another retained generation, and no old bytes are removed as part of migration.
  {
    file_stream retained{archive, file_open_mode::create};
    constexpr std::size_t copy_buffer_bytes = 64 * 1024;
    std::array<char, copy_buffer_bytes> buffer{};
    std::uint64_t remaining = state_->valid_bytes;
    state_->data->seek(0);
    while (remaining != 0) {
      const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, buffer.size()));
      state_->data->read(buffer.data(), stream_size(count));
      if (state_->data->gcount() != stream_size(count)) {
        throw std::runtime_error{"Native database changed during rebaseline archive"};
      }
      retained.write(buffer.data(), stream_size(count));
      remaining -= count;
    }
    retained.sync();
  }
  sync_parent_directory(archive);
  state_->fault(database_io_event::after_rebaseline_archive);
  state_->fault(database_io_event::before_rebaseline_replace);
  state_->uncertain = true;
  try {
    replace_file_generation(stage_path, state_->data_path);
    sync_parent_directory(state_->data_path);
    state_->fault(database_io_event::after_rebaseline_replace);
  } catch (...) {
    std::throw_with_nested(journal_indeterminate_error{"Native database rebaseline requires reopen"});
  }
  // All resources and handles were acquired before replacement. Publication cannot allocate.
  state_->data.swap(replacement);
  state_->valid_bytes = bytes.size();
  state_->uncertain = false;
}

// Return the write fence independently of the stream's ordinary EOF/error flags.
bool native_database_file::needs_recovery() const noexcept { return state_->uncertain; }

} // namespace rohit::managed::detail
