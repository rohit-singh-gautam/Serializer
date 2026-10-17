// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>
#include <rohit/xml.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
namespace package = rohit::serializer::package;

// Own literal payload bytes for the canonical writer's borrowed part list.
std::vector<std::uint8_t> bytes(std::string_view value) {
  return {value.begin(), value.end()};
}

// Finalize one canonical archive and require the native output file to close successfully.
void write_zip(const std::filesystem::path& path, std::span<const package::opaque_part> parts) {
  std::ofstream output{path, std::ios::binary};
  output.exceptions(std::ios::failbit | std::ios::badbit);
  package::write_canonical_zip(output, parts);
  output.close();
}

// Emit equivalent OPC archives with explicit relationship discovery for the independent reader.
void write_opc(const std::filesystem::path& path, std::span<const package::opaque_part> parts,
               std::span<const package::relationship> links) {
  std::ofstream output{path, std::ios::binary};
  output.exceptions(std::ios::failbit | std::ios::badbit);
  package::write_canonical_opc(output, parts, links);
  output.close();
}
} // namespace

// Produce complete native archives for independent standard-reader and exact-byte qualification.
int main(int argc, char** argv) {
  try {
    if (argc != 2) { throw std::invalid_argument{"Expected a private artifact directory"}; }
    const std::filesystem::path directory{argv[1]};
    std::filesystem::create_directories(directory);
    std::vector<package::opaque_part> parts{
        {{"z.bin", "application/octet-stream", 3}, bytes(std::string_view{"a\0b", 3}), {}},
        {{"a.json", "application/json", 11}, bytes("{\"count\":7}"), {}}};
    write_zip(directory / "canonical-first.zip", parts);
    std::reverse(parts.begin(), parts.end());
    write_zip(directory / "canonical-second.zip", parts);
    parts.front().storage.force_zip64 = true;
    write_zip(directory / "forced-zip64.zip", parts);
    parts.front().storage.force_zip64 = false;
    if (rohit::serializer::xml::available()) {
      for (auto& item : parts) { item.metadata.name.insert(0, "/"); }
      std::vector<package::relationship> links{
          {"/", "main", "urn:test:main", "a.json"},
          {"/a.json", "attachment", "urn:test:attachment", "z.bin"}};
      write_opc(directory / "canonical-first.opc", parts, links);
      std::reverse(parts.begin(), parts.end());
      std::reverse(links.begin(), links.end());
      write_opc(directory / "canonical-second.opc", parts, links);
    }
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
