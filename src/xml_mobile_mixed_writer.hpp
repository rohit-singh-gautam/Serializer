// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_content_contract.hpp"

#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_codegen {
using mobile_mixed_name = std::function<std::string(const xml::name&)>;

// Emit authored native fragments in order, charging even empty adjacent text fragments.
inline std::string mobile_mixed_write(const xml_binding::mixed_contract& mixed,
    const std::string& access, const std::string& variant, const mobile_mixed_name& name, bool swift) {
  std::string result;
  // Keep native syntax separate from the shared ordered-content contract.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  const std::string attempt = swift ? "try " : "";
  line(swift ? "do {" : "run {");
  line(swift ? "let mixed = SrlXmlMixedBudget(output.mixedLimits)" : "val mixed = SrlXmlMixedBudget(output.mixedLimits,output.packageContext())");
  if (mixed.source->fixed_extent != 0U) {
    line(swift ? "guard " + access + ".count == " + std::to_string(mixed.source->fixed_extent) +
        " else { throw SerializerError.invalid(\"XML mixed fixed extent mismatch\") }" :
        "require(" + access + ".size == " + std::to_string(mixed.source->fixed_extent) + ") { \"XML mixed fixed extent mismatch\" }");
  }
  line(swift ? "for fragment in " + access + " {" : "for (fragment in " + access + ") {");
  line(attempt + "mixed.fragment()");
  line(swift ? "switch fragment." + variant + "Index {" : "when (fragment." + variant + "Index) {");
  if (!mixed.element_only) {
    line(swift ? "case " + std::to_string(mixed.text_alternative) + ":" : std::to_string(mixed.text_alternative) + " -> {");
    const auto text = "fragment." + variant + native::pascal(mixed.text_branch);
    line(attempt + "mixed.text(" + text + ")\n" + attempt + "output.text(" + text + ")");
    if (!swift) { line("}"); }
  }
  for (const auto& child : mixed.children) {
    line(swift ? "case " + std::to_string(child.alternative) + ":" : std::to_string(child.alternative) + " -> {");
    line(attempt + "fragment." + variant + native::pascal(child.branch) + ".srlXmlEmit(output, " + name(child.qualified_name) + ")");
    if (!swift) { line("}"); }
  }
  line(swift ? "default: throw SerializerError.invalid(\"Invalid XML mixed alternative\")" :
      "else -> throw IllegalArgumentException(\"Invalid XML mixed alternative\")");
  return result + "}\n}\n}\n";
}

// Read logical text runs and typed children directly into Kotlin's private fragment sequence.
inline std::string kotlin_mixed_read(const xml_binding::mixed_contract& mixed,
    const std::string& access, const std::string& fragment_type, const std::string& variant,
    const mobile_mixed_name& name, const std::function<std::string(const type_name&)>& type) {
  std::string result;
  // Keep only one active text run and the final native sequence in memory.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  line("run {\nval mixed = SrlXmlMixedBudget(input.mixedLimits,input.packageContext())\nval values = mutableListOf<" + fragment_type + ">()");
  if (mixed.element_only) {
    line("while (true) {\ninput.skipSpace()\nif (input.kind != 's') { break }\nwhen (input.name()) {");
  } else {
    line("while (true) {\nif (input.kind == 't') {\nval text = input.mixedText(mixed)\nif (text.isNotEmpty()) {\nmixed.fragment()");
    line("val fragment = " + fragment_type + "()\nfragment." + variant + "Index = " + std::to_string(mixed.text_alternative));
    line("fragment." + variant + native::pascal(mixed.text_branch) + " = text\nvalues.add(fragment)\n}\ncontinue\n}\nif (input.kind != 's') { break }\nwhen (input.name()) {");
  }
  for (const auto& child : mixed.children) {
    line(name(child.qualified_name) + " -> {\nmixed.fragment()\nval fragment = " + fragment_type + "()");
    line("fragment." + variant + "Index = " + std::to_string(child.alternative));
    line("fragment." + variant + native::pascal(child.branch) + " = " + type(*child.value_type) +
        ".srlXmlRead(input, " + name(child.qualified_name) + ")\nvalues.add(fragment)\n}");
  }
  line("else -> throw IllegalArgumentException(\"Unknown XML mixed child\")\n}\n}");
  if (mixed.source->fixed_extent != 0U) { line("require(values.size == " + std::to_string(mixed.source->fixed_extent) + ") { \"XML mixed fixed extent mismatch\" }"); }
  return result + access + " = values\n}\n";
}

// Install Swift callbacks which coalesce provider text chunks at typed child boundaries.
inline std::string swift_mixed_frame(const xml_binding::mixed_contract& mixed,
    const std::string& access, const std::string& fragment_type, const std::string& variant,
    const mobile_mixed_name& name, const std::function<std::string(const type_name&)>& type) {
  std::string result;
  // Retain only one logical text run beside the completed native fragment values.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  line("let mixed = SrlXmlMixedBudget(mixedLimits)\nvar values: [" + fragment_type + "] = []");
  if (mixed.element_only) {
    line("frame.text = { chunk in\nguard srlXmlWhitespace(chunk) else { throw SerializerError.invalid(\"Text in element-only XML content\") }\n}");
    line("frame.child = { childName, childAttributes in");
  } else {
    line("var text = \"\"\nvar pending = false");
    line("let flushText = { () -> Void in\nif pending {\nvar fragment = " + fragment_type + "()");
    line("fragment." + variant + "Index = " + std::to_string(mixed.text_alternative));
    line("fragment." + variant + native::pascal(mixed.text_branch) + " = text\nvalues.append(fragment)\ntext = \"\"\npending = false\n}\n}");
    line("frame.text = { chunk in\nguard !chunk.isEmpty else { return }\nif !pending { try mixed.fragment(); pending = true }\ntry mixed.text(chunk)\ntext.append(contentsOf: chunk)\n}");
    line("frame.child = { childName, childAttributes in\nflushText()");
  }
  for (const auto& child : mixed.children) {
    line("if childName == " + name(child.qualified_name) + " {\ntry mixed.fragment()");
    line("return try " + type(*child.value_type) + ".srlXmlFrame(childName, childAttributes, mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions, listLimit) { item in\nvar fragment = " + fragment_type + "()");
    line("fragment." + variant + "Index = " + std::to_string(child.alternative));
    line("fragment." + variant + native::pascal(child.branch) + " = item\nvalues.append(fragment)\n}\n}");
  }
  line("throw SerializerError.invalid(\"Unknown XML mixed child\")\n}\nframe.finish = {");
  if (!mixed.element_only) { line("flushText()"); }
  if (mixed.source->fixed_extent != 0U) { line("guard values.count == " + std::to_string(mixed.source->fixed_extent) + " else { throw SerializerError.invalid(\"XML mixed fixed extent mismatch\") }"); }
  return result + access + " = values\ntry complete(state.value)\n}\n";
}

} // namespace rohit::serializer::writer::xml_codegen
