// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later

#include "record_comparison.hpp"

#include <rohit/output_options.hpp>
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>

#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
namespace schema = rohit::serializer;
namespace writer = schema::writer;
namespace values = comparison_demo;

// Supply equality and strict ordering while deliberately omitting a spaceship operator.
struct fallback_value {
  std::uint32_t value{};
  // Compare the represented value for equality.
  constexpr bool operator==(const fallback_value& other) const noexcept { return value == other.value; }
  // Compare the represented value using a strict weak ordering.
  constexpr bool operator<(const fallback_value& other) const noexcept { return value < other.value; }
};

// Supply a native nonthrowing spaceship for generic record comparison inference.
struct native_comparison_value {
  std::uint32_t value{};
  // Compare the represented scalar through the standard strong-ordering category.
  constexpr auto operator<=>(const native_comparison_value&) const noexcept = default;
};

// A generic native comparator must retain its throwing contract through tuple synthesis.
struct throwing_comparison_value {
  // Exercise reached equality failure through the generated owning record.
  bool operator==(const throwing_comparison_value&) const {
    throw std::domain_error{"Comparison failed"};
  }
  // Exercise reached native ordering failure without an implicit terminate boundary.
  std::strong_ordering operator<=>(const throwing_comparison_value&) const {
    throw std::domain_error{"Comparison failed"};
  }
};

// The fallback path must account for throwing less-than even when equality cannot throw.
struct throwing_fallback_value {
  // Equality succeeds independently of the fallible ordering operation.
  constexpr bool operator==(const throwing_fallback_value&) const noexcept { return true; }
  // Exercise the less-than path selected for values without a native spaceship.
  bool operator<(const throwing_fallback_value&) const {
    throw std::domain_error{"Fallback comparison failed"};
  }
};

// Equality may be nonthrowing while its result's required Boolean conversion still throws.
struct throwing_boolean_result {
  // Exercise conversion failure after a successful component equality call.
  operator bool() const { throw std::domain_error{"Comparison conversion failed"}; }
};
struct throwing_conversion_value {
  // Produce a fallible Boolean-like result without throwing in the equality operator itself.
  throwing_boolean_result operator==(const throwing_conversion_value&) const noexcept { return {}; }
};

// Exercise native generic instantiation without imposing serialization on application types.
struct incomparable_value {
  std::uint32_t value{};
};

template <typename Value>
concept equality_available = requires(const Value& left, const Value& right) {
  { left == right } -> std::convertible_to<bool>;
};

template <typename Value>
concept ordering_available = requires(const Value& left, const Value& right) { left <=> right; };

static_assert(std::same_as<decltype(std::declval<const values::shipment_key&>() <=>
                                   std::declval<const values::shipment_key&>()), std::strong_ordering>);
static_assert(std::same_as<decltype(std::declval<const values::float_record&>() <=>
                                   std::declval<const values::float_record&>()), std::partial_ordering>);
static_assert(std::same_as<decltype(std::declval<const values::generic_record<fallback_value>&>() <=>
                                   std::declval<const values::generic_record<fallback_value>&>()),
                           std::weak_ordering>);
static_assert(!equality_available<values::generic_record<incomparable_value>>);
static_assert(!ordering_available<values::generic_record<incomparable_value>>);
static_assert(equality_available<values::equality_only>);
static_assert(!ordering_available<values::equality_only>);
static_assert(!equality_available<values::order_only>);
static_assert(ordering_available<values::order_only>);
static_assert(noexcept(std::declval<const values::shipment_key&>() ==
                       std::declval<const values::shipment_key&>()));
static_assert(noexcept(std::declval<const values::shipment_key&>() <=>
                       std::declval<const values::shipment_key&>()));
static_assert(noexcept(std::declval<const values::derived_record&>() <=>
                       std::declval<const values::derived_record&>()));
static_assert(noexcept(std::declval<const values::generic_record<fallback_value>&>() <=>
                       std::declval<const values::generic_record<fallback_value>&>()));
static_assert(noexcept(std::declval<const values::generic_record<native_comparison_value>&>() <=>
                       std::declval<const values::generic_record<native_comparison_value>&>()));
static_assert(!noexcept(std::declval<const values::generic_record<throwing_comparison_value>&>() ==
                        std::declval<const values::generic_record<throwing_comparison_value>&>()));
