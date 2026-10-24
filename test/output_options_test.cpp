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
#include <utility>

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

// Generate selected optional capabilities through the production schema writer.
std::string emit(std::string_view schema, writer::cpp_options options) {
  const auto input = rohit::make_constant_stream(schema.data(), schema.size());
  const auto statements = rohit::serializer::parser::parse(input);
  options.format = false;
  return writer::cpp::generate(statements, options);
}

// Ignore only decimal body fingerprints on the generated unit's exact guard directives.
std::string normalized_generated_fingerprints(std::string source) {
  constexpr std::string_view guard_prefix = "SERIALIZER_GENERATED_";
  constexpr std::string_view hex_digits = "0123456789ABCDEF";
  constexpr std::string_view decimal_digits = "0123456789";
  for (std::size_t line_start{}; line_start < source.size();) {
    const auto line_end = source.find('\n', line_start);
    const auto line_size = (line_end == std::string::npos ? source.size() : line_end) - line_start;
    const std::string_view line{source.data() + line_start, line_size};
    const bool comparison = line.starts_with("#if ");
    const std::string_view directive = comparison ? "#if " : "#define ";
    if (line.starts_with(directive) && line.substr(directive.size()).starts_with(guard_prefix)) {
      const auto identity_start = directive.size() + guard_prefix.size();
      const auto identity_end = line.find_first_not_of(hex_digits, identity_start);
      const std::string_view separator = comparison ? " != " : " ";
      if (identity_end != std::string_view::npos && identity_end > identity_start &&
          line.substr(identity_end).starts_with(separator)) {
        const auto value_start = identity_end + separator.size();
        const auto value_end = line.find_first_not_of(decimal_digits, value_start);
        if (value_end != std::string_view::npos && value_end > value_start &&
            line.substr(value_end) == "ULL") {
          source.replace(line_start + value_start, value_end - value_start, "0");
        }
      }
    }
    const auto next_line = source.find('\n', line_start);
    if (next_line == std::string::npos) { break; }
    line_start = next_line + 1;
  }
  return source;
}

// Remove only the separately generated compile-time declarations before comparing existing APIs.
std::string without_constant_declarations(std::string source) {
  constexpr std::string_view marker_start =
      "  // Opt in to explicit constant-evaluation binary APIs without object storage.\n";
  constexpr std::string_view traversal_start =
      "  // Encode positional fields using the selected output sink.\n";
  for (const auto block : {marker_start, traversal_start}) {
    for (auto begin = source.find(block); begin != std::string::npos; begin = source.find(block)) {
      const auto terminator = block == marker_start ? std::string_view{"\n\n"}
                                                   : std::string_view{"\n  }\n\n"};
      const auto end = source.find(terminator, begin);
      if (end == std::string::npos) {
        throw std::logic_error{"Incomplete generated constant-evaluation declaration"};
      }
      source.erase(begin, end + terminator.size() - begin);
    }
  }
  return source;
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

// Preserve positional initialization of the XML options that predate bundle pins.
TEST(output_options, xml_options_preserve_aggregate_initialization) {
  const writer::xml_options options{true, false, "input", {}, {{"legacy", {}}}};
  EXPECT_TRUE(options.enabled);
  EXPECT_TRUE(options.classes.contains("legacy"));
  EXPECT_TRUE(options.bundle_sha256.empty());
}

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
        "class record { public uint32 serialize; }",
        "class record { public uint32 deserialize; }",
        "class serialize {}", "class deserialize {}",
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
  EXPECT_NO_THROW(emit(
      "class record { public string serialized_payload (\"serialize\", 1); }",
      writer::coding_standard::serializer));
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
       {"[output]\nlanguage = unknown_backend\n", "[rust]\nformat = true\n",
        "[cpp]\ncoding_standard = googl\n", "[cpp]\nformat = yes\n",
        "[cpp]\nformat = true\nformat = false\n", "[cpp]\n[cpp]\n",
        "[cpp]\nformat = false\nformat_file = style.yaml\n", "[cpp]\nunknown = true\n",
        "[cpp]\nnaming = guess\n", "[cpp]\nclang_format = \"missing quote\n"}) {
    EXPECT_THROW(writer::read_output_options(temporary.write(invalid)), std::invalid_argument)
        << invalid;
  }
}

