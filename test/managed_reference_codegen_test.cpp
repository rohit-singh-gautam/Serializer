// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace serializer = rohit::serializer;

// Generate the real owning backend while bypassing an optional external formatter.
std::string generate(std::string_view text) {
  const auto nodes = serializer::parser::parse(rohit::make_constant_stream(text.data(), text.size()), true);
  serializer::writer::cpp_options options{};
  options.format = false;
  return serializer::writer::cpp::generate(nodes, options);
}

// Select just one generated static tag so unrelated schema edits cannot hide identity drift.
std::string class_tag(std::string_view source, std::string_view type) {
  const auto record = source.find("struct model_traits<" + std::string{type} + ">");
  if (record == std::string_view::npos) { throw std::runtime_error{"Generated managed traits missing"}; }
  const auto begin = source.find("class_id = ", record);
  if (begin == std::string_view::npos) { throw std::runtime_error{"Generated managed class tag missing"}; }
  return std::string{source.substr(begin, source.find(';', begin) - begin)};
}
} // namespace

// Class tags survive member growth/reordering and unrelated declarations; override permits renaming.
TEST(managed_reference_codegen, static_tags_ignore_layout_and_source_order) {
  const auto first = generate("serializer version 1.20.0; namespace graph { class item managed { public uint32 x (1); } }");
  const auto second = generate("serializer version 1.20.0; class unrelated managed {} namespace graph { class item managed { public string name (2); public uint32 x (1); } }");
  EXPECT_EQ(class_tag(first, "::graph::item"), class_tag(second, "::graph::item"));
  const auto renamed = generate("serializer version 1.20.0; class renamed managed class_id(123456789) {}");
  EXPECT_EQ(class_tag(renamed, "::renamed"), "class_id = 123456789ULL");
}

// Distinct concrete emitted types must not share discriminators, including generic expansions.
TEST(managed_reference_codegen, explicit_collisions_reject_before_publication) {
  EXPECT_THROW((void)generate("serializer version 1.20.0; class first managed class_id(7) {} class second managed class_id(7) {}"), std::invalid_argument);
  EXPECT_THROW((void)generate("serializer version 1.20.0; class item<T> managed class_id(7) { public T value; } instantiate named = item<uint32>;"), std::invalid_argument);
}

// Phantom target edges require forward names but never recursive owning layout dependencies.
TEST(managed_reference_codegen, reciprocal_tokens_and_opt_in_channel_marker) {
  const auto source = generate("serializer version 1.20.0; class first managed { public optional<managed_ref<second>> next; } class second managed { public optional<managed_ref<first>> previous; }");
  EXPECT_NE(source.find("managed_ref<::second, ::std::uint32_t>"), std::string::npos);
  EXPECT_NE(source.find("serializer_has_entity_references = true"), std::string::npos);
  EXPECT_NE(source.find("visit_references"), std::string::npos);
  const auto ordinary = generate("serializer version 1; class item managed { public uint32 value; }");
  EXPECT_EQ(ordinary.find("serializer_has_entity_references"), std::string::npos);
  EXPECT_EQ(ordinary.find("visit_references"), std::string::npos);
}

// A named managed generic is a distinct record and cannot silently bind its self edge to a pooled type.
TEST(managed_reference_codegen, named_recursive_generic_self_targets_reject_explicitly) {
  EXPECT_THROW((void)generate("serializer version 1.20.0; class item<T> managed { public optional<managed_ref<item<T>>> next; } instantiate named = item<uint32>;"), std::invalid_argument);
  EXPECT_NO_THROW((void)generate("serializer version 1.20.0; class target managed {} class holder<T> managed { public optional<managed_ref<T>> next; } instantiate named = holder<target>;"));
}

