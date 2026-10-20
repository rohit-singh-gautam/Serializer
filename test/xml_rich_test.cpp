// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <xml_rich.hpp>
#include <xml_numeric_projection.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;

// Keep a fixed-XSD valid fixture whose prefixes differ from the schema's own declarations.
std::string rich_document(std::string_view body) {
  return "<document xmlns='urn:serializer:xml-rich' xmlns:e='urn:extension' "
      "xmlns:other='urn:serializer:names' transform='1 0 1' required='e'>" +
      std::string{body} + "</document>";
}

// Build one optional-attribute probe without coupling unrelated floating-point facet cases.
std::string floating_document(std::string_view attributes) {
  return "<floating_sample xmlns='urn:serializer:numeric-projection' " +
      std::string{attributes} + "/>";
}
} // namespace

// Child-local QName bindings retain their expanded identity in both XML output paths.
TEST(xml_rich, qname_child_scope_changes_preserve_value_identity) {
  const auto value = xml::deserialize<xml_numeric_projection::qname_sample>(
      "<qname_sample xmlns='urn:serializer:numeric-projection' xmlns:p='urn:parent'>"
      "<value>p:item</value></qname_sample>");
  EXPECT_EQ(value.value, "p:item");
  EXPECT_NO_THROW((void)xml::deserialize<xml_numeric_projection::qname_sample>(xml::serialize(value)));
  const auto shadowed = xml::deserialize<xml_numeric_projection::qname_sample>(
      "<qname_sample xmlns='urn:serializer:numeric-projection' xmlns:p='urn:parent'>"
      "<value xmlns:p='urn:child'>p:item</value></qname_sample>");
  EXPECT_EQ(shadowed.value, "p:item");
  const auto output = xml::parse(xml::serialize(shadowed));
  ASSERT_EQ(output.children.size(), 1U);
  EXPECT_EQ(xml::resolve_qname(output.children[0].text, output.children[0]),
      (xml::name{"item", "urn:child"}));
  const auto direct = xml::parse(xml::serialize_tokens(shadowed));
  ASSERT_EQ(direct.children.size(), 1U);
  EXPECT_EQ(xml::resolve_qname(direct.children[0].text, direct.children[0]),
      (xml::name{"item", "urn:child"}));
}

// Arbitrary-precision string projections retain numeric enumeration/fixed identity and lexical bounds.
TEST(xml_rich, numeric_string_projections_use_xsd_value_identity) {
  constexpr std::string_view source{
      "<sample xmlns='urn:serializer:numeric-projection' integer='042' decimal='+1.00' fixed='+01'/>"};
  auto value = xml::deserialize<xml_numeric_projection::sample>(source);
  EXPECT_EQ(value.integer, "042");
  EXPECT_EQ(value.decimal, "+1.00");
  EXPECT_NO_THROW((void)xml::deserialize<xml_numeric_projection::sample>(xml::serialize(value)));
  value.integer = "0";
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  value.integer = "43";
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  value.integer = "42";
  value.decimal = "1e0";
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  value.decimal = "1.0";
  value.fixed = "2";
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
}

// XSD 1.0 uses value identity for NaN enumeration and fixed values, unlike IEEE operator==.
TEST(xml_rich, floating_nan_enumeration_and_fixed_use_xsd_identity) {
  auto value = xml::deserialize<xml_numeric_projection::floating_sample>(floating_document(
      "float_nan_enum='NaN' double_nan_enum='NaN' float_nan_fixed='NaN' double_nan_fixed='NaN'"));
  ASSERT_TRUE(value.float_nan_enum);
  ASSERT_TRUE(value.double_nan_enum);
  ASSERT_TRUE(value.float_nan_fixed);
  ASSERT_TRUE(value.double_nan_fixed);
  EXPECT_TRUE(std::isnan(*value.float_nan_enum));
  EXPECT_TRUE(std::isnan(*value.double_nan_enum));
  EXPECT_NO_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(xml::serialize(value)));
  value.float_nan_enum = 0.0F;
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(
      floating_document("double_nan_fixed='0'")), std::invalid_argument);
}

