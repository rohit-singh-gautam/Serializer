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

#include "schema_generics.hpp"
#include "schema_policy.hpp"
#include "schema_scan.hpp"
#include "schema_version.hpp"

#include <algorithm>
#include <array>
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

// Parse checked-expression syntax with bounded recursion and a per-expression node budget.
dimension_expression parse_dimension_expression(
    const rohit::type_check::schema_input_buffer auto& input, unsigned minimum = 0,
    std::size_t depth = 0, std::size_t* shared_count = nullptr,
    std::vector<dimension_expression> initial = {}) {
  constexpr std::size_t maximum_levels = 32, maximum_nodes = 256;
  std::size_t local_count{};
  auto& count = shared_count ? *shared_count : local_count;
  if (depth >= maximum_levels || ++count > maximum_nodes) {
    throw exception::bad_member_type{input, "Dimension expression resource limit (32 levels, 256 nodes) exceeded"};
  }
  skip_whitespace_and_comment(input);
  dimension_expression result{};
  result.source_offset = input.current_offset();
  if (!initial.empty()) {
    result = std::move(initial.front());
  } else if (!input.full() && *input == '(') {
    ++input;
    result = parse_dimension_expression(input, 0, depth + 1, &count);
    check_and_increase(input, ')');
  } else if (!input.full() && *input >= '0' && *input <= '9') {
    while (!input.full() && *input >= '0' && *input <= '9') {
      const auto digit = static_cast<std::uint64_t>(*input - '0');
      if (result.value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
        throw exception::bad_member_type{input, "Dimension literal overflow"};
      }
      result.value = result.value * 10 + digit;
      ++input;
    }
  } else if (!input.full() && *input == '-') {
    throw exception::bad_member_type{input, "Invalid dimension: negative values are forbidden"};
  } else {
    result.kind = dimension_expression::operation::parameter;
    result.name = parse_identifier_impl(input);
  }
  skip_whitespace_and_comment(input);
  while (!input.full() && (*input == '+' || *input == '*')) {
    const auto precedence = *input == '*' ? 2u : 1u;
    if (precedence < minimum) { break; }
    if (++count > maximum_nodes) {
      throw exception::bad_member_type{input, "Dimension expression node limit (256) exceeded"};
    }
    dimension_expression operation{};
    operation.source_offset = input.current_offset();
    operation.kind = *input == '*' ? dimension_expression::operation::multiply
                                  : dimension_expression::operation::add;
    ++input;
    operation.operands.push_back(std::move(result));
    operation.operands.push_back(parse_dimension_expression(input, precedence + 1, depth + 1, &count));
    result = std::move(operation);
  }
  return result;
}

// Parse bounded, nested schema type arguments without consuming the following member name.
type_name parse_type_expression(const rohit::type_check::schema_input_buffer auto& input,
                                namespace_node* scope, std::size_t depth = 0) {
  constexpr std::size_t maximum_type_depth = 32;
  if (depth >= maximum_type_depth) {
    throw exception::bad_member_type{input, "Maximum generic type depth (32) exceeded"};
  }
  if (!input.full() && ((*input >= '0' && *input <= '9') || *input == '(' || *input == '-')) {
    type_name value{"", scope};
    value.source_offset = input.current_offset();
    value.kind = generic_argument_kind::dimension;
    value.expression.push_back(parse_dimension_expression(input));
    return value;
  }
  const auto source_offset = input.current_offset();
  type_name result{parse_hierarchical_identifier_impl(input), scope};
  result.source_offset = source_offset;
  skip_whitespace_and_comment(input);
  if (!input.full() && *input == '<') {
    result.application = true;
    ++input;
    skip_whitespace_and_comment(input);
    while (!input.full() && *input != '>') {
      skip_whitespace_and_comment(input);
      result.arguments.push_back(parse_type_expression(input, scope, depth + 1));
      if (input.full() || *input != ',') { break; }
      ++input;
      if (!input.full() && *input == '>') {
        throw exception::bad_member_type{input, "Missing generic argument after comma"};
      }
    }
    check_and_increase(input, '>');
    skip_whitespace_and_comment(input);
  }
  if (!input.full() && (*input == '+' || *input == '*')) {
    dimension_expression parameter{};
    parameter.kind = dimension_expression::operation::parameter;
    parameter.name = result.name;
    parameter.source_offset = input.current_offset();
    result.kind = generic_argument_kind::dimension;
    result.expression.push_back(parse_dimension_expression(input, 0, 0, nullptr, {parameter}));
  }
  return result;
}

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
    auto type = parse_type_expression(in_stream, declared_namespace);
    skip_whitespace_and_comment(in_stream);
    if (*in_stream == '=') {
      ++in_stream;
      skip_whitespace_and_comment(in_stream);
      auto enum_name = parse_identifier_impl(in_stream);
      type.enum_name = std::move(enum_name);
      skip_whitespace_and_comment(in_stream);
    } else {
      std::string enum_name{"e_" + std::to_string(count)};
      type.enum_name = std::move(enum_name);
      ++count;
    }
    type_name_list.push_back(std::move(type));
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
  type_name_list.push_back(parse_type_expression(in_stream, declared_namespace));
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

