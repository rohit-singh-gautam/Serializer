// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"

#include <rohit/xml_identity.hpp>

#include <string>

namespace rohit::serializer::writer::xml_binding {

// Emit one compiled namespace-exact identity path; runtime validation never interprets XPath text.
inline std::string js_identity_path_metadata(const xml::identity_path& path) {
  const auto name = [](const xml::name& value) {
    return "srlXmlTokenName(" + native::quote(value.namespace_uri) + ',' + native::quote(value.local) + ')';
  };
  std::string result = "Object.freeze({steps:Object.freeze([";
  for (const auto& step : path.steps) { result += name(step) + ','; }
  return result + "]),descendants:" + (path.descendants ? "true" : "false") +
      ",attribute:" + (path.attribute ? name(*path.attribute) : "null") + "})";
}

// Preserve checked owner-local keys, uniqueness and forward references as immutable generated metadata.
inline std::string js_identity_metadata(const xml::identity_scope& scope) {
  const auto name = [](const xml::name& value) {
    return "srlXmlTokenName(" + native::quote(value.namespace_uri) + ',' + native::quote(value.local) + ')';
  };
  std::string result = "Object.freeze({owner:" + name(scope.owner) + ",constraints:Object.freeze([";
  for (const auto& constraint : scope.constraints) {
    const auto kind = constraint.kind == xml::identity_kind::key ? "key" :
        constraint.kind == xml::identity_kind::unique ? "unique" : "keyref";
    result += "Object.freeze({id:" + name(constraint.id) + ",kind:" + native::quote(kind) +
        ",selector:" + js_identity_path_metadata(constraint.selector) + ",refer:" +
        (constraint.refer ? name(*constraint.refer) : "null") + ",fields:Object.freeze([";
    for (const auto& field : constraint.fields) {
      result += "Object.freeze({path:" + js_identity_path_metadata(field.path) +
          ",kind:" + native::quote(field.lexical_kind) + ",nillable:" + (field.nillable ? "true" : "false") + "}),";
    }
    result += "])}),";
  }
  return result + "])})";
}

} // namespace rohit::serializer::writer::xml_binding
