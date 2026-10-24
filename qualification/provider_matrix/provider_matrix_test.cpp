// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "provider_expectations.hpp"

#include <rohit/compression.hpp>
#include <rohit/xml.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>

namespace {
namespace compression = rohit::serializer::compression;
namespace xml = rohit::serializer::xml;

struct provider_case {
  const char* name;
  compression::format format;
  bool expected;
  compression::encode_options options;
};

// Count semantic decisions and fail immediately without hiding a provider mismatch.
void require(bool accepted, const char* message, std::size_t& checks) {
  if (!accepted) { throw std::runtime_error{message}; }
  ++checks;
}

// Require a stable compression error category, including availability before frame inspection.
template <typename Action>
void reject(Action action, compression::error_code expected, std::size_t& checks) {
  try {
    action();
  } catch (const compression::error& error) {
    require(error.code == expected, "Unexpected compression failure category", checks);
    return;
  }
  throw std::runtime_error{"A required compression rejection succeeded"};
}

// Emit exact encoded bytes for deterministic comparisons across provider combinations.
void print_hex(std::span<const std::uint8_t> bytes) {
  std::cout << std::hex << std::setfill('0');
  for (const auto value : bytes) { std::cout << std::setw(2) << static_cast<unsigned>(value); }
  std::cout << std::dec;
}
}

// Exercise the actual linked runtime for one independently configured provider combination.
int main() {
  try {
    // Static storage avoids older MSVC constant-array capture errors in span conversions.
    static constexpr std::uint8_t input[]{0U, 1U, 2U, 127U, 128U, 255U, 0U, 42U};
    const std::array cases{
      provider_case{"none", compression::format::none, true, compression::none_options{}},
      provider_case{"zstd", compression::format::zstd, provider_matrix::has_zstd, compression::zstd_options{}},
      provider_case{"lz4", compression::format::lz4, provider_matrix::has_lz4, compression::lz4_options{}},
      provider_case{"gzip", compression::format::gzip, provider_matrix::has_zlib, compression::gzip_options{}},
      provider_case{"zlib", compression::format::zlib, provider_matrix::has_zlib, compression::zlib_options{}},
      provider_case{"deflate", compression::format::deflate, provider_matrix::has_zlib, compression::deflate_options{}}
    };
    std::size_t checks{};
    std::cout << "{\"formats\":[";
    bool first = true;
    for (const auto& item : cases) {
      if (!first) { std::cout << ','; }
      first = false;
      require(compression::available(item.format) == item.expected, "Compiled format availability differs", checks);
      std::cout << "{\"name\":\"" << item.name << "\",\"available\":"
                << (item.expected ? "true" : "false");
      compression::decode_options decode{};
      decode.format = item.format;
      if (item.expected) {
        compression::encode_limits limits{};
        limits.max_input_bytes = std::size(input);
        const auto encoded = compression::compress(input, item.options, limits);
        require(!encoded.empty(), "Nonempty input produced no encoded bytes", checks);
        decode.max_compressed_bytes = encoded.size();
        decode.max_decompressed_bytes = std::size(input);
        const auto decoded = compression::decompress(encoded, decode);
        require(std::ranges::equal(decoded, input), "Provider roundtrip changed ordinary bytes", checks);
        limits.max_input_bytes = std::size(input) - 1U;
        reject([&] { (void)compression::compress(input, item.options, limits); },
          compression::error_code::resource_limit, checks);
        limits.max_input_bytes = std::size(input);
        limits.max_output_bytes = encoded.size() - 1U;
        reject([&] { (void)compression::compress(input, item.options, limits); },
          compression::error_code::resource_limit, checks);
        decode.max_decompressed_bytes = std::size(input) - 1U;
        reject([&] { (void)compression::decompress(encoded, decode); },
          compression::error_code::resource_limit, checks);
        std::cout << ",\"encoded_hex\":\"";
        print_hex(encoded);
        std::cout << '"';
      } else {
        reject([&] { compression::validate(item.options); }, compression::error_code::unavailable, checks);
        reject([&] { compression::validate(decode); }, compression::error_code::unavailable, checks);
        reject([&] { (void)compression::compress(input, item.options); }, compression::error_code::unavailable, checks);
        reject([&] { (void)compression::decompress({}, decode); }, compression::error_code::unavailable, checks);
      }
      std::cout << '}';
    }
    require(xml::available() == provider_matrix::has_xml, "Compiled XML availability differs", checks);
    constexpr auto document = "<root xmlns=\"urn:provider-matrix\">matrix&amp;bytes</root>";
    if (provider_matrix::has_xml) {
      const auto parsed = xml::parse(document);
      require(parsed.qualified_name.local == "root" && parsed.qualified_name.namespace_uri == "urn:provider-matrix"
        && parsed.text == "matrix&bytes", "Independent XML values differ", checks);
      const auto reparsed = xml::parse(xml::write(parsed));
      require(reparsed.qualified_name == parsed.qualified_name && reparsed.text == parsed.text,
        "XML roundtrip changed the expanded value", checks);
    } else {
      bool rejected = false;
      try { (void)xml::parse(document); }
      catch (const std::runtime_error& error) {
        rejected = std::string{error.what()} == "XML support was not enabled in this Serializer runtime";
      }
      require(rejected, "Disabled XML did not retain its unavailability diagnostic", checks);
    }
    std::cout << "],\"xml\":" << (provider_matrix::has_xml ? "true" : "false")
              << ",\"checks\":" << checks << "}\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
