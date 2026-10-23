// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#pragma once

#include <algorithm>
#include <rohit/cache.hpp>
#include <rohit/serializer.hpp>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace rohit::cache {

namespace detail {
template <typename Value>
inline constexpr bool is_variant = false;
template <typename... Values>
inline constexpr bool is_variant<std::variant<Values...>> = true;

// A cache-only variant encoding uses a checked alternative index followed by one owning value.
// This is separate from ordinary schema union discriminators and never changes document bytes.
template <typename Value>
struct variant_output {
  const Value& value;
  // Reject valueless storage before emitting its index or visiting an alternative.
  void serialize_out(auto& protocol) const {
    if (value.valueless_by_exception()) {
      throw std::invalid_argument{"Valueless cache variant"};
    }
    protocol.serialize_out(static_cast<std::uint64_t>(value.index()));
    std::visit([&](const auto& active) { protocol.serialize_out(active); }, value);
  }
};

template <typename Value>
struct variant_input {
  Value value{};
  // Decode only the selected alternative into fresh owning storage; unknown tags reject.
  template <std::size_t Index = 0>
  void read_alternative(auto& protocol, std::uint64_t selected) {
    if constexpr (Index < std::variant_size_v<Value>) {
      if (selected == Index) {
        std::variant_alternative_t<Index, Value> next{};
        protocol.serialize_in(next);
        value.template emplace<Index>(std::move(next));
      } else {
        read_alternative<Index + 1>(protocol, selected);
      }
    } else {
      throw std::invalid_argument{"Unknown cache variant alternative"};
    }
  }
  // Read the cache-specific tag before allocating or decoding an alternative.
  void serialize_in(auto& protocol) {
    std::uint64_t selected{};
    protocol.serialize_in(selected);
    read_alternative(protocol, selected);
  }
};
} // namespace detail

// Encode owning cache data with the bounded native keyed codec, never object memory layout.
template <typename Value>
bytes encode(const Value& value, std::size_t max_bytes = directory_options{}.max_entry_bytes) {
  if (max_bytes == 0) {
    throw std::length_error{"Cache encoding budget is zero"};
  }
  const stream_limits limits{std::min(stream_limits{}.min_read_buffer_bytes, max_bytes), max_bytes};
  full_stream_auto_alloc_limits output{&limits};
  serializer::detail::stream_protocol_t<serializer::binary_integer<serializer::serialize_type::out>,
                                        full_stream_auto_alloc_limits>
      encoder{output};
  if constexpr (detail::is_variant<Value>) {
    encoder.serialize_out(detail::variant_output<Value>{value});
  } else {
    encoder.serialize_out(value);
  }
  return {output.begin(), output.begin() + output.current_offset()};
}

// Decode into a fresh owning value with cumulative limits and exact-message validation.
template <typename Value>
Value decode(std::span<const std::uint8_t> value, serializer::decode_limits limits = {}) {
  const auto input = make_constant_full_stream(value.data(), value.size());
  if constexpr (detail::is_variant<Value>) {
    return serializer::deserialize_exact<detail::variant_input<Value>, serializer::binary_integer>(
               input, limits)
        .value;
  } else {
    return serializer::deserialize_exact<Value, serializer::binary_integer>(input, limits);
  }
}

} // namespace rohit::cache
