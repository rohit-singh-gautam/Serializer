// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/stream_io.hpp>
#include <rohit/xml.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Read the shared contract, exercise the generated codec and export one complete document.
int main(int argc, char** argv) {
  if (argc != 3) { throw std::invalid_argument{"Expected INPUT.xml OUTPUT.xml"}; }
  namespace xml = rohit::serializer::xml;
  std::ifstream input{argv[1], std::ios::binary};
  if (!input) { throw std::runtime_error{"Cannot open XML input"}; }
  const auto bytes = rohit::read_stream_bytes(input, xml::limits{}.max_input_bytes);
  const std::string_view document{reinterpret_cast<const char*>(bytes.begin()), bytes.current_offset()};
  const auto value = xml::deserialize<xsd_example::xsd_order_196281168>(document);
  if (value.xsd_id != 7U || value.xsd_customer != "R&D" || value.xsd_count != 3 ||
      value.xsd_tag != std::vector<std::string>{"A", "B"}) {
    throw std::runtime_error{"Shared XML fixture differs"};
  }
  const auto encoded = xml::serialize(value);
  const auto copy = xml::deserialize<xsd_example::xsd_order_196281168>(encoded);
  if (copy.xsd_customer != value.xsd_customer || copy.xsd_tag != value.xsd_tag) {
    throw std::runtime_error{"Generated XML round trip differs"};
  }
  std::ofstream output{argv[2], std::ios::binary | std::ios::trunc};
  output.exceptions(std::ios::badbit | std::ios::failbit);
  output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
  output.close();
  std::cout << "Generated C++ XML checks passed\n";
}
