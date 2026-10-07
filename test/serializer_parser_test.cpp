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

#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// New modifier spellings never steal ordinary types or generic operands from older schemas.
TEST(serialize_parser, compact_keywords_preserve_existing_type_names) {
  for (const std::string_view version : {"1", "1.0.0", "1.1.0", "1.2.0"}) {
    const auto source = "serializer version " + std::string{version} +
        "; class compact_prefix {} class compact_varint {} class strict {} class lenient {} "
        "class record { public compact_prefix prefix omit(json); public compact_varint varint omit(binary_string); "
        "public strict checked; public lenient relaxed; public compact_prefix strict; }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    const auto statements = rohit::serializer::parser::parse(input, true);
    const auto& object = static_cast<const rohit::serializer::class_node&>(*statements.back());
    ASSERT_EQ(object.member_list.size(), 5u);
    for (const auto& field : object.member_list) {
      EXPECT_EQ(field.compact, rohit::serializer::compact_encoding::none);
      EXPECT_EQ(field.type_name_list.front().type, rohit::serializer::object_type::class_type);
    }
  }
  for (const std::string_view body : {
           "class compact_prefix<T> { public T value; } class record { public compact_prefix<uint32> value; }",
           "class box<compact_prefix> { public compact_prefix value; } instantiate record = box<uint32>;",
           "namespace compact_varint { class nested {} } class record { public compact_varint::nested value; }"}) {
    const auto source = "serializer version 1.0.0; " + std::string{body};
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_NO_THROW(rohit::serializer::parser::parse(input, true));
  }
}

// Compact encodings preserve unsigned host types, field IDs, and explicit overflow policies.
TEST(serialize_parser, compact_integer_metadata_and_defaults) {
  using encoding = rohit::serializer::compact_encoding;
  for (const auto& [modifier, expected, strict] :
       std::vector<std::tuple<std::string, encoding, bool>>{
           {"compact_prefix", encoding::prefix, true},
           {"compact_prefix strict", encoding::prefix, true},
           {"compact_prefix lenient", encoding::prefix, false},
           {"compact_varint", encoding::varint, true},
           {"compact_varint strict", encoding::varint, true},
           {"compact_varint lenient", encoding::varint, false}}) {
    for (const std::string_view type : {"uint8", "uint16", "uint32", "uint64"}) {
      SCOPED_TRACE(modifier + " " + std::string{type});
      const auto source = "serializer version 1.2.0; class record stable_ids { public " +
          modifier + " " + std::string{type} + " value (3) { 0x20 }; }";
      const auto input = rohit::make_constant_stream(source.data(), source.size());
      const auto statements = rohit::serializer::parser::parse(input, true);
      const auto& object = static_cast<const rohit::serializer::class_node&>(*statements.front());
      ASSERT_EQ(object.member_list.size(), 1u);
      const auto& field = object.member_list.front();
      EXPECT_EQ(field.compact, expected);
      EXPECT_EQ(field.compact_strict, strict);
      EXPECT_EQ(field.type_name_list.front().name, type);
      EXPECT_EQ(field.id, 3u);
      EXPECT_EQ(field.default_value, "32");
    }
  }
  for (const std::string_view declaration : {
           "public compact_prefix uint32 value { 1073741823 };",
           "public compact_prefix uint32 omit (3);",
           "public compact_varint uint64 omit (\"encoded\", 3);",
           "public compact_prefix lenient uint32 value { 4294967295 };",
           "public compact_prefix lenient uint64 value { 18446744073709551615 };",
           "public compact_varint uint64 value { 18446744073709551615 };"}) {
    const auto source = "serializer version 1.2.0; class record { " + std::string{declaration} + " }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_NO_THROW(rohit::serializer::parser::parse(input, true));
  }
}

// Unsupported compact shapes and known literal overflows fail before generated files are written.
TEST(serialize_parser, rejects_invalid_compact_integer_fields) {
  for (const std::string_view declaration : {
           "public compact_prefix uint32 value { 1073741824 };",
           "public compact_prefix strict uint64 value { 0x40000000 };",
           "public compact_prefix lenient uint8 value { 256 };",
           "public compact_varint uint16 value { 65536 };",
           "public compact_varint uint64 value { 18446744073709551616 };",
           "public compact_varint uint32 value { -1 };",
           "public compact_prefix uint32 value { expression };",
           "public compact_prefix strict lenient uint32 value;",
           "public compact_varint compact_prefix uint32 value;",
           "public compact_prefix int32 value;", "public compact_varint float value;",
           "public compact_varint double value;", "public compact_varint bool value;",
           "public compact_varint char value;", "public compact_varint string value;",
           "public compact_prefix kind value;", "public compact_varint nested value;",
           "public compact_prefix array uint32 values;",
           "public compact_varint map(string) uint32 values;",
           "public compact_varint union(uint32 = one, uint64 = two) value;",
           "public compact_prefix version uint32 value { 1 };",
           "public compact_varint magic uint32 { 1 };"}) {
    SCOPED_TRACE(declaration);
    const auto source = "serializer version 1.2.0; enum kind { first } class nested {} "
        "class record { " + std::string{declaration} + " }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
  }
  for (const std::string_view mode : {"view", "view owning", "packed"}) {
    const auto source = "serializer version 1.2.0; class record " + std::string{mode} +
        " { public compact_varint uint32 value; }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
  }
}

