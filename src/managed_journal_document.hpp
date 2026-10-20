// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/digest.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rohit::managed::detail {

inline constexpr std::size_t journal_document_header_bytes = 72;
inline constexpr std::size_t max_codec_id_bytes = 256;

// Bind an external document's exact saved bytes and codec to its native journal generation.
// The native header remains opaque here; its container validation belongs to the file adapter.
struct journal_document_manifest {
  std::array<std::uint8_t, journal_document_header_bytes> header{};
  std::uint64_t document_bytes{};
  std::array<std::uint8_t, digest_size(digest_algorithm::sha256)> document_digest{};
  std::string codec{};
};

// Encode bounded, checksummed metadata without reading or retaining document bytes.
// max_bytes caps the manifest itself; the caller separately bounds document input/output.
std::vector<std::uint8_t> encode_document_manifest(const journal_document_manifest& manifest,
                                                   std::size_t max_bytes);

// Validate the complete metadata, including its exact extent, before allocating the codec ID.
// Reject unknown versions, malformed/empty codec IDs, corruption and trailing bytes.
journal_document_manifest decode_document_manifest(std::span<const std::uint8_t> input,
                                                   std::size_t max_bytes);

// Check exact document size and SHA-256 binding; no document bytes are retained or decoded.
bool document_matches(const journal_document_manifest& manifest,
                      std::span<const std::uint8_t> document);

} // namespace rohit::managed::detail
