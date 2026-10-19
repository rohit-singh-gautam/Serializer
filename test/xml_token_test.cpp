// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/xml_tokens.hpp>
#include <rohit/html.hpp>
#include <xml_fixture.hpp>
#include <xml_xsd_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {
namespace xml = rohit::serializer::xml;

// Exercise generated attributes, nested records, strings, enums, optionals and both array forms.
xml_fixture::order sample() {
  xml_fixture::order value{};
  value.id = 7U;
  value.customer = "R&D <Customer> \xf0\x9f\x8c\x8d";
  value.items.push_back(xml_fixture::item{"Widget", -3});
  value.items.push_back(xml_fixture::item{"", 4});
  value.note = "line 1\r\nline 2";
  value.status = xml_fixture::state::closed;
  value.codes = {4U, 65535U};
  return value;
}

// Assert representative values after both tree and direct token exchanges.
void check(const xml_fixture::order& value) {
  EXPECT_EQ(value.id, 7U);
  EXPECT_EQ(value.customer, sample().customer);
  ASSERT_EQ(value.items.size(), 2U);
  EXPECT_EQ(value.items[0].count, -3);
  EXPECT_TRUE(value.items[1].label.empty());
  ASSERT_TRUE(value.note);
  EXPECT_EQ(*value.note, "line 1\r\nline 2");
  EXPECT_EQ(value.status, xml_fixture::state::closed);
  EXPECT_EQ(value.codes[1], 65535U);
}
} // namespace

TEST(xml_tokens, generated_output_and_input_need_no_tree_budget) {
  static_assert(xml_fixture::order::xml_tokens_supported);
  xml::limits bounds{};
  bounds.max_tree_bytes = 0U;
  const auto bytes = xml::serialize_tokens(sample(), bounds);
  EXPECT_NE(bytes.find("R&amp;D &lt;Customer&gt;"), std::string::npos);
  check(xml::deserialize_tokens<xml_fixture::order>(bytes, bounds));
  check(xml::deserialize<xml_fixture::order>(bytes));
  check(xml::deserialize_tokens<xml_fixture::order>(xml::serialize(sample()), bounds));
}

TEST(xml_tokens, every_small_chunk_boundary_retains_exact_scalar_values) {
  const auto bytes = xml::serialize_tokens(sample());
  for (std::size_t chunk_bytes = 1U; chunk_bytes <= 19U; ++chunk_bytes) {
    SCOPED_TRACE(chunk_bytes);
    check(xml::deserialize_tokens<xml_fixture::order>(bytes, {}, nullptr, chunk_bytes));
  }
}

TEST(xml_tokens, absent_and_empty_optional_strings_remain_distinct) {
  auto original = sample();
  original.note.reset();
  const auto absent = xml::deserialize_tokens<xml_fixture::order>(xml::serialize_tokens(original));
  EXPECT_FALSE(absent.note);
  original.note = "";
  const auto empty = xml::deserialize_tokens<xml_fixture::order>(xml::serialize_tokens(original));
  ASSERT_TRUE(empty.note);
  EXPECT_TRUE(empty.note->empty());
}

TEST(xml_tokens, prefix_equivalence_shadowing_and_qualified_attributes_are_semantic) {
  xml::token_writer writer{};
  writer.open({"root", "urn:first"});
  writer.attribute_value({"value", "urn:attribute"}, "\t\r\n&\"");
  writer.open({"child", "urn:second"});
  writer.text("<>&\r");
  writer.close(); writer.close();
  const auto value = xml::parse(writer.finish());
  EXPECT_EQ(value.qualified_name, (xml::name{"root", "urn:first"}));
  ASSERT_EQ(value.attributes.size(), 1U);
  EXPECT_EQ(value.attributes[0].qualified_name, (xml::name{"value", "urn:attribute"}));
  EXPECT_EQ(value.attributes[0].value, "\t\r\n&\"");
  ASSERT_EQ(value.children.size(), 1U);
  EXPECT_EQ(value.children[0].qualified_name, (xml::name{"child", "urn:second"}));
  EXPECT_EQ(value.children[0].text, "<>&\r");
}

TEST(xml_tokens, exact_message_failures_do_not_publish_a_candidate) {
  const auto valid = xml::serialize_tokens(sample());
  auto destination = sample();
  destination.id = 99U;
  for (const auto& bytes : {valid + "<another/>", valid.substr(0U, valid.size() - 1U),
      std::string{"<!DOCTYPE order [<!ENTITY e 'x'>]>"} + valid}) {
    EXPECT_THROW((destination = xml::deserialize_tokens<xml_fixture::order>(bytes, {}, nullptr, 1U)), std::invalid_argument);
    EXPECT_EQ(destination.id, 99U);
  }
}

TEST(xml_tokens, wrong_order_unknown_attributes_and_collection_cardinalities_reject) {
  const std::string begin{"<o:order xmlns:o='urn:serializer:order' id='7'>"};
  const std::string end{"<o:status>closed</o:status><o:code>4</o:code><o:code>5</o:code></o:order>"};
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>(begin + "<o:unknown/>" + end), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>(begin + "<o:customer>x</o:customer><o:customer>x</o:customer>" + end), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>(begin + "<o:customer>x</o:customer><o:status>closed</o:status><o:code>4</o:code></o:order>"), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>("<o:order xmlns:o='urn:serializer:order' id='7' unknown='1'><o:customer>x</o:customer>" + end), std::invalid_argument);
}

TEST(xml_tokens, split_scalar_lexical_overflow_and_nested_scalar_content_reject) {
  for (const auto scalar : {"2147483648", "1x", "", "<nested/>"}) {
    const auto bytes = std::string{"<n>"} + scalar + "</n>";
    xml::token_reader reader{bytes, {}, nullptr, 1U};
    int value{};
    EXPECT_THROW(xml::read_token_value(reader, value), std::invalid_argument);
  }
}

TEST(xml_tokens, floating_underflow_rounds_to_signed_zero_without_accepting_overflow) {
  for (const auto text : {"1e-999", "+1e-999", "-1e-999"}) {
    const auto bytes = std::string{"<v>"} + text + "</v>";
    xml::token_reader reader{bytes, {}, nullptr, 1U};
    double value{1.0}; xml::read_token_value(reader, value);
    EXPECT_EQ(value, 0.0);
    EXPECT_EQ(std::signbit(value), std::string_view{text}.front() == '-');
  }
  float single{}; xml::detail::read_scalar("1e-99", single); EXPECT_EQ(single, 0.0F);
  xml::detail::read_scalar("-1e-99", single); EXPECT_TRUE(std::signbit(single));
  xml::detail::read_scalar("1.401298464324817e-45", single);
  EXPECT_EQ(single, std::numeric_limits<float>::denorm_min());
  double wide{}; xml::detail::read_scalar("4.9406564584124654e-324", wide);
  EXPECT_EQ(wide, std::numeric_limits<double>::denorm_min());
  EXPECT_THROW(xml::detail::read_scalar("1e999", wide), std::invalid_argument);
  EXPECT_THROW(xml::detail::read_scalar("-1e999", wide), std::invalid_argument);
  EXPECT_THROW(xml::detail::read_scalar("-+1e-999", wide), std::invalid_argument);
}

