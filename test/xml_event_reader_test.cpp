// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include <rohit/xml_event_reader.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef SERIALIZER_TEST_HAS_XML
#define SERIALIZER_TEST_HAS_XML 0
#endif
#if SERIALIZER_TEST_HAS_XML
#include <libxml/parser.h>
#endif

namespace {
namespace xml = rohit::serializer::xml;

#if SERIALIZER_TEST_HAS_XML
std::size_t external_loads{};

// Count attempted provider resource loads without accessing either network or filesystem.
xmlParserInputPtr count_external_load(const char*, const char*, xmlParserCtxtPtr) noexcept {
  ++external_loads;
  return nullptr;
}

// Restore process-global provider configuration after the deliberately isolated loader probe.
struct loader_probe {
  xmlExternalEntityLoader original{xmlGetExternalEntityLoader()};
  // Install a counting loader only for this sequential test process.
  loader_probe() {
    xmlSetExternalEntityLoader(&count_external_load);
  }
  // Preserve the provider's original loader even if an assertion throws.
  ~loader_probe() {
    xmlSetExternalEntityLoader(original);
  }
};
#endif

// Copy an expanded name while a provider callback still owns the borrowed payload.
std::string expanded(xml::event_name value) {
  return "{" + std::string{value.namespace_uri} + "}" + std::string{value.local};
}

// Retain only test-owned semantic data and merge adjacent provider text chunks.
struct trace {
  std::vector<std::pair<xml::event_kind, std::string>> events{};

  // Record source-order attributes/declarations and normalize arbitrary adjacent text chunks.
  void accept(const xml::event_view& value) {
    std::string payload;
    if (value.kind == xml::event_kind::text) {
      if (!events.empty() && events.back().first == value.kind) {
        events.back().second += value.text;
        return;
      }
      payload = value.text;
    } else if (value.kind == xml::event_kind::start_element ||
               value.kind == xml::event_kind::end_element) {
      payload = expanded(value.qualified_name);
      for (const auto& attribute : value.attributes) {
        payload += " @" + expanded(attribute.qualified_name) + "=" + std::string{attribute.value};
      }
      for (const auto& declaration : value.namespaces) {
        payload +=
            " #" + std::string{declaration.prefix} + "=" + std::string{declaration.namespace_uri};
      }
    }
    events.emplace_back(value.kind, std::move(payload));
  }
};

// Parse at one arbitrary byte split and retain an independent normalized trace.
trace parse_split(std::string_view document, std::size_t split) {
  trace result;
  xml::event_reader reader{[&result](const xml::event_view& value) { result.accept(value); }};
  reader.feed(document.substr(0U, split));
  reader.feed(document.substr(split));
  EXPECT_FALSE(reader.complete());
  reader.finish();
  reader.finish();
  EXPECT_TRUE(reader.complete());
  EXPECT_FALSE(reader.failed());
  EXPECT_EQ(reader.input_bytes(), document.size());
  EXPECT_EQ(reader.usage().state_bytes, 0U);
  return result;
}

// Check that every invalid prefix remains unusable, including a later finalization attempt.
void expect_fenced(xml::event_reader& reader) {
  EXPECT_TRUE(reader.failed());
  EXPECT_FALSE(reader.complete());
  EXPECT_THROW(reader.feed("<new/>"), std::logic_error);
  EXPECT_THROW(reader.finish(), std::logic_error);
}

// Feed malformed XML at every byte split and verify that completion is never observable.
void reject_every_split(std::string_view document) {
  for (std::size_t split{}; split <= document.size(); ++split) {
    SCOPED_TRACE(split);
    bool end_seen{};
    xml::event_reader reader{[&end_seen](const xml::event_view& value) {
      end_seen = end_seen || value.kind == xml::event_kind::end_document;
    }};
    EXPECT_THROW(
        {
          reader.feed(document.substr(0U, split));
          reader.feed(document.substr(split));
          reader.finish();
        },
        std::invalid_argument);
    EXPECT_FALSE(end_seen);
    expect_fenced(reader);
  }
}

// Ignore borrowed events without retaining provider memory.
void discard(const xml::event_view&) {}

} // namespace

