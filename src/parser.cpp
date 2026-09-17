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
#include <rohit/version.hpp>

#include "schema_scan.hpp"

#include <charconv>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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
bool is_whitespace(const rohit::type_check::schema_input_buffer auto& in_stream) {
  return !in_stream.full() && is_whitespace(*in_stream);
}
// Test whether a character is an ASCII decimal digit.
bool is_number(const rohit::type_check::schema_input_buffer auto& in_stream) {
  return !in_stream.full() && is_number(*in_stream);
}
// Test whether a character is an ASCII lowercase letter.
bool is_small_alphabet(const rohit::type_check::schema_input_buffer auto& in_stream) {
  return !in_stream.full() && is_small_alphabet(*in_stream);
}
// Test whether a character is an ASCII uppercase letter.
bool is_capital_alphabet(const rohit::type_check::schema_input_buffer auto& in_stream) {
  return !in_stream.full() && is_capital_alphabet(*in_stream);
}
// Test whether a character can begin a schema identifier.
bool is_first_identifier(const rohit::type_check::schema_input_buffer auto& in_stream) {
  return !in_stream.full() && is_first_identifier(*in_stream);
}
// Test whether a character can continue a schema identifier.
bool is_identifier(const rohit::type_check::schema_input_buffer auto& in_stream) {
  return !in_stream.full() && is_identifier(*in_stream);
}
// Advance past whitespace before the next token.
void skip_whitespace(const rohit::type_check::schema_input_buffer auto& in_stream) {
  const auto size = detail::scan_prefix<detail::scan_kind::whitespace>(
      in_stream.curr(), in_stream.remaining_buffer());
  in_stream.advance_unchecked(size);
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
auto get_member_spec_token(const rohit::type_check::schema_input_buffer auto& in_stream) {
  bool is_string{false};
  if (*in_stream == '"') {
    is_string = true;
    ++in_stream;
  }
  const auto size = detail::scan_prefix<detail::scan_kind::identifier>(
      in_stream.curr(), in_stream.remaining_buffer());
  std::string token{reinterpret_cast<const char*>(in_stream.curr()), size};
  in_stream.advance_unchecked(size);
  if (is_string) {
    if (*in_stream != '"') {
      throw exception::bad_member_spec{in_stream, "String must be enclosed in double quotes"};
    }
    ++in_stream;
  }
  return std::make_pair(std::move(token), is_string);
}

// Consume complete whitespace and comment spans, including a final line comment at EOF.
void skip_whitespace_and_comment(const rohit::type_check::schema_input_buffer auto& input) {
  while (!input.full()) {
    if (is_whitespace(*input)) {
      skip_whitespace(input);
      continue;
    }
    if (*input != '/') { return; }
    if (input.remaining_buffer() < 2) {
      throw exception::bad_input_data{input, "Incomplete schema comment"};
    }
    const auto kind = input.curr()[1];
    if (kind != '/' && kind != '*') {
      throw exception::bad_input_data{input, "Invalid schema comment"};
    }
    input.advance_unchecked(2);
    if (kind == '/') {
      const auto size = detail::scan_prefix<detail::scan_kind::line_comment>(
          input.curr(), input.remaining_buffer());
      input.advance_unchecked(size);
    } else {
      bool closed{};
      while (input.remaining_buffer() >= 2) {
        // Leave the last byte unread unless it belongs to a complete terminator.
        // This also preserves the diagnostic cursor for an unterminated comment.
        const auto size = detail::scan_prefix<detail::scan_kind::block_comment>(
            input.curr(), input.remaining_buffer() - 1);
        input.advance_unchecked(size);
        if (input.remaining_buffer() < 2) {
          break;
        }
        if (input.curr()[1] == '/') {
          input.advance_unchecked(2);
          closed = true;
          break;
        }
        input.advance_unchecked(1);
      }
      if (!closed) { throw exception::bad_input_data{input, "Unterminated schema comment"}; }
    }
  }
}
// Preserve initializer spelling, including whitespace and braces inside quoted literals.
std::string get_default_value(const rohit::type_check::schema_input_buffer auto& in_stream) {
  std::string default_value{};
  if (in_stream.full() || *in_stream != '{') {
    return default_value;
  }
  ++in_stream;
  skip_whitespace_and_comment(in_stream);
  char quote{};
  bool escaped{};
  while (!in_stream.full()) {
    const auto character = static_cast<char>(*in_stream);
    if (quote == 0 && (character == '}' || is_whitespace(in_stream))) {
      break;
    }
    default_value.push_back(character);
    ++in_stream;
    if (quote != 0) {
      if (escaped) {
        escaped = false;
      } else if (character == '\\') {
        escaped = true;
      } else if (character == quote) {
        quote = 0;
      }
    } else if (character == '"' ||
               (character == '\'' &&
                (default_value.size() == 1 || default_value == "u'" || default_value == "U'" ||
                 default_value == "L'" || default_value == "u8'"))) {
      // Apostrophes inside numeric tokens are digit separators, not character literals.
      quote = character;
    }
  }
  if (quote != 0) {
    throw exception::bad_member_spec{in_stream, "Unterminated quoted default value"};
  }
  skip_whitespace_and_comment(in_stream);
  if (in_stream.full() || *in_stream != '}') {
    throw exception::bad_member_spec{in_stream, "Default value must be enclosed in curly braces"};
  }
  ++in_stream;
  return default_value;
} // get_default_value

// Consume the expected character or throw a parser diagnostic.
void check_and_increase(const rohit::type_check::schema_input_buffer auto& in_stream, char value) {
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
T parse_number(const rohit::type_check::schema_input_buffer auto& in_stream) {
  T ret{0};
  while (is_number(*in_stream)) {
    ret = ret * 10 + (*in_stream - '0');
    ++in_stream;
  }
  return ret;
}

// Parse identifier from the schema input; malformed input throws.
std::string parse_identifier_impl(const rohit::type_check::schema_input_buffer auto& in_stream) {
  if (in_stream.full()) {
    throw exception::bad_identifier{in_stream, "Expected an identifier before end of input"};
  }
  auto ch = *in_stream;
  if (!is_first_identifier(ch)) {
    std::string error_text{"Identifier can start with '_' or alphabet only it cannot start with: "};
    error_text.push_back(ch);
    throw exception::bad_identifier{in_stream, error_text};
  }
  const auto size = detail::scan_prefix<detail::scan_kind::identifier>(
      in_stream.curr(), in_stream.remaining_buffer());
  std::string identifier{reinterpret_cast<const char*>(in_stream.curr()), size};
  in_stream.advance_unchecked(size);
  return identifier;
} // parse_identifier_impl

// Parse hierarchical identifier from the schema input; malformed input throws.
std::string
parse_hierarchical_identifier_impl(const rohit::type_check::schema_input_buffer auto& in_stream) {
  if (in_stream.full()) {
    throw exception::bad_identifier{in_stream, "Expected an identifier before end of input"};
  }
  const auto* begin = in_stream.curr();
  const auto available = in_stream.remaining_buffer();
  while (true) {
    if (!is_first_identifier(*in_stream)) {
      std::string error_text{"Identifier cannot start with "};
      error_text.push_back(*in_stream);
      throw exception::bad_identifier{in_stream, error_text};
    }
    const auto size = detail::scan_prefix<detail::scan_kind::identifier>(
        in_stream.curr(), in_stream.remaining_buffer());
    in_stream.advance_unchecked(size);
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
    if (in_stream.full()) {
      throw exception::bad_identifier{in_stream,
                                      {"Atleast one characted is require for identifier"}};
    }
  }
  // The cursor only advances within the original range, including namespace separators.
  return {reinterpret_cast<const char*>(begin), available - in_stream.remaining_buffer()};
} // parse_hierarchical_identifier_impl

// Invoke the callback for each whitespace-separated identifier.
void space_separated_identifier_impl(const rohit::type_check::schema_input_buffer auto& in_stream,
                                     std::function<void(std::string&&)> fn) {
  if (!is_first_identifier(in_stream)) {
    return;
  }
  while (true) {
    auto identifier = parse_identifier_impl(in_stream);
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
} // space_separated_identifier_impl

access_type parse_access_type_impl(const rohit::type_check::schema_input_buffer auto& in_stream) {
  auto access_type = parse_identifier_impl(in_stream);
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
} // parse_access_type_impl

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
void parse_member_type_union(const rohit::type_check::schema_input_buffer auto& in_stream,
                             namespace_node* declared_namespace,
                             std::vector<type_name>& type_name_list) {
  skip_whitespace_and_comment(in_stream);
  check_and_increase(in_stream, '(');
  int count{0};
  while (true) {
    skip_whitespace_and_comment(in_stream);
    auto type_name = parse_hierarchical_identifier_impl(in_stream);
    skip_whitespace_and_comment(in_stream);
    if (*in_stream == '=') {
      ++in_stream;
      skip_whitespace_and_comment(in_stream);
      auto enum_name = parse_identifier_impl(in_stream);
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
void parse_member_type_map(const rohit::type_check::schema_input_buffer auto& in_stream,
                           namespace_node* declared_namespace,
                           std::vector<type_name>& type_name_list, std::string& key) {
  skip_whitespace_and_comment(in_stream);
  check_and_increase(in_stream, '(');
  skip_whitespace_and_comment(in_stream);
  key = parse_hierarchical_identifier_impl(in_stream);
  check_and_increase(in_stream, ')');
  skip_whitespace_and_comment(in_stream);
  auto type_name = parse_hierarchical_identifier_impl(in_stream);
  type_name_list.emplace_back(std::move(type_name), declared_namespace);
} // parse_member_type_map

// Reject unrepresentable field IDs and keys reserved for object terminators.
void validate_field_key(const rohit::type_check::schema_input_buffer auto& in_stream,
                        std::uint32_t id, std::string_view display_name) {
  if (id == constants::binary_object_end_id || id > constants::variable_four_byte_max) {
    throw exception::bad_member_spec{in_stream, "Field IDs must be in the range 1 to 0x3fffffff"};
  }
  if (display_name.empty()) {
    throw exception::bad_member_spec{in_stream, "Field names must not be empty"};
  }
}

// Parse a wire name and decimal ID without narrowing or overflowing the numeric token.
void parse_name_spec(const rohit::type_check::schema_input_buffer auto& in_stream,
                     std::uint32_t& new_id, std::string& display_name, bool& explicit_id) {
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
        std::uint32_t parsed_id{};
        const auto result =
            std::from_chars(id.data(), id.data() + id.size(), parsed_id, constants::decimal_radix);
        if (result.ec != std::errc{} || result.ptr != id.data() + id.size()) {
          throw exception::bad_member_spec{in_stream, "Invalid decimal field ID"};
        }
        new_id = parsed_id;
        explicit_id = true;
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

member parse_member_impl(const rohit::type_check::schema_input_buffer auto& in_stream,
                         const std::uint32_t id, namespace_node* declared_namespace) {
  auto access = parse_access_type_impl(in_stream);
  skip_whitespace_and_comment(in_stream);
  auto next_identifier = parse_hierarchical_identifier_impl(in_stream);
  std::vector<std::string> enum_name_list{};
  std::vector<type_name> type_name_list{};
  auto member_modifier = parse_member_modifier(next_identifier);
  std::string key{};
  if (member_modifier == member::modifier_type::none) {
    type_name_list.emplace_back(std::move(next_identifier), declared_namespace);
  } else if (member_modifier == member::modifier_type::array) {
    skip_whitespace_and_comment(in_stream);
    auto type_name = parse_hierarchical_identifier_impl(in_stream);
    type_name_list.emplace_back(std::move(type_name), declared_namespace);
  } else if (member_modifier == member::modifier_type::map) {
    parse_member_type_map(in_stream, declared_namespace, type_name_list, key);
  } else if (member_modifier == member::modifier_type::variant) {
    parse_member_type_union(in_stream, declared_namespace, type_name_list);
  }
  skip_whitespace_and_comment(in_stream);
  auto name = parse_identifier_impl(in_stream);
  auto display_name = name;
  std::uint32_t new_id{id};
  bool parsed_member_spec{false};
  bool explicit_id{false};
  bool parsed_default_value{false};
  std::string default_value{};
  while (true) {
    skip_whitespace_and_comment(in_stream);
    if (*in_stream == '(') {
      if (parsed_member_spec) {
        throw exception::bad_member_spec{in_stream, "Only one member spec is allowed"};
      }
      parsed_member_spec = true;
      parse_name_spec(in_stream, new_id, display_name, explicit_id);
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
  validate_field_key(in_stream, new_id, display_name);
  return {access, member_modifier, type_name_list, name, display_name, new_id, key, default_value,
          explicit_id};
} // parse_member_impl

// Read a declaration keyword and distinguish misplaced includes from unknown declarations.
object_type parse_object_type(const rohit::type_check::schema_input_buffer auto& in_stream) {
  auto object_type = parse_identifier_impl(in_stream);
  if (object_type == "class") {
    return object_type::class_type;
  }
  if (object_type == "namespace") {
    return object_type::namespace_type;
  }
  if (object_type == "enum") {
    return object_type::enum_type;
  }
  if (object_type == "include") {
    throw exception::bad_object_type{
        in_stream, "Includes require parse_file and must precede all declarations at file scope"};
  }
  std::string error_text{"Bad identifier type it must be one of 'class' or 'namespace' case "
                         "sensitive. Unknown access type: "};
  error_text += object_type;
  throw exception::bad_object_type{in_stream, error_text};
} // parse_object_type

// Parse class body from the schema input; malformed input throws.
void parse_class_body_impl(const rohit::type_check::schema_input_buffer auto& in_stream,
                           class_node* obj, std::uint32_t& id) {
  if (*in_stream != '{') {
    std::string error_text{"Expecting '{' found: "};
    error_text += *in_stream;
    throw exception::bad_class{in_stream, error_text};
  }
  ++in_stream;
  skip_whitespace_and_comment(in_stream);
  while (*in_stream != '}') {
    auto member = parse_member_impl(in_stream, id++, obj->parent_namespace);
    obj->member_list.push_back(std::move(member));
    skip_whitespace_and_comment(in_stream);
  }
  ++in_stream;
  if (!in_stream.full() && *in_stream == ';') {
    throw exception::bad_class{in_stream, {"Semicolon is not expected at the end of a class"}};
  }
}

parent parse_parent(const rohit::type_check::schema_input_buffer auto& in_stream,
                    namespace_node* current_namespace, const std::uint32_t id) {
  auto access = parse_access_type_impl(in_stream);
  skip_whitespace_and_comment(in_stream);
  auto full_name = parse_hierarchical_identifier_impl(in_stream);
  std::string display_name = full_name;
  std::uint32_t new_id = id;
  bool explicit_id{false};
  skip_whitespace_and_comment(in_stream);
  if (*in_stream == '(') {
    parse_name_spec(in_stream, new_id, display_name, explicit_id);
    skip_whitespace_and_comment(in_stream);
  }
  validate_field_key(in_stream, new_id, display_name);
  return {access, full_name, display_name, new_id, current_namespace, nullptr, explicit_id};
}

// Parse parent list from the schema input; malformed input throws.
std::vector<parent> parse_parent_list(const rohit::type_check::schema_input_buffer auto& in_stream,
                                      namespace_node* current_namespace, std::uint32_t& id) {
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
parse_class_header(const rohit::type_check::schema_input_buffer auto& in_stream,
                   namespace_node* current_namespace, std::uint32_t& id) {
  // Object type is already parsed
  skip_whitespace_and_comment(in_stream);
  auto name = parse_identifier_impl(in_stream);
  skip_whitespace_and_comment(in_stream);
  auto attributes{class_attributes::none};
  bool view{}, owning{}, readonly{}, mutable_view{};
  space_separated_identifier_impl(in_stream, [&](std::string&& value) {
    if (value == "packed") {
      attributes |= class_attributes::packed;
    } else if (value == "stable_ids") {
      attributes |= class_attributes::stable_ids;
    } else if (value == "view") {
      view = true;
    } else if (value == "owning") {
      owning = true;
    } else if (value == "readonly") {
      readonly = true;
    } else if (value == "mutable") {
      mutable_view = true;
    } else {
      throw exception::bad_class{in_stream, "Unknown class attribute"};
    }
  });
  if (!view && (readonly || mutable_view)) {
    throw exception::bad_class{in_stream, "readonly and mutable require view"};
  }
  if (view && !readonly && !mutable_view) {
    readonly = mutable_view = true;
  }
  if (view && (attributes & class_attributes::packed) == class_attributes::packed) {
    throw exception::bad_class{in_stream, "packed cannot be combined with view"};
  }
  std::vector<parent> parents;
  // At this point all whitespace is skipped
  if (*in_stream == ':') {
    ++in_stream;
    skip_whitespace_and_comment(in_stream);
    auto parsed_parents = parse_parent_list(in_stream, current_namespace, id);
    std::swap(parents, parsed_parents);
  }

  auto result = std::make_unique<class_node>(object_type::class_type, std::move(name), current_namespace,
                                           attributes, std::move(parents));
  result->storage_modes = static_cast<std::uint8_t>(
      ((!view || owning) ? static_cast<unsigned>(storage_mode::owning) : 0u) |
      (readonly ? static_cast<unsigned>(storage_mode::read_only_view) : 0u) |
      (mutable_view ? static_cast<unsigned>(storage_mode::mutable_view) : 0u));
  return result;
}

// Reject ambiguous field identity, including collisions between explicit and implicit IDs.
void validate_class_keys(const rohit::type_check::schema_input_buffer auto& input,
                         const class_node& obj) {
  std::unordered_set<std::uint32_t> ids;
  std::unordered_set<std::string> names;
  const bool stable = (obj.attributes & class_attributes::stable_ids) == class_attributes::stable_ids;
  const auto add_id = [&](std::uint32_t id, bool explicit_id) {
    if (stable && !explicit_id) {
      throw exception::bad_member_spec{input, "stable_ids requires explicit field and parent IDs"};
    }
    if (!ids.insert(id).second) {
      throw exception::bad_member_spec{input, "Duplicate field or parent ID"};
    }
  };
  const auto add_name = [&](const std::string& name) {
    if (!names.insert(name).second) {
      throw exception::bad_member_spec{input, "Duplicate field or parent wire name"};
    }
  };
  for (const auto& base : obj.parents) {
    add_id(base.id, base.explicit_id);
    add_name(base.display_name);
  }
  for (const auto& field : obj.member_list) {
    add_id(field.id, field.explicit_id);
    if (field.modifier == member::modifier_type::variant) {
      std::unordered_set<std::string> alternatives;
      for (const auto& alternative : field.type_name_list) {
        if (!alternatives.insert(alternative.enum_name).second) {
          throw exception::bad_member_spec{input, "Duplicate union alternative name"};
        }
        add_name(field.display_name + ":" + alternative.enum_name);
      }
    } else {
      add_name(field.display_name);
    }
  }
}

// Register source declarations before parsing their bodies; reopened namespace scopes are shared.
void register_declaration(const rohit::type_check::schema_input_buffer auto& input,
                          syntax_node& node,
                          std::unordered_map<std::string, syntax_node*>& symbols);

// Parse class from the schema input; malformed input or an occupied name throws at creation.
std::unique_ptr<class_node>
parse_class(const rohit::type_check::schema_input_buffer auto& in_stream,
            namespace_node* current_namespace,
            std::unordered_map<std::string, syntax_node*>& declarations) {
  std::uint32_t id{1};
  auto obj = parse_class_header(in_stream, current_namespace, id);
  register_declaration(in_stream, *obj, declarations);
  // At this point all whitespace is skipped
  parse_class_body_impl(in_stream, obj.get(), id);
  validate_class_keys(in_stream, *obj);
  return obj;
}

// Parse enum from the schema input; malformed input throws.
std::unique_ptr<enum_node> parse_enum(const rohit::type_check::schema_input_buffer auto& in_stream,
                                      namespace_node* current_namespace,
                                      std::unordered_map<std::string, syntax_node*>& declarations) {
  skip_whitespace_and_comment(in_stream);
  auto enum_name = parse_identifier_impl(in_stream);
  auto ret = std::make_unique<enum_node>(object_type::enum_type, std::move(enum_name),
                                         current_namespace, std::vector<std::string>{});
  register_declaration(in_stream, *ret, declarations);
  skip_whitespace_and_comment(in_stream);
  if (*in_stream != '{') {
    std::string error_text{"Expecting '{' found: "};
    error_text += *in_stream;
    throw exception::bad_class{in_stream, error_text};
  }
  ++in_stream;
  skip_whitespace_and_comment(in_stream);
  std::unordered_set<std::string> enum_names;
  if (*in_stream != '}') {
    while (true) {
      auto name = parse_identifier_impl(in_stream);
      if (!enum_names.insert(name).second) {
        throw exception::bad_member_spec{in_stream, "Duplicate enum name"};
      }
      ret->enum_name_list.push_back(name);
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
  if (!in_stream.full() && *in_stream == ';') {
    throw exception::bad_class{in_stream, {"Semicolon is not expected at the end of a class"}};
  }
  return ret;
}

// Parse namespace from the schema input; malformed input throws.
std::unique_ptr<namespace_node>
parse_namespace(const rohit::type_check::schema_input_buffer auto& in_stream,
                namespace_node* parent_namespace,
                std::unordered_map<std::string, syntax_node*>& declarations);

// Parse statement list from the schema input; malformed input throws.
std::vector<std::unique_ptr<syntax_node>>
parse_statement_list(const rohit::type_check::schema_input_buffer auto& in_stream,
                     namespace_node* parent_namespace,
                     std::unordered_map<std::string, syntax_node*>& declarations) {
  std::vector<std::unique_ptr<syntax_node>> statements{};
  while (true) {
    skip_whitespace_and_comment(in_stream);
    if (in_stream.full() || *in_stream == '}') {
      break;
    }
    auto object_type = parse_object_type(in_stream);
    if (object_type == object_type::class_type) {
      statements.emplace_back(parse_class(in_stream, parent_namespace, declarations));
    } else if (object_type == object_type::namespace_type) {
      statements.emplace_back(parse_namespace(in_stream, parent_namespace, declarations));
    } else if (object_type == object_type::enum_type) {
      statements.emplace_back(parse_enum(in_stream, parent_namespace, declarations));
    } else {
      std::string error_text{
          "Bad identifier type it must be one of 'class' or 'namespace' case sensitive."};
      throw exception::bad_object_type{in_stream, error_text};
    }
  }

  return statements;
}

// Parse namespace from the schema input; malformed input throws.
std::unique_ptr<namespace_node>
parse_namespace(const rohit::type_check::schema_input_buffer auto& in_stream,
                namespace_node* parent_namespace,
                std::unordered_map<std::string, syntax_node*>& declarations) {
  // Object type is already parsed
  skip_whitespace_and_comment(in_stream);
  auto name = parse_hierarchical_identifier_impl(in_stream);
  skip_whitespace_and_comment(in_stream);
  if (*in_stream != '{') {
    std::string error_text{"Expecting '{' found: "};
    error_text += *in_stream;
    throw exception::bad_namespace{in_stream, error_text};
  }
  ++in_stream;

  // Normalize a::b into nested blocks, sharing each existing logical scope immediately.
  // Blocks retain source order for C++ emission; child declarations point to the first scope node.
  std::unique_ptr<namespace_node> ret{};
  namespace_node* block = nullptr;
  auto* scope = parent_namespace;
  std::size_t start = 0;
  do {
    const auto end = name.find("::", start);
    auto next = std::make_unique<namespace_node>(
        object_type::namespace_type,
        name.substr(start, end == std::string::npos ? end : end - start), scope);
    register_declaration(in_stream, *next, declarations);
    scope = static_cast<namespace_node*>(declarations.at(next->get_full_name()));
    auto* next_block = next.get();
    if (block) {
      block->statements.push_back(std::move(next));
    } else {
      ret = std::move(next);
    }
    block = next_block;
    if (end == std::string::npos) {
      break;
    }
    start = end + std::string_view{"::"}.size();
  } while (true);
  block->statements = parse_statement_list(in_stream, scope, declarations);

  if (*in_stream != '}') {
    std::string error_text{"Expecting '}' found: "};
    error_text += *in_stream;
    throw exception::bad_namespace{in_stream, error_text};
  }
  ++in_stream;
  return ret;
} // parse_namespace

// Resolve an unresolved primitive type or throw an unknown-type diagnostic.
void check_member_type_for_primitive(const rohit::type_check::schema_input_buffer auto& in_stream,
                                     type_name& type_name) {
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

// Resolve a declared type from the nearest namespace through global scope.
syntax_node* find_declared_type(const std::string& name, namespace_node* current_namespace,
                               const std::unordered_map<std::string, syntax_node*>& types) {
  auto scope = current_namespace ? current_namespace->get_full_name() : std::string{};
  while (!scope.empty()) {
    const auto found = types.find(scope + "::" + name);
    if (found != types.end()) { return found->second; }
    const auto parent_separator = scope.rfind("::");
    if (parent_separator == std::string::npos) { break; }
    scope.resize(parent_separator);
  }
  const auto found = types.find(name);
  return found == types.end() ? nullptr : found->second;
}

// Preserve resolved declaration identity so generated nested classes select the correct mode.
void resolve_type(const rohit::type_check::schema_input_buffer auto& input, type_name& type,
                  const std::unordered_map<std::string, syntax_node*>& types) {
  if (auto* node = find_declared_type(type.name, type.declared_namespace, types)) {
    if (node->type == object_type::namespace_type) {
      throw exception::bad_member_type{input, "Namespace cannot be used as a type: " + type.name};
    }
    type.resolved_node = node;
    type.type = node->type;
    type.defined_namespace = node->parent_namespace;
  }
  check_member_type_for_primitive(input, type);
}

// Require every nested class to provide the representation used by its containing class.
void validate_nested_modes(const rohit::type_check::schema_input_buffer auto& input,
                           const class_node& owner, const syntax_node* node, bool map_key = false) {
  if (!node || node->type != object_type::class_type) { return; }
  const auto& nested = static_cast<const class_node&>(*node);
  for (const auto mode : {storage_mode::owning, storage_mode::read_only_view, storage_mode::mutable_view}) {
    const auto required = map_key && mode != storage_mode::owning ? storage_mode::read_only_view : mode;
    if (owner.has_mode(mode) && !nested.has_mode(required)) {
      throw exception::bad_class{input, "Nested class " + nested.get_full_name() +
          " does not enable a storage mode required by " + owner.get_full_name()};
    }
  }
}

// Reject generated view accessors that would collide with each other or the mapping API.
void validate_view_names(const rohit::type_check::schema_input_buffer auto& input,
                         const class_node& obj) {
  if (obj.storage_modes == static_cast<std::uint8_t>(storage_mode::owning)) { return; }
  std::unordered_set<std::string> names{"map", "serializer_scan", "serialized_bytes",
      "serialize_out", "wire_endian", "key_type", "view_base", "view_byte", "view_limits"};
  const auto add = [&](std::string name) {
    if (!names.insert(name).second) {
      throw exception::bad_class{input, "Generated view name collision: " + name};
    }
  };
  add(obj.name);
  if (obj.name.starts_with("serializer_view_field_")) {
    throw exception::bad_class{input, "View class name uses the generated field-codec prefix"};
  }
  for (const auto& base : obj.parents) { add("get_base_" + std::to_string(base.id)); }
  for (const auto& field : obj.member_list) {
    add("get_" + field.name);
    if (field.modifier == member::modifier_type::variant) {
      add("e_" + field.name);
      add("get_" + field.name + "_type");
    } else if (obj.has_mode(storage_mode::mutable_view) &&
               field.modifier == member::modifier_type::none &&
               field.type_name_list[0].type != object_type::class_type) {
      add("set_" + field.name);
    }
  }
}

// Reuse the existing symbol table for namespace scopes as well as class and enum declarations.
// Reopening a namespace is valid; every other duplicate qualified name is a schema error.
void register_declaration(const rohit::type_check::schema_input_buffer auto& input,
                          syntax_node& node,
                          std::unordered_map<std::string, syntax_node*>& symbols) {
  const auto full_name = node.get_full_name();
  const bool is_namespace = node.type == object_type::namespace_type;
  std::size_t end = 0;
  do {
    end = is_namespace ? full_name.find("::", end) : std::string::npos;
    const auto name = full_name.substr(0, end);
    const auto [position, inserted] = symbols.emplace(name, &node);
    if (!inserted) {
      const bool previous_namespace = position->second->type == object_type::namespace_type;
      if (is_namespace != previous_namespace) {
        throw exception::bad_class{input, "Namespace/type name conflict: " + name};
      }
      if (!is_namespace) {
        throw exception::bad_class{input, "Duplicate type: " + name};
      }
    }
    if (end == std::string::npos) {
      break;
    }
    end += std::string_view{"::"}.size();
  } while (true);
}

// Resolve member types against the discovered namespace and type declarations.
void resolve_member(const rohit::type_check::schema_input_buffer auto& in_stream,
                    std::vector<std::unique_ptr<rohit::serializer::syntax_node>>& statements,
                    std::unordered_map<std::string, syntax_node*>& variable_type_map) {
  for (auto& statement : statements) {
    switch (statement->type) {
    case object_type::namespace_type: {
      auto namespace_ptr = dynamic_cast<namespace_node*>(statement.get());
      register_declaration(in_stream, *namespace_ptr, variable_type_map);
      resolve_member(in_stream, namespace_ptr->statements, variable_type_map);
    } break;

    case object_type::class_type: {
      register_declaration(in_stream, *statement, variable_type_map);
      auto class_ptr = dynamic_cast<class_node*>(statement.get());
      for (auto& base : class_ptr->parents) {
        auto* node = find_declared_type(base.name, base.current_namespace, variable_type_map);
        if (!node || node == class_ptr || node->type != object_type::class_type) {
          throw exception::bad_class{in_stream, "Parent must name a previously declared class"};
        }
        base.parent_class = static_cast<class_node*>(node);
        validate_nested_modes(in_stream, *class_ptr, node);
      }
      for (auto& member : class_ptr->member_list) {
        for (auto& type : member.type_name_list) {
          resolve_type(in_stream, type, variable_type_map);
          validate_nested_modes(in_stream, *class_ptr, type.resolved_node);
        }
        if (member.modifier == member::modifier_type::map) {
          type_name key{std::string{member.key}, class_ptr->parent_namespace};
          resolve_type(in_stream, key, variable_type_map);
          member.key_node = key.resolved_node;
          validate_nested_modes(in_stream, *class_ptr, member.key_node, true);
        }
      }
      validate_view_names(in_stream, *class_ptr);
    } break;

    case object_type::enum_type:
      register_declaration(in_stream, *statement, variable_type_map);
      break;

    default:
      break;
    }
  }
}

// Consume the leading version statement before any declarations; comments may precede it.
// Bound every read so incomplete headers report schema errors instead of stream overflow.
void parse_version_header(const rohit::type_check::schema_input_buffer auto& input, bool required) {
  skip_whitespace_and_comment(input);
  if (!(input == "serializer")) {
    if (required) {
      throw exception::bad_input_data{input, "Expected first statement: serializer version 1;"};
    }
    return;
  }
  for (const auto token : {std::string_view{"serializer"}, std::string_view{"version"}}) {
    if (!(input == token)) {
      throw exception::bad_input_data{input, "Expected header: serializer version 1;"};
    }
    input += token.size();
    if (!input.full() && is_identifier(*input)) {
      throw exception::bad_input_data{input, "Expected separated header tokens"};
    }
    skip_whitespace_and_comment(input);
  }
  const auto* begin = reinterpret_cast<const char*>(input.curr());
  std::size_t digits{};
  while (!input.full() && is_number(*input)) {
    ++input;
    ++digits;
  }
  unsigned version{};
  if (digits == 0) {
    throw exception::bad_input_data{input, "Expected decimal schema language version"};
  }
  const auto converted = std::from_chars(begin, begin + digits, version);
  if (converted.ec != std::errc{} || version != schema_language_version) {
    throw exception::bad_input_data{input, "Unsupported schema language version; supported: 1"};
  }
  skip_whitespace_and_comment(input);
  if (input.full() || *input != ';') {
    throw exception::bad_input_data{input, "Expected ';' after schema language version"};
  }
  ++input;
}

namespace {
// Recognize an include keyword without accepting identifiers beginning with that spelling.
bool starts_include(const rohit::type_check::schema_input_buffer auto& input) {
  constexpr std::string_view keyword{"include"};
  return input == keyword && (input.remaining_buffer() == keyword.size() ||
                              !is_identifier(static_cast<char>(input.curr()[keyword.size()])));
}

// Read a portable unquoted relative schema path, allowing comments between directive tokens.
std::filesystem::path parse_include(const rohit::type_check::schema_input_buffer auto& input) {
  static_cast<void>(parse_identifier_impl(input));
  skip_whitespace_and_comment(input);
  std::string value{};
  while (!input.full()) {
    const auto ch = static_cast<char>(*input);
    if (input == "//" || input == "/*") {
      break;
    }
    if (!is_identifier(ch) && ch != '.' && ch != '-' && ch != '/') {
      break;
    }
    value.push_back(ch);
    ++input;
  }
  const std::filesystem::path path{value};
  if (value.empty() || path.is_absolute() || path.has_root_path() ||
      path.extension() != ".serializer") {
    throw exception::bad_input_data{input,
                                    "Expected an unquoted relative .serializer path after include"};
  }
  skip_whitespace_and_comment(input);
  if (input.full() || *input != ';') {
    throw exception::bad_input_data{
        input, "Expected ';' after include path (use forward slashes, without quotes or spaces)"};
  }
  ++input;
  return path;
}

// Load each dependency before resolving its includer, retaining declaration-before-use order.
class file_loader {
  enum class file_state { loading, loaded };
  static constexpr std::size_t maximum_include_depth = 32;
  parsed_schema result{};
  std::unordered_map<std::string, file_state> files{};
  // Creation checks all names immediately; resolution exposes only preceding declarations.
  std::unordered_map<std::string, syntax_node*> declarations{};
  std::unordered_map<std::string, syntax_node*> types{};

  // Append a file once; unwind diagnostics through every including file on failure.
  void load(const std::filesystem::path& path, std::size_t depth) {
    try {
      if (path.extension() != ".serializer") {
        throw std::invalid_argument{"Input must use .serializer"};
      }
      const auto canonical = std::filesystem::canonical(path);
      auto identity = canonical.generic_string();
#ifdef _WIN32
      to_lower_in_place(identity);
#endif
      if (const auto found = files.find(identity); found != files.end()) {
        if (found->second == file_state::loading) {
          throw std::invalid_argument{"Include cycle"};
        }
        return;
      }
      if (depth >= maximum_include_depth) {
        throw std::invalid_argument{"Maximum include depth (" +
                                    std::to_string(maximum_include_depth) + " files) exceeded"};
      }
      files.emplace(identity, file_state::loading);
      result.dependencies.push_back(canonical);
      const auto input = rohit::make_stream_from_file(canonical);
      parse_version_header(input, true);
      skip_whitespace_and_comment(input);
      while (starts_include(input)) {
        const auto included = parse_include(input);
        load(canonical.parent_path() / included, depth + 1);
        skip_whitespace_and_comment(input);
      }
      auto statements = parse_statement_list(input, nullptr, declarations);
      if (!input.full()) {
        throw exception::bad_input_data{input, "Unexpected trailing schema input"};
      }
      resolve_member(input, statements, types);
      for (auto& statement : statements) {
        result.statements.push_back(std::move(statement));
      }
      files.at(identity) = file_state::loaded;
    } catch (const std::exception& error) {
      throw std::invalid_argument{path.generic_string() + ": " + error.what()};
    }
  }

public:
  // Return the complete compilation unit only after every dependency has been validated.
  parsed_schema run(const std::filesystem::path& path) {
    load(path, 0);
    return std::move(result);
  }
};
} // namespace

// Give filesystem-aware callers an isolated include cache and symbol table per entry schema.
parsed_schema parse_file(const std::filesystem::path& path) {
  return file_loader{}.run(path);
}

// Parse schema declarations and resolve their member types; malformed input throws.
std::vector<std::unique_ptr<syntax_node>>
parse_schema(const rohit::type_check::schema_input_buffer auto& in_stream, bool require_version) {
  parse_version_header(in_stream, require_version);
  std::unordered_map<std::string, syntax_node*> declarations{};
  auto statements = parse_statement_list(in_stream, nullptr, declarations);
  if (!in_stream.full()) {
    throw exception::bad_input_data{in_stream, "Unexpected trailing schema input"};
  }
  std::unordered_map<std::string, syntax_node*> variable_type_map;
  resolve_member(in_stream, statements, variable_type_map);
  return statements;
}

namespace detail {
// Track the internal scanner's progress without imposing its type on public callers.
template <rohit::type_check::input_buffer Input>
struct consumed_progress {
  const Input& input;
  const std::size_t initial_bytes;
  std::size_t& consumed;
  // Record exactly the bytes consumed before normal return or stack unwinding.
  ~consumed_progress() {
    consumed = initial_bytes - input.remaining_buffer();
  }
};

// Normalize a byte span once; parser internals retain checked cursor and SIMD operations.
template <typename Parse>
decltype(auto) scan_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed,
                          Parse&& parse) {
  const auto input = make_constant_full_stream(bytes.data(), bytes.size());
  const consumed_progress<full_stream> progress{input, bytes.size(), consumed};
  return parse(input);
}

// Compile one schema while retaining checked progress for the caller's independent cursor.
std::vector<std::unique_ptr<syntax_node>> parse_bytes(std::span<const std::uint8_t> bytes,
                                                      std::size_t& consumed, bool require_version) {
  return scan_bytes(bytes, consumed, [require_version](const auto& input) {
    return parse_schema(input, require_version);
  });
}
// Dispatch a single identifier scan.
std::string identifier_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed) {
  return scan_bytes(bytes, consumed,
                    [](const auto& input) { return parse_identifier_impl(input); });
}
// Dispatch a qualified identifier scan.
std::string hierarchical_identifier_bytes(std::span<const std::uint8_t> bytes,
                                          std::size_t& consumed) {
  return scan_bytes(bytes, consumed,
                    [](const auto& input) { return parse_hierarchical_identifier_impl(input); });
}
// Dispatch identifier callbacks without retaining them after this call.
void identifiers_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed,
                       std::function<void(std::string&&)> callback) {
  scan_bytes(bytes, consumed, [&callback](const auto& input) {
    space_separated_identifier_impl(input, std::move(callback));
  });
}
// Dispatch one access-keyword scan.
access_type access_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed) {
  return scan_bytes(bytes, consumed,
                    [](const auto& input) { return parse_access_type_impl(input); });
}
// Dispatch one member scan with its surrounding namespace and starting identifier.
member member_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed, std::uint32_t id,
                    namespace_node* declared_namespace) {
  return scan_bytes(bytes, consumed, [=](const auto& input) {
    return parse_member_impl(input, id, declared_namespace);
  });
}
// Dispatch a class-body scan and share the caller's next identifier.
void class_body_bytes(std::span<const std::uint8_t> bytes, std::size_t& consumed,
                      class_node* object, std::uint32_t& id) {
  scan_bytes(bytes, consumed, [&](const auto& input) { parse_class_body_impl(input, object, id); });
}
} // namespace detail
} // namespace parser
} // namespace rohit::serializer
