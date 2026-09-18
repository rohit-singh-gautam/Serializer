#include "native_schema.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

namespace rohit::serializer::writer::native {
namespace {
constexpr std::string_view swift_runtime =
#include "swift_runtime.inc"
    ;
constexpr std::string_view kotlin_runtime =
#include "kotlin_runtime.inc"
    ;
// Share field traversal while emitting native Swift and Kotlin syntax and ownership.
class mobile_emitter {
  const schema& model;
  const bool swift;
  std::string output{};
  std::size_t indent{};
  // Select syntax for the current native language.
  std::string select(std::string_view apple, std::string_view jvm) const {
    return std::string{swift ? apple : jvm};
  }
  // Emit an indented source line.
  void line(const std::string& text = {}) {
    output.append(indent * 4, ' ');
    output += text + '\n';
  }
  // Enter a source scope.
  void open(const std::string& text) {
    line(text + " {");
    ++indent;
  }
  // Leave a source scope.
  void close() {
    --indent;
    line("}");
  }
  // Produce a checked lowerCamelCase field name.
  std::string field(std::string_view value) const {
    auto result = pascal(value);
    if (!result.empty()) {
      result.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(result.front())));
    }
    validate_identifier(
        result, swift ? " associatedtype class deinit enum extension fileprivate func import init "
                        "inout internal let open operator private protocol public rethrows static "
                        "struct subscript typealias var break case continue default defer do else "
                        "fallthrough for guard if in repeat return switch where while as Any catch "
                        "false is nil super self Self throw throws true try _ some any "
                      : " as break class continue do else false for fun if in interface is null "
                        "object package return super this throw true try typealias typeof val var "
                        "when while by catch constructor delegate dynamic field file finally get "
                        "import init param property receiver set setparam where actual abstract "
                        "annotation companion const crossinline data enum expect external final "
                        "infix inline inner internal lateinit noinline open operator out override "
                        "private protected public reified sealed suspend tailrec vararg ");
    return result;
  }
  // Map all fixed-width scalar types explicitly.
  std::string type(const type_name& value, bool key = false) const {
    if (value.type != object_type::primitive) {
      return model.names.at(value.resolved_node);
    }
    if (value.name == "string") {
      return swift && key ? "WireString" : "String";
    }
    if (value.name == "bool") {
      return select("Bool", "Boolean");
    }
    if (value.name == "char") {
      return select("UInt8", "UByte");
    }
    if (value.name == "float") {
      return "Float";
    }
    if (value.name == "double") {
      return "Double";
    }
    if (swift) {
      return (value.name.starts_with('u') ? "UInt" : "Int") + std::to_string(width(value) * 8);
    }
    const auto bytes = width(value);
    return (value.name.starts_with('u') ? "U" : "") + std::string{bytes == 1   ? "Byte"
                                                                  : bytes == 2 ? "Short"
                                                                  : bytes == 4 ? "Int"
                                                                               : "Long"};
  }
  // Translate portable quoted defaults to target-specific control escapes.
  std::string text_literal(std::string value) const {
    for (std::size_t i = 1; i + 1 < value.size(); ++i) {
      if (value[i] == '$' && !swift) {
        value.insert(i, 1, '\\');
        ++i;
        continue;
      }
      if (value[i] != '\\') {
        continue;
      }
      ++i;
      if (value[i] == 'f' || (swift && value[i] == 'b')) {
        const auto replacement = swift ? value[i] == 'b' ? "u{8}" : "u{c}" : "u000c";
        value.replace(i, 1, replacement);
        i += std::string_view{replacement}.size() - 1;
      }
    }
    return value;
  }
  // Create fresh scalar defaults, with explicit full-width integer spelling.
  std::string initial(const type_name& value, std::string literal_value = {}) const {
    if (value.type == object_type::class_type) {
      return type(value) + "()";
    }
    if (value.type == object_type::enum_type) {
      const auto& constants = static_cast<const enum_node&>(*value.resolved_node).enum_name_list;
      return type(value) + "." + pascal(literal_value.empty() ? constants.front() : literal_value);
    }
    if (value.name == "string") {
      return literal_value.empty() ? "\"\"" : text_literal(literal_value);
    }
    if (value.name == "bool") {
      return literal_value.empty() ? "false" : literal_value;
    }
    if (literal_value.empty()) {
      literal_value = "0";
    }
    if (swift) {
      return literal_value;
    }
    if (value.name == "float") {
      return literal_value + "f";
    }
    if (value.name == "double") {
      return literal_value.find_first_of(".eE") == std::string::npos ? literal_value + ".0"
                                                                     : literal_value;
    }
    if (value.name == "int64") {
      return literal_value == "-9223372036854775808" ? "Long.MIN_VALUE" : literal_value + "L";
    }
    if (value.name == "uint64") {
      return literal_value + "uL";
    }
    if (value.name.starts_with('u') || value.name == "char") {
      return literal_value + "u";
    }
    return literal_value;
  }
  // Primitive Kotlin arrays avoid per-element boxing on numeric hot paths.
  bool primitive_array(const type_name& value) const {
    return !swift && value.type == object_type::primitive && value.name != "string";
  }
  // Map collections to owning native containers.
  std::string collection_type(const member& value) const {
    const auto& t = value.type_name_list.front();
    if (value.modifier == member::modifier_type::array) {
      return swift                ? "[" + type(t) + "]"
             : primitive_array(t) ? type(t) + "Array"
                                  : "MutableList<" + type(t) + ">";
    }
    return swift ? "[" + type(map_key(value), true) + ": " + type(t) + "]"
                 : "MutableMap<" + type(map_key(value)) + ", " + type(t) + ">";
  }
  // Construct empty containers without sharing mutable state.
  std::string empty(const member& value) const {
    if (swift) {
      return value.modifier == member::modifier_type::map ? "[:]" : "[]";
    }
    return value.modifier == member::modifier_type::map ? "linkedMapOf()"
           : primitive_array(value.type_name_list.front())
               ? type(value.type_name_list.front()) + "Array(0)"
               : "mutableListOf()";
  }
  // Prefix throwing Swift operations; Kotlin uses normal exception propagation.
  std::string attempt() const {
    return swift ? "try " : "";
  }
  // Decode a fresh scalar or nested value.
  std::string read(const type_name& value, bool collection, bool key = false) const {
    const auto prefix = attempt();
    if (value.type == object_type::class_type) {
      return prefix + type(value) + ".srlCreate(input)";
    }
    if (value.type == object_type::enum_type) {
      return swift ? type(value) + "(rawValue: try input.enumeration(" + type(value) +
                         ".srlNames, " + (collection ? "true" : "false") + "))!"
                   : type(value) + ".entries[input.enumeration(" + type(value) + ".srlNames, " +
                         (collection ? "true" : "false") + ")]";
    }
    if (value.name == "string") {
      return swift && key ? "WireString(try input.text())" : prefix + "input.text()";
    }
    if (value.name == "bool") {
      return prefix + "input.boolean()";
    }
    if (value.name == "char") {
      return prefix + "input.character()";
    }
    if (value.name == "float" || value.name == "double") {
      return prefix + "input." + (value.name == "float" ? "float32()" : "float64()");
    }
    const auto expression = "input.integer(" + std::to_string(width(value)) + ", " +
                            (value.name.starts_with('u') ? "true" : "false") + ")";
    return swift                    ? type(value) + "(truncatingIfNeeded: try " + expression + ")"
           : value.name == "uint64" ? expression
                                    : expression + ".to" + type(value) + "()";
  }
  // Write typed values directly; Swift map strings retain their exact byte identity.
  void write(const type_name& value, const std::string& expression, bool collection,
             bool key = false) {
    if (value.type == object_type::class_type) {
      line(attempt() + expression + ".srlWrite(out)");
    } else if (value.type == object_type::enum_type) {
      line(attempt() + "out.enumeration(" + expression + select(".rawValue", ".ordinal") + ", " +
           type(value) + ".srlNames, " + (collection ? "true" : "false") + ")");
    } else if (value.name == "string") {
      line(attempt() + "out.text(" + expression + (swift && key ? ".value" : "") + ")");
    } else if (value.name == "bool") {
      line("out.boolean(" + expression + ")");
    } else if (value.name == "char") {
      line(attempt() + "out.character(" + expression + ")");
    } else if (value.name == "float" || value.name == "double") {
      line(attempt() + "out." + (value.name == "float" ? "float32(" : "float64(") + expression +
           ")");
    } else {
      line("out.integer(" +
           (swift                    ? "UInt64(truncatingIfNeeded: " + expression + ")"
            : value.name == "uint64" ? expression
                                     : expression + ".toULong()") +
           ", " + std::to_string(width(value)) + ", " +
           (value.name.starts_with('u') ? "true" : "false") + ")");
    }
  }
  // Emit a key index into the module's pre-encoded table.
  void key(std::uint32_t id, const std::string& name, bool first) {
    line(attempt() + "out.field(" + std::to_string(id) + ", " +
         std::to_string(model.keys.at(name)) + ", " + (first ? "true" : "false") + ")");
  }
  // Construct a deterministic map traversal including unsigned and UTF-8 keys.
  std::string sorted(const member& value, const std::string& source) const {
    const auto key = map_key(value);
    if (swift) {
      return source + ".keys.sorted { " +
             (key.name == "bool"                   ? "!$0 && $1"
              : key.type == object_type::enum_type ? "$0.rawValue < $1.rawValue"
                                                   : "$0 < $1") +
             " }";
    }
    return source + ".keys.sortedWith(Comparator { a, b -> " +
           (key.name == "string"                 ? "srlCompareText(a, b)"
            : key.type == object_type::enum_type ? "a.ordinal.compareTo(b.ordinal)"
                                                 : "a.compareTo(b)") +
           " })";
  }
  // Emit direct collection and union writes with native ownership.
  void write_member(const member& value, bool first) {
    const auto source = "self." + field(value.name);
    const auto src = swift ? source : "this." + field(value.name);
    if (value.modifier == member::modifier_type::variant) {
      open(select("switch " + src + "Index", "when (" + src + "Index)"));
      for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
        const auto& alt = value.type_name_list[i];
        if (swift) {
          line("case " + std::to_string(i) + ":");
          ++indent;
        } else {
          open(std::to_string(i) + " ->");
        }
        key(value.id, value.display_name + ":" + alt.enum_name, first);
        line(select("if out.protocolValue == .BINARY_NONE || out.protocolValue == .BINARY_INTEGER "
                    "{ try out.compact(",
                    "if (out.protocol == Protocol.BINARY_NONE || out.protocol == "
                    "Protocol.BINARY_INTEGER) { out.compact(") +
             std::to_string(i) + ") }");
        write(alt, src + pascal(alt.enum_name), true);
        if (swift) {
          --indent;
        } else {
          close();
        }
      }
      line(select("default: throw SerializerError.invalid(\"Unknown union alternative\")",
                  "else -> throw SerializerException(\"Unknown union alternative\")"));
      close();
      return;
    }
    key(value.id, value.display_name, first);
    const auto& t = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      write(t, src, false);
      return;
    }
    line(attempt() + "out.beginArray(" + src + select(".count)", ".size)"));
    const auto sequence = value.modifier == member::modifier_type::map ? sorted(value, src) : src;
    open(swift ? "for (index, item) in (" + sequence + ").enumerated()"
               : "for ((index, item) in (" + sequence + ").withIndex())");
    line("out.element(index)");
    if (value.modifier == member::modifier_type::map) {
      line(select("if out.json() { try out.beginObject(); try out.field(0, 0, true) }",
                  "if (out.json()) { out.beginObject(); out.field(0, 0, true) }"));
      write(map_key(value), "item", true, true);
      line(select("if out.json() { try out.field(0, 1, false) }",
                  "if (out.json()) { out.field(0, 1, false) }"));
      write(t, src + "[item]" + select("!", "!!"), true);
      line(select("if out.json() { out.endObject() }", "if (out.json()) { out.endObject() }"));
    } else {
      write(t, "item", true);
    }
    close();
    line("out.endArray()");
  }
  // Decode one schema field, committing collections only when complete.
  void read_member(const member& value) {
    const auto dst = select("self.", "this.") + field(value.name);
    const auto& t = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      line(t.type == object_type::class_type ? attempt() + dst + ".srlRead(input)"
                                             : dst + " = " + read(t, false));
      return;
    }
    if (value.modifier == member::modifier_type::variant) {
      line(dst + "Index = " +
           select("try alternative ?? input.compact()", "alternative ?: input.compact()"));
      open(select("switch " + dst + "Index", "when (" + dst + "Index)"));
      for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
        const auto& alt = value.type_name_list[i];
        line(select("case ", "") + std::to_string(i) + select(": ", " -> ") + dst +
             pascal(alt.enum_name) + " = " + read(alt, true));
      }
      line(select("default: throw SerializerError.invalid(\"Unknown union alternative\")",
                  "else -> throw SerializerException(\"Unknown union alternative\")"));
      close();
      return;
    }
    line(select("let ", "val ") + "count = " + attempt() + "input.beginArray()");
    line("var index = 0");
    if (value.modifier == member::modifier_type::array) {
      if (swift) {
        line("var values: " + collection_type(value) + " = []");
        line("values.reserveCapacity(try input.capacity(count, " + std::to_string(width(t)) + "))");
      } else if (primitive_array(t)) {
        line("var values = " + type(t) + "Array(input.capacity(count, " + std::to_string(width(t)) +
             "))");
      } else {
        line("val values = ArrayList<" + type(t) + ">(input.capacity(count, 0))");
      }
      open(select("while try input.nextElement(index, count)",
                  "while (input.nextElement(index, count))"));
      if (swift) {
        line("values.append(" + read(t, true) + ")");
      } else if (primitive_array(t)) {
        line("if (index == values.size) values = values.copyOf(maxOf(16, values.size + "
             "values.size/2))");
        line("values[index] = " + read(t, true));
      } else {
        line("values.add(" + read(t, true) + ")");
      }
      line("index += 1");
      close();
      line(dst + " = " +
           (!swift && primitive_array(t)
                ? "if (index == values.size) values else values.copyOf(index)"
                : "values"));
      return;
    }
    const auto key_type = type(map_key(value), true), value_type = type(t);
    line(select("var values: " + collection_type(value) + " = [:]",
                "val values = linkedMapOf<" + key_type + ", " + value_type + ">()"));
    open(select("while try input.nextElement(index, count)",
                "while (input.nextElement(index, count))"));
    line("var entryKey: " + key_type + "? = " + select("nil", "null"));
    line("var entryValue: " + value_type + "? = " + select("nil", "null"));
    open(select("if input.json()", "if (input.json())"));
    line(attempt() + "input.beginObject()");
    line("var entryCursor = 0");
    if (swift) {
      open("while let entry = try input.key([], entryCursor)");
    } else {
      open("while (true)");
      line("val entry = input.key(srlEmptyIds, entryCursor) ?: break");
    }
    line("entryCursor += 1");
    open(select("switch entry.name", "when (entry.name)"));
    line(select("case \"key\": ", "\"key\" -> ") +
         "entryKey = " + read(map_key(value), true, true));
    line(select("case \"value\": ", "\"value\" -> ") + "entryValue = " + read(t, true));
    line(select("default: throw SerializerError.invalid(\"Unknown map entry field\")",
                "else -> throw SerializerException(\"Unknown map entry field\")"));
    close();
    close();
    line("input.endObject()");
    --indent;
    line("} else {");
    ++indent;
    line("entryKey = " + read(map_key(value), true, true));
    line("entryValue = " + read(t, true));
    close();
    if (swift) {
      line("guard let entryKey, let entryValue else { throw SerializerError.invalid(\"Incomplete "
           "map entry\") }");
      line("values[entryKey] = entryValue");
    } else {
      line("values[entryKey ?: throw SerializerException(\"Incomplete map entry\")] = entryValue "
           "?: throw SerializerException(\"Incomplete map entry\")");
    }
    line("index += 1");
    close();
    line(dst + " = values");
  }
  // Emit native owning models with public exact-message decoding.
  void object(const class_node& value) {
    const auto name = model.names.at(&value);
    line("// Owning schema model; avoid concurrent mutation during encoding.");
    open(select("public struct ", "class ") + name);
    const auto declare = [&](const std::string& id, const std::string& type_name,
                             const std::string& initial_value, access_type access) {
      const auto visibility =
          access == access_type::public_access ? select("public ", "") : "private ";
      line(visibility + "var " + id + ": " + type_name + " = " + initial_value);
    };
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      const auto n = model.names.at(base.parent_class);
      declare("base" + std::to_string(i), n, n + "()", base.access);
    }
    for (const auto& item : value.member_list) {
      const auto id = field(item.name);
      const auto& t = item.type_name_list.front();
      if (item.modifier == member::modifier_type::variant) {
        declare(id + "Index", "Int", "0", item.access);
        for (const auto& alt : item.type_name_list) {
          declare(id + pascal(alt.enum_name), type(alt), initial(alt), item.access);
        }
      } else if (item.modifier == member::modifier_type::none) {
        declare(id, type(t), initial(t, literal(item)), item.access);
      } else {
        declare(id, collection_type(item), empty(item), item.access);
      }
    }
    if (swift) {
      line("/// Construct independent schema defaults.");
      line("public init() {}");
    }
    line("// Encode one complete message into independent byte storage.");
    open(select("public func encode(_ protocolValue: Protocol) throws -> [UInt8]",
                "fun encode(protocolValue: Protocol): ByteArray"));
    line(select("let ", "val ") + "out = SrlWriter(protocolValue)");
    line(attempt() + "srlWrite(out)");
    line("return out." + select("bytes", "bytes()"));
    close();
    if (!swift) {
      open("companion object");
    }
    std::string ids = select("[", "intArrayOf(");
    bool id_first = true;
    const auto add_id = [&](std::uint32_t id) {
      if (!id_first) {
        ids += ',';
      }
      ids += std::to_string(id);
      id_first = false;
    };
    for (const auto& base : value.parents) {
      add_id(base.id);
    }
    for (const auto& item : value.member_list) {
      add_id(item.id);
    }
    ids += select("]", ")");
    line(select("private static let srlIds: [Int] = ", "private val srlIds = ") + ids);
    line("// Decode a fresh candidate and require exact message consumption.");
    open(select(
             "public static func decode(_ bytes: [UInt8], _ protocolValue: Protocol, _ limits: "
             "Limits = Limits()) throws -> ",
             "fun decode(bytes: ByteArray, protocolValue: Protocol, limits: Limits = Limits()): ") +
         name);
    line(select("let ", "val ") + "input = " + attempt() +
         "SrlReader(bytes, protocolValue, limits)");
    line(select("let ", "val ") + "result = " + attempt() + "srlCreate(input)");
    line(attempt() + "input.finish()");
    line("return result");
    close();
    line("// Construct a fresh nested value for a collection or selected union payload.");
    open(select("fileprivate static func srlCreate(_ input: SrlReader) throws -> ",
                "internal fun srlCreate(input: SrlReader): ") +
         name);
    line(select("var ", "val ") + "result = " + name + "()");
    line(attempt() + "result.srlRead(input)");
    line("return result");
    close();
    if (!swift) {
      close();
    }
    line("// Encode typed fields in declaration order.");
    open(select("fileprivate func srlWrite(_ out: SrlWriter) throws",
                "internal fun srlWrite(out: SrlWriter)"));
    line(attempt() + "out.beginObject()");
    bool first = true;
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      key(base.id, base.display_name, first);
      line(attempt() + "base" + std::to_string(i) + ".srlWrite(out)");
      first = false;
    }
    for (const auto& item : value.member_list) {
      write_member(item, first);
      first = false;
    }
    line("out.endObject()");
    close();
    line("// Merge repeated nested fields while replacing complete collections.");
    open(select("fileprivate mutating func srlRead(_ input: SrlReader) throws",
                "internal fun srlRead(input: SrlReader)"));
    line(attempt() + "input.beginObject()");
    line("var cursor = 0");
    if (swift) {
      open("while let key = try input.key(Self.srlIds, cursor)");
    } else {
      open("while (true)");
      line("val key = input.key(srlIds, cursor) ?: break");
    }
    line("cursor += 1");
    line((swift && value.parents.empty() && value.member_list.empty() ? "let " : "var ") +
         std::string{"id = key.id"});
    const bool has_union =
        std::any_of(value.member_list.begin(), value.member_list.end(), [](const auto& item) {
          return item.modifier == member::modifier_type::variant;
        });
    if (has_union) {
      line("var alternative: Int? = " + select("nil", "null"));
    }
    open(select("if let name = key.name", "if (key.name != null)"));
    open(select("switch WireString(name)", "when (key.name)"));
    const auto branch = [&](const std::string& text, std::uint32_t id, int alt) {
      const auto assignment =
          "id = " + std::to_string(id) + (alt < 0 ? "" : "; alternative = " + std::to_string(alt));
      line(select("case " + quote(text) + ": " + assignment,
                  quote(text) + " -> { " + assignment + " }"));
    };
    for (const auto& base : value.parents) {
      branch(base.display_name, base.id, -1);
    }
    for (const auto& item : value.member_list) {
      if (item.modifier == member::modifier_type::variant) {
        for (std::size_t i = 0; i < item.type_name_list.size(); ++i) {
          branch(item.display_name + ":" + item.type_name_list[i].enum_name, item.id,
                 static_cast<int>(i));
        }
      } else {
        branch(item.display_name, item.id, -1);
      }
    }
    line(select("default: throw SerializerError.invalid(\"Unknown field\")",
                "else -> throw SerializerException(\"Unknown field\")"));
    close();
    close();
    open(select("switch id", "when (id)"));
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      line(select("case ", "") + std::to_string(value.parents[i].id) + select(": try ", " -> ") +
           "base" + std::to_string(i) + ".srlRead(input)");
    }
    for (const auto& item : value.member_list) {
      if (swift) {
        line("case " + std::to_string(item.id) + ":");
        ++indent;
      } else {
        open(std::to_string(item.id) + " ->");
      }
      read_member(item);
      if (swift) {
        --indent;
      } else {
        close();
      }
    }
    line(select("default: throw SerializerError.invalid(\"Unknown field\")",
                "else -> throw SerializerException(\"Unknown field\")"));
    close();
    close();
    line("input.endObject()");
    close();
    close();
  }

