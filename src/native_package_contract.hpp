// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_portable_writer.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::package_codegen {
// Reject owning package graphs whose XML metadata lives only in an unsupported document companion.
inline void require_native_package_graph(const package_part_declaration& part,
    const xml_options& options, std::string_view language) {
  if (!part.value_type) { return; }
  const xml_binding::schema bindings{options};
  std::set<const class_node*> visited;
  const auto visit = [&](auto&& self, const class_node& owner) -> void {
    if (!visited.insert(&owner).second) { return; }
    if (owner.reference) {
      throw std::invalid_argument{std::string{language} +
          " typed package roles require ordinary owning XML records: " + part.name};
    }
    const auto contract = bindings.bind(owner);
    if (contract.extensions || std::any_of(contract.fields.begin(), contract.fields.end(),
            [](const auto& field) { return xml_portable::namespace_dependent(field); })) {
      throw std::invalid_argument{std::string{language} +
          " package XML document-companion namespace/extension graphs are not implemented: " +
          part.name};
    }
    for (const auto& base : owner.parents) { self(self, *base.parent_class); }
    const auto visit_type = [&](auto&& walk, const type_name& type) -> void {
      if (type.resolved_node && type.resolved_node->type == object_type::class_type) {
        self(self, static_cast<const class_node&>(*type.resolved_node));
      }
      for (const auto& argument : type.arguments) { walk(walk, argument); }
    };
    for (const auto& field : owner.member_list) {
      for (const auto& type : field.type_name_list) { visit_type(visit_type, type); }
      if (field.key_type) { visit_type(visit_type, *field.key_type); }
    }
  };
  visit(visit, static_cast<const class_node&>(*part.value_type->resolved_node));
}
} // namespace rohit::serializer::writer::package_codegen
