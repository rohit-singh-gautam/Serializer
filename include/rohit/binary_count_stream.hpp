// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/binary_scalar.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer {

// Count native binary output without allocating bytes or exposing fictitious writable storage.
class binary_count_stream {
  std::size_t position_bytes_{};
  std::size_t maximum_bytes_;

public:
  static constexpr bool count_only = true;

  // Bound the complete traversal; an oversized individual append leaves its count unchanged.
  explicit constexpr binary_count_stream(
      std::size_t maximum_bytes = rohit::detail::maximum_buffer_bytes)
      : maximum_bytes_{maximum_bytes} {
    if (maximum_bytes > rohit::detail::maximum_buffer_bytes) {
      throw std::invalid_argument{"Invalid binary output byte limit"};
    }
  }

  // Return the successfully counted prefix, including keys and object terminators.
  [[nodiscard]] constexpr std::size_t size() const noexcept { return position_bytes_; }

  // Preflight a complete operation without consuming any capacity.
  constexpr void reserve(std::size_t bytes) const {
    if (bytes > maximum_bytes_ - position_bytes_) {
      throw rohit::exception::stream_overflow_exception{};
    }
  }

  // Count an operation after its wire semantics have been validated.
  constexpr void commit_counted_bytes(std::size_t bytes) {
    reserve(bytes);
    position_bytes_ += bytes;
  }

  // Count independent raw bytes without reading the borrowed source.
  constexpr void append_external(const void*, std::size_t bytes) { commit_counted_bytes(bytes); }

  // Count a raw scalar collection without touching its storage.
  constexpr void append(const void*, std::size_t bytes) { commit_counted_bytes(bytes); }

  // Count UTF-8 code units after protocol-level validation.
  constexpr void append(std::string_view value) { commit_counted_bytes(value.size()); }

  // Preserve the ordinary stream's owning-string overload.
  constexpr void append(const std::string& value) { append(std::string_view{value}); }

  // Count one presence, character, or boolean wire byte.
  constexpr void append(std::uint8_t) { commit_counted_bytes(sizeof(std::uint8_t)); }

  // Count a transformed scalar collection without running its byte-writing callback.
  template <typename Transform>
  constexpr void append_transformed(const void*, std::size_t, std::size_t output_bytes,
                                    Transform&&) {
    commit_counted_bytes(output_bytes);
  }

  // Count an already validated compact prefix in one operation.
  template <std::same_as<std::uint8_t>... Bytes>
  constexpr void write_raw(Bytes...) { commit_counted_bytes(sizeof...(Bytes)); }
};

namespace detail {
// Only real output buffers and the explicit allocation-free counter are native binary sinks.
template <typename Stream>
concept binary_counting_stream = std::same_as<Stream, binary_count_stream>;
} // namespace detail
} // namespace rohit::serializer
