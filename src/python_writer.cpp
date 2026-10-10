#include "schema_projection.hpp"
#include "native_schema.hpp"
#include "version_writer.hpp"
#include "behavior_writer.hpp"

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
  bool dynamic_first{};
  // Restrict protocol tests to fields whose schema explicitly requests native exclusions.
  std::string omitted_condition(const std::vector<std::string>& formats, std::string_view owner) {
    std::string values{};
    for (const auto& format : formats) {
      const auto name = format == "json" ? "JSON" : format == "binary_none" ? "BINARY_NONE" :
                        format == "binary_integer" ? "BINARY_INTEGER" : format == "binary_string" ? "BINARY_STRING" : "";
      if (*name != '\0') { values += "Protocol." + std::string{name} + ","; }
    }
    return values.empty() ? std::string{} : std::string{owner} + ".protocol in (" + values + ")";
  }
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
    if (value.is_digest()) { return "[0] * " + std::to_string(value.digest_extent); }
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
    if (value.name == "string" || value.name.starts_with("version")) {
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
    if (value.is_digest()) { return "list[int]"; }
    return (value.name == "string" || value.name.starts_with("version")) ? "str"
           : value.name == "bool"                                        ? "bool"
           : value.name == "float" || value.name == "double"             ? "float"
                                                                         : "int";
  }
  // Build a scalar decoding expression; ordinary nested fields merge in order.
  std::string read(const type_name& value, bool collection, const std::string& old = {}) const {
    if (value.is_digest()) {
      return "_read_digest(input, " + std::to_string(value.digest_extent) + ")";
    }
    if (value.type == object_type::class_type) {
      return model.names.at(value.resolved_node) + "._read(input, " + (old.empty() ? "None" : old) +
             ")";
    }
    if (value.type == object_type::enum_type) {
      const auto name = model.names.at(value.resolved_node);
      return "input.enumeration(" + name + ", _" + name + "_names, " +
             (collection ? "True" : "False") + ")";
    }
    if (value.name.starts_with("version")) {
      return "_read_version(input, " + std::string{value.name.back()} + ")";
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
    if (value.is_digest()) {
      line("_write_digest(out, " + expression + ", " + std::to_string(value.digest_extent) + ")");
    } else if (value.type == object_type::class_type) {
      line(expression + "._write(out)");
    } else if (value.type == object_type::enum_type) {
      line("out.enumeration(" + expression + ", _" + model.names.at(value.resolved_node) +
           "_names, " + (collection ? "True" : "False") + ")");
    } else if (value.name.starts_with("version")) {
      line("_write_version(out, " + expression + ", " + std::string{value.name.back()} + ")");
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
         (dynamic_first ? "_srl_first" : first ? "True" : "False") + ")");
    if (dynamic_first) { line("_srl_first = False"); }
  }
  // Replace collections and reset the selected union payload when decoding a field.
  void read_member(const member& value, int alternative = -1) {
    const auto omitted = omitted_condition(value.omitted_formats, "input");
    if (!omitted.empty()) {
      line("if " + omitted + ": raise ValueError('Field omitted from selected format')");
    }
    const auto destination = "result." + field(value.name, value.access);
    const auto& item = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      if (value.compact != compact_encoding::none) {
        line(destination + " = input.compact_value(" + std::to_string(width(item)) + ", " +
             (value.compact == compact_encoding::prefix ? "True" : "False") + ")");
        return;
      }
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
      const auto extent = value.fixed_extent ? std::to_string(value.fixed_extent) : std::string{};
      line(destination + " = [" + read(item, true) + " for _ in input.entries(" + extent + ")]");
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
      if (value.compact != compact_encoding::none) {
        line("out.compact_value(" + source + ", " + std::to_string(width(item)) + ", " +
             (value.compact == compact_encoding::prefix ? "True" : "False") + ", " +
             (value.compact_strict ? "True" : "False") + ")");
        return;
      }
      write(item, source, false);
      return;
    }
    if (value.fixed_extent != 0) {
      line("if len(" + source + ") != " + std::to_string(value.fixed_extent) +
           ": raise ValueError('Fixed array extent mismatch')");
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
  // Check one revision without adding any branches to unversioned models.
  void check_version(const class_node& object, const std::string& expression, bool input) {
    const auto& version = *object.version_member();
    const auto language = version_language::python;
    auto invalid = version_less(language, version, expression, schema_version::minimum(version)) +
                   " or " + version_at_least(language, version, expression, version.default_value) +
                   " and not " +
                   version_equal(language, version, expression, version.default_value);
    if (version.type_name_list.front().name == "float" ||
        version.type_name_list.front().name == "double") {
      invalid += " or not _math.isfinite(" + expression + ")";
    }
    if (input) {
      invalid += " or (input.limits.read_policy == ReadPolicy.STRICT and not " +
                 version_equal(language, version, expression, version.default_value) + ")";
    }
    line("if " + invalid + ": raise ValueError('Unsupported schema version')");
  }
  // Track keyed presence, but consume no positional bytes for inactive historical fields.
  void read_versioned_member(const class_node& object, const member& item, int alternative = -1) {
    const auto* version = object.version_member();
    if (!version) {
      read_member(item, alternative);
      return;
    }
    line("if " + std::to_string(item.id) +
         " in seen: raise ValueError('Duplicate versioned field')");
    line("seen.add(" + std::to_string(item.id) + ")");
    const auto active = version_active(version_language::python, item, *version,
                                       "result." + field(version->name, version->access));
    if (!active.empty()) {
      open("if input.protocol != Protocol.BINARY_NONE or (" + active + ")");
    }
    read_member(item, alternative);
    if (!active.empty()) {
      --indent;
    }
    if (item.version) {
      check_version(object, "result." + field(item.name, item.access), true);
    }
  }
  // Emit native/pure methods and validate explicit external behavior attachments.
  void behaviors(const class_node& value) {
    std::vector<const function_declaration*> external;
    for (const auto& method : value.functions) {

      if (!method.expression && !behavior::native_body(method, "python")) { external.push_back(&method); }
    }
    for (const auto* method : behavior::contracts(value)) {
      if (std::find(external.begin(), external.end(), method) == external.end()) { external.push_back(method); }
    }
    behavior::require_unique_callable_names(value, "python", [&](auto n) { return field(n); });
    if (!external.empty()) {
      open("def attach_behavior(self, behavior)");
      for (const auto* method : external) {
        open("if not callable(behavior.get(" + quote(method->name) + "))");
        line("raise TypeError('Missing required behavior callable')"); --indent;
      }
      line("self._srl_behavior = dict(behavior)"); --indent;
    }
    for (const auto& item : behavior::ordered_body(value)) {
      if (item.kind == class_body_item::kind_type::native_code) {
        const auto& block = value.native_blocks.at(item.index);
        if (block.language == "python") { output += std::string(indent * 4, ' ') + behavior::source_marker(block, "python") + behavior::python_block(block.text, indent * 4); }
        continue;
      }
      if (item.kind != class_body_item::kind_type::function) { continue; }
      const auto& method = value.functions.at(item.index);
      line(behavior::source_marker(method, "python").substr(0, behavior::source_marker(method, "python").size() - 1));
      std::string parameters = "self", arguments = "self";
      for (const auto& argument : method.parameters) {
        parameters += ", " + field(argument.name); arguments += ", " + field(argument.name);
      }
      open("def " + field(method.name, method.access) + "(" + parameters + ")");
      if (method.dispatch != function_declaration::dispatch_type::ordinary &&
          (method.expression || behavior::native_body(method, "python"))) {
        open("if self._srl_behavior is not None");
        line("return self._srl_behavior[" + quote(method.name) + "](" + arguments + ")"); --indent;
      }
      if (const auto* native = behavior::native_body(method, "python")) {
        output += std::string(indent * 4, ' ') + behavior::source_marker(*native, "python") + behavior::python_block(native->text, indent * 4);
      } else if (method.expression) {
        const auto symbol = [&](std::string_view name) {
          const auto argument = std::find_if(method.parameters.begin(), method.parameters.end(),
              [&](const auto& item) { return item.name == name; });
          if (argument != method.parameters.end()) { return field(name); }
          const auto member = std::find_if(value.member_list.begin(), value.member_list.end(),
              [&](const auto& item) { return item.name == name; });
          return "self." + field(member->name, member->access);
        };
        std::set<std::string> symbols; behavior::expression_symbols(*method.expression, symbols);
        for (const auto& name : symbols) {
          const auto text = symbol(name);
          open("if type(" + text + ") not in (float, int) or not " + behavior::finite(text, "python"));
          line("raise ValueError('Portable expression requires finite numeric inputs')"); --indent;
          const auto input_type = behavior::symbol_type(value, method, name);
          if (input_type == "int32" || input_type == "uint32") {
            open("if type(" + text + ") is not int or not (" +
                (input_type == "int32" ? "-2147483648" : "0") + " <= " + text + " <= " +
                (input_type == "int32" ? "2147483647" : "4294967295") + ")");
            line("raise ValueError('Portable integer conversion is outside the declared type')"); --indent;
          }
        }
        const auto binary64_symbol = [&](std::string_view name) { return "float(" + symbol(name) + ")"; };
        line("_srl_result = " + behavior::expression(*method.expression, "python", binary64_symbol));
        open("if not _math.isfinite(_srl_result)");
        line("raise ValueError('Non-finite portable result')"); --indent;
        line("return _srl_result");
      } else {
        open("if self._srl_behavior is None");
        line("raise RuntimeError('Attach required behavior before invocation')"); --indent;
        line("return self._srl_behavior[" + quote(method.name) + "](" + arguments + ")");
      }
      --indent;
    }
  }

  // An ABC checks abstract completeness; adapters retain explicit data and implementation objects.
  void dispatch_adapter(const class_node& value) {
    const auto contracts = behavior::contracts(value);
    if (contracts.empty()) { return; }
    const auto model_name = model.names.at(&value);
    open("class " + model_name + "Behavior(_ABC)");
    for (const auto* method : contracts) {
      line("@_abstractmethod");
      std::string parameters = "self, receiver";
      for (const auto& argument : method->parameters) { parameters += ", " + field(argument.name); }
      open("def " + field(method->name) + "(" + parameters + ")");
      line("raise NotImplementedError('Required behavior implementation')"); --indent;
    }
    --indent; open("class " + model_name + "BehaviorAdapter");
    open("def __init__(self, receiver, implementation)");
    open("if not isinstance(receiver, " + model_name + ") or not isinstance(implementation, " + model_name + "Behavior)");
    line("raise TypeError('Behavior adapter requires matching data and complete behavior')"); --indent;
    line("self.receiver = receiver"); line("self.implementation = implementation"); --indent;
    for (const auto* method : contracts) {
      std::string parameters = "self", calls = "self.receiver";
      for (const auto& argument : method->parameters) { parameters += ", " + field(argument.name); calls += ", " + field(argument.name); }
      open("def " + field(method->name) + "(" + parameters + ")");
      line("return self.implementation." + field(method->name) + "(" + calls + ")"); --indent;
    }
    --indent;
  }

  // Emit slot-based owning models with independent defaults and exact decode APIs.
  void object(const class_node& value) {
    dynamic_first = value.has_magic() && !value.magic_omits("json");
    for (const auto& item : value.member_list) {
      dynamic_first = dynamic_first || !omitted_condition(item.omitted_formats, "out").empty();
    }
    const auto name = model.names.at(&value);
    open("class " + name);
    line("\"\"\"Owning schema model; do not mutate during encoding.\"\"\"");
    if (value.magic_field) {
      const auto& magic = *value.magic_field;
      line(field("magic", value.magic_access) + " = " +
           initial(magic.type_name_list.front(), literal(magic)));
    } else if (value.has_magic()) {
      std::string bytes = "bytes((";
      for (const auto byte : value.magic_bytes) {
        bytes += std::to_string(static_cast<unsigned char>(byte)) + ",";
      }
      line(field("magic", value.magic_access) + " = " + bytes + "))");
    }
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
                           item.fixed_extent ? "[" + initial(t) + " for _ in range(" +
                               std::to_string(item.fixed_extent) + ")]" :
                           item.modifier == member::modifier_type::array ? "[]" : "{}"}});
      }
    }
    std::string slots = "__slots__ = (";
    for (const auto& f : fields) {
      slots += quote(f.first) + ", ";
    }
    const bool external_behavior = !behavior::contracts(value).empty() || std::any_of(value.functions.begin(), value.functions.end(),
        [](const auto& method) { return !method.expression && !behavior::native_body(method, "python"); });
    if (external_behavior) { slots += "\"_srl_behavior\", "; }
    line(slots + ")");
    for (const auto& f : fields) {
      line(f.first + ": " + f.second.first);
    }
    open("def __init__(self)");
    line("\"\"\"Construct independent schema defaults.\"\"\"");
    for (const auto& f : fields) {
      line("self." + f.first + " = " + f.second.second);
    }
    if (external_behavior) { line("self._srl_behavior = None"); }
    --indent;
    behaviors(value);
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
    if (value.has_magic()) {
      const auto omitted = omitted_condition(value.magic_omitted_formats, "out");
      open("if out.protocol != Protocol.JSON" + (omitted.empty() ? "" : " and not (" + omitted + ")"));
      if (value.magic_field) {
        write(value.magic_field->type_name_list.front(),
              name + "." + field("magic", value.magic_access), false);
      } else {
        open("for magic_byte in " + name + "." + field("magic", value.magic_access));
        line("out.integer(magic_byte, 1, True)");
        --indent;
      }
      --indent;
    }
    if (const auto* version = value.version_member()) {
      check_version(value, "self." + field(version->name, version->access), false);
    }
    line("out.begin_object()");
    if (dynamic_first) { line("_srl_first = True"); }
    if (value.has_magic() && !value.magic_omits("json")) {
      open("if out.protocol == Protocol.JSON");
      key(value.magic_id, "magic", true);
      if (value.magic_field) {
        write(value.magic_field->type_name_list.front(),
              name + "." + field("magic", value.magic_access), false);
      } else {
        line("out.text(" + name + "." + field("magic", value.magic_access) + ".decode('utf-8'))");
      }
      --indent;
    }
    bool first = true;
    if (const auto* version = value.version_member()) {
      write_member(*version, true);
      first = false;
    }
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      key(base.id, base.display_name, first);
      line("self." + field("base" + std::to_string(i), base.access) + "._write(out)");
      first = false;
    }
    for (const auto& item : wire_members(value)) {
      if (item.version) {
        continue;
      }
      const auto* version = value.version_member();
      const auto active = version ? version_active(version_language::python, item, *version,
                                                   "self." + field(version->name, version->access))
                                  : std::string{};
      if (!active.empty()) {
        open("if " + active);
      }
      const auto omitted = omitted_condition(item.omitted_formats, "out");
      if (!omitted.empty()) { open("if not (" + omitted + ")"); }
      write_member(item, first);
      if (!omitted.empty()) { --indent; }
      if (!active.empty()) {
        --indent;
      }
      first = false;
    }
    line("out.end_object()");
    --indent;
    line("@classmethod");
    open("def _read(cls, input, result=None)");
    line("\"\"\"Merge ordinary nested fields while preserving input order.\"\"\"");
    if (value.has_magic()) {
      const auto omitted = omitted_condition(value.magic_omitted_formats, "input");
      open("if input.protocol != Protocol.JSON" + (omitted.empty() ? "" : " and not (" + omitted + ")"));
      if (value.magic_field) {
        line("if " + read(value.magic_field->type_name_list.front(), false) + " != cls." +
             field("magic", value.magic_access) + ": raise ValueError('Binary magic header mismatch')");
      } else {
        open("for magic_byte in cls." + field("magic", value.magic_access));
        line("if input.integer(1, True) != magic_byte: raise ValueError('Binary magic header mismatch')");
        --indent;
      }
      --indent;
    }
    line("if result is None: result = cls()");
    const bool json_magic = value.has_magic() && !value.magic_omits("json");
    if (json_magic) { line("_srl_magic_seen = False"); }
    if (value.version_member()) {
      line("seen = set()");
    }
    std::string ids = "(";
    if (const auto* version = value.version_member()) {
      ids += std::to_string(version->id) + ",";
    }
    for (const auto& base : value.parents) {
      ids += std::to_string(base.id) + ",";
    }
    for (const auto& item : wire_members(value)) {
      if (!item.version) {
        if (!item.omits("binary_none")) { ids += std::to_string(item.id) + ","; }
      }
    }
    open("for key in input.fields(" + ids + "))");
    open("match key");
    if (json_magic) {
      open("case 'magic' if input.protocol == Protocol.JSON");
      line("if _srl_magic_seen: raise ValueError('Duplicate magic header')");
      const auto magic_value = value.magic_field
          ? read(value.magic_field->type_name_list.front(), false) : "input.text()";
      const auto expected = "cls." + field("magic", value.magic_access) +
                            (value.magic_field ? "" : ".decode('utf-8')");
      line("if " + magic_value + " != " + expected +
           ": raise ValueError('JSON magic header mismatch')");
      line("_srl_magic_seen = True");
      --indent;
    } else if (value.has_magic()) {
      open("case 'magic' if input.protocol == Protocol.JSON");
      line("raise ValueError('Magic header omitted from JSON')");
      --indent;
    }
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      const auto dst = "result." + field("base" + std::to_string(i), base.access);
      open("case " + std::to_string(base.id) + " | " + quote(base.display_name));
      line(dst + " = " + model.names.at(base.parent_class) + "._read(input, " + dst + ")");
      --indent;
    }
    for (const auto& item : wire_members(value)) {
      open("case " + std::to_string(item.id) +
           (item.modifier == member::modifier_type::variant ? ""
                                                            : " | " + quote(item.display_name)));
      read_versioned_member(value, item);
      --indent;
      if (item.modifier == member::modifier_type::variant) {
        for (std::size_t i = 0; i < item.type_name_list.size(); ++i) {
          open("case " + quote(item.display_name + ":" + item.type_name_list[i].enum_name));
          read_versioned_member(value, item, static_cast<int>(i));
          --indent;
        }
      }
    }
    line("case _: input.skip_value()");
    indent -= 2;
    if (json_magic) {
      line("if input.protocol == Protocol.JSON and not _srl_magic_seen: raise ValueError('Missing magic header')");
    }
    if (const auto* version = value.version_member()) {
      line("if " + std::to_string(version->id) +
           " not in seen: raise ValueError('Missing schema version')");
      check_version(value, "result." + field(version->name, version->access), true);
      for (const auto& item : wire_members(value)) {
        const auto active = version_active(version_language::python, item, *version,
                                           "result." + field(version->name, version->access));
        if (!active.empty()) {
          line("if input.protocol != Protocol.BINARY_NONE and " + std::to_string(item.id) +
               " in seen and not (" + active +
               "): raise ValueError('Field outside version lifecycle')");
        }
      }
    }
    line("return result");
    indent -= 2;
  }

public:
  // Bind the validated schema for one output module.
  explicit python_emitter(const schema& value) : model{value} {}
  // Generate a complete module with pre-encoded field keys and typed models.
  std::string generate() {
    line("from abc import ABC as _ABC, abstractmethod as _abstractmethod");
    for (const auto& code : model.preambles) {
      if (code.language == "python") { output += behavior::source_marker(code, "python") + code.text + "\n"; }
    }
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
        dispatch_adapter(static_cast<const class_node&>(*node));
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
