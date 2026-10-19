// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_codegen {
// Emit one allocation-free native key length check without losing exact scalar widths.
inline std::string c_map_key_size(const xml_binding::map_contract& map, const std::string& helper,
    const std::function<std::string(const type_name&)>& type) {
  const auto& key = map.key_type;
  std::string result = "/* Measure the exact canonical XML key before output or entry admission. */\nstatic inline bool " +
      helper + "(const " + type(key) + "* value, size_t* bytes) {\n";
  if (key.name == "string") {
    result += "if (value->size && !value->data) { return false; } *bytes=value->size; return true;\n";
  } else if (key.type == object_type::enum_type) {
    const auto size = static_cast<const enum_node&>(*key.resolved_node).enum_name_list.size();
    result += "if ((unsigned)*value >= " + std::to_string(size) + "u) { return false; } *bytes=strlen(" + type(key) + "_srl_names[(unsigned)*value]); return true;\n";
  } else if (key.name == "bool") {
    result += "*bytes=*value ? sizeof(\"true\")-1u : sizeof(\"false\")-1u; return true;\n";
  } else {
    result += "char number[srl_number_scratch_bytes]; const int written=snprintf(number,sizeof(number)," +
        std::string{key.name.starts_with('u') ? "\"%\" PRIu64,(uint64_t)" : "\"%\" PRId64,(int64_t)"} +
        "*value); if (written < 0 || (size_t)written >= sizeof(number)) { return false; } *bytes=(size_t)written; return true;\n";
  }
  return result + "}\n";
}

