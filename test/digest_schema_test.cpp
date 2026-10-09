// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/digest.hpp>
#include <rohit/schema_compatibility.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
namespace schema = rohit::serializer;

// Parse source fragments through the same generic lowering used by the compiler.
auto parse_digest_schema(std::string_view text) {
  const auto input = rohit::make_constant_stream(text.data(), text.size());
  return schema::parser::parse(input);
}

// Locate a concrete class without depending on retained generic definition positions.
const schema::class_node& digest_class(
    const std::vector<std::unique_ptr<schema::syntax_node>>& nodes, std::string_view name) {
  for (const auto& node : nodes) {
    if (node->type == schema::object_type::class_type && node->name == name) {
      return static_cast<const schema::class_node&>(*node);
    }
  }
  throw std::logic_error{"Missing digest fixture class"};
}

// Create isolated versioned include fixtures and remove only this test's own directory.
class digest_include_test : public testing::Test {
protected:
  std::filesystem::path directory{};

  // Allocate a unique temporary source directory for this fixture.
  void SetUp() override {
    directory = std::filesystem::temp_directory_path() /
        ("serializer_digest_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
  }

  // Preserve test failures while cleaning this fixture's bounded temporary directory.
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }

  // Write a schema whose notice and version contract belong to its own file.
  void write(std::string_view name, std::string_view contents) const {
    std::ofstream output{directory / name, std::ios::binary};
    output << contents;
    ASSERT_TRUE(output.good());
  }
};
} // namespace

// Bare bytes, explicit lengths, and standard algorithms retain distinct metadata.
TEST(digest_schema, algorithm_widths_and_caller_supplied_storage) {
  struct algorithm_fixture { std::string_view name; std::uint32_t bytes; };
  constexpr algorithm_fixture algorithms[]{
      {"md5", 16}, {"sha1", 20}, {"sha224", 28}, {"sha256", 32},
      {"sha384", 48}, {"sha512", 64}, {"sha512_224", 28}, {"sha512_256", 32},
      {"sha3_224", 28}, {"sha3_256", 32}, {"sha3_384", 48}, {"sha3_512", 64}};
  for (const auto& algorithm : algorithms) {
    SCOPED_TRACE(algorithm.name);
    const auto nodes = parse_digest_schema("class record { public digest(" +
        std::string{algorithm.name} + ") value; }");
    const auto& type = digest_class(nodes, "record").member_list.front().type_name_list.front();
    EXPECT_TRUE(type.is_digest());
    EXPECT_NE(type.digest, rohit::digest_algorithm::none);
    EXPECT_EQ(type.digest_extent, algorithm.bytes);
    EXPECT_EQ(type.digest_extent, rohit::digest_size(type.digest));
  }
  const auto nodes = parse_digest_schema(
      "class record { public digest raw; public digest[32] fixed; public digest[65536] large; }");
  const auto& fields = digest_class(nodes, "record").member_list;
  EXPECT_TRUE(fields[0].type_name_list.front().is_digest());
  EXPECT_EQ(fields[0].type_name_list.front().digest_extent, 0u);
  EXPECT_EQ(fields[0].type_name_list.front().digest, rohit::digest_algorithm::none);
  EXPECT_EQ(fields[1].type_name_list.front().digest_extent, 32u);
  EXPECT_EQ(fields[1].type_name_list.front().digest, rohit::digest_algorithm::none);
  EXPECT_EQ(fields[2].type_name_list.front().digest_extent, 65536u);
}

// Every malformed selector, extent, and unsupported qualifier fails through bounded parsing.
TEST(digest_schema, rejects_invalid_syntax_and_constraints) {
  for (const std::string_view type : {
      "digest()", "digest(sha128)", "digest(sha192)", "digest(SHA256)", "digest(none)",
      "digest(sha256,md5)", "digest[0]", "digest[-1]", "digest[65537]",
      "digest[999999999999999999999999]", "digest[N]", "digest[16+16]", "digest[]",
      "digest(sha256)<uint8>", "digest[32]<uint8>"}) {
    SCOPED_TRACE(type);
    EXPECT_THROW(parse_digest_schema("class record { public " + std::string{type} + " value; }"),
                 rohit::exception::base_parser);
  }
  for (const std::string_view suffix : {"(", "(sha256", "[", "[32", "(sha256)"}) {
    EXPECT_THROW(parse_digest_schema("class record { public digest" + std::string{suffix}),
                 rohit::exception::base_parser);
  }
  EXPECT_THROW(parse_digest_schema("class record { public digest value { 1 }; }"),
               rohit::exception::base_parser);
  EXPECT_THROW(parse_digest_schema("class record { public map(digest) uint32 value; }"),
               rohit::exception::base_parser);
  EXPECT_THROW(parse_digest_schema("class record readonly view { public digest(sha256) value; }"),
               rohit::exception::base_parser);
}

