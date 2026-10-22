// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::xml_codegen {
// Namespace-dependent union selection requires the occurrence scope before choosing a member.
inline bool native_scalar_uses_namespaces(const xml_binding::scalar_facets& facets) {
  return facets.lexical_kind == "QName" || facets.list_item_kind == "QName" ||
      std::any_of(facets.union_members.begin(), facets.union_members.end(), native_scalar_uses_namespaces) ||
      std::any_of(facets.list_item_facets.begin(), facets.list_item_facets.end(), native_scalar_uses_namespaces);
}

// Preserve schema-selected enum identities, including ordered QName list items, without lexical aliases.
inline std::vector<xml_binding::scalar_value_identity> native_scalar_identities(
    const xml_binding::scalar_facets& facets) {
  auto result = facets.union_enumeration;
  for (const auto& name : facets.qname_enumeration) {
    result.push_back({"QName", {}, {}, {name}});
  }
  for (const auto& names : facets.qname_list_enumeration) {
    result.push_back({{}, "QName", {}, names});
  }
  return result;
}

// Keep a fixed value's schema identity separate from its authored lexical prefix spelling.
inline std::optional<xml_binding::scalar_value_identity> native_scalar_fixed_identity(
    const xml_binding::field& field) {
  if (field.union_fixed) { return field.union_fixed; }
  if (field.qname_fixed) { return xml_binding::scalar_value_identity{"QName", {}, {}, {*field.qname_fixed}}; }
  if (field.qname_list_fixed) { return xml_binding::scalar_value_identity{{}, "QName", {}, *field.qname_list_fixed}; }
  return {};
}

// Retain the exact native spelling produced when an XML occurrence applies its schema literal.
inline std::optional<std::string> native_scalar_default_lexical(const xml_binding::field& field) {
  if (!native_scalar_uses_namespaces(field.facets)) { return {}; }
  const auto& fallback = field.fixed_value ? field.fixed_value : field.default_value;
  if (!fallback || !field.facets.union_members.empty()) { return fallback; }
  const auto whitespace = field.facets.whitespace == "collapse" ? xml::whitespace_policy::collapse :
      field.facets.whitespace == "replace" ? xml::whitespace_policy::replace : xml::whitespace_policy::preserve;
  return xml::normalize_whitespace(*fallback, whitespace);
}

// Emit immutable Rust identity constants whose namespace names borrow only generated static strings.
inline std::string rust_scalar_identity(const xml_binding::scalar_value_identity& identity,
    const std::function<std::string(std::string_view)>& quote) {
  std::string result = "SrlXmlScalarIdentity{kind:" + quote(identity.lexical_kind) +
      ",list_kind:" + quote(identity.list_item_kind) + ",lexical:" + quote(identity.lexical_value) + ",names:&[";
  for (const auto& name : identity.names) {
    result += '(' + quote(name.namespace_uri) + ',' + quote(name.local) + "),";
  }
  result += "],items:&[";
  for (const auto& item : identity.items) { result += rust_scalar_identity(item, quote) + ','; }
  return result + "]}";
}
} // namespace rohit::serializer::writer::xml_codegen
