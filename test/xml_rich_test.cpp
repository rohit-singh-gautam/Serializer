// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <xml_rich.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

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
} // namespace

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
