#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>

#include <constant_names.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
namespace codec = rohit::serializer;
using json_output = codec::json<codec::serialize_type::out>;
using named_output = codec::binary_string<codec::serialize_type::out>;

// Borrow only committed output, independent of the stream's spare capacity.
std::string_view written(const rohit::full_stream& output) {
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Exercise the fallback for protocols that implement the established string_view field API.
template <typename Protocol>
class ordinary_names : public Protocol {
public:
  using Protocol::Protocol;
  // Hide the optional hook so generated code passes the original wire name unchanged.
  template <codec::detail::field_name_literal Name>
  static auto encoded_field_name() = delete;
};

// Observe each reservation without bypassing the production stream's growth policy.
class counting_stream : public rohit::full_stream_auto_alloc {
public:
  using rohit::full_stream_auto_alloc::full_stream_auto_alloc;
  std::size_t reservations{};
  // Count the operation before forwarding it, including reservations with spare capacity.
  void reserve(std::size_t bytes) override {
    ++reservations;
    rohit::full_stream_auto_alloc::reserve(bytes);
  }
};

// Reject even a name that would fit, proving encoded names honor virtual reservation policies.
class rejecting_stream : public rohit::full_stream_auto_alloc {
public:
  // Leave bytes and cursor unchanged when the caller requests a reservation.
  void reserve(std::size_t) override {
    throw rohit::exception::stream_overflow_exception{};
  }
};

// Include renamed keys, a scalar batch, enum/union names, inheritance, objects, and dynamic text.
constant_names::record sample() {
  constant_names::record value{};
  value.id = 5;
  value.label = "quote\" slash\\ newline\n";
  value.user_id = 0x1234;
  value.request_count = 0x12345678;
  value.status = constant_names::state::ready_now;
  value.payload.count_value = 9;
  value.tags = {"one", "two"};
  value.totals = {{"requests", 12}};
  value.child.id = 6;
  return value;
}

// Compare encoded and ordinary names, then verify the unchanged reader accepts the complete output.
template <std::endian Endian>
void compare_binary() {
  using output_protocol =
      codec::binary<codec::serialize_type::out, codec::serialize_key_type::string, Endian>;
  const auto value = sample();
  counting_stream output{1};
  output_protocol encoder{output};
  value.serialize_out(encoder);
  rohit::full_stream_auto_alloc reference{};
  ordinary_names<output_protocol> ordinary{reference};
  value.serialize_out(ordinary);
  EXPECT_EQ(written(output), written(reference));
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  codec::binary<codec::serialize_type::in, codec::serialize_key_type::string, Endian> decoder{
      input};
  constant_names::record decoded{};
  decoded.serialize_in(decoder);
  decoder.finish();
  reference.reset();
  decoded.serialize_out(ordinary);
  EXPECT_EQ(written(output), written(reference));
}

// Create a repeated ASCII name at compile time to cover a compact-length width transition.
template <std::size_t Size>
consteval auto repeated_name() {
  char text[Size + 1]{};
  std::fill_n(text, Size, 'x');
  return codec::detail::field_name_literal{text};
}

// Compare independent compact-prefix expectations with both scalar and batch name output.
template <codec::detail::field_name_literal Name>
void check_binary_prefix(std::string_view prefix) {
  constexpr auto token = codec::detail::constant_field_name<named_output, Name>();
  counting_stream output{1};
  named_output encoder{output};
  encoder.struct_serialize_out_fixed(std::make_pair(token, std::uint16_t{0x1234}),
                                     std::make_pair(token, std::uint32_t{0x12345678}));
  EXPECT_EQ(output.reservations, 1u);
  EXPECT_EQ(written(output).substr(0, prefix.size()), prefix);
  rohit::full_stream_auto_alloc reference{};
  named_output ordinary{reference};
  ordinary.struct_serialize_out(std::make_pair(Name.view(), std::uint16_t{0x1234}));
  ordinary.struct_serialize_out(std::make_pair(Name.view(), std::uint32_t{0x12345678}));
  EXPECT_EQ(written(output), written(reference));
}
} // namespace

