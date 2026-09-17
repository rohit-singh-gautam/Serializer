#include <rohit/serializer_creator.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::java {
namespace {
constexpr std::string_view runtime_source =
#include "java_runtime.inc"
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
  std::string source{};
  std::size_t level{1};
  std::map<const syntax_node*, std::string> names{};
  std::map<std::string, const syntax_node*> declarations{};

  // Append one line with the selected profile's indentation.
  void line(const std::string& value = {}) {
    const std::size_t indent = options.standard == java_coding_standard::oracle ? 4 : 2;
    source.append(level * indent, ' ');
    source += value + '\n';
  }

  // Resolve a field or alternative name under the selected naming policy.
  std::string field(std::string_view value) const {
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
      const auto name = type_name(node->name);
      const auto qualified = prefix + "." + name;
      const auto [previous, inserted] = declarations.emplace(qualified, node.get());
      const bool reopened_namespace = !inserted && node->type == object_type::namespace_type &&
                                      previous->second->type == object_type::namespace_type &&
                                      previous->second->get_full_name() == node->get_full_name();
      if ((!inserted && !reopened_namespace) || enclosing.contains(name)) {
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
    return value.type == object_type::primitive ? primitive(value.name, boxed)
                                                : names.at(value.resolved_node);
  }

  // Construct a fresh schema default for a single value.
  std::string initial(const rohit::serializer::type_name& value) const {
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
    if (value.name == "string") {
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
    const auto& item = value.type_name_list.front();
    const auto first = value.default_value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
      return initial(item);
    }
    const auto last = value.default_value.find_last_not_of(" \t\r\n");
    const auto text = value.default_value.substr(first, last - first + 1);
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
    if (value.type == object_type::class_type) {
      return type(value) + ".read(" + reader + ")";
    }
    if (value.type == object_type::enum_type) {
      return type(value) + ".read(" + reader + ", " + (collection ? "true" : "false") + ")";
    }
    const auto access = "(" + reader + ").";
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
    if (value.type == object_type::class_type) {
      line(expression + ".write(out);");
    } else if (value.type == object_type::enum_type) {
      line(expression + ".write(out, " + (collection ? "true" : "false") + ");");
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

  // Declare one field, including explicit union alternatives and primitive arrays.
  void declaration(const member& value) {
    const auto& item = value.type_name_list.front();
    const std::string access = value.access == access_type::private_access     ? "private "
                               : value.access == access_type::protected_access ? "protected "
                                                                               : "public ";
    const auto name = field(value.name);
    if (value.modifier != member::modifier_type::none && !value.default_value.empty()) {
      throw std::invalid_argument{"Java collection/union defaults are not supported: " +
                                  value.name};
    }
    if (value.modifier == member::modifier_type::array) {
      line(access + type(item) + "[] " + name + " = new " + type(item) + "[0];");
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
  void write_field(const member& value) {
    const auto name = "this." + field(value.name);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      write_value(item, name, false);
      return;
    }
    if (value.modifier == member::modifier_type::array) {
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
      line(name + " = " +
           (item.type == object_type::class_type ? type(item) + ".read(in, " + name + ")"
                                                 : read_value(item, "in", false)) +
           ";");
      return;
    }
    line("int count = in.beginArray();");
    if (value.modifier == member::modifier_type::array) {
      std::string element_width{"0"};
      if (item.type == object_type::primitive && item.name != "string") {
        element_width = item.name == "bool" || item.name == "char" ? "1"
                        : item.name == "float"                     ? "4"
                        : item.name == "double"                    ? "8"
                                                                   : width(item.name);
      }
      line("if (!in.json()) {");
      ++level;
      line("in.requireArray(count, " + element_width + ");");
      line(name + " = new " + type(item) + "[count];");
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
      line("values.add(" + read_value(item, "in", true) + ");");
      --level;
      line("}");
      line(name + " = new " + type(item) + "[values.size()];");
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

  // Emit a directly encoded owning class; schema parents are explicit composed fields.
  void object(const class_node& value) {
    if (value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        (value.attributes & class_attributes::packed) != class_attributes::none) {
      throw std::invalid_argument{"Java output supports owning classes only; remove view/packed: " +
                                  value.name};
    }
    std::set<std::string> fields{};
    const auto add_field = [&](const std::string& name) {
      if (!fields.insert(name).second || name == "Protocol" || name == "Limits" || name == outer) {
        throw std::invalid_argument{"Java field name collision: " + name};
      }
    };
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      add_field("base" + std::to_string(index));
    }
    for (const auto& item : value.member_list) {
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
    line("public static final class " + name + " {");
    ++level;
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
      declaration(item);
    }
    line("/** Encode this value into an independent exact-size message. */");
    line("public byte[] encode(Protocol protocol) {");
    ++level;
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
    line("Reader in = new Reader(bytes, protocol, limits);");
    line(name + " result = read(in);");
    line("in.finish();");
    line("return result;");
    --level;
    line("}");
    line("/** Write schema fields in declaration order. */");
    line("private void write(Writer out) {");
    ++level;
    line("out.beginObject();");
    bool first = true;
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      const auto& base = value.parents[index];
      line("out.field(" + std::to_string(base.id) + ", " + quote(base.display_name) + ", " +
           (first ? "true" : "false") + ");");
      line("this.base" + std::to_string(index) + ".write(out);");
      first = false;
    }
    for (const auto& item : value.member_list) {
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
               (first ? "true" : "false") + ");");
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
        line("out.field(" + std::to_string(item.id) + ", " + quote(item.display_name) + ", " +
             (first ? "true" : "false") + ");");
        write_field(item);
      }
      --level;
      line("}");
      first = false;
    }
    line("out.endObject();");
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
    line("in.beginObject();");
    line("if (in.protocol == Protocol.BINARY_NONE) {");
    ++level;
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      line("result.base" + std::to_string(index) + " = " +
           names.at(value.parents[index].parent_class) + ".read(in, result.base" +
           std::to_string(index) + ");");
    }
    for (const auto& item : value.member_list) {
      line("{");
      ++level;
      if (item.modifier == member::modifier_type::variant) {
        read_union(item);
      } else {
        read_field(item);
      }
      --level;
      line("}");
    }
    --level;
    line("} else if (in.protocol == Protocol.BINARY_INTEGER) {");
    ++level;
    line("while (true) {");
    ++level;
    line("int key = in.compact();");
    line("if (key == 0) { break; }");
    line("switch (key) {");
    ++level;
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      line("case " + std::to_string(value.parents[index].id) + ": result.base" +
           std::to_string(index) + " = " + names.at(value.parents[index].parent_class) +
           ".read(in, result.base" + std::to_string(index) + "); break;");
    }
    for (const auto& item : value.member_list) {
      line("case " + std::to_string(item.id) + ": {");
      ++level;
      if (item.modifier == member::modifier_type::variant) {
        read_union(item);
      } else {
        read_field(item);
      }
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
    line("String key = in.key();");
    line("if (!in.json() && key.isEmpty()) { break; }");
    line("switch (key) {");
    ++level;
    for (std::size_t index = 0; index < value.parents.size(); ++index) {
      line("case " + quote(value.parents[index].display_name) + ": result.base" +
           std::to_string(index) + " = " + names.at(value.parents[index].parent_class) +
           ".read(in, result.base" + std::to_string(index) + "); break;");
    }
    for (const auto& item : value.member_list) {
      if (item.modifier == member::modifier_type::variant) {
        for (std::size_t index = 0; index < item.type_name_list.size(); ++index) {
          line("case " + quote(item.display_name + ":" + item.type_name_list[index].enum_name) +
               ": {");
          ++level;
          read_union(item, static_cast<int>(index));
          line("break;");
          --level;
          line("}");
        }
      } else {
        line("case " + quote(item.display_name) + ": {");
        ++level;
        read_field(item);
        line("break;");
        --level;
        line("}");
      }
    }
    line("default: throw in.error(\"Unknown field name\");");
    --level;
    line("}");
    --level;
    line("}");
    --level;
    line("}");
    line("in.endObject();");
    line("return result;");
    --level;
    line("}");
    --level;
    line("}");
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
      }
    }
  }

public:
  // Retain generation settings for this compilation unit only.
  emitter(const java_options& options, std::string_view outer_class)
      : options{options}, outer{outer_class} {}

  // Validate and generate the complete source without external formatting tools.
  std::string generate(const std::vector<std::unique_ptr<syntax_node>>& values) {
    validate_identifier(outer);
    const std::set<std::string> reserved{outer, "Protocol", "Limits", "Writer", "Reader", "String"};
    if (outer == "Protocol" || outer == "Limits" || outer == "Writer" || outer == "Reader" ||
        outer == "String") {
      throw std::invalid_argument{"Java output filename conflicts with runtime type: " + outer};
    }
    register_nodes(values, outer, reserved);
    source = "// Generated by Serializer. Requires Java 17 or newer.\n";
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
    source +=
        "/** Schema types and dependency-free codecs. */\npublic final class " + outer + " {\n";
    line("/** Static schema container. */");
    line("private " + outer + "() {}");
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
void write(stream& out_stream, const std::vector<std::unique_ptr<syntax_node>>& statements,
           std::string_view outer_class, const java_options& options) {
  emitter generator{options, outer_class};
  out_stream.write(generator.generate(statements));
}
} // namespace rohit::serializer::writer::java
