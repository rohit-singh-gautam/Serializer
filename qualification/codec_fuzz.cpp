#include "fuzz_support.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Select a native protocol or primitive decoder with exact extents and input-selected budgets.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  using namespace rohit::serializer;
  using namespace qualification;
  if (size < fuzz_control_bytes || size - fuzz_control_bytes > fuzz_payload_bytes) {
    return 0;
  }
  const fuzz_input input{data, size};
  switch (input.selector % native_protocol_count) {
  case 0:
    decode_input<json<serialize_type::in>>(input);
    break;
  case 1:
    decode_input<binary_none<serialize_type::in>>(input);
    break;
  case 2:
    decode_input<binary_integer<serialize_type::in>>(input);
    break;
  case 3:
    decode_input<binary_string<serialize_type::in>>(input);
    break;
  case 4:
    decode_input<json<serialize_type::in>, std::string>(input);
    break;
  case 5:
    decode_input<json<serialize_type::in>, double>(input);
    break;
  case 6:
    decode_input<json<serialize_type::in>, std::vector<std::vector<int>>>(input);
    break;
  case 7:
    reject_invalid_input([&] {
      const auto stream = rohit::make_constant_full_stream(input.bytes.data(), input.bytes.size());
      binary_none<serialize_type::in> decoder{stream, input.limits};
      decoder.serialize_in_variable();
      decoder.finish();
    });
    break;
  case 8:
    decode_input<binary<serialize_type::in, serialize_key_type::none, std::endian::big>>(input);
    break;
  case 9:
    decode_input<binary<serialize_type::in, serialize_key_type::integer, std::endian::big>>(input);
    break;
  case 10:
    decode_input<binary<serialize_type::in, serialize_key_type::string, std::endian::big>>(input);
    break;
  }
  return 0;
}
