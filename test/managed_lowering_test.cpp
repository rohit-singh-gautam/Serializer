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
        "class item managed { private uint32 value; }",
        "class item managed { public array item children; }",
        "class base {} class item managed : public base { public uint32 value; }"}) {
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
