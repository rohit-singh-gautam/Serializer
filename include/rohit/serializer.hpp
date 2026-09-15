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

#pragma once
#include <rohit/stream.hpp>

#include <bit>
#include <cctype>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::serializer {

namespace constants {
// The first byte stores a two-bit length tag and six payload bits.
inline constexpr std::uint32_t variable_tag_mask = 0xc0U;
inline constexpr std::uint32_t variable_payload_mask = 0x3fU;
inline constexpr std::uint32_t variable_two_byte_tag = 0x40U;
inline constexpr std::uint32_t variable_three_byte_tag = 0x80U;
inline constexpr std::uint32_t variable_four_byte_tag = 0xc0U;
// Keep threshold types consistent with the original integral comparisons.
inline constexpr int variable_one_byte_max = 0x3f;
inline constexpr int variable_two_byte_max = 0x3fff;
inline constexpr int variable_three_byte_max = 0x3fffff;
inline constexpr int variable_four_byte_max = 0x3fffffff;
// Integer-key objects end with ID zero; string-key objects use a zero-length name.
inline constexpr std::uint32_t binary_object_end_id = 0;
inline constexpr std::uint32_t wire_byte_mask = 0xffU;
inline constexpr unsigned wire_byte_bits = 8;
inline constexpr int decimal_radix = 10;
inline constexpr std::string_view map_key_name = "key";
inline constexpr std::string_view map_value_name = "value";
static_assert(variable_payload_mask == variable_one_byte_max);
static_assert((variable_tag_mask & variable_payload_mask) == 0);
} // namespace constants
namespace exception {
using rohit::exception::base_parser;

class bad_input_data : public base_parser {
public:
  using base_parser::base_parser;
};

class bad_type : public base_parser {
public:
  using base_parser::base_parser;
};

class unknown_serialization_type : public base_parser {
public:
  using base_parser::base_parser;
};

class key_not_found : public base_parser {
public:
  using base_parser::base_parser;
};
} // namespace exception

namespace type_check {
template <typename T, typename J>
concept serializer_in_enabled = requires(T cls, J& serialize_protocol) {
  { cls->template serialize_in<J>(serialize_protocol) } -> std::same_as<void>;
};

template <typename T, typename J>
concept serializer_out_enabled_ptr = requires(T cls, J& serialize_protocol) {
  { cls->template serialize_out<J>(serialize_protocol) } -> std::same_as<void>;
};

template <typename T, typename J>
concept serializer_out_enabled = requires(T cls, J& serialize_protocol) {
  { cls.template serialize_out<J>(serialize_protocol) } -> std::same_as<void>;
};

template <typename T>
concept vector = requires(T t) {
  typename T::value_type;
  requires std::is_same_v<T, std::vector<typename T::value_type>>;
};

template <typename T>
concept map = requires(T t) {
  typename T::key_type;
  typename T::mapped_type;
  requires std::is_same_v<T, std::map<typename T::key_type, typename T::mapped_type>>;
};

template <typename T>
concept functions = requires(T t) {
  requires std::is_same_v<T, void(stream&)> || std::is_function_v<T> ||
               std::is_same_v<T, std::function<void(stream&)>>;
};

} // namespace type_check

struct write_format {
  bool newline_before_braces_open{false};
  bool newline_after_braces_open{false};
  bool newline_before_braces_close{false};
  bool newline_after_braces_close{false};
  bool newline_before_bracket_open{false};
  bool newline_after_bracket_open{false};
  bool newline_before_bracket_close{false};
  bool newline_after_bracket_close{false};
  bool newline_before_object_member{false};
  bool space_after_comma{false}; // Space will not be added if newline is enabled
  bool newline_after_comma{false};
  bool space_after_colon{false};
  std::string_view indent_text{};
};

namespace format {
static constexpr write_format compress{};

static constexpr write_format beautify{.newline_after_braces_open = true,
                                       .newline_before_braces_close = true,
                                       .newline_after_bracket_open = true,
                                       .newline_before_bracket_close = true,
                                       .newline_before_object_member = true,
                                       .space_after_comma = true,
                                       .space_after_colon = true,
                                       .indent_text = {"  "}};

static constexpr write_format beautify_vertical{.newline_before_braces_open = true,
                                                .newline_after_braces_open = true,
                                                .newline_before_braces_close = true,
                                                .newline_before_bracket_open = true,
                                                .newline_after_bracket_open = true,
                                                .newline_before_bracket_close = true,
                                                .newline_before_object_member = true,
                                                .space_after_comma = true,
                                                .newline_after_comma = true,
                                                .space_after_colon = true,
                                                .indent_text = {"  "}};
} // namespace format

template <typename TypeEnum, typename BaseType, typename T0>
// Select the concrete pointer type associated with the supplied discriminator.
auto type_cast(TypeEnum type, BaseType* ptr) {
  return reinterpret_cast<T0*>(ptr);
}
template <typename TypeEnum, typename BaseType, typename T0, typename T1>
// Select the concrete pointer type associated with the supplied discriminator.
auto type_cast(TypeEnum type, BaseType* ptr) {
  switch (reinterpret_cast<std::underlying_type<TypeEnum>>(type)) {
  default:
  case 0:
    return reinterpret_cast<T0*>(ptr);
  case 1:
    return reinterpret_cast<T1*>(ptr);
  }
}
template <typename TypeEnum, typename BaseType, typename T0, typename T1, typename T2>
// Select the concrete pointer type associated with the supplied discriminator.
auto type_cast(TypeEnum type, BaseType* ptr) {
  switch (reinterpret_cast<std::underlying_type<TypeEnum>>(type)) {
  default:
  case 0:
    return reinterpret_cast<T0*>(ptr);
  case 1:
    return reinterpret_cast<T1*>(ptr);
  case 2:
    return reinterpret_cast<T2*>(ptr);
  }
}
template <typename TypeEnum, typename BaseType, typename T0, typename T1, typename T2, typename T3>
// Select the concrete pointer type associated with the supplied discriminator.
auto type_cast(TypeEnum type, BaseType* ptr) {
  switch (reinterpret_cast<std::underlying_type<TypeEnum>>(type)) {
  default:
  case 0:
    return reinterpret_cast<T0*>(ptr);
  case 1:
    return reinterpret_cast<T1*>(ptr);
  case 2:
    return reinterpret_cast<T2*>(ptr);
  case 3:
    return reinterpret_cast<T3*>(ptr);
  }
}

