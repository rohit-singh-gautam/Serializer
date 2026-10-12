// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "generic_parameter_defaults.hpp"
#include <rohit/serializer_creator.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
bool reject_external_default{};
bool reject_frame_default{};
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

// A host frame's ordinary constructor, rather than aggregate field initialization, owns its default.
struct nonaggregate_frame {
  int value;
  // Supply a visible constexpr default with a value different from zero-initialized storage.
  constexpr nonaggregate_frame() : value{17} {}
  // Preserve an explicitly supplied placement without consulting the default constructor.
  constexpr explicit nonaggregate_frame(int input) : value{input} {}
};

// Observe each omitted placement's construction independently of its owning model's copies.
struct counted_frame {
  inline static unsigned default_constructions{};
  int value;
  // Produce a new identity marker for each default construction.
  counted_frame() : value{static_cast<int>(++default_constructions)} {}
  // Supply a placement without performing the observable default operation.
  constexpr explicit counted_frame(int input) : value{input} {}
};

// Keep the injected host failure distinguishable from generated validation errors.
struct frame_default_failure : std::runtime_error {
  // Identify the exact failure emitted by the host's ordinary default constructor.
  frame_default_failure() : std::runtime_error{"Host frame default construction failed"} {}
};

// A host default may throw, even when the generated direct binding otherwise cannot fail.
struct throwing_frame {
  int value{};
  // Reject omitted placement while preserving the original host failure category.
  throwing_frame() {
    if (reject_frame_default) { throw frame_default_failure{}; }
  }
  // Permit an explicitly supplied placement without the throwing default.
  constexpr explicit throwing_frame(int input) : value{input} {}
};

// Scope failure injection so assertion returns and unexpected exceptions restore the prior test state.
class frame_default_failure_scope {
  const bool previous_;
public:
  // Enable a runtime failure without promising that every default construction throws.
  frame_default_failure_scope() : previous_{std::exchange(reject_frame_default, true)} {}
  // Restore the prior mode before another test or normal authoring operation runs.
  ~frame_default_failure_scope() { reject_frame_default = previous_; }
};

// Compare actual encoded bytes without assuming a native object representation.
template <template <rohit::serializer::serialize_type> class Protocol, typename Value>
std::string encode(const Value& value) {
  rohit::full_stream_auto_alloc output;
  value.template serialize_out<Protocol>(output);
  return {reinterpret_cast<const char*>(output.begin()), output.current_offset()};
}

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

// Checked open constructors preserve constexpr success across exact, scalar and record inputs.
TEST(constructor_initialization, open_checked_constructors_bind_numeric_and_projected_inputs) {
  using integer_quantity = constructor_initialization::checked_quantity<std::uint16_t>;
  using floating_quantity = constructor_initialization::checked_quantity<double>;
  using integer_extent = constructor_initialization::checked_extent<std::uint16_t>;
  constexpr std::uint16_t exact_input{13};
  constexpr integer_quantity exact{exact_input};
  constexpr integer_quantity converted{17.0};
  constexpr floating_quantity input_record{19.0};
  constexpr integer_quantity projected{input_record};
  constexpr integer_extent distinct{input_record};
  static_assert(exact.value == exact_input && converted.value == 17);
  static_assert(projected.value == 19 && distinct.value == 19);
  static_assert(integer_quantity::create(exact_input).value == exact_input);
  static_assert(integer_quantity::create(17.0).value == 17);
  static_assert(integer_quantity::create(input_record).value == 19);
  static_assert(integer_extent::create(input_record).value == 19);
  static_assert(integer_quantity::from_value(17.0).value == 17);
  static_assert(integer_extent::from_quantity(input_record).value == 19);
  static_assert(constructor_initialization::checked_u16_quantity{23.0}.value == 23);
  static_assert(constructor_initialization::checked_u16_extent{input_record}.value == 19);
  static_assert(!std::is_convertible_v<double, integer_quantity>);
  static_assert(!std::is_convertible_v<floating_quantity, integer_extent>);
  static_assert(!std::is_constructible_v<integer_quantity, std::string>);
  static_assert(!noexcept(integer_quantity{exact_input}));
  static_assert(!noexcept(integer_extent{input_record}));
  EXPECT_EQ(round_trip(projected).value, input_record.value);
}

