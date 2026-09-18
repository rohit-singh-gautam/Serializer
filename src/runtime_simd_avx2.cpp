#include <rohit/runtime_simd.hpp>

#include <array>
#include <cstring>
#include <immintrin.h>
#include <limits>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace rohit::serializer::detail {
namespace {

// Locate the first special byte without ISA-dependent shared standard-library templates.
std::size_t first_stop(std::uint32_t mask) noexcept {
#if defined(_MSC_VER)
  unsigned long index{};
  _BitScanForward(&index, mask);
  return static_cast<std::size_t>(index);
#else
  return static_cast<std::size_t>(__builtin_ctz(mask));
#endif
}

// Locate the final leading byte when a UTF-8 sequence crosses the vector boundary.
std::size_t last_start(std::uint32_t mask) noexcept {
#if defined(_MSC_VER)
  unsigned long index{};
  _BitScanReverse(&index, mask);
  return static_cast<std::size_t>(index);
#else
  return std::numeric_limits<std::uint32_t>::digits - 1u -
      static_cast<std::size_t>(__builtin_clz(mask));
#endif
}

// Extract one classification bit per byte without sign extension.
std::uint32_t byte_mask(__m256i bytes) noexcept {
  return static_cast<std::uint32_t>(_mm256_movemask_epi8(bytes));
}

// Read complete 32-byte blocks only; no input padding or alignment is required.
template <json_scan_kind Kind>
std::size_t scan_vectors(const std::uint8_t* data, std::size_t size) noexcept {
  constexpr auto block_bytes = sizeof(__m256i);
  static_assert(block_bytes <= std::numeric_limits<std::uint32_t>::digits);
  std::size_t offset{};
  while (size - offset >= block_bytes) {
    __m256i bytes{};
    std::memcpy(&bytes, data + offset, block_bytes);
    std::uint32_t mask{};
    if constexpr (Kind == json_scan_kind::whitespace) {
      const auto whitespace =
          _mm256_or_si256(_mm256_or_si256(_mm256_cmpeq_epi8(bytes, _mm256_set1_epi8(' ')),
                                          _mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('\t'))),
                          _mm256_or_si256(_mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('\n')),
                                          _mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('\r'))));
      constexpr auto lane_mask = std::numeric_limits<std::uint32_t>::max() >>
                                 (std::numeric_limits<std::uint32_t>::digits - block_bytes);
      mask = static_cast<std::uint32_t>(_mm256_movemask_epi8(whitespace)) ^ lane_mask;
    } else {
      const auto controls =
          _mm256_cmpeq_epi8(_mm256_subs_epu8(bytes, _mm256_set1_epi8(json_control_limit - 1)),
                            _mm256_setzero_si256());
      auto special = _mm256_or_si256(
          controls, _mm256_or_si256(_mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('"')),
                                    _mm256_cmpeq_epi8(bytes, _mm256_set1_epi8('\\'))));
      if constexpr (Kind == json_scan_kind::ascii) {
        special = _mm256_or_si256(special, bytes);
      }
      mask = static_cast<std::uint32_t>(_mm256_movemask_epi8(special));
    }
    if (mask != 0) {
      return offset + first_stop(mask);
    }
    offset += block_bytes;
  }
  if (offset == size) {
    return offset;
  }
  _mm256_zeroupper();
  return offset + scan_json_baseline(data + offset, size - offset, Kind);
}

// Reverse each element within its 128-bit shuffle lane; supported widths divide the lane size.
template <std::size_t Width>
void swap_vectors(std::uint8_t* output, const std::uint8_t* input, std::size_t size) noexcept {
  static_assert(Width == 2 || Width == 4 || Width == 8);
  constexpr auto block_bytes = sizeof(__m256i);
  constexpr auto indices = [] {
    std::array<std::uint8_t, block_bytes> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
      result[index] =
          static_cast<std::uint8_t>((index / Width) * Width + Width - index % Width - 1);
    }
    return result;
  }();
  __m256i shuffle{};
  std::memcpy(&shuffle, indices.data(), block_bytes);
  std::size_t offset{};
  while (size - offset >= block_bytes) {
    __m256i bytes{};
    std::memcpy(&bytes, input + offset, block_bytes);
    bytes = _mm256_shuffle_epi8(bytes, shuffle);
    std::memcpy(output + offset, &bytes, block_bytes);
    offset += block_bytes;
  }
  if (offset != size) {
    _mm256_zeroupper();
    copy_swapped_baseline(output + offset, input + offset, size - offset, Width);
  }
}

} // namespace

