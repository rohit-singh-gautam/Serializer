// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_content_contract.hpp"

#include <rohit/stream.hpp>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace binding = codec::writer::xml_binding;
using parsed_model = std::vector<std::unique_ptr<codec::syntax_node>>;
constexpr std::string_view xsi_namespace{"http://www.w3.org/2001/XMLSchema-instance"};

// Resolve real schema syntax through the production parser rather than synthesizing AST metadata.
parsed_model parse_model(std::string_view source) {
  return codec::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}

// Select a top-level parsed record while the owning model remains alive for borrowed contracts.
const codec::class_node& record_at(const parsed_model& model, std::size_t index = 0U) {
  return static_cast<const codec::class_node&>(*model.at(index));
}

// Create one normalized element with an xsi:nil attribute identified independently of its prefix.
codec::xml::element nil_element(std::string_view value) {
  codec::xml::element result{};
  result.qualified_name = {"value", "urn:values"};
  result.attributes.push_back({{"nil", std::string{xsi_namespace}}, std::string{value}});
  result.namespace_bindings.emplace("arbitrary", xsi_namespace);
  return result;
}

// Produce one shared native mixed shape with text and two distinct ordinary record alternatives.
parsed_model mixed_model(std::string_view fragment =
    "public variant (string = text, em = emphasis, strong = bold) value;",
    std::string_view holder = "public array fragment content;") {
  const auto source = std::string{"serializer version 1.20.0; "}
      + "class em { public string value; } class strong { public string value; } "
      + "class fragment { " + std::string{fragment} + " } class paragraph { "
      + std::string{holder} + " }";
  return parse_model(source);
}

// Bind the last record with deterministic namespace defaults and no separate text mapping.
binding::mixed_contract mixed_contract_for(const parsed_model& model) {
  binding::mixed_options options{};
  options.field = "content";
  return binding::bind_mixed_content(record_at(model, 3U), options, false, "urn:mixed");
}

// Construct normalized text/child/text without losing empty or XML-like literal text fragments.
codec::xml::element mixed_document() {
  codec::xml::element result{};
  result.qualified_name = {"paragraph", "urn:mixed"};
  result.children.push_back({{"em", "urn:mixed"}, {}, {}, "inside", {}, {}});
  result.text_segments = {{0U, "before <em/>"}, {0U, ""}, {1U, "after & done"}};
  for (const auto& segment : result.text_segments) { result.text += segment.value; }
  return result;
}
} // namespace

TEST(xml_content_contract, parses_only_exact_presence_policy_tokens) {
  EXPECT_EQ(binding::parse_presence_policy("required"), binding::presence_policy::required);
  EXPECT_EQ(binding::parse_presence_policy("absent"), binding::presence_policy::absent);
  EXPECT_EQ(binding::parse_presence_policy("nil"), binding::presence_policy::nil);
  EXPECT_EQ(binding::parse_presence_policy("absent_or_nil"), binding::presence_policy::absent_or_nil);
  for (const auto token : {"", "optional", "Required", "nil ", " absent", "absent-or-nil"}) {
    EXPECT_THROW((void)binding::parse_presence_policy(token), std::invalid_argument) << token;
  }
}

// Element-only observation drops formatting before fragment admission and rejects any text payload.
TEST(xml_content_contract, ordered_element_observation_charges_only_typed_children) {
  const auto model = mixed_model("private variant(em=first,strong=second) value;");
  binding::mixed_options options{};
  options.field = "content";
  options.element_only = true;
  const auto contract = binding::bind_mixed_content(record_at(model, 3U), options, false, "urn:mixed");
  codec::xml::element value{};
  value.qualified_name = {"paragraph", "urn:mixed"};
  value.children.push_back({{"em", "urn:mixed"}, {}, {}, "first", {}, {}});
  value.children.push_back({{"strong", "urn:mixed"}, {}, {}, "second", {}, {}});
  value.text = " \n\t";
  value.text_segments = {{0U, " "}, {1U, "\n"}, {2U, "\t"}};
  const binding::mixed_trace_limits exact{2U, 0U};
  const auto items = binding::observe_mixed_content(contract, value, exact);
  ASSERT_EQ(items.size(), 2U);
  EXPECT_EQ(items[0].child, &value.children[0]);
  EXPECT_EQ(items[1].child, &value.children[1]);
  EXPECT_THROW((void)binding::observe_mixed_content(contract, value, {1U, 0U}), std::length_error);
  value.text_segments.back().value = "unexpected";
  EXPECT_THROW((void)binding::observe_mixed_content(contract, value, exact), std::invalid_argument);
}

