//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#pragma once

#include <rohit/binary_output_support.hpp>
#include <rohit/utf8.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace rohit::serializer {

class binary_none_output;

namespace detail {

// Count committed wire bytes without allocating or pretending to expose writable memory.
class binary_count_sink {
  std::size_t position_{};

public:
  static constexpr bool count_only = true;

  // Return the complete encoded extent accumulated so far.
  [[nodiscard]] constexpr std::size_t position_bytes() const noexcept { return position_; }

  // Check the prospective extent without changing the committed count.
  constexpr void reserve_bytes(std::size_t size) const {
    if (size > rohit::detail::maximum_buffer_bytes - position_) {
      throw rohit::exception::stream_overflow_exception{};
    }
  }

  // Commit an already semantically validated extent exactly once.
  constexpr void commit_counted_bytes(std::size_t size) {
    reserve_bytes(size);
    position_ += size;
  }

  // Count one wire byte; its value does not change its size.
  constexpr void put_u8(std::uint8_t) { commit_counted_bytes(sizeof(std::uint8_t)); }

  // Count a borrowed byte range without reading its contents.
  constexpr void append_octets(std::span<const std::uint8_t> bytes) {
    commit_counted_bytes(bytes.size());
  }

  // Count code units; protocol-level validation and prefixing precede this operation.
  constexpr void append_chars(std::string_view text) { commit_counted_bytes(text.size()); }
};

// Borrow independent writable bytes; source lifetimes and nonoverlap remain caller preconditions.
class binary_memory_sink {
  std::span<std::uint8_t> storage_;
  std::size_t position_{};

public:
  static constexpr bool count_only = false;

  // Reject a destination whose extent cannot be represented by the existing stream contract.
  explicit constexpr binary_memory_sink(std::span<std::uint8_t> storage) : storage_{storage} {
    if (storage.size() > rohit::detail::maximum_buffer_bytes) {
      throw std::invalid_argument{"Invalid stream buffer size"};
    }
  }

  // Return the committed output length, excluding any unused destination capacity.
  [[nodiscard]] constexpr std::size_t position_bytes() const noexcept { return position_; }

  // Check capacity before subtraction, indexing, or writing the next operation.
  constexpr void reserve_bytes(std::size_t size) const {
    if (size > storage_.size() - position_) {
      throw rohit::exception::stream_overflow_exception{};
    }
  }

  // Append one byte after checking its destination slot.
  constexpr void put_u8(std::uint8_t value) {
    reserve_bytes(sizeof(value));
    storage_[position_++] = value;
  }

  // Copy a complete independent byte range, preserving a failed operation's destination.
  constexpr void append_octets(std::span<const std::uint8_t> bytes) {
    reserve_bytes(bytes.size());
    if (bytes.empty()) {
      return;
    }
    if (std::is_constant_evaluated()) {
      std::copy_n(bytes.data(), bytes.size(), storage_.data() + position_);
    } else {
      std::memcpy(storage_.data() + position_, bytes.data(), bytes.size());
    }
    position_ += bytes.size();
  }

  // Copy code units into wire bytes without a constexpr-invalid pointer reinterpretation.
  constexpr void append_chars(std::string_view text) {
    reserve_bytes(text.size());
    if (text.empty()) {
      return;
    }
    if (std::is_constant_evaluated()) {
      std::copy_n(text.data(), text.size(), storage_.data() + position_);
    } else {
      std::memcpy(storage_.data() + position_, text.data(), text.size());
    }
    position_ += text.size();
  }
};

// Borrow the concrete writer and optionally its memory; count mode owns no output buffer.
class binary_dual_sink {
  binary_none_output* protocol_;
  std::span<std::uint8_t> storage_{};
  std::size_t position_{};
  bool counting_{true};

public:
  static constexpr bool count_only = false;

  // Start a pure counting traversal associated with the enclosing concrete protocol.
  explicit constexpr binary_dual_sink(binary_none_output& protocol) : protocol_{&protocol} {}

  // Borrow caller memory and reject extents beyond the established stream ceiling.
  constexpr binary_dual_sink(binary_none_output& protocol, std::span<std::uint8_t> storage)
      : protocol_{&protocol}, storage_{storage}, counting_{false} {
    if (storage.size() > rohit::detail::maximum_buffer_bytes) {
      throw std::invalid_argument{"Invalid stream buffer size"};
    }
  }

