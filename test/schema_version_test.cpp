// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>
#include <rohit/version.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

// A versioned schema has the same declarations, names, and wire IDs as a legacy fragment.
TEST(schema_version, preserves_declarations) {
  constexpr std::string_view sources[]{
    "// license\nserializer /* language */ version 1;\n"
    "class account stable_ids { public uint32 id (7); }",
    "// license\nserializer /* language */ version 1.0.0 /* revision */;\n"
    "class account stable_ids { public uint32 id (7); }"};
  for (const auto source : sources) {
    SCOPED_TRACE(source);
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    const auto statements = rohit::serializer::parser::parse(input, true);
    ASSERT_EQ(statements.size(), 1u);
    const auto* record = dynamic_cast<const rohit::serializer::class_node*>(statements.front().get());
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->get_full_name(), "account");
    ASSERT_EQ(record->member_list.size(), 1u);
    EXPECT_EQ(record->member_list.front().id, 7u);
  }
  EXPECT_EQ(rohit::serializer::schema_language_version, 1u);
  EXPECT_EQ(rohit::serializer::schema_language_version_text, "1.5.0");
  EXPECT_EQ(rohit::serializer::schema_language_version_major, 1u);
  EXPECT_EQ(rohit::serializer::schema_language_version_minor, 5u);
  EXPECT_EQ(rohit::serializer::schema_language_version_patch, 0u);
}

// All truncated header prefixes fail through schema diagnostics without out-of-bounds reads.
TEST(schema_version, truncated_headers) {
  constexpr std::string_view headers[]{"serializer version 1;", "serializer version 1.0.0;",
                                        "serializer version 1.1.0;", "serializer version 1.2.0;", "serializer version 1.3.0;", "serializer version 1.4.0;", "serializer version 1.5.0;"};
  for (const auto header : headers) {
    SCOPED_TRACE(header);
    for (std::size_t length = 0; length < header.size(); ++length) {
      SCOPED_TRACE(length);
      const auto input = rohit::make_constant_stream(header.data(), length);
      EXPECT_THROW(rohit::serializer::parser::parse(input, true), rohit::exception::base_parser);
    }
  }
}

// Unsupported versions and misplaced statements are rejected even by the fragment API.
TEST(schema_version, invalid_versions_and_positions) {
  constexpr std::string_view invalid[] = {"serializer version 0;",
                                          "serializer version 2;",
                                          "serializer version -1;",
                                          "serializer version 1.0;",
                                          "serializer version 1.;",
                                          "serializer version 1..0;",
                                          "serializer version 1.0.;",
                                          "serializer version 1.0.0.0;",
                                          "serializer version 1.0.0x;",
                                          "serializer version 1 .0.0;",
                                          "serializer version 1. 0.0;",
                                          "serializer version 1.0. 0;",
                                          "serializer version 1.0.0-beta;",
                                          "serializer version 1.0.0+build;",
                                          "serializer version 1.0.-1;",
                                          "serializer version 0.0.0;",
                                          "serializer version 1.5.1;",
                                          "serializer version 1.6.0;",
                                          "serializer version 1.10.0;",
                                          "serializer version 2.0.0;",
                                          "serializer version 999999999999999999999999.0.0;",
                                          "serializer version 1.999999999999999999999999.0;",
                                          "serializer version 1.0.999999999999999999999999;",
                                          "serializer version 999999999999999999999999;",
                                          "serializerversion 1;",
                                          "serializer version1;",
                                          "serializer version 1x;",
                                          "serializer version 1; serializer version 1;",
                                          "serializer version 1.0.0; serializer version 1;",
                                          "class account {} serializer version 1;",
                                          "class account {} serializer version 1.0.0;",
                                          "namespace demo { serializer version 1; }",
                                          "namespace demo { serializer version 1.0.0; }",
                                          "serializer version 1; /* unterminated"};
  for (const auto source : invalid) {
    SCOPED_TRACE(source);
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_THROW(rohit::serializer::parser::parse(input), rohit::exception::base_parser);
  }
}

// File parsing requires a header; the established library API still accepts schema fragments.
TEST(schema_version, optional_for_library_fragments) {
  constexpr std::string_view source{"class account {}"};
  const auto fragment = rohit::make_constant_stream(source.data(), source.size());
  EXPECT_EQ(rohit::serializer::parser::parse(fragment).size(), 1u);
  const auto file = rohit::make_constant_stream(source.data(), source.size());
  EXPECT_THROW(rohit::serializer::parser::parse(file, true), rohit::exception::base_parser);
}

// Compare dotted components numerically so a newer minor accepts every older compatible patch.
TEST(schema_version, accepts_older_contracts_with_numeric_component_order) {
  for (const std::string_view version : {"1", "1.0.0", "1.0.1", "1.0.10", "1.0.999", "1.1.0", "1.1.1", "1.2.0", "1.2.1", "1.3.0", "1.3.1", "1.4.0", "1.4.1", "1.4.999", "1.5.0"}) {
    SCOPED_TRACE(version);
    const auto source = "serializer version " + std::string{version} +
        "; class record { public uint32 value; }";
    const auto input = rohit::make_constant_stream(source.data(), source.size());
    EXPECT_EQ(rohit::serializer::parser::parse(input, true).size(), 1);
  }
}
