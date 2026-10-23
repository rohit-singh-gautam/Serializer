// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.hpp"

#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace xml = rohit::serializer::xml;
namespace codec = rohit::serializer;

// Capture an unchanged native protocol as a control independent of the XML-only scope state.
template <typename Protocol, typename T> std::vector<std::uint8_t> native_bytes(const T& value) {
  rohit::full_stream_auto_alloc storage{};
  Protocol protocol{storage};
  protocol.serialize_out(value);
  return {storage.begin(), storage.begin() + storage.current_offset()};
}

// Confirm emitted list items retain their original expanded identities through alias regeneration.
void check_output(std::string_view bytes) {
  const auto root = xml::parse(bytes);
  constexpr std::array expected{std::string_view{"urn:allowed"}, std::string_view{}, std::string_view{"urn:allowed"}};
  const auto& child = root.children.front();
  auto text = std::string_view{child.text};
  for (const auto uri : expected) {
    text = xml::detail::trim(text);
    const auto end = text.find_first_of(" \t\r\n");
    const auto token = text.substr(0U, end);
    if (xml::resolve_qname(token, child).namespace_uri != uri) {
      throw std::runtime_error{"QName list namespace changed"};
    }
    text.remove_prefix(token.size());
  }
  if (!xml::detail::trim(text).empty()) { throw std::runtime_error{"QName list length changed"}; }
}