static_assert(!noexcept(std::declval<const values::generic_record<throwing_comparison_value>&>() <=>
                        std::declval<const values::generic_record<throwing_comparison_value>&>()));
static_assert(!noexcept(std::declval<const values::generic_record<throwing_fallback_value>&>() <=>
                        std::declval<const values::generic_record<throwing_fallback_value>&>()));
static_assert(!noexcept(std::declval<const values::generic_record<throwing_conversion_value>&>() ==
                        std::declval<const values::generic_record<throwing_conversion_value>&>()));

// Compare actual generated scalar, inherited and generic values during C++20 constant evaluation.
constexpr bool qualify_constant_comparisons() {
  values::shipment_key first{};
  first.warehouse = 1;
  first.parcel = 4;
  auto second = first;
  second.cache = 9;
  if (!(first == second) || (first <=> second) != std::strong_ordering::equal) { return false; }
  ++second.parcel;
  values::generic_record<fallback_value> generic{};
  generic.value.value = 1;
  auto other_generic = generic;
  other_generic.value.value = 2;
  values::derived_record inherited{};
  inherited.value = 1;
  auto other_inherited = inherited;
  other_inherited.value = 2;
  return first < second && generic < other_generic && inherited < other_inherited;
}
static_assert(qualify_constant_comparisons());
static_assert([] {
  values::float_record nonfinite{};
  nonfinite.value = std::numeric_limits<double>::quiet_NaN();
  return !(nonfinite == nonfinite);
}());

// Generate configured output without invoking a formatter or mutating the parsed schema.
std::string generate(std::string_view source, writer::cpp_options options = {}) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = schema::parser::parse(input);
  options.format = false;
  return writer::cpp::generate(statements, options);
}

// Exercise each public portable writer with the same source-qualified comparison settings.
std::string generate_portable(std::string_view source, std::string_view language,
    const std::map<std::string, writer::class_comparison_options>& comparisons = {}) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  const auto statements = schema::parser::parse(input);
  if (language == "java") {
    writer::java_options options{};
    options.class_comparisons = comparisons;
    return writer::java::generate(statements, "Schema", options);
  }
  writer::portable_options options{};
  options.class_comparisons = comparisons;
  return writer::portable::generate(statements, language, "Schema", options);
}

// Compare API tokens across required snake_case and native PascalCase presentation.
std::string normalized_api_source(std::string_view source) {
  std::string result;
  for (auto character : source) {
    if (character == '_') { continue; }
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character + ('a' - 'A'));
    }
    result += character;
  }
  return result;
}

// Each shipment component participates in lexicographic order; transient cache does not.
TEST(record_comparison, durable_memberwise_shipment_key) {
  values::shipment_key left{};
  left.warehouse = 1;
  left.aisle = 2;
  left.shelf = 3;
  left.parcel = 4;
  auto right = left;
  right.cache = 99;
  EXPECT_EQ(left, right);
  EXPECT_EQ(left <=> right, std::strong_ordering::equal);
  for (const auto component : {&values::shipment_key::warehouse, &values::shipment_key::aisle,
                               &values::shipment_key::shelf, &values::shipment_key::parcel}) {
    right = left;
    ++(right.*component);
    EXPECT_NE(left, right);
    EXPECT_LT(left, right);
    EXPECT_GT(right, left);
  }
}

// Default selection includes bases first; an explicit list includes only authored own fields.
TEST(record_comparison, base_subobjects_and_explicit_transient_selection) {
  values::derived_record left{};
  left.value = 1;
  left.extra = 99;
  auto right = left;
  right.value = 2;
  right.extra = 0;
  EXPECT_LT(left, right);
  right.value = left.value;
  EXPECT_GT(left, right);
  values::selected_record selected{};
  selected.value = 1;
  selected.first = 7;
  selected.second = 2;
  selected.cache = 3;
  auto other = selected;
  other.value = 9;
  other.first = 99;
  EXPECT_EQ(selected, other);
  other.cache = 4;
  other.second = 0;
  EXPECT_LT(selected, other);
  other.cache = selected.cache;
  EXPECT_GT(selected, other);
}

