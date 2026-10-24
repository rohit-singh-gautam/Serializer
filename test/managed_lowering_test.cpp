#include "../src/managed_lowering.hpp"

#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace serializer = rohit::serializer;
namespace writer = serializer::writer;

// Parse through the normal schema resolver so copied references exercise real namespace ownership.
auto parse(std::string_view text) {
  return serializer::parser::parse(rohit::make_constant_stream(text.data(), text.size()));
}

// Remove runtime eligibility from a lowered fixture to test each existing ordinary codec emitter.
void codec_fixture(std::vector<std::unique_ptr<serializer::syntax_node>>& nodes) {
  for (auto& node : nodes) {
    if (node->type == serializer::object_type::namespace_type) {
      codec_fixture(static_cast<serializer::namespace_node&>(*node).statements);
    } else if (node->type == serializer::object_type::class_type) {
      auto& object = static_cast<serializer::class_node&>(*node);
      object.attributes = serializer::class_attributes::stable_ids;
      for (auto& field : object.member_list) {
        field.managed = false;
      }
    }
  }
}
} // namespace

// Every ordinary emitter must preserve the reserved identity field and resolve copied nested types.
TEST(managed_lowering, preserves_identity_in_every_language_codec) {
  const auto source =
      parse("namespace sample { class point managed stable_ids { public int32 x (1); } } "
            "namespace sample { class drawing stable_ids { public managed array point points (1); "
            "public point origin (2); } }");
  for (const auto width : {"uint32", "uint64"}) {
    writer::managed_lowering lowered{source, width};
    ASSERT_EQ(lowered.bindings.size(), 2u);
    writer::cpp_options cpp;
    cpp.format = false;
    cpp.managed_id_type = width;
    const auto original_cpp = writer::cpp::generate(source, cpp);
    for (const auto& [node, binding] : lowered.bindings) {
      EXPECT_NE(original_cpp.find(binding), std::string::npos);
      ASSERT_FALSE(node->member_list.empty());
      EXPECT_EQ(node->member_list.back().display_name, "persistent_id");
      EXPECT_EQ(node->member_list.back().id, serializer::constants::variable_four_byte_max);
      EXPECT_EQ(node->member_list.back().type_name_list.front().name, width);
    }
    codec_fixture(lowered.statements);
    EXPECT_FALSE(writer::java::generate(lowered.statements, "Schema", {}).empty());
    for (const auto language :
         {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
      EXPECT_FALSE(writer::portable::generate(lowered.statements, language, "Schema", {}).empty())
          << language;
    }
    // Preparing another language must never append metadata to the parsed source tree.
    const auto& space = static_cast<const serializer::namespace_node&>(*source.front());
    EXPECT_EQ(
        static_cast<const serializer::class_node&>(*space.statements.front()).member_list.size(),
        1u);
  }
}

// Unsupported shapes fail before a target can silently weaken identity or ownership semantics.
TEST(managed_lowering, rejects_invalid_managed_shapes) {
  for (const auto text :
       {"class item managed { public uint32 persistent_id (1); }",
        "class item managed { public uint32 data (1073741823); }",
        "class item managed { public array item children; }",
        "class base {} class item managed : private base { public uint32 value; }",
        "class metadata {} class section : public metadata {} class indexed : public metadata {} "
        "class document managed : public section, public indexed { public uint32 count; }"}) {
    const auto source = parse(text);
    EXPECT_THROW((void)writer::managed_lowering{source}, std::invalid_argument) << text;
  }
  EXPECT_THROW(writer::managed_lowering(parse("class item managed {}"), "uint16"),
               std::invalid_argument);
}

// Ordinary recursive collections and enum map keys retain valid copied references.
TEST(managed_lowering, preserves_unmanaged_shapes) {
  const auto source =
      parse("namespace sample { enum kind { a, b } "
            "class item { public array item children; public map(kind) string names; } }");
  writer::managed_lowering lowered{source};
  EXPECT_TRUE(lowered.bindings.empty());
  EXPECT_FALSE(writer::portable::generate(lowered.statements, "python", "Schema", {}).empty());
}

// Private fields and distinct public bases retain their durable metadata during language lowering.
TEST(managed_lowering, preserves_private_fields_and_public_bases) {
  const auto source = parse("class metadata stable_ids { private string author (1); } "
                            "class document managed stable_ids : public metadata (\"metadata\", 1) { "
                            "private uint32 pages (2); }");
  writer::managed_lowering lowered{source};
  ASSERT_EQ(lowered.bindings.size(), 1u);
  const auto& document = static_cast<const serializer::class_node&>(*lowered.statements.back());
  ASSERT_EQ(document.parents.size(), 1u);
  EXPECT_EQ(document.parents.front().display_name, "metadata");
  EXPECT_EQ(document.member_list.front().access, serializer::access_type::private_access);
}

// Python wire records are explicit and cannot be mistaken for native managed engines.
TEST(managed_lowering, python_token_codecs_require_explicit_selection) {
  const auto source = parse("serializer version 1.20.0; class item managed class_id(101) { "
      "public optional<managed_ref<item>> next (1); } class envelope { public managed_ref<item> target (1); }");
  EXPECT_THROW(writer::portable::generate(source, "python", "Schema"), std::invalid_argument);
  writer::portable_options options;
  options.managed_token_codecs = true;
  const auto generated = writer::portable::generate(source, "python", "Schema", options);
  EXPECT_NE(generated.find("class ManagedReference:"), std::string::npos);
  EXPECT_NE(generated.find("managed_class_id = 101"), std::string::npos);
  EXPECT_NE(generated.find("ManagedReference._read(input, 101, 4, True)"), std::string::npos);
  EXPECT_THROW(writer::portable::generate(source, "js", "Schema", options), std::invalid_argument);
  // Lowering never modifies declarations reused by another backend.
  EXPECT_TRUE(static_cast<const serializer::class_node&>(*source.front()).supports_managed());
}

// Unsupported wire-record contracts fail before source emission rather than losing behavior.
TEST(managed_lowering, python_token_codecs_reject_unsupported_contracts) {
  writer::portable_options options;
  options.managed_token_codecs = true;
  for (const auto text : {
      "serializer version 1.20.0; class item managed class_id(7) {} class other managed class_id(7) {}",
      "serializer version 1.23.0; class item<T> managed { public T value; } instantiate named = item<uint32>;",
      "serializer version 1.20.0; class item managed { public constructor() {} }",
      "serializer version 1.20.0; class item managed {} class nested { public managed_ref<item> ref; } "
      "class root { public nested required omit(json); }",
      "serializer version 1.20.0; class ManagedReference managed {}",
      "serializer version 1.20.0; class item managed { public uint32 managed_class_id; }"}) {
    const auto source = parse(text);
    EXPECT_THROW(writer::portable::generate(source, "python", "Schema", options), std::invalid_argument) << text;
  }
}

// Native Python lifetime engines require generated descriptors and cannot be inferred from codecs.
TEST(managed_lowering, python_native_engine_emits_exact_descriptors_and_rejects_unsupported_profiles) {
  writer::portable_options options;
  options.managed_engine = true;
  const auto source = parse("serializer version 1.20.0; class item managed class_id(101) { public string label (1); } "
      "class model managed { public managed array item children (1); }");
  const auto generated = writer::portable::generate(source, "python", "Schema", options);
  EXPECT_NE(generated.find("class ManagedStore:"), std::string::npos);
  EXPECT_NE(generated.find("ManagedField(\"children\", ('array', Item), True)"), std::string::npos);
  EXPECT_NE(generated.find("def managed_schema(root_type, **options):"), std::string::npos);
  EXPECT_EQ(writer::portable::generate(source, "python", "Schema", writer::portable_options{
      .managed_token_codecs = true}).find("class ManagedStore:"), std::string::npos);
  for (const auto text : {"class ManagedStore managed {}", "class ManagedField managed {}",
                         "class base {} class item managed : public base {}",
                         "class item managed { public transient uint32 cache; }"}) {
    EXPECT_THROW(writer::portable::generate(parse("serializer version 1.20.0; " + std::string{text}), "python", "Schema", options), std::invalid_argument) << text;
  }
  EXPECT_THROW(writer::portable::generate(source, "js", "Schema", options), std::invalid_argument);
}
