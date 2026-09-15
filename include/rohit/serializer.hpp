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
#include <rohit/decode.hpp>
#include <rohit/json_text.hpp>
#include <rohit/stream.hpp>

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
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

namespace detail {
// Use a fixed-width hash for generated lookup groups, followed by exact name equality.
constexpr std::uint64_t field_name_hash(std::string_view name) noexcept {
  constexpr std::uint64_t hash_seed = 100000000003ULL;
  constexpr unsigned hash_shift_bits = 9;
  auto result = hash_seed;
  for (const auto ch : name) {
    result = ((result << hash_shift_bits) + result) ^ static_cast<unsigned char>(ch);
  }
  return result;
}

// Find a generated enum's borrowed wire spelling through argument-dependent lookup.
template <typename T>
constexpr std::string_view enum_name(T value) {
  return serializer_enum_name(value);
}

// Decode a generated named enum through its namespace's mapping, preserving custom protocols.
template <typename Protocol, typename T>
void read_named_enum(Protocol& protocol, T& value) {
  if constexpr (requires { protocol.serialize_in_named_enum(value); }) {
    protocol.serialize_in_named_enum(value);
  } else {
    std::string name;
    protocol.serialize_in(name);
    try {
      serializer_enum_from_name(value, std::string_view{name});
    } catch (const std::invalid_argument&) {
      throw exception::bad_input_data{protocol.get_stream(), "Unknown enum name"};
    }
  }
}
} // namespace detail

namespace exception {
using rohit::exception::base_parser;

class bad_type : public base_parser {
public:
  using base_parser::base_parser;
  // Identify an unsupported source or destination type.
  rohit::exception::parser_error_code code() const noexcept override {
    return rohit::exception::parser_error_code::invalid_type;
  }
};

class unknown_serialization_type : public base_parser {
public:
  using base_parser::base_parser;
  // Identify an unsupported protocol value type.
  rohit::exception::parser_error_code code() const noexcept override {
    return rohit::exception::parser_error_code::invalid_type;
  }
};

class key_not_found : public base_parser {
public:
  using base_parser::base_parser;
  // Identify a field absent from the selected schema.
  rohit::exception::parser_error_code code() const noexcept override {
    return rohit::exception::parser_error_code::unknown_field;
  }
};
} // namespace exception

