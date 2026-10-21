// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/core_properties.hpp>
#include <rohit/xml.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace package = rohit::serializer::package;

// Optional-provider builds retain the API and report explicit unavailability before decoding.
TEST(core_properties_provider, optional_xml_dependency_remains_explicit) {
  package::budget shared{};
  if (rohit::serializer::xml::available()) {
    EXPECT_NO_THROW(static_cast<void>(package::encode_core_properties({}, shared)));
    return;
  }
  try {
    static_cast<void>(package::encode_core_properties({}, shared));
    FAIL() << "Accepted core-property XML without its provider";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, package::error_code::unavailable);
  }
  try {
    static_cast<void>(package::decode_core_properties("", shared));
    FAIL() << "Decoded core-property XML without its provider";
  } catch (const package::error& failure) {
    EXPECT_EQ(failure.code, package::error_code::unavailable);
  }
}

// Author independent property XML with arbitrary prefixes, independent of the adapter's writer.
std::string document(std::string_view body) {
  return std::string{"<p:coreProperties xmlns:p='http://schemas.openxmlformats.org/package/2006/metadata/core-properties' "
      "xmlns:d='http://purl.org/dc/elements/1.1/' xmlns:t='http://purl.org/dc/terms/' "
      "xmlns:i='http://www.w3.org/2001/XMLSchema-instance'>"} + std::string{body} + "</p:coreProperties>";
}

// Complete a generic OPC archive, letting the adapter's reader discover a noncanonical part name.
std::string archive(std::string_view payload, std::string_view content_type,
    package::target_mode mode = package::target_mode::internal, std::size_t link_count = 1U) {
  std::ostringstream output{std::ios::binary};
  package::opc_writer writer{output};
  writer.add_part("/metadata/independent.xml", content_type,
      {reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()});
  for (std::size_t index{}; index < link_count; ++index) {
    writer.add_relationship({"/", "properties" + std::to_string(index),
        std::string{package::core_properties_relationship_type},
        mode == package::target_mode::internal ? "metadata/independent.xml" : "https://example.org/core.xml", mode});
  }
  writer.finish();
  return output.str();
}

// Do not invoke provider-dependent cases in deliberately XML-disabled build configurations.
class core_properties : public testing::Test {
protected:
  // Record an explicit unavailable-provider skip rather than silently running another codec.
  void SetUp() override {
    if (!rohit::serializer::xml::available()) { GTEST_SKIP() << "XML provider is disabled"; }
  }
};

// All standard fields retain optional presence and application-supplied text without implicit clocks.
TEST_F(core_properties, complete_model_round_trip_and_deterministic_xml) {
  package::core_properties value{};
  value.category = "engineering";
  value.content_status = "reviewed";
  value.created = "2026-10-21T13:14:15.125+05:30";
  value.creator = "Rohit & collaborators";
  value.description = "Unicode \xe6\xa8\xa1\xe5\x9e\x8b <description>";
  value.identifier = "urn:example:42";
  value.keywords = package::core_property_keywords{{}, {std::string{"one; two"}}};
  value.language = "en-IN";
  value.last_modified_by = "reviewer";
  value.last_printed = "2026-10-20T12:00:00Z";
  value.modified = "2026-10";
  value.revision = "0042";
  value.subject = "";
  value.title = "model title";
  value.version = "1.2-beta";
  package::budget shared{};
  const auto encoded = package::encode_core_properties(value, shared);
  EXPECT_EQ(package::decode_core_properties(encoded, shared), value);
  EXPECT_EQ(package::encode_core_properties(value, shared), encoded);
  const auto root = rohit::serializer::xml::parse(encoded);
  ASSERT_EQ(root.children.size(), 15U);
  EXPECT_NE(encoded.find("&amp;"), std::string::npos);
  EXPECT_NE(encoded.find("&lt;description&gt;"), std::string::npos);
  package::budget empty_budget{};
  EXPECT_EQ(package::decode_core_properties(package::encode_core_properties({}, empty_budget), empty_budget),
      package::core_properties{});
}

