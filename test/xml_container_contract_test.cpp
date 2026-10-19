// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_container_contract.hpp"

#include <rohit/stream.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace serializer = rohit::serializer;
namespace binding = serializer::writer::xml_binding;
namespace xml = serializer::xml;
constexpr std::string_view xsi_namespace{"http://www.w3.org/2001/XMLSchema-instance"};
constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};

// Parse real native declarations so generic applications, enum resolution and aliases stay covered.
auto parse_model(std::string_view fields, std::string_view declarations = {}) {
  const auto source = std::string{"serializer version 1.20.0; "} + std::string{declarations} +
      " class sample { " + std::string{fields} + " }";
  return serializer::parser::parse(rohit::make_constant_stream(source.data(), source.size()));
}

// Borrow a resolved native member while its parsed syntax tree remains alive.
const serializer::member& member_at(
    const std::vector<std::unique_ptr<serializer::syntax_node>>& model, std::size_t index = 0U) {
  return static_cast<const serializer::class_node&>(*model.back()).member_list.at(index);
}

// Bind the explicit entry/key/value contract shared by fixture documents.
binding::map_contract map_contract_for(const serializer::member& field,
    binding::map_key_placement placement = binding::map_key_placement::element) {
  return binding::bind_map_contract(field, xml::name{"items", "urn:map"},
      {"entry", "urn:map"}, {"key", placement == binding::map_key_placement::attribute ? "" : "urn:map"},
      {"value", "urn:map"}, placement);
}

// Construct normalized elements directly so reference validation also runs with providers disabled.
xml::element scalar(xml::name name, std::string text) {
  xml::element result{};
  result.qualified_name = std::move(name);
  result.text = std::move(text);
  return result;
}

// Build one map document without bypassing its configured expanded-name placement.
xml::element map_document(const binding::map_contract& contract,
    const std::vector<std::string>& keys) {
  xml::element result{};
  result.qualified_name = contract.wrapper;
  for (const auto& key : keys) {
    xml::element entry{};
    entry.qualified_name = contract.entry;
    if (contract.key_placement == binding::map_key_placement::attribute) {
      entry.attributes.push_back({contract.key, key});
    } else { entry.children.push_back(scalar(contract.key, key)); }
    entry.children.push_back(scalar(contract.value, "payload"));
    result.children.push_back(std::move(entry));
  }
  return result;
}

// Create a single variant payload under the required field wrapper.
xml::element variant_document(const binding::variant_contract& contract, std::size_t index) {
  xml::element result{};
  result.qualified_name = contract.wrapper;
  const auto& alternative = contract.alternatives.at(index);
  auto child = scalar(contract.selection == binding::variant_selection::element
      ? alternative.tag : contract.wrapper, "payload");
  child.namespace_bindings.emplace("t", alternative.type_identity.namespace_uri);
  child.attributes.push_back({{"type", std::string{xsi_namespace}},
      "t:" + alternative.type_identity.local});
  result.children.push_back(std::move(child));
  return result;
}
} // namespace

// Legacy and generic containers bind the same XML layout without changing native field metadata.
TEST(xml_container_contract, legacy_generic_and_optional_maps_keep_native_identity) {
  const auto model = parse_model("public map(string) uint32 legacy (4); "
      "public map<string,uint32> generic (8); public optional<map<string,uint32>> maybe (12);");
  const auto legacy = map_contract_for(member_at(model));
  const auto generic = map_contract_for(member_at(model, 1U));
  const auto optional = map_contract_for(member_at(model, 2U));
  EXPECT_EQ(legacy.key_type.name, generic.key_type.name);
  EXPECT_EQ(legacy.value_type.name, generic.value_type.name);
  EXPECT_FALSE(generic.optional_container);
  EXPECT_TRUE(optional.optional_container);
  EXPECT_EQ(member_at(model).id, 4U);
  EXPECT_EQ(member_at(model, 2U).id, 12U);
  EXPECT_NO_THROW(binding::validate_map_entries(optional, map_document(optional, {})));
}

