#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
namespace writer = rohit::serializer::writer;

// Emit without running an external formatter so these assertions inspect naming and wire tokens.
std::string emit(std::string_view schema, writer::coding_standard standard,
                 bool rename_identifiers = true) {
  const auto input = rohit::make_constant_stream(schema.data(), schema.size());
  const auto statements = rohit::serializer::parser::parse(input);
  writer::cpp_options options{};
  options.standard = standard;
  options.rename_identifiers = rename_identifiers;
  options.format = false;
  rohit::full_stream_auto_alloc output{};
  writer::cpp::write(output, statements, options);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Keep each configuration case isolated from concurrent test processes.
class configuration_file {
  std::filesystem::path directory{};

public:
  // Reserve a unique directory before creating its configuration file.
  configuration_file() {
    std::random_device random{};
    do {
      directory = std::filesystem::temp_directory_path() /
                  ("serializer-config-test-" + std::to_string(random()));
    } while (!std::filesystem::create_directory(directory));
  }
  // Remove this test's owned files without masking a test failure.
  ~configuration_file() {
    std::error_code ignored{};
    std::filesystem::remove_all(directory, ignored);
  }
  // Test configuration directories have one owner.
  configuration_file(const configuration_file&) = delete;
  // Test configuration directories cannot be shared by assignment.
  configuration_file& operator=(const configuration_file&) = delete;
  // Write a configuration and expose its path to the production parser.
  std::filesystem::path write(std::string_view content) const {
    const auto file = directory / "output.ini";
    std::ofstream output{file, std::ios::binary};
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << content;
    output.close();
    return file;
  }
};
} // namespace

// Profiles rename symbols by role while preserving the schema's field and enum wire spellings.
TEST(output_options, profiles_preserve_wire_names) {
  constexpr std::string_view schema =
      "namespace ExampleAPI { enum UserState { ReadyNow } "
      "class UserRecord view owning { public uint32 userID; public uint32 requestCount; public "
      "UserState state; "
      "public union(uint32 = CountValue, float = FloatValue) payload; } }";
  struct expectation {
    writer::coding_standard standard;
    std::string_view type;
    std::string_view field;
    std::string_view getter;
    std::string_view enum_value;
    std::string_view alternative;
  };
  constexpr std::array expectations{
      expectation{writer::coding_standard::serializer, "user_record", "user_id", "get_user_id",
                  "ready_now", "count_value"},
      expectation{writer::coding_standard::core, "user_record", "user_id", "get_user_id",
                  "ready_now", "count_value"},
      expectation{writer::coding_standard::google, "UserRecord", "user_id_", "GetUserId",
                  "kReadyNow", "kCountValue"},
      expectation{writer::coding_standard::llvm, "UserRecord", "UserId", "getUserId", "ReadyNow",
                  "CountValue"},
      expectation{writer::coding_standard::gnu, "user_record", "user_id", "get_user_id",
                  "ready_now", "count_value"},
      expectation{writer::coding_standard::cert, "user_record", "user_id", "get_user_id",
                  "ready_now", "count_value"},
      expectation{writer::coding_standard::misra, "user_record", "user_id", "get_user_id",
                  "ready_now", "count_value"},
      expectation{writer::coding_standard::autosar, "user_record", "user_id", "get_user_id",
                  "ready_now", "count_value"},
      expectation{writer::coding_standard::qt, "UserRecord", "userId", "getUserId", "ReadyNow",
                  "CountValue"}};
  for (const auto& expected : expectations) {
    SCOPED_TRACE(writer::coding_standard_name(expected.standard));
    const auto output = emit(schema, expected.standard);
    EXPECT_NE(output.find("namespace example_api"), std::string::npos);
    EXPECT_NE(output.find("class " + std::string{expected.type}), std::string::npos);
    EXPECT_NE(output.find("this->" + std::string{expected.field}), std::string::npos);
    EXPECT_NE(output.find(std::string{expected.getter} + "() const"), std::string::npos);
    EXPECT_NE(output.find("::" + std::string{expected.enum_value}), std::string::npos);
    EXPECT_NE(output.find("::" + std::string{expected.alternative}), std::string::npos);
    EXPECT_NE(output.find("\"userID\""), std::string::npos);
    EXPECT_NE(output.find("return \"ReadyNow\""), std::string::npos);
    EXPECT_NE(output.find("\"payload:CountValue\""), std::string::npos);
    EXPECT_EQ(output.find("constexpr inline"), std::string::npos);
  }
}

// Preserve the exact quoted default in both snake-case and LLVM-profile output.
TEST(output_options, preserves_spaces_in_string_defaults) {
  for (const auto standard : {writer::coding_standard::serializer, writer::coding_standard::llvm}) {
    const auto source = emit(R"(class record { public string label { "  schema default  " }; })",
                             standard);
    EXPECT_NE(source.find(R"("  schema default  ")"), std::string::npos);
  }
}

