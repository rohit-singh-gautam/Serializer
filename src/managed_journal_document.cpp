// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include "managed_journal_document.hpp"

#include <rohit/managed_journal_stream.hpp>

#include <algorithm>
#include <stdexcept>

namespace rohit::managed::detail {
namespace {

constexpr std::uint64_t document_manifest_magic = 0x33434f444a5a5253;
constexpr std::uint64_t document_manifest_version = 1;
constexpr std::uint64_t origin_manifest_version = 2;
constexpr std::size_t native_header_offset = 2 * wire_word_bytes;
constexpr std::size_t document_size_offset = native_header_offset + journal_document_header_bytes;
constexpr std::size_t document_digest_offset = document_size_offset + wire_word_bytes;
constexpr std::size_t codec_size_offset =
    document_digest_offset + digest_size(digest_algorithm::sha256);
constexpr std::size_t codec_offset = codec_size_offset + wire_word_bytes;
constexpr std::size_t manifest_fixed_bytes = codec_offset + wire_word_bytes;
static_assert(manifest_fixed_bytes == journal_manifest_fixed_bytes);

static_assert(journal_document_header_bytes % wire_word_bytes == 0);
static_assert(codec_size_offset % wire_word_bytes == 0);

} // namespace

// Allocate only after checking the small codec extent and the complete metadata budget.
std::vector<std::uint8_t> encode_document_manifest(const journal_document_manifest& manifest,
                                                   std::size_t max_bytes,
                                                   std::size_t max_origin_bytes) {
  if (manifest.codec.empty() || manifest.codec.size() > max_codec_id_bytes) {
    throw std::invalid_argument{"Invalid managed journal document codec ID"};
  }
  if (manifest.origin.size() > max_origin_bytes ||
      (!manifest.confirmation.empty() && manifest.confirmation.size() != journal_save_token_bytes)) {
    throw std::length_error{"Managed journal origin or confirmation budget exceeded"};
  }
  const bool extended = !manifest.origin.empty() || !manifest.confirmation.empty();
  const auto extra = extended ? 2 * wire_word_bytes + manifest.confirmation.size() : 0;
  if (extra > max_bytes || manifest.origin.size() > max_bytes - extra ||
      manifest_fixed_bytes > max_bytes - extra - manifest.origin.size() ||
      manifest.codec.size() > max_bytes - extra - manifest.origin.size() - manifest_fixed_bytes) {
    throw std::length_error{"Managed journal document manifest budget exceeded"};
  }
  std::vector<std::uint8_t> output;
  output.reserve(manifest_fixed_bytes + manifest.codec.size() + extra + manifest.origin.size());
  put_word(output, document_manifest_magic);
  put_word(output, extended ? origin_manifest_version : document_manifest_version);
  output.insert(output.end(), manifest.header.begin(), manifest.header.end());
  put_word(output, manifest.document_bytes);
  output.insert(output.end(), manifest.document_digest.begin(), manifest.document_digest.end());
  put_word(output, manifest.codec.size());
  output.insert(output.end(), manifest.codec.begin(), manifest.codec.end());
  if (extended) {
    put_word(output, manifest.origin.size());
    output.insert(output.end(), manifest.origin.begin(), manifest.origin.end());
    put_word(output, manifest.confirmation.size());
    output.insert(output.end(), manifest.confirmation.begin(), manifest.confirmation.end());
  }
  put_word(output, checksum(output));
  return output;
}

// Treat every extent as untrusted and validate integrity before constructing owning metadata.
journal_document_manifest decode_document_manifest(std::span<const std::uint8_t> input,
                                                   std::size_t max_bytes,
                                                   std::size_t max_origin_bytes) {
  if (input.size() > max_bytes) {
    throw std::length_error{"Managed journal document manifest budget exceeded"};
  }
  if (input.size() < manifest_fixed_bytes || get_word(input, 0) != document_manifest_magic ||
      (get_word(input, 1) != document_manifest_version && get_word(input, 1) != origin_manifest_version)) {
    throw std::invalid_argument{"Invalid managed journal document manifest"};
  }
  const auto codec_bytes = get_word(input, codec_size_offset / wire_word_bytes);
  if (codec_bytes == 0 || codec_bytes > max_codec_id_bytes ||
      codec_bytes > input.size() - manifest_fixed_bytes ||
      (get_word(input, 1) == document_manifest_version &&
       codec_bytes != input.size() - manifest_fixed_bytes)) {
    throw std::invalid_argument{"Invalid managed journal document codec extent"};
  }
  const auto content = input.first(input.size() - wire_word_bytes);
  const auto footer = input.last(wire_word_bytes);
  if (get_word(footer, 0) != checksum(content)) {
    throw std::invalid_argument{"Corrupt managed journal document manifest"};
  }
  journal_document_manifest result;
  std::copy_n(input.begin() + native_header_offset, result.header.size(), result.header.begin());
  result.document_bytes = get_word(input, document_size_offset / wire_word_bytes);
  std::copy_n(input.begin() + document_digest_offset, result.document_digest.size(),
              result.document_digest.begin());
  const auto codec = input.subspan(codec_offset, static_cast<std::size_t>(codec_bytes));
  result.codec.assign(codec.begin(), codec.end());
  if (get_word(input, 1) == origin_manifest_version) {
    // Check both opaque extents against the already checksummed input before allocating either.
    auto offset = codec_offset + static_cast<std::size_t>(codec_bytes);
    const auto end = input.size() - wire_word_bytes;
    if (end - offset < 2 * wire_word_bytes) {
      throw std::invalid_argument{"Truncated managed journal origin metadata"};
    }
    const auto origin_size = get_word(input.subspan(offset), 0);
    offset += wire_word_bytes;
    if (origin_size > max_origin_bytes || origin_size > end - offset - wire_word_bytes) {
      throw std::length_error{"Managed journal origin budget exceeded"};
    }
    const auto origin = input.subspan(offset, static_cast<std::size_t>(origin_size));
    offset += origin.size();
    const auto confirmation_size = get_word(input.subspan(offset), 0);
    offset += wire_word_bytes;
    if ((confirmation_size != 0 && confirmation_size != journal_save_token_bytes) ||
        confirmation_size != end - offset) {
      throw std::invalid_argument{"Invalid managed journal confirmation extent"};
    }
    result.origin.assign(origin.begin(), origin.end());
    result.confirmation.assign(input.begin() + offset, input.begin() + end);
  }
  return result;
}

// A different byte count avoids hashing; equal lengths still require the exact saved-byte digest.
bool document_matches(const journal_document_manifest& manifest,
                      std::span<const std::uint8_t> document) {
  return manifest.document_bytes == document.size() &&
         manifest.document_digest == make_digest<digest_algorithm::sha256>(document);
}

} // namespace rohit::managed::detail
