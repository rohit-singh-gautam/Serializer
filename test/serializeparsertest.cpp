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
        {"a   ", "a", false},
        {"b\"   ", "b", false},
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

TEST(SerializeParser, SpaceSeparatedIdentifier) {
    std::vector<std::tuple<std::string, std::string>> test_list {
        {"test test1 test2   ", "test test1 test2 "},
        {"9Test   ", ""},
        {"T39cc03_232_;", "T39cc03_232_ "},
        {"#T39cc03_232_", ""},
        {"", ""}
    };

    rohit::FullStreamAutoAlloc outStream {128};
    for(auto &test: test_list) {
        std::string parsedstring { };
        auto resultfn = [&parsedstring](std::string &&value) {
            parsedstring += value;
            parsedstring += ' ';  
        };
        auto &[input, output ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        rohit::SerializerCreator creator { inStream, outStream };
        creator.SpaceSeparatedIdentifier(resultfn);
        EXPECT_EQ(parsedstring, output);
    }
}

TEST(SerializeParser, AccessType) {
    std::vector<std::tuple<std::string, rohit::AccessType>> test_list {
        {"public", rohit::AccessType::Public},
        {"protected", rohit::AccessType::Protected},
        {"private", rohit::AccessType::Private},
        {"Public", rohit::AccessType::Error},
        {"Protected", rohit::AccessType::Error},
        {"Private", rohit::AccessType::Error},
        {"", rohit::AccessType::Error},
        {"_", rohit::AccessType::Error},
        {"public1", rohit::AccessType::Error},
        {"protected ", rohit::AccessType::Protected},
    };

    rohit::FullStreamAutoAlloc outStream {128};
    for(auto &test: test_list) {
        auto &[input, output ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        rohit::SerializerCreator creator { inStream, outStream };
        if (output != rohit::AccessType::Error) {
            auto parsedstring = creator.ParseAccessType();
            EXPECT_EQ(parsedstring, output);
        } else {
            EXPECT_THROW(creator.ParseAccessType(), rohit::exception::Base);
        }
    }
}

TEST(SerializeParser, Member) {
    std::vector<std::tuple<std::string, rohit::Member, bool>> test_list {
        {"public uint8 test;", {rohit::AccessType::Public, "uint8", "test"}, false},
        {"protected \r\n uint8\ttest;", {rohit::AccessType::Protected, "uint8", "test"}, false},
        {"private \r\n newtest\t_test\r\n;", {rohit::AccessType::Private, "newtest", "_test"}, false},
        {" private \r\n 9newtest\t_test\r\n;", {rohit::AccessType::Private, "uint8", "_test"}, true},
    };

    rohit::FullStreamAutoAlloc outStream {128};
    for(auto &test: test_list) {
        auto &[input, output, negativetest ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        rohit::SerializerCreator creator { inStream, outStream };
        if (!negativetest) {
            auto parsedmember = creator.ParseMember();
            EXPECT_EQ(parsedmember, output);
        } else {
            EXPECT_THROW(creator.ParseMember(), rohit::exception::BadIdentifier);
        }
    }
}

TEST(SerializeParser, ClassBody) {
    std::string input {
        "{\n"
        "public string name; "
        "public uint64 ID;\t"
        "}"
    };

    rohit::FullStreamAutoAlloc outStream {128};
    rohit::FullStream inStream { input.data(), input.size() };
    rohit::SerializerCreator creator { inStream, outStream };
    std::string name { "person" };
    std::vector<rohit::Parent> parent { };
    rohit::Class obj {rohit::ObjectType::Class, std::move(name), nullptr, rohit::ClassAtributes::None, std::move(parent)};
    creator.ParseClassBody(obj);
    EXPECT_EQ(obj.MemberList.size(), 2);
}

TEST(SerializeParser, CompleteStruct) {
    std::string input {
        "namespace test {\r\n"
        "class person packed {\n"
        "public string name; "
        "public uint64 ID;\t"
        "}"
        "}"
    };

    rohit::FullStreamAutoAlloc outStream {128};
    rohit::FullStream inStream { input.data(), input.size() };
    rohit::SerializerCreator creator { inStream, outStream };
    auto parsed = creator.Parse();
    EXPECT_EQ(parsed.size(), 1);
}