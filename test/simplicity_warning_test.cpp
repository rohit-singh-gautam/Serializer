// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
namespace schema = rohit::serializer;
constexpr std::string_view magic_warning = "[simplicity-magic]";
constexpr std::string_view version_warning = "[simplicity-version]";

// Parse a complete in-memory source while retaining every synchronous warning.
auto parse_source(std::string_view source, std::vector<std::string>& warnings) {
  schema::parser::parse_options options{};
  options.warning = [&](std::string_view message) { warnings.emplace_back(message); };
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  return schema::parser::parse(input, true, options);
}

// Wrap one member declaration in a current, explicitly numbered owning schema.
std::string model(std::string_view declaration) {
  return "serializer version 1.5.0; class record stable_ids { " +
         std::string{declaration} + " }";
}

// Require a warning category and useful keyword guidance without pinning prose.
void expect_warning(std::string_view declaration, std::string_view category,
                    std::string_view suggestion) {
  SCOPED_TRACE(declaration);
  std::vector<std::string> warnings{};
  EXPECT_NO_THROW(parse_source(model(declaration), warnings));
  ASSERT_EQ(warnings.size(), 1u);
  EXPECT_NE(warnings.front().find(category), std::string::npos);
  EXPECT_NE(warnings.front().find(suggestion), std::string::npos);
}

// Identify explicit manual identities through either exact source or wire names.
TEST(simplicity_warning, recognizes_manual_magic_and_revision_fields) {
  for (const std::string_view declaration : {
           "public uint32 magic (7) { 42 };",
           "private uint32 MAGIC (7) { 42 };",
           "public uint32 tag (\"MaGiC\", 7) { 42 };",
           "public string magic (7) { \"FILE\" };"}) {
    expect_warning(declaration, magic_warning, "magic keyword");
  }
  for (const std::string_view declaration : {
           "public uint32 version (7) { 1 };",
           "public uint32 REVISION (7) { 1 };",
           "public uint32 version (7);",
           "public uint32 revision (7);",
           "public uint32 release (\"VeRsIoN\", 7) { 1 };",
           "public uint32 release (\"revision\", 7);"}) {
    expect_warning(declaration, version_warning, "version keyword");
  }
}

// Every supported discriminator type receives the same advice before generation.
TEST(simplicity_warning, recognizes_all_supported_revision_types) {
  constexpr std::array<std::array<std::string_view, 2>, 9> cases{{
      {{"uint8", "1"}}, {{"uint16", "1"}}, {{"uint32", "1"}}, {{"uint64", "1"}},
      {{"float", "1.5"}}, {{"double", "1.5"}}, {{"version2", "\"1.0\""}},
      {{"version3", "\"1.0.0\""}}, {{"version4", "\"1.0.0.0\""}}}};
  for (const auto& [type, value] : cases) {
    expect_warning("public " + std::string{type} + " revision (7) { " +
                       std::string{value} + " };", version_warning, "version keyword");
  }
}

// Advice remains conservative when a name only resembles metadata or a shape is unsuitable.
TEST(simplicity_warning, avoids_unrelated_names_and_unsupported_revision_shapes) {
  for (const std::string_view declaration : {
           "public uint32 magic (7);",
           "public uint32 magic_value (7) { 42 };",
           "public uint32 software_version (7) { 1 };",
           "public uint32 revision_count (7) { 1 };",
           "public string version (7) { \"application\" };",
           "public bool revision (7) { true };",
           "public int32 version (7) { 1 };",
           "public array uint32 version (7);",
           "public map(string) uint32 revision (7);",
           "public variant(uint32 = number, string = text) version (7);",
           "public magic uint32 (7) { 42 };",
           "private magic (7) { 'FILE' };",
           "public version uint32 (7) { 1 };",
           "public version uint32 Version (7) { 1 };",
           "public version uint32 release (7) { 1 };"}) {
    SCOPED_TRACE(declaration);
    std::vector<std::string> warnings{};
    EXPECT_NO_THROW(parse_source(model(declaration), warnings));
    EXPECT_TRUE(warnings.empty());
  }
}