TEST(xml_tokens, limits_cancellation_and_output_failure_fence_are_enforced) {
  const auto valid = xml::serialize_tokens(sample());
  xml::limits bounds{};
  bounds.max_input_bytes = valid.size() - 1U;
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>(valid, bounds), std::length_error);
  bounds = {}; bounds.max_output_bytes = valid.size() - 1U;
  EXPECT_THROW((void)xml::serialize_tokens(sample(), bounds), std::length_error);
  bounds = {}; bounds.max_depth = 1U;
  EXPECT_THROW((void)xml::serialize_tokens(sample(), bounds), std::length_error);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>(valid, bounds), std::length_error);
  bounds = {}; bounds.max_text_bytes = 1U;
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>(valid, bounds), std::length_error);
  xml::operation_context operation{};
  operation.is_cancelled = [] { return true; };
  EXPECT_THROW((void)xml::serialize_tokens(sample(), {}, &operation), std::runtime_error);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>(valid, {}, &operation), std::runtime_error);
  operation.is_cancelled = {};
  operation.deadline = std::chrono::steady_clock::now() - std::chrono::milliseconds{1};
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::order>(valid, {}, &operation), std::runtime_error);
  operation.deadline.reset();
  std::size_t remaining_allocation_bytes{128U};
  operation.charge = [&](xml::operation_resource selected, std::size_t amount) {
    if (selected != xml::operation_resource::allocation_bytes) { return; }
    if (amount > remaining_allocation_bytes) { throw std::length_error{"Injected XML allocation rejection"}; }
    remaining_allocation_bytes -= amount;
  };
  auto unchanged = sample(); unchanged.id = 99U;
  EXPECT_THROW((unchanged = xml::deserialize_tokens<xml_fixture::order>(valid, {}, &operation, 1U)), std::length_error);
  EXPECT_EQ(unchanged.id, 99U);
  xml::token_writer writer{};
  writer.open({"root", ""});
  writer.attribute_value({"a", ""}, "1");
  EXPECT_THROW(writer.attribute_value({"a", ""}, "2"), std::invalid_argument);
  EXPECT_THROW((void)writer.finish(), std::logic_error);
  xml::token_reader reader{"<r xmlns='urn:scope'/>"};
  while (reader.current().kind != xml::event_kind::start_element) { reader.advance(); }
  bounds = {}; bounds.max_tree_bytes = 0U;
  xml::build_budget namespace_budget{bounds};
  EXPECT_THROW((void)reader.namespace_scope(&namespace_budget), std::length_error);
  EXPECT_THROW((void)reader.current(), std::logic_error);
}

TEST(xml_tokens, xsd_numeric_facets_simplecontent_and_default_attributes_are_authoritative) {
  static_assert(xml_xsd_fixture::amount::xml_tokens_supported);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const auto read = [&](std::string_view value) {
    return xml::deserialize_tokens<xml_xsd_fixture::amount>(value, bounds, nullptr, 1U);
  };
  EXPECT_EQ(read("<amount xmlns='urn:serializer:xsd'>9007199254740992</amount>").unit, "m");
  const auto value = read("<amount xmlns='urn:serializer:xsd' unit=' cm '>9007199254740993</amount>");
  EXPECT_EQ(value.unit, "cm");
  EXPECT_EQ(value.value, 9007199254740993ULL);
  EXPECT_EQ(read(xml::serialize_tokens(value, bounds)).value, value.value);
  EXPECT_THROW((void)read("<amount xmlns='urn:serializer:xsd'>9007199254740991</amount>"), std::invalid_argument);
  EXPECT_THROW((void)read("<amount xmlns='urn:serializer:xsd'>9007199254740994</amount>"), std::invalid_argument);
  EXPECT_THROW((void)read("<amount xmlns='urn:serializer:xsd' unit='mm'>9007199254740992</amount>"), std::invalid_argument);
  auto invalid = value; invalid.value = 9007199254740994ULL;
  EXPECT_THROW((void)xml::serialize_tokens(invalid, bounds), std::invalid_argument);
}

TEST(xml_tokens, approved_patterns_check_original_numeric_lexemes_and_unicode_lengths) {
  const auto read = [](std::string_view value) {
    return xml::deserialize_tokens<xml_xsd_fixture::patterned>(value, {}, nullptr, 1U);
  };
  const auto value = read("<patterned xmlns='urn:serializer:xsd'><count>+01</count><label>&#x1F600;</label></patterned>");
  EXPECT_EQ(value.count, 1);
  EXPECT_EQ(value.label, "\xf0\x9f\x98\x80");
  EXPECT_THROW((void)xml::serialize_tokens(value), std::invalid_argument);
  EXPECT_THROW((void)read("<patterned xmlns='urn:serializer:xsd'><count>1</count><label>A</label></patterned>"), std::invalid_argument);
  EXPECT_THROW((void)read("<patterned xmlns='urn:serializer:xsd'><count>12</count><label>AA</label></patterned>"), std::invalid_argument);
  EXPECT_NO_THROW((void)read("<patterned xmlns='urn:serializer:xsd'><count>99</count><label>A</label></patterned>"));
}

TEST(xml_tokens, authoritative_lexical_lists_and_collection_wrappers_roundtrip) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const auto lists = xml::deserialize_tokens<xml_xsd_fixture::lists>(
      "<lists xmlns='urn:serializer:xsd' values='0 1 65535'/>", bounds, nullptr, 1U);
  EXPECT_EQ(lists.values[2], 65535U);
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::lists>(xml::serialize_tokens(lists, bounds), bounds));
  EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::lists>(
      "<lists xmlns='urn:serializer:xsd' values='0 1'/>", bounds), std::invalid_argument);
  const auto wrapped = xml::deserialize_tokens<xml_xsd_fixture::wrapped>(
      "<wrapped xmlns='urn:serializer:xsd'><items><value>1</value><value>2</value></items></wrapped>", bounds, nullptr, 1U);
  ASSERT_EQ(wrapped.values.size(), 2U);
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::wrapped>(xml::serialize_tokens(wrapped, bounds), bounds));
  EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::wrapped>(
      "<wrapped xmlns='urn:serializer:xsd'><items><value>1</value><value>2</value><value>3</value><value>4</value></items></wrapped>", bounds), std::invalid_argument);
}

TEST(xml_tokens, explicit_nil_absent_empty_and_wrapped_collection_states_are_distinct) {
  static_assert(xml_fixture::nullable::xml_tokens_required);
  static_assert(xml_fixture::nullable::xml_tokens_supported);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  xml_fixture::nullable source{};
  auto result = xml::deserialize<xml_fixture::nullable>(xml::serialize(source, bounds), bounds);
  EXPECT_FALSE(result.nil_value);
  EXPECT_FALSE(result.states);
  EXPECT_FALSE(result.entries);
  source.states.emplace();
  source.entries.emplace();
  result = xml::deserialize_tokens<xml_fixture::nullable>(xml::serialize_tokens(source, bounds), bounds, nullptr, 1U);
  ASSERT_TRUE(result.states);
  EXPECT_FALSE(*result.states);
  ASSERT_TRUE(result.entries);
  EXPECT_FALSE(*result.entries);
  source.nil_value = "";
  source.states.emplace(std::string{});
  source.entries.emplace(std::vector<std::string>{});
  result = xml::deserialize<xml_fixture::nullable>(xml::serialize(source, bounds), bounds);
  ASSERT_TRUE(result.nil_value);
  EXPECT_TRUE(result.nil_value->empty());
  ASSERT_TRUE(result.states && *result.states);
  EXPECT_TRUE((**result.states).empty());
  ASSERT_TRUE(result.entries && *result.entries);
  EXPECT_TRUE((**result.entries).empty());
  source.entries.emplace(std::vector<std::string>{"a", ""});
  result = xml::deserialize_tokens<xml_fixture::nullable>(xml::serialize_tokens(source, bounds), bounds);
  ASSERT_TRUE(result.entries && *result.entries);
  EXPECT_EQ((**result.entries).size(), 2U);
  static_assert(xml_fixture::nullable_holder::xml_tokens_required);
  xml_fixture::nullable_holder holder{}; holder.value = source;
  const auto nested = xml::deserialize<xml_fixture::nullable_holder>(xml::serialize(holder, bounds), bounds);
  ASSERT_TRUE(nested.value.entries && *nested.value.entries);
  EXPECT_EQ((**nested.value.entries).size(), 2U);
}

