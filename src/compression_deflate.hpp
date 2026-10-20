// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/compression.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace rohit::serializer::compression::detail {
namespace deflate_validation {
inline constexpr unsigned maximum_code_bits = 15;
inline constexpr std::size_t literal_alphabet_size = 288;
inline constexpr std::size_t distance_alphabet_size = 32;
inline constexpr std::size_t code_alphabet_size = 19;
inline constexpr std::size_t combined_alphabet_size =
    literal_alphabet_size + distance_alphabet_size;
inline constexpr std::size_t history_bytes = 32 * 1024;
inline constexpr std::size_t poll_work_mask = 4095;
inline constexpr std::size_t header_poll_mask = 64 * 1024 - 1;

// Reject malformed grammar without exposing payload excerpts or native provider diagnostics.
[[noreturn]] inline void invalid() {
  throw error{error_code::invalid_data, "Invalid or unsupported DEFLATE grammar"};
}

// Read required bits only, retaining the exact consumed byte extent and borrowed cancellation hook.
class bit_reader {
  const std::span<const std::uint8_t> bytes;
  const cancellation_callback check_cancelled;
  std::size_t offset;
  std::size_t work{};
  std::uint32_t pending{};
  unsigned available{};

public:
  // Borrow an already bounded raw DEFLATE extent; the source and hook outlive validation.
  bit_reader(std::span<const std::uint8_t> bytes, std::size_t offset,
             cancellation_callback check_cancelled)
      : bytes{bytes}, check_cancelled{check_cancelled}, offset{offset} {}

  // Poll regularly even for tiny-symbol streams that have not expanded into provider output yet.
  void poll() const {
    if (check_cancelled) {
      check_cancelled();
    }
  }

  // Decode at most sixteen little-endian wire bits without speculative reads or oversized shifts.
  unsigned read(unsigned count) {
    if ((++work & poll_work_mask) == 0) {
      poll();
    }
    while (available < count) {
      if (offset == bytes.size()) {
        invalid();
      }
      pending |= static_cast<std::uint32_t>(bytes[offset++]) << available;
      available += 8;
    }
    const auto value = pending & ((std::uint32_t{1} << count) - 1);
    pending >>= count;
    available -= count;
    return value;
  }

  // Stored blocks begin at the next byte; unread padding belongs to the current stream.
  void align() noexcept {
    pending = 0;
    available = 0;
  }

  // Skip a stored payload only after checking its exact input extent.
  void skip(std::size_t count) {
    if (count > bytes.size() - offset) {
      invalid();
    }
    offset += count;
  }

  // Report the ceil-byte boundary containing the final end-of-block symbol.
  [[nodiscard]] std::size_t position() const noexcept {
    return offset;
  }
};

enum class tree_kind { code_length, literal_length, distance };

// Fixed canonical tables require no dynamic allocation, decoded history or payload storage.
class tree {
  std::array<unsigned, maximum_code_bits + 1> counts{};
  std::array<unsigned, maximum_code_bits + 1> first{};
  std::array<unsigned, maximum_code_bits + 1> offsets{};
  std::array<unsigned, literal_alphabet_size> symbols{};
  unsigned maximum{};

public:
  // Reject oversubscribed/incomplete trees except the permitted single-bit or empty distance tree.
  void build(std::span<const unsigned> lengths, tree_kind kind) {
    if (lengths.size() > symbols.size()) {
      invalid();
    }
    *this = {};
    unsigned used{};
    for (const auto length : lengths) {
      if (length > maximum_code_bits) {
        invalid();
      }
      if (length != 0) {
        ++counts[length];
        ++used;
        maximum = std::max(maximum, length);
      }
    }
    if (used == 0) {
      if (kind != tree_kind::distance) {
        invalid();
      }
      return;
    }
    std::int32_t left = 1;
    for (unsigned length = 1; length <= maximum_code_bits; ++length) {
      left = (left << 1) - static_cast<std::int32_t>(counts[length]);
      if (left < 0) {
        invalid();
      }
    }
    if (left != 0 && (kind == tree_kind::code_length || maximum != 1)) {
      invalid();
    }
    unsigned code{}, offset{};
    std::array<unsigned, maximum_code_bits + 1> next{};
    for (unsigned length = 1; length <= maximum_code_bits; ++length) {
      code = (code + counts[length - 1]) << 1;
      first[length] = code;
      offsets[length] = offset;
      next[length] = offset;
      offset += counts[length];
    }
    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
      if (lengths[symbol] != 0) {
        symbols[next[lengths[symbol]]++] = static_cast<unsigned>(symbol);
      }
    }
  }