// The header remains provider-independent and unavailability is explicit at construction.
TEST(xml_event_reader, optional_provider_boundary) {
  EXPECT_THROW(xml::event_reader reader{xml::event_callback{}}, std::invalid_argument);
  if (!xml::available()) {
    EXPECT_THROW(xml::event_reader reader{discard}, std::runtime_error);
  } else {
    EXPECT_NO_THROW(xml::event_reader reader{discard});
  }
}

// Every byte boundary includes UTF-8 names/URIs, references, declarations and normalized values.
TEST(xml_event_reader, every_split_and_single_byte_feed_have_the_same_semantic_trace) {
  if (!xml::available()) {
    GTEST_SKIP();
  }
  const std::string document =
      "\xef\xbb\xbf<?xml version='1.0' encoding='UTF-8'?>\r\n"
      "<!--before--><?before x?><\xcf\x80 xmlns='urn:\xc3\xa9' xmlns:p='urn:p' "
      "a='&lt;&gt;&amp;&quot;&apos;&#x1F600;\r\n\t' p:a='x'>"
      "one\r\ntwo&#13;<![CDATA[<three>]]><p:child/>tail\xf0\x9f\x98\x80"
      "</\xcf\x80><!--after--><?after x?>\n";
  const auto expected = parse_split(document, document.size()).events;
  ASSERT_EQ(expected.size(), 8U);
  EXPECT_EQ(expected[1U].second, "{urn:\xc3\xa9}\xcf\x80 @{}a=<>&\"'\xf0\x9f\x98\x80   @"
                                 "{urn:p}a=x #=urn:\xc3\xa9 #p=urn:p");
  EXPECT_EQ(expected[2U].second, "one\ntwo\r<three>");
  EXPECT_EQ(expected[3U].second, "{urn:p}child");
  EXPECT_EQ(expected[4U].second, "{urn:p}child");
  EXPECT_EQ(expected[5U].second, "tail\xf0\x9f\x98\x80");
  for (std::size_t split{}; split <= document.size(); ++split) {
    SCOPED_TRACE(split);
    EXPECT_EQ(parse_split(document, split).events, expected);
  }
  trace actual;
  xml::event_reader reader{[&actual](const xml::event_view& value) { actual.accept(value); }};
  for (const char& byte : document) {
    reader.feed(std::string_view{&byte, 1U});
  }
  reader.finish();
  EXPECT_EQ(actual.events, expected);
}

// Expanded identities obey default reset, unprefixed-attribute rules and nested prefix restoration.
TEST(xml_event_reader, namespace_shadow_reset_and_pop_preserve_expanded_identity) {
  if (!xml::available()) {
    GTEST_SKIP();
  }
  constexpr std::string_view document =
      "<r xmlns='urn:r' xmlns:p='urn:outer'>"
      "<p:c xmlns:p='urn:inner' a='1' p:a='2'><z xmlns=''/></p:c><p:c/></r>";
  const auto expected = parse_split(document, 0U).events;
  ASSERT_EQ(expected.size(), 10U);
  EXPECT_EQ(expected[1U].second, "{urn:r}r #=urn:r #p=urn:outer");
  EXPECT_EQ(expected[2U].second, "{urn:inner}c @{}a=1 @{urn:inner}a=2 #p=urn:inner");
  EXPECT_EQ(expected[3U].second, "{}z #=");
  EXPECT_EQ(expected[5U].second, "{urn:inner}c");
  EXPECT_EQ(expected[6U].second, "{urn:outer}c");
  for (std::size_t split{}; split <= document.size(); ++split) {
    EXPECT_EQ(parse_split(document, split).events, expected);
  }
}

