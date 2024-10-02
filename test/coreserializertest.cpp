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
#include <rohit/serializer.h>
#include <vector>

TEST(JSONSerializer, Char) {
    std::vector<std::pair<std::string, char>> test_list {
        {"\"0\"", '0'},
        {"\"1\"", '1'},
        {"\"a\"", 'a'},
        {"\"z\"", 'z'}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value); 
    }
}

TEST(JSONSerializer, Integer8) {
    std::vector<std::pair<std::string, int8_t>> test_list {
        {"0", 0},
        {"1", 1},
        {"-1", -1},
        {"1", 1},
        {"123", 123},
        {"45", 45},
        {"67", 67},
        {"89", 89},
        {"10", 10},
        {"127", 127},
        {"-128", -128}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value); 
    }
}

TEST(JSONSerializer, Integer16) {
    std::vector<std::pair<std::string, int16_t>> test_list {
        {"0", 0},
        {"1", 1},
        {"-1", -1},
        {"10", 10},
        {"0124", 124},
        {"12345", 12345},
        {"56789", 56789},
        {"39558", 39558},
        {"2933", 2933},
        {"55443", 55443},
        {"32767", 32767},
        {"-32768", -32768}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value); 
    }
}

TEST(JSONSerializer, Integer32) {
    std::vector<std::pair<std::string, int32_t>> test_list {
        {"0", 0},
        {"1", 1},
        {"-1", -1},
        {"10", 10},
        {"0124", 124},
        {"12340", 12340},
        {"56789", 56789},
        {"39558", 39558},
        {"2933", 2933},
        {"55443", 55443},
        {"55443333", 55443333},
        {"2147483647", 2147483647},
        {"-2147483648", -2147483648}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value);
    }
}

TEST(JSONSerializer, Integer64) {
    std::vector<std::pair<std::string, int64_t>> test_list {
        {"0", 0},
        {"1", 1},
        {"-1", -1},
        {"10", 10},
        {"0124", 124},
        {"12340", 12340},
        {"56789", 56789},
        {"39558", 39558},
        {"2933", 2933},
        {"55443", 55443},
        {"55443333", 55443333},
        {"2147483647", 2147483647},
        {"-2147483648", -2147483648},
        {"9223372036854775807", 9223372036854775807LL},
        {"-9223372036854775808", std::numeric_limits<int64_t>::min()}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value);
        
    }
}

TEST(JSONSerializer, UnsignedInteger8) {
    std::vector<std::pair<std::string, uint8_t>> test_list {
        {"0", 0},
        {"1", 1},
        {"123", 123},
        {"45", 45},
        {"67", 67},
        {"89", 89},
        {"10", 10},
        {"127", 127},
        {"255", 255}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value); 
    }
}

TEST(JSONSerializer, UnsignedInteger16) {
    std::vector<std::pair<std::string, uint16_t>> test_list {
        {"0", 0},
        {"1", 1},
        {"10", 10},
        {"0124", 124},
        {"12345", 12345},
        {"56789", 56789},
        {"39558", 39558},
        {"2933", 2933},
        {"55443", 55443},
        {"32767", 32767},
        {"65535", 65535}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value); 
    }
}

TEST(JSONSerializer, UnsignedInteger32) {
    std::vector<std::pair<std::string, uint32_t>> test_list {
        {"0", 0},
        {"1", 1},
        {"10", 10},
        {"0124", 124},
        {"12340", 12340},
        {"56789", 56789},
        {"39558", 39558},
        {"2933", 2933},
        {"55443", 55443},
        {"55443333", 55443333},
        {"2147483647", 2147483647},
        {"4294967295", std::numeric_limits<uint32_t>::max()}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value);
    }
}

TEST(JSONSerializer, UnsignedInteger64) {
    std::vector<std::pair<std::string, uint64_t>> test_list {
        {"0", 0},
        {"1", 1},
        {"10", 10},
        {"0124", 124},
        {"12340", 12340},
        {"56789", 56789},
        {"39558", 39558},
        {"2933", 2933},
        {"55443", 55443},
        {"55443333", 55443333},
        {"2147483647", 2147483647},
        {"9223372036854775807", 9223372036854775807LL},
        {"18446744073709551615", std::numeric_limits<uint64_t>::max()}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value);
        
    }
}

TEST(JSONSerializer, Float) {
    std::vector<std::pair<std::string, float>> test_list {
        {"0", 0},
        {"1", 1},
        {"-1", -1},
        {"10", 10},
        {"0124", 124},
        {"12340", 12340},
        {"-56789", -56789},
        {"39558", 39558},
        {"-2933", -2933},
        {"55443", 55443},
        {"55443333", 55443333},
        {"2147483647", 2147483647},
        {"9.22337204e+18", 9.22337204e+18},
        {"3.40282347e+38", std::numeric_limits<float>::max()},
        {"1.17549435e-38", std::numeric_limits<float>::min()}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value);
        
    }
}

TEST(JSONSerializer, Double) {
    std::vector<std::pair<std::string, double>> test_list {
        {"0", 0},
        {"1", 1},
        {"-1", -1},
        {"10", 10},
        {"0124", 124},
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
        {"2.2250738585072014e-308", std::numeric_limits<double>::min()}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value);
        
    }
}

TEST(JSONSerializer, Bool) {
    std::vector<std::pair<std::string, bool>> test_list {
        {"False", false},
        {"True", true},
        {"false", false},
        {"true", true},
        {"fAlse", false},
        {"tRue", true},
        {"FALSE", false},
        {"TRUE", true},
        {"FalsE", false},
        {"TruE", true},
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value); 
    }
}

TEST(JSONSerializer, String) {
    std::vector<std::pair<std::string, std::string>> test_list {
        {"\"0\"", "0"},
        {"\"1\"", "1"},
        {"\"a\"", "a"},
        {"\"z\"", "z"},
        {"\"\"", ""},
        {"\"This is a test\"", "This is a test"}
    };

    for(auto &test: test_list) {
        auto stream = rohit::make_const_fullstream(test.first);
        auto value = rohit::serializer::json::serialize_in<decltype(test.second)>(stream);
        EXPECT_EQ(test.second, value); 
    }
}

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}