#include "native_schema.hpp"

#include <functional>
#include <string>
#include <utility>

namespace rohit::serializer::writer::native {
namespace {
constexpr std::string_view runtime =
#include "python_runtime.inc"
    ;
constexpr std::string_view keywords =
    " False None True and as assert async await break class continue def del elif else except "
    "finally for from global if import in is lambda nonlocal not or pass raise return try while "
    "with yield match case self cls ";

// Emit direct Python field operations without a generic intermediate object tree.
class python_emitter {
  const schema& model;
  std::string output{runtime};
  std::size_t indent{};
  // Append one indented Python statement.
  void line(const std::string& text = {}) {
    output.append(indent * 4, ' ');
    output += text + '\n';
  }
  // Enter a Python suite.
  void open(const std::string& text) {
    line(text + ':');
    ++indent;
  }
  // Select the schema's exported or conventionally private attribute name.
  std::string field(std::string_view name, access_type access = access_type::public_access) const {
    auto result = snake(name);
    validate_identifier(result, keywords);
    return (access == access_type::public_access ? "" : "_") + result;
  }
  // Resolve a default without sharing mutable objects between instances.
  std::string initial(const type_name& value, const std::string& literal_value = {}) const {
    if (value.type == object_type::class_type) {
      return model.names.at(value.resolved_node) + "()";
    }
    if (value.type == object_type::enum_type) {
      const auto& constants = static_cast<const enum_node&>(*value.resolved_node).enum_name_list;
      return model.names.at(value.resolved_node) + "." +
             pascal(literal_value.empty() ? constants.front() : literal_value);
    }
    if (value.name == "bool") {
      return literal_value == "true" ? "True" : "False";
    }
    if (value.name == "string") {
      return literal_value.empty() ? "\"\"" : literal_value;
    }
    const auto number = literal_value.empty() ? "0" : literal_value;
    return value.name == "float" ? "_FLOAT.unpack(_FLOAT.pack(" + number + "))[0]" : number;
  }
  // Map schema scalars to public Python type annotations.
  std::string type(const type_name& value) const {
    if (value.type != object_type::primitive) {
      return model.names.at(value.resolved_node);
    }
    return value.name == "string"                            ? "str"
           : value.name == "bool"                            ? "bool"
           : value.name == "float" || value.name == "double" ? "float"
                                                             : "int";
  }
  // Build a scalar decoding expression; ordinary nested fields merge in order.
  std::string read(const type_name& value, bool collection, const std::string& old = {}) const {
    if (value.type == object_type::class_type) {
      return model.names.at(value.resolved_node) + "._read(input, " + (old.empty() ? "None" : old) +
             ")";
    }
    if (value.type == object_type::enum_type) {
      const auto name = model.names.at(value.resolved_node);
      return "input.enumeration(" + name + ", _" + name + "_names, " +
             (collection ? "True" : "False") + ")";
    }
    if (value.name == "string") {
      return "input.text()";
    }
    if (value.name == "bool") {
      return "input.boolean()";
    }
    if (value.name == "char") {
      return "input.character()";
    }
    if (value.name == "float" || value.name == "double") {
      return "input.floating(" + std::string{value.name == "float" ? "True" : "False"} + ")";
    }
    return "input.integer(" + std::to_string(width(value)) + ", " +
           (value.name.starts_with('u') ? "True" : "False") + ")";
  }
  // Write one typed scalar directly to the output buffer.
  void write(const type_name& value, const std::string& expression, bool collection) {
    if (value.type == object_type::class_type) {
      line(expression + "._write(out)");
    } else if (value.type == object_type::enum_type) {
      line("out.enumeration(" + expression + ", _" + model.names.at(value.resolved_node) +
           "_names, " + (collection ? "True" : "False") + ")");
    } else if (value.name == "string") {
      line("out.text(" + expression + ")");
    } else if (value.name == "bool") {
      line("out.boolean(" + expression + ")");
    } else if (value.name == "char") {
      line("out.character(" + expression + ")");
    } else if (value.name == "float" || value.name == "double") {
      line("out.floating(" + expression + ", " + (value.name == "float" ? "True" : "False") + ")");
    } else {
      line("out.integer(" + expression + ", " + std::to_string(width(value)) + ", " +
           (value.name.starts_with('u') ? "True" : "False") + ")");
    }
  }
  // Emit a wire key reference without allocating key bytes per message.
  void key(std::uint32_t id, const std::string& text, bool first) {
    line("out.field(" + std::to_string(id) + ", " + std::to_string(model.keys.at(text)) + ", " +
         (first ? "True" : "False") + ")");
  }
  // Replace collections and reset the selected union payload when decoding a field.
  void read_member(const member& value, int alternative = -1) {
    const auto destination = "result." + field(value.name, value.access);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      line(destination + " = " + read(item, false, destination));
      return;
    }
    if (value.modifier == member::modifier_type::variant) {
      line(destination +
           "_index = " + (alternative < 0 ? "input.compact()" : std::to_string(alternative)));
      open("match " + destination + "_index");
      for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
        const auto& alt = value.type_name_list[i];
        open("case " + std::to_string(i));
        line(destination + "_" + snake(alt.enum_name) + " = " + read(alt, true));
        --indent;
      }
      line("case _: raise ValueError('Unknown union alternative')");
      --indent;
      return;
    }
    if (value.modifier == member::modifier_type::array) {
      line(destination + " = [" + read(item, true) + " for _ in input.entries()]");
      return;
    }
    line("values = {}");
    open("for _ in input.entries()");
    open("if input.protocol == Protocol.JSON");
    line("has_key = has_value = False");
    open("for entry in input.fields(())");
    open("if entry == 'key'");
    line("entry_key = " + read(map_key(value), true));
    line("has_key = True");
    --indent;
    open("elif entry == 'value'");
    line("entry_value = " + read(item, true));
    line("has_value = True");
    --indent;
    line("else: raise ValueError('Unknown map entry field')");
    --indent;
    line("if not has_key or not has_value: raise ValueError('Incomplete map entry')");
    --indent;
    open("else");
    line("entry_key = " + read(map_key(value), true));
    line("entry_value = " + read(item, true));
    --indent;
    line("values[entry_key] = entry_value");
    --indent;
    line(destination + " = values");
  }
  // Write collections in deterministic key order without copying their values.
  void write_member(const member& value, bool first) {
    const auto source = "self." + field(value.name, value.access);
    if (value.modifier == member::modifier_type::variant) {
      open("match " + source + "_index");
      for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
        const auto& alt = value.type_name_list[i];
        open("case " + std::to_string(i));
        key(value.id, value.display_name + ":" + alt.enum_name, first);
        line("if out.protocol in (Protocol.BINARY_NONE, Protocol.BINARY_INTEGER): out.compact(" +
             std::to_string(i) + ")");
        write(alt, source + "_" + snake(alt.enum_name), true);
        --indent;
      }
      line("case _: raise ValueError('Unknown union alternative')");
      --indent;
      return;
    }
    key(value.id, value.display_name, first);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      write(item, source, false);
      return;
    }
    line("out.begin_array(len(" + source + "))");
    open("for index, item in enumerate(" +
         (value.modifier == member::modifier_type::map ? "sorted(" + source + ")" : source) + ")");
    line("out.element(index)");
    if (value.modifier == member::modifier_type::map) {
      line("if out.protocol == Protocol.JSON: out.begin_object()");
      open("if out.protocol == Protocol.JSON");
      key(0, "key", true);
      --indent;
      write(map_key(value), "item", true);
      open("if out.protocol == Protocol.JSON");
      key(0, "value", false);
      --indent;
      write(item, source + "[item]", true);
      line("if out.protocol == Protocol.JSON: out.end_object()");
    } else {
      write(item, "item", true);
    }
    --indent;
    line("out.end_array()");
  }
  // Emit slot-based owning models with independent defaults and exact decode APIs.
  void object(const class_node& value) {
    const auto name = model.names.at(&value);
    open("class " + name);
    line("\"\"\"Owning schema model; do not mutate during encoding.\"\"\"");
    std::vector<std::pair<std::string, std::pair<std::string, std::string>>> fields;
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      const auto type_name = model.names.at(base.parent_class);
      fields.push_back(
          {field("base" + std::to_string(i), base.access), {type_name, type_name + "()"}});
    }
    for (const auto& item : value.member_list) {
      const auto f = field(item.name, item.access);
      const auto& t = item.type_name_list.front();
      if (item.modifier == member::modifier_type::variant) {
        fields.push_back({f + "_index", {"int", "0"}});
        for (const auto& alt : item.type_name_list) {
          fields.push_back({f + "_" + snake(alt.enum_name), {type(alt), initial(alt)}});
        }
      } else if (item.modifier == member::modifier_type::none) {
        fields.push_back({f, {type(t), initial(t, literal(item))}});
      } else {
        fields.push_back({f,
                          {item.modifier == member::modifier_type::array
                               ? "list[" + type(t) + "]"
                               : "dict[" + type(map_key(item)) + ", " + type(t) + "]",
                           item.modifier == member::modifier_type::array ? "[]" : "{}"}});
      }
    }
    std::string slots = "__slots__ = (";
    for (const auto& f : fields) {
      slots += quote(f.first) + ", ";
    }
    line(slots + ")");
    for (const auto& f : fields) {
      line(f.first + ": " + f.second.first);
    }
    open("def __init__(self)");
    line("\"\"\"Construct independent schema defaults.\"\"\"");
    for (const auto& f : fields) {
      line("self." + f.first + " = " + f.second.second);
    }
    --indent;
    open("def encode(self, protocol: Protocol) -> bytes");
    line("\"\"\"Encode one complete message into independent bytes.\"\"\"");
    line("out = _Writer(protocol)");
    line("self._write(out)");
    line("return bytes(out.data)");
    --indent;
    line("@classmethod");
    open("def decode(cls, data, protocol: Protocol, limits: Limits | None = None) -> " + name);
    line("\"\"\"Return a fresh value after exact-message validation.\"\"\"");
    line("input = _Reader(data, protocol, limits)");
    line("result = cls._read(input)");
    line("input.finish()");
    line("return result");
    --indent;
    open("def _write(self, out)");
    line("\"\"\"Write typed fields in schema order.\"\"\"");
    line("out.begin_object()");
    bool first = true;
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      key(base.id, base.display_name, first);
      line("self." + field("base" + std::to_string(i), base.access) + "._write(out)");
      first = false;
    }
    for (const auto& item : value.member_list) {
      write_member(item, first);
      first = false;
    }
    line("out.end_object()");
    --indent;
    line("@classmethod");
    open("def _read(cls, input, result=None)");
    line("\"\"\"Merge ordinary nested fields while preserving input order.\"\"\"");
    line("if result is None: result = cls()");
    std::string ids = "(";
    for (const auto& base : value.parents) {
      ids += std::to_string(base.id) + ",";
    }
    for (const auto& item : value.member_list) {
      ids += std::to_string(item.id) + ",";
    }
    open("for key in input.fields(" + ids + "))");
    open("match key");
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      const auto dst = "result." + field("base" + std::to_string(i), base.access);
      open("case " + std::to_string(base.id) + " | " + quote(base.display_name));
      line(dst + " = " + model.names.at(base.parent_class) + "._read(input, " + dst + ")");
      --indent;
    }
    for (const auto& item : value.member_list) {
      open("case " + std::to_string(item.id) +
           (item.modifier == member::modifier_type::variant ? ""
                                                            : " | " + quote(item.display_name)));
      read_member(item);
      --indent;
      if (item.modifier == member::modifier_type::variant) {
        for (std::size_t i = 0; i < item.type_name_list.size(); ++i) {
          open("case " + quote(item.display_name + ":" + item.type_name_list[i].enum_name));
          read_member(item, static_cast<int>(i));
          --indent;
        }
      }
    }
    line("case _: raise ValueError('Unknown field')");
    indent -= 2;
    line("return result");
    indent -= 2;
  }