// Resolve source-qualified class settings without changing backend-wide defaults.
TEST(output_options, class_comparison_configuration) {
  configuration_file temporary;
  const auto options = writer::read_output_options(temporary.write(
      "[cpp.class.example::sort_key]\nequality = true\nordering = lexicographic\n"
      "fields = category, sequence\n[cpp.class.example::empty]\nfields = durable\n"));
  const auto& key = options.cpp.class_comparisons.at("example::sort_key");
  EXPECT_TRUE(key.equality);
  EXPECT_TRUE(key.ordering);
  EXPECT_EQ(key.fields, (std::vector<std::string>{"category", "sequence"}));
  const auto& empty = options.cpp.class_comparisons.at("example::empty");
  EXPECT_FALSE(empty.equality);
  EXPECT_FALSE(empty.ordering);
  EXPECT_TRUE(empty.fields.empty());
  EXPECT_TRUE(writer::cpp_options{}.class_comparisons.empty());
  for (const auto language : {"java", "js", "typescript", "go", "csharp", "rust",
                              "python", "swift", "kotlin", "c"}) {
    const auto portable = writer::read_output_options(temporary.write(
        "[" + std::string{language} + ".class.example::sort_key]\nequality = true\n"
        "ordering = lexicographic\nfields = category, sequence\n"));
    const auto& comparisons = std::string_view{language} == "java" ?
        portable.java.class_comparisons : portable.class_comparisons.at(language);
    EXPECT_TRUE(comparisons.at("example::sort_key").equality);
    EXPECT_TRUE(comparisons.at("example::sort_key").ordering);
  }
  for (const auto invalid : {
      "[cpp.class.]\nequality = true\n", "[cpp.class.example:::key]\nequality = true\n",
      "[cpp.class.example::key]\nequality = yes\n",
      "[cpp.class.example::key]\nordering = true\n",
      "[cpp.class.example::key]\nfields = sequence, sequence\n",
      "[cpp.class.example::key]\nfields = sequence,\n",
      "[cpp.class.example::key]\nfields = base.field\n",
      "[cpp.class.example::key]\nunknown = true\n",
      "[cpp.class.example::key]\nequality = true\nequality = false\n"}) {
    EXPECT_THROW(writer::read_output_options(temporary.write(invalid)), std::invalid_argument)
        << invalid;
  }
}

