// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include "managed_journal_delta.hpp"

#include <rohit/managed_journal.hpp>
#include <rohit/managed_journal_stream.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rohit::managed::detail {
namespace {
using byte_vector = std::vector<std::uint8_t>;
constexpr std::uint64_t delta_magic = 0x3141544c444a5253; // "SRJDLTA1", little endian.
constexpr std::size_t delta_header_words = 5;
constexpr std::size_t delta_header_bytes = delta_header_words * wire_word_bytes;
constexpr std::size_t instruction_count_offset = 4 * wire_word_bytes;
constexpr std::size_t literal_prefix_bytes = 1 + wire_word_bytes;
constexpr std::size_t copy_instruction_bytes = 1 + 2 * wire_word_bytes;
constexpr std::size_t anchor_bytes = 16;
constexpr std::size_t anchor_stride_bytes = anchor_bytes;
constexpr std::size_t maximum_probes = 8;
constexpr std::size_t maximum_index_bytes = 16 * 1024 * 1024;
constexpr std::size_t empty_offset = std::numeric_limits<std::size_t>::max();
constexpr std::uint64_t hash_multiplier = 0x9e3779b185ebca87;
constexpr std::uint64_t hash_avalanche_multiplier = 0xc2b2ae3d27d4eb4f;
constexpr unsigned hash_shift_bits = 29;

enum class delta_instruction : std::uint8_t {
  literal = 1,
  copy_base = 2,
  copy_output = 3
};

struct anchor_slot {
  std::uint64_t fingerprint{};
  std::size_t first_offset{empty_offset};
  std::size_t last_offset{empty_offset};
};

// Read unaligned anchor words only after their complete span has been checked by the caller.
std::uint64_t anchor_fingerprint(std::span<const std::uint8_t> input, std::size_t offset) {
  static_assert(anchor_bytes == 2 * sizeof(std::uint64_t));
  std::uint64_t first{};
  std::uint64_t second{};
  std::memcpy(&first, input.data() + offset, sizeof(first));
  std::memcpy(&second, input.data() + offset + sizeof(first), sizeof(second));
  auto result = (first ^ (second * hash_multiplier)) * hash_avalanche_multiplier;
  result ^= result >> hash_shift_bits;
  return result;
}

// Limit both hash-table memory and every collision walk; missed matches affect size, never meaning.
class anchor_index {
  std::vector<anchor_slot> slots_{};
  std::size_t stride_bytes_{anchor_stride_bytes};

public:
  // Sample the complete source more sparsely when necessary to keep the index within its byte cap.
  explicit anchor_index(std::size_t input_bytes) {
    if (input_bytes < anchor_bytes) {
      return;
    }
    static_assert(maximum_index_bytes / sizeof(anchor_slot) >= 2);
    const auto index_slot_limit = std::min(maximum_index_bytes / sizeof(anchor_slot),
                                           slots_.max_size());
    std::size_t maximum_slots = 1;
    while (maximum_slots <= index_slot_limit / 2) {
      maximum_slots *= 2;
    }
    const auto maximum_anchors = maximum_slots / 2;
    const auto source_offsets = input_bytes - anchor_bytes + 1;
    // The subtraction form of ceiling division cannot overflow for a caller-sized source span.
    stride_bytes_ = std::max(anchor_stride_bytes, 1 + (source_offsets - 1) / maximum_anchors);
    const auto anchors = 1 + (input_bytes - anchor_bytes) / stride_bytes_;
    const auto desired_slots = anchors * 2;
    std::size_t slot_count = 1;
    while (slot_count < desired_slots) {
      if (slot_count > maximum_slots / 2) {
        throw std::length_error{"Managed journal delta index budget exceeded"};
      }
      slot_count *= 2;
    }
    slots_.resize(slot_count);
  }

  // Advance across the whole dictionary even when an unusually large source requires sparse sampling.
  std::size_t stride_bytes() const noexcept {
    return stride_bytes_;
  }

  // Keep the earliest and latest candidate for a fingerprint without building collision chains.
  void insert(std::uint64_t fingerprint, std::size_t offset) {
    if (slots_.empty()) {
      return;
    }
    const auto mask = slots_.size() - 1;
    auto slot_number = static_cast<std::size_t>(fingerprint) & mask;
    for (std::size_t probe = 0; probe < maximum_probes; ++probe) {
      auto& slot = slots_[slot_number];
      if (slot.first_offset == empty_offset) {
        slot = {fingerprint, offset, offset};
        return;
      }
      if (slot.fingerprint == fingerprint) {
        slot.last_offset = offset;
        return;
      }
      slot_number = (slot_number + 1) & mask;
    }
  }

