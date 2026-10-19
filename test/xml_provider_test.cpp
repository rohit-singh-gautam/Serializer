// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml.hpp>
#include <xml_provider.hpp>

#include <gtest/gtest.h>
#include <libxml/xmlversion.h>

#include <array>
#include <future>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace xml = rohit::serializer::xml;
constexpr std::string_view schema_source = R"(<xs:schema
 xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:serializer:xsd"
 elementFormDefault="qualified">
 <xs:element name="order"><xs:complexType><xs:sequence>
 <xs:element name="customer" type="xs:string"/>
 <xs:element name="value" type="xs:int" minOccurs="1" maxOccurs="3"/>
 <xs:element name="note" type="xs:string" minOccurs="0"/>
 </xs:sequence><xs:attribute name="id" type="xs:unsignedInt" use="required"/>
 </xs:complexType></xs:element></xs:schema>)";
constexpr std::string_view document_source =
    "<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer>"
    "<value>3</value></order>";
} // namespace

// Actual generated objects pass a real prepared provider on both complete-message paths.
TEST(xml_provider, generated_codec_uses_prepared_libxml_schema) {
  RecordProperty("provider", LIBXML_DOTTED_VERSION);
  const xml::prepared_schema schema{schema_source};
  const auto value = xml::deserialize<xml_provider_fixture::order>(document_source, {}, &schema);
  EXPECT_EQ(value.id, 7U);
  EXPECT_EQ(value.customer, "R&D");
  const auto bytes = xml::serialize(value, {}, &schema);
  const auto decoded = xml::deserialize<xml_provider_fixture::order>(bytes, {}, &schema);
  EXPECT_EQ(decoded.values, value.values);
  EXPECT_THROW(schema.validate("<wrong xmlns='urn:serializer:xsd'/>"), std::invalid_argument);
  EXPECT_THROW(schema.validate(std::string{document_source} + "<extra/>"), std::invalid_argument);
  EXPECT_THROW(schema.validate("<order id='7'><customer>A</customer><value>1</value></order>"),
      std::invalid_argument);
}

// Every validation uses an independent native context while sharing immutable compiled XSD.
TEST(xml_provider, prepared_schema_is_reusable_concurrently) {
  const xml::prepared_schema schema{schema_source};
  constexpr std::size_t worker_count = 8U;
  constexpr std::size_t validations_per_worker = 16U;
  std::array<std::future<void>, worker_count> workers{};
  for (auto& worker : workers) {
    worker = std::async(std::launch::async, [&schema] {
      for (std::size_t index = 0; index < validations_per_worker; ++index) {
        schema.validate(document_source);
        try {
          schema.validate("<order xmlns='urn:serializer:xsd' id='7'/>");
          throw std::logic_error{"Provider accepted missing required content"};
        } catch (const std::invalid_argument&) {
          // An invalid document cannot contaminate the next independent validation context.
        }
      }
    });
  }
  for (auto& worker : workers) {
    EXPECT_NO_THROW(worker.get());
  }
}

// Local/remote schema references and instance hints never cause implicit schema acquisition.
TEST(xml_provider, imports_entities_and_instance_schema_hints_do_not_fetch) {
  for (const auto* directive : {"include", "import", "redefine"}) {
    for (const auto* location : {"file:///serializer-provider-must-not-open.xsd",
                                 "https://invalid.example/serializer-provider-must-not-open.xsd"}) {
      const std::string source =
          "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:" +
          std::string{directive} + " schemaLocation='" + location + "'/></xs:schema>";
      EXPECT_THROW((void)xml::prepared_schema{source}, std::invalid_argument);
    }
  }
  const xml::prepared_schema schema{schema_source};
  auto hinted = std::string{document_source};
  hinted.insert(hinted.find(" id="),
      " xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'"
      " xsi:schemaLocation='urn:serializer:xsd https://invalid.example/must-not-open.xsd'");
  EXPECT_NO_THROW(schema.validate(hinted));
  EXPECT_THROW(schema.validate("<!DOCTYPE order SYSTEM 'file:///must-not-open'>" +
      std::string{document_source}), std::invalid_argument);
}

// Provider preparation and validation enforce logical bounds and cooperative interruption.
TEST(xml_provider, limits_and_cancellation_apply_before_provider_publication) {
  EXPECT_THROW((void)xml::prepared_schema(schema_source, {.max_input_bytes = 1U}),
      std::length_error);
  const xml::prepared_schema schema{schema_source};
  EXPECT_THROW(schema.validate(document_source, {.max_input_bytes = 1U}), std::length_error);
  EXPECT_THROW(schema.validate(document_source, {.max_nodes = 1U}), std::length_error);
  EXPECT_THROW(schema.validate(document_source, {.max_attributes = 0U}), std::length_error);
  EXPECT_THROW(schema.validate(document_source, {.max_text_bytes = 1U}), std::length_error);
  xml::operation_context cancelled{};
  cancelled.is_cancelled = [] { return true; };
  EXPECT_THROW(schema.validate(document_source, {}, &cancelled), std::exception);
  EXPECT_NO_THROW(schema.validate(document_source));
}
