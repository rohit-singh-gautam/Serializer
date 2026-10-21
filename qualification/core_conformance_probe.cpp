// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <core.hpp>
#include <rohit/stream_io.hpp>
#include <rohit/xml.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using model = core_model::xsd_c_t_model_139298458;

// Compare all native persistent fields, including geometry and resource identities, after XML round trips.
std::vector<std::uint8_t> payload(const model& value, std::size_t maximum_bytes) {
  const rohit::stream_limits bounds{0U, maximum_bytes};
  rohit::full_stream_auto_alloc_limits output{&bounds};
  rohit::serializer::serialize_to<rohit::serializer::binary_none>(output, value);
  return {output.begin(), output.begin() + output.current_offset()};
}
} // namespace

// Decode and re-encode actual imported Core graphs under an explicit bounded common policy.
int main(int argument_count, char** arguments) {
  if (argument_count != 3) { return 2; }
  namespace xml = rohit::serializer::xml;
  constexpr std::size_t maximum_xml_bytes{16U * 1024U * 1024U};
  constexpr std::size_t maximum_entries{1000000U};
  constexpr std::size_t maximum_depth{64U};
  xml::limits bounds{};
  bounds.max_input_bytes = bounds.max_output_bytes = bounds.max_text_bytes = maximum_xml_bytes;
  bounds.max_nodes = maximum_entries;
  bounds.max_attributes = maximum_entries;
  bounds.max_depth = maximum_depth;
  std::ifstream listing{arguments[1]};
  if (!listing) { return 2; }
  const std::filesystem::path output_directory{arguments[2]};
  std::filesystem::create_directories(output_directory);
  for (std::string filename; std::getline(listing, filename);) {
    const std::filesystem::path path{filename};
    try {
      std::ifstream input{path, std::ios::binary};
      if (!input) { throw std::runtime_error{"Cannot open model input"}; }
      const auto bytes = rohit::read_stream_bytes(input, bounds.max_input_bytes);
      auto value = xml::deserialize<model>(std::string_view{
          reinterpret_cast<const char*>(bytes.begin()), bytes.current_offset()}, bounds);
      if (!value.xsd_metadata.empty()) {
        value.xsd_metadata.front().xsd_value += " [Serializer qualification]";
      }
      const auto encoded = xml::serialize(value, bounds);
      const auto copy = xml::deserialize<model>(encoded, bounds);
      if (payload(value, bounds.max_output_bytes) != payload(copy, bounds.max_output_bytes)) {
        throw std::runtime_error{"Native Core fields changed during XML round trip"};
      }
      std::ofstream output{output_directory / path.filename(), std::ios::binary};
      output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
      if (!output) { throw std::runtime_error{"Cannot write model output"}; }
      std::cout << path.filename().string() << "\taccepted\n";
    } catch (const std::exception& error) {
      std::string diagnostic{error.what()};
      for (auto& character : diagnostic) {
        if (character == '\r' || character == '\n' || character == '\t') { character = ' '; }
      }
      std::cout << path.filename().string() << "\trejected\t" << diagnostic << '\n';
    }
  }
  return listing.bad() ? 2 : 0;
}
