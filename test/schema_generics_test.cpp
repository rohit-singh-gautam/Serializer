#include <gtest/gtest.h>
#include <rohit/schema_compatibility.hpp>
#include <rohit/serializer_creator.hpp>

#include "generics.hpp"

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
namespace schema = rohit::serializer;

// Parse each fixture through the same lowering and validation pipeline used by the CLI.
auto parse_generic(std::string_view source) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  return schema::parser::parse(input);
}

// Locate a concrete declaration without depending on lowered namespace block grouping.
const schema::class_node* find_class(const std::vector<std::unique_ptr<schema::syntax_node>>& nodes,
                                     std::string_view name) {
  for (const auto& node : nodes) {
    if (node->type == schema::object_type::namespace_type) {
      if (auto* found =
              find_class(static_cast<const schema::namespace_node&>(*node).statements, name)) {
        return found;
      }
    } else if (node->type == schema::object_type::class_type && node->get_full_name() == name) {
      return static_cast<const schema::class_node*>(node.get());
    }
  }
  return nullptr;
}

// Template aliases name exactly the concrete types used by generated fields and codec entry points.
TEST(schema_generics, cpp_aliases_and_wire_equivalence) {
  static_assert(std::is_same_v<generic_demo::result<generic_demo::person>,
                               decltype(generic_demo::response::person_result)>);
  static_assert(std::is_same_v<generic_demo::batch<generic_demo::result<std::uint32_t>>,
                               decltype(generic_demo::response::history)>);
  generic_demo::response value{};
  value.person_result.success = true;
  value.person_result.value.name = "generic";
  value.person_result.value.age = 42;
  value.count_result.value = 27;
  value.history.values.push_back(value.count_result);
  const auto verify = [&]<template <schema::serialize_type> class Protocol>() {
    rohit::full_stream_auto_alloc encoded{};
    value.serialize_out<Protocol>(encoded);
    auto input = rohit::make_constant_stream(encoded.begin(), encoded.current_offset());
    const auto concrete =
        schema::deserialize_exact<generic_demo::equivalent_response, Protocol>(input);
    EXPECT_EQ(concrete.person_result.value.name, "generic");
    EXPECT_EQ(concrete.history.values.at(0).value, 27u);
    rohit::full_stream_auto_alloc reproduced{};
    concrete.serialize_out<Protocol>(reproduced);
    EXPECT_EQ(
        std::string_view(reinterpret_cast<const char*>(encoded.begin()), encoded.current_offset()),
        std::string_view(reinterpret_cast<const char*>(reproduced.begin()),
                         reproduced.current_offset()));
  };
  verify.template operator()<schema::json>();
  verify.template operator()<schema::binary_none>();
  verify.template operator()<schema::binary_integer>();
  verify.template operator()<schema::binary_string>();
}

