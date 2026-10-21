// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
namespace package = rohit::serializer::package;

// Independent literal expectations follow the normative composition algorithm, including reserved escaping.
TEST(pack_iri, fifth_edition_composition_and_single_decode_preserve_outer_location) {
  const auto iri = package::compose_pack_iri("http://myalias:pswr@www.my.com/containers.aspx?my.container#removed",
      "/c/d/bar.xml");
  EXPECT_EQ(iri, "pack://http%3A,,myalias%3Apswr%40www.my.com,containers.aspx%3Fmy.container/c/d/bar.xml");
  const auto address = package::parse_pack_iri(iri);
  EXPECT_EQ(address.package_iri, "http://myalias:pswr@www.my.com/containers.aspx?my.container");
  EXPECT_EQ(address.part_name, "/c/d/bar.xml");
  EXPECT_FALSE(address.query);
  EXPECT_FALSE(address.fragment);
  const auto escaped = package::compose_pack_iri("https://example.org/a,b%20c?x=%2F", "/a%3Ab.xml");
  EXPECT_EQ(escaped, "pack://https%3A,,example.org,a%2Cb%2520c%3Fx=%252F/a%3Ab.xml");
  EXPECT_EQ(package::parse_pack_iri(escaped).package_iri, "https://example.org/a,b%20c?x=%2F");
  EXPECT_EQ(package::compose_pack_iri("https://[2001:db8::1]:443/a"),
      "pack://https%3A,,%5B2001%3Adb8%3A%3A1%5D%3A443,a/");
}

// Whole-package and outer selector forms have distinct storage but the same addressed package resource.
TEST(pack_iri, package_roots_empty_selectors_unicode_outer_iris_and_scheme_policy) {
  constexpr std::string_view no_path = "pack://https%3A,,example.org,package.opc";
  const auto first = package::parse_pack_iri(no_path);
  const auto second = package::parse_pack_iri(std::string{no_path} + "/?#");
  EXPECT_TRUE(first.part_name.empty());
  EXPECT_EQ(first.package_iri, second.package_iri);
  ASSERT_TRUE(second.query);
  ASSERT_TRUE(second.fragment);
  EXPECT_TRUE(second.query->empty());
  EXPECT_TRUE(second.fragment->empty());
  EXPECT_EQ(package::compose_pack_iri(second), std::string{no_path} + "/?#");
  const auto unicode = package::compose_pack_iri("https://example.org/\xe6\xa8\xa1\xe5\x9e\x8b.opc", "/a.xml");
  EXPECT_EQ(package::parse_pack_iri(unicode).package_iri, "https://example.org/\xe6\xa8\xa1\xe5\x9e\x8b.opc");
  const auto exact_packages = [](std::string_view left, std::string_view right) { return left == right; };
  EXPECT_TRUE(package::equivalent_pack_iri(no_path, std::string{no_path} + "/?ignored#selector", exact_packages));
  EXPECT_TRUE(package::equivalent_pack_iri(std::string{no_path} + "/A%3ab.xml",
      std::string{no_path} + "/a%3Ab.XML#section", exact_packages));
  EXPECT_FALSE(package::equivalent_pack_iri(std::string{no_path} + "/a.xml",
      std::string{no_path} + "/b.xml", exact_packages));
  EXPECT_THROW(static_cast<void>(package::equivalent_pack_iri(no_path, no_path, {})), package::error);
}

// Invalid external addresses cannot turn into internal lookup, filesystem access or nested package fetching.
TEST(pack_iri, malformed_legacy_nested_and_unsupported_addressing_rejects) {
  constexpr std::array invalid{
      "pack:", "pack:///a.xml", "pack://", "https://example.org/a.xml", "pack://relative/a.xml",
      "pack://http:,,example.org/a.xml", "pack://http%3c,,example.org/a.xml", "pack://http%3A,,example.org/a//b",
      "pack://http%3A,,example.org/a/../b", "pack://http%3A,,example.org/a.",
      "pack://http%3A,,example.org/a%2Fb", "pack://http%3A,,example.org/a%41b",
      "pack://http%3A,,example.org/a%", "pack://http%3A,,example.org%23fragment/a.xml",
      "pack://pack%3A,,https%253A,,example.org/a.xml", "pack://http%3A,,example.org/\xc3\xa9.xml",
      "pack://http%3A,,example.org/%C3%A9.xml", "pack://http%3A,,example.org/a.xml#bad fragment"};
  for (const auto input : invalid) {
    EXPECT_THROW(static_cast<void>(package::parse_pack_iri(input)), package::error) << input;
  }
  for (const auto input : {"relative.opc", "https://example.org/bad path", "pack://https%3A,,example.org/"}) {
    EXPECT_THROW(static_cast<void>(package::compose_pack_iri(input)), package::error) << input;
  }
}

