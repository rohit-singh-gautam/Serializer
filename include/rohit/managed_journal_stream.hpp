#pragma once

#include <rohit/managed_journal.hpp>
#include <rohit/stream_io.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rohit::managed {
namespace detail {
inline constexpr std::size_t wire_word_bytes = 8;
inline constexpr std::size_t frame_header_bytes = 2 * wire_word_bytes;
inline constexpr std::size_t frame_footer_bytes = wire_word_bytes;
inline constexpr std::size_t frame_overhead_bytes = frame_header_bytes + frame_footer_bytes;

// Incremental CRC-64/ECMA-182 used by both file containers and stream frames, not authentication.
std::uint64_t checksum(std::span<const std::uint8_t> input, std::uint64_t crc = 0);
// Read a bounds-checked fixed wire word; the caller supplies at least (word + 1) * 8 bytes.
std::uint64_t get_word(std::span<const std::uint8_t> input, std::size_t word);
// Append a little-endian integer while building an infrequently written container header.
void put_word(std::vector<std::uint8_t>& output, std::uint64_t value);
// Validate header integrity and its bounded payload length before allocating or consuming a payload.
std::size_t frame_payload_size(std::span<const std::uint8_t> header, std::uint64_t sequence,
                               std::uint64_t previous, const journal_options& options);
// Validate the commit CRC after reading all bytes; sequence/chain are implicit integrity inputs.
std::uint64_t frame_digest(std::span<const std::uint8_t> header,
                           std::span<const std::uint8_t> payload,
                           std::span<const std::uint8_t> footer, std::uint64_t sequence,
                           std::uint64_t previous);

struct encoded_frame {
  std::array<std::uint8_t, frame_header_bytes> header{};
  std::array<std::uint8_t, frame_footer_bytes> footer{};
  std::size_t payload_bytes{};
  std::uint64_t digest{};
};

// Prepare stack framing over existing buffers without concatenation; no I/O takes place here.
encoded_frame encode_frame(std::initializer_list<std::span<const std::uint8_t>> parts,
                           std::uint64_t sequence, std::uint64_t previous,
                           const journal_options& options);

struct decoded_frame {
  std::span<const std::uint8_t> payload{};
  std::uint64_t digest{};
  std::size_t end_offset{};
};

// Borrow a complete verified buffer record; an incomplete final record returns no value.
std::optional<decoded_frame> decode_frame(std::span<const std::uint8_t> input, std::size_t offset,
                                          std::uint64_t sequence, std::uint64_t previous,
                                          const journal_options& options);

// Write prepared framing through existing buffer/iostream adapters. Callers retain all part bytes
// unchanged from encode_frame through return. This function does not flush or claim durability.
template <type_check::output_stream Output>
void write_frame(Output& output, const encoded_frame& frame,
                 std::initializer_list<std::span<const std::uint8_t>> parts,
                 const journal_options& options) {
  write_stream_bytes(output, frame.header);
  if (options.fault_injector) {
    options.fault_injector(journal_io_event::after_header);
  }
  for (const auto part : parts) {
    write_stream_bytes(output, part);
  }
  if (options.fault_injector) {
    options.fault_injector(journal_io_event::after_payload);
  }
  write_stream_bytes(output, frame.footer);
  if (options.fault_injector) {
    options.fault_injector(journal_io_event::after_commit);
  }
}
} // namespace detail

// A verified record borrows contiguous input or owns bytes read from a byte stream.
// Borrowed input must outlive this result and remain unchanged. Byte-stream payloads own their data.
class journal_frame {
  std::vector<std::uint8_t> owned_{};
  std::span<const std::uint8_t> borrowed_{};

public:
  std::uint64_t digest{};
  // Preserve a stable borrowed payload without copying it.
  journal_frame(std::span<const std::uint8_t> payload, std::uint64_t crc)
      : borrowed_{payload}, digest{crc} {}
  // Own an exact-size byte-stream payload without another allocation or copy.
  journal_frame(std::vector<std::uint8_t> payload, std::uint64_t crc)
      : owned_{std::move(payload)}, digest{crc} {}
  // Derive owned views on demand so moving/copying a record never leaves an internal stale span.
  std::span<const std::uint8_t> payload() const noexcept {
    return owned_.empty() ? borrowed_ : std::span<const std::uint8_t>{owned_};
  }
};

// Append one version-two frame to any supported output stream and return its chain checksum.
// Parts must remain valid and unchanged independently of destination storage.
// Byte writes may partially succeed; the caller owns failure fencing, synchronization, and sequence.
template <type_check::output_stream Output>
std::uint64_t write_journal_frame(Output& output,
                                  std::initializer_list<std::span<const std::uint8_t>> parts,
                                  std::uint64_t sequence, std::uint64_t previous,
                                  const journal_options& options = {}) {
  const auto frame = detail::encode_frame(parts, sequence, previous, options);
  detail::write_frame(output, frame, parts, options);
  return frame.digest;
}

// Read exactly one frame, never consuming the following frame or requiring EOF as a delimiter.
// Incomplete input returns nullopt (byte streams consume that prefix, buffers remain unadvanced).
// Complete corruption, invalid sequence, I/O errors, and budget violations throw.
template <type_check::input_stream Input>
std::optional<journal_frame> read_journal_frame(Input& input, std::uint64_t sequence,
                                                std::uint64_t previous,
                                                const journal_options& options = {}) {
  if constexpr (type_check::input_buffer<Input>) {
    const auto frame = detail::decode_frame({input.curr(), input.remaining_buffer()}, 0, sequence,
                                            previous, options);
    if (!frame) {
      return {};
    }
    input.get_curr_and_increase_unchecked(frame->end_offset);
    return journal_frame{frame->payload, frame->digest};
  } else {
    std::array<std::uint8_t, detail::frame_header_bytes> header{};
    if (read_stream_some(input, header) != header.size()) {
      return {};
    }
    const auto length = detail::frame_payload_size(header, sequence, previous, options);
    std::vector<std::uint8_t> payload(length);
    if (read_stream_some(input, payload) != payload.size()) {
      return {};
    }
    std::array<std::uint8_t, detail::frame_footer_bytes> footer{};
    if (read_stream_some(input, footer) != footer.size()) {
      return {};
    }
    const auto crc = detail::frame_digest(header, payload, footer, sequence, previous);
    return journal_frame{std::move(payload), crc};
  }
}

} // namespace rohit::managed