// The versioned named binding emits its own exact token target while keeping named class identities distinct.
TEST(managed_reference_codegen, named_recursive_generic_self_targets_bind_to_each_named_record) {
  const auto source = generate("serializer version 1.23.0; class item<T> managed { "
      "public optional<managed_ref<item<T>>> next; } "
      "instantiate first = item<uint32>; instantiate second = item<uint32>;");
  EXPECT_NE(source.find("managed_ref<::first, ::std::uint32_t>"), std::string::npos);
  EXPECT_NE(source.find("managed_ref<::second, ::std::uint32_t>"), std::string::npos);
  EXPECT_NE(class_tag(source, "::first"), class_tag(source, "::second"));
}

// A shared nested record has one binding and cannot be silently retargeted for each named owner.
TEST(managed_reference_codegen, named_recursive_generic_shared_nested_targets_stay_rejected) {
  EXPECT_THROW((void)generate("serializer version 1.23.0; "
      "class wrapper { public optional<managed_ref<item<uint32>>> next; } "
      "class item<T> managed { public wrapper value; } instantiate named = item<uint32>;"),
      std::invalid_argument);
}

// A post-validation runtime reset must never create a default-invalid required edge for publication.
TEST(managed_reference_codegen, required_transient_and_omitted_references_reject) {
  for (const auto field : {"public transient managed_ref<target> edge;",
      "public managed_ref<target> edge omit(binary_none);",
      "public transient array[2] managed_ref<target> edges;"}) {
    EXPECT_THROW((void)generate("serializer version 1.20.0; class target managed {} class owner managed { " +
        std::string{field} + " }"), std::invalid_argument);
  }
  EXPECT_THROW((void)generate("serializer version 1.20.0; class target managed {} class data { public managed_ref<target> edge; } class owner managed { public transient data value; }"), std::invalid_argument);
  EXPECT_NO_THROW((void)generate("serializer version 1.20.0; class target managed {} class owner managed { public transient optional<managed_ref<target>> edge; public transient array managed_ref<target> edges; }"));
}

// Runtime-scoped tokens must reject output profiles that cannot provide their declared contract.
TEST(managed_reference_codegen, runtime_tokens_reject_constant_and_emission_profiles) {
  constexpr std::string_view source = "serializer version 1.20.0; class target managed {} class owner managed { public optional<managed_ref<target>> edge; }";
  const auto nodes = serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()), true);
  serializer::writer::cpp_options options{};
  options.format = false;
  options.constant_evaluation = true;
  EXPECT_THROW((void)serializer::writer::cpp::generate(nodes, options), std::invalid_argument);
  options.emission_only = true;
  options.protocols = serializer::writer::cpp_protocols::binary_none;
  EXPECT_THROW((void)serializer::writer::cpp::generate(nodes, options), std::invalid_argument);
  options = {};
  options.format = false;
  options.protobuf = true;
  options.managed_separate_values = true;
  EXPECT_ANY_THROW((void)serializer::writer::cpp::generate(nodes, options));
  EXPECT_NO_THROW((void)generate("serializer version 1.20.0; class owner { public optional<managed_ref<target>> edge; } class target managed {}"));
}

// Legal recursive value/view graphs terminate, including a token encountered after a cycle edge.
TEST(managed_reference_codegen, recursive_views_and_values_do_not_recurse_during_token_detection) {
  const auto ordinary = generate("serializer version 1; class recursive view readonly { public array recursive children; }");
  EXPECT_EQ(ordinary.find("serializer_has_entity_references"), std::string::npos);
  EXPECT_EQ(ordinary.find("managed_reference.hpp"), std::string::npos);
  const auto mixed = generate("serializer version 1.20.0; class target managed {} "
      "class recursive_view view readonly { public array recursive_view children; } "
      "class recursive_value { public array recursive_value children; public optional<managed_ref<target>> edge; }");
  EXPECT_NE(mixed.find("serializer_has_entity_references = true"), std::string::npos);
  EXPECT_NE(mixed.find("managed_ref<::target, ::std::uint32_t>"), std::string::npos);
}
