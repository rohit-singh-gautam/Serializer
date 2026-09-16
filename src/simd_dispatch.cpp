#include "simd_dispatch.hpp"

#include <bit>
#include <cstdint>

#if defined(SERIALIZER_HAS_AVX2_SCANNER) && defined(_MSC_VER)
#include <intrin.h>
#endif

namespace rohit::serializer::detail {

#if defined(SERIALIZER_HAS_AVX2_SCANNER)
// Check OS vector-state support as well as the instructions allowed in the AVX2 object file.
bool supports_avx2() noexcept {
#if defined(_MSC_VER)
  int registers[4]{};
  constexpr int feature_leaf = 7;
  __cpuid(registers, 0);
  if (registers[0] < feature_leaf) {
    return false;
  }
  __cpuidex(registers, 1, 0);
  constexpr std::uint32_t fma = 1u << 12;
  constexpr std::uint32_t popcnt = 1u << 23;
  constexpr std::uint32_t xsave = 1u << 26;
  constexpr std::uint32_t osxsave = 1u << 27;
  constexpr std::uint32_t avx = 1u << 28;
  constexpr auto required_state = fma | popcnt | xsave | osxsave | avx;
  if ((static_cast<std::uint32_t>(registers[2]) & required_state) != required_state) {
    return false;
  }
  constexpr unsigned xcr_feature_register = 0;
  constexpr std::uint64_t xmm_ymm_state = (1u << 1) | (1u << 2);
  if ((_xgetbv(xcr_feature_register) & xmm_ymm_state) != xmm_ymm_state) {
    return false;
  }
  __cpuidex(registers, feature_leaf, 0);
  constexpr std::uint32_t bmi1 = 1u << 3;
  constexpr std::uint32_t avx2 = 1u << 5;
  constexpr std::uint32_t bmi2 = 1u << 8;
  constexpr auto required_instructions = bmi1 | avx2 | bmi2;
  if ((static_cast<std::uint32_t>(registers[1]) & required_instructions) != required_instructions) {
    return false;
  }
  constexpr std::uint32_t extended_leaf = 0x80000001u;
  __cpuid(registers, std::bit_cast<int>(extended_leaf - 1));
  if (static_cast<std::uint32_t>(registers[0]) < extended_leaf) {
    return false;
  }
  __cpuid(registers, std::bit_cast<int>(extended_leaf));
  constexpr std::uint32_t lzcnt = 1u << 5;
  return (static_cast<std::uint32_t>(registers[2]) & lzcnt) != 0;
#else
  __builtin_cpu_init();
  return __builtin_cpu_supports("avx2") != 0;
#endif
}
#else
// SIMD-disabled and unsupported targets never enter an AVX2 backend.
bool supports_avx2() noexcept {
  return false;
}
#endif

} // namespace rohit::serializer::detail
