// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "package_relocation.hpp"

#include <rohit/package.hpp>
#include <zlib.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
namespace package = rohit::serializer::package;
constexpr int permutation_count = 6;
constexpr unsigned int hex_byte_max = std::numeric_limits<std::uint8_t>::max();

// Read headerless UTF-8 fixture rows while preserving an empty final payload cell.
std::vector<std::vector<std::string>> read_rows(const std::filesystem::path& path, std::size_t columns) {
  std::ifstream input{path, std::ios::binary};
  if (!input) { throw std::runtime_error{"Cannot open fixture rows"}; }
  std::vector<std::vector<std::string>> rows;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.find('\r') != std::string::npos) {
      throw std::runtime_error{"Invalid fixture row framing"};
    }
    std::vector<std::string> row;
    std::size_t begin = 0;
    for (;;) {
      const auto end = line.find('\t', begin);
      row.push_back(line.substr(begin, end == std::string::npos ? end : end - begin));
      if (end == std::string::npos) { break; }
      begin = end + 1;
    }
    if (row.size() != columns) { throw std::runtime_error{"Invalid fixture column count"}; }
    rows.push_back(std::move(row));
  }
  if (!input.eof()) { throw std::runtime_error{"Cannot complete fixture read"}; }
  return rows;
}

// Decode literal bytes independently of Serializer's native model codecs.
std::vector<std::uint8_t> decode_hex(std::string_view text) {
  if (text.size() % 2 != 0) { throw std::runtime_error{"Odd fixture hex length"}; }
  std::vector<std::uint8_t> bytes;
  bytes.reserve(text.size() / 2);
  for (std::size_t index = 0; index < text.size(); index += 2) {
    unsigned int value{};
    const auto parsed = std::from_chars(text.data() + index, text.data() + index + 2, value, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + index + 2 || value > hex_byte_max) {
      throw std::runtime_error{"Invalid fixture hex byte"};
    }
    bytes.push_back(static_cast<std::uint8_t>(value));
  }
  return bytes;
}

// Reorder only the caller's unordered sets using the independently specified permutation table.
template<typename T>
void permute(std::vector<T>& values, int selected) {
  std::vector<std::size_t> indexes(values.size());
  std::iota(indexes.begin(), indexes.end(), std::size_t{});
  if (selected == 1 || selected == 5) { std::reverse(indexes.begin(), indexes.end()); }
  if ((selected == 2 || selected == 3) && !indexes.empty()) {
    const auto shift = selected == 2 ? std::size_t{1} : indexes.size() / 2;
    std::rotate(indexes.begin(), indexes.begin() + static_cast<std::ptrdiff_t>(shift), indexes.end());
  }
  if (selected == 4 || selected == 5) {
    std::stable_partition(indexes.begin(), indexes.end(), [selected](std::size_t index) {
      return index % 2 == (selected == 5 ? 1u : 0u);
    });
  }
  std::vector<T> reordered;
  reordered.reserve(values.size());
  for (const auto index : indexes) { reordered.push_back(std::move(values[index])); }
  values = std::move(reordered);
}

// Report the actual linked compressor and explicit existing writer defaults.
void provider() {
  std::cout << "{\"implementation\":\"zlib\",\"version\":\"" << zlibVersion()
            << "\",\"options\":{\"level\":6,\"window_bits\":-15,\"memory_level\":8,\"strategy\":0},\"metadata_method\":0}";
}
} // namespace

// Produce one complete archive through public canonical writers for an independent raw ZIP oracle.
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string_view{argv[1]} == "--provider") { provider(); std::cout << '\n'; return 0; }
    if (argc != 5 && argc != 6) { throw std::runtime_error{"Expected fixtures, profile, permutation, output and optional ZIP64 flag"}; }
    const std::string_view profile{argv[2]};
    if (profile != "zip-stored" && profile != "zip-deflate" && profile != "opc-stored" && profile != "opc-deflate") {
      throw std::runtime_error{"Unknown reproducibility profile"};
    }
    int selected{};
    const std::string_view selector{argv[3]};
    const auto parsed = std::from_chars(selector.data(), selector.data() + selector.size(), selected);
    if (parsed.ec != std::errc{} || parsed.ptr != selector.data() + selector.size() || selected < 0 || selected >= permutation_count) {
      throw std::runtime_error{"Invalid permutation selector"};
    }
    const std::string_view wide = argc == 6 ? std::string_view{argv[5]} : "0";
    if (wide != "0" && wide != "1") { throw std::runtime_error{"Invalid ZIP64 flag"}; }
    const bool opc = profile.starts_with("opc-");
    const auto fixture = std::filesystem::path{argv[1]} / (opc ? "opc" : "zip");
    std::vector<package::opaque_part> parts;
    for (const auto& row : read_rows(fixture / "parts.tsv", 4)) {
      auto bytes = decode_hex(row[3]);
      package::entry_options options;
      options.method = profile.ends_with("deflate") ? package::zip_method::deflate : package::zip_method::stored;
      options.force_zip64 = wide == "1";
      parts.push_back({{row[0], row[1], bytes.size()}, std::move(bytes), options});
    }
    std::vector<package::relationship> links;
    for (const auto& row : read_rows(fixture / "links.tsv", 5)) {
      if (row[4] != "internal" && row[4] != "external") { throw std::runtime_error{"Invalid relationship mode"}; }
      links.push_back({row[0], row[1], row[2], row[3], row[4] == "external" ? package::target_mode::external : package::target_mode::internal});
    }
    permute(parts, selected); permute(links, selected);
    std::ofstream output{std::filesystem::path{argv[4]}, std::ios::binary};
    output.exceptions(std::ios::failbit | std::ios::badbit);
    if (opc) { package::write_canonical_opc(output, parts, links); }
    else { package::write_canonical_zip(output, parts); }
    output.close();
    std::cout << "{\"profile\":\"" << profile << "\",\"permutation\":" << selected
              << ",\"force_zip64\":" << (wide == "1" ? "true" : "false")
              << ",\"parts\":" << parts.size() << ",\"relationships\":" << links.size() << ",\"provider\":";
    provider(); std::cout << "}\n";
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
