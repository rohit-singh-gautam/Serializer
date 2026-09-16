#include <rohit/runtime_simd.hpp>

#include "simd_dispatch.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <limits>

#ifndef SERIALIZER_ENABLE_SIMD
#define SERIALIZER_ENABLE_SIMD 1
#endif

#if SERIALIZER_ENABLE_SIMD && (defined(__x86_64__) || defined(_M_X64)) && !defined(_M_ARM64EC)
#define SERIALIZER_RUNTIME_SSE2 1
#include <emmintrin.h>
#endif

namespace rohit::serializer::detail {

#if defined(SERIALIZER_HAS_AVX2_SCANNER)
// These entry points live in an isolated AVX2 translation unit.
std::size_t scan_json_avx2(const std::uint8_t* data, std::size_t size,
                           json_scan_kind kind) noexcept;
// Call only for disjoint or identical ranges after CPU/OS validation.
void copy_swapped_avx2(std::uint8_t* output, const std::uint8_t* input, std::size_t size,
                       std::size_t element_bytes) noexcept;
#endif

namespace {

// Skip safe bytes in full vectors, then inspect the remaining exact-size suffix.
template <json_scan_kind Kind>
std::size_t scan_native(const std::uint8_t* data, std::size_t size) noexcept {
  std::size_t offset{};
#if defined(SERIALIZER_RUNTIME_SSE2)
  constexpr auto block_bytes = sizeof(__m128i);
  static_assert(block_bytes == runtime_simd_bytes);
  while (size - offset >= block_bytes) {
    __m128i bytes{};
    std::memcpy(&bytes, data + offset, block_bytes);
    // Unsigned saturation maps precisely 0x00..0x1f to zero, including with signed char.
    const auto controls = _mm_cmpeq_epi8(
        _mm_subs_epu8(bytes, _mm_set1_epi8(json_control_limit - 1)), _mm_setzero_si128());
    auto special = _mm_or_si128(controls, _mm_or_si128(_mm_cmpeq_epi8(bytes, _mm_set1_epi8('"')),
                                                       _mm_cmpeq_epi8(bytes, _mm_set1_epi8('\\'))));
    if constexpr (Kind == json_scan_kind::ascii) {
      special = _mm_or_si128(special, bytes);
    }
    const auto mask = static_cast<std::uint32_t>(_mm_movemask_epi8(special));
    if (mask != 0) {
      return offset + static_cast<std::size_t>(std::countr_zero(mask));
    }
    offset += block_bytes;
  }
#endif
  if (offset == size) {
    return offset;
  }
  return offset + scan_json_scalar<Kind>(data + offset, size - offset);
}

// Reverse scalar representations, retaining exact bits and avoiding typed unaligned access.
template <std::size_t Width>
void swap_native(std::uint8_t* output, const std::uint8_t* input, std::size_t size) noexcept {
  static_assert(Width == 2 || Width == 4 || Width == 8);
  std::size_t offset{};
#if defined(SERIALIZER_RUNTIME_SSE2)
  constexpr auto block_bytes = sizeof(__m128i);
  constexpr auto byte_bits = std::numeric_limits<std::uint8_t>::digits;
  while (size - offset >= block_bytes) {
    __m128i bytes{};
    std::memcpy(&bytes, input + offset, block_bytes);
    bytes = _mm_or_si128(_mm_slli_epi16(bytes, byte_bits), _mm_srli_epi16(bytes, byte_bits));
    if constexpr (Width >= 4) {
      bytes = _mm_shufflelo_epi16(bytes, _MM_SHUFFLE(2, 3, 0, 1));
      bytes = _mm_shufflehi_epi16(bytes, _MM_SHUFFLE(2, 3, 0, 1));
    }
    if constexpr (Width == 8) {
      bytes = _mm_shuffle_epi32(bytes, _MM_SHUFFLE(2, 3, 0, 1));
    }
    std::memcpy(output + offset, &bytes, block_bytes);
    offset += block_bytes;
  }
#endif
  for (; offset < size; offset += Width) {
    // Snapshot a complete element so identical input/output is supported too.
    std::array<std::uint8_t, Width> bytes{};
    std::memcpy(bytes.data(), input + offset, Width);
    for (std::size_t index = 0; index < Width; ++index) {
      output[offset + index] = bytes[Width - index - 1];
    }
  }
}

} // namespace

// Select character rules once per span, outside the vector loop.
std::size_t scan_json_baseline(const std::uint8_t* data, std::size_t size,
                               json_scan_kind kind) noexcept {
  return kind == json_scan_kind::ascii ? scan_native<json_scan_kind::ascii>(data, size)
                                       : scan_native<json_scan_kind::unescaped>(data, size);
}

// Cache dispatch with thread-safe initialization, avoiding CPU queries for individual strings.
std::size_t scan_json_long(const std::uint8_t* data, std::size_t size,
                           json_scan_kind kind) noexcept {
#if defined(SERIALIZER_HAS_AVX2_SCANNER)
  if (size >= runtime_simd_bytes * 2) {
    using scan_function =
        std::size_t (*)(const std::uint8_t*, std::size_t, json_scan_kind) noexcept;
    static const scan_function implementation =
        supports_avx2() ? scan_json_avx2 : scan_json_baseline;
    return implementation(data, size, kind);
  }
#endif
  return scan_json_baseline(data, size, kind);
}

// Select the element width once, retaining specialized shifts inside the loop.
void copy_swapped_baseline(std::uint8_t* output, const std::uint8_t* input, std::size_t size,
                           std::size_t element_bytes) noexcept {
  switch (element_bytes) {
  case 2:
    return swap_native<2>(output, input, size);
  case 4:
    return swap_native<4>(output, input, size);
  case 8:
    return swap_native<8>(output, input, size);
  }
}

// Dispatch arrays once; small tails and unsupported CPUs retain the baseline path.
void copy_swapped(std::uint8_t* output, const std::uint8_t* input, std::size_t size,
                  std::size_t element_bytes) noexcept {
#if defined(SERIALIZER_HAS_AVX2_SCANNER)
  if (size >= runtime_simd_bytes * 2) {
    using copy_function =
        void (*)(std::uint8_t*, const std::uint8_t*, std::size_t, std::size_t) noexcept;
    static const copy_function implementation =
        supports_avx2() ? copy_swapped_avx2 : copy_swapped_baseline;
    implementation(output, input, size, element_bytes);
    return;
  }
#endif
  copy_swapped_baseline(output, input, size, element_bytes);
}

} // namespace rohit::serializer::detail