public:
  // Bind a schema and optional JVM package before emitting any source.
  mobile_emitter(const schema& value, bool apple, std::string_view package)
      : model{value}, swift{apple} {
    if (!swift) {
      output = "@file:OptIn(ExperimentalUnsignedTypes::class)\n";
      if (!package.empty()) {
        std::size_t start{};
        do {
          const auto end = package.find('.', start);
          const auto part =
              package.substr(start, end == std::string_view::npos ? end : end - start);
          (void)field(part);
          if (end == std::string_view::npos) {
            break;
          }
          start = end + 1;
        } while (true);
        output += "package " + std::string{package} + "\n";
      }
    }
    output += swift ? swift_runtime : kotlin_runtime;
  }
  // Emit the runtime, immutable schema key table, enums, and owning models.
  std::string generate() {
    line(select("private let srlFields: [SrlField] = [", "private val srlFields = arrayOf("));
    ++indent;
    for (const auto& key : model.ordered_keys) {
      line("srlField(" + quote(key) + "),");
    }
    --indent;
    line(select("]", ")"));
    for (const auto* node : model.nodes) {
      validate_identifier(
          model.names.at(node),
          " UInt8 UInt16 UInt32 UInt64 Int8 Int16 Int32 Int64 UByte UShort UInt ULong Byte Short "
          "Long Boolean WireString SerializerError SerializerException ByteBuffer ByteOrder "
          "ByteArrayOutputStream CodingErrorAction Charsets Character IllegalArgumentException "
          "MutableList MutableMap LinkedHashMap HashMap IntArray LongArray FloatArray DoubleArray "
          "BooleanArray UIntArray ULongArray UByteArray UShortArray ShortArray ");
      if (node->type == object_type::class_type) {
        object(static_cast<const class_node&>(*node));
        continue;
      }
      const auto& value = static_cast<const enum_node&>(*node);
      const auto name = model.names.at(node);
      line("// Stable schema enum ordinals and exact wire names.");
      open(select("public enum " + name + ": Int, Hashable", "enum class " + name));
      for (std::size_t i = 0; i < value.enum_name_list.size(); ++i) {
        const auto constant = pascal(value.enum_name_list[i]);
        validate_identifier(constant, " Self ");
        line(swift ? "case " + constant + " = " + std::to_string(i)
                   : constant + (i + 1 == value.enum_name_list.size() ? ";" : ","));
      }
      if (!swift) {
        open("companion object");
      }
      std::string names =
          select("fileprivate static let srlNames = [", "internal val srlNames = arrayOf(");
      bool first = true;
      for (const auto& item : value.enum_name_list) {
        if (!first) {
          names += ',';
        }
        names += quote(item);
        first = false;
      }
      line(names + select("]", ")"));
      if (!swift) {
        close();
      }
      close();
    }
    return output;
  }
};
} // namespace
// Emit a standalone Swift source file.
std::string swift(const schema& value) {
  return mobile_emitter{value, true, {}}.generate();
}
// Emit a standalone Kotlin/JVM source file using only the standard library.
std::string kotlin(const schema& value, std::string_view package) {
  return mobile_emitter{value, false, package}.generate();
}
} // namespace rohit::serializer::writer::native
