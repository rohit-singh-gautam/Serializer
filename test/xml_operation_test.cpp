// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace xml = rohit::serializer::xml;
namespace package = rohit::serializer::package;

// Explicitly permit only this fixture's foreign vocabulary, retaining core strictness.
xml::operation_context preserving_context() {
  xml::operation_context result{};
  result.unknown_fields = xml::unknown_policy::preserve;
  result.permits_namespace = [](std::string_view uri) { return uri == "urn:extension"; };
  return result;
}

// Own one exact temporary file; never recursively delete shared or user-created directories.
struct temporary_file {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
      ("serializer-xml-operation-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()) + ".zip");
  // Seed an existing destination whose bytes must survive a failed Save.
  temporary_file() { std::ofstream{path, std::ios::binary} << "previous"; }
  // Remove only this fixture's uniquely named file after the assertion scope.
  ~temporary_file() { std::error_code ignored; std::filesystem::remove(path, ignored); }
};
} // namespace

// XML parsing must poll within one large provider token instead of waiting for its closing tag.
TEST(xml_operation, interrupts_large_token_at_bounded_input_callbacks) {
  const auto document = std::string{"<record>"} + std::string(250000U, 'x') + "</record>";
  xml::operation_context context{};
  std::size_t checks{}, processed{};
  context.is_cancelled = [&] { return ++checks >= 12U; };
  context.charge = [&](xml::operation_resource selected, std::size_t amount) {
    if (selected == xml::operation_resource::work_units) { processed += amount; }
  };
  EXPECT_THROW((void)xml::parse(document, {}, &context), std::runtime_error);
  EXPECT_LT(processed, document.size());
}

// Package allocation counters remain cumulative across distinct XML parts and repeated attempts.
TEST(xml_operation, shares_cumulative_allocation_budget_with_package) {
  package::limits bounds{};
  bounds.max_decoder_allocation_bytes = sizeof(xml::element) * 2U + 8U;
  package::budget shared{bounds};
  auto context = shared.xml_context("/data.xml");
  EXPECT_NO_THROW((void)xml::parse("<first/>", {}, &context));
  const auto remaining = shared.remaining(package::resource::decoder_allocation_bytes);
  EXPECT_LT(remaining, bounds.max_decoder_allocation_bytes);
  EXPECT_THROW((void)xml::parse("<second><nested/></second>", {}, &context), package::error);
  EXPECT_LE(shared.remaining(package::resource::decoder_allocation_bytes), remaining);
}

// Deadline failures occur before final publication and preserve an existing complete file.
TEST(xml_operation, expired_package_deadline_preserves_destination) {
  temporary_file destination{};
  package::limits bounds{};
  bounds.deadline = std::chrono::steady_clock::now();
  try {
    package::save_zip_file(destination.path, [](package::zip_writer& writer) {
      writer.write("payload", [](std::ostream& output) { output << "replacement"; });
    }, bounds);
    FAIL() << "Expired deadline must fail";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, package::error_code::deadline_exceeded);
  }
  std::ifstream input{destination.path, std::ios::binary};
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>{input}, {}), "previous");
}

// Semantic preservation includes direct child placement, mixed text order and QName bindings.
TEST(xml_operation, preserves_foreign_slots_and_namespace_context_across_known_edits) {
  auto context = preserving_context();
  const auto document = xml::parse(
      "<r xmlns='urn:core' xmlns:e='urn:extension' e:tag='e:Thing'>"
      "<first>1</first><e:extra>before<e:child/>after</e:extra><last>2</last></r>",
      {}, &context);
  xml::extension_store extensions{};
  xml::field_reader reader{document, false, &context, &extensions};
  int first{}, last{};
  reader.child_value({"first", "urn:core"}, first);
  reader.child_value({"last", "urn:core"}, last);
  reader.finish();
  reader.finish();
  EXPECT_EQ(first, 1); EXPECT_EQ(last, 2);
  ASSERT_EQ(extensions.attributes.size(), 1U);
  ASSERT_EQ(extensions.children.size(), 1U);
  EXPECT_EQ(extensions.children.front().before_child, 1U);
  ASSERT_EQ(extensions.children.front().value.text_segments.size(), 2U);

  xml::element output{};
  output.qualified_name = {"r", "urn:core"};
  xml::build_budget budget{{}, &context};
  const auto depth = budget.enter(output.qualified_name);
  xml::add_child(output, {"first", "urn:core"}, 7, budget);
  xml::add_child(output, {"last", "urn:core"}, 9, budget);
  xml::restore_extensions(output, extensions, budget);
  const auto encoded = xml::write(output, {}, &context);
  const auto restored = xml::parse(encoded, {}, &context);
  ASSERT_EQ(restored.children.size(), 3U);
  EXPECT_EQ(restored.children[0].text, "7");
  EXPECT_EQ(restored.children[1].qualified_name, (xml::name{"extra", "urn:extension"}));
  EXPECT_EQ(restored.children[2].text, "9");
  EXPECT_EQ(restored.children[1].text_segments[0].value, "before");
  EXPECT_EQ(restored.children[1].text_segments[1].value, "after");
  EXPECT_EQ(xml::resolve_qname(restored.attributes.front().value, restored),
      (xml::name{"Thing", "urn:extension"}));
}