// Only portable scalar key identities and explicit value presence can enter the shared contract.
TEST(xml_container_contract, unsupported_key_value_types_and_ambiguous_names_reject) {
  const auto model = parse_model("public map<string,uint32> valid; "
      "public map<uint32,string> floating; public map<string,optional<uint32>> optional_value; "
      "public uint32 scalar;");
  // Native parsing already rejects floating map keys; also guard compiler IR supplied by API clients.
  auto floating = member_at(model, 1U);
  floating.type_name_list.front().arguments.front().name = "double";
  EXPECT_THROW((void)map_contract_for(floating), std::invalid_argument);
  EXPECT_THROW((void)map_contract_for(member_at(model, 2U)), std::invalid_argument);
  try {
    (void)map_contract_for(member_at(model, 2U));
    FAIL() << "Optional map values must require an explicit presence contract";
  } catch (const std::invalid_argument& error) {
    EXPECT_NE(std::string_view{error.what()}.find("per-value presence contract"),
        std::string_view::npos);
  }
  EXPECT_THROW((void)map_contract_for(member_at(model, 3U)), std::invalid_argument);
  EXPECT_THROW((void)binding::bind_map_contract(member_at(model), {}, {"entry", ""},
      {"key", ""}, {"value", ""}, binding::map_key_placement::element), std::invalid_argument);
  EXPECT_THROW((void)binding::bind_map_contract(member_at(model), xml::name{"items", ""},
      {"entry", ""}, {"same", ""}, {"same", ""}, binding::map_key_placement::element),
      std::invalid_argument);
  EXPECT_NO_THROW((void)binding::bind_map_contract(member_at(model), xml::name{"items", ""},
      {"entry", ""}, {"same", ""}, {"same", ""}, binding::map_key_placement::attribute));
  for (const auto& key : {xml::name{"xmlns", ""}, xml::name{"nil", std::string{xsi_namespace}},
      xml::name{"type", std::string{xsi_namespace}}}) {
    EXPECT_THROW((void)binding::bind_map_contract(member_at(model), xml::name{"items", ""},
        {"entry", ""}, key, {"value", ""}, binding::map_key_placement::attribute),
        std::invalid_argument);
  }
}

// Keys compare by native values, including signs, zero padding and boolean lexical aliases.
TEST(xml_container_contract, canonical_key_values_detect_numeric_and_boolean_aliases) {
  const auto model = parse_model("public map<int32,string> signed_keys; "
      "public map<uint64,string> unsigned_keys; public map<bool,string> booleans;");
  const auto signed_keys = map_contract_for(member_at(model));
  EXPECT_EQ(binding::canonical_map_key(signed_keys, " +0001 \n"), "1");
  EXPECT_EQ(binding::canonical_map_key(signed_keys, "-0"), "0");
  EXPECT_EQ(binding::canonical_map_key(signed_keys, "-2147483648"), "-2147483648");
  EXPECT_THROW((void)binding::canonical_map_key(signed_keys, "2147483648"), std::out_of_range);
  EXPECT_THROW((void)binding::canonical_map_key(signed_keys, "-2147483649"), std::out_of_range);
  EXPECT_THROW((void)binding::canonical_map_key(signed_keys, "1 2"), std::invalid_argument);
  EXPECT_THROW((void)binding::canonical_map_key(signed_keys, "--1"), std::invalid_argument);
  EXPECT_THROW(binding::validate_map_entries(signed_keys, map_document(signed_keys, {"01", "1"})),
      std::invalid_argument);
  const auto unsigned_keys = map_contract_for(member_at(model, 1U));
  EXPECT_EQ(binding::canonical_map_key(unsigned_keys, "18446744073709551615"),
      "18446744073709551615");
  EXPECT_THROW((void)binding::canonical_map_key(unsigned_keys, "18446744073709551616"),
      std::out_of_range);
  EXPECT_THROW((void)binding::canonical_map_key(unsigned_keys, "-1"), std::out_of_range);
  const auto booleans = map_contract_for(member_at(model, 2U));
  EXPECT_THROW(binding::validate_map_entries(booleans, map_document(booleans, {"true", "1"})),
      std::invalid_argument);
  EXPECT_NO_THROW(binding::validate_map_entries(booleans, map_document(booleans, {"false", "true"})));
  EXPECT_NO_THROW(binding::validate_map_entries(booleans, map_document(booleans, {"1"}), {1U, 4U}));
  EXPECT_THROW(binding::validate_map_entries(booleans, map_document(booleans, {"1"}), {1U, 3U}),
      std::length_error);
}