  // Return the concrete protocol expected by emission-only generated model methods.
  [[nodiscard]] constexpr binary_none_output& model_protocol() const noexcept { return *protocol_; }

  // Identify count mode so fixed-width payloads can be added without visiting elements.
  [[nodiscard]] constexpr bool counting() const noexcept { return counting_; }

  // Return committed bytes in either mode.
  [[nodiscard]] constexpr std::size_t position_bytes() const noexcept { return position_; }

  // Check the full next operation without committing or touching caller memory.
  constexpr void reserve_bytes(std::size_t size) const {
    const auto capacity = counting_ ? rohit::detail::maximum_buffer_bytes : storage_.size();
    if (size > capacity - position_) {
      throw rohit::exception::stream_overflow_exception{};
    }
  }

  // Advance count mode only; writing callers cannot silently skip uninitialized output.
  constexpr void commit_counted_bytes(std::size_t size) {
    if (!counting_) {
      throw std::logic_error{"Cannot commit counted bytes to a memory sink"};
    }
    reserve_bytes(size);
    position_ += size;
  }

  // Count or append one validated wire byte.
  constexpr void put_u8(std::uint8_t value) {
    reserve_bytes(sizeof(value));
    if (!counting_) {
      storage_[position_] = value;
    }
    ++position_;
  }

  // Count or copy a complete independent byte range after its reservation succeeds.
  constexpr void append_octets(std::span<const std::uint8_t> bytes) {
    reserve_bytes(bytes.size());
    if (!counting_ && !bytes.empty()) {
      if (std::is_constant_evaluated()) {
        std::copy_n(bytes.data(), bytes.size(), storage_.data() + position_);
      } else {
        std::memcpy(storage_.data() + position_, bytes.data(), bytes.size());
      }
    }
    position_ += bytes.size();
  }

