// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_js_container_writer.hpp"

#include <cstddef>
#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_binding {

// Preserve each native variant representation behind callbacks inside the declaring class scope.
struct js_variant_callbacks {
  std::string get{}, set{};
};

// Normalize owning tagged values and legacy selectors without changing generated native storage.
inline js_variant_callbacks js_variant_access(const member& source,
    const std::function<std::string(const member&)>& field_name) {
  const auto field = field_name(source);
  js_variant_callbacks result{};
  if (source.owning_variant) {
    std::string aliases{"["};
    for (const auto& alternative : source.type_name_list) {
      aliases += native::quote(alternative.enum_name) + ',';
    }
    aliases += ']';
    result.get = "owner=>{const selected=owner." + field +
        ";if(selected===null||typeof selected!=='object')throw new Error('Invalid native XML variant');const index=" +
        aliases + ".indexOf(selected.kind);if(index<0)throw new Error('Unknown native XML variant');return [index,selected.value];}";
    result.set = "(owner,value)=>{owner." + field + "={kind:" + aliases + "[value[0]],value:value[1]};}";
    return result;
  }
  result.get = "owner=>{const index=owner." + field + "Index;switch(index){";
  result.set = "(owner,value)=>{owner." + field + "Index=value[0];switch(value[0]){";
  for (std::size_t index{}; index < source.type_name_list.size(); ++index) {
    const auto alternative = field + native::pascal(source.type_name_list[index].enum_name);
    result.get += "case " + std::to_string(index) + ":return [index,owner." + alternative + "];";
    result.set += "case " + std::to_string(index) + ":owner." + alternative + "=value[1];break;";
  }
  result.get += "default:throw new Error('Unknown native XML variant');}}";
  result.set += "default:throw new Error('Unknown native XML variant');}}";
  return result;
}

// Emit the shared complete tag/type inventory as immutable exact expanded identities.
inline std::string js_variant_metadata(const variant_contract& variant,
    const std::function<std::string(const type_name&)>& type_identifier) {
  std::string result = "Object.freeze({selection:" + native::quote(
      variant.selection == variant_selection::element ? "element" : "xsi_type") + ",alternatives:Object.freeze([";
  for (const auto& alternative : variant.alternatives) {
    const auto& type = alternative.type_identity;
    result += "Object.freeze({alias:" + native::quote(alternative.alias) +
        ",type:srlXmlTokenName(" + native::quote(type.namespace_uri) + ',' + native::quote(type.local) +
        "),typeLocal:" + native::quote(type.local) + ",typeUri:" + native::quote(type.namespace_uri) +
        ",payload:" + js_container_payload(alternative.native_type, alternative.tag, false, type_identifier) + "}),";
  }
  return result + "])})";
}

} // namespace rohit::serializer::writer::xml_binding