// Integer endpoints use each declared native width rather than an arbitrary parser accumulator.
TEST(xml_container_contract, integer_key_widths_have_exact_native_boundaries) {
  const auto model = parse_model("public map<int8,string> signed_byte; "
      "public map<uint8,string> byte; public map<int16,string> short_key; "
      "public map<uint16,string> ushort_key; public map<uint32,string> uint_key; "
      "public map<int64,string> long_key;");
  const std::vector<std::string> maxima{"127", "255", "32767", "65535", "4294967295",
      "9223372036854775807"};
  const std::vector<std::string> overflow{"128", "256", "32768", "65536", "4294967296",
      "9223372036854775808"};
  for (std::size_t index{}; index < maxima.size(); ++index) {
    const auto contract = map_contract_for(member_at(model, index));
    EXPECT_EQ(binding::canonical_map_key(contract, maxima[index]), maxima[index]);
    EXPECT_THROW((void)binding::canonical_map_key(contract, overflow[index]), std::out_of_range);
  }
  EXPECT_EQ(binding::canonical_map_key(map_contract_for(member_at(model, 5U)),
      "-9223372036854775808"), "-9223372036854775808");
}

// String/enum keys preserve native value identity instead of applying numeric normalization.
TEST(xml_container_contract, string_and_enum_key_contracts_preserve_exact_values) {
  const auto model = parse_model("public map<string,string> strings; public map<color,string> colors;",
      "enum color { red, green }");
  const auto strings = map_contract_for(member_at(model));
  EXPECT_NO_THROW(binding::validate_map_entries(strings, map_document(strings, {"01", "1", ""})));
  EXPECT_THROW(binding::validate_map_entries(strings, map_document(strings, {"same", "same"})),
      std::invalid_argument);
  const auto colors = map_contract_for(member_at(model, 1U));
  EXPECT_EQ(colors.key_kind, binding::map_key_kind::enumeration);
  EXPECT_NO_THROW(binding::validate_map_entries(colors, map_document(colors, {"red", "green"})));
  EXPECT_THROW((void)binding::canonical_map_key(colors, " red "), std::invalid_argument);
  EXPECT_THROW((void)binding::canonical_map_key(colors, "blue"), std::invalid_argument);
}

// Missing, duplicate, nil and structurally malformed map slots fail before any native map mutation.
TEST(xml_container_contract, entry_presence_and_cardinality_are_exact) {
  const auto model = parse_model("public map<string,string> values;");
  const auto contract = map_contract_for(member_at(model));
  const auto valid = map_document(contract, {"key"});
  EXPECT_NO_THROW(binding::validate_map_entries(contract, valid));
  auto malformed = valid;
  malformed.children.front().children.pop_back();
  EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
  malformed = valid;
  malformed.children.front().children.erase(malformed.children.front().children.begin());
  EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
  for (std::size_t index{}; index < valid.children.front().children.size(); ++index) {
    malformed = valid;
    malformed.children.front().children.push_back(valid.children.front().children[index]);
    EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
    malformed = valid;
    malformed.children.front().children[index].attributes.push_back(
        {{"nil", std::string{xsi_namespace}}, "true"});
    EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
  }
  malformed = valid;
  malformed.children.front().children.front().children.push_back(scalar({"nested", ""}, ""));
  EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
  malformed = valid;
  malformed.children.front().children.front().text_segments.push_back({0U, "hidden different key"});
  EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
  malformed = valid;
  malformed.text_segments.push_back({0U, "hidden mixed text"});
  EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
}

// Attribute keys occupy their configured namespace and cannot be replaced by same-local children.
TEST(xml_container_contract, attribute_keys_require_exact_placement_and_expanded_names) {
  const auto model = parse_model("public map<uint32,string> values;");
  const auto contract = map_contract_for(member_at(model), binding::map_key_placement::attribute);
  const auto valid = map_document(contract, {"1", "2"});
  EXPECT_NO_THROW(binding::validate_map_entries(contract, valid));
  auto malformed = valid;
  malformed.children.front().attributes.front().qualified_name.namespace_uri = "urn:wrong";
  EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
  malformed = valid;
  malformed.children.front().attributes.push_back(malformed.children.front().attributes.front());
  EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
  malformed = valid;
  malformed.children.front().attributes.clear();
  malformed.children.front().children.push_back(scalar(contract.key, "1"));
  EXPECT_THROW(binding::validate_map_entries(contract, malformed), std::invalid_argument);
  EXPECT_THROW(binding::validate_map_entries(contract, map_document(contract, {"01", "1"})),
      std::invalid_argument);
}

