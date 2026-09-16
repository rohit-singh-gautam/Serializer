#pragma once

#include <rohit/runtime_simd.hpp>
#include <rohit/serializer.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace rohit::serializer {
// Select a standard Protobuf representation at compile time.
enum class protobuf_format { binary, json, text };

namespace detail {
inline constexpr std::uint32_t protobuf_max_field = 536870911;
inline constexpr unsigned protobuf_tag_bits = 3;
inline constexpr unsigned protobuf_varint_bits = 7;
inline constexpr std::uint8_t protobuf_continuation = 0x80;

// Enforce the Protobuf field-number contract without renumbering schema IDs.
constexpr bool protobuf_valid_id(std::uint32_t id) {
  return id != 0 && id <= protobuf_max_field && (id < 19000 || id > 19999);
}

// Select a scalar's wire kind entirely from its generated C++ type.
template <typename T>
constexpr unsigned protobuf_wire_type() {
  if constexpr (std::same_as<T, float>) {
    return 5;
  } else if constexpr (std::same_as<T, double>) {
    return 1;
  } else if constexpr (std::integral<T> || std::is_enum_v<T>) {
    return 0;
  } else {
    return 2;
  }
}

// Construct standard Protobuf defaults rather than carrying Serializer schema initializers.
template <typename T>
void protobuf_reset(T& value) {
  if constexpr (requires { value.serializer_protobuf_reset(); }) {
    value.serializer_protobuf_reset();
  } else {
    value = T{};
  }
}
} // namespace detail

template <serialize_type Direction, protobuf_format Format>
class protobuf_codec;

template <protobuf_format Format>
class protobuf_codec<serialize_type::out, Format> {
  stream& output;
  bool first{true};

  // Encode one base-128 value without allocating temporary storage.
  void varint(std::uint64_t value) {
    std::array<std::uint8_t, 10> bytes{};
    std::size_t size{};
    do {
      bytes[size] = static_cast<std::uint8_t>(value & (detail::protobuf_continuation - 1));
      value >>= detail::protobuf_varint_bits;
      if (value != 0) {
        bytes[size] |= detail::protobuf_continuation;
      }
      ++size;
    } while (value != 0);
    output.append_external(bytes.data(), size);
  }

  // Quote a UTF-8 value using JSON escapes, which are also valid TextProto string escapes.
  void quoted(std::string_view value) {
    json_out<false> writer{output};
    writer.serialize_out(value);
  }

  // Emit a format-specific field prefix; object separators remain local to this writer.
  template <std::uint32_t Id>
  void prefix(std::string_view name, std::string_view json_name, unsigned wire) {
    static_assert(detail::protobuf_valid_id(Id), "Invalid Protobuf field number");
    if constexpr (Format == protobuf_format::binary) {
      varint((static_cast<std::uint64_t>(Id) << detail::protobuf_tag_bits) | wire);
    } else if constexpr (Format == protobuf_format::json) {
      if (!first) {
        output.write(',');
      }
      first = false;
      quoted(json_name);
      output.write(':');
    } else {
      output.write(name, ": ");
    }
  }

  // Encode a typed scalar with standard Protobuf numeric and string mappings.
  template <typename T>
  void scalar(const T& value) {
    if constexpr (Format == protobuf_format::binary) {
      if constexpr (std::is_enum_v<T>) {
        if (!serializer_enum_valid(value)) {
          throw std::invalid_argument{"Unknown enum value"};
        }
        varint(static_cast<std::uint64_t>(value));
      } else if constexpr (std::same_as<T, char>) {
        varint(static_cast<unsigned char>(value));
      } else if constexpr (std::integral<T>) {
        varint(static_cast<std::uint64_t>(value));
      } else if constexpr (std::floating_point<T>) {
        using Bits =
            std::conditional_t<sizeof(T) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;
        auto bits = std::bit_cast<Bits>(value);
        std::array<std::uint8_t, sizeof(T)> bytes{};
        for (auto& byte : bytes) {
          byte = static_cast<std::uint8_t>(bits);
          bits >>= 8;
        }
        output.append_external(bytes.data(), bytes.size());
      } else {
        static_cast<void>(detail::json_escaped_size(value));
        varint(value.size());
        output.append_external(value.data(), value.size());
      }
    } else if constexpr (std::is_enum_v<T>) {
      const auto name = detail::enum_name(value);
      if constexpr (Format == protobuf_format::json) {
        quoted(name);
      } else {
        output.write(name);
      }
    } else if constexpr (std::same_as<T, std::string> || std::same_as<T, std::string_view>) {
      quoted(value);
    } else if constexpr (std::same_as<T, bool>) {
      output.write(std::string_view{value ? "true" : "false"});
    } else if constexpr (std::floating_point<T>) {
      if (std::isfinite(value)) {
        json_out<false> writer{output};
        writer.serialize_out(value);
      } else if constexpr (Format == protobuf_format::json) {
        quoted(std::isnan(value) ? "NaN" : value < 0 ? "-Infinity" : "Infinity");
      } else {
        output.write(std::string_view{std::isnan(value) ? "nan" : value < 0 ? "-inf" : "inf"});
      }
    } else if constexpr (std::same_as<T, char>) {
      output.append_string(static_cast<unsigned>(static_cast<unsigned char>(value)));
    } else {
      constexpr bool quote_integer =
          Format == protobuf_format::json && sizeof(T) == sizeof(std::uint64_t);
      if constexpr (quote_integer) {
        output.write('"');
      }
      output.append_string(value);
      if constexpr (quote_integer) {
        output.write('"');
      }
    }
  }