// Dependent unsigned types are constrained at specialization, including unused invalid templates.
TEST(serialize_parser, compact_generic_constraints_and_language_gates) {
  for (const std::string_view type : {"uint8", "uint16", "uint32", "uint64"}) {
    const auto source = "serializer version 1.2.0; class box<T> { public compact_varint T value; } "
        "instantiate record = box<" + std::string{type} + ">;";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_NO_THROW(rohit::serializer::parser::parse(input, true));
  }
  for (const std::string_view body : {
           "class box<T> { public compact_prefix float value; }",
           "class box<T> { public compact_prefix T value; } instantiate record = box<int32>;",
           "class box<T> { public compact_prefix T value { 256 }; } instantiate record = box<uint8>;"}) {
    const auto source = "serializer version 1.2.0; " + std::string{body};
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
  }
  for (const std::string_view version : {"1", "1.0.0", "1.1.0", "1.1.999"}) {
    for (const std::string_view modifier : {"compact_prefix", "compact_varint"}) {
      const auto source = "serializer version " + std::string{version} + "; class record { public " +
          std::string{modifier} + " uint32 value; }";
      const auto input = rohit::make_constant_stream(source.data(), source.size());
      try {
        rohit::serializer::parser::parse(input, true);
        FAIL() << source;
      } catch (const std::exception& error) {
        EXPECT_NE(std::string{error.what()}.find("requires serializer version 1.2.0"),
                  std::string::npos);
      }
    }
  }
}

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

// Typed identities remain static metadata and normalize hex without consuming payload identities.
TEST(serialize_parser, typed_magic_metadata_and_checked_scalars) {
  for (const auto& [type, literal, normalized] :
       std::vector<std::tuple<std::string, std::string, std::string>>{
           {"uint32", "0x534552", "5457234"}, {"int8", "-0x80", "-128"},
           {"uint64", "0xffffffffffffffff", "18446744073709551615"},
           {"int64", "-0x8000000000000000", "-9223372036854775808"},
           {"char", "'\\0'", "0"}, {"bool", "true", "true"},
           {"float", "-1.5e2", "-150.0"}, {"double", "1e200", "1e+200"}}) {
    SCOPED_TRACE(type);
    const auto source = "serializer version 1.1.0; class record { private magic " + type +
        " (99) { " + literal + " } omit(json); public uint32 value; }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    const auto statements = rohit::serializer::parser::parse(input, true);
    const auto& object = static_cast<const rohit::serializer::class_node&>(*statements.front());
    ASSERT_TRUE(object.has_magic());
    ASSERT_TRUE(object.magic_field);
    EXPECT_TRUE(object.magic_bytes.empty());
    EXPECT_TRUE(object.magic_field->magic);
    EXPECT_EQ(object.magic_field->type_name_list.front().name, type);
    EXPECT_EQ(object.magic_field->default_value, normalized);
    EXPECT_EQ(object.magic_id, 99);
    EXPECT_TRUE(object.magic_omits("json"));
    ASSERT_EQ(object.member_list.size(), 1);
    EXPECT_EQ(object.member_list.front().id, 1);
  }
}

// Enum constants resolve through normal namespace visibility, including generic schema clones.
TEST(serialize_parser, typed_magic_enum_and_generic_metadata) {
  const std::string source =
      "serializer version 1.1.0; namespace kinds { enum format { first, second } "
      "class box<T> { public magic format (9) { kinds::format::second }; public T value; } "
      "instantiate number_box = box<uint32>; }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input, true);
  const auto& scope = static_cast<const rohit::serializer::namespace_node&>(*statements.back());
  const auto& object = static_cast<const rohit::serializer::class_node&>(*scope.statements.back());
  ASSERT_TRUE(object.magic_field);
  EXPECT_EQ(object.magic_field->type_name_list.front().type,
            rohit::serializer::object_type::enum_type);
  EXPECT_EQ(object.magic_field->default_value, "second");
  EXPECT_EQ(object.magic_id, 9);
}

