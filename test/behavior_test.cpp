// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/output_options.hpp>
#include <rohit/serializer.hpp>
#include "../src/behavior_writer.hpp"
#include "../src/managed_schema.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <sstream>
#include <rohit/schema_compatibility.hpp>
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace codec = rohit::serializer;

// Resolve test input with the same parser and type checks used by the compiler.
codec::parser::parsed_schema parse_schema(std::string_view text) {
  auto input = rohit::make_constant_full_stream(text.data(), text.size());
  return {codec::parser::parse(input), {}};
}

// Behavior snippets belong to a modern owning model and retain explicit durable IDs.
std::string document_with(std::string_view body) {
  return "serializer version 1.6.0; class document_metrics stable_ids { "
         "public double page_width (1); public int32 page_count (2); " + std::string{body} + " }";
}
}

// Source order includes fields, methods and opaque class hooks without consuming wire IDs.
TEST(behavior, source_order_and_native_literal_scanning) {
  const auto parsed = parse_schema(R"SCHEMA(serializer version 1.6.0;
cpp preamble { #include <string_view> }
class document_data stable_ids {
  public double measure (1);
  public cpp { using label_type = std::string_view; }
  public function double measure_value() readonly cpp {
    auto label = R"tag({ /* literal */ } )tag";
    (void)label;
    return measure;
  };
  public int32 page_count (2);
})SCHEMA");
  ASSERT_EQ(parsed.statements.size(), 2U);
  EXPECT_EQ(parsed.statements[0]->type, codec::object_type::native_code);
  const auto& model = static_cast<const codec::class_node&>(*parsed.statements[1]);
  ASSERT_EQ(model.body_order.size(), 4U);
  EXPECT_EQ(model.body_order[1].kind, codec::class_body_item::kind_type::native_code);
  EXPECT_EQ(model.body_order[2].kind, codec::class_body_item::kind_type::function);
  ASSERT_EQ(model.functions.size(), 1U);
  EXPECT_NE(model.functions[0].bodies[0].text.find("R\"tag({ /* literal */ } )tag\""), std::string::npos);
  codec::writer::cpp_options options; options.format = false; options.rename_identifiers = false;
  const auto output = codec::writer::cpp::generate(parsed.statements, options);
  EXPECT_LT(output.find("using label_type"), output.find("measure_value()"));
}

// Portable arithmetic is bound to declared double/int32/uint32 symbols before emission.
TEST(behavior, expression_types_and_signature_failures) {
  EXPECT_NO_THROW(parse_schema(document_with("public function double area(double copies) readonly expression(page_width * to_double(page_count) * copies);")));
  for (const auto text : {
      "public function double area() readonly expression(page_width * page_count);",
      "public function double area() readonly expression(unknown + page_width);",
      "public function int32 area() readonly expression(page_width);",
      "public function double area(double copies, double copies) readonly;",
      "public function double area() edit expression(page_width);",
      "public function abstract double area() readonly cpp { return 1.0; };",
      "public function double area() readonly cpp { return 1.0; } cpp { return 2.0; };",
      "public function double area() readonly cpp { return 1.0; } expression(page_width);",
      "public function double page_width() readonly;",
      "public function double area() readonly; public function double area() readonly;"}) {
    SCOPED_TRACE(text);
    EXPECT_THROW(parse_schema(document_with(text)), std::exception);
  }
}

// New declarations cannot silently change an older schema's grammar contract.
TEST(behavior, language_gate_and_unterminated_blocks) {
  EXPECT_THROW(parse_schema("serializer version 1.5.0; class document { public function double size() readonly; }"), rohit::exception::base_parser);
  EXPECT_THROW(parse_schema(document_with("public function double area() readonly cpp { /* unfinished")), rohit::exception::base_parser);
  EXPECT_THROW(parse_schema(document_with("public function double area() readonly cpp { return R\"tag(unclosed")), rohit::exception::base_parser);
}

// Methods and native text do not enter any protocol's persisted schema projection.
TEST(behavior, wire_compatibility_excludes_behavior_and_runtime_state) {
  const auto old = parse_schema("serializer version 1.6.0; class document stable_ids { public double measure (1); }");
  const auto next = parse_schema("serializer version 1.6.0; class document stable_ids { public double measure (1); private transient uint64 cache {0}; public function double value() readonly expression(measure); }");
  for (const auto protocol : {codec::compatibility_protocol::json,
      codec::compatibility_protocol::binary_none, codec::compatibility_protocol::binary_integer,
      codec::compatibility_protocol::binary_string}) {
    EXPECT_TRUE(codec::check_schema_compatibility(old, next, protocol).empty());
  }
}

// Existing lifecycle prefixes remain visible to the member parser before behavior lookahead.
TEST(behavior, lifecycle_member_prefix_is_preserved) {
  EXPECT_NO_THROW(parse_schema("serializer version 1.6.0; class document stable_ids { public version uint32 revision (1) {2} compatibility {1}; obsolete(2) public string old_title (2); public string title (3); }"));
}

// Every ordinary backend emits a bound method; opaque code requires preserved spelling.
TEST(behavior, ordinary_backends_emit_portable_methods) {
  const auto parsed = parse_schema(document_with("public function double area() readonly expression(page_width * to_double(page_count));"));
  codec::writer::cpp_options cpp; cpp.format = false;
  EXPECT_NE(codec::writer::cpp::generate(parsed.statements, cpp).find("area()"), std::string::npos);
  EXPECT_NE(codec::writer::java::generate(parsed.statements, "Schema").find("area()"), std::string::npos);
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    SCOPED_TRACE(language);
    EXPECT_FALSE(codec::writer::portable::generate(parsed.statements, language, "Schema").empty());
  }
  const auto native = parse_schema(document_with("public function double area() readonly cpp { return page_width; };"));
  EXPECT_THROW(codec::writer::cpp::generate(native.statements, cpp), std::invalid_argument);
}

