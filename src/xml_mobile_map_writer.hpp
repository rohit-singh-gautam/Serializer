// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_container_contract.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
using mobile_map_type = std::function<std::string(const type_name&, bool)>;
using mobile_map_scalar = std::function<std::string(const type_name&, const std::string&, bool, bool)>;
using mobile_map_name = std::function<std::string(const xml::name&)>;

// Emit one complete native map in typed canonical-key order without constructing XML nodes.
inline std::string mobile_map_write(const xml_binding::map_contract& map, const std::string& access,
    const mobile_map_scalar& scalar, const mobile_map_name& name, bool swift) {
  std::string result;
  // Append one source line while retaining target syntax independently of the C++ generator.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  const std::string attempt = swift ? "try " : "";
  line(swift ? "do {" : "run {");
  if (map.optional_container) {
    if (swift) { line("if let values = " + access + " {"); }
    else { line("val container = " + access + "\nif (container.isPresent) {\nval values = container.get()"); }
  } else { line((swift ? "let values = " : "val values = requireNotNull(") + access + (swift ? "" : ")")); }
  line(swift ? "guard values.count <= output.mapLimits.maxEntries else { throw SerializerError.invalid(\"XML map entry limit exceeded\") }" :
      "require(values.size <= output.mapLimits.maxEntries) { \"XML map entry limit exceeded\" }");
  line(attempt + "output.open(" + name(map.wrapper) + ")");
  std::string comparator;
  if (swift) {
    comparator = map.key_type.name == "bool" ? "!$0 && $1" : map.key_type.type == object_type::enum_type ? "$0.rawValue < $1.rawValue" : "$0 < $1";
    line("let keys = values.keys.sorted { " + comparator + " }\nvar keyBytes = 0\nfor key in keys {\nlet item = values[key]!");
  } else {
    comparator = map.key_type.name == "string" ? "srlCompareText(a, b)" : map.key_type.type == object_type::enum_type ? "a.ordinal.compareTo(b.ordinal)" : "a.compareTo(b)";
    line("for (key in values.keys) { requireNotNull(key) { \"Null XML map key\" } }");
    line("val keys = values.keys.sortedWith(Comparator { a, b -> " + comparator + " })\nvar keyBytes = 0\nfor (key in keys) {\nval item = requireNotNull(values[key]) { \"Null XML map value\" }");
  }
  line((swift ? "let keyText = " : "val keyText = ") + scalar(map.key_type, "key", true, true));
  line("keyBytes = " + attempt + "output.mapLimits.keyBytes(keyText, keyText, keyBytes)");
  line(attempt + "output.open(" + name(map.entry) + ")");
  if (map.key_placement == xml_binding::map_key_placement::attribute) {
    line(attempt + "output.attribute(SrlXmlAttribute(" + std::string{swift ? "name: " : ""} + name(map.key) + ", " + (swift ? "value: " : "") + "keyText))");
  } else {
    line(attempt + "output.open(" + name(map.key) + ")\n" + attempt + "output.text(keyText)\n" + attempt + "output.close()");
  }
  if (map.value_type.type == object_type::class_type) { line(attempt + "item.srlXmlEmit(output, " + name(map.value) + ")"); }
  else {
    line(attempt + "output.open(" + name(map.value) + ")\n" + attempt + "output.text(" + scalar(map.value_type, "item", true, false) + ")\n" + attempt + "output.close()");
  }
  line(attempt + "output.close()\n}\n" + attempt + "output.close()");
  if (map.optional_container) { line("}"); }
  line("}");
  return result;
}