// Invalid namespaces, document shapes, character references and token EOF all fence the pass.
TEST(xml_event_reader, malformed_documents_reject_at_every_split) {
  if (!xml::available()) {
    GTEST_SKIP();
  }
  constexpr std::string_view documents[]{"",
                                         " ",
                                         "<r>",
                                         "<r",
                                         "<r></s>",
                                         "<r/><s/>",
                                         "<r/>tail",
                                         "<r/>\xc3",
                                         "<p:r/>",
                                         "<r p:a='x'/>",
                                         "<r a='1' a='2'/>",
                                         "<r xmlns:p='u' xmlns:q='u' p:a='1' q:a='2'/>",
                                         "<r xmlns:p='u' xmlns:p='v'/>",
                                         "<r xmlns:xml='wrong'/>",
                                         "<r xmlns:xmlns='urn:x'/>",
                                         "<r xmlns:p=''/>",
                                         "<r xmlns='urn:space here'/>",
                                         "<r xmlns:p='urn:%GG'/>",
                                         "<r>&undefined;</r>",
                                         "<r>&#0;</r>",
                                         "<r>&#xD800;</r>",
                                         "<r>&#x110000;</r>",
                                         "<r a='unterminated",
                                         "<r><!--unfinished</r>",
                                         "<r><![CDATA[unfinished</r>",
                                         "<?xml version='1.1'?><r/>",
                                         "<?xml version='1.0' encoding='ISO-8859-1'?><r/>",
                                         "<?xml version='1.0' encoding='UTF-16'?><r/>",
                                         "<r>\xc0\xaf</r>",
                                         "<r>\xed\xa0\x80</r>",
                                         "<r>\xf4\x90\x80\x80</r>",
                                         "<r>\xef\xbf\xbe</r>"};
  for (const auto document : documents) {
    SCOPED_TRACE(document);
    reject_every_split(document);
  }
  const std::string utf16{"\xff\xfe<\0r\0/\0>\0", 10U};
  reject_every_split(utf16);
  const std::string nul{"<r>\0</r>", 8U};
  reject_every_split(nul);
}

// DTD rejection occurs for internal, external and parameter-entity forms, across every split.
TEST(xml_event_reader, every_doctype_rejects_without_resolving_resources) {
  if (!xml::available()) {
    GTEST_SKIP();
  }
#if SERIALIZER_TEST_HAS_XML
  loader_probe probe;
  external_loads = 0U;
#endif
  constexpr std::string_view documents[]{
      "<!DOCTYPE r><r/>",
      "<!DOCTYPE r [<!ENTITY x 'expanded'>]><r>&x;</r>",
      "<!DOCTYPE r SYSTEM 'http://127.0.0.1:1/never-fetch'><r/>",
      "<!DOCTYPE r SYSTEM 'file:///serializer-never-open.dtd'><r/>",
      "<!DOCTYPE r [<!ENTITY % x SYSTEM 'http://127.0.0.1:1/never-fetch'>%x;]><r/>",
      "<!DOCTYPE r [<!ENTITY x SYSTEM 'file:///serializer-never-open'>]><r>&x;</r>"};
  for (const auto document : documents) {
    reject_every_split(document);
  }
#if SERIALIZER_TEST_HAS_XML
  EXPECT_EQ(external_loads, 0U);
#endif
}

// Closing the root does not publish success; rejected trailing bytes revoke the whole parse.
TEST(xml_event_reader, end_document_is_delivered_only_after_successful_finish) {
  if (!xml::available()) {
    GTEST_SKIP();
  }
  bool end_seen{};
  xml::event_reader reader{[&end_seen](const xml::event_view& value) {
    end_seen = value.kind == xml::event_kind::end_document || end_seen;
  }};
  reader.feed("<r/>");
  EXPECT_FALSE(reader.complete());
  EXPECT_FALSE(end_seen);
  EXPECT_THROW(
      {
        reader.feed("x");
        reader.finish();
      },
      std::invalid_argument);
  EXPECT_FALSE(end_seen);
  expect_fenced(reader);
  xml::event_reader complete{discard};
  complete.feed("<r/>");
  complete.finish();
  EXPECT_THROW(complete.feed({}), std::logic_error);
  expect_fenced(complete);
}

