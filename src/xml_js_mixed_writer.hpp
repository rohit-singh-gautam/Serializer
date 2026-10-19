// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_js_model_writer.hpp"
#include "xml_js_variant_writer.hpp"

#include <functional>
#include <initializer_list>
#include <string>

namespace rohit::serializer::writer::xml_binding {

// Emit fragment callbacks within their native lexical class so private variants keep their existing storage.
inline std::string js_mixed_fragment_methods(const member& variant, const std::string& generated_name,
    const std::function<std::string(const member&)>& field_name, bool storage_only, bool typescript) {
  if (typescript) {
    return storage_only ? "// Ordered fragment storage has XML meaning only within its owning mixed record.\n"
        "encodeXml(limits?:XmlLimits,validator?:XmlValidator):string;\n"
        "static decodeXml(text:string,limits?:XmlLimits,validator?:XmlValidator):" + generated_name + ";\n"
        "static decodeXmlChunks(chunks:Iterable<Uint8Array>,limits?:XmlLimits):" + generated_name + ";\n" : "";
  }
  const auto access = js_variant_access(variant, field_name);
  std::string result = "// Private native fragment storage is accessed only through these lexical callbacks.\n"
      "static _srlXmlMixed=Object.freeze({create:()=>new " + generated_name + "(),get:" + access.get + ",set:" + access.set + "});\n";
  if (storage_only) {
    for (const auto* method : {"encodeXml", "static decodeXml", "static decodeXmlChunks", "static _srlXmlRead", "_srlXmlWrite"}) {
      result += std::string{method} + "() {throw new Error('Mixed fragment storage has no standalone XML contract');}\n";
    }
  }
  return result;
}

// Retain ordered child identities separately from fragment variant aliases and native array member names.
inline std::string js_mixed_metadata(const class_node& owner, const mixed_contract& mixed,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier) {
  const auto inherited = js_inherited_access(owner, *mixed.source, field_name, type_identifier);
  const auto access = inherited ? *inherited : js_variant_callbacks{
      "owner=>owner." + field_name(*mixed.source), "(owner,value)=>{owner." + field_name(*mixed.source) + "=value;}"};
  const auto& source_type = mixed.source->type_name_list.front();
  const auto& fragment_type = source_type.is_sequence() ? source_type.arguments.front() : source_type;
  std::string result = "Object.freeze({get:" + access.get + ",set:" + access.set + ",fragment:()=>" +
      type_identifier(fragment_type) + "._srlXmlMixed,textIndex:" + std::to_string(mixed.text_alternative) +
      ",children:Object.freeze([";
  for (const auto& child : mixed.children) {
    result += "Object.freeze({alternative:" + std::to_string(child.alternative) + ",payload:" +
        js_container_payload(*child.value_type, child.qualified_name, false, type_identifier) + "}),";
  }
  return result + "])})";
}

} // namespace rohit::serializer::writer::xml_binding