  // Count or copy code units without a constant-evaluation-invalid pointer conversion.
  constexpr void append_chars(std::string_view text) {
    reserve_bytes(text.size());
    if (!counting_ && !text.empty()) {
      if (std::is_constant_evaluated()) {
        std::copy_n(text.data(), text.size(), storage_.data() + position_);
      } else {
        std::memcpy(storage_.data() + position_, text.data(), text.size());
      }
    }
    position_ += text.size();
  }
};

// Recognize borrowed source collections independently of the existing owning runtime concepts.
template <class Value> inline constexpr bool constant_binary_span = false;

template <class Element, std::size_t Extent>
inline constexpr bool constant_binary_span<std::span<Element, Extent>> = true;

// Require only positional binary operations, without JSON formatting or writable-pointer hooks.
template <class Sink>
concept binary_constant_sink = requires(
    Sink& sink, std::size_t size, std::span<const std::uint8_t> bytes, std::string_view text) {
  { Sink::count_only } -> std::convertible_to<bool>;
  { sink.position_bytes() } -> std::same_as<std::size_t>;
  sink.reserve_bytes(size);
  sink.put_u8(std::uint8_t{});
  sink.append_octets(bytes);
  sink.append_chars(text);
};

// Share strict UTF-8 semantics while retaining SIMD validation for explicit runtime sizing.
constexpr void validate_constant_binary_text(std::string_view text) {
  if (!std::is_constant_evaluated()) {
    validate_utf8(text);
    return;
  }
  constexpr std::size_t maximum_utf8_sequence_bytes = 4;
  std::size_t offset{};
  while (offset != text.size()) {
    if (static_cast<std::uint8_t>(text[offset]) < 0x80) {
      ++offset;
      continue;
    }
    std::array<std::uint8_t, maximum_utf8_sequence_bytes> sequence{};
    const auto available = std::min(sequence.size(), text.size() - offset);
    std::copy_n(text.begin() + offset, available, sequence.begin());
    offset += utf8_sequence_size(std::span<const std::uint8_t>{sequence}.first(available));
  }
}

// Distinguish a generated type's own opt-in helper from one inherited by a custom derived codec.
template <class Value> constexpr bool has_direct_constant_binary_support() {
  if constexpr (requires { typename Value::serializer_constant_evaluation_type; }) {
    return std::same_as<Value, typename Value::serializer_constant_evaluation_type>;
  } else {
    return false;
  }
}

// Require an exact-type emission marker so inherited metadata cannot discard derived fields.
template <class Value> constexpr bool has_emission_binary_support() {
  if constexpr (has_direct_constant_binary_support<Value>() &&
                requires { Value::serializer_emission_only; }) {
    return Value::serializer_emission_only;
  } else {
    return false;
  }
}

// Identify opt-in generated models without restricting standalone custom constexpr codecs.
template <class Value> constexpr bool has_constant_binary_support() {
  if constexpr (requires { Value::serializer_reuses_storage; }) {
    if constexpr (requires { typename Value::serializer_constant_evaluation_type; }) {
      if constexpr (!has_direct_constant_binary_support<Value>()) {
        return true;
      }
      return Value::serializer_constant_evaluation;
    } else if constexpr (requires { Value::serializer_constant_evaluation; }) {
      return Value::serializer_constant_evaluation;
    } else {
      return false;
    }
  } else {
    return true;
  }
}

// Encode existing positional wire semantics through either a real byte sink or a pure counter.
template <binary_constant_sink Sink> class constant_binary_out {
  Sink& sink_;

  // Count fixed payloads directly, including the concrete writer's runtime-selected count mode.
  constexpr bool count_fixed_bytes(std::size_t bytes) {
    if constexpr (Sink::count_only) {
      sink_.commit_counted_bytes(bytes);
      return true;
    } else {
      if constexpr (requires {
                      sink_.counting();
                      sink_.commit_counted_bytes(bytes);
                    }) {
        if (sink_.counting()) {
          sink_.commit_counted_bytes(bytes);
          return true;
        }
      }
      return false;
    }
  }

  // Encode a padding-free integer into its exact little-endian wire representation.
  template <std::integral Value> constexpr void write_integer(Value value) {
    static_assert(rohit::detail::endian_integer<Value>,
                  "Binary integers must have no padding bits");
    if (!count_fixed_bytes(sizeof(Value))) {
      using unsigned_type = std::make_unsigned_t<Value>;
      auto bits = static_cast<unsigned_type>(value);
      if constexpr (std::endian::native == std::endian::little) {
        // Padding-free unsigned storage already has the canonical little-endian wire bytes.
        sink_.append_octets(std::bit_cast<std::array<std::uint8_t, sizeof(Value)>>(bits));
      } else {
        std::array<std::uint8_t, sizeof(Value)> bytes{};
        for (auto& byte : bytes) {
          byte = static_cast<std::uint8_t>(bits & constants::wire_byte_mask);
          if constexpr (sizeof(Value) > sizeof(std::uint8_t)) {
            bits >>= constants::wire_byte_bits;
          }
        }
        sink_.append_octets(bytes);
      }
    }
  }

  // Keep C++20-ineligible map iterators outside constexpr function bodies.
  template <class Value> void write_runtime_map(const Value& value) {
    serialize_out_variable(value.size());
    for (const auto& item : value) {
      serialize_out(item.first);
      serialize_out(item.second);
    }
  }

public:
  static constexpr bool is_json = false;
  static constexpr wire_format format = wire_format::binary_none;
  static constexpr serialize_key_type key_type = serialize_key_type::none;
  static constexpr std::endian wire_endian = std::endian::little;
  static constexpr binary_text_validation text_validation = binary_text_validation::strict;

  // Borrow the sink for one complete traversal; neither protocol nor sink owns model storage.
  explicit constexpr constant_binary_out(Sink& sink) : sink_{sink} {}

  // Append the schema's fixed magic without a count, terminator, or additional validation.
  constexpr void serialize_magic(std::string_view bytes) { sink_.append_chars(bytes); }

  // Write the shortest established 30-bit prefix after checking its complete payload range.
  constexpr void serialize_out_variable(std::integral auto value) {
    if constexpr (std::is_signed_v<decltype(value)>) {
      if (value < 0) {
        throw std::out_of_range{"Binary variable integers must be nonnegative"};
      }
    }
    if (value > constants::variable_four_byte_max) {
      throw std::out_of_range{"Binary variable integer exceeds the 30-bit wire range"};
    }
    const auto candidate = static_cast<std::uint32_t>(value);
    const auto bytes = fixed_prefix_bytes(candidate);
    if (!count_fixed_bytes(bytes)) {
      std::array<std::uint8_t, fixed_prefix_bytes(constants::variable_four_byte_max)> storage{};
      auto* cursor = storage.data();
      write_fixed_prefix(cursor, candidate);
      sink_.append_octets(std::span<const std::uint8_t>{storage}.first(bytes));
    }
  }

  // Preserve existing prefix/varint policies, including deliberate low-30-bit lenient truncation.
  template <bool Prefix, bool Strict, std::unsigned_integral Value>
  constexpr void serialize_out_compact(Value value) {
    static_assert(!std::same_as<Value, bool> && std::numeric_limits<Value>::digits <= 64);
    if constexpr (Prefix) {
      if constexpr (Strict && std::numeric_limits<Value>::digits > 30) {
        if (value > constants::variable_four_byte_max) {
          throw std::out_of_range{"Compact prefix exceeds the 30-bit wire range"};
        }
      }
      serialize_out_variable(static_cast<std::uint32_t>(value & constants::variable_four_byte_max));
    } else {
      constexpr auto maximum_bytes =
          (std::numeric_limits<Value>::digits + constants::varint_payload_bits - 1) /
          constants::varint_payload_bits;
      std::array<std::uint8_t, maximum_bytes> bytes{};
      std::size_t size{};
      do {
        auto byte = static_cast<std::uint8_t>(value & constants::varint_payload_mask);
        value >>= constants::varint_payload_bits;
        if (value != 0) {
          byte |= constants::varint_continuation_mask;
        }
        bytes[size++] = byte;
      } while (value != 0);
      sink_.append_octets(std::span<const std::uint8_t>{bytes}.first(size));
    }
  }

  // Encode only the selected union discriminator followed by its active payload.
  template <class Value>
  constexpr void serialize_out(const std::integral auto& discriminator, const Value& value) {
    serialize_out_variable(discriminator);
    serialize_out(value);
  }

  // Preserve generated positional union pairs without adding a field key.
  template <class Discriminator, class Value>
  constexpr void serialize_out(const std::pair<Discriminator, Value>& value) {
    serialize_out(value.first, value.second);
  }

  // Unwrap the existing generated runtime union/reference contract without copying its payload.
  template <class Value> constexpr void serialize_out(const std::reference_wrapper<Value>& value) {
    serialize_out(value.get());
  }

  // Encode the recursively eligible value without ever copying aggregate object layout.
  template <class Value> constexpr void serialize_out(const Value& value) {
    if constexpr (std::same_as<Value, char> || std::same_as<Value, bool> ||
                  std::same_as<Value, std::uint8_t>) {
      sink_.put_u8(static_cast<std::uint8_t>(value));
    } else if constexpr (std::is_enum_v<Value>) {
      if constexpr (requires { serializer_enum_valid(value); }) {
        if (!serializer_enum_valid(value)) {
          throw std::invalid_argument{"Unknown binary enum value"};
        }
      }
      serialize_out_variable(static_cast<std::underlying_type_t<Value>>(value));
    } else if constexpr (std::integral<Value>) {
      write_integer(value);
    } else if constexpr (std::same_as<Value, std::string> ||
                         std::same_as<Value, std::string_view>) {
      if (value.size() > constants::variable_four_byte_max) {
        throw std::out_of_range{"Binary variable integer exceeds the 30-bit wire range"};
      }
      validate_constant_binary_text(std::string_view{value});
      serialize_out_variable(value.size());
      sink_.append_chars(std::string_view{value});
    } else if constexpr (std::floating_point<Value>) {
      static_assert(
          rohit::detail::endian_floating_point<Value>,
          "Binary floating-point output requires a 32-bit or 64-bit IEC 559 representation");
      if constexpr (sizeof(Value) == sizeof(std::uint32_t)) {
        serialize_out(std::bit_cast<std::uint32_t>(value));
      } else {
        serialize_out(std::bit_cast<std::uint64_t>(value));
      }
    } else if constexpr (has_emission_binary_support<Value>() &&
                         requires { value.serialize_out(sink_.model_protocol()); }) {
      value.serialize_out(sink_.model_protocol());
    } else if constexpr (std::is_pointer_v<Value> &&
                         has_emission_binary_support<
                             std::remove_cv_t<std::remove_pointer_t<Value>>>() &&
                         requires { value->serialize_out(sink_.model_protocol()); }) {
      if (!value) {
        throw std::invalid_argument{"Null source object"};
      }
      value->serialize_out(sink_.model_protocol());
    } else if constexpr (type_check::serializer_out_enabled_ptr<Value, constant_binary_out>) {
      if (!value) {
        throw std::invalid_argument{"Null source object"};
      }
      serialize_out(*value);
    } else if constexpr (type_check::serializer_out_enabled<Value, constant_binary_out>) {
      if (std::is_constant_evaluated() && !has_constant_binary_support<Value>()) {
        throw "Generated model requires cpp.constant_evaluation=true";
      }
      if constexpr (has_direct_constant_binary_support<Value>() &&
                    requires { value.serialize_constant_out(*this); }) {
        value.serialize_constant_out(*this);
      } else {
        value.serialize_out(*this);
      }
    } else if constexpr (type_check::vector<Value> || type_check::fixed_array<Value> ||
                         constant_binary_span<Value>) {
      serialize_out_variable(value.size());
      using element_type = typename Value::value_type;
      if constexpr (binary_array_scalar<element_type>) {
        if (value.size() > rohit::detail::maximum_buffer_bytes / sizeof(element_type)) {
          throw rohit::exception::stream_overflow_exception{};
        }
        const auto bytes = value.size() * sizeof(element_type);
        sink_.reserve_bytes(bytes);
        if (!count_fixed_bytes(bytes)) {
          if constexpr (std::same_as<element_type, std::uint8_t>) {
            // Byte pools already have the exact wire representation; copy the complete span once.
            sink_.append_octets(std::span<const std::uint8_t>{value.data(), value.size()});
          } else {
            for (const auto& item : value) {
              serialize_out(item);
            }
          }
        }
      } else {
        for (const auto& item : value) {
          serialize_out(item);
        }
      }
    } else if constexpr (type_check::map<Value>) {
      if (std::is_constant_evaluated()) {
        throw "std::map is runtime-only in the C++20 constant binary profile";
      }
      write_runtime_map(value);
    } else {
      static_assert(rohit::detail::unsupported_type<Value>,
                    "Value has no positional binary serialization support");
    }
  }

  // Reserve one complete fixed-field batch before writing any of its scalar payloads.
  template <binary_fixed_scalar... Values>
    requires(sizeof...(Values) > 1 && sizeof...(Values) <= constants::maximum_fixed_batch_fields)
  constexpr void struct_serialize_out_fixed(Values... values) {
    constexpr auto bytes = (binary_fixed_bytes<Values> + ...);
    sink_.reserve_bytes(bytes);
    if (!count_fixed_bytes(bytes)) {
      (serialize_out(values), ...);
    }
  }

  // Positional field boundaries add no bytes.
  constexpr void struct_serialize_out_start(const auto& value) { serialize_out(value); }

  // Append a following positional field without a key or separator.
  constexpr void struct_serialize_out(const auto& value) { serialize_out(value); }

  // Positional objects have no closing marker.
  constexpr void struct_serialize_out_end() noexcept {}

  // Empty positional objects emit no bytes.
  constexpr void struct_serialize_out_empty() noexcept {}
};

} // namespace detail

