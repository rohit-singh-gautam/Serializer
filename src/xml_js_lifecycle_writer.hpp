// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "schema_version.hpp"
#include "xml_js_model_writer.hpp"

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_binding {

// Preserve the shared canonical source spelling; generated code parses immutable boundaries at native width.
inline std::string js_revision_literal(const member& revision, std::string_view literal) {
  const auto& type = revision.type_name_list.front();
  (void)schema_version::parse(type.name, literal);
  auto lexical = schema_version::trim(literal);
  if (type.name.starts_with("version")) { lexical = lexical.substr(1U, lexical.size() - 2U); }
  return "srlXmlModelRevision(" + native::quote(lexical) + ',' + native::quote(type.name) + ')';
}

// Rebase the revision discriminator's own lexical callbacks through composed private/public native bases.
inline js_variant_callbacks js_revision_access(const class_node& owner, const member& revision,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier) {
  for (const auto& field : owner.member_list) {
    if (&field == &revision) {
      const auto selected = field_name(revision);
      return {"owner=>owner." + selected, "(owner,value)=>{owner." + selected + "=value;}"};
    }
  }
  for (std::size_t index{}; index < owner.parents.size(); ++index) {
    const auto& parent = owner.parents[index];
    if (!js_declaring_record(*parent.parent_class, revision)) { continue; }
    member naming{};
    naming.name = "base" + std::to_string(index);
    naming.access = parent.access;
    type_name parent_type{std::string{parent.parent_class->name}, nullptr};
    parent_type.type = object_type::class_type;
    parent_type.resolved_node = parent.parent_class;
    const auto descriptor = type_identifier(parent_type) + "._srlXmlToken.model.revision";
    const auto base = "owner." + field_name(naming);
    return {"owner=>" + descriptor + ".get(" + base + ')',
        "(owner,value)=>{" + descriptor + ".set(" + base + ",value);}"};
  }
  throw std::logic_error{"XML revision has no composed native base path"};
}

// Emit model revision and immutable magic without changing native storage or computing any digest.
inline std::string js_lifecycle_metadata(const class_node& owner, const record& contract,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier) {
  std::string result{"Object.freeze({revision:"};
  if (const auto* revision = contract.model.revision) {
    const auto access = js_revision_access(owner, *revision, field_name, type_identifier);
    result += "Object.freeze({kind:" + native::quote(revision->type_name_list.front().name) +
        ",minimum:" + js_revision_literal(*revision, schema_version::minimum(*revision)) +
        ",current:" + js_revision_literal(*revision, revision->default_value) +
        ",get:" + access.get + ",set:" + access.set + "})";
  } else { result += "null"; }
  return result + ",magic:" + (contract.model.magic ? native::quote(*contract.model.magic) : "null") + "})";
}

} // namespace rohit::serializer::writer::xml_binding
