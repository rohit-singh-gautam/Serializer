#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::detail {

// Return a complete UTF-8 sequence length, rejecting overlong encodings and surrogate code points.
inline std::size_t utf8_sequence_size(std::span<const std::uint8_t> bytes) {
  if (bytes.empty()) {
    throw std::invalid_argument{"Truncated UTF-8"};
  }
  const auto first = bytes[0];
  if (first < 0x80) {
    return 1;
  }
  const std::size_t size = first >= 0xc2 && first <= 0xdf ? 2 :
      first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
  if (size == 0 || size > bytes.size()) {
    throw std::invalid_argument{"Invalid UTF-8"};
  }
  for (std::size_t index = 1; index < size; ++index) {
    if ((bytes[index] & 0xc0) != 0x80) {
      throw std::invalid_argument{"Invalid UTF-8 continuation"};
    }
  }
  if ((first == 0xe0 && bytes[1] < 0xa0) || (first == 0xed && bytes[1] >= 0xa0) ||
      (first == 0xf0 && bytes[1] < 0x90) || (first == 0xf4 && bytes[1] >= 0x90)) {
    throw std::invalid_argument{"Invalid UTF-8 code point"};
  }
  return size;
}

// Read exactly four ASCII hexadecimal digits without locale-dependent conversion.
inline std::uint32_t read_hex_quad(std::span<const std::uint8_t> bytes) {
  constexpr std::size_t hex_quad_digits = 4;
  if (bytes.size() < hex_quad_digits) {
    throw std::invalid_argument{"Truncated Unicode escape"};
  }
  std::uint32_t result{};
  for (std::size_t index = 0; index < hex_quad_digits; ++index) {
    const auto ch = bytes[index];
    const auto digit = ch >= '0' && ch <= '9' ? ch - '0' :
        ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
    if (digit < 0) {
      throw std::invalid_argument{"Invalid Unicode escape"};
    }
    result = (result << 4) | static_cast<std::uint32_t>(digit);
  }
  return result;
}

// Decode one escape starting after its backslash; advance past an entire surrogate pair.
inline std::uint32_t read_json_escape(std::span<const std::uint8_t> bytes, std::size_t& index) {
  if (index == bytes.size()) {
    throw std::invalid_argument{"Truncated JSON escape"};
  }
  switch (bytes[index++]) {
  case '"': return '"';
  case '\\': return '\\';
  case '/': return '/';
  case 'b': return '\b';
  case 'f': return '\f';
  case 'n': return '\n';
  case 'r': return '\r';
  case 't': return '\t';
  case 'u': {
    auto code_point = read_hex_quad(bytes.subspan(index));
    index += 4;
    if (code_point >= 0xd800 && code_point <= 0xdbff) {
      if (bytes.size() - index < 6 || bytes[index] != '\\' || bytes[index + 1] != 'u') {
        throw std::invalid_argument{"Missing low surrogate"};
      }
      const auto low = read_hex_quad(bytes.subspan(index + 2));
      if (low < 0xdc00 || low > 0xdfff) {
        throw std::invalid_argument{"Invalid low surrogate"};
      }
      index += 6;
      code_point = 0x10000 + ((code_point - 0xd800) << 10) + (low - 0xdc00);
    } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
      throw std::invalid_argument{"Unpaired low surrogate"};
    }
    return code_point;
  }
  default: throw std::invalid_argument{"Unknown JSON escape"};
  }
}

// Measure the UTF-8 representation of a validated Unicode scalar value.
inline std::size_t utf8_code_point_size(std::uint32_t value) {
  return value <= 0x7f ? 1 : value <= 0x7ff ? 2 : value <= 0xffff ? 3 : 4;
}

// Append a validated scalar without temporary owning text.
inline void append_utf8(std::string& output, std::uint32_t value) {
  const auto size = utf8_code_point_size(value);
  if (size == 1) {
    output.push_back(static_cast<char>(value));
    return;
  }
  const auto prefix = size == 2 ? 0xc0U : size == 3 ? 0xe0U : 0xf0U;
  output.push_back(static_cast<char>(prefix | (value >> (6 * (size - 1)))));
  for (std::size_t index = size - 1; index != 0; --index) {
    output.push_back(static_cast<char>(0x80U | ((value >> (6 * (index - 1))) & 0x3fU)));
  }
}

struct json_string_range {
  std::size_t wire_bytes{};
  std::size_t text_bytes{};
  bool escaped{};
};

// Validate one quoted string in a bounded span, including its Unicode and escape grammar.
inline json_string_range scan_json_string(std::span<const std::uint8_t> bytes) {
  if (bytes.empty() || bytes[0] != '"') {
    throw std::invalid_argument{"Expected a JSON string"};
  }
  json_string_range result{};
  std::size_t index = 1;
  while (index < bytes.size()) {
    const auto ch = bytes[index];
    if (ch == '"') {
      result.wire_bytes = index + 1;
      return result;
    }
    if (ch < 0x20) {
      throw std::invalid_argument{"Unescaped JSON control character"};
    }
    if (ch == '\\') {
      result.escaped = true;
      ++index;
      result.text_bytes += utf8_code_point_size(read_json_escape(bytes, index));
    } else {
      const auto size = utf8_sequence_size(bytes.subspan(index));
      index += size;
      result.text_bytes += size;
    }
  }
  throw std::invalid_argument{"Unterminated JSON string"};
}

// Replace a destination from a validated string, copying ordinary text in runs.
inline void assign_json_string(std::string& output, std::span<const std::uint8_t> bytes,
                               json_string_range range) {
  output.clear();
  output.reserve(range.text_bytes);
  std::size_t index = 1;
  const auto end = range.wire_bytes - 1;
  while (index < end) {
    const auto start = index;
    while (index < end && bytes[index] != '\\') {
      ++index;
    }
    output.append(reinterpret_cast<const char*>(bytes.data() + start), index - start);
    if (index < end) {
      ++index;
      append_utf8(output, read_json_escape(bytes, index));
    }
  }
}

// Validate UTF-8 and measure escaping without changing the output stream.
inline std::size_t json_escaped_size(std::string_view text) {
  const auto bytes = std::span{reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
  std::size_t result{};
  for (std::size_t index = 0; index < bytes.size();) {
    const auto ch = bytes[index];
    const auto size = utf8_sequence_size(bytes.subspan(index));
    const auto encoded = ch < 0x20 ? std::size_t{6} :
        ch == '"' || ch == '\\' ? std::size_t{2} : size;
    if (encoded > std::numeric_limits<std::size_t>::max() - result) {
      throw std::length_error{"Escaped JSON string is too large"};
    }
    result += encoded;
    index += size;
  }
  return result;
}

// Escape only strings that need it; validated ordinary UTF-8 can use the direct stream batch path.
inline std::string escape_json_string(std::string_view text, std::size_t encoded_size) {
  constexpr std::string_view hex_digits = "0123456789abcdef";
  std::string result;
  result.reserve(encoded_size);
  std::size_t start{};
  for (std::size_t index = 0; index < text.size(); ++index) {
    const auto ch = static_cast<unsigned char>(text[index]);
    if (ch >= 0x20 && ch != '"' && ch != '\\') {
      continue;
    }
    result.append(text.substr(start, index - start));
    if (ch < 0x20) {
      result += "\\u00";
      result.push_back(hex_digits[ch >> 4]);
      result.push_back(hex_digits[ch & 0x0f]);
    } else {
      result.push_back('\\');
      result.push_back(static_cast<char>(ch));
    }
    start = index + 1;
  }
  result.append(text.substr(start));
  return result;
}

} // namespace rohit::serializer::detail