TEST(xml_tokens, nil_markers_use_expanded_identity_and_reject_malformed_or_nonempty_payloads) {
  const std::string begin{"<nullable xmlns='urn:serializer:order' xmlns:n='http://www.w3.org/2001/XMLSchema-instance'>"};
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::nullable>(begin + "<nil_value n:nil='0'>x</nil_value></nullable>"));
  for (const auto fragment : {"", "<nil_value n:nil='yes'/>", "<nil_value n:nil='true'> </nil_value>",
      "<nil_value n:nil='1'><child/></nil_value>", "<nil_value n:nil='true' other='1'/>"}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::nullable>(begin + fragment + "</nullable>", {}, nullptr, 1U), std::invalid_argument);
  }
}

TEST(xml_tokens, generated_string_projection_preserves_ordinary_html_for_the_independent_adapter) {
  namespace html = rohit::serializer::html;
  const std::string fragment{"<p class=note hidden>R&amp;D<br></p>"};
  const auto events = html::parse_fragment(fragment);
  const auto canonical = html::write_fragment(events);
  EXPECT_NE(canonical.find("<br>"), std::string::npos);
  EXPECT_NE(canonical.find(" hidden"), std::string::npos);
  auto value = sample(); value.customer = canonical;
  const auto copy = xml::deserialize_tokens<xml_fixture::order>(xml::serialize_tokens(value));
  EXPECT_EQ(copy.customer, canonical);
  EXPECT_EQ(html::write_fragment(html::parse_fragment(copy.customer)), canonical);
  EXPECT_THROW((void)xml::parse(fragment), std::invalid_argument);
  EXPECT_THROW((void)html::parse_fragment("<script>run()</script>"), std::invalid_argument);
}

TEST(xml_tokens, maps_check_native_key_identity_complete_entries_and_optional_containers) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  xml_fixture::maps value{};
  value.values.emplace(1U, "one"); value.values.emplace(2U, "");
  const auto roundtrip = xml::deserialize<xml_fixture::maps>(xml::serialize(value, bounds), bounds);
  EXPECT_EQ(roundtrip.values, value.values);
  EXPECT_FALSE(roundtrip.optional_values);
  value.optional_values.emplace();
  const auto empty = xml::deserialize_tokens<xml_fixture::maps>(xml::serialize_tokens(value, bounds), bounds);
  ASSERT_TRUE(empty.optional_values);
  EXPECT_TRUE(empty.optional_values->empty());
  value.optional_values->emplace(4U, "four");
  const auto present = xml::deserialize_tokens<xml_fixture::maps>(xml::serialize_tokens(value, bounds), bounds, nullptr, 1U);
  EXPECT_EQ(present.optional_values, value.optional_values);
  const auto reversed = xml::deserialize_tokens<xml_fixture::maps>(
      "<maps xmlns='urn:serializer:order'><entries/><optional_entries><entry><data>four</data><key>4</key></entry></optional_entries></maps>", bounds, nullptr, 1U);
  ASSERT_TRUE(reversed.optional_values);
  EXPECT_EQ(reversed.optional_values->at(4U), "four");
  const std::string begin{"<maps xmlns='urn:serializer:order'><entries>"};
  for (const auto entries : {"<entry id='1'><data>a</data></entry><entry id='+01'><data>b</data></entry>",
      "<entry id='4294967296'><data>a</data></entry>", "<entry id='1'/>",
      "<entry><data>a</data></entry>", "<entry id='1'><data>a</data><data>b</data></entry>"}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::maps>(begin + entries + "</entries></maps>", bounds, nullptr, 1U), std::invalid_argument);
  }
  auto limited = bounds; limited.max_map_entries = 1U;
  EXPECT_THROW((void)xml::serialize_tokens(value, limited), std::length_error);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::maps>(xml::serialize_tokens(value, bounds), limited), std::length_error);
  limited = bounds; limited.max_map_key_bytes = 1U;
  EXPECT_THROW((void)xml::serialize_tokens(value, limited), std::length_error);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::maps>(xml::serialize_tokens(value, bounds), limited), std::length_error);
  limited.max_map_key_bytes = 2U;
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::maps>(begin +
      "<entry id='+01'><data>a</data></entry></entries></maps>", limited, nullptr, 1U), std::length_error);
}

TEST(xml_tokens, owning_and_raw_variants_require_one_known_typed_payload) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  xml_fixture::variants value{};
  value.value.emplace<1>("<choice>");
  value.primitive_type = xml_fixture::variants::e_primitive::number;
  std::construct_at(&value.primitive.number, 1.25);
  auto copy = xml::deserialize<xml_fixture::variants>(xml::serialize(value, bounds), bounds);
  EXPECT_EQ(copy.value.index(), 1U);
  EXPECT_EQ(std::get<1>(copy.value), "<choice>");
  EXPECT_EQ(copy.primitive_type, xml_fixture::variants::e_primitive::number);
  EXPECT_EQ(copy.primitive.number, 1.25);
  value.value.emplace<2>(xml_fixture::item{"record", 4});
  copy = xml::deserialize_tokens<xml_fixture::variants>(xml::serialize_tokens(value, bounds), bounds, nullptr, 1U);
  ASSERT_EQ(copy.value.index(), 2U);
  EXPECT_EQ(std::get<2>(copy.value).label, "record");
  const std::string begin{"<variants xmlns='urn:serializer:order' xmlns:x='http://www.w3.org/2001/XMLSchema-instance'><choice>"};
  for (const auto payload : {"", "<unknown/>", "<code>1</code><text>x</text>",
      "<code x:type='text'>1</code>", "<code>4294967296</code>"}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::variants>(begin + payload +
        "</choice><primitive><code>0</code></primitive></variants>", bounds, nullptr, 1U), std::invalid_argument);
  }
}

