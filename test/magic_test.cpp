#include <magic.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

static_assert(sizeof(magic_test::record::magic) == 8);
static_assert(std::is_same_v<decltype(magic_test::record::magic), const char[8]>);

namespace {
template <rohit::serializer::serialize_type Direction>
using flexible_json = rohit::serializer::json<Direction, rohit::stream,
                                              rohit::serializer::read_policy::flexible>;

// Encode one generated record with exact owning output storage.
template <template <rohit::serializer::serialize_type> class Protocol, typename Value>
std::string encode(const Value& value) {
  rohit::string_stream output;
  rohit::serializer::serialize_to<Protocol>(output, value);
  return std::move(output).str();
}

// Decode a complete message and expose no candidate until final input validation succeeds.
template <template <rohit::serializer::serialize_type> class Protocol, typename Value>
Value decode(std::string_view bytes) {
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  return rohit::serializer::deserialize_exact<Value, Protocol>(input);
}

// Verify all native binary modes prefix exact bytes and omit schema-selected fields.
template <template <rohit::serializer::serialize_type> class Protocol>
void verify_binary() {
  magic_test::record value{};
  value.kept = 42;
  value.human = "JSON only";
  value.diagnostic = 99;
  const auto bytes = encode<Protocol>(value);
  EXPECT_EQ(bytes.substr(0, sizeof(value.magic)), std::string(value.magic, sizeof(value.magic)));
  const auto copy = decode<Protocol, magic_test::record>(bytes);
  EXPECT_EQ(copy.kept, value.kept);
  EXPECT_EQ(copy.human, "default");
  EXPECT_EQ(copy.diagnostic, value.diagnostic);
  auto bad = bytes;
  bad[0] ^= 1;
  EXPECT_THROW((decode<Protocol, magic_test::record>(bad)), std::exception);
  for (std::size_t length = 0; length < sizeof(value.magic); ++length) {
    EXPECT_THROW((decode<Protocol, magic_test::record>(bytes.substr(0, length))), std::exception);
  }
  EXPECT_THROW((decode<Protocol, magic_test::record>(bytes + '\0')), std::exception);
}
} // namespace

// Preserve exact static char-array headers and prove all native binary readers verify them.
TEST(magic_header, native_binary_prefix_and_format_omissions) {
  verify_binary<rohit::serializer::binary_none>();
  verify_binary<rohit::serializer::binary_integer>();
  verify_binary<rohit::serializer::binary_string>();
  static_assert(std::is_same_v<rohit::serializer::binary_positional<rohit::serializer::serialize_type::out>,
                               rohit::serializer::binary_none<rohit::serializer::serialize_type::out>>);
}

// Require and discard the JSON fixed field while preserving defaults for omitted payload fields.
TEST(magic_header, json_requires_checked_fixed_identity) {
  magic_test::record value{};
  value.human = "human value";
  value.diagnostic = 99;
  const auto bytes = encode<rohit::serializer::json>(value);
  EXPECT_NE(bytes.find("\"magic\":\"SRL\\u0000UTF8\""), std::string::npos);
  EXPECT_EQ(bytes.find("diagnostic"), std::string::npos);
  const auto copy = decode<rohit::serializer::json, magic_test::record>(bytes);
  EXPECT_EQ(copy.human, value.human);
  EXPECT_EQ(copy.diagnostic, 7);
  EXPECT_THROW((decode<rohit::serializer::json, magic_test::record>("{\"revision\":2}")), std::exception);
  EXPECT_THROW((decode<rohit::serializer::json, magic_test::record>("{\"magic\":\"wrong\",\"revision\":2}")), std::exception);
  EXPECT_THROW((decode<rohit::serializer::json, magic_test::record>("{\"magic\":\"SRL\\u0000UTF8\",\"revision\":2,\"diagnostic\":1}")), std::exception);
}

// Honor explicit header exclusion separately from ordinary field visibility and storage.
TEST(magic_header, omission_excludes_the_header_only_in_named_formats) {
  magic_test::omitted_header value{};
  value.value = 23;
  const auto json = encode<rohit::serializer::json>(value);
  EXPECT_EQ(json, "{\"value\":23}");
  EXPECT_EQ((decode<rohit::serializer::json, magic_test::omitted_header>(json)).value, 23);
  const auto integer = encode<rohit::serializer::binary_integer>(value);
  EXPECT_EQ((decode<rohit::serializer::binary_integer, magic_test::omitted_header>(integer)).value, 23);
  const auto positional = encode<rohit::serializer::binary_positional>(value);
  EXPECT_EQ(positional.substr(0, 8), "OPTIONAL");
}

// Flexible JSON may skip additive payload fields but must reject the known excluded header name.
TEST(magic_header, flexible_json_rejects_excluded_fixed_header) {
  EXPECT_EQ((decode<flexible_json, magic_test::omitted_header>(
      "{\"value\":23,\"additive\":{\"nested\":true}}")).value, 23);
  EXPECT_THROW((decode<flexible_json, magic_test::omitted_header>(
      "{\"value\":23,\"magic\":\"OPTIONAL\"}")), std::exception);
  EXPECT_THROW((decode<flexible_json, magic_test::omitted_header>(
      "{\"magic\":\"wrong\",\"value\":23}")), std::exception);
  const std::string omitted_numeric_header{"\x02\x01\x17\0\0\0\0", 7};
  EXPECT_THROW((decode<rohit::serializer::binary_integer, magic_test::omitted_header>(
      omitted_numeric_header)), std::exception);
}

// Preserve fixed header metadata when a generic contract receives a named instantiation.
TEST(magic_header, named_generic_instantiation_preserves_header_metadata) {
  magic_test::generic_uint32 value{};
  value.value = 31;
  const auto json = encode<rohit::serializer::json>(value);
  EXPECT_NE(json.find("\"magic\":\"GENERIC\""), std::string::npos);
  EXPECT_EQ((decode<rohit::serializer::json, magic_test::generic_uint32>(json)).value, 31);
  EXPECT_THROW((decode<rohit::serializer::json, magic_test::generic_uint32>("{\"value\":31}")), std::exception);
  const auto positional = encode<rohit::serializer::binary_positional>(value);
  EXPECT_EQ(positional.substr(0, 7), "GENERIC");
  EXPECT_EQ((decode<rohit::serializer::binary_positional, magic_test::generic_uint32>(positional)).value, 31);
  const auto integer = encode<rohit::serializer::binary_integer>(value);
  EXPECT_NE(integer.substr(0, 7), "GENERIC");
  EXPECT_EQ((decode<rohit::serializer::binary_integer, magic_test::generic_uint32>(integer)).value, 31);
}