template <typename TypeEnum, typename BaseType, typename T, typename... TArr>
// Select the concrete pointer type associated with the supplied discriminator.
auto type_cast(TypeEnum type, BaseType* ptr) {
  auto id = reinterpret_cast<std::underlying_type<TypeEnum>>(type);
  return type_cast<TypeEnum, BaseType, TArr...>(reinterpret_cast<TypeEnum>(id - 1), ptr);
}

template <typename TypeEnum, typename BaseType, typename... TArr>
class variable_pointer {
  TypeEnum type;
  BaseType* ptr;

public:
  // Initialize this object from the supplied storage or value state.
  constexpr variable_pointer(TypeEnum type, BaseType* ptr) : type{type}, ptr{ptr} {}
  // Release resources owned by this object.
  ~variable_pointer() {
    if (ptr) {
      delete ptr;
    }
  }

  // Return the stored discriminator for this borrowed pointer.
  constexpr auto get_type() {
    return type;
  }
  // Return the concrete borrowed pointer selected by the stored discriminator.
  constexpr auto get() {
    return type_cast<TypeEnum, BaseType, TArr...>(type, ptr);
  }
  // Access the referenced value; the caller must provide a valid cursor or pointer.
  constexpr auto operator*() {
    return *get();
  }
  // Borrow the pointed-to object selected by this wrapper.
  constexpr auto operator->() {
    return get();
  }
};

enum class serialize_key_type { none, integer, string };

enum class serialize_type { in, out };

template <serialize_type type>
class json {};

template <>
class json<serialize_type::in> {
public:
  constexpr static serialize_key_type key_type = serialize_key_type::string;

protected:
  const stream& in_stream;

public:
  // Initialize this object from the supplied storage or value state.
  json(const stream& in_stream) : in_stream{in_stream} {}

  // Borrow the protocol stream without transferring ownership.
  const auto& get_stream() {
    return in_stream;
  }
  // Borrow the protocol stream without transferring ownership.
  auto& get_stream() const {
    return in_stream;
  }

protected:
  // Test for the ASCII whitespace characters accepted by the parser.
  constexpr bool is_whitespace(const char val) noexcept {
    return val == ' ' || val == '\t' || val == '\n' || val == '\r';
  }
  // Advance past whitespace before the next token.
  void skip_whitespace() {
    while (is_whitespace(*in_stream)) {
      ++in_stream;
    }
  }

  // Consume the expected character or throw a parser diagnostic.
  void check_and_increase(char value) {
    if (*in_stream != value) {
      std::string error_text{"Expected "};
      error_text.push_back(value);
      error_text += " but found ";
      error_text.push_back(*in_stream);
      throw exception::bad_input_data{in_stream, std::move(error_text)};
    }
    ++in_stream;
  }

  // Borrow the next JSON member name and consume its colon separator.
  const std::string_view serialize_in_get_key() {
    skip_whitespace();
    check_and_increase('"');
    auto start = in_stream.curr();
    // TODO:: Escape character
    while (*in_stream != '"') {
      ++in_stream;
    }
    auto end = in_stream.curr();
    ++in_stream;
    skip_whitespace();
    check_and_increase(':');
    skip_whitespace();
    return {reinterpret_cast<const char*>(start), reinterpret_cast<const char*>(end)};
  }

  // Read a JSON boolean and advance past its spelling.
  void serialize_in_bool(bool& value) {
    if (in_stream.remaining_buffer() < 4) {
      throw exception::bad_input_data{in_stream};
    }
    auto ch = std::tolower(*in_stream);
    if (ch == 't') {
      ++in_stream;
      if (std::tolower(*in_stream) != 'r') {
        throw exception::bad_input_data{in_stream};
      }
      ++in_stream;
      if (std::tolower(*in_stream) != 'u') {
        throw exception::bad_input_data{in_stream};
      }
      ++in_stream;
      if (std::tolower(*in_stream) != 'e') {
        throw exception::bad_input_data{in_stream};
      }
      ++in_stream;
      value = true;
    } else if (ch == 'f') {
      ++in_stream;
      if (std::tolower(*in_stream) != 'a') {
        throw exception::bad_input_data{in_stream};
      }
      ++in_stream;
      if (std::tolower(*in_stream) != 'l') {
        throw exception::bad_input_data{in_stream};
      }
      ++in_stream;
      if (std::tolower(*in_stream) != 's') {
        throw exception::bad_input_data{in_stream};
      }
      ++in_stream;
      if (in_stream.full()) {
        throw exception::bad_input_data{in_stream};
      }
      if (std::tolower(*in_stream) != 'e') {
        throw exception::bad_input_data{in_stream};
      }
      ++in_stream;
      value = false;
    }
  }