// Concrete positional writer for emission-only models: default construction counts, a span writes.
class binary_none_output final {
  detail::binary_dual_sink sink_;
  detail::constant_binary_out<detail::binary_dual_sink> encoder_;

public:
  static constexpr bool is_json = false;
  static constexpr wire_format format = wire_format::binary_none;
  static constexpr serialize_key_type key_type = serialize_key_type::none;
  static constexpr std::endian wire_endian = std::endian::little;
  static constexpr binary_text_validation text_validation = binary_text_validation::strict;

  // Count validated bytes without allocating output or requiring a destination.
  constexpr binary_none_output() : sink_{*this}, encoder_{sink_} {}

  // Borrow independent caller memory for one writing traversal starting at offset zero.
  explicit constexpr binary_none_output(std::span<std::uint8_t> destination)
      : sink_{*this, destination}, encoder_{sink_} {}

  // Prevent copying the internal references into a different writer instance.
  binary_none_output(const binary_none_output&) = delete;
  // Preserve the same lifetime invariant for moves.
  binary_none_output(binary_none_output&&) = delete;
  // A borrowed writer cannot replace its internal storage/reference association.
  binary_none_output& operator=(const binary_none_output&) = delete;
  // Moving assignment cannot rebind the encoder's borrowed sink.
  binary_none_output& operator=(binary_none_output&&) = delete;

