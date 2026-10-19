// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/schema_compatibility.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
// Resolve reference fixtures through the public parser, including ordinary type lookup.
auto parse_reference_schema(std::string_view text) {
  const auto input = rohit::make_constant_stream(text.data(), text.size());
  return rohit::serializer::parser::parse(input, true);
}
} // namespace

// References lower to an ordinary address record while retaining typed stable field metadata.
TEST(reference_schema, named_addresses_and_nested_paths) {
  const auto nodes = parse_reference_schema(
      "serializer version 1.18.0; namespace models { "
      "class detail stable_ids { public string label (9); } "
      "class base stable_ids { public detail item (7); } "
      "class owner stable_ids : public base (5) { public int32 count (12); } "
      "reference label_ref = shared(owner.item.label); "
      "class consumer stable_ids { public label_ref d (1); } }");
  const auto& space = static_cast<const rohit::serializer::namespace_node&>(*nodes.front());
  const auto& record = static_cast<const rohit::serializer::class_node&>(*space.statements[3]);
  ASSERT_TRUE(record.reference);
  EXPECT_EQ(record.reference->mode, rohit::serializer::reference_declaration::mode_type::shared);
  EXPECT_EQ(record.reference->owner.get_full_name(), "models::owner");
  EXPECT_EQ(record.reference->value_type.name, "string");
  EXPECT_EQ(record.reference->field_path, (std::vector<std::uint32_t>{5, 7, 9}));
  ASSERT_EQ(record.member_list.size(), 4u);
  EXPECT_EQ(record.member_list[0].name, "document_id");
  EXPECT_EQ(record.member_list[1].name, "owner_id");
  EXPECT_EQ(record.member_list[2].name, "generation");
  EXPECT_EQ(record.member_list[3].name, "field_path");
  for (const auto& field : record.member_list) {
    EXPECT_TRUE(field.explicit_id);
    EXPECT_TRUE(field.fixed_name);
  }
}

// Unsafe targets, older language contracts, invalid modes and unresolved fields fail early.
TEST(reference_schema, rejects_unsupported_or_unstable_targets) {
  constexpr std::string_view invalid[]{
      "serializer version 1.17.0; class a { public int32 b (1); } reference r = borrowed(a.b);",
      "serializer version 1; class a { public int32 b (1); } reference r = shared(a.b);",
      "serializer version 1.18.0; class a { public int32 b (1); } reference r = owned(a.b);",
      "serializer version 1.18.0; class a { public int32 b (1); } reference r = shared(a);",
      "serializer version 1.18.0; class a { public int32 b (1); } reference r = shared(a.missing);",
      "serializer version 1.18.0; class a { private int32 b (1); } reference r = shared(a.b);",
      "serializer version 1.18.0; class a { public int32 b; } reference r = shared(a.b);",
      "serializer version 1.18.0; class a { public transient int32 b (1); } reference r = shared(a.b);",
      "serializer version 1.18.0; class a { public array int32 b (1); } reference r = shared(a.b);",
      "serializer version 1.18.0; reference r = shared(a.b); class a { public int32 b (1); }",
      "serializer version 1.18.0; class a { public int32 b (1); } reference a = shared(a.b);"};
  for (const auto source : invalid) {
    SCOPED_TRACE(source);
    EXPECT_ANY_THROW(parse_reference_schema(source));
  }
}

// The compatibility checker includes reference meaning even when address fields have identical bytes.
TEST(reference_schema, compatibility_tracks_target_and_policy) {
  constexpr std::string_view prefix = "serializer version 1.18.0; "
      "class a { public int32 b (1); public int32 c (2); } ";
  auto before = rohit::serializer::parser::parsed_schema{
      parse_reference_schema(std::string{prefix} + "reference r = shared(a.b);"), {}};
  auto after = rohit::serializer::parser::parsed_schema{
      parse_reference_schema(std::string{prefix} + "reference r = weak(a.c);"), {}};
  for (const auto protocol : {rohit::serializer::compatibility_protocol::binary_none,
           rohit::serializer::compatibility_protocol::binary_integer,
           rohit::serializer::compatibility_protocol::binary_string,
           rohit::serializer::compatibility_protocol::json}) {
    const auto issues = rohit::serializer::check_schema_compatibility(before, after, protocol);
    ASSERT_FALSE(issues.empty());
    EXPECT_NE(issues.front().message.find("Reference target"), std::string::npos);
  }
}

