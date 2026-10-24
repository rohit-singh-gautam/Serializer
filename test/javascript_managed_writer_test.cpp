// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace writer = rohit::serializer::writer;

// Exercise the production parser and JS/TS emitter without launching an SDK.
std::string emit(std::string_view schema, std::string_view language,
                 const writer::portable_options& options = {}) {
  const auto nodes = rohit::serializer::parser::parse(rohit::make_constant_stream(schema.data(), schema.size()));
  return writer::portable::generate(nodes, language, "ManagedSdk", options);
}
} // namespace

// Default outputs keep their established acceptance and managed activation remains explicit.
TEST(javascript_managed_writer, requires_explicit_native_selection) {
  constexpr std::string_view schema = "serializer version 1.20.0; class item managed class_id(101) { public optional<managed_ref<item>> next; }";
  for (const auto language : {"js", "typescript"}) {
    EXPECT_THROW(emit(schema, language), std::invalid_argument);
    EXPECT_NE(emit("class ManagedStore {}", language).find("class ManagedStore"), std::string::npos);
    EXPECT_EQ(emit("class ordinary { private uint32 value; }", language).find("ManagedStore"), std::string::npos);
    writer::portable_options options;
    options.managed_token_codecs = true;
    const auto tokens = emit(schema, language, options);
    EXPECT_NE(tokens.find("class ManagedReference"), std::string::npos);
    EXPECT_EQ(tokens.find("class ManagedStore"), std::string::npos);
    options.managed_engine = true;
    EXPECT_NE(emit(schema, language, options).find("class ManagedStore"), std::string::npos);
  }
}

// Generated metadata preserves ordinary boundaries and deterministic schema/class/protocol identity.
TEST(javascript_managed_writer, emits_native_codecs_and_typed_occurrence_metadata) {
  constexpr std::string_view schema =
      "serializer version 1.20.0; class leaf managed class_id(701) { public string text; } "
      "class branch managed class_id(702) { public managed array leaf leaves; } "
      "class graph managed class_id(703) { public branch ordinary; public managed array branch branches; "
      "public optional<managed_ref<leaf>> selected; }";
  writer::portable_options options;
  options.managed_engine = true;
  const auto source = emit(schema, "js", options);
  EXPECT_NE(source.find("srlManagedVisitBranch(value.ordinary, visitor, false)"), std::string::npos);
  EXPECT_NE(source.find("srlManagedVisitBranch(occurrence0, visitor, participating)"), std::string::npos);
  EXPECT_NE(source.find(":BINARY_INTEGER:branch=702:graph=703:leaf=701"), std::string::npos);
  EXPECT_NE(source.find(".decode(bytes, Protocol.BINARY_INTEGER)"), std::string::npos);
  EXPECT_NE(source.find("output.compactVarint(this.persistentId, 4)"), std::string::npos);
  EXPECT_EQ(source.find("output.compactVarintLenient(this.persistentId"), std::string::npos);
  EXPECT_NE(source.find("srlManagedVisitGraph(result, null, false)"), std::string::npos);
  const auto declarations = emit(schema, "typescript", options);
  EXPECT_NE(declarations.find("private readonly __srlManagedBrand: void"), std::string::npos);
  EXPECT_NE(declarations.find("ManagedSchema<Graph, number>"), std::string::npos);
  EXPECT_NE(declarations.find("ManagedReference<Leaf, number>"), std::string::npos);
  EXPECT_NE(declarations.find("=> undefined"), std::string::npos);
  EXPECT_EQ(declarations.find("@ts-ignore"), std::string::npos);
  options.managed_id_type = "uint64";
  options.managed_id_encoding = writer::persistent_id_encoding::fixed;
  options.protocols = writer::output_protocols::binary_none;
  const auto wide = emit(schema, "js", options);
  EXPECT_NE(wide.find(":fixed:BINARY_NONE:"), std::string::npos);
  EXPECT_NE(wide.find("output.integer(this.persistentId, 8, true)"), std::string::npos);
  EXPECT_NE(emit(schema, "typescript", options).find("ManagedSchema<Graph, bigint>"), std::string::npos);
}

// Unsupported selected graphs fail explicitly without changing unrelated default generation.
TEST(javascript_managed_writer, rejects_unqualified_profiles_and_reserved_helpers) {
  writer::portable_options options;
  options.managed_engine = true;
  for (const auto language : {"js", "typescript"}) {
    for (const auto schema : {
        "class base {} class root managed : public base {}",
        "class root managed { public transient uint32 value; }",
        "class root managed { private uint32 value; }",
        "class root managed {} class ordinary packed { public uint32 value; }",
        "class ManagedStore {} class root managed {}",
        "namespace ManagedScope { class root managed {} }"}) {
      EXPECT_THROW(emit(schema, language, options), std::invalid_argument) << language << ": " << schema;
    }
    EXPECT_THROW(emit("class root managed { public optional<root> cycle; }", language, options),
                 rohit::serializer::exception::bad_member_type);
    options.managed_separate_values = true;
    EXPECT_THROW(emit("class root managed {}", language, options), std::invalid_argument);
    options.managed_separate_values = false;
    options.xml.enabled = true;
    EXPECT_THROW(emit("class root managed {}", language, options), std::invalid_argument);
    options.xml.enabled = false;
  }
}

// Token-only targets retain strict generated validation even without native state/history helpers.
TEST(javascript_managed_writer, token_validation_uses_exact_native_instances) {
  writer::portable_options options;
  options.managed_token_codecs = true;
  const auto source = emit("serializer version 1.20.0; class item managed class_id(101) {} class envelope { public managed_ref<item> target; }", "js", options);
  EXPECT_NE(source.find("Object.getPrototypeOf(value) !== ManagedReference.prototype"), std::string::npos);
  EXPECT_NE(source.find("Object.freeze(ManagedReference.prototype)"), std::string::npos);
  EXPECT_NE(source.find("srlManagedValidate(reference0, 101n, 4)"), std::string::npos);
  EXPECT_NE(source.find("srlManagedVisitEnvelope(result, null, false)"), std::string::npos);
}