// Tuple synthesis preserves partial order and supports types with only equality and less-than.
TEST(record_comparison, category_and_generic_contract) {
  values::float_record floating{};
  floating.value = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(floating == floating);
  EXPECT_EQ(floating <=> floating, std::partial_ordering::unordered);
  values::generic_record<fallback_value> first{};
  first.value.value = 1;
  auto second = first;
  second.value.value = 2;
  EXPECT_LT(first, second);
  EXPECT_EQ(first <=> second, std::weak_ordering::less);
  values::byte_record byte{};
  auto other_byte = byte;
  other_byte.value = 1;
  EXPECT_LT(byte, other_byte);
  values::generic_derived<std::uint32_t> derived{};
  derived.base_value = 1;
  derived.value = 99;
  auto other_derived = derived;
  other_derived.base_value = 2;
  other_derived.value = 0;
  EXPECT_LT(derived, other_derived);
  constexpr values::empty_record empty{};
  static_assert(empty == empty);
  static_assert((empty <=> empty) == std::strong_ordering::equal);
}

// Native, fallback and Boolean-conversion exceptions propagate through conditional record qualifiers.
TEST(record_comparison, throwing_generic_comparisons) {
  values::generic_record<throwing_comparison_value> native{};
  EXPECT_THROW(static_cast<void>(native == native), std::domain_error);
  EXPECT_THROW(static_cast<void>(native <=> native), std::domain_error);
  values::generic_record<throwing_fallback_value> fallback{};
  EXPECT_THROW(static_cast<void>(fallback <=> fallback), std::domain_error);
  values::generic_record<throwing_conversion_value> conversion{};
  EXPECT_THROW(static_cast<void>(conversion == conversion), std::domain_error);
}

// Comparing a value cannot change its serialized fields, storage or any output protocol.
TEST(record_comparison, unchanged_serialization) {
  values::shipment_key original{};
  original.warehouse = 1;
  original.aisle = 2;
  original.shelf = 3;
  original.parcel = 4;
  const auto verify = [&]<template <schema::serialize_type> class Protocol>() {
    rohit::full_stream_auto_alloc before{};
    original.template serialize_out<Protocol>(before);
    auto other = original;
    other.cache = 99;
    EXPECT_EQ(original, other);
    rohit::full_stream_auto_alloc after{};
    other.template serialize_out<Protocol>(after);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(before.begin()), before.current_offset()),
              std::string_view(reinterpret_cast<const char*>(after.begin()), after.current_offset()));
    const auto input = rohit::make_constant_stream(before.begin(), before.current_offset());
    EXPECT_EQ((schema::deserialize_exact<values::shipment_key, Protocol>(input)), original);
  };
  verify.template operator()<schema::binary_none>();
  verify.template operator()<schema::binary_integer>();
  verify.template operator()<schema::binary_string>();
  verify.template operator()<schema::json>();
}

// Configuration validation rejects ambiguous selection and storage that cannot own comparison state.
TEST(record_comparison, invalid_configuration) {
  const std::string_view source = "class record { public uint32 value; }";
  writer::cpp_options options{};
  options.class_comparisons["missing"].equality = true;
  EXPECT_THROW(generate(source, options), std::invalid_argument);
  options.class_comparisons.clear();
  options.class_comparisons["record"] = {true, true, {"missing"}};
  EXPECT_THROW(generate(source, options), std::invalid_argument);
  options.class_comparisons["record"].fields = {"value", "value"};
  EXPECT_THROW(generate(source, options), std::invalid_argument);
  options.class_comparisons["record"].fields.clear();
  for (const auto unsupported : {
      "class record packed { public uint32 value; }",
      "class record view owning { public uint32 value; }",
      "class record { public union(uint32 = integer, double = real) value; }",
      "serializer version 1.4.0; class record { public variant(uint32 = integer, double = real) value; }"}) {
    SCOPED_TRACE(unsupported);
    EXPECT_THROW(generate(unsupported, options), std::invalid_argument);
  }
  options.constant_evaluation = true;
  options.protocols = writer::cpp_protocols::binary_none;
  options.emission_only = true;
  EXPECT_THROW(generate(source, options), std::invalid_argument);
}