public:
  // Bind the validated schema for one output module.
  explicit python_emitter(const schema& value) : model{value} {}
  // Generate a complete module with pre-encoded field keys and typed models.
  std::string generate() {
    line("_FIELDS = (");
    ++indent;
    for (const auto& key : model.ordered_keys) {
      line("_field(" + quote(key) + "),");
    }
    --indent;
    line(")");
    for (const auto* node : model.nodes) {
      validate_identifier(
          model.names.at(node),
          " ValueError TypeError OverflowError RuntimeError RecursionError MemoryError "
          "UnicodeError UnicodeDecodeError UnicodeEncodeError Exception BaseException ");
      if (node->type == object_type::class_type) {
        object(static_cast<const class_node&>(*node));
        continue;
      }
      const auto& value = static_cast<const enum_node&>(*node);
      const auto& name = model.names.at(node);
      open("class " + name + "(_IntEnum)");
      line("\"\"\"Stable schema ordinals; names retain their wire spelling.\"\"\"");
      std::string names = "_" + name + "_names = (";
      for (std::size_t i = 0; i < value.enum_name_list.size(); ++i) {
        const auto constant = pascal(value.enum_name_list[i]);
        validate_identifier(constant, keywords);
        line(constant + " = " + std::to_string(i));
        names += quote(value.enum_name_list[i]) + ",";
      }
      --indent;
      line(names + ")");
    }
    return output;
  }
};
} // namespace
// Keep Python source generation inside the native C++ compiler.
std::string python(const schema& value) {
  return python_emitter{value}.generate();
}
} // namespace rohit::serializer::writer::native