  // Read the character inside a quoted JSON character value.
  void serialize_in_char(char& value) {
    if (in_stream.remaining_buffer() < 3) {
      throw exception::bad_input_data{in_stream};
    }
    if (*in_stream != '"') {
      throw exception::bad_input_data{in_stream};
    }
    ++in_stream;
    value = *in_stream;
    ++in_stream;
    if (*in_stream != '"') {
      throw exception::bad_input_data{in_stream};
    }
    ++in_stream;
  }

  // Read decimal digits, saturating at the unsigned destination limit.
  void serialize_in_unsigned_integer(std::unsigned_integral auto& value) {
    using ValueType = std::remove_reference_t<decltype(value)>;
    if (in_stream.full()) {
      throw exception::bad_input_data{in_stream};
    }
    auto ch = *in_stream;
    if (ch < '0' || ch > '9') {
      throw exception::bad_input_data{in_stream};
    }
    value = ch - '0';
    ++in_stream;
    while (!in_stream.full()) {
      ch = *in_stream;
      if (ch < '0' || ch > '9') {
        break;
      }
      constexpr ValueType check_big =
          std::numeric_limits<ValueType>::max() / static_cast<ValueType>(constants::decimal_radix);
      constexpr ValueType check_big_last =
          std::numeric_limits<ValueType>::max() % static_cast<ValueType>(constants::decimal_radix);
      if (value > check_big) {
        // This will make value max
        value = std::numeric_limits<ValueType>::max();
        ++in_stream;
        while (!in_stream.full()) {
          ch = *in_stream;
          if (ch < '0' || ch > '9') {
            break;
          }
          ++in_stream;
        }
        return;

      } else if (value == check_big) {
        ++in_stream;
        if (!in_stream.full()) {
          auto ch1 = *in_stream;
          if (ch1 < '0' || ch1 > '9') {
            if (ch < '0' + check_big_last) {
              value = value * constants::decimal_radix - '0' + ch;
            } else {
              value = std::numeric_limits<ValueType>::max();
            }
            return;
          }
          value = std::numeric_limits<ValueType>::max();
          ++in_stream;
          while (!in_stream.full()) {
            ch = *in_stream;
            if (ch < '0' || ch > '9') {
              break;
            }
            ++in_stream;
          }
          return;
        } else {
          if (ch < '0' + check_big_last) {
            value = value * constants::decimal_radix - '0' + ch;
          } else {
            value = std::numeric_limits<ValueType>::max();
          }
          return;
        }
      }
      value = value * constants::decimal_radix + *in_stream - '0';
      ++in_stream;
    }
  }

  // Read a signed decimal integer using the existing conversion rules.
  void serialize_in_signed_integer(std::signed_integral auto& value) {
    if (in_stream.full()) {
      throw exception::bad_input_data{in_stream};
    }
    // TODO: Check out of range values
    if ((*in_stream < '0' || *in_stream > '9') && *in_stream != '-' && *in_stream != '+') {
      throw exception::bad_input_data{in_stream};
    }
    auto sign{static_cast<std::remove_reference_t<decltype(value)>>(1)};
    if (*in_stream == '-') {
      sign = -1;
      ++in_stream;
    } else if (*in_stream == '+') {
      ++in_stream;
    }
    value = *in_stream - '0';
    ++in_stream;
    while (!in_stream.full()) {
      if (*in_stream < '0' || *in_stream > '9') {
        break;
      }
      value = value * constants::decimal_radix + *in_stream - '0';
      ++in_stream;
    }
    value *= sign;
  }

  // Append the contents of a quoted JSON string to the destination.
  void serialize_in_string(std::string& value) {
    if (in_stream.remaining_buffer() < 2) {
      throw exception::bad_input_data{in_stream};
    }
    check_and_increase('"');
    while (*in_stream != '"') {
      if (in_stream.full()) {
        throw exception::bad_input_data{in_stream, "Expecting '\"'"};
      }
      value.push_back(*in_stream);
      ++in_stream;
    }
    ++in_stream;
  }

  // Read a numeric token and convert it to the requested floating-point type.
  void serialize_in_floating_point(std::floating_point auto& value) {
    if (in_stream.full()) {
      throw exception::bad_input_data{in_stream};
    }
    // TODO: Check out of range values
    if ((*in_stream < '0' || *in_stream > '9') && *in_stream != '-' && *in_stream != '+') {
      throw exception::bad_input_data{in_stream};
    }
    auto start = in_stream.curr();
    while (!in_stream.full() && *in_stream != ',' && *in_stream != '!' && *in_stream != ']' &&
           *in_stream != '}' && *in_stream != ' ') {
      ++in_stream;
    }
    std::string number{start, in_stream.curr()};
    if constexpr (std::is_same_v<float, std::remove_reference_t<decltype(value)>>) {
      value = std::stof(number);
    } else {
      value = std::stod(number);
    }
  }

  // Read a JSON array and append its decoded elements to the destination.
  void serialize_in_vector(type_check::vector auto& value) {
    check_and_increase('[');
    skip_whitespace();
    if (*in_stream != ']') {
      while (true) {
        using value_type = std::remove_reference_t<decltype(value)>::value_type;
        value_type element{};
        serialize_in(element);
        value.emplace_back(element);
        skip_whitespace();
        if (*in_stream == ']') {
          break;
        }
        check_and_increase(',');
        skip_whitespace();
        if (*in_stream == ']') {
          throw exception::bad_input_data{
              in_stream, "Unexpected ',', there must be next array entry after ','"};
        }
      }
    }
    ++in_stream;
  }