// Detect naming collisions, keyword conversion, and generated helper conflicts before emission.
TEST(output_options, rejects_collisions_and_keywords) {
  for (const auto schema :
       {"class record { public uint32 userID; public uint32 user_id; }",
        "class HTTPServer {} class HttpServer {}", "class record { public uint32 Class; }",
        "class ViewBase view readonly {}", "enum State { ReadyNow, ready_now }",
        "enum State { Ready } class ToState {}",
        "class record { public uint32 serializer_reuses_storage; }",
        "class record { public union(uint32 = someValue, float = some_value) payload; }"}) {
    EXPECT_THROW(emit(schema, writer::coding_standard::serializer), std::invalid_argument)
        << schema;
  }
  EXPECT_THROW(emit("class record { public uint32 mode { unknownValue }; }",
                    writer::coding_standard::google),
               std::invalid_argument);
  EXPECT_THROW(emit("class StorageSource {}", writer::coding_standard::google),
               std::invalid_argument);
  EXPECT_THROW(emit("class SerializerStream {}", writer::coding_standard::google),
               std::invalid_argument);
  const auto preserved =
      emit("class UserRecord { public uint32 userID; }", writer::coding_standard::google, false);
  EXPECT_NE(preserved.find("class UserRecord"), std::string::npos);
  EXPECT_NE(preserved.find("this->userID"), std::string::npos);
}

// Resolve enum defaults and nested/base references through renamed declarations.
TEST(output_options, defaults_and_qualified_types) {
  const auto source = emit(
      "namespace WireAPI { enum State { ReadyNow } class BaseRecord {} "
      "class UserRecord : public BaseRecord { public State currentState { State::ReadyNow }; } }",
      writer::coding_standard::google);
  EXPECT_NE(source.find("::wire_api::State::kReadyNow"), std::string::npos);
  EXPECT_NE(source.find("static_cast<::wire_api::BaseRecord*>(this)->serialize_in"),
            std::string::npos);
  EXPECT_NE(source.find("\"currentState\""), std::string::npos);
}

// Validate configuration sections, exact profile spellings, and config-relative paths.
TEST(output_options, configuration_and_diagnostics) {
  configuration_file temporary{};
  const auto file = temporary.write("[output]\nlanguage = cpp\n[cpp]\ncoding_standard = qt\n"
                                    "naming = preserve\nformat_file = \"styles/custom "
                                    "style.yaml\"\nclang_format = tools/formatter\n");
  const auto options = writer::read_output_options(file);
  EXPECT_EQ(options.cpp.standard, writer::coding_standard::qt);
  EXPECT_FALSE(options.cpp.rename_identifiers);
  EXPECT_EQ(options.cpp.format_file, file.parent_path() / "styles/custom style.yaml");
  EXPECT_EQ(options.cpp.clang_format, file.parent_path() / "tools/formatter");
  for (const auto invalid :
       {"[output]\nlanguage = rust\n", "[rust]\nformat = true\n",
        "[cpp]\ncoding_standard = googl\n", "[cpp]\nformat = yes\n",
        "[cpp]\nformat = true\nformat = false\n", "[cpp]\n[cpp]\n",
        "[cpp]\nformat = false\nformat_file = style.yaml\n", "[cpp]\nunknown = true\n",
        "[cpp]\nnaming = guess\n", "[cpp]\nclang_format = \"missing quote\n"}) {
    EXPECT_THROW(writer::read_output_options(temporary.write(invalid)), std::invalid_argument)
        << invalid;
  }
}

// All advertised profiles must have a real formatter mapping and round-trip configuration name.
TEST(output_options, java_configuration_and_diagnostics) {
  configuration_file temporary{};
  const auto options = writer::read_output_options(temporary.write(
      "[output]\nlanguage = java\n[java]\ncoding_standard = oracle\n"
      "naming = preserve\npackage = serializer.example\n"));
  EXPECT_EQ(options.language, "java");
  EXPECT_EQ(options.java.standard, writer::java_coding_standard::oracle);
  EXPECT_FALSE(options.java.rename_identifiers);
  EXPECT_EQ(options.java.package_name, "serializer.example");
  for (const auto profile : {"serializer", "google", "oracle"}) {
    EXPECT_EQ(writer::java_coding_standard_name(writer::parse_java_coding_standard(profile)), profile);
  }
  for (const auto invalid : {"[java]\ncoding_standard = llvm\n", "[java]\nnaming = guess\n",
                            "[java]\nformat = true\n", "[java]\npackage = a\npackage = b\n"}) {
    EXPECT_THROW(writer::read_output_options(temporary.write(invalid)), std::invalid_argument);
  }
}

// All advertised C++ profiles must have a real formatter mapping and stable configuration name.
TEST(output_options, formatter_mappings) {
  for (const auto profile :
       {"serializer", "core", "google", "llvm", "gnu", "cert", "misra", "autosar", "qt"}) {
    const auto standard = writer::parse_coding_standard(profile);
    EXPECT_EQ(writer::coding_standard_name(standard), profile);
    EXPECT_NE(writer::cpp_format_style(standard).find("BasedOnStyle:"), std::string::npos);
  }
  EXPECT_NE(writer::cpp_format_style(writer::coding_standard::qt).find("IndentWidth: 4"),
            std::string::npos);
}
