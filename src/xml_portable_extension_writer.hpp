// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/xml_extensions.hpp>
#include "xml_schema.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {

// Count emitted owner-level elements without counting collection items beneath a dedicated wrapper.
inline std::string portable_extension_field_count(const xml_binding::field& field,
    const std::string& access, std::string_view language) {
  const bool java = language == "java", go = language == "go";
  const bool optional = field.source->type_name_list.front().is_optional();
  if (optional && (!field.presence.explicit_policy || field.presence.allows_absent)) {
    return go ? "func()int{if " + access + "!=nil{return 1};return 0}()" :
        '(' + access + (java ? ".isPresent()?1:0)" : ".HasValue?1:0)");
  }
  if (field.repeated && !field.wrapper && !field.map_binding && !field.variant_binding) {
    return go ? "len(" + access + ')' : access + (java ? field.source->modifier == member::modifier_type::array ? ".length" : ".size()" : ".Count");
  }
  return "1";
}

// Derive independent portable extension defaults from the common semantic admission policy.
inline std::string portable_extension_constants(std::string_view language) {
  const xml::extension_limits limits{};
  const bool java = language == "java", go = language == "go";
  const std::string start = go ? "const srlXmlExtension" : java ? "private static final int srlXmlExtension" : "private const int SrlXmlExtension";
  const std::string separator = go ? " = " : "=";
  const std::string end = go ? "\n" : ";\n";
  return start + "MaxSlots" + separator + std::to_string(limits.max_slots) + end +
      start + "MaxItems" + separator + std::to_string(limits.max_items) + end +
      start + "MaxPreservedBytes" + separator + std::to_string(limits.max_preserved_bytes) + end +
      start + "Reject" + separator + "0" + end + start + "Preserve" + separator + "2" + end;
}

// Emit immutable compiler-owned slot metadata once; runtime contracts prepare their lookup indexes once.
inline std::string portable_extension_metadata(const xml::extension_contract& contract,
    const std::function<std::string(std::string_view)>& quote,
    const std::function<std::string(const xml::name&)>& name, std::string_view language) {
  const bool go = language == "go", java = language == "java";
  const std::string strings = java ? "new String[]{" : "new string[]{";
  std::string result = go ? "srlXmlExtensionContract{core:" : "new SrlXmlExtensionContract(";
  result += quote(contract.core_namespace) + (go ? ",known:map[srlxml.Name]bool{" : ',' + strings);
  for (const auto& value : contract.known_names) { result += name(value) + (go ? ":true," : ","); }
  result += go ? "},required:[]string{" : "}," + strings;
  for (const auto& value : contract.required_namespaces) { result += quote(value) + ','; }
  result += go ? "},slots:[]srlXmlExtensionSlot{" : "},new SrlXmlExtensionSlot[]{";
  for (const auto& slot : contract.slots) {
    const auto policy = slot.policy == xml::unknown_policy::preserve ? 2 : slot.policy == xml::unknown_policy::skip ? 1 : 0;
    result += go ? "{id:" : "new SrlXmlExtensionSlot(";
    result += quote(slot.id) + (go ? ",attribute:" : ",") + (slot.attribute ? "true" : "false") +
        (go ? ",position:" : ",") + std::to_string(slot.before_field) +
        (go ? ",namespaces:map[string]bool{" : ',' + strings);
    for (const auto& uri : slot.namespaces) { result += quote(uri) + (go ? ":true," : ","); }
    result += (go ? "},policy:" : "},") + std::to_string(policy) + (go ? ",minimum:" : ",") +
        std::to_string(slot.minimum) + (go ? ",maximum:" : ",") +
        (slot.maximum ? std::to_string(*slot.maximum) : "-1") + (go ? "}," : "),");
  }
  return result + (go ? "}}.prepared()" : "})");
}

} // namespace rohit::serializer::writer::xml_codegen
