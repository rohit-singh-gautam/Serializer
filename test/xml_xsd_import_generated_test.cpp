// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <shared_import.hpp>
#include <rohit/xml.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;
using model = shared_import::xsd_order;
constexpr std::string_view fixture_document{
    "<order id='9007199254740993' label='Hardware'><item name='Bolt'>"
    "<price unit='USD'>7</price></item><item name='Nut'><price>2</price></item></order>"};
} // namespace

// Import owns each heterogeneous occurrence once and retains the XSD's complete group grammar.
TEST(xml_xsd_import_generated, ordered_import_validates_group_order_counts_and_edits) {
  const auto value = xml::deserialize<model>(
      "<order id='1'><schedule><price>2</price><item name='a'><price>3</price></item>"
      "<item name='b'><price>4</price></item><price>5</price></schedule></order>");
  ASSERT_TRUE(value.xsd_schedule);
  ASSERT_EQ(value.xsd_schedule->xsd_items.size(), 4U);
  const auto encoded = xml::serialize(value);
  const auto restored = xml::deserialize<model>(encoded);
  ASSERT_TRUE(restored.xsd_schedule);
  EXPECT_EQ(restored.xsd_schedule->xsd_items.size(), 4U);
  for (const auto invalid : {
      "<schedule/>", "<schedule><price>2</price><item name='a'><price>3</price></item></schedule>",
      "<schedule><price>2</price><price>3</price><item name='a'><price>4</price></item><item name='b'><price>5</price></item></schedule>",
      "<schedule><price>2</price><item name='a'><price>9</price></item><price>3</price><item name='b'><price>4</price></item></schedule>"}) {
    EXPECT_THROW((void)xml::deserialize<model>(std::string{"<order id='1'>"} + invalid + "</order>"), std::invalid_argument);
  }
  auto edited = value;
  edited.xsd_schedule->xsd_items.pop_back();
  EXPECT_THROW((void)xml::serialize(edited), std::invalid_argument);
}

TEST(xml_xsd_import_generated, scalar_attribute_text_and_nested_include_collection_roundtrip) {
  const auto value = xml::deserialize<model>(fixture_document);
  EXPECT_EQ(value.xsd_id, std::uint64_t{9007199254740993ULL});
  ASSERT_TRUE(value.xsd_label);
  EXPECT_EQ(*value.xsd_label, "Hardware");
  ASSERT_EQ(value.xsd_item.size(), 2U);
  EXPECT_EQ(value.xsd_item.front().xsd_name, "Bolt");
  EXPECT_EQ(value.xsd_item.front().xsd_price.xsd_value, 7U);
  ASSERT_TRUE(value.xsd_item.front().xsd_price.xsd_unit);
  EXPECT_EQ(*value.xsd_item.front().xsd_price.xsd_unit, "USD");
  EXPECT_FALSE(value.xsd_item.back().xsd_price.xsd_unit);
  const auto copy = xml::deserialize<model>(xml::serialize(value));
  EXPECT_EQ(copy.xsd_id, value.xsd_id);
  ASSERT_EQ(copy.xsd_item.size(), 2U);
  EXPECT_EQ(copy.xsd_item.front().xsd_price.xsd_value, value.xsd_item.front().xsd_price.xsd_value);
  EXPECT_EQ(copy.xsd_item.back().xsd_name, "Nut");
  const auto empty = xml::deserialize<model>("<order id='0'/>");
  EXPECT_TRUE(empty.xsd_item.empty());
  EXPECT_FALSE(empty.xsd_label);
}

TEST(xml_xsd_import_generated, imported_constraints_reject_missing_out_of_range_and_malformed_values) {
  for (const auto invalid : {
      "<order/>",
      "<order id='18446744073709551616'/>",
      "<order id='1'><item name='x'><price>1</price></item></order>",
      "<order id='1'><item name='x'><price>9</price></item></order>",
      "<order id='1'><item name='x'><price>2x</price></item></order>",
      "<order id='1'><item><price>2</price></item></order>",
      "<order id='1'><item name='x'><price><value>2</value></price></item></order>"}) {
    EXPECT_THROW((void)xml::deserialize<model>(invalid), std::invalid_argument) << invalid;
  }
  auto invalid = xml::deserialize<model>(fixture_document);
  invalid.xsd_item.front().xsd_price.xsd_value = 9U;
  EXPECT_THROW((void)xml::serialize(invalid), std::invalid_argument);
}

// Generated typed list storage retains item restrictions and occurrence namespace identities.
TEST(xml_xsd_import_generated, named_inline_and_union_list_items_roundtrip_and_validate) {
  const auto value = xml::deserialize<model>(
      "<order xmlns:p='urn:item' id='1' samples='2 8' selections='p:item 7' codes='AB CD'/>");
  ASSERT_TRUE(value.xsd_samples);
  ASSERT_EQ(value.xsd_samples->size(), 2U);
  EXPECT_EQ(value.xsd_samples->front(), 2U);
  EXPECT_EQ(value.xsd_samples->back(), 8U);
  ASSERT_TRUE(value.xsd_selections);
  ASSERT_EQ(value.xsd_selections->size(), 2U);
  ASSERT_TRUE(value.xsd_codes);
  EXPECT_EQ(value.xsd_codes->front(), "AB");
  const auto restored = xml::deserialize<model>(xml::serialize(value));
  EXPECT_EQ(restored.xsd_samples, value.xsd_samples);
  EXPECT_EQ(restored.xsd_codes, value.xsd_codes);
  ASSERT_TRUE(restored.xsd_selections);
  EXPECT_EQ(restored.xsd_selections->size(), value.xsd_selections->size());
  for (const auto invalid : {
      "<order id='1' samples='1 2'/>", "<order id='1' samples='2 9'/>",
      "<order id='1' samples='2'/>", "<order id='1' samples='2 3 4'/>",
      "<order id='1' selections='missing:item'/>", "<order id='1' selections='-1'/>",
      "<order id='1' codes='AB bad'/>", "<order id='1' codes='A'/>",
      "<order id='1' codes='AB CD1'/>"}) {
    EXPECT_THROW((void)xml::deserialize<model>(invalid), std::invalid_argument) << invalid;
  }
  auto edited = value;
  edited.xsd_samples->front() = 1;
  EXPECT_THROW((void)xml::serialize(edited), std::invalid_argument);
  edited = value;
  edited.xsd_codes->front() = "bad";
  EXPECT_THROW((void)xml::serialize(edited), std::invalid_argument);
}
