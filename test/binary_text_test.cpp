#include "stream_test_support.hpp"
#include <rohit/serializer.hpp>
#include <views.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace codec = rohit::serializer;

constexpr std::string_view malformed_text[] = {"\x80",
                                               "\xff",
                                               "\xc0\xaf",
                                               "\xc2",
                                               "\xc2\x20",
                                               "\xe0\x80\x80",
                                               "\xed\xa0\x80",
                                               "\xf0\x80\x80\x80",
                                               "\xf4\x90\x80\x80",
                                               "\xf5\x80\x80\x80"};

template <codec::serialize_type Type>
using unchecked_binary =
    codec::binary_none<Type, rohit::stream, codec::binary_text_validation::unchecked>;

static_assert(codec::binary_none<codec::serialize_type::in>::text_validation ==
              codec::binary_text_validation::strict);
static_assert(unchecked_binary<codec::serialize_type::in>::rebind_stream<
                  stream_test::input_cursor>::text_validation ==
              codec::binary_text_validation::unchecked);
static_assert(unchecked_binary<codec::serialize_type::out>::rebind_stream<
                  stream_test::output_buffer>::text_validation ==
              codec::binary_text_validation::unchecked);

// The opt-out skips only text checks, preserving bytes, bounds, limits, and exact-message checks.
template <codec::serialize_key_type Keys, std::endian Endian>
void check_unchecked_strings() {
  constexpr auto unchecked = codec::binary_text_validation::unchecked;
  using encoder_type =
      codec::binary<codec::serialize_type::out, Keys, Endian, rohit::stream, unchecked>;
  using decoder_type =
      codec::binary<codec::serialize_type::in, Keys, Endian, rohit::stream, unchecked>;
  for (const auto malformed : malformed_text) {
    const auto text = std::string(64, 'x') + std::string{malformed};
    rohit::full_stream_auto_alloc output{};
    encoder_type encoder{output};
    encoder.serialize_out(text);
    encoder.serialize_out(std::string_view{text});
    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    decoder_type decoder{input};
    std::string decoded{};
    decoder.serialize_in(decoded);
    EXPECT_EQ(decoded, text);
    EXPECT_THROW(decoder.finish(), codec::exception::bad_input_data);
    EXPECT_EQ(decoder.serialize_in_name(), text);
    EXPECT_NO_THROW(decoder.finish());

    const auto truncated =
        rohit::make_constant_full_stream(output.begin(), output.current_offset() - 1);
    decoder_type truncated_decoder{truncated};
    truncated_decoder.serialize_in(decoded);
    EXPECT_THROW(truncated_decoder.serialize_in_name(), codec::exception::bad_input_data);
    codec::decode_limits limits{};
    limits.max_string_bytes = text.size() - 1;
    const auto limited = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    decoder_type limited_decoder{limited, limits};
    EXPECT_THROW(limited_decoder.serialize_in(decoded), codec::exception::resource_limit);
  }
  for (const auto& text :
       {std::string{}, std::string{"\0\n\"\\", 4}, std::string(128, 'x') + "\xe4\xb8\x96"}) {
    rohit::full_stream_auto_alloc strict_output{}, unchecked_output{};
    codec::binary<codec::serialize_type::out, Keys, Endian> strict_encoder{strict_output};
    encoder_type unchecked_encoder{unchecked_output};
    strict_encoder.serialize_out(text);
    unchecked_encoder.serialize_out(text);
    ASSERT_EQ(strict_output.current_offset(), unchecked_output.current_offset());
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(strict_output.begin()),
                               strict_output.current_offset()),
              std::string_view(reinterpret_cast<const char*>(unchecked_output.begin()),
                               unchecked_output.current_offset()));
  }
  const std::array<std::uint8_t, 1> invalid_boolean{2};
  const auto input =
      rohit::make_constant_full_stream(invalid_boolean.data(), invalid_boolean.size());
  decoder_type decoder{input};
  bool value{};
  EXPECT_THROW(decoder.serialize_in(value), codec::exception::bad_input_data);
}