  // Read the JSON key/value-entry array and insert decoded pairs.
  void serialize_in_map(type_check::map auto& value) {
    check_and_increase('[');
    skip_whitespace();
    if (*in_stream != ']') {
      while (true) {
        check_and_increase('{');
        skip_whitespace();
        std::string temp{};
        serialize_in(temp);
        if (temp != constants::map_key_name) {
          throw exception::bad_input_data{in_stream, "Expected 'key' but found " + temp};
        }
        skip_whitespace();
        check_and_increase(':');
        skip_whitespace();
        using T = std::remove_reference_t<decltype(value)>;
        typename T::key_type key{};
        serialize_in(key);
        skip_whitespace();
        check_and_increase(',');
        skip_whitespace();
        std::string temp_value{};
        serialize_in(temp_value);
        if (temp_value != constants::map_value_name) {
          throw exception::bad_input_data{in_stream, "Expected 'value' but found " + temp};
        }
        skip_whitespace();
        check_and_increase(':');
        skip_whitespace();
        typename T::mapped_type element{};
        serialize_in(element);
        value.emplace(std::move(key), std::move(element));
        skip_whitespace();
        check_and_increase('}');
        skip_whitespace();
        if (*in_stream == ']') {
          break;
        }
        check_and_increase(',');
        skip_whitespace();
        if (*in_stream == ']') {
          throw exception::bad_input_data{in_stream,
                                          "Unexpected ',', there must be next map entry after ','"};
        }
      }
    }
    ++in_stream;
  }

public:
  // Decode a supported value and advance the input cursor; invalid input may throw.
  template <typename T>
  void serialize_in(T& value) {
    if constexpr (std::is_same_v<bool, T>) {
      serialize_in_bool(value);
    } else if constexpr (std::is_same_v<char, T>) {
      serialize_in_char(value);
    } else if constexpr (std::unsigned_integral<T>) {
      serialize_in_unsigned_integer(value);
    } else if constexpr (std::signed_integral<T>) {
      serialize_in_signed_integer(value);
    } else if constexpr (std::is_same_v<std::string, T>) {
      serialize_in_string(value);
    } else if constexpr (std::floating_point<T>) {
      serialize_in_floating_point(value);
    } else if constexpr (type_check::serializer_out_enabled_ptr<T, json>) {
      value->serialize_in(*this);
    } else if constexpr (type_check::serializer_out_enabled<T, json>) {
      value.serialize_in(*this);
    } else if constexpr (type_check::vector<T>) {
      serialize_in_vector(value);
    } else if constexpr (type_check::map<T>) {
      serialize_in_map(value);
    } else {
      throw exception::bad_type{in_stream};
    }
  }

  // Dispatch serialized object members to the generated input callbacks.
  template <typename T>
  void struct_serialize_in(T* obj) {
    static_assert(key_type == serialize_key_type::string, "Only String key type supported");
    skip_whitespace();
    check_and_increase('{');
    skip_whitespace();
    while (true) {
      auto key = serialize_in_get_key();
      obj->serialize_in_member_by_name(*this, key);
      skip_whitespace();
      if (*in_stream == '}') {
        break;
      }
      check_and_increase(',');
      skip_whitespace();
      if (*in_stream == '}') {
        throw exception::bad_input_data{in_stream,
                                        "Unexpected ',', there next object expected after ','"};
      }
    }
    ++in_stream;
  }
}; // class json<serialize_type::in>

template <bool beautify>
class json_formatter {
protected:
  stream& out_stream;

public:
  // Initialize this object from the supplied storage or value state.
  json_formatter(stream& out_stream, const write_format&) : out_stream{out_stream} {}

protected:
  // Emit C++ brace open for the parsed schema.
  inline void write_brace_open() {
    out_stream.write('{');
  }

  // Emit C++ brace close for the parsed schema.
  inline void write_brace_close() {
    out_stream.write('}');
  }

  // Emit C++ bracket open for the parsed schema.
  inline void write_bracket_open() {
    out_stream.write('[');
  }

  // Emit C++ bracket close for the parsed schema.
  inline void write_bracket_close() {
    out_stream.write(']');
  }

  // Emit C++ comma for the parsed schema.
  template <bool member_object>
  inline void write_comma() {
    out_stream.write(',');
  }

  // Emit C++ colon for the parsed schema.
  inline void write_colon() {
    out_stream.write(':');
  }

  // Apply configured whitespace before writing the next value.
  inline void before_data() {}
};

template <>
class json_formatter<true> {
  bool newline_written{true};

protected:
  stream& out_stream;
  const write_format format_definition;

  std::string tab_string{};

  // Apply configured whitespace before an object opening brace.
  inline void before_brace_open() {
    if (format_definition.newline_before_braces_open) {
      if (!newline_written) {
        out_stream.write('\n');
        out_stream.write(tab_string);
      }
    }
  }

  // Update formatting state after an object opening brace.
  inline void after_brace_open() {
    if (format_definition.newline_after_braces_open ||
        format_definition.newline_before_object_member) {
      out_stream.write('\n');
      tab_string.append(format_definition.indent_text);
      out_stream.write(tab_string);
      newline_written = true;
    }
  }

  // Apply configured whitespace before an object closing brace.
  inline void before_brace_close() {
    if (format_definition.newline_before_braces_close) {
      out_stream.write('\n');
      tab_string.erase(std::end(tab_string) - std::size(format_definition.indent_text),
                       // Expose the buffer end; the returned pointer does not own storage.
                       std::end(tab_string));
      out_stream.write(tab_string);
    }
  }

  // Update formatting state after an object closing brace.
  inline void after_brace_close() {
    if (format_definition.newline_after_braces_close) {
      out_stream.write('\n');
      out_stream.write(tab_string);
      newline_written = true;
    }
  }

  // Apply configured whitespace before an array opening bracket.
  inline void before_bracket_open() {
    if (format_definition.newline_before_bracket_open) {
      if (!newline_written) {
        out_stream.write('\n');
        out_stream.write(tab_string);
      }
    }
  }