// Constructor and factory conversion paths reject the same exactness, range and finite failures.
TEST(constructor_initialization, open_checked_constructors_preserve_numeric_rejection_parity) {
  using integer_quantity = constructor_initialization::checked_quantity<std::uint16_t>;
  using floating_quantity = constructor_initialization::checked_quantity<double>;
  using integer_extent = constructor_initialization::checked_extent<std::uint16_t>;
  const auto beyond_storage = static_cast<double>(std::numeric_limits<std::uint16_t>::max()) + 1;
  for (const double input : {-1.0, 1.5, beyond_storage,
                            std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()}) {
    floating_quantity record;
    record.value = input;
    EXPECT_THROW(static_cast<void>(integer_quantity{input}), std::domain_error);
    EXPECT_THROW(static_cast<void>(integer_quantity::from_value(input)), std::domain_error);
    EXPECT_THROW(static_cast<void>(integer_quantity{record}), std::domain_error);
    EXPECT_THROW(static_cast<void>(integer_extent{record}), std::domain_error);
    EXPECT_THROW(static_cast<void>(integer_extent::from_quantity(record)), std::domain_error);
    EXPECT_THROW(static_cast<void>(constructor_initialization::checked_u16_quantity{input}), std::domain_error);
  }
  const auto beyond_exact_double = (std::uint64_t{1} << std::numeric_limits<double>::digits) + 1;
  EXPECT_THROW(static_cast<void>(floating_quantity{beyond_exact_double}), std::domain_error);
  EXPECT_THROW(static_cast<void>(floating_quantity::from_value(beyond_exact_double)), std::domain_error);
  const auto nonfinite = std::numeric_limits<double>::infinity();
  EXPECT_THROW(static_cast<void>(floating_quantity{nonfinite}), std::domain_error);
  EXPECT_THROW(static_cast<void>(floating_quantity::from_value(nonfinite)), std::domain_error);
}

// Numeric checking must not add default construction, moving or assignment to other owned inputs.
TEST(constructor_initialization, open_checked_constructors_keep_copy_only_coordinates_direct) {
  using owner = constructor_initialization::checked_coordinate<std::uint16_t, copy_only_input>;
  constexpr copy_only_input coordinate{29};
  constexpr owner constructed{31.0, coordinate};
  static_assert(constructed.coordinate.value == 29 && constructed.value == 31);
  static_assert(owner::create(31.0, coordinate).coordinate.value == 29);
  static_assert(owner::from_value(31.0, coordinate).value == 31);
  EXPECT_THROW(static_cast<void>(owner{1.5, coordinate}), std::domain_error);
  EXPECT_EQ(coordinate.value, 29);
}