// Every backend rejects optional XML until it can enforce the same typed address contract.
TEST(reference_schema, xml_requires_typed_address_validation) {
  namespace writer = rohit::serializer::writer;
  const auto nodes = parse_reference_schema("serializer version 1.18.0; "
      "class owner { public int32 field (1); } reference edge = shared(owner.field);");
  writer::cpp_options cpp{};
  cpp.format = false;
  cpp.xml.enabled = true;
  EXPECT_THROW(writer::cpp::generate(nodes, cpp), std::invalid_argument);
  writer::java_options java{};
  java.xml.enabled = true;
  EXPECT_THROW(writer::java::generate(nodes, "Schema", java), std::invalid_argument);
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    writer::portable_options options{};
    options.xml.enabled = true;
    EXPECT_THROW(writer::portable::generate(nodes, language, "Schema", options), std::invalid_argument);
  }
}

// Reference helpers share C#'s class member namespace and must fail before publishing source.
TEST(reference_schema, csharp_reference_helpers_do_not_shadow_class_names) {
  namespace writer = rohit::serializer::writer;
  constexpr std::string_view prefix = "serializer version 1.18.0; "
      "class owner { public int32 value (7); } ";
  for (const auto name : {"Bind", "Resolve", "ReadValue", "WithValue", "Unbind", "IsBound",
           "SrlValidateReferenceAddress", "SrlReferencePathMatches", "Encode", "Decode", "Read", "Write"}) {
    SCOPED_TRACE(name);
    const auto nodes = parse_reference_schema(std::string{prefix} + "reference " + name + " = shared(owner.value);");
    EXPECT_THROW(writer::portable::generate(nodes, "csharp", "Schema", {}), std::invalid_argument);
  }
}

// Reserve native WeakRef only for modes that actually rely on the JavaScript builtin.
TEST(reference_schema, javascript_weakref_shadowing_is_mode_specific) {
  namespace writer = rohit::serializer::writer;
  constexpr std::string_view prefix = "serializer version 1.18.0; class WeakRef {} "
      "class owner { public int32 value (7); } ";
  for (const auto language : {"js", "typescript"}) {
    SCOPED_TRACE(language);
    for (const auto mode : {"borrowed", "weak"}) {
      const auto nodes = parse_reference_schema(std::string{prefix} + "reference edge = " + mode + "(owner.value);");
      EXPECT_THROW(writer::portable::generate(nodes, language, "Schema", {}), std::invalid_argument);
    }
    const auto ordinary = parse_reference_schema(prefix);
    EXPECT_NO_THROW(writer::portable::generate(ordinary, language, "Schema", {}));
    const auto shared = parse_reference_schema(std::string{prefix} + "reference edge = shared(owner.value);");
    EXPECT_NO_THROW(writer::portable::generate(shared, language, "Schema", {}));
  }
}

// Preserve existing reserved prefixes and conditionally reserve newly generated scope names.
TEST(reference_schema, native_reference_runtime_name_collisions_reject) {
  namespace writer = rohit::serializer::writer;
  struct collision_case { const char* language; const char* declaration; bool ordinary_valid; };
  constexpr collision_case cases[]{
      {"rust", "class SrlReferenceBinding {}", false},
      {"swift", "class SrlReferenceOwner {}", false},
      {"go", "class EdgeScope {}", true}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.language);
    const std::string source = std::string{"serializer version 1.18.0; "} + test.declaration +
        " class owner { public int32 value (7); } ";
    const auto ordinary = parse_reference_schema(source);
    if (test.ordinary_valid) {
      EXPECT_NO_THROW(writer::portable::generate(ordinary, test.language, "Schema", {}));
    } else {
      EXPECT_THROW(writer::portable::generate(ordinary, test.language, "Schema", {}), std::invalid_argument);
    }
    const auto reference = parse_reference_schema(source + "reference Edge = shared(owner.value);");
    EXPECT_THROW(writer::portable::generate(reference, test.language, "Schema", {}), std::invalid_argument);
  }
  const auto c_reference = parse_reference_schema("serializer version 1.18.0; "
      "class owner { public int32 value (7); } reference edge = shared(owner.value);");
  EXPECT_NO_THROW(writer::portable::generate(c_reference, "c", "Schema", {}));
}