// Generate a direct map operation while callers supply the existing typed scalar/record callbacks.
inline std::string c_map_method(const xml_binding::field& field, const std::string& member,
    const std::string& storage, const std::string& entry_helper, const std::string& key_helper, bool input,
    const std::function<std::string(const type_name&, const std::string&)>& initialize,
    const std::function<std::string(const type_name&, const xml::name&, const std::string&)>& read,
    const std::function<std::string(const type_name&, const std::string&, const std::string&)>& read_leaf) {
  const auto& map = *field.map_binding;
  std::string result;
  const auto line = [&](const std::string& value) { result += value + "\n"; };
  const auto name = [](const xml::name& value) {
    return native::quote(value.local) + "," + native::quote(value.namespace_uri);
  };
  const auto has = [&](const xml::name& value) { return "srl_xml_input_has(input," + name(value) + ")"; };
  const auto key_equal = [&](const std::string& left, const std::string& right) {
    return map.key_type.name == "string" ? "srl_string_compare(&" + left + ",&" + right + ")==0" : left + "==" + right;
  };
  auto payload = member;
  line("{");
  if (map.optional_container) {
    if (input) {
      line("if (" + member + ".has_value) { " + storage + "_clear(&" + member + ".value); } memset(&" + member + ",0,sizeof(" + member + "));");
    }
    line(input ? "if (" + has(map.wrapper) + ") { " + member + ".has_value=true;" : "if (" + member + ".has_value) {");
    payload += ".value";
  }
  if (input) {
    line(storage + "_clear(&" + payload + ");");
    line("if (!" + has(map.wrapper) + " || input->member_attributes || !srl_xml_input_next(input)) { return false; }");
    line("size_t entries=0,key_bytes=0;");
    line("while (" + has(map.entry) + ") {");
    line("if (entries >= input->map_limits.max_entries) { input->status=srl_limit; return false; }");
    line("if (" + payload + ".size == " + payload + ".capacity) {");
    line("const size_t initial_capacity=4u; size_t capacity=" + payload + ".capacity ? " + payload + ".capacity : initial_capacity;");
    line("if (capacity > SIZE_MAX/2u) { return false; } capacity*=2u; if (capacity > input->map_limits.max_entries) { capacity=input->map_limits.max_entries; }");
    line("if (capacity <= " + payload + ".size || capacity > SIZE_MAX/sizeof(" + payload + ".data[0])) { return false; }");
    line("void* memory=realloc(" + payload + ".data,capacity*sizeof(" + payload + ".data[0])); if (!memory) { input->status=srl_allocation; return false; }");
    line(payload + ".data=(" + storage + "_entry*)memory; " + payload + ".capacity=capacity; }");
    line("const size_t slot=" + payload + ".size++; " + storage + "_entry* entry=&" + payload + ".data[slot]; memset(entry,0,sizeof(*entry));");
    line(initialize(map.value_type, "entry->value"));
    line("size_t lexical_bytes=0,canonical_bytes=0;");
    if (map.key_placement == xml_binding::map_key_placement::attribute) {
      line("if (input->member_attributes != 1u) { return false; }");
      line("{");
      line("xmlChar* text=xmlTextReaderGetAttributeNs(input->reader,BAD_CAST " + native::quote(map.key.local) + "," +
          (map.key.namespace_uri.empty() ? "NULL" : "BAD_CAST " + native::quote(map.key.namespace_uri)) + "); if (!text) { return false; } lexical_bytes=(size_t)xmlStrlen(text);");
      line(read_leaf(map.key_type, "entry->key", "text"));
      line("xmlFree(text); } if (!srl_xml_input_next(input)) { return false; }");
      line("{"); line(read(map.value_type, map.value, "entry->value")); line("}");
    } else {
      line("if (input->member_attributes || !srl_xml_input_next(input)) { return false; } bool key_seen=false,value_seen=false;");
      line("for (;;) { if (" + has(map.key) + ") { if (key_seen) { return false; } key_seen=true;");
      line("xmlChar* text=srl_xml_input_scalar(input," + name(map.key) + "); if (!text) { return false; } lexical_bytes=(size_t)xmlStrlen(text);");
      line(read_leaf(map.key_type, "entry->key", "text")); line("xmlFree(text);");
      line("} else if (" + has(map.value) + ") { if (value_seen) { return false; } value_seen=true;");
      line(read(map.value_type, map.value, "entry->value"));
      line("} else { break; } } if (!key_seen || !value_seen) { return false; }");
    }
    line("if (!srl_xml_input_close(input," + name(map.entry) + ") || !" + key_helper + "(&entry->key,&canonical_bytes)) { return false; }");
    line("if (!srl_xml_map_admit(&input->map_limits,&entries,&key_bytes,lexical_bytes,canonical_bytes)) { input->status=srl_limit; return false; } }");
    line("if (!srl_xml_input_close(input," + name(map.wrapper) + ")) { return false; }");
    line("if (" + payload + ".size > 1u) { qsort(" + payload + ".data," + payload + ".size,sizeof(" + payload + ".data[0])," + storage + "_compare); }");
    line("for (size_t index=1u; index < " + payload + ".size; ++index) { if (" + key_equal(payload + ".data[index-1u].key", payload + ".data[index].key") + ") { return false; } }");
  } else {
    line("if ((" + payload + ".size && !" + payload + ".data) || " + payload + ".size > out->map_limits.max_entries || " + payload + ".size > SIZE_MAX/sizeof(const " + storage + "_entry*)) { return false; }");
    line("size_t entries=0,key_bytes=0;");
    line("for (size_t index=0; index < " + payload + ".size; ++index) { size_t bytes=0; if (!" + key_helper + "(&" + payload + ".data[index].key,&bytes) || !srl_xml_map_admit(&out->map_limits,&entries,&key_bytes,bytes,bytes)) { return false; } }");
    line("if (++out->nodes > out->limits.max_elements || ++out->writer.depth > out->limits.max_depth || xmlTextWriterStartElementNS(writer," +
        std::string{map.wrapper.namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ",BAD_CAST " + native::quote(map.wrapper.local) + "," +
        (map.wrapper.namespace_uri.empty() ? "NULL" : "BAD_CAST " + native::quote(map.wrapper.namespace_uri)) + ") < 0) { return false; }");
    line("const " + storage + "_entry** sorted=" + payload + ".size ? (const " + storage + "_entry**)malloc(" + payload + ".size*sizeof(*sorted)) : NULL;");
    line("if (" + payload + ".size && !sorted) { return false; } for (size_t index=0; index < " + payload + ".size; ++index) { sorted[index]=&" + payload + ".data[index]; }");
    line("if (" + payload + ".size > 1u) { qsort(sorted," + payload + ".size,sizeof(*sorted)," + storage + "_compare_pointer); }");
    line("bool good=true; for (size_t index=0; good && index < " + payload + ".size; ++index) {");
    line("if (index && (" + key_equal("sorted[index-1u]->key", "sorted[index]->key") + ")) { good=false; break; }");
    line("good=" + entry_helper + "(writer,out,sorted[index]); } free(sorted); if (!good) { return false; }");
    line("--out->writer.depth; if (xmlTextWriterEndElement(writer) < 0) { return false; }");
  }
  if (map.optional_container) { line("}"); }
  line("}");
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
