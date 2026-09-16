#include "schema_scan.hpp"

#include "simd_dispatch.hpp"

#include <bit>
#include <cstring>

#ifndef SERIALIZER_ENABLE_SIMD
#define SERIALIZER_ENABLE_SIMD 1
#endif

#if SERIALIZER_ENABLE_SIMD && (defined(__x86_64__) || defined(_M_X64)) && !defined(_M_ARM64EC)
#define SERIALIZER_SCAN_SSE2 1
#include <emmintrin.h>
#endif

namespace rohit::serializer::parser::detail {

#if defined(SERIALIZER_HAS_AVX2_SCANNER)
// Implemented in a separate translation unit compiled for AVX2; call only after CPU/OS checks.
std::size_t scan_avx2(const std::uint8_t* data, std::size_t size, scan_kind kind) noexcept;
#endif

namespace {

#if defined(SERIALIZER_SCAN_SSE2)
// Produce one bit per matching ASCII byte without accepting high-bit bytes as identifiers.
template <scan_kind Kind>
std::uint32_t matching_mask(__m128i bytes) noexcept {
  __m128i matches{};
  if constexpr (Kind == scan_kind::whitespace) {
    matches = _mm_or_si128(_mm_or_si128(_mm_cmpeq_epi8(bytes, _mm_set1_epi8(' ')),
                                        _mm_cmpeq_epi8(bytes, _mm_set1_epi8('\t'))),
                           _mm_or_si128(_mm_cmpeq_epi8(bytes, _mm_set1_epi8('\n')),
                                        _mm_cmpeq_epi8(bytes, _mm_set1_epi8('\r'))));
  } else if constexpr (Kind == scan_kind::identifier) {
    const auto folded = _mm_or_si128(bytes, _mm_set1_epi8(ascii_case_bit));
    const auto letters = _mm_and_si128(_mm_cmpgt_epi8(folded, _mm_set1_epi8('a' - 1)),
                                       _mm_cmpgt_epi8(_mm_set1_epi8('z' + 1), folded));
    const auto digits = _mm_and_si128(_mm_cmpgt_epi8(bytes, _mm_set1_epi8('0' - 1)),
                                      _mm_cmpgt_epi8(_mm_set1_epi8('9' + 1), bytes));
    matches =
        _mm_or_si128(_mm_or_si128(letters, digits), _mm_cmpeq_epi8(bytes, _mm_set1_epi8('_')));
  } else {
    constexpr auto stop = Kind == scan_kind::line_comment ? '\n' : '*';
    matches = _mm_andnot_si128(_mm_cmpeq_epi8(bytes, _mm_set1_epi8(stop)), _mm_set1_epi8(-1));
  }
  return static_cast<std::uint32_t>(_mm_movemask_epi8(matches));
}
#endif

// Consume complete vectors only; an unaligned load never needs padding after the input.
template <scan_kind Kind>
std::size_t scan_native(const std::uint8_t* data, std::size_t size) noexcept {
  std::size_t offset{};
#if defined(SERIALIZER_SCAN_SSE2)
  constexpr auto block_bytes = sizeof(__m128i);
  static_assert(block_bytes == minimum_simd_bytes);
  while (size - offset >= block_bytes) {
    __m128i bytes{};
    std::memcpy(&bytes, data + offset, block_bytes);
    const auto matched = static_cast<std::size_t>(std::countr_one(matching_mask<Kind>(bytes)));
    if (matched != block_bytes) {
      return offset + matched;
    }
    offset += block_bytes;
  }
#endif
  if (offset == size) {
    return offset;
  }
  return offset + scan_scalar<Kind>(data + offset, size - offset);
}

} // namespace

// Dispatch outside the scanning loop so character classification remains specialized.
std::size_t scan_baseline(const std::uint8_t* data, std::size_t size, scan_kind kind) noexcept {
  switch (kind) {
  case scan_kind::whitespace:
    return scan_native<scan_kind::whitespace>(data, size);
  case scan_kind::identifier:
    return scan_native<scan_kind::identifier>(data, size);
  case scan_kind::line_comment:
    return scan_native<scan_kind::line_comment>(data, size);
  case scan_kind::block_comment:
    return scan_native<scan_kind::block_comment>(data, size);
  }
  return 0;
}

// Cache the compatible backend with thread-safe initialization; do not probe the CPU per token.
std::size_t scan_long(const std::uint8_t* data, std::size_t size, scan_kind kind) noexcept {
#if defined(SERIALIZER_HAS_AVX2_SCANNER)
  // A suffix shorter than an AVX2 block can use the baseline scanner directly.
  if (size < minimum_simd_bytes * 2) {
    return scan_baseline(data, size, kind);
  }
  using scan_function = std::size_t (*)(const std::uint8_t*, std::size_t, scan_kind) noexcept;
  static const scan_function implementation =
      ::rohit::serializer::detail::supports_avx2() ? scan_avx2 : scan_baseline;
  return implementation(data, size, kind);
#else
  return scan_baseline(data, size, kind);
#endif
}

} // namespace rohit::serializer::parser::detail