// Tokens own no per-object bytes, share constant storage, and preserve custom-protocol key types.
TEST(constant_field_name, constant_storage_and_fallback) {
  constexpr auto json_name = codec::detail::constant_field_name<json_output, "userID">();
  constexpr auto binary_name = codec::detail::constant_field_name<named_output, "userID">();
  static_assert(std::is_empty_v<decltype(json_name)>);
  static_assert(std::is_empty_v<decltype(binary_name)>);
  static_assert(std::string_view{json_name.bytes.data(), json_name.bytes.size()} == "\"userID\"");
  static_assert(binary_name.bytes == std::array<std::uint8_t, 7>{6, 'u', 's', 'e', 'r', 'I', 'D'});
  constexpr auto fallback =
      codec::detail::constant_field_name<ordinary_names<json_output>, "userID">();
  static_assert(std::is_same_v<decltype(fallback), const std::string_view>);
  static_assert(fallback == "userID");
  EXPECT_EQ(json_name.bytes.data(),
            (codec::detail::constant_field_name<json_output, "userID">().bytes.data()));
}

// Preserve compact and formatted JSON bytes, including map wrapper names and nested framing.
TEST(constant_field_name, generated_json_formats) {
  for (const auto& format :
       {codec::format::compress, codec::format::beautify, codec::format::beautify_vertical}) {
    const auto value = sample();
    counting_stream output{1};
    json_output encoder{output, format};
    value.serialize_out(encoder);
    rohit::full_stream_auto_alloc reference{};
    ordinary_names<json_output> ordinary{reference, format};
    value.serialize_out(ordinary);
    EXPECT_EQ(written(output), written(reference));
    EXPECT_NE(written(output).find("\"userID\""), std::string_view::npos);
    EXPECT_NE(written(output).find("\"wirePayload:CountValue\""), std::string_view::npos);
    if (!format.space_after_colon) {
      EXPECT_NE(written(output).find("[{\"key\":\"requests\",\"value\":12}]"),
                std::string_view::npos);
    }
    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    codec::json<codec::serialize_type::in> decoder{input};
    constant_names::record decoded{};
    decoded.serialize_in(decoder);
    decoder.finish();
    reference.reset();
    decoded.serialize_out(ordinary);
    EXPECT_EQ(written(output), written(reference));
  }
}

// Binary prefixes keep their own order for both scalar endiannesses; names need one reservation.
TEST(constant_field_name, generated_binary_and_reservations) {
  compare_binary<std::endian::little>();
  compare_binary<std::endian::big>();
  counting_stream output{1};
  named_output encoder{output};
  constant_names::single_name value{};
  value.value = 42;
  value.serialize_out(encoder);
  const auto expected = std::to_array<std::uint8_t>({6, 'u', 's', 'e', 'r', 'I', 'D', 42, 0});
  ASSERT_EQ(output.current_offset(), expected.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), output.begin()));
  EXPECT_EQ(output.reservations, 3u); // Encoded name, value, object terminator.
  counting_stream reference{};
  ordinary_names<named_output> ordinary{reference};
  value.serialize_out(ordinary);
  EXPECT_EQ(written(output), written(reference));
  EXPECT_EQ(reference.reservations, 4u); // Length, name, value, object terminator.
  check_binary_prefix<repeated_name<63>()>("\x3f");
  check_binary_prefix<repeated_name<64>()>("\x40\x40");
}

// Escaped and non-ASCII constants use the ordinary JSON validator; dynamic values still escape.
TEST(constant_field_name, json_escaping_and_validation) {
  constexpr auto escaped = codec::detail::constant_field_name<json_output, "quote\"\\\n">();
  constexpr auto utf8 = codec::detail::constant_field_name<json_output, "\xc3\xa9">();
  static_assert(std::is_same_v<decltype(escaped), const std::string_view>);
  static_assert(std::is_same_v<decltype(utf8), const std::string_view>);
  rohit::full_stream_auto_alloc output{};
  json_output encoder{output};
  encoder.struct_serialize_out_start(std::make_pair(escaped, std::string_view{"\n"}));
  encoder.struct_serialize_out(std::make_pair(utf8, 1));
  encoder.struct_serialize_out_end();
  EXPECT_EQ(written(output), "{\"quote\\\"\\\\\\u000a\":\"\\u000a\",\"\xc3\xa9\":1}");
  output.reset();
  constexpr auto invalid = codec::detail::constant_field_name<json_output, "\xc0">();
  EXPECT_THROW(encoder.serialize_out(invalid), std::invalid_argument);
  EXPECT_EQ(output.current_offset(), 0u);
}

