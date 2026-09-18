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

namespace rohit::serializer::writer::portable {
namespace {
constexpr std::string_view js_runtime =
#include "js_runtime.inc"
    ;
constexpr std::string_view go_runtime =
#include "go_runtime.inc"
    ;
constexpr std::string_view csharp_runtime =
#include "csharp_runtime.inc"
    ;
enum class target { js, go, csharp, typescript };

// Quote schema spelling without changing wire names or printable Unicode.
std::string quote(std::string_view text) {
  std::string result{"\""};
  for (const auto value : text) {
    switch (value) {
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
      if (static_cast<unsigned char>(value) < 0x20) {
        throw std::invalid_argument{"Control character in schema name"};
      }
      result += value;
    }
  }
  return result + '"';
}

// Convert schema identifiers to deterministic native presentation names.
std::string name(std::string_view text, bool upper, bool rename) {
  std::string result{};
  bool capitalize = upper && rename;
  for (const auto value : text) {
    if (rename && value == '_') {
      capitalize = true;
    } else {
      result +=
          capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(value))) : value;
      capitalize = false;
    }
  }
  if (rename && !upper && !result.empty()) {
    result.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(result.front())));
  }
  return result;
}

// Reject malformed identifiers and language keywords before output publication.
void validate_name(const std::string& value, target language) {
  constexpr std::string_view common =
      " break case const continue default else false for if return switch true var ";
  constexpr std::string_view js =
      " await class debugger delete do enum export extends finally function implements import in "
      "instanceof interface let new null package private protected public static super this throw "
      "try typeof void while with yield constructor prototype __proto__ ";
  constexpr std::string_view go =
      " chan defer fallthrough func go goto import interface map package range select struct type ";
  constexpr std::string_view cs =
      " abstract as base bool byte catch char checked class decimal delegate do double enum event "
      "explicit extern finally fixed float goto implicit in int interface internal is lock long "
      "namespace new null object operator out override params private protected public readonly "
      "ref sbyte sealed short sizeof stackalloc static string struct this throw try typeof uint "
      "ulong unchecked unsafe ushort using virtual void volatile while ";
  const auto keywords = language == target::go ? go : language == target::csharp ? cs : js;
  const auto token = " " + value + " ";
  if (!std::regex_match(value, std::regex{"[A-Za-z_][A-Za-z_0-9]*"}) || value == "_" ||
      common.find(token) != std::string_view::npos ||
      keywords.find(token) != std::string_view::npos) {
    throw std::invalid_argument{"Invalid or reserved generated identifier: " + value};
  }
}

// Derive a fixed wire width from the schema primitive.
int width(std::string_view type) {
  if (type.ends_with("8")) {
    return 1;
  }
  if (type.ends_with("16")) {
    return 2;
  }
  if (type.ends_with("32")) {
    return 4;
  }
  if (type.ends_with("64")) {
    return 8;
  }
  throw std::invalid_argument{"Unsupported integer type: " + std::string{type}};
}

// Emit direct native field codecs from the shared resolved C++ syntax tree.
class emitter {
  target language;
  const portable_options& options;
  std::string unit;
  std::string source{};
  std::size_t level{};
  std::vector<const syntax_node*> nodes{};
  std::map<const syntax_node*, std::string> names{};
  std::map<std::string, std::size_t> wire_names{};
  std::set<std::string> globals{"Protocol",     "Limits",   "DefaultLimits", "SrlWriter",
                                "SrlReader",    "SrlUtf8",  "SrlCompare",    "SrlReadEnum",
                                "SrlWriteEnum", "SrlField", "SrlFields",     "System"};

