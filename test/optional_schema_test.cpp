// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/schema_compatibility.hpp>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace schema = rohit::serializer;

// Parse the actual language header and retain fully lowered type identities.
schema::parser::parsed_schema parse(std::string_view source) {
  const auto input = rohit::make_constant_full_stream(source.data(), source.size());
  return {schema::parser::parse(input, true), {}};
}

// Find an ordinary root among dependency-ordered generic specializations.
const schema::class_node& record(const schema::parser::parsed_schema& parsed,
                                 std::string_view name) {
  const auto found = std::find_if(parsed.statements.begin(), parsed.statements.end(),
      [&](const auto& node) { return node->name == name; });
  if (found == parsed.statements.end()) { throw std::logic_error{"Missing record"}; }
  return static_cast<const schema::class_node&>(**found);
}
}

// Optional arguments participate in normal generic substitution and preserve every layer.
TEST(optional_schema, nested_types_and_substitution) {
  const auto parsed = parse("serializer version 1.7.0; "
      "class result<T> { public optional<T> value; } "
      "class root { public optional<optional<uint32>> value; "
      "public optional<array<string>> sequence; public optional<map<string,uint32>> lookup; "
      "public array optional<bool> flags; public result<optional<uint32>> generic; }");
  const auto& root = record(parsed, "root");
  const auto& value = root.member_list[0].type_name_list.front();
  ASSERT_TRUE(value.is_optional());
  ASSERT_TRUE(value.arguments.front().is_optional());
  EXPECT_EQ(value.arguments.front().arguments.front().name, "uint32");
  EXPECT_TRUE(root.member_list[1].type_name_list.front().arguments.front().is_sequence());
  EXPECT_TRUE(root.member_list[2].type_name_list.front().arguments.front().is_associative());
  EXPECT_EQ(root.member_list[3].modifier, schema::member::modifier_type::array);
  EXPECT_TRUE(root.member_list[3].type_name_list.front().is_optional());
  const auto& generic = *static_cast<const schema::class_node*>(
      root.member_list[4].type_name_list.front().resolved_node);
  EXPECT_EQ(generic.member_list.front().type_name_list.front().get_full_name(),
            "optional<optional<uint32>>");
}

// New syntax is gated per declaring contract; legacy user templates keep precedence.
TEST(optional_schema, language_gates_and_legacy_names) {
  for (const std::string_view version : {"1", "1.0.0", "1.6.0", "1.6.999"}) {
    const auto header = "serializer version " + std::string{version} + "; ";
    EXPECT_THROW(parse(header + "class root { public optional<uint32> value; }"),
                 rohit::exception::base_parser);
    const auto legacy = parse(header + "class optional<T> { public T value; } "
                             "class root { public optional<uint32> value; }");
    EXPECT_FALSE(record(legacy, "root").member_list.front().type_name_list.front().is_optional());
  }
}

// Reject impossible storage/default/key shapes before generating any output.
TEST(optional_schema, rejects_invalid_contracts) {
  for (const std::string_view declaration : {
      "class root { public optional<> value; }",
      "class root { public optional<uint32,string> value; }",
      "class root { public optional<4> value; }",
      "class root { public optional<missing> value; }",
      "class root { public optional<uint32> value {1}; }",
      "class root view { public optional<uint32> value; }",
      "class root packed { public optional<uint32> value; }",
      "class root { public optional<root> value; }",
      "class root { public optional<map<optional<uint32>,string>> value; }",
      "class root { public optional<map<double,string>> value; }"}) {
    SCOPED_TRACE(declaration);
    EXPECT_THROW(parse("serializer version 1.7.0; " + std::string{declaration}),
                 rohit::exception::base_parser);
  }
}

// Recursive presence and payload identities are incompatible changes in every native format.
TEST(optional_schema, compatibility_includes_payload_and_presence) {
  const auto baseline = parse("serializer version 1.7.0; "
      "class root stable_ids { public optional<map<string,uint32>> value (1); }");
  const auto identical = parse("serializer version 1.7.0; "
      "class root stable_ids { public optional<map<string,uint32>> value (1) {}; }");
  for (const auto protocol : {schema::compatibility_protocol::binary_none,
      schema::compatibility_protocol::binary_integer, schema::compatibility_protocol::binary_string,
      schema::compatibility_protocol::json}) {
    EXPECT_TRUE(schema::check_schema_compatibility(baseline, identical, protocol).empty());
    for (const std::string_view type : {"uint32", "optional<map<string,uint64>>",
                                       "optional<optional<map<string,uint32>>>"}) {
      const auto changed = parse("serializer version 1.7.0; class root stable_ids { public " +
          std::string{type} + " value (1); }");
      EXPECT_FALSE(schema::check_schema_compatibility(baseline, changed, protocol).empty());
    }
  }
}

// Direct Protobuf requests fail explicitly rather than emitting a lossy presence mapping.
TEST(optional_schema, protobuf_rejects_optional) {
  const auto parsed = parse("serializer version 1.7.0; "
      "class root { public optional<uint32> value; }");
  schema::writer::cpp_options options{};
  options.format = false;
  options.protobuf = true;
  EXPECT_THROW(schema::writer::cpp::generate(parsed.statements, options), std::invalid_argument);
}

// A later declaration must not capture an already bound built-in in a generic body.
TEST(optional_schema, retains_lexically_bound_builtin) {
  const auto parsed = parse("serializer version 1.7.0; "
      "class holder<T> { public optional<T> value; } "
      "class optional<T> { public T ordinary; } "
      "instantiate root = holder<uint32>;");
  EXPECT_TRUE(record(parsed, "root").member_list.front().type_name_list.front().is_optional());
}

// Managed variants require directly identified records rather than wrapped alternatives.
TEST(optional_schema, managed_variant_rejects_wrapped_alternatives) {
  EXPECT_THROW(parse("serializer version 1.7.0; class child managed { public uint32 code; } "
      "class root managed { public managed variant(optional<child> = wrapped, child = plain) value; }"),
      rohit::exception::base_parser);
}