TEST(xml_content_contract, defaults_preserve_required_and_simple_optional_members) {
  const auto model = parse_model("serializer version 1.20.0; class value { "
      "public string required_text; public optional<string> optional_text; "
      "public array uint32 values; }");
  const auto& fields = record_at(model).member_list;
  const auto required = binding::bind_presence(fields[0]);
  EXPECT_EQ(required.policy, binding::presence_policy::required);
  EXPECT_FALSE(required.explicit_policy);
  EXPECT_EQ(required.optional_depth, 0U);
  EXPECT_EQ(required.value_type, &fields[0].type_name_list.front());
  EXPECT_TRUE(required.requires_element);
  const auto optional = binding::bind_presence(fields[1], {}, true);
  EXPECT_EQ(optional.policy, binding::presence_policy::absent);
  EXPECT_TRUE(optional.allows_absent);
  EXPECT_FALSE(optional.allows_nil);
  EXPECT_EQ(optional.optional_depth, 1U);
  EXPECT_TRUE(binding::bind_presence(fields[2]).collection);
  EXPECT_THROW((void)binding::bind_presence(fields[2], binding::presence_policy::required),
      std::invalid_argument);
  EXPECT_EQ(binding::classify_presence(optional, nullptr), binding::presence_state::absent);
  EXPECT_THROW((void)binding::classify_presence(required, nullptr), std::invalid_argument);
}

TEST(xml_content_contract, binds_exact_native_optional_depth_for_each_explicit_state_contract) {
  const auto model = parse_model("serializer version 1.20.0; class value { "
      "public string plain; public optional<string> optional; "
      "public optional<optional<string>> nested; public optional<optional<optional<string>>> deep; }");
  const auto& fields = record_at(model).member_list;
  const std::array policies{binding::presence_policy::required, binding::presence_policy::absent,
      binding::presence_policy::nil, binding::presence_policy::absent_or_nil};
  for (const auto policy : policies) {
    for (std::size_t index{}; index < fields.size(); ++index) {
      const auto expected_depth = policy == binding::presence_policy::required ? 0U :
          policy == binding::presence_policy::absent_or_nil ? 2U : 1U;
      if (index == expected_depth) {
        const auto contract = binding::bind_presence(fields[index], policy);
        EXPECT_TRUE(contract.explicit_policy);
        EXPECT_EQ(contract.optional_depth, expected_depth);
        EXPECT_EQ(contract.requires_element, policy == binding::presence_policy::required ||
            policy == binding::presence_policy::nil);
      } else {
        EXPECT_THROW((void)binding::bind_presence(fields[index], policy), std::invalid_argument);
      }
    }
  }
  EXPECT_THROW((void)binding::bind_presence(fields[2]), std::invalid_argument);
  EXPECT_THROW((void)binding::bind_presence(fields[3]), std::invalid_argument);
}

TEST(xml_content_contract, rejects_nil_attributes_optional_text_and_unwrapped_optional_collections) {
  const auto model = parse_model("serializer version 1.20.0; class value { "
      "public optional<string> text; public optional<array<string>> values; "
      "public array optional<string> items; public array<array<string>> nested; }");
  const auto& fields = record_at(model).member_list;
  EXPECT_THROW((void)binding::bind_presence(fields[0], binding::presence_policy::nil, true),
      std::invalid_argument);
  EXPECT_THROW((void)binding::bind_presence(fields[0], binding::presence_policy::nil, false, true),
      std::invalid_argument);
  EXPECT_THROW((void)binding::bind_presence(fields[0], binding::presence_policy::absent, false, true),
      std::invalid_argument);
  EXPECT_THROW((void)binding::bind_presence(fields[1], binding::presence_policy::absent),
      std::invalid_argument);
  const auto wrapped = binding::bind_presence(fields[1], binding::presence_policy::absent,
      false, false, true);
  EXPECT_TRUE(wrapped.collection);
  EXPECT_TRUE(wrapped.wrapper);
  EXPECT_TRUE(wrapped.value_type->is_sequence());
  EXPECT_THROW((void)binding::bind_presence(fields[2]), std::invalid_argument);
  EXPECT_THROW((void)binding::bind_presence(fields[3]), std::invalid_argument);
}

