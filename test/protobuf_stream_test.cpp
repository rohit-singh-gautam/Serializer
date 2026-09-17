#include "stream_test_support.hpp"

#include <gtest/gtest.h>
#include <protobuf.hpp>

#include <sstream>
#include <stdexcept>
#include <string>

namespace {
namespace codec = rohit::serializer;

// Check nested and packed Protobuf output through independent buffer implementations.
template <template <codec::serialize_type> class Protocol>
void check_protocol() {
  protobuf_test::record original{};
  original.signed_value = -123;
  original.display_name = "Ada \"Lovelace\"";
  original.numbers = {1, -2, 300};
  original.counts = {{"one", 1}, {"two", 2}};
  original.nested.text = "Nested";
  original.nested.number = 42;
  rohit::full_stream_auto_alloc native;
  original.serialize_out<Protocol>(native);
  const std::string expected{reinterpret_cast<const char*>(native.begin()),
                             native.current_offset()};
  stream_test::output_buffer custom;
  original.serialize_out<Protocol>(custom);
  EXPECT_EQ(custom.bytes(), expected);
  const stream_test::input_cursor input{expected};
  protobuf_test::record decoded{};
  decoded.serialize_in<Protocol>(input);
  EXPECT_TRUE(input.full());
  EXPECT_EQ(decoded.signed_value, original.signed_value);
  EXPECT_EQ(decoded.display_name, original.display_name);
  EXPECT_EQ(decoded.numbers, original.numbers);
  EXPECT_EQ(decoded.counts, original.counts);
  EXPECT_EQ(decoded.nested.text, original.nested.text);

  const stream_test::input_cursor limited_input{expected};
  codec::decode_limits limits;
  limits.max_input_bytes = expected.size();
  decoded.serialize_in<Protocol>(limited_input, limits);
  EXPECT_TRUE(limited_input.full());
  EXPECT_EQ(decoded.display_name, original.display_name);

  std::stringstream io;
  original.serialize_out<Protocol>(io);
  EXPECT_EQ(io.str(), expected);
  decoded = {};
  decoded.serialize_in<Protocol>(io, limits);
  EXPECT_EQ(decoded.numbers, original.numbers);
  EXPECT_EQ(decoded.nested.number, original.nested.number);

  std::stringstream factory_io;
  protobuf_test::record::serialize<Protocol>(factory_io, original);
  EXPECT_EQ(factory_io.str(), expected);
  const auto factory_value = protobuf_test::record::deserialize<Protocol>(factory_io, limits);
  EXPECT_EQ(factory_value.numbers, original.numbers);
  EXPECT_EQ(factory_value.counts, original.counts);
  EXPECT_EQ(factory_value.nested.number, original.nested.number);
  const stream_test::input_cursor factory_input{expected};
  const auto custom_value = protobuf_test::record::deserialize<Protocol>(factory_input);
  stream_test::output_buffer factory_output;
  protobuf_test::record::serialize<Protocol>(factory_output, custom_value);
  EXPECT_EQ(factory_output.bytes(), expected);

  std::istringstream oversized{expected};
  --limits.max_input_bytes;
  EXPECT_THROW(decoded.serialize_in<Protocol>(oversized, limits), std::length_error);
  std::istringstream oversized_factory{expected};
  EXPECT_THROW(static_cast<void>(
                   protobuf_test::record::deserialize<Protocol>(oversized_factory, limits)),
               std::length_error);
}
} // namespace

TEST(protobuf_stream, custom_buffers_and_iostreams) {
  check_protocol<codec::protobuf_binary>();
  check_protocol<codec::protojson>();
  check_protocol<codec::textproto>();
}

namespace {
// Exact decoding also accepts opt-in owning codecs and rejects malformed final input.
template <template <codec::serialize_type> class Protocol>
void check_exact_codec() {
  protobuf_test::record original{};
  original.display_name = "Exact input";
  original.nested.number = 42;
  rohit::full_stream_auto_alloc encoded{};
  original.serialize_out<Protocol>(encoded);
  const std::string bytes{reinterpret_cast<const char*>(encoded.begin()), encoded.current_offset()};
  const stream_test::input_cursor input{bytes};
  const auto result = codec::deserialize_exact<protobuf_test::record, Protocol>(input);
  EXPECT_EQ(result.display_name, original.display_name);
  EXPECT_EQ(result.nested.number, original.nested.number);
  std::istringstream stream{bytes};
  EXPECT_EQ((codec::deserialize_exact<protobuf_test::record, Protocol>(stream)).display_name,
            original.display_name);
  // NUL is an invalid tag in binary and invalid trailing syntax in both text protocols.
  const std::string malformed = bytes + '\0';
  const stream_test::input_cursor invalid{malformed};
  EXPECT_THROW(
      static_cast<void>(codec::deserialize_exact<protobuf_test::record, Protocol>(invalid)),
      rohit::exception::base_parser);
}
} // namespace

// Protobuf binary remains externally framed: valid concatenated messages can merge.
TEST(protobuf_stream, exact_fresh_value_helper) {
  check_exact_codec<codec::protobuf_binary>();
  check_exact_codec<codec::protojson>();
  check_exact_codec<codec::textproto>();
}
