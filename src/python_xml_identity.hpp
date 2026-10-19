// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"

#include <rohit/xml_identity.hpp>

#include <string>

namespace rohit::serializer::writer::python_xml {

// Emit immutable expanded-name paths; no XPath evaluation or namespace lookup occurs at runtime.
inline std::string identity_path_metadata(const xml::identity_path& path) {
  const auto name = [](const xml::name& value) {
    return "(" + native::quote(value.namespace_uri) + "," + native::quote(value.local) + ")";
  };
  std::string result = "{'steps': (";
  for (const auto& step : path.steps) {
    result += name(step) + ",";
  }
  return result + "), 'descendants': " + (path.descendants ? "True" : "False") +
      ", 'attribute': " + (path.attribute ? name(*path.attribute) : "None") + "}";
}

// Lower the already checked owner-scope identity contract to literal Python metadata.
inline std::string identity(const xml::identity_scope& scope) {
  const auto name = [](const xml::name& value) {
    return "(" + native::quote(value.namespace_uri) + "," + native::quote(value.local) + ")";
  };
  std::string result = "{'owner': " + name(scope.owner) + ", 'constraints': (";
  for (const auto& constraint : scope.constraints) {
    const auto kind = constraint.kind == xml::identity_kind::key ? "key" :
        constraint.kind == xml::identity_kind::unique ? "unique" : "keyref";
    result += "{'id': " + name(constraint.id) + ", 'kind': " + native::quote(kind) +
        ", 'selector': " + identity_path_metadata(constraint.selector) + ", 'refer': " +
        (constraint.refer ? name(*constraint.refer) : "None") + ", 'fields': (";
    for (const auto& field : constraint.fields) {
      result += "{'path': " + identity_path_metadata(field.path) +
          ", 'kind': " + native::quote(field.lexical_kind) + ", 'nillable': " + (field.nillable ? "True" : "False") + "},";
    }
    result += ")},";
  }
  return result + ")}";
}

} // namespace rohit::serializer::writer::python_xml
