// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/serializer.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::managed::detail {

// Inspect generated integer-key records without constructing bytes or decoding a second object.
// Charges mirror the keyed binary decoder: wire bytes, value/field work, collections and depth.
class record_budget {
  serializer::decode_limits limits_;
  std::size_t bytes_{};
  std::size_t work_{};
  std::size_t allocation_{};
  std::size_t depth_{};
  std::size_t fields_{};

  // Add a bounded counter without overflowing, including products checked by the caller.
  static void add(std::size_t& total, std::size_t count, std::size_t limit) {
    if (count > limit - total) {
      throw std::length_error{"Collaboration record budget exceeded"};
    }
    total += count;
  }
  // Wire bytes consume both the input budget and decoder work.
  void bytes(std::size_t count) {
    add(bytes_, count, limits_.max_input_bytes);
    add(work_, count, limits_.max_work_units);
  }
  // Compact lengths and field IDs have the same canonical 30-bit wire limit as the encoder.
  void prefix(std::size_t count) {
    if (count > serializer::constants::variable_four_byte_max) {
      throw std::length_error{"Collaboration record wire length exceeded"};
    }
    bytes(serializer::detail::fixed_prefix_bytes(static_cast<std::uint32_t>(count)));
  }
  // Visit one generated object with independent field count and shared recursive budgets.
  void object(const auto& item) {
    if (depth_ >= limits_.max_nesting_depth) {
      throw std::length_error{"Collaboration record nesting limit exceeded"};
    }
    add(work_, 1, limits_.max_work_units);
    ++depth_;
    const auto outer_fields = std::exchange(fields_, 0);
    item.serialize_out(*this);
    fields_ = outer_fields;
    --depth_;
  }
  // Inspect owned strings, scalar arrays and nested generated records without copying their payloads.
  template <typename Value>
  void value(const Value& item) {
    add(work_, 1, limits_.max_work_units);
    if constexpr (std::is_arithmetic_v<Value>) {
      bytes(serializer::detail::binary_fixed_bytes<Value>);
    } else if constexpr (std::same_as<Value, std::string> ||
                         std::same_as<Value, std::string_view>) {
      if (item.size() > limits_.max_string_bytes) {
        throw std::length_error{"Collaboration string limit exceeded"};
      }
      serializer::detail::validate_utf8(std::string_view{item});
      prefix(item.size());
      add(allocation_, item.size(), limits_.max_allocation_bytes);
      bytes(item.size());
    } else if constexpr (requires {
                           typename Value::value_type;
                           item.size();
                           item.begin();
                         }) {
      using Element = typename Value::value_type;
      if (depth_ >= limits_.max_nesting_depth || item.size() > limits_.max_collection_elements ||
          item.size() > (limits_.max_allocation_bytes - allocation_) / sizeof(Element)) {
        throw std::length_error{"Collaboration collection limit exceeded"};
      }
      add(work_, 1, limits_.max_work_units);
      ++depth_;
      prefix(item.size());
      add(work_, item.size(), limits_.max_work_units);
      allocation_ += item.size() * sizeof(Element);
      if constexpr (std::is_arithmetic_v<Element>) {
        add(work_, item.size(), limits_.max_work_units);
        bytes(item.size() * sizeof(Element));
      } else {
        for (const auto& child : item) {
          value(child);
        }
      }
      --depth_;
    } else {
      object(item);
    }
  }

public:
  static constexpr auto key_type = serializer::serialize_key_type::integer;

  // Use the checkpoint's independent input bound and the store's remaining decoder limits.
  explicit record_budget(serializer::decode_limits limits) : limits_{limits} {}

  // Inspect a complete generated record, including deserialize_exact's root value charge.
  void check(const auto& item) {
    value(item);
  }
  // Count the first keyed field through the same path as all subsequent fields.
  void struct_serialize_out_start(const auto& field) {
    struct_serialize_out(field);
  }
  // make_pair unwraps generated reference_wrapper fields; no field value is materialized.
  template <typename Value>
  void struct_serialize_out(const std::pair<std::uint32_t, Value>& field) {
    add(fields_, 1, limits_.max_collection_elements);
    prefix(field.first);
    add(work_, 1, limits_.max_work_units);
    value(field.second);
  }
  // Keyed objects terminate with field ID zero.
  void struct_serialize_out_end() {
    prefix(0);
  }
};

// Borrow the checkpoint fields so preflight never copies the model or retained client state.
template <typename State>
struct checkpoint_view {
  std::uint32_t kind;
  const std::vector<std::uint8_t>& model;
  const State& state;
  std::string_view binding;

  // Use the standard codec directly with this borrowed field projection.
  template <template <serializer::serialize_type> class Protocol, type_check::output_stream Stream>
  void serialize_out(Stream& stream) const {
    serializer::serialize_to<Protocol>(stream, *this);
  }

  // Keep the existing checkpoint field IDs and encoding exactly aligned with the generated record.
  void serialize_out(auto& protocol) const {
    protocol.struct_serialize_out_start(std::make_pair(std::uint32_t{1}, std::cref(kind)));
    protocol.struct_serialize_out(std::make_pair(std::uint32_t{2}, std::cref(model)));
    protocol.struct_serialize_out(std::make_pair(std::uint32_t{3}, std::cref(state)));
    protocol.struct_serialize_out(std::make_pair(std::uint32_t{4}, std::cref(binding)));
    protocol.struct_serialize_out_end();
  }
};

} // namespace rohit::managed::detail