TEST(xml_tokens, xsi_type_variants_use_expanded_namespace_identity_and_reject_conflicts) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<typed_variants xmlns='urn:serializer:order' xmlns:x='http://www.w3.org/2001/XMLSchema-instance' xmlns:t='urn:serializer:types'><choice>"};
  const auto number = xml::deserialize<xml_fixture::typed_variants>(prefix +
      "<choice x:type='t:Code'>4294967295</choice></choice></typed_variants>", bounds);
  ASSERT_EQ(number.value.index(), 0U);
  EXPECT_EQ(std::get<0>(number.value), std::numeric_limits<std::uint32_t>::max());
  const auto text = xml::deserialize_tokens<xml_fixture::typed_variants>(prefix +
      "<choice x:type='t:Text' x:nil='false'/></choice></typed_variants>", bounds, nullptr, 1U);
  EXPECT_TRUE(std::get<1>(text.value).empty());
  xml_fixture::typed_variants record{}; record.value.emplace<2>(xml_fixture::item{"A&B", -7});
  const auto copy = xml::deserialize<xml_fixture::typed_variants>(xml::serialize(record, bounds), bounds);
  EXPECT_EQ(std::get<2>(copy.value).label, "A&B");
  EXPECT_EQ(std::get<2>(copy.value).count, -7);
  const auto inherited = xml::deserialize_tokens<xml_fixture::typed_variants>(
      "<typed_variants xmlns='urn:serializer:order' xmlns:x='http://www.w3.org/2001/XMLSchema-instance' xmlns:t='urn:wrong'><choice xmlns:t='urn:serializer:types'><choice x:type='t:Code'>7</choice></choice></typed_variants>", bounds, nullptr, 1U);
  EXPECT_EQ(std::get<0>(inherited.value), 7U);
  const auto default_namespace = xml::deserialize_tokens<xml_fixture::typed_variants>(
      "<o:typed_variants xmlns:o='urn:serializer:order' xmlns:x='http://www.w3.org/2001/XMLSchema-instance'><o:choice><o:choice xmlns='urn:serializer:types' x:type='Code'>9</o:choice></o:choice></o:typed_variants>", bounds);
  EXPECT_EQ(std::get<0>(default_namespace.value), 9U);
  for (const auto payload : {"<choice>1</choice>", "<wrong x:type='t:Code'>1</wrong>",
      "<choice x:type='t:Missing'>1</choice>", "<choice x:type='unknown:Code'>1</choice>",
      "<choice x:type='t:Code' xmlns:t='urn:wrong'>1</choice>", "<choice x:type='t:Code' x:nil='true'/>",
      "<choice x:type='t:Code'>4294967296</choice>", "<choice x:type='t:Code'>1</choice><choice x:type='t:Text'>x</choice>"}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::typed_variants>(prefix + payload +
        "</choice></typed_variants>", bounds, nullptr, 1U), std::invalid_argument);
  }
  const auto hinted = xml::deserialize_tokens<xml_fixture::variants>(
      "<variants xmlns='urn:serializer:order' xmlns:x='http://www.w3.org/2001/XMLSchema-instance' xmlns:s='http://www.w3.org/2001/XMLSchema'><choice><code x:type='s:unsignedInt'>7</code></choice><primitive><code>0</code></primitive></variants>", bounds);
  EXPECT_EQ(std::get<0>(hinted.value), 7U);
}

TEST(xml_tokens, singleton_xsd_choices_accept_exactly_one_declared_alternative) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const auto numeric = xml::deserialize_tokens<xml_xsd_fixture::choice>(
      "<choice xmlns='urn:serializer:xsd'><number>4</number></choice>", bounds, nullptr, 1U);
  EXPECT_EQ(numeric.number, 4);
  EXPECT_FALSE(numeric.text);
  const auto text = xml::deserialize_tokens<xml_xsd_fixture::choice>(
      "<choice xmlns='urn:serializer:xsd'><text></text></choice>", bounds);
  ASSERT_TRUE(text.text);
  EXPECT_TRUE(text.text->empty());
  EXPECT_NO_THROW((void)xml::serialize_tokens(text, bounds));
  EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::choice>(
      "<choice xmlns='urn:serializer:xsd'/>", bounds), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::choice>(
      "<choice xmlns='urn:serializer:xsd'><number>1</number><text>x</text></choice>", bounds), std::invalid_argument);
  auto invalid = numeric; invalid.text = "x";
  EXPECT_THROW((void)xml::serialize_tokens(invalid, bounds), std::invalid_argument);
}

TEST(xml_tokens, xsd_all_accepts_every_permutation_and_rejects_duplicates_and_missing_values) {
  static_assert(xml_xsd_fixture::permutation::xml_tokens_required);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  constexpr std::array<std::string_view, 3U> fields{
      "<id>7</id>", "<label>A</label>", "<extra>3</extra>"};
  std::array indices{0U, 1U, 2U};
  do {
    std::string bytes{"<permutation xmlns='urn:serializer:xsd'>"};
    for (const auto index : indices) { bytes += fields[index]; }
    bytes += "</permutation>";
    const auto value = xml::deserialize<xml_xsd_fixture::permutation>(bytes, bounds);
    EXPECT_EQ(value.id, 7U);
    EXPECT_EQ(value.label, "A");
    EXPECT_EQ(value.extra, 3);
    EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::permutation>(xml::serialize_tokens(value, bounds), bounds, nullptr, 1U));
  } while (std::next_permutation(indices.begin(), indices.end()));
  const auto optional = xml::deserialize_tokens<xml_xsd_fixture::permutation>(
      "<permutation xmlns='urn:serializer:xsd'><label>A</label><id>7</id></permutation>", bounds);
  EXPECT_FALSE(optional.extra);
  for (const auto content : {"<id>7</id>", "<id>7</id><label>A</label><id>8</id>",
      "<extra>3</extra><label>A</label>", "<id>7</id><label>A</label><unknown/>"}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::permutation>(
        std::string{"<permutation xmlns='urn:serializer:xsd'>"} + content + "</permutation>", bounds), std::invalid_argument);
  }
}

TEST(xml_tokens, concrete_generics_preserve_native_types_and_borrow_private_owner_fields) {
  static_assert(!std::same_as<xml_fixture::integer_box, xml_fixture::box<std::uint32_t>>);
  static_assert(!std::same_as<xml_fixture::text_box, xml_fixture::box<std::string>>);
  static_assert(xml_fixture::box<std::uint32_t>::xml_tokens_supported);
  static_assert(xml_fixture::integer_box::xml_tokens_supported);
  static_assert(xml_fixture::nested_box::xml_tokens_supported);
  static_assert(!xml_fixture::box<double>::xml_tokens_supported);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const auto number = xml::deserialize_tokens<xml_fixture::integer_box>(
      "<integer_box><value>4294967295</value></integer_box>", bounds, nullptr, 1U);
  EXPECT_EQ(number.get(), std::numeric_limits<std::uint32_t>::max());
  const auto native = xml::deserialize_tokens<xml_fixture::box<std::uint32_t>>(
      xml::serialize_tokens(xml_fixture::box<std::uint32_t>{}, bounds), bounds, nullptr, 1U);
  EXPECT_EQ(native.get(), 0U);
  const auto text = xml::deserialize_tokens<xml_fixture::text_box>(
      "<text_box><value>R&amp;D &#x1F30D;</value></text_box>", bounds, nullptr, 1U);
  EXPECT_EQ(text.get(), "R&D \xf0\x9f\x8c\x8d");
  const auto copy = xml::deserialize_tokens<xml_fixture::text_box>(xml::serialize_tokens(text, bounds), bounds);
  EXPECT_EQ(copy.get(), text.get());
  const auto legacy_text = xml::deserialize<xml_fixture::text_box>(xml::serialize(text));
  EXPECT_EQ(legacy_text.get(), text.get());
  const auto nested = xml::deserialize_tokens<xml_fixture::nested_box>(
      "<nested_box><nested><value>7</value></nested></nested_box>", bounds, nullptr, 1U);
  EXPECT_EQ(nested.nested.get(), 7U);
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::nested_box>(xml::serialize_tokens(nested, bounds), bounds));
  const auto sequence = xml::deserialize_tokens<xml_fixture::short_sequence>(
      "<short_sequence><values>4</values><values>65535</values></short_sequence>", bounds);
  EXPECT_EQ(sequence.values, (std::array<std::uint16_t, 2U>{4U, 65535U}));
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::integer_box>(
      "<text_box><value>4</value></text_box>", bounds), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::short_sequence>(
      "<short_sequence><values>4</values></short_sequence>", bounds), std::invalid_argument);
  EXPECT_THROW((void)xml::serialize_tokens(xml_fixture::box<double>{}, bounds), std::invalid_argument);
}

