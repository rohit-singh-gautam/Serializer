#include <rohit/schema_compatibility.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {
namespace codec = rohit::serializer;
using protocol = codec::compatibility_protocol;

// Resolve in-memory fixtures through the real parser without requiring generated C++ output.
codec::parser::parsed_schema schema(std::string_view text) {
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  return {codec::parser::parse(input), {}};
}

// Search diagnostic meaning without tying assertions to formatting or diagnostic order.
bool reports(const std::vector<codec::compatibility_issue>& issues, std::string_view text) {
  return std::any_of(issues.begin(), issues.end(), [&](const auto& issue) {
    return issue.message.find(text) != std::string::npos;
  });
}

// Identify whether any conservative hazard affects the selected reader direction.
bool breaks_new_reader(const std::vector<codec::compatibility_issue>& issues) {
  return std::any_of(issues.begin(), issues.end(),
                     [](const auto& issue) { return issue.breaks_new_reader; });
}
} // namespace

// Compact encodings change native binary shape while overflow policy and text retain the contract.
TEST(schema_compatibility, compact_encodings_are_native_binary_contracts) {
  const auto fixed = schema("class record stable_ids { public uint32 value (1); }");
  const auto prefix = schema("class record stable_ids { public compact_prefix uint32 value (1); }");
  const auto lenient = schema("class record stable_ids { public compact_prefix lenient uint32 value (1); }");
  const auto varint = schema("class record stable_ids { public compact_varint uint32 value (1); }");
  for (const auto mode : {protocol::binary_none, protocol::binary_integer, protocol::binary_string}) {
    EXPECT_TRUE(reports(codec::check_schema_compatibility(fixed, prefix, mode), "Compact integer encoding"));
    EXPECT_TRUE(reports(codec::check_schema_compatibility(prefix, varint, mode), "Compact integer encoding"));
    EXPECT_TRUE(codec::check_schema_compatibility(prefix, lenient, mode).empty());
  }
  for (const auto mode : {protocol::json, protocol::protobuf_binary}) {
    EXPECT_TRUE(codec::check_schema_compatibility(fixed, prefix, mode).empty());
    EXPECT_TRUE(codec::check_schema_compatibility(prefix, varint, mode).empty());
  }
}

// Fixed magic bytes and exclusions affect only the formats that actually include them.
TEST(schema_compatibility, magic_and_omissions_follow_selected_wire_formats) {
  const auto previous = schema("class record stable_ids { private magic (100) { 'OLD' } "
      "omit(json); public uint32 value (1); public uint32 diagnostic (2) omit(binary_positional); }");
  const auto changed = schema("class record stable_ids { private magic (100) { 'NEW' } "
      "omit(json); public uint32 value (1); public uint32 diagnostic (2) omit(binary_positional); }");
  EXPECT_TRUE(codec::check_schema_compatibility(previous, changed, protocol::json).empty());
  EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, changed, protocol::binary_none),
                      "Magic header"));
  EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, changed, protocol::protobuf_binary),
                      "Magic header"));
  const auto same_wire = schema("class record stable_ids { private magic (100) { 'OLD' } "
      "omit(json); public uint32 value (1); public uint64 diagnostic (2) omit(binary_positional); }");
  EXPECT_TRUE(codec::check_schema_compatibility(previous, same_wire, protocol::binary_none).empty());
  EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, same_wire, protocol::json), "Field type"));
  const auto included = schema("class record stable_ids { private magic (100) { 'OLD' } "
      "omit(json); public uint32 value (1); public uint32 diagnostic (2); }");
  EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, included, protocol::binary_none),
                      "Positional fields"));
}

// Named codecs permit an ordinary field and union alternatives to share a base-name prefix.
TEST(schema_compatibility, named_union_base_names_do_not_confuse_field_matching) {
  constexpr std::string_view text =
      "class record stable_ids { public uint32 plain (\"payload\", 1); "
      "public union(uint32 = number, float = real) choice (\"payload\", 2); }";
  const auto previous = schema(text);
  const auto current = schema(text);
  for (const auto mode : {protocol::json, protocol::binary_string}) {
    EXPECT_TRUE(codec::check_schema_compatibility(previous, current, mode).empty());
  }
}