// A permissive foreign policy must never discard unknown or misplaced core fields.
TEST(xml_operation, unknown_core_and_unbound_preservation_slots_remain_errors) {
  auto context = preserving_context();
  const auto core = xml::parse("<r xmlns='urn:core'><unknown/></r>");
  xml::extension_store retained{};
  xml::field_reader strict{core, false, &context, &retained};
  EXPECT_THROW(strict.finish(), std::invalid_argument);
  const auto foreign = xml::parse("<r xmlns='urn:core' xmlns:e='urn:extension'><e:extra/></r>");
  xml::field_reader no_slot{foreign, false, &context};
  EXPECT_THROW(no_slot.finish(), std::invalid_argument);
  context.unknown_fields = xml::unknown_policy::skip;
  xml::field_reader skipped{foreign, false, &context};
  EXPECT_NO_THROW(skipped.finish());
  context.permits_namespace = {};
  xml::field_reader unpermitted{foreign, false, &context};
  EXPECT_THROW(unpermitted.finish(), std::invalid_argument);
}

// Reordered known foreign declarations cannot be reclassified as optional unknown extensions.
TEST(xml_operation, registered_known_children_keep_ordering_strict) {
  auto context = preserving_context();
  context.unknown_fields = xml::unknown_policy::skip;
  const auto document = xml::parse(
      "<r xmlns='urn:core' xmlns:e='urn:extension'><e:last/><first>1</first></r>");
  const std::array known{xml::name{"first", "urn:core"}, xml::name{"last", "urn:extension"}};
  xml::field_reader reader{document, false, &context, nullptr, known};
  int first{};
  EXPECT_THROW(reader.child_value({"first", "urn:core"}, first), std::invalid_argument);
}

// Preserving ordinary mesh records costs one empty pointer and never allocates an opaque tree.
TEST(xml_operation, lazy_slots_preserve_wrapper_extensions_and_copy_independently) {
  static_assert(sizeof(xml::extension_slot) == sizeof(void*));
  auto context = preserving_context();
  const auto ordinary = xml::parse("<r xmlns='urn:core'><items><item>1</item></items></r>");
  xml::extension_slot empty{};
  std::vector<int> values{};
  xml::field_reader plain{ordinary, false, &context, empty};
  plain.wrapped_child_value({"items", "urn:core"}, {"item", "urn:core"}, values);
  plain.finish();
  EXPECT_EQ(empty.get(), nullptr);
  const auto document = xml::parse(
      "<r xmlns='urn:core' xmlns:e='urn:extension'><items e:flag='yes'>"
      "<item>1</item><e:extra/><item>2</item></items></r>", {}, &context);
  xml::extension_slot retained{};
  xml::field_reader reader{document, false, &context, retained};
  reader.wrapped_child_value({"items", "urn:core"}, {"item", "urn:core"}, values);
  reader.finish();
  ASSERT_NE(retained.get(), nullptr);
  ASSERT_EQ(retained.value().wrappers.size(), 1U);
  const auto copy = retained;
  retained.value().wrappers.front().extensions.attributes.front().value = "changed";
  EXPECT_EQ(copy.value().wrappers.front().extensions.attributes.front().value, "yes");
  xml::element output{};
  output.qualified_name = {"r", "urn:core"};
  xml::build_budget budget{{}, &context};
  const auto depth = budget.enter(output.qualified_name);
  xml::add_wrapped_children(output, {"items", "urn:core"}, {"item", "urn:core"}, values, budget);
  xml::restore_extensions(output, copy.value(), budget);
  ASSERT_EQ(output.children.size(), 1U);
  EXPECT_EQ(output.children.front().attributes.front().value, "yes");
  ASSERT_EQ(output.children.front().children.size(), 3U);
  EXPECT_EQ(output.children.front().children[1].qualified_name.namespace_uri, "urn:extension");
}

