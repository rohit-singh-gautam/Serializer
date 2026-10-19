// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <shared_import.hpp>
#include <rohit/xml.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;
using model = shared_import::xsd_order;
constexpr std::string_view fixture_document{
    "<order id='9007199254740993' label='Hardware'><item name='Bolt'>"
    "<price unit='USD'>7</price></item><item name='Nut'><price>2</price></item></order>"};
} // namespace

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
