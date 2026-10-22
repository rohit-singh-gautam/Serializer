// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <rohit/xml_pattern.hpp>

#include <map>
#include <optional>
#include <string>

namespace rohit::serializer::writer::xml_binding {

// Emit the shared immutable Thompson program without relying on the JavaScript regex dialect.
inline std::string js_pattern_metadata(const xml::portable_pattern& program) {
  std::string result = "Object.freeze([" + std::to_string(program.start()) + ",Object.freeze([";
  for (const auto& state : program.states()) {
    result += "Object.freeze([Object.freeze([";
    for (const auto target : state.epsilon) { result += std::to_string(target) + ','; }
    result += "]),Object.freeze([";
    for (const auto& range : state.ranges) {
      result += "Object.freeze([" + std::to_string(range.first) + ',' + std::to_string(range.last) + "]),";
    }
    result += "])," + std::to_string(state.next) + ',' + (state.consumes ? "true" : "false") + ',' +
        (state.inverted ? "true" : "false") + ',' + (state.dot ? "true" : "false") + ',' +
        (state.accepting ? "true" : "false") + "]),";
  }
  return result + "])])";
}

// Retain recursive list/union restrictions and exact expanded QName facet identities.
inline std::string js_union_identity(const scalar_value_identity& identity) {
  std::string result = "srlXmlSchemaIdentity(" + native::quote(identity.lexical_kind) + ',' +
      native::quote(identity.list_item_kind) + ',' + native::quote(identity.lexical_value) + ",Object.freeze([";
  for (const auto& name : identity.names) {
    result += "srlXmlTokenName(" + native::quote(name.namespace_uri) + ',' + native::quote(name.local) + "),";
  }
  result += "]),Object.freeze([";
  for (const auto& item : identity.items) { result += js_union_identity(item) + ','; }
  return result + "]))";
}

// Keep fixed field identities separate from recursive reusable scalar alternatives.
inline std::string js_scalar_facets(const scalar_facets& facets, const std::string& fallback,
    const std::optional<std::string>& fixed = {}, const std::optional<xml::name>& fixed_name = {},
    bool qname = false, bool prefix_list = false,
    const std::optional<std::vector<xml::name>>& fixed_names = {},
    const std::optional<scalar_value_identity>& fixed_union = {}) {
  const auto kind = qname ? std::string{"QName"} : facets.lexical_kind.empty() ? fallback : facets.lexical_kind;
  std::string result = "Object.freeze({kind:" + native::quote(kind) + ",whitespace:" + native::quote(facets.whitespace) +
      ",finite:" + (facets.finite ? "true" : "false");
  const auto count = [&](const char* key, const auto& selected) {
    result += ',' + std::string{key} + ':' + (selected ? std::to_string(*selected) : "null");
  };
  const auto lexical = [&](const char* key, const auto& selected) {
    result += ',' + std::string{key} + ':' + (selected ? native::quote(*selected) : "null");
  };
  count("length", facets.length); count("minLength", facets.min_length); count("maxLength", facets.max_length);
  lexical("minInclusive", facets.min_inclusive); lexical("maxInclusive", facets.max_inclusive);
  lexical("minExclusive", facets.min_exclusive); lexical("maxExclusive", facets.max_exclusive);
  lexical("fixed", fixed);
  result += ",fixedName:" + (fixed_name ? "srlXmlTokenName(" + native::quote(fixed_name->namespace_uri) + ',' + native::quote(fixed_name->local) + ')' : "null") +
      ",prefixList:" + (prefix_list ? "true" : "false") + ",listItem:" + native::quote(facets.list_item_kind) +
      ",qnames:Object.freeze([";
  for (const auto& name : facets.qname_enumeration) {
    result += "srlXmlTokenName(" + native::quote(name.namespace_uri) + ',' + native::quote(name.local) + "),";
  }
  result += "]),qnameLists:Object.freeze([";
  for (const auto& names : facets.qname_list_enumeration) {
    result += "Object.freeze([";
    for (const auto& name : names) {
      result += "srlXmlTokenName(" + native::quote(name.namespace_uri) + ',' + native::quote(name.local) + "),";
    }
    result += "]),";
  }
  result += "]),fixedNames:";
  if (fixed_names) {
    result += "Object.freeze([";
    for (const auto& name : *fixed_names) {
      result += "srlXmlTokenName(" + native::quote(name.namespace_uri) + ',' + native::quote(name.local) + "),";
    }
    result += "])";
  } else { result += "null"; }
  result += ",union:Object.freeze([";
  for (const auto& alternative : facets.union_members) {
    result += js_scalar_facets(alternative, "string") + ',';
  }
  result += "]),itemRule:" + (facets.list_item_facets.empty() ? std::string{"null"} : js_scalar_facets(facets.list_item_facets.front(), "string"));
  result += ",unionValues:Object.freeze([";
  for (const auto& identity : facets.union_enumeration) { result += js_union_identity(identity) + ','; }
  result += "]),fixedUnion:" + (fixed_union ? js_union_identity(*fixed_union) : "null");
  result += ",enumeration:Object.freeze([";
  for (const auto& item : facets.enumeration) { result += native::quote(item) + ','; }
  result += "]),patterns:Object.freeze([";
  auto groups = facets.pattern_groups;
  if (!facets.patterns.empty()) { groups.push_back(facets.patterns); }
  for (const auto& group : groups) {
    result += "Object.freeze([";
    for (const auto& expression : group) {
      result += js_pattern_metadata(xml::compile_portable_pattern(expression)) + ',';
    }
    result += "]),";
  }
  return result + "])})";
}

// Retain exact lexical bounds and complete pattern groups as static owning-field metadata.
inline std::string js_scalar_metadata(const field& value, const type_name& leaf) {
  const std::map<std::string, std::string> kinds{{"bool", "boolean"}, {"int8", "byte"}, {"uint8", "unsignedByte"},
      {"int16", "short"}, {"uint16", "unsignedShort"}, {"int32", "int"}, {"uint32", "unsignedInt"},
      {"int64", "long"}, {"uint64", "unsignedLong"}};
  const auto found = kinds.find(leaf.name);
  const auto fallback = leaf.type == object_type::enum_type ? std::string{"string"} :
      found == kinds.end() ? leaf.name : found->second;
  return js_scalar_facets(value.facets, fallback, value.fixed_value, value.qname_fixed, value.qname,
      value.prefix_list, value.qname_list_fixed, value.union_fixed);
}

} // namespace rohit::serializer::writer::xml_binding
