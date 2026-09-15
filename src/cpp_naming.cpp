#include "cpp_naming.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::serializer::writer::cpp {
namespace {
enum class letter_case { snake, pascal, camel };

// Classify ASCII letters without locale-dependent behavior or signed-char conversion.
bool upper(char value) {
  return value >= 'A' && value <= 'Z';
}

// Classify lowercase ASCII letters used by schema identifiers.
bool lower(char value) {
  return value >= 'a' && value <= 'z';
}

// Classify ASCII decimal digits, retaining them inside identifier words.
bool digit(char value) {
  return value >= '0' && value <= '9';
}

// Split underscores and case transitions, treating HTTPServer as HTTP + Server.
std::string convert(std::string_view name, letter_case style) {
  std::vector<std::string> words{};
  std::string word{};
  for (std::size_t index = 0; index < name.size(); ++index) {
    const auto character = name[index];
    if (character == '_') {
      if (!word.empty()) {
        words.push_back(std::move(word));
        word.clear();
      }
      continue;
    }
    const bool boundary =
        !word.empty() && upper(character) &&
        (lower(name[index - 1]) || digit(name[index - 1]) ||
         (upper(name[index - 1]) && index + 1 < name.size() && lower(name[index + 1])));
    if (boundary) {
      words.push_back(std::move(word));
      word.clear();
    }
    word += upper(character) ? static_cast<char>(character - 'A' + 'a') : character;
  }
  if (!word.empty()) {
    words.push_back(std::move(word));
  }
  std::string result{};
  for (auto& part : words) {
    if (style == letter_case::snake && !result.empty()) {
      result += '_';
    }
    if (style == letter_case::pascal || (style == letter_case::camel && !result.empty())) {
      if (lower(part.front())) {
        part.front() = static_cast<char>(part.front() - 'a' + 'A');
      }
    }
    result += part;
  }
  return result;
}

// Reject C++20 keywords and implementation-reserved identifiers before writing a header.
void validate_identifier(std::string_view name) {
  constexpr std::array keywords{"alignas",       "alignof",     "and",
                                "and_eq",        "asm",         "auto",
                                "bitand",        "bitor",       "bool",
                                "break",         "case",        "catch",
                                "char",          "char8_t",     "char16_t",
                                "char32_t",      "class",       "compl",
                                "concept",       "const",       "consteval",
                                "constexpr",     "constinit",   "const_cast",
                                "continue",      "co_await",    "co_return",
                                "co_yield",      "decltype",    "default",
                                "delete",        "do",          "double",
                                "dynamic_cast",  "else",        "enum",
                                "explicit",      "export",      "extern",
                                "false",         "float",       "for",
                                "friend",        "goto",        "if",
                                "inline",        "int",         "long",
                                "mutable",       "namespace",   "new",
                                "noexcept",      "not",         "not_eq",
                                "nullptr",       "operator",    "or",
                                "or_eq",         "private",     "protected",
                                "public",        "register",    "reinterpret_cast",
                                "requires",      "return",      "short",
                                "signed",        "sizeof",      "static",
                                "static_assert", "static_cast", "struct",
                                "switch",        "template",    "this",
                                "thread_local",  "throw",       "true",
                                "try",           "typedef",     "typeid",
                                "typename",      "union",       "unsigned",
                                "using",         "virtual",     "void",
                                "volatile",      "wchar_t",     "while",
                                "xor",           "xor_eq"};
  if (name.empty() || (!upper(name.front()) && !lower(name.front()) && name.front() != '_') ||
      std::ranges::any_of(name,
                          [](char character) {
                            return !upper(character) && !lower(character) && !digit(character) &&
                                   character != '_';
                          }) ||
      name.find("__") != std::string_view::npos ||
      (name.front() == '_' && (name.size() == 1 || upper(name[1]))) ||
      std::ranges::find(keywords, name) != keywords.end()) {
    throw std::invalid_argument{"Invalid or reserved generated C++ identifier: " +
                                std::string{name}};
  }
}

// Register a non-overloadable symbol and reject any duplicate in the same C++ scope.
void insert_name(std::set<std::string>& names, const std::string& name) {
  validate_identifier(name);
  if (!names.insert(name).second) {
    throw std::invalid_argument{"Generated C++ name collision: " + name};
  }
}
} // namespace

// Apply the type convention chosen by a concrete guide or the repository fallback.
std::string naming::type_name(std::string_view name) const {
  if (!options.rename_identifiers) {
    return std::string{name};
  }
  const bool pascal = options.standard == coding_standard::google ||
                      options.standard == coding_standard::llvm ||
                      options.standard == coding_standard::qt;
  return convert(name, pascal ? letter_case::pascal : letter_case::snake);
}