// C++ member spelling may change when IDs, wire names, types, and schema defaults remain fixed.
TEST(schema_compatibility, preserves_explicit_wire_identity_across_source_renames) {
  const auto previous =
      schema("namespace model { class record stable_ids { public uint32 old (\"value\", 7); } }");
  const auto current =
      schema("namespace model { class record stable_ids { public uint32 fresh (\"value\", 7); } }");
  for (const auto mode : {protocol::binary_none, protocol::binary_integer, protocol::binary_string,
                          protocol::json, protocol::protobuf_binary}) {
    EXPECT_TRUE(codec::check_schema_compatibility(previous, current, mode).empty());
  }
}

// Native keyed additions are backward-readable only; Protobuf binary can skip new fields.
TEST(schema_compatibility, additions_respect_unknown_field_rules) {
  const auto previous = schema("class record stable_ids { public uint32 value (1); }");
  const auto current =
      schema("class record stable_ids { public uint32 value (1); public string label (2); }");
  for (const auto mode : {protocol::binary_integer, protocol::binary_string, protocol::json}) {
    const auto issues = codec::check_schema_compatibility(previous, current, mode);
    ASSERT_EQ(issues.size(), 1U);
    EXPECT_TRUE(issues.front().breaks_old_reader);
    EXPECT_FALSE(issues.front().breaks_new_reader);
  }
  EXPECT_TRUE(
      codec::check_schema_compatibility(previous, current, protocol::protobuf_binary).empty());
  EXPECT_TRUE(breaks_new_reader(
      codec::check_schema_compatibility(previous, current, protocol::binary_none)));
}

// IDs cannot be reassigned to a differently named field even if its scalar width is unchanged.
TEST(schema_compatibility, detects_id_reuse_and_type_shape_changes) {
  const auto previous = schema("class record stable_ids { public uint32 value (1); }");
  const auto reused = schema("class record stable_ids { public uint32 other (1); }");
  EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, reused, protocol::binary_integer),
                      "identity changed"));
  const auto widened = schema("class record stable_ids { public uint64 value (1); }");
  EXPECT_TRUE(
      reports(codec::check_schema_compatibility(previous, widened, protocol::binary_integer),
              "Field type"));
  const auto collection = schema("class record stable_ids { public array uint32 value (1); }");
  EXPECT_TRUE(
      reports(codec::check_schema_compatibility(previous, collection, protocol::binary_integer),
              "container shape"));
}

// Removing a field needs durable retirement history even when Protobuf can skip it.
TEST(schema_compatibility, removed_fields_require_reservations) {
  const auto previous =
      schema("class record stable_ids { public uint32 value (1); public string retired (2); }");
  const auto current = schema("class record stable_ids { public uint32 value (1); }");
  EXPECT_TRUE(
      reports(codec::check_schema_compatibility(previous, current, protocol::protobuf_binary),
              "requires its ID and wire name"));
  const codec::compatibility_policy policy{{{"record", {2}, {"retired"}}}};
  EXPECT_TRUE(
      codec::check_schema_compatibility(previous, current, protocol::protobuf_binary, policy)
          .empty());
  const auto native =
      codec::check_schema_compatibility(previous, current, protocol::binary_integer, policy);
  ASSERT_EQ(native.size(), 1U);
  EXPECT_TRUE(native.front().breaks_new_reader);
  EXPECT_FALSE(native.front().breaks_old_reader);
}