// Retaining dependent checked targets must still require numeric proofs and exact destination identity.
TEST(constructor_initialization, open_checked_constructors_retain_type_admission_checks) {
  EXPECT_NO_THROW(parse("serializer version 1.14.0; "
      "class quantity<T: (integral || floating_point) && !bool> { public T value; "
      "public constructor<Input: (integral || floating_point) && !bool>(Input input) {"
      "value=checked<T>(input); } }"));
  for (const std::string_view source : {
      "serializer version 1.14.0; class quantity<T> { public T value; public constructor(T input) {value=checked<T>(input);} }",
      "serializer version 1.14.0; class quantity<T: integral> { public T value; public constructor<Input: bool || !bool>(Input input) {value=checked<T>(input);} }",
      "serializer version 1.14.0; class quantity<T: integral> { public T value; public constructor(uint32 input) {value=checked<Unknown>(input);} }",
      "serializer version 1.14.0; class quantity<T: integral> { public T value; public constructor(uint32 input) {value=checked<double>(input);} }"}) {
    EXPECT_ANY_THROW(parse(source)) << source;
  }
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

// Open owner parameters use their host default constructor and keep explicitly supplied inputs owning.
TEST(constructor_initialization, owner_parameter_defaults_support_nonaggregate_and_copy_only_frames) {
  using owner = constructor_initialization::owner_default<nonaggregate_frame>;
  static_assert(!std::is_aggregate_v<nonaggregate_frame>);
  constexpr owner omitted{1, 2};
  static_assert(omitted.a == 1 && omitted.b == 2 && omitted.extent.value == 17);
  static_assert(owner::create(1, 2).extent.value == 17);
  static_assert(owner::create_default_2(1, 2).extent.value == 17);
  static_assert(owner::from_frame(1, 2).extent.value == 17);
  static_assert(owner::from_frame_default_2(1, 2).extent.value == 17);
  constexpr nonaggregate_frame custom{23};
  static_assert(owner{1, 2, custom}.extent.value == 23);
  static_assert(owner::create(1, 2, custom).extent.value == 23);
  static_assert(owner::from_frame(1, 2, custom).extent.value == 23);

  using copy_owner = constructor_initialization::owner_default<copy_only_input>;
  static_assert(!std::is_default_constructible_v<copy_only_input>);
  static_assert(!std::is_move_constructible_v<copy_only_input>);
  static_assert(!std::is_copy_assignable_v<copy_only_input>);
  constexpr copy_only_input input{31};
  constexpr copy_owner supplied{1, 2, input};
  static_assert(supplied.extent.value == 31);
  static_assert(copy_owner::create(1, 2, input).extent.value == 31);
  static_assert(copy_owner::from_frame(1, 2, input).extent.value == 31);
  using scalar_owner = constructor_initialization::owner_default<std::uint32_t>;
  static_assert(scalar_owner::create(1, 2).extent == 0);
  using array_owner = constructor_initialization::owner_default<std::array<int, 2>>;
  static_assert(array_owner::from_frame(1, 2).extent == std::array<int, 2>{});
  EXPECT_EQ(supplied.extent.value, 31);
}

// Every omitted call constructs one fresh frame; supplied calls do not evaluate its default.
TEST(constructor_initialization, owner_parameter_defaults_are_evaluated_exactly_once_per_omission) {
  using owner = constructor_initialization::owner_default<counted_frame>;
  counted_frame::default_constructions = 0;
  auto direct = owner{1, 2};
  EXPECT_EQ(direct.extent.value, 1);
  EXPECT_EQ(counted_frame::default_constructions, 1u);
  const auto created = owner::create(1, 2);
  EXPECT_EQ(created.extent.value, 2);
  EXPECT_EQ(counted_frame::default_constructions, 2u);
  const auto wrapped = owner::create_default_2(1, 2);
  EXPECT_EQ(wrapped.extent.value, 3);
  EXPECT_EQ(counted_frame::default_constructions, 3u);
  const auto factory = owner::from_frame(1, 2);
  EXPECT_EQ(factory.extent.value, 4);
  EXPECT_EQ(counted_frame::default_constructions, 4u);
  const auto factory_wrapper = owner::from_frame_default_2(1, 2);
  EXPECT_EQ(factory_wrapper.extent.value, 5);
  EXPECT_EQ(counted_frame::default_constructions, 5u);
  direct.extent.value = -1;
  EXPECT_EQ(created.extent.value, 2);
  EXPECT_EQ(factory_wrapper.extent.value, 5);

  const counted_frame supplied{41};
  EXPECT_EQ((owner{1, 2, supplied}).extent.value, 41);
  EXPECT_EQ(owner::create(1, 2, supplied).extent.value, 41);
  EXPECT_EQ(owner::from_frame(1, 2, supplied).extent.value, 41);
  EXPECT_EQ(counted_frame::default_constructions, 5u);
}

// Generic default failures propagate through native constructors, full APIs and generated wrappers.
TEST(constructor_initialization, owner_parameter_default_failures_do_not_acquire_false_noexcept) {
  using owner = constructor_initialization::owner_default<throwing_frame>;
  static_assert(!noexcept(owner{1, 2}));
  static_assert(!noexcept(owner::create(1, 2)));
  static_assert(!noexcept(owner::create_default_2(1, 2)));
  static_assert(!noexcept(owner::from_frame(1, 2)));
  static_assert(!noexcept(owner::from_frame_default_2(1, 2)));
  EXPECT_EQ((owner{1, 2}).extent.value, 0);
  EXPECT_EQ(owner::create_default_2(1, 2).extent.value, 0);
  {
    const frame_default_failure_scope failure;
    EXPECT_THROW((owner{1, 2}), frame_default_failure);
    EXPECT_THROW(static_cast<void>(owner::create(1, 2)), frame_default_failure);
    EXPECT_THROW(static_cast<void>(owner::create_default_2(1, 2)), frame_default_failure);
    EXPECT_THROW(static_cast<void>(owner::from_frame(1, 2)), frame_default_failure);
    EXPECT_THROW(static_cast<void>(owner::from_frame_default_2(1, 2)), frame_default_failure);
    constexpr throwing_frame supplied{47};
    EXPECT_EQ((owner{1, 2, supplied}).extent.value, 47);
    EXPECT_EQ(owner::from_frame(1, 2, supplied).extent.value, 47);
  }
  EXPECT_EQ(owner::from_frame_default_2(1, 2).extent.value, 0);
}

// Parameter omission neither duplicates authoring hooks nor replays them during fresh decoding.
TEST(constructor_initialization, owner_parameter_defaults_validate_then_initialize_only_for_authoring) {
  using owner = constructor_initialization::initialized_owner_default<counted_frame>;
  generic_default_host::registrations = 0;
  counted_frame::default_constructions = 0;
  const owner codec_default{};
  EXPECT_EQ(codec_default.execution, 0u);
  EXPECT_EQ(counted_frame::default_constructions, 1u);
  EXPECT_EQ(generic_default_host::registrations, 0u);
  const owner direct{1, 2};
  EXPECT_EQ(direct.execution, 1u);
  EXPECT_EQ(direct.extent.value, 2);
  const auto created = owner::create_default_2(1, 2);
  EXPECT_EQ(created.execution, 2u);
  EXPECT_EQ(created.extent.value, 3);
  const auto full_created = owner::create(1, 2);
  EXPECT_EQ(full_created.execution, 3u);
  EXPECT_EQ(full_created.extent.value, 4);
  EXPECT_THROW((owner{0, 2}), std::domain_error);
  EXPECT_EQ(counted_frame::default_constructions, 5u);
  EXPECT_THROW(static_cast<void>(owner::create(1, 0, counted_frame{59})), std::domain_error);
  EXPECT_EQ(counted_frame::default_constructions, 5u);
  EXPECT_EQ(generic_default_host::registrations, 3u);
  const auto copied = full_created;
  EXPECT_EQ(copied.execution, 3u);
  EXPECT_EQ(generic_default_host::registrations, 3u);
  using failing_owner = constructor_initialization::initialized_owner_default<throwing_frame>;
  {
    const frame_default_failure_scope failure;
    EXPECT_THROW(static_cast<void>(failing_owner::create_default_2(1, 2)), frame_default_failure);
  }
  EXPECT_EQ(generic_default_host::registrations, 3u);

  using concrete = constructor_initialization::initialized_area_owner;
  const concrete authored{1, 2};
  EXPECT_EQ(authored.extent.width, 3u);
  EXPECT_EQ(authored.execution, 4u);
  const auto decoded = round_trip(authored);
  EXPECT_EQ(decoded.a, 1u);
  EXPECT_EQ(decoded.extent.width, 3u);
  EXPECT_EQ(decoded.execution, 0u);
  const std::string_view partial = "{\"a\":9}";
  const auto input = rohit::make_constant_stream(partial.data(), partial.size());
  const auto fresh = rohit::serializer::deserialize_exact<concrete, rohit::serializer::json>(input);
  EXPECT_EQ(fresh.a, 9u);
  EXPECT_EQ(fresh.b, 0u);
  EXPECT_EQ(fresh.extent.width, 3u);
  EXPECT_EQ(fresh.extent.height, 4u);
  EXPECT_EQ(fresh.execution, 0u);
  EXPECT_EQ(generic_default_host::registrations, 4u);
}

// Adding an owner-generic parameter default preserves the existing concrete model's four wire formats.
TEST(constructor_initialization, owner_parameter_defaults_preserve_existing_wire_and_fresh_decode) {
  using owner = constructor_initialization::default_area_owner;
  const constructor_initialization::box legacy{1, 2};
  const owner current{1, 2};
  EXPECT_EQ(encode<rohit::serializer::binary_none>(current), encode<rohit::serializer::binary_none>(legacy));
  EXPECT_EQ(encode<rohit::serializer::binary_integer>(current), encode<rohit::serializer::binary_integer>(legacy));
  EXPECT_EQ(encode<rohit::serializer::binary_string>(current), encode<rohit::serializer::binary_string>(legacy));
  EXPECT_EQ(encode<rohit::serializer::json>(current), encode<rohit::serializer::json>(legacy));
  const owner overridden{1, 2, constructor_initialization::area{11, 12}};
  EXPECT_EQ(round_trip(overridden).extent.width, 11u);
  const std::string_view empty_record = "{}";
  const auto input = rohit::make_constant_stream(empty_record.data(), empty_record.size());
  auto first = rohit::serializer::deserialize_exact<owner, rohit::serializer::json>(input);
  const owner second{};
  first.extent.width = 67;
  EXPECT_EQ(second.a, 0u);
  EXPECT_EQ(second.extent.width, 3u);
  EXPECT_EQ(second.extent.height, 4u);
}

// Only class-owned type parameters gain this spelling; unrelated default grammar remains unchanged.
TEST(constructor_initialization, owner_parameter_defaults_preserve_scope_and_literal_boundaries) {
  const auto accepted = parse("serializer version 1.16.0; class box<Frame> { public Frame value; "
      "public constructor(Frame input = { /* fresh */ }) { value=input; } "
      "public factory from_frame(Frame input = {\n}) { value=input; } }");
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(accepted, options));
  for (const auto text : {
      "serializer version 1.15.0; class box<Frame> { public Frame value; public constructor(Frame input = {}) {value=input;} }",
      "serializer version 1.15.0; class box<Frame> { public Frame value; public factory from_frame(Frame input = {}) {value=input;} }",
      "serializer version 1.16.0; class box<Frame> { public Frame value; public constructor(Frame input = {1}) {value=input;} }",
      "serializer version 1.16.0; class box<Frame> { public Frame value; public constructor(Frame input = {}, uint32 required) {value=input;} }",
      "serializer version 1.16.0; class box { public uint32 value; public constructor<Input: integral>(Input input = {}) {value=input;} }",
      "serializer version 1.16.0; class box { public uint32 value; public factory from_input<Input: integral>(Input input = {}) {value=input;} }",
      "serializer version 1.16.0; class box<Frame> { public array[2] Frame values; public constructor(array[2] Frame input = {}) {values=input;} }",
      "serializer version 1.16.0; class box { public array[2] uint32 values; public constructor(array[2] uint32 input = {}) {values=input;} }",
      "serializer version 1.16.0; class box { public uint32 value; public constructor(uint32 input = {}) {value=input;} }",
      "serializer version 1.16.0; class box<Frame> { public function Frame read(Frame input = {}) readonly expression(input); }"}) {
    EXPECT_THROW(parse(text), std::exception) << text;
  }
}

