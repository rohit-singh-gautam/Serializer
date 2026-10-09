// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
namespace schema = rohit::serializer;
constexpr std::array languages{"cpp", "java", "js", "typescript", "go", "csharp", "rust",
                               "python", "swift", "kotlin", "c"};

// Exercise every production generator while avoiding an external C++ formatter in unit assertions.
std::string generate(const std::vector<std::unique_ptr<schema::syntax_node>>& statements,
                     std::string_view language) {
  if (language == "cpp") {
    schema::writer::cpp_options options{};
    options.format = false;
    return schema::writer::cpp::generate(statements, options);
  }
  if (language == "java") {
    return schema::writer::java::generate(statements, "Models");
  }
  return schema::writer::portable::generate(statements, language, "Models");
}

// Keep file-level metadata when the source has no declarations to carry it.
std::string generate(const schema::parser::parsed_schema& parsed, std::string_view language) {
  if (language == "cpp") {
    schema::writer::cpp_options options{};
    options.format = false;
    return schema::writer::cpp::generate_schema(parsed, options);
  }
  if (language == "java") {
    return schema::writer::java::generate_schema(parsed, "Models");
  }
  return schema::writer::portable::generate_schema(parsed, language, "Models");
}

// Give include tests their own temporary directory and remove it with checked, local RAII cleanup.
class generated_notice_test : public testing::Test {
protected:
  std::filesystem::path directory{};

  // Create a unique directory under the operating system's temporary root.
  void SetUp() override {
    directory = std::filesystem::temp_directory_path() /
        ("serializer_notices_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory));
  }

  // Delete only the directory created by this fixture, including failed-test sources.
  void TearDown() override {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    EXPECT_FALSE(error) << error.message();
  }

  // Write one versioned source fixture whose includes resolve relative to this directory.
  void write(std::string_view filename, std::string_view contents) const {
    std::ofstream file{directory / filename, std::ios::binary};
    file << contents;
    ASSERT_TRUE(file.good());
  }
};
} // namespace

// Full selected comment groups survive public buffer parsing and every generated language.
TEST(generated_notices, preserves_leading_source_groups_in_all_languages) {
  constexpr std::string_view source =
      "// Unrelated leading explanation.\n\n"
      "// copyright 2026 Example Application Authors\n"
      "// All application rights reserved.\n\n"
      "/* SPDX-License-Identifier: LicenseRef-Example-Proprietary\n"
      " * Terms are defined by the schema author. */\n"
      "/* An unrelated comment block. */\n"
      "serializer version 1;\n"
      "// Copyright 2026 A non-header comment.\n"
      "namespace example { class payload { public uint32 value; } }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = schema::parser::parse(input, true);
  for (const auto language : languages) {
    SCOPED_TRACE(language);
    const auto output = generate(statements, language);
    const auto prefix = language == std::string_view{"python"} ? "# " : "// ";
    EXPECT_NE(output.find(std::string{prefix} + "copyright 2026 Example Application Authors"),
              std::string::npos);
    EXPECT_NE(output.find(std::string{prefix} + "All application rights reserved."),
              std::string::npos);
    EXPECT_NE(output.find(std::string{prefix} +
                         "SPDX-License-Identifier: LicenseRef-Example-Proprietary"),
              std::string::npos);
    EXPECT_NE(output.find("Terms are defined by the schema author."), std::string::npos);
    EXPECT_EQ(output.find("Unrelated leading explanation."), std::string::npos);
    EXPECT_EQ(output.find("An unrelated comment block."), std::string::npos);
    EXPECT_EQ(output.find("A non-header comment."), std::string::npos);
    EXPECT_EQ(output.find("GPL-3.0"), std::string::npos);
  }
}

// A schema without notices receives permission information without an invented copyright/license.
TEST(generated_notices, does_not_assign_schema_ownership_or_license) {
  constexpr std::string_view source = "class payload { public uint32 value; }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = schema::parser::parse(input);
  for (const auto language : languages) {
    SCOPED_TRACE(language);
    const auto output = generate(statements, language);
    EXPECT_NE(output.find("Serializer-authored support is available under 0BSD"),
              std::string::npos);
    EXPECT_EQ(output.find("Copyright"), std::string::npos);
    EXPECT_EQ(output.find("SPDX-License-Identifier:"), std::string::npos);
    EXPECT_EQ(output.find("GNU General Public License"), std::string::npos);
  }
}