  // Encode a scalar or an object as a JSON array/map value.
  template <typename T>
  void json_value(const T& value) {
    if constexpr (requires { value.serializer_protobuf_write(*this); }) {
      protobuf_codec nested{output};
      nested.protobuf_object(value);
    } else {
      scalar(value);
    }
  }

public:
  static constexpr serialize_key_type key_type = serialize_key_type::integer;
  // Borrow the output stream; protocol selection has no runtime discriminator.
  explicit protobuf_codec(stream& output) : output{output} {}

  // Encode the root message without an extra binary length prefix or TextProto braces.
  template <typename T>
  void protobuf_object(const T& value) {
    first = true;
    if constexpr (Format == protobuf_format::json) {
      output.write('{');
    }
    value.serializer_protobuf_write(*this);
    if constexpr (Format == protobuf_format::json) {
      output.write('}');
    }
  }

  // Support direct protocol calls as well as the generated serialize_out overload.
  template <typename T>
  void serialize_out(const T& value) {
    protobuf_object(value);
  }

  // Encode a generated message callback, including parent and union wrapper fields.
  template <std::uint32_t Id, typename Write>
  void message_field(std::string_view name, std::string_view json_name, Write&& write) {
    prefix<Id>(name, json_name, 2);
    if constexpr (Format == protobuf_format::binary) {
      full_stream_auto_alloc bytes;
      protobuf_codec nested{bytes};
      write(nested);
      varint(bytes.current_offset());
      output.append_external(bytes.begin(), bytes.current_offset());
    } else {
      output.write('{');
      protobuf_codec nested{output};
      write(nested);
      output.write('}');
      if constexpr (Format == protobuf_format::text) {
        output.write('\n');
      }
    }
  }

  // Encode one statically numbered field; arrays and maps retain their native wire shapes.
  template <std::uint32_t Id, typename T>
  void field(std::string_view name, std::string_view json_name, const T& value) {
    if constexpr (type_check::vector<T>) {
      using Element = typename T::value_type;
      if constexpr (Format == protobuf_format::json) {
        prefix<Id>(name, json_name, 2);
        output.write('[');
        bool initial = true;
        for (const Element& element : value) {
          if (!initial) {
            output.write(',');
          }
          initial = false;
          json_value(element);
        }
        output.write(']');
      } else if constexpr (Format == protobuf_format::binary &&
                           detail::protobuf_wire_type<Element>() != 2) {
        if (!value.empty()) {
          full_stream_auto_alloc bytes;
          protobuf_codec nested{bytes};
          for (const Element& element : value) {
            nested.scalar(element);
          }
          prefix<Id>(name, json_name, 2);
          varint(bytes.current_offset());
          output.append_external(bytes.begin(), bytes.current_offset());
        }
      } else {
        for (const Element& element : value) {
          field<Id>(name, json_name, element);
        }
      }
    } else if constexpr (type_check::map<T>) {
      if constexpr (Format == protobuf_format::json) {
        prefix<Id>(name, json_name, 2);
        output.write('{');
        bool initial = true;
        for (const auto& [key, mapped] : value) {
          if (!initial) {
            output.write(',');
          }
          initial = false;
          if constexpr (std::same_as<typename T::key_type, std::string>) {
            quoted(key);
          } else {
            output.write('"');
            if constexpr (std::same_as<typename T::key_type, bool>) {
              output.write(std::string_view{key ? "true" : "false"});
            } else if constexpr (std::same_as<typename T::key_type, char>) {
              output.append_string(static_cast<unsigned>(static_cast<unsigned char>(key)));
            } else {
              output.append_string(key);
            }
            output.write('"');
          }
          output.write(':');
          json_value(mapped);
        }
        output.write('}');
      } else {
        for (const auto& [key, mapped] : value) {
          message_field<Id>(name, json_name, [&](auto& entry) {
            entry.template field<1>("key", "key", key);
            entry.template field<2>("value", "value", mapped);
          });
        }
      }
    } else if constexpr (requires { value.serializer_protobuf_write(*this); }) {
      message_field<Id>(name, json_name,
                        [&](auto& nested) { value.serializer_protobuf_write(nested); });
    } else {
      prefix<Id>(name, json_name, detail::protobuf_wire_type<T>());
      scalar(value);
      if constexpr (Format == protobuf_format::text) {
        output.write('\n');
      }
    }
  }
};

