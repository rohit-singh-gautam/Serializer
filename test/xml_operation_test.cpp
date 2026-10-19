// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/package.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
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
#include <thread>
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

// Provider grammar, invalid inputs and independent concurrent executions retain XSD semantics.
TEST(xml_operation, prepared_patterns_use_xsd_grammar_and_independent_execution_state) {
  if (!xml::available()) {
    EXPECT_THROW((void)xml::prepared_pattern{"a"}, std::runtime_error);
    return;
  }
  const xml::prepared_pattern consonants{"[a-z-[aeiou]]+"};
  EXPECT_TRUE(consonants.matches("rhythm"));
  EXPECT_FALSE(consonants.matches("schema"));
  const xml::prepared_pattern categories{R"(\p{L}{2}\p{Nd}{2})"};
  EXPECT_TRUE(categories.matches("\xc3\xa9\xce\xa9" "12"));
  EXPECT_FALSE(categories.matches("ab1"));
  EXPECT_THROW((void)xml::prepared_pattern{"["}, std::invalid_argument);
  EXPECT_THROW((void)consonants.matches(std::string{"a\0b", 3U}), std::invalid_argument);
  xml::limits small{};
  small.max_input_bytes = 1U;
  EXPECT_THROW((void)(xml::prepared_pattern{"ab", small}), std::length_error);
  EXPECT_THROW((void)consonants.matches("bc", small), std::length_error);
  xml::operation_context interrupted{};
  interrupted.is_cancelled = [] { return true; };
  EXPECT_THROW((void)(xml::prepared_pattern{"a", {}, &interrupted}), std::runtime_error);
  EXPECT_THROW((void)consonants.matches("b", {}, &interrupted), std::runtime_error);
  auto moved = xml::prepared_pattern{"x"};
  const auto selected = std::move(moved);
  EXPECT_TRUE(selected.matches("x"));
  EXPECT_THROW((void)moved.matches("x"), std::logic_error);
  std::atomic<bool> matched{true};
  std::array<std::thread, 4U> workers;
  for (auto& worker : workers) {
    worker = std::thread{[&] {
      for (std::size_t iteration{}; iteration < 100U; ++iteration) {
        if (!consonants.matches("bcdf") || consonants.matches("aeiou")) { matched = false; }
      }
    }};
  }
  for (auto& worker : workers) { worker.join(); }
  EXPECT_TRUE(matched.load());
}

// Union branches retain XML Schema lexical spaces, including unbounded numeric sign constraints.
TEST(xml_operation, validates_language_unicode_names_and_unbounded_numeric_lexical_values) {
  const xml::element scope{};
  for (const auto value : {"en", "en-US", "abcdefgh-12345678"}) {
    EXPECT_NO_THROW(xml::validate_lexical_value(value, "language", scope));
  }
  for (const auto value : {"", "1en", "en-", "en--US", "abcdefghi", "en-123456789", "en_US"}) {
    EXPECT_THROW(xml::validate_lexical_value(value, "language", scope), std::invalid_argument);
  }
  EXPECT_NO_THROW(xml::validate_lexical_value("184467440737095516160000", "positiveInteger", scope));
  EXPECT_NO_THROW(xml::validate_lexical_value("-0", "nonNegativeInteger", scope));
  EXPECT_NO_THROW(xml::validate_lexical_value("-0001", "negativeInteger", scope));
  EXPECT_NO_THROW(xml::validate_lexical_value("+000", "nonPositiveInteger", scope));
  EXPECT_NO_THROW(xml::validate_lexical_value("+.25", "decimal", scope));
  EXPECT_NO_THROW(xml::validate_lexical_value("1.", "decimal", scope));
  EXPECT_THROW(xml::validate_lexical_value("0", "positiveInteger", scope), std::invalid_argument);
  EXPECT_THROW(xml::validate_lexical_value("-0", "negativeInteger", scope), std::invalid_argument);
  EXPECT_THROW(xml::validate_lexical_value("+1", "nonPositiveInteger", scope), std::invalid_argument);
  EXPECT_THROW(xml::validate_lexical_value("1e2", "decimal", scope), std::invalid_argument);
  EXPECT_THROW(xml::validate_lexical_value("1.0", "integer", scope), std::invalid_argument);
  EXPECT_TRUE(xml::lexical_lists_equal("en-US fr", "en-US\tfr", "language", scope));
  EXPECT_FALSE(xml::lexical_lists_equal("en-US fr", "en-US FR", "language", scope));
  if (xml::available()) {
    EXPECT_NO_THROW(xml::validate_lexical_value("\xc3\xa9" "clair", "NCName", scope));
    EXPECT_NO_THROW(xml::validate_lexical_value("prefix:local", "Name", scope));
    EXPECT_NO_THROW(xml::validate_lexical_value("9:token", "NMTOKEN", scope));
    EXPECT_THROW(xml::validate_lexical_value("prefix:local", "ID", scope), std::invalid_argument);
    EXPECT_THROW(xml::validate_lexical_value("9token", "IDREF", scope), std::invalid_argument);
    for (const auto kind : {"Name", "NCName", "ID", "IDREF", "ENTITY", "NMTOKEN"}) {
      EXPECT_TRUE(xml::lexical_lists_equal("one two", "one\ttwo", kind, scope));
      EXPECT_FALSE(xml::lexical_lists_equal("one two", "one TWO", kind, scope));
    }
  }
}

