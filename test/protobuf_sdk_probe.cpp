// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "protobuf_arrays.hpp"
#include <rohit/protobuf.hpp>

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;
constexpr auto sdk_values = std::to_array<std::uint32_t>({0, 150, 300});

// Exchange actual generated fixed arrays with the independently installed Protobuf SDK.
template <template <codec::serialize_type> class Protocol>
void exchange(std::string_view operation, const char* path) {
  if (operation == "encode") {
    protobuf_arrays::scalar_array value{};
    value.values = sdk_values;
    const auto bytes = codec::serialize_as<std::string, Protocol>(value);
    std::ofstream output{path, std::ios::binary};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) { throw std::runtime_error{"Cannot publish Protobuf SDK fixture"}; }
  } else {
    std::ifstream input{path, std::ios::binary};
    if (!input) { throw std::runtime_error{"Cannot read Protobuf SDK fixture"}; }
    const std::string bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto value = codec::deserialize_exact<protobuf_arrays::scalar_array, Protocol>(
        rohit::make_constant_stream(bytes.data(), bytes.size()));
    if (value.values != sdk_values) { throw std::runtime_error{"Protobuf SDK values changed"}; }
  }
}
} // namespace

// Keep each external producer/consumer run independently observable by the qualification driver.
int main(int count, char** arguments) {
  try {
    if (count != 4) { throw std::runtime_error{"Expected operation, protocol and fixture"}; }
    const std::string_view protocol{arguments[2]};
    if (protocol == "binary") { exchange<codec::protobuf_binary>(arguments[1], arguments[3]); }
    else if (protocol == "json") { exchange<codec::protojson>(arguments[1], arguments[3]); }
    else if (protocol == "text") { exchange<codec::textproto>(arguments[1], arguments[3]); }
    else { throw std::runtime_error{"Unknown Protobuf SDK protocol"}; }
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
