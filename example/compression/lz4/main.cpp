#include "example_support.hpp"

#include <rohit/stream.hpp>

// Use the generated static APIs for a standard LZ4 frame with a content checksum.
int main() {
  using namespace compression_example;
  return run_example("LZ4 frame", [] {
    const auto original = make_document();
    rohit::full_stream_auto_alloc output;
    document::serialize<codec::binary_integer>(
        output, original, compression::lz4_options{.level = 0, .checksum = true}, output_limits());

    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    const auto decoded = document::deserialize<codec::binary_integer>(
        input, message_limits(), input_options(compression::format::lz4));
    return verify_round_trip(original, decoded, output.current_offset());
  });
}
