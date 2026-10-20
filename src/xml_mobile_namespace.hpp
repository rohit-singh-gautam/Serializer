// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <algorithm>

namespace rohit::serializer::writer::xml_codegen {
// Detect every lexical branch whose meaning requires its original namespace vocabulary.
inline bool mobile_namespace_value(const xml_binding::scalar_facets& facets) {
  return facets.lexical_kind == "QName" || facets.list_item_kind == "QName" ||
      std::any_of(facets.union_members.begin(), facets.union_members.end(),
          [](const auto& member) { return mobile_namespace_value(member); });
}

// Keep namespace-only storage separate from native model fields and ordinary string scalars.
inline bool mobile_namespace_value(const xml_binding::field& field) {
  return field.qname || field.prefix_list || mobile_namespace_value(field.facets);
}

// Add lexical namespace state only to records that actually declare namespace-dependent values.
inline bool mobile_namespace_value(const xml_binding::record& record) {
  return std::any_of(record.fields.begin(), record.fields.end(),
      [](const auto& field) { return mobile_namespace_value(field); });
}
} // namespace rohit::serializer::writer::xml_codegen