// Inspect one whole keyword without consuming it or matching a longer identifier.
bool next_keyword(const rohit::type_check::schema_input_buffer auto& input, std::string_view word) {
  if (input.full()) {
    return false;
  }
  const auto size =
      detail::scan_prefix<detail::scan_kind::identifier>(input.curr(), input.remaining_buffer());
  return std::string_view{reinterpret_cast<const char*>(input.curr()), size} == word;
}

// Read one lifecycle literal or source member name inside parentheses.
std::string lifecycle_argument(const rohit::type_check::schema_input_buffer auto& input) {
  check_and_increase(input, '(');
  const auto* begin = input.curr();
  while (!input.full() && *input != ')') {
    ++input;
  }
  const auto text = schema_version::trim(std::string_view{
      reinterpret_cast<const char*>(begin), static_cast<std::size_t>(input.curr() - begin)});
  check_and_increase(input, ')');
  if (text.empty()) {
    throw exception::bad_member_spec{input, "Empty lifecycle argument"};
  }
  return text;
}

constexpr std::size_t maximum_policy_depth = 32;
constexpr std::size_t maximum_policy_nodes = 4096;
constexpr std::size_t maximum_version_releases = 4096;
constexpr std::size_t maximum_policy_literal_bytes = 128;

// Bound policy punctuation reads so truncated metadata produces a schema diagnostic.
void policy_character(const rohit::type_check::schema_input_buffer auto& input, char expected) {
  skip_whitespace_and_comment(input);
  if (input.full() || *input != expected) {
    throw exception::bad_member_spec{input, std::string{"Expected release-policy punctuation: "} +
                                                expected};
  }
  ++input;
}

// Preserve one scalar or quoted literal, stopping before comments and metadata punctuation.
std::string policy_literal(const rohit::type_check::schema_input_buffer auto& input) {
  skip_whitespace_and_comment(input);
  std::string value{};
  bool quoted{};
  bool escaped{};
  while (!input.full()) {
    const auto ch = static_cast<char>(*input);
    if (!quoted && (is_whitespace(ch) || ch == '{' || ch == '}' || ch == ';' || input == "//" ||
                    input == "/*")) {
      break;
    }
    if (value.size() >= maximum_policy_literal_bytes) {
      throw exception::bad_member_spec{input, "Release-policy literal exceeds 128 bytes"};
    }
    value.push_back(ch);
    ++input;
    if (quoted) {
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        quoted = false;
        break;
      }
    } else if (ch == '"' && value.size() == 1) {
      quoted = true;
    }
  }
  if (value.empty() || quoted) {
    throw exception::bad_member_spec{input, "Missing or unterminated release-policy literal"};
  }
  skip_whitespace_and_comment(input);
  return value;
}

// Read one bounded tree expression; all groups and leaves end with a semicolon.
version_policy parse_policy_node(const rohit::type_check::schema_input_buffer auto& input,
                                 std::size_t depth, std::size_t& nodes) {
  if (depth >= maximum_policy_depth || nodes >= maximum_policy_nodes) {
    throw exception::bad_member_spec{input, "Release policy exceeds its nesting or node limit"};
  }
  ++nodes;
  const auto name = parse_identifier_impl(input);
  version_policy result{};
  if (name == "any") {
    result.kind = version_policy_kind::any;
  } else if (name == "all") {
    result.kind = version_policy_kind::all;
  } else if (name == "max_age") {
    result.kind = version_policy_kind::max_age;
  } else if (name == "keep_last") {
    result.kind = version_policy_kind::keep_last;
  } else if (name == "released_since") {
    result.kind = version_policy_kind::released_since;
  } else if (name == "expires_on") {
    result.kind = version_policy_kind::expires_on;
  } else if (name == "compatibility") {
    result.kind = version_policy_kind::compatibility;
  } else {
    throw exception::bad_member_spec{input, "Unknown release policy: " + name};
  }
  policy_character(input, '{');
  skip_whitespace_and_comment(input);
  if (result.kind == version_policy_kind::any || result.kind == version_policy_kind::all) {
    while (!input.full() && *input != '}') {
      result.children.push_back(parse_policy_node(input, depth + 1, nodes));
      skip_whitespace_and_comment(input);
    }
    if (result.children.empty()) {
      throw exception::bad_member_spec{input, "Policy groups must not be empty"};
    }
  } else {
    result.argument = policy_literal(input);
    if (result.kind == version_policy_kind::max_age) {
      result.argument += " " + parse_identifier_impl(input);
    }
  }
  policy_character(input, '}');
  policy_character(input, ';');
  skip_whitespace_and_comment(input);
  return result;
}

