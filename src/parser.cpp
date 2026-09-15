//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#include <rohit/serializer_creator.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <queue>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rohit::serializer {

// Convert ASCII uppercase letters to lowercase without changing other bytes.
void to_lower_in_place(std::string& value) {
  for (auto& ch : value) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = ch - 'A' + 'a';
    }
  }
}

// Combine the supplied attribute flags into the left operand.
class_attributes& operator|=(class_attributes& lhs, const class_attributes& rhs) {
  using T = std::underlying_type_t<class_attributes>;
  auto ulhs = static_cast<T>(lhs);
  auto urhs = static_cast<T>(rhs);
  lhs = static_cast<class_attributes>(ulhs | urhs);
  return lhs;
}

class_attributes operator&(const class_attributes& lhs, const class_attributes& rhs) {
  using T = std::underlying_type_t<class_attributes>;
  auto ulhs = static_cast<T>(lhs);
  auto urhs = static_cast<T>(rhs);
  return static_cast<class_attributes>(ulhs & urhs);
}

// Return the qualified name of the supplied non-null namespace node.
std::string get_full_name_for_namespace(const namespace_node* namespace_ptr) {
  return namespace_ptr->get_full_name();
}

namespace parser {
// Test for the ASCII whitespace characters accepted by the parser.
constexpr bool is_whitespace(const char val) noexcept {
  return val == ' ' || val == '\t' || val == '\n' || val == '\r';
}
// Test whether a character is an ASCII decimal digit.
constexpr bool is_number(const char val) noexcept {
  return val >= '0' && val <= '9';
}
// Test whether a character is an ASCII lowercase letter.
constexpr bool is_small_alphabet(const char val) noexcept {
  return val >= 'a' && val <= 'z';
}
// Test whether a character is an ASCII uppercase letter.
constexpr bool is_capital_alphabet(const char val) noexcept {
  return val >= 'A' && val <= 'Z';
}
// Test whether a character can begin a schema identifier.
constexpr bool is_first_identifier(const char val) noexcept {
  return is_capital_alphabet(val) || is_small_alphabet(val) || val == '_';
}
// Test whether a character can continue a schema identifier.
constexpr bool is_identifier(const char val) noexcept {
  return is_number(val) || is_capital_alphabet(val) || is_small_alphabet(val) || val == '_';
}
// Test for the ASCII whitespace characters accepted by the parser.
bool is_whitespace(const stream& in_stream) {
  return is_whitespace(*in_stream);
}
// Test whether a character is an ASCII decimal digit.
bool is_number(const stream& in_stream) {
  return is_number(*in_stream);
}
// Test whether a character is an ASCII lowercase letter.
bool is_small_alphabet(const stream& in_stream) {
  return is_small_alphabet(*in_stream);
}
// Test whether a character is an ASCII uppercase letter.
bool is_capital_alphabet(const stream& in_stream) {
  return is_capital_alphabet(*in_stream);
}
// Test whether a character can begin a schema identifier.
bool is_first_identifier(const stream& in_stream) {
  return is_first_identifier(*in_stream);
}
// Test whether a character can continue a schema identifier.
bool is_identifier(const stream& in_stream) {
  return is_identifier(*in_stream);
}
// Advance past whitespace before the next token.
void skip_whitespace(const stream& in_stream) {
  while (is_whitespace(in_stream)) {
    ++in_stream;
  }
}
// Test whether every character belongs to a decimal integer token.
bool check_number(const std::string& number_text) {
  for (auto ch : number_text) {
    if (!is_number(ch)) {
      return false;
    }
  }
  return true;
}

// Read an identifier or quoted spelling from a member specification.
auto get_member_spec_token(const stream& in_stream) {
  std::string token{};
  bool is_string{false};
  if (*in_stream == '"') {
    is_string = true;
    ++in_stream;
  }
  while (is_identifier(in_stream)) {
    token.push_back(*in_stream);
    ++in_stream;
  }
  if (is_string) {
    if (*in_stream != '"') {
      throw exception::bad_member_spec{in_stream, "String must be enclosed in double quotes"};
    }
    ++in_stream;
  }
  return std::make_pair(token, is_string);
}

// Consume whitespace and line or block comments before the next token.
void skip_whitespace_and_comment(const stream& in_stream) {
  for (;;) {
    auto ch = *in_stream;
    if (is_whitespace(ch)) {
      ++in_stream;
      continue;
    }
    if (ch == '/') {
      ++in_stream;
      auto ch1 = *in_stream;
      if (ch1 == '/') {
        // Skip till new line
        ++in_stream;
        while (*in_stream && *in_stream != '\n') {
          ++in_stream;
        }
        continue;
      }
      if (ch1 == '*') {
        // Skip till */
        ++in_stream;
        for (;;) {
          const auto ch2 = *in_stream;
          if (ch2 == '*') {
            ++in_stream;
            const auto ch3 = *in_stream;
            if (ch3 == '/') {
              ++in_stream;
              break;
            }
          }
          ++in_stream;
        }
        continue;
      }
    }
    break;
  }
} // skip_whitespace_and_comment

// Read the initializer text inside a schema default-value clause.
std::string get_default_value(const stream& in_stream) {
  std::string default_value{};
  if (*in_stream != '{') {
    return default_value;
  }
  ++in_stream;
  skip_whitespace_and_comment(in_stream);
  while (*in_stream != '}' && !is_whitespace(in_stream)) {
    default_value.push_back(*in_stream);
    ++in_stream;
  }
  skip_whitespace_and_comment(in_stream);
  if (*in_stream != '}') {
    throw exception::bad_member_spec{in_stream, "Default value must be enclosed in curly braces"};
  }
  ++in_stream;
  return default_value;
} // get_default_value

// Consume the expected character or throw a parser diagnostic.
void check_and_increase(const stream& in_stream, char value) {
  if (*in_stream != value) {
    std::string error_text{"Expected: "};
    error_text.push_back(value);
    error_text += ", Found: ";
    error_text.push_back(*in_stream);
    throw exception::bad_class{in_stream, error_text};
  }
  ++in_stream;
} // check_and_increase

// Parse number from the schema input; malformed input throws.
template <std::integral T>
T parse_number(const stream& in_stream) {
  T ret{0};
  while (is_number(*in_stream)) {
    ret = ret * 10 + (*in_stream - '0');
    ++in_stream;
  }
  return ret;
}

// Parse identifier from the schema input; malformed input throws.
std::string parse_identifier(const stream& in_stream) {
  std::string identifier{};
  auto ch = *in_stream;
  if (!is_first_identifier(ch)) {
    std::string error_text{"Identifier can start with '_' or alphabet only it cannot start with: "};
    error_text.push_back(ch);
    throw exception::bad_identifier{in_stream, error_text};
  }
  identifier.push_back(ch);
  ++in_stream;
  while (is_identifier(*in_stream)) {
    identifier.push_back(*in_stream);
    ++in_stream;
  }
  return identifier;
} // parse_identifier

// Parse hierarchical identifier from the schema input; malformed input throws.
std::string parse_hierarchical_identifier(const stream& in_stream) {
  std::string identifier{};
  while (true) {
    if (!is_first_identifier(*in_stream)) {
      std::string error_text{"Identifier cannot start with "};
      error_text.push_back(*in_stream);
      throw exception::bad_identifier{in_stream, error_text};
    }
    identifier.push_back(*in_stream);
    ++in_stream;
    while (is_identifier(*in_stream)) {
      identifier.push_back(*in_stream);
      ++in_stream;
    }
    if (in_stream.remaining_buffer() < 2) {
      break;
    }
    if (*in_stream != ':') {
      break;
    }
    ++in_stream;
    if (*in_stream != ':') {
      throw exception::bad_identifier{
          in_stream,
          {"Namespace and identifier must be separated by '::', only one ':' is unsupported "}};
    }
    ++in_stream;
    identifier.push_back(':');
    identifier.push_back(':');
    if (in_stream.full()) {
      throw exception::bad_identifier{in_stream,
                                      {"Atleast one characted is require for identifier"}};
    }
  }
  return identifier;
} // parse_hierarchical_identifier

// Invoke the callback for each whitespace-separated identifier.
void space_separated_identifier(const stream& in_stream, std::function<void(std::string&&)> fn) {
  if (!is_first_identifier(in_stream)) {
    return;
  }
  while (true) {
    auto identifier = parse_identifier(in_stream);
    fn(std::move(identifier));
    if (!is_whitespace(in_stream)) {
      break;
    }
    skip_whitespace_and_comment(in_stream);
    if (!is_first_identifier(in_stream)) {
      break;
    }
  }
  return;
} // space_separated_identifier

access_type parse_access_type(const stream& in_stream) {
  auto access_type = parse_identifier(in_stream);
  if (access_type == "public") {
    return access_type::public_access;
  }
  if (access_type == "protected") {
    return access_type::protected_access;
  }
  if (access_type == "private") {
    return access_type::private_access;
  }
  std::string error_text{
      "Bad access type it must be one of 'public', 'protected' or 'private' case "
      "sensitive. Unknown access type: "};
  error_text += access_type;
  throw exception::bad_access_type{in_stream, error_text};
} // parse_access_type

// Parse member modifier from the schema input; malformed input throws.
auto parse_member_modifier(const std::string& type) {
  if (type == "array") {
    return member::modifier_type::array;
  } else if (type == "map") {
    return member::modifier_type::map;
  } else if (type == "union") {
    return member::modifier_type::variant;
  }
  return member::modifier_type::none;
} // parse_member_modifier

// Parse member type union from the schema input; malformed input throws.
void parse_member_type_union(const stream& in_stream, namespace_node* declared_namespace,
                             std::vector<type_name>& type_name_list) {
  skip_whitespace_and_comment(in_stream);
  check_and_increase(in_stream, '(');
  int count{0};
  while (true) {
    skip_whitespace_and_comment(in_stream);
    auto type_name = parse_hierarchical_identifier(in_stream);
    skip_whitespace_and_comment(in_stream);
    if (*in_stream == '=') {
      ++in_stream;
      skip_whitespace_and_comment(in_stream);
      auto enum_name = parse_identifier(in_stream);
      type_name_list.emplace_back(std::move(type_name), std::move(enum_name), declared_namespace);
      skip_whitespace_and_comment(in_stream);
    } else {
      std::string enum_name{"e_" + std::to_string(count)};
      type_name_list.emplace_back(std::move(type_name), std::move(enum_name), declared_namespace);
      ++count;
    }
    if (*in_stream != ',') {
      break;
    }
    ++in_stream;
  }
  check_and_increase(in_stream, ')');
} // parse_member_type_union

// Parse member type map from the schema input; malformed input throws.
void parse_member_type_map(const stream& in_stream, namespace_node* declared_namespace,
                           std::vector<type_name>& type_name_list, std::string& key) {
  skip_whitespace_and_comment(in_stream);
  check_and_increase(in_stream, '(');
  skip_whitespace_and_comment(in_stream);
  key = parse_hierarchical_identifier(in_stream);
  check_and_increase(in_stream, ')');
  skip_whitespace_and_comment(in_stream);
  auto type_name = parse_hierarchical_identifier(in_stream);
  type_name_list.emplace_back(std::move(type_name), declared_namespace);
} // parse_member_type_map

// Parse name spec from the schema input; malformed input throws.
void parse_name_spec(const stream& in_stream, std::uint32_t& new_id, std::string& display_name) {
  check_and_increase(in_stream, '(');
  bool string_parsed{false};
  bool number_parsed{false};
  if (*in_stream != ')') {
    while (true) {
      skip_whitespace_and_comment(in_stream);
      auto [id, is_string] = get_member_spec_token(in_stream);
      if (is_string) {
        if (string_parsed) {
          throw exception::bad_member_spec{in_stream, "Only one string is allowed in member spec"};
        }
        string_parsed = true;
        display_name = id;
      } else if (check_number(id)) {
        if (number_parsed) {
          throw exception::bad_member_spec{in_stream, "Only one number is allowed in member spec"};
        }
        number_parsed = true;
        new_id = std::stoul(id);
      } else {
        throw exception::bad_member_spec{in_stream, "Unknown parameter in member spec"};
      }
      skip_whitespace_and_comment(in_stream);
      if (*in_stream != ',') {
        break;
      }
      ++in_stream;
      if (*in_stream == ')') {
        throw exception::bad_member_spec{in_stream, "Unexpected end of member spec"};
      }
    }
  }
  check_and_increase(in_stream, ')');
} // parse_name_spec

member parse_member(const stream& in_stream, const std::uint32_t id,
                    namespace_node* declared_namespace) {
  auto access = parse_access_type(in_stream);
  skip_whitespace_and_comment(in_stream);
  auto next_identifier = parse_hierarchical_identifier(in_stream);
  std::vector<std::string> enum_name_list{};
  std::vector<type_name> type_name_list{};
  auto member_modifier = parse_member_modifier(next_identifier);
  std::string key{};
  if (member_modifier == member::modifier_type::none) {
    type_name_list.emplace_back(std::move(next_identifier), declared_namespace);
  } else if (member_modifier == member::modifier_type::array) {
    skip_whitespace_and_comment(in_stream);
    auto type_name = parse_hierarchical_identifier(in_stream);
    type_name_list.emplace_back(std::move(type_name), declared_namespace);
  } else if (member_modifier == member::modifier_type::map) {
    parse_member_type_map(in_stream, declared_namespace, type_name_list, key);
  } else if (member_modifier == member::modifier_type::variant) {
    parse_member_type_union(in_stream, declared_namespace, type_name_list);
  }
  skip_whitespace_and_comment(in_stream);
  auto name = parse_identifier(in_stream);
  auto display_name = name;
  std::uint32_t new_id{id};
  bool parsed_member_spec{false};
  bool parsed_default_value{false};
  std::string default_value{};
  while (true) {
    skip_whitespace_and_comment(in_stream);
    if (*in_stream == '(') {
      if (parsed_member_spec) {
        throw exception::bad_member_spec{in_stream, "Only one member spec is allowed"};
      }
      parsed_member_spec = true;
      parse_name_spec(in_stream, new_id, display_name);
    } else if (*in_stream == '{') {
      if (parsed_default_value) {
        throw exception::bad_member_spec{in_stream, "Only one default value is allowed"};
      }
      parsed_default_value = true;
      default_value = get_default_value(in_stream);
    } else {
      break;
    }
  }
  skip_whitespace_and_comment(in_stream);
  check_and_increase(in_stream, ';');
  return {access, member_modifier, type_name_list, name, display_name, new_id, key, default_value};
} // parse_member

object_type parse_object_type(const stream& in_stream) {
  auto object_type = parse_identifier(in_stream);
  if (object_type == "class") {
    return object_type::class_type;
  }
  if (object_type == "namespace") {
    return object_type::namespace_type;
  }
  if (object_type == "enum") {
    return object_type::enum_type;
  }
  std::string error_text{"Bad identifier type it must be one of 'class' or 'namespace' case "
                         "sensitive. Unknown access type: "};
  error_text += object_type;
  throw exception::bad_object_type{in_stream, error_text};
} // parse_object_type

// Parse class body from the schema input; malformed input throws.
void parse_class_body(const stream& in_stream, class_node* obj, std::uint32_t& id) {
  if (*in_stream != '{') {
    std::string error_text{"Expecting '{' found: "};
    error_text += *in_stream;
    throw exception::bad_class{in_stream, error_text};
  }
  ++in_stream;
  skip_whitespace_and_comment(in_stream);
  while (*in_stream != '}') {
    auto member = parse_member(in_stream, id++, obj->parent_namespace);
    obj->member_list.push_back(std::move(member));
    skip_whitespace_and_comment(in_stream);
  }
  ++in_stream;
  if (*in_stream == ';') {
    throw exception::bad_class{in_stream, {"Semicolon is not expected at the end of a class"}};
  }
}

parent parse_parent(const stream& in_stream, namespace_node* current_namespace,
                    const std::uint32_t id) {
  auto access = parse_access_type(in_stream);
  skip_whitespace_and_comment(in_stream);
  auto full_name = parse_hierarchical_identifier(in_stream);
  std::string display_name = full_name;
  std::uint32_t new_id = id;
  skip_whitespace_and_comment(in_stream);
  if (*in_stream == '(') {
    parse_name_spec(in_stream, new_id, display_name);
    skip_whitespace_and_comment(in_stream);
  }
  return {access, full_name, display_name, new_id, current_namespace, nullptr};
}

// Parse parent list from the schema input; malformed input throws.
std::vector<parent> parse_parent_list(const stream& in_stream, namespace_node* current_namespace,
                                      std::uint32_t& id) {
  std::vector<parent> ret{};
  if (!is_first_identifier(in_stream)) {
    return ret;
  }
  while (true) {
    ret.push_back(parse_parent(in_stream, current_namespace, id++));
    if (*in_stream != ',') {
      break;
    }
    ++in_stream;
    skip_whitespace_and_comment(in_stream);
  }

  return ret;
}

// Parse class header from the schema input; malformed input throws.
std::unique_ptr<class_node>
parse_class_header(const stream& in_stream, namespace_node* current_namespace, std::uint32_t& id) {
  // Object type is already parsed
  skip_whitespace_and_comment(in_stream);
  auto name = parse_identifier(in_stream);
  skip_whitespace_and_comment(in_stream);
  auto attributes{class_attributes::none};
  space_separated_identifier(in_stream, [&attributes](std::string&& value) {
    if (value == "packed") {
      attributes |= class_attributes::packed;
    }
  });
  std::vector<parent> parents;
  // At this point all whitespace is skipped
  if (*in_stream == ':') {
    ++in_stream;
    skip_whitespace_and_comment(in_stream);
    auto parsed_parents = parse_parent_list(in_stream, current_namespace, id);
    std::swap(parents, parsed_parents);
  }

  return std::make_unique<class_node>(object_type::class_type, std::move(name), current_namespace,
                                      attributes, std::move(parents));
}

// Parse class from the schema input; malformed input throws.
std::unique_ptr<class_node> parse_class(const stream& in_stream,
                                        namespace_node* current_namespace) {
  std::uint32_t id{1};
  auto obj = parse_class_header(in_stream, current_namespace, id);
  // At this point all whitespace is skipped
  parse_class_body(in_stream, obj.get(), id);
  return obj;
}

// Parse enum from the schema input; malformed input throws.
std::unique_ptr<enum_node> parse_enum(const stream& in_stream, namespace_node* current_namespace) {
  skip_whitespace_and_comment(in_stream);
  auto enum_name = parse_identifier(in_stream);
  skip_whitespace_and_comment(in_stream);
  if (*in_stream != '{') {
    std::string error_text{"Expecting '{' found: "};
    error_text += *in_stream;
    throw exception::bad_class{in_stream, error_text};
  }
  ++in_stream;
  skip_whitespace_and_comment(in_stream);
  std::vector<std::string> enum_name_list{};
  if (*in_stream != '}') {
    while (true) {
      auto name = parse_identifier(in_stream);
      enum_name_list.push_back(name);
      skip_whitespace_and_comment(in_stream);
      if (*in_stream != ',') {
        break;
      }
      ++in_stream;
      skip_whitespace_and_comment(in_stream);
      if (*in_stream == '}') {
        break;
      }
    }
    if (*in_stream != '}') {
      std::string error_text{"Expecting '}' found: "};
      error_text += *in_stream;
      throw exception::bad_class{in_stream, error_text};
    }
  }
  ++in_stream;
  if (*in_stream == ';') {
    throw exception::bad_class{in_stream, {"Semicolon is not expected at the end of a class"}};
  }
  auto ret = std::make_unique<enum_node>(object_type::enum_type, std::move(enum_name),
                                         current_namespace, std::move(enum_name_list));
  return ret;
}

// Parse namespace from the schema input; malformed input throws.
std::unique_ptr<namespace_node> parse_namespace(const stream& in_stream,
                                                namespace_node* parent_namespace);

// Parse statement list from the schema input; malformed input throws.
std::vector<std::unique_ptr<syntax_node>> parse_statement_list(const stream& in_stream,
                                                               namespace_node* parent_namespace) {
  std::vector<std::unique_ptr<syntax_node>> statements{};
  while (true) {
    skip_whitespace_and_comment(in_stream);
    if (in_stream.full() || *in_stream == '}' || *in_stream == 0xcd || *in_stream == 0x00) {
      break;
    }
    auto object_type = parse_object_type(in_stream);
    if (object_type == object_type::class_type) {
      statements.emplace_back(parse_class(in_stream, parent_namespace));
    } else if (object_type == object_type::namespace_type) {
      statements.emplace_back(parse_namespace(in_stream, parent_namespace));
    } else if (object_type == object_type::enum_type) {
      statements.emplace_back(parse_enum(in_stream, parent_namespace));
    } else {
      std::string error_text{
          "Bad identifier type it must be one of 'class' or 'namespace' case sensitive."};
      throw exception::bad_object_type{in_stream, error_text};
    }
  }

  return statements;
}

// Parse namespace from the schema input; malformed input throws.
std::unique_ptr<namespace_node> parse_namespace(const stream& in_stream,
                                                namespace_node* parent_namespace) {
  // Object type is already parsed
  skip_whitespace_and_comment(in_stream);
  auto name = parse_hierarchical_identifier(in_stream);
  skip_whitespace_and_comment(in_stream);
  if (*in_stream != '{') {
    std::string error_text{"Expecting '{' found: "};
    error_text += *in_stream;
    throw exception::bad_namespace{in_stream, error_text};
  }
  ++in_stream;

  auto ret = std::make_unique<namespace_node>(object_type::namespace_type, std::move(name),
                                              parent_namespace);
  auto statements = parse_statement_list(in_stream, ret.get());
  std::swap(ret->statements, statements);

  if (*in_stream != '}') {
    std::string error_text{"Expecting '}' found: "};
    error_text += *in_stream;
    throw exception::bad_namespace{in_stream, error_text};
  }
  ++in_stream;
  return ret;
} // parse_namespace

// Resolve an unresolved primitive type or throw an unknown-type diagnostic.
void check_member_type_for_primitive(const stream& in_stream, type_name& type_name) {
  if (type_name.type != object_type::unresolved) {
    return;
  }
  if (serializer::get_cpp_type_or_empty(type_name.name).empty()) {
    std::string error_text{"Unknown type: "};
    error_text += type_name.name;
    throw exception::bad_member_type{in_stream, error_text};
  }
  type_name.type = object_type::primitive;
}

// Resolve member types against the discovered namespace and type declarations.
void resolve_member(const stream& in_stream, member& member,
                    const std::unordered_map<std::string, object_type>& variable_type_map) {
  for (auto& type_name : member.type_name_list) {
    std::queue<namespace_node*> namespace_stack{};
    namespace_node* current_namespace = type_name.declared_namespace;
    while (current_namespace) {
      namespace_stack.push(current_namespace);
      current_namespace = current_namespace->parent_namespace;
    }
    while (!namespace_stack.empty()) {
      current_namespace = namespace_stack.front();
      namespace_stack.pop();
      auto candidate_full_name = current_namespace->get_full_name() + "::" + type_name.name;
      auto type_iterator = variable_type_map.find(candidate_full_name);
      if (type_iterator != std::end(variable_type_map)) {
        type_name.type = type_iterator->second;
        type_name.defined_namespace = current_namespace;
        break;
      }
    }
    check_member_type_for_primitive(in_stream, type_name);
  }
}

// Resolve member types against the discovered namespace and type declarations.
void resolve_member(const stream& in_stream,
                    std::vector<std::unique_ptr<rohit::serializer::syntax_node>>& statements,
                    std::unordered_map<std::string, object_type>& variable_type_map) {
  for (auto& statement : statements) {
    switch (statement->type) {
    case object_type::namespace_type: {
      auto namespace_ptr = dynamic_cast<namespace_node*>(statement.get());
      resolve_member(in_stream, namespace_ptr->statements, variable_type_map);
    } break;

    case object_type::class_type: {
      variable_type_map.insert({statement->get_full_name(), object_type::class_type});
      auto class_ptr = dynamic_cast<class_node*>(statement.get());
      for (auto& member : class_ptr->member_list) {
        resolve_member(in_stream, member, variable_type_map);
      }
    } break;

    case object_type::enum_type:
      variable_type_map.insert({statement->get_full_name(), object_type::enum_type});
      break;

    default:
      break;
    }
  }
}

// Parse schema declarations and resolve their member types; malformed input throws.
std::vector<std::unique_ptr<syntax_node>> parse(const stream& in_stream) {
  auto statements = parse_statement_list(in_stream, nullptr);
  std::unordered_map<std::string, object_type> variable_type_map;
  resolve_member(in_stream, statements, variable_type_map);
  return statements;
}

} // namespace parser
} // namespace rohit::serializer
