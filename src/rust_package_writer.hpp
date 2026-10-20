// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "package_writer.hpp"
#include "xml_schema.hpp"
#include "native_package_contract.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::package_codegen {
// Emit native owning aggregates and statically typed role adapters, without model reflection.
inline std::string rust(const package_node& package, const std::string& name,
    const std::string& helper, const xml_options& xml,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal) {
  const auto parts = ordered(package);
  std::set<std::string> fields{"extensions", "paths", "reference_links"};
  std::set<std::pair<std::string, std::string>> dispatch;
  std::string output;
  const auto line = [&](const std::string& text) { output += text + '\n'; };
  const auto api = [&](std::string_view type) { return helper + "::" + std::string{type}; };
  const auto optional = [&](const auto& value) { return value ? "Some(" + literal(*value) + ")" : "None"; };
  for (const auto* part : parts) {
    require_native_package_graph(*part, xml, "Rust");
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Rust package member or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated Rust package roles require dynamic names and relationship IDs"};
    }
    if (part->value_type && part->codec != "xml") {
      throw std::invalid_argument{"Rust typed package roles require XML"};
    }
  }
  line("/// Complete owning package candidate; failure never publishes a partially loaded value.");
  line("#[derive(Default)] pub struct " + name + "Value {");
  for (const auto* part : parts) {
    const auto type = singleton(*part) ? "Option<" + render_type(*part->value_type) + ">" :
        "Vec<" + (part->value_type ? api("ModelPart") + "<" + render_type(*part->value_type) + ">" : api("BinaryPart")) + ">";
    line("  pub " + render_field(part->name) + ": " + type + ",");
  }
  line("  pub extensions: " + api("Extensions") + ",");
  line("  pub paths: std::collections::BTreeMap<String,String>,");
  line("  /// Retain source relationship IDs for schema-selected reference diagnostics.");
  line("  pub reference_links: std::collections::BTreeMap<String,Vec<String>>,\n}");
  line("/// Use the same owning value for authoring and transactional round trips.");
  line("pub type " + name + "Input = " + name + "Value;");
  line("/// Schema-selected package entry points; all external resources remain caller-owned.");
  line("pub struct " + name + ";\nimpl " + name + " {");
  std::size_t index{};
  for (const auto* part : parts) {
    std::string root = "None", expected_type = "None";
    if (part->value_type) {
      const auto& owner = static_cast<const class_node&>(*part->value_type->resolved_node);
      const auto binding = xml_binding::schema{xml}.bind(owner);
      root = "Some((" + literal(binding.root.namespace_uri) + "," + literal(binding.root.local) + "))";
      expected_type = "Some((" + literal("urn:serializer:native:" +
          (owner.parent_namespace ? get_full_name_for_namespace(owner.parent_namespace) : std::string{})) +
          "," + literal(owner.name) + "))";
    }
    line("  const ROLE_" + std::to_string(index++) + ": " + api("Role") + " = " + api("Role") + " {field:" + literal(part->name) +
        ",name:" + optional(part->part_name) + ",content_type:" + optional(part->content_type) + ",source:" + literal(part->source) +
        ",relationship:" + literal(part->relationship) + ",id:" + optional(part->relationship_id) + ",method:" +
        (part->compression == "deflate" ? "8" : "0") + ",minimum:" + std::to_string(part->min_occurs) +
        ",maximum:" + (part->max_occurs ? "Some(" + std::to_string(*part->max_occurs) + ")" : "None") +
        ",root:" + root + ",expected_type:" + expected_type + ",fixed:" +
        (part->profile_policy == "fixed" ? "true" : "false") + "};");
  }
  line("  /// Inspect immutable generated role/model contracts before selecting any decoder.");
  line("  pub fn profiles() -> &'static [" + api("Role") + "] { &[");
  for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
    line("    Self::ROLE_" + std::to_string(role_index) + ",");
  }
  line("  ] }");
  line("  /// Validate declared application keys/references against the complete candidate.");
  line("  fn validate_references(value:&" + name + "Value,budget:&mut " + api("Budget") + ") -> " + api("Result") + "<()> {");
  line("    let _=value; let mut index=" + api("ReferenceIndex") + "::default();");
  for (const auto* part : parts) {
    if (part->keys.empty() && part->references.empty()) { continue; }
    const auto member = render_field(part->name);
    line("    " + std::string{singleton(*part) ? "if let Some(model)=&value." : "for (occurrence,item) in value."} + member +
        (singleton(*part) ? " {" : ".iter().enumerate() {"));
    if (!singleton(*part)) { line("      let model=&item.value;"); }
    const auto path = singleton(*part) ? "value.paths.get(" + literal(part->name) + ").map(String::as_str).unwrap_or(" + literal(*part->part_name) + ")" : "&item.name";
    line("      let path=" + path + "; let link=value.reference_links.get(" + literal(part->name) +
        ").and_then(|links|links.get(" + (singleton(*part) ? "0" : "occurrence") +
        ")).map(String::as_str).unwrap_or(" + literal(part->relationship_id.value_or(part->name)) + ");");
    const auto model_identity = part->value_type->resolved_node->get_full_name();
    for (const auto& key : part->keys) {
      line("      index.key(" + literal(key.scope) + ",&model." + render_field(key.field) + ".to_string()," + literal(model_identity) + ",path," + literal(key.field) + ",link,budget)?;");
    }
    for (const auto& reference : part->references) {
      const auto target = std::find_if(package.parts.begin(), package.parts.end(), [&](const auto& role) {
        return role.name == reference.target_role;
      });
      line("      index.reference(" + literal(reference.scope) + ",&model." + render_field(reference.field) + ".to_string()," + literal(target->value_type->resolved_node->get_full_name()) + ",path," + literal(reference.field) + ",link,budget)?;");
    }
    line("    }");
  }
  line("    index.finish(budget)\n  }");
  line("  /// Assemble and validate the entire archive before exposing bytes or touching a sink.");
  line("  pub fn encode(value:&" + name + "Input,budget:&mut " + api("Budget") + ") -> " + api("Result") + "<Vec<u8>> {");
  line("    " + helper + "::validate_overrides(Self::profiles(),budget)?; Self::validate_references(value,budget)?;");
  line("    let mut composer=" + api("Composer") + "::new(budget,&value.paths);");
  index = 0;
  for (const auto* part : parts) {
    line("    composer." + std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} +
        "(&Self::ROLE_" + std::to_string(index++) + ",&value." + render_field(part->name) +
        (part->value_type ? ",|model,limits| model.encode_xml_with_validation(limits,None).map_err(" + api("native_error") + ")" : "") + ")?;");
  }
  line("    composer.finish(&value.extensions)\n  }");
  line("  /// Decode declared roles and all reference checks before returning an owning aggregate.");
  line("  pub fn load(bytes:&[u8],budget:&mut " + api("Budget") + ",policy:" + api("ExtensionPolicy") + ") -> " + api("Result") + "<" + name + "Value> {");
  line("    " + helper + "::validate_overrides(Self::profiles(),budget)?;");
  line("    let mut loader=" + api("Loader") + "::new(bytes,budget)?; let mut value=" + name + "Value::default();");
  index = 0;
  for (const auto* part : parts) {
    std::string decoder;
    if (part->value_type) {
      const auto& owner = static_cast<const class_node&>(*part->value_type->resolved_node);
      const auto binding = xml_binding::schema{xml}.bind(owner);
      decoder = ",|text,limits| {let mut input=SrlXmlInput::new(text,limits).map_err(" + api("native_error") +
          ")?;let _=input.take_type().map_err(" + api("native_error") + ")?;let value=" +
          render_type(*part->value_type) + "::srl_xml_read_tokens(&mut input," + literal(binding.root.namespace_uri) +
          "," + literal(binding.root.local) + ").map_err(" + api("native_error") +
          ")?;input.finish_document().map_err(" + api("native_error") + ")?;Ok(value)}";
    }
    line("    value." + render_field(part->name) + "=loader." +
        std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} + "(&Self::ROLE_" + std::to_string(index++) +
        decoder + ")?;");
  }
  line("    value.paths=loader.paths(); value.reference_links=loader.reference_links(); value.extensions=loader.finish(policy)?; Self::validate_references(&value,budget)?; Ok(value)\n  }");
  line("  /// Read a bounded nonseekable source without taking ownership of it.");
  line("  pub fn read(source:&mut impl std::io::Read,budget:&mut " + api("Budget") + ",policy:" + api("ExtensionPolicy") + ") -> " + api("Result") + "<" + name + "Value> { let bytes=" + helper + "::read_source(source,budget)?; Self::load(&bytes,budget,policy) }");
  line("  /// Prepare a complete archive before writing; a failed borrowed sink may retain a prefix.");
  line("  pub fn write(destination:&mut impl std::io::Write,value:&" + name + "Input,budget:&mut " + api("Budget") + ") -> " + api("Result") + "<()> { let bytes=Self::encode(value,budget)?; " + helper + "::write_sink(destination,&bytes,budget) }");
  line("  /// Validate a complete candidate, then atomically replace a same-directory file.");
  line("  pub fn save(path:&std::path::Path,value:&" + name + "Input,budget:&mut " + api("Budget") + ",fault:Option<&dyn Fn(&str)->" + api("Result") + "<()>>) -> " + api("Result") + "<()> { let bytes=Self::encode(value,budget)?; let _=Self::load(&bytes,budget," + api("ExtensionPolicy") + "::preserve())?; " + helper + "::save(path,&bytes,budget,fault) }\n}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
