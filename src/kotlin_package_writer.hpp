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
// Render the XML-independent Kotlin/JVM package support under a collision-selected owner.
inline std::string kotlin_runtime(const std::string& helper, bool opc = false) {
  constexpr std::string_view zip_source =
#include "kotlin_package_runtime.inc"
#include "kotlin_package_zip_runtime.inc"
#include "kotlin_package_writer_runtime.inc"
  ;
  constexpr std::string_view opc_source =
#include "kotlin_package_opc_runtime.inc"
#include "kotlin_package_xml_runtime.inc"
#include "kotlin_package_catalog_runtime.inc"
#include "kotlin_package_profile_runtime.inc"
  ;
  std::string result{zip_source};
  if (opc) { result += opc_source; }
  result += "}\n";
  constexpr std::string_view marker = "@package_name@";
  for (std::size_t position{}; (position = result.find(marker, position)) != std::string::npos;) {
    result.replace(position, marker.size(), helper);
    position += helper.size();
  }
  return result;
}

// Emit statically typed Kotlin access over dependency-ordered schema package roles.
inline std::string kotlin(const package_node& package, const std::string& name,
    const std::string& helper,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal) {
  const auto parts = ordered(package);
  std::set<std::string> fields{"extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  for (const auto* part : parts) {
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Kotlin package field or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated package roles require dynamic names and generated relationship IDs"};
    }
    if (part->value_type && part->codec != "xml") {
      throw std::invalid_argument{"Kotlin typed package roles require the XML codec"};
    }
  }
  std::string output;
  const auto line = [&](const std::string& value = {}) { output += value + '\n'; };
  const auto api = [&](std::string_view type) { return helper + "." + std::string{type}; };
  const std::string input = name == "Input" ? "Input_2" : "Input";
  const std::string value = name == "Value" ? "Value_2" : "Value";
  line("/** Complete schema-bound OPC profile; infrastructure metadata is compiler-owned. */");
  line("object " + name + " {");
  line("    /** Bind ordinary owning model/binary values for synchronous composition. */");
  line("    open class " + std::string{input} + " {");
  for (const auto* part : parts) {
    const auto type = singleton(*part) ? render_type(*part->value_type) + "?" :
        "List<" + (part->value_type ? api("ModelPart") + "<" + render_type(*part->value_type) + ">" : api("BinaryPart")) + ">";
    line("        var " + render_field(part->name) + ": " + type + " = " +
        (singleton(*part) ? "null" : "emptyList()"));
  }
  line("        var extensions = " + api("Extensions") + "()");
  line("    }");
  line("    /** Own an unpublished freshly decoded complete aggregate. */");
  line("    class " + std::string{value} + " internal constructor() : " + input + "()");
  std::size_t index{};
  for (const auto* part : parts) {
    const auto optional = [&](const auto& item) { return item ? literal(*item) : "null"; };
    line("    private val role" + std::to_string(index++) + " = " + api("Role") + "(" +
        literal(render_field(part->name)) + "," + optional(part->part_name) + "," + optional(part->content_type) + "," +
        literal(part->source == "/" ? "/" : render_field(part->source)) + "," + literal(part->relationship) + "," +
        optional(part->relationship_id) + "," + literal(part->compression) + "," + std::to_string(part->min_occurs) + "L," +
        (part->max_occurs ? std::to_string(*part->max_occurs) : "-1") + "L)");
  }
  line("    /** Bridge the shared package ledger into nested XML/provider/member admission. */");
  line("    private fun xmlLimits(context: " + api("CodecContext") + "): XmlLimits {");
  line("        val bytes = minOf(context.budget.configured().maxPartBytes,Int.MAX_VALUE.toLong()).toInt()");
  line("        val nodes = minOf(context.budget.remaining(" + api("Resource") + ".WORK_UNITS),Int.MAX_VALUE.toLong()).toInt()");
  line("        return XmlLimits(bytes,nodes,64,bytes).also { it.srlPackageContext = SrlXmlBudget.Operation(context::check,context::consume) }");
  line("    }");
  line("    /** Return an owning archive after all declared roles and preserved edges validate. */");
  line("    fun encode(value: " + std::string{input} + ", budget: " + api("Budget") + " = " + api("Budget") + "()): ByteArray {");
  line("        val composer = " + api("Composer") + "(budget)");
  index = 0;
  for (const auto* part : parts) {
    line("        composer." + std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} +
        "(role" + std::to_string(index++) + ",value." + render_field(part->name) +
        (part->value_type ? ") { model,context -> model.encodeXml(xmlLimits(context)) }" : ")"));
  }
  line("        return composer.finish(value.extensions)");
  line("    }");
  line("    /** Borrow a sink only after complete package preparation; transport errors may leave a prefix. */");
  line("    fun write(destination: java.io.OutputStream, value: " + std::string{input} + ", budget: " + api("Budget") + " = " + api("Budget") + "()) { " + helper + ".write(destination,encode(value,budget),budget) }");
  line("    /** Publish only a fresh complete aggregate after capability and optional domain validation. */");
  line("    fun load(source: Any, budget: " + api("Budget") + " = " + api("Budget") + "(), policy: " + api("ExtensionPolicy") + " = " + api("ExtensionPolicy") + "(), validate: ((" + value + "," + api("Budget") + ") -> Unit)? = null): " + api("Result") + "<" + value + "> {");
  line("        " + helper + ".Loader(source,budget).use { loader ->");
  line("            val value = " + std::string{value} + "()");
  index = 0;
  for (const auto* part : parts) {
    line("            value." + render_field(part->name) + " = loader." +
        std::string{part->value_type ? (singleton(*part) ? "single" : "models") : "binaries"} + "(role" + std::to_string(index++) +
        (part->value_type ? ") { bytes,context -> " + render_type(*part->value_type) + ".decodeXml(bytes,xmlLimits(context)) }" : ")"));
  }
  line("            return loader.finish(value,policy,validate) { candidate,extensions -> candidate.extensions = extensions }");
  line("        }");
  line("    }");
  line("    /** Validate the entire staged round trip before creating a sibling or replacing the destination. */");
  line("    fun save(path: java.nio.file.Path, value: " + std::string{input} + ", budget: " + api("Budget") + " = " + api("Budget") + "(), validate: ((" + value + "," + api("Budget") + ") -> Unit)? = null, fault: ((String) -> Unit)? = null) {");
  line("        val bytes = encode(value,budget)");
  line("        load(bytes,budget," + api("ExtensionPolicy") + "(" + api("Unknown") + ".PRESERVE," + api("Unknown") + ".PRESERVE),validate)");
  line("        " + helper + ".save(path,bytes,budget,fault)");
  line("    }");
  line("}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
