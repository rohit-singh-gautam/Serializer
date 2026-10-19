// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_js_projection_types.hpp"
#include "xml_schema.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_binding {

// Lower one known scalar/record payload to immutable native callbacks with exact expanded names.
inline std::string js_container_payload(const type_name& original, const xml::name& name,
    bool attribute, const std::function<std::string(const type_name&)>& type_identifier) {
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const auto& type = js_projected_leaf(original);
  const bool projection = js_value_projection(original);
  const bool record = type.type == object_type::class_type;
  std::string read = "text=>srlXmlScalar(text," + quote(type.name) + ')';
  std::string write = "value=>srlXmlScalar(value," + quote(type.name) + ",true)";
  if (type.is_digest()) {
    read = "text=>srlXmlDigest(text," + std::to_string(type.digest_extent) + ')';
    write = "value=>srlXmlDigest(value," + std::to_string(type.digest_extent) + ",true)";
  }
  if (type.type == object_type::enum_type) {
    const auto names = type_identifier(type) + "Names";
    read = "text=>{const value=" + names + ".indexOf(text);if(value<0)throw new Error('Unknown XML enum');return value;}";
    write = "value=>{if(!Number.isInteger(value)||value<0||value>=" + names +
        ".length)throw new Error('Unknown XML enum');return " + names + "[value];}";
  }
  if (projection && !record) { write = "value=>(" + write + ")(value.readValue())"; }
  return "Object.freeze({name:srlXmlTokenName(" + quote(name.namespace_uri) + ',' + quote(name.local) +
      "),local:" + quote(name.local) + ",uri:" + quote(name.namespace_uri) +
      ",mode:" + quote(attribute ? "attribute" : "child") +
      ",wrapper:null,presence:null,minimum:1,maximum:1,extent:0,optional:false,repeated:false,projection:" +
      (projection ? "true" : "false") + ",model:" +
      (record ? "()=>" + type_identifier(type) + "._srlXmlToken" : "null") +
      ",read:" + (record ? "null" : read) + ",write:" + (record ? "null" : write) + "})";
}

// Retain shared map names and native key/value types without a target-specific XML interpretation.
inline std::string js_map_metadata(const map_contract& map,
    const std::function<std::string(const type_name&)>& type_identifier) {
  return "Object.freeze({entry:srlXmlTokenName(" + native::quote(map.entry.namespace_uri) + ',' +
      native::quote(map.entry.local) + "),entryLocal:" + native::quote(map.entry.local) +
      ",entryUri:" + native::quote(map.entry.namespace_uri) + ",key:" +
      js_container_payload(map.key_type, map.key, map.key_placement == map_key_placement::attribute,
          type_identifier) + ",value:" + js_container_payload(map.value_type, map.value, false, type_identifier) + "})";
}

} // namespace rohit::serializer::writer::xml_binding
