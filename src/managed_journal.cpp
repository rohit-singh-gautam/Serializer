#include <rohit/managed_journal_stream.hpp>

#include <stdexcept>

namespace rohit::managed::detail {
constexpr std::uint64_t crc_polynomial = 0x42f0e1eba9ea3693;
constexpr unsigned wire_byte_bits = 8;
constexpr unsigned crc_shift = 56;
constexpr unsigned crc_top_bit = 63;

// Construct a CRC-64/ECMA-182 table; checksums detect damage, not malicious replacement.
constexpr auto make_crc_table() {
  std::array<std::uint64_t, 256> result{};
  for (std::size_t index = 0; index < result.size(); ++index) {
    auto value = static_cast<std::uint64_t>(index) << crc_shift;
    for (unsigned bit = 0; bit < wire_byte_bits; ++bit) {
      value = (value << 1) ^ ((value >> crc_top_bit) != 0 ? crc_polynomial : 0);
    }
    result[index] = value;
  }
  return result;
}
constexpr auto crc_table = make_crc_table();

// Extend the standard zero-initialized, non-reflected CRC over a byte range.
std::uint64_t checksum(std::span<const std::uint8_t> input, std::uint64_t crc) {
  for (const auto byte : input) {
    crc = (crc << wire_byte_bits) ^ crc_table[(crc >> crc_shift) ^ byte];
  }
  return crc;
}

// Encode fixed wire integers explicitly, independently of native layout and byte order.
void put_word(std::vector<std::uint8_t>& output, std::uint64_t value) {
  for (std::size_t index = 0; index < wire_word_bytes; ++index) {
    output.push_back(static_cast<std::uint8_t>(value >> (index * wire_byte_bits)));
  }
}

// Read a word from an already bounds-checked fixed header or footer.
std::uint64_t get_word(std::span<const std::uint8_t> input, std::size_t word) {
  std::uint64_t result = 0;
  for (std::size_t index = 0; index < wire_word_bytes; ++index) {
    result |= static_cast<std::uint64_t>(input[word * wire_word_bytes + index])
              << (index * wire_byte_bits);
  }
  return result;
}

// Extend integrity state with one fixed little-endian word using stack storage.
std::uint64_t checksum_word(std::uint64_t word, std::uint64_t crc = 0) {
  std::array<std::uint8_t, wire_word_bytes> bytes{};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<std::uint8_t>(word >> (index * wire_byte_bits));
  }
  return checksum(bytes, crc);
}

// Store a fixed wire word without temporary buffers or allocation; offsets are validated by callers.
void store_word(std::span<std::uint8_t> output, std::size_t offset, std::uint64_t value) {
  for (std::size_t index = 0; index < wire_word_bytes; ++index) {
    output[offset + index] = static_cast<std::uint8_t>(value >> (index * wire_byte_bits));
  }
}

// Frame existing serialized buffers directly. Sequence/chain are implicit checksum inputs,
// avoiding repeated magic, document identity, sequence, and duplicate commit fields per write.
encoded_frame encode_frame(std::initializer_list<std::span<const std::uint8_t>> parts,
                           std::uint64_t sequence, std::uint64_t previous,
                           const journal_options& options) {
  encoded_frame result;
  for (const auto part : parts) {
    if (part.size() > options.max_record_bytes - result.payload_bytes) {
      throw std::length_error{"Managed journal record budget exceeded"};
    }
    result.payload_bytes += part.size();
  }
  if (result.payload_bytes == 0) {
    throw std::length_error{"Empty managed journal record"};
  }
  const auto seed = checksum_word(previous, checksum_word(sequence));
  store_word(result.header, 0, result.payload_bytes);
  store_word(result.header, wire_word_bytes,
             checksum(std::span{result.header}.first(wire_word_bytes), seed));
  result.digest = checksum(result.header, seed);
  for (const auto part : parts) {
    result.digest = checksum(part, result.digest);
  }
  store_word(result.footer, 0, result.digest);
  return result;
}

// Share length validation between contiguous buffers and sequential byte streams.
std::size_t frame_payload_size(std::span<const std::uint8_t> header, std::uint64_t sequence,
                               std::uint64_t previous, const journal_options& options) {
  if (header.size() != frame_header_bytes) {
    throw std::invalid_argument{"Incomplete managed journal frame header"};
  }
  const auto seed = checksum_word(previous, checksum_word(sequence));
  if (get_word(header, 1) != checksum(header.first(wire_word_bytes), seed)) {
    throw std::invalid_argument{"Invalid managed journal frame or sequence"};
  }
  const auto length = get_word(header, 0);
  if (length == 0 || length > options.max_record_bytes) {
    throw std::length_error{"Managed journal record budget exceeded"};
  }
  return static_cast<std::size_t>(length);
}

// Verify a fully received record before exposing its payload to replay.
std::uint64_t frame_digest(std::span<const std::uint8_t> header,
                           std::span<const std::uint8_t> payload,
                           std::span<const std::uint8_t> footer, std::uint64_t sequence,
                           std::uint64_t previous) {
  if (header.size() != frame_header_bytes || footer.size() != frame_footer_bytes) {
    throw std::invalid_argument{"Incomplete managed journal framing"};
  }
  const auto seed = checksum_word(previous, checksum_word(sequence));
  const auto digest = checksum(payload, checksum(header, seed));
  if (get_word(footer, 0) != digest) {
    throw std::invalid_argument{"Corrupt managed journal committed frame"};
  }
  return digest;
}

// Validate compact length/commit framing before borrowing a payload slice. Incomplete tails
// are ignored; header integrity protects lengths and detects missing, reordered, or duplicate records.
std::optional<decoded_frame> decode_frame(std::span<const std::uint8_t> input, std::size_t offset,
                                          std::uint64_t sequence, std::uint64_t previous,
                                          const journal_options& options) {
  if (offset > input.size()) {
    throw std::invalid_argument{"Invalid managed journal read offset"};
  }
  auto remaining = input.subspan(offset);
  if (remaining.size() < frame_header_bytes) {
    return {};
  }
  const auto header = remaining.first(frame_header_bytes);
  const auto length = frame_payload_size(header, sequence, previous, options);
  remaining = remaining.subspan(frame_header_bytes);
  if (length > remaining.size() ||
      remaining.size() - static_cast<std::size_t>(length) < frame_footer_bytes) {
    return {};
  }
  const auto payload = remaining.first(static_cast<std::size_t>(length));
  const auto footer = remaining.subspan(payload.size(), frame_footer_bytes);
  const auto digest = frame_digest(header, payload, footer, sequence, previous);
  return decoded_frame{payload, digest, offset + frame_overhead_bytes + payload.size()};
}

// Share the compact operation integer convention with the model store without allocation.
void journal_put_word(std::span<std::uint8_t> output, std::size_t offset, std::uint64_t value) {
  if (offset > output.size() || output.size() - offset < wire_word_bytes) {
    throw std::invalid_argument{"Managed journal prefix is too short"};
  }
  store_word(output, offset, value);
}

// Never read a truncated operation integer or subtract an unchecked offset.
std::uint64_t journal_get_word(std::span<const std::uint8_t> input, std::size_t& offset) {
  if (offset > input.size() || input.size() - offset < wire_word_bytes) {
    throw std::invalid_argument{"Truncated managed journal operation"};
  }
  const auto value = get_word(input.subspan(offset, wire_word_bytes), 0);
  offset += wire_word_bytes;
  return value;
}

} // namespace rohit::managed::detail
