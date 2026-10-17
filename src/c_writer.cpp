#include "schema_projection.hpp"
#include "native_schema.hpp"
#include "version_writer.hpp"
#include "format_writer.hpp"
#include "behavior_writer.hpp"
#include "statement_behavior_writer.hpp"
#include "auto_size_writer.hpp"
#include "construction_writer.hpp"
#include "conversion_writer.hpp"
#include "comparison_writer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::native {
namespace {
constexpr std::string_view runtime =
#include "c_runtime.inc"
    ;
constexpr std::string_view keywords =
    " auto break case char const continue default do double else enum extern float for goto if "
    "inline int long register restrict return short signed sizeof static struct switch typedef "
    "union unsigned void volatile while bool true false alignas alignof atomic static_assert "
    "thread_local ";
// Emit C11 structs and direct codecs with explicit, transactional ownership.
class c_emitter {
  const schema& model;
  behavior::checked_helpers arithmetic_helpers{};
  std::string output{
      "#pragma once\n#ifndef SRL_NATIVE_RUNTIME_INCLUDED\n#define SRL_NATIVE_RUNTIME_INCLUDED\n"};
  std::size_t indent{};
  bool variable_first{};
  std::string fields_name;
  std::map<std::string, type_name> generic_types{};
  std::set<std::string> defined_generic_types{};
  std::set<const class_node*> defined_classes{};
  std::set<const class_node*> defining_classes{};
  std::size_t next_behavior_call{};
  // Reserve generated global symbols before emitting C's shared typedef/function namespace.
  void validate_symbols() const {
    behavior::require_unique_free_names(model.nodes, "c", [&](const auto& node) { return name(&node); });
    std::set<std::string> symbols{"size_t",   "ptrdiff_t", "int8_t",   "int16_t",    "int32_t",
                                  "int64_t",  "uint8_t",   "uint16_t", "uint32_t",   "uint64_t",
                                  "intptr_t", "uintptr_t", "intmax_t", "uintmax_t",  "malloc",
                                  "calloc",   "realloc",   "free",     "memset",     "memcpy",
                                  "memcmp",   "strlen",    "strcmp",   "strtod",     "strtof",
                                  "strtoull", "qsort",     "snprintf", "localeconv", "errno"};
    if (std::any_of(model.class_comparisons.begin(), model.class_comparisons.end(),
                    [](const auto& item) { return item.second.ordering; })) {
      symbols.insert({"srl_comparison_result", "srl_less", "srl_equal", "srl_greater", "srl_unordered"});
    }
    const auto add = [&](const std::string& symbol) {
      validate_identifier(symbol, keywords);
      if (!symbols.insert(symbol).second) {
        throw std::invalid_argument{"C symbol collision: " + symbol};
      }
    };
    for (const auto* node : model.nodes) {
      const auto n = name(node);
      add(n);
      if (node->type == object_type::function_type) {
        if (static_cast<const function_node&>(*node).function.expression) { add(n + "_checked"); }
        continue;
      }
      if (node->type == object_type::enum_type) {
        add(n + "_srl_names");
        for (const auto& item : static_cast<const enum_node&>(*node).enum_name_list) {
          add(n + "_" + field(item));
        }
      } else {
        const auto& owner = static_cast<const class_node&>(*node);
        for (const auto& item : owner.member_list) {
          if (!item.array_exposure) { continue; }
          for (const auto method : {"get", "size", "values"}) { add(n + "_" + field(method)); }
          if (!item.array_exposure->readonly) { add(n + "_" + field("set")); }
          for (const auto& method : owner.functions) { add(n + "_" + field(method.name)); }
        }
        for (const auto& factory : static_cast<const class_node&>(*node).constructions) {
          const auto method = n + "_" + field(factory.constructor ? "create" : factory.name);
          add(method);
          for (std::size_t arity{}; arity < factory.parameters.size(); ++arity) {
            const auto& parameter = factory.parameters[arity];
            if (!parameter.default_value.empty() || !parameter.default_factory.empty()) {
              add(method + "_default_" + std::to_string(arity));
            }
          }
        }
        for (const auto& conversion : static_cast<const class_node&>(*node).conversions) {
          const auto method = n + "_" + field(conversion.name);
          add(method);
          add(method + "_result");
          add(method + "_result_free");
        }
        for (const auto suffix :
             {"_init", "_free", "_encode", "_decode", "_srl_read", "_srl_write"}) {
          add(n + suffix);
        }
        if (const auto* options = comparison::find(static_cast<const class_node&>(*node),
                                                  model.class_comparisons)) {
          if (options->equality) { add(n + "_equals_value"); }
          if (options->ordering) { add(n + "_compare_value"); }
        }
        for (const auto& item : static_cast<const class_node&>(*node).member_list) {
          if (item.modifier == member::modifier_type::array ||
              item.modifier == member::modifier_type::map) {
            const auto c = collection(static_cast<const class_node&>(*node), item);
            add(c);
            add(c + "_clear");
            if (item.modifier == member::modifier_type::map) {
              for (const auto suffix : {"_entry", "_normalize", "_compare", "_compare_pointer"}) {
                add(c + suffix);
              }
            }
          }
        }
      }
    }
  }
  // Append one indented C statement.
  void line(const std::string& text = {}) {
    output.append(indent * 2, ' ');
    output += text + '\n';
  }
  // Enter a C lexical scope.
  void open(const std::string& text) {
    line(text + " {");
    ++indent;
  }
  // Leave a C lexical scope.
  void close(const std::string& suffix = {}) {
    --indent;
    line("}" + suffix);
  }
  // Validate public C identifiers before publishing generated code.
  std::string field(std::string_view name) const {
    auto result = snake(name);
    validate_identifier(result, keywords);
    return result;
  }
  // Resolve a flattened native C type name.
  std::string name(const syntax_node* node) const {
    return snake(model.names.at(node));
  }
  // Select exact-width C scalar storage.
  std::string type(const type_name& value) const {
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      std::string result = "srl_" + value.name;
      for (const auto& argument : value.arguments) {
        const auto argument_type = type(argument);
        result += "_" + std::to_string(argument_type.size()) + "_" + argument_type;
      }
      return result;
    }
    if (value.type != object_type::primitive) {
      return name(value.resolved_node);
    }
    if (value.is_digest()) { return "srl_buffer"; }
    if ((value.name == "string" || value.name.starts_with("version"))) {
      return "srl_string";
    }
    if (value.name == "char") {
      return "uint8_t";
    }
    return value.name == "bool" || value.name == "float" || value.name == "double"
               ? value.name
               : value.name + "_t";
  }
  // Reject unsupported revisions before input commits or output is returned.
  void check_version(const class_node& owner, bool reading) {
    const auto& version = *owner.version_member();
    const auto value = "value->" + field(version.name);
    auto invalid =
        version_less(version_language::c, version, value, schema_version::minimum(version)) +
        " || !(" + version_less(version_language::c, version, value, version.default_value) +
        " || " + version_equal(version_language::c, version, value, version.default_value) + ")";
    if (reading) {
      invalid += " || (in->limits.read_policy == srl_read_strict && !" +
                 version_equal(version_language::c, version, value, version.default_value) + ")";
    }
    line("if (" + invalid + ") { " + std::string{reading ? "in" : "out"} +
         "->status = srl_invalid; return; }");
  }
  // Skip inactive positional bytes while retaining known keyed presence.
  void read_versioned_member(const class_node& owner, const member& item) {
    const auto omitted = omitted_condition(item.omitted_formats, "in->protocol", "srl_",
                                            protocol_name_style::lower_snake);
    if (!omitted.empty()) {
      line("if (" + omitted + ") { in->status = srl_invalid; break; }");
    }
    const auto* version = owner.version_member();
    if (version) {
      const auto slot = std::to_string(version_slot(owner, item));
      line("if (seen[" + slot + "]) { in->status = srl_invalid; break; } seen[" + slot +
           "] = true;");
    }
    const auto active = version ? version_active(version_language::c, item, *version,
                                                 "value->" + field(version->name))
                                : std::string{};
    if (!active.empty()) {
      open("if (in->protocol != srl_binary_none || (" + active + "))");
    }
    read_member(owner, item);
    if (!active.empty()) {
      close();
    }
    if (item.version) {
      line("if (in->status != srl_ok) { return; }");
      check_version(owner, true);
    }
  }
  // Name a class-specific typed collection, avoiding void-pointer public fields.
  std::string collection(const class_node& owner, const member& value) const {
    return name(&owner) + "_" + field(value.name) +
           (value.modifier == member::modifier_type::map ? "_map" : "_array");
  }
  // Format an enum constant with a collision-resistant type prefix.
  std::string enum_value(const type_name& value, const std::string& literal_value = {}) const {
    const auto& items = static_cast<const enum_node&>(*value.resolved_node).enum_name_list;
    return type(value) + "_" + field(literal_value.empty() ? items.front() : literal_value);
  }
  // Produce portable full-width integer and floating literal spellings.
  std::string scalar_default(const type_name& value, std::string literal_value) const {
    if (value.type == object_type::enum_type) {
      return enum_value(value, literal_value);
    }
    if (literal_value.empty()) {
      return "0";
    }
    if (value.name == "int32" && literal_value == "-2147483648") {
      return "INT32_MIN";
    }
    if (value.name == "int64") {
      return literal_value == "-9223372036854775808" ? "(-INT64_C(9223372036854775807)-1)"
                                                     : "INT64_C(" + literal_value + ")";
    }
    if (value.name == "uint64") {
      return "UINT64_C(" + literal_value + ")";
    }
    if (value.name == "uint32") {
      return "UINT32_C(" + literal_value + ")";
    }
    if (value.name == "float") {
      return literal_value.find_first_of(".eE") == std::string::npos ? literal_value + ".0f"
                                                                     : literal_value + "f";
    }
    if (value.name == "double" && literal_value.find_first_of(".eE") == std::string::npos) {
      // Keep accepted floating defaults out of C's integer-literal range and signedness rules.
      return literal_value + ".0";
    }
    return literal_value;
  }
  // Initialize one scalar in zeroed storage; propagate the first allocation failure.
  void init_value(const type_name& value, const std::string& dst, const std::string& literal_value,
                  const std::string& status) {
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      line("memset(&" + dst + ", 0, sizeof(" + dst + "));");
    } else if (value.is_digest()) {
      line("if (" + status + " == srl_ok) { " + status + " = srl_digest_init(&" + dst + ", " + std::to_string(value.digest_extent) + "); }");
    } else if (value.type == object_type::class_type) {
      line("if (" + status + " == srl_ok) { " + status + " = " + type(value) + "_init(&" + dst +
           "); }");
    } else if ((value.name == "string" || value.name.starts_with("version"))) {
      if (!literal_value.empty() && literal_value != "\"\"") {
        line("if (" + status + " == srl_ok) { " + status + " = srl_string_set(&" + dst + ", " +
             literal_value + ", sizeof(" + literal_value + ")-1); }");
      }
    } else {
      line(dst + " = " + scalar_default(value, literal_value) + ";");
    }
  }
  // Release one owning scalar; zeroing nested models prevents stale ownership.
  void free_value(const type_name& value, const std::string& dst) {
    if (value.is_optional()) {
      open("if (" + dst + ".has_value)");
      free_value(value.arguments.front(), dst + ".value");
      close();
      line("memset(&" + dst + ", 0, sizeof(" + dst + "));");
    } else if (value.is_sequence() || value.is_associative()) {
      line(type(value) + "_clear(&" + dst + ");");
    } else if (value.is_digest()) {
      line("srl_buffer_free(&" + dst + ");");
    } else if (value.type == object_type::class_type) {
      line(type(value) + "_free(&" + dst + ");");
    } else if ((value.name == "string" || value.name.starts_with("version"))) {
      line("srl_string_free(&" + dst + ");");
    }
  }
  // Read a scalar directly into the candidate, merging ordinary nested objects.
  void read_value(const type_name& value, const std::string& dst, bool collection_context) {
    read_value(value, dst, std::string{collection_context ? "true" : "false"});
  }
  // Keep enum context transparent through generic optional wrappers.
  void read_value(const type_name& value, const std::string& dst,
                  const std::string& collection_context) {
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      line(type(value) + "_srl_read(in, &" + dst + ", " + collection_context + ");");
    } else if (value.is_digest()) {
      line("srl_read_digest(in, &" + dst + ", " + std::to_string(value.digest_extent) + ");");
    } else if (value.type == object_type::class_type) {
      line(type(value) + "_srl_read(in, &" + dst + ");");
    } else if (value.type == object_type::enum_type) {
      line(dst + " = (" + type(value) + ")srl_read_enum(in, " + type(value) +
           "_srl_names, sizeof(" + type(value) + "_srl_names)/sizeof(" + type(value) +
           "_srl_names[0]), " + collection_context + ");");
    } else if (value.name.starts_with("version")) {
      line("srl_read_version(in, &" + dst + ", " + std::string{value.name.back()} + ");");
    } else if (value.name == "string") {
      line("srl_read_string(in, &" + dst + ");");
    } else if (value.name == "bool") {
      line(dst + " = srl_read_bool(in);");
    } else if (value.name == "char") {
      line(dst + " = srl_read_char(in);");
    } else if (value.name == "float") {
      line(dst + " = srl_read_float32(in);");
    } else if (value.name == "double") {
      line(dst + " = srl_read_float(in, false);");
    } else {
      const auto bytes = std::to_string(width(value));
      auto expression = "srl_read_integer(in, " + bytes + ", " +
                        (value.name.starts_with('u') ? "true" : "false") + ")";
      if (!value.name.starts_with('u')) {
        expression = "srl_signed(" + expression + ", " + bytes + ")";
      }
      line(dst + " = (" + type(value) + ")" + expression + ";");
    }
  }
  // Write one typed scalar without a generic object representation.
  void write_value(const type_name& value, const std::string& src, bool collection_context) {
    write_value(value, src, std::string{collection_context ? "true" : "false"});
  }
  // Keep enum context transparent through generic optional wrappers.
  void write_value(const type_name& value, const std::string& src,
                  const std::string& collection_context) {
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      line(type(value) + "_srl_write(out, &" + src + ", " + collection_context + ");");
    } else if (value.is_digest()) {
      line("srl_write_digest(out, &" + src + ", " + std::to_string(value.digest_extent) + ");");
    } else if (value.type == object_type::class_type) {
      line(type(value) + "_srl_write(out, &" + src + ");");
    } else if (value.type == object_type::enum_type) {
      line("srl_write_enum(out, (uint32_t)" + src + ", " + type(value) + "_srl_names, sizeof(" +
           type(value) + "_srl_names)/sizeof(" + type(value) + "_srl_names[0]), " +
           collection_context + ");");
    } else if (value.name.starts_with("version")) {
      line("srl_write_version(out, &" + src + ", " + std::string{value.name.back()} + ");");
    } else if (value.name == "string") {
      line("srl_write_string(out, &" + src + ");");
    } else if (value.name == "bool") {
      line("srl_write_bool(out, " + src + ");");
    } else if (value.name == "char") {
      line("srl_write_char(out, " + src + ");");
    } else if (value.name == "float") {
      line("srl_write_float32(out, " + src + ");");
    } else if (value.name == "double") {
      line("srl_write_float(out, " + src + ", false);");
    } else {
      line("srl_write_integer(out, (uint64_t)" + src + ", " + std::to_string(width(value)) + ", " +
           (value.name.starts_with('u') ? "true" : "false") + ");");
    }
  }
  // Reference immutable encoded key bytes.
  void key(std::uint32_t id, const std::string& text, bool first) {
    line("srl_write_field(out, " + std::to_string(id) + ", &" + fields_name + "[" +
         std::to_string(model.keys.at(text)) + "], " +
         (variable_first ? "first_field" : first ? "true" : "false") + ");");
  }
  // Compare exact map key values without subtracting potentially overflowing integers.
  std::string compare(const type_name& value, const std::string& a, const std::string& b) const {
    return value.name == "string" ? "srl_string_compare(&" + a + ", &" + b + ")"
                                  : "(" + a + " > " + b + ") - (" + a + " < " + b + ")";
  }
  // Emit typed collection cleanup and map normalization helpers.
  void collection_helpers(const std::string& n, const type_name& t, bool map,
                          const type_name& key_type) {
    line("/* Release every initialized collection slot, including partially decoded values. */");
    open("static inline void " + n + "_clear(" + n + "* value)");
    open("for (size_t i = 0; i < value->size; ++i)");
    if (map) {
      free_value(key_type, "value->data[i].key");
      free_value(t, "value->data[i].value");
    } else {
      free_value(t, "value->data[i]");
    }
    close();
    line("free(value->data); memset(value, 0, sizeof(*value));");
    close();
    if (!map) {
      return;
    }
    const auto entry = n + "_entry";
    line("/* Sort decoded entries by key and original input order for last-complete-wins "
         "semantics. */");
    open("static inline int " + n + "_compare(const void* left, const void* right)");
    line("const " + entry + "* a = (const " + entry + "*)left; const " + entry + "* b = (const " +
         entry + "*)right;");
    line("const int order = " + compare(key_type, "a->key", "b->key") + ";");
    line("return order ? order : (a->srl_order > b->srl_order) - (a->srl_order < b->srl_order);");
    close();
    line("/* Sort borrowed entry pointers without mutating the source map. */");
    open("static inline int " + n + "_compare_pointer(const void* left, const void* right)");
    line("const " + entry + "* a = *(const " + entry + "* const*)left; const " + entry +
         "* b = *(const " + entry + "* const*)right;");
    line("const int order = " + compare(key_type, "a->key", "b->key") + ";");
    line("return order ? order : (a > b) - (a < b);");
    close();
    line("/* Compact duplicate keys in O(n log n), freeing superseded owned values. */");
    open("static inline void " + n + "_normalize(" + n + "* value)");
    line("if (value->size < 2) { return; }");
    line("qsort(value->data, value->size, sizeof(value->data[0]), " + n + "_compare);");
    line("size_t target = 0;");
    open("for (size_t first = 0; first < value->size;)");
    line("size_t last = first;");
    line("while (last+1 < value->size && (" +
         compare(key_type, "value->data[first].key", "value->data[last+1].key") +
         ") == 0) { ++last; }");
    open("for (size_t i = first; i < last; ++i)");
    free_value(key_type, "value->data[i].key");
    free_value(t, "value->data[i].value");
    close();
    line("if (target != last) { value->data[target] = value->data[last]; "
         "memset(&value->data[last], 0, sizeof(value->data[last])); }");
    line("++target; first = last+1;");
    close();
    line("value->size = target;");
    close();
  }
  // Decode fields into owned candidate storage with complete cleanup on error.
  void read_member(const class_node& owner, const member& value) {
    const auto dst = "value->" + field(value.name);
    const auto& t = value.type_name_list.front();
    if (value.modifier == member::modifier_type::none) {
      if (value.compact != compact_encoding::none) {
        line(dst + " = (" + type(t) + ")srl_read_compact_value(in, " +
             std::to_string(width(t)) + ", " +
             (value.compact == compact_encoding::prefix ? "true" : "false") + ");");
        return;
      }
      read_value(t, dst, false);
      return;
    }
    if (value.modifier == member::modifier_type::variant) {
      line(dst + "_index = alternative >= 0 ? (uint32_t)alternative : srl_read_compact(in);");
      open("switch (" + dst + "_index)");
      for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
        const auto& alt = value.type_name_list[i];
        open("case " + std::to_string(i) + ":");
        const auto target = dst + "_" + field(alt.enum_name);
        free_value(alt, target);
        init_value(alt, target, {}, "in->status");
        read_value(alt, target, true);
        line("break;");
        close();
      }
      line("default: in->status = srl_invalid; break;");
      close();
      return;
    }
    read_collection(dst, collection(owner, value), t,
                    value.modifier == member::modifier_type::map, map_key(value), value.fixed_extent);
  }
  // Decode any typed owning collection, including a nested generic array or map.
  void read_collection(const std::string& dst, const std::string& n, const type_name& t,
                       bool map, const type_name& key_type, std::size_t fixed_extent) {
    line(n + "_clear(&" + dst + ");");
    line("const size_t count = srl_read_array(in);");
    if (fixed_extent != 0) {
      line("if (in->protocol != srl_json && count != " + std::to_string(fixed_extent) +
           ") { in->status = srl_invalid; return; }");
    }
    line("const size_t capacity = srl_array_capacity(in, count, " +
         std::to_string(map ? 0 : width(t)) + ");");
    open("if (capacity && in->status == srl_ok)");
    line(dst + ".data = (" + (map ? n + "_entry" : type(t)) +
         "*)srl_grow(in, NULL, capacity, sizeof(" + dst + ".data[0]));");
    line("if (" + dst + ".data) { " + dst + ".capacity = capacity; }");
    close();
    open("for (size_t index = 0; srl_read_element(in, index, count); ++index)");
    if (fixed_extent != 0) {
      line("if (index >= " + std::to_string(fixed_extent) +
           ") { in->status = srl_invalid; break; }");
    }
    open("if (" + dst + ".size == " + dst + ".capacity)");
    line("const size_t next_capacity = " + dst + ".capacity ? " + dst + ".capacity + " + dst +
         ".capacity/2 + 1 : 16;");
    line("void* next = srl_grow(in, " + dst + ".data, next_capacity, sizeof(" + dst +
         ".data[0]));");
    line("if (!next) { break; }");
    line(dst + ".data = (" + (map ? n + "_entry" : type(t)) + "*)next; " + dst +
         ".capacity = next_capacity;");
    close();
    line("memset(&" + dst + ".data[index], 0, sizeof(" + dst + ".data[index])); ++" + dst +
         ".size;");
    const auto slot = dst + ".data[index]";
    if (!map) {
      init_value(t, slot, {}, "in->status");
      read_value(t, slot, true);
    } else {
      line(slot + ".srl_order = index;");
      init_value(key_type, slot + ".key", {}, "in->status");
      init_value(t, slot + ".value", {}, "in->status");
      open("if (in->protocol == srl_json)");
      line(
          "bool has_key = false, has_value = false; size_t entry_cursor = 0; srl_key entry = {0};");
      line("srl_read_begin(in);");
      open("while (srl_read_key(in, NULL, 0, entry_cursor++, &entry))");
      open("if (srl_string_equal(&entry.name, \"key\"))");
      free_value(key_type, slot + ".key");
      init_value(key_type, slot + ".key", {}, "in->status");
      read_value(key_type, slot + ".key", true);
      line("has_key = true;");
      close();
      open("else if (srl_string_equal(&entry.name, \"value\"))");
      free_value(t, slot + ".value");
      init_value(t, slot + ".value", {}, "in->status");
      read_value(t, slot + ".value", true);
      line("has_value = true;");
      close();
      line("else { in->status = srl_invalid; }");
      line("srl_string_free(&entry.name);");
      close();
      line("srl_read_end(in);");
      line("if (in->status == srl_ok && (!has_key || !has_value)) { in->status = srl_invalid; }");
      close();
      open("else");
      read_value(key_type, slot + ".key", true);
      read_value(t, slot + ".value", true);
      close();
    }
    close();
    if (fixed_extent != 0) {
      line("if (in->status == srl_ok && " + dst + ".size != " + std::to_string(fixed_extent) +
           ") { in->status = srl_invalid; }");
    }
    if (map) {
      line("if (in->status == srl_ok) { " + n + "_normalize(&" + dst + "); }");
    }
  }
  // Encode collections with deterministic map ordering and checked temporary allocation.
  // Subtract the canonical selected key and scalar when payload scope excludes the size field.
  std::string auto_size_payload_bytes(const member& value, bool input) const {
    std::string result{"("};
    for (const auto& format : value.auto_size_formats) {
      if (result.size() != 1) { result += " + "; }
      result += "(" + omitted_condition({format}, input ? "in->protocol" : "out->protocol", "srl_",
          protocol_name_style::lower_snake) + " ? " + std::to_string(auto_size::field_bytes(value, format)) + " : 0)";
    }
    return result + ")";
  }

  // Encode collections with deterministic map ordering and checked temporary allocation.
  void write_member(const class_node& owner, const member& value, bool first) {
    const auto src = "value->" + field(value.name);
    const auto& t = value.type_name_list.front();
    if (value.modifier == member::modifier_type::variant) {
      open("switch (" + src + "_index)");
      for (std::size_t i = 0; i < value.type_name_list.size(); ++i) {
        const auto& alt = value.type_name_list[i];
        open("case " + std::to_string(i) + ":");
        key(value.id, value.display_name + ":" + alt.enum_name, first);
        line("if (out->protocol == srl_binary_none || out->protocol == srl_binary_integer) { "
             "srl_write_compact(out, " +
             std::to_string(i) + "); }");
        write_value(alt, src + "_" + field(alt.enum_name), true);
        line("break;");
        close();
      }
      line("default: out->status = srl_invalid; break;");
      close();
      return;
    }
    key(value.id, value.display_name, first);
    if (value.modifier == member::modifier_type::none) {
      if (!value.auto_size_formats.empty()) {
        const auto active = omitted_condition(value.auto_size_formats, "out->protocol", "srl_",
                                               protocol_name_style::lower_snake);
        open("if (" + active + ")");
        line("srl_size_pos_" + std::to_string(value.id) + " = out->buffer.size;");
        line("srl_write_integer(out, 0, " + std::to_string(width(t)) + ", true);");
        close();
        if (!value.wire_only) {
          open("else");
          write_value(t, src, false);
          close();
        }
        return;
      }
      if (value.compact != compact_encoding::none) {
        line("srl_write_compact_value(out, (uint64_t)" + src + ", " +
             std::to_string(width(t)) + ", " +
             (value.compact == compact_encoding::prefix ? "true" : "false") + ", " +
             (value.compact_strict ? "true" : "false") + ");");
        return;
      }
      write_value(t, src, false);
      return;
    }
    write_collection(src, collection(owner, value), t,
                     value.modifier == member::modifier_type::map, map_key(value), value.fixed_extent);
  }
  // Encode nested or member collections with the same ordering and ownership contract.
  void write_collection(const std::string& src, const std::string& n, const type_name& t,
                        bool map, const type_name& key_type, std::size_t fixed_extent) {
    line("if (" + src + ".size && !" + src + ".data) { out->status = srl_invalid; return; }");
    if (!map) {
      if (fixed_extent != 0) {
        line("if (" + src + ".size != " + std::to_string(fixed_extent) +
             ") { out->status = srl_invalid; return; }");
      }
      line("srl_write_array(out, " + src + ".size);");
      open("for (size_t i = 0; out->status == srl_ok && i < " + src + ".size; ++i)");
      line("srl_write_element(out, i);");
      write_value(t, src + ".data[i]", true);
      close();
    } else {
      const auto entry = n + "_entry";
      if (key_type.name == "string") {
        line("for (size_t i = 0; i < " + src + ".size; ++i) { if (!srl_utf8((const uint8_t*)" +
             src + ".data[i].key.data, " + src +
             ".data[i].key.size)) { out->status = srl_invalid; return; } }");
      }
      line("if (" + src + ".size > SIZE_MAX/sizeof(const " + entry +
           "*)) { out->status = srl_allocation; return; }");
      line("const " + entry + "** entries = " + src + ".size ? (const " + entry + "**)malloc(" +
           src + ".size*sizeof(*entries)) : NULL;");
      line("if (" + src + ".size && !entries) { out->status = srl_allocation; return; }");
      line("for (size_t i = 0; i < " + src + ".size; ++i) { entries[i] = &" + src + ".data[i]; }");
      line("if (" + src + ".size > 1) { qsort(entries, " + src + ".size, sizeof(*entries), " + n +
           "_compare_pointer); }");
      line("size_t count = 0;");
      open("for (size_t i = 0; i < " + src + ".size; ++i)");
      line("if (i+1 == " + src + ".size || (" +
           compare(key_type, "entries[i]->key", "entries[i+1]->key") +
           ") != 0) { entries[count++] = entries[i]; }");
      close();
      line("srl_write_array(out, count);");
      open("for (size_t i = 0; out->status == srl_ok && i < count; ++i)");
      line("srl_write_element(out, i);");
      line("if (out->protocol == srl_json) { srl_write_begin(out); srl_write_field(out, 0, &" +
           fields_name + "[0], true); }");
      write_value(key_type, "entries[i]->key", true);
      line("if (out->protocol == srl_json) { srl_write_field(out, 0, &" + fields_name +
           "[1], false); }");
      write_value(t, "entries[i]->value", true);
      line("if (out->protocol == srl_json) { srl_write_end(out); }");
      close();
      line("free(entries);");
    }
    line("srl_write_array_end(out);");
  }
  // Define typed collection layouts before their owning class; map entries follow it.
  void declarations(const class_node& value) {
    const auto n = name(&value);
    if (value.magic_field) {
      const auto& magic = *value.magic_field;
      const auto constant =
          (value.magic_access == access_type::public_access ? "" : "srl_") + n + "_magic";
      line("/* Immutable scalar schema metadata without per-object storage. */");
      line("static const " + type(magic.type_name_list.front()) + " " + constant + " = " +
           scalar_default(magic.type_name_list.front(), literal(magic)) + ";");
    } else if (value.has_magic()) {
      std::string bytes;
      for (const auto byte : value.magic_bytes) {
        bytes += (bytes.empty() ? "" : ", ") +
                 std::to_string(static_cast<unsigned char>(byte));
      }
      const auto constant =
          (value.magic_access == access_type::public_access ? "" : "srl_") + n + "_magic";
      line("/* Immutable schema metadata; srl_ names are internal implementation details. */");
      line("static const uint8_t " + constant + "[" +
           std::to_string(value.magic_bytes.size()) + "] = {" + bytes + "};");
    }
    for (const auto& f : value.member_list) {
      if (f.modifier != member::modifier_type::array && f.modifier != member::modifier_type::map) {
        continue;
      }
      const auto col = collection(value, f);
      const auto t = f.modifier == member::modifier_type::map ? col + "_entry"
                                                              : type(f.type_name_list.front());
      if (f.modifier == member::modifier_type::map) {
        line("typedef struct " + t + " " + t + ";");
      }
      line("typedef struct " + col + " { " + t + "* data; size_t size; size_t capacity; } " + col +
           ";");
    }
    line("/* Owned model; initialize or zero before use, and release with " + n + "_free. */");
    open("struct " + n);
    bool any = false;
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      line(name(value.parents[i].parent_class) + " base" + std::to_string(i) + ";");
      any = true;
    }
    for (const auto& f : value.member_list) {
      const auto id = field(f.name);
      if (f.wire_only) { continue; }
      any = true;
      if (f.modifier == member::modifier_type::variant) {
        line("uint32_t " + id + "_index;");
        for (const auto& alt : f.type_name_list) {
          line(type(alt) + " " + id + "_" + field(alt.enum_name) + ";");
        }
      } else {
        line((f.modifier == member::modifier_type::none ? type(f.type_name_list.front())
                                                        : collection(value, f)) +
             " " + id + ";");
      }
    }
    if (!any) {
      line("uint8_t srl_empty;");
    }
    close(";");
  }
  // Complete member map entries after all inline class and generic value layouts exist.
  void map_declarations(const class_node& value) {
    for (const auto& f : value.member_list) {
      if (f.modifier != member::modifier_type::map) {
        continue;
      }
      const auto entry = collection(value, f) + "_entry";
      line("/* srl_order is decoder bookkeeping; callers may leave it zero. */");
      line("struct " + entry + " { " + type(map_key(f)) + " key; " +
           type(f.type_name_list.front()) + " value; size_t srl_order; };");
    }
  }
  // Collect each concrete built-in type, including types used only in behavior signatures.
  void collect_generic_types(const type_name& value) {
    for (const auto& argument : value.arguments) { collect_generic_types(argument); }
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      generic_types.try_emplace(type(value), value);
    }
  }
  // Emit pointer-based collection layouts before inline optional/class layouts are scheduled.
  void generic_collection_declarations() {
    for (const auto& [n, value] : generic_types) {
      line("typedef struct " + n + " " + n + ";");
      if (value.is_associative()) { line("typedef struct " + n + "_entry " + n + "_entry;"); }
    }
    for (const auto& [n, value] : generic_types) {
      if (value.is_optional()) { continue; }
      const auto item = value.is_associative() ? n + "_entry" : type(value.arguments.front());
      line("struct " + n + " { " + item + "* data; size_t size; size_t capacity; };");
      defined_generic_types.insert(n);
    }
  }
  // Complete inline dependencies while allowing cycles that cross owning sequence/map pointers.
  void define_type(const type_name& value) {
    if (value.type == object_type::class_type) {
      define_class(static_cast<const class_node&>(*value.resolved_node));
    } else if (value.is_optional() && !defined_generic_types.contains(type(value))) {
      define_type(value.arguments.front());
      line("/* Explicit presence; absent values own no initialized element. */");
      line("struct " + type(value) + " { bool has_value; " + type(value.arguments.front()) + " value; };");
      defined_generic_types.insert(type(value));
    }
  }
  // Order C's by-value layouts independently of declaration order; reject infinite inline storage.
  void define_class(const class_node& value) {
    if (defined_classes.contains(&value)) { return; }
    if (!defining_classes.insert(&value).second) {
      throw std::invalid_argument{"Direct self-containing C optional/model layout"};
    }
    for (const auto& parent : value.parents) { define_class(*parent.parent_class); }
    for (const auto& field_value : value.member_list) {
      if (field_value.modifier == member::modifier_type::none ||
          field_value.modifier == member::modifier_type::variant) {
        for (const auto& alternative : field_value.type_name_list) { define_type(alternative); }
      }
    }
    declarations(value);
    defining_classes.erase(&value);
    defined_classes.insert(&value);
  }
  // Complete map entry payloads after all class and optional definitions are available.
  void generic_map_declarations() {
    for (const auto& [n, value] : generic_types) {
      if (value.is_associative()) {
        line("struct " + n + "_entry { " + type(value.arguments.front()) + " key; " +
             type(value.arguments.back()) + " value; size_t srl_order; };");
      }
    }
  }
  // Isolate generic codec returns so nested maps always release their borrowed sorting arrays.
  void generic_codecs(const type_name& value) {
    const auto n = type(value);
    const auto dst = std::string{"(*value)"};
    line("/* Decode a generic value; the enclosing model owns transactional publication. */");
    open("static inline void " + n + "_srl_read(srl_reader* in, " + n + "* value, bool collection_context)");
    line("(void)collection_context;");
    if (value.is_optional()) {
      line("bool present = false;");
      line("if (!srl_read_optional_begin(in, &present)) { return; }");
      free_value(value, dst);
      line(dst + ".has_value = present;");
      open("if (present)");
      init_value(value.arguments.front(), dst + ".value", {}, "in->status");
      read_value(value.arguments.front(), dst + ".value", std::string{"collection_context"});
      close();
      line("srl_read_optional_end(in, present);");
    } else {
      line("if (!srl_read_container_enter(in)) { return; }");
      read_collection(dst, n, value.arguments.back(), value.is_associative(), value.arguments.front(), 0);
      line("srl_read_container_leave(in);");
    }
    close();
    line("/* Encode a borrowed generic value without changing its owning storage. */");
    open("static inline void " + n + "_srl_write(srl_writer* out, const " + n + "* value, bool collection_context)");
    line("(void)collection_context;");
    if (value.is_optional()) {
      line("if (!srl_write_optional_begin(out, " + dst + ".has_value)) { return; }");
      open("if (" + dst + ".has_value)");
      write_value(value.arguments.front(), dst + ".value", std::string{"collection_context"});
      close();
      line("srl_write_optional_end(out);");
    } else {
      line("if (!srl_write_container_enter(out)) { return; }");
      write_collection(dst, n, value.arguments.back(), value.is_associative(), value.arguments.front(), 0);
      line("srl_write_container_leave(out);");
    }
    close();
  }
  // A copied function table provides checked dispatch independently of a model's codec storage.
  void dispatch_adapter(const class_node& value) {
    const auto contracts = behavior::contracts(value);
    if (contracts.empty()) { return; }
    const auto model_name = name(&value);
    const auto table = model_name + "_behavior";
    const auto adapter = model_name + "_behavior_adapter";
    open("typedef struct " + table);
    for (const auto* method : contracts) {
      std::string parameters = std::string(method->effect == function_declaration::effect_type::readonly ? "const " : "") + model_name + "* receiver";
      for (const auto& argument : method->parameters) { parameters += ", " + type(argument.type) + " " + field(argument.name); }
      line((method->return_type.name == "void" ? "void" : type(method->return_type)) + " (*" + field(method->name) + ")(" + parameters + ");");
    }
    close(" " + table + ";");
    line("typedef struct " + adapter + " { " + model_name + "* receiver; " + table + " implementation; } " + adapter + ";");
    open("static inline srl_status " + adapter + "_init(" + adapter + "* adapter, " + model_name + "* receiver, const " + table + "* implementation)");
    std::string invalid = "!adapter || !receiver || !implementation";
    for (const auto* method : contracts) { invalid += " || !implementation->" + field(method->name); }
    line("if (" + invalid + ") return srl_invalid;");
    line("adapter->receiver=receiver; adapter->implementation=*implementation; return srl_ok;"); close();
    for (const auto* method : contracts) {
      const auto result_type = method->return_type.name == "void" ? "void" : type(method->return_type);
      const auto function = adapter + "_" + field(method->name);
      std::string parameters = "const " + adapter + "* adapter", calls = "adapter->receiver";
      for (const auto& argument : method->parameters) { parameters += ", " + type(argument.type) + " " + field(argument.name); calls += ", " + field(argument.name); }
      const auto result_parameter = result_type == "void" ? "" : ", " + result_type + "* result";
      open("static inline srl_status " + function + "_checked(" + parameters + result_parameter + ")");
      line("if (!adapter || !adapter->receiver || !adapter->implementation." + field(method->name) + (result_type == "void" ? "" : " || !result") + ") return srl_invalid;");
      line(std::string(result_type == "void" ? "" : "*result = ") + "adapter->implementation." + field(method->name) + "(" + calls + ");");
      line("return srl_ok;"); close();
    }
  }

  // Propagate checked callee status while leaving native calls under their declared C contract.
  std::string behavior_call(const behavior_expression& node, const std::vector<std::string>& arguments) {
    if (node.call_constructor) {
      const auto& owner = static_cast<const class_node&>(*node.call_target);
      if (!plain_record_result(owner)) {
        throw std::invalid_argument{"c: owning constructor expressions require records without dynamic owned storage"};
      }
      const auto local = "srl_behavior_construct_" + std::to_string(next_behavior_call++);
      line(type(*node.resolved_type) + " " + local + " = {0};");
      std::string callable = name(&owner) + "_init", inputs = "&" + local;
      if (node.call_method_index != std::numeric_limits<std::size_t>::max()) {
        const auto& constructor = owner.constructions.at(node.call_method_index);
        callable = name(&owner) + "_" + field("create");
        if (arguments.size() != constructor.parameters.size()) {
          callable += "_default_" + std::to_string(arguments.size());
        }
        for (std::size_t index{}; index < arguments.size(); ++index) {
          const auto& parameter = constructor.parameters[index];
          if (construction_owned(parameter.type)) {
            if (parameter.type.type != object_type::class_type ||
                !plain_record_result(static_cast<const class_node&>(*parameter.type.resolved_node))) {
              throw std::invalid_argument{"c: owning constructor expressions cannot move dynamically owned arguments"};
            }
            const auto copied = local + "_argument_" + std::to_string(index);
            line(type(parameter.type) + " " + copied + " = " + arguments[index] + ";");
            inputs += ", &" + copied;
          } else { inputs += ", " + arguments[index]; }
        }
      }
      line("srl_status " + local + "_status = " + callable + "(" + inputs + ");");
      line("if (" + local + "_status != srl_ok) { return " + local + "_status; }");
      return local;
    }
    const auto& method = behavior::called_function(node);
    if (method.dispatch == function_declaration::dispatch_type::abstract_method) {
      throw std::invalid_argument{"c: abstract readonly calls require a behavior adapter receiver"};
    }
    const bool free = node.call_target->type == object_type::function_type;
    const auto identifier = name(node.call_target) + (free ? "" : "_" + field(method.name));
    auto calls = behavior::call_arguments(arguments, node.call_receiver ? 1 : 0);
    if (!free) {
      std::string receiver = node.call_receiver ? arguments.at(0) : "(*value)";
      for (const auto base : node.call_base_path) { receiver = "(" + receiver + ").base" + std::to_string(base); }
      calls = "&(" + receiver + ")" + (calls.empty() ? "" : ", " + calls);
    }
    if (!method.expression) { return identifier + "(" + calls + ")"; }
    const auto local = "srl_behavior_call_" + std::to_string(next_behavior_call++);
    line(type(*node.resolved_type) + " " + local + " = {0};");
    line("srl_status " + local + "_status = " + identifier + "_checked(" + calls +
        (calls.empty() ? "" : ", ") + "&" + local + ");");
    line("if (" + local + "_status != srl_ok) { return " + local + "_status; }");
    return local;
  }

  // Declare checked and public functions before bodies so forward readonly calls are valid C11.
  void behavior_declarations(const syntax_node& node, const function_declaration& method) {
    const auto record_selection = [&](const auto& self, const behavior_expression& expression) -> bool {
      return (expression.kind == behavior_expression::operation::conditional &&
              expression.result_type.starts_with("record:")) ||
          std::any_of(expression.operands.begin(), expression.operands.end(), [&](const auto& operand) {
            return self(self, operand);
          });
    };
    if (method.expression && method.return_type.type == object_type::class_type &&
        record_selection(record_selection, *method.expression) &&
        !plain_record_result(static_cast<const class_node&>(*method.return_type.resolved_node))) {
      throw std::invalid_argument{"c: portable record-valued expressions require records without dynamic owned storage"};
    }
    const bool free = node.type == object_type::function_type;
    if (method.dispatch == function_declaration::dispatch_type::abstract_method) { return; }
    const auto identifier = name(&node) + (free ? "" : "_" + field(method.name));
    const auto result_type = method.return_type.name == "void" ? "void" : type(method.return_type);
    std::string parameters = free ? "" : std::string{method.effect == function_declaration::effect_type::readonly ? "const " : ""} + name(&node) + "*";
    for (const auto& parameter : method.parameters) {
      if (!parameters.empty()) { parameters += ", "; }
      parameters += type(parameter.type);
    }
    const bool defined = method.expression || behavior::native_body(method, "c");
    if (method.expression) {
      line("static inline srl_status " + identifier + "_checked(" + parameters +
          (parameters.empty() ? "" : ", ") + result_type + "*);");
    }
    line(std::string{defined ? "static inline " : ""} + result_type + " " + identifier +
        "(" + (parameters.empty() ? "void" : parameters) + ");");
  }

  // Emit standalone namespace functions and their checked status-returning entry points.
  void free_function(const function_node& node) {
    const auto& method = node.function;
    const auto identifier = name(&node), result_type = method.return_type.name == "void" ? "void" : type(method.return_type);
    std::string parameters, arguments;
    for (const auto& parameter : method.parameters) {
      if (!parameters.empty()) { parameters += ", "; arguments += ", "; }
      const auto n = (method.expression ? "srl_arg_" : "") + field(parameter.name);
      parameters += type(parameter.type) + " " + n; arguments += n;
    }
    line(behavior::source_marker(method, "c"));
    if (!method.expression) {
      if (const auto* native = behavior::native_body(method, "c")) {
        open("static inline " + result_type + " " + identifier + "(" + (parameters.empty() ? "void" : parameters) + ")");
        output += behavior::source_marker(*native, "c") + native->text + "\n"; close();
      }
      return;
    }
    open("static inline srl_status " + identifier + "_checked(" + parameters +
        (parameters.empty() ? "" : ", ") + result_type + "* result)");
    line("if (!result) { return srl_invalid; }");
    for (const auto& parameter : method.parameters) { line("(void)srl_arg_" + field(parameter.name) + ";"); }
    line("bool srl_behavior_ok = true; (void)srl_behavior_ok;");
    behavior::statement_context context;
    context.predicate = method.predicate_body; context.helpers = arithmetic_helpers;
    context.symbol = [&](auto value) { return "srl_arg_" + field(value); };
    context.native_type = [&](const auto& value) { return type(value); };
    context.enum_value = [&](const auto& value, auto constant) { return type(value) + "_" + field(constant); };
    context.member = [&](const behavior_expression& member, std::string base) {
      for (const auto index : member.member_base_path) { base = "(" + base + ").base" + std::to_string(index); }
      return "(" + base + ")." + field(member.value);
    };
    context.call = [&](const auto& call, const auto& args) { return behavior_call(call, args); };
    context.line = [&](const auto& text) { line(text); }; context.open = [&](const auto& text) { open(text); };
    context.close = [&]() { close(); };
    context.fail = [&]() { line(method.predicate_body ? "*result = false; return srl_ok;" : "return srl_range;"); };
    if (behavior::has_reduction(*method.expression)) { line("size_t srl_behavior_budget = " + std::to_string(behavior_expression::maximum_reduction_visits) + ";"); }
    behavior::statement_expression lower{context};
    const auto result = lower.emit(*method.expression);
    line("*result = " + result + "; return srl_ok;"); close();
    open("static inline " + result_type + " " + identifier + "(" + (parameters.empty() ? "void" : parameters) + ")");
    line(result_type + " result;");
    line("srl_status status = " + identifier + "_checked(" + arguments + (arguments.empty() ? "" : ", ") + "&result);");
    const auto invalid = method.return_type.type == object_type::class_type ? "(" + result_type + "){0}" :
        method.return_type.name == "bool" ? "false" :
        method.return_type.name == "float" || method.return_type.name == "double" ? "NAN" : "0";
    line("if (status != srl_ok) { errno = status == srl_invalid ? EINVAL : ERANGE; return " + std::string{invalid} + "; }");
    line("return result;"); close();
  }

  // Emit receiver functions and status-returning checked portable computations.
  void behaviors(const class_node& value) {
    const auto model_name = name(&value);
    if (std::any_of(value.native_blocks.begin(), value.native_blocks.end(),
        [](const auto& block) { return block.language == "c"; })) {
      throw std::invalid_argument{"c: native behavior declarations belong in preamble/free functions, not struct fields"};
    }
    behavior::require_unique_callable_names(value, "c", [&](auto n) { return field(n); });
    dispatch_adapter(value);
    for (const auto& method : value.functions) {
      line(behavior::source_marker(method, "c"));
      if (method.dispatch == function_declaration::dispatch_type::abstract_method) { continue; }
      const auto result_type = method.return_type.name == "void" ? "void" : type(method.return_type);
      const auto identifier = model_name + "_" + field(method.name);
      const auto receiver = std::string(method.effect == function_declaration::effect_type::readonly ? "const " : "") + model_name + "* value";
      std::string parameters = receiver, arguments = "value";
      for (const auto& argument : method.parameters) {
        const auto argument_name = (method.expression ? "srl_arg_" : "") + field(argument.name);
        parameters += ", " + type(argument.type) + " " + argument_name;
        arguments += ", " + argument_name;
      }
      const auto* native = behavior::native_body(method, "c");
      if (!native && !method.expression) {
        line(result_type + " " + identifier + "(" + parameters + ");");
        continue;
      }
      if (method.expression) {
        const auto symbol = [&](std::string_view name_value) {
          const auto argument = std::find_if(method.parameters.begin(), method.parameters.end(),
              [&](const auto& item) { return item.name == name_value; });
          return argument == method.parameters.end() ? "value->" + field(name_value) : "srl_arg_" + field(name_value);
        };
        open("static inline srl_status " + identifier + "_checked(" + parameters + ", " + result_type + "* result)");
        line("if (!value || !result) { return srl_invalid; }");
        for (const auto& argument : method.parameters) { line("(void)srl_arg_" + field(argument.name) + ";"); }
        line("bool srl_behavior_ok = true;");
        if (!method.predicate_body) {
          std::set<std::string> symbols; behavior::floating_symbols(*method.expression, symbols);
          for (const auto& name_value : symbols) {
            line("if (!isfinite((double)(" + symbol(name_value) + "))) { return srl_range; }");
          }
        }
        line("(void)srl_behavior_ok;");
        behavior::statement_context context;
        context.predicate = method.predicate_body; context.helpers = arithmetic_helpers; context.symbol = symbol;
        context.native_type = [&](const auto& value_type) { return type(value_type); };
        context.enum_value = [&](const auto& value_type, auto enumerator) { return type(value_type) + "_" + field(enumerator); };
        context.member = [&](const behavior_expression& node, std::string base) {
          for (const auto index : node.member_base_path) { base = "(" + base + ").base" + std::to_string(index); }
          return "(" + base + ")." + field(node.value);
        };
        context.call = [&](const auto& call, const auto& args) { return behavior_call(call, args); };
        context.line = [&](const auto& text) { line(text); };
        context.open = [&](const auto& text) { open(text); };
        context.close = [&]() { close(); };
        context.fail = [&]() { line(method.predicate_body ? "*result = false; return srl_ok;" : "return srl_range;"); };
        if (behavior::has_reduction(*method.expression)) {
          line("size_t srl_behavior_budget = " + std::to_string(behavior_expression::maximum_reduction_visits) + ";");
        }
        behavior::statement_expression lower{context};
        const auto candidate = lower.emit(*method.expression);
        line("*result = " + candidate + ";"); line("return srl_ok;"); close();
        open("static inline " + result_type + " " + identifier + "(" + parameters + ")");
        line(result_type + " result;");
        line("srl_status status = " + identifier + "_checked(" + arguments + ", &result);");
        const auto invalid = method.return_type.type == object_type::class_type ? "(" + result_type + "){0}" :
            method.return_type.name == "bool" ? "false" :
            method.return_type.name == "float" || method.return_type.name == "double" ? "NAN" : "0";
        line("if (status != srl_ok) { errno = status == srl_invalid ? EINVAL : ERANGE; return " + std::string{invalid} + "; }");
        line("return result;"); close();
      } else {
        open("static inline " + result_type + " " + identifier + "(" + parameters + ")");
        output += behavior::source_marker(*native, "c") + native->text + "\n"; close();
      }
    }
    std::vector<const function_declaration*> required;
    for (const auto& method : value.functions) {
      if (!method.expression && !behavior::native_body(method, "c") &&
          method.dispatch != function_declaration::dispatch_type::abstract_method) { required.push_back(&method); }
    }
    if (!required.empty()) {
      open("static inline void " + model_name + "_require_behavior_definitions(void)");
      for (std::size_t index{}; index < required.size(); ++index) {
        const auto& method = *required[index];
        std::string arguments = std::string(method.effect == function_declaration::effect_type::readonly ? "const " : "") + model_name + "*";
        for (const auto& argument : method.parameters) { arguments += ", " + type(argument.type); }
        line((method.return_type.name == "void" ? "void" : type(method.return_type)) +
            " (*volatile required" + std::to_string(index) + ")(" + arguments + ") = " +
            model_name + "_" + field(method.name) + ";");
        line("(void)required" + std::to_string(index) + ";");
      }
      close();
    }
  }

  // Prove a returned C record can be copied by value without aliasing dynamic owned storage.
  bool plain_record_result(const class_node& value) const {
    const auto plain_type = [&](const auto& self, const type_name& item) -> bool {
      if (item.is_optional()) { return self(self, item.arguments.front()); }
      if (item.name == "string" || item.is_sequence() || item.is_associative() || item.is_digest()) { return false; }
      if (item.type == object_type::class_type) {
        return plain_record_result(static_cast<const class_node&>(*item.resolved_node));
      }
      return item.type == object_type::primitive || item.type == object_type::enum_type;
    };
    return std::all_of(value.parents.begin(), value.parents.end(), [&](const auto& base) {
      return base.parent_class && plain_record_result(*base.parent_class);
    }) && std::all_of(value.member_list.begin(), value.member_list.end(), [&](const auto& item) {
      if (item.modifier == member::modifier_type::map ||
          (item.modifier == member::modifier_type::array && item.fixed_extent == 0)) { return false; }
      return std::all_of(item.type_name_list.begin(), item.type_name_list.end(), [&](const auto& type) {
        return plain_type(plain_type, type);
      });
    });
  }

  // Describe an owned parameter moved on success; scalar inputs remain passed by value.
  bool construction_owned(const type_name& value) const {
    return value.type == object_type::class_type || value.name == "string" || value.is_optional() ||
        value.is_sequence() || value.is_associative() || value.is_digest();
  }
  // Compose the portable constructor/factory C signature with explicit destination ownership.
  std::string construction_signature(const class_node& value, const construction_declaration& factory,
      std::optional<std::size_t> arity = {}) const {
    const auto n = name(&value);
    std::string result = "static inline srl_status " + n + "_" +
        field(factory.constructor ? "create" : factory.name) +
        (arity ? "_default_" + std::to_string(*arity) : "") + "(" + n + "* result";
    for (std::size_t index{}; index < arity.value_or(factory.parameters.size()); ++index) {
      const auto& argument = factory.parameters[index];
      std::string argument_type = type(argument.type);
      if (argument.modifier == member::modifier_type::array) {
        const auto assignment = std::find_if(factory.assignments.begin(), factory.assignments.end(),
            [&](const auto& item) { return item.parameter && item.value == argument.name; });
        if (assignment == factory.assignments.end()) {
          throw std::invalid_argument{"c: fixed-array construction input must bind an owned field"};
        }
        const auto bound = std::find_if(value.member_list.begin(), value.member_list.end(),
            [&](const auto& item) { return item.name == assignment->field; });
        argument_type = collection(value, *bound);
      }
      result += ", " + argument_type + (argument.modifier == member::modifier_type::array || construction_owned(argument.type) ? "* " : " ") +
          "srl_arg_" + field(argument.name);
    }
    return result + ")";
  }
  // Initialize or replace one checked owning default while preserving partial-allocation cleanup.
  void initialize_default(const value_initializer& value, const std::string& destination,
                          const std::string& status, bool initialized = false) {
    if (value.kind == value_initializer::kind_type::default_record) {
      if (initialized) { free_value(value.type, destination); }
      init_value(value.type, destination, {}, status);
      return;
    }
    if (value.kind == value_initializer::kind_type::fixed_array) {
      if (!initialized) {
        const auto count = std::to_string(value.elements.size());
        line("if (" + status + " == srl_ok) { " + destination + ".data = (" + type(value.type) +
            "*)calloc(" + count + ", sizeof(" + destination + ".data[0])); if (!" + destination +
            ".data) { " + status + " = srl_allocation; } else { " + destination + ".capacity = " + count + "; } }");
      }
      for (std::size_t index{}; index < value.elements.size(); ++index) {
        open("if (" + status + " == srl_ok)");
        if (!initialized) { line("++" + destination + ".size;"); }
        initialize_default(value.elements[index], destination + ".data[" + std::to_string(index) + "]", status, initialized);
        close();
      }
      return;
    }
    if (value.kind == value_initializer::kind_type::record) {
      if (!initialized) { init_value(value.type, destination, {}, status); }
      const auto& owner = static_cast<const class_node&>(*value.type.resolved_node);
      std::size_t index{};
      for (const auto& member : owner.member_list) {
        if (member.wire_only) { continue; }
        initialize_default(value.elements.at(index++), destination + "." + field(member.name), status, true);
      }
      return;
    }
    if (initialized) { free_value(value.type, destination); }
    member scalar{access_type::public_access, member::modifier_type::none, {value.type},
        "serializer_default", "serializer_default", 0, {}, value.value};
    scalar.magic = value.type.name == "char" && !value.value.empty() && value.value.front() != '\'';
    init_value(value.type, destination, literal(scalar), status);
  }

  // Publish a fresh constructed candidate only after allocation succeeds; owned inputs move once.
  void constructions(const class_node& value) {
    const auto n = name(&value);
    construction::validate_names(value, n, [&](auto identifier, auto) { return field(identifier); });
    construction::validate_extended_backend(value, "c");
    for (const auto& factory : value.constructions) {
      if (factory.guard && factory.guard->available == false) { continue; }
      line("/* Construct into initialized/zeroed result; move inputs must be disjoint from result and each other. Failure preserves both. */");
      open(construction_signature(value, factory));
      line("if (!result) { return srl_invalid; }");
      for (const auto& argument : factory.parameters) {
        if (argument.modifier == member::modifier_type::array || construction_owned(argument.type)) {
          line("if (!srl_arg_" + field(argument.name) + ") { return srl_invalid; }");
          if (argument.modifier == member::modifier_type::array) {
            const auto input = "srl_arg_" + field(argument.name);
            line("if (" + input + "->size != " + std::to_string(argument.fixed_extent) +
                " || !" + input + "->data || " + input + "->capacity < " + input + "->size) { return srl_invalid; }");
          }
        } else {
          line("(void)srl_arg_" + field(argument.name) + ";");
        }
      }
      line(n + " srl_candidate = {0}; srl_status status = " + n + "_init(&srl_candidate);");
      for (const auto& assignment : factory.assignments) {
        const auto found = std::find_if(value.member_list.begin(), value.member_list.end(),
            [&](const auto& item) { return item.name == assignment.field; });
        const auto& t = found->type_name_list.front();
        const auto dst = "srl_candidate." + field(found->name);
        if (assignment.array_binding) {
          for (std::size_t index = 0; index < assignment.elements.size(); ++index) {
            const auto& element = assignment.elements[index];
            const auto destination = dst + ".data[" + std::to_string(index) + "]";
            if (element.parameter) {
              if (!construction_owned(t)) {
                line("if (status == srl_ok) { " + destination + " = srl_arg_" + field(element.value) + "; }");
              }
            } else {
              open("if (status == srl_ok)");
              free_value(t, destination);
              auto literal_value = *found;
              literal_value.initializer.reset();
              literal_value.modifier = member::modifier_type::none;
              literal_value.default_value = element.value;
              init_value(t, destination, literal(literal_value), "status");
              close();
            }
          }
          continue;
        }
        if (assignment.parameter) {
          if (found->modifier != member::modifier_type::array && !construction_owned(t)) { line(dst + " = srl_arg_" + field(assignment.value) + ";"); }
        } else if (assignment.factory_default) {
          line("if (status == srl_ok) { status = " + type(t) + "_" + field(assignment.value) + "(&" + dst + "); }");
        } else {
          free_value(t, dst);
          auto literal_value = *found;
          literal_value.default_value = assignment.value;
          init_value(t, dst, literal(literal_value), "status");
        }
      }
      line("if (status != srl_ok) { " + n + "_free(&srl_candidate); return status; }");
      for (const auto& assignment : factory.assignments) {
        const auto found = std::find_if(value.member_list.begin(), value.member_list.end(),
            [&](const auto& item) { return item.name == assignment.field; });
        if (assignment.array_binding) {
          const auto& t = found->type_name_list.front();
          if (construction_owned(t)) {
            for (std::size_t index = 0; index < assignment.elements.size(); ++index) {
              const auto& element = assignment.elements[index];
              if (!element.parameter) { continue; }
              const auto destination = "srl_candidate." + field(found->name) + ".data[" + std::to_string(index) + "]";
              const auto argument = "srl_arg_" + field(element.value);
              free_value(t, destination);
              line(destination + " = *" + argument + "; memset(" + argument + ", 0, sizeof(*" + argument + "));");
            }
          }
          continue;
        }
        if (!assignment.parameter || (found->modifier != member::modifier_type::array &&
            !construction_owned(found->type_name_list.front()))) { continue; }
        const auto dst = "srl_candidate." + field(found->name);
        const auto argument = "srl_arg_" + field(assignment.value);
        if (found->modifier == member::modifier_type::array) {
          line(collection(value, *found) + "_clear(&" + dst + ");");
        } else { free_value(found->type_name_list.front(), dst); }
        line(dst + " = *" + argument + "; memset(" + argument + ", 0, sizeof(*" + argument + "));");
      }
      line(n + "_free(result); *result = srl_candidate; return srl_ok;");
      close();
      for (std::size_t arity{}; arity < factory.parameters.size(); ++arity) {
        if (factory.parameters[arity].default_value.empty() && factory.parameters[arity].default_factory.empty()) { continue; }
        line("/* Construct with fresh defaults; failure preserves destination and supplied owned inputs. */");
        open(construction_signature(value, factory, arity));
        line("if (!result) { return srl_invalid; }");
        line("srl_status status = srl_ok;");
        for (std::size_t index = arity; index < factory.parameters.size(); ++index) {
          const auto& parameter = factory.parameters[index];
          const auto argument = "srl_arg_" + field(parameter.name);
          std::string parameter_type = type(parameter.type);
          if (parameter.modifier == member::modifier_type::array) {
            const auto assignment = std::find_if(factory.assignments.begin(), factory.assignments.end(),
                [&](const auto& item) { return item.parameter && item.value == parameter.name; });
            const auto member = std::find_if(value.member_list.begin(), value.member_list.end(),
                [&](const auto& item) { return assignment != factory.assignments.end() && item.name == assignment->field; });
            if (member == value.member_list.end()) { throw std::invalid_argument{"c: default array input must bind an owned field"}; }
            parameter_type = collection(value, *member);
          }
          line(parameter_type + " " + argument + " = {0};");
          auto default_field = construction::parameter_default(parameter);
          if (parameter.initializer) { initialize_default(*parameter.initializer, argument, "status"); }
          else { init_value(parameter.type, argument, literal(default_field), "status"); }
          if (!parameter.default_factory.empty()) {
            line("if (status == srl_ok) { status = " + type(parameter.type) + "_" +
                field(parameter.default_factory) + "(&" + argument + "); }");
          }
        }
        std::string arguments{"result"};
        for (std::size_t index{}; index < factory.parameters.size(); ++index) {
          const auto& parameter = factory.parameters[index];
          arguments += ", " + std::string(index >= arity &&
              (construction_owned(parameter.type) || parameter.modifier == member::modifier_type::array) ? "&" : "") +
              "srl_arg_" + field(parameter.name);
        }
        line("if (status == srl_ok) { status = " + n + "_" + field(factory.constructor ? "create" : factory.name) +
            "(" + arguments + "); }");
        for (std::size_t index = arity; index < factory.parameters.size(); ++index) {
          const auto& parameter = factory.parameters[index];
          const auto argument = "srl_arg_" + field(parameter.name);
          if (parameter.modifier == member::modifier_type::array) {
            const auto assignment = std::find_if(factory.assignments.begin(), factory.assignments.end(),
                [&](const auto& item) { return item.parameter && item.value == parameter.name; });
            const auto member = std::find_if(value.member_list.begin(), value.member_list.end(),
                [&](const auto& item) { return assignment != factory.assignments.end() && item.name == assignment->field; });
            line(collection(value, *member) + "_clear(&" + argument + ");");
          } else { free_value(parameter.type, argument); }
        }
        line("return status;");
        close();
      }
    }
  }

  // Compare initialized borrowed values without allocation; null arguments are unequal/unordered.
  void comparisons(const class_node& value) {
    const auto* options = comparison::find(value, model.class_comparisons);
    if (!options) { return; }
    const auto selected = comparison::parts(value, *options);
    struct operand {
      std::string name;
      const syntax_node* nested{};
      bool text{};
      bool floating{};
    };
    std::vector<operand> operands;
    for (const auto* base : selected.bases) {
      const auto index = static_cast<std::size_t>(base - value.parents.data());
      operands.push_back({"base" + std::to_string(index), base->parent_class, false, false});
    }
    for (const auto* item : selected.fields) {
      const auto& type = item->type_name_list.front();
      const auto* nested = type.resolved_node && type.resolved_node->type == object_type::class_type ?
          type.resolved_node : nullptr;
      operands.push_back({field(item->name), nested, type.name == "string",
                         type.name == "float" || type.name == "double"});
    }
    const auto n = name(&value);
    if (options->equality) {
      line("/* Compare selected values without moving ownership; floating NaN is unequal. */");
      open("static inline bool " + n + "_equals_value(const " + n + "* value, const " + n + "* other)");
      line("if (!value || !other) { return false; }");
      std::string expression;
      for (const auto& item : operands) {
        const auto left = "value->" + item.name, right = "other->" + item.name;
        if (!expression.empty()) { expression += " && "; }
        expression += item.nested ? name(item.nested) + "_equals_value(&" + left + ", &" + right + ")" :
            item.text ? "srl_string_compare(&" + left + ", &" + right + ") == 0" :
                        left + " == " + right;
      }
      line("return " + (expression.empty() ? std::string{"true"} : expression) + ";");
      close();
    }
    if (options->ordering) {
      line("/* Borrow selected values lexically; null or NaN returns srl_unordered. */");
      open("static inline srl_comparison_result " + n + "_compare_value(const " + n + "* value, const " + n + "* other)");
      line("if (!value || !other) { return srl_unordered; }");
      for (std::size_t index{}; index < operands.size(); ++index) {
        const auto& item = operands[index];
        const auto left = "value->" + item.name, right = "other->" + item.name;
        const auto result = "srl_order_" + std::to_string(index);
        if (item.nested) {
          line("const srl_comparison_result " + result + " = " + name(item.nested) +
               "_compare_value(&" + left + ", &" + right + ");");
          line("if (" + result + " != srl_equal) { return " + result + "; }");
        } else if (item.text) {
          line("const int " + result + " = srl_string_compare(&" + left + ", &" + right + ");");
          line("if (" + result + " != 0) { return " + result + " < 0 ? srl_less : srl_greater; }");
        } else {
          if (item.floating) {
            line("if (isnan(" + left + ") || isnan(" + right + ")) { return srl_unordered; }");
          }
          line("if (" + left + " < " + right + ") { return srl_less; }");
          line("if (" + left + " > " + right + ") { return srl_greater; }");
        }
      }
      line("return srl_equal;");
      close();
    }
  }

  // Expose checked borrowed elements and transfer replacement ownership only on success.
  void array_access(const class_node& owner) {
    const auto n = name(&owner);
    for (const auto& item : owner.member_list) {
      if (!item.array_exposure) { continue; }
      const auto& exposure = *item.array_exposure;
      const auto storage = "value->" + field(item.name);
      const auto& element = item.modifier == member::modifier_type::array ?
          item.type_name_list.front() : item.type_name_list.front().arguments.front();
      const auto scalar = type(element);
      std::vector<std::uint64_t> dimensions;
      for (const auto& dimension : exposure.dimensions) {
        if (dimension.kind != dimension_expression::operation::literal) {
          throw std::invalid_argument{"c: array exposure dimensions must be resolved"};
        }
        dimensions.push_back(dimension.value);
      }
      const auto rank = dimensions.empty() ? std::size_t{1} : dimensions.size();
      std::string parameters;
      std::string offset;
      for (std::size_t index = 0; index < rank; ++index) {
        const auto coordinate = "index" + std::to_string(index);
        parameters += ", size_t " + coordinate;
        offset = offset.empty() ? coordinate :
            "(" + offset + " * " + std::to_string(dimensions[index]) + " + " + coordinate + ")";
      }
      line("/* Validate logical storage length; failure preserves the result. */");
      open("static inline srl_status " + n + "_size(const " + n + "* value, size_t* result)");
      line("if (!value || !result) { return srl_invalid; }");
      line("if (" + storage + ".size > " + storage + ".capacity || (" + storage +
          ".size && !" + storage + ".data)) { return srl_invalid; }");
      if (item.fixed_extent != 0) {
        line("if (" + storage + ".size != " + std::to_string(item.fixed_extent) + ") { return srl_invalid; }");
      }
      line("*result = " + storage + ".size; return srl_ok;");
      close();
      const auto bounds = [&]() {
        line("size_t count = 0;");
        line("if (" + n + "_size(value, &count) != srl_ok) { return srl_invalid; }");
        for (std::size_t index = 0; index < rank; ++index) {
          const auto extent = dimensions.empty() ? "count" : std::to_string(dimensions[index]);
          line("if (index" + std::to_string(index) + " >= " + extent + ") { return srl_invalid; }");
        }
      };
      line("/* Borrow one const element; owner destruction or storage relocation invalidates it. */");
      open("static inline srl_status " + n + "_get(const " + n + "* value" + parameters + ", const " + scalar + "** result)");
      line("if (!result) { return srl_invalid; }");
      bounds();
      line("*result = &" + storage + ".data[" + offset + "]; return srl_ok;");
      close();
      if (!exposure.readonly) {
        const bool owned = construction_owned(element) || element.name.starts_with("version");
        line(owned ? "/* Move a valid owned element disjoint from the destination; bounds failure preserves both operands. */" :
            "/* Replace one scalar element after storage and coordinate validation. */");
        open("static inline srl_status " + n + "_set(" + n + "* value" + parameters + ", " + scalar + (owned ? "* input" : " input") + ")");
        if (owned) { line("if (!input) { return srl_invalid; }"); }
        bounds();
        const auto destination = storage + ".data[" + offset + "]";
        if (owned) {
          line("if (input == &" + destination + ") { return srl_ok; }");
          free_value(element, destination);
          line(destination + " = *input; memset(input, 0, sizeof(*input));");
        } else { line(destination + " = input;"); }
        line("return srl_ok;");
        close();
      }
      line("/* Borrow contiguous iteration storage and its count; do not retain it across relocation or destruction. */");
      open("static inline srl_status " + n + "_values(const " + n + "* value, const " + scalar + "** result, size_t* count)");
      line("if (!result || !count) { return srl_invalid; }");
      line("size_t length = 0;");
      line("if (" + n + "_size(value, &length) != srl_ok) { return srl_invalid; }");
      line("*result = " + storage + ".data; *count = length; return srl_ok;");
      close();
    }
  }

  // Emit lifecycle APIs and direct per-field codecs for one C model.
  void object(const class_node& value) {
    const auto n = name(&value);
    behaviors(value);
    for (const auto& f : value.member_list) {
      if (f.modifier == member::modifier_type::array || f.modifier == member::modifier_type::map) {
        collection_helpers(collection(value, f), f.type_name_list.front(),
                           f.modifier == member::modifier_type::map, map_key(f));
      }
    }
    line("/* Release every owned field; accepts zeroed and partially initialized values. */");
    open("static inline void " + n + "_free(" + n + "* value)");
    line("if (!value) { return; }");
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      line(name(value.parents[i].parent_class) + "_free(&value->base" + std::to_string(i) + ");");
    }
    for (const auto& f : value.member_list) {
      const auto dst = "value->" + field(f.name);
      if (f.wire_only) { continue; }
      if (f.modifier == member::modifier_type::variant) {
        for (const auto& alt : f.type_name_list) {
          free_value(alt, dst + "_" + field(alt.enum_name));
        }
      } else if (f.modifier == member::modifier_type::none) {
        free_value(f.type_name_list.front(), dst);
      } else {
        line(collection(value, f) + "_clear(&" + dst + ");");
      }
    }
    line("memset(value, 0, sizeof(*value));");
    close();
    line("/* Initialize fresh storage with schema defaults; free an existing value before calling. "
         "*/");
    open("static inline srl_status " + n + "_init(" + n + "* value)");
    line("if (!value) { return srl_invalid; } memset(value, 0, sizeof(*value)); srl_status status "
         "= srl_ok;");
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      line("if (status == srl_ok) { status = " + name(value.parents[i].parent_class) +
           "_init(&value->base" + std::to_string(i) + "); }");
    }
    for (const auto& f : value.member_list) {
      const auto dst = "value->" + field(f.name);
      if (f.wire_only) { continue; }
      if (f.modifier == member::modifier_type::variant) {
        for (const auto& alt : f.type_name_list) {
          init_value(alt, dst + "_" + field(alt.enum_name), {}, "status");
        }
      } else if (f.modifier == member::modifier_type::none) {
        if (f.initializer) { initialize_default(*f.initializer, dst, "status"); }
        else { init_value(f.type_name_list.front(), dst, literal(f), "status"); }
        if (!f.default_factory.empty()) {
          line("if (status == srl_ok) { status = " + type(f.type_name_list.front()) + "_" +
              field(f.default_factory) + "(&" + dst + "); }");
        }
      } else if (f.fixed_extent != 0) {
        if (f.initializer) {
          initialize_default(*f.initializer, dst, "status");
          continue;
        }
        const auto count = std::to_string(f.fixed_extent);
        line("if (status == srl_ok) { " + dst + ".data = (" + type(f.type_name_list.front()) +
             "*)calloc(" + count + ", sizeof(" + dst + ".data[0])); if (!" + dst +
             ".data) { status = srl_allocation; } else { " + dst + ".capacity = " + count +
             "; } }");
        open("for (size_t i = 0; status == srl_ok && i < " + count + "; ++i)");
        line("++" + dst + ".size;");
        init_value(f.type_name_list.front(), dst + ".data[i]", {}, "status");
        close();
      }
    }
    line("if (status != srl_ok) { " + n + "_free(value); } return status;");
    close();
    constructions(value);
    output += conversion::methods(value, n, "c",
        [&](const auto& item) { return type(item); },
        [&](auto item, auto) { return field(item); });
    comparisons(value);
    array_access(value);
    const auto magic_name = (value.magic_access == access_type::public_access ? "" : "srl_") +
                            n + "_magic";
    line("/* Encode in schema order; the source retains ownership of all its fields. */");
    open("static inline void " + n + "_srl_write(srl_writer* out, const " + n + "* value)");
    line("(void)value; if (out->status != srl_ok) { return; }");
    if (model.protocols == output_protocols::binary_none) {
      line("if (out->protocol != srl_binary_none) { out->status = srl_invalid; return; }");
    }
    if (!auto_size::fields(value).empty()) {
      line("const size_t srl_size_start = out->buffer.size;");
      for (const auto* item : auto_size::fields(value)) {
        line("size_t srl_size_pos_" + std::to_string(item->id) + " = 0;");
      }
    }
    if (value.version_member()) {
      check_version(value, false);
    }
    if (value.has_magic()) {
      const auto omitted = omitted_condition(value.magic_omitted_formats, "out->protocol",
                                              "srl_", protocol_name_style::lower_snake);
      open("if (out->protocol != srl_json" +
           (omitted.empty() ? "" : " && !(" + omitted + ")") + ")");
      if (value.magic_field) {
        write_value(value.magic_field->type_name_list.front(), magic_name, false);
      } else {
        for (const auto byte : value.magic_bytes) {
          line("srl_write_integer(out, " + std::to_string(static_cast<unsigned char>(byte)) +
               ", 1, true);");
        }
      }
      close();
    }
    line("srl_write_begin(out); if (out->status != srl_ok) { return; }");
    bool first = true;
    variable_first = std::any_of(value.member_list.begin(), value.member_list.end(),
                                 [](const auto& item) { return item.omits("json"); });
    if (variable_first) { line("bool first_field = true;"); }
    const bool magic_json = value.has_magic() && !value.magic_omits("json");
    if (magic_json) {
      open("if (out->protocol == srl_json)");
      key(value.magic_id, "magic", true);
      if (value.magic_field) {
        write_value(value.magic_field->type_name_list.front(), magic_name, false);
      } else {
        line("const srl_string text = {(char*)" + magic_name + ", sizeof(" + magic_name + ")};");
        line("srl_write_string(out, &text);");
      }
      if (variable_first) { line("first_field = false;"); }
      close();
      first = false;
    }
    if (const auto* version = value.version_member()) {
      write_member(value, *version, first);
      if (variable_first) { line("first_field = false;"); }
      first = false;
    }
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      key(base.id, base.display_name, first);
      line(name(base.parent_class) + "_srl_write(out, &value->base" + std::to_string(i) + ");");
      if (variable_first) { line("first_field = false;"); }
      first = false;
    }
    for (const auto& f : wire_members(value)) {
      if (f.version) {
        continue;
      }
      const auto* version = value.version_member();
      const auto active = version ? version_active(version_language::c, f, *version,
                                                   "value->" + field(version->name))
                                  : std::string{};
      if (!active.empty()) {
        open("if (" + active + ")");
      }
      const auto omitted = omitted_condition(f.omitted_formats, "out->protocol", "srl_",
                                              protocol_name_style::lower_snake);
      if (!omitted.empty()) { open("if (!(" + omitted + "))"); }
      open("");
      write_member(value, f, first);
      close();
      if (variable_first) { line("first_field = false;"); }
      if (!omitted.empty()) { close(); }
      if (!active.empty()) {
        close();
      }
      first = false;
    }
    variable_first = false;
    line("srl_write_end(out);");
    for (const auto* item : auto_size::fields(value)) {
      const auto active = omitted_condition(item->auto_size_formats, "out->protocol", "srl_",
                                             protocol_name_style::lower_snake);
      const auto bytes = std::to_string(width(item->type_name_list.front()));
      open("if (out->status == srl_ok && (" + active + "))");
      line("const size_t srl_size_bytes = out->buffer.size - srl_size_start" +
           (item->size_scope == auto_size_scope::payload ? " - " + auto_size_payload_bytes(*item, false) : "") + ";");
      if (item->type_name_list.front().name == "uint32") {
        line("if (srl_size_bytes > UINT32_MAX) { out->status = srl_range; return; }");
      }
      open("for (size_t srl_size_byte = 0; srl_size_byte < " + bytes + "; ++srl_size_byte)");
      line("out->buffer.data[srl_size_pos_" + std::to_string(item->id) +
           " + srl_size_byte] = (uint8_t)((uint64_t)srl_size_bytes >> (srl_size_byte * CHAR_BIT));");
      close();
      close();
    }
    close();
    line("/* Decode into a private candidate; public decoding commits only complete messages. */");
    open("static inline void " + n + "_srl_read(srl_reader* in, " + n + "* value)");
    line("(void)value; if (in->status != srl_ok) { return; }");
    if (model.protocols == output_protocols::binary_none) {
      line("if (in->protocol != srl_binary_none) { in->status = srl_invalid; return; }");
    }
    if (!auto_size::fields(value).empty()) {
      line("const size_t srl_size_start = in->offset; (void)srl_size_start;");
      for (const auto* item : auto_size::fields(value)) {
        line("bool srl_size_seen_" + std::to_string(item->id) + " = false;");
        if (item->size_scope == auto_size_scope::payload) {
          line("size_t srl_size_field_bytes_" + std::to_string(item->id) + " = 0; (void)srl_size_field_bytes_" + std::to_string(item->id) + ";");
        }
        if (item->wire_only) {
          line("uint64_t srl_size_value_" + std::to_string(item->id) + " = 0; (void)srl_size_value_" + std::to_string(item->id) + ";");
        }
      }
    }
    if (value.has_magic()) {
      const auto omitted = omitted_condition(value.magic_omitted_formats, "in->protocol",
                                              "srl_", protocol_name_style::lower_snake);
      open("if (in->protocol != srl_json" +
           (omitted.empty() ? "" : " && !(" + omitted + ")") + ")");
      if (value.magic_field) {
        const auto& magic_type = value.magic_field->type_name_list.front();
        line(type(magic_type) + " decoded_magic = {0};");
        read_value(magic_type, "decoded_magic", false);
        line("if (decoded_magic != " + magic_name + " || in->status != srl_ok) { "
             "if (in->status == srl_ok) { in->status = srl_invalid; } return; }");
      } else {
        for (const auto byte : value.magic_bytes) {
          line("if (srl_read_integer(in, 1, true) != " +
               std::to_string(static_cast<unsigned char>(byte)) +
               " || in->status != srl_ok) { if (in->status == srl_ok) { in->status = "
               "srl_invalid; } return; }");
        }
      }
      close();
    }
    if (magic_json) { line("bool magic_seen = false;"); }
    line("srl_read_begin(in);");
    if (value.version_member()) {
      line("bool seen[" + std::to_string(value.parents.size() + value.member_list.size()) +
           "] = {false};");
    }
    std::string ids = "static const uint32_t ids[] = {";
    if (const auto* version = value.version_member()) {
      ids += std::to_string(version->id) + ",";
    }
    for (const auto& base : value.parents) {
      ids += std::to_string(base.id) + ",";
    }
    for (const auto& f : wire_members(value)) {
      if (!f.version && !f.omits("binary_none")) {
        ids += std::to_string(f.id) + ",";
      }
    }
    ids += "0};";
    line(ids);
    line("size_t cursor = 0; srl_key key = {0};");
    if (auto_size::fields(value).empty()) {
      open("while (srl_read_key(in, ids, sizeof(ids)/sizeof(ids[0])-1, cursor++, &key))");
    } else {
      open("while (true)");
      line("const size_t srl_size_field_start = in->offset; (void)srl_size_field_start;");
      line("if (!srl_read_key(in, ids, sizeof(ids)/sizeof(ids[0])-1, cursor++, &key)) { break; }");
    }
    line("uint32_t id = key.id; int alternative = -1;");
    if (model.protocols == output_protocols::all) {
      open("if (key.named)");
      bool branch_first = true;
      const auto branch = [&](const std::string& text, std::uint32_t id, int alt) {
        line(std::string{branch_first ? "if" : "else if"} + " (srl_string_equal(&key.name, " +
             quote(text) + ")) { id = " + std::to_string(id) +
             "; alternative = " + std::to_string(alt) + "; }");
        branch_first = false;
      };
      if (value.has_magic()) { branch("magic", value.magic_id, -1); }
      for (const auto& base : value.parents) {
        branch(base.display_name, base.id, -1);
      }
      for (const auto& f : wire_members(value)) {
        if (f.modifier == member::modifier_type::variant) {
          for (std::size_t i = 0; i < f.type_name_list.size(); ++i) {
            branch(f.display_name + ":" + f.type_name_list[i].enum_name, f.id, static_cast<int>(i));
          }
        } else {
          branch(f.display_name, f.id, -1);
        }
      }
      line(std::string{branch_first ? "" : "else "} +
           "{ srl_string_free(&key.name); srl_skip_value(in); continue; }");
      close();
    }
    line("srl_string_free(&key.name); (void)alternative; if (in->status != srl_ok) { break; }");
    open("switch (id)");
    if (magic_json) {
      open("case " + std::to_string(value.magic_id) + ":");
      line("if (in->protocol != srl_json || magic_seen) { in->status = srl_invalid; break; }");
      if (value.magic_field) {
        const auto& magic_type = value.magic_field->type_name_list.front();
        line(type(magic_type) + " decoded_magic = {0};");
        read_value(magic_type, "decoded_magic", false);
        line("if (in->status == srl_ok && decoded_magic != " + magic_name +
             ") { in->status = srl_invalid; }");
      } else {
        line("srl_string text = {0}; srl_read_string(in, &text);");
        line("if (in->status == srl_ok && (text.size != sizeof(" + magic_name +
             ") || memcmp(text.data, " + magic_name + ", sizeof(" + magic_name +
             ")))) { in->status = srl_invalid; }");
        line("srl_string_free(&text);");
      }
      line("magic_seen = true; break;");
      close();
    }
    if (value.has_magic() && !magic_json) {
      line("case " + std::to_string(value.magic_id) + ": in->status = srl_invalid; break;");
    }
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      line("case " + std::to_string(value.parents[i].id) + ": " +
           name(value.parents[i].parent_class) + "_srl_read(in, &value->base" + std::to_string(i) +
           "); break;");
    }
    for (const auto& f : wire_members(value)) {
      open("case " + std::to_string(f.id) + ":");
      if (!f.auto_size_formats.empty()) {
        const auto active = omitted_condition(f.auto_size_formats, "in->protocol", "srl_",
                                               protocol_name_style::lower_snake);
        open("if (" + active + ")");
        line("if (srl_size_seen_" + std::to_string(f.id) + ") { in->status = srl_invalid; break; }");
        line("srl_size_seen_" + std::to_string(f.id) + " = true;");
        close();
      }
      if (f.wire_only) {
        const auto active = omitted_condition(f.auto_size_formats, "in->protocol", "srl_",
                                               protocol_name_style::lower_snake);
        line("if (!(" + active + ")) { in->status = srl_invalid; break; }");
        line("srl_size_value_" + std::to_string(f.id) + " = srl_read_integer(in, " +
             std::to_string(width(f.type_name_list.front())) + ", true);");
      } else {
        read_versioned_member(value, f);
      }
      if (f.size_scope == auto_size_scope::payload) {
        line("srl_size_field_bytes_" + std::to_string(f.id) + " = in->offset - srl_size_field_start;");
      }
      line("break;");
      close();
    }
    line("default: in->status = srl_invalid; break;");
    close();
    close();
    line("srl_read_end(in);");
    for (const auto* item : auto_size::fields(value)) {
      const auto active = omitted_condition(item->auto_size_formats, "in->protocol", "srl_",
                                             protocol_name_style::lower_snake);
      open("if (in->status == srl_ok && (" + active + "))");
      line("if (!srl_size_seen_" + std::to_string(item->id) +
           ") { in->status = srl_invalid; return; }");
      if (auto_size::verifies(value, model.class_auto_sizes)) {
        line("if (" + (item->wire_only ? "srl_size_value_" + std::to_string(item->id) : "value->" + field(item->name)) +
             " != in->offset - srl_size_start" + (item->size_scope == auto_size_scope::payload ? " - srl_size_field_bytes_" + std::to_string(item->id) : "") +
             ") { in->status = srl_invalid; return; }");
      }
      close();
    }
    if (magic_json) {
      line("if (in->status == srl_ok && in->protocol == srl_json && !magic_seen) "
           "{ in->status = srl_invalid; return; }");
    }
    if (const auto* version = value.version_member()) {
      line("if (in->status != srl_ok) { return; }");
      line("if (!seen[" + std::to_string(version_slot(value, *version)) +
           "]) { in->status = srl_invalid; return; }");
      check_version(value, true);
      for (const auto& item : wire_members(value)) {
        const auto active =
            version_active(version_language::c, item, *version, "value->" + field(version->name));
        if (!active.empty()) {
          line("if (in->protocol != srl_binary_none && seen[" +
               std::to_string(version_slot(value, item)) + "] && !(" + active +
               ")) { in->status = srl_invalid; return; }");
        }
      }
    }
    close();
    line("/* Replace initialized output bytes on success only; the caller owns the returned "
         "buffer. */");
    open("static inline srl_status " + n + "_encode(const " + n +
         "* value, srl_protocol protocol, srl_buffer* result)");
    line("if (!value || !result || protocol < srl_json || protocol > srl_binary_string) { return "
         "srl_invalid; }");
    line("srl_writer out = {0}; out.protocol = protocol; " + n + "_srl_write(&out, value);");
    line("if (out.status != srl_ok) { srl_buffer_free(&out.buffer); return out.status; } "
         "srl_buffer_free(result); *result = out.buffer; return srl_ok;");
    close();
    line(
        "/* Decode exactly into initialized or zeroed result; failure leaves result unchanged. */");
    open("static inline srl_status " + n + "_decode(" + n +
         "* result, const uint8_t* bytes, size_t size, srl_protocol protocol, const srl_limits* "
         "limits)");
    line("if (!result || (size && !bytes) || protocol < srl_json || protocol > srl_binary_string) "
         "{ return srl_invalid; }");
    line("srl_reader in = {0}; in.data = bytes; in.size = size; in.protocol = protocol; in.limits "
         "= limits ? *limits : srl_default_limits();");
    line("if (size > in.limits.max_bytes) { return srl_limit; } " + n +
         " candidate = {0}; in.status = " + n + "_init(&candidate);");
    line(n + "_srl_read(&in, &candidate); srl_finish(&in);");
    line("if (in.status != srl_ok) { " + n + "_free(&candidate); return in.status; } " + n +
         "_free(result); *result = candidate; return srl_ok;");
    close();
  }