// Older files reject new primitives while existing declarations and generic parameters still work.
TEST(digest_schema, version_gates_preserve_declared_digest_names) {
  for (const std::string_view version : {"1", "1.0.0", "1.1.0", "1.2.0"}) {
    for (const std::string_view type : {"digest", "digest[32]", "digest(sha256)"}) {
      SCOPED_TRACE(version);
      SCOPED_TRACE(type);
      EXPECT_THROW(parse_digest_schema("serializer version " + std::string{version} +
          "; class record { public " + std::string{type} + " value; }"),
          rohit::exception::base_parser);
    }
    const auto legacy = parse_digest_schema("serializer version " + std::string{version} +
        "; class digest { public uint32 value; } class record { public digest payload; }");
    EXPECT_EQ(digest_class(legacy, "record").member_list.front().type_name_list.front().type,
              schema::object_type::class_type);
  }
  const auto parameter = parse_digest_schema(
      "serializer version 1.2.0; class box<digest> { public digest value; } "
      "instantiate number = box<uint32>;");
  schema::writer::cpp_options options;
  options.format = false;
  const auto output = schema::writer::cpp::generate(parameter, options);
  EXPECT_NE(output.find("typename digest"), std::string::npos);
  EXPECT_NE(output.find("digest value"), std::string::npos);
}

// Nested collections and generic specializations retain algorithm and length identities.
TEST(digest_schema, generic_and_collection_metadata) {
  const auto nodes = parse_digest_schema(
      "class box<T> { public T value; } "
      "instantiate legacy = box<digest(md5)>; instantiate custom = box<digest[16]>; "
      "instantiate modern = box<digest(sha256)>; "
      "class record { public array digest(sha256) hashes; "
      "public map(uint32) digest[24] values; public union(digest(md5)=legacy,uint32=count) choice; }");
  const auto& legacy = digest_class(nodes, "legacy").member_list.front().type_name_list.front();
  const auto& custom = digest_class(nodes, "custom").member_list.front().type_name_list.front();
  EXPECT_NE(legacy.digest, custom.digest);
  EXPECT_EQ(legacy.digest_extent, custom.digest_extent);
  EXPECT_EQ(digest_class(nodes, "modern").member_list.front().type_name_list.front().digest_extent, 32u);
  const auto& fields = digest_class(nodes, "record").member_list;
  EXPECT_EQ(fields[0].type_name_list.front().digest, rohit::digest_algorithm::sha256);
  EXPECT_EQ(fields[1].type_name_list.front().digest_extent, 24u);
  EXPECT_EQ(fields[2].type_name_list.front().digest, rohit::digest_algorithm::md5);
  EXPECT_THROW(parse_digest_schema(
      "class box<T> { public T value { 1 }; } instantiate invalid = box<digest[16]>;"),
      rohit::exception::base_parser);
  EXPECT_THROW(parse_digest_schema(
      "class box<T> { public map(T) uint32 values; } instantiate invalid = box<digest>;"),
      rohit::exception::base_parser);
}

// Native storage derives from metadata and digest containers never enter scalar batching.
TEST(digest_schema, cpp_storage_and_explicit_protobuf_diagnostic) {
  const auto nodes = parse_digest_schema(
      "class record { public digest raw; public digest(sha256) standard; "
      "public digest[24] custom; public uint32 tail; }");
  schema::writer::cpp_options options;
  options.format = false;
  const auto output = schema::writer::cpp::generate(nodes, options);
  EXPECT_NE(output.find("::std::vector<::std::uint8_t> raw"), std::string::npos);
  EXPECT_NE(output.find("::std::array<::std::uint8_t, 32> standard"), std::string::npos);
  EXPECT_NE(output.find("::std::array<::std::uint8_t, 24> custom"), std::string::npos);
  options.protobuf = true;
  EXPECT_THROW(schema::writer::cpp::generate(nodes, options), std::invalid_argument);
  options.protobuf = false;
  options.protocols = schema::writer::cpp_protocols::binary_none;
  options.constant_evaluation = true;
  options.emission_only = true;
  const auto emission = schema::writer::cpp::generate(nodes, options);
  EXPECT_NE(emission.find("::std::span<const ::std::uint8_t> raw"), std::string::npos);
}

// The compatibility checker sees changed algorithms even when their output lengths match.
TEST(digest_schema, compatibility_retains_algorithm_and_extent) {
  schema::parser::parsed_schema before{
      parse_digest_schema("class record { public digest(sha256) value; }"), {}};
  schema::parser::parsed_schema same{
      parse_digest_schema("class record { public digest(sha256) value; }"), {}};
  schema::parser::parsed_schema changed{
      parse_digest_schema("class record { public digest(sha3_256) value; }"), {}};
  schema::parser::parsed_schema resized{
      parse_digest_schema("class record { public digest[24] value; }"), {}};
  for (const auto protocol : {schema::compatibility_protocol::binary_none,
       schema::compatibility_protocol::binary_integer, schema::compatibility_protocol::binary_string,
       schema::compatibility_protocol::json}) {
    EXPECT_TRUE(schema::check_schema_compatibility(before, same, protocol).empty());
    EXPECT_FALSE(schema::check_schema_compatibility(before, changed, protocol).empty());
    EXPECT_FALSE(schema::check_schema_compatibility(before, resized, protocol).empty());
  }
}

// Included files retain their own declared version instead of inheriting the root's newer contract.
TEST_F(digest_include_test, preserves_included_contract_gates) {
  write("root.serializer", "serializer version 1.3.0; include child.serializer; class root {}");
  write("child.serializer", "serializer version 1.2.0; class child { public digest value; }");
  EXPECT_THROW(schema::parser::parse_file(directory / "root.serializer"), std::exception);
  write("child.serializer", "serializer version 1.2.0; class digest { public uint32 value; } "
        "class child { public digest value; }");
  const auto parsed = schema::parser::parse_file(directory / "root.serializer");
  EXPECT_EQ(digest_class(parsed.statements, "child").member_list.front().type_name_list.front().type,
            schema::object_type::class_type);
}