TEST(xml_tokens, inheritance_flattens_private_fields_and_private_bases_without_payload_copies) {
  static_assert(xml_fixture::inherited_record::xml_tokens_supported);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const auto value = xml::deserialize_tokens<xml_fixture::inherited_record>(
      "<inherited_record><identity>7</identity><label>A&amp;B</label><count>-3</count><note/></inherited_record>",
      bounds, nullptr, 1U);
  ASSERT_TRUE(value.note);
  EXPECT_TRUE(value.note->empty());
  const auto bytes = xml::serialize_tokens(value, bounds);
  const auto tree = xml::parse(bytes);
  ASSERT_EQ(tree.children.size(), 4U);
  EXPECT_EQ(tree.children[0].qualified_name.local, "identity");
  EXPECT_EQ(tree.children[0].text, "7");
  EXPECT_EQ(tree.children[1].qualified_name.local, "label");
  EXPECT_EQ(tree.children[1].text, "A&B");
  EXPECT_EQ(tree.children[2].qualified_name.local, "count");
  EXPECT_EQ(tree.children[2].text, "-3");
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::inherited_record>(bytes, bounds));
  const auto legacy = xml::deserialize<xml_fixture::inherited_record>(xml::serialize(value));
  EXPECT_EQ(xml::serialize_tokens(legacy, bounds), bytes);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::inherited_record>(
      "<inherited_record><label>A</label><identity>7</identity><count>-3</count></inherited_record>", bounds), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::inherited_record>(
      "<inherited_record><identity>7</identity><label>A</label><count>-3</count><identity>8</identity></inherited_record>", bounds), std::invalid_argument);
}

TEST(xml_tokens, generated_identity_checks_forward_keys_unique_values_and_bounded_indexes) {
  static_assert(xml_xsd_fixture::registry::xml_tokens_required);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const auto value = xml::deserialize<xml_xsd_fixture::registry>(
      "<registry xmlns='urn:serializer:xsd'><ref id='007'/><entry id='7' label=' a  b '/></registry>", bounds);
  ASSERT_EQ(value.references.size(), 1U);
  ASSERT_EQ(value.entries.size(), 1U);
  EXPECT_EQ(value.references[0].id, 7U);
  EXPECT_EQ(value.entries[0].label, "a b");
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::registry>(xml::serialize(value, bounds), bounds, nullptr, 1U));
  const std::string prefix{"<registry xmlns='urn:serializer:xsd'>"};
  for (const auto content : {"<entry id='1'/><entry id='01'/>", "<entry/>",
      "<ref id='2'/><entry id='1'/>", "<entry id='1' label='a b'/><entry id='2' label=' a  b '/>"}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::registry>(prefix + content + "</registry>", bounds, nullptr, 1U), std::invalid_argument);
  }
  auto invalid = value; invalid.references.front().id = 8U;
  EXPECT_THROW((void)xml::serialize(invalid, bounds), std::invalid_argument);
  xml::identity_limits identity_bounds{}; identity_bounds.max_tuples = 1U;
  xml::operation_context operation{}; operation.identity_bounds = &identity_bounds;
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::registry>(prefix + "<entry id='1'/></registry>", bounds, &operation));
  EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::registry>(prefix + "<entry id='1'/><entry id='2'/></registry>", bounds, &operation), std::length_error);
  EXPECT_THROW((void)xml::serialize_tokens(value, bounds, &operation), std::length_error);
}

TEST(xml_tokens, generated_nested_identity_scopes_do_not_resolve_keys_in_another_owner) {
  static_assert(xml_xsd_fixture::identity_holder::xml_tokens_required);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<identity_holder xmlns='urn:serializer:xsd'>"};
  const auto value = xml::deserialize<xml_xsd_fixture::identity_holder>(prefix +
      "<registry><ref id='1'/><entry id='1'/></registry><registry><ref id='1'/><entry id='1'/></registry></identity_holder>", bounds);
  ASSERT_EQ(value.registries.size(), 2U);
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::identity_holder>(xml::serialize(value, bounds), bounds, nullptr, 1U));
  auto unchanged = value;
  EXPECT_THROW((unchanged = xml::deserialize_tokens<xml_xsd_fixture::identity_holder>(prefix +
      "<registry><ref id='2'/></registry><registry><entry id='2'/></registry></identity_holder>", bounds, nullptr, 1U)), std::invalid_argument);
  EXPECT_EQ(unchanged.registries.size(), 2U);
  EXPECT_EQ(unchanged.registries[0].entries[0].id, 1U);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::identity_holder>(xml::serialize(value, bounds) + "x", bounds), std::invalid_argument);
}

TEST(xml_tokens, ordered_mixed_content_borrows_private_fragments_and_preserves_text_positions) {
  static_assert(xml_fixture::mixed_content::xml_tokens_required);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<m:mixed_content xmlns:m='urn:serializer:mixed' xmlns:o='urn:serializer:order' id='7'>"};
  const std::string child{"<o:item><o:label>A&amp;B</o:label><o:count>-3</o:count></o:item>"};
  const auto value = xml::deserialize_tokens<xml_fixture::mixed_content>(prefix + " lead&amp; " + child + " tail &#x1F30D; </m:mixed_content>", bounds, nullptr, 1U);
  EXPECT_EQ(value.id, 7U);
  const auto bytes = xml::serialize(value, bounds);
  xml::operation_context raw{}; raw.allow_mixed_content = true;
  const auto tree = xml::parse(bytes, {}, &raw);
  EXPECT_EQ(tree.text, " lead&  tail \xf0\x9f\x8c\x8d ");
  ASSERT_EQ(tree.children.size(), 1U);
  EXPECT_EQ(tree.children[0].qualified_name.namespace_uri, "urn:serializer:order");
  EXPECT_EQ(tree.children[0].children[0].text, "A&B");
  EXPECT_EQ(tree.children[0].children[1].text, "-3");
  ASSERT_EQ(tree.text_segments.size(), 2U);
  EXPECT_EQ(tree.text_segments[0].before_child, 0U);
  EXPECT_EQ(tree.text_segments[1].before_child, 1U);
  EXPECT_EQ(xml::serialize_tokens(xml::deserialize<xml_fixture::mixed_content>(bytes, bounds), bounds), bytes);
  bounds.max_mixed_fragments = 3U;
  bounds.max_mixed_text_bytes = tree.text.size();
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::mixed_content>(bytes, bounds, nullptr, 1U));
  EXPECT_NO_THROW((void)xml::serialize_tokens(value, bounds));
  --bounds.max_mixed_text_bytes;
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::mixed_content>(bytes, bounds), std::length_error);
  EXPECT_THROW((void)xml::serialize_tokens(value, bounds), std::length_error);
  bounds.max_mixed_text_bytes = tree.text.size(); bounds.max_mixed_fragments = 2U;
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::mixed_content>(bytes, bounds), std::length_error);
  EXPECT_THROW((void)xml::serialize_tokens(value, bounds), std::length_error);
  bounds.max_mixed_fragments = 1U;
  const auto adjacent = xml::deserialize_tokens<xml_fixture::mixed_content>(prefix + "a<![CDATA[b]]>c</m:mixed_content>", bounds, nullptr, 1U);
  EXPECT_EQ(xml::parse(xml::serialize_tokens(adjacent, bounds), {}, &raw).text, "abc");
  bounds.max_mixed_fragments = 0U;
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::mixed_content>(prefix + "</m:mixed_content>", bounds));
  EXPECT_THROW((void)xml::serialize_tokens(xml_fixture::mixed_fragment{}), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::mixed_content>(prefix + "<unknown/></m:mixed_content>"), std::invalid_argument);
}

