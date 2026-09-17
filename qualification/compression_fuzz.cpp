#include "fuzz_support.hpp"

#include <rohit/compression.hpp>

#include <array>
#include <cstdlib>

// Decode hostile frames under independent transport/output/window budgets and parse successful bytes.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace codec = rohit::serializer;
  namespace compression = codec::compression;
  using namespace qualification;
  if (size < fuzz_control_bytes || size - fuzz_control_bytes > fuzz_payload_bytes) {
    return 0;
  }
  const fuzz_input input{data, size};
  constexpr std::array formats{compression::format::zstd, compression::format::lz4,
                               compression::format::gzip, compression::format::zlib,
                               compression::format::deflate};
  const auto format = formats[input.selector % formats.size()];
  if (!compression::available(format)) {
    return 0;
  }
  compression::decode_options options{.format = format,
                                      .max_compressed_bytes = fuzz_payload_bytes,
                                      .max_decompressed_bytes = fuzz_payload_bytes,
                                      .max_window_bytes = compression::mebibyte};
  switch (data[1] % fuzz_policy_count) {
  case 1:
    options.max_compressed_bytes = 16;
    break;
  case 2:
    options.max_decompressed_bytes = 16;
    break;
  case 3:
    options.max_window_bytes = 1024;
    break;
  case 4:
    options.max_decompressed_bytes = 0;
    break;
  default:
    break;
  }
  try {
    const auto bytes = compression::decompress(input.bytes, options);
    reject_invalid_input([&] {
      const auto source = rohit::make_constant_full_stream(bytes.data(), bytes.size());
      static_cast<void>(
          codec::deserialize_exact<owning_record, codec::binary_none>(source, input.limits));
    });
  } catch (const compression::error&) {
  }
  return 0;
}