// A policy survives gaps in schema history and covers parent IDs and expanded union keys too.
TEST(schema_compatibility, reservations_reject_reuse_after_intermediate_revisions) {
  const auto previous = schema("class record stable_ids { public uint32 value (1); }");
  const codec::compatibility_policy policy{{{"record", {2}, {"retired", "payload:number"}}}};
  for (const auto body : {"public uint32 other (2);", "public string retired (3);",
                          "public union(uint32 = number, float = real) payload (3);"}) {
    const auto current =
        schema(std::string{"class record stable_ids { public uint32 value (1); "} + body + " }");
    EXPECT_TRUE(reports(
        codec::check_schema_compatibility(previous, current, protocol::protobuf_binary, policy),
        "reserved ID or wire name"));
  }
  const auto with_parent = schema(
      "class base {} class record stable_ids : public base (2) { public uint32 value (1); }");
  EXPECT_TRUE(reports(
      codec::check_schema_compatibility(previous, with_parent, protocol::binary_integer, policy),
      "reserved ID or wire name"));
}

// Positional order matters even with stable IDs; native keyed decoding dispatches by identity.
TEST(schema_compatibility, detects_positional_reordering) {
  const auto previous =
      schema("class record stable_ids { public uint32 first (1); public string second (2); }");
  const auto current =
      schema("class record stable_ids { public string second (2); public uint32 first (1); }");
  EXPECT_TRUE(
      codec::check_schema_compatibility(previous, current, protocol::binary_integer).empty());
  EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, current, protocol::binary_none),
                      "reordered"));
}

// Enum ordinals are explicit compatibility obligations even though schema syntax assigns them implicitly.
TEST(schema_compatibility, detects_enum_ordinal_reordering_removal_and_append) {
  const auto previous = schema("enum state { idle, active }");
  const auto reordered = schema("enum state { active, idle }");
  EXPECT_TRUE(
      reports(codec::check_schema_compatibility(previous, reordered, protocol::binary_integer),
              "ordinal changed"));
  const auto removed = schema("enum state { idle }");
  EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, removed, protocol::json),
                      "retain retired ordinal slots"));
  const auto appended = schema("enum state { idle, active, done }");
  const auto changes =
      codec::check_schema_compatibility(previous, appended, protocol::protobuf_binary);
  ASSERT_EQ(changes.size(), 1U);
  EXPECT_TRUE(changes.front().breaks_old_reader);
  EXPECT_FALSE(changes.front().breaks_new_reader);
}

// Union indices and payload types must remain stable independently of the enclosing field ID.
TEST(schema_compatibility, detects_union_reordering_and_payload_changes) {
  const auto previous =
      schema("class record { public union(uint32 = number, float = real) payload; }");
  const auto reordered =
      schema("class record { public union(float = real, uint32 = number) payload; }");
  EXPECT_TRUE(
      reports(codec::check_schema_compatibility(previous, reordered, protocol::binary_integer),
              "Union ordinal"));
  const auto changed =
      schema("class record { public union(uint64 = number, float = real) payload; }");
  EXPECT_TRUE(
      reports(codec::check_schema_compatibility(previous, changed, protocol::protobuf_binary),
              "Union ordinal"));
}

// Defaults, map keys, and nested declarations can change meaning without altering the root field ID.
TEST(schema_compatibility, checks_defaults_maps_and_nested_types) {
  const auto previous =
      schema("namespace n { class child { public uint32 code; } class record { public child "
             "nested; public map(uint32) string values; public uint32 version { 1 }; } }");
  const auto current =
      schema("namespace n { class child { public uint64 code; } class record { public child "
             "nested; public map(uint64) string values; public uint32 version { 2 }; } }");
  const auto changes =
      codec::check_schema_compatibility(previous, current, protocol::binary_integer);
  EXPECT_TRUE(reports(changes, "default changed"));
  EXPECT_GE(std::count_if(changes.begin(), changes.end(),
                          [](const auto& item) {
                            return item.message.find("Field type") != std::string::npos;
                          }),
            2);
}