TEST(xml_tokens, fixed_mixed_storage_normalizes_empty_text_without_retaining_an_xml_tree) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<fixed_mixed_content xmlns='urn:serializer:mixed' xmlns:o='urn:serializer:order'>"};
  const std::string child{"<o:item><o:label>A</o:label><o:count>1</o:count></o:item>"};
  const auto value = xml::deserialize_tokens<xml_fixture::fixed_mixed_content>(prefix + child + "</fixed_mixed_content>", bounds, nullptr, 1U);
  ASSERT_EQ(value.content.size(), 3U);
  EXPECT_EQ(value.content[0].xml_mixed_index(), 1U);
  EXPECT_EQ(value.content[1].xml_mixed_index(), 0U);
  EXPECT_TRUE(value.content[1].xml_mixed_text().empty());
  EXPECT_TRUE(value.content[2].xml_mixed_text().empty());
  const auto bytes = xml::serialize_tokens(value, bounds);
  EXPECT_EQ(xml::serialize_tokens(xml::deserialize_tokens<xml_fixture::fixed_mixed_content>(bytes, bounds), bounds), bytes);
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::fixed_mixed_content>(prefix + "before" + child + "after</fixed_mixed_content>", bounds, nullptr, 1U));
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::fixed_mixed_content>(prefix + "a" + child + "b" + child + "</fixed_mixed_content>", bounds), std::invalid_argument);
  bounds.max_mixed_fragments = 2U;
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::fixed_mixed_content>(bytes, bounds), std::length_error);
  EXPECT_THROW((void)xml::serialize_tokens(value, bounds), std::length_error);
}

TEST(xml_tokens, model_revision_magic_and_lifecycles_use_exact_historical_layouts) {
  static_assert(xml_fixture::model_numeric::xml_tokens_required);
  static_assert(xml_fixture::model_envelope::xml_tokens_required);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<model_numeric xmlns='urn:serializer:model-tokens' xmlns:m='urn:serializer:xml:model' kept='7' m:magic='53524c' m:revision='"};
  for (const auto revision : {"9007199254740993", "9007199254740994", "9007199254740995"}) {
    const bool historical = std::string_view{revision} == "9007199254740993";
    const auto child = historical ? std::string{"<old>retained</old>"} : std::string{"<current>selected</current>"};
    const auto value = xml::deserialize_tokens<xml_fixture::model_numeric>(prefix + revision + "'>" + child + "</model_numeric>", bounds, nullptr, 1U);
    EXPECT_EQ(value.kept, 7U);
    EXPECT_EQ(value.old, historical ? "retained" : "old-default");
    EXPECT_EQ(value.current, historical ? "new-default" : "selected");
    const auto bytes = xml::serialize(value, bounds);
    EXPECT_EQ(xml::deserialize<xml_fixture::model_numeric>(bytes, bounds).ver, value.ver);
    const auto tree = xml::parse(bytes);
    ASSERT_EQ(tree.children.size(), 1U);
    EXPECT_EQ(tree.children.front().qualified_name.local, historical ? "old" : "current");
  }
  for (const auto revision : {"9007199254740992", "9007199254740996", "+9007199254740995", "09007199254740995", "9007199254740995 "}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::model_numeric>(prefix + revision + "'><current>x</current></model_numeric>", bounds), std::invalid_argument);
  }
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::model_numeric>(prefix + "9007199254740995'><old>x</old></model_numeric>", bounds), std::invalid_argument);
  const auto valid = prefix + "9007199254740995'><current>x</current></model_numeric>";
  for (const auto& replacement : std::array<std::pair<std::string_view, std::string_view>, 5U>{
      {{" m:magic='53524c'", ""}, {"53524c", "53524C"}, {"m:revision=", "revision="},
       {" m:revision='9007199254740995'", ""}, {" kept='7'", " m:unknown='x' kept='7'"}}}) {
    auto malformed = valid;
    const auto position = malformed.find(replacement.first);
    ASSERT_NE(position, std::string::npos);
    malformed.replace(position, replacement.first.size(), replacement.second);
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::model_numeric>(malformed, bounds), std::invalid_argument);
  }
  const auto dotted = xml::deserialize_tokens<xml_fixture::model_dotted>(
      "<model_dotted xmlns='urn:serializer:model-tokens' xmlns:m='urn:serializer:xml:model' m:revision='1.2.9'><old>5</old></model_dotted>", bounds, nullptr, 1U);
  EXPECT_EQ(dotted.ver, (rohit::serializer::version3{1, 2, 9}));
  EXPECT_EQ(dotted.old, 5U);
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::model_dotted>(xml::serialize(dotted, bounds), bounds));
  const auto floating = xml::deserialize_tokens<xml_fixture::model_floating>(
      "<model_floating xmlns='urn:serializer:model-tokens' xmlns:m='urn:serializer:xml:model' m:revision='0.74999999'><current>5</current></model_floating>", bounds);
  EXPECT_EQ(floating.ver, 0.75F);
  const auto inherited = xml::deserialize_tokens<xml_fixture::model_inherited>(
      "<model_inherited xmlns='urn:serializer:model-tokens' xmlns:m='urn:serializer:xml:model' m:revision='9007199254740993' m:magic='53524c' kept='7'><old>x</old><extra>y</extra></model_inherited>", bounds);
  EXPECT_EQ(inherited.extra, "y");
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::model_inherited>(xml::serialize(inherited, bounds), bounds));
  EXPECT_NO_THROW((void)xml::deserialize_tokens<xml_fixture::model_typed_magic>(
      "<model_typed_magic xmlns='urn:serializer:model-tokens' xmlns:m='urn:serializer:xml:model' m:magic='4660'><value>9</value></model_typed_magic>", bounds));
}

