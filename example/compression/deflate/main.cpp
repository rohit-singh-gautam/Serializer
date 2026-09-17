#include "example_support.hpp"

#include <rohit/stream.hpp>

#include <cstdint>
#include <span>

// Separate serialization from raw DEFLATE compression when the transport owns byte buffers.
int main() {
  using namespace compression_example;
  return run_example("raw DEFLATE", [] {
    const auto original = make_document();
    rohit::full_stream_auto_alloc serialized;
    original.serialize_out<codec::binary_integer>(serialized);
    const std::span<const std::uint8_t> bytes{serialized.begin(), serialized.current_offset()};
    const auto compressed =
        compression::compress(bytes, compression::deflate_options{.level = 6}, output_limits());

    // Raw DEFLATE has no wrapper/checksum; selection and message extent are explicit.
    const auto expanded =
        compression::decompress(compressed, input_options(compression::format::deflate));
    const auto input = rohit::make_constant_full_stream(expanded.data(), expanded.size());
    const auto decoded =
        codec::deserialize_exact<document, codec::binary_integer>(input, message_limits());
    return verify_round_trip(original, decoded, compressed.size());
  });
}
