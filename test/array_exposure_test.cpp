// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "array_exposure.hpp"
#include "array_generation.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// Detect whether a conditional schema factory is present on the selected representation.
template<class Matrix>
concept has_identity = requires { Matrix::identity(); };

// Detect setters without invoking mutation or imposing arbitrary storage requirements.
template<class Value>
concept has_set = requires(Value& value) { value.set(0, 7u); };

// Availability on a guarded constructor must also cover its shortened create wrapper.
template<class Value>
concept has_default_create = requires { Value::create_default_0(); };

// A guarded named factory and its shortened wrapper must expose identical availability.
template<class Value>
concept has_limited_default = requires { Value::limited_default_0(); };

// Compare closed builtin argument identities without requiring those types to appear in wire storage.
template<class Value>
concept has_matching_default = requires { Value::matching_default_0(); };

// Explicitly supplied matrices need no default construction, factory, or assignment operation.
struct copy_only_basis {
  int value;
  copy_only_basis() = delete;
  // Preserve a distinguishable explicitly supplied matrix value.
  constexpr explicit copy_only_basis(int input) : value{input} {}
  copy_only_basis(const copy_only_basis&) = default;
  copy_only_basis(copy_only_basis&&) = delete;
  copy_only_basis& operator=(const copy_only_basis&) = delete;
  copy_only_basis& operator=(copy_only_basis&&) = delete;
};

// Count ordinary field factory calls without changing representation constraints on callers.
struct counted_basis {
  inline static unsigned calls{};
  int value{};
  // Create a fresh identity when and only when the corresponding field is omitted.
  static counted_basis identity() { return {static_cast<int>(++calls)}; }
};

// A field factory may return a distinct value that explicitly constructs the field representation.
struct basis_seed { int value; };
struct constructed_basis {
  int value;
  // Keep construction distinct from implicit conversion and exact factory-result identity.
  constexpr explicit constructed_basis(basis_seed seed) : value{seed.value} {}
  // Supply an identity seed rather than returning the field's exact type.
  static constexpr basis_seed identity() { return {17}; }
};

struct field_factory_failure : std::runtime_error {
  // Preserve a distinguishable native factory error through schema field initialization.
  field_factory_failure() : std::runtime_error{"Field identity construction failed"} {}
};

// Runtime-controlled rejection avoids unreachable-code warnings while checking original error propagation.
struct throwing_basis {
  inline static bool reject{};
  inline static unsigned calls{};
  int value{};
  // Count each requested fresh default and propagate the host's failure category.
  static throwing_basis identity() {
    ++calls;
    if (reject) { throw field_factory_failure{}; }
    return {29};
  }
};

// Restore the fixture flag even if an assertion or construction exits its scope.
struct reject_field_factory_scope {
  bool previous;
  // Temporarily reject defaults without changing explicit field construction.
  reject_field_factory_scope() : previous{std::exchange(throwing_basis::reject, true)} {}
  // Keep later tests independent of this fixture's rejection state.
  ~reject_field_factory_scope() { throwing_basis::reject = previous; }
};

using native_throwing_frame = array_generation::frame<std::uint32_t, throwing_basis>;
using generated_basis = array_generation::matrix<2, 2, double>;
using generated_frame = array_generation::frame<std::uint32_t, generated_basis>;