// The prequoted JSON path preserves the same committed prefix at every fixed-buffer boundary.
TEST(constant_field_name, json_buffer_boundaries) {
  constant_names::single_name value{};
  value.value = 42;
  rohit::full_stream_auto_alloc complete{};
  json_output complete_encoder{complete};
  value.serialize_out(complete_encoder);
  EXPECT_EQ(written(complete), "{\"userID\":42}");
  for (std::size_t capacity = 0; capacity <= complete.current_offset(); ++capacity) {
    std::array<std::uint8_t, 32> actual_storage{};
    std::array<std::uint8_t, 32> expected_storage{};
    rohit::full_stream actual{actual_storage.data(), capacity};
    rohit::full_stream expected{expected_storage.data(), capacity};
    json_output encoder{actual};
    ordinary_names<json_output> ordinary{expected};
    bool actual_failure{};
    bool expected_failure{};
    try {
      value.serialize_out(encoder);
    } catch (const rohit::exception::stream_overflow_exception&) {
      actual_failure = true;
    }
    try {
      value.serialize_out(ordinary);
    } catch (const rohit::exception::stream_overflow_exception&) {
      expected_failure = true;
    }
    EXPECT_EQ(actual_failure, expected_failure) << capacity;
    EXPECT_EQ(written(actual), written(expected)) << capacity;
    EXPECT_EQ(actual_storage, expected_storage) << capacity;
  }
  rejecting_stream rejected{};
  json_output rejecting_encoder{rejected};
  constexpr auto token = codec::detail::constant_field_name<json_output, "userID">();
  EXPECT_THROW(rejecting_encoder.serialize_out(token), rohit::exception::stream_overflow_exception);
  EXPECT_EQ(rejected.current_offset(), 0u);
}

// A rejected binary name writes neither its prefix nor its text, while previous output survives.
TEST(constant_field_name, bounded_output_and_custom_policy) {
  constexpr auto token = codec::detail::constant_field_name<named_output, "userID">();
  std::array<std::uint8_t, 16> storage{};
  constexpr auto name_bytes = token.bytes.size();
  for (std::size_t capacity = 0; capacity < name_bytes; ++capacity) {
    storage.fill(0xcc);
    rohit::full_stream output{storage.data(), capacity + 1};
    output.write_raw(std::uint8_t{0xee});
    named_output encoder{output};
    EXPECT_THROW(encoder.struct_serialize_out(std::make_pair(token, std::uint8_t{42})),
                 rohit::exception::stream_overflow_exception);
    EXPECT_EQ(output.current_offset(), 1u);
    EXPECT_EQ(storage.front(), 0xee);
    EXPECT_TRUE(
        std::all_of(storage.begin() + 1, storage.end(), [](auto byte) { return byte == 0xcc; }));
  }
  rohit::full_stream exact_name{storage.data(), name_bytes};
  named_output encoder{exact_name};
  EXPECT_THROW(encoder.struct_serialize_out(std::make_pair(token, std::uint8_t{42})),
               rohit::exception::stream_overflow_exception);
  EXPECT_EQ(exact_name.current_offset(), name_bytes); // The subsequent value fails separately.
  rejecting_stream rejected{};
  named_output rejecting_encoder{rejected};
  EXPECT_THROW(rejecting_encoder.struct_serialize_out(std::make_pair(token, std::uint8_t{42})),
               rohit::exception::stream_overflow_exception);
  EXPECT_EQ(rejected.current_offset(), 0u);
}