// Bounds and enumerations round to the declared XSD IEEE width before comparing their values.
TEST(xml_rich, floating_facets_compare_at_the_declared_width) {
  const auto valid = floating_document(
      "float_enum='0.1000000001' double_enum='0.100000000000000001' "
      "float_min_inclusive='0.1' float_max_inclusive='0.1' "
      "double_min_inclusive='0.1' double_max_inclusive='0.1' "
      "float_min_exclusive='0.10000001' float_max_exclusive='0.09999999' "
      "double_min_exclusive='0.10000000000000002' double_max_exclusive='0.09999999999999999'");
  auto value = xml::deserialize<xml_numeric_projection::floating_sample>(valid);
  EXPECT_NO_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(xml::serialize(value)));
  for (const auto attribute : {"float_min_exclusive", "float_max_exclusive",
      "double_min_exclusive", "double_max_exclusive"}) {
    EXPECT_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(
        floating_document(std::string{attribute} + "='0.1'")), std::invalid_argument);
  }
  value.float_min_exclusive = 0.1F;
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  value.float_min_exclusive.reset();
  value.double_max_exclusive = 0.1;
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
}

// XSD 1.0 makes NaN incomparable with ordinary bounds; only inclusive NaN bounds admit NaN.
TEST(xml_rich, floating_nan_bounds_follow_xsd_1_0_ordering) {
  const auto source = floating_document("nan_min_inclusive='NaN' nan_max_inclusive='NaN'");
  auto value = xml::deserialize<xml_numeric_projection::floating_sample>(source);
  EXPECT_NO_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(xml::serialize(value)));
  for (const auto attribute : {"nan_min_exclusive", "nan_max_exclusive",
      "float_min_inclusive", "float_max_inclusive", "float_min_exclusive", "float_max_exclusive",
      "double_min_inclusive", "double_max_inclusive", "double_min_exclusive", "double_max_exclusive"}) {
    EXPECT_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(
        floating_document(std::string{attribute} + "='NaN'")), std::invalid_argument);
  }
  for (const auto attribute : {"nan_min_inclusive", "nan_max_inclusive", "nan_min_exclusive", "nan_max_exclusive"}) {
    EXPECT_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(
        floating_document(std::string{attribute} + "='0'")), std::invalid_argument);
  }
  value.nan_min_inclusive = 0.0F;
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
}

// Recursive union branches preserve their numeric width instead of comparing their decimal spellings.
TEST(xml_rich, floating_union_branches_retain_selected_width_bounds) {
  auto value = xml::deserialize<xml_numeric_projection::floating_sample>(
      floating_document("float_union='0.09999999' double_union='0.09999999999999999'"));
  EXPECT_NO_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(xml::serialize(value)));
  for (const auto attribute : {"float_union", "double_union"}) {
    EXPECT_THROW((void)xml::deserialize<xml_numeric_projection::floating_sample>(
        floating_document(std::string{attribute} + "='0.1'")), std::invalid_argument);
  }
  value.float_union = "0.1";
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
}

// Numeric/list patterns inspect the original normalized lexical form before native conversion.
TEST(xml_rich, typed_numeric_patterns_validate_original_input_lexical_values) {
  constexpr std::string_view prefix{"<pattern_sample xmlns='urn:serializer:numeric-projection' "};
  const auto read = [&](std::string_view attributes) {
    return xml::deserialize<xml_numeric_projection::pattern_sample>(
        std::string{prefix} + std::string{attributes} + "/>");
  };
  auto value = read("value='01'");
  EXPECT_EQ(value.value, 1);
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  value = read("value='10'");
  EXPECT_NO_THROW((void)xml::serialize(value));
  EXPECT_THROW((void)read("value='1'"), std::invalid_argument);
  EXPECT_THROW((void)read("value='+01'"), std::invalid_argument);
  value = read("value='10' values='1.00 2.00'");
  ASSERT_TRUE(value.values);
  ASSERT_EQ(value.values->size(), 2U);
  EXPECT_EQ(value.values->front(), 1.0F);
  EXPECT_THROW((void)read("value='10' values='1 2'"), std::invalid_argument);
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
}