// Redundant revision names are optional, while deliberately named public members remain valid.
TEST(simplicity_warning, advises_omitting_only_the_redundant_version_name) {
  expect_warning("public version uint32 version (7) { 1 };", version_warning, "omit");
  std::vector<std::string> warnings{};
  const auto nodes = parse_source(
      model("public version uint32 version (\"schema_version\", 7) { 1 } "
            "ignore(warning version);"), warnings);
  ASSERT_EQ(nodes.size(), 1u);
  const auto& record = static_cast<const schema::class_node&>(*nodes.front());
  ASSERT_EQ(record.member_list.size(), 1u);
  EXPECT_TRUE(record.member_list.front().version);
  EXPECT_EQ(record.member_list.front().name, "version");
  EXPECT_EQ(record.member_list.front().display_name, "schema_version");
  EXPECT_EQ(record.member_list.front().id, 7u);
  EXPECT_TRUE(warnings.empty());
}

// Suppression belongs to one declaration and removes only the listed warning categories.
TEST(simplicity_warning, suppresses_selected_warnings_on_one_declaration) {
  std::vector<std::string> warnings{};
  const auto nodes = parse_source(
      model("public uint32 magic (\"version\", 7) { 42 } ignore(warning magic); "
            "public uint32 revision (8) ignore(warning magic, version); "
            "public uint32 MAGIC (9) { 43 };"), warnings);
  ASSERT_EQ(nodes.size(), 1u);
  ASSERT_EQ(warnings.size(), 2u);
  EXPECT_NE(warnings[0].find(version_warning), std::string::npos);
  EXPECT_NE(warnings[1].find(magic_warning), std::string::npos);
}

// Suppression accepts formatting comments and declarations that presently produce no advice.
TEST(simplicity_warning, accepts_known_categories_and_existing_magic_declarations) {
  for (const std::string_view declaration : {
           "public uint32 value (7) ignore(warning magic, version);",
           "public uint32 magic (7) { 42 } "
               "ignore /* why */ ( /* category */ warning /* ids */ magic, /* other */ version );",
           "public magic uint32 (7) { 42 } ignore(warning magic);",
           "private magic (7) { 'FILE' } ignore(warning magic, version);"}) {
    SCOPED_TRACE(declaration);
    std::vector<std::string> warnings{};
    EXPECT_NO_THROW(parse_source(model(declaration), warnings));
    EXPECT_TRUE(warnings.empty());
  }
}

// Compiler advice and its suppression preserve member identities, defaults, and protocol metadata.
TEST(simplicity_warning, leaves_compiler_and_wire_metadata_unchanged) {
  constexpr std::string_view declaration =
      "public uint32 magic (\"version\", 7) { 42 } omit(json)";
  std::vector<std::string> plain_warnings{};
  std::vector<std::string> ignored_warnings{};
  const auto plain_nodes = parse_source(model(std::string{declaration} + ";"), plain_warnings);
  const auto ignored_nodes = parse_source(
      model(std::string{declaration} + " ignore(warning magic, version);"), ignored_warnings);
  const auto& plain = static_cast<const schema::class_node&>(*plain_nodes.front());
  const auto& ignored = static_cast<const schema::class_node&>(*ignored_nodes.front());
  ASSERT_EQ(plain.member_list.size(), 1u);
  ASSERT_EQ(ignored.member_list.size(), 1u);
  const auto& before = plain.member_list.front();
  const auto& after = ignored.member_list.front();
  EXPECT_EQ(before, after);
  EXPECT_EQ(before.name, after.name);
  EXPECT_EQ(before.display_name, after.display_name);
  EXPECT_EQ(before.id, after.id);
  EXPECT_EQ(before.explicit_id, after.explicit_id);
  EXPECT_EQ(before.default_value, after.default_value);
  EXPECT_FALSE(after.magic);
  EXPECT_FALSE(after.version);
  EXPECT_FALSE(ignored.has_magic());
  EXPECT_EQ(plain_warnings.size(), 2u);
  EXPECT_TRUE(ignored_warnings.empty());
}

// Empty, misspelled, repeated, and malformed suppression lists fail instead of silently hiding advice.
TEST(simplicity_warning, rejects_invalid_ignore_suffixes) {
  for (const std::string_view suffix : {
           "ignore();", "ignore(warning);", "ignore(warning unknown);",
           "ignore(warning magic, magic);", "ignore(warning magic, version, version);",
           "ignore(warning magic,);", "ignore(warning ,magic);",
           "ignore(magic);", "ignore(warnings magic);", "ignore warning magic;",
           "ignore(warning magic;", "ignore(warning magic)) ;",
           "ignore(warning magic version);",
           "ignore(warning magic) ignore(warning version);"}) {
    SCOPED_TRACE(suffix);
    const auto source = model("public uint32 magic (7) { 42 } " + std::string{suffix});
    std::vector<std::string> warnings{};
    EXPECT_THROW(parse_source(source, warnings), std::exception);
  }
}

