// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <xml_projection.hpp>
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>

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

// Synchronous direct output pins weak owners through root namespace selection and every field visit.
TEST(xml_projection, direct_shared_weak_and_scalar_output_borrows_live_owning_storage) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  auto source = std::make_shared<xml_projection::source>();
  source->current.number = 19U; source->current.label = "direct & borrowed";
  xml_projection::envelope envelope{};
  envelope.value.bind(source); envelope.label.bind(*source);
  EXPECT_EQ(xml::serialize_tokens(envelope.value, bounds), xml::serialize_tokens(source->current, bounds));
  const auto exported = xml::parse(xml::serialize_tokens(envelope, bounds));
  ASSERT_EQ(exported.attributes.size(), 1U);
  EXPECT_EQ(exported.attributes[0].value, source->current.label);
  ASSERT_EQ(exported.children.size(), 1U);
  ASSERT_EQ(exported.children[0].attributes.size(), 2U);
  EXPECT_EQ(exported.children[0].attributes[1].value, source->current.label);
  EXPECT_THROW((void)xml::deserialize_tokens<decltype(envelope.value)>(
      xml::serialize_tokens(envelope.value, bounds), bounds), std::invalid_argument);

  auto qualified = std::make_shared<xml_projection::qualified_source>();
  qualified->current.value = "p:Thing";
  qualified->current.xml_namespace_bindings.emplace("p", "urn:types");
  xml_projection::weak_envelope weak{}; weak.value.bind(qualified);
  const std::weak_ptr<xml_projection::qualified_source> lifetime{qualified};
  xml::operation_context context{};
  context.charge = [&](xml::operation_resource, std::size_t) { qualified.reset(); };
  const auto bytes = xml::serialize_tokens(weak.value, bounds, &context);
  const auto root = xml::parse(bytes);
  EXPECT_EQ(xml::resolve_qname(root.text, root), (xml::name{"Thing", "urn:types"}));
  EXPECT_TRUE(lifetime.expired());
  EXPECT_THROW((void)xml::serialize_tokens(weak.value, bounds), std::logic_error);
}

// Existing whole-object dispatch follows a required target token contract through a live reference.
TEST(xml_projection, reference_graphs_keep_nil_states_on_the_required_token_path) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  auto source = std::make_shared<xml_projection::nil_source>();
  xml_projection::nil_envelope envelope{}; envelope.value.bind(source);
  static_assert(decltype(envelope.value)::xml_tokens_required);
  static_assert(xml_projection::nil_envelope::xml_tokens_supported);
  EXPECT_EQ(xml::serialize(envelope.value, bounds), xml::serialize(source->current, bounds));
  EXPECT_NE(xml::serialize(envelope, bounds).find(":nil=\"true\""), std::string::npos);
  source->current.text = "";
  EXPECT_EQ(xml::serialize(envelope.value, bounds), xml::serialize(source->current, bounds));
  EXPECT_EQ(xml::serialize_tokens(envelope, bounds).find(":nil=\"true\""), std::string::npos);
}

// Translator field callbacks retain their target validation while avoiding an owning target copy.
TEST(xml_projection, scoped_translator_direct_output_uses_zero_tree_budget) {
  xml_projection::source source{};
  source.current.number = 23U; source.current.label = "borrowed translator";
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  source.with_project([&](const auto& view) {
    EXPECT_EQ(xml::serialize_tokens(view, bounds), xml::serialize_tokens(source.current, bounds));
  });
}
