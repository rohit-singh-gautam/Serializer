// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <opc_profile.hpp>
#include <rohit/package.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace package = rohit::serializer::package;

// Create a small model whose placement is derived entirely from inline defaults.
profile::model fixture_model() {
  profile::model value{};
  value.name = "sample & geometry";
  profile::point point{};
  point.x = 1.0;
  point.y = 2.0;
  point.z = 3.0;
  value.points.push_back(point);
  return value;
}
} // namespace

// The generated facade owns complete assembly, relationship discovery and typed decoding.
TEST(opc_profile, complete_package_with_dynamic_attachments_round_trips) {
  const auto model = fixture_model();
  constexpr std::array<std::uint8_t, 4U> pixels{0U, 1U, 254U, 255U};
  const std::array attachments{
      package::binary_part_view{pixels, "/Textures/a.bin", "application/octet-stream", 0U}};
  const profile::archive::input input{&model, attachments};
  std::ostringstream output{std::ios::binary};
  profile::archive::write(output, input);
  std::istringstream source{output.str(), std::ios::binary};
  const auto decoded = profile::archive::load(source);
  EXPECT_EQ(decoded.value.main.name, model.name);
  ASSERT_EQ(decoded.value.main.points.size(), 1U);
  EXPECT_EQ(decoded.value.main.points.front().z, 3.0);
  ASSERT_EQ(decoded.value.attachments.size(), 1U);
  EXPECT_EQ(decoded.value.attachments.front().metadata.name, "/Textures/a.bin");
  EXPECT_EQ(decoded.value.attachments.front().bytes,
      (std::vector<std::uint8_t>{pixels.begin(), pixels.end()}));
}

// Invalid role cardinality and unbound parents reject before a complete output is exposed.
TEST(opc_profile, required_model_and_source_occurrences_are_checked) {
  std::ostringstream output{std::ios::binary};
  EXPECT_THROW(profile::archive::write(output, {}), package::error);
  EXPECT_TRUE(output.str().empty());
  const auto model = fixture_model();
  const std::array attachments{
      package::binary_part_view{{}, "/bad.bin", "application/octet-stream", 1U}};
  EXPECT_THROW(profile::archive::write(output, {&model, attachments}), package::error);
}

// XML, ZIP and metadata all charge the caller's single cumulative operation budget.
TEST(opc_profile, shared_budget_and_deadline_propagate_to_generated_xml) {
  const auto model = fixture_model();
  package::limits limits{};
  limits.max_decoder_allocation_bytes = 1U;
  auto shared = std::make_shared<package::budget>(limits);
  std::ostringstream output{std::ios::binary};
  EXPECT_THROW(profile::archive::write(output, {&model, {}}, shared), package::error);
  limits = {};
  limits.deadline = std::chrono::steady_clock::now();
  shared = std::make_shared<package::budget>(limits);
  EXPECT_THROW(profile::archive::write(output, {&model, {}}, shared), package::error);
}
