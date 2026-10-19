#include "schema_projection.hpp"
#include "version_writer.hpp"
#include "behavior_writer.hpp"
#include "construction_writer.hpp"
#include "conversion_writer.hpp"
#include "translator_writer.hpp"
#include "reference_portable_writer.hpp"
#include "comparison_writer.hpp"
#include "format_writer.hpp"
#include "auto_size_writer.hpp"
#include "generated_notices.hpp"
#include "xml_schema.hpp"
#include <rohit/serializer_creator.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::java {
namespace {
constexpr std::string_view runtime_source =
#include "java_runtime.inc"
    ;
constexpr std::string_view xml_runtime_source =
#include "java_xml_runtime.inc"
    ;

// Escape schema text as a Java string literal without changing wire spelling.
std::string quote(std::string_view value) {
  std::string result{"\""};
  for (const auto character : value) {
    switch (character) {
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20) {
        throw std::invalid_argument{"Java schema text contains a control character"};
      }
      result += character;
    }
  }
  return result + '"';
}

// Reject Java keywords, restricted identifiers, and invalid ASCII identifiers.
void validate_identifier(std::string_view value) {
  static const std::set<std::string_view> reserved{
      "abstract", "assert", "boolean",    "break",     "byte",       "case",      "catch",
      "char",     "class",  "const",      "continue",  "default",    "do",        "double",
      "else",     "enum",   "extends",    "final",     "finally",    "float",     "for",
      "goto",     "if",     "implements", "import",    "instanceof", "int",       "interface",
      "long",     "native", "new",        "package",   "private",    "protected", "public",
      "return",   "short",  "static",     "strictfp",  "super",      "switch",    "synchronized",
      "this",     "throw",  "throws",     "transient", "try",        "void",      "volatile",
      "while",    "true",   "false",      "null",      "_",          "var",       "yield",
      "record",   "sealed", "permits",    "java"};
  const auto first = [](unsigned char value) {
    return std::isalpha(value) || value == '_' || value == '$';
  };
  if (value.empty() || !first(static_cast<unsigned char>(value.front())) ||
      reserved.contains(value) ||
      !std::all_of(value.begin(), value.end(), [&](unsigned char character) {
        return first(character) || std::isdigit(character);
      })) {
    throw std::invalid_argument{"Invalid or reserved Java identifier: " + std::string{value}};
  }
}

// Protect the external XML packages referenced by fully qualified runtime helper types.
bool is_xml_package_name(std::string_view value) {
  return value == "javax" || value == "org";
}

// Convert schema words to standard Java type, field, or constant spelling.
std::string identifier(std::string_view value, bool type, bool constant, bool rename) {
  if (!rename) {
    validate_identifier(value);
    return std::string{value};
  }
  std::vector<std::string> words{};
  std::string word{};
  for (std::size_t index = 0; index < value.size(); ++index) {
    const auto current = static_cast<unsigned char>(value[index]);
    const bool boundary =
        index != 0 && std::isupper(current) &&
        (std::islower(static_cast<unsigned char>(value[index - 1])) ||
         (index + 1 < value.size() && std::islower(static_cast<unsigned char>(value[index + 1]))));
    if (current == '_' || boundary) {
      if (!word.empty()) {
        words.push_back(word);
        word.clear();
      }
    }
    if (current != '_') {
      word += static_cast<char>(std::tolower(current));
    }
  }
  if (!word.empty()) {
    words.push_back(word);
  }
  std::string result{};
  for (auto& part : words) {
    if (constant) {
      if (!result.empty()) {
        result += '_';
      }
      for (auto& character : part) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
      }
    } else if (type || !result.empty()) {
      part.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(part.front())));
    }
    result += part;
  }
  validate_identifier(result);
  return result;
}

// Map schema primitive storage to Java bit-preserving primitive types.
std::string primitive(std::string_view name, bool boxed = false) {
  if (name.starts_with("version")) {
    return "java.lang.String";
  }
  if (name == "string") {
    return "java.lang.String";
  }
  if (name == "bool") {
    return boxed ? "java.lang.Boolean" : "boolean";
  }
  if (name == "char" || name == "int8" || name == "uint8") {
    return boxed ? "java.lang.Byte" : "byte";
  }
  if (name == "int16" || name == "uint16") {
    return boxed ? "java.lang.Short" : "short";
  }
  if (name == "int32" || name == "uint32") {
    return boxed ? "java.lang.Integer" : "int";
  }
  if (name == "int64" || name == "uint64") {
    return boxed ? "java.lang.Long" : "long";
  }
  if (name == "float") {
    return boxed ? "java.lang.Float" : "float";
  }
  if (name == "double") {
    return boxed ? "java.lang.Double" : "double";
  }
  throw std::invalid_argument{"Unsupported Java primitive: " + std::string{name}};
}

// Report the fixed binary width of a schema integer.
std::string width(std::string_view name) {
  if (name.ends_with("8")) {
    return "1";
  }
  if (name.ends_with("16")) {
    return "2";
  }
  if (name.ends_with("32")) {
    return "4";
  }
  if (name.ends_with("64")) {
    return "8";
  }
  throw std::invalid_argument{"Unsupported Java integer: " + std::string{name}};
}

// Accumulate and validate a complete compilation unit before publishing it.
class emitter {
  const java_options& options;
  std::string outer;
  behavior::checked_helpers arithmetic_helpers{};
  std::string source{};
  std::size_t level{1};
  std::map<const syntax_node*, std::string> names{};
  std::map<std::string, const syntax_node*> declarations{};
  xml_binding::schema xml_contract;