// These literal normal/abnormal vectors are RFC3986 section5.4, with only its base scheme/authority changed.
TEST(pack_iri, complete_rfc3986_relative_resolution_inventory) {
  constexpr std::string_view base = "pack://https%3A,,example.org,package.opc/b/c/d;p?q";
  constexpr std::string_view prefix = "pack://https%3A,,example.org,package.opc";
  constexpr std::pair<std::string_view, std::string_view> vectors[]{
      {"g", "/b/c/g"}, {"./g", "/b/c/g"}, {"g/", "/b/c/g/"}, {"/g", "/g"},
      {"?y", "/b/c/d;p?y"}, {"g?y", "/b/c/g?y"}, {"#s", "/b/c/d;p?q#s"},
      {"g#s", "/b/c/g#s"}, {"g?y#s", "/b/c/g?y#s"}, {";x", "/b/c/;x"},
      {"g;x", "/b/c/g;x"}, {"g;x?y#s", "/b/c/g;x?y#s"}, {"", "/b/c/d;p?q"},
      {".", "/b/c/"}, {"./", "/b/c/"}, {"..", "/b/"}, {"../", "/b/"},
      {"../g", "/b/g"}, {"../..", "/"}, {"../../", "/"}, {"../../g", "/g"},
      {"../../../g", "/g"}, {"../../../../g", "/g"}, {"/./g", "/g"}, {"/../g", "/g"},
      {"g.", "/b/c/g."}, {".g", "/b/c/.g"}, {"g..", "/b/c/g.."}, {"..g", "/b/c/..g"},
      {"./../g", "/b/g"}, {"./g/.", "/b/c/g/"}, {"g/./h", "/b/c/g/h"},
      {"g/../h", "/b/c/h"}, {"g;x=1/./y", "/b/c/g;x=1/y"}, {"g;x=1/../y", "/b/c/y"},
      {"g?y/./x", "/b/c/g?y/./x"}, {"g?y/../x", "/b/c/g?y/../x"},
      {"g#s/./x", "/b/c/g#s/./x"}, {"g#s/../x", "/b/c/g#s/../x"}};
  for (const auto& [reference, expected] : vectors) {
    EXPECT_EQ(package::resolve_pack_reference(base, reference), std::string{prefix} + std::string{expected}) << reference;
  }
  EXPECT_EQ(package::resolve_pack_reference(base, "g:h"), "g:h");
  EXPECT_EQ(package::resolve_pack_reference(base, "//g"), "pack://g");
  EXPECT_EQ(package::resolve_pack_reference(base, "https:g"), "https:g");
  EXPECT_EQ(package::resolve_pack_reference("pack://https%3A,,example.org,package.opc", "a.xml"),
      "pack://https%3A,,example.org,package.opc/a.xml");
  EXPECT_THROW(static_cast<void>(package::parse_pack_iri(package::resolve_pack_reference(base, "."))), package::error);
}

// One caller policy owns outer-scheme semantics; callback exceptions and cumulative resource state survive.
TEST(pack_iri, shared_budget_interruption_and_caller_equivalence_policy) {
  constexpr std::string_view iri = "pack://https%3A,,example.org,package.opc/a.xml";
  package::budget shared{};
  const auto before = shared.remaining(package::resource::metadata_bytes);
  static_cast<void>(package::parse_pack_iri(iri, &shared));
  EXPECT_LT(shared.remaining(package::resource::metadata_bytes), before);
  package::limits limits{};
  limits.max_decoder_allocation_bytes = 0U;
  package::budget narrow{limits};
  EXPECT_THROW(static_cast<void>(package::parse_pack_iri(iri, &narrow)), package::error);
  limits = {};
  limits.is_cancelled = [] { return true; };
  package::budget cancelled{limits};
  EXPECT_THROW(static_cast<void>(package::compose_pack_iri("https://example.org/a.opc", {}, &cancelled)), package::error);
  limits = {};
  limits.deadline = std::chrono::steady_clock::now() - std::chrono::seconds{1};
  package::budget expired{limits};
  EXPECT_THROW(static_cast<void>(package::resolve_pack_reference(iri, "a.xml", &expired)), package::error);
  struct policy_failure : std::runtime_error {
    // Identify an original application exception without a runtime-owned replacement.
    policy_failure() : std::runtime_error{"original equivalence policy failure"} {}
  };
  EXPECT_THROW(static_cast<void>(package::equivalent_pack_iri(iri, iri,
      [](auto, auto) -> bool { throw policy_failure{}; })), policy_failure);
}

// Exact output metadata capacity admits one composition, while cumulative reuse and one-byte reductions reject.
TEST(pack_iri, exact_output_metadata_capacity_and_mid_operation_cancellation) {
  constexpr std::string_view package_iri = "https://example.org/package.opc";
  const auto encoded = package::compose_pack_iri(package_iri, "/a.xml");
  package::limits exact{};
  exact.max_metadata_bytes = encoded.size();
  package::budget shared{exact};
  EXPECT_EQ(package::compose_pack_iri(package_iri, "/a.xml", &shared), encoded);
  EXPECT_EQ(shared.remaining(package::resource::metadata_bytes), 0U);
  EXPECT_THROW(static_cast<void>(package::compose_pack_iri(package_iri, "/a.xml", &shared)), package::error);
  --exact.max_metadata_bytes;
  package::budget reduced{exact};
  EXPECT_THROW(static_cast<void>(package::compose_pack_iri(package_iri, "/a.xml", &reduced)), package::error);
  package::limits interrupted{};
  std::size_t checks{};
  interrupted.is_cancelled = [&checks] { return ++checks > 10U; };
  package::budget cancelled{interrupted};
  const auto long_package = std::string{"https://example.org/"} + std::string(65536U, 'a');
  EXPECT_THROW(static_cast<void>(package::compose_pack_iri(long_package, "/a.xml", &cancelled)), package::error);
  EXPECT_GT(checks, 10U);
}
} // namespace