  // Select target-specific syntax; declaration output shares JavaScript naming.
  std::string choose(std::string_view js, std::string_view go, std::string_view cs) const {
    return std::string{language == target::go ? go : language == target::csharp ? cs : js};
  }
  // Append an indented source line.
  void line(const std::string& text = {}) {
    source.append(level * 2, ' ');
    source += text + '\n';
  }
  // Open a scope and increase indentation.
  void open(const std::string& text) {
    line(text + " {");
    ++level;
  }
  // Close the current scope.
  void close() {
    --level;
    line("}");
  }
  // Append a native statement terminator.
  void statement(const std::string& text) {
    line(text + (language == target::go ? "" : ";"));
  }
  // Open a native conditional.
  void condition(const std::string& expression) {
    open(language == target::go ? "if " + expression : "if (" + expression + ")");
  }
  // Qualify a protocol constant.
  std::string protocol(std::string_view value) const {
    return (language == target::go ? "" : "Protocol.") + std::string{value};
  }
  // Resolve a field name and Go export visibility.
  std::string field(std::string_view value, access_type access = access_type::public_access) const {
    auto result = name(value,
                       language == target::csharp ||
                           (language == target::go && access == access_type::public_access),
                       options.rename_identifiers);
    validate_name(result, language);
    if (language == target::go && access == access_type::public_access &&
        !std::isupper(static_cast<unsigned char>(result.front()))) {
      throw std::invalid_argument{"Public Go names must be exported: " + result};
    }
    return language == target::js && access != access_type::public_access ? "#" + result : result;
  }
  // Map schema scalar storage to native types.
  std::string type(const rohit::serializer::type_name& value) const {
    if (value.type != object_type::primitive) {
      return (language == target::go && value.type == object_type::class_type ? "*" : "") +
             names.at(value.resolved_node);
    }
    if (language == target::js || language == target::typescript) {
      return value.name == "string"                            ? "string"
             : value.name == "bool"                            ? "boolean"
             : value.name == "int64" || value.name == "uint64" ? "bigint"
                                                               : "number";
    }
    if (language == target::go) {
      return value.name == "char"     ? "byte"
             : value.name == "float"  ? "float32"
             : value.name == "double" ? "float64"
                                      : value.name;
    }
    static const std::map<std::string, std::string> primitives{
        {"char", "byte"},     {"int8", "sbyte"}, {"uint8", "byte"},  {"int16", "short"},
        {"uint16", "ushort"}, {"int32", "int"},  {"uint32", "uint"}, {"int64", "long"},
        {"uint64", "ulong"},  {"bool", "bool"},  {"float", "float"}, {"double", "double"},
        {"string", "string"}};
    return primitives.at(value.name);
  }
  // Resolve map key metadata in the common syntax tree.
  rohit::serializer::type_name key_type(const member& value) const {
    rohit::serializer::type_name key{std::string{value.key}, nullptr};
    key.type = value.key_node ? value.key_node->type : object_type::primitive;
    key.resolved_node = value.key_node;
    return key;
  }
  // Compose the language-specific enum constant expression.
  std::string enum_constant(const rohit::serializer::type_name& value,
                            std::string_view item) const {
    return names.at(value.resolved_node) + (language == target::go ? "" : ".") +
           name(item, true, options.rename_identifiers);
  }
  // Construct a fresh schema default.
  std::string initial(const rohit::serializer::type_name& value) const {
    if (value.type == object_type::class_type) {
      return choose("new ", "New", "new ") + names.at(value.resolved_node) + "()";
    }
    if (value.type == object_type::enum_type) {
      return enum_constant(
          value, static_cast<const enum_node&>(*value.resolved_node).enum_name_list.front());
    }
    if (value.name == "string") {
      return "\"\"";
    }
    if (value.name == "bool") {
      return "false";
    }
    if ((language == target::js || language == target::typescript) &&
        (value.name == "int64" || value.name == "uint64")) {
      return "0n";
    }
    return "0";
  }
  // Validate portable literals before translating their native spelling.
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
      const auto constant = separator == std::string::npos ? text : text.substr(separator + 2);
      if (separator != std::string::npos) {
        const auto prefix = text.substr(0, separator);
        if (prefix != item.name && prefix != item.resolved_node->name &&
            prefix != item.resolved_node->get_full_name()) {
          throw std::invalid_argument{"Enum default names the wrong type: " + value.name};
        }
      }
      const auto& values = static_cast<const enum_node&>(*item.resolved_node).enum_name_list;
      if (std::find(values.begin(), values.end(), constant) != values.end()) {
        return enum_constant(item, constant);
      }
    } else if (item.type == object_type::primitive) {
      if (item.name == "string" &&
          std::regex_match(text, std::regex{"\"([^\"\\\\]|\\\\[\"\\\\bfnrt])*\""})) {
        return text;
      }
      if (item.name == "bool" && (text == "true" || text == "false")) {
        return text;
      }
      if (item.name == "char" && text.size() >= 3 && text.front() == '\'' && text.back() == '\'') {
        if (text.size() == 3 && text[1] != '\\' && text[1] != '\'' &&
            static_cast<unsigned char>(text[1]) < 128) {
          return std::to_string(static_cast<unsigned char>(text[1]));
        }
        if (text.size() == 4 && text[1] == '\\') {
          const std::string_view escapes{"bfnrt\\\"'0"};
          constexpr std::array values{8, 12, 10, 13, 9, 92, 34, 39, 0};
          const auto index = escapes.find(text[2]);
          if (index != std::string_view::npos) {
            return std::to_string(values[index]);
          }
        }
      }
      if (item.name == "float" || item.name == "double") {
        if (std::regex_match(text, std::regex{"-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?"})) {
          float single{};
          double real{};
          const auto parsed = item.name == "float"
                                  ? std::from_chars(text.data(), text.data() + text.size(), single)
                                  : std::from_chars(text.data(), text.data() + text.size(), real);
          if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()) {
            if (language == target::csharp) {
              return text + (item.name == "float" ? "F" : "D");
            }
            if ((language == target::js || language == target::typescript) &&
                item.name == "float") {
              return "Math.fround(" + text + ")";
            }
            return text;
          }
        }
      } else if (item.name.starts_with("int") || item.name.starts_with("uint")) {
        const auto bits = width(item.name) * 8;
        const bool is_unsigned = item.name.starts_with("u");
        std::uint64_t unsigned_value{};
        std::int64_t signed_value{};
        const auto parsed =
            is_unsigned ? std::from_chars(text.data(), text.data() + text.size(), unsigned_value)
                        : std::from_chars(text.data(), text.data() + text.size(), signed_value);
        bool valid = std::regex_match(text, std::regex{"-?(0|[1-9][0-9]*)"}) &&
                     parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
        if (valid && bits < std::numeric_limits<std::uint64_t>::digits) {
          valid = is_unsigned ? unsigned_value < (std::uint64_t{1} << bits)
                              : signed_value >= -(std::int64_t{1} << (bits - 1)) &&
                                    signed_value < (std::int64_t{1} << (bits - 1));
        }
        if (valid) {
          if ((language == target::js || language == target::typescript) && bits == 64) {
            return text + "n";
          }
          if (language == target::csharp && bits == 64) {
            return text + (is_unsigned ? "UL" : "L");
          }
          if (language == target::csharp && item.name == "uint32") {
            return text + "U";
          }
          return text;
        }
      }
    }
    throw std::invalid_argument{"Expected a supported literal default: " + value.name};
  }
  // Register flattened names and reject namespace-flattening collisions.
  void register_nodes(const std::vector<std::unique_ptr<syntax_node>>& values,
                      const std::string& prefix = {}) {
    for (const auto& value : values) {
      const auto generated = prefix + name(value->name, true, options.rename_identifiers);
      if (value->type == object_type::namespace_type) {
        register_nodes(static_cast<const namespace_node&>(*value).statements, generated);
        continue;
      }
      validate_name(generated, language);
      if (!globals.insert(generated).second || (language == target::csharp && generated == unit) ||
          generated.starts_with("srl")) {
        throw std::invalid_argument{"Generated type name collision: " + generated};
      }
      names.emplace(value.get(), generated);
      nodes.push_back(value.get());
    }
  }
  // Validate native names, default construction, and the supported owning subset.
  void validate() {
    for (const auto* node : nodes) {
      if (!globals.insert(names.at(node) + "Names").second) {
        throw std::invalid_argument{"Generated helper name collision"};
      }
      if (node->type == object_type::enum_type) {
        const auto& values = static_cast<const enum_node&>(*node).enum_name_list;
        if (values.empty()) {
          throw std::invalid_argument{"Empty enum"};
        }
        std::set<std::string> constants{};
        for (const auto& value : values) {
          const auto identifier = name(value, true, options.rename_identifiers);
          validate_name(identifier, language);
          if (!constants.insert(identifier).second) {
            throw std::invalid_argument{"Enum naming collision"};
          }
          if (language == target::go && !globals.insert(names.at(node) + identifier).second) {
            throw std::invalid_argument{"Enum global naming collision"};
          }
        }
        continue;
      }
      const auto& value = static_cast<const class_node&>(*node);
      if (value.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
          (value.attributes & class_attributes::packed) != class_attributes::none) {
        throw std::invalid_argument{"Portable output supports owning classes only: " + value.name};
      }
      if (language == target::go) {
        for (const auto prefix : {"New", "Decode", "read"}) {
          if (!globals.insert(std::string{prefix} + names.at(node)).second) {
            throw std::invalid_argument{"Generated helper name collision"};
          }
        }
      }
      std::set<std::string> fields{"encode", "decode", "write", "read",        "Encode",
                                   "Decode", "Read",   "Write", names.at(node)};
      if (language == target::csharp) {
        // Unqualified type/helper references inside methods must not bind to fields.
        fields.insert(globals.begin(), globals.end());
      }
      const auto add = [&](const std::string& identifier) {
        const auto checked = identifier.starts_with('#') ? identifier.substr(1) : identifier;
        validate_name(checked, language);
        if (!fields.insert(checked).second) {
          throw std::invalid_argument{"Generated field collision: " + identifier};
        }
      };
      for (std::size_t i = 0; i < value.parents.size(); ++i) {
        add(field("base" + std::to_string(i), value.parents[i].access));
      }
      for (const auto& item : value.member_list) {
        const auto identifier = field(item.name, item.access);
        if (item.modifier != member::modifier_type::none && !item.default_value.empty()) {
          throw std::invalid_argument{"Collection and union defaults are unsupported"};
        }
        if (item.modifier == member::modifier_type::map &&
            ((item.key_node && item.key_node->type != object_type::enum_type) ||
             item.key == "float" || item.key == "double")) {
          throw std::invalid_argument{"Maps require integral, boolean, string, or enum keys"};
        }
        for (const auto& alternative : item.type_name_list) {
          if (alternative.resolved_node == node &&
              (item.modifier == member::modifier_type::none ||
               item.modifier == member::modifier_type::variant)) {
            throw std::invalid_argument{"Direct self-containing owning default"};
          }
        }
        if (item.modifier == member::modifier_type::variant) {
          add(identifier + "Index");
          for (const auto& alternative : item.type_name_list) {
            add(identifier + name(alternative.enum_name, true, true));
          }
        } else {
          add(identifier);
        }
        if (item.modifier == member::modifier_type::none) {
          (void)default_value(item);
        }
      }
    }
  }
  // Return the immutable pre-encoded key shared by every use of a wire name.
  std::string wire_key(const std::string& value) const {
    return choose("srlFields[", "srlFields[", "SrlFields[") + std::to_string(wire_names.at(value)) +
           "]";
  }
  // Pre-encode schema keys once, avoiding UTF-8 allocation on each encoded field.
  void emit_fields() {
    std::vector<std::string> ordered{};
    const auto add = [&](const std::string& value) {
      if (wire_names.emplace(value, ordered.size()).second) {
        ordered.push_back(value);
      }
    };
    for (const auto* node : nodes) {
      if (node->type != object_type::class_type) {
        continue;
      }
      const auto& value = static_cast<const class_node&>(*node);
      for (const auto& base : value.parents) {
        add(base.display_name);
      }
      for (const auto& item : value.member_list) {
        if (item.modifier == member::modifier_type::variant) {
          for (const auto& alternative : item.type_name_list) {
            add(item.display_name + ":" + alternative.enum_name);
          }
        } else {
          add(item.display_name);
        }
      }
    }
    line(choose("const srlFields = [", "var srlFields = []srlField{",
                "private static readonly SrlField[] SrlFields = {"));
    ++level;
    for (const auto& value : ordered) {
      line(choose("srlMakeField(", "srlMakeField(", "new SrlField(") + quote(value) + "),");
    }
    --level;
    line(choose("];", "}", "};"));
  }
  // Emit a scalar read expression, optionally merging an ordinary nested field.
  std::string read_value(const rohit::serializer::type_name& value, bool collection,
                         const std::string& previous = {}) const {
    if (value.type == object_type::class_type) {
      const auto identifier = names.at(value.resolved_node);
      const auto old = previous.empty() ? initial(value) : previous;
      return choose(identifier + ".read", "read" + identifier, identifier + ".Read") + "(input, " +
             old + ")";
    }
    if (value.type == object_type::enum_type) {
      const auto identifier = names.at(value.resolved_node);
      const auto expression = choose("srlReadEnum", "srlReadEnum", "SrlReadEnum") + "(input, " +
                              identifier + "Names, " + (collection ? "true" : "false") + ")";
      return language == target::go       ? identifier + "(" + expression + ")"
             : language == target::csharp ? "(" + identifier + ")" + expression
                                          : expression;
    }
    if (value.name == "string") {
      return "input.text()";
    }
    if (value.name == "bool") {
      return choose("input.bool()", "input.boolean()", "input.boolean()");
    }
    if (value.name == "char") {
      return "input.character()";
    }
    if (value.name == "float") {
      return choose("input.floating(true)", "input.float32()", "input.float32()");
    }
    if (value.name == "double") {
      return "input.floating(false)";
    }
    const auto expression = "input.integer(" + std::to_string(width(value.name)) + ", " +
                            (value.name.starts_with("u") ? "true" : "false") + ")";
    return language == target::go       ? type(value) + "(" + expression + ")"
           : language == target::csharp ? "unchecked((" + type(value) + ")" + expression + ")"
                                        : expression;
  }
  // Write a scalar directly with explicit wire width and enum context.
  void write_value(const rohit::serializer::type_name& value, const std::string& expression,
                   bool collection) {
    if (value.type == object_type::class_type) {
      statement(expression + choose(".write(output)", ".write(output)", ".Write(output)"));
    } else if (value.type == object_type::enum_type) {
      const auto converted = language == target::go       ? "int(" + expression + ")"
                             : language == target::csharp ? "(int)" + expression
                                                          : expression;
      statement(choose("srlWriteEnum", "srlWriteEnum", "SrlWriteEnum") + "(output, " + converted +
                ", " + names.at(value.resolved_node) + "Names, " + (collection ? "true" : "false") +
                ")");
    } else if (value.name == "string") {
      statement("output.text(" + expression + ")");
    } else if (value.name == "bool") {
      statement(choose("output.bool(", "output.boolean(", "output.boolean(") + expression + ")");
    } else if (value.name == "char") {
      statement("output.character(" + expression + ")");
    } else if (value.name == "float") {
      statement(choose("output.floating(" + expression + ", true)",
                       "output.float32(" + expression + ")", "output.float32(" + expression + ")"));
    } else if (value.name == "double") {
      statement("output.floating(" + expression + ", false)");
    } else {
      const auto converted = language == target::go       ? "uint64(" + expression + ")"
                             : language == target::csharp ? "unchecked((ulong)" + expression + ")"
                                                          : expression;
      statement("output.integer(" + converted + ", " + std::to_string(width(value.name)) + ", " +
                (value.name.starts_with("u") ? "true" : "false") + ")");
    }
  }
  // Declare a local using native inference.
  void local(const std::string& identifier, const std::string& value) {
    statement(choose("let " + identifier + " = " + value, identifier + " := " + value,
                     "var " + identifier + " = " + value));
  }
  // Open an incremental collection read loop.
  void read_loop() {
    open(choose("for (let index = 0; input.nextElement(index, count); ++index)",
                "for index := 0; input.nextElement(index, count); index++",
                "for (int index = 0; input.nextElement(index, count); ++index)"));
  }
  // Map schema collections to native typed storage.
  std::string collection_type(const member& value) const {
    const auto item = type(value.type_name_list.front());
    if (value.modifier == member::modifier_type::array) {
      return choose(item + "[]", "[]" + item, "System.Collections.Generic.List<" + item + ">");
    }
    const auto key = type(key_type(value));
    return choose("Map<" + key + ", " + item + ">", "map[" + key + "]" + item,
                  "System.Collections.Generic.Dictionary<" + key + ", " + item + ">");
  }
  // Allocate empty collections without trusting incoming sizes.
  std::string empty_collection(const member& value) const {
    if (language == target::js || language == target::typescript) {
      return value.modifier == member::modifier_type::array ? "[]" : "new Map()";
    }
    return language == target::go ? "make(" + collection_type(value) + ", 0)"
                                  : "new " + collection_type(value) + "()";
  }
  // Declare or initialize one field using target-specific visibility.
  void declaration(const std::string& identifier, const std::string& field_type,
                   const std::string& initial_value, access_type access) {
    if (language == target::go) {
      line(identifier + " " + field_type);
    } else if (language == target::typescript) {
      line(std::string{access == access_type::public_access ? "" : "private "} + identifier + ": " +
           field_type + ";");
    } else if (language == target::js) {
      statement("this." + identifier + " = " + initial_value);
    } else {
      statement(std::string{access == access_type::public_access ? "public " : "private "} +
                field_type + " " + identifier + " = " + initial_value);
    }
  }
  // Emit parent composition, fields, and explicit union selectors.
  void declarations(const class_node& value, bool go_defaults = false) {
    const auto declare = [&](const std::string& identifier, const std::string& field_type,
                             const std::string& initial_value, access_type access) {
      if (go_defaults) {
        line(identifier + ": " + initial_value + ",");
      } else {
        declaration(identifier, field_type, initial_value, access);
      }
    };
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      const auto base_type = names.at(base.parent_class);
      declare(field("base" + std::to_string(i), base.access),
              (language == target::go ? "*" : "") + base_type,
              choose("new ", "New", "new ") + base_type + "()", base.access);
    }
    for (const auto& item : value.member_list) {
      const auto identifier = field(item.name, item.access);
      if (item.modifier == member::modifier_type::variant) {
        declare(identifier + "Index", choose("number", "int", "int"), "0", item.access);
        for (const auto& alternative : item.type_name_list) {
          declare(identifier + name(alternative.enum_name, true, true), type(alternative),
                  initial(alternative), item.access);
        }
      } else if (item.modifier == member::modifier_type::none) {
        declare(identifier, type(item.type_name_list.front()), default_value(item), item.access);
      } else {
        declare(identifier, collection_type(item), empty_collection(item), item.access);
      }
    }
  }
  // Decode collections by replacement, merging only ordinary nested object fields.
  void read_field(const member& value) {
    const auto destination = "result." + field(value.name, value.access);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      statement(destination + " = " + read_value(item, false, destination));
      return;
    }
    local("count", "input.beginArray()");
    std::string initial_collection = empty_collection(value);
    if (value.modifier == member::modifier_type::array && language != target::js) {
      const auto& scalar = value.type_name_list.front();
      int scalar_width{};
      if (scalar.type == object_type::primitive && scalar.name != "string") {
        scalar_width = scalar.name == "bool" || scalar.name == "char" ? 1
                       : scalar.name == "float"                       ? 4
                       : scalar.name == "double"                      ? 8
                                                                      : width(scalar.name);
      }
      const auto capacity = "input.arrayCapacity(count, " + std::to_string(scalar_width) + ")";
      initial_collection = language == target::go
                               ? "make(" + collection_type(value) + ", 0, " + capacity + ")"
                               : "new " + collection_type(value) + "(" + capacity + ")";
    }
    local("values", initial_collection);
    read_loop();
    if (value.modifier == member::modifier_type::array) {
      const auto expression = read_value(item, true);
      statement(choose("values.push(" + expression + ")",
                       "values = append(values, " + expression + ")",
                       "values.Add(" + expression + ")"));
    } else {
      const auto key = key_type(value);
      statement(
          choose("let entryKey", "var entryKey " + type(key), type(key) + " entryKey = default!"));
      statement(choose("let entryValue", "var entryValue " + type(item),
                       type(item) + " entryValue = default!"));
      condition("input.json()");
      local("hasKey", "false");
      local("hasValue", "false");
      statement("input.beginObject()");
      local("firstEntry", "true");
      open(choose("while (input.nextObject(firstEntry))", "for input.nextObject(firstEntry)",
                  "while (input.nextObject(firstEntry))"));
      statement("firstEntry = false");
      open(choose("switch (input.key())", "switch input.key()", "switch (input.key())"));
      line("case \"key\":");
      ++level;
      statement("entryKey = " + read_value(key, true));
      statement("hasKey = true");
      if (language != target::go) {
        statement("break");
      }
      --level;
      line("case \"value\":");
      ++level;
      statement("entryValue = " + read_value(item, true));
      statement("hasValue = true");
      if (language != target::go) {
        statement("break");
      }
      --level;
      line("default:");
      ++level;
      statement("input.fail(\"Unknown map entry field\")");
      if (language != target::go) {
        statement("break");
      }
      --level;
      close();
      close();
      statement("input.endObject()");
      condition("!hasKey || !hasValue");
      statement("input.fail(\"Incomplete map entry\")");
      close();
      --level;
      line("} else {");
      ++level;
      statement("entryKey = " + read_value(key, true));
      statement("entryValue = " + read_value(item, true));
      close();
      if (language == target::go) {
        condition("input.err == nil");
      }
      statement(choose("values.set(entryKey, entryValue)", "values[entryKey] = entryValue",
                       "values[entryKey] = entryValue"));
      if (language == target::go) {
        close();
      }
    }
    close();
    statement(destination + " = values");
  }
  // Encode arrays and maps directly, sorting keys to preserve native map ordering.
  void write_field(const member& value) {
    const auto expression = choose("this.", "value.", "this.") + field(value.name, value.access);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      write_value(item, expression, false);
      return;
    }
    if (value.modifier == member::modifier_type::array) {
      const auto count =
          choose(expression + ".length", "len(" + expression + ")", expression + ".Count");
      statement("output.beginArray(" + count + ")");
      open(choose("for (let index = 0; index < " + count + "; ++index)",
                  "for index := 0; index < " + count + "; index++",
                  "for (int index = 0; index < " + count + "; ++index)"));
      statement("output.element(index)");
      write_value(item, expression + "[index]", true);
      close();
    } else {
      const auto key = key_type(value);
      if (language == target::js) {
        local("keys", "Array.from(" + expression + ".keys()).sort(srlCompare)");
      } else if (language == target::go) {
        local("keys", "make([]" + type(key) + ", 0, len(" + expression + "))");
        open("for key := range " + expression);
        statement("keys = append(keys, key)");
        close();
        const auto comparison = key.name == "bool" ? "!keys[i] && keys[j]" : "keys[i] < keys[j]";
        line("sort.Slice(keys, func(i, j int) bool { return " + std::string{comparison} + " })");
      } else {
        local("keys",
              "new System.Collections.Generic.List<" + type(key) + ">(" + expression + ".Keys)");
        statement("keys.Sort(SrlCompare)");
      }
      const auto count = choose("keys.length", "len(keys)", "keys.Count");
      statement("output.beginArray(" + count + ")");
      open(choose("for (let index = 0; index < " + count + "; ++index)",
                  "for index := 0; index < " + count + "; index++",
                  "for (int index = 0; index < " + count + "; ++index)"));
      local("entryKey", "keys[index]");
      statement("output.element(index)");
      condition("output.json()");
      statement("output.raw(\"{\\\"key\\\":\")");
      close();
      write_value(key, "entryKey", true);
      condition("output.json()");
      statement("output.raw(\",\\\"value\\\":\")");
      close();
      write_value(item,
                  choose(expression + ".get(entryKey)", expression + "[entryKey]",
                         expression + "[entryKey]"),
                  true);
      condition("output.json()");
      statement("output.raw(\"}\")");
      close();
      close();
    }
    statement("output.endArray()");
  }
  // Decode a fresh selected union payload, never merge an earlier selection.
  void read_union(const member& value, int selected = -1) {
    const auto destination = "result." + field(value.name, value.access);
    if (selected >= 0) {
      const auto& item = value.type_name_list.at(static_cast<std::size_t>(selected));
      statement(destination + "Index = " + std::to_string(selected));
      statement(destination + name(item.enum_name, true, true) + " = " + read_value(item, true));
      return;
    }
    open(choose("switch (input.compact())", "switch input.compact()", "switch (input.compact())"));
    for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
      line("case " + std::to_string(i) + ":");
      ++level;
      read_union(value, static_cast<int>(i));
      if (language != target::go) {
        statement("break");
      }
      --level;
    }
    line("default:");
    ++level;
    statement("input.fail(\"Unknown union alternative\")");
    if (language != target::go) {
      statement("break");
    }
    --level;
    close();
  }
  // Decode a complete field according to its schema modifier.
  void read_member(const member& value) {
    if (value.modifier == member::modifier_type::variant) {
      read_union(value);
    } else {
      read_field(value);
    }
  }
  // Write every parent and member in declaration order.
  void write_object(const class_node& value) {
    statement("output.beginObject()");
    if (language == target::go) {
      condition("output.err != nil");
      statement("return");
      close();
    }
    bool first = true;
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      statement("output.field(" + std::to_string(base.id) + ", " + wire_key(base.display_name) +
                ", " + (first ? "true" : "false") + ")");
      statement(choose("this.", "value.", "this.") +
                field("base" + std::to_string(i), base.access) +
                choose(".write(output)", ".write(output)", ".Write(output)"));
      first = false;
    }
    for (const auto& item : value.member_list) {
      open("");
      if (item.modifier == member::modifier_type::variant) {
        const auto expression = choose("this.", "value.", "this.") + field(item.name, item.access);
        open(choose("switch (" + expression + "Index)", "switch " + expression + "Index",
                    "switch (" + expression + "Index)"));
        for (std::size_t i = 0; i < item.type_name_list.size(); ++i) {
          const auto& alternative = item.type_name_list[i];
          line("case " + std::to_string(i) + ":");
          ++level;
          statement("output.field(" + std::to_string(item.id) + ", " +
                    wire_key(item.display_name + ":" + alternative.enum_name) + ", " +
                    (first ? "true" : "false") + ")");
          condition("output.protocol == " + protocol("BINARY_NONE") +
                    " || output.protocol == " + protocol("BINARY_INTEGER"));
          statement("output.compact(" + std::to_string(i) + ")");
          close();
          write_value(alternative, expression + name(alternative.enum_name, true, true), true);
          if (language != target::go) {
            statement("break");
          }
          --level;
        }
        line("default:");
        ++level;
        statement("output.fail(\"Unknown union alternative\")");
        if (language != target::go) {
          statement("break");
        }
        --level;
        close();
      } else {
        statement("output.field(" + std::to_string(item.id) + ", " + wire_key(item.display_name) +
                  ", " + (first ? "true" : "false") + ")");
        write_field(item);
      }
      close();
      first = false;
    }
    statement("output.endObject()");
  }
  // Merge a composed parent's fields in input order.
  void read_parent(const parent& base, std::size_t index) {
    const auto destination = "result." + field("base" + std::to_string(index), base.access);
    const auto identifier = names.at(base.parent_class);
    statement(destination + " = " +
              choose(identifier + ".read", "read" + identifier, identifier + ".Read") + "(input, " +
              destination + ")");
  }
  // Emit a field dispatch case with a scope for collection temporaries.
  void read_case(const std::string& key, const member& value, int alternative = -1) {
    line("case " + key + ":");
    open("");
    if (alternative >= 0) {
      read_union(value, alternative);
    } else {
      read_member(value);
    }
    if (language != target::go) {
      statement("break");
    }
    close();
  }
  // Dispatch known keys through native switches, avoiding linear field lookup.
  void read_object(const class_node& value) {
    statement("input.beginObject()");
    if (language == target::go) {
      condition("input.err != nil");
      statement("return result");
      close();
    }
    condition("input.protocol == " + protocol("BINARY_NONE"));
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      read_parent(value.parents[i], i);
    }
    for (const auto& item : value.member_list) {
      open("");
      read_member(item);
      close();
    }
    for (const bool numeric : {true, false}) {
      --level;
      line(numeric ? "} else if (input.protocol == " + protocol("BINARY_INTEGER") + ") {"
                   : "} else {");
      // Go does not permit parentheses around a condition followed by an implicit semicolon;
      // the parenthesized expression here remains legal before the opening brace.
      ++level;
      if (!numeric) {
        local("first", "true");
      }
      open(choose("while (true)", "for input.err == nil", "while (true)"));
      if (numeric) {
        local("key", "input.compact()");
        condition("key == 0");
        statement("break");
        close();
      } else {
        condition("input.json()");
        condition("!input.nextObject(first)");
        statement("break");
        close();
        statement("first = false");
        close();
        local("key", "input.key()");
        condition("!input.json() && key == \"\"");
        statement("break");
        close();
      }
      if (language == target::go) {
        condition("input.err != nil");
        statement("break");
        close();
      }
      open(choose("switch (key)", "switch key", "switch (key)"));
      for (std::size_t i = 0; i < value.parents.size(); ++i) {
        const auto& base = value.parents[i];
        line("case " + (numeric ? std::to_string(base.id) : quote(base.display_name)) + ":");
        open("");
        read_parent(base, i);
        if (language != target::go) {
          statement("break");
        }
        close();
      }
      for (const auto& item : value.member_list) {
        if (!numeric && item.modifier == member::modifier_type::variant) {
          for (std::size_t i = 0; i < item.type_name_list.size(); ++i) {
            read_case(quote(item.display_name + ":" + item.type_name_list[i].enum_name), item,
                      static_cast<int>(i));
          }
        } else {
          read_case(numeric ? std::to_string(item.id) : quote(item.display_name), item);
        }
      }
      line("default:");
      ++level;
      statement("input.fail(\"Unknown field\")");
      if (language != target::go) {
        statement("break");
      }
      --level;
      close();
      close();
    }
    close();
    statement("input.endObject()");
    statement("return result");
  }
  // Emit enum constants and the original-name lookup required by native codecs.
  void enumeration(const enum_node& value) {
    const auto identifier = names.at(&value);
    line("// " + identifier + " retains schema ordinals independently of generated names.");
    if (language == target::typescript) {
      line("export declare const " + identifier + ": Readonly<{");
      ++level;
      for (std::size_t i = 0; i < value.enum_name_list.size(); ++i) {
        line(name(value.enum_name_list[i], true, options.rename_identifiers) + ": " +
             std::to_string(i) + ";");
      }
      --level;
      line("}>;");
      line("export type " + identifier + " = typeof " + identifier + "[keyof typeof " + identifier +
           "];");
      return;
    }

    if (language == target::go) {
      line("type " + identifier + " int32");
      line("const (");
      ++level;
    } else if (language == target::js) {
      open("export const " + identifier + " = Object.freeze(");
    } else {
      open((language == target::typescript ? "export " : "public ") + std::string{"enum "} +
           identifier);
    }
    for (std::size_t i = 0; i < value.enum_name_list.size(); ++i) {
      const auto constant = name(value.enum_name_list[i], true, options.rename_identifiers);
      line(language == target::go
               ? identifier + constant + " " + identifier + " = " + std::to_string(i)
               : constant + (language == target::js ? ": " : " = ") + std::to_string(i) + ",");
    }
    --level;
    line(language == target::typescript ? "}" : choose("});", ")", "}"));
    if (language == target::typescript) {
      return;
    }
    std::string values{};
    for (const auto& item : value.enum_name_list) {
      if (!values.empty()) {
        values += ", ";
      }
      values += quote(item);
    }
    line(choose("const " + identifier + "Names = [" + values + "];",
                "var " + identifier + "Names = []string{" + values + "}",
                "private static readonly string[] " + identifier + "Names = {" + values + "};"));
  }
  // Emit an owning class with exact-message public APIs.
  void object(const class_node& value) {
    const auto identifier = names.at(&value);
    line("// " + identifier +
         " owns its fields; concurrent mutation during encoding is unsupported.");
    open(choose("export class " + identifier, "type " + identifier + " struct",
                "public sealed class " + identifier));
    if (language == target::js) {
      for (std::size_t i = 0; i < value.parents.size(); ++i) {
        const auto& base = value.parents[i];
        if (base.access != access_type::public_access) {
          statement(field("base" + std::to_string(i), base.access));
        }
      }
      for (const auto& item : value.member_list) {
        if (item.access == access_type::public_access) {
          continue;
        }
        const auto member_identifier = field(item.name, item.access);
        if (item.modifier == member::modifier_type::variant) {
          statement(member_identifier + "Index");
          for (const auto& alternative : item.type_name_list) {
            statement(member_identifier + name(alternative.enum_name, true, true));
          }
        } else {
          statement(member_identifier);
        }
      }
      line("// Construct fresh schema defaults.");
      open("constructor()");
      declarations(value);
      close();
    } else {
      declarations(value);
    }
    if (language == target::typescript) {
      line("constructor();");
      line("encode(protocol: Protocol): Uint8Array;");
      line("static decode(bytes: Uint8Array, protocol: Protocol, limits?: Limits): " + identifier +
           ";");
      close();
      return;
    }
    if (language == target::go) {
      close();
      line("// New" + identifier + " constructs schema defaults; the Go zero value may differ.");
      open("func New" + identifier + "() *" + identifier);
      open("return &" + identifier);
      declarations(value, true);
      close();
      close();
    }
    line("// Encode one complete message into independent storage.");
    open(choose("encode(protocol)",
                "func (value *" + identifier + ") Encode(protocol Protocol) ([]byte, error)",
                "public byte[] Encode(Protocol protocol)"));
    local("output",
          choose("new SrlWriter(protocol)", "srlNewWriter(protocol)", "new SrlWriter(protocol)"));
    statement(choose("this.write(output)", "value.write(output)", "this.Write(output)"));
    if (language == target::go) {
      condition("output.err != nil");
      statement("return nil, output.err");
      close();
      statement("return output.bytes, nil");
    } else {
      statement("return output.bytes()");
    }
    close();
    line("// Decode a fresh value and expose it only after exact-message validation.");
    open(choose("static decode(bytes, protocol, limits = new Limits())",
                "func Decode" + identifier +
                    "(bytes []byte, protocol Protocol, policies ...Limits) (*" + identifier +
                    ", error)",
                "public static " + identifier +
                    " Decode(System.ReadOnlyMemory<byte> bytes, Protocol protocol, Limits? limits "
                    "= null)"));
    if (language == target::go) {
      local("limits", "DefaultLimits()");
      condition("len(policies) > 1");
      statement("return nil, fmt.Errorf(\"Expected at most one limits policy\")");
      close();
      condition("len(policies) == 1");
      statement("limits = policies[0]");
      close();
    }
    local("input",
          choose("new SrlReader(bytes, protocol, limits)", "srlNewReader(bytes, protocol, limits)",
                 "new SrlReader(bytes, protocol, limits ?? new Limits())"));
    local("result", choose(identifier + ".read(input, new " + identifier + "())",
                           "read" + identifier + "(input, New" + identifier + "())",
                           identifier + ".Read(input, new " + identifier + "())"));
    statement("input.finish()");
    if (language == target::go) {
      condition("input.err != nil");
      statement("return nil, input.err");
      close();
      statement("return result, nil");
    } else {
      statement("return result");
    }
    close();
    line("// Write schema fields directly in declaration order.");
    open(choose("write(output)", "func (value *" + identifier + ") write(output *srlWriter)",
                "internal void Write(SrlWriter output)"));
    if (language == target::go) {
      condition("value == nil");
      statement("output.fail(\"Nil object\")");
      statement("return");
      close();
    }
    write_object(value);
    close();
    line("// Merge repeated nested fields within the fresh decoding candidate.");
    open(choose(
        "static read(input, result)",
        "func read" + identifier + "(input *srlReader, result *" + identifier + ") *" + identifier,
        "internal static " + identifier + " Read(SrlReader input, " + identifier + " result)"));
    if (language == target::go) {
      condition("input.err != nil");
      statement("return result");
      close();
    }
    read_object(value);
    close();
    if (language != target::go) {
      close();
    }
  }