// Comment punctuation is trivia while both supported empty-default contracts retain their own gates.
TEST(constructor_initialization, empty_parameter_defaults_ignore_comment_delimiters_and_quotes) {
  for (const std::string_view literal : {"{ /* { */ }", "{ /* } */ }",
      "{ /* unpaired \" ' , ) ( */ }", "{ // { } , ) ( \" '\n}"}) {
    SCOPED_TRACE(literal);
    for (const auto declaration : {"constructor", "factory from_frame"}) {
      SCOPED_TRACE(declaration);
      const auto owner = std::string{"serializer version 1.16.0; class owner<Frame> { public Frame value; public "} +
          declaration + "(Frame input = " + std::string{literal} + ") { value=input; } }";
      EXPECT_NO_THROW(parse(owner));
      const auto record = std::string{"serializer version 1.15.0; class frame { public uint32 marker {17}; } "
          "class owner { public frame value; public "} + declaration +
          "(frame input = " + std::string{literal} + ") { value=input; } }";
      EXPECT_NO_THROW(parse(record));
    }
  }
}

// Comment skipping keeps the existing byte and aggregate-depth admission limits intact.
TEST(constructor_initialization, commented_parameter_defaults_retain_capture_resource_bounds) {
  constexpr std::size_t maximum_default_bytes = 4096;
  constexpr std::size_t comment_wrapper_bytes = std::string_view{"{/**/}"}.size();
  const std::string prefix = "serializer version 1.16.0; class owner<Frame> { public Frame value; "
      "public constructor(Frame input = ";
  const std::string suffix = ") { value=input; } }";
  const auto maximum = "{/*" + std::string(maximum_default_bytes - comment_wrapper_bytes, 'x') + "*/}";
  ASSERT_EQ(maximum.size(), maximum_default_bytes);
  EXPECT_NO_THROW(parse(prefix + maximum + suffix));
  const auto oversized = "{/*" + std::string(maximum_default_bytes - comment_wrapper_bytes + 1, 'x') + "*/}";
  try {
    static_cast<void>(parse(prefix + oversized + suffix));
    FAIL() << "Oversized commented default was accepted";
  } catch (const std::exception& error) {
    EXPECT_NE(std::string_view{error.what()}.find("exceeds 4096 bytes"), std::string_view::npos);
  }
  constexpr std::size_t excessive_aggregate_depth = 33;
  EXPECT_NO_THROW(parse(prefix + "{/*" + std::string(excessive_aggregate_depth, '{') + "*/}" + suffix));
  try {
    static_cast<void>(parse(prefix + std::string(excessive_aggregate_depth, '{') +
        std::string(excessive_aggregate_depth, '}') + suffix));
    FAIL() << "Excessively nested default was accepted";
  } catch (const std::exception& error) {
    EXPECT_NE(std::string_view{error.what()}.find("at most 32 levels"), std::string_view::npos);
  }
  EXPECT_THROW(parse(prefix + "{/* unterminated"), std::exception);
}

