// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include "../src/managed_journal_delta.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using byte_vector = std::vector<std::uint8_t>;
using rohit::managed::detail::journal_delta_base;
// Independent fixture offsets pin the sidecar-delta wire contract rather than native object sizes.
constexpr std::size_t wire_word_bytes = 8;
constexpr std::size_t delta_header_bytes = 40;
constexpr std::size_t base_bytes_offset = 8;
constexpr std::size_t base_digest_offset = 16;
constexpr std::size_t target_bytes_offset = 24;
constexpr std::size_t instruction_count_offset = 32;
constexpr std::size_t copy_offset_position = delta_header_bytes + 1;
constexpr std::size_t copy_length_position = copy_offset_position + wire_word_bytes;
constexpr std::size_t test_record_budget_bytes = 1024 * 1024;

// Produce reproducible incompressible bytes so matching tests cannot pass through repeated fixtures.
byte_vector make_bytes(std::size_t size, std::uint64_t state = 0xd1b54a32d192ed03) {
  constexpr std::uint64_t multiplier = 0x2545f4914f6cdd1d;
  byte_vector result(size);
  for (auto& byte : result) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    byte = static_cast<std::uint8_t>((state * multiplier) >> 56);
  }
  return result;
}

// Rewrite one independently specified little-endian fixture field.
void set_word(byte_vector& patch, std::size_t offset, std::uint64_t value) {
  ASSERT_LE(wire_word_bytes, patch.size());
  ASSERT_LE(offset, patch.size() - wire_word_bytes);
  for (std::size_t index = 0; index < wire_word_bytes; ++index) {
    patch[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
  }
}

// Decode fixture lengths to inspect output references without sharing production parser code.
std::uint64_t read_word(const byte_vector& patch, std::size_t offset) {
  std::uint64_t result = 0;
  for (std::size_t index = 0; index < wire_word_bytes; ++index) {
    result |= static_cast<std::uint64_t>(patch.at(offset + index)) << (index * 8);
  }
  return result;
}

// Confirm a compact identical-target fixture contains the one expected copy-base instruction.
byte_vector baseline_copy(const journal_delta_base& dictionary) {
  auto patch = dictionary.encode({dictionary.bytes()}, test_record_budget_bytes);
  EXPECT_EQ(patch.size(), delta_header_bytes + 1 + 2 * wire_word_bytes);
  EXPECT_EQ(patch.at(delta_header_bytes), 2);
  return patch;
}

// Distant edits and shifted record/envelope layouts reuse the same unchanged saved spans.
TEST(managed_journal_delta_test, sparse_changes_and_structural_shifts_copy_saved_bytes) {
  const auto baseline = make_bytes(32768);
  journal_delta_base dictionary{baseline};
  auto target = baseline;
  target[4096] ^= 1;
  target[24576] ^= 1;
  target.erase(target.begin() + 16384, target.begin() + 16421);
  const auto prefix = make_bytes(29, 17);
  target.insert(target.begin(), prefix.begin(), prefix.end());
  const auto patch = dictionary.encode({target}, test_record_budget_bytes);
  EXPECT_LT(patch.size(), 1024);
  EXPECT_EQ(dictionary.decode(patch, test_record_budget_bytes), target);
  EXPECT_EQ(dictionary.bytes(), baseline);
}

// Matching crosses the caller's metadata/label/snapshot boundaries without retaining borrowed parts.
TEST(managed_journal_delta_test, split_logical_parts_form_one_differential_record) {
  const auto baseline = make_bytes(4096);
  journal_delta_base dictionary{baseline};
  auto target = baseline;
  target.insert(target.begin(), {3, 4, 5, 6, 7});
  const auto bytes = std::span<const std::uint8_t>{target};
  const auto patch = dictionary.encode({bytes.first(11), {}, bytes.subspan(11, 37),
                                         bytes.subspan(48)}, test_record_budget_bytes);
  EXPECT_LT(patch.size(), 256);
  EXPECT_EQ(dictionary.decode(patch, test_record_budget_bytes), target);
}

// Repeated newly added payloads are stored once even when absent from the saved baseline.
TEST(managed_journal_delta_test, repeated_new_payload_uses_completed_output_references) {
  journal_delta_base dictionary{{}};
  const auto payload = make_bytes(4096);
  auto target = payload;
  target.insert(target.end(), payload.begin(), payload.end());
  target.insert(target.end(), payload.begin(), payload.end());
  const auto patch = dictionary.encode({target}, test_record_budget_bytes);
  EXPECT_LT(patch.size(), payload.size() + 256);
  EXPECT_EQ(dictionary.decode(patch, test_record_budget_bytes), target);
  bool saw_output_copy = false;
  auto cursor = delta_header_bytes;
  const auto count = read_word(patch, instruction_count_offset);
  for (std::uint64_t index = 0; index < count; ++index) {
    const auto instruction = patch.at(cursor++);
    if (instruction == 1) {
      const auto length = read_word(patch, cursor);
      cursor += wire_word_bytes + static_cast<std::size_t>(length);
    } else {
      saw_output_copy = saw_output_copy || instruction == 3;
      cursor += 2 * wire_word_bytes;
    }
  }
  EXPECT_TRUE(saw_output_copy);
}

// Each patch names the original saved dictionary and does not depend on intervening records.
TEST(managed_journal_delta_test, independent_records_do_not_form_a_snapshot_chain) {
  const auto baseline = make_bytes(4096);
  journal_delta_base dictionary{baseline};
  byte_vector last_patch;
  byte_vector last_target;
  for (std::size_t index = 0; index < 100; ++index) {
    auto target = baseline;
    target[index * 31] ^= static_cast<std::uint8_t>(index + 1);
    auto patch = dictionary.encode({target}, test_record_budget_bytes);
    EXPECT_LT(patch.size(), 256);
    EXPECT_EQ(dictionary.decode(patch, test_record_budget_bytes), target);
    last_target = std::move(target);
    last_patch = std::move(patch);
  }
  journal_delta_base recovered{baseline};
  EXPECT_EQ(recovered.decode(last_patch, test_record_budget_bytes), last_target);
}

// Empty and sub-anchor payloads retain exact byte semantics through literal-only instructions.
TEST(managed_journal_delta_test, empty_and_small_records_round_trip) {
  for (std::size_t size = 0; size < 32; ++size) {
    journal_delta_base dictionary{make_bytes(size)};
    const auto target = make_bytes(size, 23);
    const auto patch = dictionary.encode({target}, test_record_budget_bytes);
    EXPECT_EQ(dictionary.decode(patch, test_record_budget_bytes), target);
  }
  journal_delta_base empty{{}};
  const auto patch = empty.encode({}, delta_header_bytes);
  EXPECT_EQ(patch.size(), delta_header_bytes);
  EXPECT_TRUE(empty.decode(patch, delta_header_bytes).empty());
}

// Repeated-byte dictionaries remain compact without collision chains or overlapping output copies.
TEST(managed_journal_delta_test, repetitive_saved_and_new_bytes_remain_compact) {
  journal_delta_base dictionary{byte_vector(65536, 0)};
  const byte_vector target(65536, 0);
  auto patch = dictionary.encode({target}, test_record_budget_bytes);
  EXPECT_EQ(patch.size(), delta_header_bytes + 1 + 2 * wire_word_bytes);
  EXPECT_EQ(dictionary.decode(patch, test_record_budget_bytes), target);
  journal_delta_base empty{{}};
  patch = empty.encode({target}, test_record_budget_bytes);
  EXPECT_LT(patch.size(), 512);
  EXPECT_EQ(empty.decode(patch, test_record_budget_bytes), target);
}

// Capped indexes keep sampling the late source rather than silently indexing only its beginning.
TEST(managed_journal_delta_test, capped_index_reuses_saved_data_near_end_of_large_baseline) {
  constexpr std::size_t baseline_bytes = 8 * 1024 * 1024;
  constexpr std::size_t retained_bytes = 64 * 1024;
  const auto baseline = make_bytes(baseline_bytes);
  journal_delta_base dictionary{baseline};
  auto target = make_bytes(23, 31);
  target.insert(target.end(), baseline.end() - retained_bytes, baseline.end());
  target[retained_bytes / 2] ^= 1;
  const auto patch = dictionary.encode({target}, test_record_budget_bytes);
  EXPECT_LT(patch.size(), 512);
  EXPECT_EQ(dictionary.decode(patch, test_record_budget_bytes), target);
}

// Identical sizes cannot bind a patch to different saved content, and every identity field is checked.
TEST(managed_journal_delta_test, foreign_baselines_and_malformed_identity_are_rejected) {
  const auto baseline = make_bytes(256);
  journal_delta_base dictionary{baseline};
  const auto patch = baseline_copy(dictionary);
  auto foreign = baseline;
  foreign.back() ^= 1;
  journal_delta_base other{foreign};
  EXPECT_THROW(other.decode(patch, test_record_budget_bytes), std::invalid_argument);
  for (const auto offset : {std::size_t{0}, base_bytes_offset, base_digest_offset}) {
    auto damaged = patch;
    damaged[offset] ^= 1;
    EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
  }
}

// Copy ranges reject zero progress, overflow-sized extents, future output, and target overruns.
TEST(managed_journal_delta_test, malformed_copy_ranges_are_rejected) {
  journal_delta_base dictionary{make_bytes(256)};
  const auto patch = baseline_copy(dictionary);
  const auto maximum_word = std::numeric_limits<std::uint64_t>::max();
  for (const auto offset : {std::uint64_t{256}, maximum_word}) {
    auto damaged = patch;
    set_word(damaged, copy_offset_position, offset);
    EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
  }
  for (const auto length : {std::uint64_t{0}, std::uint64_t{257}, maximum_word}) {
    auto damaged = patch;
    set_word(damaged, copy_length_position, length);
    EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
  }
  auto damaged = patch;
  damaged[delta_header_bytes] = 3;
  EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
  damaged = patch;
  set_word(damaged, target_bytes_offset, 255);
  EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
}

// Output references cannot overlap their destination even when the claimed final target is large.
TEST(managed_journal_delta_test, overlapping_output_reference_is_rejected) {
  journal_delta_base dictionary{{}};
  auto patch = dictionary.encode({byte_vector{1, 2, 3}}, test_record_budget_bytes);
  set_word(patch, target_bytes_offset, 7);
  set_word(patch, instruction_count_offset, 2);
  patch.push_back(3);
  for (const auto word : {std::uint64_t{0}, std::uint64_t{4}}) {
    for (std::size_t index = 0; index < wire_word_bytes; ++index) {
      patch.push_back(static_cast<std::uint8_t>(word >> (index * 8)));
    }
  }
  EXPECT_THROW(dictionary.decode(patch, test_record_budget_bytes), std::invalid_argument);
}

// Framing rejects missing fields, instruction-count amplification, unknown tags, and trailing bytes.
TEST(managed_journal_delta_test, malformed_framing_and_every_truncation_are_rejected) {
  journal_delta_base dictionary{make_bytes(256)};
  const auto patch = baseline_copy(dictionary);
  for (std::size_t size = 0; size < patch.size(); ++size) {
    const auto truncated = std::span<const std::uint8_t>{patch}.first(size);
    EXPECT_THROW(dictionary.decode(truncated, test_record_budget_bytes), std::invalid_argument);
  }
  auto damaged = patch;
  set_word(damaged, instruction_count_offset, std::numeric_limits<std::uint64_t>::max());
  EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
  damaged = patch;
  damaged[delta_header_bytes] = 255;
  EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
  damaged = patch;
  damaged.push_back(0);
  EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
  damaged = patch;
  set_word(damaged, target_bytes_offset, 257);
  EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
}

// Literal admission validates its positive length and physical input before copying any bytes.
TEST(managed_journal_delta_test, malformed_literal_lengths_are_rejected) {
  journal_delta_base dictionary{{}};
  const auto patch = dictionary.encode({byte_vector{1, 2, 3}}, test_record_budget_bytes);
  for (const auto length : {std::uint64_t{0}, std::uint64_t{4},
                            std::numeric_limits<std::uint64_t>::max()}) {
    auto damaged = patch;
    set_word(damaged, delta_header_bytes + 1, length);
    EXPECT_THROW(dictionary.decode(damaged, test_record_budget_bytes), std::invalid_argument);
  }
}

// Both encoded and expanded budgets apply; a small compressed patch cannot bypass target limits.
TEST(managed_journal_delta_test, physical_and_expanded_record_budgets_are_enforced) {
  journal_delta_base dictionary{make_bytes(4096)};
  const auto patch = baseline_copy(dictionary);
  EXPECT_THROW(dictionary.encode({dictionary.bytes()}, dictionary.bytes().size() - 1),
                 std::length_error);
  EXPECT_THROW(dictionary.decode(patch, dictionary.bytes().size() - 1), std::length_error);
  EXPECT_THROW(dictionary.decode(patch, patch.size() - 1), std::length_error);
  EXPECT_THROW(dictionary.encode({}, delta_header_bytes - 1),
                 std::length_error);
  journal_delta_base small{make_bytes(16)};
  EXPECT_THROW(small.encode({small.bytes()}, delta_header_bytes + 16), std::length_error);
  journal_delta_base empty{{}};
  const auto literal = make_bytes(64);
  EXPECT_THROW(empty.encode({literal}, literal.size()), std::length_error);
  const auto encoded = empty.encode({literal}, 128);
  EXPECT_EQ(empty.decode(encoded, 128), literal);
  auto oversized_target = encoded;
  set_word(oversized_target, target_bytes_offset, std::numeric_limits<std::uint64_t>::max());
  EXPECT_THROW(empty.decode(oversized_target, 128), std::length_error);
}

} // namespace
