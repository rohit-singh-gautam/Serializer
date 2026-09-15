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
#include <rohit/serializer_creator.hpp>

TEST(serialize_parser, identifier) {
  std::vector<std::tuple<std::string, std::string, bool>> test_list{
      {"a   ", "a", false},
      {"b\"   ", "b", false},
      {"_Test   ", "_Test", false},
      {"9Test   ", "9Test", true},
      {"T39cc03_232_;", "T39cc03_232_", false},
      {"#T39cc03_232_", "#T39cc03_232_", true},
      {"", "_Test", true},
  };

  for (auto& test : test_list) {
    auto& [input, output, negative_test] = test;
    rohit::full_stream in_stream{input.data(), input.size()};
    if (!negative_test) {
      auto parsed_string = rohit::serializer::parser::parse_identifier(in_stream);
      EXPECT_EQ(parsed_string, output);
    } else {
      EXPECT_THROW(rohit::serializer::parser::parse_identifier(in_stream),
                   rohit::serializer::exception::bad_identifier);
    }
  }
}

TEST(serialize_parser, hierarchical_identifier) {
  std::vector<std::tuple<std::string, std::string, bool>> test_list{
      {"_Test   ", "_Test", false},
      {"9Test   ", "9Test", true},
      {"T39cc03_232_;", "T39cc03_232_", false},
      {"#T39cc03_232_", "#T39cc03_232_", true},
      {"", "_Test", true},
      {"rohit::_Test   ", "rohit::_Test", false},
      {"rohit::9Test   ", "rohit::_Test", true},
      {"a::b::c::d::e::fast::_Test   ", "a::b::c::d::e::fast::_Test", false},
  };

  for (auto& test : test_list) {
    auto& [input, output, negative_test] = test;
    rohit::full_stream in_stream{input.data(), input.size()};
    if (!negative_test) {
      auto parsed_string = rohit::serializer::parser::parse_hierarchical_identifier(in_stream);
      EXPECT_EQ(parsed_string, output);
    } else {
      EXPECT_THROW(rohit::serializer::parser::parse_hierarchical_identifier(in_stream),
                   rohit::serializer::exception::bad_identifier);
    }
  }
}

TEST(serialize_parser, space_separated_identifier) {
  std::vector<std::tuple<std::string, std::string>> test_list{
      {"test test1 test2   ", "test test1 test2 "},
      {"9Test   ", ""},
      {"T39cc03_232_;", "T39cc03_232_ "},
      {"#T39cc03_232_", ""},
      {"", ""}};

  for (auto& test : test_list) {
    std::string parsed_string{};
    auto on_identifier = [&parsed_string](std::string&& value) {
      parsed_string += value;
      parsed_string += ' ';
    };
    auto& [input, output] = test;
    rohit::full_stream in_stream{input.data(), input.size()};
    rohit::serializer::parser::space_separated_identifier(in_stream, on_identifier);
    EXPECT_EQ(parsed_string, output);
  }
}

TEST(serialize_parser, access_type) {
  std::vector<std::tuple<std::string, rohit::serializer::access_type>> test_list{
      {"public", rohit::serializer::access_type::public_access},
      {"protected", rohit::serializer::access_type::protected_access},
      {"private", rohit::serializer::access_type::private_access},
      {"Public", rohit::serializer::access_type::error},
      {"Protected", rohit::serializer::access_type::error},
      {"Private", rohit::serializer::access_type::error},
      {"", rohit::serializer::access_type::error},
      {"_", rohit::serializer::access_type::error},
      {"public1", rohit::serializer::access_type::error},
      {"protected ", rohit::serializer::access_type::protected_access},
  };

  for (auto& test : test_list) {
    auto& [input, output] = test;
    rohit::full_stream in_stream{input.data(), input.size()};
    if (output != rohit::serializer::access_type::error) {
      auto parsed_string = rohit::serializer::parser::parse_access_type(in_stream);
      EXPECT_EQ(parsed_string, output);
    } else {
      EXPECT_THROW(rohit::serializer::parser::parse_access_type(in_stream),
                   rohit::exception::base_parser);
    }
  }
}

