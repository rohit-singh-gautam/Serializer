#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>
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
