// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "owning_variants_samples.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;
namespace samples = owning_variants_samples;

// Save actual generated native output for independent portable-codec interoperability checks.
template <template <codec::serialize_type> class Protocol, class Value>
void write_fixture(const std::filesystem::path& directory, std::string_view name,
                   std::string_view protocol, const Value& value) {
  const auto bytes = samples::encode<Protocol>(value);
  const auto path = directory / (std::string{name} + "-" + std::string{protocol} + ".wire");
  std::ofstream output{path, std::ios::binary | std::ios::trunc};
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!output.good()) { throw std::runtime_error{"Unable to write owning variant fixture"}; }
}

// Cover all owning arms and primitive raw/owning parity through one native protocol.
template <template <codec::serialize_type> class Protocol>
void write_protocol(const std::filesystem::path& directory, std::string_view protocol) {
  for (std::size_t index{}; index < std::variant_size_v<owning_variants::record::u_payload>; ++index) {
    write_fixture<Protocol>(directory, "record-alt" + std::to_string(index), protocol, samples::sample(index));
  }
  using owned = owning_variants::primitive_record;
  owning_variants::raw_record raw{};
  owned variant{};
  raw.payload.code = 42U;
  variant.emplace_payload<owned::e_payload::code>(42U);
  write_fixture<Protocol>(directory, "raw-alt0", protocol, raw);
  write_fixture<Protocol>(directory, "primitive-alt0", protocol, variant);
  raw.payload_type = owning_variants::raw_record::e_payload::measure;
  std::construct_at(&raw.payload.measure, 1.25);
  variant.emplace_payload<owned::e_payload::measure>(1.25);
  write_fixture<Protocol>(directory, "raw-alt1", protocol, raw);
  write_fixture<Protocol>(directory, "primitive-alt1", protocol, variant);
}
}

// Produce actual native fixtures in a caller-provided build directory; errors are reported to stderr.
int main(int argc, char** argv) {
  if (argc != 2) { return 2; }
  try {
    const std::filesystem::path directory{argv[1]};
    std::filesystem::create_directories(directory);
    write_protocol<codec::binary_none>(directory, "none");
    write_protocol<codec::binary_integer>(directory, "integer");
    write_protocol<codec::binary_string>(directory, "string");
    write_protocol<codec::json>(directory, "json");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
