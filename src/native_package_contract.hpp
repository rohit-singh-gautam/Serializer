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
    const xml_options& options, std::string_view language, bool document_companions = false) {
  if (!part.value_type) { return; }
  const xml_binding::schema bindings{options};
  const bool fixed_native = part.codec != "xml" && part.profile_policy == "fixed";
  std::set<const class_node*> visited;
  const auto visit = [&](auto&& self, const class_node& owner) -> void {
    if (!visited.insert(&owner).second) { return; }
    if (owner.reference) {
      throw std::invalid_argument{std::string{language} +
          " typed package roles require ordinary owning XML records: " + part.name};
    }
    if (!fixed_native && !document_companions) {
      const auto contract = bindings.bind(owner);
      if (contract.extensions || std::any_of(contract.fields.begin(), contract.fields.end(),
              [](const auto& field) { return xml_portable::namespace_dependent(field); })) {
        throw std::invalid_argument{std::string{language} +
            " package XML document-companion namespace/extension graphs are not implemented: " +
            part.name};
      }
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
// Detect XML-only state through the full bound graph without changing native model storage.
inline bool native_package_companion_required(const package_part_declaration& part, const xml_options& options) {
  if (!part.value_type) { return false; }
  const xml_binding::schema bindings{options};
  std::set<const class_node*> visited{};
  const auto visit = [&](auto&& self, const class_node& owner) -> bool {
    if (!visited.insert(&owner).second) { return false; }
    const auto contract = bindings.bind(owner);
    if (contract.extensions || std::any_of(contract.fields.begin(), contract.fields.end(),
        [](const auto& field) { return xml_portable::namespace_dependent(field); })) { return true; }
    for (const auto& base : owner.parents) { if (self(self, *base.parent_class)) { return true; } }
    const auto visit_type = [&](auto&& walk, const type_name& type) -> bool {
      if (type.resolved_node && type.resolved_node->type == object_type::class_type &&
          self(self, static_cast<const class_node&>(*type.resolved_node))) { return true; }
      for (const auto& argument : type.arguments) { if (walk(walk, argument)) { return true; } }
      return false;
    };
    for (const auto& field : owner.member_list) {
      for (const auto& type : field.type_name_list) { if (visit_type(visit_type, type)) { return true; } }
      if (field.key_type && visit_type(visit_type, *field.key_type)) { return true; }
    }
    return false;
  };
  return visit(visit, static_cast<const class_node&>(*part.value_type->resolved_node));
}
} // namespace rohit::serializer::writer::package_codegen