// Codec-owning data never needs a vtable implementation to satisfy an abstract behavior contract.
TEST(behavior, validates_overrides_and_multiple_base_ambiguity) {
  const std::string base = "serializer version 1.6.0; class document_base stable_ids { public double measure (1); public function abstract double score(double extra) readonly; } ";
  EXPECT_NO_THROW(parse_schema(base + "class document_data stable_ids : public document_base (\"base\",1) { public function override double score(double extra) readonly expression(extra); }"));
  EXPECT_THROW(parse_schema(document_with("public function override double score() readonly;")), std::exception);
  EXPECT_THROW(parse_schema(base + "class document_data stable_ids : public document_base (\"base\",1) { public function override int32 score(double extra) readonly; }"), std::exception);
  EXPECT_THROW(parse_schema(base + "class document_data stable_ids : public document_base (\"base\",1) { public function override double score(double extra) edit; }"), std::exception);
  const std::string two = base + "class document_metadata stable_ids { public function virtual double score(double extra) readonly; } ";
  EXPECT_THROW(parse_schema(two + "class document_data stable_ids : public document_base (\"base\",1), public document_metadata (\"metadata\",2) {}"), std::exception);
  EXPECT_NO_THROW(parse_schema(two + "class document_data stable_ids : public document_base (\"base\",1), public document_metadata (\"metadata\",2) { public function override double score(double extra) readonly expression(extra); }"));
  EXPECT_THROW(parse_schema("serializer version 1.6.0; class document managed stable_ids { public function virtual void update() edit; }"), std::exception);
}

