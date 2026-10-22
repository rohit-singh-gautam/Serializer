// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"
#include "xml_ordered_writer.hpp"

#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_binding {
// Borrow private native fragments on output or build a fresh ordered owning sequence on input.
inline std::string rust_mixed_method(const mixed_contract& mixed, const std::string& member,
    const std::string& fragment_type, bool input,
    const std::function<std::string(const type_name&)>& type_identifier) {
  std::string result;
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const auto variant = native::snake(mixed.variant->name);
  const auto text = variant + '_' + native::snake(mixed.text_branch);
  line("{");
  line(xml_codegen::ordered_start(mixed, "rust"));
  if (input) {
    line("let mut items=Vec::new();");
    if (!mixed.element_only) { line("let mut text_bytes=0usize;"); }
    line("loop {");
    if (mixed.element_only) { line("input.skip_space()?;"); }
    line("if matches!(input.current,SrlXmlToken::End(_)) { break; }");
    line("if items.len() >= input.mixed_limits.max_fragments { return Err(input.quota(\"XML mixed fragment limit\")); }");
    if (mixed.source->fixed_extent) {
      line("if items.len() >= " + std::to_string(mixed.source->fixed_extent) + " { return Err(Error(\"XML mixed fixed extent mismatch\")); }");
    }
    line("input.allocate(std::mem::size_of::<" + fragment_type + ">())?; items.try_reserve(1).map_err(|_| input.quota(\"XML mixed fragment allocation\"))?;");
    line("let mut fragment=" + fragment_type + "::default();");
    if (!mixed.element_only) {
      line("if matches!(input.current,SrlXmlToken::Text(_)) {");
      line("let value=input.mixed_text(input.mixed_limits.max_text_bytes.saturating_sub(text_bytes))?; if value.is_empty() { continue; }");
      line("text_bytes+=value.len(); fragment." + variant + "_index=" + std::to_string(mixed.text_alternative) + "; fragment." + text + "=value;");
    }
    for (const auto& child : mixed.children) {
      line(std::string{mixed.element_only && &child == mixed.children.data() ? "if" : "} else if"} + " matches!(&input.current,SrlXmlToken::Start(name,_) if name.uri == " + quote(child.qualified_name.namespace_uri) + " && name.local == " + quote(child.qualified_name.local) + ") {");
      line(xml_codegen::ordered_step(mixed, std::to_string(child.alternative), "rust", "return Err(Error(\"Ordered XSD content mismatch\"));"));
      line("fragment." + variant + "_index=" + std::to_string(child.alternative) + "; fragment." + variant + '_' + native::snake(child.branch) + '=' +
          type_identifier(*child.value_type) + "::srl_xml_read_tokens(input," + quote(child.qualified_name.namespace_uri) + ',' + quote(child.qualified_name.local) + ")?;");
    }
    line("} else { return Err(Error(\"Unknown XML mixed content\")); } items.push(fragment); }");
    if (mixed.source->fixed_extent) {
      line("if items.len() != " + std::to_string(mixed.source->fixed_extent) + " { return Err(Error(\"XML mixed fixed extent mismatch\")); }");
    }
    line(member + "=items;");
  } else {
    line("if " + member + ".len() > output.mixed_limits.max_fragments { return Err(output.quota(\"XML mixed fragment limit\")); }");
    if (mixed.source->fixed_extent) {
      line("if " + member + ".len() != " + std::to_string(mixed.source->fixed_extent) + " { return Err(Error(\"XML mixed fixed extent mismatch\")); }");
    }
    if (!mixed.element_only) { line("let mut text_bytes=0usize;"); }
    line("for fragment in " + member + ".iter() { match fragment." + variant + "_index {");
    if (!mixed.element_only) {
      line(std::to_string(mixed.text_alternative) + "=>{ let value=&fragment." + text + "; if value.len() > output.mixed_limits.max_text_bytes.saturating_sub(text_bytes) { return Err(output.quota(\"XML mixed text byte limit\")); } text_bytes+=value.len(); output.escaped(value,false)?; },");
    }
    for (const auto& child : mixed.children) {
      line(std::to_string(child.alternative) + "=>{");
      line(xml_codegen::ordered_step(mixed, std::to_string(child.alternative), "rust", "return Err(Error(\"Ordered XSD content mismatch\"));"));
      line("fragment." + variant + '_' + native::snake(child.branch) + ".srl_xml_write(output," + quote(child.qualified_name.local) + ',' + quote(child.qualified_name.namespace_uri) + ")?; },");
    }
    line("_=>return Err(Error(\"Unknown native XML mixed discriminator\")) } }");
  }
  line(xml_codegen::ordered_finish(mixed, "rust", "return Err(Error(\"Incomplete ordered XSD content\"));"));
  line("}");
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
