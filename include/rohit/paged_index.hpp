// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/cache_codec.hpp>
#include <rohit/paged_index_file.hpp>
#include <rohit/paged_index_records.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rohit::managed {

// A derived index belongs to one exact document/schema/definition/source snapshot, never latest.
struct paged_index_binding {
  std::uint64_t document_high{};
  std::uint64_t document_low{};
  std::uint64_t authority_epoch{};
  std::uint64_t source_revision{};
  std::array<std::uint8_t, digest_size(digest_algorithm::sha256)> schema_digest{};
  std::array<std::uint8_t, digest_size(digest_algorithm::sha256)> definition_digest{};
  std::array<std::uint8_t, digest_size(digest_algorithm::sha256)> source_digest{};
  bool operator==(const paged_index_binding&) const = default;
};

namespace detail {

// Serialize durable binding through its versioned generated schema and ordinary bounded codec.
inline std::vector<std::uint8_t> encode_paged_index_binding(const paged_index_binding& binding) {
  if ((binding.document_high == 0 && binding.document_low == 0) || binding.authority_epoch == 0) {
    throw std::invalid_argument{"Paged index requires a document namespace and authority epoch"};
  }
  page_records::source_binding record;
  record.format_version = 1;
  record.document_high = binding.document_high;
  record.document_low = binding.document_low;
  record.authority_epoch = binding.authority_epoch;
  record.source_revision = binding.source_revision;
  record.schema_digest = binding.schema_digest;
  record.definition_digest = binding.definition_digest;
  record.source_digest = binding.source_digest;
  return cache::encode(record);
}

// Keep manifest serialization outside the compiler-independent transport build graph.
inline std::vector<std::uint8_t> encode_paged_index_metadata(const paged_index_metadata& metadata) {
  constexpr std::size_t max_manifest_bytes = 64 * 1024;
  page_records::index_manifest record;
  record.format_version = 1;
  record.comparator_version = 1;
  record.page_bytes = metadata.page_bytes;
  record.page_count = metadata.page_count;
  record.root_page = metadata.root_page;
  record.depth = metadata.depth;
  record.entries = metadata.entries;
  record.generation_high = metadata.generation_high;
  record.generation_low = metadata.generation_low;
  record.binding = metadata.binding;
  return cache::encode(record, max_manifest_bytes);
}

// Reject unknown versions and consume the exact bounded generated record before trusting fields.
inline paged_index_metadata decode_paged_index_metadata(std::span<const std::uint8_t> bytes) {
  serializer::decode_limits limits;
  limits.max_input_bytes = bytes.size();
  limits.max_allocation_bytes = bytes.size();
  limits.max_collection_elements = bytes.size();
  const auto record = cache::decode<page_records::index_manifest>(bytes, limits);
  if (record.format_version != 1 || record.comparator_version != 1) {
    throw std::invalid_argument{"Unsupported paged index manifest or numeric comparator version"};
  }
  return {record.page_bytes, record.page_count, record.root_page, record.depth, record.entries,
          record.generation_high, record.generation_low, record.binding};
}
} // namespace detail

// Immutable derived uint64 index. It locates identities; authoritative loading, permissions,
// uniqueness and transaction acceptance stay with the source document's existing mechanisms.
class paged_index {
  paged_index_binding binding_;
  detail::paged_index_file file_;
public:
  using cursor = detail::paged_index_file::cursor;

  // Open only a generation matching the exact expected source snapshot and definition.
  paged_index(const std::filesystem::path& path, paged_index_binding binding,
              paged_index_options options = {})
      : binding_{std::move(binding)}, file_{path, detail::encode_paged_index_binding(binding_),
          detail::decode_paged_index_metadata, options} {}
  paged_index(const paged_index&) = delete;
  paged_index& operator=(const paged_index&) = delete;
  // Transfer the immutable generation; previously acquired cursors retain their own pins.
  paged_index(paged_index&&) noexcept = default;
  // Replace an index owner without invalidating other generation-bound cursors.
  paged_index& operator=(paged_index&&) noexcept = default;

  // Externally sort an unsorted source stream using bounded runs/merge buffers and disk budgets.
  // Source callbacks are read-only and synchronous; no model edits/business effects may run here.
  static paged_index_build_statistics build(const std::filesystem::path& path,
      const paged_index_binding& binding, const detail::paged_index_source& source,
      const paged_index_options& options = {}) {
    const auto encoded = detail::encode_paged_index_binding(binding);
    return detail::build_paged_index(path, encoded, source, detail::encode_paged_index_metadata, options);
  }

  // Accept generated traversal's synchronous push shape without materializing a key catalog.
  static paged_index_build_statistics build_from(const std::filesystem::path& path,
      const paged_index_binding& binding, const detail::paged_index_producer& source,
      const paged_index_options& options = {}) {
    const auto encoded = detail::encode_paged_index_binding(binding);
    return detail::build_paged_index_from(path, encoded, source, detail::encode_paged_index_metadata, options);
  }

  // Query an inclusive numeric range through the bounded pageable directory and leaf cursor.
  cursor range(std::uint64_t lower, std::uint64_t upper) const { return file_.range(lower, upper); }
  // Query every identity for one exact numeric key without allocating a result vector.
  cursor equal(std::uint64_t key) const { return range(key, key); }
  // Inspect the source identity independently of mutable path names and current model state.
  const paged_index_binding& binding() const noexcept { return binding_; }
  // Return the number of source entries retained by this complete immutable generation.
  std::uint64_t size() const { return file_.metadata().entries; }
  // Return the number of fixed pages, excluding the manifest page.
  std::uint64_t page_count() const { return file_.metadata().page_count; }
  // Inspect controlled page buffers, pinned pages and I/O counts without claiming process limits.
  paged_index_statistics statistics() const { return file_.statistics(); }
};

} // namespace rohit::managed