template <protobuf_format Format>
class protobuf_codec<serialize_type::in, Format> : public json<serialize_type::in> {
  using base = json<serialize_type::in>;
  using base::available_input;
  using base::charge_allocation;
  using base::charge_work;
  using base::check_and_increase;
  using base::check_string;
  using base::fail;
  using base::fail_limit;
  using base::in_stream;
  using base::limits;
  using base::peek;
  using base::read_bytes;
  using base::require_input;
  const std::uint8_t* message_end{};
  std::uint32_t current_id{};
  unsigned wire{};
  std::string current_name{};
  bool first{true};
  char closer{};
  std::size_t collection_elements{};

  // Report the remaining bytes in this nested message without pointer subtraction on null.
  std::size_t remaining() const {
    return message_end == in_stream.curr()
               ? 0
               : static_cast<std::size_t>(message_end - in_stream.curr());
  }

  // Require bytes within both the current nested message and the shared input/work budgets.
  const std::uint8_t* take(std::size_t size) {
    if (size > remaining()) {
      fail("Truncated Protobuf field");
    }
    return read_bytes(size);
  }

  // Decode at most ten bytes, rejecting truncated and overflowing uint64 values.
  std::uint64_t varint() {
    std::uint64_t value{};
    constexpr unsigned final_shift = 63;
    for (unsigned shift = 0; shift <= final_shift; shift += detail::protobuf_varint_bits) {
      const auto byte = *take(1);
      if (shift == final_shift && byte > 1) {
        fail("Protobuf varint overflow");
      }
      value |= static_cast<std::uint64_t>(byte & (detail::protobuf_continuation - 1)) << shift;
      if ((byte & detail::protobuf_continuation) == 0) {
        return value;
      }
    }
    fail("Protobuf varint overflow");
  }

  // Read and validate a length before any allocation or boundary adjustment.
  std::size_t length() {
    const auto size = varint();
    if (size > remaining()) {
      fail("Truncated Protobuf length-delimited field");
    }
    return static_cast<std::size_t>(size);
  }

  // Skip legal whitespace and TextProto line comments within the message bounds.
  void whitespace() {
    if constexpr (Format == protobuf_format::json) {
      const auto bytes = available_input();
      const auto size = std::min(bytes.size(), remaining());
      const auto count =
          detail::scan_json_prefix<detail::json_scan_kind::whitespace>(bytes.data(), size);
      take(count);
      // Exhausting a budget before the next byte must fail at the consumed cursor.
      if (count == size && remaining() != 0) {
        require_input(1);
      }
    } else {
      while (remaining() != 0) {
        const auto byte = peek();
        if (base::is_whitespace(byte)) {
          take(1);
        } else if constexpr (Format == protobuf_format::text) {
          if (byte != '#') {
            break;
          }
          while (remaining() != 0 && peek() != '\n') {
            take(1);
          }
        } else {
          break;
        }
      }
    }
  }

  // Read a TextProto identifier or numeric token without accepting adjacent punctuation.
  std::string token() {
    whitespace();
    std::string result;
    while (remaining() != 0) {
      const auto byte = peek();
      if (base::is_whitespace(byte) || byte == ':' || byte == ',' || byte == ';' || byte == '{' ||
          byte == '}' || byte == '<' || byte == '>' || byte == '[' || byte == ']' || byte == '#') {
        break;
      }
      if (result.size() >= limits.max_string_bytes) {
        fail_limit("Protobuf token limit exceeded");
      }
      result += static_cast<char>(*take(1));
    }
    if (result.empty()) {
      fail("Expected Protobuf token");
    }
    return result;
  }

