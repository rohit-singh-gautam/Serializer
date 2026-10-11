// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/serializer.hpp>

#include "constexpr_codecs.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
namespace codec = rohit::serializer;
namespace models = constexpr_codecs;
using keys = codec::serialize_key_type;

// Borrow typed byte storage; the cursor and every referent are created within each evaluation.
template <typename Byte>
class borrowed_stream {
  std::span<Byte> storage_;
  mutable std::size_t offset_{};

public:
  // Retain caller-owned storage without allocating or changing its contents.
  explicit constexpr borrowed_stream(std::span<Byte> storage) noexcept : storage_{storage} {}

  // Return the current borrowed position, including the valid one-past endpoint.
  [[nodiscard]] constexpr Byte* curr() const noexcept { return storage_.data() + offset_; }

  // Return the unconsumed capacity without moving the cursor.
  [[nodiscard]] constexpr std::size_t remaining_buffer() const noexcept {
    return storage_.size() - offset_;
  }

  // Detect exact completion for the production decoder's trailing-byte check.
  [[nodiscard]] constexpr bool full() const noexcept { return offset_ == storage_.size(); }

  // Observe the successfully committed byte count.
  [[nodiscard]] constexpr std::size_t position_bytes() const noexcept { return offset_; }

  // Consume a prevalidated range; the protocol checks bounds before this unchecked primitive.
  [[nodiscard]] constexpr Byte* get_curr_and_increase_unchecked(std::size_t size) const noexcept {
    auto* position = curr();
    offset_ += size;
    return position;
  }

  // Reject a complete output operation before exposing or changing any destination byte.
  constexpr void reserve(std::size_t size) const {
    if (size > remaining_buffer()) {
      throw rohit::exception::stream_overflow_exception{};
    }
  }

  // Copy a typed byte range without a C++20-invalid cast through void.
  constexpr void append_external(const std::uint8_t* data, std::size_t size) {
    reserve(size);
    std::copy_n(data, size, get_curr_and_increase_unchecked(size));
  }

  // Convert text code units into typed wire bytes while preserving their exact unsigned values.
  constexpr void append_external(const char* data, std::size_t size) {
    reserve(size);
    auto* output = get_curr_and_increase_unchecked(size);
    for (std::size_t index = 0; index < size; ++index) {
      output[index] = static_cast<std::uint8_t>(data[index]);
    }
  }

  // Supply the ordinary output concept's raw-memory hook for runtime-only bulk operations.
  void append_external(const void* data, std::size_t size) {
    reserve(size);
    std::memcpy(get_curr_and_increase_unchecked(size), data, size);
  }

  // Preserve the raw-memory append hook used by production runtime scalar batches.
  void append(const void* data, std::size_t size) { append_external(data, size); }

  // Append a complete text view without treating embedded NUL code units as terminators.
  constexpr void append(std::string_view text) { append_external(text.data(), text.size()); }

  // Preserve owning-string calls while borrowing their bytes only for this append.
  constexpr void append(const std::string& text) { append(std::string_view{text}); }

  // Append one null-terminated literal through the same text-copy operation.
  constexpr void append(const char* text) { append(std::string_view{text}); }

  // Append one unsigned scalar wire byte after successful reservation.
  constexpr void append(std::uint8_t value) {
    reserve(sizeof(value));
    *get_curr_and_increase_unchecked(sizeof(value)) = value;
  }

  // Preserve the output concept's character hook with an unsigned wire conversion.
  constexpr void append(char value) { append(static_cast<std::uint8_t>(value)); }

  // Apply a runtime-only bulk transform after reserving its complete output extent.
  void append_transformed(const void* data, std::size_t input_bytes, std::size_t output_bytes,
      void (*transform)(std::uint8_t*, const std::uint8_t*, std::size_t) noexcept) {
    reserve(output_bytes);
    transform(get_curr_and_increase_unchecked(output_bytes),
        static_cast<const std::uint8_t*>(data), input_bytes);
  }