// Reference validators honor logical resource limits, cancellation and deadlines cumulatively.
TEST(xml_container_contract, map_limits_and_shared_context_propagate) {
  const auto model = parse_model("public map<string,string> values;");
  const auto contract = map_contract_for(member_at(model));
  const auto document = map_document(contract, {"a", "b"});
  EXPECT_NO_THROW(binding::validate_map_entries(contract, document, {2U, 2U}));
  EXPECT_THROW(binding::validate_map_entries(contract, document, {1U, 2U}), std::length_error);
  EXPECT_THROW(binding::validate_map_entries(contract, document, {2U, 1U}), std::length_error);
  std::size_t work{}, allocations{};
  xml::operation_context context{};
  context.charge = [&](xml::operation_resource resource, std::size_t amount) {
    if (resource == xml::operation_resource::work_units) { work += amount; }
    if (resource == xml::operation_resource::allocation_bytes) { allocations += amount; }
  };
  binding::validate_map_entries(contract, document, {}, &context);
  EXPECT_GT(work, 0U);
  EXPECT_GE(allocations, 2U);
  context.is_cancelled = [] { return true; };
  EXPECT_THROW(binding::validate_map_entries(contract, document, {}, &context), std::runtime_error);
  context.is_cancelled = {};
  context.deadline = std::chrono::steady_clock::now() - std::chrono::seconds{1};
  EXPECT_THROW(binding::validate_map_entries(contract, document, {}, &context), std::runtime_error);
}

// Scalar and record map values share entry presence while record internals stay with their binder.
TEST(xml_container_contract, record_map_values_retain_native_record_contract) {
  const auto model = parse_model("public map<string,payload> values;", "class payload { public int32 id; }");
  const auto contract = map_contract_for(member_at(model));
  auto document = map_document(contract, {"entry"});
  document.children.front().children.back().text.clear();
  document.children.front().children.back().children.push_back(scalar({"id", ""}, "42"));
  EXPECT_EQ(contract.value_type.type, serializer::object_type::class_type);
  EXPECT_NO_THROW(binding::validate_map_entries(contract, document));
}

// Owning variants and legacy unions preserve aliases, native types and schema wire IDs.
TEST(xml_container_contract, variants_and_unions_bind_stable_element_aliases) {
  const auto model = parse_model("public union(int32 = code, double = real) raw (3); "
      "public variant(string = text, payload = record) owned (7);", "class payload { public uint32 id; }");
  const auto raw = binding::bind_variant_contract(member_at(model), xml::name{"raw", "urn:test"},
      binding::variant_selection::element);
  const auto owned = binding::bind_variant_contract(member_at(model, 1U), xml::name{"owned", "urn:test"},
      binding::variant_selection::element);
  ASSERT_EQ(raw.alternatives.size(), 2U);
  EXPECT_EQ(raw.alternatives.front().tag, (xml::name{"code", "urn:test"}));
  EXPECT_EQ(raw.alternatives.front().type_identity, (xml::name{"int", std::string{xsd_namespace}}));
  const auto type_only = binding::bind_variant_contract(member_at(model), raw.wrapper,
      binding::variant_selection::element, {{"code", {}, xml::name{"Code", "urn:types"}}});
  EXPECT_EQ(type_only.alternatives.front().tag, raw.alternatives.front().tag);
  EXPECT_EQ(type_only.alternatives.front().type_identity, (xml::name{"Code", "urn:types"}));
  EXPECT_EQ(owned.alternatives.back().native_identity, "payload");
  EXPECT_EQ(owned.alternatives.back().type_identity, (xml::name{"payload", "urn:serializer:native:"}));
  EXPECT_EQ(member_at(model).id, 3U);
  EXPECT_EQ(member_at(model, 1U).id, 7U);
  EXPECT_EQ(binding::select_variant_alternative(owned, variant_document(owned, 1U)), 1U);
}

// Incomplete, unknown and colliding inventories fail at binding rather than emitting partial codecs.
TEST(xml_container_contract, variant_mapping_rejects_incomplete_unknown_and_ambiguous_tags) {
  const auto model = parse_model("public variant(int32 = code, string = text) value; "
      "public variant(int32 = first, int32 = second) same_native;");
  const std::optional<xml::name> wrapper{xml::name{"value", "urn:test"}};
  const auto& field = member_at(model);
  EXPECT_THROW((void)binding::bind_variant_contract(field, {}, binding::variant_selection::element),
      std::invalid_argument);
  EXPECT_THROW((void)binding::bind_variant_contract(field, wrapper, binding::variant_selection::xsi_type),
      std::invalid_argument);
  EXPECT_THROW((void)binding::bind_variant_contract(field, wrapper, binding::variant_selection::xsi_type,
      {{"code", {}, xml::name{"Code", "urn:types"}}}), std::invalid_argument);
  EXPECT_THROW((void)binding::bind_variant_contract(field, wrapper, binding::variant_selection::element,
      {{"unknown", {"tag", ""}, {}}}), std::invalid_argument);
  EXPECT_THROW((void)binding::bind_variant_contract(field, wrapper, binding::variant_selection::element,
      {{"code", {"tag", ""}, {}}, {"code", {"other", ""}, {}}}), std::invalid_argument);
  EXPECT_THROW((void)binding::bind_variant_contract(field, wrapper, binding::variant_selection::element,
      {{"code", {"same", ""}, {}}, {"text", {"same", ""}, {}}}), std::invalid_argument);
  const auto same_native = binding::bind_variant_contract(member_at(model, 1U), wrapper,
      binding::variant_selection::element);
  EXPECT_EQ(binding::select_variant_alternative(same_native, variant_document(same_native, 0U)), 0U);
  EXPECT_EQ(binding::select_variant_alternative(same_native, variant_document(same_native, 1U)), 1U);
  EXPECT_THROW((void)binding::bind_variant_contract(member_at(model, 1U), wrapper,
      binding::variant_selection::xsi_type, {{"first", {"int", std::string{xsd_namespace}}, {}},
          {"second", {"int", std::string{xsd_namespace}}, {}}}), std::invalid_argument);
}

