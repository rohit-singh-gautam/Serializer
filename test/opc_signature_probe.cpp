// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/opc_signatures.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
// Load a local qualification fixture without placing any signing material in diagnostics.
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) { throw std::runtime_error{"Cannot read signature fixture"}; }
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
// Publish a complete probe result only after the provider returns successfully.
void write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream output{path, std::ios::binary};
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (!output) { throw std::runtime_error{"Cannot write signature fixture"}; }
}
} // namespace

// Expose provider operations to an independent ZIP/.NET qualification driver.
int main(int count, char** arguments) {
  try {
    namespace signatures = rohit::serializer::package::signatures;
    if (count < 4) { throw std::runtime_error{"Expected operation, package and certificate/output"}; }
    const std::string operation{arguments[1]};
    const auto bytes = read(arguments[2]);
    signatures::coverage required{{"/model.bin"}, {"/"}};
    if (operation == "sign") {
      if (count != 5) { throw std::runtime_error{"Expected output path"}; }
      const auto certificate = read(arguments[3]);
      const auto result = signatures::sign(bytes, certificate, L"serializer-test", required);
      write(arguments[4], result.bytes);
      write(std::string{arguments[4]} + ".cer", result.certificate_der);
    } else if (operation == "verify" || operation == "verify-part") {
      const auto certificate = read(arguments[3]);
      if (operation == "verify-part") { required.relationship_sources.clear(); }
      const auto verified = signatures::verify(bytes, certificate, required);
      std::cout << "verified_signatures=" << verified.size() << '\n';
    } else if (operation == "remove") { write(arguments[3], signatures::remove(bytes)); }
    else { throw std::runtime_error{"Unknown signature operation"}; }
    return 0;
  } catch (const rohit::serializer::package::error& failure) {
    std::cerr << "package_error=" << static_cast<unsigned>(failure.code) << " " << failure.what() << '\n';
    return 1;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