  // Return the complete committed encoded extent in either mode.
  [[nodiscard]] constexpr std::size_t position_bytes() const noexcept {
    return sink_.position_bytes();
  }

  // Expose mode for callers that need to inspect a writer, without changing wire layout.
  [[nodiscard]] constexpr bool counting() const noexcept { return sink_.counting(); }

  // Emit raw schema magic without an additional prefix or terminator.
  constexpr void serialize_magic(std::string_view bytes) { encoder_.serialize_magic(bytes); }

  // Encode the established 30-bit length/count/discriminator prefix.
  constexpr void serialize_out_variable(std::integral auto value) {
    encoder_.serialize_out_variable(value);
  }

  // Preserve prefix/varint strictness through the shared positional implementation.
  template <bool Prefix, bool Strict, std::unsigned_integral Value>
  constexpr void serialize_out_compact(Value value) {
    encoder_.template serialize_out_compact<Prefix, Strict>(value);
  }

  // Encode a value or an active union discriminator/payload without model templates.
  constexpr void serialize_out(const auto&... values) { encoder_.serialize_out(values...); }

  // Reserve/count a complete fixed-width batch before encoding any scalar payload.
  template <detail::binary_fixed_scalar... Values>
  constexpr void struct_serialize_out_fixed(Values... values) {
    encoder_.struct_serialize_out_fixed(values...);
  }