// Compare actual encoded bytes through each existing protocol, without copying via another codec.
template<template<rohit::serializer::serialize_type> class Protocol, class Value>
std::string encode(const Value& value) {
  rohit::full_stream_auto_alloc output{};
  value.template serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

// Check fresh and omitted field defaults against the explicitly supplied identity on the same wire.
template<template<rohit::serializer::serialize_type> class Protocol>
void qualify_identity_wire(std::string_view protocol) {
  SCOPED_TRACE(protocol);
  const generated_frame fresh{};
  const generated_frame omitted{0u};
  const generated_frame supplied{0u, generated_basis::identity()};
  const auto bytes = encode<Protocol>(fresh);
  EXPECT_EQ(bytes, encode<Protocol>(omitted));
  EXPECT_EQ(bytes, encode<Protocol>(supplied));
  const generated_frame authored{9u};
  const generated_frame overridden{9u, generated_basis::identity()};
  EXPECT_EQ(encode<Protocol>(authored), encode<Protocol>(overridden));
  auto stream = rohit::make_constant_stream(bytes.data(), bytes.size());
  const auto decoded = rohit::serializer::deserialize_exact<generated_frame, Protocol>(stream);
  EXPECT_EQ(decoded.origin, 0u);
  EXPECT_EQ(decoded.basis.elements, supplied.basis.elements);
}

static_assert(has_identity<array_generation::square>);
static_assert(!has_identity<array_generation::rectangular>);
static_assert(has_identity<array_generation::matrix<2, 2, std::int32_t>>);
static_assert(!has_identity<array_generation::matrix<2, 3, std::int32_t>>);
static_assert(!has_set<array_exposure::readonly_triangle>);
static_assert(std::same_as<decltype(std::declval<array_exposure::triangle&>()[0]), std::uint32_t&>);
static_assert(std::same_as<decltype(std::declval<const array_exposure::triangle&>()[0]), const std::uint32_t&>);
static_assert(std::same_as<decltype(std::declval<const array_exposure::grid&>()(0, 0)), std::int32_t>);
static_assert(std::same_as<decltype(std::declval<array_exposure::readonly_triangle&>().data()), const std::uint32_t*>);
constexpr auto identity = array_generation::matrix<2, 2, std::int32_t>::identity();
static_assert(identity(0, 0) == 1 && identity(0, 1) == 0 && identity(1, 1) == 1);
constexpr array_exposure::triangle triangle{3, 2, 1};
static_assert(triangle[0] == 3 && triangle.size() == 3);
static_assert(!noexcept(array_generation::checked_pair{-1, 5}));
static_assert(!noexcept(native_throwing_frame{}));
static_assert(!noexcept(native_throwing_frame{0u}));
static_assert(std::constructible_from<constructed_basis, basis_seed>);
static_assert(!std::same_as<decltype(constructed_basis::identity()), constructed_basis>);
constexpr array_generation::frame<std::uint32_t, constructed_basis> seeded_frame{5u};
static_assert(seeded_frame.basis.value == 17);
static_assert(has_default_create<array_generation::guarded_defaults<3>>);
static_assert(!has_default_create<array_generation::guarded_defaults<2>>);
static_assert(has_limited_default<array_generation::guarded_defaults<3>>);
static_assert(!has_limited_default<array_generation::guarded_defaults<2>>);
static_assert(has_matching_default<array_generation::same_array>);
static_assert(!has_matching_default<array_generation::different_array>);
static_assert(has_matching_default<array_generation::same_optional>);
static_assert(!has_matching_default<array_generation::different_optional>);
static_assert(has_matching_default<array_generation::same_map>);
static_assert(!has_matching_default<array_generation::different_map>);
static_assert(!has_matching_default<array_generation::different_digest_extent>);
static_assert(has_matching_default<array_generation::same_digest_extent>);
static_assert(has_matching_default<array_generation::same_opaque_digest>);
using digest_32 = std::array<std::uint8_t, 32>;
using digest_64 = std::array<std::uint8_t, 64>;
using opaque_digest = std::vector<std::uint8_t>;
static_assert(!has_matching_default<array_generation::type_guard<digest_32, digest_64>>);
static_assert(has_matching_default<array_generation::type_guard<digest_32, digest_32>>);
static_assert(has_matching_default<array_generation::type_guard<opaque_digest, std::vector<std::uint8_t>>>);
constexpr auto collision_safe = array_generation::generated_locals<3, std::uint32_t>::generated(std::uint32_t{1});
static_assert(collision_safe[0] == 1 && collision_safe[1] == 2 && collision_safe[2] == 3);
static_assert(std::same_as<decltype(std::declval<array_generation::map_facade&>()[0]),
    std::map<std::string, std::uint32_t>&>);
static_assert(std::same_as<decltype(std::declval<const array_generation::map_facade&>()[0]),
    std::map<std::string, std::uint32_t>>);
static_assert(std::same_as<decltype(std::declval<const array_generation::record_facade&>().get(0)),
    array_generation::map_record>);

// Preserve contiguous borrowing and mutable algorithms over the actual owned elements.
TEST(array_exposure, iterators_and_scalar_constructor_use_original_storage) {
  array_exposure::triangle value{3, 2, 1};
  std::sort(value.begin(), value.end());
  EXPECT_EQ(value.indices, (std::array<std::uint32_t, 3>{1, 2, 3}));
  EXPECT_EQ(value.data(), value.indices.data());
  EXPECT_EQ(&value.values(), &value.indices);
  const std::array<std::uint32_t, 3>& borrowed = value;
  EXPECT_EQ(&borrowed, &value.indices);
  value.set(1, 7);
  EXPECT_EQ(value.get(1), 7u);
  EXPECT_THROW(value.set(3, 9), std::out_of_range);
  EXPECT_EQ(value.indices[1], 7u);
}

// Validate each coordinate before flattening, including large indices that would otherwise overflow.
TEST(array_exposure, multidimensional_checked_access_preserves_failure_state) {
  array_exposure::grid value;
  EXPECT_EQ(value.get(1, 2), 6);
  value.set(1, 0, 17);
  EXPECT_EQ(value(1, 0), 17);
  const auto before = value.elements;
  EXPECT_THROW(value.set(0, 3, 8), std::out_of_range);
  EXPECT_THROW(value.get(std::numeric_limits<std::size_t>::max(), 0), std::out_of_range);
  EXPECT_EQ(value.elements, before);
}

// Empty dynamic storage is a valid iterable range and rejects indexed mutation without growth.
TEST(array_exposure, dynamic_empty_and_nonempty_ranges) {
  array_exposure::dynamic_values value;
  EXPECT_EQ(value.size(), 0u);
  EXPECT_EQ(value.begin(), value.end());
  EXPECT_THROW(value.set(0, 9), std::out_of_range);
  value.elements = {2, 4};
  value[1] = 7;
  EXPECT_EQ(value.get(1), 7u);
}

// A checked pack element retains its original narrowing exception instead of an inferred noexcept promise.
TEST(array_exposure, checked_pack_elements_reject_without_terminating) {
  EXPECT_THROW((static_cast<void>(array_generation::checked_pair{-1, 5})), std::domain_error);
  const array_generation::checked_pair valid{7, 5};
  EXPECT_EQ(valid[0], 7u);
  EXPECT_EQ(valid[1], 5u);
}

// Omitted field factories run once per fresh construction; explicit fields bypass them entirely.
TEST(array_exposure, identity_field_defaults_preserve_custom_basis_types) {
  counted_basis::calls = 0;
  array_generation::frame<std::int32_t, counted_basis> first;
  array_generation::frame<std::int32_t, counted_basis> second{3};
  EXPECT_EQ(first.basis.value, 1);
  EXPECT_EQ(second.basis.value, 2);
  EXPECT_EQ(counted_basis::calls, 2u);
  const copy_only_basis input{31};
  const array_generation::frame<std::int32_t, copy_only_basis> explicit_frame{4, input};
  EXPECT_EQ(explicit_frame.origin, 4);
  EXPECT_EQ(explicit_frame.basis.value, 31);
  const array_generation::pair<copy_only_basis> packed{input, input};
  EXPECT_EQ(packed[0].value, 31);
  EXPECT_EQ(packed[1].value, 31);
}

// Native factory failures retain their exact category, while fully supplied fields bypass the factory.
TEST(array_exposure, identity_field_factory_failures_and_constructible_results) {
  throwing_basis::reject = false;
  throwing_basis::calls = 0;
  const native_throwing_frame successful{};
  EXPECT_EQ(successful.basis.value, 29);
  EXPECT_EQ(throwing_basis::calls, 1u);
  {
    const reject_field_factory_scope rejection{};
    EXPECT_THROW(static_cast<void>(native_throwing_frame{}), field_factory_failure);
    EXPECT_THROW(static_cast<void>(native_throwing_frame{7u}), field_factory_failure);
    EXPECT_THROW(static_cast<void>(native_throwing_frame::create(7u)), field_factory_failure);
    EXPECT_EQ(throwing_basis::calls, 4u);
    const native_throwing_frame supplied{7u, throwing_basis{31}};
    EXPECT_EQ(supplied.basis.value, 31);
    EXPECT_EQ(throwing_basis::calls, 4u);
  }
  EXPECT_FALSE(throwing_basis::reject);
  const native_throwing_frame later{9u};
  EXPECT_EQ(later.basis.value, 29);
  EXPECT_EQ(throwing_basis::calls, 5u);
  const array_generation::frame<std::uint32_t, constructed_basis> constructed{11u};
  EXPECT_EQ(constructed.origin, 11u);
  EXPECT_EQ(constructed.basis.value, 17);
}

// The authored identity survives missing keyed fields and all four unchanged codec representations.
TEST(array_exposure, generated_identity_defaults_preserve_wire_and_missing_json) {
  qualify_identity_wire<rohit::serializer::binary_none>("binary_none");
  qualify_identity_wire<rohit::serializer::binary_integer>("binary_integer");
  qualify_identity_wire<rohit::serializer::binary_string>("binary_string");
  qualify_identity_wire<rohit::serializer::json>("json");
  constexpr std::string_view missing{"{}"};
  auto empty = rohit::make_constant_stream(missing.data(), missing.size());
  const auto decoded = rohit::serializer::deserialize_exact<generated_frame,
      rohit::serializer::json>(empty);
  EXPECT_EQ(decoded.origin, 0u);
  EXPECT_EQ(decoded.basis.elements, generated_basis::identity().elements);
  constexpr std::string_view explicit_values{"{\"origin\":7,\"basis\":{\"elements\":[2,3,4,5]}}"};
  auto explicit_stream = rohit::make_constant_stream(explicit_values.data(), explicit_values.size());
  const auto overridden = rohit::serializer::deserialize_exact<generated_frame,
      rohit::serializer::json>(explicit_stream);
  EXPECT_EQ(overridden.origin, 7u);
  EXPECT_EQ(overridden.basis.elements, (std::array<double, 4>{2, 3, 4, 5}));
}

// Concrete and open guards preserve both primary and omission APIs without constraining storage.
TEST(array_exposure, guarded_default_wrappers_and_builtin_type_arguments) {
  EXPECT_EQ(array_generation::guarded_defaults<3>::create_default_0().value, 9u);
  EXPECT_EQ(array_generation::guarded_defaults<3>::limited_default_0().value, 7u);
  const array_generation::guarded_defaults<2> ordinary{};
  EXPECT_EQ(ordinary.value, 0u);
  EXPECT_EQ(array_generation::same_array::matching_default_0().value, 11u);
  EXPECT_EQ(array_generation::same_optional::matching_default_0().value, 11u);
  EXPECT_EQ(array_generation::same_map::matching_default_0().value, 11u);
  EXPECT_EQ(array_generation::different_array{}.value, 0u);
  EXPECT_EQ(array_generation::different_optional{}.value, 0u);
  EXPECT_EQ(array_generation::different_map{}.value, 0u);
  EXPECT_EQ(array_generation::different_digest_extent{}.value, 0u);
  EXPECT_EQ(array_generation::same_digest_extent::matching_default_0().value, 11u);
  EXPECT_EQ(array_generation::same_opaque_digest::matching_default_0().value, 11u);
  EXPECT_EQ(collision_safe.elements, (std::array<std::uint32_t, 3>{1, 2, 3}));
}

// Nonliteral C++20 elements retain ordinary setters/value copies and zero-copy native ranges.
TEST(array_exposure, nonliteral_map_and_record_value_apis_remain_usable) {
  array_generation::map_facade maps;
  const std::map<std::string, std::uint32_t> input{{"first", 7}};
  maps.set(0, input);
  EXPECT_EQ(maps.get(0), input);
  EXPECT_EQ(maps.data(), maps.elements.data());
  EXPECT_EQ(maps.begin(), maps.elements.begin());
  EXPECT_EQ(maps.end(), maps.elements.end());
  EXPECT_EQ(&maps.values(), &maps.elements);
  auto copied = std::as_const(maps).get(0);
  copied["first"] = 19;
  EXPECT_EQ(maps[0].at("first"), 7u);
  auto indexed_copy = std::as_const(maps)[0];
  indexed_copy["first"] = 23;
  EXPECT_EQ(maps[0].at("first"), 7u);
  EXPECT_THROW(maps.set(2, input), std::out_of_range);
  EXPECT_EQ(maps[0], input);

  array_generation::record_facade records;
  array_generation::map_record record;
  record.values = input;
  records.set(1, record);
  EXPECT_EQ(records[1].values, input);
  EXPECT_EQ(records.data(), records.elements.data());
  EXPECT_EQ(records.begin(), records.elements.begin());
  EXPECT_EQ(records.end(), records.elements.end());
  EXPECT_EQ(&records.values(), &records.elements);
  auto record_copy = std::as_const(records).get(1);
  record_copy.values["first"] = 29;
  EXPECT_EQ(records[1].values.at("first"), 7u);
}

// Reject shapes and binding ownership that cannot satisfy the explicitly selected array contract.
TEST(array_exposure, invalid_schema_shapes_and_owned_bindings_reject) {
  for (const auto body : {
      "class item { public uint32 value (1) expose array(public); }",
      "class item { public array[3] uint32 value (1) expose array(public) dimensions(2, 2); }",
      "class item { public array[3] uint32 value (1); public constructor(uint32 a, uint32 b) { value = {a, b}; } }",
      "class item { public array[2] uint32 value (1); public constructor(uint32 a) { value = {a, a}; } }",
      "class item { public array[2] uint32 value (1) expose array(public); public function uint64 size() readonly; }",
      "class item { public array[2] uint32 value (1); public factory generated() { value = generate(a, b => checked<uint32>(0)); } }"}) {
    const auto source = std::string{"serializer version 1.17.0; "} + body;
    EXPECT_ANY_THROW(static_cast<void>(rohit::serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()))));
  }
  const std::string old_source = "serializer version 1.16.0; class item { public array[2] uint32 value (1) expose array(public); }";
  EXPECT_ANY_THROW(static_cast<void>(rohit::serializer::parser::parse(rohit::make_constant_stream(old_source.data(), old_source.size()))));
}

// Array convenience APIs neither add wire fields nor change the stored serialized representation.
TEST(array_exposure, wire_roundtrip_and_bounded_failure) {
  const array_exposure::triangle input{8, 5, 3};
  rohit::full_stream_auto_alloc output{};
  input.serialize_out<rohit::serializer::binary_none>(output);
  const auto bytes = output.current_offset();
  auto encoded = rohit::make_constant_stream(output.begin(), bytes);
  const auto result = rohit::serializer::deserialize_exact<array_exposure::triangle,
      rohit::serializer::binary_none>(encoded);
  EXPECT_EQ(result.indices, input.indices);
  auto truncated = rohit::make_constant_stream(output.begin(), bytes - 1);
  EXPECT_ANY_THROW((static_cast<void>(rohit::serializer::deserialize_exact<array_exposure::triangle,
      rohit::serializer::binary_none>(truncated))));
}

} // namespace
