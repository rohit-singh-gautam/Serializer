// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <xml_projection.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace {
namespace xml = rohit::serializer::xml;
}

// Record references export the same XML as their selected owning values without copying models.
TEST(xml_projection, shared_record_and_scalar_borrows_export_live_values) {
  auto source = std::make_shared<xml_projection::source>();
  source->current.number = 7U;
  source->current.label = "first";
  xml_projection::envelope envelope{};
  using record_reference = decltype(envelope.value);
  record_reference reference{};
  reference.bind(source);
  EXPECT_EQ(xml::serialize(reference), xml::serialize(source->current));
  envelope.value.bind(source);
  envelope.label.bind(*source);
  const auto first = xml::parse(xml::serialize(envelope));
  ASSERT_EQ(first.attributes.size(), 1U);
  EXPECT_EQ(first.attributes.front().value, "first");
  ASSERT_EQ(first.children.size(), 1U);
  EXPECT_EQ(first.children.front().qualified_name.local, "value");
  source->current.label = "changed";
  const auto changed = xml::parse(xml::serialize(envelope));
  EXPECT_EQ(changed.attributes.front().value, "changed");
  EXPECT_THROW((void)xml::deserialize<record_reference>(xml::serialize(reference)), std::invalid_argument);
  record_reference detached{};
  EXPECT_THROW((void)xml::serialize(detached), std::logic_error);
}

// Scoped translator getters feed the target XML mapping while the source remains borrowed.
TEST(xml_projection, scoped_translator_uses_target_xml_contract) {
  xml_projection::source source{};
  source.current.number = 8U;
  source.current.label = "translated";
  source.with_project([&](const auto& view) {
    const auto bytes = xml::serialize(view);
    EXPECT_EQ(bytes, xml::serialize(source.current));
    const auto owning = xml::deserialize<xml_projection::payload>(bytes);
    EXPECT_EQ(owning.number, source.current.number);
    EXPECT_EQ(owning.label, source.current.label);
  });
}
