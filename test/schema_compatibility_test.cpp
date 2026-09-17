#include <rohit/schema_compatibility.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
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
