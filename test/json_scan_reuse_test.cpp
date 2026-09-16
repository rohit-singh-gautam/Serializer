#include <rohit/json_text.hpp>
#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;
namespace text = codec::detail;
using json_input = codec::json<codec::serialize_type::in>;

// Borrow only the bytes committed by an output operation.
std::string_view written(const rohit::full_stream& output) {
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Produce the established lowercase-hex escaping independently of the production scanners.
std::string quote_reference(std::string_view value) {
  constexpr std::string_view hex_digits = "0123456789abcdef";
  std::string result{"\""};
  for (const unsigned char byte : value) {
    if (byte < 0x20) {
      result += "\\u00";
      result += hex_digits[byte >> 4];
      result += hex_digits[byte & 0x0f];
    } else {
      if (byte == '"' || byte == '\\') {
        result += '\\';
      }
      result += static_cast<char>(byte);
    }
  }
  result += '"';
  return result;
}

// Exercise the same protected borrowing path used for generated field names and enum spellings.
class key_reader : public json_input {
public:
  using json_input::json_input;
  using json_input::read_string;
};

// Count output-policy calls to ensure validation still precedes every string reservation.
class counting_stream : public rohit::full_stream_auto_alloc {
public:
  std::size_t reservations{};
  // Record and forward requests even when there is enough spare capacity.
  void reserve(std::size_t bytes) override {
    ++reservations;
    rohit::full_stream_auto_alloc::reserve(bytes);
  }
};

// Prove that the optimized copy remains subject to a custom stream's reservation policy.
class rejecting_stream : public rohit::full_stream_auto_alloc {
public:
  // Reject without moving the cursor or touching the destination bytes.
  void reserve(std::size_t) override {
    throw rohit::exception::stream_overflow_exception{};
  }
};

// Decode from an exact-size allocation, preserving reusable destination capacity and helper callers.
void check_input(std::string_view wire, std::string_view expected) {
  for (const std::size_t offset : {0u, 1u, 7u}) {
    auto storage = std::make_unique<std::uint8_t[]>(offset + wire.size());
    std::memcpy(storage.get() + offset, wire.data(), wire.size());
    const auto input = rohit::make_constant_full_stream(storage.get() + offset, wire.size());
    json_input decoder{input};
    std::string decoded{"previous value"};
    decoded.reserve(expected.size() + decoded.size());
    const auto capacity = decoded.capacity();
    decoder.serialize_in(decoded);
    decoder.finish();
    EXPECT_EQ(decoded, expected);
    EXPECT_EQ(decoded.capacity(), capacity);
    EXPECT_EQ(input.current_offset(), wire.size());

    const auto bytes = std::span{storage.get() + offset, wire.size()};
    const auto range = text::scan_json_string(bytes);
    std::string legacy;
    text::assign_json_string(legacy, bytes, {range.wire_bytes, range.text_bytes, range.escaped});
    EXPECT_EQ(legacy, expected);
  }
}

// Compare exact reserved output and both helper interfaces with independent expected wire bytes.
void check_output(std::string_view value) {
  const auto expected = quote_reference(value);
  auto storage = std::make_unique<std::uint8_t[]>(expected.size());
  rohit::full_stream output{storage.get(), expected.size()};
  codec::json_out<false> encoder{output};
  encoder.serialize_out(value);
  EXPECT_EQ(written(output), expected);
  EXPECT_EQ(text::escape_json_string(value, 0), expected.substr(1, expected.size() - 2));
  std::string legacy(expected.size() - 2, '\0');
  text::write_json_escaped(reinterpret_cast<std::uint8_t*>(legacy.data()),
                           reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  EXPECT_EQ(legacy, expected.substr(1, expected.size() - 2));
}
} // namespace

// Cover empty/plain UTF-8 and escape intervals at scalar/SIMD boundaries with long plain tails.
TEST(json_scan_reuse, input_plain_spans_and_escape_intervals) {
  struct sample {
    std::string_view wire;
    std::string_view decoded;
  };
  const std::array samples{sample{"", ""},
                           sample{"ascii", "ascii"},
                           sample{"\xc2\xa3\xe2\x82\xac", "\xc2\xa3\xe2\x82\xac"},
                           sample{R"(\n)", "\n"},
                           sample{R"(\u0000)", std::string_view{"\0", 1}},
                           sample{R"(\u20ac)", "\xe2\x82\xac"},
                           sample{R"(\uD83D\uDE00)", "\xf0\x9f\x98\x80"},
                           sample{R"(\b\f\n\r\t\"\\\/)", "\b\f\n\r\t\"\\/"},
                           sample{R"(\nmid\u0000end\t)", std::string_view{"\nmid\0end\t", 9}}};
  for (const std::size_t length : {0u, 1u, 7u, 8u, 15u, 16u, 31u, 32u, 63u, 64u, 65u, 127u}) {
    const std::string prefix(length, 'a');
    const std::string suffix(length, 'z');
    for (const auto& item : samples) {
      const auto wire = "\"" + prefix + std::string{item.wire} + suffix + "\"";
      check_input(wire, prefix + std::string{item.decoded} + suffix);
    }
    // UTF-8 adjacent to both sides of the escape interval must be copied without alteration.
    check_input("\"" + prefix + "\xc2\xa3\\n\xe2\x82\xac" + suffix + "\"",
                prefix + "\xc2\xa3\n\xe2\x82\xac" + suffix);
  }
}

// Reject every incomplete string before changing its destination or consuming the quoted token.
TEST(json_scan_reuse, input_failures_and_resource_limits) {
  const std::string decoded = std::string(33, 'a') + "\n\xe2\x82\xac" + std::string(65, 'z');
  const auto wire = quote_reference(decoded);
  for (std::size_t size = 0; size < wire.size(); ++size) {
    const auto input = rohit::make_constant_full_stream(wire.data(), size);
    json_input decoder{input};
    std::string destination{"unchanged"};
    EXPECT_THROW(decoder.serialize_in(destination), codec::exception::bad_input_data) << size;
    EXPECT_EQ(destination, "unchanged");
    EXPECT_EQ(input.current_offset(), 0u);
  }
  for (std::size_t budget = 0; budget <= wire.size() + 1; ++budget) {
    for (const bool work_budget : {false, true}) {
      codec::decode_limits limits{};
      if (work_budget) {
        limits.max_work_units = budget;
      } else {
        limits.max_input_bytes = budget;
      }
      const auto input = rohit::make_constant_full_stream(wire.data(), wire.size());
      json_input decoder{input, limits};
      std::string destination{"unchanged"};
      const auto needed = wire.size() + (work_budget ? 1u : 0u);
      if (budget < needed) {
        EXPECT_THROW(decoder.serialize_in(destination), codec::exception::resource_limit);
        EXPECT_EQ(destination, "unchanged");
        EXPECT_EQ(input.current_offset(), 0u);
      } else {
        decoder.serialize_in(destination);
        decoder.finish();
        EXPECT_EQ(destination, decoded);
        EXPECT_EQ(input.current_offset(), wire.size());
      }
    }
  }
  for (const bool allocation_budget : {false, true}) {
    codec::decode_limits limits{};
    if (allocation_budget) {
      limits.max_allocation_bytes = decoded.size() - 1;
    } else {
      limits.max_string_bytes = decoded.size() - 1;
    }
    const auto input = rohit::make_constant_full_stream(wire.data(), wire.size());
    json_input decoder{input, limits};
    std::string destination{"unchanged"};
    destination.reserve(decoded.size());
    EXPECT_THROW(decoder.serialize_in(destination), codec::exception::resource_limit);
    EXPECT_EQ(destination, "unchanged");
    EXPECT_EQ(input.current_offset(), 0u);
  }
}

// Reusing scan results must not remove cumulative byte, value-work, or allocation charges.
TEST(json_scan_reuse, cumulative_string_budgets) {
  const std::string value = "prefix\nsuffix";
  const auto wire = quote_reference(value);
  const auto sequence = wire + wire;
  enum class budget_kind { input, work, allocation };
  for (const auto budget : {budget_kind::input, budget_kind::work, budget_kind::allocation}) {
    codec::decode_limits limits{};
    if (budget == budget_kind::input) {
      limits.max_input_bytes = 2 * wire.size() - 1;
    } else if (budget == budget_kind::work) {
      limits.max_work_units = 2 * (wire.size() + 1) - 1;
    } else {
      limits.max_allocation_bytes = 2 * value.size() - 1;
    }
    const auto input = rohit::make_constant_full_stream(sequence.data(), sequence.size());
    json_input decoder{input, limits};
    std::string first;
    std::string second{"unchanged"};
    decoder.serialize_in(first);
    EXPECT_EQ(first, value);
    EXPECT_THROW(decoder.serialize_in(second), codec::exception::resource_limit);
    EXPECT_EQ(second, "unchanged");
    EXPECT_EQ(input.current_offset(), wire.size());
  }
}

// Validate the complete suffix even after all escapes have ended; preserve earlier whitespace reads.
TEST(json_scan_reuse, malformed_suffixes_are_still_validated) {
  for (const auto suffix :
       {std::string_view{"\xc0\xaf"}, std::string_view{"\xed\xa0\x80"},
        std::string_view{"\xf4\x90\x80\x80"}, std::string_view{"\xe2\x82"}, std::string_view{"\n"},
        std::string_view{R"(\q)"}, std::string_view{R"(\uD800)"}, std::string_view{R"(\uDC00)"}}) {
    const auto wire = " \"prefix\\n" + std::string(65, 'a') + std::string{suffix} + "\"";
    const auto input = rohit::make_constant_full_stream(wire.data(), wire.size());
    json_input decoder{input};
    std::string destination{"unchanged"};
    EXPECT_THROW(decoder.serialize_in(destination), codec::exception::bad_input_data);
    EXPECT_EQ(destination, "unchanged");
    EXPECT_EQ(input.current_offset(), 1u);
  }
}

// Borrow plain names without allocation charges and decode escaped names into reusable scratch.
TEST(json_scan_reuse, borrowed_and_escaped_keys) {
  const std::string plain = "\"" + std::string(65, 'a') + "\xc2\xa3\"";
  const auto input = rohit::make_constant_full_stream(plain.data(), plain.size());
  codec::decode_limits limits{};
  limits.max_allocation_bytes = 0;
  key_reader reader{input, limits};
  std::string scratch{"unchanged"};
  const auto key = reader.read_string(scratch, true);
  EXPECT_EQ(key.data(), plain.data() + 1);
  EXPECT_EQ(scratch, "unchanged");
  reader.finish();

  const std::string escaped = "\"" + std::string(65, 'a') + "\\u006d" + std::string(33, 'z') + "\"";
  const auto escaped_input = rohit::make_constant_full_stream(escaped.data(), escaped.size());
  key_reader rejected{escaped_input, limits};
  EXPECT_THROW(rejected.read_string(scratch, true), codec::exception::resource_limit);
  EXPECT_EQ(scratch, "unchanged");
  EXPECT_EQ(escaped_input.current_offset(), 0u);
  key_reader accepted{escaped_input};
  const auto decoded_key = accepted.read_string(scratch, true);
  EXPECT_EQ(decoded_key.data(), scratch.data());
  EXPECT_EQ(decoded_key, std::string(65, 'a') + "m" + std::string(33, 'z'));
  accepted.finish();
}

// Reusing scan offsets must preserve exact escaping with zero, one, adjacent, and separated escapes.
TEST(json_scan_reuse, output_bytes_and_aliases) {
  std::string controls;
  for (int value = 0; value < 0x20; ++value) {
    controls.push_back(static_cast<char>(value));
  }
  for (const std::size_t length : {0u, 1u, 7u, 8u, 15u, 16u, 31u, 32u, 63u, 64u, 65u, 127u}) {
    for (const std::string_view middle :
         {std::string_view{}, std::string_view{"\n"}, std::string_view{"\"\\"},
          std::string_view{"\nmid\t"}, std::string_view{controls}}) {
      check_output(std::string(length, 'a') + std::string{middle} + std::string(length, 'z'));
    }
    check_output(std::string(length, 'a') + "\xc2\xa3\n\xe2\x82\xac" + std::string(length, 'z'));
  }
  const std::string value = std::string(65, 'a') + "\xc2\xa3\n" + std::string(65, 'z');
  rohit::full_stream_auto_alloc growing{value.size()};
  growing.append(value);
  codec::json_out<false> append{growing};
  append.serialize_out(
      std::string_view{reinterpret_cast<const char*>(growing.begin()), value.size()});
  EXPECT_EQ(written(growing), value + quote_reference(value));
  std::array<std::uint8_t, 256> storage{};
  std::memcpy(storage.data() + 1, value.data(), value.size());
  rohit::full_stream overlapping{storage.data(), storage.size()};
  codec::json_out<false> replace{overlapping};
  replace.serialize_out(
      std::string_view{reinterpret_cast<const char*>(storage.data() + 1), value.size()});
  EXPECT_EQ(written(overlapping), quote_reference(value));
}

// Invalid text and failed reservations retain all earlier bytes and never expose a partial string.
TEST(json_scan_reuse, output_validation_and_reservation_failure) {
  for (const auto invalid : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82"}) {
    const auto value = std::string(65, 'a') + "\n" + std::string(65, 'z') + invalid;
    counting_stream output{};
    output.append('x');
    codec::json_out<false> encoder{output};
    const auto reservations = output.reservations;
    EXPECT_THROW(encoder.serialize_out(value), std::invalid_argument);
    EXPECT_EQ(output.reservations, reservations);
    EXPECT_EQ(written(output), "x");
  }
  const std::string value = "prefix\nsuffix";
  const auto expected = quote_reference(value);
  for (std::size_t capacity = 0; capacity < expected.size(); ++capacity) {
    std::array<std::uint8_t, 32> storage{};
    storage.fill(0xcc);
    rohit::full_stream output{storage.data(), capacity + 1};
    output.append('x');
    codec::json_out<false> encoder{output};
    EXPECT_THROW(encoder.serialize_out(value), rohit::exception::stream_overflow_exception);
    EXPECT_EQ(written(output), "x");
    EXPECT_TRUE(
        std::all_of(storage.begin() + 1, storage.end(), [](auto byte) { return byte == 0xcc; }));
  }
  rejecting_stream output{};
  codec::json_out<false> encoder{output};
  EXPECT_THROW(encoder.serialize_out(value), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(output.current_offset(), 0u);
}