  // Return only fixed-count candidates; byte comparison, not a fingerprint, admits each match.
  std::array<std::size_t, 2> find(std::uint64_t fingerprint) const {
    if (slots_.empty()) {
      return {empty_offset, empty_offset};
    }
    const auto mask = slots_.size() - 1;
    auto slot_number = static_cast<std::size_t>(fingerprint) & mask;
    for (std::size_t probe = 0; probe < maximum_probes; ++probe) {
      const auto& slot = slots_[slot_number];
      if (slot.first_offset == empty_offset) {
        break;
      }
      if (slot.fingerprint == fingerprint) {
        return {slot.first_offset, slot.last_offset};
      }
      slot_number = (slot_number + 1) & mask;
    }
    return {empty_offset, empty_offset};
  }
};

struct copy_match {
  delta_instruction instruction{delta_instruction::copy_base};
  std::size_t offset{};
  std::size_t length{};
};

// Prefer the longest verified copy; scans are charged to the target bytes consumed by that copy.
void consider_match(copy_match& best, std::span<const std::uint8_t> source,
                    std::span<const std::uint8_t> target, std::size_t target_offset,
                    std::size_t source_offset, std::size_t source_end,
                    delta_instruction instruction) {
  if (source_offset == empty_offset || source_offset > source_end ||
      anchor_bytes > source_end - source_offset) {
    return;
  }
  const auto maximum_length = std::min(source_end - source_offset, target.size() - target_offset);
  if (maximum_length <= best.length ||
      !std::equal(source.begin() + static_cast<std::ptrdiff_t>(source_offset),
                  source.begin() + static_cast<std::ptrdiff_t>(source_offset + anchor_bytes),
                  target.begin() + static_cast<std::ptrdiff_t>(target_offset))) {
    return;
  }
  auto length = anchor_bytes;
  while (length < maximum_length && source[source_offset + length] == target[target_offset + length]) {
    ++length;
  }
  if (length > best.length) {
    best = {instruction, source_offset, length};
  }
}

// Check additive output sizes before growing a patch or publishing any journal bytes.
void check_patch_growth(const byte_vector& output, std::size_t prefix_bytes,
                         std::size_t payload_bytes, std::size_t max_record_bytes) {
  if (output.size() > max_record_bytes || prefix_bytes > max_record_bytes - output.size() ||
      payload_bytes > max_record_bytes - output.size() - prefix_bytes) {
    throw std::length_error{"Managed journal delta record budget exceeded"};
  }
}

// Append one nonempty literal run and count its strictly positive decoding progress.
void append_literal(byte_vector& output, std::span<const std::uint8_t> literal,
                     std::size_t max_record_bytes, std::uint64_t& instruction_count) {
  if (literal.empty()) {
    return;
  }
  check_patch_growth(output, literal_prefix_bytes, literal.size(), max_record_bytes);
  output.push_back(static_cast<std::uint8_t>(delta_instruction::literal));
  put_word(output, literal.size());
  output.insert(output.end(), literal.begin(), literal.end());
  ++instruction_count;
}
} // namespace

struct journal_delta_base::implementation {
  byte_vector baseline{};
  std::uint64_t digest{};
  anchor_index index;

  // Construct the complete immutable index before exposing the dictionary to a journal writer.
  explicit implementation(byte_vector bytes)
      : baseline{std::move(bytes)}, digest{checksum(baseline)}, index{baseline.size()} {
    if (baseline.size() < anchor_bytes) {
      return;
    }
    const auto last_anchor = baseline.size() - anchor_bytes;
    for (std::size_t offset = 0;;) {
      index.insert(anchor_fingerprint(baseline, offset), offset);
      if (index.stride_bytes() > last_anchor - offset) {
        break;
      }
      offset += index.stride_bytes();
    }
  }
};

// Baseline identity and matching data are shared by every independently encoded record in a generation.
journal_delta_base::journal_delta_base(byte_vector baseline)
    : state_{std::make_unique<implementation>(std::move(baseline))} {}

// Keep the implementation private without requiring complete index types in its header.
journal_delta_base::~journal_delta_base() = default;

// Borrowing this immutable buffer does not affect independently returned patches or reconstructed data.
const byte_vector& journal_delta_base::bytes() const noexcept {
  return state_->baseline;
}