TEST(xml_content_contract, distinguishes_absent_empty_nil_and_false_nil_without_inference) {
  const auto model = parse_model("serializer version 1.20.0; class value { "
      "public optional<optional<string>> text; }");
  const auto contract = binding::bind_presence(record_at(model).member_list.front(),
      binding::presence_policy::absent_or_nil);
  codec::xml::element empty{};
  EXPECT_EQ(binding::classify_presence(contract, nullptr), binding::presence_state::absent);
  EXPECT_EQ(binding::classify_presence(contract, &empty), binding::presence_state::value);
  for (const auto lexical : {"true", "1", " \ttrue\r\n"}) {
    const auto node = nil_element(lexical);
    EXPECT_EQ(binding::classify_presence(contract, &node), binding::presence_state::nil);
  }
  for (const auto lexical : {"false", "0", " false "}) {
    auto node = nil_element(lexical);
    node.text = "ordinary content";
    EXPECT_EQ(binding::classify_presence(contract, &node), binding::presence_state::value);
  }
  empty.attributes.push_back({{"nil", "urn:unrelated"}, "true"});
  EXPECT_EQ(binding::classify_presence(contract, &empty), binding::presence_state::value);
}

TEST(xml_content_contract, rejects_invalid_duplicate_or_content_bearing_nil) {
  const auto model = parse_model("serializer version 1.20.0; class value { "
      "public optional<string> text; }");
  const auto& field = record_at(model).member_list.front();
  const auto nil = binding::bind_presence(field, binding::presence_policy::nil);
  const auto absent = binding::bind_presence(field, binding::presence_policy::absent);
  EXPECT_THROW((void)binding::classify_presence(nil, nullptr), std::invalid_argument);
  for (const auto lexical : {"", "True", "TRUE", "yes", "2", "true false"}) {
    const auto node = nil_element(lexical);
    EXPECT_THROW((void)binding::classify_presence(nil, &node), std::invalid_argument) << lexical;
  }
  auto node = nil_element("true");
  EXPECT_THROW((void)binding::classify_presence(absent, &node), std::invalid_argument);
  node.attributes.push_back(node.attributes.front());
  EXPECT_THROW((void)binding::classify_presence(nil, &node), std::invalid_argument);
  node = nil_element("true");
  node.text = " ";
  EXPECT_THROW((void)binding::classify_presence(nil, &node), std::invalid_argument);
  node.text.clear();
  node.text_segments.push_back({0U, "unaggregated"});
  EXPECT_THROW((void)binding::classify_presence(nil, &node), std::invalid_argument);
  node.text_segments.clear();
  node.children.emplace_back();
  EXPECT_THROW((void)binding::classify_presence(nil, &node), std::invalid_argument);
}

TEST(xml_content_contract, empty_collection_wrapper_is_a_value_state) {
  const auto model = parse_model("serializer version 1.20.0; class value { "
      "public optional<optional<array<string>>> values; }");
  const auto contract = binding::bind_presence(record_at(model).member_list.front(),
      binding::presence_policy::absent_or_nil, false, false, true);
  codec::xml::element wrapper{};
  EXPECT_EQ(binding::classify_presence(contract, &wrapper), binding::presence_state::value);
  EXPECT_EQ(binding::classify_presence(contract, nullptr), binding::presence_state::absent);
  wrapper = nil_element("true");
  EXPECT_EQ(binding::classify_presence(contract, &wrapper), binding::presence_state::nil);
}

TEST(xml_content_contract, preserves_legacy_optional_lexical_list_presence_without_a_wrapper) {
  const auto model = parse_model("serializer version 1.20.0; class value { "
      "public optional<array<double>> values; "
      "public optional<optional<array<double>>> nested; "
      "public optional<array<optional<double>>> optional_items; }");
  const auto& fields = record_at(model).member_list;
  for (const bool attribute : {false, true}) {
    const auto contract = binding::bind_presence(fields.front(), {}, attribute, false, false, true);
    EXPECT_TRUE(contract.collection);
    EXPECT_TRUE(contract.lexical_list);
    EXPECT_FALSE(contract.wrapper);
    EXPECT_FALSE(contract.explicit_policy);
    EXPECT_EQ(contract.optional_depth, 1U);
    EXPECT_TRUE(contract.allows_absent);
    EXPECT_FALSE(contract.allows_nil);
    codec::xml::element empty_list{};
    EXPECT_EQ(binding::classify_presence(contract, nullptr), binding::presence_state::absent);
    EXPECT_EQ(binding::classify_presence(contract, &empty_list), binding::presence_state::value);
    for (const auto policy : {binding::presence_policy::required, binding::presence_policy::absent,
        binding::presence_policy::nil, binding::presence_policy::absent_or_nil}) {
      EXPECT_THROW((void)binding::bind_presence(fields.front(), policy,
          attribute, false, false, true), std::invalid_argument);
    }
  }
  EXPECT_THROW((void)binding::bind_presence(fields[1], {}, false, false, false, true),
      std::invalid_argument);
  EXPECT_THROW((void)binding::bind_presence(fields[2], {}, false, false, false, true),
      std::invalid_argument);
}

