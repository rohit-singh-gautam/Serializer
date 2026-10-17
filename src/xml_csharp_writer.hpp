// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_csharp {

// Supply whole-object XML runtime helpers inside the generated outer Schema container.
inline std::string_view runtime() {
  static constexpr std::string_view source =
#include "xml_csharp_runtime.inc"
      ;
  return source;
}

// Escape a checked contract spelling for a C# string literal without changing wire names.
inline std::string quote(std::string_view value) {
  std::string result{"\""};
  for (const auto character : value) {
    switch (character) {
    case '\\': result += "\\\\"; break;
    case '"': result += "\\\""; break;
    case '\n': result += "\\n"; break;
    case '\r': result += "\\r"; break;
    case '\t': result += "\\t"; break;
    default: result += character; break;
    }
  }
  return result + '"';
}

// Resolve the scalar or record stored beneath a supported optional or repeated field.
inline const type_name& leaf(const member& value) {
  const auto& selected = value.type_name_list.front();
  return selected.is_optional() || selected.is_sequence() ? selected.arguments.front() : selected;
}

// Emit exact static mappings inside one generated C# owning class; callbacks retain naming profiles.
inline std::string methods(const class_node&, const xml_binding::record& contract,
                           const xml_options& options, std::string_view model_name,
                           const std::function<std::string(const member&)>& field_name,
                           const std::function<std::string(const type_name&)>& native_type) {
  std::string source;
  std::size_t indent{2};
  // Retain a short native statement at the current class nesting level.
  const auto line = [&](const std::string& text) {
    source.append(indent * 2, ' '); source += text + '\n';
  };
  // Select a C# lexical conversion with fixed integer and enum constraints.
  const auto scalar = [&](const rohit::serializer::type_name& type,
                          const std::string& expression, bool encoding) {
    if (type.type == object_type::enum_type) {
      std::string result = "(" + (encoding ? "((int)" + expression + ")" : expression) + " switch { ";
      const auto& values = static_cast<const enum_node&>(*type.resolved_node).enum_name_list;
      for (std::size_t index{}; index < values.size(); ++index) {
        result += (encoding ? std::to_string(index) : quote(values[index])) + " => " +
            (encoding ? quote(values[index]) : "(" + native_type(type) + ")" + std::to_string(index)) + ", ";
      }
      return result + "_ => throw new System.FormatException(\"Invalid XML enum\") })";
    }
    if (type.name == "string") { return expression; }
    if (type.name == "bool") {
      return encoding ? "(" + expression + " ? \"true\" : \"false\")" : "SrlXmlBoolean(" + expression + ")";
    }
    if (type.name == "float" || type.name == "double") {
      const auto single = type.name == "float" ? "true" : "false";
      return encoding ? "SrlXmlFloatText(" + expression + ", " + single + ")" :
          "(" + native_type(type) + ")SrlXmlFloat(" + expression + ", " + single + ")";
    }
    if (encoding) { return expression + ".ToString(System.Globalization.CultureInfo.InvariantCulture)"; }
    const auto bits = type.name.ends_with("64") ? 64 : type.name.ends_with("32") ? 32 : type.name.ends_with("16") ? 16 : 8;
    return "(" + native_type(type) + ")SrlXmlInteger(" + expression + ", " + std::to_string(bits) + ", " +
        (type.name.starts_with('u') ? "true" : "false") + ")";
  };
  // Select stable expanded-name spelling independently of XML document prefixes.
  const auto expanded = [](const xml::name& name) {
    return quote(name.namespace_uri.empty() ? name.local : "{" + name.namespace_uri + "}" + name.local);
  };
  // The portable emitter validates normalized field, function, factory and conversion names.
  line("// Encode one complete XML document using its fixed generated mapping.");
  line("public byte[] EncodeXml() { return EncodeXml(new Limits(), null); }");
  line("// Bound output and optionally reuse an explicitly prepared runtime XSD.");
  line("public byte[] EncodeXml(Limits limits, XmlSchema? schema = null) {"); ++indent;
  line("var budget = new SrlXmlBudget(limits);");
  line("byte[] bytes = SrlXmlWrite(XmlNode(" + expanded(contract.root) + ", budget, 1), limits);");
  if (!options.runtime_xsd) { line("if (schema != null) throw new System.InvalidOperationException(\"Runtime XSD was not enabled during generation\");"); }
  else if (options.validation_scope != "input") { line("if (schema != null) schema.Validate(bytes, limits);"); }
  line("return bytes;"); --indent; line("}");
  line("// Decode exactly one XML document into a fresh unpublished owning value.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes) { return DecodeXml(bytes, new Limits(), null); }");
  line("// Enforce static shape, bounded parsing and selected optional runtime XSD validation.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes, Limits limits, XmlSchema? schema = null) {"); ++indent;
  if (!options.runtime_xsd) { line("if (schema != null) throw new System.InvalidOperationException(\"Runtime XSD was not enabled during generation\");"); }
  else if (options.validation_scope != "output") { line("if (schema != null) schema.Validate(bytes, limits);"); }
  line("var node = SrlXmlParse(bytes, limits);");
  line("if (node.Name != " + expanded(contract.root) + ") throw new System.FormatException(\"Unexpected XML root or namespace\");");
  line("return XmlFromNode(node);"); --indent; line("}");
  line("// Build direct typed fields under the same complete-document resource budget.");
  line("internal SrlXmlNode XmlNode(string name, SrlXmlBudget budget, int depth) {"); ++indent;
  line("var node = budget.Node(name, depth);");
  for (const auto& item : contract.fields) {
    const auto& value = *item.source;
    const auto& type = leaf(value);
    const auto access = "this." + field_name(value);
    const bool optional = value.type_name_list.front().is_optional();
    std::string selected = access;
    line("{"); ++indent;
    if (item.wrapper) {
      line("var wrapper = budget.Node(" + expanded(*item.wrapper) + ", depth + 1);");
      line("node.Children.Add(wrapper);");
    }
    const auto parent = item.wrapper ? std::string{"wrapper"} : std::string{"node"};
    const auto child_depth = item.wrapper ? "depth + 2" : "depth + 1";
    if (optional) { line("if (" + access + ".HasValue) {"); ++indent; selected += ".Value.Item1"; }
    if (item.repeated) {
      line("if (" + access + ".Count < " + std::to_string(item.min_occurs) +
           (item.max_occurs ? " || " + access + ".Count > " + std::to_string(*item.max_occurs) : "") +
           ") throw new System.FormatException(\"XML occurrence mismatch\");");
      if (value.fixed_extent) { line("if (" + access + ".Count != " + std::to_string(value.fixed_extent) + ") throw new System.FormatException(\"XML fixed extent mismatch\");"); }
      line("foreach (var item in " + access + ") {"); ++indent; selected = "item";
    }
    if (item.text) { line("node.Text.Append(budget.Text(" + scalar(type, selected, true) + "));"); }
    else if (item.attribute) { line("budget.Attribute(node, " + expanded(item.qualified_name) + ", " + scalar(type, selected, true) + ");"); }
    else if (type.type == object_type::class_type) { line(parent + ".Children.Add(" + selected + ".XmlNode(" + expanded(item.qualified_name) + ", budget, " + child_depth + "));"); }
    else {
      line("var child = budget.Node(" + expanded(item.qualified_name) + ", " + child_depth + ");");
      line("child.Text.Append(budget.Text(" + scalar(type, selected, true) + "));");
      line(parent + ".Children.Add(child);");
    }
    if (item.repeated) { --indent; line("}"); }
    if (optional) { --indent; line("}"); }
    --indent; line("}");
  }
  line("return node;"); --indent; line("}");
  line("// Decode exact members into an unpublished candidate and reject all unconsumed input.");
  line("internal static " + std::string{model_name} + " XmlFromNode(SrlXmlNode node) {"); ++indent;
  line("var fields = new SrlXmlFields(node, " + std::string{std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& item) { return item.text; }) ? "true" : "false"} + ");"); line("var result = new " + std::string{model_name} + "();");
  for (const auto& item : contract.fields) {
    if (item.min_occurs > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        (item.max_occurs && *item.max_occurs > static_cast<std::size_t>(std::numeric_limits<int>::max()))) {
      throw std::invalid_argument{"C# XML cardinality exceeds supported collection size"};
    }
    const auto& value = *item.source;
    const auto& type = leaf(value);
    const bool optional = value.type_name_list.front().is_optional();
    const auto access = "result." + field_name(value);
    const auto read_value = [&](const std::string& expression) {
      return type.type == object_type::class_type ? native_type(type) + ".XmlFromNode(" + expression + ")" :
          scalar(type, item.attribute || item.text ? expression : "SrlXmlScalarText(" + expression + ")", false);
    };
    line("{"); ++indent;
    if (item.text) {
      line(access + " = " + read_value("fields.Text()") + ";");
    } else if (item.attribute) {
      line("var text = fields.Attribute(" + expanded(item.qualified_name) + ", " + (optional ? "true" : "false") + ");");
      line(access + " = " + (optional ? "text == null ? null : new System.ValueTuple<" + native_type(type) + ">(" + read_value("text") + ")" : read_value("text!")) + ";");
    } else {
      line("var children = fields." + std::string{item.wrapper ? "Wrapped(" + expanded(*item.wrapper) + ", " : "Children("} + expanded(item.qualified_name) + ", " + std::to_string(item.min_occurs) + ", " +
           (item.max_occurs ? std::to_string(*item.max_occurs) : "-1") + ");");
      if (item.repeated) {
        if (value.fixed_extent) { line("if (children.Count != " + std::to_string(value.fixed_extent) + ") throw new System.FormatException(\"XML fixed extent mismatch\");"); }
        line(access + " = new System.Collections.Generic.List<" + native_type(type) + ">();");
        line("foreach (var child in children) " + access + ".Add(" + read_value("child") + ");");
      } else { line(access + " = " + (optional ? "children.Count == 0 ? null : new System.ValueTuple<" + native_type(type) + ">(" + read_value("children[0]") + ")" : read_value("children[0]")) + ";"); }
    }
    --indent; line("}");
  }
  line("fields.Finish();"); line("return result;"); --indent; line("}");
  return source;
}
} // namespace rohit::serializer::writer::xml_csharp
