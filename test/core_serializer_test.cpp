//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <rohit/serializer.hpp>
#include <vector>

TEST(json_serializer, char) {
  std::vector<std::pair<std::string, char>> test_list{
      {"\"0\"", '0'}, {"\"1\"", '1'}, {"\"a\"", 'a'}, {"\"z\"", 'z'}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, integer8) {
  std::vector<std::pair<std::string, std::int8_t>> test_list{
      {"0", 0},   {"1", 1},   {"-1", -1}, {"1", 1},     {"123", 123},  {"45", 45},
      {"67", 67}, {"89", 89}, {"10", 10}, {"127", 127}, {"-128", -128}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, integer16) {
  std::vector<std::pair<std::string, std::int16_t>> test_list{
      {"0", 0},       {"1", 1},         {"-1", -1},       {"10", 10},
      {"124", 124},  {"12345", 12345}, {"16789", 16789}, {"19558", 19558},
      {"2933", 2933}, {"25443", 25443}, {"32767", 32767}, {"-32768", -32768}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, integer32) {
  std::vector<std::pair<std::string, std::int32_t>> test_list{
      {"0", 0},
      {"1", 1},
      {"-1", -1},
      {"10", 10},
      {"124", 124},
      {"12340", 12340},
      {"56789", 56789},
      {"39558", 39558},
      {"2933", 2933},
      {"55443", 55443},
      {"55443333", 55443333},
      {"2147483647", 2147483647},
      {"-2147483648", static_cast<std::uint32_t>(-2147483648)}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, integer64) {
  std::vector<std::pair<std::string, std::int64_t>> test_list{
      {"0", 0},
      {"1", 1},
      {"-1", -1},
      {"10", 10},
      {"124", 124},
      {"12340", 12340},
      {"56789", 56789},
      {"39558", 39558},
      {"2933", 2933},
      {"55443", 55443},
      {"55443333", 55443333},
      {"2147483647", 2147483647},
      {"-2147483648", -2147483648},
      {"9223372036854775807", 9223372036854775807LL},
      {"-9223372036854775808", std::numeric_limits<std::int64_t>::min()}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, unsigned_integer8) {
  std::vector<std::pair<std::string, std::uint8_t>> test_list{
      {"255", 255}, {"0", 0},   {"1", 1},   {"123", 123}, {"45", 45},
      {"67", 67},   {"89", 89}, {"10", 10}, {"127", 127},
  };

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, unsigned_integer16) {
  std::vector<std::pair<std::string, std::uint16_t>> test_list{
      {"0", 0},         {"1", 1},         {"10", 10},       {"124", 124},
      {"12345", 12345}, {"56789", 56789}, {"39558", 39558}, {"2933", 2933},
      {"55443", 55443}, {"32767", 32767}, {"65535", 65535}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, unsigned_integer32) {
  std::vector<std::pair<std::string, std::uint32_t>> test_list{
      {"0", 0},
      {"1", 1},
      {"10", 10},
      {"124", 124},
      {"12340", 12340},
      {"56789", 56789},
      {"39558", 39558},
      {"2933", 2933},
      {"55443", 55443},
      {"55443333", 55443333},
      {"2147483647", 2147483647},
      {"4294967295", std::numeric_limits<std::uint32_t>::max()}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, unsigned_integer64) {
  std::vector<std::pair<std::string, std::uint64_t>> test_list{
      {"0", 0},
      {"1", 1},
      {"10", 10},
      {"124", 124},
      {"12340", 12340},
      {"56789", 56789},
      {"39558", 39558},
      {"2933", 2933},
      {"55443", 55443},
      {"55443333", 55443333},
      {"2147483647", 2147483647},
      {"9223372036854775807", 9223372036854775807LL},
      {"18446744073709551615", std::numeric_limits<std::uint64_t>::max()}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, float) {
  std::vector<std::pair<std::string, float>> test_list{
      {"0", 0.0f},
      {"1", 1.0f},
      {"-1", -1.0f},
      {"10", 10.0f},
      {"124", 124.0f},
      {"12340", 12340.0f},
      {"-56789", -56789.0f},
      {"39558", 39558.0f},
      {"-2933", -2933.0f},
      {"55443", 55443.0f},
      {"55443333", 55443333.0f},
      {"2147483647", 2147483647.0f},
      {"9.22337204e+18", 9.22337204e+18f},
      {"3.40282347e+38", std::numeric_limits<float>::max()},
      {"1.17549435e-38", std::numeric_limits<float>::min()}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, double) {
  std::vector<std::pair<std::string, double>> test_list{
      {"0", 0},
      {"1", 1},
      {"-1", -1},
      {"10", 10},
      {"124", 124},
      {"12340", 12340},
      {"-56789", -56789},
      {"39558", 39558},
      {"-2933", -2933},
      {"55443", 55443},
      {"55443333", 55443333},
      {"2147483647", 2147483647},
      {"9.22337204e+18", 9.22337204e+18},
      {"3.4028234663852886e+38", std::numeric_limits<float>::max()},
      {"1.1754943508222875e-38", std::numeric_limits<float>::min()},
      {"1.7976931348623157e+308", std::numeric_limits<double>::max()},
      {"2.2250738585072014e-308", std::numeric_limits<double>::min()}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, bool) {
  std::vector<std::pair<std::string, bool>> test_list{{"false", false}, {"true", true}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(json_serializer, string) {
  std::vector<std::pair<std::string, std::string>> test_list{
      {"\"0\"", "0"}, {"\"1\"", "1"}, {"\"a\"", "a"},
      {"\"z\"", "z"}, {"\"\"", ""},   {"\"This is a test\"", "This is a test"}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::json<rohit::serializer::serialize_type::in> json_in{stream};
    json_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(binary_serializer, char) {
  std::vector<std::pair<std::string, char>> test_list{
      {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'}, {"5", '5'}, {"6", '6'},
      {"7", '7'}, {"8", '8'}, {"9", '9'}, {"A", 'A'}, {"Z", 'Z'}, {"a", 'a'}, {"z", 'z'}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::binary_none<rohit::serializer::serialize_type::in> binary_in{stream};
    binary_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(binary_serializer, integer8) {
  std::vector<std::pair<std::string, std::int8_t>> test_list{
      {"\x01", 1}, {"\xff", -1}, {"\x01", 1}, {"{", 123},    {"-", 45},
      {"C", 67},   {"Y", 89},    {"\n", 10},  {"\x7f", 127}, {"\x80", -128}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::binary_none<rohit::serializer::serialize_type::in> binary_in{stream};
    binary_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(binary_serializer, integer16) {
  std::vector<std::pair<std::string, std::int16_t>> test_list{
      {"\x89\xab", -30293}, {"\xcc\xff", -13057}, {"\x56\x48", 22088}, {"\xff\xff", -1},
      {"\x7f\xff", 32767},  {"\x01\x23", 291},    {"\x45\x67", 17767}, {"\x12\x34", 4660},
      {"\x9a\xbc", -25924}, {"\xde\xa8", -8536},  {"\x7f\xff", 32767}, {"\xff\xff", -1}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::binary_none<rohit::serializer::serialize_type::in> binary_in{stream};
    binary_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

TEST(binary_serializer, integer16_second) {
  std::vector<std::int16_t> test_list{-13057, 22088,  -1,   32767,  -32768, 291,
                                      17767,  -30293, 4660, -25924, -8536,  32767,
                                      1,      0,      127,  128,    -127,   -128};

  for (auto test : test_list) {
    std::uint8_t* begin = reinterpret_cast<std::uint8_t*>(&test);
    std::uint8_t* end = begin + sizeof(test);
    auto stream = rohit::make_constant_full_stream(begin, end);
    decltype(test) value{};
    rohit::serializer::binary_none<rohit::serializer::serialize_type::in> binary_in{stream};
    binary_in.serialize_in(value);
    EXPECT_EQ(test, rohit::byteswap(value));
  }
}

TEST(binary_serializer, integer32) {
  std::vector<std::int32_t> test_list{
      -13057, 22088, -1,    32767, -32768,   291,        17767,      -30293, 4660,
      -25924, -8536, 32767, 1,     0,        127,        128,        -127,   -128,
      56789,  39558, 2933,  55443, 55443333, 2147483647, -2147483648};

  for (auto test : test_list) {
    std::uint8_t* begin = reinterpret_cast<std::uint8_t*>(&test);
    std::uint8_t* end = begin + sizeof(test);
    auto stream = rohit::make_constant_full_stream(begin, end);
    decltype(test) value{};
    rohit::serializer::binary_none<rohit::serializer::serialize_type::in> binary_in{stream};
    binary_in.serialize_in(value);
    EXPECT_EQ(test, rohit::byteswap(value));
  }
}

TEST(binary_serializer, integer64) {
  std::vector<std::int64_t> test_list{-13057,
                                      22088,
                                      -1,
                                      32767,
                                      -32768,
                                      291,
                                      17767,
                                      -30293,
                                      4660,
                                      -25924,
                                      -8536,
                                      32767,
                                      1,
                                      0,
                                      127,
                                      128,
                                      -127,
                                      -128,
                                      56789,
                                      39558,
                                      2933,
                                      55443,
                                      55443333,
                                      2147483647,
                                      -2147483648,
                                      9223372036854775807,
                                      std::numeric_limits<std::int64_t>::min()};

  for (auto test : test_list) {
    std::uint8_t* begin = reinterpret_cast<std::uint8_t*>(&test);
    std::uint8_t* end = begin + sizeof(test);
    auto stream = rohit::make_constant_full_stream(begin, end);
    decltype(test) value{};
    rohit::serializer::binary_none<rohit::serializer::serialize_type::in> binary_in{stream};
    binary_in.serialize_in(value);
    EXPECT_EQ(test, rohit::byteswap(value));
  }
}

TEST(binary_serializer, u_integer32_variable) {
  std::vector<std::uint32_t> test_list{
      0x015648, 22088, 536870912, 1073741823, 13057, 32767, 32768, 291,  17767, 30293,
      4660,     25924, 8536,      32767,      1,     0,     127,   128,  56789, 39558,
      2933,     55443, 55443333,  65536,      1024,  1023,  16384, 16383};

  for (auto test : test_list) {
    rohit::full_stream_auto_alloc stream{256};
    rohit::serializer::binary_none<rohit::serializer::serialize_type::out> binary_out{stream};
    binary_out.serialize_out_variable(test);
    const auto input = rohit::make_constant_full_stream(stream.begin(), stream.current_offset());
    rohit::serializer::binary_none<rohit::serializer::serialize_type::in> binary_in{input};
    const auto value = binary_in.serialize_in_variable();
    EXPECT_EQ(test, value);
  }
}

TEST(binary_serializer, string) {
  std::vector<std::pair<std::string, std::string>> test_list{{"\x0e"
                                                              "This is a test",
                                                              "This is a test"},
                                                             {"\x01"
                                                              "0",
                                                              "0"},
                                                             {"\x01"
                                                              "1",
                                                              "1"},
                                                             {"\x01"
                                                              "a",
                                                              "a"},
                                                             {"\x01"
                                                              "z",
                                                              "z"}};

  for (auto& test : test_list) {
    auto stream = rohit::make_constant_full_stream(test.first);
    decltype(test.second) value{};
    rohit::serializer::binary_none<rohit::serializer::serialize_type::in> binary_in{stream};
    binary_in.serialize_in(value);
    EXPECT_EQ(test.second, value);
  }
}

// Initialize GoogleTest and run the registered cases.
int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}