  // Supply runtime text formatting required by the broad output-buffer concept.
  template <std::integral Value>
  void append_string(Value value) { append(std::to_string(value)); }

  // Append mixed text fragments through their existing typed overloads.
  template <typename... Values>
  constexpr void write(const Values&... values) { (append(values), ...); }

  // Write a compact scalar prefix as typed bytes without raw-memory reinterpretation.
  template <std::same_as<std::uint8_t>... Values>
  constexpr void write_raw(Values... values) { (append(values), ...); }
};

static_assert(rohit::type_check::input_buffer<borrowed_stream<const std::uint8_t>>);
static_assert(rohit::type_check::output_buffer<borrowed_stream<std::uint8_t>>);

// Prove this fixture exercises ordinary generation rather than the separate opt-in byte-image API.
template <typename Value>
concept opts_into_constant_output = requires { typename Value::serializer_constant_evaluation_type; };
static_assert(!opts_into_constant_output<models::packet>);

constexpr auto positional_golden = std::to_array<std::uint8_t>({
    0xab, 0xfe, 0xff, 0x78, 0x56, 0x34, 0x12, 0, 0, 0x80, 0x3f, 1,
    2, 0x34, 0x12, 0xcd, 0xab, 0x40, 0x40, 0xac, 2, 1, 0x77, 0x88, 1});
constexpr auto integer_golden = std::to_array<std::uint8_t>({
    1, 0xab, 2, 0xfe, 0xff, 3, 0x78, 0x56, 0x34, 0x12, 4, 0, 0, 0x80, 0x3f, 5, 1,
    6, 2, 0x34, 0x12, 0xcd, 0xab, 7, 0x40, 0x40, 8, 0xac, 2, 9, 1,
    10, 1, 0x77, 0x88, 2, 1, 0, 0});
constexpr auto positional_big_golden = std::to_array<std::uint8_t>({
    0xab, 0xff, 0xfe, 0x12, 0x34, 0x56, 0x78, 0x3f, 0x80, 0, 0, 1,
    2, 0x12, 0x34, 0xab, 0xcd, 0x40, 0x40, 0xac, 2, 1, 0x88, 0x77, 1});
constexpr auto integer_big_golden = std::to_array<std::uint8_t>({
    1, 0xab, 2, 0xff, 0xfe, 3, 0x12, 0x34, 0x56, 0x78, 4, 0x3f, 0x80, 0, 0, 5, 1,
    6, 2, 0x12, 0x34, 0xab, 0xcd, 7, 0x40, 0x40, 8, 0xac, 2, 9, 1,
    10, 1, 0x88, 0x77, 2, 1, 0, 0});
constexpr auto string_golden = std::to_array<std::uint8_t>({
    6, 'm', 'a', 'r', 'k', 'e', 'r', 0xab,
    5, 'd', 'e', 'l', 't', 'a', 0xfe, 0xff,
    8, 's', 'e', 'q', 'u', 'e', 'n', 'c', 'e', 0x78, 0x56, 0x34, 0x12,
    5, 'r', 'a', 't', 'i', 'o', 0, 0, 0x80, 0x3f,
    7, 'e', 'n', 'a', 'b', 'l', 'e', 'd', 1,
    7, 's', 'a', 'm', 'p', 'l', 'e', 's', 2, 0x34, 0x12, 0xcd, 0xab,
    6, 'p', 'r', 'e', 'f', 'i', 'x', 0x40, 0x40,
    8, 'v', 'a', 'r', 'i', 'a', 'b', 'l', 'e', 0xac, 2,
    5, 's', 't', 'a', 't', 'e', 6, 'a', 'c', 't', 'i', 'v', 'e',
    6, 'n', 'e', 's', 't', 'e', 'd',
    5, 'v', 'a', 'l', 'u', 'e', 0x77, 0x88,
    7, 'e', 'n', 'a', 'b', 'l', 'e', 'd', 1, 0, 0});