// Every incomplete suffix fails safely even when a member and class terminator follow it.
TEST(simplicity_warning, rejects_truncated_ignore_suffixes) {
  constexpr std::string_view suffix = "ignore(warning magic, version)";
  for (std::size_t length = 1; length < suffix.size(); ++length) {
    SCOPED_TRACE(length);
    const auto source = model("public uint32 magic (7) { 42 } " +
                              std::string{suffix.substr(0, length)} + ";");
    std::vector<std::string> warnings{};
    EXPECT_THROW(parse_source(source, warnings), std::exception);
  }
}

// The suffix is new language syntax; contextual words retain their ordinary identifier meaning.
TEST(simplicity_warning, gates_ignore_without_reserving_identifiers) {
  for (const std::string_view version : {"1", "1.0.0", "1.1.0", "1.2.0", "1.3.0", "1.4.0",
                                        "1.4.999"}) {
    SCOPED_TRACE(version);
    std::vector<std::string> warnings{};
    const auto source = "serializer version " + std::string{version} +
        "; class record { public uint32 magic { 42 } ignore(warning magic); }";
    try {
      static_cast<void>(parse_source(source, warnings));
      FAIL() << "Expected older language contract to reject ignore";
    } catch (const std::exception& error) {
      EXPECT_NE(std::string{error.what()}.find("requires serializer version 1.5.0"),
                std::string::npos) << error.what();
    }
  }
  for (const std::string_view version : {"1", "1.0.0", "1.4.0", "1.5.0"}) {
    SCOPED_TRACE(version);
    std::vector<std::string> warnings{};
    const auto source = "serializer version " + std::string{version} +
        "; class ignore {} class warning {} class record { public ignore warning; "
        "public warning ignore; public uint32 ordinary ignore(warning magic, version); }";
    if (version != "1.5.0") {
      const auto ordinary = "serializer version " + std::string{version} +
          "; class ignore {} class warning {} class record { public ignore warning; "
          "public warning ignore; } class scalars { public uint32 ignore; "
          "public uint32 warning; }";
      EXPECT_NO_THROW(parse_source(ordinary, warnings));
    } else {
      EXPECT_NO_THROW(parse_source(source, warnings));
    }
    EXPECT_TRUE(warnings.empty());
  }
}

// Parse callbacks are optional, and warning delivery preserves the existing abort contract.
TEST(simplicity_warning, reuses_optional_synchronous_warning_callback) {
  const auto source = model("public uint32 magic (7) { 42 }; public uint32 revision (8);");
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  EXPECT_NO_THROW(schema::parser::parse(input, true));
  schema::parser::parse_options options{};
  options.warning = [](std::string_view) { throw std::runtime_error{"warning callback abort"}; };
  const auto aborting = rohit::make_constant_stream(source.data(), source.size());
  EXPECT_THROW(schema::parser::parse(aborting, true, options), std::runtime_error);
}

// A failed compilation delivers no simplicity advice from partially validated declarations.
TEST(simplicity_warning, delays_warnings_until_validation_succeeds) {
  std::vector<std::string> warnings{};
  const auto source = model("public uint32 magic (7) { 42 }; public missing invalid (8);");
  EXPECT_THROW(parse_source(source, warnings), std::exception);
  EXPECT_TRUE(warnings.empty());
}

// One generic source declaration produces one warning even when several concrete layouts are lowered.
TEST(simplicity_warning, warns_once_for_generic_source_declarations) {
  constexpr std::string_view source =
      "serializer version 1.5.0; class box<T> { public uint32 revision; public T value; } "
      "instantiate first = box<uint32>; instantiate second = box<string>;";
  std::vector<std::string> warnings{};
  EXPECT_NO_THROW(parse_source(source, warnings));
  ASSERT_EQ(warnings.size(), 1u);
  EXPECT_NE(warnings.front().find(version_warning), std::string::npos);
}

