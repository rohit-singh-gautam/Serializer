#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include "../src/command_line.hpp"
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
// Exercise the actual C++ parser and backend without requiring target-language tools.
std::string emit_portable(std::string_view schema, std::string_view language,
                          const rohit::serializer::writer::portable_options& options = {}) {
  const auto input = rohit::make_constant_stream(schema.data(), schema.size());
  const auto nodes = rohit::serializer::parser::parse(input);
  return rohit::serializer::writer::portable::generate(nodes, language, "Schema", options);
}
} // namespace

// Scalar access borrows parsed storage and cannot silently mutate absent options.
TEST(command_line, borrows_first_value_without_flattening_arguments) {
  using namespace rohit::serializer;
  const cli::arguments arguments{{"language", {"rust", "python"}}, {"empty", {""}}, {"none", {}}};
  EXPECT_EQ(&cli::first(arguments, "language"), &arguments.at("language").front());
  EXPECT_EQ(arguments.at("language").size(), 2u);
  EXPECT_TRUE(cli::first(arguments, "empty").empty());
  EXPECT_THROW(cli::first(arguments, "missing"), std::out_of_range);
  EXPECT_THROW(cli::first(arguments, "none"), std::out_of_range);
}

// Internal runtime symbols and C lifecycle helpers must not be shadowed by schema declarations.
TEST(portable_writer, native_runtime_and_lifecycle_collisions) {
  EXPECT_THROW(emit_portable("class type_error {}", "python"), std::invalid_argument);
  EXPECT_THROW(emit_portable("class b_tree_map {}", "rust"), std::invalid_argument);
  EXPECT_THROW(emit_portable("class byte_buffer {}", "kotlin"), std::invalid_argument);
  EXPECT_THROW(emit_portable("class malloc {}", "c"), std::invalid_argument);
  EXPECT_THROW(emit_portable("class account {} class account_init {}", "c"), std::invalid_argument);
  for (const auto language : {"rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_THROW(emit_portable("class account { public int32 srlWrite; }", language),
                 std::invalid_argument);
  }
  rohit::serializer::writer::portable_options options{};
  options.package_name = "bad..package";
  EXPECT_THROW(emit_portable("class account {}", "kotlin", options), std::invalid_argument);
}

// Unsupported shapes and ambiguous names must fail before files can be overwritten.
TEST(portable_writer, rejects_invalid_schemas_before_publication) {
  for (const auto language :
       {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    for (const auto schema :
         {"class account view { public int32 value; }",
          "class account packed { public int32 value; }",
          "class account { public int32 account_id; public int32 accountId; }",
          "namespace api { class message {} } class api_message {}",
          "enum state { ready_now, ReadyNow }", "class account { public uint8 value { 256 }; }",
          "class account { public int64 value { 9223372036854775808 }; }",
          "class account { public uint64 value { -1 }; }",
          "class account { public float value { 1e100 }; }",
          "class account { public int32 value { sizeof(int) }; }",
          "class account { public account value; }",
          "class account { public map(float) int32 values; }",
          "class key {} class account { public map(key) int32 values; }", "class protocol {}"}) {
      SCOPED_TRACE(std::string{language} + ": " + schema);
      EXPECT_THROW(emit_portable(schema, language), std::invalid_argument);
    }
  }
}

// Renaming code must not change schema identifiers or literal defaults.
TEST(portable_writer, preserves_wire_identity_and_full_width_defaults) {
  constexpr std::string_view schema =
      "namespace api { enum state { ready } class account stable_ids { "
      "public uint64 account_id (\"account_wire\", 16384) { 18446744073709551615 }; "
      "public int64 signed_value (2) { -9223372036854775808 }; "
      "public string name (3) { \"  literal spaces  \" }; } }";
  for (const auto language : {"js", "go", "csharp"}) {
    const auto output = emit_portable(schema, language);
    EXPECT_NE(output.find("output.field(16384,"), std::string::npos);
    EXPECT_NE(output.find("\"account_wire\""), std::string::npos);
    EXPECT_NE(output.find("18446744073709551615"), std::string::npos);
    EXPECT_NE(output.find("-9223372036854775808"), std::string::npos);
    EXPECT_NE(output.find("\"  literal spaces  \""), std::string::npos);
    EXPECT_NE(output.find("switch"), std::string::npos);
  }
}

// Keep generation policy validation in the C++ executable, not target runtimes.
TEST(portable_writer, rejects_invalid_module_names) {
  rohit::serializer::writer::portable_options options{};
  options.package_name = "bad.package";
  EXPECT_THROW(emit_portable("class message {}", "go", options), std::invalid_argument);
  options.namespace_name = "Bad..Namespace";
  EXPECT_THROW(emit_portable("class message {}", "csharp", options), std::invalid_argument);
  options.rename_identifiers = false;
  options.package_name = "generated";
  EXPECT_THROW(emit_portable("class message { public int32 value; }", "go", options),
               std::invalid_argument);
}

// Every requested output is selected in order, including JS's declaration companion.
TEST(portable_writer, accepts_all_language_selection) {
  const auto languages = rohit::serializer::writer::parse_output_languages(
      "cpp,java,js,typescript,go,csharp,rust,python,swift,kotlin,c");
  ASSERT_EQ(languages.size(), 11u);
  EXPECT_EQ(languages[2], "js");
  EXPECT_EQ(languages.back(), "c");
  EXPECT_THROW(rohit::serializer::writer::parse_output_languages("go,js,go"),
               std::invalid_argument);
}

// Native runtime globals and C# enclosing types must not be shadowed by schema names.
TEST(portable_writer, rejects_runtime_name_shadowing) {
  for (const auto language : {"js", "typescript"}) {
    for (const auto schema : {"class map {}", "class number {}", "class uint8_array {}"}) {
      EXPECT_THROW(emit_portable(schema, language), std::invalid_argument);
    }
  }
  for (const auto schema :
       {"class message { public int32 protocol; }", "class message { public int32 system; }",
        "enum state { ready } class message { public int32 state; }"}) {
    EXPECT_THROW(emit_portable(schema, "csharp"), std::invalid_argument);
  }
  rohit::serializer::writer::portable_options options{};
  options.rename_identifiers = false;
  for (const auto schema : {"class int64 {}", "class JSON {}", "class append {}"}) {
    EXPECT_THROW(emit_portable(schema, "go", options), std::invalid_argument);
  }
}
