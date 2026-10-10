// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <owning_variants.hpp>
#include <rohit/serializer.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace owning_variants_samples {
namespace codec = rohit::serializer;

// Build one active alternative with independently observable owned values.
inline owning_variants::record sample(std::size_t index) {
  using record = owning_variants::record;
  record value{};
  switch (index) {
  case 0:
    value.emplace_payload<record::e_payload::code>(17U);
    break;
  case 1:
    value.emplace_payload<record::e_payload::text>("owned text");
    break;
  case 2: {
    auto& event = value.emplace_payload<record::e_payload::event>();
    event.text = "nested event";
    event.numbers = {1U, 2U, 3U};
    event.labels = {{"alpha", "one"}, {"beta", "two"}};
    break;
  }
  case 3:
    value.emplace_payload<record::e_payload::alias>("alias text");
    break;
  default:
    throw std::invalid_argument{"Unknown owning variant fixture alternative"};
  }
  return value;
}

// Encode through the generated native codec rather than constructing wire fixtures manually.
template <template <codec::serialize_type> class Protocol, class Value>
std::vector<std::uint8_t> encode(const Value& value) {
  rohit::full_stream_auto_alloc output{};
  Protocol<codec::serialize_type::out> encoder{output};
  encoder.serialize_out(value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Decode exact message bytes into a supplied destination with the normal native limits.
template <template <codec::serialize_type> class Protocol, class Value>
void decode(const std::vector<std::uint8_t>& bytes, Value& value,
            codec::decode_limits limits = {}) {
  const auto input = rohit::make_constant_full_stream(bytes.data(), bytes.size());
  Protocol<codec::serialize_type::in> decoder{input, limits};
  decoder.serialize_in(value);
  decoder.finish();
}
}
