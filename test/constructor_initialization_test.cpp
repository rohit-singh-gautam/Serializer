// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "constructor_initialization.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
bool reject_external_default{};
}

namespace constructor_initialization {
// Supply a failing host default constructor to verify omitted schema arguments propagate errors.
external_area::external_area() {
  if (reject_external_default) {
    throw std::runtime_error{"External area default construction failed"};
  }
}
} // namespace constructor_initialization

namespace {
// Qualify input ownership without granting default construction or either assignment operation.
struct immutable_input {
  int value;
  immutable_input() = delete;
  // Author one literal input; generated bindings must initialize their member from this value.
  constexpr explicit immutable_input(int input) : value{input} {}
  immutable_input(const immutable_input&) = default;
  immutable_input& operator=(const immutable_input&) = delete;
  immutable_input& operator=(immutable_input&&) = delete;
};

// An explicitly deleted move overload must not prevent the permitted copy construction.
struct copy_only_input {
  int value;
  copy_only_input() = delete;
  // Supply a copy-only coordinate value with no assignment/default-construction requirement.
  constexpr explicit copy_only_input(int input) : value{input} {}
  copy_only_input(const copy_only_input&) = default;
  copy_only_input(copy_only_input&&) = delete;
  copy_only_input& operator=(const copy_only_input&) = delete;
  copy_only_input& operator=(copy_only_input&&) = delete;
};

// Preserve one generated instance's exact default/partial decode contract.
template <typename Value>
Value round_trip(const Value& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<rohit::serializer::binary_integer>(output);
  const auto input = rohit::make_constant_stream(output.begin(), output.current_offset());
  return rohit::serializer::deserialize_exact<Value, rohit::serializer::binary_integer>(input);
}

// Compile schema snippets with explicit language contracts for diagnostic and output qualification.
auto parse(std::string_view source) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}
} // namespace

TEST(constructor_initialization, binds_nondefault_nonassignable_copyable_inputs_directly) {
  using value_type = constructor_initialization::identity<immutable_input>;
  static_assert(!std::is_default_constructible_v<immutable_input>);
  static_assert(!std::is_copy_assignable_v<immutable_input>);
  constexpr immutable_input input{7};
  constexpr value_type direct{input};
  static_assert(direct.value.value == 7);
  static_assert(value_type::create(input).value.value == 7);
  static_assert(value_type::from_value(input).value.value == 7);
  using custom_vector = constructor_initialization::vector3<immutable_input>;
  constexpr custom_vector neutral{};
  static_assert(neutral.x.value == 0 && neutral.y.value == 0 && neutral.z.value == 1);
  constexpr custom_vector coordinates{immutable_input{2}, immutable_input{3}, immutable_input{4}};
  static_assert(coordinates.x.value == 2 && coordinates.y.value == 3 && coordinates.z.value == 4);
  static_assert(custom_vector::create().z.value == 1);
  using copy_only_identity = constructor_initialization::identity<copy_only_input>;
  static_assert(std::is_copy_constructible_v<copy_only_input>);
  static_assert(!std::is_move_constructible_v<copy_only_input>);
  constexpr copy_only_input copy_input{11};
  constexpr copy_only_identity copied{copy_input};
  static_assert(copied.value.value == 11);
  static_assert(copy_only_identity::create(copy_input).value.value == 11);
  static_assert(copy_only_identity::from_value(copy_input).value.value == 11);
  using copy_only_vector = constructor_initialization::vector3<copy_only_input>;
  static_assert(copy_only_vector::create_default_1(copy_input).x.value == 11);
  using collision_identity = constructor_initialization::collision_identity<copy_only_input>;
  static_assert(collision_identity::from_value(copy_input).value.value == 11);
  using collision_factories = constructor_initialization::collision_factories<copy_only_input>;
  static_assert(collision_factories::a(copy_input).value.value == 11);
  static_assert(collision_factories::a_1(copy_input).value.value == 11);
  EXPECT_EQ(direct.value.value, 7);
}

TEST(constructor_initialization, generic_scalar_and_fixed_array_defaults_preserve_custom_coordinates) {
  constexpr constructor_initialization::inferred_bytes inferred{};
  static_assert(inferred.text[0] == 'A' && inferred.text[1] == 'B');
  EXPECT_EQ(round_trip(inferred).text, inferred.text);
  using vector = constructor_initialization::vector3<double>;
  constexpr vector neutral{};
  static_assert(neutral.x == 0 && neutral.y == 0 && neutral.z == 1);
  static_assert(vector::create().z == 1);
  constexpr vector custom{2.0, 3.0, 4.0};
  static_assert(custom.x == 2 && custom.y == 3 && custom.z == 4);
  const auto decoded = round_trip(custom);
  EXPECT_EQ(decoded.x, 2);
  EXPECT_EQ(decoded.y, 3);
  EXPECT_EQ(decoded.z, 4);
  using direction = constructor_initialization::array_direction<double>;
  constexpr direction default_array{};
  static_assert(default_array.coordinates == std::array<double, 3>{0, 0, 1});
  static_assert(direction::create().coordinates[2] == 1);
  const direction array_custom{std::array<double, 3>{5, 6, 7}};
  EXPECT_EQ(round_trip(array_custom).coordinates, array_custom.coordinates);
}

