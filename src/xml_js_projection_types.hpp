// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"

#include <set>
#include <stdexcept>

namespace rohit::serializer::writer::xml_binding {

// Resolve readonly selected values without replacing their native handle/container storage.
inline const type_name& js_projected_leaf(const type_name& value) {
  const auto* selected = &value;
  std::set<const class_node*> references;
  while (true) {
    while (selected->is_optional() || selected->is_sequence()) { selected = &selected->arguments.front(); }
    if (selected->type == object_type::class_type) {
      const auto& owner = static_cast<const class_node&>(*selected->resolved_node);
      if (owner.reference && owner.reference->value_projection) {
        if (!references.insert(&owner).second) { throw std::invalid_argument{"Cyclic readonly XML value projection"}; }
        selected = &owner.reference->value_type;
        continue;
      }
    }
    return *selected;
  }
}

// Identify readonly handles beneath existing optional/repeated storage before borrowing their values.
inline bool js_value_projection(const type_name& value) {
  const auto* selected = &value;
  while (selected->is_optional() || selected->is_sequence()) { selected = &selected->arguments.front(); }
  if (selected->type != object_type::class_type) { return false; }
  const auto& owner = static_cast<const class_node&>(*selected->resolved_node);
  return owner.reference && owner.reference->value_projection;
}

} // namespace rohit::serializer::writer::xml_binding