  // Select the scalar or record under one supported optional or repeated XML mapping.
  const rohit::serializer::type_name& xml_leaf(const member& value) const {
    const auto& selected = value.type_name_list.front();
    return selected.is_optional() || selected.is_sequence() ? selected.arguments.front() : selected;
  }
  // Preserve unsigned Java model bits and strict XML/XSD scalar lexical values.
  std::string xml_scalar(const rohit::serializer::type_name& value,
                         const std::string& expression, bool encoding) const {
    if (value.type == object_type::enum_type) {
      if (encoding) { return expression + ".wireName"; }
      std::string result = "(switch (" + expression + ") { ";
      for (const auto& item : static_cast<const enum_node&>(*value.resolved_node).enum_name_list) {
        result += "case " + quote(item) + " -> " + type(value) + "." + constant(item) + "; ";
      }
      return result + "default -> throw new IllegalArgumentException(\"Invalid XML enum\"); })";
    }
    if (value.name == "string") { return expression; }
    if (value.name == "bool") {
      return encoding ? "(" + expression + " ? \"true\" : \"false\")" : "srlXmlBoolean(" + expression + ")";
    }
    if (value.name == "float" || value.name == "double") {
      const auto single = value.name == "float" ? "true" : "false";
      return encoding ? "srlXmlFloatText(" + expression + ", " + single + ")" :
          (value.name == "float" ? "(float)" : "") + std::string{"srlXmlFloat("} + expression + ", " + single + ")";
    }
    if (!encoding) {
      return (value.name.ends_with("64") ? "" : "(" + type(value) + ")") + std::string{"srlXmlInteger("} + expression + ", " +
          std::to_string(std::stoi(width(value.name)) * 8) + ", " +
          (value.name.starts_with('u') ? "true" : "false") + ")";
    }
    if (value.name == "uint64") { return "java.lang.Long.toUnsignedString(" + expression + ")"; }
    if (value.name == "uint32") { return "java.lang.Integer.toUnsignedString(" + expression + ")"; }
    if (value.name == "uint16") { return "java.lang.Integer.toString(java.lang.Short.toUnsignedInt(" + expression + "))"; }
    if (value.name == "uint8") { return "java.lang.Integer.toString(java.lang.Byte.toUnsignedInt(" + expression + "))"; }
    return "java.lang.String.valueOf(" + expression + ")";
  }
  // Add whole-object XML entry points and fixed direct field bindings to an owning class.
  void xml_object(const class_node& value) {
    if (!options.xml.enabled) { return; }
    const auto contract = xml_contract.bind(value);
    // Authored callables share Java's member lookup with the generated codec and runtime calls.
    const auto reject_method = [&](const std::string& selected) {
      static const std::set<std::string> methods{
          "encodeXml", "decodeXml", "xmlNode", "xmlFromNode", "srlXmlName", "srlXmlTrim",
          "srlXmlParse", "srlXmlScalarText", "srlXmlInteger", "srlXmlBoolean", "srlXmlFloat",
          "srlXmlFloatText", "srlXmlWrite", "srlXmlEscape"};
      if (methods.contains(selected)) {
        throw std::invalid_argument{"Java XML method collides with declared behavior: " + selected};
      }
    };
    for (const auto& method : value.functions) { reject_method(field(method.name)); }
    for (const auto& factory : value.constructions) {
      reject_method(field(factory.constructor ? "create" : factory.name));
    }
    for (const auto& conversion : value.conversions) { reject_method(field(conversion.name)); }
    const auto name = type_name(value.name);
    const auto expanded = [](const xml::name& item) {
      return quote(item.namespace_uri.empty() ? item.local : "{" + item.namespace_uri + "}" + item.local);
    };
    line("/** Encode one complete XML document using the fixed generated mapping. */");
    line("public byte[] encodeXml() { return encodeXml(Limits.defaults(), null); }");
    line("/** Bound XML output and optionally reuse an explicitly prepared runtime XSD. */");
    line("public byte[] encodeXml(Limits limits, XmlSchema schema) {"); ++level;
    line("var budget = new SrlXmlBudget(limits);");
    line("byte[] bytes = srlXmlWrite(xmlNode(" + expanded(contract.root) + ", budget, 1), limits);");
    if (!options.xml.runtime_xsd) { line("if (schema != null) throw new IllegalArgumentException(\"Runtime XSD was not enabled during generation\");"); }
    else if (options.xml.validation_scope != "input") { line("if (schema != null) schema.validate(bytes, limits);"); }
    line("return bytes;"); --level; line("}");
    line("/** Decode exactly one XML document into an unpublished fresh owning value. */");
    line("public static " + name + " decodeXml(byte[] bytes) { return decodeXml(bytes, Limits.defaults(), null); }");
    line("/** Enforce fixed mapping, complete resource bounds and selected runtime XSD checks. */");
    line("public static " + name + " decodeXml(byte[] bytes, Limits limits, XmlSchema schema) {"); ++level;
    if (!options.xml.runtime_xsd) { line("if (schema != null) throw new IllegalArgumentException(\"Runtime XSD was not enabled during generation\");"); }
    else if (options.xml.validation_scope != "output") { line("if (schema != null) schema.validate(bytes, limits);"); }
    line("var node = srlXmlParse(bytes, limits);");
    line("if (!node.name.equals(" + expanded(contract.root) + ")) throw new IllegalArgumentException(\"Unexpected XML root or namespace\");");
    line("return xmlFromNode(node);"); --level; line("}");
    line("/** Build fixed XML fields while sharing a complete-document allocation budget. */");
    line("private SrlXmlNode xmlNode(String name, SrlXmlBudget budget, int depth) {"); ++level;
    line("var node = budget.node(name, depth);");
    for (const auto& item : contract.fields) {
      const auto& source_field = *item.source;
      const auto& leaf = xml_leaf(source_field);
      const auto access = "this." + field(source_field.name);
      const bool optional = source_field.type_name_list.front().is_optional();
      std::string selected = access;
      line("{"); ++level;
      if (item.wrapper) {
        line("var wrapper = budget.node(" + expanded(*item.wrapper) + ", depth + 1);");
        line("node.children.add(wrapper);");
      }
      const auto parent = item.wrapper ? std::string{"wrapper"} : std::string{"node"};
      const auto child_depth = item.wrapper ? "depth + 2" : "depth + 1";
      if (optional) { line("if (" + access + ".isPresent()) {"); ++level; selected += ".get()"; }
      if (item.repeated) {
        const auto count = access + (source_field.modifier == member::modifier_type::array ? ".length" : ".size()");
        line("if (" + count + " < " + std::to_string(item.min_occurs) +
             (item.max_occurs ? " || " + count + " > " + std::to_string(*item.max_occurs) : "") +
             ") throw new IllegalArgumentException(\"XML occurrence mismatch\");");
        if (source_field.fixed_extent) { line("if (" + count + " != " + std::to_string(source_field.fixed_extent) + ") throw new IllegalArgumentException(\"XML fixed extent mismatch\");"); }
        line("for (var item : " + access + ") {"); ++level; selected = "item";
      }
      if (item.text) { line("node.text.append(budget.text(" + xml_scalar(leaf, selected, true) + "));"); }
      else if (item.attribute) { line("budget.attribute(node, " + expanded(item.qualified_name) + ", " + xml_scalar(leaf, selected, true) + ");"); }
      else if (leaf.type == object_type::class_type) { line(parent + ".children.add(" + selected + ".xmlNode(" + expanded(item.qualified_name) + ", budget, " + child_depth + "));"); }
      else {
        line("var child = budget.node(" + expanded(item.qualified_name) + ", " + child_depth + ");");
        line("child.text.append(budget.text(" + xml_scalar(leaf, selected, true) + "));");
        line(parent + ".children.add(child);");
      }
      if (item.repeated) { --level; line("}"); }
      if (optional) { --level; line("}"); }
      --level; line("}");
    }
    line("return node;"); --level; line("}");
    line("/** Decode exact generated fields without assigning a partially checked value. */");
    line("private static " + name + " xmlFromNode(SrlXmlNode node) {"); ++level;
    line("var fields = new SrlXmlFields(node, " + std::string{std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& item) { return item.text; }) ? "true" : "false"} + ");"); line("var result = new " + name + "();");
    for (const auto& item : contract.fields) {
      if (item.min_occurs > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
          (item.max_occurs && *item.max_occurs > static_cast<std::size_t>(std::numeric_limits<int>::max()))) {
        throw std::invalid_argument{"Java XML cardinality exceeds supported collection size"};
      }
      const auto& source_field = *item.source;
      const auto& leaf = xml_leaf(source_field);
      const bool optional = source_field.type_name_list.front().is_optional();
      const auto access = "result." + field(source_field.name);
      const auto read_value = [&](const std::string& expression) {
        return leaf.type == object_type::class_type ? type(leaf) + ".xmlFromNode(" + expression + ")" :
            xml_scalar(leaf, item.attribute || item.text ? expression : "srlXmlScalarText(" + expression + ")", false);
      };
      line("{"); ++level;
      if (item.text) {
        line(access + " = " + read_value("fields.text()") + ";");
      } else if (item.attribute) {
        line("var text = fields.attribute(" + expanded(item.qualified_name) + ", " + (optional ? "true" : "false") + ");");
        line(access + " = " + (optional ? "text == null ? java.util.Optional.empty() : java.util.Optional.of(" + read_value("text") + ")" : read_value("text")) + ";");
      } else {
        line("var children = fields." + std::string{item.wrapper ? "wrapped(" + expanded(*item.wrapper) + ", " : "children("} + expanded(item.qualified_name) + ", " + std::to_string(item.min_occurs) + ", " +
            (item.max_occurs ? std::to_string(*item.max_occurs) : "-1") + ");");
        if (item.repeated) {
          if (source_field.fixed_extent) { line("if (children.size() != " + std::to_string(source_field.fixed_extent) + ") throw new IllegalArgumentException(\"XML fixed extent mismatch\");"); }
          if (source_field.modifier == member::modifier_type::array) {
            line(access + " = " + array_initial(leaf, "children.size()") + ";");
            line("for (int index = 0; index < children.size(); ++index) " + access + "[index] = " + read_value("children.get(index)") + ";");
          } else {
            line(access + " = new java.util.ArrayList<>();");
            line("for (var child : children) " + access + ".add(" + read_value("child") + ");");
          }
        } else { line(access + " = " + (optional ? "children.isEmpty() ? java.util.Optional.empty() : java.util.Optional.of(" + read_value("children.get(0)") + ")" : read_value("children.get(0)")) + ";"); }
      }
      --level; line("}");
    }
    line("fields.finish();"); line("return result;"); --level; line("}");
  }

  // Append one line with the selected profile's indentation.
  void line(const std::string& value = {}) {
    const std::size_t indent = options.standard == java_coding_standard::oracle ? 4 : 2;
    source.append(level * indent, ' ');
    source += value + '\n';
  }

  // Resolve a field or alternative name under the selected naming policy.
  std::map<std::string, std::string> projection_fields_{};

  std::string field(std::string_view value) const {
    if (const auto found = projection_fields_.find(std::string{value}); found != projection_fields_.end()) { return found->second; }
    return identifier(value, false, false, options.rename_identifiers);
  }

  // Resolve a type name under the selected naming policy.
  std::string type_name(std::string_view value) const {
    return identifier(value, true, false, options.rename_identifiers);
  }

  // Resolve an enum constant without changing its original wire spelling.
  std::string constant(std::string_view value) const {
    return identifier(value, false, true, options.rename_identifiers);
  }

  // Register names up front and reject collisions including enclosing Java classes.
  void register_nodes(const std::vector<std::unique_ptr<syntax_node>>& nodes,
                      const std::string& prefix, std::set<std::string> enclosing) {
    for (const auto& node : nodes) {
      if (node->type == object_type::generic_definition || node->type == object_type::native_code) { continue; }
      const auto name = node->type == object_type::function_type ? field(node->name) : type_name(node->name);
      if (options.xml.enabled && is_xml_package_name(name)) {
        throw std::invalid_argument{"Java XML package name collision: " + name};
      }
      const auto qualified = prefix + "." + name;
      const auto [previous, inserted] = declarations.emplace(qualified, node.get());
      const bool reopened_namespace = !inserted && node->type == object_type::namespace_type &&
                                      previous->second->type == object_type::namespace_type &&
                                      previous->second->get_full_name() == node->get_full_name();
      const bool overload = !inserted && node->type == object_type::function_type &&
          previous->second->type == object_type::function_type &&
          previous->second->get_full_name() == node->get_full_name();
      if ((!inserted && !reopened_namespace && !overload) || enclosing.contains(name)) {
        throw std::invalid_argument{"Java type name collision: " + name};
      }
      names.emplace(node.get(), qualified);
      if (node->type == object_type::namespace_type) {
        auto nested = enclosing;
        nested.insert(name);
        register_nodes(static_cast<const namespace_node&>(*node).statements, prefix + "." + name,
                       nested);
      }
    }
  }

  // Resolve a fully qualified generated Java type to avoid shadowing by schema fields.
  std::string type(const rohit::serializer::type_name& value, bool boxed = false) const {
    if (value.is_optional()) { return "java.util.Optional<" + type(value.arguments.front(), true) + ">"; }
    if (value.is_sequence()) { return "java.util.List<" + type(value.arguments.front(), true) + ">"; }
    if (value.is_associative()) { return "java.util.NavigableMap<" + type(value.arguments[0], true) + ", " + type(value.arguments[1], true) + ">"; }
    if (value.is_digest()) { return "byte[]"; }
    return value.type == object_type::primitive ? primitive(value.name, boxed)
                                                : names.at(value.resolved_node);
  }

  // Allocate outer arrays before a digest value's byte dimension.
  std::string array_initial(const rohit::serializer::type_name& value, const std::string& count) const {
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      const auto erased = value.is_optional() ? "java.util.Optional" : value.is_sequence() ? "java.util.List" : "java.util.NavigableMap";
      return "(" + type(value) + "[]) new " + erased + "[" + count + "]";
    }
    return value.is_digest() ? "new byte[" + count + "][]" : "new " + type(value) + "[" + count + "]";
  }

  // Construct a fresh schema default for a single value.
  std::string initial(const rohit::serializer::type_name& value) const {
    if (value.is_optional()) { return "java.util.Optional.empty()"; }
    if (value.is_sequence()) { return "new java.util.ArrayList<>()"; }
    if (value.is_associative()) {
      member map{};
      map.key = value.arguments[0].name;
      map.key_node = value.arguments[0].resolved_node;
      return map_initializer(map);
    }
    if (value.is_digest()) { return "new byte[" + std::to_string(value.digest_extent) + "]"; }
    if (value.type == object_type::class_type) {
      return "new " + type(value) + "()";
    }
    if (value.type == object_type::enum_type) {
      const auto& enumeration = static_cast<const enum_node&>(*value.resolved_node);
      if (enumeration.enum_name_list.empty()) {
        throw std::invalid_argument{"Empty Java enum"};
      }
      return type(value) + "." + constant(enumeration.enum_name_list.front());
    }
    if (value.name == "string" || value.name.starts_with("version")) {
      return "\"\"";
    }
    if (value.name == "bool") {
      return "false";
    }
    return "0";
  }

  // Use a comparator matching the C++ unsigned and UTF-8 ordering contracts.
  std::string map_initializer(const member& value) const {
    if (value.key_node && value.key_node->type != object_type::enum_type) {
      throw std::invalid_argument{"Java maps support primitive and enum keys, not object keys"};
    }
    if (value.key == "float" || value.key == "double") {
      throw std::invalid_argument{"Java maps do not support floating-point keys"};
    }
    if (value.key == "string") {
      return "new java.util.TreeMap<>(" + outer + "::compareText)";
    }
    if (value.key == "uint64") {
      return "new java.util.TreeMap<>(java.lang.Long::compareUnsigned)";
    }
    if (value.key == "uint32") {
      return "new java.util.TreeMap<>(java.lang.Integer::compareUnsigned)";
    }
    if (value.key == "uint16") {
      return "new java.util.TreeMap<>((a, b) -> "
             "java.lang.Integer.compare(java.lang.Short.toUnsignedInt(a), "
             "java.lang.Short.toUnsignedInt(b)))";
    }
    if (value.key == "uint8") {
      return "new java.util.TreeMap<>((a, b) -> "
             "java.lang.Integer.compare(java.lang.Byte.toUnsignedInt(a), "
             "java.lang.Byte.toUnsignedInt(b)))";
    }
    return "new java.util.TreeMap<>()";
  }

  // Resolve map key metadata in the shared parser representation.
  rohit::serializer::type_name key_type(const member& value) const {
    rohit::serializer::type_name result{std::string{value.key}, nullptr};
    result.type = value.key_node ? value.key_node->type : object_type::primitive;
    result.resolved_node = value.key_node;
    return result;
  }

  // Translate portable literal defaults and reject arbitrary C++ expressions.
  std::string default_value(const member& value) const {
    if (value.initializer) {
      return construction::initializer(*value.initializer, "java",
          [&](const auto& item) { return type(item); },
          [&](const auto& item) { return default_value(item); },
          [&](auto item) { return field(item); });
    }
    if (!value.default_factory.empty()) {
      return construction::factory_call(names.at(value.type_name_list.front().resolved_node),
          field(value.default_factory), "java");
    }
    const auto& item = value.type_name_list.front();
    if (item.name.starts_with("version")) {
      return schema_version::trim(value.default_value);
    }
    const auto first = value.default_value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      return initial(item);
    }
    const auto last = value.default_value.find_last_not_of(" \t\r\n");
    const auto text = value.default_value.substr(first, last - first + 1);
    if (value.magic && item.name == "char") {
      // Typed magic is canonicalized to its validated byte value by the parser.
      return "(byte) " + text;
    }
    if (item.type == object_type::enum_type) {
      const auto separator = text.rfind("::");
      const auto name = separator == std::string::npos ? text : text.substr(separator + 2);
      if (separator != std::string::npos) {
        const auto prefix = text.substr(0, separator);
        if (prefix != item.name && prefix != item.resolved_node->name &&
            prefix != item.resolved_node->get_full_name()) {
          throw std::invalid_argument{"Java enum default names the wrong type: " + value.name};
        }
      }
      const auto& values = static_cast<const enum_node&>(*item.resolved_node).enum_name_list;
      if (std::find(values.begin(), values.end(), name) != values.end()) {
        return type(item) + "." + constant(name);
      }
    } else if (item.type == object_type::primitive) {
      if (item.name == "string" &&
          std::regex_match(text, std::regex{"\"([^\"\\\\]|\\\\[\"\\\\bfnrt])*\""})) {
        return text;
      }
      if (item.name == "bool" && (text == "true" || text == "false")) {
        return text;
      }
      if (item.name == "char" && text.front() == '\'' && text.back() == '\'') {
        if ((text.size() == 3 && static_cast<unsigned char>(text[1]) < 0x80 && text[1] != '\\' &&
             text[1] != '\'') ||
            (text.size() == 4 && text[1] == '\\' &&
             std::string_view{"bfnrt\\\"'0"}.find(text[2]) != std::string_view::npos)) {
          return "(byte) " + text;
        }
      }
      if (std::regex_match(text, std::regex{"-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?"}) &&
          item.name != "char" && item.name != "string" && item.name != "bool") {
        if (item.name == "float" || item.name == "double") {
          float single{};
          double real{};
          const auto parsed = item.name == "float"
                                  ? std::from_chars(text.data(), text.data() + text.size(), single)
                                  : std::from_chars(text.data(), text.data() + text.size(), real);
          if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
            throw std::invalid_argument{"Java floating default out of range: " + value.name};
          }
          return text + (item.name == "float" ? "F" : "D");
        }
        const auto bits = std::stoi(width(item.name)) * 8;
        const bool is_unsigned = item.name.starts_with("u");
        std::uint64_t unsigned_value{};
        std::int64_t signed_value{};
        const auto parsed =
            is_unsigned ? std::from_chars(text.data(), text.data() + text.size(), unsigned_value)
                        : std::from_chars(text.data(), text.data() + text.size(), signed_value);
        bool valid = parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
        if (valid && bits < std::numeric_limits<std::uint64_t>::digits) {
          valid = is_unsigned ? unsigned_value < (std::uint64_t{1} << bits)
                              : signed_value >= -(std::int64_t{1} << (bits - 1)) &&
                                    signed_value < (std::int64_t{1} << (bits - 1));
        }
        if (!valid) {
          throw std::invalid_argument{"Java integer default out of range: " + value.name};
        }
        if (!is_unsigned) {
          return text + (bits == 64 ? "L" : "");
        }
        std::array<char, std::numeric_limits<std::uint64_t>::digits / 4> digits{};
        const auto converted =
            std::to_chars(digits.data(), digits.data() + digits.size(), unsigned_value, 16);
        const auto cast = bits < 32 ? "(" + type(item) + ") " : "";
        return cast + "0x" + std::string{digits.data(), converted.ptr} + (bits == 64 ? "L" : "");
      }
    }
    throw std::invalid_argument{"Java requires a portable literal or enum default: " + value.name};
  }

  // Build a direct scalar read expression with context-sensitive enum representation.
  std::string read_value(const rohit::serializer::type_name& value, const std::string& reader,
                         bool collection) const {
    if (value.is_optional()) {
      return "srlReadOptional(" + reader + ", () -> " + read_value(value.arguments.front(), reader, collection) + ")";
    }
    if (value.is_sequence()) {
      return "srlReadArray(" + reader + ", () -> " + read_value(value.arguments.front(), reader, true) + ")";
    }
    if (value.is_associative()) {
      return "srlReadMap(" + reader + ", " + initial(value) + ", () -> " +
             read_value(value.arguments[0], reader, true) + ", () -> " +
             read_value(value.arguments[1], reader, true) + ")";
    }
    if (value.is_digest()) {
      return "srlReadDigest(" + reader + ", " + std::to_string(value.digest_extent) + ")";
    }
    if (value.type == object_type::class_type) {
      return type(value) + ".read(" + reader + ")";
    }
    if (value.type == object_type::enum_type) {
      return type(value) + ".read(" + reader + ", " + (collection ? "true" : "false") + ")";
    }
    const auto access = "(" + reader + ").";
    if (value.name.starts_with("version")) {
      return access + "version(" + std::string{value.name.back()} + ")";
    }
    if (value.name == "string") {
      return access + "text()";
    }
    if (value.name == "bool") {
      return access + "bool()";
    }
    if (value.name == "char") {
      return access + "character()";
    }
    if (value.name == "float") {
      return access + "float32()";
    }
    if (value.name == "double") {
      return access + "floating(false)";
    }
    const auto cast = type(value) == "long" ? "" : "(" + type(value) + ") ";
    return cast + access + "integer(" + width(value.name) + ", " +
           (value.name.starts_with("u") ? "true" : "false") + ")";
  }

  // Emit a scalar write without reflection or a generic object dispatch table.
  void write_value(const rohit::serializer::type_name& value, const std::string& expression,
                   bool collection) {
    if (value.is_optional()) {
      line("out.beginOptional(" + expression + ".isPresent());");
      line("if (" + expression + ".isPresent()) {");
      ++level;
      write_value(value.arguments.front(), "(" + expression + ").get()", collection);
      --level;
      line("}");
      line("out.endArray();");
    } else if (value.is_sequence() || value.is_associative()) {
      const auto suffix = std::to_string(level);
      const auto item = "item" + suffix;
      const auto index = "index" + suffix;
      line("out.beginArray(" + expression + ".size());");
      line("int " + index + " = 0;");
      line("for (var " + item + " : " + expression + (value.is_associative() ? ".entrySet()" : "") + ") {");
      ++level;
      line("out.element(" + index + "++);");
      if (value.is_associative()) {
        line("if (out.json()) { out.beginObject(); out.field(0, \"key\", true); }");
        write_value(value.arguments[0], item + ".getKey()", true);
        line("if (out.json()) { out.field(0, \"value\", false); }");
        write_value(value.arguments[1], item + ".getValue()", true);
        line("if (out.json()) { out.endObject(); }");
      } else {
        write_value(value.arguments.front(), item, true);
      }
      --level;
      line("}");
      line("out.endArray();");
    } else if (value.is_digest()) {
      line("srlWriteDigest(out, " + expression + ", " + std::to_string(value.digest_extent) + ");");
    } else if (value.type == object_type::class_type) {
      line(expression + ".write(out);");
    } else if (value.type == object_type::enum_type) {
      line(expression + ".write(out, " + (collection ? "true" : "false") + ");");
    } else if (value.name.starts_with("version")) {
      line("out.version(" + expression + ", " + std::string{value.name.back()} + ");");
    } else if (value.name == "string") {
      line("out.text(" + expression + ");");
    } else if (value.name == "bool") {
      line("out.bool(" + expression + ");");
    } else if (value.name == "char") {
      line("out.character(" + expression + ");");
    } else if (value.name == "float") {
      line("out.float32(" + expression + ");");
    } else if (value.name == "double") {
      line("out.floating(" + expression + ", false);");
    } else {
      line("out.integer(" + expression + ", " + width(value.name) + ", " +
           (value.name.starts_with("u") ? "true" : "false") + ");");
    }
  }

  // Check supported revision bounds using unsigned-safe comparisons.
  void check_version(const class_node& owner, bool reading) {
    const auto& version = *owner.version_member();
    const auto value = std::string{reading ? "result." : "this."} + field(version.name);
    auto invalid =
        version_less(version_language::java, version, value, schema_version::minimum(version)) +
        " || !(" + version_less(version_language::java, version, value, version.default_value) +
        " || " + version_equal(version_language::java, version, value, version.default_value) + ")";
    if (reading) {
      invalid += " || (in.limits.readPolicy == ReadPolicy.STRICT && !" +
                 version_equal(version_language::java, version, value, version.default_value) + ")";
    }
    line("if (" + invalid +
         ") { throw new IllegalArgumentException(\"Unsupported schema version\"); }");
  }
  // Form immutable UTF-8 metadata without embedding arbitrary bytes in source literals.
  std::string magic_text(const class_node& value) const {
    std::string bytes;
    for (const auto byte : value.magic_bytes) {
      bytes += (bytes.empty() ? "" : ", ") + std::string{"(byte)"} +
               std::to_string(static_cast<unsigned char>(byte));
    }
    return "new String(new byte[] {" + bytes + "}, java.nio.charset.StandardCharsets.UTF_8)";
  }
  // Retain keyed field presence while skipping inactive positional bytes.
  void read_versioned_member(const class_node& owner, const member& item, int alternative = -1,
                             bool positional = false) {
    if (positional && item.omits("binary_none")) {
      return;
    }
    const auto omitted = omitted_condition(item.omitted_formats, "in.protocol", "Protocol.",
                                            protocol_name_style::upper_snake);
    if (!positional && !omitted.empty()) {
      line("if (" + omitted + ") { throw in.error(\"Field omitted from selected format\"); }");
    }
    if (!item.auto_size_formats.empty()) {
      const auto selected = omitted_condition(item.auto_size_formats, "in.protocol", "Protocol.",
          protocol_name_style::upper_snake);
      const auto seen_name = "autoSizeSeen" + std::to_string(item.id);
      line("if (" + selected + ") {");
      ++level;
      line("if (" + seen_name + ") { throw in.error(\"Duplicate auto_size field\"); }");
      line(seen_name + " = true;");
      --level;
      line("}");
    }
    const auto* version = owner.version_member();
    if (version) {
      const auto slot = std::to_string(version_slot(owner, item));
      line("if (seen[" + slot + "]) { throw in.error(\"Duplicate field\"); }");
      line("seen[" + slot + "] = true;");
    }
    const auto active = version ? version_active(version_language::java, item, *version,
                                                 "result." + field(version->name))
                                : std::string{};
    if (!active.empty()) {
      line("if (in.protocol != Protocol.BINARY_NONE || (" + active + ")) {");
      ++level;
    }
    if (item.wire_only) {
      line("autoSizeValue" + std::to_string(item.id) + " = " +
          read_value(item.type_name_list.front(), "in", false) + ";");
    } else if (item.modifier == member::modifier_type::variant) {
      read_union(item, alternative);
    } else {
      read_field(item);
    }
    if (item.size_scope == auto_size_scope::payload && !item.auto_size_formats.empty() &&
        auto_size::verifies(owner, options.class_auto_sizes)) {
      line("autoSizeFieldBytes" + std::to_string(item.id) + " = in.offset - autoSizeFieldStart;");
    }
    if (!active.empty()) {
      --level;
      line("}");
    }
    if (item.version) {
      check_version(owner, true);
    }
  }
  // Declare one field, including explicit union alternatives and primitive arrays.
  void declaration(const member& value) {
    const auto& item = value.type_name_list.front();
    const std::string access = value.access == access_type::private_access     ? "private "
                               : value.access == access_type::protected_access ? "protected "
                                                                               : "public ";
    const auto name = field(value.name);
    if (value.modifier != member::modifier_type::none && !value.default_value.empty() && !value.initializer) {
      throw std::invalid_argument{"Java collection/union defaults are not supported: " +
                                  value.name};
    }
    if (value.modifier == member::modifier_type::array) {
      line(access + type(item) + "[] " + name + " = " + (value.initializer ? default_value(value) : value.fixed_extent == 0 ? array_initial(item, "0") :
          item.type == object_type::primitive && item.name != "string" && !item.is_digest() &&
                  !item.is_optional() && !item.is_sequence() && !item.is_associative()
              ? array_initial(item, std::to_string(value.fixed_extent))
              : "java.util.stream.IntStream.range(0, " + std::to_string(value.fixed_extent) +
                    ").mapToObj(index -> " + initial(item) + ").toArray(size -> " +
                    array_initial(item, "size") + ")") + ";");
    } else if (value.modifier == member::modifier_type::map) {
      line(access + "java.util.NavigableMap<" + type(key_type(value), true) + ", " +
           type(item, true) + "> " + name + " = " + map_initializer(value) + ";");
    } else if (value.modifier == member::modifier_type::variant) {
      line(access + "int " + name + "Index;");
      for (const auto& alternative : value.type_name_list) {
        line(access + type(alternative) + " " + name + type_name(alternative.enum_name) + " = " +
             initial(alternative) + ";");
      }
    } else {
      line(access + type(item) + " " + name + " = " + default_value(value) + ";");
    }
  }

  // Write a field payload, with collection framing and enum collection semantics.
  void write_field(const member& value, const std::string& selected_expression = {}) {
    const auto name = selected_expression.empty() ? "this." + field(value.name) : selected_expression;
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      if (value.compact != compact_encoding::none) {
        line("out.compactValue(" + name + ", " + width(item.name) + ", " +
             (value.compact == compact_encoding::prefix ? "true" : "false") + ", " +
             (value.compact_strict ? "true" : "false") + ");");
        return;
      }
      write_value(item, name, false);
      return;
    }
    if (value.modifier == member::modifier_type::array) {
      if (value.fixed_extent != 0) {
        line("if (" + name + ".length != " + std::to_string(value.fixed_extent) +
             ") { throw new IllegalArgumentException(\"Fixed array extent mismatch\"); }");
      }
      line("out.beginArray(" + name + ".length);");
      line("for (int index = 0; index < " + name + ".length; ++index) {");
      ++level;
      line("out.element(index);");
      write_value(item, name + "[index]", true);
      --level;
      line("}");
    } else if (value.modifier == member::modifier_type::map) {
      line("out.beginArray(" + name + ".size());");
      line("int index = 0;");
      line("for (var entry : " + name + ".entrySet()) {");
      ++level;
      line("out.element(index++);");
      line("if (out.json()) { out.raw(\"{\\\"key\\\":\"); }");
      write_value(key_type(value), "entry.getKey()", true);
      line("if (out.json()) { out.raw(\",\\\"value\\\":\"); }");
      write_value(item, "entry.getValue()", true);
      line("if (out.json()) { out.raw(\"}\"); }");
      --level;
      line("}");
    }
    line("out.endArray();");
  }

  // Read collections incrementally so untrusted counts do not drive eager allocation.
  void read_field(const member& value) {
    const auto name = "result." + field(value.name);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      if (value.compact != compact_encoding::none) {
        const auto cast = type(item) == "long" ? "" : "(" + type(item) + ") ";
        line(name + " = " + cast + "in.compactValue(" + width(item.name) + ", " +
             (value.compact == compact_encoding::prefix ? "true" : "false") + ");");
        return;
      }
      line(name + " = " +
           (item.type == object_type::class_type ? type(item) + ".read(in, " + name + ")"
                                                 : read_value(item, "in", false)) +
           ";");
      return;
    }
    line("int count = in.beginArray();");
    if (value.fixed_extent != 0) {
      line("if (count != -1 && count != " + std::to_string(value.fixed_extent) +
           ") { throw new IllegalArgumentException(\"Fixed array extent mismatch\"); }");
    }
    if (value.modifier == member::modifier_type::array) {
      std::string element_width{"0"};
      if (item.is_optional()) {
        element_width = "1";
      } else if (item.type == object_type::primitive && item.name != "string" && !item.is_digest() &&
                 !item.is_sequence() && !item.is_associative()) {
        element_width = item.name == "bool" || item.name == "char" ? "1"
                        : item.name == "float"                     ? "4"
                        : item.name == "double"                    ? "8"
                                                                   : width(item.name);
      }
      line("if (!in.json()) {");
      ++level;
      line("in.requireArray(count, " + element_width + ");");
      line(name + " = " + array_initial(item, "count") + ";");
      line("for (int index = 0; in.nextElement(index, count); ++index) {");
      ++level;
      line(name + "[index] = " + read_value(item, "in", true) + ";");
      --level;
      line("}");
      --level;
      line("} else {");
      ++level;
      line("java.util.ArrayList<" + type(item, true) + "> values = new java.util.ArrayList<>();");
      line("for (int index = 0; in.nextElement(index, count); ++index) {");
      ++level;
      if (value.fixed_extent != 0) {
        line("if (index >= " + std::to_string(value.fixed_extent) +
             ") { throw new IllegalArgumentException(\"Fixed array extent mismatch\"); }");
      }
      line("values.add(" + read_value(item, "in", true) + ");");
      --level;
      line("}");
      if (value.fixed_extent != 0) {
        line("if (values.size() != " + std::to_string(value.fixed_extent) +
             ") { throw new IllegalArgumentException(\"Fixed array extent mismatch\"); }");
      }
      line(name + " = " + array_initial(item, "values.size()") + ";");
      line("for (int index = 0; index < values.size(); ++index) { " + name +
           "[index] = values.get(index); }");
      --level;
      line("}");
    } else {
      line(name + ".clear();");
      line("for (int index = 0; in.nextElement(index, count); ++index) {");
      ++level;
      line(type(key_type(value), true) + " entryKey = null;");
      line(type(item, true) + " value = null;");
      line("if (in.json()) {");
      ++level;
      line("in.beginObject();");
      line("boolean firstEntryField = true;");
      line("while (in.nextObject(firstEntryField)) {");
      ++level;
      line("firstEntryField = false;");
      line("switch (in.key()) {");
      ++level;
      line("case \"key\": entryKey = " + read_value(key_type(value), "in", true) + "; break;");
      line("case \"value\": value = " + read_value(item, "in", true) + "; break;");
      line("default: throw in.error(\"Unknown map entry field\");");
      --level;
      line("}");
      --level;
      line("}");
      line("in.endObject();");
      line("if (entryKey == null || value == null) { throw in.error(\"Incomplete map entry\"); }");
      --level;
      line("} else {");
      ++level;
      line("entryKey = " + read_value(key_type(value), "in", true) + ";");
      line("value = " + read_value(item, "in", true) + ";");
      --level;
      line("}");
      line(name + ".put(entryKey, value);");
      --level;
      line("}");
    }
  }

  // Read a union selected by its compact index or its already matched wire key.
  void read_union(const member& value, int selected = -1) {
    const auto name = "result." + field(value.name);
    if (selected >= 0) {
      const auto& alternative = value.type_name_list.at(static_cast<std::size_t>(selected));
      line(name + "Index = " + std::to_string(selected) + ";");
      line(name + type_name(alternative.enum_name) + " = " + read_value(alternative, "in", true) +
           ";");
      return;
    }
    line("switch (in.compact()) {");
    ++level;
    for (std::size_t index = 0; index < value.type_name_list.size(); ++index) {
      line("case " + std::to_string(index) + ": {");
      ++level;
      read_union(value, static_cast<int>(index));
      line("break;");
      --level;
      line("}");
    }
    line("default: throw in.error(\"Unknown union alternative\");");
    --level;
    line("}");
  }

  // Emit an enum with stable original names and explicit ordinal validation.
  void enumeration(const enum_node& value) {
    if (value.enum_name_list.empty()) {
      throw std::invalid_argument{"Java enums must declare a value"};
    }
    const auto name = type_name(value.name);
    line("public enum " + name + " {");
    ++level;
    std::set<std::string> constants{};
    for (std::size_t index = 0; index < value.enum_name_list.size(); ++index) {
      const auto item = constant(value.enum_name_list[index]);
      if (!constants.insert(item).second || item == "wireName" || item == "Protocol") {
        throw std::invalid_argument{"Java enum name collision: " + item};
      }
      line(item + "(" + quote(value.enum_name_list[index]) + ")" +
           (index + 1 == value.enum_name_list.size() ? ";" : ","));
    }
    line("private final String wireName;");
    line("/** Retain the schema spelling independently of Java naming. */");
    line(name + "(String wireName) { this.wireName = wireName; }");
    line("/** Encode names for JSON/string-key fields and compact ordinals elsewhere. */");
    line("private void write(Writer out, boolean collection) {");
    ++level;
    line("if (out.json() || (!collection && out.protocol == Protocol.BINARY_STRING)) { "
         "out.text(wireName); }");
    line("else { out.compact(ordinal()); }");
    --level;
    line("}");
    line("/** Decode only declared enum values. */");
    line("private static " + name + " read(Reader in, boolean collection) {");
    ++level;
    line("if (in.json() || (!collection && in.protocol == Protocol.BINARY_STRING)) {");
    ++level;
    line("switch (in.text()) {");
    ++level;
    for (const auto& item : value.enum_name_list) {
      line("case " + quote(item) + ": return " + constant(item) + ";");
    }
    line("default: throw in.error(\"Unknown enum name\");");
    --level;
    line("}");
    --level;
    line("}");
    line("switch (in.compact()) {");
    ++level;
    for (std::size_t index = 0; index < value.enum_name_list.size(); ++index) {
      line("case " + std::to_string(index) + ": return " + constant(value.enum_name_list[index]) +
           ";");
    }
    line("default: throw in.error(\"Unknown enum value\");");
    --level;
    line("}");
    --level;
    line("}");
    --level;
    line("}");
  }

  // Generate pure/native methods or a checked external behavior interface.
  void behaviors(const class_node& value) {
    behavior::require_native_names(value, "java", options.rename_identifiers);
    std::vector<const function_declaration*> external;
    for (const auto& method : value.functions) {
      if (!method.expression && !behavior::native_body(method, "java")) { external.push_back(&method); }
    }
    for (const auto* method : behavior::contracts(value)) {
      if (std::find(external.begin(), external.end(), method) == external.end()) { external.push_back(method); }
    }
    behavior::require_unique_callable_names(value, "java", [&](auto n) { return field(n); }, [&](const auto& t) { return type(t); });
    if (!external.empty()) {
      line("public interface Behavior {"); ++level;
      for (const auto* method : external) {
        std::string parameters = type_name(value.name) + " receiver";
        for (const auto& argument : method->parameters) {
          parameters += ", " + type(argument.type) + " " + field(argument.name);
        }
        line((method->return_type.name == "void" ? "void" : type(method->return_type)) + " " +
            field(method->name) + "(" + parameters + ");");
      }
      --level; line("}");
      line("private transient Behavior srlBehavior;");
      line("public void attachBehavior(Behavior behavior) { srlBehavior = java.util.Objects.requireNonNull(behavior); }");
      line("public static final class BehaviorAdapter {"); ++level;
      line("private final " + type_name(value.name) + " receiver;");
      line("private final Behavior implementation;");
      line("public BehaviorAdapter(" + type_name(value.name) + " receiver, Behavior implementation) { this.receiver = java.util.Objects.requireNonNull(receiver); this.implementation = java.util.Objects.requireNonNull(implementation); }");
      for (const auto* method : external) {
        std::string parameters, calls = "receiver";
        for (const auto& argument : method->parameters) {
          parameters += (parameters.empty() ? "" : ", ") + type(argument.type) + " " + field(argument.name);
          calls += ", " + field(argument.name);
        }
        const auto result_type = method->return_type.name == "void" ? "void" : type(method->return_type);
        line("public " + result_type + " " + field(method->name) + "(" + parameters + ") { " +
            (result_type == "void" ? "" : "return ") + "implementation." + field(method->name) + "(" + calls + "); }");
      }
      --level; line("}");
    }
    for (const auto& item : behavior::ordered_body(value)) {
      if (item.kind == class_body_item::kind_type::native_code) {
        const auto& block = value.native_blocks.at(item.index);
        if (block.language == "java") { source += behavior::source_marker(block, "java") + block.text + "\n"; }
        continue;
      }
      if (item.kind != class_body_item::kind_type::function) { continue; }
      const auto& method = value.functions.at(item.index);
      line(behavior::source_marker(method, "java").substr(0, behavior::source_marker(method, "java").size() - 1));
      const auto return_type = method.return_type.name == "void" ? "void" : type(method.return_type);
      std::string parameters, arguments = "this";
      for (const auto& argument : method.parameters) {
        if (!parameters.empty()) { parameters += ", "; }
        parameters += type(argument.type) + " " + field(argument.name);
        arguments += ", " + field(argument.name);
      }
      line(std::string(method.access == access_type::public_access ? "public " : "private ") +
          return_type + " " + field(method.name) + "(" + parameters + ") {"); ++level;
      if (method.dispatch != function_declaration::dispatch_type::ordinary &&
          (method.expression || behavior::native_body(method, "java"))) {
        line("if (srlBehavior != null) { " + std::string(return_type == "void" ? "" : "return ") +
            "srlBehavior." + field(method.name) + "(" + arguments + "); " + (return_type == "void" ? "return;" : "") + " }");
      }
      if (const auto* body = behavior::native_body(method, "java")) {
        source += behavior::source_marker(*body, "java") + body->text + "\n";
      } else if (method.expression) {
        const auto symbol = [&](std::string_view name) {
          const auto argument = std::find_if(method.parameters.begin(), method.parameters.end(),
              [&](const auto& item) { return item.name == name; });
          return argument == method.parameters.end() ? "this." + field(name) : field(name);
        };
        if (!method.predicate_body) {
          std::set<std::string> symbols; behavior::expression_symbols(*method.expression, symbols);
          for (const auto& input : symbols) {
            const auto scalar = behavior::symbol_type(value, method, input);
            const auto field_input = std::find_if(value.member_list.begin(), value.member_list.end(), [&](const auto& item) { return item.name == input; });
            if ((scalar != "float" && scalar != "double") || (field_input != value.member_list.end() && field_input->modifier != member::modifier_type::none)) { continue; }
            line("if (!Double.isFinite(" + symbol(input) + ")) { throw new ArithmeticException(\"Non-finite input\"); }");
          }
        }
        behavior::expression_context context;
        context.member_name = [&](std::string_view name) { return field(name); };
        context.native_type = [&](const auto& item) { return type(item); };
        context.enum_value = [&](const auto& item, std::string_view name) { return type(item) + "." + constant(name); };
        context.call = [&](const behavior_expression& node, const std::vector<std::string>& arguments) {
          if (node.call_constructor) { return behavior::constructor_expression(node, arguments, "java",
              [&](const auto& value) { return type(value); }, [&](auto name) { return field(name); }); }
          const auto& target = behavior::called_function(node);
          std::string receiver = node.call_receiver ? "(" + arguments.at(0) + ")" : "this";
          for (const auto base : node.call_base_path) { receiver += ".base" + std::to_string(base); }
          const auto callable = node.call_target->type == object_type::function_type ? names.at(node.call_target) : receiver + "." + field(target.name);
          return callable + "(" + behavior::call_arguments(arguments, node.call_receiver ? 1 : 0) + ")";
        };
        context.predicate = method.predicate_body;
        while (std::any_of(method.parameters.begin(), method.parameters.end(), [&](const auto& parameter) {
          return field(parameter.name) == context.budget_name;
        })) { context.budget_name += '_'; }
        if (behavior::expression_has_reduction(*method.expression)) { line("long[] " + context.budget_name + " = {" + std::to_string(behavior_expression::maximum_reduction_visits) + "};"); }
        if (method.predicate_body) { line("try {"); ++level; }
        line(return_type + " srlBehaviorResult = " + behavior::expression(*method.expression, "java", symbol, arithmetic_helpers, context) + ";");
        if (method.return_type.name == "float" || method.return_type.name == "double") { line("if (!Double.isFinite(srlBehaviorResult)) { throw new ArithmeticException(\"Non-finite result\"); }"); }
        line("return srlBehaviorResult;");
        if (method.predicate_body) { --level; line("} catch (RuntimeException srlFailure) { return false; }"); }
      } else {
        line("if (srlBehavior == null) { throw new IllegalStateException(\"Attach the required behavior before invocation\"); }");
        line(std::string(return_type == "void" ? "" : "return ") + "srlBehavior." + field(method.name) + "(" + arguments + ");");
      }
      --level; line("}");
    }
  }

  // Compare selected owned values lexicographically without treating NaN as an ordered value.
  void comparisons(const class_node& value) {
    const auto* settings = comparison::find(value, options.class_comparisons);
    if (!settings || (!settings->equality && !settings->ordering)) { return; }
    const auto selected = comparison::parts(value, *settings);
    struct operand {
      const rohit::serializer::type_name* type;
      bool record;
      std::string left, right;
    };
    std::vector<operand> operands;
    for (std::size_t index{}; index < value.parents.size(); ++index) {
      if (std::find(selected.bases.begin(), selected.bases.end(), &value.parents[index]) != selected.bases.end()) {
        operands.push_back({nullptr, true, "this.base" + std::to_string(index), "other.base" + std::to_string(index)});
      }
    }
    for (const auto* member : selected.fields) {
      const auto& type = member->type_name_list.front();
      const bool record = type.resolved_node && (type.resolved_node->type == object_type::class_type ||
                                               type.resolved_node->type == object_type::generic_definition);
      operands.push_back({&type, record, "this." + field(member->name), "other." + field(member->name)});
    }
    const auto name = type_name(value.name);
    if (settings->equality) {
      line("/** Compare selected values; NaN remains unequal, including to itself. */");
      line("public boolean equals_value(" + name + " other) {"); ++level;
      line("if (other == null) { return false; }");
      for (const auto& part : operands) {
        const auto equal = part.record ? part.left + ".equals_value(" + part.right + ")" :
            part.type->name == "string" ? part.left + ".equals(" + part.right + ")" :
            part.left + " == " + part.right;
        line("if (!(" + equal + ")) { return false; }");
      }
      line("return true;"); --level; line("}");
    }
    if (settings->ordering) {
      line("/** Return -1, 0 or 1 lexicographically, or null for an unordered compared value. */");
      line("public java.lang.Integer compare_value(" + name + " other) {"); ++level;
      line("if (other == null) { return null; }");
      std::size_t index{};
      for (const auto& part : operands) {
        const auto result = "srlComparison" + std::to_string(index++);
        if (part.record) {
          line("java.lang.Integer " + result + " = " + part.left + ".compare_value(" + part.right + ");");
          line("if (" + result + " == null || " + result + " != 0) { return " + result + "; }");
          continue;
        }
        const auto& type = *part.type;
        if (type.name == "float" || type.name == "double") {
          const auto native = type.name == "float" ? "java.lang.Float" : "java.lang.Double";
          line("if (" + std::string{native} + ".isNaN(" + part.left + ") || " + native + ".isNaN(" + part.right + ")) { return null; }");
        }
        if (type.name == "bool") {
          line("if (" + part.left + " != " + part.right + ") { return " + part.left + " ? 1 : -1; }");
        } else if (type.name == "string" || type.type == object_type::enum_type || type.name == "uint64") {
          const auto compare = type.name == "string" ? outer + ".compareText(" + part.left + ", " + part.right + ")" :
              type.name == "uint64" ? "java.lang.Long.compareUnsigned(" + part.left + ", " + part.right + ")" :
              "java.lang.Integer.compare(" + part.left + ".ordinal(), " + part.right + ".ordinal())";
          line("int " + result + " = " + compare + ";");
          line("if (" + result + " < 0) { return -1; }");
          line("if (" + result + " > 0) { return 1; }");
        } else {
          const auto convert = [&](const std::string& source) {
            return type.name == "uint8" || type.name == "char" ? "java.lang.Byte.toUnsignedInt(" + source + ")" :
                type.name == "uint16" ? "java.lang.Short.toUnsignedInt(" + source + ")" :
                type.name == "uint32" ? "java.lang.Integer.toUnsignedLong(" + source + ")" : source;
          };
          line("if (" + convert(part.left) + " < " + convert(part.right) + ") { return -1; }");
          line("if (" + convert(part.left) + " > " + convert(part.right) + ") { return 1; }");
        }
      }
      line("return 0;"); --level; line("}");
    }
  }

  // Expose checked logical array access without changing field ownership or codecs.
  void array_access(const class_node& owner) {
    for (const auto& item : owner.member_list) {
      if (!item.array_exposure) { continue; }
      const auto& exposure = *item.array_exposure;
      const auto storage = "this." + field(item.name);
      const bool native_array = item.modifier == member::modifier_type::array;
      const auto count = storage + (native_array ? ".length" : ".size()");
      const auto& element = item.modifier == member::modifier_type::array ?
          item.type_name_list.front() : item.type_name_list.front().arguments.front();
      const auto visibility = exposure.access == access_type::public_access ? "public " :
          exposure.access == access_type::protected_access ? "protected " : "private ";
      std::vector<std::uint64_t> dimensions;
      for (const auto& dimension : exposure.dimensions) {
        if (dimension.kind != dimension_expression::operation::literal) {
          throw std::invalid_argument{"java: array exposure dimensions must be resolved"};
        }
        dimensions.push_back(dimension.value);
      }
      const auto rank = dimensions.empty() ? std::size_t{1} : dimensions.size();
      std::string parameters;
      std::string offset;
      for (std::size_t index = 0; index < rank; ++index) {
        const auto coordinate = "index" + std::to_string(index);
        parameters += (parameters.empty() ? "" : ", ") + std::string{"int "} + coordinate;
        offset = offset.empty() ? coordinate :
            "(" + offset + " * " + std::to_string(dimensions[index]) + " + " + coordinate + ")";
      }
      line("/** Return the validated logical length of the selected storage. */");
      line(std::string{visibility} + "int " + field("size") + "() {");
      ++level;
      line("if (" + storage + " == null) { throw new java.lang.IllegalStateException(\"Null array storage\"); }");
      if (item.fixed_extent != 0) {
        line("if (" + count + " != " + std::to_string(item.fixed_extent) +
            ") { throw new java.lang.IllegalStateException(\"Invalid fixed array length\"); }");
      }
      line("return " + count + ";");
      --level;
      line("}");
      const auto bounds = [&]() {
        line("int count = " + field("size") + "();");
        for (std::size_t index = 0; index < rank; ++index) {
          const auto coordinate = "index" + std::to_string(index);
          const auto extent = dimensions.empty() ? "count" : std::to_string(dimensions[index]);
          line("if (" + coordinate + " < 0 || " + coordinate + " >= " + extent +
              ") { throw new java.lang.IndexOutOfBoundsException(\"Array coordinate out of bounds\"); }");
        }
      };
      line("/** Read one logical element; object elements retain their ordinary reference semantics. */");
      line(std::string{visibility} + type(element) + " " + field("get") + "(" + parameters + ") {");
      ++level;
      bounds();
      line("return " + storage + (native_array ? "[" + offset + "]" : ".get(" + offset + ")") + ";");
      --level;
      line("}");
      if (!exposure.readonly) {
        line("/** Replace one element after validating the storage and every coordinate. */");
        line(std::string{visibility} + "void " + field("set") + "(" + parameters + ", " + type(element) + " item) {");
        ++level;
        bounds();
        line(storage + (native_array ? "[" + offset + "] = item" : ".set(" + offset + ", item)") + ";");
        --level;
        line("}");
      }
      line("/** Allocate an iterable snapshot; object elements are shared, not deeply copied. */");
      line(std::string{visibility} + "java.util.List<" + type(element, true) + "> " + field("values") + "() {");
      ++level;
      line("int count = " + field("size") + "();");
      line("java.util.ArrayList<" + type(element, true) + "> result = new java.util.ArrayList<>(count);");
      line("for (int index = 0; index < count; ++index) { result.add(" + storage +
          (native_array ? "[index]" : ".get(index)") + "); }");
      line(std::string{"return "} + (exposure.readonly ? "java.util.Collections.unmodifiableList(result)" : "result") + ";");
      --level;
      line("}");
    }
  }

  // Emit a directly encoded owning class; schema parents are explicit composed fields.
  // Reuse the owning target codec with readonly getter expressions and no target allocation.
  std::string translator_codecs(const conversion_declaration& method, std::string_view) {
    auto saved_source = std::move(source);
    const auto saved_level = level;
    source.clear();
    level = 2;
    const auto& target = ::rohit::serializer::conversion::record(method.target);
    auto projected = translator::output_fields(method, "java", [&](auto value, auto) { return field(value); });
    if (target.has_magic()) {
      line("private static final " + std::string{target.magic_field ? type(target.magic_field->type_name_list.front()) : "java.util.List<Byte>"} + " MAGIC = " + type(method.target) + ".MAGIC;");
    }
    line("/** Encode projected fields without constructing the target model. */");
    line("public byte[] encode(Protocol protocol) {"); ++level;
    if (options.protocols == output_protocols::binary_none) {
      line("if (protocol != Protocol.BINARY_NONE) throw new IllegalArgumentException(\"Protocol disabled during generation\");");
    }
    line("Writer out = new Writer(protocol); write(out); return out.bytes.toByteArray();");
    --level; line("}");
    line("/** Traverse the target's keys and readonly field projections. */");
    line("private void write(Writer out) {"); ++level;
    projection_fields_ = std::move(projected);
    write_output_body(target);
    projection_fields_.clear();
    --level; line("}");
    auto result = std::move(source);
    source = std::move(saved_source);
    level = saved_level;
    return result;
  }

  // Encode owning or projected fields through the same target traversal.
  void write_output_body(const class_node& value) {
    if (value.reference && value.reference->value_projection) {
      line("var srlSelected = readValue();");
      write_field(value.member_list.front(), "srlSelected");
      return;
    }
    if (value.reference) { line("srlValidateReferenceAddress();"); }
    const auto sized_fields = auto_size::fields(value);
    if (!sized_fields.empty()) {
      line("int autoSizeStart = out.bytes.size();");
      for (const auto* item : sized_fields) {
        line("int autoSizePosition" + std::to_string(item->id) + " = -1;");
        if (item->size_scope == auto_size_scope::payload) {
          line("int autoSizeFieldBytes" + std::to_string(item->id) + " = 0;");
        }
      }
    }
    if (value.version_member()) {
      check_version(value, false);
    }
    if (value.has_magic()) {
      const auto omitted = omitted_condition(value.magic_omitted_formats, "out.protocol",
                                              "Protocol.", protocol_name_style::upper_snake);
      line("if (out.protocol != Protocol.JSON" +
           (omitted.empty() ? "" : " && !(" + omitted + ")") + ") {");
      ++level;
      if (value.magic_field) {
        write_value(value.magic_field->type_name_list.front(), "MAGIC", false);
      } else {
        for (const auto byte : value.magic_bytes) {
          line("out.integer(" + std::to_string(static_cast<unsigned char>(byte)) + ", 1, true);");
        }
      }
      --level;
      line("}");
    }
    line("out.beginObject();");
    bool first = true;
    const bool variable_first = std::any_of(value.member_list.begin(), value.member_list.end(),
                                           [](const auto& item) { return item.omits("json"); });
    if (variable_first) {
      line("boolean firstField = true;");
    }
    const auto first_flag = [&](bool initial) {
      return variable_first ? std::string{"firstField"} : std::string{initial ? "true" : "false"};
    };
    const bool magic_json = value.has_magic() && !value.magic_omits("json");
    if (magic_json) {
      line("if (out.json()) {");
      ++level;
      line("out.field(" + std::to_string(value.magic_id) + ", \"magic\", true);");
      if (value.magic_field) {
        write_value(value.magic_field->type_name_list.front(), "MAGIC", false);
      } else {
        line("out.text(" + magic_text(value) + ");");
      }
      if (variable_first) { line("firstField = false;"); }
      --level;
      line("}");
      first = false;
    }
    if (const auto* version = value.version_member()) {
      line("out.field(" + std::to_string(version->id) + ", " + quote(version->display_name) +
           ", " + first_flag(first) + ");");
      write_field(*version);
      if (variable_first) { line("firstField = false;"); }
      first = false;
    }
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      const auto& base = value.parents[index];
      line("out.field(" + std::to_string(base.id) + ", " + quote(base.display_name) + ", " +
           first_flag(first) + ");");
      line("this." + field("base" + std::to_string(index)) + ".write(out);");
      if (variable_first) { line("firstField = false;"); }
      first = false;
    }
    for (const auto& item : wire_members(value)) {
      if (item.version) {
        continue;
      }
      const auto* version = value.version_member();
      const auto active = version ? version_active(version_language::java, item, *version,
                                                   "this." + field(version->name))
                                  : std::string{};
      if (!active.empty()) {
        line("if (" + active + ") {");
        ++level;
      }
      const auto omitted = omitted_condition(item.omitted_formats, "out.protocol", "Protocol.",
                                              protocol_name_style::upper_snake);
      if (!omitted.empty()) {
        line("if (!(" + omitted + ")) {");
        ++level;
      }
      line("{");
      ++level;
      if (item.modifier == member::modifier_type::variant) {
        line("switch (this." + field(item.name) + "Index) {");
        ++level;
        for (std::size_t index = 0; index < item.type_name_list.size(); ++index) {
          const auto& alternative = item.type_name_list[index];
          line("case " + std::to_string(index) + ": {");
          ++level;
          line("out.field(" + std::to_string(item.id) + ", " +
               quote(item.display_name + ":" + alternative.enum_name) + ", " +
               first_flag(first) + ");");
          line("if (out.protocol == Protocol.BINARY_NONE || out.protocol == "
               "Protocol.BINARY_INTEGER) { out.compact(" +
               std::to_string(index) + "); }");
          write_value(alternative, "this." + field(item.name) + type_name(alternative.enum_name),
                      true);
          line("break;");
          --level;
          line("}");
        }
        line("default: throw new java.lang.IllegalArgumentException(\"Unknown union "
             "alternative\");");
        --level;
        line("}");
      } else {
        if (item.size_scope == auto_size_scope::payload && !item.auto_size_formats.empty()) {
          line("int autoSizeFieldStart = out.bytes.size();");
        }
        line("out.field(" + std::to_string(item.id) + ", " + quote(item.display_name) + ", " +
             first_flag(first) + ");");
        if (item.auto_size_formats.empty()) {
          write_field(item);
        } else {
          const auto selected = omitted_condition(item.auto_size_formats, "out.protocol", "Protocol.",
              protocol_name_style::upper_snake);
          line("if (" + selected + ") {");
          ++level;
          line("autoSizePosition" + std::to_string(item.id) + " = out.bytes.size();");
          line("out.integer(0, " + width(item.type_name_list.front().name) + ", true);");
          if (item.size_scope == auto_size_scope::payload) {
            line("autoSizeFieldBytes" + std::to_string(item.id) + " = out.bytes.size() - autoSizeFieldStart;");
          }
          --level;
          line("} else {");
          ++level;
          if (item.wire_only) {
            line("throw new java.lang.IllegalArgumentException(\"Inactive wire_only field\");");
          } else {
            write_field(item);
          }
          --level;
          line("}");
        }
      }
      --level;
      line("}");
      if (variable_first) { line("firstField = false;"); }
      if (!omitted.empty()) {
        --level;
        line("}");
      }
      if (!active.empty()) {
        --level;
        line("}");
      }
      first = false;
    }
    line("out.endObject();");
    for (const auto* item : sized_fields) {
      const auto position = "autoSizePosition" + std::to_string(item->id);
      line("if (" + position + " >= 0) {");
      ++level;
      // JVM byte buffers are Int bounded, so every positive span fits uint32 and uint64.
      line("out.bytes.patchInteger(" + position + ", out.bytes.size() - autoSizeStart" +
          (item->size_scope == auto_size_scope::payload ? " - autoSizeFieldBytes" + std::to_string(item->id) : "") + ", " +
          width(item->type_name_list.front().name) + ");");
      --level;
      line("}");
    }
  }

  void object(const class_node& value) {
    if (value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        (value.attributes & class_attributes::packed) != class_attributes::none) {
      throw std::invalid_argument{"Java output supports owning classes only; remove view/packed: " +
                                  value.name};
    }
    std::set<std::string> fields{};
    const auto add_field = [&](const std::string& name) {
      if (!fields.insert(name).second || name == "ReadPolicy" || name == "Protocol" ||
          name == "Limits" || name == outer) {
        throw std::invalid_argument{"Java field name collision: " + name};
      }
    };
    for (const auto& item : value.member_list) {
      if (!item.array_exposure) { continue; }
      for (const auto method : {"get", "size", "values"}) { add_field(field(method)); }
      if (!item.array_exposure->readonly) { add_field(field("set")); }
      for (const auto& method : value.functions) {
        if (fields.contains(field(method.name))) {
          throw std::invalid_argument{"Java array exposure method collision: " + field(method.name)};
        }
      }
      for (const auto& factory : value.constructions) {
        if (fields.contains(field(factory.constructor ? "create" : factory.name))) {
          throw std::invalid_argument{"Java array exposure factory collision"};
        }
      }
      for (const auto& conversion : value.conversions) {
        if (fields.contains(field(conversion.name))) {
          throw std::invalid_argument{"Java array exposure conversion collision"};
        }
      }
    }
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      add_field("base" + std::to_string(index));
    }
    for (const auto& item : value.member_list) {
      if (item.wire_only) { continue; }
      const auto name = field(item.name);
      for (const auto& alternative : item.type_name_list) {
        if (alternative.resolved_node == &value &&
            (item.modifier == member::modifier_type::none ||
             item.modifier == member::modifier_type::variant)) {
          throw std::invalid_argument{
              "Java owning defaults cannot directly contain their own class: " + value.name};
        }
      }
      if (item.modifier == member::modifier_type::variant) {
        add_field(name + "Index");
        for (const auto& alternative : item.type_name_list) {
          add_field(name + type_name(alternative.enum_name));
        }
      } else {
        add_field(name);
      }
    }
    const auto name = type_name(value.name);
    if (std::any_of(value.member_list.begin(), value.member_list.end(), [](const auto& field) {
          const auto& item = field.type_name_list.front();
          return field.modifier == member::modifier_type::array &&
                 (item.is_optional() || item.is_sequence() || item.is_associative());
        })) {
      // Java cannot create arrays of parameterized types; the emitted cast preserves the API.
      line("@SuppressWarnings({\"unchecked\", \"rawtypes\"})");
    }
    line("public static final class " + name + " {");
    ++level;
    if (value.magic_field) {
      const auto& magic = *value.magic_field;
      const auto visibility = value.magic_access == access_type::public_access ? "public " :
          value.magic_access == access_type::protected_access ? "protected " : "private ";
      line("/** Immutable scalar schema identity without instance storage. */");
      line(std::string(visibility) + "static final " + type(magic.type_name_list.front()) +
           " MAGIC = " + default_value(magic) + ";");
    } else if (value.has_magic()) {
      std::string bytes;
      for (const auto byte : value.magic_bytes) {
        bytes += (bytes.empty() ? "" : ", ") + std::string("(byte)") +
                 std::to_string(static_cast<unsigned char>(byte));
      }
      const auto visibility = value.magic_access == access_type::public_access ? "public " :
          value.magic_access == access_type::protected_access ? "protected " : "private ";
      line("/** Immutable schema identity without instance storage. */");
      line(std::string(visibility) + "static final java.util.List<Byte> MAGIC = "
           "java.util.List.of(" + bytes + ");");
    }
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      const auto base_type = names.at(value.parents[index].parent_class);
      const auto access = value.parents[index].access;
      const std::string visibility = access == access_type::private_access     ? "private "
                                     : access == access_type::protected_access ? "protected "
                                                                               : "public ";
      line(visibility + base_type + " base" + std::to_string(index) + " = new " + base_type +
           "();");
    }
    for (const auto& item : value.member_list) {
      if (!item.wire_only && !(value.reference && value.reference->value_projection)) { declaration(item); }
    }
    array_access(value);
    behaviors(value);
    source += reference_support::portable_methods(value, name, "java",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto) { return field(item); });
    source += construction::factories(value, name, "java",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto) { return field(item); },
        [&](const auto& item) { return default_value(item); });
    source += conversion::methods(value, name, "java",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto) { return field(item); });
    source += translator::methods(value, name, "java",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto) { return field(item); },
        [&](const auto& item) { return default_value(item); },
        [&](const auto& method, auto view) { return translator_codecs(method, view); });
    comparisons(value);
    xml_object(value);
    line("/** Encode this value into an independent exact-size message. */");
    line("public byte[] encode(Protocol protocol) {");
    ++level;
    if (options.protocols == output_protocols::binary_none) {
      line("if (protocol != Protocol.BINARY_NONE) throw new IllegalArgumentException(\"Protocol disabled during generation\");");
    }
    line("Writer out = new Writer(protocol);");
    line("write(out);");
    line("return out.bytes.toByteArray();");
    --level;
    line("}");
    line("/** Decode one exact message using default resource limits. */");
    line("public static " + name + " decode(byte[] bytes, Protocol protocol) {");
    ++level;
    line("return decode(bytes, protocol, Limits.defaults());");
    --level;
    line("}");
    line("/** Decode transactionally into a fresh value; malformed input throws. */");
    line("public static " + name + " decode(byte[] bytes, Protocol protocol, Limits limits) {");
    ++level;
    if (options.protocols == output_protocols::binary_none) {
      line("if (protocol != Protocol.BINARY_NONE) throw new IllegalArgumentException(\"Protocol disabled during generation\");");
    }
    line("Reader in = new Reader(bytes, protocol, limits);");
    line(name + " result = read(in);");
    line("in.finish();");
    line("return result;");
    --level;
    line("}");
    line("/** Write schema fields in declaration order. */");
    line("private void write(Writer out) {");
    ++level;
    write_output_body(value);
    const auto sized_fields = auto_size::fields(value);
    const bool magic_json = value.has_magic() && !value.magic_omits("json");
    --level;
    line("}");
    line("/** Read known fields only; keyed omissions retain schema defaults. */");
    line("private static " + name + " read(Reader in) {");
    ++level;
    line("return read(in, new " + name + "());");
    --level;
    line("}");
    line("/** Merge duplicate nested fields in input order within this fresh message. */");
    line("private static " + name + " read(Reader in, " + name + " result) {");
    ++level;
    if (value.reference && value.reference->value_projection) {
      line("throw new IllegalStateException(\"Export references cannot be decoded; use the ordinary owning type\");");
      --level; line("}");
      --level; line("}");
      return;
    }
    if (value.reference) { line("result.unbind();"); }
    if (!sized_fields.empty() && auto_size::verifies(value, options.class_auto_sizes)) {
      line("int autoSizeStart = in.offset;");
    }
    for (const auto* item : sized_fields) {
      line("boolean autoSizeSeen" + std::to_string(item->id) + " = false;");
      if (item->wire_only) { line(type(item->type_name_list.front()) + " autoSizeValue" + std::to_string(item->id) + " = 0;"); }
      if (item->size_scope == auto_size_scope::payload && auto_size::verifies(value, options.class_auto_sizes)) {
        line("int autoSizeFieldBytes" + std::to_string(item->id) + " = 0;");
      }
    }
    const bool payload_size = auto_size::verifies(value, options.class_auto_sizes) &&
        std::any_of(sized_fields.begin(), sized_fields.end(), [](const auto* item) {
      return item->size_scope == auto_size_scope::payload;
    });
    if (payload_size) { line("int autoSizeFieldStart = in.offset;"); }
    if (value.has_magic()) {
      const auto omitted = omitted_condition(value.magic_omitted_formats, "in.protocol",
                                              "Protocol.", protocol_name_style::upper_snake);
      line("if (in.protocol != Protocol.JSON" +
           (omitted.empty() ? "" : " && !(" + omitted + ")") + ") {");
      ++level;
      if (value.magic_field) {
        line("if (" + read_value(value.magic_field->type_name_list.front(), "in", false) +
             " != MAGIC) { throw in.error(\"Incorrect schema magic\"); }");
      } else {
        for (const auto byte : value.magic_bytes) {
          line("if (in.integer(1, true) != " + std::to_string(static_cast<unsigned char>(byte)) +
               ") { throw new java.lang.IllegalArgumentException(\"Incorrect schema magic\"); }");
        }
      }
      --level;
      line("}");
    }
    if (magic_json) { line("boolean magicSeen = false;"); }
    if (value.version_member()) {
      line("boolean[] seen = new boolean[" +
           std::to_string(value.parents.size() + value.member_list.size()) + "];");
    }
    line("in.beginObject();");
    line("if (in.protocol == Protocol.BINARY_NONE) {");
    ++level;
    if (const auto* version = value.version_member()) {
      read_versioned_member(value, *version, -1, true);
    }
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      line("result.base" + std::to_string(index) + " = " +
           names.at(value.parents[index].parent_class) + ".read(in, result.base" +
           std::to_string(index) + ");");
    }
    for (const auto& item : wire_members(value)) {
      if (item.version) {
        continue;
      }
      line("{");
      ++level;
      if (payload_size) { line("autoSizeFieldStart = in.offset;"); }
      read_versioned_member(value, item, -1, true);
      --level;
      line("}");
    }
    --level;
    if (options.protocols == output_protocols::all) {
      line("} else if (in.protocol == Protocol.BINARY_INTEGER) {");
      ++level;
      line("while (true) {");
      ++level;
      if (payload_size) { line("autoSizeFieldStart = in.offset;"); }
      line("int key = in.compact();");
      line("if (key == 0) { break; }");
      line("switch (key) {");
      ++level;
      for (std::size_t index = 0; index < value.parents.size(); ++index) {
        line("case " + std::to_string(value.parents[index].id) + ": result.base" +
             std::to_string(index) + " = " + names.at(value.parents[index].parent_class) +
             ".read(in, result.base" + std::to_string(index) + "); break;");
      }
      for (const auto& item : wire_members(value)) {
        line("case " + std::to_string(item.id) + ": {");
        ++level;
        read_versioned_member(value, item);
        line("break;");
        --level;
        line("}");
      }
      line("default: throw in.error(\"Unknown field ID\");");
      --level;
      line("}");
      --level;
      line("}");
      --level;
      line("} else {");
      ++level;
      line("boolean first = true;");
      line("while (!in.json() || in.nextObject(first)) {");
      ++level;
      line("first = false;");
      if (payload_size) { line("autoSizeFieldStart = in.offset;"); }
      line("String key = in.key();");
      line("if (!in.json() && key.isEmpty()) { break; }");
      line("switch (key) {");
      ++level;
      if (magic_json) {
        line("case \"magic\": {");
        ++level;
        const auto comparison = value.magic_field
            ? read_value(value.magic_field->type_name_list.front(), "in", false) + " != MAGIC"
            : "!in.text().equals(" + magic_text(value) + ")";
        line("if (!in.json() || magicSeen || " + comparison +
             ") { throw in.error(\"Incorrect or duplicate schema magic\"); }");
        line("magicSeen = true; break;");
        --level;
        line("}");
      }
      if (value.has_magic() && !magic_json) {
        line("case \"magic\": throw in.error(\"Field omitted from selected format\");");
      }
      for (std::size_t index = 0; index < value.parents.size(); ++index) {
        line("case " + quote(value.parents[index].display_name) + ": result.base" +
             std::to_string(index) + " = " + names.at(value.parents[index].parent_class) +
             ".read(in, result.base" + std::to_string(index) + "); break;");
      }
      for (const auto& item : wire_members(value)) {
        if (item.modifier == member::modifier_type::variant) {
          for (std::size_t index = 0; index < item.type_name_list.size(); ++index) {
            line("case " + quote(item.display_name + ":" + item.type_name_list[index].enum_name) +
                 ": {");
            ++level;
            read_versioned_member(value, item, static_cast<int>(index));
            line("break;");
            --level;
            line("}");
          }
        } else {
          line("case " + quote(item.display_name) + ": {");
          ++level;
          read_versioned_member(value, item);
          line("break;");
          --level;
          line("}");
        }
      }
      line("default: in.skipValue(); break;");
      --level;
      line("}");
      --level;
      line("}");
      --level;
      line("}");
    } else {
      line("}");
    }
    line("in.endObject();");
    for (const auto* item : sized_fields) {
      const auto selected = omitted_condition(item->auto_size_formats, "in.protocol", "Protocol.",
          protocol_name_style::upper_snake);
      line("if (" + selected + ") {");
      ++level;
      line("if (!autoSizeSeen" + std::to_string(item->id) +
          ") { throw in.error(\"Missing auto_size field\"); }");
      if (auto_size::verifies(value, options.class_auto_sizes)) {
        auto stored = item->wire_only ? "autoSizeValue" + std::to_string(item->id) : "result." + field(item->name);
        if (item->type_name_list.front().name == "uint32") { stored = "java.lang.Integer.toUnsignedLong(" + stored + ")"; }
        const auto adjustment = item->size_scope == auto_size_scope::payload
            ? " - autoSizeFieldBytes" + std::to_string(item->id) : "";
        line("if (" + stored + " != in.offset - autoSizeStart" + adjustment + ") { throw in.error(\"auto_size span mismatch\"); }");
      }
      --level;
      line("}");
    }
    if (magic_json) {
      line("if (in.json() && !magicSeen) { throw in.error(\"Missing schema magic\"); }");
    }
    if (const auto* version = value.version_member()) {
      line("if (!seen[" + std::to_string(version_slot(value, *version)) +
           "]) { throw in.error(\"Missing schema version\"); }");
      check_version(value, true);
      for (const auto& item : wire_members(value)) {
        const auto active = version_active(version_language::java, item, *version,
                                           "result." + field(version->name));
        if (!active.empty()) {
          line("if (in.protocol != Protocol.BINARY_NONE && seen[" +
               std::to_string(version_slot(value, item)) + "] && !(" + active +
               ")) { throw in.error(\"Field outside version lifecycle\"); }");
        }
      }
    }
    if (value.reference) { line("result.srlValidateReferenceAddress();"); }
    line("return result;");
    --level;
    line("}");
    --level;
    line("}");
  }

  // Publish readonly namespace functions as static methods on their namespace container.
  void free_function(const function_node& node) {
    const auto& method = node.function;
    const auto result_type = method.return_type.name == "void" ? "void" : type(method.return_type);
    std::string parameters;
    for (const auto& parameter : method.parameters) {
      if (!parameters.empty()) { parameters += ", "; }
      parameters += type(parameter.type) + " " + field(parameter.name);
    }
    line(behavior::source_marker(method, "java"));
    line("public static " + result_type + " " + field(method.name) + "(" + parameters + ") {"); ++level;
    if (const auto* native = behavior::native_body(method, "java")) {
      if (options.rename_identifiers) { throw std::invalid_argument{"java: opaque native functions require naming = preserve"}; }
      source += behavior::source_marker(*native, "java") + native->text + "\n";
    } else if (method.expression) {
      behavior::expression_context context;
      context.member_name = [&](auto value) { return field(value); };
      context.native_type = [&](const auto& value) { return type(value); };
      context.enum_value = [&](const auto& value, auto name) { return type(value) + "." + constant(name); };
      context.call = [&](const behavior_expression& call, const std::vector<std::string>& arguments) {
        if (call.call_constructor) { return behavior::constructor_expression(call, arguments, "java",
            [&](const auto& value) { return type(value); }, [&](auto name) { return field(name); }); }
        if (call.call_receiver) {
          std::string receiver = "(" + arguments.at(0) + ")";
          for (const auto base : call.call_base_path) { receiver += ".base" + std::to_string(base); }
          return receiver + "." + field(behavior::called_function(call).name) + "(" +
              behavior::call_arguments(arguments, 1) + ")";
        }
        return names.at(call.call_target) + "(" + behavior::call_arguments(arguments) + ")";
      };
      behavior::free_expression_body(method, "java", result_type, [&](auto value) { return field(value); },
          arithmetic_helpers, context, [&](const auto& text) { line(text); },
          [&](const auto& text) { line(text + " {"); ++level; }, [&]() { --level; line("}"); });
    } else { throw std::invalid_argument{"java: standalone functions require an expression or selected native body"}; }
    --level; line("}");
  }

  // Combine reopened namespaces into one Java container without changing the source AST order.
  void nodes(const std::vector<const syntax_node*>& values) {
    std::map<std::string, std::vector<const syntax_node*>> namespace_children{};
    for (const auto* value : values) {
      if (value->type == object_type::namespace_type) {
        auto& children = namespace_children[value->name];
        for (const auto& child : static_cast<const namespace_node&>(*value).statements) {
          children.push_back(child.get());
        }
      }
    }
    std::set<std::string> emitted_namespaces{};
    for (const auto& value : values) {
      if (value->type == object_type::namespace_type) {
        if (!emitted_namespaces.insert(value->name).second) {
          continue;
        }
        line("public static final class " + type_name(value->name) + " {");
        ++level;
        line("/** Namespace container; no instances are needed. */");
        line("private " + type_name(value->name) + "() {}");
        nodes(namespace_children.at(value->name));
        --level;
        line("}");
      } else if (value->type == object_type::enum_type) {
        enumeration(static_cast<const enum_node&>(*value));
      } else if (value->type == object_type::class_type) {
        object(static_cast<const class_node&>(*value));
      } else if (value->type == object_type::function_type) {
        free_function(static_cast<const function_node&>(*value));
      }
    }
  }