// Unsupported protocols and malformed in-memory policies fail instead of silently weakening checks.
TEST(schema_compatibility, rejects_invalid_configuration_and_protobuf_mapping) {
  EXPECT_THROW(codec::parse_compatibility_protocol("protojson"), std::invalid_argument);
  const auto value = schema("class record { public uint32 value; }");
  const codec::compatibility_policy duplicate{{{"record", {1, 1}, {}}}};
  EXPECT_THROW(codec::check_schema_compatibility(value, value, protocol::json, duplicate),
               std::invalid_argument);
  const auto invalid = schema("class record { public uint32 value (19000); }");
  EXPECT_THROW(codec::check_schema_compatibility(invalid, invalid, protocol::protobuf_binary),
               std::invalid_argument);
}

// Supported historical positional contracts survive additions gated to a later revision.
TEST(schema_compatibility, compares_versioned_historical_layouts) {
  const auto previous =
      schema("class record stable_ids { public version { 8 }; public uint64 id (2); }");
  const auto current =
      schema("class record stable_ids { public version { 10 } compatibility { 8 }; "
             "public uint64 id (2); created(9) public bool enabled (3) { true }; }");
  for (const auto mode : {protocol::binary_none, protocol::binary_integer, protocol::json}) {
    const auto changes = codec::check_schema_compatibility(previous, current, mode);
    EXPECT_FALSE(breaks_new_reader(changes));
    EXPECT_TRUE(reports(changes, "previous supported range"));
  }
  const auto damaged =
      schema("class record stable_ids { public version { 10 } compatibility { 8 }; "
             "public uint32 id (2); }");
  EXPECT_TRUE(breaks_new_reader(
      codec::check_schema_compatibility(previous, damaged, protocol::binary_none)));
}

// Embedded reservations retire removed field identities without an external JSON policy.
TEST(schema_compatibility, recognizes_schema_reservations) {
  const auto previous = schema("class record { public uint32 value (2); }");
  const auto current = schema("class record { reserve id { 2 } display { \"value\" }; }");
  const auto changes =
      codec::check_schema_compatibility(previous, current, protocol::protobuf_binary);
  EXPECT_FALSE(reports(changes, "requires its ID"));
}

// Typed header values and types are durable contracts; canonical hex spelling is equivalent.
TEST(schema_compatibility, typed_magic_tracks_type_value_identity_and_omissions) {
  const auto original = schema("class record { public magic uint32 (99) { 42 }; }");
  const auto same = schema("class record { public magic uint32 (99) { 0x2a }; }");
  const auto changed_value = schema("class record { public magic uint32 (99) { 43 }; }");
  const auto changed_type = schema("class record { public magic uint64 (99) { 42 }; }");
  const auto changed_id = schema("class record { public magic uint32 (98) { 42 }; }");
  for (const auto mode : {protocol::binary_none, protocol::binary_integer, protocol::binary_string,
                          protocol::json, protocol::protobuf_binary}) {
    EXPECT_TRUE(codec::check_schema_compatibility(original, same, mode).empty());
    EXPECT_TRUE(reports(codec::check_schema_compatibility(original, changed_value, mode), "Magic header"));
    EXPECT_TRUE(reports(codec::check_schema_compatibility(original, changed_type, mode), "Magic header"));
  }
  EXPECT_TRUE(codec::check_schema_compatibility(original, changed_id, protocol::json).empty());
  EXPECT_TRUE(reports(codec::check_schema_compatibility(original, changed_id, protocol::protobuf_binary), "Magic header"));
  const auto excluded = schema("class record { public magic uint32 { 42 } omit(json); }");
  EXPECT_TRUE(codec::check_schema_compatibility(excluded, schema("class record {}"), protocol::json).empty());
}

