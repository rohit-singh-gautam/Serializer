#include "native_schema.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>

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
  std::string output{
      "#pragma once\n#ifndef SRL_NATIVE_RUNTIME_INCLUDED\n#define SRL_NATIVE_RUNTIME_INCLUDED\n"};
  std::size_t indent{};
  std::string fields_name;
  // Reserve generated global symbols before emitting C's shared typedef/function namespace.
  void validate_symbols() const {
    std::set<std::string> symbols{"size_t",   "ptrdiff_t", "int8_t",   "int16_t",    "int32_t",
                                  "int64_t",  "uint8_t",   "uint16_t", "uint32_t",   "uint64_t",
                                  "intptr_t", "uintptr_t", "intmax_t", "uintmax_t",  "malloc",
                                  "calloc",   "realloc",   "free",     "memset",     "memcpy",
                                  "memcmp",   "strlen",    "strcmp",   "strtod",     "strtof",
                                  "strtoull", "qsort",     "snprintf", "localeconv", "errno"};
    const auto add = [&](const std::string& symbol) {
      validate_identifier(symbol, keywords);
      if (!symbols.insert(symbol).second) {
        throw std::invalid_argument{"C symbol collision: " + symbol};
      }
    };
    for (const auto* node : model.nodes) {
      const auto n = name(node);
      add(n);
      if (node->type == object_type::enum_type) {
        add(n + "_srl_names");
        for (const auto& item : static_cast<const enum_node&>(*node).enum_name_list) {
          add(n + "_" + field(item));
        }
      } else {
        for (const auto suffix :
             {"_init", "_free", "_encode", "_decode", "_srl_read", "_srl_write"}) {
          add(n + suffix);
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
    if (value.type != object_type::primitive) {
      return name(value.resolved_node);
    }
    if (value.name == "string") {
      return "srl_string";
    }
    if (value.name == "char") {
      return "uint8_t";
    }
    return value.name == "bool" || value.name == "float" || value.name == "double"
               ? value.name
               : value.name + "_t";
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
    return literal_value;
  }
  // Initialize one scalar in zeroed storage; propagate the first allocation failure.
  void init_value(const type_name& value, const std::string& dst, const std::string& literal_value,
                  const std::string& status) {
    if (value.type == object_type::class_type) {
      line("if (" + status + " == srl_ok) { " + status + " = " + type(value) + "_init(&" + dst +
           "); }");
    } else if (value.name == "string") {
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
    if (value.type == object_type::class_type) {
      line(type(value) + "_free(&" + dst + ");");
    } else if (value.name == "string") {
      line("srl_string_free(&" + dst + ");");
    }
  }
  // Read a scalar directly into the candidate, merging ordinary nested objects.
  void read_value(const type_name& value, const std::string& dst, bool collection_context) {
    if (value.type == object_type::class_type) {
      line(type(value) + "_srl_read(in, &" + dst + ");");
    } else if (value.type == object_type::enum_type) {
      line(dst + " = (" + type(value) + ")srl_read_enum(in, " + type(value) +
           "_srl_names, sizeof(" + type(value) + "_srl_names)/sizeof(" + type(value) +
           "_srl_names[0]), " + (collection_context ? "true" : "false") + ");");
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
    if (value.type == object_type::class_type) {
      line(type(value) + "_srl_write(out, &" + src + ");");
    } else if (value.type == object_type::enum_type) {
      line("srl_write_enum(out, (uint32_t)" + src + ", " + type(value) + "_srl_names, sizeof(" +
           type(value) + "_srl_names)/sizeof(" + type(value) + "_srl_names[0]), " +
           (collection_context ? "true" : "false") + ");");
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
         std::to_string(model.keys.at(text)) + "], " + (first ? "true" : "false") + ");");
  }
  // Compare exact map key values without subtracting potentially overflowing integers.
  std::string compare(const type_name& value, const std::string& a, const std::string& b) const {
    return value.name == "string" ? "srl_string_compare(&" + a + ", &" + b + ")"
                                  : "(" + a + " > " + b + ") - (" + a + " < " + b + ")";
  }
  // Emit typed collection cleanup and map normalization helpers.
  void collection_helpers(const class_node& owner, const member& value) {
    const auto n = collection(owner, value);
    const auto& t = value.type_name_list.front();
    const bool map = value.modifier == member::modifier_type::map;
    line("/* Release every initialized collection slot, including partially decoded values. */");
    open("static inline void " + n + "_clear(" + n + "* value)");
    open("for (size_t i = 0; i < value->size; ++i)");
    if (map) {
      free_value(map_key(value), "value->data[i].key");
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
    line("const int order = " + compare(map_key(value), "a->key", "b->key") + ";");
    line("return order ? order : (a->srl_order > b->srl_order) - (a->srl_order < b->srl_order);");
    close();
    line("/* Sort borrowed entry pointers without mutating the source map. */");
    open("static inline int " + n + "_compare_pointer(const void* left, const void* right)");
    line("const " + entry + "* a = *(const " + entry + "* const*)left; const " + entry +
         "* b = *(const " + entry + "* const*)right;");
    line("const int order = " + compare(map_key(value), "a->key", "b->key") + ";");
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
         compare(map_key(value), "value->data[first].key", "value->data[last+1].key") +
         ") == 0) { ++last; }");
    open("for (size_t i = first; i < last; ++i)");
    free_value(map_key(value), "value->data[i].key");
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
    const auto n = collection(owner, value);
    const bool map = value.modifier == member::modifier_type::map;
    line(n + "_clear(&" + dst + ");");
    line("const size_t count = srl_read_array(in);");
    line("const size_t capacity = srl_array_capacity(in, count, " +
         std::to_string(map ? 0 : width(t)) + ");");
    open("if (capacity && in->status == srl_ok)");
    line(dst + ".data = (" + (map ? n + "_entry" : type(t)) +
         "*)srl_grow(in, NULL, capacity, sizeof(" + dst + ".data[0]));");
    line("if (" + dst + ".data) { " + dst + ".capacity = capacity; }");
    close();
    open("for (size_t index = 0; srl_read_element(in, index, count); ++index)");
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
      init_value(map_key(value), slot + ".key", {}, "in->status");
      init_value(t, slot + ".value", {}, "in->status");
      open("if (in->protocol == srl_json)");
      line(
          "bool has_key = false, has_value = false; size_t entry_cursor = 0; srl_key entry = {0};");
      line("srl_read_begin(in);");
      open("while (srl_read_key(in, NULL, 0, entry_cursor++, &entry))");
      open("if (srl_string_equal(&entry.name, \"key\"))");
      free_value(map_key(value), slot + ".key");
      init_value(map_key(value), slot + ".key", {}, "in->status");
      read_value(map_key(value), slot + ".key", true);
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
      read_value(map_key(value), slot + ".key", true);
      read_value(t, slot + ".value", true);
      close();
    }
    close();
    if (map) {
      line("if (in->status == srl_ok) { " + n + "_normalize(&" + dst + "); }");
    }
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
      write_value(t, src, false);
      return;
    }
    line("if (" + src + ".size && !" + src + ".data) { out->status = srl_invalid; return; }");
    if (value.modifier == member::modifier_type::array) {
      line("srl_write_array(out, " + src + ".size);");
      open("for (size_t i = 0; out->status == srl_ok && i < " + src + ".size; ++i)");
      line("srl_write_element(out, i);");
      write_value(t, src + ".data[i]", true);
      close();
    } else {
      const auto n = collection(owner, value), entry = n + "_entry";
      if (map_key(value).name == "string") {
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
           compare(map_key(value), "entries[i]->key", "entries[i+1]->key") +
           ") != 0) { entries[count++] = entries[i]; }");
      close();
      line("srl_write_array(out, count);");
      open("for (size_t i = 0; out->status == srl_ok && i < count; ++i)");
      line("srl_write_element(out, i);");
      line("if (out->protocol == srl_json) { srl_write_begin(out); srl_write_field(out, 0, &" +
           fields_name + "[0], true); }");
      write_value(map_key(value), "entries[i]->key", true);
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
  // Emit lifecycle APIs and direct per-field codecs for one C model.
  void object(const class_node& value) {
    const auto n = name(&value);
    for (const auto& f : value.member_list) {
      if (f.modifier == member::modifier_type::array || f.modifier == member::modifier_type::map) {
        collection_helpers(value, f);
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
      if (f.modifier == member::modifier_type::variant) {
        for (const auto& alt : f.type_name_list) {
          init_value(alt, dst + "_" + field(alt.enum_name), {}, "status");
        }
      } else if (f.modifier == member::modifier_type::none) {
        init_value(f.type_name_list.front(), dst, literal(f), "status");
      }
    }
    line("if (status != srl_ok) { " + n + "_free(value); } return status;");
    close();
    line("/* Encode in schema order; the source retains ownership of all its fields. */");
    open("static inline void " + n + "_srl_write(srl_writer* out, const " + n + "* value)");
    line("(void)value; if (out->status != srl_ok) { return; } srl_write_begin(out); if "
         "(out->status != srl_ok) { return; }");
    bool first = true;
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      const auto& base = value.parents[i];
      key(base.id, base.display_name, first);
      line(name(base.parent_class) + "_srl_write(out, &value->base" + std::to_string(i) + ");");
      first = false;
    }
    for (const auto& f : value.member_list) {
      open("");
      write_member(value, f, first);
      close();
      first = false;
    }
    line("srl_write_end(out);");
    close();
    line("/* Decode into a private candidate; public decoding commits only complete messages. */");
    open("static inline void " + n + "_srl_read(srl_reader* in, " + n + "* value)");
    line("(void)value; if (in->status != srl_ok) { return; } srl_read_begin(in);");
    std::string ids = "static const uint32_t ids[] = {";
    for (const auto& base : value.parents) {
      ids += std::to_string(base.id) + ",";
    }
    for (const auto& f : value.member_list) {
      ids += std::to_string(f.id) + ",";
    }
    ids += "0};";
    line(ids);
    line("size_t cursor = 0; srl_key key = {0};");
    open("while (srl_read_key(in, ids, sizeof(ids)/sizeof(ids[0])-1, cursor++, &key))");
    line("uint32_t id = key.id; int alternative = -1;");
    open("if (key.named)");
    bool branch_first = true;
    const auto branch = [&](const std::string& text, std::uint32_t id, int alt) {
      line(std::string{branch_first ? "if" : "else if"} + " (srl_string_equal(&key.name, " +
           quote(text) + ")) { id = " + std::to_string(id) +
           "; alternative = " + std::to_string(alt) + "; }");
      branch_first = false;
    };
    for (const auto& base : value.parents) {
      branch(base.display_name, base.id, -1);
    }
    for (const auto& f : value.member_list) {
      if (f.modifier == member::modifier_type::variant) {
        for (std::size_t i = 0; i < f.type_name_list.size(); ++i) {
          branch(f.display_name + ":" + f.type_name_list[i].enum_name, f.id, static_cast<int>(i));
        }
      } else {
        branch(f.display_name, f.id, -1);
      }
    }
    line(std::string{branch_first ? "" : "else "} + "{ in->status = srl_invalid; }");
    close();
    line("srl_string_free(&key.name); (void)alternative; if (in->status != srl_ok) { break; }");
    open("switch (id)");
    for (std::size_t i = 0; i < value.parents.size(); ++i) {
      line("case " + std::to_string(value.parents[i].id) + ": " +
           name(value.parents[i].parent_class) + "_srl_read(in, &value->base" + std::to_string(i) +
           "); break;");
    }
    for (const auto& f : value.member_list) {
      open("case " + std::to_string(f.id) + ":");
      read_member(value, f);
      line("break;");
      close();
    }
    line("default: in->status = srl_invalid; break;");
    close();
    close();
    line("srl_read_end(in);");
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
  }
  // Emit immutable key tables, public owning layouts, and checked direct codecs.
  std::string generate() {
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
      } else {
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
    for (const auto* node : model.nodes) {
      if (node->type == object_type::class_type) {
        declarations(static_cast<const class_node&>(*node));
      }
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
    }
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