// Read the ordered release catalog without allocating runtime or generated-language metadata.
std::vector<version_release>
parse_releases(const rohit::type_check::schema_input_buffer auto& input) {
  policy_character(input, '{');
  skip_whitespace_and_comment(input);
  std::vector<version_release> releases{};
  while (!input.full() && *input != '}') {
    if (releases.size() >= maximum_version_releases) {
      throw exception::bad_member_spec{input, "Release catalog exceeds 4096 entries"};
    }
    auto version = policy_literal(input);
    policy_character(input, '{');
    auto date = policy_literal(input);
    policy_character(input, '}');
    policy_character(input, ';');
    releases.push_back({std::move(version), std::move(date)});
    skip_whitespace_and_comment(input);
  }
  if (releases.empty()) {
    throw exception::bad_member_spec{input, "Release catalog must not be empty"};
  }
  policy_character(input, '}');
  return releases;
}

// Normalize output exclusions once so backends receive only canonical format names.
std::vector<std::string> parse_omitted_formats(const rohit::type_check::schema_input_buffer auto& input) {
  parse_identifier_impl(input);
  skip_whitespace_and_comment(input);
  check_and_increase(input, '(');
  std::vector<std::string> result{};
  do {
    skip_whitespace_and_comment(input);
    auto format = parse_identifier_impl(input);
    if (format == "binary_positional") { format = "binary_none"; }
    if (format == "protobuf_binary") { format = "protobuf"; }
    if (format != "json" && format != "binary_none" && format != "binary_integer" &&
        format != "binary_string" && format != "protobuf" && format != "protojson" && format != "textproto") {
      throw exception::bad_member_spec{input, "Unsupported omitted output format: " + format};
    }
    if (std::find(result.begin(), result.end(), format) != result.end()) {
      throw exception::bad_member_spec{input, "Repeated omitted output format"};
    }
    result.push_back(std::move(format));
    skip_whitespace_and_comment(input);
    if (*input != ',') { break; }
    ++input;
  } while (true);
  check_and_increase(input, ')');
  return result;
}

