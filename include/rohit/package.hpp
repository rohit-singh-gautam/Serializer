// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::serializer::xml { struct operation_context; }

namespace rohit::serializer::package {

inline constexpr std::size_t mebibyte = 1024 * 1024;
enum class error_code {
  unavailable, unsupported_feature, invalid_archive, invalid_name, duplicate_name,
  integrity, resource_limit, cancelled, io, invalid_state, invalid_metadata,
  missing_part, invalid_relationship, publication, publication_uncertain,
  codec, invalid_reference, semantic, deadline_exceeded
};

// A package error separates archive offsets from the logical source part.
class error : public std::runtime_error {
public:
  const error_code code;
  const std::string part;
  const std::uint64_t offset_bytes;
  const std::uint64_t required;
  const std::uint64_t permitted;
  const std::string field_path;
  const std::string reference;
  const std::string relationship_id;
  // Capture bounded diagnostics; messages never include payload contents.
  error(error_code code, const char* message, std::string part = {},
        std::uint64_t offset_bytes = 0, std::uint64_t required = 0,
        std::uint64_t permitted = 0, std::string field_path = {},
        std::string reference = {}, std::string relationship_id = {});
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
  std::uint64_t max_decoder_allocation_bytes{128 * mebibyte};
  std::uint64_t max_work_units{512 * mebibyte};
  std::uint64_t max_references{65536};
  std::uint64_t max_preserved_bytes{64 * mebibyte};
  std::optional<std::chrono::steady_clock::time_point> deadline{};
};

enum class resource {
  io_bytes, metadata_bytes, expanded_bytes, entries, relationships,
  decoder_allocation_bytes, work_units, references, preserved_bytes
};

// One caller-owned, non-thread-safe budget follows nested archive/OPC operations.
// Counters are cumulative, including repeated reads; failed work is not refunded.
class budget {
  friend class buffer_reservation;
  limits limits_;
  std::uint64_t io_bytes_{};
  std::uint64_t metadata_bytes_{};
  std::uint64_t expanded_bytes_{};
  std::uint64_t entries_{};
  std::uint64_t relationships_{};
  std::uint64_t decoder_allocation_bytes_{};
  std::uint64_t work_units_{};
  std::uint64_t references_{};
  std::uint64_t preserved_bytes_{};
  std::uint64_t reserved_buffer_bytes_{};
public:
  // Keep the cancellation callback and immutable limits for this operation.
  explicit budget(limits configured = {});
  // Inspect the operation's configured bounds without resetting counters.
  [[nodiscard]] const limits& configured() const noexcept;
  // Inspect unspent cumulative capacity before a nested provider allocates storage.
  [[nodiscard]] std::uint64_t remaining(resource selected) const noexcept;
  // Inspect unreserved simultaneous scratch capacity for composable nested operations.
  [[nodiscard]] std::uint64_t available_buffer_bytes() const noexcept;
  // Check cancellation at a bounded processing boundary.
  void check_cancelled() const;
  // Charge work before allocation/I/O; failure never wraps a counter.
  void consume(resource selected, std::uint64_t count, std::string_view part = {});
  // Bound a simultaneous per-operation scratch allocation without charging retained metadata.
  void check_buffer(std::uint64_t count, std::string_view part = {}) const;
  // Borrow this budget into XML codecs; the returned context must not outlive the budget.
  [[nodiscard]] xml::operation_context xml_context(std::string_view part = {});
};

// Reserve caller-owned scratch across nested archive/compression operations on the same budget.
class buffer_reservation {
  budget& budget_;
  std::uint64_t bytes_;
public:
  // Refuse an over-limit reservation before the caller allocates its scratch storage.
  buffer_reservation(budget& shared, std::uint64_t bytes, std::string_view part = {});
  // Release only this live reservation; cumulative work counters are unaffected.
  ~buffer_reservation();
  buffer_reservation(const buffer_reservation&) = delete;
  buffer_reservation& operator=(const buffer_reservation&) = delete;
};

enum class zip_method : std::uint16_t { stored = 0, deflate = 8 };
struct entry_options {
  zip_method method{zip_method::stored};
  int compression_level{6};
  // Force standard ZIP64 size fields even for small entries (for fixed profile requirements).
  bool force_zip64{};
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
  bool zip64_sizes{};
  // APPNOTE central-directory metadata; attributes remain host-defined and are never OS permissions.
  std::uint8_t creator_system{};
  std::uint32_t external_attributes{};
};

struct input_options {
  // Nonseekable input is rejected unless this explicit caller-owned staging policy is set.
  std::filesystem::path spool_directory{};
  std::uint64_t max_spool_bytes{};
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
  bool opc_names_{};
  friend class opc_writer;
  // OPC URI segments permit colons; this policy is unavailable to ordinary archive callers.
  zip_writer(std::ostream& output, std::shared_ptr<budget> shared, bool opc_names);
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

// A ZIP32/ZIP64 reader borrows a seekable input for its entire lifetime. Construction
// checks metadata and extents without decompressing payloads; read checks only
// the selected payload's integrity. No filesystem extraction ever occurs.
class zip_reader {
  struct spool_state;
  std::unique_ptr<spool_state> spool_{};
  std::istream* input_;
  std::shared_ptr<budget> budget_;
  std::vector<zip_entry> entries_{};
  std::uint64_t archive_bytes_{};
  friend class opc_reader;
  // Apply the OPC part-URI naming policy only while indexing an OPC physical package.
  zip_reader(std::istream& input, std::shared_ptr<budget> shared,
             input_options options, bool opc_names);
  // Read an exactly bounded archive range and preserve structured I/O failures.
  [[nodiscard]] std::vector<std::uint8_t> read_at(std::uint64_t offset, std::size_t count);
public:
  // Optional nonseekable input is spooled to an exclusively created, bounded temporary file.
  // Reject encryption, split archives and unsupported methods; never spool implicitly.
  explicit zip_reader(std::istream& input, std::shared_ptr<budget> shared = {},
                      input_options options = {});
  // Release a private spool file, if selected, without affecting the borrowed source.
  ~zip_reader();
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
// Query/fragment components identify the whole part; dot segments follow RFC 3986.
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
  explicit opc_reader(std::istream& input, std::shared_ptr<budget> shared = {},
                      input_options options = {});
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

enum class unknown_policy { reject, skip, preserve };

struct opaque_part {
  part metadata;
  std::vector<std::uint8_t> bytes;
  entry_options storage{};
};

// Own optional unknown payloads verbatim with their authored relationship context.
// XML bytes retain their own namespace declarations; no lexical/signature guarantee is made
// for known rewritten parts or for the enclosing archive.
struct preserved_extensions {
  std::vector<opaque_part> parts{};
  std::vector<relationship> relationships{};
};

struct extension_policy {
  unknown_policy parts{unknown_policy::reject};
  unknown_policy relationships{unknown_policy::reject};
  // Profiles identify required capabilities explicitly; preserve/skip never accepts these.
  std::function<bool(const part&)> is_required_part{};
  std::function<bool(const relationship&)> is_required_relationship{};
};

// A caller-authored profile selects expected types by relationships, decodes fresh candidates,
// and supplies vocabulary/reference semantics. Callback codecs must charge allocations/work
// to shared_budget(); generated automatic codec-budget integration remains a separate feature.
class profile_loader {
  opc_reader& reader_;
  extension_policy policy_;
  std::set<std::string, std::less<>> decoded_{};
  std::set<const relationship*> selected_{};
  bool finished_{};
  bool failed_{};
public:
  // Borrow an OPC reader and its cumulative budget for one aggregate load.
  explicit profile_loader(opc_reader& reader, extension_policy policy = {});
  // Discover and decode a declared role; callback failure reports its source part/link.
  // A callback may throw package::error with field/reference details for precise diagnostics.
  void read_related(std::string_view source, std::string_view relationship_type,
      std::string_view expected_content_type,
      const std::function<void(std::span<const std::uint8_t>, const part&, budget&)>& decode,
      std::size_t minimum = 1, std::size_t maximum = 1);
  // Dynamic-MIME roles explicitly opt out of exact type matching while retaining OPC validation.
  void read_related(std::string_view source, std::string_view relationship_type,
      std::string_view expected_content_type,
      const std::function<void(std::span<const std::uint8_t>, const part&, budget&)>& decode,
      std::size_t minimum, std::size_t maximum, bool allow_any_content_type);
  // Validate every remaining payload's integrity, then apply optional extension policies.
  // Call only after application references and semantic checks succeed; no destination is changed.
  [[nodiscard]] preserved_extensions finish();
  // Share cumulative accounting with nested codecs, references and application validation.
  [[nodiscard]] budget& shared_budget() const noexcept;
};

template <typename Model>
struct aggregate_result {
  Model value{};
  preserved_extensions extensions{};
};

// Build a complete candidate and retained extensions before returning any typed result.
// The callback decodes required parts and performs reference/semantic checks on the candidate.
template <typename Model, typename Decode>
[[nodiscard]] aggregate_result<Model> load_opc(opc_reader& reader, Decode&& decode,
                                              extension_policy policy = {}) {
  aggregate_result<Model> candidate;
  profile_loader loader{reader, std::move(policy)};
  std::forward<Decode>(decode)(candidate.value, loader);
  candidate.extensions = loader.finish();
  return candidate;
}

// Commit the whole aggregate with one nonthrowing swap; every preparation failure preserves it.
template <typename Model, typename Decode>
  requires std::is_nothrow_swappable_v<aggregate_result<Model>>
void load_opc_into(opc_reader& reader, aggregate_result<Model>& destination, Decode&& decode,
                   extension_policy policy = {}) {
  auto candidate = load_opc<Model>(reader, std::forward<Decode>(decode), std::move(policy));
  using std::swap;
  swap(destination, candidate);
}

// Append retained opaque bytes/links after known edits; finish revalidates all target identities.
void write_preserved(opc_writer& writer, const preserved_extensions& extensions);
// Canonical writers sort complete supplied parts and relationships, use fixed ZIP metadata and
// preserve authored IDs. Byte identity requires identical codec bytes, compressor/version/options.
// The caller owns part bytes; aggregate scratch is bounded before any archive output is emitted.
void write_canonical_zip(std::ostream& output, std::span<const opaque_part> parts,
                          std::shared_ptr<budget> shared = {});
void write_canonical_opc(std::ostream& output, std::span<const opaque_part> parts,
                          std::span<const relationship> relationships,
                          std::shared_ptr<budget> shared = {});

// Stage a full ZIP Save beside the destination and publish only after finish/close.
// Replacement syncs the file and parent directory; this is not a sidecar transaction.
// The callback must not retain the writer. Pre-publication errors preserve the old file.
void save_zip_file(const std::filesystem::path& destination,
                   const std::function<void(zip_writer&)>& producer, limits configured = {});
// Apply the same full-Save publication contract to a standard OPC builder.
void save_opc_file(const std::filesystem::path& destination,
                   const std::function<void(opc_writer&)>& producer, limits configured = {});

} // namespace rohit::serializer::package