// Independent input exercises local QName declarations and arbitrary ordering rather than our writer.
TEST_F(core_properties, independent_prefixes_empty_presence_and_utf16) {
  const auto input = document("<d:title>title &amp; details</d:title><d:subject/>"
      "<t:created xmlns:date='http://purl.org/dc/terms/' i:type='date:W3CDTF'>2000-02-29</t:created>");
  package::budget shared{};
  const auto value = package::decode_core_properties(input, shared);
  EXPECT_EQ(value.title, "title & details");
  EXPECT_EQ(value.subject, "");
  EXPECT_FALSE(value.creator);
  EXPECT_EQ(value.created, "2000-02-29");
  std::string utf16{"\xff\xfe", 2U};
  for (const auto byte : input) { utf16.push_back(byte); utf16.push_back('\0'); }
  EXPECT_EQ(package::decode_core_properties(utf16, shared), value);
}

// Fifth-edition keywords permit ordered mixed text and separately localized value elements.
TEST_F(core_properties, fifth_edition_localized_mixed_keywords) {
  package::budget shared{};
  const auto value = package::decode_core_properties(document(
      "<p:keywords xml:lang='en-US'>color <p:value xml:lang='en-CA'>colour</p:value> / "
      "<p:value xml:lang='fr-FR'>couleur</p:value> tail</p:keywords>"), shared);
  ASSERT_TRUE(value.keywords);
  EXPECT_EQ(value.keywords->language, "en-US");
  ASSERT_EQ(value.keywords->content.size(), 5U);
  EXPECT_EQ(std::get<std::string>(value.keywords->content.front()), "color ");
  EXPECT_EQ(std::get<package::core_property_keyword>(value.keywords->content[1]),
      (package::core_property_keyword{"colour", "en-CA"}));
  EXPECT_EQ(std::get<package::core_property_keyword>(value.keywords->content[3]),
      (package::core_property_keyword{"couleur", "fr-FR"}));
  EXPECT_EQ(package::decode_core_properties(package::encode_core_properties(value, shared), shared), value);
  for (const auto text : {"<p:contentType>removed legacy property</p:contentType>",
      "<p:keywords><p:other/></p:keywords>", "<p:keywords><p:value><p:value/></p:value></p:keywords>",
      "<p:keywords xmlns:mc='http://schemas.openxmlformats.org/markup-compatibility/2006' mc:Ignorable='p'/>",
      "<p:keywords><p:value i:type='t:W3CDTF'>2026</p:value></p:keywords>"}) {
    EXPECT_THROW(static_cast<void>(package::decode_core_properties(document(text), shared)), package::error);
  }
}

// The normative imported schema selects a date union, with exact precision and offset retention.
TEST_F(core_properties, date_precision_calendar_and_xml_schema_boundaries) {
  constexpr std::array accepted{"1997", "1997Z", "1997-07", "1997-07+01:00", "2000-02-29",
      "1997-07-16T19:20:30Z", "1997-07-16T19:20:30.123456789-05:30", " 2000-02-29 ",
      "1997-07-16T19:20:30", "1997-07-16T24:00:00Z"};
  for (const auto text : accepted) {
    package::budget shared{};
    const auto value = package::decode_core_properties(
        document(std::string{"<t:created i:type='t:W3CDTF'>"} + text + "</t:created>"), shared);
    EXPECT_EQ(value.created, text);
  }
  constexpr std::array rejected{"", "0000", "1997-00", "1997-13", "1900-02-29", "2026-04-31",
      "2026-10-21T12:00", "2026-10-21T24:00Z", "2026-10-21T12:60Z", "1997-07-16T19:20+01:00",
      "2026-10-21T12:00:60Z", "2026-10-21T12:00:00.Z", "2026-10-21T12:00+24:00",
      "2026-10-21T12:00+01:60", "2026-10-21T12:00Ztrailing", "2026-1", "2026-10-2"};
  for (const auto text : rejected) {
    package::budget shared{};
    package::core_properties value{};
    value.created = text;
    EXPECT_THROW(static_cast<void>(package::encode_core_properties(value, shared)), package::error) << text;
  }
  constexpr std::array schema_dates{"2026-10-21T24:00:00Z", "2026-10-21T12:00:00",
      "-0001-01-01T00:00:00-14:00", "12000-02-29T00:00:00.000+14:00"};
  for (const auto text : schema_dates) {
    package::budget shared{};
    const auto decoded = package::decode_core_properties(
        document(std::string{"<p:lastPrinted>"} + text + "</p:lastPrinted>"), shared);
    EXPECT_EQ(decoded.last_printed, text);
  }
  constexpr std::array schema_rejected{"2026", "2026-10-21T12:00Z", "2026-10-21T24:00:00.1Z",
      "2026-10-21T00:00:00+14:01", "02026-10-21T00:00:00Z", "-0000-01-01T00:00:00"};
  for (const auto text : schema_rejected) {
    package::budget shared{};
    EXPECT_THROW(static_cast<void>(package::decode_core_properties(
        document(std::string{"<p:lastPrinted>"} + text + "</p:lastPrinted>"), shared)), package::error) << text;
  }
}

