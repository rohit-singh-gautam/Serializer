// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/xml_extensions.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {

// Derive extension admission defaults from the same shared contract as native XML validation.
inline std::string mobile_extension_constants(bool swift) {
  const xml::extension_limits limits{};
  const std::string declaration = swift ? "fileprivate let " : "internal const val ";
  return declaration + "srlXmlExtensionMaxSlots = " + std::to_string(limits.max_slots) + '\n' +
      declaration + "srlXmlExtensionMaxItems = " + std::to_string(limits.max_items) + '\n' +
      declaration + "srlXmlExtensionMaxPreservedBytes = " + std::to_string(limits.max_preserved_bytes) + '\n';
}

// Emit immutable expanded-name permissions without interpreting any opaque vocabulary's lexical values.
inline std::string mobile_extension_metadata(const xml::extension_contract& contract,
    const std::function<std::string(std::string_view)>& quote,
    const std::function<std::string(const xml::name&)>& name, bool swift) {
  const std::string start = swift ? "[" : "arrayOf(", end = swift ? "]" : ")";
  std::string result = "SrlXmlExtensionContract(" + quote(contract.core_namespace) + ',' + start;
  for (const auto& value : contract.known_names) { result += name(value) + ','; }
  result += end + ',' + start;
  for (const auto& value : contract.required_namespaces) { result += quote(value) + ','; }
  result += end + ',' + start;
  for (const auto& slot : contract.slots) {
    result += "SrlXmlExtensionSlot(" + quote(slot.id) + ',' + (slot.attribute ? "true" : "false") + ',' +
        std::to_string(slot.before_field) + ',' + start;
    for (const auto& uri : slot.namespaces) { result += quote(uri) + ','; }
    const auto policy = slot.policy == xml::unknown_policy::preserve ? 2 : slot.policy == xml::unknown_policy::skip ? 1 : 0;
    result += end + ',' + std::to_string(policy) + ',' + std::to_string(slot.minimum) + ',' +
        (slot.maximum ? std::to_string(*slot.maximum) : swift ? "nil" : "null") + "),";
  }
  return result + end + ')';
}

} // namespace rohit::serializer::writer::xml_codegen