public:
  // Bind schema metadata and a private key table name for one generated header.
  explicit c_emitter(const schema& value)
      : model{value},
        fields_name{"srl_" +
                    (value.nodes.empty() ? std::string{"empty"}
                                         : snake(value.names.at(value.nodes.front()))) +
                    "_fields"} {
    output += runtime;
    output += "\n#endif /* SRL_NATIVE_RUNTIME_INCLUDED */\n";
    if (std::any_of(model.class_comparisons.begin(), model.class_comparisons.end(),
                    [](const auto& item) { return item.second.ordering; })) {
      output += "\n#ifndef SRL_NATIVE_COMPARISON_INCLUDED\n#define SRL_NATIVE_COMPARISON_INCLUDED\n"
          "/* Partial ordering preserves floating unordered values and exact scalar comparisons. */\n"
          "typedef enum srl_comparison_result { srl_less = -1, srl_equal = 0, srl_greater = 1, "
          "srl_unordered = 2 } srl_comparison_result;\n#endif\n";
    }
    arithmetic_helpers = behavior::select_helpers(model.nodes,
        [&](const auto& node) { return name(&node); },
        [&](const auto name) { return field(name); });
    output += behavior::helpers("c", arithmetic_helpers);
  }
  // Emit immutable key tables, public owning layouts, and checked direct codecs.
  std::string generate() {
    for (const auto& code : model.preambles) {
      if (code.language == "c") { output += behavior::source_marker(code, "c") + code.text + "\n"; }
    }
    for (const auto* node : model.nodes) {
      if (node->type != object_type::class_type) { continue; }
      const auto& object_value = static_cast<const class_node&>(*node);
      construction::validate_extended_backend(object_value, "c");
      for (const auto& field_value : object_value.member_list) {
        for (const auto& alternative : field_value.type_name_list) { collect_generic_types(alternative); }
      }
      for (const auto& method : object_value.functions) {
        collect_generic_types(method.return_type);
        for (const auto& argument : method.parameters) { collect_generic_types(argument.type); }
      }
      for (const auto& factory : object_value.constructions) {
        for (const auto& argument : factory.parameters) { collect_generic_types(argument.type); }
      }
    }
    validate_symbols();
    for (std::size_t i = 0; i < model.ordered_keys.size(); ++i) {
      const auto& key = model.ordered_keys[i];
      std::string encoded;
      const auto size = key.size();
      const auto extra = size <= 63 ? 0 : size <= 16383 ? 1 : size <= 4194303 ? 2 : 3;
      encoded += static_cast<char>((extra << 6) | (size >> (extra * 8)));
      for (int j = extra - 1; j >= 0; --j) {
        encoded += static_cast<char>(size >> (j * 8));
      }
      encoded += key;
      for (const bool json : {true, false}) {
        const auto bytes = json ? quote(key) + ":" : encoded;
        std::string text = "static const uint8_t " + fields_name + "_" + std::to_string(i) +
                           (json ? "_json[] = {" : "_binary[] = {");
        for (const auto ch : bytes) {
          text += std::to_string(static_cast<unsigned char>(ch)) + ",";
        }
        line(text + "};");
      }
    }
    line("static const srl_field " + fields_name + "[] = {");
    ++indent;
    for (std::size_t i = 0; i < model.ordered_keys.size(); ++i) {
      const auto n = fields_name + "_" + std::to_string(i);
      line("{" + n + "_json, sizeof(" + n + "_json), " + n + "_binary, sizeof(" + n + "_binary)},");
    }
    --indent;
    line("};");
    for (const auto* node : model.nodes) {
      validate_identifier(name(node), keywords);
      if (node->type == object_type::class_type) {
        line("typedef struct " + name(node) + " " + name(node) + ";");
      } else if (node->type == object_type::enum_type) {
        const auto& e = static_cast<const enum_node&>(*node);
        open("typedef enum " + name(node));
        for (std::size_t i = 0; i < e.enum_name_list.size(); ++i) {
          line(name(node) + "_" + field(e.enum_name_list[i]) + " = " + std::to_string(i) + ",");
        }
        close(" " + name(node) + ";");
        std::string names = "static const char* const " + name(node) + "_srl_names[] = {";
        for (const auto& item : e.enum_name_list) {
          names += quote(item) + ",";
        }
        line(names + "};");
      }
    }
    generic_collection_declarations();
    for (const auto* node : model.nodes) {
      if (node->type == object_type::class_type) { define_class(static_cast<const class_node&>(*node)); }
    }
    for (const auto& [n, value] : generic_types) { (void)n; define_type(value); }
    generic_map_declarations();
    for (const auto* node : model.nodes) {
      if (node->type == object_type::function_type) {
        behavior_declarations(*node, static_cast<const function_node&>(*node).function);
      } else if (node->type == object_type::class_type) {
        for (const auto& method : static_cast<const class_node&>(*node).functions) { behavior_declarations(*node, method); }
      }
    }
    for (const auto* node : model.nodes) {
      if (node->type == object_type::function_type) { free_function(static_cast<const function_node&>(*node)); }
    }
    for (const auto* node : model.nodes) {
      if (node->type == object_type::class_type) { map_declarations(static_cast<const class_node&>(*node)); }
    }
    for (const auto* node : model.nodes) {
      if (node->type != object_type::class_type) {
        continue;
      }
      const auto n = name(node);
      line("/* Lifecycle and internal codec declarations allow recursive collection fields. */");
      line("static inline srl_status " + n + "_init(" + n + "* value);");
      line("static inline void " + n + "_free(" + n + "* value);");
      line("static inline void " + n + "_srl_read(srl_reader* in, " + n + "* value);");
      line("static inline void " + n + "_srl_write(srl_writer* out, const " + n + "* value);");
      if (const auto* options = comparison::find(static_cast<const class_node&>(*node),
                                                model.class_comparisons)) {
        if (options->equality) {
          line("static inline bool " + n + "_equals_value(const " + n + "* value, const " + n + "* other);");
        }
        if (options->ordering) {
          line("static inline srl_comparison_result " + n + "_compare_value(const " + n + "* value, const " + n + "* other);");
        }
      }
      for (const auto& factory : static_cast<const class_node&>(*node).constructions) {
        line(construction_signature(static_cast<const class_node&>(*node), factory) + ";");
        for (std::size_t arity{}; arity < factory.parameters.size(); ++arity) {
          if (!factory.parameters[arity].default_value.empty() || !factory.parameters[arity].default_factory.empty()) {
            line(construction_signature(static_cast<const class_node&>(*node), factory, arity) + ";");
          }
        }
      }
    }
    for (const auto& [n, value] : generic_types) {
      line("static inline void " + n + "_srl_read(srl_reader* in, " + n + "* value, bool collection_context);");
      line("static inline void " + n + "_srl_write(srl_writer* out, const " + n + "* value, bool collection_context);");
      if (!value.is_optional()) { line("static inline void " + n + "_clear(" + n + "* value);"); }
    }
    for (const auto& [n, value] : generic_types) {
      if (value.is_optional()) { continue; }
      collection_helpers(n, value.arguments.back(), value.is_associative(), value.arguments.front());
    }
    for (const auto& [n, value] : generic_types) { (void)n; generic_codecs(value); }
    for (const auto* node : model.nodes) {
      if (node->type == object_type::class_type) {
        object(static_cast<const class_node&>(*node));
      }
    }
    return output;
  }
};
} // namespace
// Generate C11 source without C++ runtime dependencies.
std::string c(const schema& value) {
  return c_emitter{value}.generate();
}
} // namespace rohit::serializer::writer::native