// Parse field visibility, storage, wire identity, and optional version lifecycle metadata.
member parse_member_impl(const rohit::type_check::schema_input_buffer auto& in_stream,
                         const std::uint32_t id, namespace_node* declared_namespace) {
  member lifecycle{};
  skip_whitespace_and_comment(in_stream);
  while (next_keyword(in_stream, "obsolete") || next_keyword(in_stream, "created") ||
         next_keyword(in_stream, "replaced")) {
    const auto keyword = parse_identifier_impl(in_stream);
    skip_whitespace_and_comment(in_stream);
    if (keyword == "obsolete") {
      if (lifecycle.obsolete) {
        throw exception::bad_member_spec{in_stream, "Repeated obsolete"};
      }
      lifecycle.obsolete = true;
      if (!in_stream.full() && *in_stream == '(') {
        lifecycle.obsolete_version = lifecycle_argument(in_stream);
      }
    } else {
      auto& value = keyword == "created" ? lifecycle.created_version : lifecycle.replaced_member;
      if (!value.empty()) {
        throw exception::bad_member_spec{in_stream, "Repeated lifecycle keyword"};
      }
      value = lifecycle_argument(in_stream);
    }
    skip_whitespace_and_comment(in_stream);
  }
  auto access = parse_access_type_impl(in_stream);
  skip_whitespace_and_comment(in_stream);
  if (next_keyword(in_stream, "magic")) {
    if (lifecycle.obsolete || !lifecycle.created_version.empty() ||
        !lifecycle.obsolete_version.empty() || !lifecycle.replaced_member.empty()) {
      throw exception::bad_member_spec{in_stream, "Magic cannot have field lifecycle annotations"};
    }
    parse_identifier_impl(in_stream);
    skip_whitespace_and_comment(in_stream);
    std::uint32_t magic_id{1};
    std::string magic_name{"magic"};
    bool explicit_magic_id{};
    if (*in_stream == '(') {
      parse_name_spec(in_stream, magic_id, magic_name, explicit_magic_id);
      skip_whitespace_and_comment(in_stream);
      if (magic_name != "magic") {
        throw exception::bad_member_spec{in_stream, "Magic retains the fixed wire name magic"};
      }
    }
    const auto literal = get_default_value(in_stream);
    if (literal.size() < 3 || literal.front() != '\'' || literal.back() != '\'') {
      throw exception::bad_member_spec{in_stream, "Magic requires a nonempty single-quoted byte literal"};
    }
    std::string bytes{};
    constexpr std::size_t maximum_magic_bytes = 64;
    // Decode only portable byte escapes, keeping every generator independent of source syntax.
    for (std::size_t index = 1; index + 1 < literal.size(); ++index) {
      auto character = literal[index];
      if (character == '\\') {
        if (++index + 1 >= literal.size()) {
          throw exception::bad_member_spec{in_stream, "Incomplete magic byte escape"};
        }
        character = literal[index];
        switch (character) {
        case 'n': character = '\n'; break;
        case 'r': character = '\r'; break;
        case 't': character = '\t'; break;
        case '0': character = '\0'; break;
        case '\\': case '\'': case '"': break;
        case 'x': {
          if (index + 3 >= literal.size()) {
            throw exception::bad_member_spec{in_stream, "Magic hex escape requires exactly two digits"};
          }
          unsigned value{};
          const auto* first = literal.data() + index + 1;
          const auto parsed = std::from_chars(first, first + 2, value, 16);
          if (parsed.ec != std::errc{} || parsed.ptr != first + 2) {
            throw exception::bad_member_spec{in_stream, "Invalid magic hex byte escape"};
          }
          character = static_cast<char>(value);
          index += 2;
          break;
        }
        default:
          throw exception::bad_member_spec{in_stream, "Unsupported magic byte escape"};
        }
      } else if (character == '\'' || character == '\n' || character == '\r') {
        throw exception::bad_member_spec{in_stream, "Magic literals require escaped quotes and newlines"};
      }
      if (bytes.size() >= maximum_magic_bytes) {
        throw exception::bad_member_spec{in_stream, "Magic exceeds the 64-byte header bound"};
      }
      bytes.push_back(character);
    }
    skip_whitespace_and_comment(in_stream);
    std::vector<std::string> omitted{};
    if (next_keyword(in_stream, "omit")) {
      omitted = parse_omitted_formats(in_stream);
      skip_whitespace_and_comment(in_stream);
    }
    try {
      ::rohit::serializer::detail::validate_utf8({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    } catch (const std::invalid_argument&) {
      throw exception::bad_member_spec{in_stream, "Magic bytes must form valid UTF-8 for portable text output"};
    }
    check_and_increase(in_stream, ';');
    member result{access, member::modifier_type::none, {}, "magic", "magic", magic_id, {}, {}, explicit_magic_id};
    result.magic_bytes = std::move(bytes);
    result.omitted_formats = std::move(omitted);
    return result;
  }
  const bool version = next_keyword(in_stream, "version");
  if (version) {
    parse_identifier_impl(in_stream);
    skip_whitespace_and_comment(in_stream);
  }
  type_name next_type{std::string{"uint8"}, declared_namespace};
  if (!version || (is_first_identifier(in_stream) &&
                   schema_version::supported_type(
                       std::string_view{reinterpret_cast<const char*>(in_stream.curr()),
                                        detail::scan_prefix<detail::scan_kind::identifier>(
                                            in_stream.curr(), in_stream.remaining_buffer())}))) {
    next_type = parse_type_expression(in_stream, declared_namespace);
  }
  if (next_type.kind == generic_argument_kind::dimension) {
    throw exception::bad_identifier{in_stream, "Expected a member type identifier"};
  }
  const bool managed = next_type.name == "managed";
  if (managed) {
    skip_whitespace_and_comment(in_stream);
    if (*in_stream == '(') {
      throw exception::bad_member_spec{in_stream, "Managed feature selectors are not implemented; use bare managed for the C++ history profile"};
    }
    next_type = parse_type_expression(in_stream, declared_namespace);
  }
  std::vector<std::string> enum_name_list{};
  std::vector<type_name> type_name_list{};
  auto member_modifier = parse_member_modifier(next_type.name);
  if (member_modifier != member::modifier_type::none && !next_type.arguments.empty()) {
    throw exception::bad_member_type{in_stream, "Collection modifiers do not accept type arguments"};
  }
  std::string key{};
  std::vector<dimension_expression> extent{};
  if (member_modifier == member::modifier_type::none) {
    type_name_list.push_back(std::move(next_type));
  } else if (member_modifier == member::modifier_type::array) {
    skip_whitespace_and_comment(in_stream);
    if (!in_stream.full() && *in_stream == '[') {
      ++in_stream;
      extent.push_back(parse_dimension_expression(in_stream));
      check_and_increase(in_stream, ']');
      skip_whitespace_and_comment(in_stream);
    }
    type_name_list.push_back(parse_type_expression(in_stream, declared_namespace));
  } else if (member_modifier == member::modifier_type::map) {
    parse_member_type_map(in_stream, declared_namespace, type_name_list, key);
  } else if (member_modifier == member::modifier_type::variant) {
    parse_member_type_union(in_stream, declared_namespace, type_name_list);
  }
  skip_whitespace_and_comment(in_stream);
  auto name = version && !is_first_identifier(in_stream) ? std::string{"version"}
                                                         : parse_identifier_impl(in_stream);
  auto display_name = name;
  std::uint32_t new_id{version ? 1 : id};
  bool parsed_member_spec{false};
  bool explicit_id{false};
  bool parsed_default_value{false};
  std::string default_value{};
  std::string compatibility{};
  std::vector<version_release> releases{};
  std::optional<version_policy> policy{};
  std::vector<std::string> omitted{};
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
    } else if (next_keyword(in_stream, "compatibility")) {
      if (!version || !compatibility.empty()) {
        throw exception::bad_member_spec{in_stream,
                                         "Compatibility requires one version declaration"};
      }
      parse_identifier_impl(in_stream);
      skip_whitespace_and_comment(in_stream);
      compatibility = get_default_value(in_stream);
      if (compatibility.empty()) {
        throw exception::bad_member_spec{in_stream, "Compatibility requires a version literal"};
      }
    } else if (next_keyword(in_stream, "releases")) {
      if (!version || !releases.empty()) {
        throw exception::bad_member_spec{in_stream, "Releases require one version declaration"};
      }
      parse_identifier_impl(in_stream);
      releases = parse_releases(in_stream);
    } else if (next_keyword(in_stream, "omit")) {
      if (version || !omitted.empty()) {
        throw exception::bad_member_spec{in_stream, "Version cannot omit formats; only one omit declaration is allowed"};
      }
      omitted = parse_omitted_formats(in_stream);
    } else if (next_keyword(in_stream, "policy")) {
      if (!version || policy) {
        throw exception::bad_member_spec{in_stream, "Policy requires one version declaration"};
      }
      parse_identifier_impl(in_stream);
      policy_character(in_stream, '{');
      skip_whitespace_and_comment(in_stream);
      std::size_t nodes{};
      policy = parse_policy_node(in_stream, 0, nodes);
      policy_character(in_stream, '}');
    } else {
      break;
    }
  }
  skip_whitespace_and_comment(in_stream);
  check_and_increase(in_stream, ';');
  validate_field_key(in_stream, new_id, display_name);
  member result{access, member_modifier, type_name_list, name, display_name, new_id, key, default_value,
                explicit_id, nullptr, managed};
  result.extent_expression = std::move(extent);
  result.version = version;
  result.omitted_formats = std::move(omitted);
  result.compatibility_version = std::move(compatibility);
  result.releases = std::move(releases);
  result.policy = std::move(policy);
  result.obsolete = lifecycle.obsolete;
  result.created_version = std::move(lifecycle.created_version);
  result.obsolete_version = std::move(lifecycle.obsolete_version);
  result.replaced_member = std::move(lifecycle.replaced_member);
  return result;
} // parse_member_impl