// Separate class field naming from types, functions, and their unchanged wire keys.
std::string naming::field_name(std::string_view name) const {
  if (!options.rename_identifiers) {
    return std::string{name};
  }
  switch (options.standard) {
  case coding_standard::google:
    return convert(name, letter_case::snake) + "_";
  case coding_standard::llvm:
    return convert(name, letter_case::pascal);
  case coding_standard::qt:
    return convert(name, letter_case::camel);
  default:
    return convert(name, letter_case::snake);
  }
}

// Local variables follow ordinary variable rules rather than class data member rules.
std::string naming::local_name(std::string_view name) const {
  if (!options.rename_identifiers) {
    return std::string{name};
  }
  switch (options.standard) {
  case coding_standard::llvm:
    return convert(name, letter_case::pascal);
  case coding_standard::qt:
    return convert(name, letter_case::camel);
  default:
    return convert(name, letter_case::snake);
  }
}

// Scoped enum values use the guide's constant convention, retaining their numeric order.
std::string naming::enum_name(std::string_view name) const {
  if (!options.rename_identifiers) {
    return std::string{name};
  }
  if (options.standard == coding_standard::google) {
    return "k" + convert(name, letter_case::pascal);
  }
  return type_name(name);
}

// Use normal function naming for generated accessors and enum conversion helpers.
std::string naming::function_name(std::string_view name) const {
  if (!options.rename_identifiers) {
    return std::string{name};
  }
  switch (options.standard) {
  case coding_standard::google:
    return convert(name, letter_case::pascal);
  case coding_standard::llvm:
  case coding_standard::qt:
    return convert(name, letter_case::camel);
  default:
    return convert(name, letter_case::snake);
  }
}

// Namespace components follow a stable lower-case convention in the provided profiles.
std::string naming::namespace_name(std::string_view name) const {
  return options.rename_identifiers ? convert(name, letter_case::snake) : std::string{name};
}

// Follow resolved declaration ownership, avoiding a second ambiguous type-name lookup.
std::string naming::full_type_name(const syntax_node* node) const {
  std::string result = type_name(node->name);
  for (auto* parent = node->parent_namespace; parent; parent = parent->parent_namespace) {
    result = namespace_name(parent->name) + "::" + result;
  }
  return "::" + result;
}

// Literal defaults are language-neutral enough to retain; declared enum references are resolved.
std::string naming::default_value(const member& field) const {
  if (!options.rename_identifiers || field.default_value.empty()) {
    return field.default_value;
  }
  const auto& text = field.default_value;
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = text.find_last_not_of(" \t\r\n");
  const auto value = text.substr(first, last - first + 1);
  if (field.modifier == member::modifier_type::none &&
      field.type_name_list.front().type == object_type::enum_type) {
    const auto* node = static_cast<const enum_node*>(field.type_name_list.front().resolved_node);
    for (const auto& alternative : node->enum_name_list) {
      if (value == node->name + "::" + alternative ||
          value == node->get_full_name() + "::" + alternative ||
          value == "::" + node->get_full_name() + "::" + alternative) {
        return full_type_name(node) + "::" + enum_name(alternative);
      }
    }
  }
  // Skip ordinary quoted literals and numbers as tokens, not by replacing substrings in C++ text.
  for (std::size_t index = 0; index < value.size();) {
    const auto character = value[index];
    if (character == '"' || character == '\'') {
      const auto quote = character;
      bool closed = false;
      while (++index < value.size()) {
        if (value[index] == '\\') {
          if (++index == value.size()) {
            break;
          }
        } else if (value[index] == quote) {
          ++index;
          closed = true;
          break;
        }
      }
      if (!closed) {
        throw std::invalid_argument{"Unterminated C++ default literal for " + field.name};
      }
    } else if (digit(character)) {
      while (++index < value.size() &&
             (digit(value[index]) || upper(value[index]) || lower(value[index]) ||
              value[index] == '.' || value[index] == '\'')) {
      }
    } else if (lower(character) || upper(character) || character == '_') {
      const auto start = index++;
      while (index < value.size() && (lower(value[index]) || upper(value[index]) ||
                                      digit(value[index]) || value[index] == '_')) {
        ++index;
      }
      const auto token = value.substr(start, index - start);
      if (token != "true" && token != "false" && token != "nullptr") {
        throw std::invalid_argument{
            "Opaque C++ default expression for " + field.name +
            "; use a literal, a qualified declared enum value, or naming = preserve"};
      }
    } else {
      ++index;
    }
  }
  return value;
}