// Unsupported types and nonconstant, overflowing, or nonfinite identities fail before generation.
TEST(serialize_parser, rejects_invalid_typed_magic) {
  for (const std::string_view declaration : {
           "private magic uint8 { 256 };", "private magic uint8 { -1 };",
           "private magic int8 { 128 };", "private magic int8 { -129 };",
           "private magic uint64 { 0x10000000000000000 };",
           "private magic bool { 1 };", "private magic char { 'ab' };",
           "private magic float { 1e100 };", "private magic double { nan };",
           "private magic string { \"identifier\" };",
           "private magic version2 { \"1.0\" };",
           "private magic array uint8 { 1 };",
           "private magic kind { absent };",
           "private magic kind { other::first };",
           "private magic nested { 1 };",
           "private magic uint32;", "private magic uint32 { value };",
           "private magic uint32 { 1 }; public magic { 'FILE' };",
           "private magic { 'FILE' }; private magic uint32 { 1 };",
           "private magic uint32 { 1 }; public uint32 magic;"}) {
    SCOPED_TRACE(declaration);
    const auto source = "serializer version 1.1.0; enum kind { first } "
        "class nested { public uint32 value; } class record { " + std::string{declaration} + " }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
  }
  const std::string generic = "serializer version 1.1.0; class box<T> { private magic T { 1 }; }";
  const auto input = rohit::make_constant_stream(generic.data(), generic.size());
  EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
}

// Inferred arrays count explicit values or decoded text bytes without a trailing NUL.
TEST(serialize_parser, inferred_array_extent_and_char_bytes) {
  for (const auto& [type, initializer, count, normalized] :
       std::vector<std::tuple<std::string, std::string, std::uint64_t, std::string>>{
           {"uint32", "1, 2, 3,", 3, "1, 2, 3"},
           {"char", "'SRL\\0FILE'", 8, "83, 82, 76, 0, 70, 73, 76, 69"},
           {"char", "\"é\"", 2, "195, 169"}, {"char", "'a', '\\0', 'b'", 3, "97, 0, 98"},
           {"bool", "true, false", 2, "true, false"}}) {
    SCOPED_TRACE(initializer);
    const auto source = "serializer version 1.1.0; class record { public array[] " + type +
        " values { " + initializer + " }; public array char variable; }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    const auto statements = rohit::serializer::parser::parse(input, true);
    const auto& object = static_cast<const rohit::serializer::class_node&>(*statements.front());
    const auto& field = object.member_list.front();
    EXPECT_TRUE(field.inferred_extent);
    EXPECT_EQ(field.fixed_extent, count);
    ASSERT_EQ(field.extent_expression.size(), 1);
    EXPECT_EQ(field.extent_expression.front().value, count);
    EXPECT_EQ(field.default_value, normalized);
    EXPECT_FALSE(object.member_list.back().inferred_extent);
    EXPECT_EQ(object.member_list.back().fixed_extent, 0);
  }
}

// Enum list defaults retain declaration identity instead of leaking unqualified C++ tokens.
TEST(serialize_parser, inferred_enum_array_defaults) {
  const std::string source = "serializer version 1.1.0; namespace kinds { enum kind { first, second } "
      "class record { public array[] kind values { first, kind::second }; } }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input, true);
  const auto* scope = dynamic_cast<const rohit::serializer::namespace_node*>(statements.back().get());
  ASSERT_NE(scope, nullptr);
  ASSERT_EQ(scope->statements.size(), 1);
  const auto* object = dynamic_cast<const rohit::serializer::class_node*>(scope->statements.front().get());
  ASSERT_NE(object, nullptr);
  ASSERT_EQ(object->member_list.size(), 1);
  EXPECT_EQ(object->member_list.front().default_value, "kinds::kind::first, kinds::kind::second");
}

// Empty, malformed, and excessive inferred initializers cannot become zero-sized fixed storage.
TEST(serialize_parser, rejects_invalid_inferred_array_initializers) {
  for (const std::string_view declaration : {
           "public array[] char values;", "public array[] char values { '' };",
           "public array[] uint8 values { };", "public array[] uint8 values { ,1 };",
           "public array[] uint8 values { 1,,2 };", "public array[] uint8 values { 1 2 };",
           "public array[] char values { 256 };", "public array[] char values { -1 };",
           "public array[] char values { '\\q' };",
           "public array[] char values { 'unfinished };",
           "public array[] uint8 values { 1) };"}) {
    SCOPED_TRACE(declaration);
    const auto source = "serializer version 1.1.0; class record { " + std::string{declaration} + " }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
  }
  const auto source = "serializer version 1.1.0; class record { public array[] char values { \"" +
      std::string(65537, 'x') + "\" }; }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
}

