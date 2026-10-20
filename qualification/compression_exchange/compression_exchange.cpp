// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/compression.hpp>
#include <rohit/stream_io.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace compression = codec::compression;
namespace fs = std::filesystem;
using bytes = std::vector<std::uint8_t>;
constexpr std::array protocols{"JSON", "BINARY_NONE", "BINARY_INTEGER", "BINARY_STRING"};
constexpr std::array formats{"NONE", "GZIP", "ZLIB", "DEFLATE"};

// Read only bounded fixture files and keep their storage owned throughout decoding.
bytes read_file(const fs::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error{"Cannot open compression fixture"};
  }
  const auto storage = rohit::read_stream_bytes(input, 8 * compression::mebibyte);
  return {storage.begin(), storage.begin() + storage.current_offset()};
}

// Publish a complete successful fixture result to its isolated qualification directory.
void write_file(const fs::path& path, const bytes& value) {
  std::ofstream output{path, std::ios::binary};
  output.exceptions(std::ios::badbit | std::ios::failbit);
  rohit::write_stream_bytes(output, value.data(), value.size());
  output.close();
}

// Select the same mandatory core provider profile in every language adapter.
std::pair<compression::format, compression::encode_options> selection(std::string_view name) {
  if (name == "NONE") {
    return {compression::format::none, compression::none_options{}};
  }
  if (name == "GZIP") {
    return {compression::format::gzip, compression::gzip_options{}};
  }
  if (name == "ZLIB") {
    return {compression::format::zlib, compression::zlib_options{}};
  }
  if (name == "DEFLATE") {
    return {compression::format::deflate, compression::deflate_options{}};
  }
  throw std::invalid_argument{"Unknown shared compression format"};
}