public:
  // Retain generation settings for this compilation unit only.
  emitter(const java_options& options, std::string_view outer_class)
      : options{options}, outer{outer_class}, xml_contract{options.xml} {}

  // Validate and generate the complete source without external formatting tools.
  std::string generate(const std::vector<std::unique_ptr<syntax_node>>& values) {
    behavior::require_callable_backend(values, "java");
    validate_identifier(outer);
    std::set<std::string> reserved{outer,    "ReadPolicy", "Protocol", "Limits",
                                         "Writer", "Reader",     "String"};
    if (options.xml.enabled) {
      for (const auto* helper : {"XmlSchema", "SrlXmlNode", "SrlXmlBudget", "SrlXmlFields",
                                "srlXmlName", "srlXmlTrim", "srlXmlParse", "srlXmlScalarText",
                                "srlXmlInteger", "srlXmlBoolean", "srlXmlFloat", "srlXmlFloatText",
                                "srlXmlWrite", "srlXmlEscape"}) {
        reserved.insert(helper);
      }
    }
    if (outer == "ReadPolicy" || outer == "Protocol" || outer == "Limits" || outer == "Writer" ||
        outer == "Reader" || outer == "String") {
      throw std::invalid_argument{"Java output filename conflicts with runtime type: " + outer};
    }
    if (options.xml.enabled && reserved.contains(outer) &&
        (is_xml_package_name(outer) || outer == "XmlSchema" || outer == "SrlXmlNode" ||
         outer == "SrlXmlBudget" || outer == "SrlXmlFields")) {
      throw std::invalid_argument{"Java XML output filename conflicts with a runtime helper"};
    }
    register_nodes(values, outer, reserved);
    std::vector<const syntax_node*> helper_nodes;
    for (const auto& [node, name] : names) { static_cast<void>(name); helper_nodes.push_back(node); }
    behavior::require_unique_free_names(helper_nodes, "java", [&](const auto& node) { return names.at(&node); },
        [&](const auto& value) { return type(value); });
    arithmetic_helpers = behavior::select_helpers(helper_nodes,
        [&](const auto& node) { return node.type == object_type::function_type ? field(node.name) : type_name(node.name); },
        [&](const auto name) { return field(name); });
    source = generated_license_notice(values, "//", true);
    if (!options.package_name.empty()) {
      std::size_t start{};
      while (start <= options.package_name.size()) {
        auto end = options.package_name.find('.', start);
        if (end == std::string::npos) {
          end = options.package_name.size();
        }
        validate_identifier(std::string_view{options.package_name}.substr(start, end - start));
        start = end + 1;
      }
      source += "package " + options.package_name + ";\n\n";
    }
    source += behavior::preambles(values, "java");
    source +=
        "/** Schema types and dependency-free codecs. */\npublic final class " + outer + " {\n";
    line("/** Static schema container. */");
    line("private " + outer + "() {}");
    source += behavior::helpers("java", arithmetic_helpers);
    // Runtime source uses two-space nesting; reindent for the selected profile.
    std::size_t start{};
    while (start < runtime_source.size()) {
      auto end = runtime_source.find('\n', start);
      if (end == std::string_view::npos) {
        end = runtime_source.size();
      }
      const auto current = runtime_source.substr(start, end - start);
      auto leading = current.find_first_not_of(' ');
      if (leading == std::string_view::npos) {
        leading = 0;
      }
      level = 1 + leading / 2;
      line(std::string{current.substr(leading)});
      start = end + 1;
    }
    level = 1;
    if (options.xml.enabled) { source += xml_runtime_source; }
    std::vector<const syntax_node*> roots{};
    roots.reserve(values.size());
    for (const auto& value : values) {
      roots.push_back(value.get());
    }
    nodes(roots);
    source += "}\n";
    return source;
  }
};
} // namespace

// Publish only a completely validated source file.
std::string generate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     std::string_view outer_class, const java_options& options) {
  xml_binding::validate_options(statements, options.xml);
  require_unmanaged_backend(statements);
  auto_size::validate(statements, options.class_auto_sizes);
  comparison::validate(statements, options.class_comparisons, true);
  emitter generator{options, outer_class};
  return behavior::finalize_mappings(generator.generate(statements));
}

// Retain source-file notices when no generated declaration can carry their metadata.
std::string generate_schema(const parser::parsed_schema& schema, std::string_view outer_class,
                            const java_options& options) {
  const auto source = generate(schema.statements, outer_class, options);
  return schema.statements.empty()
      ? generated_source_notices(schema.source_notices, "//", true) + source : source;
}
} // namespace rohit::serializer::writer::java
