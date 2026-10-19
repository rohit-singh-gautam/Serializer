// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_codegen {
// Generate exact discriminator selection over existing inline C payloads, without an XML tree.
inline std::string c_variant_method(const xml_binding::field& field, const std::string& member, bool input,
    const std::function<std::string(const type_name&, const xml::name&, const std::string&)>& read,
    const std::function<std::string(const type_name&, const xml::name&, const std::string&)>& write) {
  const auto& variant = *field.variant_binding;
  const bool typed = variant.selection == xml_binding::variant_selection::xsi_type;
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const auto name = [&](const xml::name& value) { return quote(value.local) + ',' + quote(value.namespace_uri); };
  std::string result;
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  const auto type_is = [&](const xml::name& value) {
    return "selected.present && xmlStrEqual(selected.uri,BAD_CAST " + quote(value.namespace_uri) + ") && xmlStrEqual(selected.local,BAD_CAST " + quote(value.local) + ')';
  };
  line("{");
  if (input) {
    line("if (!srl_xml_input_has(input," + name(variant.wrapper) + ") || input->member_attributes || !srl_xml_input_next(input)) { return false; }");
    if (typed) {
      line("if (!srl_xml_input_has(input," + name(variant.wrapper) + ")) { return false; }");
      line("srl_xml_type_identity selected; if (!srl_xml_input_type(input,&selected)) { return false; }");
    }
    for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
      const auto& alternative = variant.alternatives[index];
      const auto& payload_name = typed ? variant.wrapper : alternative.tag;
      const auto condition = typed ? type_is(alternative.type_identity) : "srl_xml_input_has(input," + name(payload_name) + ')';
      line(std::string{index ? "else if (" : "if ("} + condition + ") {");
      if (!typed) {
        line("srl_xml_type_identity selected; if (!srl_xml_input_type(input,&selected)) { return false; }");
        line("const bool conflicting=selected.present && !(" + type_is(alternative.type_identity) + "); srl_xml_type_free(&selected); if (conflicting) { return false; }");
      } else { line("srl_xml_type_free(&selected);"); }
      line(read(alternative.native_type, payload_name, member + '_' + native::snake(alternative.alias)));
      line(member + "_index=" + std::to_string(index) + "u; }");
    }
    line("else { " + std::string{typed ? "srl_xml_type_free(&selected); " : ""} + "return false; }");
    line("if (!srl_xml_input_close(input," + name(variant.wrapper) + ")) { return false; }");
  } else {
    line("if (++out->nodes > out->limits.max_elements || ++out->writer.depth > out->limits.max_depth || xmlTextWriterStartElementNS(writer," +
        std::string{variant.wrapper.namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ",BAD_CAST " + quote(variant.wrapper.local) + ',' +
        (variant.wrapper.namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(variant.wrapper.namespace_uri)) + ") < 0) { return false; }");
    line("switch (" + member + "_index) {");
    for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
      const auto& alternative = variant.alternatives[index];
      const auto& payload_name = typed ? variant.wrapper : alternative.tag;
      line("case " + std::to_string(index) + "u: {");
      if (typed) {
        line("out->pending_type_uri=" + quote(alternative.type_identity.namespace_uri) + "; out->pending_type_local=" + quote(alternative.type_identity.local) + ';');
      }
      line(write(alternative.native_type, payload_name, member + '_' + native::snake(alternative.alias)));
      line("break; }");
    }
    line("default: return false; }");
    line("--out->writer.depth; if (xmlTextWriterEndElement(writer) < 0) { return false; }");
  }
  line("}");
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
