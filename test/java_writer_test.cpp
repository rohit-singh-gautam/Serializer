#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
// Exercise real schema parsing and Java emission without external dependencies.
std::string emit_java(std::string_view schema, rohit::serializer::writer::java_options options = {},
                      std::string_view outer = "Schema") {
  const auto input = rohit::make_constant_stream(schema.data(), schema.size());
  const auto nodes = rohit::serializer::parser::parse(input);
  rohit::full_stream_auto_alloc output{};
  rohit::serializer::writer::java::write(output, nodes, outer, options);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Java literal classification preserves large accepted strings/numbers and clean legacy errors.
TEST(java_writer, large_literal_defaults_preserve_backend_contracts) {
  constexpr std::size_t literal_bytes = 32768;
  const auto text = std::string(literal_bytes, 'a');
  const auto number = "1." + std::string(literal_bytes, '0');
  const auto source = "serializer version 1.14.0; class account { public string text { \"" +
      text + "\" }; public double number { " + number + " }; }";
  const auto output = emit_java(source);
  EXPECT_NE(output.find(text), std::string::npos);
  EXPECT_NE(output.find(number), std::string::npos);
  EXPECT_THROW(emit_java("serializer version 1.13.0; class account { public uint64 value { " +
      std::string(literal_bytes, '9') + " }; }"), std::invalid_argument);
}

// Concrete generic lowering retains the original field coordinates in backend errors.
TEST(java_writer, legacy_default_errors_retain_member_locations) {
  constexpr std::string_view sources[]{
      "serializer version 1.13.0;\nclass sample {\n  public uint8 value {999};\n}",
      "serializer version 1.13.0;\nclass holder<T> {\n  public T value {999};\n}\n"
      "instantiate tiny = holder<uint8>;"};
  for (const auto source : sources) {
    try {
      static_cast<void>(emit_java(source));
      FAIL() << "Expected invalid legacy Java default";
    } catch (const std::invalid_argument& error) {
      EXPECT_EQ(std::string_view{error.what()},
          "<schema>:3:3: member value: Java integer default out of range: value");
    }
  }
}

// Additive output-window codecs preserve ordinary encoding and private helper/name boundaries.
TEST(java_writer, caller_owned_output_overloads_and_helper_names) {
  constexpr std::string_view schema =
      "class fixed_buffer { public uint32 value; } class account { public string label; }";
  EXPECT_EQ(emit_java(schema).find("public int encode(Protocol protocol, byte[] destination"), std::string::npos);
  rohit::serializer::writer::java_options options{};
  options.output_buffer_reuse = true;
  const auto output = emit_java(schema, options);
  EXPECT_NE(output.find("public byte[] encode(Protocol protocol)"), std::string::npos);
  EXPECT_NE(output.find("public int encode(Protocol protocol, byte[] destination, int offset, int length)"),
      std::string::npos);
  EXPECT_NE(output.find("class FixedBuffer {"), std::string::npos);
  EXPECT_NE(output.find("window must not alias mutable model storage"), std::string::npos);
  EXPECT_NE(output.find("private static final class FixedBuffer extends SizeBuffer"), std::string::npos);
  const auto renamed = emit_java("class account { public uint32 value; }", options, "FixedBuffer");
  EXPECT_NE(renamed.find("private static final class FixedBuffer_2 extends SizeBuffer"), std::string::npos);
  EXPECT_NE(renamed.find("new FixedBuffer_2(destination, offset, length)"), std::string::npos);
}

// Profile renaming must preserve serialized names and explicit identifiers.
TEST(java_writer, naming_profiles_and_wire_contract) {
  const auto schema = "namespace wire_api { enum state { ReadyNow, Waiting } "
                      "class account stable_ids { public uint32 account_id (17); "
                      "public state current_state (18) { state::ReadyNow }; } }";
  auto source = emit_java(schema);
  EXPECT_NE(source.find("class WireApi"), std::string::npos);
  EXPECT_NE(source.find("int accountId"), std::string::npos);
  EXPECT_NE(source.find("READY_NOW(\"ReadyNow\")"), std::string::npos);
  EXPECT_NE(source.find("out.field(17, \"account_id\""), std::string::npos);
  rohit::serializer::writer::java_options options{};
  options.standard = rohit::serializer::writer::java_coding_standard::oracle;
  options.package_name = "serializer.test";
  source = emit_java(schema, options);
  EXPECT_NE(source.find("package serializer.test;"), std::string::npos);
  EXPECT_NE(source.find("    public static final class WireApi"), std::string::npos);
  options.rename_identifiers = false;
  source = emit_java(schema, options);
  EXPECT_NE(source.find("class wire_api"), std::string::npos);
  EXPECT_NE(source.find("int account_id"), std::string::npos);
}

// Emit literal spaces without replacing them with escapes or trimming the string contents.
TEST(java_writer, preserves_spaces_in_string_defaults) {
  const auto source = emit_java(R"(class record { public string label { "  schema default  " }; })");
  EXPECT_NE(source.find(R"("  schema default  ")"), std::string::npos);
}

// Invalid Java output must be rejected before a destination can be overwritten.
TEST(java_writer, rejects_unsupported_and_colliding_declarations) {
  for (const auto schema :
       {"class record view { public int32 value; }", "class record packed { public int32 value; }",
        "class account { public int32 account_id; public int32 accountId; }",
        "class account { public int32 class; }", "class reader {}", "namespace a { class a {} }",
        "enum state { ready_now, ReadyNow }",
        "class account { public int32 value { sizeof(int) }; }",
        "class account { public uint8 value { 256 }; }",
        "class account { public int8 value { 128 }; }",
        "class account { public int64 value { 9223372036854775808 }; }",
        "class account { public uint64 value { -1 }; }",
        "class account { public float value { 1e100 }; }",
        "class account { public account value; }",
        "class account { public map(float) int32 values; }",
        "class key {} class account { public map(key) int32 values; }"}) {
    EXPECT_THROW(emit_java(schema), std::invalid_argument) << schema;
  }
  EXPECT_THROW(emit_java("class account {}", {}, "bad-name"), std::invalid_argument);
  EXPECT_THROW(emit_java("class account {}", {}, "Reader"), std::invalid_argument);
  rohit::serializer::writer::java_options options{};
  options.package_name = "bad..name";
  EXPECT_THROW(emit_java("class account {}", options), std::invalid_argument);
}
} // namespace