  // Decode JSON strings or TextProto's concatenated quoted strings and C-style escapes.
  std::string string_value() {
    whitespace();
    std::string result;
    if constexpr (Format == protobuf_format::json) {
      base::read_string(result, false);
    } else {
      do {
        const auto quote = *take(1);
        if (quote != '\'' && quote != '"') {
          fail("Expected TextProto string");
        }
        bool closed = false;
        while (remaining() != 0) {
          auto byte = *take(1);
          if (byte == quote) {
            closed = true;
            break;
          }
          if (byte == '\n' || byte == '\r') {
            fail("Newline in TextProto string");
          }
          if (byte == '\\') {
            byte = *take(1);
            switch (byte) {
            case 'a':
              byte = '\a';
              break;
            case 'b':
              byte = '\b';
              break;
            case 'f':
              byte = '\f';
              break;
            case 'n':
              byte = '\n';
              break;
            case 'r':
              byte = '\r';
              break;
            case 't':
              byte = '\t';
              break;
            case 'v':
              byte = '\v';
              break;
            case '\\':
            case '\'':
            case '"':
            case '?':
              break;
            case 'x':
            case 'X': {
              unsigned value{};
              unsigned count{};
              while (count < 2 && remaining() != 0) {
                const auto character = peek();
                const int digit = character >= '0' && character <= '9'   ? character - '0'
                                  : character >= 'a' && character <= 'f' ? character - 'a' + 10
                                  : character >= 'A' && character <= 'F' ? character - 'A' + 10
                                                                         : -1;
                if (digit < 0) {
                  break;
                }
                value = value * 16 + static_cast<unsigned>(digit);
                take(1);
                ++count;
              }
              if (count == 0) {
                fail("Invalid TextProto hex escape");
              }
              byte = static_cast<std::uint8_t>(value);
              break;
            }
            case 'u':
            case 'U': {
              const auto digits = byte == 'u' ? 4U : 8U;
              const auto* bytes = take(digits);
              auto point = detail::read_hex_quad({bytes, 4});
              if (digits == 8) {
                point = (point << 16) | detail::read_hex_quad({bytes + 4, 4});
              }
              if (point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) {
                fail("Invalid TextProto Unicode escape");
              }
              detail::append_utf8(result, point);
              if (result.size() > limits.max_string_bytes) {
                fail_limit("String length limit exceeded");
              }
              continue;
            }
            default:
              if (byte < '0' || byte > '7') {
                fail("Invalid TextProto escape");
              }
              unsigned value = byte - '0';
              for (unsigned count = 1;
                   count < 3 && remaining() != 0 && peek() >= '0' && peek() <= '7'; ++count) {
                value = value * 8 + (*take(1) - '0');
              }
              if (value > 255) {
                fail("TextProto octal escape out of range");
              }
              byte = static_cast<std::uint8_t>(value);
            }
          }
          if (result.size() >= limits.max_string_bytes) {
            fail_limit("String length limit exceeded");
          }
          result += static_cast<char>(byte);
        }
        if (!closed) {
          fail("Unterminated TextProto string");
        }
        whitespace();
      } while (remaining() != 0 && (peek() == '\'' || peek() == '"'));
      check_string(result.size());
      static_cast<void>(detail::json_escaped_size(result));
    }
    return result;
  }