// Validate decoder-only derived-size policy independently of comparison opt-in.
TEST(output_options, class_auto_size_configuration) {
  configuration_file temporary;
  for (const auto language : {"cpp", "java", "js", "typescript", "go", "csharp", "rust",
                              "python", "swift", "kotlin", "c"}) {
    const auto options = writer::read_output_options(temporary.write(
        "[" + std::string{language} + ".class.example::packet]\nauto_size_validation = retain\n"));
    const auto& sizes = std::string_view{language} == "cpp" ? options.cpp.class_auto_sizes :
        std::string_view{language} == "java" ? options.java.class_auto_sizes :
        options.class_auto_sizes.at(language);
    EXPECT_EQ(sizes.at("example::packet").validation, writer::auto_size_validation::retain);
  }
  EXPECT_EQ(writer::class_auto_size_options{}.validation, writer::auto_size_validation::verify);
  const auto mixed = writer::read_output_options(temporary.write(
      "[cpp.class.example::packet]\nauto_size_validation = verify\nequality = true\n"));
  EXPECT_TRUE(mixed.cpp.class_comparisons.at("example::packet").equality);
  EXPECT_EQ(mixed.cpp.class_auto_sizes.at("example::packet").validation,
            writer::auto_size_validation::verify);
  for (const auto invalid : {
      "[cpp.class.example::packet]\nauto_size_validation = unchecked\n",
      "[cpp.class.example::packet]\nauto_size_validation = verify\nauto_size_validation = retain\n"}) {
    EXPECT_THROW(writer::read_output_options(temporary.write(invalid)), std::invalid_argument);
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

// Caller-owned Java encoding remains an explicit strict-Boolean backend opt-in.
TEST(output_options, java_output_buffer_reuse_configuration) {
  configuration_file file;
  EXPECT_FALSE(writer::java_options{}.output_buffer_reuse);
  EXPECT_TRUE(writer::read_output_options(file.write("[java]\noutput_buffer_reuse = true\n")).java.output_buffer_reuse);
  EXPECT_FALSE(writer::read_output_options(file.write("[java]\noutput_buffer_reuse = false\n")).java.output_buffer_reuse);
  EXPECT_THROW(writer::read_output_options(file.write("[java]\noutput_buffer_reuse = yes\n")), std::invalid_argument);
  EXPECT_THROW(writer::read_output_options(file.write("[java]\noutput_buffer_reuse = true\noutput_buffer_reuse = false\n")),
      std::invalid_argument);
}

// NIO windows have an independent strict opt-in and cannot silently enable array overloads.
TEST(output_options, java_nio_output_buffer_reuse_configuration) {
  configuration_file file;
  EXPECT_FALSE(writer::java_options{}.nio_output_buffer_reuse);
  const auto enabled = writer::read_output_options(file.write("[java]\nnio_output_buffer_reuse = true\n"));
  EXPECT_TRUE(enabled.java.nio_output_buffer_reuse);
  EXPECT_FALSE(enabled.java.output_buffer_reuse);
  EXPECT_FALSE(writer::read_output_options(file.write("[java]\nnio_output_buffer_reuse = false\n")).java.nio_output_buffer_reuse);
  EXPECT_THROW(writer::read_output_options(file.write("[java]\nnio_output_buffer_reuse = yes\n")), std::invalid_argument);
  EXPECT_THROW(writer::read_output_options(file.write("[java]\nnio_output_buffer_reuse = true\nnio_output_buffer_reuse = false\n")),
      std::invalid_argument);
}

// Central managed identity width is strict and independent of C++ presentation settings.
TEST(output_options, managed_identity_configuration) {
  configuration_file file;
  EXPECT_EQ(writer::read_output_options(file.write("[managed]\nid_type = uint64\n")).cpp.managed_id_type, "uint64");
  EXPECT_EQ(writer::read_output_options(file.write("[managed]\nid_type = uint32\n")).cpp.managed_id_type, "uint32");
  EXPECT_EQ(writer::cpp_options{}.managed_id_encoding, writer::persistent_id_encoding::compact_varint);
  EXPECT_EQ(writer::read_output_options(file.write("[managed]\nid_encoding = fixed\n")).cpp.managed_id_encoding,
            writer::persistent_id_encoding::fixed);
  EXPECT_EQ(writer::read_output_options(file.write("[managed]\nid_encoding = compact_varint\n")).cpp.managed_id_encoding,
            writer::persistent_id_encoding::compact_varint);
  EXPECT_THROW(writer::read_output_options(file.write("[managed]\nid_encoding = compact_variant\n")), std::invalid_argument);
  EXPECT_THROW(writer::read_output_options(file.write("[managed]\nid_encoding = compact_prefix\n")), std::invalid_argument);
  EXPECT_FALSE(writer::cpp_options{}.managed_separate_values);
  EXPECT_TRUE(writer::read_output_options(file.write("[managed]\nseparate_values = true\n")).cpp.managed_separate_values);
  EXPECT_FALSE(writer::read_output_options(file.write("[managed]\nseparate_values = false\n")).cpp.managed_separate_values);
  EXPECT_THROW(writer::read_output_options(file.write("[managed]\nseparate_values = maybe\n")), std::invalid_argument);
  EXPECT_THROW(writer::read_output_options(file.write("[managed]\nid_type = uuid\n")), std::invalid_argument);
}

// Compiler activation and positional selection are exact, independent configuration values.
TEST(output_options, constant_evaluation_configuration) {
  EXPECT_FALSE(writer::cpp_options{}.constant_evaluation);
  EXPECT_FALSE(writer::cpp_options{}.emission_only);
  EXPECT_EQ(writer::cpp_options{}.protocols, writer::cpp_protocols::all);
  configuration_file file;
  const auto options = writer::read_output_options(file.write(
      "[cpp]\nconstant_evaluation = true\nprotocols = binary_none\nformat = false\n"));
  EXPECT_TRUE(options.cpp.constant_evaluation);
  EXPECT_EQ(options.cpp.protocols, writer::cpp_protocols::binary_none);
  EXPECT_FALSE(writer::read_output_options(
      file.write("[cpp]\nconstant_evaluation = false\nprotocols = all\n")).cpp.constant_evaluation);
  for (const auto profile : {"all", "binary_none"}) {
    EXPECT_EQ(writer::cpp_protocols_name(writer::parse_cpp_protocols(profile)), profile);
  }
  EXPECT_TRUE(writer::read_output_options(file.write("[cpp]\nemission_only = true\n")).cpp.emission_only);
  for (const auto invalid : {"[cpp]\nemission_only = yes\n",
                             "[cpp]\nemission_only = true\nemission_only = false\n",
                             "[cpp]\nconstant_evaluation = yes\n",
                             "[cpp]\nconstant_evaluation = true\nconstant_evaluation = false\n",
                             "[cpp]\nprotocols = binary_integer\n",
                             "[cpp]\nprotocols = all\nprotocols = binary_none\n"}) {
    EXPECT_THROW(writer::read_output_options(file.write(invalid)), std::invalid_argument);
  }
}

// Global native selection reaches every SDK independently of XML and C++ overrides.
TEST(output_options, common_protocol_selection) {
  configuration_file file;
  for (const auto text : {"[output]\nprotocols = binary_none\n[cpp]\nprotocols = all\n",
                          "[cpp]\nprotocols = all\n[output]\nprotocols = binary_none\n"}) {
    const auto options = writer::read_output_options(file.write(text));
    EXPECT_EQ(options.cpp.protocols, writer::output_protocols::all);
    EXPECT_EQ(options.protocols, writer::output_protocols::binary_none);
    EXPECT_EQ(options.java.protocols, writer::output_protocols::binary_none);
    for (const auto* portable : {&options.js, &options.go, &options.csharp, &options.kotlin}) {
      EXPECT_EQ(portable->protocols, writer::output_protocols::binary_none);
      EXPECT_FALSE(portable->xml.enabled);
    }
  }
  const auto options = writer::read_output_options(file.write(
      "[output]\nprotocols = binary_none\n[xml]\nenabled = true\n"));
  EXPECT_TRUE(options.java.xml.enabled);
  EXPECT_EQ(options.cpp.protocols, writer::output_protocols::binary_none);
  EXPECT_THROW(writer::read_output_options(file.write("[output]\nprotocols = json\n")), std::invalid_argument);
  EXPECT_THROW(writer::read_output_options(file.write("[output]\nprotocols = all\nprotocols = binary_none\n")), std::invalid_argument);
}

// Existing C++ callers retain their option-specific diagnostic alongside the common parser.
TEST(output_options, protocol_selection_diagnostics) {
  const auto check = [](auto parse, std::string_view expected) {
    try {
      static_cast<void>(parse("json"));
      FAIL() << "Expected unsupported native protocol selection to fail";
    } catch (const std::invalid_argument& failure) {
      EXPECT_EQ(std::string_view{failure.what()}, expected);
    }
  };
  check(writer::parse_cpp_protocols, "cpp.protocols must be all or binary_none");
  check(writer::parse_output_protocols, "protocols must be all or binary_none");
}

// Restricted codecs diagnose before reading or writing, and omit keyed record dispatch where supported.
TEST(output_options, portable_protocol_selection) {
  const std::string schema{"class sample { public uint32 value { 7 }; }"};
  const auto input = rohit::make_constant_stream(schema);
  const auto statements = rohit::serializer::parser::parse(input);
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    SCOPED_TRACE(language);
    writer::portable_options options{};
    options.protocols = writer::output_protocols::binary_none;
    const auto source = writer::portable::generate(statements, language, "Schema", options);
    EXPECT_NE(source.find(language == std::string_view{"typescript"} ? "encode(protocol: 1)" :
        language == std::string_view{"c"} ? "out->protocol != srl_binary_none" : "Protocol disabled during generation"), std::string::npos);
    if (language == std::string_view{"js"} || language == std::string_view{"go"} ||
        language == std::string_view{"csharp"}) {
      EXPECT_EQ(source.find("else if (input.protocol == "), std::string::npos);
    }
  }
  writer::java_options options{};
  options.protocols = writer::output_protocols::binary_none;
  const auto source = writer::java::generate(statements, "Schema", options);
  EXPECT_NE(source.find("Protocol disabled during generation"), std::string::npos);
  EXPECT_EQ(source.find("else if (in.protocol == Protocol.BINARY_INTEGER)"), std::string::npos);
}

// Enabling constant output leaves the established runtime traversal unchanged.
TEST(output_options, additive_constant_evaluation_traversal) {
  constexpr std::string_view schema =
      "enum kind { first, second } class record { public uint32 first; public uint32 second; "
      "public array uint8 bytes; public union(uint32 = number, float = real) item; }";
  writer::cpp_options options{};
  const auto baseline = emit(schema, options);
  EXPECT_EQ(baseline.find("serialize_constant_out"), std::string::npos);
  EXPECT_EQ(baseline.find("serializer_constant_evaluation"), std::string::npos);
  options.constant_evaluation = true;
  const auto enabled = emit(schema, options);
  EXPECT_NE(enabled.find("constexpr void serialize_constant_out"), std::string::npos);
  EXPECT_NE(enabled.find("static constexpr bool serializer_constant_evaluation = true"),
            std::string::npos);
  const auto runtime_start = baseline.find("  // Encode fields directly");
  const auto runtime_end = baseline.find("  // Encode to a buffer", runtime_start);
  ASSERT_NE(runtime_start, std::string::npos);
  ASSERT_NE(runtime_end, std::string::npos);
  EXPECT_NE(enabled.find(baseline.substr(runtime_start, runtime_end - runtime_start)),
            std::string::npos);
  EXPECT_NO_THROW(emit("class record { public map(string) uint32 values; }", options));
  EXPECT_THROW(emit("class record { public uint32 serialize_constant_out; }", options),
               std::invalid_argument);
  options.protobuf = true;
  EXPECT_NE(emit(schema, options).find("serializer_protobuf_write"), std::string::npos);
}

// Compare every retained declaration and body, including keyed readers and static conveniences.
TEST(output_options, constant_evaluation_retains_complete_runtime_source) {
  constexpr std::string_view schema =
      "namespace parity { enum state { idle, active } "
      "class base { public uint16 code; } "
      "class record : public base { public string label; public array uint16 samples; "
      "public state mode; public union(uint32 = number, double = real) result; } "
      "class runtime_map { public map(uint8) string values; } ";
  constexpr std::string_view native_extensions =
      "class historical stable_ids { public version { 2 } compatibility { 1 }; "
      "obsolete(2) public uint16 old_value (2); "
      "created(2) replaced(old_value) public uint32 value (3); } "
      "class box<uint64 N, T = uint16> { public array[N] T values; } ";
  for (const bool protobuf : {false, true}) {
    // The existing Protobuf profile rejects lifecycle versions and fixed-size arrays.
    const auto selected_schema = std::string{schema} +
                                 (protobuf ? std::string{} : std::string{native_extensions}) + "}";
    writer::cpp_options baseline_options{};
    baseline_options.protobuf = protobuf;
    const auto baseline = emit(selected_schema, baseline_options);
    auto enabled_options = baseline_options;
    enabled_options.constant_evaluation = true;
    const auto enabled = emit(selected_schema, enabled_options);
    EXPECT_EQ(normalized_generated_fingerprints(without_constant_declarations(enabled)),
              normalized_generated_fingerprints(baseline));
  }
}

// Reusable guards distinguish new behavior/construction bodies and configured comparison definitions.
TEST(output_options, reusable_guards_cover_behavior_construction_and_comparisons) {
  const auto schema = [](std::string_view factory_value, std::string_view predicate_value) {
    return "serializer version 1.8.0; class guard_probe { public uint64 token; "
        "public constructor(uint64 input) { token = input; } "
        "public factory seeded() { token = " + std::string{factory_value} + "; } "
        "public function bool matches() readonly predicate(token == " +
        std::string{predicate_value} + "); }";
  };
  const auto signature = [](const std::string& source) {
    const auto begin = source.find("#define SERIALIZER_GENERATED_");
    if (begin == std::string::npos) { throw std::runtime_error{"Missing generated definition guard"}; }
    return source.substr(begin, source.find('\n', begin) - begin);
  };
  writer::cpp_options options;
  const auto baseline = signature(emit(schema("1", "1"), options));
  EXPECT_EQ(baseline, signature(emit(schema("1", "1"), options)));
  for (const auto& changed : {signature(emit(schema("2", "1"), options)),
                              signature(emit(schema("1", "2"), options))}) {
    EXPECT_NE(baseline, changed);
    EXPECT_EQ(baseline.substr(0, baseline.rfind(' ')), changed.substr(0, changed.rfind(' ')));
  }
  options.class_comparisons.emplace("guard_probe", writer::class_comparison_options{true, true, {}});
  const auto compared = signature(emit(schema("1", "1"), options));
  EXPECT_NE(baseline, compared);
  EXPECT_EQ(baseline.substr(0, baseline.rfind(' ')), compared.substr(0, compared.rfind(' ')));
}

// Guard fingerprints are metadata; guard identities, generated bodies and other numbers remain observable.
TEST(output_options, runtime_source_comparison_preserves_nonmetadata_changes) {
  constexpr std::string_view before =
      "#if SERIALIZER_GENERATED_3A3A7265636F7264 != 123ULL\n"
      "#define SERIALIZER_GENERATED_3A3A7265636F7264 123ULL\n"
      "#if OTHER_GUARD != 123ULL\n#define OTHER_GUARD 123ULL\n"
      "constexpr auto value = 123ULL;\n";
  constexpr std::string_view metadata_changed =
      "#if SERIALIZER_GENERATED_3A3A7265636F7264 != 456ULL\n"
      "#define SERIALIZER_GENERATED_3A3A7265636F7264 456ULL\n"
      "#if OTHER_GUARD != 123ULL\n#define OTHER_GUARD 123ULL\n"
      "constexpr auto value = 123ULL;\n";
  const auto normalized = normalized_generated_fingerprints(std::string{before});
  EXPECT_EQ(normalized, normalized_generated_fingerprints(std::string{metadata_changed}));
  for (const auto token : {"OTHER_GUARD != 123ULL", "OTHER_GUARD 123ULL", "value = 123ULL"}) {
    auto changed = std::string{before};
    const auto begin = changed.find(token);
    ASSERT_NE(begin, std::string::npos);
    changed.replace(changed.find("123ULL", begin), std::string_view{"123"}.size(), "456");
    EXPECT_NE(normalized, normalized_generated_fingerprints(std::move(changed)));
  }
  for (const auto token : {"3A3A7265636F7264", "OTHER_GUARD", "value = 123ULL"}) {
    auto changed = std::string{before};
    const auto begin = changed.find(token);
    ASSERT_NE(begin, std::string::npos);
    changed.insert(begin, "A");
    EXPECT_NE(normalized, normalized_generated_fingerprints(std::move(changed)));
  }
  constexpr std::string_view expression = "#if SERIALIZER_GENERATED_3A3A7265636F7264 != 123ULL + 1ULL\n";
  EXPECT_EQ(normalized_generated_fingerprints(std::string{expression}), expression);
  EXPECT_NE(normalized_generated_fingerprints(emit("class record { public uint16 value; }", writer::cpp_options{})),
            normalized_generated_fingerprints(emit("class record { public uint32 value; }", writer::cpp_options{})));
}

// Restricted output retains positional codecs and rejects conflicts without losing semantics.
TEST(output_options, positional_only_generation) {
  writer::cpp_options options{};
  options.protocols = writer::cpp_protocols::binary_none;
  const auto source = emit("class record { public version { 2 }; public uint32 value; }", options);
  EXPECT_EQ(source.find("serialize_in_member_by_identifier"), std::string::npos);
  EXPECT_EQ(source.find("serialize_in_member_by_name"), std::string::npos);
  EXPECT_EQ(source.find("serializer_version_reader"), std::string::npos);
  EXPECT_NE(source.find("serializer_current_version = static_cast<std::uint8_t>(2ULL)"), std::string::npos);
  EXPECT_EQ(source.find("serialize_constant_out"), std::string::npos);
  EXPECT_NE(source.find("void serialize_out(SerializeOutProtocol&"), std::string::npos);
  EXPECT_NE(source.find("void serialize_in(SerializeInProtocol&"), std::string::npos);
  options.constant_evaluation = true;
  EXPECT_NE(emit("class record {}", options).find("constexpr void serialize_constant_out"),
            std::string::npos);
  const auto union_source = emit(
      "class node { public uint32 value; } "
      "class record { public union(uint32 = number, node = object) payload; }", options);
  EXPECT_NE(union_source.find("::std::construct_at(&this->payload.object);"), std::string::npos);
  options.protobuf = true;
  EXPECT_THROW(emit("class record {}", options), std::invalid_argument);
  options.protobuf = false;
  EXPECT_THROW(emit("class record view readonly { public uint32 value; }", options),
               std::invalid_argument);
  EXPECT_THROW(emit("class record managed { public uint32 value; }", options),
               std::invalid_argument);
}

// Borrowed models expose one concrete output method in a separate namespace without owning APIs.
TEST(output_options, emission_only_generation) {
  writer::cpp_options options{};
  options.emission_only = true;
  EXPECT_THROW(emit("class value {}", options), std::invalid_argument);
  options.constant_evaluation = true;
  EXPECT_THROW(emit("class value {}", options), std::invalid_argument);
  options.protocols = writer::cpp_protocols::binary_none;
  const auto source = emit(
      "namespace outer { enum state { ready } class base { public uint16 code; } "
      "namespace inner { class child { public string label; } } "
      "class record : public base { public string label; public array uint8 data; "
      "public array[3] uint16 fixed; public inner::child nested; "
      "public state mode { state::ready }; public union(uint32 = number, inner::child = node) payload; } "
      "class box<T = inner::child> { public array T values; } "
      "class generic_value<T> { public T first; public T second; } }", options);
  EXPECT_NE(source.find("#include <rohit/constant_binary_output.hpp>"), std::string::npos);
  EXPECT_NE(source.find("namespace emission {"), std::string::npos);
  EXPECT_NE(source.find("::outer::inner::emission::child"), std::string::npos);
  EXPECT_NE(source.find("::outer::emission::base"), std::string::npos);
  EXPECT_NE(source.find("::outer::emission::state::ready"), std::string::npos);
  EXPECT_NE(source.find("::std::string_view label"), std::string::npos);
  EXPECT_NE(source.find("::std::span<const ::std::uint8_t> data"), std::string::npos);
  EXPECT_NE(source.find("::std::span<const ::std::uint16_t> fixed"), std::string::npos);
  EXPECT_NE(source.find("Emission array extent mismatch"), std::string::npos);
  EXPECT_NE(source.find("serializer_emission_only = true"), std::string::npos);
  EXPECT_EQ(source.find("is_trivially_copyable"), std::string::npos);
  EXPECT_NE(source.find("is_trivially_destructible"), std::string::npos);
  EXPECT_NE(source.find("constexpr void serialize_out(::rohit::serializer::binary_none_output&"),
            std::string::npos);
  for (const auto removed : {"serialize_constant_out", "serialize_in(", "static auto serialize(",
                              "::std::vector<", "::std::map<", "::std::string to_string"}) {
    EXPECT_EQ(source.find(removed), std::string::npos) << removed;
  }
  for (const auto invalid : {"class record { public map(uint8) string values; }",
                              "class record { public array[2] uint8 values {1,2}; }",
                              "class emission {}", "namespace emission { class record {} }"}) {
    EXPECT_THROW(emit(invalid, options), std::invalid_argument) << invalid;
  }
  options.rename_identifiers = false;
  EXPECT_NE(emit("namespace api { enum State { Ready } "
                "class Record { public State mode { State::Ready }; } }", options)
                .find("::api::emission::State::Ready"), std::string::npos);
}

// Keep header declarations readable without changing their exact byte-array contract.
TEST(output_options, readable_magic_literals_in_owning_and_emission_profiles) {
  constexpr std::string_view schema = R"schema(
    class readable_header { public magic (100) { 'HEAD1' }; public uint8 value (1); }
    class escaped_header {
      public magic (100) { 'A\'\"\\\0\n\xC3\xA9\xC2\x80\xC3\xBFF' };
      public uint8 value (1);
    }
  )schema"
      "class question_header { public magic (100) { '" "?" "?/" "' }; }";
  constexpr std::string_view readable_comment =
      R"comment(// Fixed schema-owned header "HEAD1"; no per-object storage.)comment";
  constexpr std::string_view readable_array =
      "inline static constexpr char magic[] = {'H', 'E', 'A', 'D', '1'};";
  constexpr std::string_view escaped_comment =
      R"comment(// Fixed schema-owned header "A\'\"\\\000\n\303\251\302\200\303\277F"; no per-object storage.)comment";
  constexpr std::string_view escaped_array =
      R"magic(inline static constexpr char magic[] = {'A', '\'', '\"', '\\', '\000', '\n', '\303', '\251', '\302', '\200', '\303', '\277', 'F'};)magic";
  constexpr std::string_view question_comment =
      R"comment(// Fixed schema-owned header "\?\?/"; no per-object storage.)comment";
  constexpr std::string_view question_array =
      R"magic(inline static constexpr char magic[] = {'\?', '\?', '/'};)magic";
  for (const bool emission_only : {false, true}) {
    SCOPED_TRACE(emission_only ? "emission" : "owning");
    writer::cpp_options options{};
    if (emission_only) {
      options.constant_evaluation = true;
      options.protocols = writer::cpp_protocols::binary_none;
      options.emission_only = true;
    }
    const auto source = emit(schema, options);
    EXPECT_NE(source.find(readable_comment), std::string::npos);
    EXPECT_NE(source.find(readable_array), std::string::npos);
    EXPECT_NE(source.find(escaped_comment), std::string::npos);
    EXPECT_NE(source.find(escaped_array), std::string::npos);
    EXPECT_NE(source.find(question_comment), std::string::npos);
    EXPECT_NE(source.find(question_array), std::string::npos);
    EXPECT_EQ(source.find("static_cast<char>"), std::string::npos);
    EXPECT_EQ(source.find(std::string(1, '\0')), std::string::npos);
    EXPECT_EQ(source.find(std::string(1, static_cast<char>(0x80))), std::string::npos);
    EXPECT_EQ(source.find(std::string(1, static_cast<char>(0xc3))), std::string::npos);
  }
}

