// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "managed_token_codecs.hpp"

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
// Decode a complete Python-produced token and return C++'s independent encoding.
template <template <rohit::serializer::serialize_type> typename Protocol>
std::string exchange(const std::string& bytes) {
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  Protocol<rohit::serializer::serialize_type::in> reader{input};
  token_wire::envelope value{};
  reader.serialize_in(value);
  reader.finish();
  rohit::full_stream_auto_alloc output{};
  value.serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}
} // namespace

// Exchange one corpus entry using an explicitly selected native protocol.
int main(int argc, char** argv) {
  try {
    if (argc != 4) { throw std::invalid_argument{"Expected protocol, input and output paths"}; }
    std::ifstream input{argv[2], std::ios::binary};
    if (!input) { throw std::runtime_error{"Cannot read token fixture"}; }
    const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
    const std::string protocol{argv[1]};
    const auto result = protocol == "0" ? exchange<rohit::serializer::json>(bytes) :
        protocol == "1" ? exchange<rohit::serializer::binary_none>(bytes) :
        protocol == "2" ? exchange<rohit::serializer::binary_integer>(bytes) :
        protocol == "3" ? exchange<rohit::serializer::binary_string>(bytes) :
        throw std::invalid_argument{"Unknown token fixture protocol"};
    std::ofstream output{argv[3], std::ios::binary};
    output.write(result.data(), static_cast<std::streamsize>(result.size()));
    output.close();
    if (!output) { throw std::runtime_error{"Cannot write token fixture"}; }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
