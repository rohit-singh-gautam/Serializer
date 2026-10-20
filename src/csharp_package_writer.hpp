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
// Substitute collision-selected native helpers without reserving new schema identifiers.
inline std::string csharp_runtime(std::string_view helper_name, std::string_view compression_name, bool include_profile = true) {
  std::string result{
#include "csharp_package_runtime.inc"
#include "csharp_package_writer_runtime.inc"
  };
  if (include_profile) {
    result += std::string{
#include "csharp_package_opc_runtime.inc"
#include "csharp_package_contract_runtime.inc"
#include "csharp_package_profile_runtime.inc"
    };
  } else {
    result += "  }\n";
  }
  const auto replace = [&](std::string_view token, std::string_view value) {
    for (std::size_t offset{}; (offset = result.find(token, offset)) != std::string::npos;) {
      result.replace(offset, token.size(), value);
      offset += value.size();
    }
  };
  replace("@package_name@", helper_name);
  replace("@compression_name@", compression_name);
  return result;
}

// Generate owning role adapters while standard infrastructure remains runtime-owned.
inline std::string csharp(const package_node& package, const std::string& name,
    std::string_view qualified_name, const std::string& helper,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal, const xml_options& xml = {}) {
  const auto parts = ordered(package);
  const auto contract = compile_package_profile(package, xml);
  std::set<std::string> fields{"Extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  for (const auto* part : parts) {
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"C# package field or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated package roles require dynamic names and generated relationship IDs"};
    }
    if (part->value_type && part->codec != "xml") {
      throw std::invalid_argument{"C# typed package roles currently require XML"};
    }
  }
  const auto nested = [&](std::string candidate) {
    while (qualified_name == candidate || qualified_name.ends_with("." + candidate) || name == candidate) {
      candidate += "_";
    }
    return candidate;
  };
  const auto input = nested("Input"), value = nested("Value");
  const auto profiles_method = nested("SelectProfiles"), references_method = nested("CheckReferences");
  std::string profiles_field{"ProfileOverrides"};
  while (std::any_of(parts.begin(), parts.end(),
      [&](const auto* part) { return render_field(part->name) == profiles_field; })) {
    profiles_field += '_';
  }
  std::string output;
  const auto line = [&](const std::string& text = {}) { output += text + '\n'; };
  const auto api = [&](std::string_view type) { return helper + "." + std::string{type}; };
  const auto role_type = [&](const package_part_declaration& part) {
    if (singleton(part)) { return render_type(*part.value_type) + "?"; }
    return "global::System.Collections.Generic.IReadOnlyList<" + (part.value_type ?
        api("ModelPart") + "<" + render_type(*part.value_type) + ">" : api("BinaryPart")) + ">";
  };
  line("/// <summary>Schema-bound OPC profile with complete transactional aggregate publication.</summary>");
  line("public static class " + name + " {");
  line("  public class " + input + " {");
  for (const auto* part : parts) {
    line("    public " + role_type(*part) + " " + render_field(part->name) +
        (singleton(*part) ? ";" : " = global::System.Array.Empty<" + (part->value_type ?
            api("ModelPart") + "<" + render_type(*part->value_type) + ">" : api("BinaryPart")) + ">();"));
  }
  line("    public " + api("Extensions") + " Extensions = new();");
  line("    public global::System.Collections.Generic.IReadOnlyDictionary<string,string>? " + profiles_field + ";");
  line("  }");
  line("  public sealed class " + value + " : " + input + " { }");
  std::size_t index{};
  for (const auto* part : parts) {
    const auto profile = std::find_if(contract.parts.begin(), contract.parts.end(),
        [&](const auto& item) { return item.role == part->name; });
    const auto expanded = [&](const auto& selected) {
      return literal(selected.namespace_uri.empty() ? selected.local : "{" + selected.namespace_uri + "}" + selected.local);
    };
    const auto optional = [&](const auto& item) { return item ? literal(*item) : "null"; };
    line("  private static readonly " + api("Role") + " Role" + std::to_string(index++) + " = new(" +
        literal(part->name) + "," + optional(part->part_name) + "," + optional(part->content_type) + "," +
        literal(part->source) + "," + literal(part->relationship) + "," +
        optional(part->relationship_id) + "," + literal(part->compression) + "," + std::to_string(part->min_occurs) + "L," +
        (part->max_occurs ? std::to_string(*part->max_occurs) : "-1") + "L," + literal(part->profile_policy) + "," +
        (profile->decoder ? expanded(profile->decoder->root) : "null") + "," +
        (profile->decoder && profile->decoder->type ? expanded(*profile->decoder->type) : "null") + ");");
  }
  std::string declared_roles;
  for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
    declared_roles += "Role" + std::to_string(role_index) + ",";
  }
  line("  // Resolve only declared storage overrides and retain immutable expected model identities.");
  line("  public static " + api("Role") + "[] " + profiles_method + "(global::System.Collections.Generic.IReadOnlyDictionary<string,string>? profiles=null)=>" + helper + ".ResolveProfiles(new[]{" + declared_roles + "},profiles);");
  line("  // Validate compiler-declared keys and forward references before publication.");
  line("  private static void " + references_method + "(" + input + " value," + api("Budget") + " budget,global::System.Func<string,int," + api("PayloadLocation") + ">? locate=null) {");
  line("    var references=new " + api("ReferenceIndex") + "(budget);");
  for (const auto* part : parts) {
    if (part->keys.empty() && part->references.empty()) { continue; }
    const auto field = render_field(part->name);
    line("    {");
    if (!singleton(*part)) { line("    int index=0;"); }
    line(singleton(*part) ? "    if(value." + field + " is { } item) {" :
        "    foreach(var part in value." + field + ") { var item=part.Value;");
    const auto declared_part = singleton(*part) ? literal(*part->part_name) : "part.Name";
    const auto occurrence = singleton(*part) ? "0" : "index";
    const auto link = part->relationship_id ? literal(*part->relationship_id) :
        literal("srl_" + part->name + "_") + "+(" + occurrence + ").ToString(global::System.Globalization.CultureInfo.InvariantCulture)";
    line("      var location=locate?.Invoke(" + literal(part->name) + "," + occurrence + ")??new " + api("PayloadLocation") + "(" + declared_part + "," + link + ");");
    for (const auto& key : part->keys) {
      line("      references.Add(" + literal(key.scope) + ",item." + render_field(key.field) + "," +
          literal(part->value_type->resolved_node->get_full_name()) + ",location.Part," + literal(part->name + "." + key.field) + ",location.RelationshipId);");
    }
    for (const auto& reference : part->references) {
      const auto target = std::find_if(parts.begin(), parts.end(), [&](const auto* item) { return item->name == reference.target_role; });
      line("      references.Require(" + literal(reference.scope) + ",item." + render_field(reference.field) + "," +
          literal((*target)->value_type->resolved_node->get_full_name()) + ",location.Part," + literal(part->name + "." + reference.field) + ",location.RelationshipId);");
    }
    if (!singleton(*part)) { line("      ++index;"); }
    line("    }");
    line("    }");
  }
  line("    references.Validate();");
  line("  }");
  line("  public static byte[] Encode(" + input + " value," + api("Budget") + "? budget=null) {");
  line("    global::System.ArgumentNullException.ThrowIfNull(value);budget?" "?=new();" + references_method + "(value,budget);var profiles=" + profiles_method + "(value." + profiles_field + ");var composer=new " + api("Composer") + "(budget);");
  index = 0;
  for (const auto* part : parts) {
    line("    composer." + std::string{part->value_type ? (singleton(*part) ? "Single" : "Models") : "Binaries"} +
        "(profiles[" + std::to_string(index++) + "],value." + render_field(part->name) +
        (part->value_type ? ",static (model,limits)=>model.EncodeXml(limits,null)" : "") + ");");
  }
  line("    return composer.Finish(value.Extensions);");
  line("  }");
  line("  public static void Write(global::System.IO.Stream destination," + input + " value," + api("Budget") + "? budget=null){budget?" "?=new();" + helper + ".Write(destination,Encode(value,budget),budget);}");
  line("  public static " + api("Result") + "<" + value + "> Load(object source," + api("Budget") + "? budget=null," + api("ExtensionPolicy") + "? policy=null,global::System.Action<" + value + "," + api("Budget") + ">? validate=null) {");
  line("    budget?" "?=new();using var loader=new " + api("Loader") + "(source,budget);var value=new " + value + "();");
  index = 0;
  for (const auto* part : parts) {
    line("    value." + render_field(part->name) + "=loader." + std::string{part->value_type ?
        (singleton(*part) ? "Single" : "Models") : "Binaries"} + "(Role" + std::to_string(index++) +
        (part->value_type ? ",static (bytes,limits)=>" + render_type(*part->value_type) + ".DecodeXml(bytes,limits,null)" : "") + ");");
  }
  line("    " + references_method + "(value,budget,loader.Location);");
  line("    return loader.Finish(value,policy,validate,static (candidate,extensions)=>candidate.Extensions=extensions);");
  line("  }");
  line("  public static void Save(string path," + input + " value," + api("Budget") + "? budget=null,global::System.Action<" + value + "," + api("Budget") + ">? validate=null,global::System.Action<string>? fault=null){budget?" "?=new();byte[] bytes=Encode(value,budget);Load(bytes,budget,new(" + api("Unknown") + ".Preserve," + api("Unknown") + ".Preserve),validate);" + helper + ".Save(path,bytes,budget,fault);}");
  line("}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