// Allocator-aware ownership is explicit and leaves existing standard-container output unchanged.
TEST(output_options, payload_allocator_profile_is_opt_in_and_rejects_unsupported_contracts) {
  constexpr std::string_view source =
      "class record { public string text; public array string values; public map(uint32) string lookup; }";
  writer::cpp_options options;
  options.format = false;
  const auto defaults = emit(source, options);
  EXPECT_EQ(defaults.find("payload_allocator.hpp"), std::string::npos);
  options.payload_allocator = true;
  const auto generated = emit(source, options);
  EXPECT_NE(generated.find("::rohit::serializer::payload_string"), std::string::npos);
  EXPECT_NE(generated.find("serializer_payload_resources_match"), std::string::npos);
  EXPECT_NE(generated.find("Allocator-aware payloads support positional and integer-key native binary only"),
      std::string::npos);
  for (const auto contract : {"class record packed { public uint32 value; }",
                             "class record { public magic {'HDR'}; public uint32 value; }",
                             "class record { public uint32 serializer_payload_binding; }"}) {
    EXPECT_THROW(emit(contract, options), std::exception);
  }
  options.managed_separate_values = true;
  EXPECT_THROW(emit(source, options), std::invalid_argument);
  options.managed_separate_values = false;
  options.constant_evaluation = true;
  EXPECT_THROW(emit(source, options), std::invalid_argument);
}
