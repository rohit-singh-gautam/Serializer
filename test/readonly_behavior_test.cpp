// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "readonly_behavior.hpp"
#include <rohit/serializer_creator.hpp>

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
// Execute the inferred constant methods, including reached invalid index and reduction-bound failures.
constexpr bool qualify_readonly_constants() {
  readonly_identity value{};
  value.identity = std::numeric_limits<std::uint64_t>::max();
  value.enabled = true;
  value.phase = readonly_phase::ready;
  value.values = {1, 2, std::numeric_limits<std::uint64_t>::max()};
  return value.matches(std::numeric_limits<std::uint64_t>::max()) &&
      !value.matches(std::numeric_limits<std::uint64_t>::max() - 1) && value.ready() &&
      value.indexed(2, std::numeric_limits<std::uint64_t>::max()) &&
      !value.indexed(-1, 1) && !value.indexed(3, 1) && value.positive() && !value.too_many();
}
static_assert(qualify_readonly_constants());
static_assert(noexcept(std::declval<const readonly_identity&>().matches(0)));
static_assert(noexcept(std::declval<const readonly_identity&>().ready()));
static_assert(noexcept(std::declval<const readonly_identity&>().indexed(0, 0)));
static_assert(noexcept(std::declval<const readonly_identity&>().guarded(false, 0)));
static_assert(noexcept(std::declval<const readonly_nonliteral&>().matches(0)));

// Qualify generated checked arithmetic and total predicates using the C++20 evaluation rules.
constexpr bool qualify_readonly_arithmetic() {
  readonly_arithmetic value{};
  readonly_identity predicate{};
  return value.sum(3.0) == 15.0 && value.difference(3.0) == 9.0 &&
      value.scaled(2.0) == 24.0 && value.divided(3.0) == 4.0 &&
      value.signed_value(true) == 12.0 && value.signed_value(false) == -12.0 &&
      value.converted(42) == 42.0 && value.scaled_positive(2.0) &&
      !value.scaled_positive(-2.0) && predicate.ratio(1.0) && !predicate.ratio(0.0) &&
      !predicate.ratio(-0.0) && !predicate.ratio(std::numeric_limits<double>::infinity()) &&
      predicate.finite(std::numeric_limits<double>::max()) &&
      !predicate.finite(std::numeric_limits<double>::infinity()) &&
      !predicate.finite(std::numeric_limits<double>::quiet_NaN());
}
static_assert(qualify_readonly_arithmetic());
static_assert(!noexcept(std::declval<const readonly_arithmetic&>().sum(0.0)));
static_assert(!noexcept(std::declval<const readonly_arithmetic&>().divided(0.0)));
static_assert(noexcept(std::declval<const readonly_arithmetic&>().converted(0)));
static_assert(noexcept(std::declval<const readonly_arithmetic&>().scaled_positive(0.0)));
} // namespace

// The same generated methods retain runtime checks for nonfinite input, overflow and signed zero.
TEST(readonly_behavior, checked_runtime_arithmetic) {
  readonly_arithmetic value{};
  EXPECT_EQ(value.sum(3.0), 15.0);
  EXPECT_EQ(value.difference(3.0), 9.0);
  EXPECT_EQ(value.scaled(2.0), 24.0);
  EXPECT_EQ(value.divided(3.0), 4.0);
  EXPECT_THROW(static_cast<void>(value.divided(0.0)), std::domain_error);
  EXPECT_THROW(static_cast<void>(value.divided(-0.0)), std::domain_error);
  EXPECT_THROW(static_cast<void>(value.sum(std::numeric_limits<double>::quiet_NaN())), std::domain_error);
  EXPECT_THROW(static_cast<void>(value.scaled(std::numeric_limits<double>::infinity())), std::domain_error);
  EXPECT_THROW(static_cast<void>(value.scaled(std::numeric_limits<double>::max())), std::domain_error);
  EXPECT_FALSE(value.scaled_positive(std::numeric_limits<double>::infinity()));
  EXPECT_FALSE(value.scaled_positive(std::numeric_limits<double>::max()));
}