// Required transform-style fixed lists enforce the exact extent and complete numeric lexemes.
TEST(xml_operation, fixed_numeric_lists_and_exact_integer_facets) {
  const auto values = xml::parse_list<std::array<double, 3>>(" 1\t2.5\n-3 ");
  EXPECT_EQ(values, (std::array<double, 3>{1, 2.5, -3}));
  EXPECT_EQ(xml::format_list(values), "1 2.5 -3");
  EXPECT_THROW((void)(xml::parse_list<std::array<double, 3>>("1 2")), std::invalid_argument);
  EXPECT_THROW((void)(xml::parse_list<std::vector<int>>("1 2suffix")), std::invalid_argument);
  xml::scalar_constraints constraints{};
  constraints.lexical_min_inclusive = "1";
  constraints.lexical_max_inclusive = "18446744073709551615";
  EXPECT_NO_THROW(xml::validate_scalar_constraints("18446744073709551615", constraints));
  EXPECT_THROW(xml::validate_scalar_constraints("18446744073709551616", constraints), std::invalid_argument);
  EXPECT_THROW(xml::validate_scalar_constraints("0", constraints), std::invalid_argument);
  EXPECT_EQ(xml::compare_numeric_values("100.00e-2", "1"), 0);
  EXPECT_LT(xml::compare_numeric_values("-1.01", "-1"), 0);
  EXPECT_GT(xml::compare_numeric_values("0.0001", "0"), 0);
  EXPECT_THROW((void)xml::compare_numeric_values("NaN", "1"), std::invalid_argument);
}

// String facets count Unicode scalar values and apply the selected XML whitespace policy.
TEST(xml_operation, unicode_length_and_normalized_enumeration_facets) {
  xml::scalar_constraints constraints{};
  constraints.length = 2U;
  EXPECT_NO_THROW(xml::validate_scalar_constraints("\xc3\xa9\xf0\x9f\x98\x80", constraints));
  EXPECT_THROW(xml::validate_scalar_constraints("\xc3\xa9", constraints), std::invalid_argument);
  const std::array<std::string_view, 1> allowed{"red green"};
  constraints = {};
  constraints.whitespace = xml::whitespace_policy::collapse;
  constraints.enumeration = allowed;
  EXPECT_NO_THROW(xml::validate_scalar_constraints(" red\t \ngreen ", constraints));
  EXPECT_THROW(xml::validate_scalar_constraints("red blue", constraints), std::invalid_argument);
}

// Typed QName/prefix-list values survive different input prefix spellings and generated collisions.
TEST(xml_operation, qname_and_prefix_lists_preserve_namespace_identities) {
  auto scope = xml::parse("<r xmlns='urn:core' xmlns:e='urn:extension' xmlns:q0='urn:occupied'/>");
  EXPECT_EQ(xml::resolve_qname("e:Thing", scope), (xml::name{"Thing", "urn:extension"}));
  EXPECT_THROW((void)xml::resolve_qname("missing:Thing", scope), std::invalid_argument);
  EXPECT_THROW((void)xml::resolve_qname("e:bad:name", scope), std::invalid_argument);
  const auto qualified = xml::qualify_qname({"Thing", "urn:new"}, scope);
  EXPECT_EQ(qualified, "q1:Thing");
  EXPECT_EQ(xml::resolve_qname(qualified, scope), (xml::name{"Thing", "urn:new"}));
  const std::vector<std::string> required{"urn:extension", "urn:new"};
  const auto prefixes = xml::qualify_prefix_list(required, scope);
  EXPECT_EQ(xml::resolve_prefix_list(prefixes, scope), required);
  EXPECT_THROW((void)xml::qualify_qname({"plain", ""}, scope), std::invalid_argument);
}
