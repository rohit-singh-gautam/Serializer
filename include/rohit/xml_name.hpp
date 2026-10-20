// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rohit::serializer::xml {
namespace detail {

// Recognize XML 1.0 fifth-edition NameStartChar without the namespace separator.
[[nodiscard]] constexpr bool ncname_start(std::uint32_t value) noexcept {
  return value == '_' || (value >= 'A' && value <= 'Z') ||
      (value >= 'a' && value <= 'z') || (value >= 0xc0U && value <= 0xd6U) ||
      (value >= 0xd8U && value <= 0xf6U) || (value >= 0xf8U && value <= 0x2ffU) ||
      (value >= 0x370U && value <= 0x37dU) || (value >= 0x37fU && value <= 0x1fffU) ||
      (value >= 0x200cU && value <= 0x200dU) || (value >= 0x2070U && value <= 0x218fU) ||
      (value >= 0x2c00U && value <= 0x2fefU) || (value >= 0x3001U && value <= 0xd7ffU) ||
      (value >= 0xf900U && value <= 0xfdcfU) || (value >= 0xfdf0U && value <= 0xfffdU) ||
      (value >= 0x10000U && value <= 0xeffffU);
}

// Recognize additional XML name characters, keeping colon invalid within an NCName.
[[nodiscard]] constexpr bool ncname_character(std::uint32_t value) noexcept {
  return ncname_start(value) || value == '-' || value == '.' ||
      (value >= '0' && value <= '9') || value == 0xb7U ||
      (value >= 0x300U && value <= 0x36fU) || (value >= 0x203fU && value <= 0x2040U);
}

} // namespace detail

// Validate a complete UTF-8 NCName without allocation or an XML provider. Malformed,
// overlong, surrogate and out-of-range encodings fail; Unicode spelling stays byte-exact.
[[nodiscard]] constexpr bool is_ncname(std::string_view value) noexcept {
  if (value.empty()) { return false; }
  for (std::size_t index{}; index < value.size();) {
    const bool initial = index == 0U;
    const auto first = static_cast<unsigned char>(value[index++]);
    std::uint32_t codepoint{first};
    std::size_t continuation{};
    std::uint32_t minimum{};
    if (first >= 0xc2U && first <= 0xdfU) {
      continuation = 1U; minimum = 0x80U; codepoint &= 0x1fU;
    } else if (first >= 0xe0U && first <= 0xefU) {
      continuation = 2U; minimum = 0x800U; codepoint &= 0x0fU;
    } else if (first >= 0xf0U && first <= 0xf4U) {
      continuation = 3U; minimum = 0x10000U; codepoint &= 0x07U;
    } else if (first >= 0x80U) {
      return false;
    }
    if (continuation > value.size() - index) { return false; }
    for (std::size_t next{}; next < continuation; ++next) {
      const auto byte = static_cast<unsigned char>(value[index++]);
      if ((byte & 0xc0U) != 0x80U) { return false; }
      codepoint = (codepoint << 6U) | (byte & 0x3fU);
    }
    if (codepoint < minimum ||
        !(initial ? detail::ncname_start(codepoint) : detail::ncname_character(codepoint))) {
      return false;
    }
  }
  return true;
}

} // namespace rohit::serializer::xml
