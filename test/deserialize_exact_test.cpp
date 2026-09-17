#include "stream_test_support.hpp"

#include <person.hpp>
#include <rohit/serializer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
using person = test::test1::person;

// Check exact native messages through borrowed buffers and EOF-delimited byte sources.
template <template <codec::serialize_type> class Protocol>
void check_exact_protocol() {
  const person original{"Ada", 8};
  rohit::full_stream_auto_alloc output{};
  original.serialize_out<Protocol>(output);
  const std::string bytes{reinterpret_cast<const char*>(output.begin()), output.current_offset()};
  const stream_test::input_cursor input{bytes};
  const auto decoded = codec::deserialize_exact<person, Protocol>(input);
  EXPECT_EQ(decoded.name, original.name);
  EXPECT_EQ(decoded.id, original.id);
  EXPECT_TRUE(input.full());
  std::istringstream memory{bytes};
  EXPECT_EQ((codec::deserialize_exact<person, Protocol>(memory)).name, original.name);
  std::istringstream backing{bytes};
  std::istream& erased = backing;
  EXPECT_EQ((codec::deserialize_exact<person, Protocol>(erased)).id, original.id);

  const std::string trailing = bytes + "x";
  const stream_test::input_cursor extra{trailing};
  EXPECT_THROW(static_cast<void>(codec::deserialize_exact<person, Protocol>(extra)),
               codec::exception::bad_input_data);
  const stream_test::input_cursor truncated{std::string_view{bytes}.substr(0, bytes.size() - 1)};
  EXPECT_THROW(static_cast<void>(codec::deserialize_exact<person, Protocol>(truncated)),
               rohit::exception::base_parser);
}
} // namespace

// All native modes reject suffixes, including buffer inputs whose convenience APIs allow them.
TEST(deserialize_exact, native_buffers_and_byte_streams) {
  check_exact_protocol<codec::json>();
  check_exact_protocol<codec::binary_none>();
  check_exact_protocol<codec::binary_integer>();
  check_exact_protocol<codec::binary_string>();
}

// Fresh construction retains schema defaults, while finish permits only JSON whitespace.
TEST(deserialize_exact, defaults_whitespace_and_existing_convenience_behavior) {
  const stream_test::input_cursor empty_object{"{} \t\r\n"};
  const auto decoded = codec::deserialize_exact<person, codec::json>(empty_object);
  EXPECT_EQ(decoded.name, "None");
  EXPECT_TRUE(empty_object.full());
  const stream_test::input_cursor concatenated{"{} {}"};
  EXPECT_THROW(static_cast<void>(codec::deserialize_exact<person, codec::json>(concatenated)),
               codec::exception::bad_input_data);
  const stream_test::input_cursor existing{"{} {}"};
  EXPECT_NO_THROW(static_cast<void>(person::deserialize<codec::json>(existing)));
  EXPECT_FALSE(existing.full());
}

// A late error or exhausted budget must not assign a partially decoded candidate to the caller.
TEST(deserialize_exact, errors_preserve_the_callers_value) {
  person destination{"original", 42};
  const stream_test::input_cursor malformed{"{\"fullname\":\"changed\",\"ID\":oops}"};
  const auto* const beginning = malformed.curr();
  EXPECT_THROW((destination = codec::deserialize_exact<person, codec::json>(malformed)),
               rohit::exception::base_parser);
  EXPECT_EQ(destination.name, "original");
  EXPECT_EQ(destination.id, 42);
  EXPECT_GT(malformed.curr(), beginning);
  codec::decode_limits limits{};
  limits.max_string_bytes = 2;
  const stream_test::input_cursor limited{"{\"fullname\":\"changed\"}"};
  EXPECT_THROW((destination = codec::deserialize_exact<person, codec::json>(limited, limits)),
               codec::exception::resource_limit);
  EXPECT_EQ(destination.name, "original");
  const stream_test::input_cursor suffix{"{\"fullname\":\"changed\"} extra"};
  EXPECT_THROW((destination = codec::deserialize_exact<person, codec::json>(suffix)),
               codec::exception::bad_input_data);
  EXPECT_EQ(destination.name, "original");
}

// The helper also supports runtime scalar/collection codecs without generated wrapper methods.
TEST(deserialize_exact, runtime_values_and_transport_limits) {
  const stream_test::input_cursor number{"123 "};
  EXPECT_EQ((codec::deserialize_exact<std::uint32_t, codec::json>(number)), 123U);
  std::istringstream values{"[1,2,3]"};
  EXPECT_EQ((codec::deserialize_exact<std::vector<int>, codec::json>(values)),
            (std::vector<int>{1, 2, 3}));
  codec::decode_limits limits{};
  limits.max_input_bytes = 1;
  std::istringstream oversized{"123"};
  EXPECT_THROW(static_cast<void>(codec::deserialize_exact<int, codec::json>(oversized, limits)),
               std::length_error);
  std::istringstream failed{"123"};
  failed.setstate(std::ios::badbit);
  EXPECT_THROW(static_cast<void>(codec::deserialize_exact<int, codec::json>(failed)),
               std::ios_base::failure);
}
