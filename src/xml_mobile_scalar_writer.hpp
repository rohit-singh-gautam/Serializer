// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"
#include "xml_mobile_namespace.hpp"

#include <rohit/xml_pattern.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Retain the shared Thompson instruction contract without using target-language regex dialects.
inline std::string mobile_pattern_blob(const xml::portable_pattern& pattern) {
  constexpr unsigned consumes{1U}, inverted{2U}, dot{4U}, accepting{8U};
  std::string result = std::to_string(pattern.start());
  for (const auto& state : pattern.states()) {
    result += ';';
    for (std::size_t index{}; index < state.epsilon.size(); ++index) {
      if (index != 0U) { result += ','; }
      result += std::to_string(state.epsilon[index]);
    }
    result += '|';
    for (std::size_t index{}; index < state.ranges.size(); ++index) {
      if (index != 0U) { result += ','; }
      result += std::to_string(state.ranges[index].first) + ',' + std::to_string(state.ranges[index].last);
    }
    const auto flags = (state.consumes ? consumes : 0U) | (state.inverted ? inverted : 0U) |
        (state.dot ? dot : 0U) | (state.accepting ? accepting : 0U);
    result += '|' + std::to_string(state.next) + '|' + std::to_string(flags);
  }
  return result;
}

// Derive target admission constants from the same shared limits used when compiling programs.
inline std::string mobile_scalar_constants(bool swift) {
  const xml::pattern_limits limits{};
  const std::string declaration = swift ? "fileprivate let " : "internal const val ";
  return declaration + "srlXmlMaxPatternStates = " + std::to_string(limits.max_states) + '\n' +
      declaration + "srlXmlMaxPatternBytes = " + std::to_string(limits.max_input_bytes) + '\n' +
      declaration + "srlXmlMaxPatternWork = " + std::to_string(limits.max_work_units) + '\n' +
      declaration + "srlXmlMaxListItems = " + std::to_string(xml::limits{}.max_nodes) + '\n';
}

// Emit immutable recursive scalar metadata once per generated field, including grouped patterns.
inline std::string mobile_scalar_rule(const xml_binding::field& field,
    const std::function<std::string(std::string_view)>& quote, bool swift) {
  const std::string start = swift ? "[" : "arrayOf(", end = swift ? "]" : ")";
  const auto identity_literal = [&](auto&& self, const xml_binding::scalar_value_identity& identity) -> std::string {
    std::string result = "SrlXmlRule.UnionValue(" + quote(identity.lexical_kind) + ',' + quote(identity.list_item_kind) + ',' + quote(identity.lexical_value) + ',' + start;
    for (const auto& item : identity.names) {
      result += "SrlXmlName(" + std::string{swift ? "local:" : ""} + quote(item.local) + ',' +
          (swift ? "namespace:" : "") + quote(item.namespace_uri) + "),";
    }
    result += end + ',' + start;
    for (const auto& item : identity.items) { result += self(self, item) + ','; }
    return result + end + ')';
  };
  // Reject unrepresentable target lengths before generating any consumer source.
  const auto extent = [&](const std::optional<std::size_t>& count) {
    if (count && *count > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
      throw std::invalid_argument{"Mobile XML facet length exceeds signed 64-bit range"};
    }
    return (count ? std::to_string(*count) : "-1") + (swift ? "" : "L");
  };
  // Preserve independent union members and inherited pattern groups in the shared value contract.
  const auto rule = [&](auto&& self, const xml_binding::scalar_facets& facets,
                        const std::optional<std::string>& fixed) -> std::string {
    std::string result = "SrlXmlRule(" + quote(facets.lexical_kind) + ',' + quote(facets.whitespace) + ',' + start;
    for (const auto* bound : {&facets.min_inclusive, &facets.max_inclusive, &facets.min_exclusive, &facets.max_exclusive}) {
      result += quote(bound->value_or("")) + ',';
    }
    result += end + ',' + extent(facets.length) + ',' + extent(facets.min_length) + ',' + extent(facets.max_length) + ',' + start;
    for (const auto& value : facets.enumeration) { result += quote(value) + ','; }
    result += end + ',' + (facets.finite ? "true" : "false") + ',' + start;
    auto groups = facets.pattern_groups;
    if (!facets.patterns.empty()) { groups.push_back(facets.patterns); }
    constexpr std::size_t maximum_fragment_bytes{10000U};
    for (const auto& group : groups) {
      result += start;
      for (const auto& expression : group) {
        result += "SrlXmlRule.Pattern.prepare(" + start;
        const auto blob = mobile_pattern_blob(xml::compile_portable_pattern(expression));
        for (std::size_t offset{}; offset < blob.size(); offset += maximum_fragment_bytes) {
          result += quote(std::string_view{blob}.substr(offset, maximum_fragment_bytes)) + ',';
        }
        result += end + "),";
      }
      result += end + ',';
    }
    result += end + ',' + quote(facets.list_item_kind) + ',' + start;
    for (const auto& member : facets.union_members) { result += self(self, member, {}) + ','; }
    result += end + ',' + (fixed ? quote(*fixed) : swift ? "nil" : "null") + ')';
    if (!facets.qname_enumeration.empty() || !facets.qname_list_enumeration.empty()) {
      const auto name = [&](const auto& value) {
        return std::string{"SrlXmlName("} + (swift ? "local:" : "") + quote(value.local) + ',' +
            (swift ? "namespace:" : "") + quote(value.namespace_uri) + ')';
      };
      result += ".namespaceValues(" + start;
      for (const auto& value : facets.qname_enumeration) { result += name(value) + ','; }
      result += end + ',' + start;
      for (const auto& values : facets.qname_list_enumeration) {
        result += start;
        for (const auto& value : values) { result += name(value) + ','; }
        result += end + ',';
      }
      result += end + ')';
    }
    if (!facets.union_enumeration.empty()) {
      result += ".unionValues(" + start;
      for (const auto& identity : facets.union_enumeration) { result += identity_literal(identity_literal, identity) + ','; }
      result += end + ')';
    }
    if (!facets.list_item_facets.empty()) { result += ".listItemRule(" + self(self, facets.list_item_facets.front(), {}) + ')'; }
    return result;
  };
  auto result = rule(rule, field.facets, field.fixed_value);
  if (field.union_fixed) { result += ".unionFixed(" + identity_literal(identity_literal, *field.union_fixed) + ')'; }
  else if (field.qname_fixed || field.qname_list_fixed) {
    xml_binding::scalar_value_identity identity{};
    identity.lexical_kind = field.facets.lexical_kind;
    identity.list_item_kind = field.facets.list_item_kind;
    identity.lexical_value = *field.fixed_value;
    if (field.qname_fixed) { identity.names.push_back(*field.qname_fixed); }
    else { identity.names = *field.qname_list_fixed; }
    result += ".unionFixed(" + identity_literal(identity_literal, identity) + ')';
  }
  if (field.union_fixed || field.union_default || mobile_namespace_default(field)) {
    result += ".namespaceDefault(" + quote(field.fixed_value ? *field.fixed_value : *field.default_value) + ',' +
        std::string{swift ? "[" : "mapOf("};
    for (const auto& [prefix, uri] : mobile_default_bindings(field)) {
      result += swift ? "WireString(" + quote(prefix) + "):" + quote(uri) + ',' : quote(prefix) + " to " + quote(uri) + ',';
    }
    result += swift ? "])" : "))";
  }
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