// Exercise actual generated typed lists and nested union branches with equivalent and shadowed aliases.
int main() {
  static_assert(!qname_lists::empty_default::xml_tokens_required);
  constexpr std::string_view empty_source = R"(<t:empty_default xmlns:t="urn:qname-lists" xmlns="urn:active" category="Name"/>)";
  const auto empty = xml::deserialize_tokens<qname_lists::empty_default>(empty_source);
  if (!empty.names.empty() || empty.category != "Name") { throw std::runtime_error{"Legacy empty default changed native spelling"}; }
  (void)xml::deserialize<qname_lists::empty_default>(xml::serialize(empty));
  constexpr std::string_view document = R"(<t:sample xmlns:t="urn:qname-lists" xmlns:p="urn:allowed" xmlns:q="urn:shadow" fixed_names="p:first local p:second"><t:names xmlns:c="urn:allowed">c:first local c:second</t:names><t:choice>p:first local p:second</t:choice></t:sample>)";
  const auto value = xml::deserialize_tokens<qname_lists::sample>(document);
  check_output(xml::serialize_tokens(value));
  const auto defaults = xml::deserialize_tokens<qname_lists::fixed_list>(
      R"(<t:fixed_list xmlns:t="urn:qname-lists" xmlns="urn:active" xmlns:q="urn:shadow"><t:names/></t:fixed_list>)");
  if (defaults.names.size() != 3U) { throw std::runtime_error{"QName list fixed-array default length changed"}; }
  check_output(xml::serialize_tokens(defaults));
  const auto scoped = xml::deserialize_tokens<qname_lists::root_scope>(
      R"(<t:root_scope xmlns:t="urn:qname-lists" xmlns="urn:active" xmlns:q="urn:shadow" category="Name" fallback="q0:literal"/>)");
  const auto scoped_output = xml::parse(xml::serialize_tokens(scoped));
  if (scoped_output.namespace_bindings.contains("q0") || scoped.fallback != "q0:literal") {
    throw std::runtime_error{"Private root QName default changed a union string"};
  }
  const std::array expected_root_names{
      xml::name{"first", "urn:allowed"}, xml::name{"local", ""}, xml::name{"second", "urn:allowed"}};
  for (const auto& attribute : scoped_output.attributes) {
    if (attribute.qualified_name.local == "category" &&
        xml::resolve_qname(attribute.value, scoped_output).namespace_uri != "urn:active") {
      throw std::runtime_error{"Private root QName default changed another attribute's identity"};
    }
    if (attribute.qualified_name.local == "names" &&
        !xml::lexical_qname_list_equal(attribute.value, expected_root_names, scoped_output)) {
      throw std::runtime_error{"Private root QName list default changed schema identities"};
    }
  }
  const auto check_native = [&]<typename Protocol>() {
    const auto before = native_bytes<Protocol>(scoped);
    const auto emitted = xml::serialize_tokens(scoped);
    if (native_bytes<Protocol>(scoped) != before ||
        native_bytes<Protocol>(xml::deserialize_tokens<qname_lists::root_scope>(emitted)) != before) {
      throw std::runtime_error{"XML default projection changed a native protocol"};
    }
  };
  check_native.operator()<codec::binary_none<codec::serialize_type::out>>();
  check_native.operator()<codec::binary_integer<codec::serialize_type::out>>();
  check_native.operator()<codec::binary_string<codec::serialize_type::out>>();
  check_native.operator()<codec::json<codec::serialize_type::out>>();
  const auto scalar_defaults = xml::deserialize_tokens<qname_lists::root_scalar_default>(
      R"(<t:root_scalar_default xmlns:t="urn:qname-lists" xmlns="urn:active" xmlns:q="urn:shadow" fallback="q0:literal"/>)");
  const auto scalar_output = xml::parse(xml::serialize_tokens(scalar_defaults));
  if (scalar_output.namespace_bindings.contains("q0") || scalar_defaults.fallback != "q0:literal") {
    throw std::runtime_error{"Schema scalar defaults changed an unrelated union string"};
  }
  for (const auto& attribute : scalar_output.attributes) {
    if (attribute.qualified_name.local == "category" &&
        xml::resolve_qname(attribute.value, scalar_output) != xml::name{"local", ""}) {
      throw std::runtime_error{"No-namespace scalar default changed identity"};
    }
    if (attribute.qualified_name.local == "qualified" &&
        xml::resolve_qname(attribute.value, scalar_output) != xml::name{"value", "urn:allowed"}) {
      throw std::runtime_error{"Qualified scalar default changed identity"};
    }
  }
  for (const auto chunk_bytes : {1U, 65536U}) {
    const auto attributes = xml::deserialize_tokens<qname_lists::attribute_default>(
        R"(<t:attribute_default xmlns:t="urn:qname-lists" xmlns:q="urn:shadow" fallback="q0:literal"/>)",
        {}, nullptr, chunk_bytes);
    const auto emitted = xml::parse(xml::serialize_tokens(attributes));
    if (emitted.namespace_bindings.contains("q0") || attributes.names.size() != 2U ||
        attributes.fallback != "q0:literal") {
      throw std::runtime_error{"QName attribute default changed another union value"};
    }
    const auto text = xml::deserialize_tokens<qname_lists::text_default>(
        R"(<t:text_default xmlns:t="urn:qname-lists" xmlns:q="urn:shadow" fallback="q0:literal"/>)",
        {}, nullptr, chunk_bytes);
    const auto text_output = xml::parse(xml::serialize_tokens(text));
    if (text_output.namespace_bindings.contains("q0") || text.fallback != "q0:literal") {
      throw std::runtime_error{"QName text default changed another union value"};
    }
    const auto early = xml::deserialize_tokens<qname_lists::early_default>(
        R"(<t:early_default xmlns:t="urn:qname-lists"><t:fallback>q&#48;:literal</t:fallback></t:early_default>)",
        {}, nullptr, chunk_bytes);
    const auto early_output = xml::parse(xml::serialize_tokens(early));
    if (early_output.namespace_bindings.contains("q0") || early.fallback != "q0:literal") {
      throw std::runtime_error{"Early QName default changed a later character-reference union value"};
    }
  }
  if (value.names.size() != 3U || value.choice != "p:first local p:second") {
    throw std::runtime_error{"QName list native values changed"};
  }
  auto changed = value;
  changed.fixed_names = {"q:first", "local", "q:second"};
  bool rejected{};
  try { (void)xml::serialize_tokens(changed); }
  catch (const std::invalid_argument&) { rejected = true; }
  if (!rejected) { throw std::runtime_error{"Edited fixed QName list was published"}; }
  const std::string invalid = R"(<t:sample xmlns:t="urn:qname-lists" xmlns:p="urn:allowed" fixed_names="p:first local p:second"><t:names>p:second local p:first</t:names><t:choice>65535</t:choice></t:sample>)";
  rejected = false;
  try { (void)xml::deserialize_tokens<qname_lists::sample>(invalid); }
  catch (const std::invalid_argument&) { rejected = true; }
  if (!rejected) { throw std::runtime_error{"Invalid QName list enumeration was admitted"}; }
}
