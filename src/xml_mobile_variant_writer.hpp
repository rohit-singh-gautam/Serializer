// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_container_contract.hpp"
#include "native_schema.hpp"

#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_codegen {
using mobile_variant_name = std::function<std::string(const xml::name&)>;
using mobile_variant_type = std::function<std::string(const type_name&)>;
using mobile_variant_scalar = std::function<std::string(const type_name&, const std::string&, bool)>;

// Write exactly one native alternative inside its dedicated wrapper, with optional type metadata.
inline std::string mobile_variant_write(const xml_binding::variant_contract& variant,
    const std::string& access, const mobile_variant_scalar& scalar,
    const mobile_variant_name& name, bool swift) {
  std::string result;
  // Keep target statements independent of generator indentation and private member paths.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  const bool typed = variant.selection == xml_binding::variant_selection::xsi_type;
  const std::string attempt = swift ? "try " : "";
  line(swift ? "do {" : "run {");
  line(attempt + "output.open(" + name(variant.wrapper) + ")");
  line(swift ? "switch " + access + "Index {" : "when (" + access + "Index) {");
  for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
    const auto& alternative = variant.alternatives[index];
    const auto selected = access + native::pascal(alternative.alias);
    const auto& payload_name = typed ? variant.wrapper : alternative.tag;
    line(swift ? "case " + std::to_string(index) + ":" : std::to_string(index) + " -> {");
    if (alternative.native_type.type == object_type::class_type) {
      line(attempt + selected + ".srlXmlEmit(output, " + name(payload_name) +
          (typed ? ", " + name(alternative.type_identity) : "") + ")");
    } else {
      line(attempt + "output.open(" + name(payload_name) + ")");
      if (typed) { line(attempt + "output.typeName(" + name(alternative.type_identity) + ")"); }
      line(attempt + "output.text(" + scalar(alternative.native_type, selected, true) + ")");
      line(attempt + "output.close()");
    }
    if (!swift) { line("}"); }
  }
  line(swift ? "default: throw SerializerError.invalid(\"Invalid XML variant index\")" :
      "else -> throw IllegalArgumentException(\"Invalid XML variant index\")");
  line("}\n" + attempt + "output.close()\n}");
  return result;
}

// Read a complete Kotlin alternative from the active pull event without retaining XML nodes.
inline std::string kotlin_variant_read(const xml_binding::variant_contract& variant,
    const std::string& access, const mobile_variant_type& type,
    const mobile_variant_scalar& scalar, const mobile_variant_name& name) {
  std::string result;
  // Append one complete statement while the wrapper owns the active selection scope.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  const bool typed = variant.selection == xml_binding::variant_selection::xsi_type;
  line("run {\nval wrapper = input.open(" + name(variant.wrapper) + ")\nwrapper.requireNoAttributes()\ninput.skipSpace()");
  line("require(input.kind == 's') { \"Missing XML variant payload\" }");
  if (typed) { line("require(input.has(" + name(variant.wrapper) + ")) { \"Conflicting XML variant payload element\" }"); }
  line("when (" + std::string{typed ? "input.typeIdentity(true)" : "input.name()"} + ") {");
  for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
    const auto& alternative = variant.alternatives[index];
    const auto& payload_name = typed ? variant.wrapper : alternative.tag;
    line(name(typed ? alternative.type_identity : alternative.tag) + " -> {");
    if (!typed) { line("input.expectType(" + name(alternative.type_identity) + ", false)"); }
    const auto decoded = alternative.native_type.type == object_type::class_type ?
        type(alternative.native_type) + ".srlXmlRead(input, " + name(payload_name) + ")" :
        scalar(alternative.native_type, "input.scalar(" + name(payload_name) + ")", false);
    line(access + native::pascal(alternative.alias) + " = " + decoded);
    line(access + "Index = " + std::to_string(index) + "\n}");
  }
  line("else -> throw IllegalArgumentException(\"Unknown XML variant payload\")\n}\nwrapper.finish()\n}");
  return result;
}

// Build one Swift wrapper frame which admits exactly one known tag/type pair into its owner.
inline std::string swift_variant_frame(const xml_binding::variant_contract& variant,
    const std::string& access, const mobile_variant_type& type,
    const mobile_variant_scalar& scalar, const mobile_variant_name& name) {
  std::string result;
  // Keep the selected payload private until both it and the wrapper have completed.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  const bool typed = variant.selection == xml_binding::variant_selection::xsi_type;
  line("guard childAttributes.isEmpty else { throw SerializerError.invalid(\"Unexpected XML variant wrapper attribute\") }");
  line("let wrapper = SrlXmlTokenFrame(childName)\nvar selected = false\nwrapper.child = { payloadName, payloadAttributes in");
  line("guard !selected else { throw SerializerError.invalid(\"Multiple XML variant payloads\") }");
  if (typed) { line("guard payloadName == " + name(variant.wrapper) + " else { throw SerializerError.invalid(\"Conflicting XML variant payload element\") }"); }
  line("let metadata = try srlXmlVariantType(payloadAttributes, " + std::string{typed ? "true" : "false"} + ")");
  for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
    const auto& alternative = variant.alternatives[index];
    line("if " + std::string{typed ? "metadata.identity" : "payloadName"} + " == " +
        name(typed ? alternative.type_identity : alternative.tag) + " {");
    if (!typed) {
      line("guard metadata.identity == nil || metadata.identity == " + name(alternative.type_identity) +
          " else { throw SerializerError.invalid(\"Conflicting XML variant type\") }");
    }
    line("selected = true");
    if (alternative.native_type.type == object_type::class_type) {
      line("return try " + type(alternative.native_type) + ".srlXmlFrame(payloadName, metadata.attributes, mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions, listLimit) { item in");
    } else {
      line("return try srlXmlScalarFrame(payloadName, metadata.attributes) { text in\nlet item = " +
          scalar(alternative.native_type, "text", false));
    }
    line(access + native::pascal(alternative.alias) + " = item\n" + access + "Index = " + std::to_string(index) + "\n}\n}");
  }
  line("throw SerializerError.invalid(\"Unknown XML variant payload\")\n}\nwrapper.finish = { guard selected else { throw SerializerError.invalid(\"Missing XML variant payload\") } }\nreturn wrapper");
  return result;
}

} // namespace rohit::serializer::writer::xml_codegen