// Inline references retain the field's value shape and add no durable address members.
TEST(reference_schema, inline_values_lower_before_their_consumer) {
  namespace serializer = rohit::serializer;
  for (const auto mode : {"borrowed", "shared", "weak"}) {
    SCOPED_TRACE(mode);
    const auto nodes = parse_reference_schema(std::string{"serializer version 1.19.0; "}
        + "namespace models { class owner { public uint32 b (7) {9}; } "
        + "class consumer stable_ids { public " + mode + "(owner.b) d (3); } }");
    const auto& space = static_cast<const serializer::namespace_node&>(*nodes.front());
    ASSERT_EQ(space.statements.size(), 3u);
    const auto& wrapper = static_cast<const serializer::class_node&>(*space.statements[1]);
    const auto& consumer = static_cast<const serializer::class_node&>(*space.statements[2]);
    ASSERT_TRUE(wrapper.reference);
    EXPECT_TRUE(wrapper.reference->value_projection);
    EXPECT_TRUE(wrapper.reference->field_path.empty());
    EXPECT_EQ(wrapper.reference->owner.get_full_name(), "models::owner");
    EXPECT_EQ(wrapper.reference->value_type.name, "uint32");
    ASSERT_EQ(wrapper.member_list.size(), 1u);
    const auto& payload = wrapper.member_list.front();
    EXPECT_EQ(payload.name, "srl_value");
    EXPECT_EQ(payload.access, serializer::access_type::private_access);
    EXPECT_TRUE(payload.fixed_name);
    EXPECT_TRUE(payload.default_value.empty());
    EXPECT_FALSE(payload.initializer);
    EXPECT_EQ(consumer.member_list.front().id, 3u);
    EXPECT_EQ(consumer.member_list.front().display_name, "d");
    EXPECT_EQ(consumer.member_list.front().type_name_list.front().resolved_node, &wrapper);
    EXPECT_FALSE(consumer.member_list.front().value_reference);
  }
}

// Borrowing a payload_record record leaves its arrays in place and needs no encoded source-slot IDs.
TEST(reference_schema, inline_values_borrow_records_containing_collections) {
  namespace serializer = rohit::serializer;
  const auto nodes = parse_reference_schema("serializer version 1.19.0; "
      "class payload_record { public array float values; public array uint32 codes; } "
      "class base { public payload_record payload; } class owner : public base {} "
      "class export_view { public shared(owner.payload) value (1); }");
  ASSERT_EQ(nodes.size(), 5u);
  const auto& wrapper = static_cast<const serializer::class_node&>(*nodes[3]);
  ASSERT_TRUE(wrapper.reference);
  EXPECT_TRUE(wrapper.reference->value_projection);
  EXPECT_TRUE(wrapper.reference->field_path.empty());
  EXPECT_EQ(wrapper.reference->source_base_paths,
      (std::vector<std::vector<std::size_t>>{{0}}));
  EXPECT_EQ(wrapper.member_list.front().type_name_list.front().name, "payload_record");
  const auto& payload_record = static_cast<const serializer::class_node&>(*nodes.front());
  EXPECT_EQ(payload_record.member_list.front().modifier, serializer::member::modifier_type::array);
}

// Ordinary containers can independently retain the same payload type without retaining each other.
TEST(reference_schema, inline_type_references_select_independent_payloads) {
  namespace serializer = rohit::serializer;
  for (const auto mode : {"borrowed", "shared", "weak"}) {
    SCOPED_TRACE(mode);
    const auto nodes = parse_reference_schema(std::string{"serializer version 1.19.0; "}
        + "class payload { public array float values; public array uint32 codes; } "
        + "class owner { public shared(payload) b (1); } "
        + "class consumer { public " + mode + "(payload) d (1); }");
    ASSERT_EQ(nodes.size(), 5u);
    const auto& payload = static_cast<const serializer::class_node&>(*nodes[0]);
    const auto& owner_binding = static_cast<const serializer::class_node&>(*nodes[1]);
    const auto& consumer_binding = static_cast<const serializer::class_node&>(*nodes[3]);
    for (const auto* binding : {&owner_binding, &consumer_binding}) {
      ASSERT_TRUE(binding->reference);
      EXPECT_TRUE(binding->reference->value_projection);
      EXPECT_TRUE(binding->reference->source_path.empty());
      EXPECT_TRUE(binding->reference->source_base_paths.empty());
      EXPECT_TRUE(binding->reference->field_path.empty());
      EXPECT_EQ(binding->reference->owner.resolved_node, &payload);
      EXPECT_EQ(binding->reference->value_type.resolved_node, &payload);
      ASSERT_EQ(binding->member_list.size(), 1u);
      EXPECT_EQ(binding->member_list.front().type_name_list.front().resolved_node, &payload);
      EXPECT_TRUE(binding->member_list.front().default_value.empty());
      EXPECT_FALSE(binding->member_list.front().initializer);
    }
  }
}

