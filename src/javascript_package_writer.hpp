// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "package_writer.hpp"

#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::package_codegen {
// Emit matching asynchronous JavaScript facades and TypeScript types from existing package roles.
inline std::string javascript(const package_node& package, const std::string& name,
    const std::string& helper, bool declarations,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal, const xml_options& xml = {},
    bool binary_only = false) {
  const auto configured_package = configure_package(package, xml);
  const auto parts = ordered(configured_package);
  for (const auto* part : parts) {
    if (binary_only && part->codec == "json") { throw std::invalid_argument{"JSON package pipeline disabled during generation"}; }
  }
  const auto contract = compile_package_profile(package,xml);
  std::set<std::string> fields{"extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  for (const auto* part : parts) {
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"JavaScript package field or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated package roles require dynamic names and generated relationship IDs"};
    }
  }
  std::string output;
  const auto line = [&](const std::string& text = {}) { output += text + '\n'; };
  if (!declarations) {
    line("/** Schema-bound complete OPC package facade; typed model codecs remain unchanged. */");
    line("export const " + name + " = " + helper + ".profile([");
    for (const auto* part : parts) {
      const auto metadata = std::find_if(contract.parts.begin(),contract.parts.end(),[&](const auto& value){return value.role==part->name;});
      std::string selectors;
      const auto get = [&](std::string_view field){return "value=>value."+render_field(selector_member(*part,field).name);};
      selectors += ",policy:"+literal(part->profile_policy)+",codec:"+literal(part->codec)+",binaryOnly:"+(binary_only ? "true" : "false")+",targetMode:"+literal(part->target_mode)+",keys:[";
      for(const auto& key:part->keys){selectors+="{scope:"+literal(key.scope)+",field:"+literal(key.field)+",get:"+get(key.field)+"},";}
      selectors += "],references:[";
      for(const auto& reference:part->references){selectors+="{scope:"+literal(reference.scope)+",field:"+literal(reference.field)+",target:"+literal(render_field(reference.target_role))+",get:"+get(reference.field)+"},";}
      selectors += "]";
      if(metadata->decoder){const auto& decoder=*metadata->decoder;selectors+=",expectedTypeUri:"+literal(decoder.type->namespace_uri)+",expectedTypeLocal:"+literal(decoder.type->local)+",requiresType:"+(decoder.requires_type?"true":"false")+",vocabularies:[";for(const auto& uri:decoder.vocabularies){selectors+=literal(uri)+",";}selectors+="]";}
      const auto optional = [&](const auto& value) { return value ? literal(*value) : "null"; };
      line("  {field:" + literal(render_field(part->name)) + ",role:" + literal(part->name) +
          ",model:" + (part->value_type ? render_type(*part->value_type) : "null") +
          ",singleton:" + (singleton(*part) ? "true" : "false") + ",name:" + optional(part->part_name) +
          ",mime:" + optional(part->content_type) + ",source:" +
          literal(part->source == "/" ? "/" : render_field(part->source)) +
          ",relationship:" + literal(part->relationship) + ",id:" + optional(part->relationship_id) +
          ",compression:" + literal(part->compression) + ",minimum:" + std::to_string(part->min_occurs) +
          ",maximum:" + (part->max_occurs ? std::to_string(*part->max_occurs) : "null") + selectors + "},");
    }
    line("]);");
    return output;
  }
  line("/** Schema-bound asynchronous ZIP/OPC facade. */");
  line("export declare namespace " + name + " {");
  const auto role_type = [&](const package_part_declaration& part) {
    if (singleton(part)) { return render_type(*part.value_type) + " | null"; }
    return "readonly (" + helper + (part.value_type ? ".ModelPart<" + render_type(*part.value_type) + ">" : ".BinaryPart") + ")[]";
  };
  line("  interface Values {");
  for (const auto* part : parts) { line("    " + render_field(part->name) + "?: " + role_type(*part) + ";"); }
  line("    extensions?: " + helper + ".Extensions | null;");
  line("  }");
  line("  class Input {");
  for (const auto* part : parts) { line("    " + render_field(part->name) + ": " + role_type(*part) + ";"); }
  line("    extensions: " + helper + ".Extensions | null;");
  line("    constructor(values?: Values);");
  line("  }");
  line("  class Value extends Input {}");
  line("  interface Codecs {");
  for (const auto* part : parts) {
    if (part->value_type) {
      line("    readonly " + literal(part->name) + "?: " + helper + ".Codec<" + render_type(*part->value_type) + ">;");
    }
  }
  line("  }");
  line("  interface Options extends " + helper + ".Options { codecs?: Codecs; }");
  line("  interface LoadOptions extends " + helper + ".LoadOptions<Value> { codecs?: Codecs; }");
  line("  interface SaveOptions extends " + helper + ".SaveOptions<Value> { codecs?: Codecs; }");
  line("  function encode(value: Input, options?: Options): globalThis.Promise<Uint8Array>;");
  line("  function write(sink: " + helper + ".Sink, value: Input, options?: Options): globalThis.Promise<void>;");
  line("  function load(source: " + helper + ".Source, options?: LoadOptions): globalThis.Promise<" + helper + ".Result<Value>>;");
  line("  function save(path: string, value: Input, options: SaveOptions): globalThis.Promise<void>;");
  line("}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