  // Update formatting state after an array opening bracket.
  inline void after_bracket_open() {
    if (format_definition.newline_after_bracket_open) {
      out_stream.write('\n');
      tab_string.append(format_definition.indent_text);
      out_stream.write(tab_string);
      newline_written = true;
    }
  }

  // Apply configured whitespace before an array closing bracket.
  inline void before_bracket_close() {
    if (format_definition.newline_before_bracket_close) {
      out_stream.write('\n');
      tab_string.erase(std::end(tab_string) - std::size(format_definition.indent_text),
                       // Expose the buffer end; the returned pointer does not own storage.
                       std::end(tab_string));
      out_stream.write(tab_string);
    }
  }

  // Update formatting state after an array closing bracket.
  inline void after_bracket_close() {
    if (format_definition.newline_after_bracket_close) {
      out_stream.write('\n');
      out_stream.write(tab_string);
      newline_written = true;
    }
  }

  // Emit C++ brace open for the parsed schema.
  inline void write_brace_open() {
    before_brace_open();
    out_stream.write('{');
    after_brace_open();
  }

  // Emit C++ brace close for the parsed schema.
  inline void write_brace_close() {
    before_brace_close();
    out_stream.write('}');
    after_brace_close();
  }

  // Emit C++ bracket open for the parsed schema.
  inline void write_bracket_open() {
    before_bracket_open();
    out_stream.write('[');
    after_bracket_open();
  }

  // Emit C++ bracket close for the parsed schema.
  inline void write_bracket_close() {
    before_bracket_close();
    out_stream.write(']');
    after_bracket_close();
  }

  // Emit C++ comma for the parsed schema.
  template <bool member_object>
  inline void write_comma() {
    if (format_definition.newline_after_comma ||
        (member_object && format_definition.newline_before_object_member)) {
      out_stream.write(",\n", tab_string);
      newline_written = true;
    } else if (format_definition.space_after_comma) {
      out_stream.write(", ");
    } else {
      out_stream.write(',');
    }
  }

  // Emit C++ colon for the parsed schema.
  inline void write_colon() {
    if (format_definition.space_after_colon) {
      out_stream.write(": ");
    } else {
      out_stream.write(':');
    }
  }

  // Apply configured whitespace before writing the next value.
  inline void before_data() {
    newline_written = false;
  }

public:
  // Initialize this object from the supplied storage or value state.
  json_formatter(stream& out_stream, const write_format& format_definition)
      : out_stream{out_stream}, format_definition{format_definition} {}
};

template <bool beautify>
class json_out : public json_formatter<beautify> {
public:
  constexpr static serialize_key_type key_type = serialize_key_type::string;

public:
  // Initialize this object from the supplied storage or value state.
  json_out(stream& out_stream) : json_formatter<beautify>{out_stream, format::compress} {}
  // Initialize this object from the supplied storage or value state.
  json_out(stream& out_stream, const write_format& format_definition)
      : json_formatter<beautify>{out_stream, format_definition} {}

private:
  using json_formatter<beautify>::out_stream;

  using json_formatter<beautify>::write_brace_open;
  using json_formatter<beautify>::write_brace_close;
  using json_formatter<beautify>::write_bracket_open;
  using json_formatter<beautify>::write_bracket_close;
  using json_formatter<beautify>::write_colon;
  using json_formatter<beautify>::before_data;

  // Write the first collection element without a leading separator.
  template <typename T>
  void serialize_out_first(auto& name, const T& value) {
    serialize_out(name);
    write_colon();
    serialize_out(value);
  }

  // Write a subsequent collection element with its separator.
  template <typename T>
  void serialize_out_second(auto& name, const T& value) {
    json_formatter<beautify>::template write_comma<true>();
    serialize_out(name);
    write_colon();
    serialize_out(value);
  }

  // Write a map entry using the established key and value field names.
  void serialize_out_key_value_pair(const auto& value) {
    constexpr auto key_str = constants::map_key_name;
    constexpr auto value_str = constants::map_value_name;
    write_brace_open();
    serialize_out_first(key_str, value.first);
    serialize_out_second(value_str, value.second);
    write_brace_close();
  }

  template <typename T>
  // Write collection elements in iteration order.
  void serialize_out_list(const T& value_list,
                          std::function<void(const typename T::value_type&)> serialize_func) {
    write_bracket_open();
    auto itr = std::begin(value_list);
    if (itr != std::end(value_list)) {
      serialize_func(*itr);
      itr = std::next(itr);
      while (itr != std::end(value_list)) {
        json_formatter<beautify>::template write_comma<false>();
        serialize_func(*itr);
        itr = std::next(itr);
      }
    }
    write_bracket_close();
  }

public:
  // Encode a supported value or field through this protocol and advance the output cursor.
  template <typename T>
  void serialize_out(const T& value) {
    if constexpr (std::is_same_v<T, char>) {
      before_data();
      out_stream.write('"', value, '"');
    } else if constexpr (std::is_same_v<T, bool>) {
      before_data();
      if (value) {
        out_stream.append("true");
      } else {
        out_stream.append("false");
      }
    } else if constexpr (std::integral<T>) {
      before_data();
      out_stream.append_string(value);
    } else if constexpr (std::floating_point<T>) {
      before_data();
      auto float_str = std::to_string(value);
      out_stream.append(float_str);
    } else if constexpr (std::same_as<T, std::string>) {
      before_data();
      out_stream.write('"', value, '"');
    } else if constexpr (std::same_as<T, std::string_view>) {
      before_data();
      out_stream.write('"', value, '"');
    } else if constexpr (type_check::serializer_out_enabled_ptr<T, json<serialize_type::out>>) {
      value->serialize_out(*this);
    } else if constexpr (type_check::serializer_out_enabled<T, json<serialize_type::out>>) {
      value.serialize_out(*this);
    } else {
      throw exception::unknown_serialization_type{out_stream, "Unknown Serialization Type"};
    }
  }

