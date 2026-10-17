// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::package {

inline constexpr std::size_t mebibyte = 1024 * 1024;
enum class error_code {
  unavailable, unsupported_feature, invalid_archive, invalid_name, duplicate_name,
  integrity, resource_limit, cancelled, io, invalid_state, invalid_metadata,
  missing_part, invalid_relationship, publication, publication_uncertain
};

// A package error separates archive offsets from the logical source part.
class error : public std::runtime_error {
public:
  const error_code code;
  const std::string part;
  const std::uint64_t offset_bytes;
  const std::uint64_t required;
  const std::uint64_t permitted;
  // Capture bounded diagnostics; messages never include payload contents.
  error(error_code code, const char* message, std::string part = {},
        std::uint64_t offset_bytes = 0, std::uint64_t required = 0,
        std::uint64_t permitted = 0);
};

struct limits {
  std::uint64_t max_archive_bytes{256 * mebibyte};
  std::uint64_t max_io_bytes{1024 * mebibyte};
  std::uint64_t max_metadata_bytes{16 * mebibyte};
  std::uint64_t max_part_bytes{64 * mebibyte};
  std::uint64_t max_total_expanded_bytes{256 * mebibyte};
  std::uint64_t max_buffer_bytes{128 * mebibyte};
  std::uint64_t max_entries{4096};
  std::uint64_t max_relationships{16384};
  std::function<bool()> is_cancelled{};
};

enum class resource { io_bytes, metadata_bytes, expanded_bytes, entries, relationships };

// One caller-owned, non-thread-safe budget follows nested archive/OPC operations.
// Counters are cumulative, including repeated reads; failed work is not refunded.
class budget {
  limits limits_;
  std::uint64_t io_bytes_{};
  std::uint64_t metadata_bytes_{};
  std::uint64_t expanded_bytes_{};
  std::uint64_t entries_{};
  std::uint64_t relationships_{};
public:
  // Keep the cancellation callback and immutable limits for this operation.
  explicit budget(limits configured = {});
  // Inspect the operation's configured bounds without resetting counters.
  [[nodiscard]] const limits& configured() const noexcept;
  // Inspect unspent cumulative capacity before a nested provider allocates storage.
  [[nodiscard]] std::uint64_t remaining(resource selected) const noexcept;
  // Check cancellation at a bounded processing boundary.
  void check_cancelled() const;
  // Charge work before allocation/I/O; failure never wraps a counter.
  void consume(resource selected, std::uint64_t count, std::string_view part = {});
  // Bound a simultaneous per-operation scratch allocation without charging retained metadata.
  void check_buffer(std::uint64_t count, std::string_view part = {}) const;
};

enum class zip_method : std::uint16_t { stored = 0, deflate = 8 };
struct entry_options {
  zip_method method{zip_method::stored};
  int compression_level{6};
};
struct zip_entry {
  std::string name;
  zip_method method{};
  std::uint64_t compressed_bytes{};
  std::uint64_t expanded_bytes{};
  std::uint32_t crc32{};
  std::uint16_t flags{};
  std::uint64_t header_offset_bytes{};
  std::uint64_t data_offset_bytes{};
};

// An archive writer borrows its sink; failure can leave a partial stream.
// finish() is required, fallible and never called by a destructor. One complete
// part is buffered for compression; no incremental model/record API is implied.
class zip_writer {
  std::ostream& output_;
  std::shared_ptr<budget> budget_;
  std::vector<zip_entry> entries_{};
  std::uint64_t offset_bytes_{};
  bool finished_{};
  bool failed_{};
  // Append archive bytes in bounded batches and mark fallible sink failures.
  void append(std::span<const std::uint8_t> bytes);
public:
  // Use a fresh operation budget unless the caller supplies a shared one.
  explicit zip_writer(std::ostream& output, std::shared_ptr<budget> shared = {});
  zip_writer(const zip_writer&) = delete;
  zip_writer& operator=(const zip_writer&) = delete;
  // Add one complete codec-independent part; duplicate logical names are rejected.
  void add(std::string_view name, std::span<const std::uint8_t> bytes,
           entry_options options = {});
  // Serialize a complete object through a bounded standard output stream, then add it.
  // The callback must not retain the stream; a thrown callback publishes no entry.
  void write(std::string_view name, const std::function<void(std::ostream&)>& producer,
             entry_options options = {});
  // Complete the central directory; repeated finish succeeds without additional output.
  void finish();
  // Inspect the cumulative operation budget for nested codec integrations.
  [[nodiscard]] const std::shared_ptr<budget>& shared_budget() const noexcept;
};