// The typed adapter has a closed vocabulary; opaque whole-part preservation remains separately usable.
TEST_F(core_properties, duplicate_nested_foreign_attributes_and_wrong_types_reject) {
  constexpr std::array rejected{
      "<d:title>first</d:title><d:title>second</d:title>", "<d:title><d:subject/></d:title>",
      "<p:title>wrong namespace</p:title>", "<d:unknown/>", "<d:title xml:lang='en'>title</d:title>",
      "<d:title i:type='t:W3CDTF'>title</d:title>", "<t:created>2026</t:created>",
      "<t:created i:type='d:W3CDTF'>2026</t:created>", "<t:created i:type='t:date'>2026</t:created>",
      "<t:created i:type='t:W3CDTF' extra='value'>2026</t:created>"};
  for (const auto text : rejected) {
    package::budget shared{};
    EXPECT_THROW(static_cast<void>(package::decode_core_properties(document(text), shared)), package::error) << text;
  }
}

// Register standard metadata in a normal OPC archive and consume it through a caller-owned profile.
TEST_F(core_properties, package_composition_and_relationship_discovery) {
  package::core_properties value{};
  value.title = "application supplied";
  std::ostringstream output{std::ios::binary};
  package::opc_writer writer{output};
  package::write_core_properties(writer, value, "/metadata/properties.xml", "metadata-link");
  writer.finish();
  std::istringstream input{output.str(), std::ios::binary};
  package::opc_reader reader{input};
  package::profile_loader loader{reader};
  EXPECT_EQ(package::read_core_properties(loader), value);
  const auto unknown = loader.finish();
  EXPECT_TRUE(unknown.parts.empty());
  const auto independent = archive(document("<d:title>independent producer</d:title>"),
      package::core_properties_content_type);
  std::istringstream independent_input{independent, std::ios::binary};
  package::opc_reader independent_reader{independent_input};
  package::profile_loader independent_loader{independent_reader};
  ASSERT_TRUE(package::read_core_properties(independent_loader));
  EXPECT_TRUE(independent_loader.finish().parts.empty());
}

// Optional absence is valid; malformed singleton linkage never returns a partially decoded value.
TEST_F(core_properties, discovery_rejects_wrong_mode_type_or_cardinality) {
  const auto xml = document("<d:title>candidate</d:title>");
  for (const auto& bytes : std::array{
      archive(xml, "application/xml"),
      archive(xml, package::core_properties_content_type, package::target_mode::external),
      archive(xml, package::core_properties_content_type, package::target_mode::internal, 2U)}) {
    std::istringstream input{bytes, std::ios::binary};
    package::opc_reader reader{input};
    package::profile_loader loader{reader};
    EXPECT_THROW(static_cast<void>(package::read_core_properties(loader)), package::error);
  }
  std::ostringstream empty{std::ios::binary};
  package::opc_writer writer{empty};
  writer.finish();
  std::istringstream input{empty.str(), std::ios::binary};
  package::opc_reader reader{input};
  package::profile_loader loader{reader};
  EXPECT_FALSE(package::read_core_properties(loader));
  EXPECT_TRUE(loader.finish().parts.empty());
}

