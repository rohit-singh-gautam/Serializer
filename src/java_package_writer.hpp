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
    const std::function<std::string(std::string_view)>& literal) {
  const auto parts = ordered(package);
  std::set<std::string> fields{"extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  for (const auto* part : parts) {
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Java package field or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated package roles require dynamic names and generated relationship IDs"};
    }
    if (part->value_type && part->codec != "xml") {
      throw std::invalid_argument{"Java typed package roles currently require the XML codec"};
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
    const auto optional = [&](const auto& item) { return item ? literal(*item) : "null"; };
    line("  private static final " + api("Role") + " role" + std::to_string(index++) + " = new " + api("Role") + "(" +
        literal(render_field(part->name)) + "," + optional(part->part_name) + "," + optional(part->content_type) + "," +
        literal(part->source == "/" ? "/" : render_field(part->source)) + "," + literal(part->relationship) + "," +
        optional(part->relationship_id) + "," + literal(part->compression) + "," + std::to_string(part->min_occurs) + "L," +
        (part->max_occurs ? std::to_string(*part->max_occurs) : "-1") + "L);");
  }
  line("  /** Return an owning archive after complete declared-role validation. */");
  line("  public static byte[] encode(" + input + " value) throws java.io.IOException { return encode(value,new " + api("Budget") + "()); }");
  line("  /** Share one cumulative package budget through all composition stages. */");
  line("  public static byte[] encode(" + input + " value," + api("Budget") + " budget) throws java.io.IOException {");
  line("    java.util.Objects.requireNonNull(value);var composer=new " + api("Composer") + "(budget);");
  index = 0;
  for (const auto* part : parts) {
    const auto field = render_field(part->name);
    line("    composer." + std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} +
        "(role" + std::to_string(index++) + ",value." + field +
        (part->value_type ? ",(model,limits)->model.encodeXml(limits,null)" : "") + ");");
  }
  line("    return composer.finish(value.extensions);");
  line("  }");
  line("  /** Borrow a sink only after package preparation; a sink error may leave a prefix. */");
  line("  public static void write(java.io.OutputStream destination," + input + " value," + api("Budget") + " budget) throws java.io.IOException { " + helper + ".write(destination,encode(value,budget),budget); }");
  line("  /** Load one complete fresh aggregate; unknown capabilities reject by default. */");
  line("  public static " + api("Result") + "<" + value + "> load(Object source) throws java.io.IOException { return load(source,new " + api("Budget") + "(),null,null); }");
  line("  /** Apply explicit extension policy and complete-aggregate domain validation before publication. */");
  line("  public static " + api("Result") + "<" + value + "> load(Object source," + api("Budget") + " budget," + api("ExtensionPolicy") + " policy," + api("Validator") + "<" + value + "> validate) throws java.io.IOException {");
  line("    try(var loader=new " + api("Loader") + "(source,budget)){var value=new " + value + "();");
  index = 0;
  for (const auto* part : parts) {
    line("      value." + render_field(part->name) + "=loader." +
        std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} + "(role" + std::to_string(index++) +
        (part->value_type ? ",(bytes,limits)->" + render_type(*part->value_type) + ".decodeXml(bytes,limits,null)" : "") + ");");
  }
  line("      return loader.finish(value,policy,validate,(candidate,extensions)->candidate.extensions=extensions);");
  line("    }");
  line("  }");
  line("  /** Validate a staged round-trip before creating any private sibling or replacing the final file. */");
  line("  public static void save(java.nio.file.Path path," + input + " value," + api("Budget") + " budget," + api("Validator") + "<" + value + "> validate," + api("SaveFault") + " fault) throws java.io.IOException {");
  line("    byte[] bytes=encode(value,budget);load(bytes,budget,new " + api("ExtensionPolicy") + "(" + api("Unknown") + ".PRESERVE," + api("Unknown") + ".PRESERVE,null,null),validate);" + helper + ".save(path,bytes,budget,fault);");
  line("  }");
  line("}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
