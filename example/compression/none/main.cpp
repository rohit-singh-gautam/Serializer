#include "example_support.hpp"

#include <rohit/stream.hpp>

#include <algorithm>
#include <stdexcept>

// Explicit identity options retain the bytes of an ordinary uncompressed message.
int main() {
  using namespace compression_example;
  return run_example("none", [] {
    const auto original = make_document();
    rohit::full_stream_auto_alloc output;
    original.serialize_out<codec::binary_integer>(output, compression::none_options{},
                                                  output_limits());

    rohit::full_stream_auto_alloc ordinary;
    original.serialize_out<codec::binary_integer>(ordinary);
    if (output.current_offset() != ordinary.current_offset() ||
        !std::equal(output.begin(), output.curr(), ordinary.begin())) {
      throw std::runtime_error{"Identity output changed the original wire bytes"};
    }
    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    document decoded{};
    decoded.serialize_in<codec::binary_integer>(input, message_limits(),
                                                input_options(compression::format::none));
    return verify_round_trip(original, decoded, output.current_offset());
  });
}
