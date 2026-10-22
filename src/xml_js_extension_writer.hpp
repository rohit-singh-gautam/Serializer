// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"

#include <rohit/xml_extensions.hpp>

#include <cstddef>
#include <limits>
#include <string>

namespace rohit::serializer::writer::xml_binding {

// Emit precise positioned URI permissions separately from application understanding of those vocabularies.
inline std::string js_extension_metadata(const xml::extension_contract& contract, std::size_t field_count,
                                         std::string_view declarations = "null") {
  std::string result = "Object.freeze({core:" + native::quote(contract.core_namespace) + ",fieldCount:" +
      std::to_string(field_count) + ",known:Object.freeze([";
  for (const auto& name : contract.known_names) {
    result += "srlXmlTokenName(" + native::quote(name.namespace_uri) + ',' + native::quote(name.local) + "),";
  }
  result += "]),knownAttributes:Object.freeze([";
  for (const auto& name : contract.distinct_known_kinds ? contract.known_attributes : contract.known_names) {
    result += "srlXmlTokenName(" + native::quote(name.namespace_uri) + ',' + native::quote(name.local) + "),";
  }
  result += "]),required:Object.freeze([";
  for (const auto& uri : contract.required_namespaces) { result += native::quote(uri) + ','; }
  result += "]),slots:Object.freeze([";
  for (const auto& slot : contract.slots) {
    result += "Object.freeze({id:" + native::quote(slot.id) + ",attribute:" + (slot.attribute ? "true" : "false") +
        ",position:" + (slot.before_field == std::numeric_limits<std::size_t>::max() ? "-1" : std::to_string(slot.before_field)) + ",policy:" + native::quote(
            slot.policy == xml::unknown_policy::preserve ? "preserve" :
            slot.policy == xml::unknown_policy::skip ? "skip" : "reject") +
        ",minimum:" + std::to_string(slot.minimum) + ",maximum:" + (slot.maximum ? std::to_string(*slot.maximum) : "null") +
        ",match:" + std::to_string(static_cast<int>(slot.namespace_match)) + ",process:" +
        std::to_string(static_cast<int>(slot.process_contents)) + ",namespaces:Object.freeze([";
    for (const auto& uri : slot.namespaces) { result += native::quote(uri) + ','; }
    result += "])}),";
  }
  result += "]),knownPositions:Object.freeze([";
  for (const auto position : contract.known_positions) {
    result += (position == std::numeric_limits<std::size_t>::max() ? "-1" : std::to_string(position)) + ',';
  }
  return result + "]),declarations:" + std::string{declarations} + "})";
}

} // namespace rohit::serializer::writer::xml_binding
