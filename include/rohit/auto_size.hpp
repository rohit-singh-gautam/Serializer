// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/binary_output_support.hpp>

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace rohit::serializer::detail {

// Identify real count sinks without treating ordinary output capacity as a computed record size.
template <typename Protocol>
[[nodiscard]] constexpr bool auto_size_counting(const Protocol& protocol) {
  if constexpr (requires { protocol.counting(); }) {
    return protocol.counting();
  } else if constexpr (requires { protocol.get_stream(); }) {
    using sink = std::remove_cvref_t<decltype(protocol.get_stream())>;
    if constexpr (requires { sink::count_only; }) { return sink::count_only; }
    else { return false; }
  } else {
    return false;
  }
}

// Observe committed bytes from a counting protocol while preserving its borrowed sink lifetime.
template <typename Protocol>
[[nodiscard]] constexpr std::size_t auto_size_count_position(const Protocol& protocol) {
  if constexpr (requires { protocol.position_bytes(); }) {
    return protocol.position_bytes();
  } else if constexpr (requires { protocol.get_stream().size(); }) {
    return protocol.get_stream().size();
  } else {
    return 0;
  }
}

// Validate one completed count traversal without allocating or recursively counting nested records.
template <std::unsigned_integral Size, typename Protocol>
class auto_size_count_guard {
  const Protocol& protocol_;
  bool active_;
  std::size_t begin_bytes_;
  std::size_t excluded_bytes_;

public:
  // Borrow a protocol until finish(); inactive guards impose no work on ordinary output paths.
  constexpr auto_size_count_guard(const Protocol& protocol, bool selected, std::size_t excluded_bytes)
      : protocol_{protocol}, active_{selected && auto_size_counting(protocol)},
        begin_bytes_{active_ ? auto_size_count_position(protocol) : 0}, excluded_bytes_{excluded_bytes} {
    static_assert(sizeof(Size) == sizeof(std::uint32_t) || sizeof(Size) == sizeof(std::uint64_t));
  }

  // Reject a derived header overflow after the full record was counted; no partial size is returned.
  constexpr void finish() const {
    if (!active_) { return; }
    const auto end_bytes = auto_size_count_position(protocol_);
    if (end_bytes < begin_bytes_ || excluded_bytes_ > end_bytes - begin_bytes_) {
      throw std::invalid_argument{"Invalid auto_size field span"};
    }
    if (end_bytes - begin_bytes_ - excluded_bytes_ > std::numeric_limits<Size>::max()) {
      throw rohit::exception::stream_overflow_exception{};
    }
  }
};

// Keep the count-only width check local to one generated record and its selected native protocol.
template <std::unsigned_integral Size, typename Protocol>
[[nodiscard]] constexpr auto make_auto_size_count_guard(
    const Protocol& protocol, bool selected, std::size_t excluded_bytes = 0) {
  return auto_size_count_guard<Size, Protocol>{protocol, selected, excluded_bytes};
}

// Observe record-local native bytes without allowing a declared size to control resource bounds.
template <typename Protocol>
[[nodiscard]] constexpr std::size_t auto_size_remaining(const Protocol& protocol) {
  constexpr auto format = protocol_wire_format<Protocol>();
  if constexpr (format == wire_format::binary_none || format == wire_format::binary_integer ||
                format == wire_format::binary_string) {
    static_assert(requires { protocol.get_stream().remaining_buffer(); },
                  "auto_size requires a native input protocol exposing its bounded cursor");
    return protocol.get_stream().remaining_buffer();
  } else {
    return 0;
  }
}

// Borrow a generated reader and track one required size header while preserving its decode/donor logic.
template <typename Reader>
struct auto_size_reader {
  static constexpr bool tracks_auto_sizes = true;
  Reader& value;
  bool& seen;
  std::uint32_t id;
  std::string_view name;
  std::uint64_t* discarded{};
  std::size_t width_bytes{};
  std::size_t* field_bytes{};

  // Capture the actual accepted key and scalar span, including noncanonical compact keys.
  template <typename Protocol>
  constexpr void finish_field(Protocol& protocol, bool selected) {
    if (id != 0 && selected && field_bytes) {
      if constexpr (requires { protocol.auto_size_field_remaining(); }) {
        *field_bytes = protocol.auto_size_field_remaining() - auto_size_remaining(protocol);
      }
    }
  }

  // Decode transport-only metadata into scoped scratch without creating object storage.
  template <typename Protocol>
  constexpr void read_discarded(Protocol& protocol) {
    if (width_bytes == sizeof(std::uint32_t)) {
      std::uint32_t actual{};
      protocol.serialize_in(actual);
      *discarded = actual;
    } else {
      protocol.serialize_in(*discarded);
    }
  }

  // Reject a duplicate selected header before decoding its scalar; an inactive adapter has id zero.
  constexpr void observe(bool selected) {
    if (id != 0 && selected) {
      if (seen) { throw std::invalid_argument{"Duplicate auto_size field"}; }
      seen = true;
    }
  }

  // Forward the numeric key to the same generated reader and shared input session.
  template <typename Protocol>
  constexpr void serialize_in_member_by_identifier(Protocol& protocol, std::uint32_t key) {
    observe(key == id);
    if (id != 0 && key == id && discarded) { read_discarded(protocol); }
    else { value.serialize_in_member_by_identifier(protocol, key); }
    finish_field(protocol, key == id);
  }

  // Forward the exact wire name without changing unknown-field or storage reuse behavior.
  template <typename Protocol>
  constexpr void serialize_in_member_by_name(Protocol& protocol, std::string_view key) {
    observe(key == name);
    if (id != 0 && key == name && discarded) { read_discarded(protocol); }
    else { value.serialize_in_member_by_name(protocol, key); }
    finish_field(protocol, key == name);
  }
};

// Keep a stack-local adapter; reader, seen state, and literal name must outlive synchronous decoding.
template <typename Reader>
[[nodiscard]] constexpr auto make_auto_size_reader(
    Reader& value, bool& seen, std::uint32_t id, std::string_view name,
    std::uint64_t* discarded = nullptr, std::size_t width_bytes = 0,
    std::size_t* field_bytes = nullptr) noexcept {
  return auto_size_reader<Reader>{value, seen, id, name, discarded, width_bytes, field_bytes};
}

} // namespace rohit::serializer::detail