public:
  // Bind immutable generation policy for one compilation unit.
  emitter(target language, const portable_options& options, std::string_view unit_name)
      : language{language}, options{options},
        unit{name(unit_name, true, options.rename_identifiers)} {}
  // Validate all declarations before returning complete native source.
  std::string generate(const std::vector<std::unique_ptr<syntax_node>>& values) {
    if (language == target::csharp) {
      validate_name(unit, language);
      if (!globals.insert(unit).second) {
        throw std::invalid_argument{"C# outer class shadows a runtime type"};
      }
    }
    if (language == target::go) {
      for (const auto identifier :
           {"binary",       "json",    "fmt",    "math",    "regexp",      "sort",
            "strconv",      "strings", "utf8",   "bool",    "byte",        "int",
            "int8",         "int16",   "int32",  "int64",   "uint",        "uint8",
            "uint16",       "uint32",  "uint64", "float32", "float64",     "string",
            "error",        "nil",     "iota",   "len",     "cap",         "make",
            "append",       "copy",    "new",    "JSON",    "BINARY_NONE", "BINARY_INTEGER",
            "BINARY_STRING"}) {
        globals.insert(identifier);
      }
    } else if (language == target::js || language == target::typescript) {
      for (const auto identifier :
           {"Array", "BigInt", "DataView", "Error", "Infinity", "JSON", "Map", "Math", "NaN",
            "Number", "Object", "RangeError", "String", "TextDecoder", "TextEncoder", "TypeError",
            "Uint8Array", "undefined"}) {
        globals.insert(identifier);
      }
    }
    register_nodes(values);
    validate();
    line("// Generated by Serializer. Regenerate from the .serializer schema; do not edit.");
    if (language == target::go) {
      validate_name(options.package_name, language);
      line("package " + options.package_name);
      source += go_runtime;
    } else if (language == target::csharp) {
      line("#nullable enable");
      if (!options.namespace_name.empty()) {
        std::size_t start{};
        do {
          const auto end = options.namespace_name.find('.', start);
          validate_name(
              options.namespace_name.substr(start, end == std::string::npos ? end : end - start),
              language);
          if (end == std::string::npos) {
            break;
          }
          start = end + 1;
        } while (true);
        open("namespace " + options.namespace_name);
      }
      open("public static class " + unit);
      source += csharp_runtime;
    } else if (language == target::js) {
      source += js_runtime;
    } else {
      line("export declare const Protocol: Readonly<{JSON: 0; BINARY_NONE: 1; BINARY_INTEGER: 2; "
           "BINARY_STRING: 3}>;");
      line("export type Protocol = typeof Protocol[keyof typeof Protocol];");
      line("export declare class Limits {");
      line("  readonly maxBytes: number; readonly maxStringBytes: number; readonly maxElements: "
           "number; readonly maxDepth: number;");
      line("  constructor(maxBytes?: number, maxStringBytes?: number, maxElements?: number, "
           "maxDepth?: number);");
      line("}");
    }
    if (language != target::typescript) {
      emit_fields();
    }
    for (const auto* node : nodes) {
      if (node->type == object_type::enum_type) {
        enumeration(static_cast<const enum_node&>(*node));
      } else {
        object(static_cast<const class_node&>(*node));
      }
    }
    if (language == target::csharp) {
      close();
      if (!options.namespace_name.empty()) {
        close();
      }
    }
    return source;
  }
};
} // namespace

// Dispatch native owning output while preserving the compiler's C++-only implementation.
std::string generate(const std::vector<std::unique_ptr<syntax_node>>& statements,
                     std::string_view language, std::string_view unit_name,
                     const portable_options& options) {
  const auto selected = language == "js"       ? target::js
                        : language == "go"     ? target::go
                        : language == "csharp" ? target::csharp
                        : language == "typescript"
                            ? target::typescript
                            : throw std::invalid_argument{"Unknown portable language"};
  return emitter{selected, options, unit_name}.generate(statements);
}
} // namespace rohit::serializer::writer::portable
