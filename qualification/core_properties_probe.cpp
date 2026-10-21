// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/core_properties.hpp>

#include <exception>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#endif

namespace package = rohit::serializer::package;

// Exercise all fifth-edition standard properties, including localized ordered mixed keywords.
package::core_properties sample_properties() {
  package::core_properties value{};
  value.category = "Specification";
  value.content_status = "Reviewed";
  value.created = "2005-06-12";
  value.creator = "Application author";
  value.description = "Document & metadata";
  value.identifier = "urn:serializer:core-properties:oracle";
  value.language = "eng";
  value.last_modified_by = "Application reviewer";
  value.last_printed = "2017-04-17T14:20:10+09:00";
  value.modified = "2005-11-23T12:00:00Z";
  value.revision = "4";
  value.subject = "";
  value.title = "OPC Core Properties";
  value.version = "1.0";
  value.keywords = package::core_property_keywords{"en-US", {
      std::string{"color "}, package::core_property_keyword{"colour", "en-CA"},
      std::string{" / "}, package::core_property_keyword{"couleur", "fr-FR"}, std::string{" tail"}}};
  return value;
}

// Expose a small native test boundary for independent XML Schema and OPC producer/consumer oracles.
int main(int argc, const char* const* argv) {
  try {
#ifdef _WIN32
    // OPC archives cross this pipe as bytes; CRT newline translation would corrupt ZIP offsets.
    if (_setmode(_fileno(stdin), _O_BINARY) < 0 || _setmode(_fileno(stdout), _O_BINARY) < 0) {
      throw std::runtime_error{"Cannot select binary standard streams"};
    }
#endif
    if (argc != 2) { throw std::invalid_argument{"Expected model, roundtrip, pack or unpack"}; }
    const std::string_view selected{argv[1]};
    package::budget shared{};
    if (selected == "model") {
      std::cout << package::encode_core_properties(sample_properties(), shared);
      return std::cout ? 0 : 1;
    }
    const std::string bytes{std::istreambuf_iterator<char>{std::cin}, std::istreambuf_iterator<char>{}};
    if (selected == "roundtrip") {
      const auto value = package::decode_core_properties(bytes, shared);
      std::cout << package::encode_core_properties(value, shared);
    } else if (selected == "pack") {
      const auto value = package::decode_core_properties(bytes, shared);
      package::opc_writer writer{std::cout};
      package::write_core_properties(writer, value);
      writer.finish();
    } else if (selected == "unpack") {
      std::istringstream input{bytes, std::ios::binary};
      package::opc_reader reader{input};
      package::profile_loader loader{reader};
      const auto value = package::read_core_properties(loader);
      static_cast<void>(loader.finish());
      if (!value) { throw std::invalid_argument{"Core properties are absent"}; }
      std::cout << package::encode_core_properties(*value, *reader.shared_budget());
    } else { throw std::invalid_argument{"Unknown probe operation"}; }
    return std::cout ? 0 : 1;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 2;
  }
}
