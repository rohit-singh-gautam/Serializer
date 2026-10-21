// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>

#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

// Exercise real bounded readers/writers using binary files independent of terminal encodings.
int main(int argc, const char* const* argv) {
  try {
    namespace package = rohit::serializer::package;
    if (argc < 3) { throw std::invalid_argument{"Expected validate, opc-validate, rewrite or edit"}; }
    std::ifstream input{argv[2], std::ios::binary};
    input.exceptions(std::ios::badbit);
    if (!input) { throw std::runtime_error{"Cannot open input"}; }
    const std::string_view mode{argv[1]};
    if (mode == "opc-validate") {
      package::opc_reader reader{input};
      reader.validate_all();
      return 0;
    }
    auto shared = std::make_shared<package::budget>();
    package::zip_reader reader{input, shared};
    if (mode == "validate") { reader.validate_all(); return 0; }
    if ((mode != "rewrite" && mode != "edit") || argc != 4) {
      throw std::invalid_argument{"Invalid rewrite arguments"};
    }
    std::ofstream output{argv[3], std::ios::binary};
    output.exceptions(std::ios::badbit | std::ios::failbit);
    package::zip_writer writer{output, shared};
    writer.set_archive_metadata(reader.archive_metadata());
    for (const auto& entry : reader.entries()) {
      auto bytes = reader.read(entry.name);
      if (mode == "edit") { bytes.push_back('!'); }
      package::entry_options options{};
      options.method = entry.method;
      options.force_zip64 = entry.zip64_sizes;
      const auto metadata = reader.entry_metadata(entry.name);
      options.metadata = metadata.get();
      writer.add(entry.name, bytes, options);
    }
    writer.finish();
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 2;
  }
}
