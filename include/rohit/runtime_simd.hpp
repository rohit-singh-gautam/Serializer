#pragma once

#include <cstddef>
#include <cstdint>

namespace rohit::serializer::detail {

enum class json_scan_kind { ascii, unescaped };
inline constexpr std::size_t runtime_probe_bytes = 8;
inline constexpr std::size_t runtime_simd_bytes = 16;
inline constexpr std::uint8_t json_control_limit = 0x20;
inline constexpr std::uint8_t json_ascii_limit = 0x80;

// ASCII scans stop for UTF-8 validation; escape scans also accept already validated UTF-8 bytes.
template <json_scan_kind Kind>
constexpr bool json_plain_byte(std::uint8_t value) noexcept {
  return value >= json_control_limit && value != '"' && value != '\\' &&
         (Kind == json_scan_kind::unescaped || value < json_ascii_limit);
}

// Read only the supplied range; null is permitted for an empty input.
template <json_scan_kind Kind>
std::size_t scan_json_scalar(const std::uint8_t* data, std::size_t size) noexcept {
  std::size_t offset{};
  while (offset < size && json_plain_byte<Kind>(data[offset])) {
    ++offset;
  }
  return offset;
}

// Scan with the baseline ISA or scalar fallback, without requiring readable input padding.
std::size_t scan_json_baseline(const std::uint8_t* data, std::size_t size,
                               json_scan_kind kind) noexcept;

// Dispatch long spans to a cached CPU/OS-compatible backend.
std::size_t scan_json_long(const std::uint8_t* data, std::size_t size,
                           json_scan_kind kind) noexcept;

// Keep short strings and adjacent escapes out of runtime dispatch.
template <json_scan_kind Kind>
std::size_t scan_json_prefix(const std::uint8_t* data, std::size_t size) noexcept {
  const auto probe = size < runtime_probe_bytes ? size : runtime_probe_bytes;
  const auto prefix = scan_json_scalar<Kind>(data, probe);
  if (prefix != probe || prefix == size) {
    return prefix;
  }
  const auto remaining = size - prefix;
  if (remaining < runtime_simd_bytes) {
    return prefix + scan_json_scalar<Kind>(data + prefix, remaining);
  }
  return prefix + scan_json_long(data + prefix, remaining, Kind);
}

// Reverse each 2-, 4-, or 8-byte element in an exact multiple of element_bytes.
// Both ranges must contain size bytes and be disjoint or identical; neither requires alignment.
void copy_swapped_baseline(std::uint8_t* output, const std::uint8_t* input, std::size_t size,
                           std::size_t element_bytes) noexcept;

// Apply the same bounded conversion using a cached CPU-compatible backend for long spans.
void copy_swapped(std::uint8_t* output, const std::uint8_t* input, std::size_t size,
                  std::size_t element_bytes) noexcept;

} // namespace rohit::serializer::detail
