#include "schema_scan.hpp"

#include <cstring>
#include <immintrin.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace rohit::serializer::parser::detail {
namespace {

// Produce one bit per matching byte using signed ranges that exclude non-ASCII identifiers.
template <scan_kind Kind>
std::uint32_t matching_mask(__m256i bytes) noexcept {
  __m256i matches{};
  if constexpr (Kind == scan_kind::whitespace) {
    matches = _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(bytes, _mm256_set1_epi8(' ')),
                                              _mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('\t'))),
                              _mm256_or_si256(_mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('\n')),
                                              _mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('\r'))));
  } else if constexpr (Kind == scan_kind::identifier) {
    const auto folded = _mm256_or_si256(bytes, _mm256_set1_epi8(ascii_case_bit));
    const auto letters = _mm256_and_si256(_mm256_cmpgt_epi8(folded, _mm256_set1_epi8('a' - 1)),
                                          _mm256_cmpgt_epi8(_mm256_set1_epi8('z' + 1), folded));
    const auto digits = _mm256_and_si256(_mm256_cmpgt_epi8(bytes, _mm256_set1_epi8('0' - 1)),
                                         _mm256_cmpgt_epi8(_mm256_set1_epi8('9' + 1), bytes));
    matches = _mm256_or_si256(_mm256_or_si256(letters, digits),
                              _mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('_')));
  } else {
    constexpr auto stop = Kind == scan_kind::line_comment ? '\n' : '*';
    matches =
        _mm256_andnot_si256(_mm256_cmpeq_epi8(bytes, _mm256_set1_epi8(stop)), _mm256_set1_epi8(-1));
  }
  return static_cast<std::uint32_t>(_mm256_movemask_epi8(matches));
}

// Find a set bit in a nonzero mask without sharing ISA-dependent standard-library templates.
std::size_t first_stop(std::uint32_t mask) noexcept {
#if defined(_MSC_VER)
  unsigned long index{};
  _BitScanForward(&index, mask);
  return static_cast<std::size_t>(index);
#else
  return static_cast<std::size_t>(__builtin_ctz(mask));
#endif
}

// Scan only complete AVX2 blocks; baseline code handles the remaining bounded suffix.
template <scan_kind Kind>
std::size_t scan_vectors(const std::uint8_t* data, std::size_t size) noexcept {
  constexpr auto block_bytes = sizeof(__m256i);
  std::size_t offset{};
  while (size - offset >= block_bytes) {
    __m256i bytes{};
    std::memcpy(&bytes, data + offset, block_bytes);
    const auto stops = ~matching_mask<Kind>(bytes);
    if (stops != 0) {
      return offset + first_stop(stops);
    }
    offset += block_bytes;
  }
  if (offset == size) {
    return offset;
  }
  _mm256_zeroupper();
  return offset + scan_baseline(data + offset, size - offset, Kind);
}

} // namespace

// Keep AVX2 instructions behind CPU dispatch even when link-time optimization is enabled.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
std::size_t scan_avx2(const std::uint8_t* data, std::size_t size, scan_kind kind) noexcept {
  switch (kind) {
  case scan_kind::whitespace:
    return scan_vectors<scan_kind::whitespace>(data, size);
  case scan_kind::identifier:
    return scan_vectors<scan_kind::identifier>(data, size);
  case scan_kind::line_comment:
    return scan_vectors<scan_kind::line_comment>(data, size);
  case scan_kind::block_comment:
    return scan_vectors<scan_kind::block_comment>(data, size);
  }
  return 0;
}

} // namespace rohit::serializer::parser::detail
