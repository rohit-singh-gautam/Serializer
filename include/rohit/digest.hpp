// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace rohit {

// Select a standard, unkeyed digest; none denotes caller-provided bytes without a hash algorithm.
enum class digest_algorithm {
  none,
  md5,
  sha1,
  sha224,
  sha256,
  sha384,
  sha512,
  sha512_224,
  sha512_256,
  sha3_224,
  sha3_256,
  sha3_384,
  sha3_512
};

// Return the standard digest length in bytes, or zero for none and unrecognized values.
constexpr std::size_t digest_size(digest_algorithm algorithm) noexcept {
  switch (algorithm) {
  case digest_algorithm::md5:
    return 16;
  case digest_algorithm::sha1:
    return 20;
  case digest_algorithm::sha224:
  case digest_algorithm::sha512_224:
  case digest_algorithm::sha3_224:
    return 28;
  case digest_algorithm::sha256:
  case digest_algorithm::sha512_256:
  case digest_algorithm::sha3_256:
    return 32;
  case digest_algorithm::sha384:
  case digest_algorithm::sha3_384:
    return 48;
  case digest_algorithm::sha512:
  case digest_algorithm::sha3_512:
    return 64;
  default:
    return 0;
  }
}

// Hash bytes into an exactly sized output, throwing invalid_argument before modifying it on
// invalid algorithms/extents. Input and output may overlap; no input is retained or allocated.
// MD5 and SHA-1 serve legacy interoperability and must not protect against deliberate collisions.
void compute_digest(digest_algorithm algorithm, std::span<const std::uint8_t> input,
                    std::span<std::uint8_t> output);

// Compute an explicitly selected digest into its fixed-size, owning byte representation.
template <digest_algorithm Algorithm>
std::array<std::uint8_t, digest_size(Algorithm)> make_digest(std::span<const std::uint8_t> input) {
  static_assert(digest_size(Algorithm) != 0, "A digest computation requires a supported algorithm");
  std::array<std::uint8_t, digest_size(Algorithm)> result{};
  compute_digest(Algorithm, input, result);
  return result;
}

// Hash every byte of a string view, including embedded zero bytes; no text encoding is applied.
template <digest_algorithm Algorithm>
std::array<std::uint8_t, digest_size(Algorithm)> make_digest(std::string_view input) {
  return make_digest<Algorithm>(std::span<const std::uint8_t>{
      reinterpret_cast<const std::uint8_t*>(input.data()), input.size()});
}

// Encode bytes as lowercase hexadecimal; the returned string owns its text.
std::string digest_to_hex(std::span<const std::uint8_t> bytes);

// Decode exact-length ASCII hexadecimal (either case), returning false without changing output
// for malformed input. Whitespace, prefixes, odd lengths, and truncated/oversized digests fail.
// The text must not overlap output; neither buffer is retained.
bool digest_from_hex(std::string_view text, std::span<std::uint8_t> output) noexcept;

} // namespace rohit
