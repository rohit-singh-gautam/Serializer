// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_binding {
// Generate one map's direct owning read or borrowed output with independent wrapper budgets.
inline std::string rust_map_method(const field& field, const std::string& member, bool input,
    const std::function<std::string(const type_name&)>& type_identifier,
    const std::function<std::string(const type_name&, const std::string&)>& read_scalar,
    const std::function<std::string(const type_name&, const std::string&)>& scalar) {
  const auto& map = *field.map_binding;
  std::string result;
  const auto line = [&](const std::string& value) { result += value + "\n"; };
  const auto uri = [](const xml::name& name) { return native::quote(name.namespace_uri); };
  const auto local = [](const xml::name& name) { return native::quote(name.local); };
  const auto read_value = map.value_type.type == object_type::class_type ?
      type_identifier(map.value_type) + "::srl_xml_read_tokens(input," + uri(map.value) + "," + local(map.value) + ")?" :
      read_scalar(map.value_type, "input.scalar(" + uri(map.value) + "," + local(map.value) + ")?");
  line("{");
  if (input) {
    if (map.optional_container) { line("if input.has(" + uri(map.wrapper) + "," + local(map.wrapper) + ")? {"); }
    line("input.open(" + uri(map.wrapper) + "," + local(map.wrapper) + ")?.finish()?;");
    line("let mut entries=std::collections::BTreeMap::new(); let mut budget=SrlXmlMapBudget::new_with_operation(input.map_limits,input.operation.clone());");
    line("while input.has(" + uri(map.entry) + "," + local(map.entry) + ")? {");
    line("if budget.entries >= budget.limits.max_entries { return Err(input.quota(\"XML map entry limit\")); }");
    if (map.key_placement == map_key_placement::attribute) {
      line("let mut attributes=input.open(" + uri(map.entry) + "," + local(map.entry) + ")?;");
      line("let text=attributes.attribute(" + uri(map.key) + "," + local(map.key) + ",false)?.ok_or(Error(\"XML map key missing\"))?; attributes.finish()?;");
      line("let lexical_bytes=text.len(); let key=" + read_scalar(map.key_type, "text") + ";");
      line("let value=" + read_value + ";");
    } else {
      line("input.open(" + uri(map.entry) + "," + local(map.entry) + ")?.finish()?;");
      line("let mut key=None; let mut value=None; let mut lexical_bytes=0usize;");
      line("loop { if input.has(" + uri(map.key) + "," + local(map.key) + ")? {");
      line("if key.is_some() { return Err(Error(\"Duplicate XML map key member\")); }");
      line("let text=input.scalar(" + uri(map.key) + "," + local(map.key) + ")?; lexical_bytes=text.len(); key=Some(" + read_scalar(map.key_type, "text") + ");");
      line("} else if input.has(" + uri(map.value) + "," + local(map.value) + ")? {");
      line("if value.is_some() { return Err(Error(\"Duplicate XML map value member\")); } value=Some(" + read_value + ");");
      line("} else { break; } }");
      line("let key=key.ok_or(Error(\"XML map key missing\"))?; let value=value.ok_or(Error(\"XML map value missing\"))?;");
    }
    line("input.close(" + uri(map.entry) + "," + local(map.entry) + ")?;");
    line("budget.admit(lexical_bytes,(" + scalar(map.key_type, "&key") + ").len())?;");
    line("input.allocate(std::mem::size_of::<(" + type_identifier(map.key_type) + "," + type_identifier(map.value_type) + ")>())?; if entries.insert(key,value).is_some() { return Err(Error(\"Duplicate native XML map key\")); } }");
    line("input.close(" + uri(map.wrapper) + "," + local(map.wrapper) + ")?;");
    line(member + "=" + (map.optional_container ? "Some(entries)" : "entries") + ";");
    if (map.optional_container) { line("} else { " + member + "=None; }"); }
  } else {
    if (map.optional_container) { line("if let Some(entries)=" + member + ".as_ref() {"); }
    else { line("let entries=&" + member + ";"); }
    line("let mut budget=SrlXmlMapBudget::new_with_operation(output.map_limits,output.operation.clone());");
    line("if entries.len() > budget.limits.max_entries { return Err(output.quota(\"XML map entry limit\")); }");
    line("output.open(" + local(map.wrapper) + "," + uri(map.wrapper) + ")?; output.put(\">\")?;");
    line("for (key,value) in entries {");
    line("let lexical=" + scalar(map.key_type, "key") + "; budget.admit(lexical.len(),lexical.len())?;");
    line("output.open(" + local(map.entry) + "," + uri(map.entry) + ")?;");
    if (map.key_placement == map_key_placement::attribute) {
      line("output.attribute(" + local(map.key) + "," + uri(map.key) + ",lexical,0)?; output.put(\">\")?;");
    } else {
      line("output.put(\">\")?; output.child(" + local(map.key) + "," + uri(map.key) + ",lexical)?;");
    }
    if (map.value_type.type == object_type::class_type) {
      line("value.srl_xml_write(output," + local(map.value) + "," + uri(map.value) + ")?;");
    } else { line("output.child(" + local(map.value) + "," + uri(map.value) + "," + scalar(map.value_type, "value") + ")?;"); }
    line("output.close(" + local(map.entry) + ")?; } output.close(" + local(map.wrapper) + ")?;");
    if (map.optional_container) { line("}"); }
  }
  line("}");
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