// A callback may parse another schema without leaking options or suppression to the caller.
TEST(simplicity_warning, preserves_scope_during_reentrant_callbacks) {
  const auto source = model("public uint32 magic (7) { 42 }; public uint32 revision (8);");
  std::vector<std::string> warnings{};
  std::vector<std::string> nested_warnings{};
  schema::parser::parse_options options{};
  options.warning = [&](std::string_view message) {
    warnings.emplace_back(message);
    if (warnings.size() == 1u) {
      EXPECT_NO_THROW(parse_source(
          model("public uint32 magic (7) { 42 } ignore(warning magic);"), nested_warnings));
      constexpr std::string_view member_source = "public uint32 magic { 43 };";
      const auto member_input = rohit::make_constant_stream(member_source.data(), member_source.size());
      EXPECT_NO_THROW(schema::parser::parse_member(member_input, 1, nullptr));
    }
  };
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  EXPECT_NO_THROW(schema::parser::parse(input, true, options));
  ASSERT_EQ(warnings.size(), 2u);
  EXPECT_NE(warnings.front().find(magic_warning), std::string::npos);
  EXPECT_NE(warnings.back().find(version_warning), std::string::npos);
  EXPECT_TRUE(nested_warnings.empty());
}

// Isolate real-source diagnostics and include language contracts in a disposable temporary tree.
class simplicity_warning_file_test : public testing::Test {
protected:
  std::filesystem::path directory{};

  // Give each fixture a unique directory whose files are owned only by this test.
  void SetUp() override {
    directory = std::filesystem::temp_directory_path() /
        ("serializer_simplicity_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
  }

  // Remove only this fixture's temporary tree, preserving unrelated temporary files.
  void TearDown() override {
    std::error_code error{};
    std::filesystem::remove_all(directory, error);
    EXPECT_FALSE(error) << error.message();
  }

  // Write a complete input fixture under the isolated test directory.
  void write(std::string_view name, std::string_view source) const {
    std::ofstream output{directory / name, std::ios::binary};
    output << source;
    ASSERT_TRUE(output.good());
  }

  // Retain diagnostics from the entry source and every transitive dependency.
  auto parse(std::vector<std::string>& warnings) const {
    schema::parser::parse_options options{};
    options.warning = [&](std::string_view message) { warnings.emplace_back(message); };
    return schema::parser::parse_file(directory / "root.serializer", options);
  }
};

// Included diagnostics identify the actual declaration file and its one-based line and column.
TEST_F(simplicity_warning_file_test, reports_source_locations_without_suppression_leaks) {
  write("common.serializer", "serializer version 1.5.0;\nclass common {\n"
        "  public uint32 magic (7) { 42 } ignore(warning magic);\n"
        "  public uint32 revision (8);\n}\n");
  write("root.serializer", "serializer version 1.5.0;\ninclude common;\nclass root {\n"
        "  public uint32 magic (9) { 43 };\n}\n");
  std::vector<std::string> warnings{};
  EXPECT_NO_THROW(parse(warnings));
  ASSERT_EQ(warnings.size(), 2u);
  EXPECT_TRUE(std::regex_search(warnings[0], std::regex{R"(common\.serializer:4:[0-9]+)"}));
  EXPECT_TRUE(std::regex_search(warnings[1], std::regex{R"(root\.serializer:4:[0-9]+)"}));
  EXPECT_NE(warnings[0].find(version_warning), std::string::npos);
  EXPECT_NE(warnings[1].find(magic_warning), std::string::npos);
}

// A dependency's own language header determines whether it may use warning suppression.
TEST_F(simplicity_warning_file_test, evaluates_ignore_language_contract_per_file) {
  write("common.serializer", "serializer version 1.4.0; class common { "
        "public uint32 magic { 42 } ignore(warning magic); }");
  write("root.serializer", "serializer version 1.5.0; include common;");
  std::vector<std::string> warnings{};
  try {
    static_cast<void>(parse(warnings));
    FAIL() << "Expected dependency language contract failure";
  } catch (const std::exception& error) {
    const std::string diagnostic{error.what()};
    EXPECT_NE(diagnostic.find("common.serializer"), std::string::npos);
    EXPECT_NE(diagnostic.find("requires serializer version 1.5.0"), std::string::npos);
  }
  write("common.serializer", "serializer version 1.5.0; class common { "
        "public uint32 magic { 42 } ignore(warning magic); }");
  write("root.serializer", "serializer version 1; include common;");
  warnings.clear();
  EXPECT_NO_THROW(parse(warnings));
  EXPECT_TRUE(warnings.empty());
}
} // namespace
