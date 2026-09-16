#pragma once

#include <cstddef>
#include <cstdint>

namespace rohit::serializer::parser::detail {

enum class scan_kind { whitespace, identifier, line_comment, block_comment };

inline constexpr std::size_t scalar_probe_bytes = 8;
inline constexpr std::size_t minimum_simd_bytes = 16;
inline constexpr char ascii_case_bit = 'a' ^ 'A';

// Match bytes that may be consumed without another grammar decision.
template <scan_kind Kind>
constexpr bool continues_scan(std::uint8_t value) noexcept {
  if constexpr (Kind == scan_kind::whitespace) {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r';
  } else if constexpr (Kind == scan_kind::identifier) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
  } else if constexpr (Kind == scan_kind::line_comment) {
    return value != '\n';
  } else {
    // The parser checks the following slash, including across SIMD block boundaries.
    return value != '*';
  }
}

// Return the matching prefix length; data may be null only when size is zero.
template <scan_kind Kind>
std::size_t scan_scalar(const std::uint8_t* data, std::size_t size) noexcept {
  std::size_t offset{};
  while (offset < size && continues_scan<Kind>(data[offset])) {
    ++offset;
  }
  return offset;
}

// Scan exactly size readable bytes using baseline SIMD when available, otherwise scalar code.
std::size_t scan_baseline(const std::uint8_t* data, std::size_t size, scan_kind kind) noexcept;

// Select a CPU-compatible scanner once, then scan without reading past the supplied range.
std::size_t scan_long(const std::uint8_t* data, std::size_t size, scan_kind kind) noexcept;

// Scan a bounded prefix, keeping short tokens out of CPU dispatch and vector setup.
template <scan_kind Kind>
std::size_t scan_prefix(const std::uint8_t* data, std::size_t size) noexcept {
  const auto probe_size = size < scalar_probe_bytes ? size : scalar_probe_bytes;
  const auto prefix = scan_scalar<Kind>(data, probe_size);
  if (prefix != probe_size || prefix == size) {
    return prefix;
  }
  const auto remaining = size - prefix;
  if (remaining < minimum_simd_bytes) {
    return prefix + scan_scalar<Kind>(data + prefix, remaining);
  }
  return prefix + scan_long(data + prefix, remaining, Kind);
}

} // namespace rohit::serializer::parser::detail