// Existing positional literals keep punctuation and comment spellings inside quoted string values.
TEST(constructor_initialization, parameter_capture_preserves_quoted_aggregate_literals) {
  const auto schema = parse("serializer version 1.14.0; class frame { public string text; public char marker; } "
      "class owner { public frame value; public constructor(frame input = {\"/* { } ' , ) */\", '('}) { value=input; } }");
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(schema, options));
}

// A concrete owning schema record retains the established portable fresh-default implementation.
TEST(constructor_initialization, substituted_record_owner_defaults_generate_in_every_backend) {
  const auto schema = parse("serializer version 1.16.0; class frame { public uint32 marker {17}; } "
      "class owner<Frame> { public Frame placement; "
      "public constructor(uint32 id, Frame input = {}) { placement=input; } "
      "public factory from_frame(uint32 id, Frame input = {}) { placement=input; } } "
      "instantiate placed_owner = owner<frame>;");
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(schema, options));
  EXPECT_FALSE(rohit::serializer::writer::java::generate(schema, "Schema", {}).empty());
  for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
    EXPECT_FALSE(rohit::serializer::writer::portable::generate(schema, language, "Schema", {}).empty()) << language;
  }
}

// Generic host defaults must not silently become unsupported portable literal or base-record defaults.
TEST(constructor_initialization, substituted_unsupported_owner_defaults_diagnose_non_cpp_outputs) {
  for (const auto type : {"uint32", "array<uint32>", "derived"}) {
    SCOPED_TRACE(type);
    const auto schema = parse(std::string{"serializer version 1.16.0; class base { public uint32 marker {17}; } "
        "class derived : public base {} class owner<Frame> { public Frame value; "
        "public constructor(Frame input = {}) { value=input; } "
        "public factory from_frame(Frame input = {}) { value=input; } } instantiate concrete_owner = owner<"} + type + ">;");
    rohit::serializer::writer::cpp_options options;
    options.format = false;
    EXPECT_NO_THROW(rohit::serializer::writer::cpp::generate(schema, options));
    EXPECT_THROW(rohit::serializer::writer::java::generate(schema, "Schema", {}), std::exception);
    for (const auto language : {"js", "typescript", "go", "csharp", "rust", "python", "swift", "kotlin", "c"}) {
      EXPECT_THROW(rohit::serializer::writer::portable::generate(schema, language, "Schema", {}), std::exception) << language;
    }
  }
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