// Callback exceptions never unwind through LibXml2, and retain their original application type.
TEST(xml_event_reader, observer_exceptions_and_reentry_permanently_fence_the_reader) {
  if (!xml::available()) {
    GTEST_SKIP();
  }
  struct observer_failure {};
  for (const auto selected :
       {xml::event_kind::start_document, xml::event_kind::start_element, xml::event_kind::text,
        xml::event_kind::end_element, xml::event_kind::end_document}) {
    xml::event_reader reader{[selected](const xml::event_view& value) {
      if (value.kind == selected) {
        throw observer_failure{};
      }
    }};
    EXPECT_THROW(
        {
          reader.feed("<r>text</r>");
          reader.finish();
        },
        observer_failure);
    expect_fenced(reader);
  }
  xml::event_reader* owner{};
  xml::event_reader reentrant{
      [&owner](const xml::event_view&) { EXPECT_THROW(owner->feed({}), std::logic_error); }};
  owner = &reentrant;
  EXPECT_THROW(
      {
        reentrant.feed("<r/>");
        reentrant.finish();
      },
      std::logic_error);
  expect_fenced(reentrant);
}

// Byte limits admit equality, count source rather than normalized bytes and never reset per feed.
TEST(xml_event_reader, cumulative_input_and_event_limits_are_enforced) {
  if (!xml::available()) {
    GTEST_SKIP();
  }
  xml::event_limits bounds;
  bounds.document.max_input_bytes = 4U;
  xml::event_reader exact{discard, bounds};
  exact.feed("<r");
  exact.feed("/>");
  exact.finish();
  EXPECT_EQ(exact.input_bytes(), 4U);
  xml::event_reader excess{discard, bounds};
  excess.feed("<r/>");
  EXPECT_THROW(excess.feed(" "), std::length_error);
  expect_fenced(excess);
  const auto check = [](xml::event_limits selected, std::string_view document) {
    xml::event_reader reader{discard, selected};
    EXPECT_THROW(
        {
          reader.feed(document);
          reader.finish();
        },
        std::length_error);
    expect_fenced(reader);
  };
  bounds = {};
  bounds.document.max_depth = 1U;
  check(bounds, "<r><c/></r>");
  bounds = {};
  bounds.document.max_text_bytes = 2U;
  check(bounds, "<r>abc</r>");
  bounds = {};
  bounds.document.max_attributes = 0U;
  check(bounds, "<r a='1'/>");
  bounds = {};
  bounds.max_event_bytes = 2U;
  check(bounds, "<root/>");
  bounds = {};
  bounds.max_events = 3U;
  check(bounds, "<r/>");
  bounds = {};
  bounds.max_state_bytes = 0U;
  check(bounds, "<r/>");
}

// One borrowed context carries interruption and callback failures through input and event work.
TEST(xml_event_reader, cumulative_context_cancellation_deadline_and_charge_failures) {
  if (!xml::available()) {
    GTEST_SKIP();
  }
  bool cancelled{};
  xml::operation_context context;
  context.is_cancelled = [&cancelled] { return cancelled; };
  xml::event_reader reader{discard, {}, &context};
  reader.feed("<r>");
  cancelled = true;
  EXPECT_THROW(reader.feed("text</r>"), std::exception);
  expect_fenced(reader);
  context = {};
  context.deadline = std::chrono::steady_clock::now() - std::chrono::seconds{1};
  xml::event_reader expired{discard, {}, &context};
  EXPECT_THROW(expired.finish(), std::exception);
  expect_fenced(expired);
  struct charge_failure {};
  context = {};
  context.charge = [](xml::operation_resource resource, std::size_t) {
    if (resource == xml::operation_resource::allocation_bytes) {
      throw charge_failure{};
    }
  };
  xml::event_reader charged{discard, {}, &context};
  EXPECT_THROW(charged.feed("<r/>"), charge_failure);
  expect_fenced(charged);
}