// Construct values with distinguishable fixed-width bytes and nontrivial compact prefixes.
[[nodiscard]] constexpr models::packet sample() noexcept {
  models::packet value{};
  value.marker = 0xab;
  value.delta = -2;
  value.sequence = 0x12345678;
  value.ratio = 1.0f;
  value.enabled = true;
  value.samples = {0x1234, 0xabcd};
  value.prefix = 64;
  value.variable = 300;
  value.state = models::phase::active;
  value.nested.value = 0x8877;
  value.nested.enabled = true;
  return value;
}

// Compare every generated field without depending on separately configured comparison generation.
[[nodiscard]] constexpr bool matches_sample(const models::packet& value) noexcept {
  return value.marker == 0xab && value.delta == -2 && value.sequence == 0x12345678 &&
      value.ratio == 1.0f && value.enabled &&
      value.samples == std::array<std::uint16_t, 2>{0x1234, 0xabcd} &&
      value.prefix == 64 && value.variable == 300 && value.state == models::phase::active &&
      value.nested.value == 0x8877 && value.nested.enabled;
}

// Execute the ordinary generated member through a real native protocol and caller-owned bytes.
template <keys Keys, std::size_t Size, std::endian Endian = std::endian::little>
[[nodiscard]] constexpr auto encode(const models::packet& value) {
  std::array<std::uint8_t, Size> bytes{};
  borrowed_stream<std::uint8_t> stream{bytes};
  codec::binary<codec::serialize_type::out, Keys, Endian,
      borrowed_stream<std::uint8_t>> encoder{stream};
  value.serialize_out(encoder);
  if (stream.position_bytes() != bytes.size()) {
    throw std::logic_error{"Unexpected generated output extent"};
  }
  return bytes;
}

// Decode directly into the generated owner, sharing native resource accounting through nested values.
template <keys Keys, std::endian Endian = std::endian::little, std::size_t Size>
[[nodiscard]] constexpr models::packet decode(const std::array<std::uint8_t, Size>& bytes,
    codec::decode_limits limits = {}) {
  borrowed_stream<const std::uint8_t> stream{bytes};
  codec::binary<codec::serialize_type::in, Keys, Endian,
      borrowed_stream<const std::uint8_t>> decoder{stream, limits};
  models::packet value{};
  value.serialize_in(decoder);
  decoder.finish();
  return value;
}

static_assert(encode<keys::none, positional_golden.size()>(sample()) == positional_golden);
static_assert(encode<keys::integer, integer_golden.size()>(sample()) == integer_golden);
static_assert(matches_sample(decode<keys::none>(positional_golden)));
static_assert(matches_sample(decode<keys::integer>(integer_golden)));
static_assert(encode<keys::none, positional_big_golden.size(), std::endian::big>(sample()) ==
    positional_big_golden);
static_assert(encode<keys::integer, integer_big_golden.size(), std::endian::big>(sample()) ==
    integer_big_golden);
static_assert(matches_sample(decode<keys::none, std::endian::big>(positional_big_golden)));
static_assert(matches_sample(decode<keys::integer, std::endian::big>(integer_big_golden)));
static_assert(codec::binary_size<keys::none>(sample()) == positional_golden.size());
static_assert(codec::binary_integer_size(sample(), integer_golden.size()) == integer_golden.size());

