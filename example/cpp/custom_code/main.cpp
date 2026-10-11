// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.hpp"

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
static_assert(std::is_same_v<custom_code::inventory_item::quantity_type, std::uint32_t>);

// Encode an ordinary generated value into owned bytes using the selected native protocol.
template <template <rohit::serializer::serialize_type> typename Protocol>
[[nodiscard]] std::string encode(const custom_code::inventory_item& value) {
  rohit::full_stream_auto_alloc output{};
  value.serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Return a fresh value only after the entire message passes decoding and finish validation.
template <template <rohit::serializer::serialize_type> typename Protocol>
[[nodiscard]] custom_code::inventory_item decode(const std::string& bytes) {
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  return rohit::serializer::deserialize_exact<custom_code::inventory_item, Protocol>(input);
}

// Require malformed messages to fail through the codec's parse-error contract.
template <template <rohit::serializer::serialize_type> typename Protocol>
void verify_rejected(const std::string& bytes) {
  try {
    static_cast<void>(decode<Protocol>(bytes));
  } catch (const rohit::exception::base_parser&) {
    return;
  }
  throw std::runtime_error{"Malformed message was accepted"};
}

// Check durable fields, custom behavior after decoding, and exact-message failure boundaries.
template <template <rohit::serializer::serialize_type> typename Protocol>
void verify_protocol(const custom_code::inventory_item& value) {
  const auto bytes = encode<Protocol>(value);
  const auto copy = decode<Protocol>(bytes);
  if (copy.description != value.description || copy.unit_price_cents != value.unit_price_cents ||
      copy.quantity != value.quantity || copy.total_cents() != value.total_cents() ||
      copy.summary() != value.summary()) {
    throw std::runtime_error{"Round trip changed the inventory item"};
  }
  if (bytes.empty()) {
    throw std::runtime_error{"Expected a nonempty encoded message"};
  }
  verify_rejected<Protocol>(bytes.substr(0, bytes.size() - 1));
  verify_rejected<Protocol>(bytes + "!");
}
} // namespace

// Exercise native schema hooks, inline methods, an external definition, and generated codecs.
int main() {
  try {
    custom_code::inventory_item::serializer_require_behavior_definitions();
    custom_code::inventory_item value{};
    value.add_units(3);
    if (value.total_cents() != 1250 || value.summary() != "notebook x5") {
      throw std::runtime_error{"Unexpected custom C++ result"};
    }

    const auto previous_quantity = value.quantity;
    bool rejected_overflow{};
    try {
      value.add_units(std::numeric_limits<custom_code::inventory_item::quantity_type>::max());
    } catch (const std::overflow_error&) {
      rejected_overflow = true;
    }
    if (!rejected_overflow || value.quantity != previous_quantity) {
      throw std::runtime_error{"Rejected addition changed quantity"};
    }

    verify_protocol<rohit::serializer::json>(value);
    verify_protocol<rohit::serializer::binary_none>(value);
    verify_protocol<rohit::serializer::binary_integer>(value);
    verify_protocol<rohit::serializer::binary_string>(value);

    std::cout << "summary: " << value.summary() << '\n'
              << "total cents: " << value.total_cents() << '\n'
              << "overflow: rejected, quantity unchanged\n"
              << "four protocols: exact round trips passed\n"
              << "truncated and trailing input: rejected\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
