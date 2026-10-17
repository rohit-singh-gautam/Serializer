// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_native_writer.hpp"
#include "xml_schema.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::serializer::writer::xml_codegen {
namespace {
constexpr std::string_view kotlin_runtime =
#include "xml_kotlin_runtime.inc"
;
constexpr std::string_view swift_runtime =
#include "xml_swift_runtime.inc"
;
constexpr std::string_view c_runtime =
#include "xml_c_runtime.inc"
;
// Emit reflection-free field bindings over native optional XML parsing providers.
class generator {
  const native::schema& model_;
  xml_binding::schema bindings_;
  const xml_options& options_;
  std::string language_;
  bool swift_;
  bool c_;
  std::string output_{};
  std::size_t indent_{};
  // Append one generated line at the active indentation.
  void line(std::string_view value = {}) { output_.append(indent_ * 4U, ' '); output_ += value; output_ += '\n'; }
  // Enter a source scope.
  void open(const std::string& value) { line(value + " {"); ++indent_; }
  // Complete a source scope.
  void close(std::string_view suffix = {}) { --indent_; line("}" + std::string{suffix}); }
  // Select target syntax without changing schema metadata.
  std::string select(std::string_view swift, std::string_view kotlin) const { return std::string{swift_ ? swift : kotlin}; }
  // Derive the native presentation spelling used by existing generators.
  std::string field_name(std::string_view value) const {
    if (c_) { return native::snake(value); }
    auto result = native::pascal(value);
    if (!result.empty()) { result.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(result.front()))); }
    return result;
  }
  // Preserve configured UTF-8 source literals and Kotlin interpolation characters.
  std::string quote(std::string_view value) const {
    auto result = native::quote(value);
    if (!swift_ && !c_) {
      std::size_t offset{};
      while ((offset = result.find('$', offset)) != std::string::npos) { result.insert(offset, "\\"); offset += 2U; }
    }
    return result;
  }
  // Return the scalar/record type under the supported presence/collection mapping.
  const type_name& leaf(const member& value) const {
    const auto& type = value.type_name_list.front();
    return type.is_optional() || type.is_sequence() ? type.arguments.front() : type;
  }
  // Resolve exact native model and scalar type names.
  std::string type(const type_name& value) const {
    if (value.type != object_type::primitive) { return c_ ? native::snake(model_.names.at(value.resolved_node)) : model_.names.at(value.resolved_node); }
    if (value.name == "string") { return c_ ? "srl_string" : "String"; }
    if (value.name == "bool") { return c_ ? "bool" : select("Bool", "Boolean"); }
    if (value.name == "float") { return c_ ? "float" : "Float"; }
    if (value.name == "double") { return c_ ? "double" : "Double"; }
    if (c_) { return value.name + "_t"; }
    if (swift_) { return std::string{value.name.starts_with('u') ? "UInt" : "Int"} + std::to_string(native::width(value) * 8); }
    return std::string{value.name.starts_with('u') ? "U" : ""} +
        (native::width(value) == 1 ? "Byte" : native::width(value) == 2 ? "Short" : native::width(value) == 4 ? "Int" : "Long");
  }
  // Emit a checked expanded-name literal for native XML trees.
  std::string name(const xml::name& value) const {
    return "SrlXmlName(" + select("local: ", "") + quote(value.local) + ", " + select("namespace: ", "") + quote(value.namespace_uri) + ")";
  }
  // Format scalar lexical text without escaping until the XML writer owns it.
  std::string scalar_text(const type_name& value, const std::string& expression) const {
    if (value.name == "string") { return expression; }
    if (value.type == object_type::enum_type) {
      return type(value) + ".srlNames[" + expression + select(".rawValue]", ".ordinal]");
    }
    if (value.name == "bool") { return swift_ ? "(" + expression + " ? \"true\" : \"false\")" : expression + ".toString()"; }
    if (value.name == "float" || value.name == "double") {
      return "srlXmlFloatText(" + expression + ")";
    }
    return swift_ ? "String(" + expression + ")" : expression + ".toString()";
  }
  // Decode one supported lexical scalar or nested record using exact range checks.
  std::string read_value(const type_name& value, const std::string& raw, bool attribute) const {
    if (value.type == object_type::class_type) { return select("try ", "") + type(value) + ".srlXmlRead(" + raw + ")"; }
    const auto text = attribute ? raw : select("try ", "") + "srlXmlText(" + raw + ")";
    if (value.name == "string") { return text; }
    if (value.type == object_type::enum_type) { return select("try ", "") + "srlXml" + type(value) + "Enum(" + text + ")"; }
    if (value.name == "bool") { return select("try ", "") + "srlXmlBoolean(" + text + ")"; }
    if (value.name == "float" || value.name == "double") {
      return select("try ", "") + (value.name == "float" ? "srlXmlFloat32(" : "srlXmlFloat(") + text + ")";
    }
    return swift_ ? "try srlXmlInteger(" + text + ", " + type(value) + ".self)" :
        "srlXmlIntegerText(" + text + ").to" + type(value) + "()";
  }
  // Add one supported scalar/record XML child or scalar attribute.
  void emit_mobile_value(const xml_binding::field& field, const std::string& expression) {
    const auto& value_type = leaf(*field.source);
    if (field.text) {
      line(select("try ", "") + "node.setValueText(" + scalar_text(value_type, expression) + ")");
      return;
    }
    if (field.attribute) {
      line(swift_ ? "try node.addAttribute(SrlXmlAttribute(name: " + name(field.qualified_name) + ", value: " + scalar_text(value_type, expression) + "))" :
          "node.addAttribute(SrlXmlAttribute(" + name(field.qualified_name) + ", " + scalar_text(value_type, expression) + "))");
      return;
    }
    const auto parent = field.wrapper ? std::string{"wrapper"} : std::string{"node"};
    line(select("let ", "val ") + "child = " + select("try ", "") + parent + ".makeChild(" + name(field.qualified_name) + ")");
    if (value_type.type == object_type::class_type) { line(select("try ", "") + expression + ".srlXmlFill(child)"); }
    else { line(select("try ", "") + "child.setValueText(" + scalar_text(value_type, expression) + ")"); }
    line(parent + select(".children.append(child)", ".children.add(child)"));
  }
  // Emit complete-object XML methods for one native Swift/Kotlin owning record.
  void mobile_record(const class_node& source, const xml_binding::record& contract) {
    const auto model_name = model_.names.at(&source);
    if (swift_) { open("extension " + model_name); }
    line("// Encode one complete object with the fixed XML root and field mapping.");
    const auto validator = options_.runtime_xsd ? select(", validator: (any XmlValidator)? = nil", ", validator: XmlValidator? = null") : "";
    open(swift_ ? "public func encodeXml(_ limits: XmlLimits = XmlLimits()" + validator + ") throws -> [UInt8]" :
        "fun " + model_name + ".encodeXml(limits: XmlLimits = XmlLimits()" + validator + "): ByteArray");
    line(select("let ", "val ") + "node = " + select("try ", "") + "SrlXmlNode.root(" + name(contract.root) + ", limits)");
    line(select("try self.srlXmlFill(node)", "this.srlXmlFill(node)"));
    line(select("let bytes = try srlXmlWrite(node, limits)", "val bytes = srlXmlWrite(node, limits)"));
    if (options_.runtime_xsd && options_.validation_scope != "input") {
      line(select("try validator?.validate(bytes, limits)", "validator?.validate(bytes, limits)"));
    }
    line("return bytes");
    close();
    line("// Parse and validate a fresh candidate before returning it to the caller.");
    open(swift_ ? "public static func decodeXml(_ bytes: [UInt8], _ limits: XmlLimits = XmlLimits()" + validator + ") throws -> " + model_name :
        "fun " + model_name + ".Companion.decodeXml(bytes: ByteArray, limits: XmlLimits = XmlLimits()" + validator + "): " + model_name);
    if (options_.runtime_xsd && options_.validation_scope != "output") {
      line(select("try validator?.validate(bytes, limits)", "validator?.validate(bytes, limits)"));
    }
    line(select("let ", "val ") + "node = " + select("try ", "") + "srlXmlParse(bytes, limits)");
    line(swift_ ? "guard node.name == " + name(contract.root) + " else { throw SerializerError.invalid(\"Unexpected XML root\") }" :
        "require(node.name == " + name(contract.root) + ") { \"Unexpected XML root\" }");
    line("return " + select("try ", "") + model_name + ".srlXmlRead(node)");
    close();
    line("// Fill statically bound attributes and child elements without reflection.");
    open(swift_ ? "fileprivate func srlXmlFill(_ node: SrlXmlNode) throws" :
        "internal fun " + model_name + ".srlXmlFill(node: SrlXmlNode)");
    if (contract.fields.empty()) { line(select("_ = node", "@Suppress(\"UNUSED_VARIABLE\") val unused = node")); }
    for (const auto& field : contract.fields) {
      const auto value = swift_ ? "self." + field_name(field.source->name) :
          "this.srlXmlField" + native::pascal(field.source->name);
      if (field.wrapper) {
        open(swift_ ? "do" : "run");
        line(select("let ", "val ") + "wrapper = " + select("try ", "") + "node.makeChild(" + name(*field.wrapper) + ")");
        line(select("node.children.append(wrapper)", "node.children.add(wrapper)"));
      }
      if (field.repeated) {
        const auto count = value + select(".count", ".size");
        if (field.source->fixed_extent != 0U) {
          line(swift_ ? "guard " + count + " == " + std::to_string(field.source->fixed_extent) + " else { throw SerializerError.invalid(\"XML fixed array size\") }" :
              "require(" + count + " == " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
        }
        if (field.min_occurs > 0U || field.max_occurs) {
          std::string condition = field.min_occurs > 0U ? count + " >= " + std::to_string(field.min_occurs) : "";
          if (field.max_occurs) { condition += (condition.empty() ? "" : " && ") + count + " <= " + std::to_string(*field.max_occurs); }
          line(swift_ ? "guard " + condition + " else { throw SerializerError.invalid(\"XSD occurrence bound\") }" : "require(" + condition + ") { \"XSD occurrence bound\" }");
        }
        open(swift_ ? "for item in " + value : "for (item in " + value + ")");
        emit_mobile_value(field, "item"); close();
      } else if (field.source->type_name_list.front().is_optional()) {
        open(swift_ ? "if let item = " + value : "if (" + value + ".isPresent)");
        if (!swift_) { line("val item = " + value + ".get()"); }
        emit_mobile_value(field, "item"); close();
      } else {
        open(swift_ ? "do" : "run"); emit_mobile_value(field, value); close();
      }
      if (field.wrapper) { close(); }
    }
    close();
    line("// Read every required/optional field and reject all remaining XML members.");
    open(swift_ ? "fileprivate static func srlXmlRead(_ node: SrlXmlNode) throws -> " + model_name :
        "internal fun " + model_name + ".Companion.srlXmlRead(node: SrlXmlNode): " + model_name);
    const auto has_text = std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; });
    line(select("let reader = try SrlXmlReader(node, ", "val reader = SrlXmlReader(node, ") + (has_text ? "true)" : "false)"));
    line(select("var ", "val ") + "result = " + model_name + "()");
    std::size_t index{};
    for (const auto& field : contract.fields) {
      const auto target = swift_ ? "result." + field_name(field.source->name) :
          "result.srlXmlField" + native::pascal(field.source->name);
      const auto& value_type = leaf(*field.source);
      const auto raw = "raw" + std::to_string(index++);
      if (field.text) {
        line(select("let ", "val ") + raw + " = " + select("try ", "") + "reader.text()");
        line(target + " = " + read_value(value_type, raw, true));
      } else if (field.repeated) {
        line(select("let ", "val ") + raw + " = " + std::string{field.wrapper ? select("try ", "") + "reader.wrapped(" + name(*field.wrapper) + ", " : "reader.repeated("} + name(field.qualified_name) + ")");
        if (field.source->fixed_extent != 0U) {
          line(swift_ ? "guard " + raw + ".count == " + std::to_string(field.source->fixed_extent) + " else { throw SerializerError.invalid(\"XML fixed array size\") }" :
              "require(" + raw + ".size == " + std::to_string(field.source->fixed_extent) + ") { \"XML fixed array size\" }");
        }
        std::string condition;
        if (field.min_occurs > 0U) { condition = raw + select(".count", ".size") + " >= " + std::to_string(field.min_occurs); }
        if (field.max_occurs) { condition += (condition.empty() ? "" : " && ") + raw + select(".count", ".size") + " <= " + std::to_string(*field.max_occurs); }
        if (!condition.empty()) { line(swift_ ? "guard " + condition + " else { throw SerializerError.invalid(\"XSD occurrence bound\") }" : "require(" + condition + ") { \"XSD occurrence bound\" }"); }
        auto expression = select("try ", "") + raw + select(".map { item in ", ".map { item -> ") + read_value(value_type, "item", false) + " }";
        if (!swift_) {
          const bool primitive_array = field.source->modifier == member::modifier_type::array &&
              value_type.type == object_type::primitive && value_type.name != "string";
          expression += primitive_array ? ".to" + type(value_type) + "Array()" : ".toMutableList()";
        }
        line(target + " = " + expression);
      } else {
        const bool optional = field.source->type_name_list.front().is_optional();
        const auto read = "reader." + std::string{field.attribute ? "attribute" : "child"} + "(" + name(field.qualified_name) + ", " + (optional ? "true" : "false") + ")";
        if (optional) {
          line(select("let ", "val ") + raw + " = " + select("try ", "") + read);
          if (swift_) {
            open("if let item = " + raw); line(target + " = " + read_value(value_type, "item", field.attribute)); close(" else {");
            ++indent_; line(target + " = nil"); --indent_; line("}");
          } else {
            line(target + " = " + raw + "?.let { java.util.Optional.of(" + read_value(value_type, "it", field.attribute) + ") } ?: java.util.Optional.empty()");
          }
        } else {
          line(select("let ", "val ") + raw + " = " + select("try ", "") + read + select("!", "!!"));
          line(target + " = " + read_value(value_type, raw, field.attribute));
        }
      }
    }
    line(select("try ", "") + "reader.finish()"); line("return result"); close();
    if (swift_) { close(); }
  }
  // Emit C's explicitly owned and transactional optional XML APIs.
  void c_record(const class_node& source, const xml_binding::record& contract);