  // Encode a supported value or field through this protocol and advance the output cursor.
  template <type_check::functions T>
  void serialize_out(const T& value) {
    value(out_stream);
  }

  // Encode a supported value or field through this protocol and advance the output cursor.
  template <type_check::vector T>
  void serialize_out(const T& value) {
    serialize_out_list(value, [this](const T::value_type& val) { serialize_out(val); });
  }

  // Encode a supported value or field through this protocol and advance the output cursor.
  template <type_check::map T>
  void serialize_out(const T& value) {
    serialize_out_list(value,
                       [this](const T::value_type& val) { serialize_out_key_value_pair(val); });
  }

  // Write the first object member with the protocol opening delimiter.
  void struct_serialize_out_start(const auto& value) {
    write_brace_open();
    serialize_out_first(value.first, value.second);
  }

  // Write a subsequent object member with the required separation.
  void struct_serialize_out(const auto& value) {
    serialize_out_second(value.first, value.second);
  }

  // Finish an object using the protocol closing delimiter or sentinel.
  void struct_serialize_out_end() {
    write_brace_close();
  }
}; // class json_out<>

template <>
class json<serialize_type::out> : public json_out<false> {
public:
  using json_out<false>::json_out;
};

template <serialize_type type, serialize_key_type KeyType>
class binary {};

template <serialize_key_type KeyType>
class binary_in_base {
public:
  constexpr static serialize_key_type key_type = KeyType;

protected:
  const stream& in_stream;

public:
  // Initialize this object from the supplied storage or value state.
  binary_in_base(const stream& in_stream) : in_stream{in_stream} {}

  // Borrow the protocol stream without transferring ownership.
  const auto& get_stream() {
    return in_stream;
  }
  // Borrow the protocol stream without transferring ownership.
  auto& get_stream() const {
    return in_stream;
  }

  // Decode the established one-to-four-byte unsigned integer representation.
  std::uint32_t serialize_in_variable() {
    if (in_stream.full()) {
      throw exception::bad_input_data{in_stream};
    }
    const std::uint32_t val = *in_stream.get_curr_and_increase_unchecked(1);
    switch (val & constants::variable_tag_mask) {
    default:
    case 0x00:
      return val;
    case constants::variable_two_byte_tag:
      if (in_stream.full()) {
        throw exception::bad_input_data{in_stream};
      }
      return ((val & constants::variable_payload_mask) << constants::wire_byte_bits) |
             *in_stream.get_curr_and_increase_unchecked(1);
    case constants::variable_three_byte_tag: {
      constexpr std::size_t payload_bytes = 2;
      if (in_stream.remaining_buffer() < payload_bytes) {
        throw exception::bad_input_data{in_stream};
      }
      const auto* payload = in_stream.get_curr_and_increase_unchecked(payload_bytes);
      const std::uint32_t val8 = payload[0];
      return ((val & constants::variable_payload_mask) << (2 * constants::wire_byte_bits)) |
             (val8 << constants::wire_byte_bits) | payload[1];
    }
    case constants::variable_four_byte_tag: {
      constexpr std::size_t payload_bytes = 3;
      if (in_stream.remaining_buffer() < payload_bytes) {
        throw exception::bad_input_data{in_stream};
      }
      const auto* payload = in_stream.get_curr_and_increase_unchecked(payload_bytes);
      const std::uint32_t val16 = payload[0];
      const std::uint32_t val8 = payload[1];
      return ((val & constants::variable_payload_mask) << (3 * constants::wire_byte_bits)) |
             (val16 << (2 * constants::wire_byte_bits)) | (val8 << constants::wire_byte_bits) |
             payload[2];
    }
    }
  }

  // Decode a supported value and advance the input cursor; invalid input may throw.
  template <typename T>
  void serialize_in(T& value) {
    if constexpr (std::is_same_v<char, T>) {
      if (in_stream.full()) {
        throw exception::bad_input_data{in_stream};
      }
      value = *in_stream.get_curr_and_increase_unchecked(1);
    } else if constexpr (std::is_same_v<bool, T>) {
      if (in_stream.full()) {
        throw exception::bad_input_data{in_stream};
      }
      value = !!(*in_stream.get_curr_and_increase_unchecked(1));
    } else if constexpr (std::is_enum_v<T>) {
      auto ival = serialize_in_variable();
      value = static_cast<T>(ival);
    } else if constexpr (std::integral<T>) {
      if (in_stream.remaining_buffer() < sizeof(T)) {
        throw exception::bad_input_data{in_stream};
      }
      T source = *reinterpret_cast<const T*>(in_stream.curr());
      value = change_endian<std::endian::big, std::endian::native>(source);
      in_stream.advance_unchecked(sizeof(T));
    } else if constexpr (std::is_same_v<std::string, T>) {
      // variable size following string of size
      auto size = serialize_in_variable();
      if (in_stream.remaining_buffer() < size) {
        throw exception::bad_input_data{in_stream};
      }
      value = std::string{in_stream.curr(), in_stream.curr() + size};
      in_stream.advance_unchecked(size);
    } else if constexpr (std::floating_point<T>) {
      if (in_stream.remaining_buffer() < sizeof(T)) {
        throw exception::bad_input_data{in_stream};
      }
      T source = *reinterpret_cast<const T*>(in_stream.curr());
      value = change_endian<std::endian::big, std::endian::native>(source);
      in_stream.advance_unchecked(sizeof(T));
    } else if constexpr (type_check::serializer_out_enabled_ptr<
                             T, binary<serialize_type::in, KeyType>>) {
      value->serialize_in(*this);
    } else if constexpr (type_check::serializer_out_enabled<T,
                                                            binary<serialize_type::in, KeyType>>) {
      value.serialize_in(*this);
    } else if constexpr (type_check::vector<T>) {
      // variable size following vector members
      auto size = serialize_in_variable();
      for (std::size_t i = 0; i < size; ++i) {
        typename T::value_type element{};
        serialize_in(element);
        value.emplace_back(std::move(element));
      }
    } else if constexpr (type_check::map<T>) {
      // variable size following map members
      auto size = serialize_in_variable();
      for (std::size_t i = 0; i < size; ++i) {
        typename T::key_type key{};
        serialize_in(key);
        typename T::mapped_type element{};
        serialize_in(element);
        value.emplace(std::move(key), std::move(element));
      }
    } else {
      throw exception::bad_type{in_stream};
    }
  }