TEST(xml_content_contract, binds_native_ordered_variant_storage_and_owns_overrides) {
  const auto model = mixed_model();
  binding::mixed_contract contract{};
  {
    binding::mixed_options options{};
    options.field = "content";
    options.children.emplace("emphasis", codec::xml::name{"italic", "urn:format"});
    contract = binding::bind_mixed_content(record_at(model, 3U), options, false, "urn:mixed");
  }
  EXPECT_EQ(contract.source, &record_at(model, 3U).member_list.front());
  EXPECT_EQ(contract.fragment, &record_at(model, 2U));
  EXPECT_EQ(contract.variant, &record_at(model, 2U).member_list.front());
  EXPECT_EQ(contract.text_branch, "text");
  EXPECT_EQ(contract.text_alternative, 0U);
  ASSERT_EQ(contract.children.size(), 2U);
  EXPECT_EQ(contract.children[0].branch, "emphasis");
  EXPECT_EQ(contract.children[0].qualified_name, (codec::xml::name{"italic", "urn:format"}));
  EXPECT_EQ(contract.children[1].qualified_name, (codec::xml::name{"strong", "urn:mixed"}));
  EXPECT_EQ(contract.children[1].alternative, 2U);
}

TEST(xml_content_contract, binds_generic_array_spelling_to_the_same_ordered_contract) {
  const auto model = mixed_model("public variant (string = text, em = emphasis) value;",
      "public array<fragment> content;");
  EXPECT_NO_THROW((void)mixed_contract_for(model));
}

TEST(xml_content_contract, rejects_parallel_text_missing_selectors_and_unrelated_durable_fields) {
  const auto model = mixed_model();
  binding::mixed_options options{};
  options.field = "content";
  EXPECT_THROW((void)binding::bind_mixed_content(record_at(model, 3U), options, true),
      std::invalid_argument);
  options.field = "missing";
  EXPECT_THROW((void)binding::bind_mixed_content(record_at(model, 3U), options),
      std::invalid_argument);
  options.field = "content";
  options.variant_member = "missing";
  EXPECT_THROW((void)binding::bind_mixed_content(record_at(model, 3U), options),
      std::invalid_argument);
  const auto unrelated = mixed_model(
      "public variant (string = text, em = emphasis) value; public uint32 unrelated;");
  EXPECT_THROW((void)mixed_contract_for(unrelated), std::invalid_argument);
}

TEST(xml_content_contract, rejects_ambiguous_text_raw_unions_and_scalar_or_nested_alternatives) {
  for (const auto fields : {
      "public variant (string = text, string = more, em = emphasis) value;",
      "public variant (em = emphasis, strong = bold) value;",
      "public variant (string = other, em = emphasis) value;",
      "public union (string = text, em = emphasis) value;",
      "public variant (string = text, uint32 = number) value;",
      "public variant (string = text, optional<em> = emphasis) value;",
      "public variant (string = text, array<em> = emphasis) value;"}) {
    const auto model = mixed_model(fields);
    EXPECT_THROW((void)mixed_contract_for(model), std::invalid_argument) << fields;
  }
  for (const auto field : {"public fragment content;", "public optional<array<fragment>> content;",
      "public array optional<fragment> content;", "public array<array<fragment>> content;"}) {
    const auto model = mixed_model("public variant (string = text, em = emphasis) value;", field);
    EXPECT_THROW((void)mixed_contract_for(model), std::invalid_argument) << field;
  }
}