TEST(xml_tokens, owning_digest_projection_preserves_exact_native_bytes_and_extent) {
  static_assert(xml_fixture::model_digests::xml_tokens_required);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<model_digests xmlns='urn:serializer:model-tokens' raw='' exact='00fF' maybe='aABB'><algorithm>"};
  const std::string content = prefix + std::string(32U, '1') + "</algorithm><items><item>AaBb</item><item>0000</item></items></model_digests>";
  const auto value = xml::deserialize_tokens<xml_fixture::model_digests>(content, bounds, nullptr, 1U);
  EXPECT_TRUE(value.raw.empty());
  EXPECT_EQ(value.exact, (std::array<std::uint8_t, 2U>{0U, 255U}));
  EXPECT_EQ(value.algorithm.front(), 17U);
  ASSERT_EQ(value.items.size(), 2U);
  EXPECT_EQ(value.items.front(), (std::array<std::uint8_t, 2U>{170U, 187U}));
  ASSERT_TRUE(value.maybe);
  const auto bytes = xml::serialize(value, bounds);
  EXPECT_EQ(xml::deserialize<xml_fixture::model_digests>(bytes, bounds).items, value.items);
  const auto tree = xml::parse(bytes);
  EXPECT_EQ(tree.attributes[1].value, "00ff");
  auto invalid = value; invalid.raw.resize(17U);
  bounds.max_digest_bytes = 16U;
  EXPECT_THROW((void)xml::serialize_tokens(invalid, bounds), std::length_error);
  for (const auto lexical : {"00", "00gg", "00 ff", "00000", "+000"}) {
    auto malformed = content;
    malformed.replace(malformed.find("00fF"), 4U, lexical);
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::model_digests>(malformed, bounds), std::invalid_argument);
  }
  bounds.max_digest_bytes = xml::limits{}.max_digest_bytes;
  auto boundary = value; boundary.raw.assign(bounds.max_digest_bytes, 255U);
  const auto boundary_bytes = xml::serialize_tokens(boundary, bounds);
  EXPECT_EQ(xml::deserialize_tokens<xml_fixture::model_digests>(boundary_bytes, bounds).raw, boundary.raw);
  boundary.raw.push_back(0U);
  EXPECT_THROW((void)xml::serialize_tokens(boundary, bounds), std::length_error);
}

TEST(xml_tokens, positioned_extensions_retain_only_foreign_payloads_and_original_namespace_context) {
  static_assert(xml_fixture::extended::xml_tokens_required);
  static_assert(xml_fixture::extension_holder::xml_tokens_required);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<o:extended xmlns:o='urn:serializer:extended' xmlns:f='urn:foreign' xmlns:s='urn:skip' xmlns:srl='urn:collision' xmlns:srl0='urn:other' xmlns='urn:qnames' id='7' f:flag='srl:Kind'>"};
  const auto value = xml::deserialize_tokens<xml_fixture::extended>(prefix +
      "<f:opaque kind='srl0:Other'>before<f:nested/>after</f:opaque><o:known>9</o:known><f:middle/><o:tail>done</o:tail><f:last/></o:extended>", bounds, nullptr, 1U);
  EXPECT_EQ(value.id, 7U); EXPECT_EQ(value.known, 9U); EXPECT_EQ(value.tail, "done");
  ASSERT_NE(value.xml_extensions.get(), nullptr);
  const auto& retained = value.xml_extensions.value();
  ASSERT_EQ(retained.attributes.size(), 1U);
  EXPECT_EQ(retained.attributes[0].value, "srl:Kind");
  EXPECT_EQ(retained.namespace_bindings.at("srl"), "urn:collision");
  ASSERT_EQ(retained.children.size(), 3U);
  EXPECT_EQ(retained.children[0].before_child, 0U);
  EXPECT_EQ(retained.children[1].before_child, 1U);
  EXPECT_EQ(retained.children[2].before_child, 2U);
  EXPECT_EQ(retained.children[0].value.namespace_bindings.at("srl0"), "urn:other");
  EXPECT_EQ(retained.children[0].value.text, "beforeafter");
  const auto bytes = xml::serialize(value, bounds);
  xml::operation_context raw{}; raw.allow_mixed_content = true;
  const auto tree = xml::parse(bytes, {}, &raw);
  ASSERT_EQ(tree.children.size(), 5U);
  EXPECT_EQ(tree.children[0].qualified_name, (xml::name{"opaque", "urn:foreign"}));
  EXPECT_EQ(tree.children[1].text, "9"); EXPECT_EQ(tree.children[3].text, "done");
  EXPECT_EQ(tree.namespace_bindings.at("srl"), "urn:collision");
  EXPECT_EQ(xml::serialize_tokens(xml::deserialize_tokens<xml_fixture::extended>(bytes, bounds), bounds), bytes);
  const auto skipped = xml::deserialize_tokens<xml_fixture::extended>(
      "<extended xmlns='urn:serializer:extended' xmlns:s='urn:skip' id='1'><s:discard><s:nested>opaque</s:nested></s:discard><known>2</known><tail>x</tail></extended>", bounds);
  EXPECT_EQ(skipped.xml_extensions.get(), nullptr);
  EXPECT_EQ(xml::parse(xml::serialize_tokens(skipped, bounds)).children.size(), 2U);
  for (const auto foreign : {"<unknown/>", "<o:unknown/>", "<r:no xmlns:r='urn:reject'/>", "<f:one/><f:two/><f:three/>"}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::extended>(prefix + foreign + "<o:known>1</o:known><o:tail>x</o:tail></o:extended>", bounds), std::exception);
  }
  auto invalid = value;
  invalid.xml_extensions.value().children.front().before_field = 99U;
  EXPECT_THROW((void)xml::serialize_tokens(invalid, bounds), std::invalid_argument);
}

// Permissive foreign slots cannot capture or replay reserved Serializer model metadata children.
TEST(xml_tokens, positioned_extensions_reject_reserved_model_children) {
  const std::string bytes{"<root xmlns='urn:core' xmlns:m='urn:serializer:xml:model'><m:future/></root>"};
  for (const auto policy : {xml::unknown_policy::skip, xml::unknown_policy::preserve}) {
    const xml::extension_contract contract{.core_namespace = "urn:core",
        .slots = {{"foreign", false, 0U, {std::string{xml::model_metadata_namespace}}, policy, 0U, 1U}}};
    xml::token_reader reader{bytes, {}, nullptr, 1U};
    while (reader.current().kind != xml::event_kind::start_element) { reader.advance(); }
    xml::extension_slot retained{};
    xml::token_extension_input input{reader, contract, retained};
    reader.advance();
    EXPECT_THROW(input.before_field(0U), std::invalid_argument);
    EXPECT_EQ(retained.get(), nullptr);
    xml::extension_store opaque{};
    xml::element child{};
    child.qualified_name = {"future", std::string{xml::model_metadata_namespace}};
    opaque.children.push_back({0U, std::move(child), 0U});
    xml::token_writer writer{};
    writer.open({"root", "urn:core"});
    EXPECT_THROW((void)xml::token_extension_output(writer, contract, opaque), std::invalid_argument);
  }
}

TEST(xml_tokens, positioned_extensions_require_capabilities_and_enforce_preservation_limits) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string bytes{"<required_extension xmlns='urn:serializer:extended' xmlns:f='urn:foreign'><f:data/><known>7</known></required_extension>"};
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::required_extension>(bytes, bounds), std::invalid_argument);
  const std::vector<std::string> understood{"urn:foreign"};
  xml::operation_context operation{}; operation.understood_namespaces = &understood;
  const auto value = xml::deserialize_tokens<xml_fixture::required_extension>(bytes, bounds, &operation, 1U);
  EXPECT_EQ(value.known, 7U);
  EXPECT_THROW((void)xml::serialize_tokens(value, bounds), std::invalid_argument);
  EXPECT_NO_THROW((void)xml::serialize_tokens(value, bounds, &operation));
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::required_extension>(
      "<required_extension xmlns='urn:serializer:extended'><known>7</known></required_extension>", bounds, &operation), std::invalid_argument);
  xml::extension_limits extension_bounds{}; extension_bounds.max_preserved_bytes = 0U;
  operation.extension_bounds = &extension_bounds;
  EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::required_extension>(bytes, bounds, &operation), std::length_error);
  EXPECT_THROW((void)xml::serialize_tokens(value, bounds, &operation), std::length_error);
  const auto empty = xml::deserialize_tokens<xml_fixture::extended>(
      "<extended xmlns='urn:serializer:extended' id='1'><known>2</known><tail>x</tail></extended>", bounds, &operation);
  EXPECT_EQ(empty.xml_extensions.get(), nullptr);
}