TEST(constructor_initialization, nested_defaults_are_fresh_and_partial_decode_keeps_schema_values) {
  using frame = constructor_initialization::frame<double>;
  constexpr frame neutral{};
  static_assert(neutral.normal.z == 1);
  static_assert(neutral.axes[0].x == 1 && neutral.axes[1].y == 1);
  static_assert(frame::create().normal.z == 1);
  frame first;
  frame second;
  first.normal.z = 8;
  first.axes[0].x = 9;
  EXPECT_EQ(second.normal.z, 1);
  EXPECT_EQ(second.axes[0].x, 1);
  const std::string_view empty_record = "{}";
  const auto input = rohit::make_constant_stream(empty_record.data(), empty_record.size());
  const auto decoded = rohit::serializer::deserialize_exact<frame, rohit::serializer::json>(input);
  EXPECT_EQ(decoded.normal.z, 1);
  EXPECT_EQ(decoded.axes[0].x, 1);
  EXPECT_EQ(round_trip(first).normal.z, 8);
  EXPECT_EQ(round_trip(first).axes[0].x, 9);
}

TEST(constructor_initialization, empty_record_parameter_defaults_keep_schema_values_and_overrides) {
  using area = constructor_initialization::area;
  using box = constructor_initialization::box;
  constexpr box omitted{1, 2};
  static_assert(omitted.a == 1 && omitted.b == 2);
  static_assert(omitted.extent.width == 3 && omitted.extent.height == 4);
  static_assert(box::create(1, 2).extent.width == 3);
  static_assert(box::create_default_2(1, 2).extent.height == 4);
  static_assert(box::sized(1, 2).extent.width == 10);
  static_assert(box::sized_default_2(1, 2).extent.height == 20);
  constexpr box customized{1, 2, area::create(11, 12)};
  static_assert(customized.extent.width == 11 && customized.extent.height == 12);
  const auto decoded = round_trip(customized);
  EXPECT_EQ(decoded.extent.width, 11);
  EXPECT_EQ(decoded.extent.height, 12);

  using owner = constructor_initialization::frame_owner;
  static_assert(owner::create().value.normal.z == 1);
  static_assert(owner::from_frame().value.axes[0].x == 1);
  static_assert(owner::from_nested().value.axes[0].z == 1);
  static_assert(owner::from_nested().value.axes[1].z == 1);
  auto first = owner::create();
  const auto second = owner::create();
  first.value.normal.z = 8;
  first.value.axes[0].x = 9;
  EXPECT_EQ(second.value.normal.z, 1);
  EXPECT_EQ(second.value.axes[0].x, 1);
  EXPECT_EQ(owner::from_frame_default_0().value.axes[1].y, 1);

  using generic_owner = constructor_initialization::generic_frame_owner<double>;
  static_assert(generic_owner::create().value.normal.z == 1);
  static_assert(generic_owner::from_frame().value.axes[0].x == 1);
  using custom_owner = constructor_initialization::generic_frame_owner<immutable_input>;
  static_assert(custom_owner::create().value.normal.z.value == 1);
  static_assert(custom_owner::from_frame().value.axes[1].y.value == 1);
}

TEST(constructor_initialization, empty_record_defaults_preserve_language_and_shape_boundaries) {
  for (const auto text : {
      "serializer version 1.14.0; class area { public uint32 width {3}; } class box { public area value; public constructor(area input = {}) {value=input;} }",
      "serializer version 1.15.0; class area { public uint32 width; public uint32 height; } class box { public area value; public constructor(area input = {10}) {value=input;} }",
      "serializer version 1.15.0; class box<T> { public T value; public constructor(T input = {}) {value=input;} }",
      "serializer version 1.15.0; class box { public uint32 value; public constructor(uint32 input = {}) {value=input;} }",
      "serializer version 1.15.0; class box { public array[2] uint32 value; public constructor(array[2] uint32 input = {}) {value=input;} }",
      "serializer version 1.15.0; class area { public uint32 width; private constructor() = default; } class box { public area value; public constructor(area input = {}) {value=input;} }",
      "serializer version 1.15.0; class area { public uint32 width; } class box { public area value; public constructor(area input = {}, uint32 required) {value=input;} }"}) {
    EXPECT_THROW(parse(text), std::exception) << text;
  }
  const auto legacy_empty = parse("serializer version 1.14.0; class area {} "
      "class box { public area value; public constructor(area input = {}) {value=input;} }");
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(legacy_empty, options));
}

