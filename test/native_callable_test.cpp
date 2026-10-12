// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_callables.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
const void* observed_input{};

// Make a host element whose object identity cannot survive a by-value API boundary.
struct noncopyable_element {
  int value{};
  noncopyable_element() = default;
  noncopyable_element(const noncopyable_element&) = delete;
  noncopyable_element(noncopyable_element&&) = delete;
  noncopyable_element& operator=(const noncopyable_element&) = delete;
  noncopyable_element& operator=(noncopyable_element&&) = delete;
};

// Keep host error construction failures distinct from errors returned through expected.
struct throwing_error {
  // A native error adapter may throw; the expected result declaration makes no noexcept promise.
  throwing_error(std::uint32_t, const char*) { throw std::runtime_error{"Host error construction failed"}; }
};

// Probe actual constrained member lookup rather than inspecting generated requires text alone.
template <typename Value>
concept supports_cross = requires(const Value& value) { value.cross(value); };

// Keep independently generic input rejection in a dependent requires-expression.
template <typename Owner, typename Input>
concept supports_from = requires(Input input) { Owner::from(input); };

// Compile neutral snippets using their declared language and retain all parser diagnostics.
auto parse(std::string_view source) {
  return rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}
} // namespace

namespace native_callables {
// Define one external static function with two exact borrowed array inputs.
template <std::uint64_t N, typename T>
requires ((((::std::integral<T> || ::std::floating_point<T>) && !(::std::same_as<T, bool>))))
vector<N, T> vector<N, T>::difference(const std::array<T, N>& left, const std::array<T, N>& right) {
  std::array<T, N> result{};
  for (std::size_t index{}; index < result.size(); ++index) { result[index] = left[index] - right[index]; }
  return vector{result};
}

// Preserve exact checked conversion failures before any host expected error is constructed.
template <std::uint64_t N, typename T>
requires ((((::std::integral<T> || ::std::floating_point<T>) && !(::std::same_as<T, bool>))))
std::array<double, N> vector<N, T>::converted() const {
  std::array<double, N> result{};
  for (std::size_t index{}; index < result.size(); ++index) {
    result[index] = rohit::serializer::checked_numeric_cast<double>(coordinates[index]);
  }
  return result;
}

// Supply a const receiver definition with a fixed-array reference, without changing either input.
template <std::uint64_t N, typename T>
requires ((((::std::integral<T> || ::std::floating_point<T>) && !(::std::same_as<T, bool>))))
double vector<N, T>::dot(const std::array<T, N>& other) const {
  double result{};
  for (std::size_t index{}; index < coordinates.size(); ++index) {
    result += static_cast<double>(coordinates[index]) * static_cast<double>(other[index]);
  }
  return result;
}

// Keep the guarded formula external so concretized declarations do not rewrite opaque template tokens.
template <std::uint64_t N, typename T>
requires ((((::std::integral<T> || ::std::floating_point<T>) && !(::std::same_as<T, bool>))))
vector<N, T> vector<N, T>::cross(const vector& other) const
    requires (((N == 3) && ::std::same_as<T, double>)) {
  return vector{std::array<T, N>{coordinates[1] * other.coordinates[2] - coordinates[2] * other.coordinates[1],
      coordinates[2] * other.coordinates[0] - coordinates[0] * other.coordinates[2],
      coordinates[0] * other.coordinates[1] - coordinates[1] * other.coordinates[0]}};
}

// Normalize valid checked coordinates; only the explicit magnitude failure returns a host error.
template <std::uint64_t N, typename Error>
template <typename Input>
requires ((((::std::integral<Input> || ::std::floating_point<Input>) && !(::std::same_as<Input, bool>))))
std::expected<normalized_value<N, Error>, Error>
normalized_value<N, Error>::from(vector<N, Input> input) {
  auto values = input.converted();
  double squared{};
  for (const auto value : values) { squared += value * value; }
  const auto magnitude = std::sqrt(squared);
  if (!std::isfinite(magnitude) || magnitude <= 0) {
    return std::unexpected(Error{7u, "Invalid norm"});
  }
  for (auto& value : values) { value /= magnitude; }
  return normalized_value{values};
}

// Observe caller-owned storage without copying, moving, indexing, or retaining it beyond this test.
template <typename Value>
void borrowed_owner<Value>::observe(const std::array<Value, 2>& input) {
  observed_input = std::addressof(input);
}

// Borrow an independently unconstrained native representation without requiring it to copy or move.
template <typename Value>
template <typename Input>
void borrowed_owner<Value>::observe_input(const Input& input) const {
  observed_input = std::addressof(input);
}

// Supply a nongeneric static expected definition checked through an ordinary function pointer.
std::expected<static_api, operation_error> static_api::make(std::uint64_t input) {
  static_api result;
  result.value = input;
  return result;
}

// Preserve success-without-a-value and the explicitly authored failure payload.
std::expected<void, operation_error> static_api::validate(bool accepted) {
  if (!accepted) { return std::unexpected(operation_error{11u, "Rejected"}); }
  return {};
}

// A scalar error payload preserves the explicitly promised nonthrowing static definition.
std::expected<void, std::uint32_t> static_api::ready(bool accepted) noexcept {
  if (!accepted) { return std::unexpected(std::uint32_t{13}); }
  return {};
}
} // namespace native_callables

