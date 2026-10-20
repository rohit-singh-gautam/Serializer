// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include "../src/managed_journal_document.hpp"

#include <rohit/managed_journal_stream.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace detail = rohit::managed::detail;
using bytes = std::vector<std::uint8_t>;

// Independent byte offsets pin the external-document manifest's published wire layout.
constexpr std::size_t native_header_offset = 16;
constexpr std::size_t document_size_offset = 88;
constexpr std::size_t codec_size_offset = 128;
constexpr std::size_t manifest_fixed_bytes = 144;
constexpr std::size_t checksum_bytes = 8;
constexpr std::size_t metadata_budget_bytes = 1024;
constexpr std::array<std::uint8_t, 5> document_bytes{0, 1, 2, 0, 255};

// Bind deterministic native-header bytes to a document without depending on a model schema.
detail::journal_document_manifest make_manifest() {
  detail::journal_document_manifest result;
  std::iota(result.header.begin(), result.header.end(), std::uint8_t{});
  result.document_bytes = document_bytes.size();
  result.document_digest = rohit::make_digest<rohit::digest_algorithm::sha256>(document_bytes);
  result.codec = "example.document.v1";
  return result;
}

// Repair only the outer metadata CRC so malformed-field tests exercise semantic validation.
void update_checksum(bytes& encoded) {
  const auto offset = encoded.size() - checksum_bytes;
  detail::journal_put_word(encoded, offset, detail::checksum(std::span{encoded}.first(offset)));
}

// Every field and the exact binary document binding survive a manifest round trip.
TEST(managed_journal_document, round_trip_preserves_binding_and_native_header) {
  const auto manifest = make_manifest();
  const auto encoded = detail::encode_document_manifest(manifest, metadata_budget_bytes);
  EXPECT_EQ(encoded.size(), manifest_fixed_bytes + manifest.codec.size());
  const auto decoded = detail::decode_document_manifest(encoded, metadata_budget_bytes);
  EXPECT_EQ(decoded.header, manifest.header);
  EXPECT_EQ(decoded.document_bytes, manifest.document_bytes);
  EXPECT_EQ(decoded.document_digest, manifest.document_digest);
  EXPECT_EQ(decoded.codec, manifest.codec);
  EXPECT_TRUE(detail::document_matches(decoded, document_bytes));
  EXPECT_EQ(detail::get_word(encoded, 0), std::uint64_t{0x33434f444a5a5253});
  EXPECT_EQ(detail::get_word(encoded, 1), 1u);
}

// Each changed byte, including native metadata and the checksum itself, invalidates the manifest.
TEST(managed_journal_document, metadata_checksum_rejects_each_corrupted_byte) {
  const auto encoded = detail::encode_document_manifest(make_manifest(), metadata_budget_bytes);
  for (std::size_t index = 0; index < encoded.size(); ++index) {
    auto damaged = encoded;
    damaged[index] ^= 1;
    EXPECT_THROW(detail::decode_document_manifest(damaged, metadata_budget_bytes),
                 std::invalid_argument)
        << index;
  }
}

// A manifest is exact metadata: incomplete input and extra bytes never constitute a valid record.
TEST(managed_journal_document, rejects_all_truncations_and_trailing_bytes) {
  const auto encoded = detail::encode_document_manifest(make_manifest(), metadata_budget_bytes);
  for (std::size_t length = 0; length < encoded.size(); ++length) {
    EXPECT_THROW(
        detail::decode_document_manifest(std::span{encoded}.first(length), metadata_budget_bytes),
        std::invalid_argument)
        << length;
  }
  auto trailing = encoded;
  trailing.insert(trailing.end() - checksum_bytes, 0);
  update_checksum(trailing);
  EXPECT_THROW(detail::decode_document_manifest(trailing, metadata_budget_bytes),
               std::invalid_argument);
}

