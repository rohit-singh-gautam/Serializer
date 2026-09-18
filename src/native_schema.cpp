#include "native_schema.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <limits>
#include <regex>
#include <set>
#include <stdexcept>

namespace rohit::serializer::writer::native {

// Keep conversion deterministic for the parser's ASCII identifier alphabet.
std::string pascal(std::string_view value) {
  std::string result;
  bool upper = true;
  for (const auto ch : value) {
    if (ch == '_') {
      upper = true;
    } else {
      result += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(ch))) : ch;
      upper = false;
    }
  }
  return result;
}
// Insert boundaries before capitals while keeping existing underscores stable.
std::string snake(std::string_view value) {
  std::string result;
  for (std::size_t i = 0; i < value.size(); ++i) {
    const auto ch = static_cast<unsigned char>(value[i]);
    if (std::isupper(ch) && i && value[i - 1] != '_' &&
        (std::islower(static_cast<unsigned char>(value[i - 1])) ||
         (i + 1 < value.size() && std::islower(static_cast<unsigned char>(value[i + 1]))))) {
      result += '_';
    }
    result += static_cast<char>(std::tolower(ch));
  }
  return result;
}
// Emit the subset of escapes understood by every target language.
std::string quote(std::string_view value) {
  std::string result = "\"";
  for (const auto ch : value) {
    switch (ch) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
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
      result += ch;
      break;
    }
  }
  return result + '"';
}
// Validate source identifiers before a generated file is published.
void validate_identifier(std::string_view value, std::string_view reserved) {
  const std::string name{value};
  if (!std::regex_match(name, std::regex{"[A-Za-z][A-Za-z_0-9]*"}) ||
      reserved.find(" " + name + " ") != std::string_view::npos || name.starts_with("Srl") ||
      name.starts_with("srl")) {
    throw std::invalid_argument{"Invalid or reserved native identifier: " + name};
  }
}
// Fixed-width values have an input lower bound usable before reserving collections.
int width(const type_name& value) {
  if (value.type != object_type::primitive || value.name == "string") {
    return 0;
  }
  if (value.name == "bool" || value.name == "char" || value.name.ends_with("8")) {
    return 1;
  }
  if (value.name.ends_with("16")) {
    return 2;
  }
  if (value.name == "float" || value.name.ends_with("32")) {
    return 4;
  }
  return 8;
}
// Map keys carry their resolved enum node independently of field values.
type_name map_key(const member& value) {
  type_name result{std::string{value.key}, nullptr};
  result.type = value.key_node ? value.key_node->type : object_type::primitive;
  result.resolved_node = value.key_node;
  return result;
}
// Validate defaults once; backends only translate the already checked spelling.
std::string literal(const member& value) {
  const auto first = value.default_value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto text = value.default_value.substr(
      first, value.default_value.find_last_not_of(" \t\r\n") - first + 1);
  const auto& type = value.type_name_list.front();
  if (type.type == object_type::enum_type) {
    const auto pos = text.rfind("::");
    const auto name = pos == std::string::npos ? text : text.substr(pos + 2);
    const auto& node = static_cast<const enum_node&>(*type.resolved_node);
    if ((pos == std::string::npos || text.substr(0, pos) == type.name ||
         text.substr(0, pos) == node.name || text.substr(0, pos) == node.get_full_name()) &&
        std::find(node.enum_name_list.begin(), node.enum_name_list.end(), name) !=
            node.enum_name_list.end()) {
      return name;
    }
  } else if (type.type == object_type::primitive) {
    if (type.name == "string" &&
        std::regex_match(text, std::regex{"\"([^\"\\\\]|\\\\[\"\\\\bfnrt])*\""})) {
      return text;
    }
    if (type.name == "bool" && (text == "true" || text == "false")) {
      return text;
    }
    if (type.name == "char" && text.front() == '\'' && text.back() == '\'') {
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
    if ((type.name == "float" || type.name == "double") &&
        std::regex_match(text, std::regex{"-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?"})) {
      float f{};
      double d{};
      const auto parsed = type.name == "float"
                              ? std::from_chars(text.data(), text.data() + text.size(), f)
                              : std::from_chars(text.data(), text.data() + text.size(), d);
      if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()) {
        return text;
      }
    }
    if ((type.name.starts_with("int") || type.name.starts_with("uint")) &&
        std::regex_match(text, std::regex{"-?(0|[1-9][0-9]*)"})) {
      std::uint64_t u{};
      std::int64_t s{};
      const bool positive = type.name.starts_with('u');
      const auto parsed = positive ? std::from_chars(text.data(), text.data() + text.size(), u)
                                   : std::from_chars(text.data(), text.data() + text.size(), s);
      const auto bits = width(type) * 8;
      if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
          (bits == 64 || (positive ? u < (std::uint64_t{1} << bits)
                                   : s >= -(std::int64_t{1} << (bits - 1)) &&
                                         s < (std::int64_t{1} << (bits - 1))))) {
        return text;
      }
    }
  }
  throw std::invalid_argument{"Expected a portable literal default: " + value.name};
}
// Intern names including JSON map-entry framing.
void schema::add_key(const std::string& value) {
  if (keys.emplace(value, ordered_keys.size()).second) {
    ordered_keys.push_back(value);
  }
}
// Flatten namespaces, rejecting ambiguous generated type spellings.
void schema::collect(const std::vector<std::unique_ptr<syntax_node>>& values,
                     const std::string& prefix) {
  for (const auto& value : values) {
    const auto name = prefix + pascal(value->name);
    if (value->type == object_type::namespace_type) {
      collect(static_cast<const namespace_node&>(*value).statements, name);
      continue;
    }
    validate_identifier(
        name, " Protocol Limits Error Result String Int Float Double Bool Data Vec Option Default "
              "Self None Some Ok Err List Map Set Array ByteArray Unit Any ");
    if (std::any_of(names.begin(), names.end(),
                    [&](const auto& item) { return item.second == name; })) {
      throw std::invalid_argument{"Native type name collision: " + name};
    }
    names.emplace(value.get(), name);
    nodes.push_back(value.get());
  }
}
// Reject unsupported shapes before emitting code in any of the new languages.
schema::schema(const std::vector<std::unique_ptr<syntax_node>>& statements) {
  keys.emplace("key", 0);
  keys.emplace("value", 1);
  collect(statements);
  for (const auto* node : nodes) {
    if (node->type == object_type::enum_type) {
      if (static_cast<const enum_node&>(*node).enum_name_list.empty()) {
        throw std::invalid_argument{"Native enums require at least one value"};
      }
      std::set<std::string> used;
      for (const auto& item : static_cast<const enum_node&>(*node).enum_name_list) {
        if (!used.insert(pascal(item)).second || !used.insert("snake_" + snake(item)).second) {
          throw std::invalid_argument{"Enum name collision"};
        }
      }
      continue;
    }
    const auto& object = static_cast<const class_node&>(*node);
    if (object.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        (object.attributes & class_attributes::packed) != class_attributes::none) {
      throw std::invalid_argument{"Native output supports owning classes only"};
    }
    std::set<std::string> fields{"encode", "decode", "read", "write", "init", "free", "default"};
    const auto add = [&](const std::string& name) {
      validate_identifier(name, "");
      if (!fields.insert(snake(name)).second) {
        throw std::invalid_argument{"Native field collision: " + name};
      }
    };
    for (std::size_t i = 0; i < object.parents.size(); ++i) {
      add("base" + std::to_string(i));
      add_key(object.parents[i].display_name);
    }
    for (const auto& field : object.member_list) {
      if (field.modifier != member::modifier_type::none && !field.default_value.empty()) {
        throw std::invalid_argument{"Collection and union defaults are unsupported"};
      }
      if (field.modifier == member::modifier_type::map) {
        const auto key = map_key(field);
        if (key.type == object_type::class_type || key.name == "float" || key.name == "double") {
          throw std::invalid_argument{"Unsupported map key"};
        }
      }
      for (const auto& alternative : field.type_name_list) {
        if (alternative.resolved_node == node &&
            (field.modifier == member::modifier_type::none ||
             field.modifier == member::modifier_type::variant)) {
          throw std::invalid_argument{"Direct self-containing owning default"};
        }
      }
      if (field.modifier == member::modifier_type::variant) {
        add(field.name + "_index");
        for (const auto& alternative : field.type_name_list) {
          add(field.name + "_" + alternative.enum_name);
          add_key(field.display_name + ":" + alternative.enum_name);
        }
      } else {
        add(field.name);
        add_key(field.display_name);
        if (field.modifier == member::modifier_type::none) {
          (void)literal(field);
        }
      }
    }
  }
}

} // namespace rohit::serializer::writer::native