namespace {
// Static and instance declarations must match actual C++ function/member pointer types exactly.
TEST(native_callable, exact_static_array_and_borrowed_signatures) {
  using vector = native_callables::vector<3>;
  using difference = vector (*)(const std::array<double, 3>&, const std::array<double, 3>&);
  using converted = std::array<double, 3> (vector::*)() const;
  using dot = double (vector::*)(const std::array<double, 3>&) const;
  static_assert(std::same_as<decltype(&vector::difference), difference>);
  static_assert(std::same_as<decltype(&vector::converted), converted>);
  static_assert(std::same_as<decltype(&vector::dot), dot>);
  static_assert(sizeof(vector) == sizeof(std::array<double, 3>));
  const std::array<double, 3> left{4, 6, 8}, right{1, 2, 3};
  const auto result = vector::difference(left, right);
  EXPECT_EQ(result.coordinates, (std::array<double, 3>{3, 4, 5}));
  EXPECT_EQ(result.dot(right), 26);
  EXPECT_EQ(left[0], 4);
  EXPECT_EQ(right[0], 1);
  rohit::full_stream_auto_alloc output;
  result.serialize_out<rohit::serializer::binary_integer>(output);
  const auto input_bytes = rohit::make_constant_stream(output.begin(), output.current_offset());
  const auto decoded = rohit::serializer::deserialize_exact<vector, rohit::serializer::binary_integer>(input_bytes);
  EXPECT_EQ(decoded.coordinates, result.coordinates);
  vector::serializer_require_behavior_definitions();
  const std::array<noncopyable_element, 2> input{};
  native_callables::borrowed_owner<noncopyable_element>::observe(input);
  EXPECT_EQ(observed_input, std::addressof(input));
  native_callables::borrowed_owner<noncopyable_element>::serializer_require_behavior_definitions();
  const native_callables::borrowed_owner<noncopyable_element> borrowed{};
  borrowed.observe_input(input.front());
  EXPECT_EQ(observed_input, std::addressof(input.front()));
  observed_input = nullptr;
}

// A callable-local guard must remove unsupported operations without constraining the owning class.
TEST(native_callable, dimension_and_exact_type_guards_preserve_ordinary_owners) {
  static_assert(supports_cross<native_callables::vector<3>>);
  static_assert(!supports_cross<native_callables::vector<2>>);
  static_assert(!supports_cross<native_callables::vector<3, float>>);
  const native_callables::vector<3> left{std::array<double, 3>{1, 0, 0}};
  const native_callables::vector<3> right{std::array<double, 3>{0, 1, 0}};
  EXPECT_EQ(left.cross(right).coordinates, (std::array<double, 3>{0, 0, 1}));
  native_callables::vector<2>::serializer_require_behavior_definitions();
  native_callables::vector<3, float>::serializer_require_behavior_definitions();
}

// Expected represents only authored failures; checked conversion and error construction still throw.
TEST(native_callable, expected_self_owner_and_independent_inputs_preserve_failures) {
  using owner = native_callables::normalized_value<3>;
  const native_callables::vector<3, std::uint32_t> input{std::array<std::uint32_t, 3>{3, 4, 0}};
  const auto success = owner::from(input);
  ASSERT_TRUE(success.has_value());
  EXPECT_DOUBLE_EQ(success->coordinates[0], 0.6);
  EXPECT_DOUBLE_EQ(success->coordinates[1], 0.8);
  EXPECT_EQ(input.coordinates[0], 3);
  const auto rejected = owner::from(native_callables::vector<3>{});
  ASSERT_FALSE(rejected.has_value());
  EXPECT_EQ(rejected.error().code, 7);
  EXPECT_EQ(rejected.error().message, "Invalid norm");
  const native_callables::vector<3> overflowing{std::array<double, 3>{1e308, 0, 0}};
  EXPECT_FALSE(owner::from(overflowing).has_value());
  const auto inexact = (std::uint64_t{1} << std::numeric_limits<double>::digits) + 1;
  const native_callables::vector<3, std::uint64_t> wide{std::array<std::uint64_t, 3>{inexact, 0, 0}};
  EXPECT_THROW(static_cast<void>(owner::from(wide)), std::domain_error);
  const native_callables::vector<3> nonfinite{
      std::array<double, 3>{std::numeric_limits<double>::infinity(), 0, 0}};
  EXPECT_THROW(static_cast<void>(owner::from(nonfinite)), std::domain_error);
  using throwing_owner = native_callables::normalized_value<3, throwing_error>;
  EXPECT_THROW(static_cast<void>(throwing_owner::from(native_callables::vector<3>{})), std::runtime_error);
  static_assert(!noexcept(owner::from(input)));
  static_assert(supports_from<owner, native_callables::vector<3, std::uint32_t>>);
  static_assert(!supports_from<owner, bool>);
  static_assert(!supports_from<owner, std::string>);
}

// Nongeneric static methods retain external-definition qualification and legal void expected results.
TEST(native_callable, external_static_expected_and_void_definitions_are_qualified) {
  using api = native_callables::static_api;
  using make = std::expected<api, native_callables::operation_error> (*)(std::uint64_t);
  using validate = std::expected<void, native_callables::operation_error> (*)(bool);
  using ready = std::expected<void, std::uint32_t> (*)(bool) noexcept;
  static_assert(std::same_as<decltype(&api::make), make>);
  static_assert(std::same_as<decltype(&api::validate), validate>);
  static_assert(std::same_as<decltype(&api::ready), ready>);
  static_assert(noexcept(api::ready(true)));
  static_assert(api::native_identity(17) == 17 && noexcept(api::native_identity(17)));
  api::serializer_require_behavior_definitions();
  EXPECT_EQ(api::make(std::numeric_limits<std::uint64_t>::max())->value,
      std::numeric_limits<std::uint64_t>::max());
  EXPECT_TRUE(api::validate(true).has_value());
  const auto rejected = api::validate(false);
  ASSERT_FALSE(rejected.has_value());
  EXPECT_EQ(rejected.error().code, 11);
  EXPECT_TRUE(api::ready(true).has_value());
  EXPECT_EQ(api::ready(false).error(), 13);
}

// Typed native metadata cannot introduce storage policies, invalid extents, or legacy-language syntax.
TEST(native_callable, parser_rejects_unsupported_result_storage_and_signature_contracts) {
  for (const std::string_view source : {
      "serializer version 1.16.0; class value { public function static uint64 f() readonly; }",
      "serializer version 1.16.0; class value { public function array[3] double f() readonly; }",
      "serializer version 1.16.0; class value { public function uint64 f(uint64 x const_ref) readonly; }",
      "serializer version 1.16.0; class value { public function expected<uint64,uint32> f() readonly; }",
      "serializer version 1.16.0; class value { public function void f<Input>(Input input) readonly; }",
      "serializer version 1.17.0; class value { public function Input f<Input>(Input input) readonly expression(input); }",
      "serializer version 1.17.0; class value { public expected<uint64,uint32> field; }",
      "serializer version 1.17.0; class value { public optional<expected<uint64,uint32>> field; }",
      "serializer version 1.17.0; class value { public map<uint64,expected<uint64,uint32>> field; }",
      "serializer version 1.17.0; class value { public function uint64 f(expected<uint64,uint32> x) readonly; }",
      "serializer version 1.17.0; class value { public function expected<uint64,void> f() readonly; }",
      "serializer version 1.17.0; class value { public function expected<expected<uint64,uint32>,uint32> f() readonly; }",
      "serializer version 1.17.0; class value { public function array[2] expected<uint64,uint32> f() readonly; }",
      "serializer version 1.17.0; class value { public function array[0] double f() readonly; }",
      "serializer version 1.17.0; class value { public function array[65537] double f() readonly; }",
      "serializer version 1.17.0; class value { public function uint64 f(array[Unknown] double x const_ref) readonly; }",
      "serializer version 1.17.0; class value { public uint64 x; public function static uint64 f() readonly expression(x); }",
      "serializer version 1.17.0; function static uint64 f() readonly;",
      "serializer version 1.17.0; class value { public function static virtual uint64 f() readonly; }",
      "serializer version 1.17.0; class value { public function static uint64 f() edit; }",
      "serializer version 1.17.0; class value { public function expected<uint64,uint32> f() readonly expression(1); }",
      "serializer version 1.17.0; class value { public function uint64 f() readonly when(Unknown == 3); }"}) {
    EXPECT_ANY_THROW(parse(source)) << source;
  }
}

// Guard parsing remains bounded and rejects runtime fields, overflow, raw code, and unbound types.
TEST(native_callable, callable_guards_enforce_resource_and_lexical_boundaries) {
  const auto guarded = [](std::string_view condition) {
    return "serializer version 1.17.0; class probe { public uint64 field; "
        "public function static uint64 f() readonly when(" + std::string{condition} + "); }";
  };
  for (const std::string_view condition : {"field == 3", "Unknown == 3", "same_type(Unknown,double)",
        "18446744073709551616 == 1", "1", "", "cpp { return true; }"}) {
    EXPECT_ANY_THROW(parse(guarded(condition))) << condition;
  }
  std::string deep(33, '!');
  deep += "(1 == 1)";
  EXPECT_ANY_THROW(parse(guarded(deep)));
  std::string bounded = "1 == 1";
  constexpr unsigned maximum_comparison_count = 64;
  for (unsigned index{1}; index < maximum_comparison_count; ++index) { bounded += " && 1 == 1"; }
  EXPECT_NO_THROW(parse(guarded(bounded)));
  bounded += " && 1 == 1";
  EXPECT_ANY_THROW(parse(guarded(bounded)));
  EXPECT_ANY_THROW(parse("serializer version 1.16.0; class probe<uint64 N> { "
      "public function uint64 f() readonly when(N == 3); }"));
  EXPECT_ANY_THROW(parse("serializer version 1.17.0; function uint64 f() readonly when(1 == 1);"));
}

// New native policies diagnose every other selected output before mapping their result or receiver.
TEST(native_callable, native_only_outputs_diagnose_and_ordinary_cpp20_models_stay_unaffected) {
  const auto schema = parse("serializer version 1.17.0; class value { public uint64 id; "
      "public function static expected<value,uint32> from(uint64 input) readonly; }");
  for (const auto language : {"js", "typescript", "go", "csharp", "c", "rust", "python", "swift", "kotlin"}) {
    try {
      static_cast<void>(rohit::serializer::writer::portable::generate(schema, language, "probe"));
      FAIL() << language;
    } catch (const std::invalid_argument& error) {
      EXPECT_NE(std::string_view{error.what()}.find("require C++ output"), std::string_view::npos) << language;
    }
  }
  EXPECT_THROW(static_cast<void>(rohit::serializer::writer::java::generate(schema, "Probe")), std::invalid_argument);
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  const auto native_output = rohit::serializer::writer::cpp::generate(schema, options);
  EXPECT_NE(native_output.find("#include <expected>"), std::string::npos);
  EXPECT_NE(native_output.find("require C++23 std::expected"), std::string::npos);
  const auto ordinary = parse("serializer version 1.14.0; class value { public uint64 id; "
      "public function uint64 echo(uint64 input) readonly expression(input); }");
  EXPECT_EQ(rohit::serializer::writer::cpp::generate(ordinary, options).find("#include <expected>"), std::string::npos);
  const auto shadowed = parse("serializer version 1.14.0; class expected<T,Error> { public T value; } "
      "class sample { public expected<uint64,uint32> value; }");
  EXPECT_EQ(rohit::serializer::writer::cpp::generate(shadowed, options).find("#include <expected>"), std::string::npos);
}

// A new contextual static modifier must preserve older record return types with that spelling.
TEST(native_callable, contextual_static_preserves_declared_return_types_in_old_and_new_languages) {
  for (const auto version : {"1.16.0", "1.17.0"}) {
    const auto source = "serializer version " + std::string{version} + "; "
        "class static { public uint64 value; } "
        "class holder { public function static echo(static input) readonly expression(input); } "
        "function static echo(static input) readonly expression(input);";
    const auto schema = parse(source);
    EXPECT_NO_THROW(rohit::serializer::writer::portable::generate(schema, "python", "probe"));
    EXPECT_NO_THROW(rohit::serializer::writer::java::generate(schema, "Probe"));
    const auto& owner = static_cast<const rohit::serializer::class_node&>(*schema.at(1));
    EXPECT_FALSE(owner.functions.at(0).is_static);
    EXPECT_FALSE(static_cast<const rohit::serializer::function_node&>(*schema.at(2)).function.is_static);

    const auto generic = parse("serializer version " + std::string{version} + "; "
        "class static { public uint64 value; public constructor(uint64 input) {value=input;} } "
        "class holder { public function static make<Input: integral && !bool>(Input input) readonly "
        "expression(static(checked<uint64>(input))); }");
    rohit::serializer::writer::cpp_options options;
    options.format = false;
    options.standard = rohit::serializer::writer::coding_standard::google;
    EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(generic, options));
    const auto& generic_owner = static_cast<const rohit::serializer::class_node&>(*generic.at(1));
    EXPECT_FALSE(generic_owner.functions.at(0).is_static);
  }
}
} // namespace