namespace type_check {
template <typename T, typename J>
concept serializer_in_enabled = requires(T cls, J& serialize_protocol) {
  { cls->template serialize_in<J>(serialize_protocol) } -> std::same_as<void>;
};

template <typename T, typename J>
concept serializer_in_enabled_ptr = requires(T cls, J& protocol) {
  { cls->serialize_in(protocol) } -> std::same_as<void>;
};

template <typename T, typename J>
concept serializer_in_enabled_value = requires(T cls, J& protocol) {
  { cls.serialize_in(protocol) } -> std::same_as<void>;
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

// Select the generated object's storage; values also form a generator mode mask.
enum class storage_mode : std::uint8_t { owning = 1, read_only_view = 2, mutable_view = 4 };

template <serialize_type type>
class json {};

template <>
class json<serialize_type::in> : public detail::decoder_input {
public:
  constexpr static serialize_key_type key_type = serialize_key_type::string;
  using detail::decoder_input::decoder_input;

protected:
  // Report malformed JSON without echoing payload data by default.
  [[noreturn]] void fail(const char* message) const {
    throw exception::bad_input_data{in_stream, message, limits.diagnostics};
  }

  // Recognize only the whitespace permitted by JSON.
  static constexpr bool is_whitespace(std::uint8_t ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
  }

  // Recognize the end of a scalar token without accepting a prefix of another token.
  static constexpr bool is_boundary(std::uint8_t ch) noexcept {
    return is_whitespace(ch) || ch == ',' || ch == ']' || ch == '}';
  }

  // Advance a whole whitespace run with one cursor update.
  void skip_whitespace() {
    const auto bytes = available_input();
    std::size_t size{};
    while (size < bytes.size() && is_whitespace(bytes[size])) {
      ++size;
    }
    read_bytes(size);
    if (size == bytes.size() && !in_stream.full()) {
      require_input(1);
    }
  }

  // Read one byte only after both the input range and budgets permit it.
  std::uint8_t peek() const {
    require_input(1);
    return *in_stream.curr();
  }

  // Consume one expected punctuation byte.
  void check_and_increase(char expected) {
    if (peek() != static_cast<std::uint8_t>(expected)) {
      fail("Unexpected JSON punctuation");
    }
    read_bytes(1);
  }

  // Validate a string before replacing storage; ordinary keys borrow the input only until dispatch.
  std::string_view read_string(std::string& destination, bool borrow) {
    const auto bytes = available_input();
    detail::json_string_range range;
    try {
      range = detail::scan_json_string(bytes);
    } catch (const std::invalid_argument& error) {
      if (bytes.size() < in_stream.remaining_buffer()) {
        fail_limit("String scan exceeded input or work limit");
      }
      fail(error.what());
    }
    check_string(range.text_bytes, !borrow || range.escaped);
    if (borrow && !range.escaped) {
      const auto* start = read_bytes(range.wire_bytes);
      return {reinterpret_cast<const char*>(start + 1), range.text_bytes};
    }
    if (range.text_bytes > destination.max_size()) {
      fail_limit("String exceeds destination capacity limit");
    }
    detail::assign_json_string(destination, bytes, range);
    read_bytes(range.wire_bytes);
    return destination;
  }

  // Read a wire name, decoding escapes into caller-owned scratch storage when necessary.
  std::string_view serialize_in_get_key(std::string& scratch) {
    skip_whitespace();
    const auto key = read_string(scratch, true);
    skip_whitespace();
    check_and_increase(':');
    skip_whitespace();
    return key;
  }

  // Validate JSON number grammar in a bounded span before numeric conversion.
  std::string_view number_token() const {
    const auto bytes = available_input();
    if (bytes.empty()) { require_input(1); }
    std::size_t index{};
    const auto digit = [](std::uint8_t ch) { return ch >= '0' && ch <= '9'; };
    if (index < bytes.size() && bytes[index] == '-') {
      ++index;
    }
    if (index == bytes.size()) {
      if (bytes.size() < in_stream.remaining_buffer()) { fail_limit("Number scan limit exceeded"); }
      fail("Incomplete JSON number");
    }
    if (bytes[index] == '0') {
      ++index;
    } else if (bytes[index] >= '1' && bytes[index] <= '9') {
      do { ++index; } while (index < bytes.size() && digit(bytes[index]));
    } else {
      fail("Invalid JSON number");
    }
    if (index < bytes.size() && bytes[index] == '.') {
      const auto start = ++index;
      while (index < bytes.size() && digit(bytes[index])) { ++index; }
      if (index == start) {
        if (index == bytes.size() && bytes.size() < in_stream.remaining_buffer()) {
          fail_limit("Number scan exceeded input or work limit");
        }
        fail("Missing JSON fractional digits");
      }
    }
    if (index < bytes.size() && (bytes[index] == 'e' || bytes[index] == 'E')) {
      ++index;
      if (index < bytes.size() && (bytes[index] == '+' || bytes[index] == '-')) { ++index; }
      const auto start = index;
      while (index < bytes.size() && digit(bytes[index])) { ++index; }
      if (index == start) {
        if (index == bytes.size() && bytes.size() < in_stream.remaining_buffer()) {
          fail_limit("Number scan exceeded input or work limit");
        }
        fail("Missing JSON exponent digits");
      }
    }
    if (index < bytes.size() && !is_boundary(bytes[index])) {
      fail("Invalid JSON number suffix");
    }
    if (index == bytes.size() && bytes.size() < in_stream.remaining_buffer()) {
      fail_limit("Number scan exceeded input or work limit");
    }
    return {reinterpret_cast<const char*>(bytes.data()), index};
  }

  // Parse directly from the token; range failures leave the scalar unchanged.
  template <typename T>
  void read_number(T& value) {
    const auto token = number_token();
    if constexpr (std::unsigned_integral<T>) {
      if (token.front() == '-') {
        throw exception::numeric_range{in_stream, "Negative JSON number for an unsigned destination", limits.diagnostics};
      }
    }
    T parsed{};
    const auto result = [&] {
      if constexpr (std::floating_point<T>) {
        return std::from_chars(token.data(), token.data() + token.size(), parsed,
                               std::chars_format::general);
      } else {
        return std::from_chars(token.data(), token.data() + token.size(), parsed);
      }
    }();
    if (result.ec == std::errc::result_out_of_range) {
      throw exception::numeric_range{in_stream, "JSON number is out of range", limits.diagnostics};
    }
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
      fail("JSON number does not match the destination type");
    }
    if constexpr (std::floating_point<T>) {
      if (!std::isfinite(parsed)) { fail("Non-finite JSON number"); }
    }
    read_bytes(token.size());
    value = parsed;
  }

  // Read a lowercase JSON literal and reject suffixes such as truex.
  void read_bool(bool& value) {
    const auto literal = peek() == 't' ? std::string_view{"true"} : std::string_view{"false"};
    require_input(literal.size());
    if (std::memcmp(in_stream.curr(), literal.data(), literal.size()) != 0) {
      fail("Invalid JSON boolean");
    }
    const auto bytes = available_input();
    if (bytes.size() > literal.size() && !is_boundary(bytes[literal.size()])) {
      fail("Invalid JSON boolean suffix");
    }
    if (bytes.size() == literal.size() && bytes.size() < in_stream.remaining_buffer()) {
      fail_limit("Boolean scan exceeded input or work limit");
    }
    read_bytes(literal.size());
    value = literal == "true";
  }

  // Replace a vector, move parsed elements, and account for capacity before growing it.
  void read_vector(type_check::vector auto& value) {
    auto nesting = enter_object();
    check_and_increase('[');
    value.clear();
    skip_whitespace();
    std::size_t count{};
    std::size_t accounted_capacity{};
    if (peek() != ']') {
      while (true) {
        if (count >= limits.max_collection_elements || count >= value.max_size()) {
          fail_limit("Collection element limit exceeded");
        }
        ++count;
        using value_type = typename std::remove_reference_t<decltype(value)>::value_type;
        if (count > accounted_capacity) {
          const auto maximum = std::min(limits.max_collection_elements, value.max_size());
          const auto requested = count <= value.capacity() ? count :
              std::max(count, value.capacity() > maximum / 2 ? maximum : value.capacity() * 2);
          charge_allocation(requested - accounted_capacity, sizeof(value_type));
          if (requested > value.capacity()) { value.reserve(requested); }
          accounted_capacity = requested;
        }
        value_type element{};
        serialize_in(element);
        value.emplace_back(std::move(element));
        skip_whitespace();
        if (peek() == ']') { break; }
        check_and_increase(',');
        skip_whitespace();
      }
    }
    check_and_increase(']');
  }

  // Replace an ordered map; duplicate keys use the last complete entry.
  void read_map(type_check::map auto& value) {
    auto nesting = enter_object();
    check_and_increase('[');
    value.clear();
    skip_whitespace();
    std::size_t count{};
    if (peek() != ']') {
      while (true) {
        if (count >= limits.max_collection_elements || count >= value.max_size()) {
          fail_limit("Collection element limit exceeded");
        }
        ++count;
        using T = std::remove_reference_t<decltype(value)>;
        // Include a conservative node-link allowance; allocator overhead is implementation-specific.
        charge_allocation(1, sizeof(typename T::value_type) + 4 * sizeof(void*));
        auto entry_nesting = enter_object();
        check_and_increase('{');
        std::string scratch;
        if (serialize_in_get_key(scratch) != constants::map_key_name) { fail("Expected map key"); }
        typename T::key_type key{};
        serialize_in(key);
        skip_whitespace();
        check_and_increase(',');
        if (serialize_in_get_key(scratch) != constants::map_value_name) { fail("Expected map value"); }
        typename T::mapped_type element{};
        serialize_in(element);
        skip_whitespace();
        check_and_increase('}');
        value.insert_or_assign(std::move(key), std::move(element));
        skip_whitespace();
        if (peek() == ']') { break; }
        check_and_increase(',');
        skip_whitespace();
      }
    }
    check_and_increase(']');
  }

public:
  // Reuse the borrowed-name path for generated ordinary enum fields.
  template <typename T>
  void serialize_in_named_enum(T& value) {
    static_assert(std::is_enum_v<T>);
    serialize_in(value);
  }

  // Decode one value; completed fields and consumed input remain committed on later failure.
  template <typename T>
  void serialize_in(T& value) {
    charge_work();
    skip_whitespace();
    if constexpr (std::same_as<T, std::nullptr_t>) {
      constexpr std::string_view literal = "null";
      require_input(literal.size());
      if (std::memcmp(in_stream.curr(), literal.data(), literal.size()) != 0) { fail("Invalid JSON null"); }
      const auto bytes = available_input();
      if (bytes.size() > literal.size() && !is_boundary(bytes[literal.size()])) { fail("Invalid JSON null suffix"); }
      if (bytes.size() == literal.size() && bytes.size() < in_stream.remaining_buffer()) {
        fail_limit("Null scan exceeded input or work limit");
      }
      read_bytes(literal.size());
      value = nullptr;
    } else if constexpr (std::same_as<T, bool>) {
      read_bool(value);
    } else if constexpr (std::same_as<T, char>) {
      std::string character;
      read_string(character, false);
      if (character.size() != sizeof(char)) { fail("Expected one byte for a char value"); }
      value = character[0];
    } else if constexpr (std::integral<T> || std::floating_point<T>) {
      read_number(value);
    } else if constexpr (std::same_as<T, std::string>) {
      read_string(value, false);
    } else if constexpr (std::is_enum_v<T>) {
      std::string scratch;
      const auto name = read_string(scratch, true);
      if constexpr (requires { serializer_enum_from_name(value, name); }) {
        try {
          serializer_enum_from_name(value, name);
        } catch (const std::invalid_argument&) {
          fail("Unknown enum name");
        }
      } else {
        throw exception::bad_type{in_stream, "Enum has no JSON name mapping", limits.diagnostics};
      }
    } else if constexpr (type_check::serializer_in_enabled_ptr<T, json>) {
      if (!value) { fail("Null destination object"); }
      value->serialize_in(*this);
    } else if constexpr (type_check::serializer_in_enabled_value<T, json>) {
      value.serialize_in(*this);
    } else if constexpr (type_check::vector<T>) {
      read_vector(value);
    } else if constexpr (type_check::map<T>) {
      read_map(value);
    } else {
      throw exception::bad_type{in_stream, "Unsupported JSON destination", limits.diagnostics};
    }
  }

  // Decode an object, including an empty object; duplicate fields apply in input order.
  template <typename T>
  void struct_serialize_in(T* obj) {
    auto nesting = enter_object();
    skip_whitespace();
    check_and_increase('{');
    skip_whitespace();
    std::size_t count{};
    if (peek() != '}') {
      std::string scratch;
      while (true) {
        if (count >= limits.max_collection_elements) { fail_limit("Object field limit exceeded"); }
        ++count;
        charge_work();
        const auto key = serialize_in_get_key(scratch);
        obj->serialize_in_member_by_name(*this, key);
        skip_whitespace();
        if (peek() == '}') { break; }
        check_and_increase(',');
        skip_whitespace();
      }
    }
    check_and_increase('}');
  }

  // Require an exact JSON message after decoding, allowing trailing JSON whitespace.
  void finish() {
    skip_whitespace();
    if (!in_stream.full()) { fail("Trailing data after JSON value"); }
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
    tab_string.append(format_definition.indent_text);
    if (format_definition.newline_after_braces_open ||
        format_definition.newline_before_object_member) {
      out_stream.write('\n');
      out_stream.write(tab_string);
      newline_written = true;
    }
  }

  // Apply configured whitespace before an object closing brace.
  inline void before_brace_close() {
    if (tab_string.size() < format_definition.indent_text.size()) {
      throw std::logic_error{"Unbalanced JSON formatter braces"};
    }
    tab_string.resize(tab_string.size() - format_definition.indent_text.size());
    if (format_definition.newline_before_braces_close) {
      out_stream.write('\n');
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
    tab_string.append(format_definition.indent_text);
    if (format_definition.newline_after_bracket_open) {
      out_stream.write('\n');
      out_stream.write(tab_string);
      newline_written = true;
    }
  }

  // Apply configured whitespace before an array closing bracket.
  inline void before_bracket_close() {
    if (tab_string.size() < format_definition.indent_text.size()) {
      throw std::logic_error{"Unbalanced JSON formatter brackets"};
    }
    tab_string.resize(tab_string.size() - format_definition.indent_text.size());
    if (format_definition.newline_before_bracket_close) {
      out_stream.write('\n');
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

  // Write collection elements in iteration order.
  template <typename T, typename Serialize>
  void serialize_out_list(const T& value_list, Serialize&& serialize_func) {
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

  // Validate and quote UTF-8, retaining the allocation-free path for ordinary text.
  void write_string(std::string_view text) {
    const auto encoded_size = detail::json_escaped_size(text);
    before_data();
    if (encoded_size == text.size()) {
      out_stream.write('"', text, '"');
    } else {
      const auto escaped = detail::escape_json_string(text, encoded_size);
      out_stream.write('"', escaped, '"');
    }
  }

public:
  // Encode a supported value or field through this protocol and advance the output cursor.
  template <typename T>
  void serialize_out(const T& value) {
    if constexpr (std::same_as<T, std::nullptr_t>) {
      before_data();
      out_stream.append("null");
    } else if constexpr (std::is_same_v<T, char>) {
      write_string(std::string_view{&value, sizeof(value)});
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
      if (!std::isfinite(value)) {
        throw std::invalid_argument{"JSON cannot encode non-finite floating-point values"};
      }
      // Shortest general-format output round-trips to the same floating-point value.
      constexpr std::size_t exponent_sign_and_punctuation_bytes = 16;
      char buffer[std::numeric_limits<T>::max_digits10 + exponent_sign_and_punctuation_bytes];
      const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value,
                                        std::chars_format::general);
      if (result.ec != std::errc{}) {
        throw std::runtime_error{"Unable to format JSON floating-point value"};
      }
      before_data();
      out_stream.append_external(buffer, static_cast<std::size_t>(result.ptr - buffer));
    } else if constexpr (std::same_as<T, std::string>) {
      write_string(value);
    } else if constexpr (std::same_as<T, std::string_view>) {
      write_string(value);
    } else if constexpr (std::is_array_v<T> &&
                         std::same_as<std::remove_extent_t<T>, char>) {
      constexpr auto extent = std::extent_v<T>;
      write_string(std::string_view{value, value[extent - 1] == '\0' ? extent - 1 : extent});
    } else if constexpr (std::is_enum_v<T>) {
      write_string(detail::enum_name(value));
    } else if constexpr (type_check::serializer_out_enabled_ptr<T, json<serialize_type::out>>) {
      if (!value) { throw std::invalid_argument{"Null source object"}; }
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

  // Encode an object with no fields using both braces.
  void struct_serialize_out_empty() {
    write_brace_open();
    write_brace_close();
  }
}; // class json_out<>

template <>
class json<serialize_type::out> : public json_out<false> {
public:
  using json_out<false>::json_out;
};

template <serialize_type type, serialize_key_type KeyType,
          std::endian WireEndian = std::endian::little>
class binary {};

template <serialize_key_type KeyType, std::endian WireEndian = std::endian::little>
class binary_in_base : public detail::decoder_input {
public:
  constexpr static serialize_key_type key_type = KeyType;
  constexpr static std::endian wire_endian = WireEndian;
  static_assert(KeyType == serialize_key_type::none || KeyType == serialize_key_type::integer ||
                KeyType == serialize_key_type::string, "Unsupported binary key mode");
  static_assert(WireEndian == std::endian::little || WireEndian == std::endian::big,
                "Binary storage requires little-endian or big-endian byte order");
  using detail::decoder_input::decoder_input;

  // Decode a complete one-to-four-byte compact integer with one cursor update.
  std::uint32_t serialize_in_variable() {
    require_input(1);
    const auto first = *in_stream.curr();
    constexpr unsigned length_tag_shift = 6;
    const auto size = static_cast<std::size_t>((first & constants::variable_tag_mask) >> length_tag_shift) + 1;
    const auto* bytes = read_bytes(size);
    std::uint32_t value = first & constants::variable_payload_mask;
    for (std::size_t index = 1; index < size; ++index) {
      value = (value << constants::wire_byte_bits) | bytes[index];
    }
    return value;
  }

  // Borrow a length-prefixed name for immediate dispatch; callers must retain the input storage.
  std::string_view serialize_in_name() {
    const auto size = serialize_in_variable();
    check_string(size, false);
    const auto* bytes = read_bytes(size);
    return size == 0 ? std::string_view{} :
        std::string_view{reinterpret_cast<const char*>(bytes), size};
  }

  // Decode a string-key enum field without allocating an owning name.
  template <typename T>
  void serialize_in_named_enum(T& value) {
    static_assert(KeyType == serialize_key_type::string && std::is_enum_v<T>);
    charge_work();
    const auto name = serialize_in_name();
    try {
      serializer_enum_from_name(value, name);
    } catch (const std::invalid_argument&) {
      throw exception::bad_input_data{in_stream, "Unknown enum name", limits.diagnostics};
    }
  }

  // Decode a scalar without unaligned typed access, or replace a validated collection.
  template <typename T>
  void serialize_in(T& value) {
    charge_work();
    if constexpr (std::same_as<T, char>) {
      value = static_cast<char>(*read_bytes(sizeof(char)));
    } else if constexpr (std::same_as<T, bool>) {
      require_input(1);
      if (*in_stream.curr() > 1) {
        throw exception::bad_input_data{in_stream, "Invalid binary boolean", limits.diagnostics};
      }
      value = *read_bytes(1) != 0;
    } else if constexpr (std::is_enum_v<T>) {
      const auto encoded = serialize_in_variable();
      using underlying_type = std::underlying_type_t<T>;
      if (encoded > static_cast<std::uintmax_t>(std::numeric_limits<underlying_type>::max())) {
        throw exception::numeric_range{in_stream, "Binary enum exceeds its underlying type", limits.diagnostics};
      }
      const auto candidate = static_cast<T>(encoded);
      if constexpr (requires { serializer_enum_valid(candidate); }) {
        if (!serializer_enum_valid(candidate)) {
          throw exception::bad_input_data{in_stream, "Unknown binary enum value", limits.diagnostics};
        }
      }
      value = candidate;
    } else if constexpr (std::integral<T>) {
      static_assert(rohit::detail::endian_integer<T>, "Binary integers must have no padding bits");
      using wire_type = std::make_unsigned_t<T>;
      wire_type source;
      const auto* bytes = read_bytes(sizeof(source));
      std::memcpy(&source, bytes, sizeof(source));
      source = change_endian<WireEndian, std::endian::native>(source);
      value = std::bit_cast<T>(source);
    } else if constexpr (std::floating_point<T>) {
      static_assert(rohit::detail::endian_floating_point<T>,
                    "Binary floats require 32-bit or 64-bit binary IEC 559 storage");
      using wire_type = std::conditional_t<sizeof(T) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;
      wire_type source;
      std::memcpy(&source, read_bytes(sizeof(source)), sizeof(source));
      value = std::bit_cast<T>(change_endian<WireEndian, std::endian::native>(source));
    } else if constexpr (std::same_as<T, std::string>) {
      const auto size = serialize_in_variable();
      require_input(size);
      check_string(size);
      if (size > value.max_size()) { fail_limit("String exceeds destination capacity limit"); }
      if (size == 0) {
        value.clear();
      } else {
        value.assign(reinterpret_cast<const char*>(in_stream.curr()), size);
      }
      read_bytes(size);
    } else if constexpr (type_check::serializer_in_enabled_ptr<T, binary_in_base>) {
      if (!value) { throw exception::bad_input_data{in_stream, "Null destination object", limits.diagnostics}; }
      value->serialize_in(*this);
    } else if constexpr (type_check::serializer_in_enabled_value<T, binary_in_base>) {
      value.serialize_in(*this);
    } else if constexpr (type_check::vector<T>) {
      auto nesting = enter_object();
      const auto count = serialize_in_variable();
      if (count > value.max_size()) { fail_limit("Vector exceeds destination capacity limit"); }
      using value_type = typename T::value_type;
      check_collection(count, sizeof(value_type));
      // Fixed-width primitives also permit a byte-range check before any allocation.
      if constexpr (std::integral<value_type> || std::floating_point<value_type>) {
        constexpr std::size_t wire_element_bytes = std::same_as<value_type, bool> ? 1 : sizeof(value_type);
        if (count > in_stream.remaining_buffer() / wire_element_bytes) {
          throw exception::bad_input_data{in_stream, "Truncated binary vector", limits.diagnostics};
        }
        require_input(static_cast<std::size_t>(count) * wire_element_bytes);
      }
      value.clear();
      value.reserve(count);
      for (std::size_t index = 0; index < count; ++index) {
        value_type element{};
        serialize_in(element);
        value.emplace_back(std::move(element));
      }
    } else if constexpr (type_check::map<T>) {
      auto nesting = enter_object();
      const auto count = serialize_in_variable();
      if (count > value.max_size()) { fail_limit("Map exceeds destination capacity limit"); }
      check_collection(count, sizeof(typename T::value_type) + 4 * sizeof(void*));
      value.clear();
      for (std::size_t index = 0; index < count; ++index) {
        typename T::key_type key{};
        typename T::mapped_type element{};
        serialize_in(key);
        serialize_in(element);
        value.insert_or_assign(std::move(key), std::move(element));
      }
    } else {
      throw exception::bad_type{in_stream, "Unsupported binary destination", limits.diagnostics};
    }
  }

  // Dispatch keyed fields without owning names; unknown fields are rejected by the generated schema.
  template <typename T>
  void struct_serialize_in(T* obj) {
    static_assert(KeyType != serialize_key_type::none,
                  "Positional objects must read their fields in schema order");
    auto nesting = enter_object();
    std::size_t count{};
    while (true) {
      if constexpr (KeyType == serialize_key_type::integer) {
        const auto key = serialize_in_variable();
        if (key == constants::binary_object_end_id) { break; }
        if (count >= limits.max_collection_elements) { fail_limit("Object field limit exceeded"); }
        ++count;
        charge_work();
        obj->serialize_in_member_by_identifier(*this, key);
      } else {
        const auto key = serialize_in_name();
        if (key.empty()) { break; }
        if (count >= limits.max_collection_elements) { fail_limit("Object field limit exceeded"); }
        ++count;
        charge_work();
        obj->serialize_in_member_by_name(*this, key);
      }
    }
  }

  // Require an exact binary message after decoding.
  void finish() const {
    if (!in_stream.full()) {
      throw exception::bad_input_data{in_stream, "Trailing binary data", limits.diagnostics};
    }
  }
}; // class binary_in_base

template <serialize_key_type KeyType, std::endian WireEndian>
class binary<serialize_type::in, KeyType, WireEndian>
    : public binary_in_base<KeyType, WireEndian> {
public:
  using binary_in_base<KeyType, WireEndian>::binary_in_base;
};

template <serialize_key_type KeyType, std::endian WireEndian = std::endian::little>
class binary_out_base {
public:
  constexpr static serialize_key_type key_type = KeyType;
  constexpr static std::endian wire_endian = WireEndian;
  static_assert(KeyType == serialize_key_type::none || KeyType == serialize_key_type::integer ||
                KeyType == serialize_key_type::string, "Unsupported binary key mode");
  static_assert(WireEndian == std::endian::little || WireEndian == std::endian::big,
                "Binary storage requires little-endian or big-endian byte order");

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
      if constexpr (requires { serializer_enum_valid(value); }) {
        if (!serializer_enum_valid(value)) {
          throw std::invalid_argument{"Unknown binary enum value"};
        }
      }
      serialize_out_variable(static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (std::integral<T>) {
      static_assert(rohit::detail::endian_integer<T>, "Binary integers must have no padding bits");
      using wire_type = std::make_unsigned_t<T>;
      const auto wire_value = change_endian<std::endian::native, WireEndian>(
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
      static_assert(rohit::detail::endian_floating_point<T>,
                    "Binary floating-point output requires a 32-bit or 64-bit IEC 559 representation");
      // Preserve scalar bits, then use the integer path for the selected wire byte order.
      if constexpr (sizeof(T) == sizeof(std::uint32_t)) {
        serialize_out(std::bit_cast<std::uint32_t>(value));
      } else if constexpr (sizeof(T) == sizeof(std::uint64_t)) {
        serialize_out(std::bit_cast<std::uint64_t>(value));
      }
    } else if constexpr (type_check::serializer_out_enabled_ptr<
                             T, binary<serialize_type::out, KeyType, WireEndian>>) {
      if (!value) { throw std::invalid_argument{"Null source object"}; }
      value->serialize_out(*this);
    } else if constexpr (type_check::serializer_out_enabled<T,
                                                            binary<serialize_type::out, KeyType, WireEndian>>) {
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

  // Empty binary objects use the same terminator as any other object in their mode.
  void struct_serialize_out_empty() {
    struct_serialize_out_end();
  }
}; // class binary_out_base

template <serialize_key_type KeyType, std::endian WireEndian>
class binary<serialize_type::out, KeyType, WireEndian>
    : public binary_out_base<KeyType, WireEndian> {
public:
  using binary_out_base<KeyType, WireEndian>::binary_out_base;
};

template <serialize_type type>
using binary_integer = binary<type, serialize_key_type::integer>;

template <serialize_type type>
using binary_string = binary<type, serialize_key_type::string>;

template <serialize_type type>
using binary_none = binary<type, serialize_key_type::none>;

} // namespace rohit::serializer
