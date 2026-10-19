// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace rohit::serializer::writer::python_xml {
// Preserve expanded QName identity independently of the source schema's presentation prefix.
inline std::string name(const xml::name& value) {
  return "(" + native::quote(value.namespace_uri) + ", " + native::quote(value.local) + ")";
}

// Emit complete immutable scalar/union metadata rather than interpreting XSD during every codec call.
inline std::string facets(const xml_binding::scalar_facets& value, std::string kind = {}) {
  if (!value.lexical_kind.empty()) { kind = value.lexical_kind; }
  std::string result = "{'kind': " + native::quote(kind) + ", 'whitespace': " + native::quote(value.whitespace) +
      ", 'finite': " + (value.finite ? "True" : "False");
  const auto number = [&](const char* key, const auto& selected) {
    result += ", '" + std::string{key} + "': " + (selected ? std::to_string(*selected) : "None");
  };
  const auto lexical = [&](const char* key, const auto& selected) {
    result += ", '" + std::string{key} + "': " + (selected ? native::quote(*selected) : "None");
  };
  number("length", value.length); number("min_length", value.min_length); number("max_length", value.max_length);
  lexical("min_inclusive", value.min_inclusive); lexical("max_inclusive", value.max_inclusive);
  lexical("min_exclusive", value.min_exclusive); lexical("max_exclusive", value.max_exclusive);
  result += ", 'enumeration': (";
  for (const auto& selected : value.enumeration) { result += native::quote(selected) + ","; }
  result += "), 'qnames': (";
  for (const auto& selected : value.qname_enumeration) { result += name(selected) + ","; }
  result += "), 'patterns': (";
  auto groups = value.pattern_groups;
  if (!value.patterns.empty()) { groups.push_back(value.patterns); }
  for (const auto& group : groups) {
    result += '(';
    for (const auto& pattern : group) { result += native::quote(pattern) + ','; }
    result += "),";
  }
  result += "), 'list_item': " + native::quote(value.list_item_kind) + ", 'union': (";
  for (const auto& alternative : value.union_members) { result += facets(alternative) + ','; }
  return result + "), 'qname': False, 'prefix_list': False, 'fixed': None, 'fixed_qname': None}";
}

// Bind field-only lexical adapters without changing the underlying ordinary model representation.
inline std::string field(const xml_binding::field& value, const type_name& leaf) {
  const std::map<std::string, std::string> kinds{{"bool", "boolean"}, {"int8", "byte"}, {"uint8", "unsignedByte"},
      {"int16", "short"}, {"uint16", "unsignedShort"}, {"int32", "int"}, {"uint32", "unsignedInt"},
      {"int64", "long"}, {"uint64", "unsignedLong"}, {"float32", "float"}, {"float64", "double"}};
  const auto found = kinds.find(leaf.name);
  const auto kind = leaf.type == object_type::enum_type ? std::string{"string"} :
      found == kinds.end() ? leaf.name : found->second;
  auto selected = value.facets;
  if (value.list && selected.list_item_kind.empty()) { selected.list_item_kind = kind; }
  if (value.list && value.source->fixed_extent && !selected.length) {
    selected.length = static_cast<std::size_t>(value.source->fixed_extent);
  }
  auto result = facets(selected, kind);
  result.pop_back();
  result += ", 'qname': " + std::string{value.qname ? "True" : "False"} +
      ", 'prefix_list': " + (value.prefix_list ? "True" : "False") +
      ", 'fixed': " + (value.fixed_value ? native::quote(*value.fixed_value) : "None") +
      ", 'fixed_qname': " + (value.qname_fixed ? name(*value.qname_fixed) : "None") + "}";
  return result;
}

// Retain wildcard positions and namespace/process rules for exact generated sequence validation.
inline std::string record(const xml_binding::record& value) {
  const auto unknown = !value.unknown || *value.unknown == xml_unknown_policy::reject ? "reject" :
      *value.unknown == xml_unknown_policy::skip ? "skip" : "preserve";
  std::string result = "{'namespace': " + native::quote(value.root.namespace_uri) + ", 'unknown': " +
      native::quote(unknown) + ", 'wildcards': (";
  for (const auto& wildcard : value.wildcards) {
    result += "{'attribute': " + std::string{wildcard.attribute ? "True" : "False"} +
        ", 'namespace': " + native::quote(wildcard.namespace_rule) +
        ", 'target': " + native::quote(wildcard.target_namespace.value_or(value.root.namespace_uri)) +
        ", 'process': " + native::quote(wildcard.process_contents) +
        ", 'minimum': " + std::to_string(wildcard.minimum) +
        ", 'maximum': " + (wildcard.maximum ? std::to_string(*wildcard.maximum) : "None") +
        ", 'position': " + std::to_string(wildcard.before_field) + "},";
  }
  result += "), 'known_children': (";
  for (const auto& selected : value.fields) {
    if (selected.attribute || selected.text) { continue; }
    const auto& qualified = selected.wrapper ? *selected.wrapper : selected.qualified_name;
    result += native::quote(qualified.namespace_uri.empty() ? qualified.local :
        "{" + qualified.namespace_uri + "}" + qualified.local) + ',';
  }
  result += "), 'known_attributes': (";
  for (const auto& selected : value.fields) {
    if (selected.attribute) {
      result += native::quote(selected.qualified_name.namespace_uri.empty() ? selected.qualified_name.local :
          "{" + selected.qualified_name.namespace_uri + "}" + selected.qualified_name.local) + ',';
    }
  }
  result += "), 'choices': (";
  for (const auto& choice : value.choices) {
    if (!choice.repeated) { continue; }
    result += "{'position': " + std::to_string(choice.before_field) +
        ", 'minimum': " + std::to_string(choice.minimum) +
        ", 'maximum': " + (choice.max_repetitions ? std::to_string(*choice.max_repetitions) : "None") +
        ", 'branches': (";
    for (const auto& branch : choice.branches) {
      std::string selected{"None"}, namespace_rule{}, target_namespace{value.root.namespace_uri};
      if (branch.source) {
        const auto found = std::find_if(value.fields.begin(), value.fields.end(),
            [&](const auto& item) { return item.source == branch.source; });
        if (found == value.fields.end() || found->attribute || found->text || found->wrapper) {
          throw std::invalid_argument{"Python repeated XML choices require unwrapped child fields"};
        }
        const auto& qualified = found->qualified_name;
        selected = native::quote(qualified.namespace_uri.empty() ? qualified.local :
            "{" + qualified.namespace_uri + "}" + qualified.local);
      } else if (branch.wildcard_index) {
        const auto& wildcard = value.wildcards.at(*branch.wildcard_index);
        namespace_rule = wildcard.namespace_rule;
        target_namespace = wildcard.target_namespace.value_or(value.root.namespace_uri);
      }
      result += "{'name': " + selected + ", 'namespace': " + native::quote(namespace_rule) +
          ", 'target': " + native::quote(target_namespace) +
          ", 'minimum': " + std::to_string(branch.minimum) +
          ", 'maximum': " + (branch.maximum ? std::to_string(*branch.maximum) : "None") + "},";
    }
    result += ")},";
  }
  return result + ")}";
}

// Select only records whose XML-only state must retain namespace declarations or opaque extensions.
inline bool needs_state(const xml_binding::record& value) {
  return value.unknown == xml_unknown_policy::preserve || std::any_of(value.fields.begin(), value.fields.end(),
      [](const auto& selected) { return selected.qname || selected.prefix_list || selected.facets.list_item_kind == "QName"; });
}
} // namespace rohit::serializer::writer::python_xml
