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

// Generate selected optional capabilities through the production schema writer.
std::string emit(std::string_view schema, writer::cpp_options options) {
  const auto input = rohit::make_constant_stream(schema.data(), schema.size());
  const auto statements = rohit::serializer::parser::parse(input);
  options.format = false;
  return writer::cpp::generate(statements, options);
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

// Central managed identity width is strict and independent of C++ presentation settings.
TEST(output_options, managed_identity_configuration) {
  configuration_file file;
  EXPECT_EQ(writer::read_output_options(file.write("[managed]\nid_type = uint64\n")).cpp.managed_id_type, "uint64");
  EXPECT_EQ(writer::read_output_options(file.write("[managed]\nid_type = uint32\n")).cpp.managed_id_type, "uint32");
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
    EXPECT_EQ(without_constant_declarations(enabled), baseline);
  }
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
