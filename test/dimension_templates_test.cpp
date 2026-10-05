#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>
#include <rohit/schema_compatibility.hpp>
#include "dimensions.hpp"

#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
namespace schema = rohit::serializer;

// Compile fixtures through the public parser, including shared lowering.
auto parse_dimensions(std::string_view source) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  return schema::parser::parse(input);
}

// Return the exact output bytes with no unused stream capacity.
template <template <schema::serialize_type> class Protocol, typename Value>
std::string encode(const Value& value) {
  rohit::full_stream_auto_alloc output{};
  value.template serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Defaults share underlying types; named roots remain independent owning declarations.
TEST(dimension_templates, generated_types) {
  static_assert(std::is_same_v<dimensions::matrix<3>, dimensions::matrix<3, 3, double>>);
  static_assert(std::is_same_v<dimensions::matrix<3, 3>, dimensions::matrix<3>>);
  static_assert(!std::is_same_v<dimensions::matrix_3, dimensions::matrix_3_explicit>);
  static_assert(std::tuple_size_v<decltype(dimensions::frame<7>::basis.elements)> == 49);
  static_assert(std::is_same_v<decltype(dimensions::point<3>::coordinates), std::array<double, 3>>);
  dimensions::sample value{};
  value.position.coordinates = {1.25, 2.5, 3.75};
  value.coordinate_frame.origin.coordinates = {4.5, 5.5, 6.5};
  value.coordinate_frame.basis.elements = {1,2,3,4,5,6,7,8,9};
  value.implicit_square.elements = {9,8,7,6,5,4,3,2,1};
  value.explicit_square.elements = {2,4,6,8,10,12,14,16,18};
  const auto check = [&]<template <schema::serialize_type> class Protocol>() {
    const auto bytes = encode<Protocol>(value);
    const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
    const auto recovered = schema::deserialize_exact<dimensions::sample, Protocol>(input);
    EXPECT_EQ(encode<Protocol>(recovered), bytes);
    EXPECT_EQ(recovered.coordinate_frame.basis.elements, value.coordinate_frame.basis.elements);
  };
  check.template operator()<schema::binary_none>();
  check.template operator()<schema::binary_integer>();
  check.template operator()<schema::binary_string>();
  check.template operator()<schema::json>();
}

// Frozen primitive bytes and ordinary arrays must agree in every native protocol.
TEST(dimension_templates, wire_and_cardinality) {
  dimensions::fixed_bytes fixed{};
  fixed.values = {1, 2, 3};
  dimensions::variable_bytes variable{};
  variable.values = {1, 2, 3};
  EXPECT_EQ(encode<schema::binary_none>(fixed), (std::string{"\x03\x01\x02\x03", 4}));
  EXPECT_EQ(encode<schema::binary_integer>(fixed), (std::string{"\x01\x03\x01\x02\x03\x00", 6}));
  EXPECT_EQ(encode<schema::binary_string>(fixed), (std::string{"\x06values\x03\x01\x02\x03\x00", 12}));
  EXPECT_EQ(encode<schema::json>(fixed), "{\"values\":[1,2,3]}");
  // Reject the largest wire count before attempting to read or allocate elements.
  const std::string huge_count{"\xff\xff\xff\xff", 4};
  const auto huge_input = rohit::make_constant_stream(huge_count.data(), huge_count.size());
  EXPECT_THROW((void)(schema::deserialize_exact<dimensions::fixed_bytes, schema::binary_none>(huge_input)), std::exception);
  const auto check = [&]<template <schema::serialize_type> class Protocol>() {
    EXPECT_EQ(encode<Protocol>(fixed), encode<Protocol>(variable));
    for (const auto size : {0u, 2u, 4u, 100u}) {
      variable.values.resize(size);
      const auto bytes = encode<Protocol>(variable);
      const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
      EXPECT_THROW((void)(schema::deserialize_exact<dimensions::fixed_bytes, Protocol>(input)), std::exception);
    }
    variable.values = {1, 2, 3};
    auto bytes = encode<Protocol>(fixed);
    bytes.pop_back();
    auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
    EXPECT_THROW((void)(schema::deserialize_exact<dimensions::fixed_bytes, Protocol>(input)), std::exception);
    bytes = encode<Protocol>(fixed) + "x";
    input = rohit::make_constant_stream(bytes.data(), bytes.size());
    EXPECT_THROW((void)(schema::deserialize_exact<dimensions::fixed_bytes, Protocol>(input)), std::exception);
  };
  check.template operator()<schema::binary_none>();
  check.template operator()<schema::binary_integer>();
  check.template operator()<schema::binary_string>();
  check.template operator()<schema::json>();
}

// Validate unused defaults and expression errors before any output backend runs.
TEST(dimension_templates, invalid_dimensions_and_defaults) {
  for (const std::string_view source : {
      "class p<uint64 N> { public array[N] double x; } class a { public p<0> value; }",
      "class p<uint64 N> {} class a { public p<-1> value; }",
      "class p<uint64 N> {} class a { public p<18446744073709551616> value; }",
      "class p<uint64 N> {} class a { public p<18446744073709551615+1> value; }",
      "class p<uint64 N> {} class a { public p<18446744073709551615*2> value; }",
      "class p<uint64 N> {} class a { public p<double> value; }",
      "class p<T> {} class a { public p<3> value; }",
      "class p<uint64 N = N> {}",
      "class p<uint64 N = M, uint64 M = 3> {}",
      "class p<uint64 N = 3, T> {}",
      "class p<uint64 N = 0> {} class a { public p<3> value; }",
      "class p<T = N, uint64 N = 3> {}",
      "class p<uint64 N> { public N x; }",
      "class p { public array[0] double x; }",
      "class p { public array[65537] double x; }",
      "class p { public array[2/1] double x; }",
      "class p { public array[2] double x; } class q<T> {} class a { public q<> value; }"}) {
    SCOPED_TRACE(source);
    EXPECT_THROW(parse_dimensions(source), std::exception);
  }
  EXPECT_NO_THROW(parse_dimensions("class p<uint64 N = 3, T = double> { public array[N] T x; } class a { public p<> value; }"));
  EXPECT_NO_THROW(parse_dimensions("class p { public array[65536] uint8 x; }"));
  EXPECT_NO_THROW(parse_dimensions("class p<uint64 N, uint64 M = N+2, T = double, U = T> { public array[N*M] U x; } class a { public p<1+2*3> value; }"));
}

// Canonical identity and generic depth budgets accept the boundary and reject the next value.
TEST(dimension_templates, canonical_resource_boundaries) {
  for (const auto length : {std::size_t{4087}, std::size_t{4088}}) {
    const std::string name(length, 'g');
    const auto source = "class " + name + "<T> { public T value; } instantiate root=" + name + "<uint32>;";
    if (length == 4087) { EXPECT_NO_THROW(parse_dimensions(source)); }
    else { EXPECT_THROW(parse_dimensions(source), std::exception); }
  }
  for (const auto levels : {31u, 32u}) {
    std::string source = "class box<T> { public T value; } instantiate root=";
    for (unsigned index = 0; index < levels; ++index) { source += "box<"; }
    source += "uint32";
    source.append(levels, '>');
    source += ';';
    if (levels == 31) { EXPECT_NO_THROW(parse_dimensions(source)); }
    else { EXPECT_THROW(parse_dimensions(source), std::exception); }
  }
  std::string source = "class tag<uint64 N> {} class root { ";
  for (unsigned index = 1; index <= 1024; ++index) {
    source += "public tag<" + std::to_string(index) + "> item_" + std::to_string(index) + ";";
  }
  EXPECT_NO_THROW(parse_dimensions(source + "}"));
  EXPECT_THROW(parse_dimensions(source + "public tag<1025> extra; }"), std::exception);
}

// Every non-C++ backend explicitly refuses loss of fixed-array cardinality.
TEST(dimension_templates, backend_diagnostics) {
  const auto nodes = parse_dimensions("class p { public array[3] double x; }");
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    SCOPED_TRACE(language);
    EXPECT_THROW(schema::writer::portable::generate(nodes, language, "Schema"), std::invalid_argument);
  }
  rohit::full_stream_auto_alloc java{};
  EXPECT_THROW(schema::writer::java::write(java, nodes, "Schema"), std::invalid_argument);
  schema::writer::cpp_options options{};
  options.protobuf = true;
  EXPECT_THROW(schema::writer::cpp::generate(nodes, options), std::invalid_argument);
}