public:
  // Bind the same checked XML contract across all native generation targets.
  generator(const native::schema& model, const xml_options& options, std::string_view language)
      : model_{model}, bindings_{options}, options_{options}, language_{language},
        swift_{language == "swift"}, c_{language == "c"} {}
  // Generate the provider adapter once and direct typed bindings for every selected record.
  std::string generate();
};

// Emit C provider calls and typed assignments without reflection or an intermediate codec.
void generator::c_record(const class_node& source, const xml_binding::record& contract) {
  const auto n = native::snake(model_.names.at(&source));
  const auto storage_type = [&](auto&& self, const type_name& value) -> std::string {
    if (value.is_optional() || value.is_sequence()) {
      auto result = "srl_" + value.name;
      for (const auto& item : value.arguments) {
        const auto inner = self(self, item);
        result += "_" + std::to_string(inner.size()) + "_" + inner;
      }
      return result;
    }
    return type(value);
  };
  const auto free_leaf = [&](const type_name& value, const std::string& target) {
    if (value.name == "string") { line("srl_string_free(&" + target + ");"); }
    else if (value.type == object_type::class_type) { line(type(value) + "_free(&" + target + ");"); }
  };
  const auto initialize_leaf = [&](const type_name& value, const std::string& target) {
    if (value.type == object_type::class_type) {
      line("if (" + type(value) + "_init(&" + target + ") != srl_ok) { return false; }");
    }
  };
  const auto emit_leaf = [&](const xml_binding::field& field, const std::string& expression, std::size_t prefix) {
    const auto& value_type = leaf(*field.source);
    if (value_type.type == object_type::class_type) {
      line("if (!" + type(value_type) + "_srl_xml_write(writer, out, &" + expression + ", " +
           quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + ")) { return false; }");
      return;
    }
    if (!field.attribute && !field.text) {
      line("if (++out->nodes > out->limits.max_elements || out->writer.depth >= out->limits.max_depth || xmlTextWriterStartElementNS(writer, " +
           std::string{field.qualified_name.namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ", BAD_CAST " + quote(field.qualified_name.local) + ", " +
           (field.qualified_name.namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(field.qualified_name.namespace_uri)) + ") < 0) { return false; }");
    }
    if (value_type.name == "string") {
      line("const char* text = " + expression + ".data ? " + expression + ".data : \"\"; const size_t text_size = " + expression + ".size;");
    } else if (value_type.type == object_type::enum_type) {
      const auto count = static_cast<const enum_node&>(*value_type.resolved_node).enum_name_list.size();
      line("if ((unsigned)" + expression + " >= " + std::to_string(count) + "u) { return false; }");
      line("const char* text = " + type(value_type) + "_srl_names[(unsigned)" + expression + "]; const size_t text_size = strlen(text);");
    } else if (value_type.name == "bool") {
      line("const char* text = " + expression + " ? \"true\" : \"false\"; const size_t text_size = strlen(text);");
    } else if (value_type.name == "float" || value_type.name == "double") {
      line("srl_writer number = {0}; number.protocol = srl_json;");
      line("if (isnan(" + expression + ")) { srl_raw(&number, \"NaN\"); } else if (isinf(" + expression + ")) { srl_raw(&number, " + expression + " < 0 ? \"-INF\" : \"INF\"); } else { srl_write_float(&number, (double)" + expression + ", " + (value_type.name == "float" ? "true" : "false") + "); }");
      line("srl_put_byte(&number, 0); if (number.status != srl_ok) { srl_buffer_free(&number.buffer); return false; }");
      line("const char* text = (const char*)number.buffer.data; const size_t text_size = number.buffer.size-1;");
    } else {
      line("char number[srl_number_scratch_bytes]; const int written = snprintf(number, sizeof(number), " +
           std::string{value_type.name.starts_with('u') ? "\"%\" PRIu64, (uint64_t)" : "\"%\" PRId64, (int64_t)"} + expression + ");");
      line("if (written < 0 || (size_t)written >= sizeof(number)) { return false; } const char* text = number; const size_t text_size = (size_t)written;");
    }
    line("const bool emitted = srl_xml_emit_text(writer, out, " +
         (field.attribute ? quote(field.qualified_name.local) : "NULL") + ", " +
         quote(field.qualified_name.namespace_uri) + ", " + quote("a" + std::to_string(prefix)) + ", text, text_size);");
    if (value_type.name == "float" || value_type.name == "double") { line("srl_buffer_free(&number.buffer);"); }
    line("if (!emitted) { return false; }");
    if (!field.attribute && !field.text) { line("if (xmlTextWriterEndElement(writer) < 0) { return false; }"); }
  };
  const auto read_leaf = [&](const type_name& value_type, const std::string& target, const std::string& raw) {
    if (value_type.name == "string") {
      line("if (srl_string_set(&" + target + ", (const char*)" + raw + ", (size_t)xmlStrlen(" + raw + ")) != srl_ok) { xmlFree(" + raw + "); return false; }");
    } else if (value_type.type == object_type::enum_type) {
      const auto count = static_cast<const enum_node&>(*value_type.resolved_node).enum_name_list.size();
      line("size_t ordinal = 0; while (ordinal < " + std::to_string(count) + "u && strcmp((const char*)" + raw + ", " + type(value_type) + "_srl_names[ordinal]) != 0) { ++ordinal; }");
      line("if (ordinal == " + std::to_string(count) + "u) { xmlFree(" + raw + "); return false; } " + target + " = (" + type(value_type) + ")ordinal;");
    } else if (value_type.name == "bool") {
      line("if (!srl_xml_boolean((const char*)" + raw + ", &" + target + ")) { xmlFree(" + raw + "); return false; }");
    } else if (value_type.name == "float" || value_type.name == "double") {
      line("double number = 0; if (!srl_xml_float((const char*)" + raw + ", " + (value_type.name == "float" ? "true" : "false") + ", &number)) { xmlFree(" + raw + "); return false; } " + target + " = (" + type(value_type) + ")number;");
    } else {
      const bool positive = value_type.name.starts_with('u');
      line(std::string{positive ? "uint64_t" : "int64_t"} + " number = 0; if (!srl_xml_" +
           (positive ? "unsigned" : "signed") + "((const char*)" + raw + ", " + std::to_string(native::width(value_type) * 8) +
           "u, &number)) { xmlFree(" + raw + "); return false; } " + target + " = (" + type(value_type) + ")number;");
    }
  };
  line("/* Write statically mapped fields through the optional bounded libxml2 provider. */");
  open("static inline bool " + n + "_srl_xml_write(xmlTextWriterPtr writer, srl_xml_output* out, const " + n + "* value, const char* local, const char* uri)");
  line("if (++out->nodes > out->limits.max_elements || ++out->writer.depth > out->limits.max_depth || xmlTextWriterStartElementNS(writer, *uri ? BAD_CAST \"srl\" : NULL, BAD_CAST local, *uri ? BAD_CAST uri : NULL) < 0) { return false; }");
  if (contract.fields.empty()) { line("(void)value;"); }
  std::size_t prefix{};
  for (const bool attributes : {true, false}) {
    for (const auto& field : contract.fields) {
      if (field.attribute != attributes) { continue; }
      const auto expression = "value->" + field_name(field.source->name);
      const auto& source_type = field.source->type_name_list.front();
      if (field.wrapper) {
        line("if (++out->nodes > out->limits.max_elements || ++out->writer.depth > out->limits.max_depth || xmlTextWriterStartElementNS(writer, " +
            std::string{field.wrapper->namespace_uri.empty() ? "NULL" : "BAD_CAST \"srl\""} + ", BAD_CAST " + quote(field.wrapper->local) + ", " +
            (field.wrapper->namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(field.wrapper->namespace_uri)) + ") < 0) { return false; }");
      }
      if (field.repeated) {
        line("if ((" + expression + ".size && !" + expression + ".data) || " + expression + ".size > out->limits.max_elements) { return false; }");
        if (field.min_occurs > 0U) { line("if (" + expression + ".size < " + std::to_string(field.min_occurs) + "u) { return false; }"); }
        if (field.max_occurs) { line("if (" + expression + ".size > " + std::to_string(*field.max_occurs) + "u) { return false; }"); }
        if (field.source->fixed_extent != 0U) { line("if (" + expression + ".size != " + std::to_string(field.source->fixed_extent) + "u) { return false; }"); }
        open("for (size_t index = 0; index < " + expression + ".size; ++index)");
        emit_leaf(field, expression + ".data[index]", prefix++); close();
      } else if (source_type.is_optional()) {
        open("if (" + expression + ".has_value)"); emit_leaf(field, expression + ".value", prefix++); close();
      } else { open(""); emit_leaf(field, expression, prefix++); close(); }
      if (field.wrapper) { line("--out->writer.depth; if (xmlTextWriterEndElement(writer) < 0) { return false; }"); }
    }
  }
  line("--out->writer.depth; return xmlTextWriterEndElement(writer) >= 0;"); close();
  line("/* Decode a complete record into candidate-owned fields without publishing partial state. */");
  open("static inline bool " + n + "_srl_xml_read(xmlNodePtr node, " + n + "* value, const srl_limits* limits)");
  line("(void)limits;");
  const auto has_text = std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; });
  if (!has_text) { line("if (!srl_xml_record(node)) { return false; }"); }
  line("xmlNodePtr cursor = srl_xml_next(node->children); size_t used_attributes = 0;");
  if (contract.fields.empty()) { line("(void)value; (void)limits;"); }
  for (const auto& field : contract.fields) {
    const auto target = "value->" + field_name(field.source->name);
    const auto& value_type = leaf(*field.source);
    const auto& source_type = field.source->type_name_list.front();
    const auto matches = "srl_xml_name(cursor, " + quote(field.qualified_name.local) + ", " + quote(field.qualified_name.namespace_uri) + ")";
    open("");
    if (field.wrapper) {
      line("if (!srl_xml_name(cursor, " + quote(field.wrapper->local) + ", " + quote(field.wrapper->namespace_uri) + ") || !srl_xml_record(cursor) || cursor->properties) { return false; }");
      line("xmlNodePtr after_wrapper = srl_xml_next(cursor->next); cursor = srl_xml_next(cursor->children);");
    }
    if (field.text) {
      line("if (cursor) { return false; } xmlChar* text = xmlNodeGetContent(node); if (!text) { return false; }");
      read_leaf(value_type, target, "text"); line("xmlFree(text);");
    } else if (field.repeated) {
      const auto collection_name = field.source->modifier == member::modifier_type::array ?
          n + "_" + field_name(field.source->name) + "_array" : storage_type(storage_type, source_type);
      line(collection_name + "_clear(&" + target + ");");
      open("while (" + matches + ")");
      line("if (" + target + ".size >= limits->max_elements) { return false; }");
      open("if (" + target + ".size == " + target + ".capacity)");
      line("size_t capacity = " + target + ".capacity ? " + target + ".capacity : 4u;");
      line("if (capacity > SIZE_MAX/2u) { return false; } capacity *= 2u; if (capacity > limits->max_elements) { capacity = limits->max_elements; }");
      line("if (capacity <= " + target + ".size || capacity > SIZE_MAX/sizeof(" + target + ".data[0])) { return false; }");
      line("void* memory = realloc(" + target + ".data, capacity*sizeof(" + target + ".data[0])); if (!memory) { return false; } " + target + ".data = (" + type(value_type) + "*)memory; " + target + ".capacity = capacity;"); close();
      line("memset(&" + target + ".data[" + target + ".size], 0, sizeof(" + target + ".data[0]));");
      line("const size_t slot = " + target + ".size++;");
      const auto slot = target + ".data[slot]";
      initialize_leaf(value_type, slot);
      if (value_type.type == object_type::class_type) {
        line("if (!" + type(value_type) + "_srl_xml_read(cursor, &" + slot + ", limits)) { return false; }");
      } else {
        line("xmlChar* text = srl_xml_text(cursor); if (!text) { return false; }");
        read_leaf(value_type, slot, "text"); line("xmlFree(text);");
      }
      line("cursor = srl_xml_next(cursor->next);"); close();
      if (field.min_occurs > 0U) { line("if (" + target + ".size < " + std::to_string(field.min_occurs) + "u) { return false; }"); }
      if (field.max_occurs) { line("if (" + target + ".size > " + std::to_string(*field.max_occurs) + "u) { return false; }"); }
      if (field.source->fixed_extent != 0U) { line("if (" + target + ".size != " + std::to_string(field.source->fixed_extent) + "u) { return false; }"); }
    } else {
      const bool optional = source_type.is_optional();
      if (optional) {
        open("if (" + target + ".has_value)"); free_leaf(value_type, target + ".value"); close();
        line("memset(&" + target + ", 0, sizeof(" + target + "));");
      }
      const auto destination = target + (optional ? ".value" : "");
      if (field.attribute) {
        line("xmlChar* text = xmlGetNsProp(node, BAD_CAST " + quote(field.qualified_name.local) + ", " +
             (field.qualified_name.namespace_uri.empty() ? "NULL" : "BAD_CAST " + quote(field.qualified_name.namespace_uri)) + ");");
        if (!optional) { line("if (!text) { return false; }"); }
        open("if (text)");
        line("++used_attributes;");
        if (optional) { line(target + ".has_value = true;"); }
        read_leaf(value_type, destination, "text"); line("xmlFree(text);"); close();
      } else {
        if (!optional) { line("if (!" + matches + ") { return false; }"); }
        open("if (" + matches + ")");
        if (optional) { line(target + ".has_value = true;"); initialize_leaf(value_type, destination); }
        if (value_type.type == object_type::class_type) {
          line("if (!" + type(value_type) + "_srl_xml_read(cursor, &" + destination + ", limits)) { return false; }");
        } else {
          line("xmlChar* text = srl_xml_text(cursor); if (!text) { return false; }");
          read_leaf(value_type, destination, "text"); line("xmlFree(text);");
        }
        line("cursor = srl_xml_next(cursor->next);"); close();
      }
    }
    if (field.wrapper) { line("if (cursor) { return false; } cursor = after_wrapper;"); }
    close();
  }
  line("size_t attribute_count = 0; for (xmlAttrPtr attribute = node->properties; attribute; attribute = attribute->next) { ++attribute_count; }");
  line("return cursor == NULL && used_attributes == attribute_count;"); close();
  line("/* Replace initialized result bytes only after the complete XML document is finalized. */");
  const auto validator_parameter = options_.runtime_xsd ? ", const srl_xml_validation* validation" : "";
  open("static inline srl_status " + n + "_encode_xml(const " + n + "* value, srl_buffer* result, const srl_limits* limits" + validator_parameter + ")");
  if (options_.runtime_xsd && options_.validation_scope == "input") { line("(void)validation;"); }
  line("if (!value || !result) { return srl_invalid; } srl_xml_output out = {0}; out.limits = limits ? *limits : srl_default_limits();");
  line("xmlOutputBufferPtr buffer = xmlOutputBufferCreateIO(srl_xml_output_write, srl_xml_output_close, &out, NULL); if (!buffer) { return srl_allocation; }");
  line("xmlTextWriterPtr writer = xmlNewTextWriter(buffer); if (!writer) { xmlOutputBufferClose(buffer); return srl_allocation; }");
  line("const bool good = " + n + "_srl_xml_write(writer, &out, value, " + quote(contract.root.local) + ", " + quote(contract.root.namespace_uri) + ") && xmlTextWriterFlush(writer) >= 0; xmlFreeTextWriter(writer);");
  line("if (!good || out.writer.status != srl_ok) { srl_buffer_free(&out.writer.buffer); return out.writer.status == srl_ok ? srl_invalid : out.writer.status; }");
  if (options_.runtime_xsd && options_.validation_scope != "input") {
    line("if (validation && validation->validate) { const srl_status validated = validation->validate(out.writer.buffer.data, out.writer.buffer.size, &out.limits, validation->context); if (validated != srl_ok) { srl_buffer_free(&out.writer.buffer); return validated; } }");
  }
  line("srl_buffer_free(result); *result = out.writer.buffer; return srl_ok;"); close();
  line("/* Decode exactly into a fresh candidate; syntax/contract failure retains the destination. */");
  open("static inline srl_status " + n + "_decode_xml(" + n + "* result, const uint8_t* bytes, size_t size, const srl_limits* limits" + validator_parameter + ")");
  if (options_.runtime_xsd && options_.validation_scope == "output") { line("(void)validation;"); }
  line("if (!result || (size && !bytes)) { return srl_invalid; } const srl_limits bounds = limits ? *limits : srl_default_limits();");
  line("const srl_status checked = srl_xml_preflight(bytes, size, &bounds); if (checked != srl_ok) { return checked; }");
  if (options_.runtime_xsd && options_.validation_scope != "output") {
    line("if (validation && validation->validate) { const srl_status validated = validation->validate(bytes, size, &bounds, validation->context); if (validated != srl_ok) { return validated; } }");
  }
  line("xmlDocPtr document = xmlReadMemory((const char*)bytes, (int)size, NULL, \"UTF-8\", XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING); if (!document) { return srl_invalid; }");
  line("xmlNodePtr root = xmlDocGetRootElement(document); " + n + " candidate = {0}; const srl_status initialized = " + n + "_init(&candidate);");
  line("const bool good = initialized == srl_ok && srl_xml_name(root, " + quote(contract.root.local) + ", " + quote(contract.root.namespace_uri) + ") && " + n + "_srl_xml_read(root, &candidate, &bounds); xmlFreeDoc(document);");
  line("if (!good) { " + n + "_free(&candidate); return initialized == srl_ok ? srl_invalid : initialized; } " + n + "_free(result); *result = candidate; return srl_ok;"); close();
}

