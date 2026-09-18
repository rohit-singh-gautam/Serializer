#pragma once

#include <rohit/runtime_simd.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::detail {

// Return a complete UTF-8 sequence length, rejecting overlong encodings and surrogate code points.
constexpr std::size_t utf8_sequence_size(std::span<const std::uint8_t> bytes) {
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

// Validate binary text without allocation or padded reads using the bounded UTF-8 backend.
inline void validate_utf8(std::span<const std::uint8_t> bytes) {
  if (!is_valid_utf8(bytes.data(), bytes.size())) {
    throw std::invalid_argument{"Invalid UTF-8"};
  }
}

// Treat string storage as bytes without changing embedded NULs or Unicode normalization.
inline void validate_utf8(std::string_view text) {
  validate_utf8(std::span<const std::uint8_t>{
      reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
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
  // Offsets are relative to the opening quote; defaults retain the full-span helper path.
  std::size_t first_escape{1};
  std::size_t last_escape_end{std::numeric_limits<std::size_t>::max()};
};

// Validate one quoted string in a bounded span, including its Unicode and escape grammar.
inline json_string_range scan_json_string(std::span<const std::uint8_t> bytes) {
  if (bytes.empty() || bytes[0] != '"') {
    throw std::invalid_argument{"Expected a JSON string"};
  }
  json_string_range result{};
  std::size_t index = 1;
  while (index < bytes.size()) {
    const auto plain =
        scan_json_prefix<json_scan_kind::ascii>(bytes.data() + index, bytes.size() - index);
    index += plain;
    result.text_bytes += plain;
    if (index == bytes.size()) {
      break;
    }
    const auto ch = bytes[index];
    if (ch == '"') {
      result.wire_bytes = index + 1;
      return result;
    }
    if (ch < 0x20) {
      throw std::invalid_argument{"Unescaped JSON control character"};
    }
    if (ch == '\\') {
      if (!result.escaped) {
        result.first_escape = index;
      }
      result.escaped = true;
      ++index;
      result.text_bytes += utf8_code_point_size(read_json_escape(bytes, index));
      result.last_escape_end = index;
    } else {
      const auto size = utf8_sequence_size(bytes.subspan(index));
      index += size;
      result.text_bytes += size;
    }
  }
  throw std::invalid_argument{"Unterminated JSON string"};
}

// Replace from a fully validated range; input must remain alive and disjoint from output storage.
// Copy known plain spans directly and rescan only between the first and last escapes.
inline void assign_json_string(std::string& output, std::span<const std::uint8_t> bytes,
                               json_string_range range) {
  output.clear();
  output.reserve(range.text_bytes);
  if (!range.escaped) {
    output.append(reinterpret_cast<const char*>(bytes.data() + 1), range.text_bytes);
    return;
  }
  output.append(reinterpret_cast<const char*>(bytes.data() + 1), range.first_escape - 1);
  std::size_t index = range.first_escape;
  const auto text_end = range.wire_bytes - 1;
  // A manually supplied three-member range retains its original full-span behavior.
  const auto end = std::min(range.last_escape_end, text_end);
  while (index < end) {
    const auto start = index;
    // This range was validated; only a backslash can stop an unescaped span here.
    index += scan_json_prefix<json_scan_kind::unescaped>(bytes.data() + index, end - index);
    output.append(reinterpret_cast<const char*>(bytes.data() + start), index - start);
    if (index < end) {
      ++index;
      append_utf8(output, read_json_escape(bytes, index));
    }
  }
  output.append(reinterpret_cast<const char*>(bytes.data() + end), text_end - end);
}

struct json_escape_analysis {
  std::size_t encoded_bytes{};
  // Offsets bound the raw bytes needing escaping; both equal input size when there are none.
  std::size_t first_escape{};
  std::size_t last_escape_end{};
};

// Validate the whole UTF-8 string and retain escape boundaries alongside its exact encoded size.
inline json_escape_analysis analyze_json_escaping(std::string_view text) {
  const auto bytes = std::span{reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
  json_escape_analysis result{text.size(), text.size(), text.size()};
  for (std::size_t index = 0; index < bytes.size();) {
    index += scan_json_prefix<json_scan_kind::ascii>(bytes.data() + index, bytes.size() - index);
    if (index == bytes.size()) {
      break;
    }
    const auto ch = bytes[index];
    const auto size = utf8_sequence_size(bytes.subspan(index));
    const auto encoded = ch < 0x20 ? std::size_t{6} :
        ch == '"' || ch == '\\' ? std::size_t{2} : size;
    const auto extra = encoded - size;
    if (extra > std::numeric_limits<std::size_t>::max() - result.encoded_bytes) {
      throw std::length_error{"Escaped JSON string is too large"};
    }
    if (extra != 0) {
      if (result.first_escape == text.size()) {
        result.first_escape = index;
      }
      result.last_escape_end = index + size;
    }
    result.encoded_bytes += extra;
    index += size;
  }
  return result;
}

// Retain size-only callers and their validation/errors without requiring them to keep scan metadata.
inline std::size_t json_escaped_size(std::string_view text) {
  return analyze_json_escaping(text).encoded_bytes;
}

// Write validated UTF-8 into exactly json_escaped_size(text) writable bytes without allocating.
// Input and output must be disjoint; the caller reserves storage and resolves aliases first.
inline void write_json_escaped(std::uint8_t* output, const std::uint8_t* input,
                               std::size_t size) noexcept {
  constexpr std::string_view hex_digits = "0123456789abcdef";
  std::size_t written{};
  for (std::size_t index = 0; index < size;) {
    const auto plain = scan_json_prefix<json_scan_kind::unescaped>(input + index, size - index);
    if (plain != 0) {
      std::memcpy(output + written, input + index, plain);
      written += plain;
      index += plain;
    }
    if (index == size) {
      break;
    }
    const auto ch = input[index++];
    output[written++] = '\\';
    if (ch < 0x20) {
      output[written++] = 'u';
      output[written++] = '0';
      output[written++] = '0';
      output[written++] = static_cast<std::uint8_t>(hex_digits[ch >> 4]);
      output[written++] = static_cast<std::uint8_t>(hex_digits[ch & 0x0f]);
    } else {
      output[written++] = ch;
    }
  }
}

// Write with analysis from the same unchanged input; reserve analysis.encoded_bytes first.
// Input/output must be disjoint. Offsets survive source rebasing by a stream reservation.
inline void write_json_escaped(std::uint8_t* output, const std::uint8_t* input, std::size_t size,
                               const json_escape_analysis& analysis) noexcept {
  if (analysis.first_escape != 0) {
    std::memcpy(output, input, analysis.first_escape);
  }
  if (analysis.first_escape != analysis.last_escape_end) {
    write_json_escaped(output + analysis.first_escape, input + analysis.first_escape,
                       analysis.last_escape_end - analysis.first_escape);
  }
  const auto tail_bytes = size - analysis.last_escape_end;
  if (tail_bytes != 0) {
    std::memcpy(output + analysis.encoded_bytes - tail_bytes, input + analysis.last_escape_end,
                tail_bytes);
  }
}

// Retain the owning helper; treat the supplied size as a reservation hint, never as a write bound.
inline std::string escape_json_string(std::string_view text, std::size_t encoded_size) {
  std::string result;
  result.reserve(encoded_size);
  const auto analysis = analyze_json_escaping(text);
  result.resize(analysis.encoded_bytes);
  write_json_escaped(reinterpret_cast<std::uint8_t*>(result.data()),
                     reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), analysis);
  return result;
}

} // namespace rohit::serializer::detail