// Exercise exact and adjacent parser/storage boundaries before allocating generated storage.
TEST(dimension_templates, expression_and_storage_limits) {
  const auto array_schema = [](const std::string& expression) {
    return "class root { public array[" + expression + "] uint8 values; }";
  };
  EXPECT_NO_THROW(parse_dimensions(array_schema(std::string(31, '(') + "1" + std::string(31, ')'))));
  EXPECT_THROW(parse_dimensions(array_schema(std::string(32, '(') + "1" + std::string(32, ')'))), std::exception);
  std::string sum = "(1)";
  for (unsigned index = 1; index < 128; ++index) { sum += "+1"; }
  EXPECT_NO_THROW(parse_dimensions(array_schema(sum)));
  EXPECT_THROW(parse_dimensions(array_schema(sum + "+1")), std::exception);
  EXPECT_NO_THROW(parse_dimensions(array_schema("65535+1")));
  EXPECT_THROW(parse_dimensions(array_schema("65536+1")), std::exception);
  EXPECT_THROW(parse_dimensions(array_schema("(18446744073709551615+1)*0+1")), std::exception);
  EXPECT_THROW(parse_dimensions("class a { public array[65536] uint64 x; } class b { public array[65536] a x; } class c { public array[65536] b x; } class d { public array[65536] c x; }"), std::exception);
}

