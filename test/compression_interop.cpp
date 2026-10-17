#include <rohit/compression.hpp>
#include <rohit/compression_stream.hpp>
#include <rohit/stream_io.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string_view>

// Read/write files through the public adapter so external tools can check both directions.
int main(int argc, char** argv) {
  namespace compression = rohit::serializer::compression;
  try {
    if (argc != 5) {
      throw std::invalid_argument{"Expected MODE FORMAT INPUT OUTPUT"};
    }
    const std::string_view mode{argv[1]};
    const std::string_view name{argv[2]};
    compression::format format;
    compression::encode_options options;
    if (name == "zstd") {
      format = compression::format::zstd;
      options = compression::zstd_options{};
    } else if (name == "lz4") {
      format = compression::format::lz4;
      options = compression::lz4_options{};
    } else if (name == "gzip") {
      format = compression::format::gzip;
      options = compression::gzip_options{};
    } else if (name == "zlib") {
      format = compression::format::zlib;
      options = compression::zlib_options{};
    } else if (name == "deflate") {
      format = compression::format::deflate;
      options = compression::deflate_options{};
    } else {
      throw std::invalid_argument{"Unknown compression format"};
    }
    std::ifstream input{argv[3], std::ios::binary};
    const auto storage = rohit::read_stream_bytes(input, 8 * compression::mebibyte);
    const std::span bytes{storage.begin(), storage.current_offset()};
    std::vector<std::uint8_t> result;
    if (mode == "stream_encode" || mode == "stream_decode") {
      std::ofstream output{argv[4], std::ios::binary};
      output.exceptions(std::ios::failbit | std::ios::badbit);
      if (mode == "stream_encode") {
        compression::output_stream compressed{output, options};
        constexpr std::size_t batch_bytes = 997;
        auto remaining = bytes;
        while (!remaining.empty()) {
          const auto count = std::min(remaining.size(), batch_bytes);
          compressed.write(reinterpret_cast<const char*>(remaining.data()),
                           static_cast<std::streamsize>(count));
          remaining = remaining.subspan(count);
        }
        compressed.finish();
      } else {
        const auto source = rohit::make_constant_full_stream(bytes.data(), bytes.size());
        compression::input_stream decompressed{source, {.format = format}};
        const auto count = decompressed.remaining_buffer();
        rohit::write_stream_bytes(output, decompressed.curr(), count);
        decompressed.get_curr_and_increase_unchecked(count);
        decompressed.finish();
      }
      output.close();
      return 0;
    }
    if (mode == "encode") {
      result = compression::compress(bytes, options);
    } else if (mode == "decode") {
      result = compression::decompress(bytes, {.format = format});
    } else {
      throw std::invalid_argument{"Unknown operation"};
    }
    std::ofstream output{argv[4], std::ios::binary};
    output.exceptions(std::ios::failbit | std::ios::badbit);
    rohit::write_stream_bytes(output, result.data(), result.size());
    output.close();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
