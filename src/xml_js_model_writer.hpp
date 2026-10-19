// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_js_variant_writer.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace rohit::serializer::writer::xml_binding {

// Locate the declaring owning record by stable member identity through the existing native bases.
inline const class_node* js_declaring_record(const class_node& owner, const member& source) {
  for (const auto& field : owner.member_list) {
    if (&field == &source) { return &owner; }
  }
  for (const auto& parent : owner.parents) {
    if (const auto* declaring = js_declaring_record(*parent.parent_class, source)) { return declaring; }
  }
  return nullptr;
}

// Retain the original declaring-field identity even when derived mappings change XML spellings.
inline std::string js_native_field_identity(const class_node& owner, const member& source) {
  const auto* declaring = js_declaring_record(owner, source);
  if (!declaring) { throw std::logic_error{"XML member does not belong to its native owning graph"}; }
  return declaring->get_full_name() + "::" + source.name;
}

// Rebase a parent's own lexical private-safe callbacks onto this class's composed native base object.
inline std::optional<js_variant_callbacks> js_inherited_access(const class_node& owner, const member& source,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier) {
  for (const auto& field : owner.member_list) {
    if (&field == &source) { return std::nullopt; }
  }
  for (std::size_t index{}; index < owner.parents.size(); ++index) {
    const auto& parent = owner.parents[index];
    if (!js_declaring_record(*parent.parent_class, source)) { continue; }
    member naming{};
    naming.name = "base" + std::to_string(index);
    naming.access = parent.access;
    type_name parent_type{std::string{parent.parent_class->name}, nullptr};
    parent_type.type = object_type::class_type;
    parent_type.resolved_node = parent.parent_class;
    const auto descriptor = type_identifier(parent_type) + "._srlXmlToken.fields.find(field=>field.native===" +
        native::quote(js_native_field_identity(owner, source)) + ')';
    const auto base = "owner." + field_name(naming);
    return js_variant_callbacks{"owner=>" + descriptor + ".get(" + base + ')',
        "(owner,value)=>{" + descriptor + ".set(" + base + ",value);}"};
  }
  throw std::logic_error{"XML inherited member has no composed native base path"};
}

} // namespace rohit::serializer::writer::xml_binding