// C# interpolation expressions and Rust raw strings contain braces that are not schema delimiters.
TEST(behavior, native_interpolation_and_raw_string_boundaries) {
  const auto parsed = parse_schema(document_with(R"SCHEMA(public function double score() readonly
    csharp { var text = $"{{literal}} {string.Concat("{", "}")}"; return page_width; }
    rust { let text = r###"{ "literal" }"###; let _ = text; self.page_width }
    python { text = """{
  preserved literal indentation
}"""; return self.page_width };
  )SCHEMA"));
  const auto& owner = static_cast<const codec::class_node&>(*parsed.statements[0]);
  ASSERT_EQ(owner.functions[0].bodies.size(), 3U);
  EXPECT_NE(owner.functions[0].bodies[2].text.find("  preserved literal indentation"), std::string::npos);
}

// Behavior/API/native changes and runtime-only values cannot alter durable synchronization identity.
TEST(behavior, managed_fingerprint_excludes_behavior_and_runtime_state) {
  const auto original = parse_schema("serializer version 1.6.0; class document managed stable_ids { public double measure (1); }");
  const auto changed = parse_schema("serializer version 1.6.0; cpp preamble { /* implementation-only */ } class document managed stable_ids { public double measure (1); private transient uint64 cache {17}; public function virtual double calculate(double extra) readonly cpp { return measure + extra; }; }");
  std::map<const codec::class_node*, std::uint64_t> old_cache, new_cache;
  const auto* old_model = static_cast<const codec::class_node*>(original.statements[0].get());
  const auto* new_model = static_cast<const codec::class_node*>(changed.statements[1].get());
  EXPECT_EQ(codec::writer::managed_schema_hash(old_model, old_cache), codec::writer::managed_schema_hash(new_model, new_cache));
}

// Source spans are available without warning callbacks and final anchors contain physical output lines.
TEST(behavior, source_spans_and_post_generation_mapping) {
  const auto parsed = parse_schema("serializer version 1.6.0;\nclass document stable_ids {\n public double measure (1);\n public function double calculate() readonly cpp {\n   return measure;\n };\n}");
  const auto& owner = static_cast<const codec::class_node&>(*parsed.statements[0]);
  const auto& method = owner.functions[0];
  EXPECT_EQ(method.source_line, 4U);
  EXPECT_EQ(method.bodies[0].source_line, 4U);
  EXPECT_GT(method.source_end, method.source_offset);
  codec::writer::cpp_options options; options.format = false; options.rename_identifiers = false;
  const auto source = codec::writer::cpp::generate(parsed.statements, options);
  EXPECT_EQ(source.find("@GENERATED_LINE@"), std::string::npos);
  EXPECT_NE(source.find("#line 4 \"<schema>\""), std::string::npos);
  std::istringstream lines{source}; std::string line; std::size_t number{};
  while (std::getline(lines, line)) {
    ++number;
    if (line.find("serializer-source-map:") != std::string::npos) {
      EXPECT_NE(line.find("\"generated_line\":" + std::to_string(number + 1)), std::string::npos);
    }
  }
}

// Complete-unit formatting is idempotent and never rewrites authored access order or raw literal tokens.
TEST(behavior, complete_unit_formatting_preserves_native_code) {
  std::string formatter;
#ifdef _MSC_VER
  char* environment_value{}; std::size_t environment_length{};
  ASSERT_EQ(_dupenv_s(&environment_value, &environment_length, "SERIALIZER_TEST_CLANG_FORMAT"), 0);
  if (environment_value) { formatter = environment_value; }
  std::free(environment_value);
#else
  if (const auto* environment_value = std::getenv("SERIALIZER_TEST_CLANG_FORMAT")) { formatter = environment_value; }
#endif
  if (formatter.empty()) { GTEST_SKIP() << "Set SERIALIZER_TEST_CLANG_FORMAT to the configured clang-format 19+"; }
  const auto parsed = parse_schema(R"SCHEMA(serializer version 1.6.0;
cpp preamble { #include <string_view> }
class document stable_ids {
 public double measure (1);
 private cpp { friend class document_reader; }
 public cpp { using label_type = std::string_view; }
 public function double calculate() readonly cpp {
   auto text = R"tag({ literal  spacing })tag";
   (void)text;
   return measure;
 };
})SCHEMA");
  const std::string schema_path = "schemas/document metrics with spaces.serializer";
  auto& preamble = static_cast<codec::native_code_node&>(*parsed.statements[0]);
  preamble.code.source_path = schema_path;
  auto& model = static_cast<codec::class_node&>(*parsed.statements[1]);
  for (auto& block : model.native_blocks) { block.source_path = schema_path; }
  for (auto& method : model.functions) {
    method.source_path = schema_path;
    for (auto& body : method.bodies) { body.source_path = schema_path; }
  }
  for (const auto profile : {"serializer", "core", "google", "llvm", "gnu", "cert", "misra", "autosar", "qt"}) {
    SCOPED_TRACE(profile);
    codec::writer::cpp_options options; options.rename_identifiers = false; options.clang_format = formatter;
    options.standard = codec::writer::parse_coding_standard(profile);
    const auto source = codec::writer::cpp::generate(parsed.statements, options);
    EXPECT_EQ(codec::writer::behavior::finalize_mappings(codec::writer::format_cpp(source, options)), source);
    EXPECT_LT(source.find("friend class document_reader"), source.find("using label_type"));
    ASSERT_NE(source.find("calculate"), std::string::npos);
    EXPECT_LT(source.find("using label_type"), source.find("calculate"));
    EXPECT_NE(source.find(R"CHECK(R"tag({ literal  spacing })tag")CHECK"), std::string::npos);
    EXPECT_NE(source.find("#line 7 \"" + schema_path + "\""), std::string::npos);
    EXPECT_EQ(source.find("@GENERATED_LINE@"), std::string::npos);
    std::istringstream lines{source}; std::string line; std::size_t number{}, markers{};
    while (std::getline(lines, line)) {
      ++number;
      if (line.find("serializer-source-map:") == std::string::npos) { continue; }
      ++markers;
      EXPECT_NE(line.find("\"generated_line\":" + std::to_string(number + 1)), std::string::npos);
      const auto begin = line.find("\"schema\":") + std::string_view{"\"schema\":"}.size();
      const auto end = line.find("\",\"line\"", begin);
      ASSERT_NE(end, std::string::npos);
      const auto quoted_path = line.substr(begin, end - begin + 1);
      auto stream = rohit::make_constant_stream(quoted_path.data(), quoted_path.size());
      codec::json<codec::serialize_type::in> input(stream);
      std::string restored_path;
      input.serialize_in(restored_path); input.finish();
      EXPECT_EQ(restored_path, schema_path);
    }
    EXPECT_GE(markers, 4U);
  }
}

// External declarations expose explicit qualification for otherwise-unused required definitions.
TEST(behavior, required_definition_contracts_are_generated) {
  const auto parsed = parse_schema("serializer version 1.6.0; class document stable_ids { public function double required(double extra) readonly; }");
  codec::writer::cpp_options options; options.format = false;
  const auto cpp = codec::writer::cpp::generate(parsed.statements, options);
  EXPECT_NE(cpp.find("serializer_require_behavior_definitions()"), std::string::npos);
  EXPECT_NE(cpp.find("volatile required0"), std::string::npos);
  const auto c = codec::writer::portable::generate(parsed.statements, "c", "Schema");
  EXPECT_NE(c.find("document_require_behavior_definitions(void)"), std::string::npos);
}

// Mapping finalization must never replace a native literal merely resembling its private placeholders.
TEST(behavior, mapping_finalization_preserves_native_literals) {
  const std::string native = R"CHECK(const char* token = "@GENERATED_LINE@";
const char* reset = "#line 1 \"<serializer-generated>\"";
const char* metadata = "\"generated_line\":@GENERATED_LINE@";
const char* multiline = R"RAW(
#line 1 "<serializer-generated>"
@GENERATED_LINE@
)RAW";
)CHECK";
  EXPECT_EQ(codec::writer::behavior::finalize_mappings(native), native);
}

// Previously legal type words resolve normally when declarations precede their use in old and current schemas.
TEST(behavior, legacy_type_names_remain_unambiguous) {
  for (const auto version : {"1.5.0", "1.6.0"}) {
    const auto text = std::string{"serializer version "} + version +
        "; class transient stable_ids { public int32 count (1); }"
        " class function stable_ids { public int32 count (1); }"
        " class managed stable_ids { public int32 count (1); }"
        " class holder stable_ids { public transient transient_value (1); public function function_value (2); public function variant (3); public transient array (4); }"
        " class derived stable_ids : public managed (\"base\", 1) { public int32 own_count (2); }";
    const auto parsed = parse_schema(text);
    const auto& holder = static_cast<const codec::class_node&>(*parsed.statements[3]);
    ASSERT_EQ(holder.member_list.size(), 4U);
    EXPECT_FALSE(holder.member_list[0].transient);
    EXPECT_TRUE(holder.functions.empty());
    const auto& derived = static_cast<const codec::class_node&>(*parsed.statements[4]);
    EXPECT_FALSE(derived.parents[0].managed);
  }
}