// Build target-native support and direct bindings after the owning model declarations.
std::string generator::generate() {
  if (language_ != "c" && language_ != "swift" && language_ != "kotlin") {
    throw std::invalid_argument{"Unsupported native XML generator target"};
  }
  output_ = std::string{c_ ? c_runtime : swift_ ? swift_runtime : kotlin_runtime};
  const std::set<std::string> reserved_runtime_names{"XmlLimits", "XmlValidator", "PreparedXmlSchema", "SrlXmlName",
      "SrlXmlAttribute", "SrlXmlNode", "SrlXmlReader", "SrlXmlParser", "SrlXmlBudget"};
  const std::set<std::string> reserved_runtime_functions{"srlXmlEscape", "srlXmlWrite", "srlXmlParse", "srlXmlHasDoctype",
      "srlXmlText", "srlXmlBoolean", "srlXmlFloat", "srlXmlFloat32", "srlXmlFloatText", "srlXmlInteger",
      "srlXmlIntegerText", "srlXmlUtf8Size", "srlXmlWhitespace"};
  const std::set<std::string> reserved_mobile_methods{"encodeXml", "decodeXml", "srlXmlFill", "srlXmlRead"};
  // These additional Foundation/provider types are referenced unqualified only by XML support.
  const std::set<std::string> swift_provider_names{"XMLParser", "XMLParserDelegate", "NSObject", "CharacterSet"};
  std::set<std::string> generated_c_functions{};
  std::set<std::string> generated_enum_functions{};
  for (const auto* node : model_.nodes) {
    const auto n = native::snake(model_.names.at(node));
    if (node->type == object_type::class_type) {
      generated_c_functions.insert({n + "_encode_xml", n + "_decode_xml", n + "_srl_xml_write", n + "_srl_xml_read"});
    } else if (node->type == object_type::enum_type) {
      generated_enum_functions.insert("srlXml" + model_.names.at(node) + "Enum");
    }
  }
  const auto check_c_name = [&](const std::string& name) {
    if (name.starts_with("srl_xml_") || generated_c_functions.contains(name)) {
      throw std::invalid_argument{"XML generated C symbol conflicts with schema declaration: " + name};
    }
  };
  for (const auto* node : model_.nodes) {
    if (!c_ && node->type != object_type::function_type && reserved_runtime_names.contains(model_.names.at(node))) {
      throw std::invalid_argument{"XML runtime name conflicts with native declaration: " + model_.names.at(node)};
    }
    if (swift_ && node->type != object_type::function_type && swift_provider_names.contains(model_.names.at(node))) {
      throw std::invalid_argument{"XML provider name conflicts with Swift declaration: " + model_.names.at(node)};
    }
    if (c_) {
      const auto n = native::snake(model_.names.at(node));
      check_c_name(n);
      if (node->type == object_type::enum_type) {
        for (const auto& enumerator : static_cast<const enum_node&>(*node).enum_name_list) {
          check_c_name(n + "_" + field_name(enumerator));
        }
      }
    } else if (node->type == object_type::function_type) {
      const auto generated_name = field_name(model_.names.at(node));
      if (reserved_runtime_functions.contains(generated_name) || generated_enum_functions.contains(generated_name)) {
        throw std::invalid_argument{"XML runtime helper conflicts with authored function: " + generated_name};
      }
    }
    if (node->type != object_type::class_type) { continue; }
    const auto& source = static_cast<const class_node&>(*node);
    const auto n = native::snake(model_.names.at(node));
    std::set<std::string> mobile_names = reserved_mobile_methods;
    for (const auto& field : source.member_list) {
      if (!swift_ && !c_ && !field.omits("xml")) {
        const auto bridge = "srlXmlField" + native::pascal(field.name);
        mobile_names.insert({bridge, "get" + native::pascal(bridge), "set" + native::pascal(bridge)});
      }
    }
    for (const auto& field : source.member_list) {
      if (!c_ && mobile_names.contains(field_name(field.name))) {
        throw std::invalid_argument{"XML generated API conflicts with native field: " + field.name};
      }
    }
    const auto check_method = [&](std::string_view name) {
      if (c_) { check_c_name(n + "_" + field_name(name)); }
      else if (mobile_names.contains(field_name(name))) {
        throw std::invalid_argument{"XML generated API conflicts with native callable: " + std::string{name}};
      }
    };
    for (const auto& method : source.functions) {
      check_method(method.name);
    }
    for (const auto& factory : source.constructions) { if (!factory.constructor) { check_method(factory.name); } }
    for (const auto& conversion : source.conversions) { check_method(conversion.name); }
  }
  if (!c_) {
    if (swift_) {
      output_ += R"swift(
// Applications may provide an explicit trusted prepared XSD validator when enabled in generation.
public protocol XmlValidator { func validate(_ bytes: [UInt8], _ limits: XmlLimits) throws }
// Parse exact integer lexical values and reject representational overflow.
fileprivate func srlXmlInteger<T: FixedWidthInteger>(_ text: String, _ type: T.Type) throws -> T {
    let value = text.trimmingCharacters(in: CharacterSet(charactersIn: " \t\r\n"))
    guard value.range(of: "^[+-]?[0-9]+$", options: .regularExpression) != nil else { throw SerializerError.invalid("Invalid XML integer") }
    let normalized = value.hasPrefix("-") && value.dropFirst().allSatisfy({ $0 == "0" }) ? "0" : value
    guard let result = T(normalized) else { throw SerializerError.invalid("Overflowing XML integer") }
    return result
}
// Retain canonical IEEE special values while bounding binary32 conversion.
fileprivate func srlXmlFloat32(_ text: String) throws -> Float {
    let number = try srlXmlFloat(text)
    let result = Float(number)
    guard !number.isFinite || result.isFinite else { throw SerializerError.invalid("XML binary32 overflow") }
    return result
}
// Use XSD spellings for nonfinite values and Swift's round-trip finite formatting.
fileprivate func srlXmlFloatText(_ number: Double) -> String {
    if number.isNaN { return "NaN" }; if number.isInfinite { return number < 0 ? "-INF" : "INF" }; return String(number)
}
// Keep binary32 output short instead of formatting an artificially widened binary64 value.
fileprivate func srlXmlFloatText(_ number: Float) -> String {
    if number.isNaN { return "NaN" }; if number.isInfinite { return number < 0 ? "-INF" : "INF" }; return String(number)
}
)swift";
    } else {
      output_ += R"kotlin(
// Runtime schema validation is explicit and can reuse one trusted prepared provider schema.
fun interface XmlValidator { fun validate(bytes: ByteArray, limits: XmlLimits) }
// Prepare a standalone trusted XSD once; external DTD/schema resolution is prohibited.
class PreparedXmlSchema(bytes: ByteArray, limits: XmlLimits = XmlLimits()) : XmlValidator {
    private val schema: javax.xml.validation.Schema
    init {
        require(bytes.size <= limits.maxBytes) { "XSD byte limit" }
        val text = Charsets.UTF_8.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT).decode(java.nio.ByteBuffer.wrap(bytes)).toString()
        require(!text.contains('\u0000') && !srlXmlHasDoctype(text)) { "Invalid UTF-8 XSD or prohibited DTD" }
        val declaration = Regex("^\\uFEFF?<\\?xml\\s[^?]*\\?>").find(text)?.value
        val encoding = declaration?.let { Regex("encoding\\s*=\\s*['\"]([^'\"]+)['\"]").find(it)?.groupValues?.get(1) }
        require(encoding == null || encoding.equals("UTF-8", ignoreCase = true)) { "XSD encoding must be UTF-8" }
        val factory = javax.xml.validation.SchemaFactory.newInstance(javax.xml.XMLConstants.W3C_XML_SCHEMA_NS_URI)
        factory.setFeature(javax.xml.XMLConstants.FEATURE_SECURE_PROCESSING, true)
        factory.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, "")
        factory.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, "")
        factory.resourceResolver = org.w3c.dom.ls.LSResourceResolver { _, _, _, _, _ -> throw IllegalArgumentException("External XSD references prohibited") }
        schema = factory.newSchema(javax.xml.transform.stream.StreamSource(java.io.ByteArrayInputStream(bytes)))
    }
    override fun validate(bytes: ByteArray, limits: XmlLimits) {
        srlXmlParse(bytes, limits)
        val validator = schema.newValidator()
        validator.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD, "")
        validator.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA, "")
        validator.validate(javax.xml.transform.stream.StreamSource(java.io.ByteArrayInputStream(bytes)))
    }
}
// Exact XML integer grammar is checked before native width/range conversion.
internal fun srlXmlIntegerText(text: String): String {
    val value = text.trim(' ', '\t', '\r', '\n')
    require(Regex("[+-]?[0-9]+").matches(value)) { "Invalid XML integer" }
    return if (value.startsWith('-') && value.drop(1).all { it == '0' }) "0" else value
}
// Reject finite-to-infinite narrowing when binding XML binary32 values.
internal fun srlXmlFloat32(text: String): Float {
    val number = srlXmlFloat(text); val result = number.toFloat()
    require(!number.isFinite() || result.isFinite()) { "XML binary32 overflow" }; return result
}
// Emit canonical XSD special-value spellings.
internal fun srlXmlFloatText(number: Double): String = when {
    number.isNaN() -> "NaN"
    number == Double.POSITIVE_INFINITY -> "INF"
    number == Double.NEGATIVE_INFINITY -> "-INF"
    else -> number.toString()
}
// Preserve the shortest native binary32 representation without widening its lexical output.
internal fun srlXmlFloatText(number: Float): String = when {
    number.isNaN() -> "NaN"
    number == Float.POSITIVE_INFINITY -> "INF"
    number == Float.NEGATIVE_INFINITY -> "-INF"
    else -> number.toString()
}
)kotlin";
    }
  }
  if (c_) {
    for (const auto* node : model_.nodes) {
      if (node->type != object_type::class_type) { continue; }
      const auto n = native::snake(model_.names.at(node));
      line("/* Internal XML declarations support nested owning records. */");
      line("static inline bool " + n + "_srl_xml_write(xmlTextWriterPtr writer, srl_xml_output* out, const " + n + "* value, const char* local, const char* uri);");
      line("static inline bool " + n + "_srl_xml_read(xmlNodePtr node, " + n + "* value, const srl_limits* limits);");
    }
  }
  for (const auto* node : model_.nodes) {
    if (!c_ && node->type == object_type::enum_type) {
      const auto n = model_.names.at(node);
      if (swift_) {
        open("fileprivate func srlXml" + n + "Enum(_ text: String) throws -> " + n);
        line("guard let index = " + n + ".srlNames.firstIndex(of: text), let result = " + n + "(rawValue: index) else { throw SerializerError.invalid(\"Unknown XML enum\") }");
        line("return result"); close();
      } else {
        open("internal fun srlXml" + n + "Enum(text: String): " + n);
        line("val index = " + n + ".srlNames.indexOf(text)");
        line("require(index >= 0) { \"Unknown XML enum\" }; return " + n + ".entries[index]"); close();
      }
    }
    if (node->type != object_type::class_type) { continue; }
    const auto& source = static_cast<const class_node&>(*node);
    const auto contract = bindings_.bind(source);
    if (c_) { c_record(source, contract); }
    else { mobile_record(source, contract); }
  }
  return output_;
}
} // namespace

std::string generate_native(const native::schema& model, const xml_options& options, std::string_view language) {
  if (!options.enabled) { return {}; }
  return generator{model, options, language}.generate();
}
} // namespace rohit::serializer::writer::xml_codegen