// Language 1.0 schemas retain their original syntax while current fragments accept the additions.
TEST(serialize_parser, typed_magic_and_inferred_arrays_require_language_1_1) {
  for (const std::string_view version : {"1", "1.0.0", "1.0.1"}) {
    for (const std::string_view declaration : {
             "private magic uint32 { 42 };", "public array[] char values { 'FILE' };"}) {
      const auto source = "serializer version " + std::string{version} +
          "; class record { " + std::string{declaration} + " }";
      const auto input = rohit::make_constant_stream(source.data(), source.size());
      try {
        static_cast<void>(rohit::serializer::parser::parse(input, true));
        FAIL() << "An older language accepted new syntax";
      } catch (const std::exception& error) {
        EXPECT_NE(std::string{error.what()}.find("requires serializer version 1.1.0"),
                  std::string::npos);
      }
    }
  }
  const std::string fragment = "class record { private magic uint32 { 42 }; "
      "public array[] char values { 'FILE' }; }";
  const auto input = rohit::make_constant_stream(fragment.data(), fragment.size());
  EXPECT_NO_THROW(rohit::serializer::parser::parse(input));
}

// Balanced aggregate elements retain inner commas while only the outer list determines its extent.
TEST(serialize_parser, inferred_nested_aggregate_elements) {
  const std::string source = R"(serializer version 1.1.0;
    class entry { public uint32 id; public string label; }
    class record { public array[] entry entries { {1, "a,b"}, {2, "c"} }; }
  )";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input, true);
  const auto& object = static_cast<const rohit::serializer::class_node&>(*statements.back());
  EXPECT_EQ(object.member_list.front().fixed_extent, 2);
  EXPECT_EQ(object.member_list.front().default_value, R"({1, "a,b"}, {2, "c"})");
}

// Parentheses and nested aggregates must close before the outer inferred initializer list.
TEST(serialize_parser, rejects_unbalanced_or_excessive_inferred_groups) {
  for (const std::string_view initializer : {
           "{ {1}, {2 };", "{ (1,2] };", "{ [1,2) };", "{ {1,2) };", "{ 1) };",
           "{ (1, 2 };", "{ \"unfinished };", "{ {'a}, {'b'} };"}) {
    SCOPED_TRACE(initializer);
    const auto source = "serializer version 1.1.0; class record { public array[] uint32 values " +
        std::string{initializer} + " }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
  }
  const auto source = "serializer version 1.1.0; class record { public array[] uint32 values { " +
      std::string(33, '(') + "1" + std::string(33, ')') + " }; }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
}

// Canonical floating identities compare their declared wire precision instead of source spelling.
TEST(serialize_parser, typed_magic_float_constants_use_canonical_wire_values) {
  std::string expected{};
  for (const std::string_view literal : {"0.1", "0.10", "1e-1", "0.10000000149011612"}) {
    const auto source = "serializer version 1.1.0; class record { private magic float { " +
        std::string{literal} + " }; }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    const auto statements = rohit::serializer::parser::parse(input, true);
    const auto& object = static_cast<const rohit::serializer::class_node&>(*statements.front());
    if (expected.empty()) {
      expected = object.magic_field->default_value;
    }
    EXPECT_EQ(object.magic_field->default_value, expected);
  }
}

// Signed floating zero keeps its sign in the canonical literal used by every generator.
TEST(serialize_parser, typed_magic_float_preserves_negative_zero) {
  const std::string source = "serializer version 1.1.0; class record { private magic float { -0 }; }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = rohit::serializer::parser::parse(input, true);
  const auto& object = static_cast<const rohit::serializer::class_node&>(*statements.front());
  EXPECT_EQ(object.magic_field->default_value, "-0.0");
}

// Arbitrary char header bytes remain available when the ASCII native JSON representation is omitted.
TEST(serialize_parser, non_ascii_char_magic_requires_native_json_omission) {
  for (const std::string_view literal : {"'\\xFF'", "128", "255"}) {
    const auto declaration = "serializer version 1.1.0; class record { private magic char { " +
        std::string{literal} + " }";
    {
      const auto source = declaration + "; }";
      const auto input = rohit::make_constant_stream(source.data(), source.size());
      EXPECT_THROW(rohit::serializer::parser::parse(input, true), std::exception);
    }
    {
      const auto source = declaration + " omit(json); }";
      const auto input = rohit::make_constant_stream(source.data(), source.size());
      EXPECT_NO_THROW(rohit::serializer::parser::parse(input, true));
    }
  }
}
