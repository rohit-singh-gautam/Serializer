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
#include <rohit/serializercreator.h>

TEST(SerializeParser, Identifier) {
    std::vector<std::tuple<std::string, std::string, bool>> test_list {
        {"_Test   ", "_Test", false},
        {"9Test   ", "9Test", true},
        {"T39cc03_232_;", "T39cc03_232_", false},
        {"#T39cc03_232_", "#T39cc03_232_", true},
        {"", "_Test", true},
    };

    rohit::FullStreamAutoAlloc outStream {128};
    for(auto &test: test_list) {
        auto &[input, output, negativetest ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        rohit::SerializerCreator creator { inStream, outStream };
        if (!negativetest) {
            auto parsedstring = creator.ParseIdentifier();
            EXPECT_EQ(parsedstring, output);
        } else {
            EXPECT_THROW(creator.ParseIdentifier(), rohit::exception::BadIdentifier);
        }
    }
}

TEST(SerializeParser, HierarchicalIdentifier) {
    std::vector<std::tuple<std::string, std::string, bool>> test_list {
        {"_Test   ", "_Test", false},
        {"9Test   ", "9Test", true},
        {"T39cc03_232_;", "T39cc03_232_", false},
        {"#T39cc03_232_", "#T39cc03_232_", true},
        {"", "_Test", true},
        {"rohit::_Test   ", "rohit::_Test", false},
        {"rohit::9Test   ", "rohit::_Test", true},
        {"a::b::c::d::e::fast::_Test   ", "a::b::c::d::e::fast::_Test", false},
    };

    rohit::FullStreamAutoAlloc outStream {128};
    for(auto &test: test_list) {
        auto &[input, output, negativetest ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        rohit::SerializerCreator creator { inStream, outStream };
        if (!negativetest) {
            auto parsedstring = creator.ParseHierarchicalIdentifier();
            EXPECT_EQ(parsedstring, output);
        } else {
            EXPECT_THROW(creator.ParseHierarchicalIdentifier(), rohit::exception::BadIdentifier);
        }
    }
}