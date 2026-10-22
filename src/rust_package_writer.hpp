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
    const std::function<std::string(std::string_view)>& literal,
    const std::string& operation_module, bool binary_only = false) {
  const auto configured_package = configure_package(package, xml);
  const auto parts = ordered(configured_package);
  for (const auto* part : parts) {
    if (binary_only && part->codec == "json") { throw std::invalid_argument{"JSON package pipeline disabled during generation"}; }
  }
  const auto contract = compile_package_profile(package, xml);
  std::set<std::string> fields{"extensions", "paths", "reference_links"};
  const bool companions = std::any_of(parts.begin(), parts.end(), [&](const auto* part) {
    return native_package_companion_required(*part, xml);
  });
  if (companions) { fields.insert("xml_companions"); }
  std::set<std::pair<std::string, std::string>> dispatch;
  std::string output;
  const auto line = [&](const std::string& text) { output += text + '\n'; };
  const auto api = [&](std::string_view type) { return helper + "::" + std::string{type}; };
  const auto optional = [&](const auto& value) { return value ? "Some(" + literal(*value) + ")" : "None"; };
  // Keep selected codecs in typed function arguments instead of introducing collision-prone model names.
  const auto codec_parameters = [&](bool encode, const std::string& prefix) {
    std::string result;
    for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
      const auto* part = parts[role_index];
      if (!part->value_type) { continue; }
      const auto type = render_type(*part->value_type);
      result += ',' + prefix + '_' + std::to_string(role_index) + ":impl FnMut(" +
          (encode ? '&' + type : "&str") + ",&mut " + api("XmlOperation") +
          "<'_,'_>)->" + api("Result") + '<' + (encode ? "String" : type) + '>';
    }
    return result;
  };
  // Forward each typed role exactly once, allowing callers to borrow mutable callback state explicitly.
  const auto codec_arguments = [&](const std::string& prefix) {
    std::string result;
    for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
      if (parts[role_index]->value_type) { result += ',' + prefix + '_' + std::to_string(role_index); }
    }
    return result;
  };
  for (const auto* part : parts) {
    require_native_package_graph(*part, xml, "Rust", true);
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Rust package member or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated Rust package roles require dynamic names and relationship IDs"};
    }
  }
  line("/// Complete owning package candidate; failure never publishes a partially loaded value.");
  line("// Generated Rust package operation module: " + operation_module);
  line("#[derive(Default)] pub struct " + name + "Value {");
  for (const auto* part : parts) {
    const auto type = singleton(*part) ? "Option<" + render_type(*part->value_type) + ">" :
        "Vec<" + (part->value_type ? api("ModelPart") + "<" + render_type(*part->value_type) + ">" : api("BinaryPart")) + ">";
    line("  pub " + render_field(part->name) + ": " + type + ",");
  }
  line("  pub extensions: " + api("Extensions") + ",");
  line("  pub paths: std::collections::BTreeMap<String,String>,");
  if (companions) {
    line("  /// Keep XML presentation under stable role/physical identities, independently of native values.");
    line("  pub xml_companions: " + api("XmlCompanions") + ",");
  }
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
  index = 0;
  for (const auto* part : parts) {
    const auto metadata = std::find_if(contract.parts.begin(), contract.parts.end(), [&](const auto& item) { return item.role == part->name; });
    std::string vocabularies{"&["};
    if (metadata->decoder) { for (const auto& uri : metadata->decoder->vocabularies) { vocabularies += literal(uri) + ','; } }
    vocabularies += ']';
    const auto number = std::to_string(index++);
    line("  const PROFILE_" + number + ": " + api("RoleProfile") + " = " + api("RoleProfile") + "{role:Self::ROLE_" + number + ",codec:" + api("PayloadCodec") + "::" + (part->codec == "xml" ? "Xml" : part->codec == "json" ? "Json" : part->codec == "binary_none" ? "BinaryNone" : "Bytes") + ",target_mode:" + api("TargetMode") + "::" + (part->target_mode == "external" ? "External" : part->target_mode == "both" ? "Both" : "Internal") + ",requires_type:" + (part->requires_type ? "true" : "false") + ",vocabularies:" + vocabularies + "};");
  }
  line("  /// Inspect complete profile metadata without extending old public role or value structs.");
  line("  pub fn pipeline_profiles()->&'static [" + api("RoleProfile") + "]{&[");
  for (std::size_t role_index{}; role_index < parts.size(); ++role_index) { line("    Self::PROFILE_" + std::to_string(role_index) + ','); }
  line("  ]}");
  line("  /// Resolve only declared codecs and compression under fixed/default profile policy.");
  line("  pub fn select_profiles(overrides:&[" + api("PipelineOverride") + "],budget:&" + api("Budget") + ")->" + api("Result") + "<Vec<" + api("RoleProfile") + ">> {" +
      (binary_only ? "let selected=" + helper + "::resolve_profiles(Self::pipeline_profiles(),overrides,budget)?;if selected.iter().any(|role|role.codec==" + api("PayloadCodec") + "::Json){return Err(" + helper + "::fail(" + api("Code") + "::InvalidOptions,\"JSON package pipeline disabled during generation\",\"\",\"\",\"\"));}Ok(selected)" : helper + "::resolve_profiles(Self::pipeline_profiles(),overrides,budget)") + "}");
  const auto pipeline_encoder = [&](const package_part_declaration& part) {
    const auto encode = native_package_companion_required(part, xml) ?
        "context.encode_document(model,value.xml_companions.selected(" + literal(part.name) +
        ",context.part," + (singleton(part) ? "true" : "false") + ",renames))" : "context.encode(model)";
    return ",|model,profile,context|{if profile.codec==" + api("PayloadCodec") + "::Xml{" + encode + ".map(String::into_bytes)}else{context.encode_native(profile.codec,|writer|model.srl_write(writer))}}";
  };
  const auto pipeline_decoder = [&](const package_part_declaration& part) {
    const auto decode = native_package_companion_required(part, xml) ?
        std::string{"let document=context.decode_document(text)?;context.retain_document(&mut value.xml_companions,"} +
        (singleton(part) ? "true" : "false") + ",document.extensions)?;Ok(document.value)" : "context.decode(text)";
    return ",|bytes,profile,context|{if profile.codec==" + api("PayloadCodec") + "::Xml{let text=std::str::from_utf8(bytes).map_err(" + helper + "::native_error)?;" + decode + "}else{context.decode_native(profile.codec,bytes,|reader|{let mut value=" + render_type(*part.value_type) + "::default();value.srl_read(reader)?;Ok(value)})}}";
  };
  line("  /// Compose existing XML/JSON/positional-binary payloads under one operation ledger.");
  line("  pub fn encode_profiles(value:&" + name + "Input,budget:&mut " + api("Budget") + ",overrides:&[" + api("PipelineOverride") + "]) -> " + api("Result") + "<Vec<u8>> {");
  line("    let profiles=Self::select_profiles(overrides,budget)?;Self::validate_references(value,budget)?;let mut composer=" + api("Composer") + "::new(budget,&value.paths);");
  if (companions) { line("    let renames:&[" + api("PartRename") + "]=&[];"); }
  index = 0;
  for (const auto* part : parts) {
    line("    composer." + std::string{part->value_type ? (singleton(*part) ? "single_pipeline" : "models_pipeline") : "binaries_pipeline"} + "(&profiles[" + std::to_string(index++) + "],&value." + render_field(part->name) + (part->value_type ? pipeline_encoder(*part) : "") + ")?;");
  }
  line("    composer.finish(&value.extensions)\n  }");
  line("  /// Load a selected envelope without retaining overrides in or changing public value structs.");
  line("  pub fn load_profiles(bytes:&[u8],budget:&mut " + api("Budget") + ",policy:" + api("ExtensionPolicy") + ",overrides:&[" + api("PipelineOverride") + "]) -> " + api("Result") + "<" + name + "Value> {");
  line("    let profiles=Self::select_profiles(overrides,budget)?;let mut loader=" + api("Loader") + "::new(bytes,budget)?;let mut value=" + name + "Value::default();");
  index = 0;
  for (const auto* part : parts) {
    line("    value." + render_field(part->name) + "=loader." + std::string{part->value_type ? (singleton(*part) ? "single_pipeline" : "models_pipeline") : "binaries_pipeline"} + "(&profiles[" + std::to_string(index++) + "]" + (part->value_type ? pipeline_decoder(*part) : "") + ")?;");
  }
  line("    value.paths=loader.paths();value.reference_links=loader.reference_links();value.extensions=loader.finish(policy)?;Self::validate_references(&value,budget)?;Ok(value)\n  }");
  line("  /// Revalidate the same selected native pipelines before atomic package publication.");
  line("  pub fn save_profiles(path:&std::path::Path,value:&" + name + "Input,budget:&mut " + api("Budget") + ",overrides:&[" + api("PipelineOverride") + "],fault:Option<&dyn Fn(&str)->" + api("Result") + "<()>>) -> " + api("Result") + "<()> {let bytes=Self::encode_profiles(value,budget,overrides)?;let _=Self::load_profiles(&bytes,budget," + api("ExtensionPolicy") + "::preserve(),overrides)?;" + helper + "::save(path,&bytes,budget,fault)}");
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
  line("  pub fn encode(value:&" + name + "Input,budget:&mut " + api("Budget") + ") -> " + api("Result") + "<Vec<u8>> {Self::encode_profiles(value,budget,&[])}");
  line("  /// Compose caller-selected synchronous encoders with the same package operation context.");
  line("  pub fn encode_with_codecs(value:&" + name + "Input,budget:&mut " + api("Budget") + codec_parameters(true, "codec") + ") -> " + api("Result") + "<Vec<u8>> {");
  line("    " + helper + "::validate_overrides(Self::profiles(),budget)?; Self::validate_references(value,budget)?;");
  line("    let mut composer=" + api("Composer") + "::new(budget,&value.paths);");
  index = 0;
  for (const auto* part : parts) {
    if (part->value_type) { line("    let mut selected_codec_" + std::to_string(index) + "=codec_" + std::to_string(index) + ";"); }
    line("    composer." + std::string{part->value_type ? (singleton(*part) ? "single_pipeline" : "models_pipeline") : "binaries_pipeline"} +
        "(&Self::PROFILE_" + std::to_string(index) + ",&value." + render_field(part->name) +
        (part->value_type ? ",|model,profile,context|{if profile.codec!=" + api("PayloadCodec") + "::Xml{return Err(" + helper + "::invalid_options(\"Selected XML codec requires XML envelope\"));}selected_codec_" + std::to_string(index) + "(model,context).map(String::into_bytes)}" : "") + ")?;");
    ++index;
  }
  line("    composer.finish(&value.extensions)\n  }");
  line("  /// Decode declared roles and all reference checks before returning an owning aggregate.");
  line("  pub fn load(bytes:&[u8],budget:&mut " + api("Budget") + ",policy:" + api("ExtensionPolicy") + ") -> " + api("Result") + "<" + name + "Value> {Self::load_profiles(bytes,budget,policy,&[])}");
  line("  /// Select typed decoders after root dispatch while retaining exact caller errors and shared budgets.");
  line("  pub fn load_with_codecs(bytes:&[u8],budget:&mut " + api("Budget") + ",policy:" + api("ExtensionPolicy") + codec_parameters(false, "codec") + ") -> " + api("Result") + "<" + name + "Value> {");
  line("    " + helper + "::validate_overrides(Self::profiles(),budget)?;");
  line("    let mut loader=" + api("Loader") + "::new(bytes,budget)?; let mut value=" + name + "Value::default();");
  index = 0;
  for (const auto* part : parts) {
    if (part->value_type) { line("    let mut selected_codec_" + std::to_string(index) + "=codec_" + std::to_string(index) + ";"); }
    const auto capture = native_package_companion_required(*part, xml) ?
        "let extensions=context.prepare_document::<" + render_type(*part->value_type) + ">(text)?;" : "";
    const auto retain = native_package_companion_required(*part, xml) ?
        std::string{"context.retain_document(&mut value.xml_companions,"} + (singleton(*part) ? "true" : "false") + ",extensions)?;" : "";
    line("    value." + render_field(part->name) + "=loader." +
        std::string{part->value_type ? (singleton(*part) ? "single_pipeline" : "models_pipeline") : "binaries_pipeline"} + "(&Self::PROFILE_" + std::to_string(index) +
        (part->value_type ? ",|bytes,profile,context|{if profile.codec!=" + api("PayloadCodec") + "::Xml{return Err(" + helper + "::invalid_options(\"Selected XML codec requires XML envelope\"));}let text=std::str::from_utf8(bytes).map_err(" + helper + "::native_error)?;" + capture + "let model=selected_codec_" + std::to_string(index) + "(text,context)?;" + retain + "Ok(model)}" : "") + ")?;");
    ++index;
  }
  line("    value.paths=loader.paths(); value.reference_links=loader.reference_links(); value.extensions=loader.finish(policy)?; Self::validate_references(&value,budget)?; Ok(value)\n  }");
  line("  /// Read a bounded nonseekable source without taking ownership of it.");
  line("  pub fn read(source:&mut impl std::io::Read,budget:&mut " + api("Budget") + ",policy:" + api("ExtensionPolicy") + ") -> " + api("Result") + "<" + name + "Value> { let bytes=" + helper + "::read_source(source,budget)?; Self::load(&bytes,budget,policy) }");
  line("  /// Prepare a complete archive before writing; a failed borrowed sink may retain a prefix.");
  line("  pub fn write(destination:&mut impl std::io::Write,value:&" + name + "Input,budget:&mut " + api("Budget") + ") -> " + api("Result") + "<()> { let bytes=Self::encode(value,budget)?; " + helper + "::write_sink(destination,&bytes,budget) }");
  line("  /// Validate a complete candidate, then atomically replace a same-directory file.");
  line("  pub fn save(path:&std::path::Path,value:&" + name + "Input,budget:&mut " + api("Budget") + ",fault:Option<&dyn Fn(&str)->" + api("Result") + "<()>>) -> " + api("Result") + "<()> { let bytes=Self::encode(value,budget)?; let _=Self::load(&bytes,budget," + api("ExtensionPolicy") + "::preserve())?; " + helper + "::save(path,&bytes,budget,fault) }");
  line("  /// Read and compose selected decoders with the same source, XML and reference ledger.");
  line("  pub fn read_with_codecs(source:&mut impl std::io::Read,budget:&mut " + api("Budget") + ",policy:" + api("ExtensionPolicy") + codec_parameters(false, "codec") + ") -> " + api("Result") + "<" + name + "Value> {let bytes=" + helper + "::read_source(source,budget)?;Self::load_with_codecs(&bytes,budget,policy" + codec_arguments("codec") + ")}");
  line("  /// Finish every selected encoder before borrowing the destination sink.");
  line("  pub fn write_with_codecs(destination:&mut impl std::io::Write,value:&" + name + "Input,budget:&mut " + api("Budget") + codec_parameters(true, "codec") + ") -> " + api("Result") + "<()> {let bytes=Self::encode_with_codecs(value,budget" + codec_arguments("codec") + ")?;" + helper + "::write_sink(destination,&bytes,budget)}");
  line("  /// Run selected encoders and decoders before atomically publishing a validated package file.");
  line("  pub fn save_with_codecs(path:&std::path::Path,value:&" + name + "Input,budget:&mut " + api("Budget") + ",fault:Option<&dyn Fn(&str)->" + api("Result") + "<()>>" + codec_parameters(true, "encode") + codec_parameters(false, "decode") + ") -> " + api("Result") + "<()> {let bytes=Self::encode_with_codecs(value,budget" + codec_arguments("encode") + ")?;let _=Self::load_with_codecs(&bytes,budget," + api("ExtensionPolicy") + "::preserve()" + codec_arguments("decode") + ")?;" + helper + "::save(path,&bytes,budget,fault)}\n}");
  line("/// Add canonical output through a helper-owned trait without changing inherent methods or value layouts.");
  line("impl " + api("CanonicalPackage") + " for " + name + " {");
  line("  type Input=" + name + "Input;");
  line("  /// Borrow all typed payloads while rebasing original preserved link scopes simultaneously.");
  line("  fn encode_canonical(value:&Self::Input,budget:&mut " + api("Budget") + ") -> " + api("Result") + "<Vec<u8>> {");
  line("    <Self as " + api("CanonicalPackage") + ">::encode_renamed(value,budget,&[])\n  }");
  line("  /// Retain stable occurrence identities independently of native list reorder.");
  const auto dynamic_parts = std::any_of(parts.begin(), parts.end(), [](const auto* part) { return !part->part_name; });
  line("  fn dynamic_paths(value:&Self::Input)->Vec<" + api("PartRename") + ">{let " + std::string{dynamic_parts ? "mut " : ""} + "result=Vec::new();");
  for (const auto* part : parts) {
    if (part->part_name) { continue; }
    line("    for item in &value." + render_field(part->name) + "{" + std::string{part->target_mode != "internal" ? "if item.content_type.is_empty(){continue;}" : ""} + "result.push(" + api("PartRename") + "{role:" + literal(part->name) + ".into(),original:item.name.clone(),destination:item.name.clone()});}");
  }
  line("    let _=value;result\n  }");
  line("  /// Apply explicit dynamic moves while retaining original relative relationship context.");
  const auto renamed_begin = output.size();
  line("  fn encode_renamed(value:&Self::Input,budget:&mut " + api("Budget") + ",renames:&[" + api("PartRename") + "]) -> " + api("Result") + "<Vec<u8>> {");
  line("    " + helper + "::validate_overrides(Self::profiles(),budget)?;Self::validate_references(value,budget)?;");
  std::string present;
  for (const auto* part : parts) {
    if (!present.empty()) { present += ','; }
    present += singleton(*part) ? "value." + render_field(part->name) + ".is_some()" : "false";
  }
  line("    let mut paths=" + api("CanonicalPaths") + "::new(Self::profiles(),&value.paths,&[" + present + "],budget)?;");
  if (dynamic_parts) {
  line("    for rename in renames{let present=match rename.role.as_str(){");
  for (const auto* part : parts) {
    if (part->part_name) { continue; }
    line("      " + literal(part->name) + "=>value." + render_field(part->name) + ".iter().any(|item|" + std::string{part->target_mode != "internal" ? "!item.content_type.is_empty()&&" : ""} + "item.name.eq_ignore_ascii_case(&rename.destination)),");
  }
  line("      _=>return Err(" + helper + "::invalid_options(\"Rename role is not dynamic\")),};paths.add_dynamic(rename,present,budget)?;}");
  } else {
    line("    if !renames.is_empty(){return Err(" + helper + "::invalid_options(\"Rename role is not dynamic\"));}");
  }
  line("    let mut composer=" + api("Composer") + "::new_canonical(budget,std::mem::take(&mut paths.paths));");
  index = 0;
  for (const auto* part : parts) {
    line("    composer." + std::string{part->value_type ? (singleton(*part) ? "single_pipeline" : "models_pipeline") : "binaries_pipeline"} +
        "(&Self::PROFILE_" + std::to_string(index++) + ",&value." + render_field(part->name) +
        (part->value_type ? pipeline_encoder(*part) : "") + ")?;");
  }
  line("    composer.finish_canonical(&value.extensions,&paths)\n  }");
  // Reuse the canonical implementation for explicit profiles instead of maintaining a second traversal.
  auto renamed_profiles = output.substr(renamed_begin);
  const auto signature_end = renamed_profiles.find('\n');
  renamed_profiles.replace(0, signature_end,
      "  /// Compose declared runtime pipelines and explicit physical moves under the same operation.\n"
      "  pub fn encode_renamed_profiles(value:&" + name + "Input,budget:&mut " + api("Budget") +
      ",renames:&[" + api("PartRename") + "],overrides:&[" + api("PipelineOverride") +
      "]) -> " + api("Result") + "<Vec<u8>> {");
  const auto old_selection = helper + "::validate_overrides(Self::profiles(),budget)?;";
  renamed_profiles.replace(renamed_profiles.find(old_selection), old_selection.size(),
      "let profiles=Self::select_profiles(overrides,budget)?;");
  for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
    const auto marker = "&Self::PROFILE_" + std::to_string(role_index) + ',';
    renamed_profiles.replace(renamed_profiles.find(marker), marker.size(),
        "&profiles[" + std::to_string(role_index) + "],");
  }
  output.replace(renamed_begin, output.size() - renamed_begin,
      "  fn encode_renamed(value:&Self::Input,budget:&mut " + api("Budget") + ",renames:&[" +
      api("PartRename") + "]) -> " + api("Result") +
      "<Vec<u8>> {Self::encode_renamed_profiles(value,budget,renames,&[])}\n");
  line("  /// Publish only a fully revalidated dynamic rename operation.");
  line("  fn save_renamed(path:&std::path::Path,value:&Self::Input,budget:&mut " + api("Budget") + ",renames:&[" + api("PartRename") + "],fault:Option<&dyn Fn(&str)->" + api("Result") + "<()>>) -> " + api("Result") + "<()> {let bytes=<Self as " + api("CanonicalPackage") + ">::encode_renamed(value,budget,renames)?;let _=Self::load(&bytes,budget," + api("ExtensionPolicy") + "::preserve())?;" + helper + "::save(path,&bytes,budget,fault)}");
  line("  /// Complete canonical encoding before writing to the borrowed destination.");
  line("  fn write_canonical(destination:&mut impl std::io::Write,value:&Self::Input,budget:&mut " + api("Budget") + ") -> " + api("Result") + "<()> {let bytes=<Self as " + api("CanonicalPackage") + ">::encode_canonical(value,budget)?;" + helper + "::write_sink(destination,&bytes,budget)}");
  line("  /// Validate preserved closure and native codecs before atomic canonical-path Save.");
  line("  fn save_canonical(path:&std::path::Path,value:&Self::Input,budget:&mut " + api("Budget") + ",fault:Option<&dyn Fn(&str)->" + api("Result") + "<()>>) -> " + api("Result") + "<()> {let bytes=<Self as " + api("CanonicalPackage") + ">::encode_canonical(value,budget)?;let _=Self::load(&bytes,budget," + api("ExtensionPolicy") + "::preserve())?;" + helper + "::save(path,&bytes,budget,fault)}\n}");
  line("impl " + name + " {");
  output += renamed_profiles;
  line("  /// Revalidate the same selected pipelines before publishing an explicit rename operation.");
  line("  pub fn save_renamed_profiles(path:&std::path::Path,value:&" + name + "Input,budget:&mut " + api("Budget") + ",renames:&[" + api("PartRename") + "],overrides:&[" + api("PipelineOverride") + "],fault:Option<&dyn Fn(&str)->" + api("Result") + "<()>>) -> " + api("Result") + "<()> {let bytes=Self::encode_renamed_profiles(value,budget,renames,overrides)?;let _=Self::load_profiles(&bytes,budget," + api("ExtensionPolicy") + "::preserve(),overrides)?;" + helper + "::save(path,&bytes,budget,fault)}\n}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
