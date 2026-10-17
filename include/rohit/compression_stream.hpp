// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/compression.hpp>
#include <rohit/stream_io.hpp>

#include <cstddef>
#include <cstdint>
#include <ios>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace rohit::serializer::compression {

namespace detail {
class encoding_state;
}

// Encode one complete message through bounded backend chunks. No record protocol is introduced.
// The sink is borrowed until destruction. A failed operation fences the encoder permanently;
// output already delivered to external storage cannot be rolled back. Destruction never finishes.
class stream_encoder {
  std::unique_ptr<detail::encoding_state> state;

public:
  // Validate configuration before creating backend state; construction does not write to the sink.
  stream_encoder(output_sink& output, const encode_options& options, encode_limits limits = {});
  // Release backend state without writing a trailer or invoking borrowed resources.
  ~stream_encoder();
  // Encoder state and its borrowed destination cannot be copied or transferred.
  stream_encoder(const stream_encoder&) = delete;
  stream_encoder& operator=(const stream_encoder&) = delete;
  stream_encoder(stream_encoder&&) = delete;
  stream_encoder& operator=(stream_encoder&&) = delete;
  // Supply bytes of the same whole message; backend storage and total input/output are bounded.
  void write(std::span<const std::uint8_t> bytes);
  // Complete exactly one frame/member and its trailer; repeated successful completion is a no-op.
  void finish();
  // Report whether a prior write or finalization failed; failed encoders cannot be reused.
  [[nodiscard]] bool failed() const noexcept;
  // Report successful explicit finalization independently of failure state.
  [[nodiscard]] bool finished() const noexcept;
  // Expose the remaining message bound so codec scratch rejects oversized reservations early.
  [[nodiscard]] std::size_t remaining_input_bytes() const noexcept;
};

// A byte-sink wrapper accepted by existing whole-object serialize_to calls. Codecs use their
// ordinary contiguous reservation adapter; a single large codec reservation may grow scratch.
// Built-in compressors consume chunks without staging the whole message. Custom adapters retain
// their existing whole-span contract and therefore stage at most max_input_bytes. One wrapper
// represents one message; call finish() explicitly after serializing the entire object.
template <rohit::type_check::output_stream Output>
class output_stream {
  class destination_sink final : public output_sink {
    Output& destination;

  public:
    // Borrow an output stream whose lifetime exceeds this wrapper's lifetime.
    explicit destination_sink(Output& destination) : destination{destination} {}
    // Forward a compressed chunk without closing, flushing, or synchronizing its destination.
    void append(std::span<const std::uint8_t> bytes) override {
      write_stream_bytes(destination, bytes);
    }
  } sink;
  stream_encoder encoder;
  bool failed_{};

public:
  // Select a fixed compressor for one whole object; the output stream remains caller-owned.
  output_stream(Output& output, const encode_options& options, encode_limits limits = {})
      : sink{output}, encoder{sink, options, limits} {}
  // Forward one codec batch; an invalid range or sink/backend failure permanently fences writes.
  void write(const char* bytes, std::streamsize size) {
    if (fail() || size < 0 || (size != 0 && bytes == nullptr)) {
      failed_ = true;
      throw std::ios_base::failure{"Compression output is not writable"};
    }
    try {
      encoder.write({reinterpret_cast<const std::uint8_t*>(bytes), static_cast<std::size_t>(size)});
    } catch (...) {
      failed_ = true;
      throw;
    }
  }
  // Expose adapter/backend failure to standard-style Serializer stream helpers.
  [[nodiscard]] bool fail() const noexcept {
    return failed_ || encoder.failed();
  }
  // Emit the complete stream trailer without flushing or closing the borrowed destination.
  void finish() {
    if (fail()) {
      throw std::ios_base::failure{"Compression output has failed"};
    }
    try {
      encoder.finish();
    } catch (...) {
      failed_ = true;
      throw;
    }
  }
  // Report whether the required explicit finalization has succeeded.
  [[nodiscard]] bool finished() const noexcept {
    return encoder.finished();
  }
  // Bound Serializer's contiguous codec adapter before a single large reservation allocates.
  [[nodiscard]] std::size_t max_buffer_bytes() const noexcept {
    return encoder.remaining_input_bytes();
  }
};

namespace detail {
// Validate one complete bounded compressed extent before exposing any decompressed bytes.
template <rohit::type_check::input_stream Input>
std::vector<std::uint8_t> read_compressed_message(Input& input, const decode_options& options) {
  validate(options);
  if constexpr (rohit::type_check::input_buffer<Input>) {
    const auto size = input.remaining_buffer();
    auto bytes = decompress({input.curr(), size}, options);
    input.get_curr_and_increase_unchecked(size);
    return bytes;
  } else if constexpr (rohit::detail::memory_input_stream<Input>) {
    const auto view = borrow_stream_bytes(input, options.max_compressed_bytes);
    return decompress({view.curr(), view.remaining_buffer()}, options);
  } else {
    const auto bytes = read_stream_bytes(input, options.max_compressed_bytes);
    return decompress({bytes.begin(), bytes.current_offset()}, options);
  }
}
} // namespace detail

// Own the validated, decompressed bytes of one whole message. General byte sources are read to
// EOF; buffer sources must already be bounded to one frame/member. Input and decompressed output
// may both be staged, so this is bounded whole-message decoding rather than constant-memory input.
// Use deserialize_exact to additionally enforce that the object consumes the complete message.
template <rohit::type_check::input_stream Input>
class input_stream : public rohit::stream {
  std::vector<std::uint8_t> storage;

public:
  // Consume the source only under configured bounds; validate checksums/trailers before decoding.
  input_stream(Input& input, const decode_options& options)
      : rohit::stream{}, storage{detail::read_compressed_message(input, options)} {
    current_data = storage.data();
    end_data = storage.empty() ? current_data : current_data + storage.size();
  }
  // Owned bytes and their cursor must retain the same address throughout decoding.
  input_stream(const input_stream&) = delete;
  input_stream& operator=(const input_stream&) = delete;
  input_stream(input_stream&&) = delete;
  input_stream& operator=(input_stream&&) = delete;
  // Require complete object consumption after decoding; JSON callers should use decoder.finish().
  void finish() const {
    if (!full()) {
      throw error{error_code::invalid_data, "Trailing decompressed message data"};
    }
  }
};

} // namespace rohit::serializer::compression