// Correct CRCs cannot authorize unsupported protocol versions or forged codec extents.
TEST(managed_journal_document, rejects_unknown_version_and_forged_extents) {
  const auto encoded = detail::encode_document_manifest(make_manifest(), metadata_budget_bytes);
  for (const auto offset : {std::size_t{}, std::size_t{8}}) {
    auto unsupported = encoded;
    detail::journal_put_word(unsupported, offset, 2);
    update_checksum(unsupported);
    EXPECT_THROW(detail::decode_document_manifest(unsupported, metadata_budget_bytes),
                 std::invalid_argument);
  }
  for (const std::uint64_t length :
       {std::uint64_t{}, std::uint64_t{257}, std::numeric_limits<std::uint64_t>::max()}) {
    auto malformed = encoded;
    detail::journal_put_word(malformed, codec_size_offset, length);
    update_checksum(malformed);
    EXPECT_THROW(detail::decode_document_manifest(malformed, metadata_budget_bytes),
                 std::invalid_argument);
  }
}

// Codec IDs are nonempty exact identifiers, bounded independently of the operation's byte budget.
TEST(managed_journal_document, validates_codec_identifier_bounds) {
  auto manifest = make_manifest();
  manifest.codec.clear();
  EXPECT_THROW(detail::encode_document_manifest(manifest, metadata_budget_bytes),
               std::invalid_argument);
  manifest.codec.assign(detail::max_codec_id_bytes, 'x');
  const auto encoded = detail::encode_document_manifest(manifest, metadata_budget_bytes);
  EXPECT_EQ(detail::decode_document_manifest(encoded, metadata_budget_bytes).codec, manifest.codec);
  manifest.codec.push_back('x');
  EXPECT_THROW(detail::encode_document_manifest(manifest, metadata_budget_bytes),
               std::invalid_argument);
}

// Metadata budgets apply before output allocation or input parsing and permit an exact fit.
TEST(managed_journal_document, enforces_encoded_metadata_budget) {
  const auto manifest = make_manifest();
  const auto required = manifest_fixed_bytes + manifest.codec.size();
  const auto encoded = detail::encode_document_manifest(manifest, required);
  EXPECT_NO_THROW(detail::decode_document_manifest(encoded, required));
  for (const auto budget : {std::size_t{}, manifest_fixed_bytes - 1, required - 1}) {
    EXPECT_THROW(detail::encode_document_manifest(manifest, budget), std::length_error);
    EXPECT_THROW(detail::decode_document_manifest(encoded, budget), std::length_error);
  }
}

// Equal lengths are insufficient: changed document contents must fail the saved-baseline binding.
TEST(managed_journal_document, rejects_stale_documents_by_length_and_digest) {
  const auto manifest = make_manifest();
  auto changed = document_bytes;
  changed.back() ^= 1;
  EXPECT_FALSE(detail::document_matches(manifest, changed));
  EXPECT_FALSE(detail::document_matches(manifest, std::span{document_bytes}.first(4)));
  EXPECT_FALSE(detail::document_matches(manifest, {}));
  auto empty = manifest;
  empty.document_bytes = 0;
  empty.document_digest =
      rohit::make_digest<rohit::digest_algorithm::sha256>(std::span<const std::uint8_t>{});
  EXPECT_TRUE(detail::document_matches(empty, {}));
}

// Document lengths are inert metadata here; callers enforce document I/O budgets separately.
TEST(managed_journal_document, preserves_wide_document_extent_without_allocating_document) {
  auto encoded = detail::encode_document_manifest(make_manifest(), metadata_budget_bytes);
  detail::journal_put_word(encoded, document_size_offset,
                           std::numeric_limits<std::uint64_t>::max());
  encoded[native_header_offset] ^= 1;
  update_checksum(encoded);
  const auto decoded = detail::decode_document_manifest(encoded, metadata_budget_bytes);
  EXPECT_EQ(decoded.document_bytes, std::numeric_limits<std::uint64_t>::max());
  EXPECT_FALSE(detail::document_matches(decoded, document_bytes));
}

} // namespace