// Match any logical payload shape against saved bytes and previously produced bytes in this record.
byte_vector journal_delta_base::encode(
    std::initializer_list<std::span<const std::uint8_t>> parts,
    std::size_t max_record_bytes) const {
  if (max_record_bytes < delta_header_bytes) {
    throw std::length_error{"Managed journal delta record budget exceeded"};
  }
  std::size_t target_bytes = 0;
  for (const auto part : parts) {
    if (part.size() > max_record_bytes - target_bytes) {
      throw std::length_error{"Managed journal delta expansion budget exceeded"};
    }
    target_bytes += part.size();
  }
  byte_vector target;
  target.reserve(target_bytes);
  for (const auto part : parts) {
    target.insert(target.end(), part.begin(), part.end());
  }
  byte_vector output;
  output.reserve(delta_header_bytes);
  for (const auto word : {delta_magic, static_cast<std::uint64_t>(state_->baseline.size()),
                          state_->digest, static_cast<std::uint64_t>(target.size()),
                          std::uint64_t{0}}) {
    put_word(output, word);
  }
  anchor_index previous_output{target.size()};
  std::uint64_t instruction_count = 0;
  std::size_t cursor = 0;
  std::size_t literal_start = 0;
  std::size_t next_output_anchor = 0;
  while (cursor < target.size()) {
    // Only already produced bytes enter the output dictionary, including pending literal runs.
    while (next_output_anchor <= cursor && anchor_bytes <= cursor - next_output_anchor) {
      previous_output.insert(anchor_fingerprint(target, next_output_anchor), next_output_anchor);
      if (previous_output.stride_bytes() > target.size() - next_output_anchor) {
        next_output_anchor = target.size();
      } else {
        next_output_anchor += previous_output.stride_bytes();
      }
    }
    copy_match best;
    if (anchor_bytes <= target.size() - cursor) {
      const auto fingerprint = anchor_fingerprint(target, cursor);
      for (const auto offset : state_->index.find(fingerprint)) {
        consider_match(best, state_->baseline, target, cursor, offset, state_->baseline.size(),
                        delta_instruction::copy_base);
      }
      for (const auto offset : previous_output.find(fingerprint)) {
        consider_match(best, target, target, cursor, offset, cursor,
                        delta_instruction::copy_output);
      }
    }
    if (best.length == 0) {
      ++cursor;
      continue;
    }
    append_literal(output, std::span{target}.subspan(literal_start, cursor - literal_start),
                     max_record_bytes, instruction_count);
    check_patch_growth(output, copy_instruction_bytes, 0, max_record_bytes);
    output.push_back(static_cast<std::uint8_t>(best.instruction));
    put_word(output, best.offset);
    put_word(output, best.length);
    ++instruction_count;
    cursor += best.length;
    literal_start = cursor;
  }
  append_literal(output, std::span{target}.subspan(literal_start), max_record_bytes,
                   instruction_count);
  journal_put_word(output, instruction_count_offset, instruction_count);
  return output;
}

// Every instruction consumes bytes and produces a nonempty bounded range, preventing replay loops.
byte_vector journal_delta_base::decode(std::span<const std::uint8_t> patch,
                                        std::size_t max_record_bytes,
                                        std::size_t max_expanded_bytes) const {
  if (patch.size() > max_record_bytes) {
    throw std::length_error{"Managed journal delta record budget exceeded"};
  }
  if (patch.size() < delta_header_bytes) {
    throw std::invalid_argument{"Incomplete managed journal delta header"};
  }
  std::size_t cursor = 0;
  if (journal_get_word(patch, cursor) != delta_magic ||
      journal_get_word(patch, cursor) != state_->baseline.size() ||
      journal_get_word(patch, cursor) != state_->digest) {
    throw std::invalid_argument{"Managed journal delta has a foreign baseline"};
  }
  const auto target_bytes = journal_get_word(patch, cursor);
  const auto instruction_count = journal_get_word(patch, cursor);
  if (target_bytes > max_record_bytes || target_bytes > max_expanded_bytes) {
    throw std::length_error{"Managed journal delta expansion budget exceeded"};
  }
  if (instruction_count > target_bytes ||
      instruction_count > (patch.size() - cursor) / literal_prefix_bytes) {
    throw std::invalid_argument{"Invalid managed journal delta instruction count"};
  }
  byte_vector output(static_cast<std::size_t>(target_bytes));
  std::size_t produced = 0;
  for (std::uint64_t instruction_number = 0; instruction_number < instruction_count;
       ++instruction_number) {
    if (cursor == patch.size()) {
      throw std::invalid_argument{"Incomplete managed journal delta instruction"};
    }
    const auto instruction = static_cast<delta_instruction>(patch[cursor++]);
    if (instruction == delta_instruction::literal) {
      const auto length = journal_get_word(patch, cursor);
      if (length == 0 || length > output.size() - produced || length > patch.size() - cursor) {
        throw std::invalid_argument{"Invalid managed journal delta literal"};
      }
      std::copy_n(patch.data() + cursor, static_cast<std::size_t>(length), output.data() + produced);
      cursor += static_cast<std::size_t>(length);
      produced += static_cast<std::size_t>(length);
    } else if (instruction == delta_instruction::copy_base ||
               instruction == delta_instruction::copy_output) {
      const auto offset = journal_get_word(patch, cursor);
      const auto length = journal_get_word(patch, cursor);
      const auto source = instruction == delta_instruction::copy_base
                              ? std::span<const std::uint8_t>{state_->baseline}
                              : std::span<const std::uint8_t>{output}.first(produced);
      if (length == 0 || length > output.size() - produced || offset > source.size() ||
          length > source.size() - static_cast<std::size_t>(offset)) {
        throw std::invalid_argument{"Invalid managed journal delta copy range"};
      }
      std::copy_n(source.data() + static_cast<std::size_t>(offset), static_cast<std::size_t>(length),
                    output.data() + produced);
      produced += static_cast<std::size_t>(length);
    } else {
      throw std::invalid_argument{"Unknown managed journal delta instruction"};
    }
  }
  if (produced != output.size() || cursor != patch.size()) {
    throw std::invalid_argument{"Incomplete or trailing managed journal delta bytes"};
  }
  return output;
}

} // namespace rohit::managed::detail