// Include-once traversal keeps entry-only notices and distinct dependency notices in stable order.
TEST_F(generated_notice_test, preserves_include_notices_and_deduplicates) {
  write("common.serializer", "// Copyright 2026 Common Authors\n"
                             "serializer version 1; class common { public uint32 value; }");
  write("left.serializer", "// Copyright 2026 Shared Authors\n"
                           "serializer version 1; include common; class left {}");
  write("right.serializer", "// Copyright 2026 Shared Authors\n"
                            "serializer version 1; include common; class right {}");
  write("root.serializer", "// Copyright 2026 Entry Authors\n"
                           "serializer version 1; include left; include right;");
  const auto parsed = schema::parser::parse_file(directory / "root.serializer");
  for (const auto language : languages) {
    SCOPED_TRACE(language);
    const auto output = generate(parsed.statements, language);
    const auto entry = output.find("Copyright 2026 Entry Authors");
    const auto shared = output.find("Copyright 2026 Shared Authors");
    const auto common = output.find("Copyright 2026 Common Authors");
    EXPECT_NE(entry, std::string::npos);
    EXPECT_NE(shared, std::string::npos);
    EXPECT_NE(common, std::string::npos);
    EXPECT_LT(entry, shared);
    EXPECT_LT(shared, common);
    EXPECT_EQ(output.find("Copyright 2026 Shared Authors", shared + 1), std::string::npos);
  }
}

// Header-only files and empty namespaces retain notices through the metadata-aware API.
TEST_F(generated_notice_test, preserves_notices_without_model_declarations) {
  for (const auto declaration : {"", "namespace empty {}"}) {
    SCOPED_TRACE(declaration);
    write("empty.serializer", "// SPDX-FileCopyrightText: 2026 Empty Schema Authors\n"
                              "serializer version 1;\n" + std::string{declaration});
    write("root.serializer", "// Copyright 2026 Entry Authors\n"
                             "serializer version 1; include empty;");
    const auto parsed = schema::parser::parse_file(directory / "root.serializer");
    for (const auto language : languages) {
      SCOPED_TRACE(language);
      const auto output = generate(parsed, language);
      EXPECT_NE(output.find("SPDX-FileCopyrightText: 2026 Empty Schema Authors"),
                std::string::npos);
      EXPECT_NE(output.find("Copyright 2026 Entry Authors"), std::string::npos);
      EXPECT_EQ(output.find("SPDX-License-Identifier: 0BSD"), std::string::npos);
    }
  }
}

// Lowered generic instances keep the notice ownership attached after schema transformations.
TEST(generated_notices, preserves_notices_after_generic_lowering) {
  constexpr std::string_view source =
      "// Copyright 2026 Generic Schema Authors\n"
      "namespace example { class box<T> { public T value; } "
      "class payload { public box<uint32> boxed_value; } }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = schema::parser::parse(input);
  for (const auto language : languages) {
    SCOPED_TRACE(language);
    const auto output = generate(statements, language);
    EXPECT_NE(output.find("Copyright 2026 Generic Schema Authors"), std::string::npos);
  }
}

// Source text stays commented even when it contains line continuations or Java Unicode escapes.
TEST(generated_notices, isolates_comment_control_sequences) {
  constexpr std::string_view source =
      "// Copyright 2026 Notice Authors \\u000a class injected {} \\\n"
      "// Whitespace continuation \\ \t\n"
      "serializer version 1; class payload { public uint32 value; }";
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = schema::parser::parse(input, true);
  const auto java = generate(statements, "java");
  EXPECT_NE(java.find("\\u005cu000a class injected {} \\u005c\n"), std::string::npos);
  for (const auto language : {"cpp", "c", "python"}) {
    SCOPED_TRACE(language);
    const auto output = generate(statements, language);
    const auto backslash = language == std::string_view{"python"} ? "\\" : "\\u005c";
    EXPECT_NE(output.find("class injected {} " + std::string{backslash} + "\n"),
              std::string::npos);
    EXPECT_NE(output.find("Whitespace continuation " + std::string{backslash} + " \t\n"),
              std::string::npos);
  }
}

// Every target keeps extra line separators inside comments even though the schema ends them at LF.
TEST(generated_notices, isolates_target_language_line_separators) {
  constexpr std::string_view line_source =
      "// Copyright 2026 Notice Authors\rclass injected_cr {}"
      "\xc2\x85" "class injected_next {}"
      "\xe2\x80\xa8" "class injected_line {}"
      "\xe2\x80\xa9" "class injected_paragraph {}\n"
      "serializer version 1; class payload { public uint32 value; }";
  constexpr std::string_view block_source =
      "/* Copyright 2026 Notice Authors\rclass injected_cr {}"
      "\xc2\x85" "class injected_next {}"
      "\xe2\x80\xa8" "class injected_line {}"
      "\xe2\x80\xa9" "class injected_paragraph {} */\n"
      "serializer version 1; class payload { public uint32 value; }";
  for (const auto source : {line_source, block_source}) {
    SCOPED_TRACE(source);
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    const auto statements = schema::parser::parse(input, true);
    for (const auto language : languages) {
      SCOPED_TRACE(language);
      const auto output = generate(statements, language);
      const auto prefix = language == std::string_view{"python"} ? "# " : "// ";
      for (const auto name : {"injected_cr", "injected_next", "injected_line", "injected_paragraph"}) {
        EXPECT_NE(output.find("\n" + std::string{prefix} + "class " + name + " {}"),
                  std::string::npos);
      }
    }
  }
}
