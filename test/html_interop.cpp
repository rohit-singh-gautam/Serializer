// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/html.hpp>

#include <array>
#include <cstdio>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

// Canonicalize one independently supplied HTML fixture for the standard-library parser oracle.
int main() {
  try {
#ifdef _WIN32
    // Preserve exact UTF-8 bytes and canonical newlines through the Windows fixture transport.
    if (_setmode(_fileno(stdin), _O_BINARY) < 0 || _setmode(_fileno(stdout), _O_BINARY) < 0) {
      throw std::runtime_error{"Cannot select binary HTML oracle transport"};
    }
#endif
    const rohit::serializer::html::limits bounds{};
    std::array<char, 4096U> buffer{};
    std::string bytes{};
    while (std::cin) {
      std::cin.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const auto count = static_cast<std::size_t>(std::cin.gcount());
      if (count > bounds.max_input_bytes - bytes.size()) {
        throw std::length_error{"HTML oracle input bound exceeded"};
      }
      bytes.append(buffer.data(), count);
    }
    if (!std::cin.eof()) {
      throw std::runtime_error{"HTML oracle input read failed"};
    }
    std::cout << rohit::serializer::html::write_fragment(
        rohit::serializer::html::parse_fragment(bytes));
    return std::cout ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
