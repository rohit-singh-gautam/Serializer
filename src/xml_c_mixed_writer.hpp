// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_codegen {
// Preserve ordered native text/child fragments while all failure state remains candidate-owned.
inline std::string c_mixed_method(const xml_binding::mixed_contract& mixed, const std::string& member,
    const std::string& collection_type, const std::string& fragment_type, const std::string& variant,
    bool input, const std::function<std::string(const type_name&, const xml::name&, const std::string&)>& read,
    const std::function<std::string(const type_name&, const xml::name&, const std::string&)>& write,
    const std::string& diagnostic = {}) {
  std::string result;
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const auto text = "fragment->" + variant + '_' + native::snake(mixed.text_branch);
  line("{");
  if (input) {
    line(collection_type + "_clear(&" + member + "); size_t text_bytes=0;");
    line("while (input->kind != XML_READER_TYPE_END_ELEMENT) {");
    line("srl_xml_operation_select(srl_xml_operation_input_context(input->reader)," + quote(diagnostic) + ");");
    line("if (input->status != srl_ok) { return false; }");
    line("if (" + member + ".size >= input->mixed_limits.max_fragments" +
        (mixed.source->fixed_extent ? " || " + member + ".size >= " + std::to_string(mixed.source->fixed_extent) + "u" : "") + ") { input->status=srl_xml_operation_fail(srl_xml_operation_input_context(input->reader),srl_limit); return false; }");
    line("if (" + member + ".size == " + member + ".capacity) {");
    line("const size_t initial_capacity=4u; size_t capacity=" + member + ".capacity ? " + member + ".capacity : initial_capacity;");
    line("if (capacity > SIZE_MAX/2u) { return false; } capacity*=2u; if (capacity > input->mixed_limits.max_fragments) { capacity=input->mixed_limits.max_fragments; }");
    line("if (capacity <= " + member + ".size || capacity > SIZE_MAX/sizeof(" + member + ".data[0])) { return false; }");
    line("if(!srl_xml_operation_input_allocate(input,(capacity-" + member + ".capacity)*sizeof(" + member + ".data[0]))){return false;}");
    line("void* memory=realloc(" + member + ".data,capacity*sizeof(" + member + ".data[0])); if (!memory) { input->status=srl_allocation; return false; }");
    line(member + ".data=(" + fragment_type + "*)memory; " + member + ".capacity=capacity; }");
    line(fragment_type + "* fragment=&" + member + ".data[" + member + ".size++]; memset(fragment,0,sizeof(*fragment)); if (" + fragment_type + "_init(fragment) != srl_ok) { return false; }");
    line("if (input->kind == XML_READER_TYPE_TEXT || input->kind == XML_READER_TYPE_CDATA || input->kind == XML_READER_TYPE_WHITESPACE || input->kind == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {");
    line("xmlChar* text=srl_xml_input_text_run(input,input->mixed_limits.max_text_bytes-text_bytes,false); if (!text) { return false; } const size_t bytes=(size_t)xmlStrlen(text);");
    line("if (!bytes) { xmlFree(text); " + fragment_type + "_free(fragment); --" + member + ".size; continue; }");
    line("srl_xml_operation_select(srl_xml_operation_input_context(input->reader)," + quote(diagnostic) + ");");
    line("fragment->" + variant + "_index=" + std::to_string(mixed.text_alternative) + "u; const srl_status copied=srl_xml_operation_string_set(srl_xml_operation_input_context(input->reader),&" + text + ",(const char*)text,bytes); xmlFree(text); if (copied != srl_ok) { input->status=copied; return false; } text_bytes+=bytes;");
    for (const auto& child : mixed.children) {
      line("} else if (srl_xml_input_name(input,XML_READER_TYPE_ELEMENT," + quote(child.qualified_name.local) + ',' + quote(child.qualified_name.namespace_uri) + ")) {");
      line("fragment->" + variant + "_index=" + std::to_string(child.alternative) + "u;");
      line(read(*child.value_type, child.qualified_name, "fragment->" + variant + '_' + native::snake(child.branch)));
    }
    line("} else { return false; } }");
    if (mixed.source->fixed_extent) {
      line("if (" + member + ".size != " + std::to_string(mixed.source->fixed_extent) + "u) { return false; }");
    }
  } else {
    line("srl_xml_operation_select(operation," + quote(diagnostic) + ");");
    line("if(" + member + ".size>out->mixed_limits.max_fragments){return srl_xml_operation_output_limit(out,operation);}");
    line("if ((" + member + ".size && !" + member + ".data) || " + member + ".size > out->mixed_limits.max_fragments" +
        (mixed.source->fixed_extent ? " || " + member + ".size != " + std::to_string(mixed.source->fixed_extent) + "u" : "") + ") { return false; }");
    line("size_t text_bytes=0; for (size_t index=0; index < " + member + ".size; ++index) { const " + fragment_type + "* fragment=&" + member + ".data[index]; switch (fragment->" + variant + "_index) {");
    line("case " + std::to_string(mixed.text_alternative) + "u: {");
    line("srl_xml_operation_select(operation," + quote(diagnostic) + ");");
    line("if(" + text + ".size>out->mixed_limits.max_text_bytes-text_bytes){return srl_xml_operation_output_limit(out,operation);}if(" + text + ".size&&!" + text + ".data){return false;}text_bytes+=" + text + ".size;");
    line("if (!srl_xml_operation_emit_text(operation,writer,out,NULL,\"\",\"\"," + text + ".data ? " + text + ".data : \"\"," + text + ".size)) { return false; } break; }");
    for (const auto& child : mixed.children) {
      line("case " + std::to_string(child.alternative) + "u: {");
      line(write(*child.value_type, child.qualified_name, "fragment->" + variant + '_' + native::snake(child.branch)));
      line("break; }");
    }
    line("default: return false; } }");
  }
  line("}");
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