// Read arbitrary key/value member order directly into a private Kotlin map candidate.
inline std::string kotlin_map_read(const xml_binding::map_contract& map, const std::string& access,
    const mobile_map_type& type, const mobile_map_scalar& scalar, const mobile_map_name& name) {
  std::string result;
  // Append one complete Kotlin statement without retaining schema data in generated code.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  line("run {");
  if (map.optional_container) {
    line(access + " = java.util.Optional.empty()\nif (input.has(" + name(map.wrapper) + ")) {");
  }
  line("val wrapper = input.open(" + name(map.wrapper) + ")\nwrapper.requireNoAttributes()");
  line("val values = linkedMapOf<" + type(map.key_type, true) + ", " + type(map.value_type, false) + ">()\nvar keyBytes = 0");
  line("while (input.has(" + name(map.entry) + ")) {\nrequire(values.size < input.mapLimits.maxEntries) { \"XML map entry limit exceeded\" }");
  line("val entry = input.open(" + name(map.entry) + ")\nvar key: " + type(map.key_type, true) + "? = null\nvar item: " + type(map.value_type, false) + "? = null\nvar keyLexical = \"\"\nvar hasKey = false\nvar hasValue = false");
  if (map.key_placement == xml_binding::map_key_placement::attribute) {
    line("keyLexical = entry.attribute(" + name(map.key) + ", false)!!\nkey = " + scalar(map.key_type, "keyLexical", false, true) + "\nhasKey = true");
  }
  line("entry.requireNoAttributes()\nwhile (true) {\ninput.skipSpace()\nif (input.kind != 's') { break }\nwhen (input.name()) {");
  if (map.key_placement == xml_binding::map_key_placement::element) {
    line(name(map.key) + " -> {\nrequire(!hasKey) { \"Duplicate XML map key member\" }\nkeyLexical = input.scalar(" + name(map.key) + ")\nkey = " + scalar(map.key_type, "keyLexical", false, true) + "\nhasKey = true\n}");
  }
  line(name(map.value) + " -> {\nrequire(!hasValue) { \"Duplicate XML map value member\" }");
  line("item = " + (map.value_type.type == object_type::class_type ? type(map.value_type, false) + ".srlXmlRead(input, " + name(map.value) + ")" :
      scalar(map.value_type, "input.scalar(" + name(map.value) + ")", false, false)) + "\nhasValue = true\n}");
  line("else -> throw IllegalArgumentException(\"Unexpected XML map member\")\n}\n}\nentry.finish()");
  line("require(hasKey && hasValue) { \"Incomplete XML map entry\" }\nval selectedKey = requireNotNull(key)\nval selectedItem = requireNotNull(item)");
  line("keyBytes = input.mapLimits.keyBytes(keyLexical, " + scalar(map.key_type, "selectedKey", true, true) + ", keyBytes)");
  line("require(!values.containsKey(selectedKey)) { \"Duplicate canonical XML map key\" }\nvalues[selectedKey] = selectedItem\n}\nwrapper.finish()");
  line(access + " = " + (map.optional_container ? "java.util.Optional.of(values)" : "values"));
  if (map.optional_container) { line("}"); }
  line("}");
  return result;
}

// Build bounded Swift callbacks for one map wrapper while retaining only active entry state.
inline std::string swift_map_frame(const xml_binding::map_contract& map, const std::string& access,
    const mobile_map_type& type, const mobile_map_scalar& scalar, const mobile_map_name& name) {
  std::string result;
  // Append one Swift callback statement to the private frame factory.
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  line("guard childAttributes.isEmpty else { throw SerializerError.invalid(\"Unexpected XML map wrapper attribute\") }");
  line("let mapFrame = SrlXmlTokenFrame(childName)\nvar values: [" + type(map.key_type, true) + ": " + type(map.value_type, false) + "] = [:]\nvar keyBytes = 0");
  line("mapFrame.child = { entryName, entryAttributes in\nguard entryName == " + name(map.entry) +
      " else { throw SerializerError.invalid(\"Unexpected XML map entry\") }\nguard values.count < mapLimits.maxEntries else { throw SerializerError.invalid(\"XML map entry limit exceeded\") }");
  line("let fields = SrlXmlInputAttributes(entryAttributes)\nvar key: " + type(map.key_type, true) + "? = nil\nvar item: " + type(map.value_type, false) + "? = nil\nvar keyLexical = \"\"\nvar hasKey = false\nvar hasValue = false");
  if (map.key_placement == xml_binding::map_key_placement::attribute) {
    line("keyLexical = try fields.take(" + name(map.key) + ")!\nkey = " + scalar(map.key_type, "keyLexical", false, true) + "\nhasKey = true");
  }
  line("try fields.finish()\nlet entryFrame = SrlXmlTokenFrame(entryName)\nentryFrame.child = { partName, partAttributes in");
  if (map.key_placement == xml_binding::map_key_placement::element) {
    line("if partName == " + name(map.key) + " {\nguard !hasKey else { throw SerializerError.invalid(\"Duplicate XML map key member\") }\nhasKey = true");
    line("return try srlXmlScalarFrame(partName, partAttributes) { text in\nkeyLexical = text\nkey = " + scalar(map.key_type, "text", false, true) + "\n}\n}");
  }
  line("if partName == " + name(map.value) + " {\nguard !hasValue else { throw SerializerError.invalid(\"Duplicate XML map value member\") }\nhasValue = true");
  if (map.value_type.type == object_type::class_type) {
    line("return try " + type(map.value_type, false) + ".srlXmlFrame(partName, partAttributes, mapLimits, mixedLimits, digestLimits, identityLimits, extensionOptions, listLimit) { item = $0 }");
  } else {
    line("return try srlXmlScalarFrame(partName, partAttributes) { text in item = " + scalar(map.value_type, "text", false, false) + " }");
  }
  line("}\nthrow SerializerError.invalid(\"Unexpected XML map member\")\n}");
  line("entryFrame.finish = {\nguard hasKey && hasValue, let selectedKey = key, let selectedItem = item else { throw SerializerError.invalid(\"Incomplete XML map entry\") }");
  line("keyBytes = try mapLimits.keyBytes(keyLexical, " + scalar(map.key_type, "selectedKey", true, true) + ", keyBytes)");
  line("guard values[selectedKey] == nil else { throw SerializerError.invalid(\"Duplicate canonical XML map key\") }\nvalues[selectedKey] = selectedItem\n}\nreturn entryFrame\n}");
  line("mapFrame.finish = { " + access + " = " + (map.optional_container ? "Optional.some(values)" : "values") + " }\nreturn mapFrame");
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