// Reject malformed binary text before committing a value, for every key mode and byte order.
template <codec::serialize_key_type Keys, std::endian Endian>
void check_strings() {
  using encoder_type = codec::binary<codec::serialize_type::out, Keys, Endian>;
  using decoder_type = codec::binary<codec::serialize_type::in, Keys, Endian>;
  constexpr std::size_t prefix_lengths[] = {0, 7, 8, 15, 16, 31, 32, 63};
  for (const auto malformed : malformed_text) {
    for (const auto prefix : prefix_lengths) {
      const auto text = std::string(prefix, 'x') + std::string{malformed};
      rohit::full_stream_auto_alloc output{};
      encoder_type encoder{output};
      EXPECT_THROW(encoder.serialize_out(text), std::invalid_argument);
      EXPECT_THROW(encoder.serialize_out(std::string_view{text}), std::invalid_argument);
      EXPECT_EQ(output.current_offset(), 0u);
      encoder.serialize_out_variable(text.size());
      output.append(text);
      const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
      decoder_type decoder{input};
      std::string destination{"unchanged"};
      EXPECT_THROW(decoder.serialize_in(destination), rohit::exception::base_parser);
      EXPECT_EQ(destination, "unchanged");
      const auto borrowed_input =
          rohit::make_constant_full_stream(output.begin(), output.current_offset());
      decoder_type borrowed{borrowed_input};
      EXPECT_THROW(borrowed.serialize_in_name(), rohit::exception::base_parser);
    }
  }

  const std::string valid_text[] = {"",
                                    std::string{"\0\n\"\\", 4},
                                    "\xc2\x80",
                                    "\xe0\xa0\x80",
                                    "\xed\x9f\xbf",
                                    "\xf0\x90\x80\x80",
                                    "\xf4\x8f\xbf\xbf",
                                    "\xef\xbb\xbf",
                                    std::string(128, 'a') + "\xc3\xa9"};
  for (const auto& text : valid_text) {
    rohit::full_stream_auto_alloc output{};
    encoder_type encoder{output};
    encoder.serialize_out(text);
    const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
    decoder_type decoder{input};
    std::string destination{};
    decoder.serialize_in(destination);
    decoder.finish();
    EXPECT_EQ(destination, text);
  }
}
} // namespace

// String validity is independent of numeric wire order and of the enclosing field-key mode.
TEST(binary_text, uniform_utf8_contract) {
  check_strings<codec::serialize_key_type::none, std::endian::little>();
  check_strings<codec::serialize_key_type::integer, std::endian::little>();
  check_strings<codec::serialize_key_type::string, std::endian::little>();
  check_strings<codec::serialize_key_type::none, std::endian::big>();
  check_strings<codec::serialize_key_type::integer, std::endian::big>();
  check_strings<codec::serialize_key_type::string, std::endian::big>();
}

// All wire modes retain their format and non-text validation when runtime UTF-8 checks are disabled.
TEST(binary_text, unchecked_policy) {
  check_unchecked_strings<codec::serialize_key_type::none, std::endian::little>();
  check_unchecked_strings<codec::serialize_key_type::integer, std::endian::little>();
  check_unchecked_strings<codec::serialize_key_type::string, std::endian::little>();
  check_unchecked_strings<codec::serialize_key_type::none, std::endian::big>();
  check_unchecked_strings<codec::serialize_key_type::integer, std::endian::big>();
  check_unchecked_strings<codec::serialize_key_type::string, std::endian::big>();
}

