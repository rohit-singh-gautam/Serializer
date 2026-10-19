// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/xml_identity.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace rohit::serializer::writer::xml_codegen {

// Derive runtime policy defaults from the same owner-local identity contract as native validation.
inline std::string mobile_identity_constants(bool swift) {
  const xml::identity_limits limits{};
  const std::string declaration = swift ? "fileprivate let " : "internal const val ";
  std::string result;
  for (const auto& [name, count] : {
      std::pair{"Constraints", limits.max_constraints}, {"Fields", limits.max_fields},
      {"Depth", limits.max_depth}, {"SelectedNodes", limits.max_selected_nodes},
      {"Tuples", limits.max_tuples}, {"TupleBytes", limits.max_tuple_bytes},
      {"WorkUnits", limits.max_work_units}}) {
    result += declaration + "srlXmlIdentityMax" + name + " = " + std::to_string(count) + '\n';
  }
  return result;
}

// Emit prepared expanded paths and local target indexes; no XPath strings are interpreted at runtime.
inline std::string mobile_identity_metadata(const xml::identity_scope& scope,
    const std::function<std::string(std::string_view)>& quote,
    const std::function<std::string(const xml::name&)>& name, bool swift) {
  const std::string start = swift ? "[" : "arrayOf(", end = swift ? "]" : ")";
  const auto path = [&](const xml::identity_path& value) {
    std::string result = "SrlXmlIdentityPath(" + start;
    for (const auto& step : value.steps) { result += name(step) + ','; }
    return result + end + ',' + (value.descendants ? "true" : "false") + ',' +
        (value.attribute ? name(*value.attribute) : swift ? "nil" : "null") + ')';
  };
  std::string result = "SrlXmlIdentityScope(" + start;
  for (const auto& constraint : scope.constraints) {
    const auto kind = constraint.kind == xml::identity_kind::key ? "key" :
        constraint.kind == xml::identity_kind::unique ? "unique" : "keyref";
    result += "SrlXmlIdentityConstraint(" + quote(kind) + ',' + path(constraint.selector) + ',' + start;
    for (const auto& field : constraint.fields) {
      result += "SrlXmlIdentityField(" + path(field.path) + ',' + quote(field.lexical_kind) + ',' +
          (field.nillable ? "true" : "false") + "),";
    }
    result += end + ',';
    if (constraint.refer) {
      const auto target = std::find_if(scope.constraints.begin(), scope.constraints.end(),
          [&](const auto& candidate) { return candidate.id == *constraint.refer; });
      if (target == scope.constraints.end()) { throw std::invalid_argument{"XML keyref target is outside its owner scope"}; }
      result += std::to_string(std::distance(scope.constraints.begin(), target));
    } else { result += "-1"; }
    result += "),";
  }
  return result + end + ')';
}

} // namespace rohit::serializer::writer::xml_codegen