// Reserve unused identities at class scope; every clause and entry must be unique.
void parse_reserve(const rohit::type_check::schema_input_buffer auto& input, class_node& object) {
  parse_identifier_impl(input);
  std::unordered_set<std::string> clauses{};
  skip_whitespace_and_comment(input);
  while (!input.full() && *input != ';') {
    const auto clause = parse_identifier_impl(input);
    if (!clauses.insert(clause).second ||
        (clause != "id" && clause != "variable" && clause != "display")) {
      throw exception::bad_member_spec{input, "Invalid or repeated reservation clause"};
    }
    skip_whitespace_and_comment(input);
    check_and_increase(input, '{');
    skip_whitespace_and_comment(input);
    if (*input == '}') {
      throw exception::bad_member_spec{input, "Empty reservation list"};
    }
    while (true) {
      if (clause == "variable") {
        object.reserved_variables.push_back(parse_identifier_impl(input));
      } else {
        auto [text, quoted] = get_member_spec_token(input);
        if (clause == "display") {
          if (!quoted || text.empty()) {
            throw exception::bad_member_spec{input, "Expected a nonempty reserved wire name"};
          }
          object.reserved_names.push_back(std::move(text));
        } else {
          std::uint32_t value{};
          const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
          if (quoted || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
            throw exception::bad_member_spec{input, "Expected a reserved field ID"};
          }
          validate_field_key(input, value, "reserved");
          object.reserved_ids.push_back(value);
        }
      }
      skip_whitespace_and_comment(input);
      if (*input != ',') {
        break;
      }
      ++input;
      skip_whitespace_and_comment(input);
    }
    check_and_increase(input, '}');
    skip_whitespace_and_comment(input);
  }
  if (clauses.empty()) {
    throw exception::bad_member_spec{input, "Empty reservation"};
  }
  check_and_increase(input, ';');
}

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
  if (object_type == "instantiate") {
    return object_type::instantiation;
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
    if (next_keyword(in_stream, "reserve")) {
      parse_reserve(in_stream, *obj);
      skip_whitespace_and_comment(in_stream);
      continue;
    }
    auto member = parse_member_impl(in_stream, id++, obj->parent_namespace);
    if (!member.magic_bytes.empty()) {
      if (!obj->magic_bytes.empty()) {
        throw exception::bad_member_spec{in_stream, "Only one magic declaration is allowed per class"};
      }
      obj->magic_bytes = std::move(member.magic_bytes);
      obj->magic_access = member.access;
      obj->magic_id = member.id;
      obj->magic_explicit_id = member.explicit_id;
      obj->magic_omitted_formats = std::move(member.omitted_formats);
      --id; // Header metadata does not consume an ordinary schema field identity.
      skip_whitespace_and_comment(in_stream);
      continue;
    }
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
  std::vector<std::string> parameters{};
  std::vector<generic_parameter> parameter_specs{};
  bool has_default = false;
  if (!in_stream.full() && *in_stream == '<') {
    ++in_stream;
    do {
      skip_whitespace_and_comment(in_stream);
      auto parameter = parse_identifier_impl(in_stream);
      generic_parameter spec{};
      if (parameter == "uint64") {
        spec.kind = generic_argument_kind::dimension;
        skip_whitespace_and_comment(in_stream);
        parameter = parse_identifier_impl(in_stream);
      }
      constexpr std::string_view reserved_parameters[] = {
          "array", "map", "union", "managed", "class", "namespace", "enum", "instantiate",
          "serializer", "version", "include", "public", "protected", "private", "owning",
          "readonly", "mutable", "view", "stable_ids", "packed"};
      if (std::find(parameters.begin(), parameters.end(), parameter) != parameters.end() ||
          std::find(std::begin(reserved_parameters), std::end(reserved_parameters), parameter) !=
              std::end(reserved_parameters) ||
          !serializer::get_cpp_type_or_empty(parameter).empty() || parameter == name) {
        throw exception::bad_class{in_stream, "Duplicate or reserved generic parameter: " + parameter};
      }
      spec.name = parameter;
      parameters.push_back(std::move(parameter));
      skip_whitespace_and_comment(in_stream);
      if (!in_stream.full() && *in_stream == '=') {
        ++in_stream;
        skip_whitespace_and_comment(in_stream);
        spec.default_argument.push_back(parse_type_expression(in_stream, current_namespace));
        has_default = true;
      } else if (has_default) {
        throw exception::bad_class{in_stream, "Invalid default: all trailing parameters require defaults"};
      }
      parameter_specs.push_back(std::move(spec));
      if (in_stream.full() || *in_stream != ',') { break; }
      ++in_stream;
    } while (true);
    check_and_increase(in_stream, '>');
    skip_whitespace_and_comment(in_stream);
  }
  auto attributes{class_attributes::none};
  bool view{}, owning{}, readonly{}, mutable_view{};
  space_separated_identifier_impl(in_stream, [&](std::string&& value) {
    if (value == "packed") {
      attributes |= class_attributes::packed;
    } else if (value == "stable_ids") {
      attributes |= class_attributes::stable_ids;
    } else if (value == "managed") {
      attributes |= class_attributes::managed;
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
  result->type_parameters = std::move(parameters);
  result->generic_parameters = std::move(parameter_specs);
  return result;
}

// Reject ambiguous field identity, including collisions between explicit and implicit IDs.
void validate_class_keys(const rohit::type_check::schema_input_buffer auto& input,
                         class_node& obj) {
  // A default discriminator ID uses the first free identity, independent of declaration order.
  std::unordered_set<std::uint32_t> occupied(obj.reserved_ids.begin(), obj.reserved_ids.end());
  for (const auto& base : obj.parents) {
    occupied.insert(base.id);
  }
  if (!obj.magic_bytes.empty() && obj.magic_explicit_id) {
    occupied.insert(obj.magic_id);
  }
  for (const auto& field : obj.member_list) {
    if (!field.version || field.explicit_id) {
      occupied.insert(field.id);
    }
  }
  for (auto& field : obj.member_list) {
    if (field.version && !field.explicit_id) {
      while (occupied.contains(field.id)) {
        ++field.id;
      }
      validate_field_key(input, field.id, field.display_name);
      occupied.insert(field.id);
    }
  }
  if (!obj.magic_bytes.empty() && !obj.magic_explicit_id) {
    while (occupied.contains(obj.magic_id)) { ++obj.magic_id; }
    occupied.insert(obj.magic_id);
  }
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
  for (const auto id : obj.reserved_ids) {
    add_id(id, true);
  }
  for (const auto& name : obj.reserved_names) {
    add_name(name);
  }
  if (!obj.magic_bytes.empty()) {
    add_id(obj.magic_id, true);
    add_name("magic");
  }
  std::unordered_set<std::string> variables{};
  for (const auto& name : obj.reserved_variables) {
    if (!variables.insert(name).second) {
      throw exception::bad_member_spec{input, "Duplicate reserved variable name"};
    }
  }
  for (const auto& base : obj.parents) {
    add_id(base.id, base.explicit_id);
    add_name(base.display_name);
  }
  for (const auto& field : obj.member_list) {
    if (!variables.insert(field.name).second) {
      throw exception::bad_member_spec{input, "Duplicate or reserved variable name"};
    }
    add_id(field.id, field.explicit_id || field.version);
    if (field.modifier == member::modifier_type::variant) {
      if (std::find(obj.reserved_names.begin(), obj.reserved_names.end(), field.display_name) !=
          obj.reserved_names.end()) {
        throw exception::bad_member_spec{input, "Union reuses a reserved wire name"};
      }
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
  const auto* version = obj.version_member();
  if (!version) {
    for (const auto& field : obj.member_list) {
      if (!field.created_version.empty() || !field.obsolete_version.empty() ||
          !field.replaced_member.empty()) {
        throw exception::bad_member_spec{input, "Version lifecycle requires a version declaration"};
      }
    }
    return;
  }
  try {
    const auto& type = version->type_name_list.front().name;
    const auto current = schema_version::parse(type, version->default_value);
    const auto minimum = schema_version::parse(type, schema_version::minimum(*version));
    if (current < minimum) {
      throw std::invalid_argument{"Compatibility exceeds current version"};
    }
    std::size_t count{};
    for (const auto& field : obj.member_list) {
      if (field.version) {
        ++count;
        if (field.obsolete || !field.created_version.empty() || !field.replaced_member.empty()) {
          throw std::invalid_argument{"Version discriminator cannot have a lifecycle"};
        }
      }
      for (const auto& boundary : {field.created_version, field.obsolete_version}) {
        if (!boundary.empty() && current < schema_version::parse(type, boundary)) {
          throw std::invalid_argument{"Lifecycle boundary exceeds current version"};
        }
      }
      if (!field.created_version.empty() && !field.obsolete_version.empty() &&
          !(schema_version::parse(type, field.created_version) <
            schema_version::parse(type, field.obsolete_version))) {
        throw std::invalid_argument{"Created version must precede obsolete version"};
      }
      if (!field.replaced_member.empty()) {
        const auto predecessor = std::find_if(
            obj.member_list.begin(), obj.member_list.end(),
            [&](const auto& candidate) { return candidate.name == field.replaced_member; });
        if (field.created_version.empty() || predecessor == obj.member_list.end() ||
            predecessor->obsolete_version.empty() ||
            !(schema_version::parse(type, field.created_version) ==
              schema_version::parse(type, predecessor->obsolete_version))) {
          throw std::invalid_argument{"Replacement must match an existing obsolete boundary"};
        }
        if (std::count_if(obj.member_list.begin(), obj.member_list.end(),
                          [&](const auto& candidate) {
                            return candidate.replaced_member == field.replaced_member;
                          }) != 1) {
          throw std::invalid_argument{"A field may have only one replacement"};
        }
      }
    }
    if (count != 1) {
      throw std::invalid_argument{"A class must have exactly one version declaration"};
    }
    if (obj.storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        obj.supports_managed()) {
      throw std::invalid_argument{"Version lifecycle currently requires unmanaged owning classes"};
    }
  } catch (const std::invalid_argument& error) {
    throw exception::bad_member_spec{input, error.what()};
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
  if (obj->storage_modes != static_cast<std::uint8_t>(storage_mode::owning) &&
      std::any_of(obj->member_list.begin(), obj->member_list.end(), [](const auto& field) {
        return !field.omitted_formats.empty();
      })) {
    throw exception::bad_class{in_stream, "Output omissions currently require an owning class without view modes"};
  }
  if (!obj->magic_bytes.empty()) {
    if (obj->storage_modes != static_cast<std::uint8_t>(storage_mode::owning) ||
        obj->supports_managed()) {
      throw exception::bad_class{in_stream, "Binary magic requires an unmanaged owning class"};
    }
    if (std::any_of(obj->member_list.begin(), obj->member_list.end(), [](const auto& field) {
          return field.name == "magic";
        })) {
      throw exception::bad_member_spec{in_stream, "The magic constant conflicts with an ordinary field"};
    }
  }
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
    } else if (object_type == object_type::instantiation) {
      skip_whitespace_and_comment(in_stream);
      auto name = parse_identifier_impl(in_stream);
      skip_whitespace_and_comment(in_stream);
      check_and_increase(in_stream, '=');
      skip_whitespace_and_comment(in_stream);
      auto node = std::make_unique<class_node>(object_type::class_type, std::move(name),
          parent_namespace, class_attributes::none, std::vector<parent>{});
      node->instance_of.push_back(parse_type_expression(in_stream, parent_namespace));
      check_and_increase(in_stream, ';');
      register_declaration(in_stream, *node, declarations);
      statements.push_back(std::move(node));
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
  if (type.type != object_type::unresolved) { return; }
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
    try {
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
            if (member.managed &&
                (member.modifier == member::modifier_type::variant || !type.resolved_node ||
                 type.resolved_node->type != object_type::class_type ||
                 !static_cast<const class_node*>(type.resolved_node)->supports_managed())) {
              throw exception::bad_member_type{in_stream,
                  "managed members require a class declared managed or containing its own managed member"};
            }
          }
          if (member.modifier == member::modifier_type::map) {
          type_name key{std::string{member.key}, member.type_name_list.front().declared_namespace};
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
    } catch (const std::exception& error) {
      if (statement->source_path.empty()) { throw; }
      throw std::invalid_argument{statement->source_path + ": " + error.what()};
    }
  }
}

// Consume the leading version statement before any declarations; comments may precede it.
// Bound every read so incomplete headers report schema errors instead of stream overflow.
void parse_version_header(const rohit::type_check::schema_input_buffer auto& input, bool required) {
  skip_whitespace_and_comment(input);
  if (!(input == "serializer")) {
    if (required) {
      throw exception::bad_input_data{input, "Expected first statement: serializer version 1.0.0;"};
    }
    return;
  }
  for (const auto token : {std::string_view{"serializer"}, std::string_view{"version"}}) {
    if (!(input == token)) {
      throw exception::bad_input_data{input, "Expected header: serializer version 1.0.0;"};
    }
    input += token.size();
    if (!input.full() && is_identifier(*input)) {
      throw exception::bad_input_data{input, "Expected separated header tokens"};
    }
    skip_whitespace_and_comment(input);
  }
  // Read each bounded decimal component without arithmetic overflow or unbounded lookahead.
  const auto read_component = [&input]() {
    const auto* begin = reinterpret_cast<const char*>(input.curr());
    std::size_t digits{};
    while (!input.full() && is_number(*input)) {
      ++input;
      ++digits;
    }
    unsigned component{};
    if (digits == 0) {
      throw exception::bad_input_data{input, "Expected decimal schema language version component"};
    }
    const auto converted = std::from_chars(begin, begin + digits, component);
    if (converted.ec != std::errc{}) {
      throw exception::bad_input_data{input, "Schema language version component is out of range"};
    }
    return component;
  };
  constexpr std::array supported_version{schema_language_version_major,
                                         schema_language_version_minor,
                                         schema_language_version_patch};
  constexpr std::array legacy_version{1u, 0u, 0u};
  std::array<unsigned, supported_version.size()> version{};
  version.front() = read_component();
  if (!input.full() && *input == '.') {
    for (std::size_t component = 1; component < version.size(); ++component) {
      if (input.full() || *input != '.') {
        throw exception::bad_input_data{input, "Expected schema language version major.minor.patch"};
      }
      ++input;
      version[component] = read_component();
    }
  } else if (version != legacy_version) {
    // Only the original version 1 has an integer alias, even after future major releases.
    throw exception::bad_input_data{
      input, "Expected major.minor.patch; only schema language version 1 aliases 1.0.0"};
  }
  if (version.front() != supported_version.front() || version > supported_version) {
    throw exception::bad_input_data{
      input, "Unsupported schema language version; supported: " +
               std::string{schema_language_version_text}};
  }
  skip_whitespace_and_comment(input);
  if (input.full() || *input != ';') {
    throw exception::bad_input_data{input, "Expected ';' after schema language version"};
  }
  ++input;
}

namespace {
constexpr std::string_view schema_extension{".serializer"};

// Recognize an include keyword without accepting identifiers beginning with that spelling.
bool starts_include(const rohit::type_check::schema_input_buffer auto& input) {
  constexpr std::string_view keyword{"include"};
  return input == keyword && (input.remaining_buffer() == keyword.size() ||
                              !is_identifier(static_cast<char>(input.curr()[keyword.size()])));
}

// Resolve a portable unquoted include to its schema filename without probing the filesystem.
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
  std::filesystem::path path{value};
  const auto filename = path.filename();
  const auto extension = path.extension();
  if (value.empty() || path.is_absolute() || path.has_root_path() || filename.empty() ||
      filename == "." || filename == ".." ||
      (!extension.empty() && extension != schema_extension)) {
    throw exception::bad_input_data{
        input, "Expected an unquoted relative path with no extension or .serializer after include"};
  }
  skip_whitespace_and_comment(input);
  if (input.full() || *input != ';') {
    throw exception::bad_input_data{
        input, "Expected ';' after include path (use forward slashes, without quotes or spaces)"};
  }
  ++input;
  if (extension.empty()) {
    path += schema_extension;
  }
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
      if (path.extension() != schema_extension) {
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
      const auto locate = [&](const auto& self, auto& nodes) -> void {
        for (auto& node : nodes) {
          node->source_path = canonical.generic_string();
          if (node->type == object_type::namespace_type) {
            self(self, static_cast<namespace_node&>(*node).statements);
          }
        }
      };
      locate(locate, statements);
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
  parsed_schema run(const std::filesystem::path& path, const parse_options& options) {
    load(path, 0);
    try {
      const auto input = rohit::make_stream_from_file(path);
      schema_policy::resolve(result.statements, options);
      lower_generics(input, result.statements);
      resolve_member(input, result.statements, types);
    } catch (const std::exception& error) {
      throw std::invalid_argument{path.generic_string() + ": " + error.what()};
    }
    return std::move(result);
  }
};
} // namespace

// Give filesystem-aware callers an isolated include cache and symbol table per entry schema.
parsed_schema parse_file(const std::filesystem::path& path, const parse_options& options) {
  const auto captured = schema_policy::capture_options(options);
  return file_loader{}.run(path, captured);
}

// Parse schema declarations and resolve their member types; malformed input throws.
std::vector<std::unique_ptr<syntax_node>>
parse_schema(const rohit::type_check::schema_input_buffer auto& in_stream, bool require_version,
             const parse_options& options) {
  parse_version_header(in_stream, require_version);
  std::unordered_map<std::string, syntax_node*> declarations{};
  auto statements = parse_statement_list(in_stream, nullptr, declarations);
  if (!in_stream.full()) {
    throw exception::bad_input_data{in_stream, "Unexpected trailing schema input"};
  }
  std::unordered_map<std::string, syntax_node*> variable_type_map;
  schema_policy::resolve(statements, options);
  lower_generics(in_stream, statements);
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
                                                      std::size_t& consumed, bool require_version,
                                                      const parse_options& options) {
  const auto captured = schema_policy::capture_options(options);
  return scan_bytes(bytes, consumed, [require_version, &captured](const auto& input) {
    return parse_schema(input, require_version, captured);
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
