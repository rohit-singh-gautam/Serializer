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

#include <string>
#include <string_view>

// Preserve fixed byte literals and normalize selector aliases before generation.
TEST(serialize_parser, magic_and_format_omission_metadata) {
  const std::string source = "private magic (99) { 'SRL\\0FILE' } omit(json, binary_positional);";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto header = rohit::serializer::parser::parse_member(input, 1, nullptr);
  EXPECT_EQ(header.magic_bytes, std::string("SRL\0FILE", 8));
  EXPECT_EQ(header.access, rohit::serializer::access_type::private_access);
  EXPECT_EQ(header.id, 99);
  EXPECT_TRUE(header.omits("json"));
  EXPECT_TRUE(header.omits("binary_none"));
  EXPECT_FALSE(header.omits("binary_positional"));

  const std::string field_source = "public uint32 value (7) { 42 } omit(protobuf_binary, binary_integer);";
  const auto field_input = rohit::make_constant_stream(field_source.data(), field_source.size());
  const auto field = rohit::serializer::parser::parse_member(field_input, 1, nullptr);
  EXPECT_TRUE(field.omits("protobuf"));
  EXPECT_TRUE(field.omits("binary_integer"));
  EXPECT_EQ(field.default_value, "42");
}

// Reject format typos, repeated aliases, malformed header bytes, and revision exclusion.
TEST(serialize_parser, rejects_invalid_magic_and_omissions) {
  for (const std::string_view source : {
      "private magic { '' };", "private magic { \"wrong quotes\" };",
      "private magic { '\\xFF' };", "private magic { '\\q' };",
      "public uint32 value omit(unknown);", "public uint32 value omit(json, json);",
      "public uint32 value omit(binary_none, binary_positional);",
      "public version { 1 } omit(json);"}) {
    SCOPED_TRACE(source);
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse_member(input, 1, nullptr), std::exception);
  }
}

// Fixed class metadata owns an identity without becoming an instance payload member.
TEST(serialize_parser, magic_does_not_consume_positional_member_ids) {
  const std::string source = "serializer version 1; class file { private magic { 'FILE' }; public uint32 value; }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input, true);
  ASSERT_EQ(statements.size(), 1);
  const auto& object = static_cast<const rohit::serializer::class_node&>(*statements.front());
  ASSERT_EQ(object.member_list.size(), 1);
  EXPECT_EQ(object.member_list.front().id, 1);
  EXPECT_EQ(object.magic_id, 2);
  EXPECT_EQ(object.magic_bytes, "FILE");
}

// Byte-backed views cannot silently retain fields excluded by an owning codec's wire layout.
TEST(serialize_parser, rejects_format_omissions_on_view_classes) {
  for (const std::string_view modes : {"view readonly", "view mutable", "view owning"}) {
    const auto source = "serializer version 1; class record " + std::string(modes) +
        " { public uint32 value omit(json); }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    try {
      static_cast<void>(rohit::serializer::parser::parse(input, true));
      FAIL() << "View omission was accepted: " << modes;
    } catch (const std::exception& error) {
      EXPECT_NE(std::string(error.what()).find("owning class without view modes"), std::string::npos);
    }
  }
}

// Keep literal spaces and quoted delimiters unchanged in schema defaults.
TEST(serialize_parser, quoted_defaults_preserve_spelling) {
  for (const std::string_view literal : {
           R"("schema default")", R"("  leading  and trailing  ")", R"("}")",
           R"("a \"quoted value\" and \\ path")", R"("/* literal comment */")", R"(' ')",
           R"('\'')", R"('}')", R"(1'000)"}) {
    SCOPED_TRACE(literal);
    const std::string source = "public string label { " + std::string{literal} + " };";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    const auto field = rohit::serializer::parser::parse_member(input, 1, nullptr);
    EXPECT_EQ(field.default_value, literal);
    EXPECT_TRUE(input.full());
  }
}

// Reject truncated quotes, dangling escapes, and missing closing braces with schema diagnostics.
TEST(serialize_parser, malformed_quoted_defaults) {
  for (const std::string_view source : {
           R"(public string label { "unfinished space)",
           R"(public string label { "unfinished \)",
           R"(public string label { "closed" )",
           R"(public char label { ' )"}) {
    SCOPED_TRACE(source);
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse_member(input, 1, nullptr),
                 rohit::serializer::exception::bad_member_spec);
  }
}

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
  rohit::serializer::writer::cpp_options options{};
  options.format = false;
  rohit::serializer::writer::cpp::write(out_stream, statements, options);
}