  // Parse a numeric token, permitting ProtoJSON exponent notation for integral destinations.
  template <typename T>
  void numeric(std::string text, T& value) {
    if constexpr (std::same_as<T, char>) {
      std::uint32_t number{};
      numeric(std::move(text), number);
      if (number > std::numeric_limits<unsigned char>::max()) {
        fail("Protobuf char out of range");
      }
      value = static_cast<char>(static_cast<unsigned char>(number));
    } else if constexpr (std::floating_point<T>) {
      if (text == "NaN" || text == "nan") {
        value = std::numeric_limits<T>::quiet_NaN();
        return;
      }
      if (text == "Infinity" || text == "inf" || text == "infinity") {
        value = std::numeric_limits<T>::infinity();
        return;
      }
      if (text == "-Infinity" || text == "-inf" || text == "-infinity") {
        value = -std::numeric_limits<T>::infinity();
        return;
      }
      if constexpr (Format == protobuf_format::text) {
        if (!text.empty() && (text.back() == 'f' || text.back() == 'F')) {
          text.pop_back();
        }
        if (!text.empty() && text.front() == '+') {
          text.erase(0, 1);
        }
      }
      T parsed{};
      const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
      if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
          !std::isfinite(parsed)) {
        fail("Invalid Protobuf floating-point value");
      }
      value = parsed;
    } else {
      // Normalize decimal exponent notation exactly, avoiding a lossy float intermediate.
      if constexpr (Format == protobuf_format::json) {
        const auto exponent_at = text.find_first_of("eE");
        int exponent{};
        if (exponent_at != std::string::npos) {
          auto suffix = std::string_view{text}.substr(exponent_at + 1);
          if (!suffix.empty() && suffix.front() == '+') {
            suffix.remove_prefix(1);
          }
          const auto parsed =
              std::from_chars(suffix.data(), suffix.data() + suffix.size(), exponent);
          if (parsed.ec != std::errc{} || parsed.ptr != suffix.data() + suffix.size() ||
              exponent < -1000 || exponent > 1000) {
            fail("Invalid Protobuf integer exponent");
          }
          text.resize(exponent_at);
        }
        const auto dot = text.find('.');
        if (dot != std::string::npos) {
          exponent -= static_cast<int>(text.size() - dot - 1);
          text.erase(dot, 1);
        }
        const auto digits_begin = !text.empty() && text.front() == '-' ? 1U : 0U;
        if (text.size() > digits_begin &&
            text.find_first_not_of('0', digits_begin) == std::string::npos) {
          value = 0;
          return;
        }
        while (exponent < 0 && text.size() > digits_begin + 1 && text.back() == '0') {
          text.pop_back();
          ++exponent;
        }
        if (exponent < 0 || exponent > 20) {
          fail("Nonintegral or out-of-range Protobuf integer");
        }
        text.append(static_cast<std::size_t>(exponent), '0');
      }
      bool negative = !text.empty() && text.front() == '-';
      std::string_view digits{text};
      if (negative) {
        digits.remove_prefix(1);
      }
      int radix = 10;
      if constexpr (Format == protobuf_format::text) {
        if (digits.starts_with("0x") || digits.starts_with("0X")) {
          radix = 16;
          digits.remove_prefix(2);
        } else if (digits.size() > 1 && digits.front() == '0') {
          radix = 8;
        }
      }
      std::uint64_t magnitude{};
      const auto parsed =
          std::from_chars(digits.data(), digits.data() + digits.size(), magnitude, radix);
      if (digits.empty() || parsed.ec != std::errc{} ||
          parsed.ptr != digits.data() + digits.size()) {
        fail("Invalid Protobuf integer");
      }
      if constexpr (std::unsigned_integral<T>) {
        if ((negative && magnitude != 0) || magnitude > std::numeric_limits<T>::max()) {
          fail("Protobuf integer out of range");
        }
        value = static_cast<T>(magnitude);
      } else {
        const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<T>::max());
        if (magnitude > maximum + (negative ? 1U : 0U)) {
          fail("Protobuf integer out of range");
        }
        value = negative ? static_cast<T>(-static_cast<std::int64_t>(magnitude - (magnitude != 0)) -
                                          (magnitude != 0))
                         : static_cast<T>(magnitude);
      }
    }
  }

  // Parse one scalar under the statically selected wire format.
  template <typename T>
  void scalar(T& value) {
    if constexpr (Format == protobuf_format::binary) {
      if (wire != detail::protobuf_wire_type<T>()) {
        fail("Protobuf wire type mismatch");
      }
      if constexpr (std::is_enum_v<T>) {
        const auto raw = varint();
        if (raw > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
          fail("Unknown enum value");
        }
        value = static_cast<T>(raw);
        if (!serializer_enum_valid(value)) {
          fail("Unknown enum value");
        }
      } else if constexpr (std::same_as<T, bool>) {
        value = varint() != 0;
      } else if constexpr (std::integral<T>) {
        const auto raw = varint();
        if constexpr (std::same_as<T, char>) {
          if (raw > std::numeric_limits<unsigned char>::max()) {
            fail("Protobuf char out of range");
          }
          value = static_cast<char>(static_cast<unsigned char>(raw));
        } else if constexpr (std::unsigned_integral<T>) {
          if (raw > std::numeric_limits<T>::max()) {
            fail("Protobuf integer out of range");
          }
          value = static_cast<T>(raw);
        } else {
          // int32 consumes the low 32 bits, including legal five-byte negative encodings.
          const auto signed_value = sizeof(T) <= sizeof(std::int32_t)
                                        ? static_cast<std::int64_t>(std::bit_cast<std::int32_t>(
                                              static_cast<std::uint32_t>(raw)))
                                        : std::bit_cast<std::int64_t>(raw);
          if (!std::in_range<T>(signed_value)) {
            fail("Protobuf integer out of range");
          }
          value = static_cast<T>(signed_value);
        }
      } else if constexpr (std::floating_point<T>) {
        using Bits =
            std::conditional_t<sizeof(T) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;
        const auto* bytes = take(sizeof(T));
        Bits bits{};
        for (std::size_t index = 0; index < sizeof(T); ++index) {
          bits |= static_cast<Bits>(bytes[index]) << (index * 8);
        }
        value = std::bit_cast<T>(bits);
      } else {
        const auto size = length();
        check_string(size);
        const auto* bytes = take(size);
        value.assign(reinterpret_cast<const char*>(bytes), size);
        static_cast<void>(detail::json_escaped_size(value));
      }
    } else if constexpr (std::same_as<T, std::string>) {
      value = string_value();
    } else if constexpr (std::same_as<T, bool>) {
      const auto text = token();
      if (text == "true" ||
          (Format == protobuf_format::text && (text == "True" || text == "t" || text == "1"))) {
        value = true;
      } else if (text == "false" || (Format == protobuf_format::text &&
                                     (text == "False" || text == "f" || text == "0"))) {
        value = false;
      } else {
        fail("Invalid Protobuf boolean");
      }
    } else if constexpr (std::is_enum_v<T>) {
      whitespace();
      std::string text;
      if constexpr (Format == protobuf_format::json) {
        if (peek() == '"') {
          text = string_value();
        } else {
          const auto bytes = base::number_token();
          text.assign(bytes);
          take(bytes.size());
        }
      } else {
        text = token();
      }
      if (!text.empty() && (text.front() == '-' || (text.front() >= '0' && text.front() <= '9'))) {
        int ordinal{};
        numeric(text, ordinal);
        value = static_cast<T>(ordinal);
        if (!serializer_enum_valid(value)) {
          fail("Unknown enum value");
        }
      } else {
        serializer_enum_from_name(value, text);
      }
    } else {
      whitespace();
      std::string text;
      if constexpr (Format == protobuf_format::json) {
        if (peek() == '"') {
          text = string_value();
        } else {
          const auto bytes = base::number_token();
          text.assign(bytes);
          take(bytes.size());
        }
      } else {
        text = token();
      }
      numeric(text, value);
    }
  }

  // Charge cumulative repeated/map entries before growing destination storage.
  void collection_entry(std::size_t bytes) {
    if (collection_elements >= limits.max_collection_elements) {
      fail_limit("Collection element limit exceeded");
    }
    ++collection_elements;
    charge_work();
    charge_allocation(1, bytes);
  }