// Multiple arguments, nested applications and map parameters all resolve to concrete codec types.
TEST(schema_generics, substitutions_and_deduplication) {
  const auto nodes = parse_generic(
      "namespace lib { class pair<K,V> stable_ids { public K first (1); public array V rest (2); } "
      "class table<K,V> { public map(K) V values; } } "
      "class use { public lib::pair<uint32, lib::pair<string,int32>> a; "
      "public lib::pair<uint32,lib::pair<string,int32>> b; public lib::table<uint32,string> c; }");
  const auto* use = find_class(nodes, "use");
  ASSERT_NE(use, nullptr);
  EXPECT_EQ(use->member_list[0].type_name_list[0].resolved_node,
            use->member_list[1].type_name_list[0].resolved_node);
  const auto* table =
      static_cast<const schema::class_node*>(use->member_list[2].type_name_list[0].resolved_node);
  EXPECT_EQ(table->member_list[0].key, "uint32");
  EXPECT_EQ(table->member_list[0].type_name_list[0].name, "string");
  for (const auto language :
       {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    SCOPED_TRACE(language);
    EXPECT_FALSE(schema::writer::portable::generate(nodes, language, "Schema").empty());
  }
  rohit::full_stream_auto_alloc java{};
  EXPECT_NO_THROW(schema::writer::java::write(java, nodes, "Schema"));
}

// Reject malformed, open, recursive and unsupported generic shapes during schema compilation.
TEST(schema_generics, rejects_invalid_schemas) {
  for (const std::string_view source :
       {"class box<> {}",
        "class box<T,T> {}",
        "class box<uint32> {}",
        "class box<T> { public missing value; }",
        "class box<T> { public T<uint32> value; }",
        "class box<T> {} class use { public box value; }",
        "class box<T> {} class use { public box<uint32,string> value; }",
        "class use { public uint32<string> value; }",
        "class box<T> {} class use { public box<missing> value; }",
        "class box<T> {} class use { public box<uint32 value; }",
        "class box<T> {} class use { public box<uint32,> value; }",
        "class box<T> { public box<T> value; } instantiate root = box<uint32>;",
        "class box<T> { public box<box<T>> value; } instantiate root = box<uint32>;",
        "class box<T> view {}",
        "class box<T> managed {}",
        "class box<T> packed {}",
        "class base {} class box<T> : public base {}",
        "class box<T> { public union(T,uint32) value; }",
        "class box<T> { public T value; } class use { public box<use> value; }",
        "class box<T> {} instantiate root = uint32;"}) {
    SCOPED_TRACE(source);
    EXPECT_THROW(parse_generic(source), std::exception);
  }
}

// A later nested namespace cannot capture qualified map keys frozen in a generic definition.
TEST(schema_generics, map_keys_keep_definition_scope) {
  const auto nodes = parse_generic(
      "namespace lib { enum state { ready } class table<T> { public map(state) T values; } "
      "namespace lib { enum state { different } } instantiate root = table<string>; }");
  const auto* root = find_class(nodes, "lib::root");
  ASSERT_NE(root, nullptr);
  ASSERT_NE(root->member_list.front().key_node, nullptr);
  EXPECT_EQ(root->member_list.front().key_node->get_full_name(), "lib::state");
}

// Limits apply before recursive parse/expansion can exhaust the process stack.
TEST(schema_generics, bounded_nesting) {
  std::string source = "class box<T> { public T value; } instantiate root = ";
  for (unsigned depth = 0; depth < 40; ++depth) {
    source += "box<";
  }
  source += "uint32";
  source.append(40, '>');
  source += ';';
  EXPECT_THROW(parse_generic(source), std::exception);
}

// Bound total expansion independently of depth, including roots that are never used as fields.
TEST(schema_generics, bounded_instance_count) {
  std::string source = "class box<T> { public T value; } ";
  constexpr unsigned rejected_instance_count = 1025;
  for (unsigned index = 0; index < rejected_instance_count; ++index) {
    const auto suffix = std::to_string(index);
    source +=
        "class tag_" + suffix + " {} instantiate root_" + suffix + " = box<tag_" + suffix + ">; ";
  }
  EXPECT_THROW(parse_generic(source), std::exception);
}

// Generic helper identifiers obey the same keyword/collision checks as ordinary generated classes.
TEST(schema_generics, cpp_alias_validation) {
  schema::writer::cpp_options options{};
  options.format = false;
  for (const auto source :
       {"class box<T> {} class box_serializer_binding {} instantiate root = box<uint32>;",
        "class template<T> {} instantiate root = template<uint32>;"}) {
    const auto nodes = parse_generic(source);
    EXPECT_THROW(schema::writer::cpp::generate(nodes, options), std::invalid_argument);
  }
  EXPECT_THROW(parse_generic("class box<array> {}"), std::exception);
  const auto unused = parse_generic("class box<T> { public T value; }");
  EXPECT_TRUE(unused.empty());
}

// Concrete instantiated contracts participate in the existing compatibility checker.
TEST(schema_generics, compatibility_checks_substituted_fields) {
  schema::parser::parsed_schema before{
      parse_generic(
          "class box<T> stable_ids { public T value (1); } instantiate root = box<uint32>;"),
      {}};
  schema::parser::parsed_schema same{parse_generic("class box<Value> stable_ids { public Value "
                                                   "value (1); } instantiate root = box<uint32>;"),
                                     {}};
  schema::parser::parsed_schema changed{
      parse_generic(
          "class box<T> stable_ids { public T value (1); } instantiate root = box<string>;"),
      {}};
  EXPECT_TRUE(schema::check_schema_compatibility(before, same,
                                                 schema::compatibility_protocol::binary_integer)
                  .empty());
  EXPECT_FALSE(schema::check_schema_compatibility(before, changed,
                                                  schema::compatibility_protocol::binary_integer)
                   .empty());
}
} // namespace