// Exercise both generated stream-wrapper forms through the runtime protocol rebinding contract.
[[nodiscard]] constexpr bool qualify_generated_stream_wrappers() {
  const auto value = sample();
  std::array<std::uint8_t, positional_golden.size()> bytes{};
  borrowed_stream<std::uint8_t> output{bytes};
  models::packet::serialize<codec::binary_none>(output, value);
  if (bytes != positional_golden || !output.full()) { return false; }
  borrowed_stream<const std::uint8_t> input{bytes};
  const auto decoded = models::packet::deserialize<codec::binary_none>(input);
  if (!matches_sample(decoded) || !input.full()) { return false; }
  bytes = {};
  borrowed_stream<std::uint8_t> member_output{bytes};
  value.serialize_out<codec::binary_none>(member_output);
  if (bytes != positional_golden || !member_output.full()) { return false; }
  borrowed_stream<const std::uint8_t> member_input{bytes};
  models::packet member_decoded{};
  member_decoded.serialize_in<codec::binary_none>(member_input, codec::decode_limits{});
  return matches_sample(member_decoded) && member_input.full();
}
static_assert(qualify_generated_stream_wrappers());

// Verify the same generated/native calls produce independent golden bytes at runtime.
TEST(constexpr_codecs, default_generated_native_round_trip) {
  EXPECT_EQ((encode<keys::none, positional_golden.size()>(sample())), positional_golden);
  EXPECT_EQ((encode<keys::integer, integer_golden.size()>(sample())), integer_golden);
  EXPECT_EQ((encode<keys::string, string_golden.size()>(sample())), string_golden);
  EXPECT_TRUE(matches_sample(decode<keys::none>(positional_golden)));
  EXPECT_TRUE(matches_sample(decode<keys::integer>(integer_golden)));
  EXPECT_TRUE(matches_sample(decode<keys::string>(string_golden)));
  EXPECT_TRUE(qualify_generated_stream_wrappers());
  EXPECT_EQ((encode<keys::none, positional_big_golden.size(), std::endian::big>(sample())),
      positional_big_golden);
  EXPECT_EQ((encode<keys::integer, integer_big_golden.size(), std::endian::big>(sample())),
      integer_big_golden);
  EXPECT_TRUE((matches_sample(decode<keys::none, std::endian::big>(positional_big_golden))));
  EXPECT_TRUE((matches_sample(decode<keys::integer, std::endian::big>(integer_big_golden))));
  EXPECT_EQ(codec::binary_size<keys::none>(sample()), positional_golden.size());
  EXPECT_EQ(codec::binary_integer_size(sample()), integer_golden.size());
  EXPECT_EQ(codec::binary_string_size(sample()), string_golden.size());
}

// Keep checked input/output limits and exact-message validation active in ordinary runtime calls.
TEST(constexpr_codecs, checked_native_failures) {
  EXPECT_THROW((static_cast<void>(encode<keys::none, positional_golden.size() - 1>(sample()))),
      rohit::exception::stream_overflow_exception);
  EXPECT_THROW(static_cast<void>(codec::binary_integer_size(sample(), integer_golden.size() - 1)),
      rohit::exception::stream_overflow_exception);
  codec::decode_limits limits{};
  limits.max_input_bytes = positional_golden.size() - 1;
  EXPECT_THROW(static_cast<void>(decode<keys::none>(positional_golden, limits)),
      codec::exception::resource_limit);
  limits = {};
  limits.max_collection_elements = 1;
  EXPECT_THROW(static_cast<void>(decode<keys::none>(positional_golden, limits)),
      codec::exception::resource_limit);
  limits = {};
  limits.max_nesting_depth = 1;
  EXPECT_THROW(static_cast<void>(decode<keys::none>(positional_golden, limits)),
      codec::exception::resource_limit);
  limits = {};
  limits.max_allocation_bytes = 1;
  EXPECT_THROW(static_cast<void>(decode<keys::none>(positional_golden, limits)),
      codec::exception::resource_limit);
  limits = {};
  limits.max_work_units = 0;
  EXPECT_THROW(static_cast<void>(decode<keys::none>(positional_golden, limits)),
      codec::exception::resource_limit);
  std::array<std::uint8_t, positional_golden.size() + 1> trailing{};
  std::copy(positional_golden.begin(), positional_golden.end(), trailing.begin());
  EXPECT_THROW(static_cast<void>(decode<keys::none>(trailing)), codec::exception::bad_input_data);
}

} // namespace
