// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <package_application_contract.hpp>

#include <rohit/package.hpp>
#include <rohit/package_profile.hpp>
#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace package = rohit::serializer::package;
namespace document = rohit::serializer::document;
namespace compression = rohit::serializer::compression;

// Keep native values alive while the generated input borrows their typed package occurrences.
struct fixture {
  contract::target target{};
  contract::source source{};
  contract::other_target other{};
  std::array<package::model_part_view<contract::target>, 1> targets{};
  std::array<package::model_part_view<contract::source>, 1> sources{};

  // Create one valid forward reference and a distinct optional model with a different key.
  fixture() {
    target.id = 7U;
    target.name = "target";
    source.target_id = target.id;
    other.id = 9U;
    other.name = "other";
    targets[0] = {&target, "/target.xml", "application/vnd.serializer.target+xml"};
    sources[0] = {&source, "/source.xml", "application/vnd.serializer.source+xml"};
  }

  // Borrow this fixture's model storage until encoding completes synchronously.
  contract::archive::input input() const { return {targets, sources, &other}; }
};

// Compose through the actual generated facade and return the independently readable ZIP bytes.
std::string encode(const contract::archive::input& input) {
  std::ostringstream output{std::ios::binary};
  contract::archive::write(output, input);
  return output.str();
}

// Validate package categories while keeping the failing source member visible.
template <typename Operation> void reject_reference(Operation operation) {
  try {
    operation();
    FAIL() << "Accepted invalid declared application reference";
  } catch (const package::error& error) {
    EXPECT_EQ(error.code, package::error_code::invalid_reference);
    EXPECT_FALSE(error.field_path.empty());
    EXPECT_FALSE(error.part.empty());
    EXPECT_FALSE(error.relationship_id.empty());
  }
}

// Deferred forward-reference validation retains the actual discovered source link and part name.
TEST(package_application, loaded_reference_failure_retains_discovered_link) {
  std::ostringstream output{std::ios::binary};
  package::opc_writer writer{output};
  const std::string target = "<target xmlns=\"urn:serializer:application-contract\" id=\"7\" name=\"target\"/>";
  const std::string source = "<source xmlns=\"urn:serializer:application-contract\" target_id=\"8\"/>";
  writer.add_part("/actual-target.xml", "application/vnd.serializer.target+xml",
      {reinterpret_cast<const std::uint8_t*>(target.data()), target.size()});
  writer.add_part("/actual-source.xml", "application/vnd.serializer.source+xml",
      {reinterpret_cast<const std::uint8_t*>(source.data()), source.size()});
  writer.add_relationship({"/", "target-link", "urn:serializer:targets", "/actual-target.xml"});
  writer.add_relationship({"/", "source-link", "urn:serializer:sources", "/actual-source.xml"});
  writer.finish();
  std::istringstream input{output.str(), std::ios::binary};
  try {
    static_cast<void>(contract::archive::load(input));
    FAIL() << "Accepted an unresolved forward reference";
  } catch (const package::error& error) {
    EXPECT_EQ(error.code, package::error_code::invalid_reference);
    EXPECT_EQ(error.part, "/actual-source.xml");
    EXPECT_EQ(error.relationship_id, "source-link");
    EXPECT_EQ(error.field_path, "sources.target_id");
    EXPECT_EQ(error.reference, "8");
  }
}

// Exercise default and runtime ZIP storage without changing native values or expected types.
TEST(package_application, profiles_and_generated_forward_references) {
  fixture values;
  auto input = values.input();
  package::profile_overrides overrides{{"targets", {.compression = compression::format::none}}};
  input.profiles = &overrides;
  const auto bytes = encode(input);
  std::istringstream archive{bytes, std::ios::binary};
  package::zip_reader physical{archive};
  const auto& entries = physical.entries();
  const auto target = std::find_if(entries.begin(), entries.end(),
      [](const auto& entry) { return entry.name == "target.xml"; });
  ASSERT_NE(target, entries.end());
  EXPECT_EQ(target->method, package::zip_method::stored);
  std::istringstream source{bytes, std::ios::binary};
  const auto loaded = contract::archive::load(source);
  ASSERT_EQ(loaded.value.targets.size(), 1U);
  ASSERT_EQ(loaded.value.sources.size(), 1U);
  EXPECT_EQ(loaded.value.sources.front().value.target_id, loaded.value.targets.front().value.id);
  overrides = {{"fixed", {.compression = compression::format::deflate}}};
  EXPECT_THROW(encode(input), std::invalid_argument);
  EXPECT_THROW(contract::archive::select_profiles({{"absent", {}}}), std::invalid_argument);
  EXPECT_THROW(contract::archive::select_profiles({{"targets", {.format = document::payload_format::json}}}),
      std::invalid_argument);
}

// Generated indexes reject duplicates, missing targets and targets from a different native model.
TEST(package_application, rejects_invalid_application_graphs_before_publication) {
  fixture values;
  values.other.id = values.target.id;
  reject_reference([&] { static_cast<void>(encode(values.input())); });
  values.other.id = 9U;
  values.source.target_id = 8U;
  reject_reference([&] { static_cast<void>(encode(values.input())); });
  values.source.target_id = values.other.id;
  reject_reference([&] { static_cast<void>(encode(values.input())); });
}

// Independent OPC payloads must match the declared root/type before their native codec can run.
TEST(package_application, rejects_wrong_root_and_explicit_type) {
  for (const auto xml : {std::string_view{"<other xmlns=\"urn:serializer:application-contract\" id=\"7\" name=\"bad\"/>"},
           std::string_view{"<target xmlns=\"urn:serializer:application-contract\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:n=\"urn:wrong\" xsi:type=\"n:target\" id=\"7\" name=\"bad\"/>"}}) {
    std::ostringstream output{std::ios::binary};
    package::opc_writer writer{output};
    writer.add_part("/target.xml", "application/vnd.serializer.target+xml",
        {reinterpret_cast<const std::uint8_t*>(xml.data()), xml.size()});
    writer.add_relationship({"/", "target", "urn:serializer:targets", "/target.xml"});
    writer.finish();
    std::istringstream source{output.str(), std::ios::binary};
    try {
      static_cast<void>(contract::archive::load(source));
      FAIL() << "Accepted a wrong package XML identity";
    } catch (const package::error& error) {
      EXPECT_EQ(error.code, package::error_code::codec);
      EXPECT_EQ(error.part, "/target.xml");
    }
  }
}

// Resource admission identifies the selected generated key field before XML encoding begins.
TEST(package_application, shared_reference_budget_retains_field_location) {
  fixture values;
  auto shared = std::make_shared<package::budget>(package::limits{.max_references = 0U});
  std::ostringstream output;
  try {
    contract::archive::write(output, values.input(), shared);
    FAIL() << "Reference admission did not exhaust";
  } catch (const package::error& error) {
    EXPECT_EQ(error.code, package::error_code::resource_limit);
    EXPECT_EQ(error.part, "/target.xml");
    EXPECT_EQ(error.field_path, "targets.id");
  }
}
} // namespace