// Nested generated values and both structural and standard stream adapters preserve the policy.
TEST(binary_text, unchecked_generated_streams) {
  using model = view_test::record<codec::storage_mode::owning>;
  model value{};
  value.label = "\xff";
  value.nested.label = "\xc0\xaf";
  value.labels.emplace(7, "\xed\xa0\x80");
  value.children.emplace_back();
  value.children.back().label = "\xf4\x90\x80\x80";
  stream_test::output_buffer output{};
  value.serialize_out<unchecked_binary>(output);
  const auto bytes = output.bytes();
  stream_test::input_cursor input{bytes};
  const auto decoded = codec::deserialize_exact<model, unchecked_binary>(input);
  EXPECT_EQ(decoded.label, value.label);
  EXPECT_EQ(decoded.nested.label, value.nested.label);
  EXPECT_EQ(decoded.labels, value.labels);
  ASSERT_EQ(decoded.children.size(), value.children.size());
  EXPECT_EQ(decoded.children.back().label, value.children.back().label);

  std::stringstream standard{};
  value.serialize_out<unchecked_binary>(standard);
  EXPECT_EQ(standard.str(), bytes);
  const auto restored = codec::deserialize_exact<model, unchecked_binary>(standard);
  EXPECT_EQ(restored.children.back().label, value.children.back().label);

  using view = view_test::record<codec::storage_mode::read_only_view>;
  const auto span = std::span<const std::uint8_t>{
      reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
  EXPECT_THROW(view::map(span), codec::exception::bad_input_data);
  stream_test::output_buffer json_output{};
  EXPECT_THROW(value.serialize_out<codec::json>(json_output), std::invalid_argument);
}

// Dynamic names are unchecked in both batched and ordinary output; constant names stay constexpr.
TEST(binary_text, unchecked_dynamic_names) {
  using encoder_type = codec::binary_string<codec::serialize_type::out, rohit::stream,
                                            codec::binary_text_validation::unchecked>;
  using decoder_type = codec::binary_string<codec::serialize_type::in, rohit::stream,
                                            codec::binary_text_validation::unchecked>;
  rohit::full_stream_auto_alloc output{};
  encoder_type encoder{output};
  const auto first = std::pair{std::string_view{"\xff"}, std::uint16_t{7}};
  const auto second = std::pair{std::string_view{"\xc0\xaf"}, std::uint32_t{9}};
  encoder.struct_serialize_out_fixed(first, second);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  decoder_type decoder{input};
  EXPECT_EQ(decoder.serialize_in_name(), first.first);
  std::uint16_t first_value{};
  decoder.serialize_in(first_value);
  EXPECT_EQ(first_value, first.second);
  EXPECT_EQ(decoder.serialize_in_name(), second.first);
  std::uint32_t second_value{};
  decoder.serialize_in(second_value);
  EXPECT_EQ(second_value, second.second);
  decoder.finish();
}

// Generated views validate initial text and preserve all bytes when a replacement is invalid.
TEST(binary_text, mapped_strings_validate_before_mutation) {
  using model = view_test::child<codec::storage_mode::owning>;
  using view = view_test::child<codec::storage_mode::mutable_view>;
  model value{};
  value.label = "ok";
  rohit::full_stream_auto_alloc output{};
  value.serialize_out<codec::binary_none>(output);
  const auto bytes = std::span<std::uint8_t>{output.begin(), output.current_offset()};
  auto mapped = view::map(bytes);
  const std::vector<std::uint8_t> original{bytes.begin(), bytes.end()};
  EXPECT_THROW(mapped.set_label("\xc0\xaf"), std::invalid_argument);
  EXPECT_EQ(std::vector<std::uint8_t>(bytes.begin(), bytes.end()), original);
  mapped.set_label("\xc3\xa9");
  EXPECT_EQ(mapped.get_label(), "\xc3\xa9");
  bytes.back() = 0xff;
  EXPECT_THROW(view::map(bytes), rohit::exception::base_parser);
}

// Dynamic batched field names must obey the same UTF-8 rules as ordinary field names.
TEST(binary_text, batched_names_reject_invalid_text) {
  rohit::full_stream_auto_alloc output{};
  codec::binary_string<codec::serialize_type::out> encoder{output};
  const auto valid = std::pair{std::string_view{"first"}, std::uint16_t{7}};
  const auto invalid = std::pair{std::string_view{"\xff"}, std::uint32_t{9}};
  EXPECT_THROW(encoder.struct_serialize_out_fixed(valid, invalid), std::invalid_argument);
  EXPECT_EQ(output.current_offset(), 0u);
}

// Arbitrary binary payloads remain available as byte collections, including every byte value.
TEST(binary_text, byte_arrays_are_not_text) {
  const std::vector<std::uint8_t> value{0x00, 0x80, 0xc0, 0xaf, 0xff};
  rohit::full_stream_auto_alloc output{};
  codec::binary_none<codec::serialize_type::out> encoder{output};
  encoder.serialize_out(value);
  const auto input = rohit::make_constant_full_stream(output.begin(), output.current_offset());
  codec::binary_none<codec::serialize_type::in> decoder{input};
  std::vector<std::uint8_t> decoded{};
  decoder.serialize_in(decoded);
  decoder.finish();
  EXPECT_EQ(decoded, value);
}