// Raw-tree opt-in retains mixed text order while ordinary and typed XML contracts remain strict.
TEST(xml_operation, raw_mixed_content_is_explicit_and_schema_annotations_remain_validated) {
  if (!xml::available()) { GTEST_SKIP(); }
  constexpr std::string_view source{"<r>before<a/>after</r>"};
  EXPECT_THROW((void)xml::parse(source), std::invalid_argument);
  xml::operation_context context{};
  context.allow_mixed_content = true;
  const auto tree = xml::parse(source, {}, &context);
  ASSERT_EQ(tree.text_segments.size(), 2U);
  EXPECT_EQ(tree.text, "beforeafter");
  EXPECT_EQ(xml::parse(xml::write(tree, {}, &context), {}, &context).text, "beforeafter");
  xml::field_reader reader{tree, true, &context};
  std::string value;
  EXPECT_THROW(reader.text_value(value), std::invalid_argument);
  constexpr std::string_view annotated_schema =
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' xmlns:h='urn:html'>"
      "<xs:annotation><xs:documentation>Direct <h:em>inline</h:em> text.</xs:documentation>"
      "</xs:annotation><xs:element name='r' type='xs:string'/></xs:schema>";
  const xml::prepared_schema schema{annotated_schema};
  EXPECT_NO_THROW(schema.validate("<r>value</r>"));
  EXPECT_THROW((void)xml::prepared_schema{
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>unexpected text</xs:schema>"},
      std::invalid_argument);
}

// Pattern callbacks see original spelling/defaults and exact child scope before numeric conversion.
TEST(xml_operation, field_callbacks_validate_raw_numeric_spelling_before_conversion) {
  if (!xml::available()) { GTEST_SKIP(); }
  const xml::lexical_validator validator = [](std::string_view text, const xml::element& scope,
      xml::operation_context* operation) {
    if (!operation) { throw std::logic_error{"Lexical callback lost its operation"}; }
    if (scope.qualified_name.local == "v" && scope.namespace_bindings.at("p") != "urn:child") {
      throw std::logic_error{"Lexical callback lost its original child scope"};
    }
    operation->consume(xml::operation_resource::work_units, text.size());
    if (text.find('.') != std::string_view::npos) {
      throw std::invalid_argument{"Original numeric pattern rejects a decimal point"};
    }
  };
  xml::operation_context context{};
  double value = 42.0;
  const auto text = xml::parse("<r>1.</r>");
  xml::field_reader legacy{text, true};
  legacy.text_value(value);
  EXPECT_EQ(value, 1.0);
  value = 42.0;
  xml::field_reader checked{text, true, &context};
  EXPECT_THROW(checked.text_value(value, validator), std::invalid_argument);
  EXPECT_EQ(value, 42.0);
  std::vector<double> values{9.0};
  const auto list = xml::parse("<r>1. 2</r>");
  xml::field_reader list_reader{list, true, &context};
  EXPECT_THROW(list_reader.list_text_value(values, validator), std::invalid_argument);
  EXPECT_EQ(values, std::vector<double>{9.0});
  const auto attributes = xml::parse("<r a='1.' list='1. 2'/>");
  xml::field_reader attribute_reader{attributes, false, &context};
  EXPECT_THROW(attribute_reader.attribute_value({"a", {}}, value, {}, validator), std::invalid_argument);
  EXPECT_THROW(attribute_reader.list_attribute_value({"list", {}}, values, {}, validator), std::invalid_argument);
  const auto empty = xml::parse("<r/>");
  xml::field_reader defaults{empty, false, &context};
  EXPECT_THROW(defaults.attribute_value({"absent", {}}, value, "1.", validator), std::invalid_argument);
  std::optional<std::vector<double>> optional_list{};
  EXPECT_THROW(defaults.list_attribute_value({"absent", {}}, optional_list, "1. 2", validator),
      std::invalid_argument);
  EXPECT_FALSE(optional_list);
  const auto children = xml::parse(
      "<r xmlns:p='urn:root'><v xmlns:p='urn:child'>1</v><v xmlns:p='urn:child'>1.</v></r>");
  xml::field_reader child_reader{children, false, &context};
  std::array<double, 2U> fixed{20.0, 20.0};
  EXPECT_THROW(child_reader.child_value({"v", {}}, fixed, {}, validator), std::invalid_argument);
  EXPECT_EQ(fixed[0U], 1.0);
  EXPECT_EQ(fixed[1U], 20.0);
  const auto default_child = xml::parse("<r><v xmlns:p='urn:child'/></r>");
  xml::field_reader default_reader{default_child, false, &context};
  std::optional<double> optional_value{20.0};
  EXPECT_THROW(default_reader.child_value({"v", {}}, optional_value, "1.", validator), std::invalid_argument);
  EXPECT_EQ(optional_value, 20.0);
  const auto wrapper = xml::parse("<r><items xmlns:p='urn:child'><v>1.</v></items></r>");
  xml::field_reader wrapper_reader{wrapper, false, &context};
  EXPECT_THROW(wrapper_reader.wrapped_child_value({"items", {}}, {"v", {}}, values, validator),
      std::invalid_argument);
  const auto valid = xml::parse("<r a='1' list='1 2'/>");
  xml::field_reader valid_reader{valid, false, &context};
  EXPECT_NO_THROW(valid_reader.attribute_value({"a", {}}, value, {}, validator));
  EXPECT_NO_THROW(valid_reader.list_attribute_value({"list", {}}, values, {}, validator));
  EXPECT_EQ(values, (std::vector<double>{1.0, 2.0}));
}

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

// Floating facets compare rounded IEEE values; integer/decimal facets keep exact lexical ordering.
TEST(xml_operation, floating_bounds_use_the_declared_xsd_value_width) {
  xml::scalar_constraints constraints{};
  constraints.lexical_kind = "float";
  constraints.lexical_max_exclusive = "0.1000000005";
  EXPECT_THROW(xml::validate_scalar_constraints("0.1", constraints), std::invalid_argument);
  EXPECT_NO_THROW(xml::validate_scalar_constraints("0.09999999", constraints));
  constraints.lexical_max_exclusive.reset();
  constraints.lexical_min_exclusive = "0.0999999995";
  EXPECT_THROW(xml::validate_scalar_constraints("0.1", constraints), std::invalid_argument);
  constraints = {};
  constraints.lexical_kind = "double";
  constraints.lexical_max_exclusive = "0.1000000000000000005";
  EXPECT_THROW(xml::validate_scalar_constraints("0.1", constraints), std::invalid_argument);
  constraints.lexical_max_exclusive.reset();
  constraints.lexical_max_inclusive = "INF";
  EXPECT_NO_THROW(xml::validate_scalar_constraints("INF", constraints));
  EXPECT_THROW(xml::validate_scalar_constraints("NaN", constraints), std::invalid_argument);
  constraints.lexical_max_inclusive = "NaN";
  EXPECT_NO_THROW(xml::validate_scalar_constraints("NaN", constraints));
  EXPECT_THROW(xml::validate_scalar_constraints("0", constraints), std::invalid_argument);
  constraints.lexical_max_inclusive.reset();
  constraints.lexical_min_inclusive = "NaN";
  EXPECT_NO_THROW(xml::validate_scalar_constraints("NaN", constraints));
  EXPECT_THROW(xml::validate_scalar_constraints("INF", constraints), std::invalid_argument);
  constraints.lexical_min_inclusive.reset();
  constraints.lexical_max_exclusive = "NaN";
  EXPECT_THROW(xml::validate_scalar_constraints("NaN", constraints), std::invalid_argument);
  EXPECT_THROW(xml::validate_scalar_constraints("0", constraints), std::invalid_argument);
  constraints.lexical_max_exclusive.reset();
  constraints.lexical_max_inclusive = "-0";
  EXPECT_NO_THROW(xml::validate_scalar_constraints("+0", constraints));
  constraints.lexical_max_inclusive.reset();
  constraints.lexical_max_exclusive = "0";
  EXPECT_THROW(xml::validate_scalar_constraints("-0", constraints), std::invalid_argument);
  constraints.lexical_kind = "decimal";
  constraints.lexical_max_exclusive = "0.1000000005";
  EXPECT_NO_THROW(xml::validate_scalar_constraints("0.1", constraints));
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
