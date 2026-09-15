#include <rohit/serializer.hpp>

#include <array.hpp>
#include <person.hpp>
#include <variable.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

namespace {

// Compare encoded output with bytes captured from the implementation before renaming.
template <template <rohit::serializer::serialize_type> class Protocol, typename T>
void expect_wire_bytes(const T& value, std::string_view expected_hex) {
  constexpr std::size_t initial_capacity_bytes = 1024;
  constexpr std::string_view hex_digits = "0123456789abcdef";
  rohit::full_stream_auto_alloc output{initial_capacity_bytes};
  value.template serialize_out<Protocol>(output);
  std::string actual_hex;
  for (std::size_t index = 0; index < output.current_offset(); ++index) {
    const auto byte = output.begin()[index];
    actual_hex.push_back(hex_digits[byte >> 4]);
    actual_hex.push_back(hex_digits[byte & 0x0f]);
  }
  EXPECT_EQ(actual_hex, expected_hex);
}

// Check the same object against its four independent format snapshots.
template <typename T>
void expect_all_formats(const T& value, const std::array<std::string_view, 4>& expected_hex) {
  expect_wire_bytes<rohit::serializer::json>(value, expected_hex[0]);
  expect_wire_bytes<rohit::serializer::binary_none>(value, expected_hex[1]);
  expect_wire_bytes<rohit::serializer::binary_integer>(value, expected_hex[2]);
  expect_wire_bytes<rohit::serializer::binary_string>(value, expected_hex[3]);
}

} // namespace

// Preserve pre-migration bytes for member names and identifiers.
TEST(wire_format, member_names_and_identifiers) {
  const auto value = test::test1::person{"Ada", 322};
  expect_all_formats(value, {
                                "7b2266756c6c6e616d65223a22416461222c224944223a3332327d",
                                "034164610000000000000142",
                                "030341646104000000000000014200",
                                "0866756c6c6e616d6503416461024944000000000000014200",
                            });
}

// Preserve pre-migration bytes for inherited members.
TEST(wire_format, inherited_members) {
  const auto value = test::test1::personex{"Ada", 322, 122};
  expect_all_formats(
      value,
      {
          "7b22706572736f6e223a7b2266756c6c6e616d65223a22416461222c224944223a3332327d2c226163636f75"
          "6e74223a3132327d",
          "034164610000000000000142000000000000007a",
          "0503034164610400000000000001420006000000000000007a00",
          "06706572736f6e0866756c6c6e616d6503416461024944000000000000014200076163636f756e7400000000"
          "0000007a00",
      });
}

// Preserve pre-migration bytes for arrays and maps.
TEST(wire_format, arrays_and_maps) {
  const auto value = arraytest::personlist{556, true, {{"Ada", 1}, {"Grace", 2}}, {{1, 0}, {2, 1}}};
  expect_all_formats(
      value,
      {
          "7b226c6973746964223a3535362c22636865636b223a747275652c226c697374223a5b7b226e616d65223a22"
          "416461222c224944223a317d2c7b226e616d65223a224772616365222c224944223a327d5d2c227265766572"
          "73654c6973744d6170223a5b7b226b6579223a312c2276616c7565223a307d2c7b226b6579223a322c227661"
          "6c7565223a317d5d7d",
          "000000000000022c010203416461000000000000000105477261636500000000000000020200000001000000"
          "000000000200000001",
          "01000000000000022c0201030201034164610200000000000000010001054772616365020000000000000002"
          "0004020000000100000000000000020000000100",
          "066c6973746964000000000000022c05636865636b01046c69737402046e616d650341646102494400000000"
          "0000000100046e616d650547726163650249440000000000000002000e726576657273654c6973744d617002"
          "0000000100000000000000020000000100",
      });
}

// Preserve pre-migration bytes for unions and enums.
TEST(wire_format, unions_and_enums) {
  const auto value = test::server1{test::server1::e_entry::http,
                                   {.http = {10, 10, 10, 10, 2010, 10240, 5021}},
                                   test::test112::em3};
  expect_all_formats(
      value,
      {
          "7b22656e7472793a68747470223a7b2273657276657262617365223a7b226e616d65223a7b2261223a31302c"
          "2262223a31302c2263223a31302c2264223a31307d2c22706f7274223a323031307d2c2273697a65223a3130"
          "3234302c226d696d6573697a65223a353032317d2c22746573743132223a22656d33227d",
          "010a0a0a0a07da000028000000139d02",
          "01010101010a020a030a040a000207da000200002800030000139d00020200",
          "0a656e7472793a687474700a73657276657262617365046e616d6501610a01620a01630a01640a0004706f72"
          "7407da000473697a6500002800086d696d6573697a650000139d000674657374313203656d3300",
      });
}