TEST(constructor_initialization, external_default_constructor_controls_empty_default_qualification) {
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  options.rename_identifiers = false;
  for (const auto declaration : {"public constructor();",
                                 "public cpp { constexpr } constructor();"}) {
    const auto schema = parse(std::string{"serializer version 1.15.0; "
        "class area { public uint32 width; "} + declaration +
        " } class box { public area value; public constructor(area input = {}) {value=input;} }");
    const auto output = rohit::serializer::writer::cpp::generate(schema, options);
    EXPECT_EQ(output.find("box create_default_0() noexcept"), std::string::npos);
    if (std::string_view{declaration}.find("constexpr") == std::string_view::npos) {
      EXPECT_NE(output.find("[[nodiscard]] static box create_default_0()"), std::string::npos);
      EXPECT_EQ(output.find("static constexpr box create_default_0()"), std::string::npos);
    } else {
      EXPECT_NE(output.find("static constexpr box create_default_0()"), std::string::npos);
    }
  }
  const auto expression_schema = parse("serializer version 1.15.0; "
      "class area { public uint32 width; public constructor(); } "
      "class box { public uint32 first; public area value; "
      "public constructor(uint32 first_input, area input = {}) { first=first_input; value=input; } } "
      "function box make() readonly expression(box(1)); "
      "function box copy(box input) readonly expression(input);");
  const auto expression_output = rohit::serializer::writer::cpp::generate(expression_schema, options);
  EXPECT_NE(expression_output.find("::box make()"), std::string::npos);
  EXPECT_EQ(expression_output.find("constexpr ::box make()"), std::string::npos);
  EXPECT_NE(expression_output.find("::box copy("), std::string::npos);
  EXPECT_EQ(expression_output.find("constexpr ::box copy("), std::string::npos);
}

TEST(constructor_initialization, external_empty_record_defaults_propagate_host_constructor_failures) {
  using owner = constructor_initialization::external_area_owner;
  static_assert(!noexcept(owner::create_default_1(1)));
  reject_external_default = true;
  EXPECT_THROW(static_cast<void>(owner::create_default_1(1)), std::runtime_error);
  EXPECT_THROW(static_cast<void>(owner::create(1)), std::runtime_error);
  reject_external_default = false;
}

TEST(constructor_initialization, older_constructor_lowering_retains_its_contract) {
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  options.rename_identifiers = false;
  const auto old_schema = parse("serializer version 1.13.0; class sample<T> { public T value; public constructor(T input) { value = input; } }");
  const auto new_schema = parse("serializer version 1.14.0; class sample<T> { public T value; public constructor(T input) { value = input; } }");
  const auto old_output = rohit::serializer::writer::cpp::generate(old_schema, options);
  const auto new_output = rohit::serializer::writer::cpp::generate(new_schema, options);
  EXPECT_NE(old_output.find(": sample()"), std::string::npos);
  EXPECT_EQ(new_output.find(": sample()"), std::string::npos);
  EXPECT_NE(new_output.find("value{[](auto& srl_owned_value)"), std::string::npos);

  // Parsing a new declared contract must not tighten subsequent headerless library fragments.
  for (const auto prefix : {"", "serializer version 1.13.0; "}) {
    const auto legacy = parse(std::string{prefix} +
        "class sample { public array[2] uint32 values {1+2,0xFF}; }");
    const auto& record = static_cast<const rohit::serializer::class_node&>(*legacy.back());
    EXPECT_FALSE(record.member_list.front().portable_default_enabled);
    EXPECT_FALSE(record.member_list.front().initializer);
    EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(legacy, options));
  }
}

TEST(constructor_initialization, typed_default_shape_and_range_diagnostics_are_explicit) {
  for (const auto text : {
      "serializer version 1.13.0; class p { public array[3] double value; public constructor(array[3] double input = {0,0,1}) {value=input;} }",
      "serializer version 1.14.0; class p { public array[3] uint8 value {0,1}; }",
      "serializer version 1.14.0; class p { public array[3] uint8 value {0,1,256}; }",
      "serializer version 1.14.0; class p { public array[2] uint32 value {1+2,4}; }",
      "serializer version 1.14.0; class p { public double x; public double y; } class q { public p value {{1}}; }",
      "serializer version 1.14.0; class p { public double x; } class q { public p value {{1,2}}; }",
      "serializer version 1.14.0; class p { public array[3] uint8 value; public constructor(array[3] uint8 input = {0,1,256}) {value=input;} }"}) {
    EXPECT_THROW(parse(text), std::exception) << text;
  }
  const auto quoted = parse("serializer version 1.14.0; class item { public string text; public char marker; public bool active; } "
      "class record { public item value {{\"brace{}, comma\", '{', false}}; }");
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(quoted, options));
  EXPECT_FALSE(rohit::serializer::writer::java::generate(quoted, "Schema", {}).empty());
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_FALSE(rohit::serializer::writer::portable::generate(quoted, language, "Schema", {}).empty()) << language;
  }
  const auto inferred = parse("serializer version 1.14.0; class bytes { public array[] char text {'A', 'B'}; }");
  EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(inferred, options));
  EXPECT_FALSE(rohit::serializer::writer::java::generate(inferred, "Schema", {}).empty());
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_FALSE(rohit::serializer::writer::portable::generate(inferred, language, "Schema", {}).empty()) << language;
  }
}
