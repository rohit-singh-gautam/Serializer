// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "package_writer.hpp"

#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace rohit::serializer::writer::package_codegen {
// Bind standalone Swift ZIP support to independently collision-selected runtime names.
inline std::string swift_runtime(const std::string& helper, const std::string& compression, bool opc = false) {
  constexpr std::string_view source =
#include "swift_package_runtime.inc"
      ;
  constexpr std::string_view opc_source =
#include "swift_package_uri_runtime.inc"
#include "swift_package_mce_runtime.inc"
#include "swift_package_opc_runtime.inc"
#include "swift_package_profile_runtime.inc"
      ;
  std::string result{source};
  if (opc) { result += opc_source; }
  for (const auto& [marker, value] : {
           std::pair<std::string_view, std::string_view>{"@package_name@", helper},
           {"@compression_name@", compression}}) {
    for (std::size_t position{}; (position = result.find(marker, position)) != std::string::npos;) {
      result.replace(position, marker.size(), value);
      position += value.size();
    }
  }
  return "// Generated Swift package helper name: " + helper + '\n' + result;
}

// Emit owning Swift package aggregates with immutable model identity and checked role profiles.
inline std::string swift(const package_node& package, const std::string& name,
    const std::string& helper,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal, const xml_options& xml = {}) {
  const auto parts = ordered(package);
  const auto contract = compile_package_profile(package, xml);
  std::set<std::string> fields{};
  std::set<std::pair<std::string, std::string>> dispatch;
  for (const auto* part : parts) {
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Swift package field or dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated Swift package roles require dynamic names and generated IDs"};
    }
    if (part->value_type && part->codec != "xml") {
      throw std::invalid_argument{"Swift typed package roles require XML"};
    }
  }
  // Add metadata properties without excluding any previously valid schema role name.
  const auto helper_field = [&](const std::string& base) {
    auto result = base;
    std::size_t suffix{1U};
    while (fields.contains(result)) { result = base + '_' + std::to_string(++suffix); }
    fields.insert(result);
    return result;
  };
  const auto extensions_field = helper_field("extensions");
  const auto profiles_field = helper_field("profiles");
  std::string output;
  const auto line = [&](const std::string& value = {}) { output += value + '\n'; };
  const auto api = [&](std::string_view type) { return helper + '.' + std::string{type}; };
  const std::string input = name == "Input" ? "Input_2" : "Input";
  line("/// Generated owning OPC aggregate; schema roles select structure and callers supply values.");
  line("public enum " + name + " {");
  line("    public struct " + input + " {");
  for (const auto* part : parts) {
    const auto type = singleton(*part) ? render_type(*part->value_type) + "?" :
        "[" + (part->value_type ? api("ModelPart") + '<' + render_type(*part->value_type) + '>' : api("BinaryPart")) + ']';
    line("        public var " + render_field(part->name) + ": " + type + " = " +
        (singleton(*part) ? "nil" : "[]"));
  }
  line("        public var " + extensions_field + " = " + api("Extensions") + "()");
  line("        public var " + profiles_field + ": [String:" + api("Compression") + "] = [:]");
  line("        /// Construct an empty aggregate; occurrence validation precedes publication.");
  line("        public init() {}");
  line("    }");
  std::size_t index{};
  for (const auto* part : parts) {
    const auto optional = [&](const auto& item) { return item ? literal(*item) : "nil"; };
    const auto profile = std::find_if(contract.parts.begin(), contract.parts.end(),
        [&](const auto& item) { return item.role == part->name; });
    const auto expanded = [&](const auto& selected) {
      return literal(selected.namespace_uri.empty() ? selected.local : "{" + selected.namespace_uri + "}" + selected.local);
    };
    line("    private static let role" + std::to_string(index++) + " = " + api("Role") + "(" +
        literal(part->name) + ", name:" + optional(part->part_name) + ", contentType:" + optional(part->content_type) +
        ", source:" + literal(part->source) + ", relationship:" + literal(part->relationship) +
        ", id:" + optional(part->relationship_id) + ", compression:." + (part->compression == "deflate" ? "deflate" : "stored") +
        ", minimum:" + std::to_string(part->min_occurs) + ", maximum:" + (part->max_occurs ? std::to_string(*part->max_occurs) : "nil") +
        ", fixed:" + (part->profile_policy == "fixed" ? "true" : "false") +
        ", expectedRoot:" + (profile->decoder ? expanded(profile->decoder->root) : "nil") +
        ", expectedType:" + (profile->decoder && profile->decoder->type ? expanded(*profile->decoder->type) : "nil") + ")");
  }
  line("    /// Validate complete typed keys and references before aggregate or archive publication.");
  line("    private static func references(_ value: " + input + ", _ budget: " + api("Budget") + ", _ loader: " + api("Loader") + "? = nil) throws {");
  line("        let registry = " + api("References") + "(budget)");
  for (const auto* part : parts) {
    if (part->keys.empty() && part->references.empty()) { continue; }
    const auto field = render_field(part->name);
    line(singleton(*part) ? "        if let model = value." + field + " {" :
        "        for (index,item) in value." + field + ".enumerated() { let model = item.value");
    const auto location = singleton(*part) ? literal(part->part_name.value_or("")) : "item.name";
    const auto occurrence = singleton(*part) ? "0" : "index";
    line("            let location = loader?.location(" + literal(part->name) + "," + occurrence + ")");
    line("            let part = location?.part ?? " + location);
    line("            let link = location?.relationshipId ?? " + (part->relationship_id ? literal(*part->relationship_id) :
        literal("srl_" + part->name + "_") + " + String(" + occurrence + ")"));
    for (const auto& key : part->keys) {
      line("            try registry.add(" + literal(part->value_type->resolved_node->get_full_name()) + ", scope:" + literal(key.scope) +
          ", value:String(model." + render_field(key.field) + "), part:part, field:" + literal(part->name + '.' + key.field) + ", relationshipId:link)");
    }
    for (const auto& reference : part->references) {
      const auto target = std::find_if(parts.begin(), parts.end(),
          [&](const auto* item) { return item->name == reference.target_role; });
      line("            try registry.require(" + literal((*target)->value_type->resolved_node->get_full_name()) + ", scope:" + literal(reference.scope) +
          ", value:String(model." + render_field(reference.field) + "), part:part, field:" + literal(part->name + '.' + reference.field) + ", relationshipId:link)");
    }
    line("        }");
  }
  line("        try registry.finish()");
  line("    }");
  line("    /// Encode complete schema-bound values using one cumulative package/XML/reference ledger.");
  line("    public static func encode(_ value: " + input + ", budget selected: " + api("Budget") + "? = nil) throws -> [UInt8] {");
  line("        let budget = try selected ?? " + api("Budget") + "(); try references(value,budget)");
  line("        let composer = " + api("Composer") + "(budget:budget,profiles:value." + profiles_field + ")");
  index = 0;
  for (const auto* part : parts) {
    line("        try composer." + std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} +
        "(role" + std::to_string(index++) + ",value." + render_field(part->name) + (part->value_type ?
        ") { model,context in try model.encodeXml(" + api("xmlLimits") + "(context)) }" : ")"));
  }
  line("        return try composer.finish(value." + extensions_field + ")");
  line("    }");
  line("    /// Decode into an unpublished local aggregate; a middle codec/reference failure returns no value.");
  line("    public static func decode(_ bytes: [UInt8], budget selected: " + api("Budget") + "? = nil, policy: " + api("ExtensionPolicy") + " = " + api("ExtensionPolicy") + "()) throws -> " + input + " {");
  line("        let budget = try selected ?? " + api("Budget") + "(); let loader = try " + api("Loader") + "(bytes,budget:budget); defer { loader.close() }");
  line("        var result = " + input + "()");
  index = 0;
  for (const auto* part : parts) {
    line("        result." + render_field(part->name) + " = try loader." +
        std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} +
        "(role" + std::to_string(index++) + (part->value_type ? ") { bytes,context in try " + render_type(*part->value_type) +
        ".decodeXml(bytes," + api("xmlLimits") + "(context)) }" : ")"));
  }
  line("        try references(result,budget,loader); result." + extensions_field + " = try loader.finish(policy); try budget.check(); return result");
  line("    }");
  line("    /// Validate and stage a complete package before same-directory atomic replacement.");
  line("    public static func save(_ value: " + input + ", to path: Foundation.URL, budget selected: " + api("Budget") + "? = nil) throws {");
  line("        let budget = try selected ?? " + api("Budget") + "(); let bytes = try encode(value,budget:budget); try " + api("saveOpc") + "(bytes,to:path,budget:budget)");
  line("    }");
  line("}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
