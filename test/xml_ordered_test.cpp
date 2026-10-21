// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <xml_ordered.hpp>
#include <xml_ordered_native.hpp>

#include <rohit/serializer.hpp>
#include <rohit/stream.hpp>
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace xml = codec::xml;
constexpr std::string_view document{
    "<job id=\"7\"><move x=\"10\"/><pause milliseconds=\"50\"/><move x=\"20\"/></job>"};

// Preserve caller identity independently of implementation-specific exception-object copies.
struct interrupted : std::runtime_error {
  std::shared_ptr<const int> marker;
  // Keep the operation's identity token alive through propagation and unwind.
  explicit interrupted(std::shared_ptr<const int> value)
      : std::runtime_error{"ordered traversal interrupted"}, marker{std::move(value)} {}
};

// Author the same native values through either generated model's existing typed helpers.
template <typename Job>
Job make_job() {
  Job value{};
  value.id = 7U;
  using step = typename decltype(value.steps)::value_type;
  value.steps.emplace_back().template emplace_value<step::e_value::move>().x = 10U;
  value.steps.emplace_back().template emplace_value<step::e_value::pause>().milliseconds = 50U;
  value.steps.emplace_back().template emplace_value<step::e_value::move>().x = 20U;
  return value;
}

// Observe real native bytes independently of XML helpers or the visibility of variant storage.
template <template <codec::serialize_type> class Protocol, typename Value>
std::vector<std::uint8_t> native_bytes(const Value& value) {
  rohit::full_stream_auto_alloc output{};
  Protocol<codec::serialize_type::out> encoder{output};
  encoder.serialize_out(value);
  return {output.begin(), output.begin() + output.current_offset()};
}

// Compare against a separate XML-disabled generation with unchanged names, IDs and alternatives.
template <template <codec::serialize_type> class Protocol>
void expect_native_parity() {
  const auto value = make_job<xml_ordered::job>();
  const auto control = make_job<xml_ordered_native::job>();
  const auto expected = native_bytes<Protocol>(control);
  EXPECT_EQ(native_bytes<Protocol>(value), expected);
  const auto bytes = xml::serialize_tokens(value);
  EXPECT_EQ(native_bytes<Protocol>(value), expected);
  const auto restored = xml::deserialize_tokens<xml_ordered::job>(bytes);
  EXPECT_EQ(native_bytes<Protocol>(restored), expected);
}
} // namespace

// Native bytes and model sizes remain those of an XML-disabled generated control.
TEST(xml_ordered, native_layout_and_all_protocols_match_disabled_control) {
  static_assert(sizeof(xml_ordered::step) == sizeof(xml_ordered_native::step));
  static_assert(sizeof(xml_ordered::job) == sizeof(xml_ordered_native::job));
  expect_native_parity<codec::binary_none>();
  expect_native_parity<codec::binary_integer>();
  expect_native_parity<codec::binary_string>();
  expect_native_parity<codec::json>();
}

// One vector retains every child position, and standard vector edits define the new order.
TEST(xml_ordered, interleaving_and_authored_edits_have_one_source_of_order) {
  auto value = xml::deserialize_tokens<xml_ordered::job>(document);
  EXPECT_EQ(xml::serialize_tokens(value), document);
  ASSERT_EQ(value.steps.size(), 3U);
  EXPECT_EQ(value.steps[1].get_value_type(), xml_ordered::step::e_value::pause);
  const auto* original = value.steps.data();
  std::swap(value.steps[0], value.steps[1]);
  EXPECT_EQ(xml::serialize_tokens(value),
      "<job id=\"7\"><pause milliseconds=\"50\"/><move x=\"10\"/><move x=\"20\"/></job>");
  EXPECT_EQ(value.steps.data(), original);
  value.steps.erase(value.steps.begin());
  EXPECT_EQ(xml::serialize_tokens(value), "<job id=\"7\"><move x=\"10\"/><move x=\"20\"/></job>");
}

