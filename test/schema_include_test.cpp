// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>
#include <includes.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
namespace schema = rohit::serializer;

// Isolate filesystem tests from source fixtures and from other test processes.
class schema_include_test : public testing::Test {
protected:
  std::filesystem::path directory{};

  // Create a unique temporary source directory; every include is relative to these files.
  void SetUp() override {
    directory = std::filesystem::temp_directory_path() /
                ("serializer_includes_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
  }

  // Remove only this fixture's temporary directory without masking a failed assertion.
  void TearDown() override {
    std::error_code error{};
    std::filesystem::remove_all(directory, error);
  }

  // Write a versioned source fixture, creating any relative subdirectory it needs.
  void write(std::string_view name, std::string_view contents) const {
    const auto path = directory / name;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file{path, std::ios::binary};
    file << contents;
    ASSERT_TRUE(file.good());
  }

  // Report a useful diagnostic while requiring the complete compilation to fail.
  void reject(std::string_view expected) const {
    try {
      static_cast<void>(schema::parser::parse_file(directory / "root.serializer"));
      FAIL() << "Expected include failure";
    } catch (const std::exception& error) {
      EXPECT_NE(std::string{error.what()}.find(expected), std::string::npos) << error.what();
      EXPECT_NE(std::string{error.what()}.find("root.serializer"), std::string::npos);
    }
  }
};

// Nested and diamond includes share declaration identities and retain original wire IDs.
TEST_F(schema_include_test, relative_diamond_and_repeated_includes) {
  write("shared/common.serializer",
        "serializer version 1; class account { public uint32 id (17); }");
  write("branch/left.serializer", "serializer version 1; include ../shared/common.serializer; "
                                  "class left { public account value; }");
  write("branch/right.serializer", "serializer version 1; include ../shared/./common.serializer; "
                                   "class right : public account { }");
  write("root.serializer", "serializer version 1; include branch/left.serializer; "
                           "include branch/right.serializer; include shared/common.serializer; "
                           "class request { public left a; public right b; }");
  const auto parsed = schema::parser::parse_file(directory / "root.serializer");
  ASSERT_EQ(parsed.dependencies.size(), 4u);
  ASSERT_EQ(parsed.statements.size(), 4u);
  const auto& account = static_cast<const schema::class_node&>(*parsed.statements[0]);
  const auto& left = static_cast<const schema::class_node&>(*parsed.statements[1]);
  const auto& right = static_cast<const schema::class_node&>(*parsed.statements[2]);
  EXPECT_EQ(account.member_list[0].id, 17u);
  EXPECT_EQ(left.member_list[0].type_name_list[0].resolved_node, &account);
  EXPECT_EQ(right.parents[0].parent_class, &account);
  // A second entry compilation owns a fresh include cache and fresh declarations.
  EXPECT_EQ(schema::parser::parse_file(directory / "root.serializer").statements.size(), 4u);
}

// Includes accept comments/newlines between tokens while their path remains one bare token.
TEST_F(schema_include_test, directive_comments) {
  write("common.serializer", "serializer version 1; class account {}");
  write("root.serializer",
        "serializer version 1; include/*why*/\n"
        "common.serializer /*where*/ ; // done\n class request { public account owner; }");
  EXPECT_EQ(schema::parser::parse_file(directory / "root.serializer").statements.size(), 2u);
}

// Reject malformed tokens and includes in namespaces or after declarations.
TEST_F(schema_include_test, invalid_directives) {
  write("common.serializer", "serializer version 1; class account {}");
  constexpr std::string_view invalid[] = {"include;",
                                          "include",
                                          "include common.serializer",
                                          "include common.serializer extra;",
                                          "include \"common.serializer\";",
                                          "include <common.serializer>;",
                                          "include common.hpp;",
                                          "include /common.serializer;",
                                          "include C:/common.serializer;",
                                          "include folder\\common.serializer;",
                                          "include common file.serializer;",
                                          "namespace models { include common.serializer; }",
                                          "class earlier {} include common.serializer;",
                                          "include_common.serializer;"};
  for (const auto directive : invalid) {
    SCOPED_TRACE(directive);
    write("root.serializer", "serializer version 1; " + std::string{directive});
    reject("root.serializer");
  }
}

// Cycles report the complete include chain instead of silently suppressing an active file.
TEST_F(schema_include_test, include_cycles) {
  write("root.serializer", "serializer version 1; include child.serializer;");
  write("child.serializer", "serializer version 1; include root.serializer;");
  reject("Include cycle");
  reject("child.serializer");
  write("root.serializer", "serializer version 1; include ./root.serializer;");
  reject("Include cycle");
}

// Errors in an included source identify that source, including missing/versionless files.
TEST_F(schema_include_test, dependency_errors) {
  write("root.serializer", "serializer version 1; include missing.serializer;");
  reject("missing.serializer");
  write("missing.serializer", "class account {}");
  reject("Expected first statement");
  write("missing.serializer", "serializer version 2; class account {}");
  reject("version");
  write("missing.serializer", "serializer version 1; class account { public absent value; }");
  reject("Unknown type: absent");
}

// Distinct files cannot redeclare a qualified type; include-once does not hide such conflicts.
TEST_F(schema_include_test, duplicate_types) {
  write("common.serializer", "serializer version 1; namespace models { class account {} }");
  write("root.serializer", "serializer version 1; include common.serializer; "
                           "namespace models { class account {} }");
  reject("Duplicate type: models::account");
  write("root.serializer", "serializer version 1; include common.serializer; "
                           "namespace models { enum account { ready } }");
  reject("Duplicate type: models::account");
}

// Includes keep declaration-before-use rules; an includer cannot supply a dependency's types.
TEST_F(schema_include_test, unresolved_forward_references) {
  write("common.serializer", "serializer version 1; class first { public later value; }");
  write("root.serializer", "serializer version 1; include common.serializer; class later {}");
  reject("Unknown type: later");
}

// Namespace blocks share their scope across files, while equal leaf names in other scopes coexist.
TEST_F(schema_include_test, reopened_namespaces_and_distinct_scopes) {
  write("common.serializer", "serializer version 1; namespace models { class account {} } "
                             "namespace other { class account {} }");
  write("root.serializer", "serializer version 1; include common.serializer; "
                           "namespace models { class request { public account owner; } }");
  const auto parsed = schema::parser::parse_file(directory / "root.serializer");
  rohit::full_stream_auto_alloc output{};
  schema::writer::java::write(output, parsed.statements, "Schema");
  const std::string generated{reinterpret_cast<const char*>(output.begin()),
                              output.current_offset()};
  const auto first = generated.find("class Models");
  ASSERT_NE(first, std::string::npos);
  EXPECT_EQ(generated.find("class Models", first + 1), std::string::npos);
  EXPECT_NE(generated.find("class Other"), std::string::npos);
  EXPECT_NE(generated.find("Schema.Models.Account owner"), std::string::npos);
}

// Namespace/type collisions are rejected in either order, including shorthand namespace prefixes.
TEST_F(schema_include_test, namespace_type_conflicts) {
  constexpr std::string_view invalid[] = {
      "namespace models {} class models {}",
      "class models {} namespace models {}",
      "enum models { ready } namespace models {}",
      "namespace models {} enum models { ready }",
      "class models {} namespace models::nested {}",
      "namespace models::nested {} class models {}",
      "namespace models { class nested {} } namespace models::nested {}",
      "namespace models { namespace nested {} class nested {} }"};
  for (const auto source : invalid) {
    SCOPED_TRACE(source);
    write("root.serializer", "serializer version 1; " + std::string{source});
    reject("Namespace/type name conflict");
  }
  write("root.serializer", "serializer version 1; namespace models {} "
                           "class request { public models invalid; }");
  reject("Namespace cannot be used as a type");
}

// Excessive acyclic nesting fails predictably before exhausting the native call stack.
TEST_F(schema_include_test, bounded_include_depth) {
  constexpr unsigned file_count = 32;
  write("root.serializer", "serializer version 1; include file1.serializer;");
  for (unsigned index = 1; index < file_count; ++index) {
    write("file" + std::to_string(index) + ".serializer",
          "serializer version 1; include file" + std::to_string(index + 1) + ".serializer;");
  }
  write("file31.serializer", "serializer version 1;");
  EXPECT_EQ(schema::parser::parse_file(directory / "root.serializer").dependencies.size(),
            file_count);
  write("file31.serializer", "serializer version 1; include file32.serializer;");
  write("file32.serializer", "serializer version 1;");
  reject("Maximum include depth");
}

// Equivalent nested and shorthand declarations share the scope as their children are created.
TEST_F(schema_include_test, canonical_namespace_at_creation) {
  write("common.serializer", "serializer version 1; namespace models::nested { class account {} }");
  write("root.serializer",
        "serializer version 1; include common.serializer; "
        "namespace models { namespace nested { class request { public account owner; } } }");
  const auto parsed = schema::parser::parse_file(directory / "root.serializer");
  const auto& first = static_cast<const schema::namespace_node&>(*parsed.statements.at(0));
  const auto& second = static_cast<const schema::namespace_node&>(*parsed.statements.at(1));
  const auto& first_nested = static_cast<const schema::namespace_node&>(*first.statements.at(0));
  const auto& second_nested = static_cast<const schema::namespace_node&>(*second.statements.at(0));
  const auto& account = *first_nested.statements.at(0);
  const auto& request = static_cast<const schema::class_node&>(*second_nested.statements.at(0));
  EXPECT_EQ(account.parent_namespace, request.parent_namespace);
  EXPECT_EQ(request.member_list[0].type_name_list[0].declared_namespace, account.parent_namespace);
  EXPECT_EQ(request.member_list[0].type_name_list[0].resolved_node, &account);
  rohit::full_stream_auto_alloc java{};
  EXPECT_NO_THROW(schema::writer::java::write(java, parsed.statements, "Schema"));
}

// A duplicate declaration is diagnosed before parsing the second declaration's invalid body.
TEST_F(schema_include_test, duplicate_rejected_during_creation) {
  write("root.serializer", "serializer version 1; namespace models { class account {} } "
                           "namespace models { class account { invalid body");
  reject("Duplicate type: models::account");
  write("root.serializer", "serializer version 1; enum status { ready } enum status { invalid!");
  reject("Duplicate type: status");
}

// Bare stream callers must opt into filesystem loading explicitly.
TEST(schema_include, stream_api_requires_file_context) {
  constexpr std::string_view text{"serializer version 1; include common.serializer;"};
  const auto input = rohit::make_constant_stream(text.data(), text.size());
  EXPECT_THROW(schema::parser::parse(input, true), schema::exception::bad_object_type);
}

// Compile and exercise the actual generated header for the maintained diamond include fixture.
TEST(schema_include, generated_cpp_round_trip) {
  include_demo::request source{};
  source.first.owner.id = 73;
  source.second.id = 29;
  source.second.label = "included";
  rohit::full_stream_auto_alloc output{};
  source.serialize_out<schema::json>(output);
  const auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
  include_demo::request decoded{};
  decoded.serialize_in<schema::json>(input);
  EXPECT_EQ(decoded.first.owner.id, 73u);
  EXPECT_EQ(decoded.first.owner.status, include_demo::state::ready);
  EXPECT_EQ(decoded.second.id, 29u);
  EXPECT_EQ(decoded.second.label, "included");
}
} // namespace