// Inferring an existing fixed cardinality preserves defaults and every native wire contract.
TEST(schema_compatibility, explicit_and_inferred_fixed_arrays_preserve_literal_defaults) {
  const auto previous = schema("serializer version 1.0.0; class record stable_ids { "
      "public array[2] uint32 values (1) {1,2}; }");
  const auto current = schema("serializer version 1.1.0; class record stable_ids { "
      "public array[] uint32 values (1) { 1, 2 }; }");
  for (const auto mode : {protocol::binary_none, protocol::binary_integer,
                          protocol::binary_string, protocol::json}) {
    EXPECT_TRUE(codec::check_schema_compatibility(previous, current, mode).empty());
    EXPECT_TRUE(codec::check_schema_compatibility(current, previous, mode).empty());
  }
  const auto& before = static_cast<const codec::class_node&>(*previous.statements.back());
  const auto& after = static_cast<const codec::class_node&>(*current.statements.back());
  EXPECT_EQ(before.member_list.front().default_value, "1,2");
  EXPECT_EQ(after.member_list.front().default_value, "1, 2");

  const auto different_extent = schema("class record stable_ids { "
      "public array[] uint32 values (1) { 1, 2, 3 }; }");
  const auto different_default = schema("class record stable_ids { "
      "public array[] uint32 values (1) { 1, 3 }; }");
  for (const auto mode : {protocol::binary_none, protocol::binary_integer,
                          protocol::binary_string, protocol::json}) {
    EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, different_extent, mode),
                        "Fixed array cardinality"));
    EXPECT_TRUE(reports(codec::check_schema_compatibility(previous, different_default, mode),
                        "Schema default changed"));
  }
}

// Safe literal normalization covers declaration-qualified enums, char bytes, numeric precision, and text.
TEST(schema_compatibility, fixed_array_defaults_compare_safe_literals_by_value) {
  for (const auto& [type, before, after] :
       std::vector<std::tuple<std::string, std::string, std::string>>{
           {"char", "'A','\\0'", "65, 0"},
           {"char", "'\\xFF','A'", "255, 65"},
           {"uint64", "0xFF,0x10", "255, 16"},
           {"float", "1.0,0.50", "1, 5e-1"},
           {"float", "1'000,2", "1000, 2"},
           {"string", "\"a,b\",\"a b\"", "\"a,b\", \"a b\""},
           {"kind", "kind::first,kind::second", "first, second"}}) {
    SCOPED_TRACE(type);
    const std::string prefix = "namespace model { enum kind { first, second } class record stable_ids { ";
    const auto previous = schema(prefix + "public array[2] " + type + " values (1) {" + before + "}; } }");
    const auto current = schema(prefix + "public array[] " + type + " values (1) {" + after + "}; } }");
    for (const auto mode : {protocol::binary_none, protocol::binary_integer,
                            protocol::binary_string, protocol::json}) {
      EXPECT_TRUE(codec::check_schema_compatibility(previous, current, mode).empty());
    }
  }
}

// Quoted whitespace and unevaluated host expressions retain conservative default diagnostics.
TEST(schema_compatibility, fixed_array_default_normalization_preserves_significant_source) {
  const auto spaced = schema("class record stable_ids { public array[] string values (1) { \"a b\" }; }");
  const auto joined = schema("class record stable_ids { public array[] string values (1) { \"ab\" }; }");
  const auto expression = schema("class record stable_ids { public array[] uint32 values (1) { 1+2, 4 }; }");
  const auto evaluated = schema("class record stable_ids { public array[] uint32 values (1) { 3, 4 }; }");
  const auto integer_zero = schema("class record stable_ids { public array[] float values (1) { -0 }; }");
  const auto floating_zero = schema("class record stable_ids { public array[] float values (1) { -0.0 }; }");
  const auto unsigned_negation = schema("class record stable_ids { public array[] float values (1) { -0xFFFFFFFF }; }");
  const auto signed_negation = schema("class record stable_ids { public array[] float values (1) { -4294967295 }; }");
  for (const auto mode : {protocol::binary_none, protocol::binary_integer,
                          protocol::binary_string, protocol::json}) {
    EXPECT_TRUE(reports(codec::check_schema_compatibility(spaced, joined, mode), "Schema default changed"));
    EXPECT_TRUE(reports(codec::check_schema_compatibility(expression, evaluated, mode), "Schema default changed"));
    EXPECT_TRUE(reports(codec::check_schema_compatibility(integer_zero, floating_zero, mode), "Schema default changed"));
    EXPECT_TRUE(reports(codec::check_schema_compatibility(unsigned_negation, signed_negation, mode), "Schema default changed"));
  }
}
