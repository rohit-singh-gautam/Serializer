// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "readonly_behavior.hpp"
#include <rohit/serializer_creator.hpp>

#include <gtest/gtest.h>
#include <limits>
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
} // namespace

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
