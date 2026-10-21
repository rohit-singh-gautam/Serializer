// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "order.hpp"

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>
#include <rohit/xml.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;
using order = evolution::order;

// Stop before publishing an output when a public model contract differs.
void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error{std::string{message}};
  }
}

// Verify defaults and the readonly native operation independently of XML binding.
void check_defaults(const order& value) {
  require(value.id == 7U && value.customer == "native default" && !value.note &&
      value.has_id(7U) && !value.has_id(8U), "Native defaults or readonly operation changed");
}

// Require the complete authored state after each native or XML decode.
void check_authored(const order& value) {
  require(value.id == 7U && value.customer == "R&D" && value.note && *value.note == "memo" &&
      value.has_id(7U) && !value.has_id(8U), "Authored model or readonly operation changed");
}

// Read one complete fixture, retaining ordinary file errors as operation failures.
std::string read_file(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  require(static_cast<bool>(input), "Cannot open fixture");
  const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
  require(!input.bad(), "Cannot read fixture");
  return bytes;
}

// Publish validated bytes only after all model and codec checks have succeeded.
void write_file(const std::filesystem::path& path, const std::string& bytes) {
  std::ofstream output{path, std::ios::binary};
  require(static_cast<bool>(output), "Cannot open output");
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  require(static_cast<bool>(output), "Cannot write output");
}

// Serialize to owned memory before creating a caller-visible file.
template <template <codec::serialize_type> typename Protocol>
std::string encode(const order& value) {
  rohit::full_stream_auto_alloc output{};
  value.serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Exercise the ordinary default/native/XML authoring path for one compiled variant.
void emit(const std::filesystem::path& directory) {
  const order defaults{};
  check_defaults(defaults);
  order authored{};
  authored.customer = "R&D";
  authored.note = "memo";
  check_authored(authored);
  const auto xml = codec::xml::serialize(authored);
  check_authored(codec::xml::deserialize<order>(xml));
  std::filesystem::create_directories(directory);
  write_file(directory / "authored.xml", xml);
  write_file(directory / "authored.bin", encode<codec::binary_none>(authored));
  write_file(directory / "authored-integer.bin", encode<codec::binary_integer>(authored));
  write_file(directory / "authored-string.bin", encode<codec::binary_string>(authored));
  write_file(directory / "default.bin", encode<codec::binary_none>(defaults));
  write_file(directory / "xml-projection.bin", encode<codec::binary_none>(authored));
}
} // namespace

// Consume independently produced fixtures through the actual generated C++ APIs.
int main(int argc, char** argv) {
  try {
    require(argc >= 4, "Usage: consumer VARIANT emit DIRECTORY | check-xml/check-native INPUT OUTPUT");
    const std::string_view variant{argv[1]};
    require(variant == "v1" || variant == "v2", "Unknown variant");
    const std::string_view mode{argv[2]};
    if (mode == "emit") {
      require(argc == 4, "Unexpected emit arguments");
      emit(argv[3]);
    } else {
      require(argc == 5, "Unexpected check arguments");
      const auto bytes = read_file(argv[3]);
      if (mode == "check-xml") {
        const auto value = codec::xml::deserialize<order>(bytes);
        check_authored(value);
        write_file(argv[4], encode<codec::binary_none>(value));
      } else if (mode == "check-native") {
        const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
        const auto value = codec::deserialize_exact<order, codec::binary_none>(input);
        check_authored(value);
        write_file(argv[4], codec::xml::serialize(value));
      } else {
        throw std::invalid_argument{"Unknown mode"};
      }
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