// Generated readers apply exact facets/defaults and retain QName identity through regenerated prefixes.
TEST(xml_rich, facets_defaults_qnames_and_typed_lists_round_trip) {
  auto value = xml::deserialize<xml_rich::document>(rich_document(
      "<number>18446744073709551614</number>"
      "<metadata name='other:thing'>text</metadata>"));
  EXPECT_EQ(value.number, std::numeric_limits<std::uint64_t>::max() - 1U);
  EXPECT_EQ(value.unit, "millimeter");
  EXPECT_EQ(value.mode, "model");
  EXPECT_EQ(value.transform[0], 1.0);
  EXPECT_FALSE(value.optional_transform);
  ASSERT_TRUE(value.data);
  EXPECT_TRUE(value.data->category);
  const auto encoded = xml::serialize(value);
  const auto copy = xml::deserialize<xml_rich::document>(encoded);
  EXPECT_EQ(copy.number, value.number);
  EXPECT_EQ(copy.data->value, "text");
  EXPECT_FALSE(copy.optional_transform);
  auto optional_list = rich_document("<number>2</number>");
  optional_list.insert(optional_list.find("transform='1 0 1'"), "optional_transform='2 3 4' ");
  const auto present_list = xml::deserialize<xml_rich::document>(optional_list);
  ASSERT_TRUE(present_list.optional_transform);
  EXPECT_EQ(present_list.optional_transform->size(), 3U);
  optional_list.replace(optional_list.find("optional_transform='2 3 4'"),
      std::string_view{"optional_transform='2 3 4'"}.size(), "optional_transform=''");
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(optional_list), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(rich_document(
      "<number>18446744073709551615</number>")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(rich_document(
      "<label>too long</label>")), std::invalid_argument);
  auto normalized = xml::deserialize<xml_rich::document>(rich_document("<label>  a\t b </label>"));
  EXPECT_EQ(normalized.label, "a b");
}

// Choices remain exclusive and preserve slots follow the exact declared wildcard position/count.
TEST(xml_rich, choice_and_positioned_wildcard_contracts_are_enforced) {
  auto value = xml::deserialize<xml_rich::document>(rich_document(
      "<number>2</number><e:extra>before<e:child/>after</e:extra>"));
  ASSERT_EQ(value.xml_extensions.value().children.size(), 1U);
  EXPECT_NO_THROW((void)xml::deserialize<xml_rich::document>(xml::serialize(value)));
  value.label = "a";
  EXPECT_THROW((void)xml::serialize(value), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(rich_document("")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(rich_document(
      "<e:extra/><number>2</number>")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(rich_document(
      "<number>2</number><e:a/><e:b/><e:c/>")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(rich_document(
      "<number>2</number><metadata name='other:thing'/><e:extra/>")), std::invalid_argument);
}

// QName enums compare expanded identities; undeclared names and prefix lists never pass as plain strings.
TEST(xml_rich, qname_and_required_prefix_lexical_validation_is_namespace_aware) {
  EXPECT_NO_THROW((void)xml::deserialize<xml_rich::document>(rich_document(
      "<number>2</number><metadata name='other:thing'/>")));
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(rich_document(
      "<number>2</number><metadata name='other:different'/>")), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(rich_document(
      "<number>2</number><metadata name='missing:thing'/>")), std::invalid_argument);
  auto invalid = rich_document("<number>2</number>");
  invalid.replace(invalid.find("required='e'"), std::string_view{"required='e'"}.size(), "required='missing'");
  EXPECT_THROW((void)xml::deserialize<xml_rich::document>(invalid), std::invalid_argument);
}