// Arbitrary input chunks and independent XSD validation observe the same typed child sequence.
TEST(xml_ordered, chunk_boundaries_and_independent_choice_validation) {
  const xml::prepared_schema schema{
      "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='job'><xs:complexType>"
      "<xs:choice minOccurs='0' maxOccurs='unbounded'>"
      "<xs:element name='move'><xs:complexType><xs:attribute name='x' type='xs:unsignedInt' use='required'/></xs:complexType></xs:element>"
      "<xs:element name='pause'><xs:complexType><xs:attribute name='milliseconds' type='xs:unsignedInt' use='required'/></xs:complexType></xs:element>"
      "</xs:choice><xs:attribute name='id' type='xs:unsignedInt' use='required'/>"
      "</xs:complexType></xs:element></xs:schema>"};
  for (const auto chunk : {1U, 2U, 7U, 4096U}) {
    const auto value = xml::deserialize_tokens<xml_ordered::job>(document, {}, nullptr, chunk);
    const auto result = xml::serialize_tokens(value);
    EXPECT_EQ(result, document);
    EXPECT_NO_THROW(schema.validate(result));
  }
}

// Owner-selected expanded names stay independent of input prefix aliases and a reused fragment type.
TEST(xml_ordered, expanded_names_and_owner_overrides_preserve_order) {
  constexpr std::string_view source{
      "<j:namespaced_job xmlns:j='urn:jobs' xmlns:m='urn:motion' xmlns:t='urn:timing'>"
      "<m:move x='10'/><t:wait milliseconds='50'/><m:move x='20'/></j:namespaced_job>"};
  const auto value = xml::deserialize_tokens<xml_ordered::namespaced_job>(source);
  const auto result = xml::parse(xml::serialize_tokens(value));
  ASSERT_EQ(result.children.size(), 3U);
  EXPECT_EQ(result.children[0].qualified_name, (xml::name{"move", "urn:motion"}));
  EXPECT_EQ(result.children[1].qualified_name, (xml::name{"wait", "urn:timing"}));
  EXPECT_EQ(result.children[2].qualified_name, (xml::name{"move", "urn:motion"}));
}

// Formatting whitespace is ignored only between ordered elements; text and unknown content reject.
TEST(xml_ordered, element_only_content_rejects_text_and_unknown_children) {
  EXPECT_EQ(xml::serialize_tokens(xml::deserialize_tokens<xml_ordered::job>(
      "<job id='7'>\n <move x='10'/>\t<pause milliseconds='50'/><move x='20'/>\r\n</job>")), document);
  for (const auto invalid : {
      "<job id='7'>unexpected<move x='10'/></job>",
      "<job id='7'><move x='10'/>unexpected</job>",
      "<job id='7'><unknown/></job>",
      "<job id='7'><move xmlns='urn:wrong' x='10'/></job>",
      "<job id='7'><move x='10' extra='1'/></job>",
      "<job id='7'><move x='10'><pause milliseconds='1'/></move></job>"}) {
    EXPECT_THROW((void)xml::deserialize_tokens<xml_ordered::job>(invalid), std::invalid_argument) << invalid;
  }
}

// Fixed storage requires exactly its declared number of typed children, without synthetic padding.
TEST(xml_ordered, fixed_arrays_enforce_exact_cardinality) {
  constexpr std::string_view source{
      "<fixed_job><move x=\"1\"/><pause milliseconds=\"2\"/><move x=\"3\"/></fixed_job>"};
  const auto value = xml::deserialize_tokens<xml_ordered::fixed_job>(source);
  EXPECT_EQ(xml::serialize_tokens(value), source);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_ordered::fixed_job>("<fixed_job/>"), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_ordered::fixed_job>(
      "<fixed_job><move x='1'/><pause milliseconds='2'/></fixed_job>"), std::invalid_argument);
  EXPECT_THROW((void)xml::deserialize_tokens<xml_ordered::fixed_job>(
      "<fixed_job><move x='1'/><move x='2'/><move x='3'/><move x='4'/></fixed_job>"), std::invalid_argument);
}

// Existing tree-dispatch entry points select the required token path through nested owning records.
TEST(xml_ordered, ordinary_entry_points_and_nested_owners_use_direct_tokens) {
  constexpr std::string_view source{
      "<envelope><value id=\"7\"><move x=\"10\"/><pause milliseconds=\"50\"/><move x=\"20\"/></value></envelope>"};
  xml::limits bounds{};
  bounds.max_tree_bytes = 0U;
  const auto value = xml::deserialize<xml_ordered::envelope>(source, bounds);
  EXPECT_EQ(xml::serialize(value, bounds), source);
}

