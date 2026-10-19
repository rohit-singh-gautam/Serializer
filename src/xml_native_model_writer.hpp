// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "version_writer.hpp"
#include "xml_schema.hpp"
#include <exception>
#include <functional>
#include <string>
#include <set>
#include <algorithm>
#include <utility>

namespace rohit::serializer::writer::xml_codegen {
// Close a generated conditional even when its field emitter returns through an early branch.
class model_emission_scope {
  std::function<void()> finish_{};
public:
  // Take an optional closing callback; the surrounding output buffer must outlive this scope.
  explicit model_emission_scope(std::function<void()> finish) : finish_{std::move(finish)} {}
  model_emission_scope(const model_emission_scope&) = delete;
  model_emission_scope& operator=(const model_emission_scope&) = delete;
  // Finish the selected generated scope without changing runtime model values.
  ~model_emission_scope() noexcept(false) {
    // Failed generation discards its output; do not allocate closing syntax during exception unwinding.
    if (finish_ && std::uncaught_exceptions() == 0) { finish_(); }
  }
};
// Select one lifecycle predicate using the original exact native revision storage.
inline std::string native_xml_active(const xml_binding::record& contract, const member& field,
    version_language language, const std::string& revision) {
  return contract.model.revision ? version_active(language, field, *contract.model.revision, revision) : std::string{};
}
// Mark complete reference-containing graphs export-only before selecting any native XML input codec.
inline bool native_xml_export_graph(const class_node& root) {
  std::set<const class_node*> visited{};
  std::function<bool(const type_name&)> contains_type;
  const auto contains_record = [&](auto&& self, const class_node& owner) -> bool {
    if (owner.reference && owner.reference->value_projection) { return true; }
    if (!visited.insert(&owner).second) { return false; }
    for (const auto& base : owner.parents) { if (self(self, *base.parent_class)) { return true; } }
    for (const auto& field : owner.member_list) {
      if (field.omits("xml") || field.wire_only || field.persistent_identity) { continue; }
      if ((field.key_type && contains_type(*field.key_type)) ||
          std::any_of(field.type_name_list.begin(), field.type_name_list.end(), contains_type)) { return true; }
    }
    return false;
  };
  contains_type = [&](const type_name& type) {
    if (type.type == object_type::class_type && type.resolved_node &&
        contains_record(contains_record, static_cast<const class_node&>(*type.resolved_node))) { return true; }
    return std::any_of(type.arguments.begin(), type.arguments.end(), contains_type);
  };
  return contains_record(contains_record, root);
}
// Bound accepted native historical revisions independently from their lexical XML adapter.
inline std::string native_xml_revision_invalid(const member& revision, version_language language,
    const std::string& value) {
  return version_less(language, revision, value, schema_version::minimum(revision)) + " || !(" +
      version_less(language, revision, value, revision.default_value) + " || " +
      version_equal(language, revision, value, revision.default_value) + ")";
}
} // namespace rohit::serializer::writer::xml_codegen
