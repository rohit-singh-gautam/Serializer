// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/compression.hpp>
#include <rohit/stream_io.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <limits>
#include <stdexcept>

namespace rohit::serializer::compression::detail {

// Read one bounded borrowed source, polling only after validating each callback's result.
// Preserve normal EOF exception handling and transport failures; never close or flush the source.
inline full_stream_auto_alloc read_bounded_source(rohit::type_check::byte_input_stream auto& input,
                                                  std::size_t maximum_bytes,
                                                  cancellation_callback check_cancelled) {
  if (!check_cancelled) {
    return read_stream_bytes(input, maximum_bytes);
  }
  if (input.fail() || input.eof()) {
    throw std::ios_base::failure{"Input stream is not readable"};
  }
  maximum_bytes = std::min(maximum_bytes, rohit::detail::maximum_buffer_bytes);
  constexpr auto read_chunk_bytes = rohit::detail::stream_buffer_bytes<decltype(input)>;
  full_stream_auto_alloc result;
  for (;;) {
    check_cancelled();
    const auto remaining = maximum_bytes - result.current_offset();
    if (remaining == 0) {
      // Exact-bound EOF inspection borrows the source too, so cancellation surrounds its probe.
      try {
        static_cast<void>(input.peek());
      } catch (const std::ios_base::failure&) {
        if (input.bad() || !input.eof()) {
          throw;
        }
      }
      if (input.bad() || (input.fail() && !input.eof())) {
        throw std::ios_base::failure{"Unable to inspect input stream"};
      }
      if (!input.eof()) {
        throw std::length_error{"Input stream exceeds message byte limit"};
      }
      check_cancelled();
      return result;
    }
    const auto count =
        std::min({remaining, read_chunk_bytes,
                  static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())});
    result.reserve(count);
    try {
      input.read(reinterpret_cast<char*>(result.curr()), static_cast<std::streamsize>(count));
    } catch (const std::ios_base::failure&) {
      if (input.bad() || !input.eof()) {
        throw;
      }
    }
    const auto received = input.gcount();
    if (received < 0 || static_cast<std::size_t>(received) > count || input.bad() ||
        (input.fail() && !input.eof())) {
      throw std::ios_base::failure{"Unable to read complete message"};
    }
    if (!input.eof() && static_cast<std::size_t>(received) != count) {
      throw std::ios_base::failure{"Incomplete stream read without EOF"};
    }
    result.advance_unchecked(static_cast<std::size_t>(received));
    check_cancelled();
    if (input.eof()) {
      return result;
    }
  }
}

} // namespace rohit::serializer::compression::detail