// Element selectors compare expanded names and reject a conflicting type even under other prefixes.
TEST(xml_container_contract, variant_element_selection_rejects_wrong_type_tag_and_presence) {
  const auto model = parse_model("public variant(int32 = code, string = text) value;");
  const auto contract = binding::bind_variant_contract(member_at(model), xml::name{"value", "urn:test"},
      binding::variant_selection::element);
  auto document = variant_document(contract, 0U);
  EXPECT_EQ(binding::select_variant_alternative(contract, document), 0U);
  document.children.front().namespace_bindings.emplace("different", std::string{xsd_namespace});
  document.children.front().attributes.front().value = "different:int";
  EXPECT_EQ(binding::select_variant_alternative(contract, document), 0U);
  document.children.front().attributes.front().value = "different:string";
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 0U);
  document.children.front().qualified_name.namespace_uri = "urn:wrong";
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 0U);
  document.children.push_back(document.children.front());
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document.children.clear();
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
}

// xsi:type selection requires a complete inventory and resolves QName values through their scope.
TEST(xml_container_contract, xsi_type_inventory_is_prefix_independent_and_checked) {
  const auto model = parse_model("public variant(int32 = code, string = text) value;");
  const auto contract = binding::bind_variant_contract(member_at(model), xml::name{"value", "urn:test"},
      binding::variant_selection::xsi_type, {{"code", {"number", "urn:types"}, {}},
          {"text", {"message", "urn:types"}, {}}});
  auto document = variant_document(contract, 1U);
  EXPECT_EQ(binding::select_variant_alternative(contract, document), 1U);
  document.children.front().namespace_bindings.emplace("different", "urn:types");
  document.children.front().attributes.front().value = "different:message";
  EXPECT_EQ(binding::select_variant_alternative(contract, document), 1U);
  document.children.front().namespace_bindings.emplace("", "urn:types");
  document.children.front().attributes.front().value = "message";
  EXPECT_EQ(binding::select_variant_alternative(contract, document), 1U);
  document.children.front().attributes.front().value = "unbound:message";
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 1U);
  document.children.front().attributes.clear();
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 1U);
  document.children.front().qualified_name = {"text", "urn:test"};
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 1U);
  document.children.front().attributes.front().value = "t:unknown";
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 1U);
  document.children.front().attributes.push_back(document.children.front().attributes.front());
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
}

// Nil payloads and malformed wrapper content cannot bypass selection or shared interruption checks.
TEST(xml_container_contract, variant_nil_wrong_roots_mixed_content_and_context_reject) {
  const auto model = parse_model("public variant(int32 = code, string = text) value;");
  const auto contract = binding::bind_variant_contract(member_at(model), xml::name{"value", "urn:test"},
      binding::variant_selection::element);
  auto document = variant_document(contract, 0U);
  document.children.front().attributes.push_back({{"nil", std::string{xsi_namespace}}, "1"});
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 0U);
  document.qualified_name.namespace_uri = "urn:wrong";
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 0U);
  document.text_segments.push_back({0U, "unexpected"});
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document), std::invalid_argument);
  document = variant_document(contract, 0U);
  xml::operation_context context{};
  context.is_cancelled = [] { return true; };
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document, &context), std::runtime_error);
  context.is_cancelled = {};
  context.charge = [](xml::operation_resource resource, std::size_t amount) {
    if (resource == xml::operation_resource::allocation_bytes && amount != 0U) {
      throw std::length_error{"Selected QName allocation budget exhausted"};
    }
  };
  EXPECT_THROW((void)binding::select_variant_alternative(contract, document, &context), std::length_error);
}
