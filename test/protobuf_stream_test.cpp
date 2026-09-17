#include "stream_test_support.hpp"

#include <gtest/gtest.h>
#include <protobuf.hpp>

#include <sstream>
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

  std::stringstream io;
  original.serialize_out<Protocol>(io);
  EXPECT_EQ(io.str(), expected);
  decoded = {};
  decoded.serialize_in<Protocol>(io);
  EXPECT_EQ(decoded.numbers, original.numbers);
  EXPECT_EQ(decoded.nested.number, original.nested.number);
}
} // namespace

TEST(protobuf_stream, custom_buffers_and_iostreams) {
  check_protocol<codec::protobuf_binary>();
  check_protocol<codec::protojson>();
  check_protocol<codec::textproto>();
}
