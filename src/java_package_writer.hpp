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
// Emit statically typed Java access over the schema's existing source-ordered package roles.
inline std::string java(const package_node& package, const std::string& name, std::string_view qualified_name,
    const std::string& helper,
    const std::function<std::string(std::string_view)>& helper_type,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal, const xml_options& xml = {}, bool binary_none_only = false) {
  const auto configured_package = configure_package(package, xml);
  const auto parts = ordered(configured_package);
  const auto contract = compile_package_profile(package,xml);
  std::set<std::string> fields{"extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  for (const auto* part : parts) {
    if (binary_none_only && part->value_type && part->codec == "json") {
      throw std::invalid_argument{"JSON package profile requires the JSON native protocol"};
    }
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Java package field or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated package roles require dynamic names and generated relationship IDs"};
    }
  }
  std::string output;
  const auto line = [&](const std::string& value = {}) { output += value + '\n'; };
  const auto api = [&](std::string_view type) { return helper + "." + helper_type(type); };
  const auto role_type = [&](const package_part_declaration& part) {
    if (singleton(part)) { return render_type(*part.value_type); }
    return "java.util.List<" + (part.value_type ? api("ModelPart") + "<" + render_type(*part.value_type) + ">" : api("BinaryPart")) + ">";
  };
  std::set<std::string> enclosing;
  for (std::size_t start{}; start < qualified_name.size();) {
    const auto end = qualified_name.find('.', start);
    enclosing.emplace(qualified_name.substr(start, end == std::string_view::npos ? end : end - start));
    if (end == std::string_view::npos) { break; }
    start = end + 1;
  }
  const auto nested = [&](std::string candidate) {
    const auto base = candidate;
    for (std::size_t suffix = 2; enclosing.contains(candidate); ++suffix) { candidate = base + "_" + std::to_string(suffix); }
    return candidate;
  };
  const auto input = nested("Input");
  const auto value = nested("Value");
  line("/** Complete schema-bound OPC profile; standard metadata is compiler-owned. */");
  line("public static final class " + name + " {");
  line("  /** Static package facade. */");
  line("  private " + name + "() {}");
  line("  /** Borrow typed/binary values for synchronous composition. */");
  line("  public static class " + input + " {");
  for (const auto* part : parts) {
    line("    public " + role_type(*part) + " " + render_field(part->name) +
        (singleton(*part) ? ";" : " = java.util.List.of();"));
  }
  line("    public " + api("Extensions") + " extensions = new " + api("Extensions") + "();");
  line("    /** Construct empty role bindings; encode enforces required occurrences. */");
  line("    public " + input + "() {}");
  line("  }");
  line("  /** Own a fresh complete decoded aggregate. */");
  line("  public static final class " + value + " extends " + input + " {");
  line("    /** Initialize unpublished empty state. */");
  line("    private " + value + "() {}");
  line("  }");
  std::size_t index{};
  for (const auto* part : parts) {
    const auto metadata=std::find_if(contract.parts.begin(),contract.parts.end(),[&](const auto& item){return item.role==part->name;});
    const auto expanded=[&](const auto& item){return literal(item.namespace_uri.empty()?item.local:"{"+item.namespace_uri+"}"+item.local);};
    const auto expected=metadata->decoder?","+expanded(metadata->decoder->root)+","+expanded(*metadata->decoder->type):",null,null";
    std::string vocabularies{"java.util.List.of("};
    if (metadata->decoder) { for (const auto& uri : metadata->decoder->vocabularies) { if (vocabularies.back() != '(') { vocabularies += ','; } vocabularies += literal(uri); } }
    vocabularies += ')';
    const auto optional = [&](const auto& item) { return item ? literal(*item) : "null"; };
    line("  private static final " + api("Role") + " role" + std::to_string(index++) + " = new " + api("Role") + "(" +
        literal(render_field(part->name)) + "," + optional(part->part_name) + "," + optional(part->content_type) + "," +
        literal(part->source == "/" ? "/" : render_field(part->source)) + "," + literal(part->relationship) + "," +
        optional(part->relationship_id) + "," + literal(part->compression) + "," + std::to_string(part->min_occurs) + "L," +
        (part->max_occurs ? std::to_string(*part->max_occurs) : "-1") + "L,"+literal(part->profile_policy)+expected+","+literal(part->name)+","+literal(part->value_type?part->codec:"bytes")+","+literal(part->target_mode)+","+(part->requires_type?"true":"false")+","+vocabularies+");");
  }
  line("  /** Validate compiler-declared scalar application references before publication. */");
  line("  private static void references("+input+" value,"+api("Budget")+" budget)throws java.io.IOException{references(value,budget,null);}");
  line("  private static void references("+input+" value,"+api("Budget")+" budget,"+api("Loader")+" loader)throws java.io.IOException{var index=new "+api("References")+"(budget);");
  for(const auto* part:parts){
    if(part->keys.empty()&&part->references.empty()){continue;}
    const auto field=render_field(part->name);
    line(singleton(*part)?"    {int occurrence=0;if(value."+field+"!=null){var model=value."+field+";":"    {int occurrence=0;for(var item:value."+field+"){var model=item.value;");
    const auto path=singleton(*part)?literal(*part->part_name):"item.name";
    const auto link=part->relationship_id?literal(*part->relationship_id):literal("srl_"+part->name+"_")+"+occurrence";
    line("      if(loader!=null)loader.locateReferences(index,"+literal(field)+",occurrence);else index.location("+path+","+link+");");
    const auto lexical=[&](std::string_view field){const auto& member=selector_member(*part,field);return literal(member.type_name_list.front().name=="string"?"s:":"i:")+"+String.valueOf(model."+render_field(member.name)+")";};
    const auto model_identity=literal(part->value_type->resolved_node->get_full_name());
    for(const auto& key:part->keys){line("      index.key("+literal(key.scope)+","+lexical(key.field)+","+literal(field)+","+literal(key.field)+","+model_identity+");");}
    for(const auto& reference:part->references){const auto target=std::find_if(parts.begin(),parts.end(),[&](const auto* item){return item->name==reference.target_role;});const auto expected=literal((*target)->value_type->resolved_node->get_full_name());line("      index.reference("+literal(reference.scope)+","+lexical(reference.field)+","+expected+","+literal(field)+","+literal(reference.field)+");");}
    line("      ++occurrence;}}");
  }
  line("    index.finish();}");
  line("  /** Return an owning archive after complete declared-role validation. */");
  line("  public static byte[] encode(" + input + " value) throws java.io.IOException { return encode(value,new " + api("Budget") + "()); }");
  line("  /** Share one cumulative package budget through all composition stages. */");
  line("  public static byte[] encode(" + input + " value," + api("Budget") + " budget) throws java.io.IOException {");
  line("    return encode(value,budget,java.util.Map.of());}");
  line("  /** Apply per-role compression defaults while fixed policies retain their declared envelope. */");
  line("  public static byte[] encode("+input+" value,"+api("Budget")+" budget,java.util.Map<String,String> profiles)throws java.io.IOException{");
  line("    return encodeProfiles(value,budget,profiles,java.util.Map.of());}");
  line("  /** Select native/XML codecs and entry storage without replacing bound model identities. */");
  line("  public static byte[] encodeWithPipelineProfiles("+input+" value,"+api("Budget")+" budget,java.util.Map<String,"+api("PipelineProfile")+"> profiles)throws java.io.IOException{return encodeProfiles(value,budget,java.util.Map.of(),profiles);}");
  line("  /** Snapshot and validate every envelope before invoking any native codec. */");
  line("  private static byte[] encodeProfiles("+input+" value,"+api("Budget")+" budget,java.util.Map<String,String> profiles,java.util.Map<String,"+api("PipelineProfile")+"> pipelines)throws java.io.IOException{");
  line("    java.util.Objects.requireNonNull(value);profiles=java.util.Map.copyOf(profiles);pipelines=java.util.Map.copyOf(pipelines);references(value,budget);");
  std::string known;
  for(const auto* part:parts){if(!known.empty()){known+="&&";}known+="!key.equals("+literal(part->name)+")";}
  line("    for(String key:profiles.keySet())if("+(known.empty()?"true":known)+")throw new "+api("PackageError")+"("+api("Code")+".INVALID_OPTIONS,\"Unknown package profile role\");var composer=new "+api("Composer")+"(budget);");
  line("    for(String key:pipelines.keySet())if("+(known.empty()?"true":known)+")throw new "+api("PackageError")+"("+api("Code")+".INVALID_OPTIONS,\"Unknown package profile role\");");
  for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
    line("    final var selectedRole"+std::to_string(role_index)+"="+helper+".selectProfile(role"+std::to_string(role_index)+",profiles,pipelines);");
    if (binary_none_only) { line("    if(selectedRole"+std::to_string(role_index)+".codec.equals(\"json\"))throw new "+api("PackageError")+"("+api("Code")+".INVALID_OPTIONS,\"JSON native protocol is disabled\");"); }
  }
  index = 0;
  for (const auto* part : parts) {
    const auto field = render_field(part->name);
    line("    composer." + std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} +
        "(selectedRole" + std::to_string(index) + ",value." + field +
        (part->value_type ? ",(model,limits)->{if(selectedRole"+std::to_string(index)+".codec.equals(\"xml\"))return model.encodeXml(limits,null);var output=new Writer(selectedRole"+std::to_string(index)+".codec.equals(\"json\")?Protocol.JSON:Protocol.BINARY_NONE,limits);model.write(output);return output.bytes.toByteArray();}" : "") + ");");
    ++index;
  }
  line("    return composer.finish(value.extensions);");
  line("  }");
  line("  /** Borrow a sink only after package preparation; a sink error may leave a prefix. */");
  line("  public static void write(java.io.OutputStream destination," + input + " value," + api("Budget") + " budget) throws java.io.IOException { " + helper + ".write(destination,encode(value,budget),budget); }");
  line("  /** Load one complete fresh aggregate; unknown capabilities reject by default. */");
  line("  public static " + api("Result") + "<" + value + "> load(Object source) throws java.io.IOException { return load(source,new " + api("Budget") + "(),null,null); }");
  line("  /** Apply explicit extension policy and complete-aggregate domain validation before publication. */");
  line("  public static " + api("Result") + "<" + value + "> load(Object source," + api("Budget") + " budget," + api("ExtensionPolicy") + " policy," + api("Validator") + "<" + value + "> validate) throws java.io.IOException {");
  line("    return loadWithPipelineProfiles(source,budget,policy,validate,java.util.Map.of());}");
  line("  /** Decode selected native/XML envelopes into a fresh complete aggregate. */");
  line("  public static "+api("Result")+"<"+value+"> loadWithPipelineProfiles(Object source,"+api("Budget")+" budget,"+api("ExtensionPolicy")+" policy,"+api("Validator")+"<"+value+"> validate,java.util.Map<String,"+api("PipelineProfile")+"> pipelines)throws java.io.IOException{");
  line("    pipelines=java.util.Map.copyOf(pipelines);for(String key:pipelines.keySet())if("+(known.empty()?"true":known)+")throw new "+api("PackageError")+"("+api("Code")+".INVALID_OPTIONS,\"Unknown package profile role\");");
  for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
    line("    final var selectedRole"+std::to_string(role_index)+"="+helper+".selectProfile(role"+std::to_string(role_index)+",java.util.Map.of(),pipelines);");
    if (binary_none_only) { line("    if(selectedRole"+std::to_string(role_index)+".codec.equals(\"json\"))throw new "+api("PackageError")+"("+api("Code")+".INVALID_OPTIONS,\"JSON native protocol is disabled\");"); }
  }
  line("    try(var loader=new " + api("Loader") + "(source,budget)){var value=new " + value + "();");
  index = 0;
  for (const auto* part : parts) {
    line("      value." + render_field(part->name) + "=loader." +
        std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} + "(selectedRole" + std::to_string(index) +
        (part->value_type ? ",(bytes,limits)->selectedRole"+std::to_string(index)+".codec.equals(\"xml\")?"+render_type(*part->value_type)+".decodeXml(bytes,limits,null):"+render_type(*part->value_type)+".decode(bytes,selectedRole"+std::to_string(index)+".codec.equals(\"json\")?Protocol.JSON:Protocol.BINARY_NONE,limits)" : "") + ");");
    ++index;
  }
  line("      references(value,budget,loader);");
  line("      return loader.finish(value,policy,validate,(candidate,extensions)->candidate.extensions=extensions);");
  line("    }");
  line("  }");
  line("  /** Validate a staged round-trip before creating any private sibling or replacing the final file. */");
  line("  public static void save(java.nio.file.Path path," + input + " value," + api("Budget") + " budget," + api("Validator") + "<" + value + "> validate," + api("SaveFault") + " fault) throws java.io.IOException {");
  line("    byte[] bytes=encode(value,budget);load(bytes,budget,new " + api("ExtensionPolicy") + "(" + api("Unknown") + ".PRESERVE," + api("Unknown") + ".PRESERVE,null,null),validate);" + helper + ".save(path,bytes,budget,fault);");
  line("  }");
  line("  /** Revalidate the same selected envelopes before atomic final file publication. */");
  line("  public static void saveWithPipelineProfiles(java.nio.file.Path path,"+input+" value,"+api("Budget")+" budget,"+api("Validator")+"<"+value+"> validate,"+api("SaveFault")+" fault,java.util.Map<String,"+api("PipelineProfile")+"> profiles)throws java.io.IOException{profiles=java.util.Map.copyOf(profiles);byte[] bytes=encodeWithPipelineProfiles(value,budget,profiles);loadWithPipelineProfiles(bytes,budget,new "+api("ExtensionPolicy")+"("+api("Unknown")+".PRESERVE,"+api("Unknown")+".PRESERVE,null,null),validate,profiles);"+helper+".save(path,bytes,budget,fault);}");
  line("}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
