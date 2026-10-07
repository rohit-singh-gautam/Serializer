#include <typed_magic.hpp>
#include <rohit/serializer.hpp>
#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <type_traits>

static_assert(std::is_same_v<decltype(typed_magic_test::uint32_record::magic), const std::uint32_t>);
static_assert(std::is_same_v<decltype(typed_magic_test::enum_record::magic), const typed_magic_test::kind>);
static_assert(sizeof(typed_magic_test::uint32_record) == sizeof(std::uint32_t));

namespace {
// Encode a generated value through its complete native message API.
template <template <rohit::serializer::serialize_type> class Protocol, typename Value>
std::string encode(const Value& value) {
  rohit::string_stream output;
  rohit::serializer::serialize_to<Protocol>(output, value);
  return std::move(output).str();
}

// Publish only a fully validated, freshly decoded value.
template <template <rohit::serializer::serialize_type> class Protocol, typename Value>
Value decode(std::string_view bytes) {
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  return rohit::serializer::deserialize_exact<Value, Protocol>(input);
}

// Require matching typed prefixes and reject truncation before any mutable payload is decoded.
template <template <rohit::serializer::serialize_type> class Protocol, typename Value>
void verify_binary(std::size_t header_bytes) {
  Value value{};
  value.value = 31;
  const auto bytes = encode<Protocol>(value);
  EXPECT_EQ((decode<Protocol, Value>(bytes)).value, value.value);
  auto bad = bytes;
  bad.front() ^= 1;
  EXPECT_THROW((decode<Protocol, Value>(bad)), std::exception);
  for (std::size_t length = 0; length < header_bytes; ++length) {
    EXPECT_THROW((decode<Protocol, Value>(bytes.substr(0, length))), std::exception);
  }
  EXPECT_THROW((decode<Protocol, Value>(bytes + '\0')), std::exception);
}

// Exercise each scalar's boundary constant in every native codec.
template <typename Value>
void verify_scalar() {
  verify_binary<rohit::serializer::binary_none, Value>(sizeof(Value::magic));
  verify_binary<rohit::serializer::binary_integer, Value>(sizeof(Value::magic));
  verify_binary<rohit::serializer::binary_string, Value>(sizeof(Value::magic));
  EXPECT_EQ((decode<rohit::serializer::json, Value>(encode<rohit::serializer::json>(Value{}))).value, 7);
}

template <rohit::serializer::serialize_type Direction>
using big_endian = rohit::serializer::binary<Direction, rohit::serializer::serialize_key_type::none, std::endian::big>;

template <rohit::serializer::serialize_type Direction>
using flexible_json = rohit::serializer::json<Direction, rohit::stream, rohit::serializer::read_policy::flexible>;
} // namespace

// Validate both signed limits, unsigned limits, finite fractions, booleans, and character magic.
TEST(typed_magic, scalar_constants_round_trip_and_reject_incorrect_headers) {
  verify_scalar<typed_magic_test::bool_record>();
  verify_scalar<typed_magic_test::char_record>();
  verify_scalar<typed_magic_test::int8_record>();
  verify_scalar<typed_magic_test::uint8_record>();
  verify_scalar<typed_magic_test::int16_record>();
  verify_scalar<typed_magic_test::uint16_record>();
  verify_scalar<typed_magic_test::int32_record>();
  verify_scalar<typed_magic_test::uint32_record>();
  verify_scalar<typed_magic_test::int64_record>();
  verify_scalar<typed_magic_test::uint64_record>();
  verify_scalar<typed_magic_test::float_record>();
  verify_scalar<typed_magic_test::double_record>();
}

// Pin integer byte order independently of generated code and the host platform.
TEST(typed_magic, scalar_prefix_respects_selected_wire_endian) {
  const typed_magic_test::uint32_record value{};
  EXPECT_EQ(encode<rohit::serializer::binary_none>(value).substr(0, sizeof(value.magic)), "LRES");
  EXPECT_EQ(encode<big_endian>(value).substr(0, sizeof(value.magic)), "SERL");
  EXPECT_EQ((decode<big_endian, typed_magic_test::uint32_record>(encode<big_endian>(value))).value, 7);
}

// Preserve each codec's existing enum encoding while retaining the declared enum C++ type.
TEST(typed_magic, enum_headers_use_names_in_string_codecs_and_ordinals_otherwise) {
  verify_binary<rohit::serializer::binary_none, typed_magic_test::enum_record>(1);
  verify_binary<rohit::serializer::binary_integer, typed_magic_test::enum_record>(1);
  verify_binary<rohit::serializer::binary_string, typed_magic_test::enum_record>(7);
  EXPECT_EQ(encode<rohit::serializer::json>(typed_magic_test::enum_record{}), "{\"magic\":\"second\",\"value\":7}");
  EXPECT_EQ(encode<rohit::serializer::binary_string>(typed_magic_test::enum_record{}).substr(0, 7), std::string("\x06second", 7));
}

// Require one typed JSON value in any key order, including when the constant is false.
TEST(typed_magic, json_checks_presence_value_type_duplicates_and_order) {
  using value_type = typed_magic_test::bool_record;
  EXPECT_EQ((decode<rohit::serializer::json, value_type>("{\"value\":31,\"magic\":false}")).value, 31);
  for (const auto* text : {"{\"value\":31}", "{\"magic\":true}", "{\"magic\":0}", "{\"magic\":null}",
                           "{\"magic\":false,\"magic\":false}"}) {
    EXPECT_THROW((decode<rohit::serializer::json, value_type>(text)), std::exception);
  }
  EXPECT_THROW((decode<rohit::serializer::json, typed_magic_test::enum_record>("{\"magic\":\"first\"}")), std::exception);
}

// Keep typed omissions and generic metadata consistent with legacy fixed byte headers.
TEST(typed_magic, omission_and_generic_instances_preserve_header_contracts) {
  using value_type = typed_magic_test::omitted_record;
  EXPECT_EQ(encode<rohit::serializer::json>(value_type{}), "{\"value\":0}");
  EXPECT_THROW((decode<flexible_json, value_type>("{\"magic\":42,\"value\":0}")), std::exception);
  const typed_magic_test::concrete_record value{};
  EXPECT_EQ((decode<rohit::serializer::json, typed_magic_test::concrete_record>(encode<rohit::serializer::json>(value))).value, 0);
  EXPECT_EQ(encode<rohit::serializer::binary_none>(value).substr(0, 4), std::string("\x2a\0\0\0", 4));
}