// Runtime-only C++20 math predicates report reached domain failure through false without exceptions.
TEST(readonly_behavior, total_runtime_predicates) {
  readonly_identity value{};
  EXPECT_TRUE(value.finite(1.0));
  EXPECT_FALSE(value.finite(std::numeric_limits<double>::infinity()));
  EXPECT_FALSE(value.finite(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_TRUE(value.guarded(false, std::numeric_limits<double>::quiet_NaN()));
  EXPECT_FALSE(value.guarded(true, -1.0));
  EXPECT_TRUE(value.guarded(true, 4.0));
  EXPECT_FALSE(value.ratio(0.0));
  EXPECT_FALSE(value.ratio(-0.0));
  EXPECT_FALSE(value.ratio(std::numeric_limits<double>::infinity()));
  EXPECT_TRUE(value.ratio(1.0));
  readonly_nonliteral nonliteral{};
  nonliteral.identity = std::numeric_limits<std::uint64_t>::max();
  EXPECT_TRUE(nonliteral.matches(std::numeric_limits<std::uint64_t>::max()));
  EXPECT_FALSE(nonliteral.matches(std::numeric_limits<std::uint64_t>::max() - 1));
}

// A borrowed nonliteral receiver must retain the inferred declaration even when no instance is a constant.
TEST(readonly_behavior, nonliteral_receiver_retains_qualifiers) {
  constexpr std::string_view source = R"SCHEMA(serializer version 1.8.0;
class nonliteral {
  public uint64 identity;
  public map<uint64, uint64> cache;
  public function bool matches(uint64 candidate) readonly expression(identity == candidate);
})SCHEMA";
  const auto statements = rohit::serializer::parser::parse(
      rohit::make_constant_stream(source.data(), source.size()));
  rohit::serializer::writer::cpp_options options;
  options.format = false;
  const auto generated = rohit::serializer::writer::cpp::generate(statements, options);
  EXPECT_NE(generated.find("constexpr bool matches([[maybe_unused]] ::std::uint64_t candidate) const noexcept"), std::string::npos);
}

// Projected readonly calls retain exact record values and evaluate only the selected branch.
TEST(readonly_behavior, projected_calls_and_lazy_record_selection) {
  projected_identity_probe value{};
  value.preferred.value = std::numeric_limits<std::uint64_t>::max();
  value.preferred.label = "selected payload";
  value.fallback.value = 7;
  EXPECT_EQ(value.selected().value, std::numeric_limits<std::uint64_t>::max());
  const auto selected = value.selected();
  value.preferred.label = "changed source";
  EXPECT_EQ(selected.label, "selected payload");
  EXPECT_TRUE(value.matches(std::numeric_limits<std::uint64_t>::max()));
  EXPECT_EQ(value.guarded(true).value, std::numeric_limits<std::uint64_t>::max());
  EXPECT_THROW(static_cast<void>(value.guarded(false)), std::domain_error);
  value.preferred.value = 0;
  EXPECT_EQ(value.selected().value, 7u);
  value.child.value = 3;
  EXPECT_TRUE(value.inherited());
  EXPECT_TRUE(value.items_valid());
  value.items.push_back({9, {}});
  EXPECT_TRUE(value.items_valid());
  value.items.push_back({0, {}});
  EXPECT_FALSE(value.items_valid());
}

// Generic projected calls substitute exact argument/result types through public bases.
TEST(readonly_behavior, projected_generic_receiver_signatures) {
  projected_generic_probe<std::uint64_t> value{};
  value.item.value = std::numeric_limits<std::uint64_t>::max();
  value.child.value = 7;
  EXPECT_EQ(value.read(), std::numeric_limits<std::uint64_t>::max());
  EXPECT_TRUE(value.matches(std::numeric_limits<std::uint64_t>::max()));
  EXPECT_EQ(value.inherited_read(), 7u);
}