TEST(xml_tokens, foreign_attribute_slots_compose_with_ordered_mixed_native_fragments) {
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<m:mixed_content xmlns:m='urn:serializer:mixed' xmlns:o='urn:serializer:order' xmlns:f='urn:foreign' xmlns:p='urn:types' id='7' f:flag='p:Thing'>"};
  const std::string content{"before<o:item><o:label>A</o:label><o:count>2</o:count></o:item>after"};
  const auto value = xml::deserialize_tokens<xml_fixture::mixed_content>(prefix + content + "</m:mixed_content>", bounds, nullptr, 1U);
  ASSERT_NE(value.xml_extensions.get(), nullptr);
  ASSERT_EQ(value.xml_extensions.value().attributes.size(), 1U);
  EXPECT_EQ(value.xml_extensions.value().namespace_bindings.at("p"), "urn:types");
  const auto bytes = xml::serialize(value, bounds);
  xml::operation_context raw{}; raw.allow_mixed_content = true;
  const auto tree = xml::parse(bytes, {}, &raw);
  EXPECT_EQ(tree.text, "beforeafter");
  ASSERT_EQ(tree.text_segments.size(), 2U);
  EXPECT_EQ(tree.text_segments[0].before_child, 0U); EXPECT_EQ(tree.text_segments[1].before_child, 1U);
  EXPECT_EQ(xml::serialize_tokens(xml::deserialize_tokens<xml_fixture::mixed_content>(bytes, bounds), bounds), bytes);
  const xml::prepared_schema oracle{R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:serializer:mixed"><xs:element name="mixed_content"><xs:complexType mixed="true"><xs:sequence><xs:any namespace="urn:serializer:order" processContents="skip" minOccurs="0" maxOccurs="unbounded"/></xs:sequence><xs:attribute name="id" type="xs:unsignedInt" use="required"/><xs:anyAttribute namespace="urn:foreign urn:skip urn:reject" processContents="skip"/></xs:complexType></xs:element></xs:schema>)"};
  EXPECT_NO_THROW(oracle.validate(bytes, {}, &raw));
  for (const auto uri : {"urn:reject", "urn:unknown", "urn:serializer:mixed"}) {
    auto malformed = prefix + content + "</m:mixed_content>";
    const auto position = malformed.find("urn:foreign"); malformed.replace(position, std::string_view{"urn:foreign"}.size(), uri);
    EXPECT_THROW((void)xml::deserialize_tokens<xml_fixture::mixed_content>(malformed, bounds), std::invalid_argument);
  }
  const auto skipped = xml::deserialize_tokens<xml_fixture::mixed_content>(
      "<mixed_content xmlns='urn:serializer:mixed' xmlns:s='urn:skip' id='1' s:discard='x'>text</mixed_content>", bounds);
  EXPECT_EQ(skipped.xml_extensions.get(), nullptr);
}

TEST(xml_tokens, qualified_scalar_lists_and_schema_defaults_keep_expanded_namespace_identity) {
  static_assert(xml_xsd_fixture::qualified_names::xml_tokens_supported);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string input{"<x:qualified_names xmlns:x='urn:serializer:xsd' xmlns:p='urn:serializer:names' xmlns:q='urn:types' name='p:thing' names='p:a q:b' required='p q'>q:Type</x:qualified_names>"};
  const auto value = xml::deserialize_tokens<xml_xsd_fixture::qualified_names>(input, bounds, nullptr, 1U);
  EXPECT_EQ(value.value, "q:Type");
  ASSERT_TRUE(value.category); EXPECT_EQ(*value.category, "p:thing");
  EXPECT_EQ(value.names[0], "p:a"); EXPECT_EQ(value.names[1], "q:b");
  EXPECT_EQ(value.xml_namespace_bindings.at("q"), "urn:types");
  const auto bytes = xml::serialize_tokens(value, bounds);
  const auto tree = xml::parse(bytes);
  EXPECT_EQ(xml::resolve_qname(tree.text, tree), (xml::name{"Type", "urn:types"}));
  const auto copy = xml::deserialize_tokens<xml_xsd_fixture::qualified_names>(bytes, bounds);
  EXPECT_EQ(copy.names, value.names); EXPECT_EQ(copy.category, value.category);
  EXPECT_EQ(xml::serialize_tokens(copy, bounds), bytes);
  const auto default_namespace = xml::deserialize_tokens<xml_xsd_fixture::qualified_names>(
      "<x:qualified_names xmlns:x='urn:serializer:xsd' xmlns='urn:types' xmlns:p='urn:serializer:names' name='p:thing' names='p:a Type'>Type</x:qualified_names>", bounds);
  const auto default_tree = xml::parse(xml::serialize_tokens(default_namespace, bounds));
  EXPECT_EQ(xml::resolve_qname(default_tree.text, default_tree), (xml::name{"Type", "urn:types"}));
  for (const auto& replacement : std::array<std::pair<std::string_view, std::string_view>, 4U>{
      {{"name='p:thing'", "name='p:other'"}, {">q:Type<", ">missing:Type<"},
       {"names='p:a q:b'", "names='p:a missing:b'"}, {"required='p q'", "required='p missing'"}}}) {
    auto malformed = input;
    const auto position = malformed.find(replacement.first); ASSERT_NE(position, std::string::npos);
    malformed.replace(position, replacement.first.size(), replacement.second);
    EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::qualified_names>(malformed, bounds), std::invalid_argument);
  }
}

TEST(xml_tokens, child_qname_projection_rejects_namespace_context_that_native_storage_cannot_represent) {
  static_assert(xml_xsd_fixture::qname_children::xml_tokens_supported);
  xml::limits bounds{}; bounds.max_tree_bytes = 0U;
  const std::string prefix{"<qname_children xmlns='urn:serializer:xsd' xmlns:p='urn:parent'>"};
  const auto value = xml::deserialize_tokens<xml_xsd_fixture::qname_children>(
      prefix + "<value>p:first</value><value xmlns:p='urn:parent'>p:second</value></qname_children>", bounds, nullptr, 1U);
  ASSERT_EQ(value.values.size(), 2U);
  EXPECT_EQ(value.values[1], "p:second");
  EXPECT_EQ(xml::deserialize_tokens<xml_xsd_fixture::qname_children>(xml::serialize_tokens(value, bounds), bounds).values, value.values);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::qname_children>(
      prefix + "<value xmlns:p='urn:child'>p:item</value></qname_children>", bounds), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_xsd_fixture::qname_children>(
      prefix + "<value xmlns:r='urn:parent'>r:item</value></qname_children>", bounds), std::invalid_argument);
}