// Known storage still consumes the decoder's aggregate budgets, and failure never publishes a value.
TEST(dimension_templates, exact_decode_budgets_and_missing_fields) {
  dimensions::fixed_bytes published{};
  published.values = {7, 8, 9};
  const auto check = [&]<template <schema::serialize_type> class Protocol>() {
    const auto bytes = encode<Protocol>(published);
    for (unsigned budget = 0; budget < 5; ++budget) {
      schema::decode_limits limits{};
      if (budget == 0) { limits.max_input_bytes = 1; }
      if (budget == 1) { limits.max_collection_elements = 2; }
      if (budget == 2) { limits.max_allocation_bytes = 2; }
      if (budget == 3) { limits.max_nesting_depth = 0; }
      if (budget == 4) { limits.max_work_units = 1; }
      const auto input = rohit::make_constant_stream(bytes.data(), bytes.size());
      EXPECT_THROW((published = schema::deserialize_exact<dimensions::fixed_bytes, Protocol>(input, limits)), std::exception);
      EXPECT_EQ(published.values, (std::array<std::uint8_t, 3>{7, 8, 9}));
    }
  };
  check.template operator()<schema::binary_none>();
  check.template operator()<schema::binary_integer>();
  check.template operator()<schema::binary_string>();
  check.template operator()<schema::json>();
  const std::string missing = "{}";
  const auto input = rohit::make_constant_stream(missing.data(), missing.size());
  const auto value = schema::deserialize_exact<dimensions::fixed_bytes, schema::json>(input);
  EXPECT_EQ(value.values, (std::array<std::uint8_t, 3>{}));
  for (const std::string invalid : {"{\"values\":[]}", "{\"values\":[1,2,999]}", "{\"values\":[1,2,3,]}"}) {
    const auto malformed = rohit::make_constant_stream(invalid.data(), invalid.size());
    EXPECT_THROW((void)(schema::deserialize_exact<dimensions::fixed_bytes, schema::json>(malformed)), std::exception);
  }
}

// Compare canonical extents and enforce the reader direction when a sequence gains a constraint.
TEST(dimension_templates, compatibility_directions_and_defaults) {
  const auto make = [](std::string_view source) { return schema::parser::parsed_schema{parse_dimensions(source), {}}; };
  const auto fixed = make("class root stable_ids { public array[3] uint8 value (1); }");
  const auto variable = make("class root stable_ids { public array uint8 value (1); }");
  const auto different = make("class root stable_ids { public array[4] uint8 value (1); }");
  const auto before = make("class m<uint64 N, uint64 C=N, T=double> { public array[N*C] T x; } instantiate root=m<3>;");
  const auto same = make("class m<uint64 R, uint64 C=R, T=double> { public array[R*C] T x; } instantiate root=m<3,3,double>;");
  const auto changed = make("class m<uint64 N, uint64 C=4, T=double> { public array[N*C] T x; } instantiate root=m<3>;");
  const auto explicit_use = make("class m<uint64 N, uint64 C=4, T=double> { public array[N*C] T x; } instantiate root=m<3,3,double>;");
  for (const auto protocol : {schema::compatibility_protocol::binary_none, schema::compatibility_protocol::binary_integer,
       schema::compatibility_protocol::binary_string, schema::compatibility_protocol::json}) {
    const auto issues = schema::check_schema_compatibility(fixed, variable, protocol);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_TRUE(issues.front().breaks_old_reader);
    EXPECT_FALSE(issues.front().breaks_new_reader);
    const auto reverse = schema::check_schema_compatibility(variable, fixed, protocol);
    ASSERT_EQ(reverse.size(), 1u);
    EXPECT_FALSE(reverse.front().breaks_old_reader);
    EXPECT_TRUE(reverse.front().breaks_new_reader);
    EXPECT_FALSE(schema::check_schema_compatibility(fixed, different, protocol).empty());
    EXPECT_TRUE(schema::check_schema_compatibility(before, same, protocol).empty());
    EXPECT_TRUE(schema::check_schema_compatibility(before, explicit_use, protocol).empty());
    EXPECT_FALSE(schema::check_schema_compatibility(before, changed, protocol).empty());
  }
}
} // namespace