// Encode a real owning generated object through an existing selected native codec.
template <template <codec::serialize_type> class Protocol>
bytes encode_object(const exchange_record& value) {
  rohit::full_stream_auto_alloc output;
  codec::serialize_to<Protocol>(output, value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Require exact native EOF before returning an independently owned generated value.
template <template <codec::serialize_type> class Protocol>
exchange_record decode_object(const bytes& value) {
  const auto input = rohit::make_constant_full_stream(value.data(), value.size());
  return codec::deserialize_exact<exchange_record, Protocol>(input);
}

// Dispatch stable protocol spellings without changing their native wire definitions.
bytes encode_object(const exchange_record& value, std::string_view protocol) {
  if (protocol == "JSON") {
    return encode_object<codec::json>(value);
  }
  if (protocol == "BINARY_NONE") {
    return encode_object<codec::binary_none>(value);
  }
  if (protocol == "BINARY_INTEGER") {
    return encode_object<codec::binary_integer>(value);
  }
  if (protocol == "BINARY_STRING") {
    return encode_object<codec::binary_string>(value);
  }
  throw std::invalid_argument{"Unknown shared native protocol"};
}

// Decode the selected native representation with the same complete-object boundary.
exchange_record decode_object(const bytes& value, std::string_view protocol) {
  if (protocol == "JSON") {
    return decode_object<codec::json>(value);
  }
  if (protocol == "BINARY_NONE") {
    return decode_object<codec::binary_none>(value);
  }
  if (protocol == "BINARY_INTEGER") {
    return decode_object<codec::binary_integer>(value);
  }
  if (protocol == "BINARY_STRING") {
    return decode_object<codec::binary_string>(value);
  }
  throw std::invalid_argument{"Unknown shared native protocol"};
}

// Split the runner's bounded tab-separated jobs while preserving every required column.
std::vector<std::string> columns(const std::string& line) {
  std::vector<std::string> result;
  std::size_t begin{};
  while (true) {
    const auto end = line.find('\t', begin);
    result.push_back(line.substr(begin, end == std::string::npos ? end : end - begin));
    if (end == std::string::npos) {
      return result;
    }
    begin = end + 1;
  }
}

// Normalize stable compression categories for the common language-independent oracle.
std::string_view category(compression::error_code code) {
  switch (code) {
  case compression::error_code::unavailable:
    return "unavailable";
  case compression::error_code::invalid_options:
    return "invalid_options";
  case compression::error_code::invalid_data:
    return "invalid_data";
  case compression::error_code::resource_limit:
    return "resource_limit";
  }
  throw std::runtime_error{"Unknown compression error category"};
}

// Replay one identical byte/framing/limit fixture, returning only its structured decision.
std::string replay(const std::vector<std::string>& row, const fs::path& output) {
  if (row.size() != 9) {
    throw std::runtime_error{"Incomplete common corpus row"};
  }
  const auto [format, encoding] = selection(row[2]);
  const auto data = read_file(row[3]);
  try {
    if (row[1] == "encode_bounds") {
      const auto packed = compression::compress(data, encoding);
      if (packed.empty()) {
        throw std::runtime_error{"Empty encoding-boundary fixture"};
      }
      const compression::encode_limits exact{data.size(), packed.size()};
      if (compression::compress(data, encoding, exact) != packed) {
        throw std::runtime_error{"Exact output admission changed the frame"};
      }
      bool rejected{};
      try {
        static_cast<void>(compression::compress(data, encoding, {data.size(), packed.size() - 1}));
      } catch (const compression::error& failure) {
        if (failure.code != compression::error_code::resource_limit) {
          throw;
        }
        rejected = true;
      }
      if (!rejected) {
        throw std::runtime_error{"One-tight encoding output bound was accepted"};
      }
      write_file(output / (row[0] + ".bin"), packed);
    } else if (row[1] == "encode") {
      static_cast<void>(compression::compress(data, encoding,
                                              {static_cast<std::size_t>(std::stoull(row[6])),
                                               static_cast<std::size_t>(std::stoull(row[7]))}));
    } else {
      const compression::decode_options options{
          .format = format,
          .max_compressed_bytes = static_cast<std::size_t>(std::stoull(row[6])),
          .max_decompressed_bytes = static_cast<std::size_t>(std::stoull(row[7])),
          .max_window_bytes = static_cast<std::size_t>(std::stoull(row[8]))};
      const auto plain = compression::decompress(data, options);
      if (row[1] == "inner") {
        static_cast<void>(decode_object(plain, row[4]));
      } else if (plain != read_file(row[4])) {
        throw std::runtime_error{"Independent compression fixture differs"};
      }
    }
  } catch (const compression::error& failure) {
    return std::string{category(failure.code)};
  } catch (const std::exception&) {
    if (row[1] == "inner") {
      return "native_error";
    }
    throw;
  }
  return "ok";
}
} // namespace

// Produce or consume every mandatory job; SDK/provider absence and unknown cases are hard failures.
int main(int argc, char** argv) {
  try {
    if (argc != 4 && argc != 6) {
      throw std::invalid_argument{"Expected qualification mode and paths"};
    }
    const std::string_view mode{argv[1]};
    const auto expected = decode_object<codec::json>(read_file(argv[2]));
    for (const auto name : formats) {
      if (!compression::available(selection(name).first)) {
        throw std::runtime_error{"A required C++ compression provider is unavailable"};
      }
    }
    if (mode == "produce" && argc == 4) {
      for (const auto protocol : protocols) {
        for (const auto name : formats) {
          const auto filename = std::string{"cpp-"} + protocol + "-" + name + ".bin";
          write_file(
              fs::path{argv[3]} / filename,
              compression::compress(encode_object(expected, protocol), selection(name).second));
        }
      }
      std::cout << "{\"produced\":" << protocols.size() * formats.size() << "}\n";
      return 0;
    }
    if (mode != "check" || argc != 6) {
      throw std::invalid_argument{"Unknown qualification mode"};
    }
    std::ifstream jobs{argv[3]}, corpus{argv[4]};
    if (!jobs || !corpus) {
      throw std::runtime_error{"Cannot open mandatory shared jobs"};
    }
    const fs::path output{argv[5]};
    fs::create_directories(output);
    std::size_t exchanges{}, decisions{};
    std::string line;
    while (std::getline(jobs, line)) {
      const auto row = columns(line);
      if (row.size() != 3) {
        throw std::runtime_error{"Incomplete model exchange row"};
      }
      const auto plain =
          compression::decompress(read_file(row[2]), {.format = selection(row[1]).first});
      const auto actual = decode_object(plain, row[0]);
      if (actual.code != expected.code || actual.text != expected.text) {
        throw std::runtime_error{"Shared generated model differs"};
      }
      ++exchanges;
    }
    while (std::getline(corpus, line)) {
      const auto row = columns(line);
      const auto observed = replay(row, output);
      if (observed != row[5]) {
        throw std::runtime_error{row[0] + ": expected " + row[5] + ", observed " + observed};
      }
      ++decisions;
    }
    std::cout << "{\"model_exchanges\":" << exchanges << ",\"corpus_decisions\":" << decisions
              << "}\n";
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