// A ZIP32 reader borrows a seekable input for its entire lifetime. Construction
// checks metadata and extents without decompressing payloads; read checks only
// the selected payload's integrity. No filesystem extraction ever occurs.
class zip_reader {
  std::istream& input_;
  std::shared_ptr<budget> budget_;
  std::vector<zip_entry> entries_{};
  std::uint64_t archive_bytes_{};
  // Read an exactly bounded archive range and preserve structured I/O failures.
  [[nodiscard]] std::vector<std::uint8_t> read_at(std::uint64_t offset, std::size_t count);
public:
  // Reject nonseekable sources, ZIP64, encryption, split archives and unsupported methods.
  explicit zip_reader(std::istream& input, std::shared_ptr<budget> shared = {});
  zip_reader(const zip_reader&) = delete;
  zip_reader& operator=(const zip_reader&) = delete;
  // Enumerate borrowed metadata; entries remain valid for the reader lifetime.
  [[nodiscard]] const std::vector<zip_entry>& entries() const noexcept;
  // Find an exact, case-sensitive ZIP identity; return null when absent.
  [[nodiscard]] const zip_entry* find(std::string_view name) const noexcept;
  // Expand one selected entry under the shared budget and verify its size and CRC.
  [[nodiscard]] std::vector<std::uint8_t> read(std::string_view name);
  // Verify all payloads without retaining decoded objects.
  void validate_all();
  // Inspect the operation budget without resetting its cumulative counters.
  [[nodiscard]] const std::shared_ptr<budget>& shared_budget() const noexcept;
};

struct part {
  std::string name;
  std::string content_type;
  std::uint64_t expanded_bytes{};
};
enum class target_mode { internal, external };
struct relationship {
  std::string source{"/"};
  std::string id;
  std::string type;
  std::string target;
  target_mode mode{target_mode::internal};
};

// Resolve one OPC internal URI reference without filesystem access or repeated decoding.
// Returned part names use a leading slash; root traversal and fragments are rejected.
[[nodiscard]] std::string resolve_target(std::string_view source, std::string_view target);

// A standard OPC builder generates infrastructure XML through Serializer's XML
// provider. Parts can use any codec; no history/journal/session parts are added.
class opc_writer {
  zip_writer archive_;
  std::vector<part> parts_{};
  std::set<std::string, std::less<>> identities_{};
  std::vector<relationship> relationships_{};
  std::vector<std::pair<std::string, std::string>> defaults_{};
  bool finished_{};
  bool failed_{};
public:
  // Require the optional XML provider before accepting OPC output.
  explicit opc_writer(std::ostream& output, std::shared_ptr<budget> shared = {});
  // Declare an extension default; per-part overrides remain authoritative.
  void content_type_default(std::string_view extension, std::string_view content_type);
  // Add an opaque or already encoded typed part using its OPC URI identity.
  void add_part(std::string_view name, std::string_view content_type,
                std::span<const std::uint8_t> bytes, entry_options options = {});
  // Encode a complete model to a bounded stream and store it as one typed part.
  void write_part(std::string_view name, std::string_view content_type,
                  const std::function<void(std::ostream&)>& producer,
                  entry_options options = {});
  // Add a source-local relationship; targets are validated at finish for forward references.
  void add_relationship(relationship link);
  // Validate references and emit [Content_Types].xml/.rels and the ZIP directory.
  void finish();
  // Share limits/cancellation with caller codecs and reference checking.
  [[nodiscard]] const std::shared_ptr<budget>& shared_budget() const noexcept;
};

// An OPC reader validates metadata/relationships eagerly, payload integrity selectively.
// External targets remain data and are never fetched. Vocabulary validation belongs
// to the caller; this is an OPC subset, not a document-format/Office implementation.
class opc_reader {
  zip_reader archive_;
  std::vector<part> parts_{};
  std::vector<relationship> relationships_{};
public:
  // Require a content-types manifest and namespace-aware, bounded XML metadata parsing.
  explicit opc_reader(std::istream& input, std::shared_ptr<budget> shared = {});
  // Inspect non-infrastructure parts without decompressing their payloads.
  [[nodiscard]] const std::vector<part>& parts() const noexcept;
  // Inspect source-local relationships, including external targets as inert data.
  [[nodiscard]] const std::vector<relationship>& relationships() const noexcept;
  // Discover links by source and absolute relationship type; cardinality is caller policy.
  [[nodiscard]] std::vector<const relationship*> find_relationships(
      std::string_view source, std::string_view type) const;
  // Find a part by OPC identity, using OPC's ASCII case-insensitive comparison.
  [[nodiscard]] const part* find(std::string_view name) const;
  // Read one complete part; the returned bytes retain its selected codec representation.
  [[nodiscard]] std::vector<std::uint8_t> read(std::string_view name);
  // Check every entry's CRC and size; no typed object is published by this operation.
  void validate_all();
  // Share cumulative budgets with profile/codec validation performed by the caller.
  [[nodiscard]] const std::shared_ptr<budget>& shared_budget() const noexcept;
};

// Stage a full ZIP Save beside the destination and publish only after finish/close.
// Replacement syncs the file and parent directory; this is not a sidecar transaction.
// The callback must not retain the writer. Pre-publication errors preserve the old file.
void save_zip_file(const std::filesystem::path& destination,
                   const std::function<void(zip_writer&)>& producer, limits configured = {});
// Apply the same full-Save publication contract to a standard OPC builder.
void save_opc_file(const std::filesystem::path& destination,
                   const std::function<void(opc_writer&)>& producer, limits configured = {});

} // namespace rohit::serializer::package
