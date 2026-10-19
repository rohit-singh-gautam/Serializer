// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

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
  if (input) {
    line("let mut items=Vec::new(); let mut text_bytes=0usize;");
    line("loop { if matches!(input.current,SrlXmlToken::End(_)) { break; }");
    line("if items.len() >= input.mixed_limits.max_fragments { return Err(Error(\"XML mixed fragment limit\")); }");
    if (mixed.source->fixed_extent) {
      line("if items.len() >= " + std::to_string(mixed.source->fixed_extent) + " { return Err(Error(\"XML mixed fixed extent mismatch\")); }");
    }
    line("items.try_reserve(1).map_err(|_| Error(\"XML mixed fragment allocation\"))?;");
    line("let mut fragment=" + fragment_type + "::default();");
    line("if matches!(input.current,SrlXmlToken::Text(_)) {");
    line("let value=input.mixed_text(input.mixed_limits.max_text_bytes.saturating_sub(text_bytes))?; if value.is_empty() { continue; }");
    line("text_bytes+=value.len(); fragment." + variant + "_index=" + std::to_string(mixed.text_alternative) + "; fragment." + text + "=value;");
    for (const auto& child : mixed.children) {
      line("} else if matches!(&input.current,SrlXmlToken::Start(name,_) if name.uri == " + quote(child.qualified_name.namespace_uri) + " && name.local == " + quote(child.qualified_name.local) + ") {");
      line("fragment." + variant + "_index=" + std::to_string(child.alternative) + "; fragment." + variant + '_' + native::snake(child.branch) + '=' +
          type_identifier(*child.value_type) + "::srl_xml_read_tokens(input," + quote(child.qualified_name.namespace_uri) + ',' + quote(child.qualified_name.local) + ")?;");
    }
    line("} else { return Err(Error(\"Unknown XML mixed content\")); } items.push(fragment); }");
    if (mixed.source->fixed_extent) {
      line("if items.len() != " + std::to_string(mixed.source->fixed_extent) + " { return Err(Error(\"XML mixed fixed extent mismatch\")); }");
    }
    line(member + "=items;");
  } else {
    line("if " + member + ".len() > output.mixed_limits.max_fragments { return Err(Error(\"XML mixed fragment limit\")); }");
    if (mixed.source->fixed_extent) {
      line("if " + member + ".len() != " + std::to_string(mixed.source->fixed_extent) + " { return Err(Error(\"XML mixed fixed extent mismatch\")); }");
    }
    line("let mut text_bytes=0usize; for fragment in " + member + ".iter() { match fragment." + variant + "_index {");
    line(std::to_string(mixed.text_alternative) + "=>{ let value=&fragment." + text + "; if value.len() > output.mixed_limits.max_text_bytes.saturating_sub(text_bytes) { return Err(Error(\"XML mixed text byte limit\")); } text_bytes+=value.len(); output.escaped(value,false)?; },");
    for (const auto& child : mixed.children) {
      line(std::to_string(child.alternative) + "=>fragment." + variant + '_' + native::snake(child.branch) + ".srl_xml_write(output," + quote(child.qualified_name.local) + ',' + quote(child.qualified_name.namespace_uri) + ")?,");
    }
    line("_=>return Err(Error(\"Unknown native XML mixed discriminator\")) } }");
  }
  line("}");
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