  // Dispatch serialized object members to the generated input callbacks.
  template <typename T>
  void struct_serialize_in(T* obj) {
    if constexpr (KeyType == serialize_key_type::integer) {
      while (true) {
        auto key = serialize_in_variable();
        if (key == constants::binary_object_end_id) {
          break;
        }
        obj->serialize_in_member_by_identifier(*this, key);
      }
    } else if constexpr (KeyType == serialize_key_type::string) {
      while (true) {
        std::string key{};
        serialize_in(key);
        if (key.empty()) {
          break;
        }
        obj->serialize_in_member_by_name(*this, key);
      }
    }
  }

}; // class binary_in_base

template <>
class binary<serialize_type::in, serialize_key_type::none>
    : public binary_in_base<serialize_key_type::none> {
public:
  using binary_in_base<serialize_key_type::none>::binary_in_base;
}; // class binary<serialize_type::in, serialize_key_type::none>

template <>
class binary<serialize_type::in, serialize_key_type::integer>
    : public binary_in_base<serialize_key_type::integer> {
public:
  using binary_in_base<serialize_key_type::integer>::binary_in_base;
}; // class binary<serialize_type::in, serialize_key_type::integer>

template <>
class binary<serialize_type::in, serialize_key_type::string>
    : public binary_in_base<serialize_key_type::string> {
public:
  using binary_in_base<serialize_key_type::string>::binary_in_base;
}; // class binary<serialize_type::in, serialize_key_type::string>

template <serialize_key_type KeyType>
class binary_out_base {
public:
  constexpr static serialize_key_type key_type = KeyType;

protected:
  stream& out_stream;

public:
  // Initialize this object from the supplied storage or value state.
  binary_out_base(stream& out_stream) : out_stream{out_stream} {}

  // Borrow the protocol stream without transferring ownership.
  const auto& get_stream() {
    return out_stream;
  }
  // Borrow the protocol stream without transferring ownership.
  auto& get_stream() const {
    return out_stream;
  }

protected:
  // Encode a field ID or a positional union discriminator, followed by its value.
  template <typename T>
  void serialize_out(const std::integral auto& id, const T& value) {
    static_assert(KeyType == serialize_key_type::none || KeyType == serialize_key_type::integer,
                  "Numeric binary keys require positional or integer-key mode");
    if constexpr (KeyType == serialize_key_type::integer) {
      if (id == constants::binary_object_end_id) {
        throw std::invalid_argument{"Binary field ID zero is reserved for object termination"};
      }
    }
    serialize_out_variable(id);
    serialize_out(value);
  }

  // Borrow the field name and encode it before the field value.
  template <typename T>
  void serialize_out(const std::string& name, const T& value) {
    serialize_out(std::string_view{name}, value);
  }

  // Preserve a nonempty wire name; an empty name is reserved for object termination.
  template <typename T>
  void serialize_out(const std::string_view& name, const T& value) {
    static_assert(KeyType == serialize_key_type::string,
                  "Named binary fields require string-key mode");
    if (name.empty()) {
      throw std::invalid_argument{"An empty binary field name is reserved for object termination"};
    }
    serialize_out(name);
    serialize_out(value);
  }

  // Encode a supported value or field through this protocol and advance the output cursor.
  template <typename T, typename U>
  void serialize_out(const std::pair<T, U>& value) {
    serialize_out(value.first, value.second);
  }

  // Encode an integer-key union as its field ID, alternative index, and payload.
  template <typename T>
  void serialize_out(const std::integral auto& id, const std::integral auto& index,
                     const T& value) {
    static_assert(KeyType == serialize_key_type::integer,
                  "Binary field ID and union index require integer-key mode");
    if (id == constants::binary_object_end_id) {
      throw std::invalid_argument{"Binary field ID zero is reserved for object termination"};
    }
    serialize_out_variable(id);
    serialize_out_variable(index);
    serialize_out(value);
  }

