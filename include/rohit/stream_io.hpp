#pragma once

#include <rohit/stream.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <ios>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace rohit {

namespace detail {
// Only standard string streams promise that view() describes the complete stable character area.
template <typename T>
concept memory_input_stream =
    type_check::byte_input_stream<T> && (std::same_as<std::remove_cvref_t<T>, std::istringstream> ||
                                         std::same_as<std::remove_cvref_t<T>, std::stringstream>);

// Known file streams amortize stream calls with larger batches; erased/custom streams stay generic.
template <typename T>
inline constexpr bool is_file_stream = std::same_as<std::remove_cvref_t<T>, std::ifstream> ||
                                       std::same_as<std::remove_cvref_t<T>, std::ofstream> ||
                                       std::same_as<std::remove_cvref_t<T>, std::fstream>;

inline constexpr std::size_t generic_stream_buffer_bytes = 8192;
inline constexpr std::size_t file_stream_buffer_bytes = 64 * 1024;
template <typename T>
inline constexpr std::size_t stream_buffer_bytes =
    is_file_stream<T> ? file_stream_buffer_bytes : generic_stream_buffer_bytes;
} // namespace detail

// Write bytes without changing the caller's formatting, exception mask, or flush policy.
// A failed write may already have delivered a prefix to the external destination.
inline void write_stream_bytes(type_check::byte_output_stream auto& output,
                               const std::uint8_t* bytes, std::size_t size) {
  if (output.fail()) {
    throw std::ios_base::failure{"Output stream is not writable"};
  }
  constexpr auto maximum_write_bytes =
      static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
  while (size != 0) {
    const auto count = std::min(size, maximum_write_bytes);
    output.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(count));
    if (output.fail()) {
      throw std::ios_base::failure{"Unable to write complete message"};
    }
    bytes += count;
    size -= count;
  }
}

// Adapt a byte sink to the contiguous reservation API, flushing between complete reservations.
// Large single reservations grow the scratch buffer; aliases retain native buffer semantics.
// Call finish explicitly: destruction releases scratch storage and never performs fallible I/O.
template <type_check::byte_output_stream Output>
class buffered_output_stream : public full_stream_auto_alloc {
  Output& destination;
  bool retain_buffer{};
  bool failed{};

  // Test whether a source borrows storage that a flush must not recycle beneath its writer.
  bool aliases_storage(const std::uint8_t* source) const {
    const auto less = std::less<const std::uint8_t*>{};
    return begin_data != nullptr && !less(source, begin_data) && less(source, end_data);
  }

  // Restore the reservation policy even when a capacity or allocation check fails.
  struct retention_scope {
    bool& state;
    const bool previous;
    // Retain storage if this reservation has at least one aliased source.
    retention_scope(bool& state, bool retain) : state{state}, previous{state} {
      state = state || retain;
    }
    // Restore the enclosing reservation's policy.
    ~retention_scope() {
      state = previous;
    }
  };

  // Deliver pending bytes once; a partial external failure permanently poisons this adapter.
  void drain() {
    if (failed) {
      throw std::ios_base::failure{"Buffered output has already failed"};
    }
    failed = true;
    write_stream_bytes(destination, begin_data, current_offset());
    failed = false;
    reset();
  }

protected:
  // Preserve native alias rebasing and avoid recycling a source borrowed from this buffer.
  void reserve_append(const std::uint8_t*& source, std::size_t size) override {
    const retention_scope policy{retain_buffer, aliases_storage(source)};
    full_stream_auto_alloc::reserve_append(source, size);
  }

  // Preserve all transformed or text-fragment sources across the single reservation.
  void reserve_fragments(std::span<detail::write_fragment> fragments, std::size_t size) override {
    const bool aliased = std::any_of(fragments.begin(), fragments.end(), [this](const auto& item) {
      return !item.is_byte && item.size != 0 && aliases_storage(item.data);
    });
    const retention_scope policy{retain_buffer, aliased};
    full_stream_auto_alloc::reserve_fragments(fragments, size);
  }

public:
  static constexpr std::size_t default_capacity_bytes = detail::stream_buffer_bytes<Output>;
  using full_stream_auto_alloc::reserve;