// Repeated XML operations use one budget; interruption before construction leaves the sink untouched.
TEST_F(core_properties, shared_limits_cancellation_deadline_and_prevalidation) {
  package::core_properties value{};
  value.title = std::string(2048U, 'a');
  package::budget shared{};
  const auto before = shared.remaining(package::resource::decoder_allocation_bytes);
  static_cast<void>(package::encode_core_properties(value, shared));
  const auto after = shared.remaining(package::resource::decoder_allocation_bytes);
  EXPECT_LT(after, before);
  static_cast<void>(package::encode_core_properties(value, shared));
  EXPECT_LT(shared.remaining(package::resource::decoder_allocation_bytes), after);
  package::limits narrow{};
  narrow.max_decoder_allocation_bytes = value.title->size() - 1U;
  package::budget small{narrow};
  EXPECT_THROW(static_cast<void>(package::encode_core_properties(value, small)), package::error);
  narrow = {};
  narrow.is_cancelled = [] { return true; };
  package::budget cancelled{narrow};
  EXPECT_THROW(static_cast<void>(package::encode_core_properties(value, cancelled)), package::error);
  narrow = {};
  narrow.deadline = std::chrono::steady_clock::now() - std::chrono::seconds{1};
  package::budget expired{narrow};
  EXPECT_THROW(static_cast<void>(package::decode_core_properties(document(""), expired)), package::error);
  std::ostringstream output{std::ios::binary};
  package::opc_writer writer{output};
  value.created = "invalid date";
  EXPECT_THROW(package::write_core_properties(writer, value), package::error);
  EXPECT_TRUE(output.str().empty());
}

// Keyword inventories are bounded by the shared context rather than the fixed scalar-property count.
TEST_F(core_properties, large_keyword_inventory_uses_shared_resource_and_interruption_limits) {
  package::core_properties value{};
  value.keywords.emplace();
  constexpr std::size_t keyword_count = 1000U;
  for (std::size_t index{}; index < keyword_count; ++index) {
    value.keywords->content.emplace_back(package::core_property_keyword{std::to_string(index), "en"});
  }
  package::budget shared{};
  const auto encoded = package::encode_core_properties(value, shared);
  package::limits exact{};
  exact.max_part_bytes = encoded.size();
  package::budget exact_budget{exact};
  const auto decoded = package::decode_core_properties(encoded, exact_budget);
  ASSERT_TRUE(decoded.keywords);
  EXPECT_EQ(decoded.keywords->content.size(), keyword_count);
  EXPECT_EQ(decoded, value);
  package::limits narrow{};
  narrow.max_decoder_allocation_bytes = encoded.size();
  package::budget small{narrow};
  EXPECT_THROW(static_cast<void>(package::decode_core_properties(encoded, small)), package::error);
  narrow = {};
  std::size_t checks{};
  narrow.is_cancelled = [&checks] { return ++checks > 100U; };
  package::budget cancelled{narrow};
  EXPECT_THROW(static_cast<void>(package::encode_core_properties(value, cancelled)), package::error);
  EXPECT_GT(checks, 100U);
}

// Encoded-byte bounds do not double as tree-size/node bounds for a small valid document.
TEST_F(core_properties, exact_encoded_size_does_not_restrict_native_tree_storage) {
  package::budget shared{};
  const auto encoded = package::encode_core_properties({}, shared);
  package::limits exact{};
  exact.max_part_bytes = encoded.size();
  package::budget exact_budget{exact};
  EXPECT_EQ(package::decode_core_properties(encoded, exact_budget), package::core_properties{});
  EXPECT_EQ(package::encode_core_properties({}, exact_budget), encoded);
  --exact.max_part_bytes;
  package::budget too_small{exact};
  EXPECT_THROW(static_cast<void>(package::decode_core_properties(encoded, too_small)), package::error);
}
} // namespace
