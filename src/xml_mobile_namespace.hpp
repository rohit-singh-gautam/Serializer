// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::xml_codegen {
// Detect every lexical branch whose meaning requires its original namespace vocabulary.
inline bool mobile_namespace_value(const xml_binding::scalar_facets& facets) {
  return facets.lexical_kind == "QName" || facets.list_item_kind == "QName" ||
      std::any_of(facets.union_members.begin(), facets.union_members.end(),
          [](const auto& member) { return mobile_namespace_value(member); }) ||
      std::any_of(facets.list_item_facets.begin(), facets.list_item_facets.end(),
          [](const auto& item) { return mobile_namespace_value(item); });
}

// Keep namespace-only storage separate from native model fields and ordinary string scalars.
inline bool mobile_namespace_value(const xml_binding::field& field) {
  return field.qname || field.prefix_list || mobile_namespace_value(field.facets);
}

// Keep scalar schema defaults separate from lexical bound-prefix-list annotations.
inline bool mobile_namespace_default(const xml_binding::field& field) {
  return field.qname_default || field.qname_fixed || field.qname_list_default || field.qname_list_fixed ||
      (mobile_namespace_value(field) && (field.union_default || field.union_fixed));
}

// Reconstruct only the schema lexical aliases needed by one QName/default occurrence.
inline std::map<std::string, std::string> mobile_default_bindings(const xml_binding::field& field) {
  if (field.union_fixed) { return field.union_fixed_scope; }
  if (field.union_default) { return field.union_default_scope; }
  const auto& text = field.fixed_value ? field.fixed_value : field.default_value;
  std::vector<xml::name> names;
  if (field.qname_fixed || field.qname_default) { names.push_back(field.qname_fixed ? *field.qname_fixed : *field.qname_default); }
  else if (field.qname_list_fixed || field.qname_list_default) { names = field.qname_list_fixed ? *field.qname_list_fixed : *field.qname_list_default; }
  std::map<std::string, std::string> result;
  constexpr std::string_view whitespace{" \t\r\n"};
  std::size_t offset{}, index{};
  while (text && (offset = text->find_first_not_of(whitespace, offset)) != std::string::npos) {
    const auto end = text->find_first_of(whitespace, offset);
    const auto token = std::string_view{*text}.substr(offset, end - offset);
    const auto colon = token.find(':');
    if (index >= names.size()) { throw std::logic_error{"Missing mobile schema QName default identity"}; }
    result.emplace(colon == std::string_view::npos ? "" : std::string{token.substr(0U, colon)}, names[index++].namespace_uri);
    if (end == std::string::npos) { break; }
    offset = end;
  }
  result.try_emplace("", "");
  return result;
}

// Add lexical namespace state only to records that actually declare namespace-dependent values.
inline bool mobile_namespace_value(const xml_binding::record& record) {
  return std::any_of(record.fields.begin(), record.fields.end(),
      [](const auto& field) { return mobile_namespace_value(field); });
}
} // namespace rohit::serializer::writer::xml_codegen