// Validate separately per namespace, class representation, and scoped enum.
void naming::validate_names(const std::vector<std::unique_ptr<syntax_node>>& statements) const {
  std::map<std::string, std::map<std::string, std::string>> namespace_symbols{};
  const auto visit = [&](const auto& self, const auto& nodes, const std::string& scope) -> void {
    for (const auto& pointer : nodes) {
      const auto& node = *pointer;
      const bool is_namespace = node.type == object_type::namespace_type;
      const auto name = is_namespace ? namespace_name(node.name) : type_name(node.name);
      validate_identifier(name);
      if (name.starts_with('_')) {
        throw std::invalid_argument{"Reserved namespace-scope identifier: " + name};
      }
      const auto [position, inserted] =
          namespace_symbols[scope].emplace(name, node.get_full_name());
      if (!inserted && (!is_namespace || position->second != node.get_full_name())) {
        throw std::invalid_argument{"Generated C++ name collision: " + scope + "::" + name};
      }
      if (is_namespace) {
        self(self, static_cast<const namespace_node&>(node).statements, scope + "::" + name);
      } else if (node.type == object_type::enum_type) {
        auto& symbols = namespace_symbols[scope];
        for (const auto& helper :
             {std::string{"serializer_enum_valid"}, std::string{"serializer_enum_name"},
              std::string{"serializer_enum_from_name"}, std::string{"to_string"},
              function_name("to_" + node.name)}) {
          validate_identifier(helper);
          const auto [symbol, added] = symbols.emplace(helper, "<generated function>");
          if (!added && symbol->second != "<generated function>") {
            throw std::invalid_argument{"Generated C++ name collision: " + helper};
          }
        }
        std::set<std::string> values{};
        for (const auto& value : static_cast<const enum_node&>(node).enum_name_list) {
          insert_name(values, enum_name(value));
        }
      } else if (node.type == object_type::class_type) {
        const auto& object = static_cast<const class_node&>(node);
        std::set<std::string> owning{"serialize_in",
                                     "serialize_out",
                                     "serialize_in_member_by_identifier",
                                     "serialize_in_member_by_name",
                                     "SerializeInProtocol",
                                     "SerializeOutProtocol",
                                     "Protocol"};
        std::set<std::string> views{"map",           "serializer_scan", "serialized_bytes",
                                    "serialize_out", "wire_endian",     "key_type",
                                    "view_limits",   "field_bytes"};
        if (object.multiple_modes()) {
          insert_name(owning, "Mode");
          insert_name(views, "Mode");
        }
        if (object.has_mode(storage_mode::owning)) {
          insert_name(owning, name);
        }
        insert_name(views, type_name("view_base"));
        insert_name(views, type_name("view_byte"));
        if (object.has_mode(storage_mode::read_only_view) ||
            object.has_mode(storage_mode::mutable_view)) {
          insert_name(views, name);
        }
        for (std::size_t index = 0; index < object.parents.size() + object.member_list.size();
             ++index) {
          insert_name(views, type_name("serializer_view_field_" + std::to_string(index)));
        }
        for (const auto& base : object.parents) {
          insert_name(views, function_name("get_base_" + std::to_string(base.id)));
        }
        for (const auto& field : object.member_list) {
          if (object.has_mode(storage_mode::owning)) {
            insert_name(owning, field_name(field.name));
            static_cast<void>(default_value(field));
          }
          if (object.has_mode(storage_mode::read_only_view) ||
              object.has_mode(storage_mode::mutable_view)) {
            insert_name(views, function_name("get_" + field.name));
            if (object.has_mode(storage_mode::mutable_view) &&
                field.modifier == member::modifier_type::none &&
                field.type_name_list.front().type != object_type::class_type) {
              insert_name(views, function_name("set_" + field.name));
            }
          }
          if (field.modifier == member::modifier_type::variant) {
            if (object.has_mode(storage_mode::owning)) {
              insert_name(owning, type_name("e_" + field.name));
              insert_name(owning, type_name("u_" + field.name));
              insert_name(owning, field_name(field.name + "_type"));
              insert_name(owning, function_name("to_e_" + field.name));
            }
            insert_name(views, type_name("e_" + field.name));
            insert_name(views, function_name("get_" + field.name + "_type"));
            std::set<std::string> values{};
            std::set<std::string> fields{type_name("u_" + field.name)};
            for (const auto& alternative : field.type_name_list) {
              insert_name(values, enum_name(alternative.enum_name));
              insert_name(fields, field_name(alternative.enum_name));
            }
          }
        }
        if (object.has_mode(storage_mode::owning) && owning.contains("to_string")) {
          for (const auto& field : object.member_list) {
            if (field.modifier == member::modifier_type::variant) {
              throw std::invalid_argument{"Generated C++ name collision: to_string"};
            }
          }
        }
      }
    }
  };
  visit(visit, statements, "");
}
} // namespace rohit::serializer::writer::cpp