TEST(serialize_parser, member) {
  // tuple list are: source, Member, is negative test
  std::vector<std::tuple<std::string, rohit::serializer::member, bool>> test_list{
      {"private \r\n array \r\n\t uint8\t_test\r\n;",
       {rohit::serializer::access_type::private_access,
        rohit::serializer::member::modifier_type::array,
        {{"uint8", nullptr}},
        "_test",
        "_test",
        1,
        {},
        {}},
       false},
      {"public uint8 test;",
       {rohit::serializer::access_type::public_access,
        rohit::serializer::member::modifier_type::none,
        {{"uint8", nullptr}},
        "test",
        "test",
        2,
        {},
        {}},
       false},
      {"protected \r\n uint8\ttest;",
       {rohit::serializer::access_type::protected_access,
        rohit::serializer::member::modifier_type::none,
        {{"uint8", nullptr}},
        "test",
        "test",
        3,
        {},
        {}},
       false},
      {"private \r\n newtest\t_test\r\n;",
       {rohit::serializer::access_type::private_access,
        rohit::serializer::member::modifier_type::none,
        {{"newtest", nullptr}},
        "_test",
        "_test",
        4,
        {},
        {}},
       false},
      {"private \r\n 9newtest\t_test\r\n;",
       {rohit::serializer::access_type::private_access,
        rohit::serializer::member::modifier_type::none,
        {{"uint8", nullptr}},
        "_test",
        "_test",
        5,
        {},
        {}},
       true},
  };

  for (auto& test : test_list) {
    auto& [input, output, negative_test] = test;
    rohit::full_stream in_stream{input.data(), input.size()};
    if (!negative_test) {
      auto parsed_member = rohit::serializer::parser::parse_member(in_stream, output.id, nullptr);
      EXPECT_EQ(parsed_member, output);
    } else {
      EXPECT_THROW(rohit::serializer::parser::parse_member(in_stream, output.id, nullptr),
                   rohit::serializer::exception::bad_identifier);
    }
  }
}

TEST(serialize_parser, class_body) {
  std::string input{"{\n"
                    "public string name {\"None\"}(\"Name\"); "
                    "public uint64 ID (\"id\", 3) { 1 };\t"
                    "}"};

  rohit::full_stream in_stream{input.data(), input.size()};
  std::string name{"person"};
  std::vector<rohit::serializer::parent> parent{};
  rohit::serializer::class_node obj{rohit::serializer::object_type::class_type, std::move(name),
                                    nullptr, rohit::serializer::class_attributes::none,
                                    std::move(parent)};
  std::uint32_t id{1};
  rohit::serializer::parser::parse_class_body(in_stream, &obj, id);
  EXPECT_EQ(obj.member_list.size(), 2);
  EXPECT_EQ(obj.member_list[0].name, "name");
  EXPECT_EQ(obj.member_list[0].display_name, "Name");
  EXPECT_EQ(obj.member_list[0].id, 1);
  EXPECT_EQ(obj.member_list[0].default_value, "\"None\"");
  EXPECT_EQ(obj.member_list[1].default_value, "1");
  EXPECT_EQ(obj.member_list[1].name, "ID");
  EXPECT_EQ(obj.member_list[1].display_name, "id");
  EXPECT_EQ(obj.member_list[1].id, 3);
}

TEST(serialize_parser, complete_struct) {
  std::string input{"namespace test {\r\n"
                    "class person packed {\n"
                    "/*Name of the person*/"
                    "public string name; "
                    "public uint64 ID;\t"
                    "}"
                    "}"};

  rohit::full_stream in_stream{input.data(), input.size()};
  auto parsed = rohit::serializer::parser::parse(in_stream);
  EXPECT_EQ(parsed.size(), 1);
}

TEST(serialize_parser, complete_struct_with_map) {
  std::string input{"namespace arraytest {"
                    "class person {"
                    "public string name;"
                    "public uint64 ID;}"
                    "class personlist {"
                    "public uint64 listid;"
                    "public map(uint64) person list;}}"};

  rohit::full_stream in_stream{input.data(), input.size()};
  auto parsed = rohit::serializer::parser::parse(in_stream);
  EXPECT_EQ(parsed.size(), 1);
}

TEST(serialize_parser, bad_struct_with_map) {
  std::string input{"namespace arraytest {"
                    "class person {"
                    "public string name;"
                    "public uint64 ID;}"
                    "class personlist {"
                    "public uint64 listid;"
                    "public map(uint64) person;}}"};

  rohit::full_stream in_stream{input.data(), input.size()};
  EXPECT_THROW(rohit::serializer::parser::parse(in_stream),
               rohit::serializer::exception::bad_identifier);
}

TEST(serialize_parser, bad_test) {
  std::string input{"namespace enumtest {"
                    "enum test {"
                    "\ttest1,"
                    "\ttest2,"
                    "\ttest3,"
                    "\ttest4,"
                    "\ttest5,"
                    "}}"};

  rohit::full_stream in_stream{input.data(), input.size()};
  auto parsed = rohit::serializer::parser::parse(in_stream);
  EXPECT_EQ(parsed.size(), 1);
}

TEST(serialize_parser, variable_member) {
  std::string input{
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
)"};

  rohit::full_stream in_stream{input.data(), input.size()};
  auto parsed = rohit::serializer::parser::parse(in_stream);
  EXPECT_EQ(parsed.size(), 1);
}

TEST(serialize_parser, complete_enum_test) {
  std::string input{
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
)"};

  rohit::full_stream_auto_alloc out_stream{128};
  rohit::full_stream in_stream{input.data(), input.size()};
  auto statements = rohit::serializer::parser::parse(in_stream);
  rohit::serializer::writer::cpp::write(out_stream, statements);
}
