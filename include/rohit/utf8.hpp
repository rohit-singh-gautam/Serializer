// Copyright (C) 2024, 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/runtime_simd.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>

namespace rohit::serializer::detail {
// Return a complete UTF-8 sequence length, rejecting overlong encodings and surrogate code points.
constexpr std::size_t utf8_sequence_size(std::span<const std::uint8_t> bytes) {
  if (bytes.empty()) {
    throw std::invalid_argument{"Truncated UTF-8"};
  }
  const auto first = bytes[0];
  if (first < 0x80) {
    return 1;
  }
  const std::size_t size = first >= 0xc2 && first <= 0xdf   ? 2
                           : first >= 0xe0 && first <= 0xef ? 3
                           : first >= 0xf0 && first <= 0xf4 ? 4
                                                            : 0;
  if (size == 0 || size > bytes.size()) {
    throw std::invalid_argument{"Invalid UTF-8"};
  }
  for (std::size_t index = 1; index < size; ++index) {
    if ((bytes[index] & 0xc0) != 0x80) {
      throw std::invalid_argument{"Invalid UTF-8 continuation"};
    }
  }
  if ((first == 0xe0 && bytes[1] < 0xa0) || (first == 0xed && bytes[1] >= 0xa0) ||
      (first == 0xf0 && bytes[1] < 0x90) || (first == 0xf4 && bytes[1] >= 0x90)) {
    throw std::invalid_argument{"Invalid UTF-8 code point"};
  }
  return size;
}

// Validate binary text without allocation or padded reads using the bounded UTF-8 backend.
inline void validate_utf8(std::span<const std::uint8_t> bytes) {
  if (!is_valid_utf8(bytes.data(), bytes.size())) {
    throw std::invalid_argument{"Invalid UTF-8"};
  }
}

// Treat string storage as bytes without changing embedded NULs or Unicode normalization.
inline void validate_utf8(std::string_view text) {
  validate_utf8(std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(text.data()),
                                              text.size()});
}

} // namespace rohit::serializer::detail