// Admission failures return no partial document or replacement model, and fresh operations recover.
TEST(xml_ordered, exact_limits_and_recovery_preserve_caller_results) {
  auto value = make_job<xml_ordered::job>();
  xml::limits bounds{};
  bounds.max_tree_bytes = 0U;
  bounds.max_mixed_fragments = value.steps.size();
  bounds.max_mixed_text_bytes = 0U;
  bounds.max_output_bytes = document.size();
  EXPECT_EQ(xml::serialize_tokens(value, bounds), document);
  --bounds.max_output_bytes;
  std::string published{"previous"};
  EXPECT_THROW(published = xml::serialize_tokens(value, bounds), std::length_error);
  EXPECT_EQ(published, "previous");
  bounds.max_output_bytes = document.size();
  --bounds.max_mixed_fragments;
  EXPECT_THROW(value = xml::deserialize_tokens<xml_ordered::job>(document, bounds), std::length_error);
  EXPECT_EQ(xml::serialize_tokens(value), document);
  EXPECT_THROW((void)xml::serialize_tokens(value, bounds), std::length_error);
  bounds.max_mixed_fragments = 0U;
  EXPECT_TRUE(xml::deserialize_tokens<xml_ordered::job>("<job id='7'> \n </job>", bounds).steps.empty());
}

// Existing mixed content keeps significant text and its original text/child sequence behavior.
TEST(xml_ordered, existing_mixed_text_behavior_is_unchanged) {
  constexpr std::string_view source{
      "<paragraph>before <move x=\"10\"/> between <pause milliseconds=\"50\"/> after</paragraph>"};
  const auto value = xml::deserialize_tokens<xml_ordered::paragraph>(source);
  ASSERT_EQ(value.content.size(), 5U);
  EXPECT_EQ(xml::serialize_tokens(value), source);
}

// XML-specific member bridges can traverse a private payload without changing native visibility.
TEST(xml_ordered, private_variant_storage_loads_and_saves_through_its_owner) {
  constexpr std::string_view source{
      "<private_job><move x=\"10\"/><pause milliseconds=\"50\"/><move x=\"20\"/></private_job>"};
  const auto value = xml::deserialize_tokens<xml_ordered::private_job>(source);
  EXPECT_EQ(xml::serialize_tokens(value), source);
  EXPECT_EQ(value.steps.size(), 3U);
}

// Large output borrows the original vector and preserves caller identity through bounded failure.
TEST(xml_ordered, large_borrowed_sequence_cancels_without_publishing_and_recovers) {
  auto value = make_job<xml_ordered::job>();
  constexpr std::size_t item_count{25000U};
  value.steps.resize(item_count);
  for (std::size_t index{}; index < value.steps.size(); ++index) {
    value.steps[index].emplace_value<xml_ordered::step::e_value::move>().x = static_cast<std::uint32_t>(index);
  }
  const auto* original = value.steps.data();
  const auto marker = std::make_shared<const int>(17);
  xml::operation_context context{};
  std::size_t callbacks{};
  context.charge = [&](xml::operation_resource, std::size_t) {
    if (++callbacks == 100U) { throw interrupted{marker}; }
  };
  xml::limits bounds{};
  bounds.max_tree_bytes = 0U;
  std::string published{"previous"};
  try {
    published = xml::serialize_tokens(value, bounds, &context);
    FAIL() << "Traversal was not interrupted";
  } catch (const interrupted& failure) {
    EXPECT_EQ(failure.marker.get(), marker.get());
  }
  EXPECT_EQ(published, "previous");
  EXPECT_EQ(value.steps.data(), original);
  const auto output = xml::serialize_tokens(value, bounds);
  const auto restored = xml::deserialize_tokens<xml_ordered::job>(output, bounds);
  EXPECT_EQ(native_bytes<codec::binary_none>(restored), native_bytes<codec::binary_none>(value));
  EXPECT_EQ(value.steps.data(), original);
}