// Check leading/continuation masks and Unicode range restrictions on complete vectors.
// Revisit at most three trailing bytes so every block and scalar tail starts at a code point.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
bool is_valid_utf8_avx2(const std::uint8_t* data, std::size_t size) noexcept {
  constexpr auto block_bytes = sizeof(__m256i);
  static_assert(block_bytes == std::numeric_limits<std::uint32_t>::digits);
  std::size_t offset{};
  while (size - offset >= block_bytes) {
    __m256i bytes{};
    std::memcpy(&bytes, data + offset, block_bytes);
    const auto non_ascii = byte_mask(bytes);
    if (non_ascii == 0) {
      offset += block_bytes;
      continue;
    }
    const auto continuation = byte_mask(_mm256_cmpeq_epi8(
        _mm256_and_si256(bytes, _mm256_set1_epi8(static_cast<char>(0xc0))),
        _mm256_set1_epi8(static_cast<char>(0x80))));
    const auto two_byte = byte_mask(_mm256_and_si256(
        _mm256_cmpgt_epi8(bytes, _mm256_set1_epi8(static_cast<char>(0xc1))),
        _mm256_cmpgt_epi8(_mm256_set1_epi8(static_cast<char>(0xe0)), bytes)));
    const auto three_byte = byte_mask(_mm256_cmpeq_epi8(
        _mm256_and_si256(bytes, _mm256_set1_epi8(static_cast<char>(0xf0))),
        _mm256_set1_epi8(static_cast<char>(0xe0))));
    const auto four_byte = byte_mask(_mm256_and_si256(
        _mm256_cmpgt_epi8(bytes, _mm256_set1_epi8(static_cast<char>(0xef))),
        _mm256_cmpgt_epi8(_mm256_set1_epi8(static_cast<char>(0xf5)), bytes)));
    const auto leading = two_byte | three_byte | four_byte;
    const auto expected = (static_cast<std::uint64_t>(leading) << 1) |
        (static_cast<std::uint64_t>(three_byte | four_byte) << 2) |
        (static_cast<std::uint64_t>(four_byte) << 3);
    if ((leading | continuation) != non_ascii ||
        static_cast<std::uint32_t>(expected) != continuation) {
      return false;
    }

    // A lead's next byte must exclude overlong forms, surrogates, and values above U+10FFFF.
    // Shifts intentionally discard lane 32: a split sequence is rechecked in the next block.
    const auto after_e0 = byte_mask(_mm256_cmpeq_epi8(
        bytes, _mm256_set1_epi8(static_cast<char>(0xe0)))) << 1;
    const auto after_ed = byte_mask(_mm256_cmpeq_epi8(
        bytes, _mm256_set1_epi8(static_cast<char>(0xed)))) << 1;
    const auto after_f0 = byte_mask(_mm256_cmpeq_epi8(
        bytes, _mm256_set1_epi8(static_cast<char>(0xf0)))) << 1;
    const auto after_f4 = byte_mask(_mm256_cmpeq_epi8(
        bytes, _mm256_set1_epi8(static_cast<char>(0xf4)))) << 1;
    const auto below_a0 = byte_mask(_mm256_cmpgt_epi8(
        _mm256_set1_epi8(static_cast<char>(0xa0)), bytes));
    const auto above_9f = byte_mask(_mm256_cmpgt_epi8(
        bytes, _mm256_set1_epi8(static_cast<char>(0x9f))));
    const auto below_90 = byte_mask(_mm256_cmpgt_epi8(
        _mm256_set1_epi8(static_cast<char>(0x90)), bytes));
    const auto above_8f = byte_mask(_mm256_cmpgt_epi8(
        bytes, _mm256_set1_epi8(static_cast<char>(0x8f))));
    if (((after_e0 & below_a0) | (after_ed & above_9f) |
         (after_f0 & below_90) | (after_f4 & above_8f)) != 0) {
      return false;
    }
    offset += (expected >> block_bytes) == 0 ? block_bytes : last_start(leading);
  }
  if (offset == size) { return true; }
  _mm256_zeroupper();
  return is_valid_utf8_baseline(data + offset, size - offset);
}

// Prevent link-time inlining from moving AVX2 instructions ahead of CPU dispatch.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
std::size_t scan_json_avx2(const std::uint8_t* data, std::size_t size,
                           json_scan_kind kind) noexcept {
  if (kind == json_scan_kind::whitespace) {
    return scan_vectors<json_scan_kind::whitespace>(data, size);
  }
  return kind == json_scan_kind::ascii ? scan_vectors<json_scan_kind::ascii>(data, size)
                                       : scan_vectors<json_scan_kind::unescaped>(data, size);
}

// Keep byte-shuffle instructions within the separately compiled AVX2 backend.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void copy_swapped_avx2(std::uint8_t* output, const std::uint8_t* input, std::size_t size,
                       std::size_t element_bytes) noexcept {
  switch (element_bytes) {
  case 2:
    return swap_vectors<2>(output, input, size);
  case 4:
    return swap_vectors<4>(output, input, size);
  case 8:
    return swap_vectors<8>(output, input, size);
  }
}

} // namespace rohit::serializer::detail