  // Decode canonical symbols in transmission order without reading beyond the current code.
  unsigned read(bit_reader& input) const {
    unsigned code{};
    for (unsigned length = 1; length <= maximum; ++length) {
      code = (code << 1) | input.read(1);
      if (code >= first[length] && code - first[length] < counts[length]) {
        return symbols[offsets[length] + code - first[length]];
      }
    }
    invalid();
  }
};

// Count expansion with checked subtraction before native provider allocation or decompression.
inline void add_expansion(std::size_t& expanded, std::size_t count, std::size_t maximum) {
  if (count > maximum - expanded) {
    throw error{error_code::resource_limit, "Decompressed message limit exceeded"};
  }
  expanded += count;
}

// Validate exact DEFLATE grammar, extent, expansion and the wrapper-advertised history window.
// The native provider remains responsible for wrapper header/trailer checksums and decoded bytes.
inline void validate(std::span<const std::uint8_t> bytes, const decode_options& options) {
  constexpr std::array length_base{3u,  4u,  5u,  6u,   7u,   8u,   9u,   10u,  11u, 13u,
                                   15u, 17u, 19u, 23u,  27u,  31u,  35u,  43u,  51u, 59u,
                                   67u, 83u, 99u, 115u, 131u, 163u, 195u, 227u, 258u};
  constexpr std::array length_extra{0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u,
                                    2u, 3u, 3u, 3u, 3u, 4u, 4u, 4u, 4u, 5u, 5u, 5u, 5u, 0u};
  constexpr std::array distance_base{1u,    2u,    3u,    4u,     5u,     7u,    9u,    13u,
                                     17u,   25u,   33u,   49u,    65u,    97u,   129u,  193u,
                                     257u,  385u,  513u,  769u,   1025u,  1537u, 2049u, 3073u,
                                     4097u, 6145u, 8193u, 12289u, 16385u, 24577u};
  constexpr std::array distance_extra{0u, 0u, 0u,  0u,  1u,  1u,  2u,  2u,  3u,  3u,
                                      4u, 4u, 5u,  5u,  6u,  6u,  7u,  7u,  8u,  8u,
                                      9u, 9u, 10u, 10u, 11u, 11u, 12u, 12u, 13u, 13u};
  constexpr std::array code_order{16u, 17u, 18u, 0u, 8u,  7u, 9u,  6u, 10u, 5u,
                                  11u, 4u,  12u, 3u, 13u, 2u, 14u, 1u, 15u};
  static_assert(length_base.size() == length_extra.size());
  static_assert(distance_base.size() == distance_extra.size());
  static_assert(code_order.size() == code_alphabet_size);
  std::size_t start{}, end = bytes.size(), expanded{}, window = history_bytes;
  if (options.check_cancelled) {
    options.check_cancelled();
  }
  if (options.format == format::zlib) {
    if (bytes.size() < 6 || (bytes[0] & 15) != 8 || bytes[0] >> 4 > 7 ||
        ((static_cast<unsigned>(bytes[0]) << 8) | bytes[1]) % 31 != 0 || (bytes[1] & 32) != 0) {
      invalid();
    }
    window = std::size_t{1} << ((bytes[0] >> 4) + 8);
    start = 2;
    end -= 4;
  } else if (options.format == format::gzip) {
    if (bytes.size() < 18 || bytes[0] != 31 || bytes[1] != 139 || bytes[2] != 8 ||
        (bytes[3] & 224) != 0) {
      invalid();
    }
    start = 10;
    end -= 8;
    if ((bytes[3] & 4) != 0) {
      if (end - start < 2) {
        invalid();
      }
      const std::size_t extra = bytes[start] | (static_cast<std::size_t>(bytes[start + 1]) << 8);
      start += 2;
      if (extra > end - start) {
        invalid();
      }
      start += extra;
    }
    for (unsigned flag = 8; flag <= 16; flag *= 2) {
      if ((bytes[3] & flag) == 0) {
        continue;
      }
      for (;;) {
        if (start == end) {
          invalid();
        }
        if ((start & header_poll_mask) == 0 && options.check_cancelled) {
          options.check_cancelled();
        }
        if (bytes[start++] == 0) {
          break;
        }
      }
    }
    if ((bytes[3] & 2) != 0) {
      if (end - start < 2) {
        invalid();
      }
      start += 2;
    }
  }
  bit_reader input{bytes.first(end), start, options.check_cancelled};
  tree literal, distance, codes, fixed_literal, fixed_distance;
  std::array<unsigned, combined_alphabet_size> lengths{};
  std::array<unsigned, code_alphabet_size> code_lengths{};
  for (std::size_t index = 0; index < literal_alphabet_size; ++index) {
    lengths[index] = index < 144 ? 8 : index < 256 ? 9 : index < 280 ? 7 : 8;
  }
  fixed_literal.build(std::span{lengths}.first(literal_alphabet_size), tree_kind::literal_length);
  std::fill_n(lengths.begin(), distance_alphabet_size, 5u);
  fixed_distance.build(std::span{lengths}.first(distance_alphabet_size), tree_kind::distance);
  bool final{};
  do {
    input.poll();
    final = input.read(1) != 0;
    const auto kind = input.read(2);
    if (kind == 0) {
      input.align();
      const auto count = input.read(16), complement = input.read(16);
      if ((count ^ 65535u) != complement) {
        invalid();
      }
      input.skip(count);
      add_expansion(expanded, count, options.max_decompressed_bytes);
      continue;
    }
    if (kind == 3) {
      invalid();
    }
    const tree* selected_literal = &fixed_literal;
    const tree* selected_distance = &fixed_distance;
    if (kind == 2) {
      const auto literal_count = input.read(5) + 257, distance_count = input.read(5) + 1,
                 code_count = input.read(4) + 4;
      if (literal_count > 286) {
        invalid();
      }
      code_lengths.fill(0);
      for (unsigned index = 0; index < code_count; ++index) {
        code_lengths[code_order[index]] = input.read(3);
      }
      codes.build(code_lengths, tree_kind::code_length);
      const auto total = literal_count + distance_count;
      unsigned position{};
      while (position < total) {
        const auto symbol = codes.read(input);
        if (symbol < 16) {
          lengths[position++] = symbol;
        } else {
          const auto count = symbol == 16   ? input.read(2) + 3
                             : symbol == 17 ? input.read(3) + 3
                                            : input.read(7) + 11;
          if ((symbol == 16 && position == 0) || count > total - position) {
            invalid();
          }
          const auto value = symbol == 16 ? lengths[position - 1] : 0;
          std::fill_n(lengths.begin() + position, count, value);
          position += count;
        }
      }
      if (lengths[256] == 0) {
        invalid();
      }
      literal.build(std::span{lengths}.first(literal_count), tree_kind::literal_length);
      distance.build(std::span{lengths}.subspan(literal_count, distance_count),
                     tree_kind::distance);
      selected_literal = &literal;
      selected_distance = &distance;
    }
    for (;;) {
      const auto symbol = selected_literal->read(input);
      if (symbol < 256) {
        add_expansion(expanded, 1, options.max_decompressed_bytes);
        continue;
      }
      if (symbol == 256) {
        break;
      }
      if (symbol - 257 >= length_base.size()) {
        invalid();
      }
      const auto count = length_base[symbol - 257] + input.read(length_extra[symbol - 257]);
      const auto back_code = selected_distance->read(input);
      if (back_code >= distance_base.size()) {
        invalid();
      }
      const auto back = distance_base[back_code] + input.read(distance_extra[back_code]);
      if (back > expanded || back > window) {
        invalid();
      }
      add_expansion(expanded, count, options.max_decompressed_bytes);
    }
  } while (!final);
  if (input.position() != end) {
    invalid();
  }
  input.poll();
}
} // namespace deflate_validation
} // namespace rohit::serializer::compression::detail
