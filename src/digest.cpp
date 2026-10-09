// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/digest.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit {
namespace {

// These are independent implementations of the algorithms defined in RFC 1321 sections 3.1-3.5,
// FIPS 180-4 sections 4-6, and FIPS 202 sections 3-6. No reference implementation is incorporated.
constexpr std::size_t bits_per_byte = 8;
constexpr std::size_t md5_block_bytes = 64;
constexpr std::size_t sha256_block_bytes = 64;
constexpr std::size_t sha512_block_bytes = 128;
constexpr std::size_t sha3_state_lanes = 25;
constexpr std::size_t sha3_lane_bytes = sizeof(std::uint64_t);
constexpr std::size_t sha3_state_bytes = sha3_state_lanes * sha3_lane_bytes;

constexpr auto md5_round_constants = std::to_array<std::uint32_t>(
    {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613,
     0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193,
     0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d,
     0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
     0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
     0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
     0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244,
     0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
     0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
     0xeb86d391});
constexpr std::array<std::array<int, 4>, 4> md5_rotation_counts{
    {{7, 12, 17, 22}, {5, 9, 14, 20}, {4, 11, 16, 23}, {6, 10, 15, 21}}};

constexpr auto sha256_round_constants = std::to_array<std::uint32_t>(
    {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
     0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
     0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
     0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
     0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
     0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
     0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
     0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
     0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
     0xc67178f2});

constexpr auto sha512_round_constants = std::to_array<std::uint64_t>(
    {0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
     0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
     0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
     0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
     0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
     0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
     0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
     0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
     0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
     0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
     0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
     0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
     0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
     0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
     0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
     0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
     0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
     0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
     0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
     0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817});

constexpr auto sha3_round_constants = std::to_array<std::uint64_t>(
    {0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000,
     0x000000000000808b, 0x0000000080000001, 0x8000000080008081, 0x8000000000008009,
     0x000000000000008a, 0x0000000000000088, 0x0000000080008009, 0x000000008000000a,
     0x000000008000808b, 0x800000000000008b, 0x8000000000008089, 0x8000000000008003,
     0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
     0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008});
constexpr auto sha3_rotation_counts = std::to_array<int>(
    {0, 1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43, 25, 39, 41, 45, 15, 21, 8, 18, 2, 61, 56, 14});

// Load an unaligned word using the specified byte order, independent of host representation.
template <typename Word, bool LittleEndian = false>
Word load_word(const std::uint8_t* bytes) noexcept {
  Word result{};
  for (std::size_t index = 0; index < sizeof(Word); ++index) {
    const auto source = LittleEndian ? sizeof(Word) - index - 1 : index;
    result = static_cast<Word>((result << bits_per_byte) | bytes[source]);
  }
  return result;
}

// Write a word's complete byte representation without alignment or host-endian assumptions.
template <typename Word, bool LittleEndian = false>
void store_word(Word value, std::uint8_t* bytes) noexcept {
  for (std::size_t index = 0; index < sizeof(Word); ++index) {
    const auto destination = LittleEndian ? index : sizeof(Word) - index - 1;
    bytes[destination] = static_cast<std::uint8_t>(value);
    value >>= bits_per_byte;
  }
}

// Process complete input blocks and one/two stack-resident padding blocks; input is never copied
// wholesale. SHA-512 writes its full 128-bit bit length, including carries from byte conversion.
template <std::size_t BlockBytes, bool LittleEndian, typename Transform>
void process_padded_blocks(std::span<const std::uint8_t> input, Transform transform) noexcept {
  constexpr auto length_bytes = BlockBytes == sha512_block_bytes ? 16u : 8u;
  std::size_t offset{};
  while (input.size() - offset >= BlockBytes) {
    transform(input.data() + offset);
    offset += BlockBytes;
  }
  std::array<std::uint8_t, BlockBytes * 2> tail{};
  const auto remainder = input.size() - offset;
  if (remainder != 0) {
    std::copy_n(input.data() + offset, remainder, tail.data());
  }
  tail[remainder] = 0x80;
  const auto padded_bytes = remainder < BlockBytes - length_bytes ? BlockBytes : tail.size();
  const auto byte_length = static_cast<std::uint64_t>(input.size());
  store_word<std::uint64_t, LittleEndian>(byte_length << 3,
                                          tail.data() + padded_bytes - sizeof(std::uint64_t));
  if constexpr (length_bytes == 16) {
    store_word<std::uint64_t>(byte_length >> 61, tail.data() + padded_bytes - length_bytes);
  }
  transform(tail.data());
  if (padded_bytes != BlockBytes) {
    transform(tail.data() + BlockBytes);
  }
}

// Emit the requested leading state bytes; truncated SHA variants use their distinct initial state.
template <typename Word, std::size_t N, bool LittleEndian = false>
void write_state(const std::array<Word, N>& state, std::span<std::uint8_t> output) noexcept {
  for (std::size_t index = 0; index < output.size(); ++index) {
    const auto byte = index % sizeof(Word);
    const auto shift = (LittleEndian ? byte : sizeof(Word) - byte - 1) * bits_per_byte;
    output[index] = static_cast<std::uint8_t>(state[index / sizeof(Word)] >> shift);
  }
}

// Apply RFC 1321's four Boolean phases and little-endian message schedule to a single block.
void transform_md5(std::array<std::uint32_t, 4>& state, const std::uint8_t* block) noexcept {
  std::array<std::uint32_t, 16> words{};
  for (std::size_t index = 0; index < words.size(); ++index) {
    words[index] = load_word<std::uint32_t, true>(block + index * sizeof(words[0]));
  }
  auto [a, b, c, d] = state;
  for (std::size_t round = 0; round < md5_round_constants.size(); ++round) {
    std::uint32_t function{};
    std::size_t word_index{};
    if (round < 16) {
      function = (b & c) | (~b & d);
      word_index = round;
    } else if (round < 32) {
      function = (d & b) | (~d & c);
      word_index = (5 * round + 1) % words.size();
    } else if (round < 48) {
      function = b ^ c ^ d;
      word_index = (3 * round + 5) % words.size();
    } else {
      function = c ^ (b | ~d);
      word_index = (7 * round) % words.size();
    }
    const auto next = b + std::rotl(a + function + md5_round_constants[round] + words[word_index],
                                    md5_rotation_counts[round / 16][round % 4]);
    a = d;
    d = c;
    c = b;
    b = next;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

// Compute MD5 for legacy byte-level compatibility, with its modulo-2^64 length encoding.
void compute_md5(std::span<const std::uint8_t> input, std::span<std::uint8_t> output) noexcept {
  auto state = std::to_array<std::uint32_t>({0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476});
  process_padded_blocks<md5_block_bytes, true>(
      input, [&state](const std::uint8_t* block) { transform_md5(state, block); });
  write_state<std::uint32_t, 4, true>(state, output);
}

// Apply SHA-1's eighty rounds to one big-endian block; unsigned additions wrap by definition.
void transform_sha1(std::array<std::uint32_t, 5>& state, const std::uint8_t* block) noexcept {
  constexpr auto round_constants =
      std::to_array<std::uint32_t>({0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6});
  constexpr std::size_t rounds_per_phase = 20;
  std::array<std::uint32_t, rounds_per_phase * round_constants.size()> words{};
  constexpr std::size_t input_words = sha256_block_bytes / sizeof(words[0]);
  for (std::size_t index = 0; index < input_words; ++index) {
    words[index] = load_word<std::uint32_t>(block + index * sizeof(words[0]));
  }
  for (std::size_t index = input_words; index < words.size(); ++index) {
    words[index] =
        std::rotl(words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
  }
  auto [a, b, c, d, e] = state;
  for (std::size_t round = 0; round < words.size(); ++round) {
    const auto phase = round / rounds_per_phase;
    const auto function = phase == 0   ? (b & c) | (~b & d)
                          : phase == 2 ? (b & c) | (b & d) | (c & d)
                                       : b ^ c ^ d;
    const auto next = std::rotl(a, 5) + function + e + round_constants[phase] + words[round];
    e = d;
    d = c;
    c = std::rotl(b, 30);
    b = a;
    a = next;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

// Compute SHA-1 only when explicitly selected for legacy interoperability.
void compute_sha1(std::span<const std::uint8_t> input, std::span<std::uint8_t> output) noexcept {
  auto state =
      std::to_array<std::uint32_t>({0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0});
  process_padded_blocks<sha256_block_bytes, false>(
      input, [&state](const std::uint8_t* block) { transform_sha1(state, block); });
  write_state(state, output);
}

// Compute SHA-2's first schedule mixing function at the selected standard word width.
template <typename Word>
Word schedule_sigma_zero(Word value) noexcept {
  if constexpr (sizeof(Word) == sizeof(std::uint32_t)) {
    return std::rotr(value, 7) ^ std::rotr(value, 18) ^ (value >> 3);
  } else {
    return std::rotr(value, 1) ^ std::rotr(value, 8) ^ (value >> 7);
  }
}

// Compute SHA-2's second schedule mixing function at the selected standard word width.
template <typename Word>
Word schedule_sigma_one(Word value) noexcept {
  if constexpr (sizeof(Word) == sizeof(std::uint32_t)) {
    return std::rotr(value, 17) ^ std::rotr(value, 19) ^ (value >> 10);
  } else {
    return std::rotr(value, 19) ^ std::rotr(value, 61) ^ (value >> 6);
  }
}

// Compute SHA-2's first round mixing function at the selected standard word width.
template <typename Word>
Word round_sigma_zero(Word value) noexcept {
  if constexpr (sizeof(Word) == sizeof(std::uint32_t)) {
    return std::rotr(value, 2) ^ std::rotr(value, 13) ^ std::rotr(value, 22);
  } else {
    return std::rotr(value, 28) ^ std::rotr(value, 34) ^ std::rotr(value, 39);
  }
}

// Compute SHA-2's second round mixing function at the selected standard word width.
template <typename Word>
Word round_sigma_one(Word value) noexcept {
  if constexpr (sizeof(Word) == sizeof(std::uint32_t)) {
    return std::rotr(value, 6) ^ std::rotr(value, 11) ^ std::rotr(value, 25);
  } else {
    return std::rotr(value, 14) ^ std::rotr(value, 18) ^ std::rotr(value, 41);
  }
}

// Apply the FIPS 180-4 SHA-2 round structure, sharing only the standard width-dependent functions.
template <typename Word, std::size_t Rounds>
void transform_sha2(std::array<Word, 8>& state, const std::uint8_t* block,
                    const std::array<Word, Rounds>& constants) noexcept {
  std::array<Word, Rounds> words{};
  constexpr std::size_t input_words = 16;
  for (std::size_t index = 0; index < input_words; ++index) {
    words[index] = load_word<Word>(block + index * sizeof(Word));
  }
  for (std::size_t index = input_words; index < words.size(); ++index) {
    words[index] = schedule_sigma_one(words[index - 2]) + words[index - 7] +
                   schedule_sigma_zero(words[index - 15]) + words[index - 16];
  }
  auto [a, b, c, d, e, f, g, h] = state;
  for (std::size_t round = 0; round < words.size(); ++round) {
    const auto choose = (e & f) ^ (~e & g);
    const auto majority = (a & b) ^ (a & c) ^ (b & c);
    const auto first = h + round_sigma_one(e) + choose + constants[round] + words[round];
    const auto second = round_sigma_zero(a) + majority;
    h = g;
    g = f;
    f = e;
    e = d + first;
    d = c;
    c = b;
    b = a;
    a = first + second;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

// Initialize and compute SHA-224 or SHA-256; SHA-224 is not plain truncation of SHA-256.
void compute_sha256(digest_algorithm algorithm, std::span<const std::uint8_t> input,
                    std::span<std::uint8_t> output) noexcept {
  auto state = algorithm == digest_algorithm::sha224
                   ? std::to_array<std::uint32_t>({0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
                                                   0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4})
                   : std::to_array<std::uint32_t>({0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19});
  process_padded_blocks<sha256_block_bytes, false>(input, [&state](const std::uint8_t* block) {
    transform_sha2(state, block, sha256_round_constants);
  });
  write_state(state, output);
}

// Select the independently specified SHA-512 family initial state before processing any input.
std::array<std::uint64_t, 8> sha512_initial_state(digest_algorithm algorithm) noexcept {
  switch (algorithm) {
  case digest_algorithm::sha384:
    return {0xcbbb9d5dc1059ed8, 0x629a292a367cd507, 0x9159015a3070dd17, 0x152fecd8f70e5939,
            0x67332667ffc00b31, 0x8eb44a8768581511, 0xdb0c2e0d64f98fa7, 0x47b5481dbefa4fa4};
  case digest_algorithm::sha512_224:
    return {0x8c3d37c819544da2, 0x73e1996689dcd4d6, 0x1dfab7ae32ff9c82, 0x679dd514582f9fcf,
            0x0f6d2b697bd44da8, 0x77e36f7304c48942, 0x3f9d85a86a1d36c8, 0x1112e6ad91d692a1};
  case digest_algorithm::sha512_256:
    return {0x22312194fc2bf72c, 0x9f555fa3c84c64c2, 0x2393b86b6f53b151, 0x963877195940eabd,
            0x96283ee2a88effe3, 0xbe5e1e2553863992, 0x2b0199fc2c85b8aa, 0x0eb72ddc81c52ca2};
  default:
    return {0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
            0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179};
  }
}

// Compute SHA-384, SHA-512, SHA-512/224, or SHA-512/256 with 128-byte blocks and 128-bit lengths.
void compute_sha512(digest_algorithm algorithm, std::span<const std::uint8_t> input,
                    std::span<std::uint8_t> output) noexcept {
  auto state = sha512_initial_state(algorithm);
  process_padded_blocks<sha512_block_bytes, false>(input, [&state](const std::uint8_t* block) {
    transform_sha2(state, block, sha512_round_constants);
  });
  write_state(state, output);
}

// Permute Keccak-f[1600] in x+5*y lane order using theta, rho/pi, chi, then iota each round.
void permute_sha3(std::array<std::uint64_t, sha3_state_lanes>& state) noexcept {
  constexpr std::size_t side = 5;
  for (const auto round_constant : sha3_round_constants) {
    std::array<std::uint64_t, side> columns{};
    for (std::size_t x = 0; x < side; ++x) {
      for (std::size_t y = 0; y < side; ++y) {
        columns[x] ^= state[x + side * y];
      }
    }
    for (std::size_t x = 0; x < side; ++x) {
      const auto delta = columns[(x + side - 1) % side] ^ std::rotl(columns[(x + 1) % side], 1);
      for (std::size_t y = 0; y < side; ++y) {
        state[x + side * y] ^= delta;
      }
    }
    std::array<std::uint64_t, sha3_state_lanes> moved{};
    for (std::size_t x = 0; x < side; ++x) {
      for (std::size_t y = 0; y < side; ++y) {
        const auto source = x + side * y;
        moved[y + side * ((2 * x + 3 * y) % side)] =
            std::rotl(state[source], sha3_rotation_counts[source]);
      }
    }
    for (std::size_t x = 0; x < side; ++x) {
      for (std::size_t y = 0; y < side; ++y) {
        state[x + side * y] = moved[x + side * y] ^ (~moved[(x + 1) % side + side * y] &
                                                     moved[(x + 2) % side + side * y]);
      }
    }
    state[0] ^= round_constant;
  }
}

// Compute a FIPS 202 SHA-3 fixed digest; suffix 0x06 separates SHA-3 from SHAKE and plain Keccak.
void compute_sha3(std::span<const std::uint8_t> input, std::span<std::uint8_t> output) noexcept {
  std::array<std::uint64_t, sha3_state_lanes> state{};
  const auto rate_bytes = sha3_state_bytes - 2 * output.size();
  std::size_t offset{};
  while (input.size() - offset >= rate_bytes) {
    for (std::size_t lane = 0; lane < rate_bytes / sha3_lane_bytes; ++lane) {
      state[lane] ^= load_word<std::uint64_t, true>(input.data() + offset + lane * sha3_lane_bytes);
    }
    permute_sha3(state);
    offset += rate_bytes;
  }
  for (std::size_t index = 0; index < input.size() - offset; ++index) {
    state[index / sha3_lane_bytes] ^= static_cast<std::uint64_t>(input[offset + index])
                                      << ((index % sha3_lane_bytes) * bits_per_byte);
  }
  const auto remainder = input.size() - offset;
  state[remainder / sha3_lane_bytes] ^= std::uint64_t{0x06}
                                        << ((remainder % sha3_lane_bytes) * bits_per_byte);
  state[(rate_bytes - 1) / sha3_lane_bytes] ^=
      std::uint64_t{0x80} << (((rate_bytes - 1) % sha3_lane_bytes) * bits_per_byte);
  permute_sha3(state);
  // All four fixed outputs fit in the first squeeze block; no further permutation is necessary.
  write_state<std::uint64_t, sha3_state_lanes, true>(state, output);
}

// Convert one ASCII hexadecimal digit, returning a negative sentinel for every other byte.
int hex_digit(char value) noexcept {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

} // namespace

// Validate the runtime selection and exact destination extent, then compute without dependencies.
void compute_digest(digest_algorithm algorithm, std::span<const std::uint8_t> input,
                    std::span<std::uint8_t> output) {
  const auto required_bytes = digest_size(algorithm);
  if (required_bytes == 0 || output.size() != required_bytes) {
    throw std::invalid_argument{"Digest algorithm or output length is invalid"};
  }
  if ((algorithm == digest_algorithm::sha1 || algorithm == digest_algorithm::sha224 ||
       algorithm == digest_algorithm::sha256) &&
      input.size() > (std::numeric_limits<std::uint64_t>::max() >> 3)) {
    throw std::invalid_argument{"Digest input exceeds the algorithm's 64-bit bit-length limit"};
  }
  switch (algorithm) {
  case digest_algorithm::md5:
    compute_md5(input, output);
    break;
  case digest_algorithm::sha1:
    compute_sha1(input, output);
    break;
  case digest_algorithm::sha224:
  case digest_algorithm::sha256:
    compute_sha256(algorithm, input, output);
    break;
  case digest_algorithm::sha384:
  case digest_algorithm::sha512:
  case digest_algorithm::sha512_224:
  case digest_algorithm::sha512_256:
    compute_sha512(algorithm, input, output);
    break;
  default:
    compute_sha3(input, output);
    break;
  }
}

// Reserve the exact hexadecimal extent before encoding, checking multiplication before overflow.
std::string digest_to_hex(std::span<const std::uint8_t> bytes) {
  constexpr std::string_view digits = "0123456789abcdef";
  constexpr std::size_t digits_per_byte = 2;
  std::string result;
  if (bytes.size() > result.max_size() / digits_per_byte) {
    throw std::length_error{"Digest hexadecimal representation exceeds string limits"};
  }
  result.resize(bytes.size() * digits_per_byte);
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    result[index * digits_per_byte] = digits[bytes[index] >> 4];
    result[index * digits_per_byte + 1] = digits[bytes[index] & 0x0f];
  }
  return result;
}

// Validate the entire text before committing bytes, keeping malformed input transactional.
bool digest_from_hex(std::string_view text, std::span<std::uint8_t> output) noexcept {
  constexpr std::size_t digits_per_byte = 2;
  if (text.size() % digits_per_byte != 0 || text.size() / digits_per_byte != output.size()) {
    return false;
  }
  if (!std::all_of(text.begin(), text.end(), [](char value) { return hex_digit(value) >= 0; })) {
    return false;
  }
  for (std::size_t index = 0; index < output.size(); ++index) {
    output[index] = static_cast<std::uint8_t>((hex_digit(text[index * digits_per_byte]) << 4) |
                                              hex_digit(text[index * digits_per_byte + 1]));
  }
  return true;
}

} // namespace rohit