  // Positional first fields require no key or object marker.
  constexpr void struct_serialize_out_start(const auto& value) {
    encoder_.struct_serialize_out_start(value);
  }

  // Append a positional field without a separator.
  constexpr void struct_serialize_out(const auto& value) { encoder_.struct_serialize_out(value); }

  // Positional objects require no closing marker.
  constexpr void struct_serialize_out_end() noexcept {}

  // Empty positional objects add no bytes.
  constexpr void struct_serialize_out_empty() noexcept {}
};

namespace detail {

// Encode explicit constant/emission output without requiring the owning runtime stream headers.
template <class Value>
constexpr std::size_t serialize_constant_binary_to(std::span<std::uint8_t> destination,
                                                   const Value& value) {
  if constexpr (has_emission_binary_support<Value>() || constant_binary_span<Value>) {
    binary_none_output encoder{destination};
    encoder.serialize_out(value);
    return encoder.position_bytes();
  } else {
    binary_memory_sink sink{destination};
    constant_binary_out encoder{sink};
    encoder.serialize_out(value);
    return sink.position_bytes();
  }
}

} // namespace detail

// Validate and count exact strict little-endian positional bytes without allocating output.
template <class Value> [[nodiscard]] constexpr std::size_t binary_none_size(const Value& value) {
  if constexpr (detail::has_emission_binary_support<Value>() ||
                detail::constant_binary_span<Value>) {
    binary_none_output encoder{};
    encoder.serialize_out(value);
    return encoder.position_bytes();
  } else {
    detail::binary_count_sink sink{};
    detail::constant_binary_out encoder{sink};
    encoder.serialize_out(value);
    return sink.position_bytes();
  }
}

// Write an emission model or borrowed span once into independent caller storage.
template <class Value>
  requires(detail::has_emission_binary_support<Value>() || detail::constant_binary_span<Value>)
[[nodiscard]] constexpr std::size_t serialize_binary_none_to(std::span<std::uint8_t> destination,
                                                             const Value& value) {
  return detail::serialize_constant_binary_to(destination, value);
}

// Evaluate the deterministic factory twice, keeping only its exact-size owned wire bytes.
template <auto Factory> [[nodiscard]] consteval auto make_binary_none_bytes() {
  constexpr auto byte_count = [] {
    const auto value = Factory();
    return binary_none_size(value);
  }();
  std::array<std::uint8_t, byte_count> bytes{};
  const auto value = Factory();
  const auto written = detail::serialize_constant_binary_to(std::span<std::uint8_t>{bytes}, value);
  if (written != byte_count) {
    throw "binary_none count/write mismatch";
  }
  return bytes;
}

} // namespace rohit::serializer
