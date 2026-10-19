// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/package.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
namespace package = rohit::serializer::package;
constexpr std::array<std::uint8_t, 4> attachment_bytes{0U, 1U, 254U, 255U};

// Check the complete owning aggregate after the generated loader validates package integrity.
void verify(const opc_example::document::value& value) {
  if (value.main.title != "Parts & tools" || value.main.items.size() != 2U ||
      value.main.items[0].id != 7U || value.main.items[0].label != "Bolt <M6>" ||
      value.main.items[1].id != 8U || value.main.items[1].label != "Nut" ||
      value.attachments.size() != 1U ||
      value.attachments.front().metadata.name != "/Attachments/sample.bin" ||
      value.attachments.front().bytes.size() != attachment_bytes.size() ||
      !std::equal(attachment_bytes.begin(), attachment_bytes.end(),
          value.attachments.front().bytes.begin())) {
    throw std::runtime_error{"OPC example payload mismatch"};
  }
}
} // namespace

// Save a typed catalog and attachment, or read an independently produced matching package.
int main(int argc, char** argv) {
  try {
    if (argc < 2 || argc > 3 || (argc == 3 && std::string_view{argv[1]} != "--read")) {
      throw std::invalid_argument{"Usage: serializer_opc_example [--read] FILE.opc"};
    }
    const bool read_only = argc == 3;
    const std::filesystem::path path{argv[read_only ? 2 : 1]};
    if (!read_only) {
      // Fill document.main: this catalog becomes /Catalog/catalog.xml, with its
      // title attribute and the repeated <item> records declared in the schema.
      opc_example::catalog model{};
      model.title = "Parts & tools";
      model.items = {{7U, "Bolt <M6>"}, {8U, "Nut"}};
      // Fill document.attachments: these are the opaque bytes of
      // /Attachments/sample.bin. Source index 0 links it to the first main part.
      const std::array attachments{package::binary_part_view{
          attachment_bytes, "/Attachments/sample.bin", "application/octet-stream", 0U}};
      // Assemble both roles. The generated facade supplies [Content_Types].xml,
      // /_rels/.rels and /Catalog/_rels/catalog.xml.rels, then stages the complete
      // file before replacing the destination. Model/byte views borrow this scope.
      opc_example::document::save(path, {&model, attachments});
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) {
      throw std::runtime_error{"Cannot open OPC example input"};
    }
    // Discover the two roles from OPC relationships and decode the main XML part
    // into an owning catalog; attachment bytes remain opaque to Serializer.
    const auto loaded = opc_example::document::load(input);
    verify(loaded.value);
    std::cout << "Validated catalog and attachment: " << path.string() << '\n';
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