// Opt-in APIs leave ordinary generated output unchanged and use the selected native naming profile.
TEST(record_comparison, opt_in_and_naming) {
  const std::string_view source = "namespace names { class test_value { public uint32 field_value; } }";
  const auto baseline = generate(source);
  EXPECT_EQ(baseline.find("operator=="), std::string::npos);
  EXPECT_EQ(baseline.find("operator<=>"), std::string::npos);
  writer::cpp_options options{};
  options.class_comparisons["names::test_value"] = {true, true, {"field_value"}};
  options.standard = writer::coding_standard::google;
  const auto generated = generate(source, options);
  EXPECT_NE(generated.find("operator==(const TestValue&"), std::string::npos);
  EXPECT_NE(generated.find("(*this).field_value_"), std::string::npos);
  options.class_comparisons.clear();
  EXPECT_EQ(generate(source), baseline);
  options.standard = writer::coding_standard::serializer;
  options.class_comparisons["record"] = {true, true, {}};
  const auto shadowed = generate("class record<T> { public T other; public T left; "
                                "public T right; }", options);
  EXPECT_NE(shadowed.find("const record& other_1"), std::string::npos);
  EXPECT_NE(shadowed.find("const record& left_1"), std::string::npos);
  EXPECT_NE(shadowed.find("const record& right_1"), std::string::npos);
  const auto parameter_shadow = generate(
      "class record<other,left,right> { public other first; public left second; "
      "public right third; }", options);
  EXPECT_NE(parameter_shadow.find("const record& other_1"), std::string::npos);
  EXPECT_NE(parameter_shadow.find("const record& left_1"), std::string::npos);
  EXPECT_NE(parameter_shadow.find("const record& right_1"), std::string::npos);
}

// Authored named instances remain concrete native records with inherited or independent settings.
TEST(record_comparison, named_instance_configuration) {
  const std::string_view source =
      "serializer version 1.2.0; class box<T> { public T value; } "
      "instantiate first = box<uint8>; instantiate second = box<uint8>;";
  writer::cpp_options options{};
  options.class_comparisons["box"] = {true, true, {}};
  const auto generated = generate(source, options);
  EXPECT_NE(generated.find("class first {"), std::string::npos);
  EXPECT_NE(generated.find("class second {"), std::string::npos);
  EXPECT_NE(generated.find("operator==(const first&"), std::string::npos);
  EXPECT_NE(generated.find("operator<=>(const second&"), std::string::npos);
  EXPECT_NE(generated.find("operator==(const box&"), std::string::npos);
  options.class_comparisons["second"] = {true, false, {"value"}};
  const auto independent = generate(source, options);
  EXPECT_NE(independent.find("operator==(const second&"), std::string::npos);
  EXPECT_EQ(independent.find("operator<=>(const second&"), std::string::npos);
  options.class_comparisons["first"] = {false, true, {"value"}};
  const auto order_only = generate(source, options);
  EXPECT_EQ(order_only.find("operator==(const first&"), std::string::npos);
  EXPECT_NE(order_only.find("operator<=>(const first&"), std::string::npos);
}

// Direct managed values compare authored fields while separate value companions reject opt-in.
TEST(record_comparison, direct_managed_values) {
  const std::string_view source =
      "serializer version 1.8.0; class record stable_ids managed { public uint32 value(1); }";
  writer::cpp_options options{};
  options.class_comparisons["record"] = {true, true, {}};
  const auto generated = generate(source, options);
  EXPECT_NE(generated.find("operator==(const record&"), std::string::npos);
  EXPECT_NE(generated.find("::std::tie((*this).value)"), std::string::npos);
  EXPECT_EQ(generated.find("::std::tie((*this).persistent_id"), std::string::npos);
  options.managed_separate_values = true;
  EXPECT_THROW(generate(source, options), std::invalid_argument);
  options.managed_separate_values = false;
  options.class_comparisons.clear();
  options.class_comparisons["box"] = {true, true, {}};
  const auto inherited = generate(
      "serializer version 1.8.0; class box<T> stable_ids managed { public T value(1); } "
      "instantiate byte_record = box<uint8>;", options);
  EXPECT_NE(inherited.find("operator==(const byte_record&"), std::string::npos);
  EXPECT_NE(inherited.find("::std::tie((*this).value)"), std::string::npos);
  EXPECT_EQ(inherited.find("::std::tie((*this).persistent_id"), std::string::npos);
}

