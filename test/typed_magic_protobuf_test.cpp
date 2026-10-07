#include <typed_magic.hpp>
#include <rohit/protobuf.hpp>
#include <gtest/gtest.h>

#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace codec = rohit::serializer;

// Encode one generated typed-header message in the selected Protobuf representation.
template <template <codec::serialize_type> class Protocol, typename Value>
std::string encode(const Value& value) {
  rohit::string_stream output;
  codec::serialize_to<Protocol>(output, value);
  return std::move(output).str();
}

// Decode one complete message without publishing a partially validated candidate.
template <template <codec::serialize_type> class Protocol, typename Value>
Value decode(std::string_view bytes) {
  auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
  return codec::deserialize_exact<Value, Protocol>(input);
}

// Cover every typed constant's native Protobuf scalar mapping, including signed/unsigned limits.
template <typename Value>
void verify_scalar() {
  const Value value{};
  EXPECT_EQ((decode<codec::protobuf_binary, Value>(encode<codec::protobuf_binary>(value))).value, 7);
  EXPECT_EQ((decode<codec::protojson, Value>(encode<codec::protojson>(value))).value, 7);
  EXPECT_EQ((decode<codec::textproto, Value>(encode<codec::textproto>(value))).value, 7);
}
} // namespace

// Require matching static magic through Protobuf binary, ProtoJSON, and TextProto.
TEST(typed_magic_protobuf, scalar_and_enum_constants_use_existing_field_mappings) {
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
  verify_scalar<typed_magic_test::enum_record>();
}

// Preserve explicit presence even for false and reject null, absent, duplicate or mismatched magic.
TEST(typed_magic_protobuf, presence_and_value_are_checked_for_default_scalar_constants) {
  using value_type = typed_magic_test::bool_record;
  const auto binary = encode<codec::protobuf_binary>(value_type{});
  EXPECT_EQ(binary.substr(0, 3), std::string("\x98\x06\0", 3));
  for (const auto* text : {"{}", "{\"magic\":null}", "{\"magic\":true}",
                           "{\"magic\":false,\"magic\":false}"}) {
    EXPECT_THROW((decode<codec::protojson, value_type>(text)), std::exception);
  }
  EXPECT_THROW((decode<codec::protobuf_binary, value_type>(std::string("\x08\x07", 2))), std::exception);
  EXPECT_THROW((decode<codec::protobuf_binary, value_type>(binary + binary.substr(0, 3))), std::exception);
  EXPECT_THROW((decode<codec::textproto, value_type>("magic:true")), std::exception);
  EXPECT_THROW((decode<codec::protojson, typed_magic_test::enum_record>("{\"magic\":\"first\"}")), std::exception);
}
