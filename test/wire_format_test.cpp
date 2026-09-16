#include <rohit/serializer.hpp>

#include <array.hpp>
#include <person.hpp>
#include <variable.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

namespace {

// Compare encoded output with the documented field-by-field wire representation.
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

// Keep member names and identifiers while using little-endian fixed-width values.
TEST(wire_format, member_names_and_identifiers) {
  const auto value = test::test1::person{"Ada", 322};
  expect_all_formats(value, {
                                "7b2266756c6c6e616d65223a22416461222c224944223a3332327d",
                                "034164614201000000000000",
                                "030341646104420100000000000000",
                                "0866756c6c6e616d6503416461024944420100000000000000",
                            });
}

// Encode inherited members with the same byte order as their containing object.
TEST(wire_format, inherited_members) {
  const auto value = test::test1::personex{{"Ada", 322}, 122};
  expect_all_formats(
      value,
      {
          "7b22706572736f6e223a7b2266756c6c6e616d65223a22416461222c224944223a3332327d2c226163636f75"
          "6e74223a3132327d",
          "03416461" "4201000000000000" "7a00000000000000",
          "05" "030341646104420100000000000000" "06" "7a00000000000000" "00",
          "06706572736f6e" "0866756c6c6e616d6503416461024944420100000000000000"
          "076163636f756e74" "7a00000000000000" "00",
      });
}

// Apply little-endian scalar encoding within arrays and map entries.
TEST(wire_format, arrays_and_maps) {
  const auto value = arraytest::personlist{556, true, {{"Ada", 1}, {"Grace", 2}}, {{1, 0}, {2, 1}}};
  expect_all_formats(
      value,
      {
          "7b226c6973746964223a3535362c22636865636b223a747275652c226c697374223a5b7b226e616d65223a22"
          "416461222c224944223a317d2c7b226e616d65223a224772616365222c224944223a327d5d2c227265766572"
          "73654c6973744d6170223a5b7b226b6579223a312c2276616c7565223a307d2c7b226b6579223a322c227661"
          "6c7565223a317d5d7d",
          "2c02000000000000" "01" "02"
          "03416461" "0100000000000000" "054772616365" "0200000000000000"
          "02" "01000000" "00000000" "02000000" "01000000",
          "01" "2c02000000000000" "0201" "0302"
          "010341646102010000000000000000" "0105477261636502020000000000000000"
          "0402" "01000000" "00000000" "02000000" "01000000" "00",
          "066c6973746964" "2c02000000000000" "05636865636b01" "046c69737402"
          "046e616d6503416461024944" "0100000000000000" "00"
          "046e616d65054772616365024944" "0200000000000000" "00"
          "0e726576657273654c6973744d617002" "01000000" "00000000" "02000000" "01000000" "00",
      });
}

// Keep compact discriminators and enum values independent of scalar byte order.
TEST(wire_format, unions_and_enums) {
  const auto value = test::server1{test::server1::e_entry::http,
                                   {.http = {{{10, 10, 10, 10}, 2010}, 10240, 5021}},
                                   test::test112::em3};
  expect_all_formats(
      value,
      {
          "7b22656e7472793a68747470223a7b2273657276657262617365223a7b226e616d65223a7b2261223a31302c"
          "2262223a31302c2263223a31302c2264223a31307d2c22706f7274223a323031307d2c2273697a65223a3130"
          "3234302c226d696d6573697a65223a353032317d2c22746573743132223a22656d33227d",
          "010a0a0a0ada07002800009d13000002",
          "01010101010a020a030a040a0002da07000200280000039d13000000020200",
          "0a656e7472793a687474700a73657276657262617365046e616d6501610a01620a01630a01640a0004706f72"
          "74da07000473697a6500280000086d696d6573697a659d130000000674657374313203656d3300",
      });
}
