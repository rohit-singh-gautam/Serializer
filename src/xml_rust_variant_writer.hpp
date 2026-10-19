// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_binding {
// Emit an exact active native alternative without changing its discriminator or payload fields.
inline std::string rust_variant_method(const field& field, const std::string& member, bool input,
    const std::function<std::string(const type_name&)>& type_identifier,
    const std::function<std::string(const type_name&, const std::string&)>& read_scalar,
    const std::function<std::string(const type_name&, const std::string&)>& scalar) {
  const auto& variant = *field.variant_binding;
  const bool typed = variant.selection == variant_selection::xsi_type;
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const auto name = [&](const xml::name& value) { return quote(value.namespace_uri) + ',' + quote(value.local); };
  std::string result;
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  line("{");
  if (input) {
    line("input.open(" + name(variant.wrapper) + ")?.finish()?;");
    if (typed) {
      line("if !input.has(" + name(variant.wrapper) + ")? { return Err(Error(\"XML variant payload missing\")); }");
      line("let selected=input.take_type()?.ok_or(Error(\"XML variant xsi:type missing\"))?;");
    }
    for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
      const auto& alternative = variant.alternatives[index];
      const auto& payload_name = typed ? variant.wrapper : alternative.tag;
      auto condition = typed ? "selected.0 == " + quote(alternative.type_identity.namespace_uri) +
          " && selected.1 == " + quote(alternative.type_identity.local) : "input.has(" + name(payload_name) + ")?";
      line(std::string{index ? "else if " : "if "} + condition + " {");
      if (!typed) {
        line("if input.take_type()?.is_some_and(|selected| selected.0 != " + quote(alternative.type_identity.namespace_uri) +
            " || selected.1 != " + quote(alternative.type_identity.local) + ") { return Err(Error(\"Conflicting XML variant type\")); }");
      }
      const auto value = alternative.native_type.type == object_type::class_type ?
          type_identifier(alternative.native_type) + "::srl_xml_read_tokens(input," + name(payload_name) + ")?" :
          read_scalar(alternative.native_type, "input.scalar(" + name(payload_name) + ")?");
      line(member + '_' + native::snake(alternative.alias) + '=' + value + "; " + member + "_index=" + std::to_string(index) + "; }");
    }
    line("else { return Err(Error(\"Unknown XML variant alternative\")); }");
    line("input.close(" + name(variant.wrapper) + ")?;");
  } else {
    line("output.open(" + quote(variant.wrapper.local) + ',' + quote(variant.wrapper.namespace_uri) + ")?; output.put(\">\")?;");
    line("match " + member + "_index {");
    for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
      const auto& alternative = variant.alternatives[index];
      const auto& payload_name = typed ? variant.wrapper : alternative.tag;
      const auto selected = "(&" + member + '_' + native::snake(alternative.alias) + ')';
      line(std::to_string(index) + "=>{");
      if (typed) {
        line("output.pending_type=Some((" + quote(alternative.type_identity.namespace_uri) + ',' + quote(alternative.type_identity.local) + "));");
      }
      if (alternative.native_type.type == object_type::class_type) {
        line(selected + ".srl_xml_write(output," + quote(payload_name.local) + ',' + quote(payload_name.namespace_uri) + ")?;");
      } else { line("output.child(" + quote(payload_name.local) + ',' + quote(payload_name.namespace_uri) + ',' + scalar(alternative.native_type, selected) + ")?;"); }
      line("},");
    }
    line("_=>return Err(Error(\"Unknown native XML variant discriminator\")) }");
    line("output.close(" + quote(variant.wrapper.local) + ")?;");
  }
  line("}");
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