  // Borrow the sink and allocate reusable scratch storage for this encoding session.
  explicit buffered_output_stream(Output& output,
                                  std::size_t capacity_bytes = default_capacity_bytes)
      : full_stream_auto_alloc{capacity_bytes}, destination{output} {}

  // Flush completed batches before growing; a single batch remains contiguous and unwritten on failure.
  void reserve(std::size_t size) override {
    if (failed) {
      throw std::ios_base::failure{"Buffered output has already failed"};
    }
    if (size > detail::maximum_buffer_bytes) {
      throw exception::stream_overflow_exception{};
    }
    if (!retain_buffer && size > remaining_buffer()) {
      drain();
    }
    full_stream_auto_alloc::reserve(size);
  }

  // Deliver the final batch without flushing or closing the caller's underlying stream.
  void finish() {
    drain();
  }
};

// Borrow a standard memory stream's unread suffix without copying its encoded message.
// The caller must keep the stream's storage alive and unchanged until decoding finishes.
// Consume through EOF before decoding, like the generic whole-message input path.
inline const full_stream borrow_stream_bytes(detail::memory_input_stream auto& input,
                                             std::size_t maximum_bytes) {
  if (input.fail() || input.eof()) {
    throw std::ios_base::failure{"Input stream is not readable"};
  }
  const auto position = input.tellg();
  const auto storage = input.view();
  if (position < std::streampos{} ||
      static_cast<std::uintmax_t>(static_cast<std::streamoff>(position)) > storage.size()) {
    throw std::ios_base::failure{"Invalid memory stream read position"};
  }
  const auto bytes =
      storage.substr(static_cast<std::size_t>(static_cast<std::streamoff>(position)));
  if (bytes.size() > maximum_bytes || bytes.size() > detail::maximum_buffer_bytes) {
    throw std::length_error{"Input stream exceeds message byte limit"};
  }
  input.seekg(0, std::ios::end);
  if (input.fail()) {
    throw std::ios_base::failure{"Unable to consume memory stream"};
  }
  try {
    static_cast<void>(input.peek());
  } catch (const std::ios_base::failure&) {
    if (input.bad() || !input.eof()) {
      throw;
    }
  }
  if (input.bad() || !input.eof()) {
    throw std::ios_base::failure{"Unable to finish memory stream input"};
  }
  return make_constant_full_stream(bytes.data(), bytes.size());
}

// Load one EOF-delimited message under a byte bound; no seeking or ownership transfer is needed.
// Normal EOF is accepted even when the caller enables EOF/fail exceptions; error state is retained.
inline full_stream_auto_alloc read_stream_bytes(type_check::byte_input_stream auto& input,
                                                std::size_t maximum_bytes) {
  if (input.fail() || input.eof()) {
    throw std::ios_base::failure{"Input stream is not readable"};
  }
  maximum_bytes = std::min(maximum_bytes, detail::maximum_buffer_bytes);
  constexpr auto read_chunk_bytes = detail::stream_buffer_bytes<decltype(input)>;
  full_stream_auto_alloc result;
  while (true) {
    const auto remaining = maximum_bytes - result.current_offset();
    if (remaining == 0) {
      // Inspect, without consuming, one byte to distinguish exact-limit EOF from oversized input.
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
      return result;
    }
    const auto count =
        std::min({remaining, read_chunk_bytes,
                  static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())});
    result.reserve(count);
    try {
      // Read directly into the final staging allocation, avoiding an intermediate chunk copy.
      input.read(reinterpret_cast<char*>(result.curr()), static_cast<std::streamsize>(count));
    } catch (const std::ios_base::failure&) {
      if (input.bad() || !input.eof()) {
        throw;
      }
    }
    const std::streamsize received = input.gcount();
    if (received < 0 || static_cast<std::size_t>(received) > count || input.bad() ||
        (input.fail() && !input.eof())) {
      throw std::ios_base::failure{"Unable to read complete message"};
    }
    result.advance_unchecked(static_cast<std::size_t>(received));
    if (input.eof()) {
      return result;
    }
    if (static_cast<std::size_t>(received) != count) {
      throw std::ios_base::failure{"Incomplete stream read without EOF"};
    }
  }
}

} // namespace rohit