public:
  static constexpr serialize_key_type key_type = serialize_key_type::integer;
  // Share one input cursor and limit budget across all nested message reads.
  explicit protobuf_codec(const stream& input, decode_limits policy = {})
      : base{input, policy},
        message_end{input.curr() == nullptr ? nullptr : input.curr() + input.remaining_buffer()} {}

  // Decoder sessions share a cursor and budget and cannot be duplicated.
  protobuf_codec(const protobuf_codec&) = delete;
  // Keep the inherited noncopyable decoder contract explicit for warning checks.
  protobuf_codec& operator=(const protobuf_codec&) = delete;

  // Match a generated field without reflection; JSON accepts both original and lowerCamel names.
  template <std::uint32_t Id>
  bool match(std::string_view name, std::string_view json_name) const {
    static_assert(detail::protobuf_valid_id(Id), "Invalid Protobuf field number");
    if constexpr (Format == protobuf_format::binary) {
      return current_id == Id;
    } else if constexpr (Format == protobuf_format::json) {
      return current_name == name || current_name == json_name;
    } else {
      return current_name == name;
    }
  }

  // TextProto permits repeated fields but rejects duplicate singular assignments.
  template <bool Repeated>
  void occurrence(bool& seen) const {
    if constexpr (Format == protobuf_format::text && !Repeated) {
      if (seen) {
        fail("Duplicate singular TextProto field");
      }
    }
    seen = true;
  }

  // Named Protobuf formats cannot specify conflicting members of the same oneof.
  void oneof(int& selected, int alternative) const {
    if constexpr (Format != protobuf_format::binary) {
      if (selected >= 0 && selected != alternative) {
        fail("Conflicting Protobuf union alternatives");
      }
      if constexpr (Format == protobuf_format::text) {
        if (selected >= 0) {
          fail("Duplicate TextProto union alternative");
        }
      }
    }
    selected = alternative;
  }

  // Enter a bounded nested message, restoring framing state on success or failure.
  template <typename Read>
  void message(Read&& read, bool root = false) {
    auto nesting = this->enter_object();
    const auto saved_end = message_end;
    const auto saved_first = first;
    const auto saved_closer = closer;
    first = true;
    closer = 0;
    if constexpr (Format == protobuf_format::binary) {
      if (!root) {
        if (wire != 2) {
          fail("Expected Protobuf message");
        }
        const auto size = length();
        message_end = in_stream.curr() + size;
      }
    } else {
      whitespace();
      if constexpr (Format == protobuf_format::json) {
        check_and_increase('{');
        closer = '}';
      } else if (!root) {
        const auto open = *take(1);
        if (open != '{' && open != '<') {
          fail("Expected TextProto message");
        }
        closer = open == '{' ? '}' : '>';
      }
    }
    try {
      read(*this);
    } catch (...) {
      message_end = saved_end;
      first = saved_first;
      closer = saved_closer;
      throw;
    }
    message_end = saved_end;
    first = saved_first;
    closer = saved_closer;
  }

  // Read the next key, enforcing object delimiters and Protobuf tag validity.
  bool next_field() {
    if constexpr (Format == protobuf_format::binary) {
      if (remaining() == 0) {
        return false;
      }
      const auto tag = varint();
      if (tag > std::numeric_limits<std::uint32_t>::max()) {
        fail("Invalid Protobuf tag");
      }
      current_id = static_cast<std::uint32_t>(tag >> detail::protobuf_tag_bits);
      wire = static_cast<unsigned>(tag & 7);
      if (current_id == 0 || wire > 5) {
        fail("Invalid Protobuf tag");
      }
      return true;
    } else {
      whitespace();
      if constexpr (Format == protobuf_format::text) {
        if (!first && remaining() != 0 && (peek() == ',' || peek() == ';')) {
          take(1);
          whitespace();
        }
      }
      if (remaining() == 0) {
        if (closer != 0) {
          fail("Truncated Protobuf text object");
        }
        return false;
      }
      if (closer != 0 && peek() == static_cast<std::uint8_t>(closer)) {
        take(1);
        return false;
      }
      if constexpr (Format == protobuf_format::json) {
        if (!first) {
          check_and_increase(',');
          whitespace();
        }
        current_name = string_value();
        whitespace();
        check_and_increase(':');
      } else {
        current_name = token();
        whitespace();
        if (remaining() != 0 && peek() == ':') {
          take(1);
        } else if (remaining() == 0 || (peek() != '{' && peek() != '<')) {
          fail("Missing TextProto colon");
        }
      }
      first = false;
      whitespace();
      return true;
    }
  }

  // Skip unknown binary fields by wire type; named formats reject unknown fields.
  void unknown() {
    if constexpr (Format != protobuf_format::binary) {
      fail("Unknown Protobuf field");
    } else {
      switch (wire) {
      case 0:
        static_cast<void>(varint());
        break;
      case 1:
        take(8);
        break;
      case 2:
        take(length());
        break;
      case 5:
        take(4);
        break;
      case 3: {
        auto nesting = this->enter_object();
        const auto group = current_id;
        while (next_field()) {
          if (wire == 4) {
            if (current_id != group) {
              fail("Mismatched Protobuf group");
            }
            return;
          }
          unknown();
        }
        fail("Unterminated Protobuf group");
      }
      default:
        fail("Unexpected Protobuf end-group");
      }
    }
  }

  // Consume JSON null as an unset field, including whole repeated/map fields.
  bool null_value() {
    if constexpr (Format == protobuf_format::json) {
      whitespace();
      if (remaining() >= 4 && std::memcmp(in_stream.curr(), "null", 4) == 0) {
        take(4);
        return true;
      }
    }
    return false;
  }

  // Decode a statically typed field, appending binary repetitions and merging nested messages.
  template <typename T>
  void field(T& value) {
    if (null_value()) {
      return;
    }
    if constexpr (type_check::vector<T>) {
      using Element = typename T::value_type;
      const auto append = [&] {
        collection_entry(sizeof(Element));
        Element element{};
        detail::protobuf_reset(element);
        if constexpr (Format == protobuf_format::json) {
          whitespace();
          if (remaining() >= 4 && std::memcmp(in_stream.curr(), "null", 4) == 0) {
            fail("Null repeated element");
          }
        }
        field(element);
        value.push_back(std::move(element));
      };
      if constexpr (Format == protobuf_format::binary) {
        if constexpr (detail::protobuf_wire_type<Element>() != 2) {
          if (wire == 2) {
            const auto size = length();
            const auto saved = message_end;
            message_end = in_stream.curr() + size;
            wire = detail::protobuf_wire_type<Element>();
            try {
              while (remaining() != 0) {
                append();
              }
            } catch (...) {
              message_end = saved;
              throw;
            }
            message_end = saved;
            return;
          }
        }
        append();
      } else {
        whitespace();
        if constexpr (Format == protobuf_format::text) {
          if (peek() != '[') {
            append();
            return;
          }
        }
        check_and_increase('[');
        whitespace();
        if constexpr (Format == protobuf_format::json) {
          value.clear();
        }
        if (peek() != ']') {
          while (true) {
            append();
            whitespace();
            if (peek() == ']') {
              break;
            }
            check_and_increase(',');
            whitespace();
            if constexpr (Format == protobuf_format::text) {
              if (peek() == ']') {
                break;
              }
            }
          }
        }
        check_and_increase(']');
      }
    } else if constexpr (type_check::map<T>) {
      using Key = typename T::key_type;
      using Value = typename T::mapped_type;
      if constexpr (Format == protobuf_format::json) {
        auto nesting = this->enter_object();
        check_and_increase('{');
        whitespace();
        value.clear();
        if (peek() != '}') {
          while (true) {
            collection_entry(sizeof(typename T::value_type));
            const auto key_text = string_value();
            Key key{};
            if constexpr (std::same_as<Key, std::string>) {
              key = key_text;
            } else if constexpr (std::same_as<Key, bool>) {
              if (key_text != "true" && key_text != "false") {
                fail("Invalid Protobuf map key");
              }
              key = key_text == "true";
            } else {
              numeric(key_text, key);
            }
            whitespace();
            check_and_increase(':');
            whitespace();
            if (remaining() >= 4 && std::memcmp(in_stream.curr(), "null", 4) == 0) {
              fail("Null map value");
            }
            Value mapped{};
            detail::protobuf_reset(mapped);
            field(mapped);
            value.insert_or_assign(std::move(key), std::move(mapped));
            whitespace();
            if (peek() == '}') {
              break;
            }
            check_and_increase(',');
            whitespace();
          }
        }
        check_and_increase('}');
      } else {
        const auto append_entry = [&] {
          collection_entry(sizeof(typename T::value_type));
          Key key{};
          Value mapped{};
          detail::protobuf_reset(mapped);
          message([&](auto& entry) {
            bool seen_key{};
            bool seen_value{};
            while (entry.next_field()) {
              if (entry.template match<1>("key", "key")) {
                entry.template occurrence<false>(seen_key);
                entry.field(key);
              } else if (entry.template match<2>("value", "value")) {
                entry.template occurrence<false>(seen_value);
                entry.field(mapped);
              } else {
                entry.unknown();
              }
            }
          });
          value.insert_or_assign(std::move(key), std::move(mapped));
        };
        if constexpr (Format == protobuf_format::text) {
          whitespace();
          if (peek() == '[') {
            take(1);
            whitespace();
            while (peek() != ']') {
              append_entry();
              whitespace();
              if (peek() == ']') {
                break;
              }
              check_and_increase(',');
              whitespace();
            }
            take(1);
            return;
          }
        }
        append_entry();
      }
    } else if constexpr (requires { value.serializer_protobuf_read(*this); }) {
      message([&](auto& nested) { value.serializer_protobuf_read(nested); });
    } else {
      scalar(value);
    }
  }

  // Decode one exact message transactionally; absent fields receive Protobuf defaults.
  template <typename T>
  void protobuf_object(T& value) {
    require_input(in_stream.remaining_buffer());
    T replacement{};
    detail::protobuf_reset(replacement);
    message([&](auto& input) { replacement.serializer_protobuf_read(input); }, true);
    finish();
    value = std::move(replacement);
  }

  // Match the regular runtime entry point for generated owning objects.
  template <typename T>
  void serialize_in(T& value) {
    protobuf_object(value);
  }

  // Require an exact binary frame or whitespace-only trailing text.
  void finish() {
    if constexpr (Format != protobuf_format::binary) {
      whitespace();
    }
    if (remaining() != 0) {
      fail("Trailing Protobuf input");
    }
  }
};

template <serialize_type Direction>
using protobuf_binary = protobuf_codec<Direction, protobuf_format::binary>;
template <serialize_type Direction>
using protojson = protobuf_codec<Direction, protobuf_format::json>;
template <serialize_type Direction>
using textproto = protobuf_codec<Direction, protobuf_format::text>;
} // namespace rohit::serializer
