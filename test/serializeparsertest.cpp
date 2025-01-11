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

    for(auto &test: test_list) {
        auto &[input, output, negativetest ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        if (!negativetest) {
            auto parsedstring = rohit::Serializer::Parser::ParseIdentifier(inStream);
            EXPECT_EQ(parsedstring, output);
        } else {
            EXPECT_THROW(rohit::Serializer::Parser::ParseIdentifier(inStream), rohit::Serializer::exception::BadIdentifier);
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

    for(auto &test: test_list) {
        auto &[input, output, negativetest ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        if (!negativetest) {
            auto parsedstring = rohit::Serializer::Parser::ParseHierarchicalIdentifier(inStream);
            EXPECT_EQ(parsedstring, output);
        } else {
            EXPECT_THROW(rohit::Serializer::Parser::ParseHierarchicalIdentifier(inStream), rohit::Serializer::exception::BadIdentifier);
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

    for(auto &test: test_list) {
        std::string parsedstring { };
        auto resultfn = [&parsedstring](std::string &&value) {
            parsedstring += value;
            parsedstring += ' ';  
        };
        auto &[input, output ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        rohit::Serializer::Parser::SpaceSeparatedIdentifier(inStream, resultfn);
        EXPECT_EQ(parsedstring, output);
    }
}

TEST(SerializeParser, AccessType) {
    std::vector<std::tuple<std::string, rohit::Serializer::AccessType>> test_list {
        {"public", rohit::Serializer::AccessType::Public},
        {"protected", rohit::Serializer::AccessType::Protected},
        {"private", rohit::Serializer::AccessType::Private},
        {"Public", rohit::Serializer::AccessType::Error},
        {"Protected", rohit::Serializer::AccessType::Error},
        {"Private", rohit::Serializer::AccessType::Error},
        {"", rohit::Serializer::AccessType::Error},
        {"_", rohit::Serializer::AccessType::Error},
        {"public1", rohit::Serializer::AccessType::Error},
        {"protected ", rohit::Serializer::AccessType::Protected},
    };

    for(auto &test: test_list) {
        auto &[input, output ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        if (output != rohit::Serializer::AccessType::Error) {
            auto parsedstring = rohit::Serializer::Parser::ParseAccessType(inStream);
            EXPECT_EQ(parsedstring, output);
        } else {
            EXPECT_THROW(rohit::Serializer::Parser::ParseAccessType(inStream), rohit::exception::BaseParser);
        }
    }
}

TEST(SerializeParser, Member) {
    // tuple list are: source, Member, is negative test
    std::vector<std::tuple<std::string, rohit::Serializer::Member, bool>> test_list {
        {"private \r\n array \r\n\t uint8\t_test\r\n;", {rohit::Serializer::AccessType::Private, rohit::Serializer::Member::array, { {"uint8", nullptr} }, "_test", 1, {}}, false},
        {"public uint8 test;", {rohit::Serializer::AccessType::Public, rohit::Serializer::Member::none, { {"uint8", nullptr} }, "test", 2, {}}, false},
        {"protected \r\n uint8\ttest;", {rohit::Serializer::AccessType::Protected, rohit::Serializer::Member::none, { {"uint8", nullptr} }, "test", 3, {}}, false},
        {"private \r\n newtest\t_test\r\n;", {rohit::Serializer::AccessType::Private, rohit::Serializer::Member::none, { {"newtest", nullptr} }, "_test", 4, {}}, false},
        {"private \r\n 9newtest\t_test\r\n;", {rohit::Serializer::AccessType::Private, rohit::Serializer::Member::none, { {"uint8", nullptr} }, "_test", 5, {}}, true},
    };

    for(auto &test: test_list) {
        auto &[input, output, negativetest ] = test;
        rohit::FullStream inStream { input.data(), input.size() };
        if (!negativetest) {
            auto parsedmember = rohit::Serializer::Parser::ParseMember(inStream, output.id, nullptr);
            EXPECT_EQ(parsedmember, output);
        } else {
            EXPECT_THROW(rohit::Serializer::Parser::ParseMember(inStream, output.id, nullptr), rohit::Serializer::exception::BadIdentifier);
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

    rohit::FullStream inStream { input.data(), input.size() };
    std::string name { "person" };
    std::vector<rohit::Serializer::Parent> parent { };
    rohit::Serializer::Class obj {rohit::Serializer::ObjectType::Class, std::move(name), nullptr, rohit::Serializer::ClassAtributes::None, std::move(parent)};
    uint32_t id { 1 };
    rohit::Serializer::Parser::ParseClassBody(inStream, &obj, id);
    EXPECT_EQ(obj.MemberList.size(), 2);
}

TEST(SerializeParser, CompleteStruct) {
    std::string input {
        "namespace test {\r\n"
        "class person packed {\n"
        "/*Name of the person*/"
        "public string name; "
        "public uint64 ID;\t"
        "}"
        "}"
    };

    rohit::FullStream inStream { input.data(), input.size() };
    auto parsed = rohit::Serializer::Parser::Parse(inStream);
    EXPECT_EQ(parsed.size(), 1);
}

TEST(SerializeParser, CompleteStructWithMap) {
    std::string input {
        "namespace arraytest {"
        "class person {"
        "public string name;"
        "public uint64 ID;}"
        "class personlist {"
        "public uint64 listid;"
        "public map(uint64) person list;}}"
    };

    rohit::FullStream inStream { input.data(), input.size() };
    auto parsed = rohit::Serializer::Parser::Parse(inStream);
    EXPECT_EQ(parsed.size(), 1);
}

TEST(SerializeParser, VariableMember) {
    std::string input {
        R"(
namespace test {
class IP {
    public uint8 a;
    public uint8 b;
    public uint8 c;
    public uint8 d;
}

class serverbase {
    public IP name;
    public uint16 port;
}

class cacheserver : public serverbase {
    public uint32 size;
}

class httpserver : public serverbase {
    public uint32 size;
    public uint32 mimesize;
}


class server {
    // entry_enum enumeration will be created
    // pair<entry_enum, union> will be created
    public union (cacheserver, httpserver) entry;
}

class server1 {
    // entry_enum enumeration will be created
    // pair<entry_enum, union> will be created
    public union (cacheserver = cache, httpserver = http) entry;
}

} // namespace test
)"
    };

    rohit::FullStream inStream { input.data(), input.size() };
    auto parsed = rohit::Serializer::Parser::Parse(inStream);
    EXPECT_EQ(parsed.size(), 1);
}

TEST(SerializeParser, CompleteEnumTest) {
    std::string input {
        R"(
namespace test {
class IP {
    public uint8 a;
    public uint8 b;
    public uint8 c;
    public uint8 d;
}

class serverbase {
    public IP name;
    public uint16 port;
}

class cacheserver : public serverbase {
    public uint32 size;
}

class httpserver : public serverbase {
    public uint32 size;
    public uint32 mimesize;
}


class server {
    // entry_enum enumeration will be created
    // pair<entry_enum, union> will be created
    public union (cacheserver, httpserver) entry;
}

enum test {
    test1,
    test2,
    test3
}

class server1 {
    // entry_enum enumeration will be created
    // pair<entry_enum, union> will be created
    public union (cacheserver = cache, httpserver = http) entry;
}

} // namespace test
)"
    };

    rohit::FullStreamAutoAlloc outStream {128};
    rohit::FullStream inStream { input.data(), input.size() };
    auto statementlist = rohit::Serializer::Parser::Parse(inStream);
    rohit::Serializer::Writer::CPP::Write(outStream, statementlist);
}