// All ten portable APIs select qualified records consistently while opt-out output stays exact.
TEST(record_comparison, portable_opt_in_contract) {
  const std::string_view source =
      "serializer version 1.8.0; namespace catalog { "
      "enum phase { ready, shipped } "
      "class parcel { public uint64 weight; } "
      "class shipment { public parcel item; public string destination; public phase state; "
      "public transient uint32 cache; } "
      "class untouched { public uint32 value; } }";
  const std::map<std::string, writer::class_comparison_options> settings{
      {"catalog::parcel", {true, true, {}}}, {"catalog::shipment", {true, true, {}}}};
  for (const auto language : {"java", "js", "typescript", "go", "csharp", "rust", "python",
                              "swift", "kotlin", "c"}) {
    SCOPED_TRACE(language);
    const auto baseline = generate_portable(source, language);
    const auto generated = generate_portable(source, language, settings);
    const auto normalized = normalized_api_source(generated);
    EXPECT_NE(normalized.find("equalsvalue("), std::string::npos);
    EXPECT_NE(normalized.find("comparevalue("), std::string::npos);
    EXPECT_EQ(normalized_api_source(baseline).find("equalsvalue("), std::string::npos);
    EXPECT_EQ(normalized_api_source(baseline).find("comparevalue("), std::string::npos);
    const std::map<std::string, writer::class_comparison_options> disabled{
        {"catalog::shipment", {false, false, {}}}};
    EXPECT_EQ(generate_portable(source, language, disabled), baseline);
  }
}

// Reject unsupported selected storage, missing record dependencies and colliding authored APIs.
TEST(record_comparison, portable_validation_contract) {
  const std::map<std::string, writer::class_comparison_options> settings{
      {"record", {true, true, {}}}};
  for (const auto language : {"java", "js", "typescript", "go", "csharp", "rust", "python",
                              "swift", "kotlin", "c"}) {
    SCOPED_TRACE(language);
    for (const auto source : {
        "class record { public array uint32 values; }",
        "class record { public array[2] uint32 values; }",
        "class record { public map(uint32) uint32 values; }",
        "serializer version 1.7.0; class record { public optional<uint32> value; }",
        "class record { public version version2 revision { \"1.0\" }; }",
        "class record { public version version3 revision { \"1.0.0\" }; }",
        "class record { public version version4 revision { \"1.0.0.0\" }; }",
        "class record { public union(uint32 = integer, double = real) value; }",
        "serializer version 1.4.0; class record { public variant(uint32 = integer, double = real) value; }",
        "class child { public uint32 value; } class record { public child item; }",
        "class child { public uint32 value; } class record : public child(1) {}",
        "class record { public uint32 equals_value; }",
        "class record { public uint32 compare_value; }",
        "serializer version 1.8.0; class record { public uint32 value; "
        "public function bool equals_value() readonly expression(true); }"}) {
      SCOPED_TRACE(source);
      EXPECT_THROW(generate_portable(source, language, settings), std::invalid_argument);
    }
    auto explicit_selection = settings;
    explicit_selection.at("record").fields = {"cache"};
    EXPECT_NO_THROW(generate_portable(
        "serializer version 1.8.0; class record { public array uint32 values; "
        "public transient uint32 cache; }", language, explicit_selection));
    explicit_selection.at("record").fields = {"cache", "cache"};
    EXPECT_THROW(generate_portable(
        "serializer version 1.8.0; class record { public transient uint32 cache; }",
        language, explicit_selection), std::invalid_argument);
    EXPECT_THROW(generate_portable("class other {}", language, settings), std::invalid_argument);
    auto nested = settings;
    nested.emplace("child", writer::class_comparison_options{true, false, {}});
    EXPECT_THROW(generate_portable(
        "class child { public uint32 value; } class record { public child item; }",
        language, nested), std::invalid_argument);
    nested.at("child").ordering = true;
    EXPECT_NO_THROW(generate_portable(
        "class child { public uint32 value; } class record { public child item; }",
        language, nested));
  }
}

// C reserves per-record comparison functions only while their corresponding APIs are enabled.
TEST(record_comparison, c_global_comparison_symbols) {
  std::map<std::string, writer::class_comparison_options> settings{
      {"record", {true, true, {}}}};
  for (const auto name : {"record_equals_value", "record_compare_value"}) {
    SCOPED_TRACE(name);
    const auto source = std::string{"class "} + name +
        " {} class record { public uint32 value; }";
    EXPECT_THROW(generate_portable(source, "c", settings), std::invalid_argument);
    EXPECT_NO_THROW(generate_portable(source, "c"));
  }
  settings.at("record").ordering = false;
  EXPECT_NO_THROW(generate_portable(
      "class record_compare_value {} class record { public uint32 value; }", "c", settings));
  settings.at("record") = {false, true, {}};
  EXPECT_NO_THROW(generate_portable(
      "class record_equals_value {} class record { public uint32 value; }", "c", settings));
}
} // namespace