// A named concrete generic alias is an ordinary payload identity for direct references.
TEST(reference_schema, inline_type_references_accept_concrete_aliases) {
  const auto nodes = parse_reference_schema("serializer version 1.19.0; "
      "class record<T> { public T value; } instantiate payload = record<uint32>; "
      "class consumer { public shared(payload) d (1); }");
  bool found{};
  for (const auto& node : nodes) {
    if (node->type != rohit::serializer::object_type::class_type) { continue; }
    const auto& record = static_cast<const rohit::serializer::class_node&>(*node);
    if (!record.reference) { continue; }
    found = true;
    EXPECT_TRUE(record.reference->source_path.empty());
    EXPECT_EQ(record.reference->owner.get_full_name(), "payload");
    EXPECT_EQ(record.reference->value_type.get_full_name(), "payload");
  }
  EXPECT_TRUE(found);
}

// Ownership spellings remain ordinary schema types unless followed by the inline selector.
TEST(reference_schema, inline_values_preserve_existing_ownership_type_names) {
  const auto nodes = parse_reference_schema("serializer version 1; "
      "class borrowed {} class shared {} class weak {} "
      "class holder { public borrowed a; public shared b; public weak c; }");
  ASSERT_EQ(nodes.size(), 4u);
  const auto& holder = static_cast<const rohit::serializer::class_node&>(*nodes.back());
  ASSERT_EQ(holder.member_list.size(), 3u);
  EXPECT_EQ(holder.member_list[0].type_name_list.front().name, "borrowed");
  EXPECT_EQ(holder.member_list[1].type_name_list.front().name, "shared");
  EXPECT_EQ(holder.member_list[2].type_name_list.front().name, "weak");
}

// Unsupported paths, ownership profiles and storage defaults fail before generation.
TEST(reference_schema, inline_values_reject_unsupported_shapes) {
  constexpr std::string_view invalid[]{
      "serializer version 1.18.0; class a { public int32 b; } class c { public shared(a.b) d; }",
      "serializer version 1.18.0; class payload {} class c { public shared(payload) d; }",
      "serializer version 1.19.0; class c { public shared(uint32) d; }",
      "serializer version 1.19.0; class c { public shared(missing) d; }",
      "serializer version 1.19.0; class payload { public int32 value; } class owner { public shared(payload) b; } class c { public shared(owner) d; }",
      "serializer version 1.19.0; class a { private int32 b; } class c { public shared(a.b) d; }",
      "serializer version 1.19.0; class a { public int32 b; } class c { public shared(a.missing) d; }",
      "serializer version 1.19.0; class a { public array int32 b; } class c { public shared(a.b) d; }",
      "serializer version 1.19.0; class a { public map (string) int32 b; } class c { public shared(a.b) d; }",
      "serializer version 1.19.0; class a { public int32 b; } class c { public shared(a.b) d (1) {2}; }",
      "serializer version 1.19.0; class a { public int32 b; } class c packed { public shared(a.b) d; }",
      "serializer version 1.19.0; class a { public int32 b; } class c managed { public shared(a.b) d; }",
      "serializer version 1.19.0; class c { public shared(a.b) d; } class a { public int32 b; }",
      "serializer version 1.19.0; class a { public int32 b; } class c_d_value_reference {} class c { public shared(a.b) d; }",
      "serializer version 1.19.0; class a { public int32 b; } class c { public shared(",
      "serializer version 1.19.0; class a { public int32 b; } class c { public shared(a.",
      "serializer version 1.19.0; class a { public int32 b; } class c { public shared(a.b"};
  for (const auto source : invalid) {
    SCOPED_TRACE(source);
    EXPECT_ANY_THROW(parse_reference_schema(source));
  }
}