TEST(xml_content_contract, rejects_unknown_overrides_colliding_names_and_invalid_xml_names) {
  const auto model = mixed_model();
  binding::mixed_options options{};
  options.field = "content";
  options.children.emplace("unknown", codec::xml::name{"child", ""});
  EXPECT_THROW((void)binding::bind_mixed_content(record_at(model, 3U), options),
      std::invalid_argument);
  options.children.clear();
  options.children.emplace("emphasis", codec::xml::name{"same", "urn:same"});
  options.children.emplace("bold", codec::xml::name{"same", "urn:same"});
  EXPECT_THROW((void)binding::bind_mixed_content(record_at(model, 3U), options),
      std::invalid_argument);
  options.children["bold"].namespace_uri = "urn:different";
  EXPECT_NO_THROW((void)binding::bind_mixed_content(record_at(model, 3U), options));
  options.children["bold"].local = "1invalid";
  EXPECT_THROW((void)binding::bind_mixed_content(record_at(model, 3U), options),
      std::invalid_argument);
}

TEST(xml_content_contract, observes_text_child_text_order_empty_segments_and_literal_xml_text) {
  const auto model = mixed_model();
  const auto contract = mixed_contract_for(model);
  const auto document = mixed_document();
  const auto trace = binding::observe_mixed_content(contract, document);
  ASSERT_EQ(trace.size(), 4U);
  EXPECT_EQ(trace[0].kind, binding::mixed_item_kind::text);
  EXPECT_EQ(trace[0].text, "before <em/>");
  EXPECT_EQ(trace[0].text.data(), document.text_segments[0].value.data());
  EXPECT_EQ(trace[1].kind, binding::mixed_item_kind::text);
  EXPECT_TRUE(trace[1].text.empty());
  EXPECT_EQ(trace[2].kind, binding::mixed_item_kind::child);
  EXPECT_EQ(trace[2].child, &document.children.front());
  EXPECT_EQ(trace[3].text, "after & done");
  const auto bytes = codec::xml::write(document);
  EXPECT_NE(bytes.find("before &lt;em/&gt;"), std::string::npos);
  EXPECT_NE(bytes.find("after &amp; done"), std::string::npos);
}

TEST(xml_content_contract, rejects_unknown_children_and_malformed_mixed_fragment_discriminators) {
  const auto model = mixed_model();
  const auto contract = mixed_contract_for(model);
  auto document = mixed_document();
  document.children.front().qualified_name.namespace_uri = "urn:unknown";
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document), std::invalid_argument);
  const std::array missing_child{binding::mixed_item_view{binding::mixed_item_kind::child, {}, nullptr}};
  EXPECT_THROW(binding::validate_mixed_trace(contract, missing_child), std::invalid_argument);
  const std::array extra_child{binding::mixed_item_view{
      binding::mixed_item_kind::text, "text", &document.children.front()}};
  EXPECT_THROW(binding::validate_mixed_trace(contract, extra_child), std::invalid_argument);
}

TEST(xml_content_contract, rejects_lossy_unordered_or_inconsistent_mixed_text_observations) {
  const auto model = mixed_model();
  const auto contract = mixed_contract_for(model);
  auto document = mixed_document();
  document.text_segments.clear();
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document), std::invalid_argument);
  document = mixed_document();
  document.text_segments[0].before_child = 1U;
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document), std::invalid_argument);
  document = mixed_document();
  document.text_segments.back().before_child = 2U;
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document), std::invalid_argument);
  document = mixed_document();
  document.text_segments.front().value = "altered";
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document), std::invalid_argument);
  document = mixed_document();
  document.text += "unsegmented";
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document), std::invalid_argument);
}

TEST(xml_content_contract, enforces_exact_mixed_fragment_text_and_interruption_boundaries) {
  const auto model = mixed_model();
  const auto contract = mixed_contract_for(model);
  const auto document = mixed_document();
  binding::mixed_trace_limits limits{4U, document.text.size()};
  EXPECT_NO_THROW((void)binding::observe_mixed_content(contract, document, limits));
  --limits.max_fragments;
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document, limits), std::length_error);
  ++limits.max_fragments;
  --limits.max_text_bytes;
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document, limits), std::length_error);
  codec::xml::operation_context context{};
  context.is_cancelled = [] { return true; };
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document, {}, &context),
      std::runtime_error);
  context.is_cancelled = {};
  context.deadline = std::chrono::steady_clock::now() - std::chrono::seconds{1};
  EXPECT_THROW((void)binding::observe_mixed_content(contract, document, {}, &context),
      std::runtime_error);
}