  template <typename T, typename U, typename V>
  // Encode a supported value or field through this protocol and advance the output cursor.
  void serialize_out(const std::tuple<T, U, V>& value) {
    serialize_out(std::get<0>(value), std::get<1>(value), std::get<2>(value));
  }

public:
  // Encode a nonnegative 30-bit integer in one to four bytes; reject invalid values before writing.
  void serialize_out_variable(const std::integral auto id) {
    if constexpr (std::is_signed_v<decltype(id)>) {
      if (id < 0) {
        throw std::out_of_range{"Binary variable integers must be nonnegative"};
      }
    }
    if (id <= constants::variable_one_byte_max) {
      out_stream.write_raw(static_cast<std::uint8_t>(id));
    } else if (id <= constants::variable_two_byte_max) {
      out_stream.write_raw(static_cast<std::uint8_t>(((id >> constants::wire_byte_bits) |
                                                      constants::variable_two_byte_tag)),
                           static_cast<std::uint8_t>(id & constants::wire_byte_mask));
    } else if (id <= constants::variable_three_byte_max) {
      out_stream.write_raw(
          static_cast<std::uint8_t>(
              ((id >> (2 * constants::wire_byte_bits)) | constants::variable_three_byte_tag)),
          static_cast<std::uint8_t>((id >> constants::wire_byte_bits) & constants::wire_byte_mask),
          static_cast<std::uint8_t>(id & constants::wire_byte_mask));
    } else if (id <= constants::variable_four_byte_max) {
      out_stream.write_raw(
          static_cast<std::uint8_t>(
              ((id >> (3 * constants::wire_byte_bits)) | constants::variable_four_byte_tag)),
          static_cast<std::uint8_t>((id >> (2 * constants::wire_byte_bits)) &
                                    constants::wire_byte_mask),
          static_cast<std::uint8_t>((id >> constants::wire_byte_bits) & constants::wire_byte_mask),
          static_cast<std::uint8_t>(id & constants::wire_byte_mask));
    } else {
      throw std::out_of_range{"Binary variable integer exceeds the 30-bit wire range"};
    }
  }

  // Append encoded fields to the contiguous stream; never copy an aggregate object layout.
  // Byte writes follow the stream reservation policy; later failures can leave prior output.
  template <typename T>
  void serialize_out(const T& value) {
    if constexpr (std::is_same_v<char, T> || std::is_same_v<bool, T>) {
      out_stream.append(static_cast<std::uint8_t>(value));
    } else if constexpr (std::is_enum_v<T>) {
      serialize_out_variable(static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (std::integral<T>) {
      using wire_type = std::make_unsigned_t<T>;
      const auto wire_value = change_endian<std::endian::native, std::endian::big>(
          static_cast<wire_type>(value));
      // Byte copying does not require the stream cursor to be aligned for wire_type.
      out_stream.append_external(&wire_value, sizeof(wire_value));
    } else if constexpr (std::is_same_v<std::string, T>) {
      // variable size following string of size
      serialize_out_variable(value.size());
      out_stream.append(value);
    } else if constexpr (std::is_same_v<std::string_view, T>) {
      // variable size following string of size
      serialize_out_variable(value.size());
      out_stream.append(value);
    } else if constexpr (std::floating_point<T>) {
      static_assert(std::numeric_limits<T>::is_iec559 &&
                        (sizeof(T) == sizeof(std::uint32_t) || sizeof(T) == sizeof(std::uint64_t)),
                    "Binary floating-point output requires a 32-bit or 64-bit IEC 559 representation");
      // Preserve the scalar bits, then use the integer path for big-endian byte output.
      if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
        serialize_out(std::bit_cast<std::uint32_t>(value));
      } else if constexpr (sizeof(T) == sizeof(std::uint64_t)) {
        serialize_out(std::bit_cast<std::uint64_t>(value));
      }
    } else if constexpr (type_check::serializer_out_enabled_ptr<
                             T, binary<serialize_type::out, KeyType>>) {
      value->serialize_out(*this);
    } else if constexpr (type_check::serializer_out_enabled<T,
                                                            binary<serialize_type::out, KeyType>>) {
      value.serialize_out(*this);
    } else if constexpr (type_check::vector<T>) {
      serialize_out_variable(value.size());
      for (const auto& item : value) {
        serialize_out(item);
      }
    } else if constexpr (type_check::map<T>) {
      serialize_out_variable(value.size());
      for (const auto& item : value) {
        serialize_out(item.first);
        serialize_out(item.second);
      }
    } else {
      throw exception::bad_type{out_stream, "Bad Type, this is internal error."};
    }
  }

  // Encode a supported value or field through this protocol and advance the output cursor.
  template <type_check::functions T>
  void serialize_out(const T& value) {
    value(out_stream);
  }

  // Write the first object member with the protocol opening delimiter.
  void struct_serialize_out_start(const auto& value) {
    struct_serialize_out(value);
  }

  // Write a subsequent object member with the required separation.
  void struct_serialize_out(const auto& value) {
    serialize_out(value);
  }

  // Positional objects need no terminator; keyed objects end with a zero ID or name length.
  void struct_serialize_out_end() {
    if constexpr (KeyType == serialize_key_type::integer || KeyType == serialize_key_type::string) {
      serialize_out_variable(constants::binary_object_end_id);
    }
  }
}; // class binary_out_base

template <>
class binary<serialize_type::out, serialize_key_type::none>
    : public binary_out_base<serialize_key_type::none> {
public:
  using binary_out_base<serialize_key_type::none>::binary_out_base;
}; // class binary<serialize_type::out, serialize_key_type::none>

template <>
class binary<serialize_type::out, serialize_key_type::integer>
    : public binary_out_base<serialize_key_type::integer> {
public:
  using binary_out_base<serialize_key_type::integer>::binary_out_base;
}; // class binary<serialize_type::out, serialize_key_type::integer>

template <>
class binary<serialize_type::out, serialize_key_type::string>
    : public binary_out_base<serialize_key_type::string> {
public:
  using binary_out_base<serialize_key_type::string>::binary_out_base;
}; // class binary<serialize_type::out, serialize_key_type::string>

template <serialize_type type>
using binary_integer = binary<type, serialize_key_type::integer>;

template <serialize_type type>
using binary_string = binary<type, serialize_key_type::string>;

template <serialize_type type>
using binary_none = binary<type, serialize_key_type::none>;

} // namespace rohit::serializer