// Changing in-memory ownership preserves exactly the existing owning field's wire contract.
TEST(reference_schema, inline_value_ownership_is_wire_transparent) {
  namespace serializer = rohit::serializer;
  constexpr std::string_view prefix = "serializer version 1.19.0; "
      "class owner { public uint32 b (1) {7}; } ";
  auto owning = serializer::parser::parsed_schema{parse_reference_schema(std::string{prefix}
      + "class consumer stable_ids { public uint32 d (3) {7}; }"), {}};
  for (const auto mode : {"borrowed", "shared", "weak"}) {
    auto projected = serializer::parser::parsed_schema{parse_reference_schema(std::string{prefix}
        + "class consumer stable_ids { public " + mode + "(owner.b) d (3); }"), {}};
    for (const auto protocol : {serializer::compatibility_protocol::binary_none,
             serializer::compatibility_protocol::binary_integer,
             serializer::compatibility_protocol::binary_string,
             serializer::compatibility_protocol::json}) {
      EXPECT_TRUE(serializer::check_schema_compatibility(owning, projected, protocol).empty());
      EXPECT_TRUE(serializer::check_schema_compatibility(projected, owning, protocol).empty());
    }
  }
}

// Direct payload ownership emits the same fields as independently stored ordinary records.
TEST(reference_schema, inline_type_ownership_is_wire_transparent) {
  namespace serializer = rohit::serializer;
  constexpr std::string_view prefix = "serializer version 1.19.0; "
      "class payload { public array float values (1); public array uint32 codes (2); } ";
  auto owning = serializer::parser::parsed_schema{parse_reference_schema(std::string{prefix}
      + "class owner { public payload b (1); } class consumer { public payload d (1); }"), {}};
  auto shared = serializer::parser::parsed_schema{parse_reference_schema(std::string{prefix}
      + "class owner { public shared(payload) b (1); } class consumer { public shared(payload) d (1); }"), {}};
  for (const auto protocol : {serializer::compatibility_protocol::binary_none,
           serializer::compatibility_protocol::binary_integer,
           serializer::compatibility_protocol::binary_string,
           serializer::compatibility_protocol::json}) {
    EXPECT_TRUE(serializer::check_schema_compatibility(owning, shared, protocol).empty());
    EXPECT_TRUE(serializer::check_schema_compatibility(shared, owning, protocol).empty());
  }
}

// Callable-only dependencies must reject undeclared Rust lifetimes before publishing source.
TEST(reference_schema, rust_borrowed_records_reject_callable_only_dependencies) {
  namespace serializer = rohit::serializer;
  namespace writer = serializer::writer;
  constexpr std::string_view prefix = "serializer version 1.19.0; "
      "class owner { public uint32 value (1); } "
      "class borrowed_consumer { public borrowed(owner.value) d (1); } ";
  const auto construction_nodes = parse_reference_schema(std::string{prefix} +
      "class receiver { public constructor(borrowed_consumer input) {} }");
  EXPECT_THROW(writer::portable::generate(construction_nodes, "rust", "Schema", {}), std::invalid_argument);

  // Bind a backend target directly so the diagnostic remains independent of copy-policy validation.
  for (const bool translator : {false, true}) {
    auto nodes = parse_reference_schema(std::string{prefix} + "class receiver {}");
    auto& receiver = static_cast<serializer::class_node&>(*nodes.back());
    serializer::conversion_declaration conversion{};
    conversion.name = "as_consumer";
    conversion.translator = translator;
    for (const auto& node : nodes) {
      if (node->name == "borrowed_consumer") {
        conversion.target = serializer::type_name{"borrowed_consumer", nullptr};
        conversion.target.type = serializer::object_type::class_type;
        conversion.target.resolved_node = node.get();
      }
    }
    ASSERT_NE(conversion.target.resolved_node, nullptr);
    receiver.conversions.push_back(std::move(conversion));
    EXPECT_THROW(writer::portable::generate(nodes, "rust", "Schema", {}), std::invalid_argument);
  }
}
