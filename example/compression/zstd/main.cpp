#include "example_support.hpp"

#include <rohit/stream.hpp>

// Write a checksummed Zstandard frame and decode a fresh value after exact validation.
int main() {
  using namespace compression_example;
  return run_example("Zstandard", [] {
    const auto original = make_document();
    rohit::full_stream_auto_alloc output;
    original.serialize_out<codec::binary_integer>(
        output, compression::zstd_options{.level = 3, .checksum = true}, output_limits());

    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    const auto decoded = codec::deserialize_exact<document, codec::binary_integer>(
        input, message_limits(), input_options(compression::format::zstd));
    return verify_round_trip(original, decoded, output.current_offset());
  });
}
