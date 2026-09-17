#include "fuzz_support.hpp"

#include <rohit/protobuf.hpp>

#include <cstddef>
#include <cstdint>

// Exercise the opt-in binary and text codecs without an external Protobuf runtime.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace rohit::serializer;
  using namespace qualification;
  if (size < fuzz_control_bytes || size - fuzz_control_bytes > fuzz_payload_bytes) {
    return 0;
  }
  const fuzz_input input{data, size};
  switch (input.selector % protobuf_protocol_count) {
  case 0:
    decode_input<protobuf_binary<serialize_type::in>>(input);
    break;
  case 1:
    decode_input<protojson<serialize_type::in>>(input);
    break;
  case 2:
    decode_input<textproto<serialize_type::in>>(input);
    break;
  }
  return 0;
}
