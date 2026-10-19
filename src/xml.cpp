// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <deque>
#include <exception>
#include <map>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef SERIALIZER_HAS_XML
#define SERIALIZER_HAS_XML 0
#endif
#if SERIALIZER_HAS_XML
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlregexp.h>
#include <libxml/xmlschemas.h>
#include <libxml/tree.h>
#endif

namespace rohit::serializer::xml {
namespace {
// Fail bounded counters before overflow or allocation beyond the document budget.
void consume(std::size_t& used, std::size_t amount, std::size_t bound) {
  if (used > bound || amount > bound - used) { throw std::length_error{"XML resource limit exceeded"}; }
  used += amount;
}
// Reject names outside the supported portable NCName subset rather than emitting invalid XML.
void check_name(std::string_view value) {
  const auto initial = [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  };
  if (value.empty() || !initial(static_cast<unsigned char>(value.front()))) {
    throw std::invalid_argument{"XML mapping requires a nonempty ASCII NCName"};
  }
  for (const auto character : value) {
    const auto c = static_cast<unsigned char>(character);
    if (!initial(c) && !(c >= '0' && c <= '9') && c != '-' && c != '.') {
      throw std::invalid_argument{"Invalid XML mapping NCName"};
    }
  }
}
// Validate canonical UTF-8 and XML 1.0 character ranges before any bytes are emitted.
void check_text(std::string_view value, operation_context* context = nullptr) {
  for (std::size_t index{}; index < value.size();) {
    constexpr std::size_t checkpoint_bytes = 4096U;
    if (context && index % checkpoint_bytes == 0U) { context->check(); }
    const auto first = static_cast<unsigned char>(value[index++]);
    std::uint32_t codepoint{first};
    std::size_t count{};
    if (first >= 0xc2U && first <= 0xdfU) { count = 1U; codepoint &= 0x1fU; }
    else if (first >= 0xe0U && first <= 0xefU) { count = 2U; codepoint &= 0x0fU; }
    else if (first >= 0xf0U && first <= 0xf4U) { count = 3U; codepoint &= 0x07U; }
    else if (first >= 0x80U) { throw std::invalid_argument{"Invalid XML UTF-8"}; }
    if (count > value.size() - index) { throw std::invalid_argument{"Truncated XML UTF-8"}; }
    for (std::size_t next{}; next < count; ++next) {
      const auto byte = static_cast<unsigned char>(value[index++]);
      if ((byte & 0xc0U) != 0x80U) { throw std::invalid_argument{"Invalid XML UTF-8 continuation"}; }
      codepoint = (codepoint << 6U) | (byte & 0x3fU);
    }
    if ((count == 1U && codepoint < 0x80U) || (count == 2U && codepoint < 0x800U) ||
        (count == 3U && codepoint < 0x10000U) ||
        !(codepoint == 0x09U || codepoint == 0x0aU || codepoint == 0x0dU ||
          (codepoint >= 0x20U && codepoint <= 0xd7ffU) ||
          (codepoint >= 0xe000U && codepoint <= 0xfffdU) ||
          (codepoint >= 0x10000U && codepoint <= 0x10ffffU))) {
      throw std::invalid_argument{"Invalid XML 1.0 character"};
    }
  }
}
// Build deterministic output without permitting intermediate escaped output beyond its limit.
class document_writer {
  // One mutable encoding pass owns its output and never copies its borrowed operation context.
  document_writer(const document_writer&) = delete;
  // An encoding pass cannot replace its output/operation ownership with another writer's state.
  document_writer& operator=(const document_writer&) = delete;
  limits bounds_;
  operation_context* context_{};
  std::string bytes_{};
  std::size_t nodes_{};
  std::size_t attributes_{};
  std::size_t text_bytes_{};
  std::size_t name_bytes_{};
  std::map<std::string, std::string> prefixes_{};
  std::set<std::string> retained_prefixes_{};
  std::size_t namespace_bindings_{};
  // Append within the complete output budget.
  void append(std::string_view value) {
    if (bytes_.size() > bounds_.max_output_bytes || value.size() > bounds_.max_output_bytes - bytes_.size()) {
      throw std::length_error{"XML output limit exceeded"};
    }
    if (context_) {
      context_->consume(operation_resource::work_units, value.size());
      context_->consume(operation_resource::allocation_bytes, value.size());
    }
    bytes_ += value;
  }
  // Preserve text/attribute whitespace using character references where XML normalizes literals.
  void escaped(std::string_view value, bool is_attribute) {
    check_text(value, context_);
    std::size_t start{};
    for (std::size_t index{}; index < value.size(); ++index) {
      std::string_view replacement{};
      switch (value[index]) {
      case '&': replacement = "&amp;"; break;
      case '<': replacement = "&lt;"; break;
      case '>': replacement = "&gt;"; break;
      case '"': if (is_attribute) { replacement = "&quot;"; } break;
      case '\r': replacement = "&#13;"; break;
      case '\n': if (is_attribute) { replacement = "&#10;"; } break;
      case '\t': if (is_attribute) { replacement = "&#9;"; } break;
      default: break;
      }
      if (!replacement.empty()) {
        append(value.substr(start, index - start));
        append(replacement);
        start = index + 1U;
      }
      constexpr std::size_t checkpoint_bytes = 4096U;
      if (context_ && index % checkpoint_bytes == 0U) { context_->check(); }
    }
    append(value.substr(start));
  }
  // Register stable namespace prefixes in first-use traversal order.
  void register_name(const name& value) {
    consume(name_bytes_, value.local.size(), bounds_.max_name_bytes);
    consume(name_bytes_, value.namespace_uri.size(), bounds_.max_name_bytes);
    check_name(value.local);
    check_text(value.namespace_uri);
    if (value.namespace_uri == "http://www.w3.org/2000/xmlns/") {
      throw std::invalid_argument{"Namespace declaration names are reserved"};
    }
    if (!value.namespace_uri.empty() && !prefixes_.contains(value.namespace_uri)) {
      prefixes_.emplace(value.namespace_uri, value.namespace_uri == "http://www.w3.org/XML/1998/namespace" ? "xml" : "");
    }
  }
  // Verify shape, budgets and duplicate expanded attributes before serializing.
  void inspect(const element& node, std::size_t depth) {
    if (context_) { context_->consume(operation_resource::work_units, 1U); }
    if (depth > bounds_.max_depth) { throw std::length_error{"XML depth limit exceeded"}; }
    consume(nodes_, 1U, bounds_.max_nodes);
    consume(text_bytes_, node.text.size(), bounds_.max_text_bytes);
    register_name(node.qualified_name);
    consume(namespace_bindings_, node.namespace_bindings.size(), bounds_.max_namespace_bindings);
    for (const auto& [prefix, uri] : node.namespace_bindings) {
      if (!prefix.empty()) { check_name(prefix); }
      check_text(uri);
      if (prefix == "xmlns" || uri == "http://www.w3.org/2000/xmlns/" ||
          (prefix == "xml") != (uri == "http://www.w3.org/XML/1998/namespace")) {
        throw std::invalid_argument{"Invalid retained XML namespace binding"};
      }
      consume(name_bytes_, prefix.size(), bounds_.max_name_bytes);
      consume(name_bytes_, uri.size(), bounds_.max_name_bytes);
      retained_prefixes_.insert(prefix);
    }
    if (!node.children.empty() && !detail::trim(node.text).empty() && node.text_segments.empty() &&
        !(context_ && context_->allow_mixed_content)) {
      throw std::invalid_argument{"XML mixed content is unsupported"};
    }
    std::size_t segment_bytes{}, previous_child{};
    for (const auto& segment : node.text_segments) {
      if (segment.before_child < previous_child || segment.before_child > node.children.size() ||
          segment_bytes > node.text.size() ||
          !std::string_view{node.text}.substr(segment_bytes).starts_with(segment.value)) {
        throw std::invalid_argument{"Invalid XML extension content order"};
      }
      previous_child = segment.before_child;
      segment_bytes += segment.value.size();
    }
    if (!node.text_segments.empty() && segment_bytes != node.text.size()) {
      throw std::invalid_argument{"Incomplete XML extension content order"};
    }
    std::set<std::pair<std::string, std::string>> names{};
    for (const auto& attribute : node.attributes) {
      consume(attributes_, 1U, bounds_.max_attributes);
      consume(text_bytes_, attribute.value.size(), bounds_.max_text_bytes);
      register_name(attribute.qualified_name);
      if (attribute.qualified_name.namespace_uri.empty() && attribute.qualified_name.local == "xmlns") {
        throw std::invalid_argument{"Namespace declarations are not ordinary attributes"};
      }
      if (!names.emplace(attribute.qualified_name.namespace_uri, attribute.qualified_name.local).second) {
        throw std::invalid_argument{"Duplicate XML attribute"};
      }
    }
    for (const auto& child : node.children) { inspect(child, depth + 1U); }
  }
  // Emit an expanded name using its registered namespace prefix.
  void emit_name(const name& value) {
    if (!value.namespace_uri.empty()) { append(prefixes_.at(value.namespace_uri)); append(":"); }
    append(value.local);
  }
  // Emit one already bounded record, with declarations centralized on the root.
  void emit(const element& node, bool root, std::string_view inherited_default = {}) {
    const auto default_binding = node.namespace_bindings.find("");
    auto effective_default = default_binding == node.namespace_bindings.end() ?
        inherited_default : std::string_view{default_binding->second};
    const bool reset_default = node.qualified_name.namespace_uri.empty() && !effective_default.empty();
    append("<"); emit_name(node.qualified_name);
    if (root) {
      for (const auto& [uri, prefix] : prefixes_) {
        if (prefix == "xml") { continue; }
        append(" xmlns:"); append(prefix); append("=\""); escaped(uri, true); append("\"");
      }
    }
    // Retain QName-valued attribute/text bindings, using distinct prefixes for expanded element names.
    for (const auto& [prefix, uri] : node.namespace_bindings) {
      if (prefix == "xml") { continue; }
      if (prefix.empty() && reset_default) { continue; }
      append(" xmlns");
      if (!prefix.empty()) { append(":"); append(prefix); }
      append("=\""); escaped(uri, true); append("\"");
    }
    if (reset_default) {
      append(" xmlns=\"\"");
      effective_default = {};
    }
    for (const auto& attribute : node.attributes) {
      append(" "); emit_name(attribute.qualified_name); append("=\"");
      escaped(attribute.value, true); append("\"");
    }
    if (node.children.empty() && node.text.empty()) { append("/>"); return; }
    append(">");
    if (node.text_segments.empty()) {
      escaped(node.text, false);
      for (const auto& child : node.children) { emit(child, false, effective_default); }
    } else {
      std::size_t next_segment{};
      for (std::size_t index{}; index <= node.children.size(); ++index) {
        while (next_segment < node.text_segments.size() &&
               node.text_segments[next_segment].before_child == index) {
          escaped(node.text_segments[next_segment++].value, false);
        }
        if (index < node.children.size()) { emit(node.children[index], false, effective_default); }
      }
    }
    append("</"); emit_name(node.qualified_name); append(">");
  }
public:
  // Bind immutable complete-document limits.
  explicit document_writer(limits bounds, operation_context* context = nullptr)
      : bounds_{bounds}, context_{context} {}
  // Return one complete document after all semantic/resource checks succeed.
  std::string encode(const element& document) {
    inspect(document, 1U);
    std::size_t next_prefix{};
    for (auto& [uri, prefix] : prefixes_) {
      (void)uri;
      if (prefix == "xml") { continue; }
      do { prefix = "srl" + std::to_string(next_prefix++); } while (retained_prefixes_.contains(prefix));
    }
    emit(document, true); return std::move(bytes_);
  }
};
#if SERIALIZER_HAS_XML
// Read provider input in bounded chunks; exceptions never cross a C callback boundary.
struct checked_input {
  std::string_view remaining;
  operation_context& operation;
  std::exception_ptr failure{};
  // Cooperatively interrupt parsing even while libxml consumes one large token.
  static int read(void* state, char* destination, int capacity) noexcept {
    auto& input = *static_cast<checked_input*>(state);
    try {
      constexpr std::size_t checkpoint_bytes = 4096U;
      input.operation.check();
      if (capacity <= 0) { return 0; }
      const auto count = std::min({input.remaining.size(), static_cast<std::size_t>(capacity),
                                  checkpoint_bytes});
      input.operation.consume(operation_resource::work_units, count);
      std::copy_n(input.remaining.data(), count, destination);
      input.remaining.remove_prefix(count);
      return static_cast<int>(count);
    } catch (...) {
      input.failure = std::current_exception();
      return -1;
    }
  }
};
// Copy an optional provider UTF-8 string before advancing its cursor.
std::string copy(const xmlChar* value) { return value ? reinterpret_cast<const char*>(value) : ""; }
// Enforce the UTF-8-only input contract rather than silently overriding another declared encoding.
void check_encoding(std::string_view bytes) {
  if (bytes.starts_with("\xef\xbb\xbf")) { bytes.remove_prefix(3U); }
  if (!bytes.starts_with("<?xml") || bytes.size() <= 5U ||
      std::string_view{" \t\r\n"}.find(bytes[5U]) == std::string_view::npos) { return; }
  const auto end = bytes.find("?>");
  const auto offset = bytes.find("encoding");
  if (end == std::string_view::npos || offset == std::string_view::npos || offset >= end) { return; }
  if (end-offset < std::string_view{"encoding"}.size()) { throw std::invalid_argument{"Malformed XML encoding declaration"}; }
  auto attribute = bytes.substr(offset + std::string_view{"encoding"}.size(), end-offset-std::string_view{"encoding"}.size());
  attribute = detail::trim(attribute);
  if (attribute.empty() || attribute.front() != '=') { throw std::invalid_argument{"Malformed XML encoding declaration"}; }
  attribute.remove_prefix(1U); attribute = detail::trim(attribute);
  if (attribute.empty() || (attribute.front() != '\'' && attribute.front() != '"')) { throw std::invalid_argument{"Malformed XML encoding declaration"}; }
  const auto quote = attribute.front(); attribute.remove_prefix(1U);
  const auto closing = attribute.find(quote);
  const auto value = attribute.substr(0U, closing);
  if (closing == std::string_view::npos || value.size() != 5U ||
      (value[0] != 'u' && value[0] != 'U') || (value[1] != 't' && value[1] != 'T') ||
      (value[2] != 'f' && value[2] != 'F') || value[3] != '-' || value[4] != '8') {
    throw std::invalid_argument{"XML input must declare UTF-8 encoding"};
  }
}
// Inspect only the encoding declaration in original UTF-16 units; never transcode or rewrite input.
void check_utf16_encoding(std::string_view bytes, bool little_endian, operation_context* context) {
  if (bytes.size() % 2U != 0U) { throw std::invalid_argument{"Truncated UTF-16 XML input"}; }
  bytes.remove_prefix(2U);
  const auto units = bytes.size() / 2U;
  const auto unit = [&](std::size_t index) {
    const auto first = static_cast<unsigned char>(bytes[index * 2U]);
    const auto second = static_cast<unsigned char>(bytes[index * 2U + 1U]);
    return little_endian ? static_cast<unsigned int>(first) | (static_cast<unsigned int>(second) << 8U)
                         : (static_cast<unsigned int>(first) << 8U) | static_cast<unsigned int>(second);
  };
  const auto matches = [&](std::size_t position, std::string_view expected) {
    if (position > units || expected.size() > units - position) { return false; }
    for (std::size_t index{}; index < expected.size(); ++index) {
      if (unit(position + index) != static_cast<unsigned char>(expected[index])) { return false; }
    }
    return true;
  };
  const auto whitespace = [](unsigned int value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
  };
  if (!matches(0U, "<?xml") || units <= 5U || !whitespace(unit(5U))) { return; }
  std::size_t declaration_end = units, encoding = units;
  for (std::size_t index = 5U; index < units; ++index) {
    if (context && index % 4096U == 0U) { context->check(); }
    if (matches(index, "?>")) { declaration_end = index; break; }
    if (matches(index, "encoding")) { encoding = index + std::string_view{"encoding"}.size(); }
  }
  if (encoding >= declaration_end) { return; }
  while (encoding < declaration_end && whitespace(unit(encoding))) { ++encoding; }
  if (encoding == declaration_end || unit(encoding++) != '=') {
    throw std::invalid_argument{"Malformed XML encoding declaration"};
  }
  while (encoding < declaration_end && whitespace(unit(encoding))) { ++encoding; }
  if (encoding == declaration_end || (unit(encoding) != '\'' && unit(encoding) != '"')) {
    throw std::invalid_argument{"Malformed XML encoding declaration"};
  }
  const auto quote = unit(encoding++);
  constexpr std::string_view expected{"utf-16"};
  if (expected.size() >= declaration_end - encoding) {
    throw std::invalid_argument{"XML input must declare UTF-16 encoding"};
  }
  for (const auto value : expected) {
    auto actual = unit(encoding++);
    if (actual >= 'A' && actual <= 'Z') { actual += 'a' - 'A'; }
    if (actual != static_cast<unsigned char>(value)) {
      throw std::invalid_argument{"XML input must declare UTF-16 encoding"};
    }
  }
  if (unit(encoding) != quote) { throw std::invalid_argument{"XML input must declare UTF-16 encoding"}; }
}
// Only OPC's explicit policy enables provider auto-detection, restricted to XML UTF-16 signatures.
const char* provider_encoding(std::string_view bytes, operation_context* context) {
  const bool little_endian = bytes.starts_with("\xff\xfe");
  const bool big_endian = bytes.starts_with("\xfe\xff");
  if (!little_endian && !big_endian) { check_encoding(bytes); return "UTF-8"; }
  if (!context || context->input_encoding != input_encoding_policy::utf8_or_utf16) {
    throw std::invalid_argument{"XML input must use UTF-8 encoding"};
  }
  // UCS-4 signatures share a UTF-16 BOM prefix but are not permitted OPC infrastructure encodings.
  if (bytes.size() >= 4U && bytes[2U] == '\0' && bytes[3U] == '\0') {
    throw std::invalid_argument{"XML UTF-32 input is unsupported"};
  }
  check_utf16_encoding(bytes, little_endian, context);
  return nullptr;
}
// Keep libxml diagnostics local and return errors through typed exceptions.
void discard_error(void*, const char*, ...) {}
// Reject XSD reference constructs recursively before libxml's schema loader can fetch anything.
void check_schema_references(const element& node) {
  constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};
  if (node.qualified_name.namespace_uri == xsd_namespace &&
      (node.qualified_name.local == "include" || node.qualified_name.local == "import" ||
       node.qualified_name.local == "redefine" || node.qualified_name.local == "override")) {
    throw std::invalid_argument{"XSD external references require a bundled resolver (not supported)"};
  }
  for (const auto& child : node.children) { check_schema_references(child); }
}
#endif
} // namespace

// Keep interruption errors independent of package diagnostics; a bridge may throw richer errors.
void operation_context::check() const {
  if (is_cancelled && is_cancelled()) { throw std::runtime_error{"XML operation cancelled"}; }
  if (deadline && std::chrono::steady_clock::now() >= *deadline) {
    throw std::runtime_error{"XML operation deadline exceeded"};
  }
}

// Failed/repeated work remains charged by the owning package; XML never refunds callbacks.
void operation_context::consume(operation_resource selected, std::size_t amount) const {
  check();
  if (charge) { charge(selected, amount); }
}

// Saturate durations rather than overflowing the steady-clock time-point representation.
void operation_context::set_timeout(std::chrono::steady_clock::duration duration) {
  const auto now = std::chrono::steady_clock::now();
  if (duration <= std::chrono::steady_clock::duration::zero()) { deadline = now; }
  else if (duration > std::chrono::steady_clock::time_point::max() - now) {
    deadline = std::chrono::steady_clock::time_point::max();
  } else { deadline = now + duration; }
}

// XML whitespace is exactly space/tab/CR/LF, regardless of the process locale.
std::string normalize_whitespace(std::string_view text, whitespace_policy policy) {
  if (policy == whitespace_policy::preserve) { return std::string{text}; }
  std::string result{};
  result.reserve(text.size());
  bool pending_space{};
  for (const auto character : text) {
    const bool whitespace = std::string_view{" \t\r\n"}.find(character) != std::string_view::npos;
    if (policy == whitespace_policy::replace) { result += whitespace ? ' ' : character; }
    else if (whitespace) { pending_space = !result.empty(); }
    else {
      if (pending_space) { result += ' '; }
      result += character;
      pending_space = false;
    }
  }
  return result;
}

namespace {
// Decimal significands keep integer facets exact even when long double has binary64 precision.
struct decimal_value {
  bool negative{};
  int infinity{};
  std::string digits{};
  std::int64_t magnitude{};
};

// Normalize decimal/scientific notation without converting to a binary floating representation.
decimal_value decimal_number(std::string_view lexical) {
  lexical = detail::trim(lexical);
  decimal_value result{};
  if (lexical == "INF" || lexical == "+INF") { result.infinity = 1; return result; }
  if (lexical == "-INF") { result.infinity = -1; return result; }
  if (lexical.empty()) { throw std::invalid_argument{"Empty XML facet number"}; }
  if (lexical.front() == '-' || lexical.front() == '+') {
    result.negative = lexical.front() == '-'; lexical.remove_prefix(1U);
  }
  std::size_t index{}, fractional_digits{};
  bool point_seen{}, digit_seen{};
  for (; index < lexical.size(); ++index) {
    const auto character = lexical[index];
    if (character >= '0' && character <= '9') {
      result.digits += character; digit_seen = true;
      if (point_seen) { ++fractional_digits; }
    } else if (character == '.' && !point_seen) { point_seen = true; }
    else { break; }
  }
  if (!digit_seen) { throw std::invalid_argument{"Invalid XML facet number"}; }
  std::int64_t exponent{};
  if (index < lexical.size() && (lexical[index] == 'e' || lexical[index] == 'E')) {
    auto encoded = lexical.substr(index + 1U);
    if (!encoded.empty() && encoded.front() == '+') { encoded.remove_prefix(1U); }
    if (encoded.empty()) { throw std::invalid_argument{"Invalid XML facet exponent"}; }
    const auto converted = std::from_chars(encoded.data(), encoded.data() + encoded.size(), exponent);
    if (converted.ec != std::errc{} || converted.ptr != encoded.data() + encoded.size()) {
      throw std::invalid_argument{"Invalid or overflowing XML facet exponent"};
    }
    index = lexical.size();
  }
  if (index != lexical.size()) { throw std::invalid_argument{"Invalid XML facet number suffix"}; }
  const auto first = result.digits.find_first_not_of('0');
  if (first == std::string::npos) { result.digits = "0"; result.negative = false; return result; }
  result.digits.erase(0, first);
  constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
  if (result.digits.size() > static_cast<std::uint64_t>(maximum) ||
      fractional_digits > static_cast<std::uint64_t>(maximum)) {
    throw std::length_error{"XML facet significand is too large"};
  }
  const auto position = static_cast<std::int64_t>(result.digits.size()) -
      static_cast<std::int64_t>(fractional_digits);
  if ((position > 0 && exponent > maximum - position) ||
      (position < 0 && exponent < std::numeric_limits<std::int64_t>::min() - position)) {
    throw std::invalid_argument{"XML facet decimal position overflows"};
  }
  result.magnitude = exponent + position;
  return result;
}
} // namespace

// Compare sign, decimal position and zero-padded digits; no platform floating precision is involved.
int compare_numeric_values(std::string_view left, std::string_view right) {
  const auto first = decimal_number(left);
  const auto second = decimal_number(right);
  if (first.infinity || second.infinity) {
    return (first.infinity > second.infinity) - (first.infinity < second.infinity);
  }
  const bool first_zero = first.digits == "0";
  const bool second_zero = second.digits == "0";
  if (first_zero && second_zero) { return 0; }
  if (first.negative != second.negative) { return first.negative ? -1 : 1; }
  int order{};
  if (first_zero || second_zero) { order = first_zero ? -1 : 1; }
  else if (first.magnitude != second.magnitude) { order = first.magnitude < second.magnitude ? -1 : 1; }
  else {
    const auto count = std::max(first.digits.size(), second.digits.size());
    for (std::size_t index{}; index < count; ++index) {
      const auto first_digit = index < first.digits.size() ? first.digits[index] : '0';
      const auto second_digit = index < second.digits.size() ? second.digits[index] : '0';
      if (first_digit != second_digit) { order = first_digit < second_digit ? -1 : 1; break; }
    }
  }
  return first.negative ? -order : order;
}

namespace {
// Round both the instance and facet into the selected XSD floating value space before comparison.
template <typename Value> void validate_floating_bounds(std::string_view text,
    const scalar_constraints& constraints) {
  Value value{};
  detail::read_scalar(text, value);
  const auto bound = [](std::string_view lexical) {
    Value result{};
    detail::read_scalar(lexical, result);
    return result;
  };
  // XSD 1.0 treats NaN as equal to itself, while it remains unordered with every other value.
  const auto inclusive = [value](Value limit, bool minimum) {
    return (std::isnan(value) && std::isnan(limit)) || (minimum ? value >= limit : value <= limit);
  };
  if ((constraints.finite && !std::isfinite(value)) ||
      (constraints.lexical_min_inclusive && !inclusive(bound(*constraints.lexical_min_inclusive), true)) ||
      (constraints.lexical_max_inclusive && !inclusive(bound(*constraints.lexical_max_inclusive), false)) ||
      (constraints.lexical_min_exclusive && !(value > bound(*constraints.lexical_min_exclusive))) ||
      (constraints.lexical_max_exclusive && !(value < bound(*constraints.lexical_max_exclusive))) ||
      (constraints.min_inclusive && !inclusive(static_cast<Value>(*constraints.min_inclusive), true)) ||
      (constraints.max_inclusive && !inclusive(static_cast<Value>(*constraints.max_inclusive), false)) ||
      (constraints.min_exclusive && !(value > static_cast<Value>(*constraints.min_exclusive))) ||
      (constraints.max_exclusive && !(value < static_cast<Value>(*constraints.max_exclusive)))) {
    throw std::invalid_argument{"XML scalar floating numeric facet failed"};
  }
}
} // namespace

// Validate supported facets without locale-dependent parsing or byte-count/character confusion.
void validate_scalar_constraints(std::string_view text, const scalar_constraints& constraints,
    operation_context* context) {
  if (context) {
    context->consume(operation_resource::work_units, text.size());
    context->consume(operation_resource::allocation_bytes, text.size());
  }
  check_text(text, context);
  const auto normalized = normalize_whitespace(text, constraints.whitespace);
  auto character_count = static_cast<std::size_t>(std::count_if(
      normalized.begin(), normalized.end(), [](unsigned char byte) { return (byte & 0xc0U) != 0x80U; }));
  if (constraints.list_length) {
    character_count = 0U;
    bool token{};
    for (const auto character : normalized) {
      const bool whitespace = std::string_view{" \t\r\n"}.find(character) != std::string_view::npos;
      if (!whitespace && !token) { ++character_count; }
      token = !whitespace;
    }
  }
  if ((constraints.length && character_count != *constraints.length) ||
      (constraints.min_length && character_count < *constraints.min_length) ||
      (constraints.max_length && character_count > *constraints.max_length)) {
    throw std::invalid_argument{"XML scalar length facet failed"};
  }
  if (!constraints.enumeration.empty() &&
      std::none_of(constraints.enumeration.begin(), constraints.enumeration.end(),
          [&](std::string_view candidate) {
            if (context) { context->consume(operation_resource::work_units, candidate.size()); }
            return normalized == normalize_whitespace(candidate, constraints.whitespace);
          })) {
    throw std::invalid_argument{"XML scalar enumeration facet failed"};
  }
  const bool numeric_constraints = constraints.finite || constraints.min_inclusive || constraints.max_inclusive ||
      constraints.min_exclusive || constraints.max_exclusive || constraints.lexical_min_inclusive ||
      constraints.lexical_max_inclusive || constraints.lexical_min_exclusive || constraints.lexical_max_exclusive;
  if (constraints.lexical_kind == "float" || constraints.lexical_kind == "double") {
    if (numeric_constraints) {
      if (constraints.lexical_kind == "float") { validate_floating_bounds<float>(normalized, constraints); }
      else { validate_floating_bounds<double>(normalized, constraints); }
    }
    return;
  }
  if ((constraints.lexical_min_inclusive && compare_numeric_values(normalized, *constraints.lexical_min_inclusive) < 0) ||
      (constraints.lexical_max_inclusive && compare_numeric_values(normalized, *constraints.lexical_max_inclusive) > 0) ||
      (constraints.lexical_min_exclusive && compare_numeric_values(normalized, *constraints.lexical_min_exclusive) <= 0) ||
      (constraints.lexical_max_exclusive && compare_numeric_values(normalized, *constraints.lexical_max_exclusive) >= 0)) {
    throw std::invalid_argument{"XML scalar exact numeric facet failed"};
  }
  if (constraints.finite || constraints.min_inclusive || constraints.max_inclusive ||
      constraints.min_exclusive || constraints.max_exclusive) {
    long double value{};
    detail::read_scalar(normalized, value);
    if (std::isnan(value) || (constraints.finite && !std::isfinite(value)) ||
        (constraints.min_inclusive && !(value >= *constraints.min_inclusive)) ||
        (constraints.max_inclusive && !(value <= *constraints.max_inclusive)) ||
        (constraints.min_exclusive && !(value > *constraints.min_exclusive)) ||
        (constraints.max_exclusive && !(value < *constraints.max_exclusive))) {
      throw std::invalid_argument{"XML scalar numeric facet failed"};
    }
  }
}

// Resolve unprefixed QNames using the in-scope default namespace, as XML Schema requires.
name resolve_qname(std::string_view lexical, const element& scope) {
  lexical = detail::trim(lexical);
  const auto colon = lexical.find(':');
  if (colon == std::string_view::npos) {
    check_name(lexical);
    const auto binding = scope.namespace_bindings.find("");
    return {std::string{lexical}, binding == scope.namespace_bindings.end() ? "" : binding->second};
  }
  const auto prefix = lexical.substr(0, colon);
  const auto local = lexical.substr(colon + 1U);
  check_name(prefix);
  check_name(local);
  if (prefix == "xmlns") { throw std::invalid_argument{"Reserved QName prefix"}; }
  if (prefix == "xml") { return {std::string{local}, "http://www.w3.org/XML/1998/namespace"}; }
  const auto binding = scope.namespace_bindings.find(std::string{prefix});
  if (binding == scope.namespace_bindings.end() || binding->second.empty()) {
    throw std::invalid_argument{"Unbound QName prefix"};
  }
  return {std::string{local}, binding->second};
}

// Prefer an existing named binding; generated prefixes never overwrite retained namespace context.
std::string qualify_qname(const name& value, element& scope) {
  check_name(value.local);
  check_text(value.namespace_uri);
  if (value.namespace_uri.empty()) {
    const auto binding = scope.namespace_bindings.find("");
    if (binding != scope.namespace_bindings.end() && !binding->second.empty()) {
      throw std::invalid_argument{"Unqualified QName conflicts with retained default namespace"};
    }
    return value.local;
  }
  if (value.namespace_uri == "http://www.w3.org/XML/1998/namespace") { return "xml:" + value.local; }
  if (value.namespace_uri == "http://www.w3.org/2000/xmlns/") {
    throw std::invalid_argument{"Reserved QName namespace"};
  }
  for (const auto& [prefix, uri] : scope.namespace_bindings) {
    if (!prefix.empty() && prefix != "xmlns" && uri == value.namespace_uri) {
      return prefix + ":" + value.local;
    }
  }
  std::size_t next_prefix{};
  std::string prefix{};
  do { prefix = "q" + std::to_string(next_prefix++); }
  while (scope.namespace_bindings.contains(prefix));
  scope.namespace_bindings.emplace(prefix, value.namespace_uri);
  return prefix + ":" + value.local;
}

// Prefix-list values refer to namespace URI identities, never their arbitrary authored spellings.
std::vector<std::string> resolve_prefix_list(std::string_view lexical, const element& scope) {
  std::vector<std::string> result{};
  lexical = detail::trim(lexical);
  while (!lexical.empty()) {
    const auto end = lexical.find_first_of(" \t\r\n");
    const auto prefix = lexical.substr(0, end);
    check_name(prefix);
    if (prefix == "xml") { result.emplace_back("http://www.w3.org/XML/1998/namespace"); }
    else {
      const auto binding = scope.namespace_bindings.find(std::string{prefix});
      if (prefix == "xmlns" || binding == scope.namespace_bindings.end() || binding->second.empty()) {
        throw std::invalid_argument{"Unbound namespace-list prefix"};
      }
      result.push_back(binding->second);
    }
    if (end == std::string_view::npos) { lexical = {}; }
    else { lexical = detail::trim(lexical.substr(end)); }
  }
  return result;
}

// Reuse the QName declaration allocator while emitting only its validated prefix component.
std::string qualify_prefix_list(std::span<const std::string> namespaces, element& scope) {
  std::string result{};
  for (const auto& uri : namespaces) {
    if (uri.empty()) { throw std::invalid_argument{"Namespace-list URI is empty"}; }
    auto qualified = qualify_qname({"value", uri}, scope);
    qualified.resize(qualified.find(':'));
    if (!result.empty()) { result += ' '; }
    result += qualified;
  }
  return result;
}

// Keep imported built-in type checks independent of application/model storage choices.
void validate_lexical_value(std::string_view text, std::string_view kind, const element& scope,
    operation_context* context) {
  if (context) { context->consume(operation_resource::work_units, text.size()); }
  if (kind.empty() || kind == "string" || kind == "normalizedString" || kind == "token" || kind == "anyURI") {
    check_text(text, context); return;
  }
  if (kind == "QName") { (void)resolve_qname(text, scope); return; }
  if (kind == "language") {
    std::size_t group{};
    bool first = true;
    for (std::size_t index{}; index < text.size(); ++index) {
      if (context && index % 4096U == 0U) { context->check(); }
      const auto byte = text[index];
      if (byte == '-') {
        if (group == 0U) { throw std::invalid_argument{"Invalid XSD language value"}; }
        group = 0U;
        first = false;
      } else {
        const bool alpha = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z');
        if ((!alpha && (first || byte < '0' || byte > '9')) || ++group > 8U) {
          throw std::invalid_argument{"Invalid XSD language value"};
        }
      }
    }
    if (group == 0U) { throw std::invalid_argument{"Invalid XSD language value"}; }
    return;
  }
  if (kind == "Name" || kind == "NCName" || kind == "ID" || kind == "IDREF" ||
      kind == "ENTITY" || kind == "NMTOKEN") {
#if SERIALIZER_HAS_XML
    check_text(text, context);
    if (context) { context->consume(operation_resource::allocation_bytes, text.size() + 1U); }
    const std::string terminated{text};
    const auto* value = reinterpret_cast<const xmlChar*>(terminated.c_str());
    const auto status = kind == "Name" ? xmlValidateName(value, 0)
        : kind == "NMTOKEN" ? xmlValidateNMToken(value, 0) : xmlValidateNCName(value, 0);
    if (context) { context->check(); }
    if (status != 0) { throw std::invalid_argument{"Invalid XSD name lexical value"}; }
    return;
#else
    throw std::runtime_error{"XSD name validation requires the XML provider"};
#endif
  }
  if (kind == "integer" || kind == "positiveInteger" || kind == "nonNegativeInteger" ||
      kind == "negativeInteger" || kind == "nonPositiveInteger" || kind == "decimal") {
    std::size_t position = !text.empty() && (text.front() == '+' || text.front() == '-') ? 1U : 0U;
    std::size_t digits{};
    bool decimal{};
    bool nonzero{};
    for (; position < text.size(); ++position) {
      if (context && position % 4096U == 0U) { context->check(); }
      const auto byte = text[position];
      if (byte >= '0' && byte <= '9') { ++digits; nonzero = nonzero || byte != '0'; }
      else if (kind == "decimal" && byte == '.' && !decimal) { decimal = true; }
      else { throw std::invalid_argument{"Invalid unbounded XSD numeric lexical value"}; }
    }
    if (digits == 0U) { throw std::invalid_argument{"Empty XSD numeric lexical value"}; }
    const bool negative = text.front() == '-';
    if ((kind == "positiveInteger" && (negative || !nonzero)) ||
        (kind == "nonNegativeInteger" && negative && nonzero) ||
        (kind == "negativeInteger" && (!negative || !nonzero)) ||
        (kind == "nonPositiveInteger" && !negative && nonzero)) {
      throw std::invalid_argument{"XSD integer sign constraint failed"};
    }
    return;
  }
  if (kind == "boolean") { bool value{}; detail::read_scalar(text, value); }
  else if (kind == "byte") { std::int8_t value{}; detail::read_scalar(text, value); }
  else if (kind == "unsignedByte") { std::uint8_t value{}; detail::read_scalar(text, value); }
  else if (kind == "short") { std::int16_t value{}; detail::read_scalar(text, value); }
  else if (kind == "unsignedShort") { std::uint16_t value{}; detail::read_scalar(text, value); }
  else if (kind == "int") { std::int32_t value{}; detail::read_scalar(text, value); }
  else if (kind == "unsignedInt") { std::uint32_t value{}; detail::read_scalar(text, value); }
  else if (kind == "long") { std::int64_t value{}; detail::read_scalar(text, value); }
  else if (kind == "unsignedLong") { std::uint64_t value{}; detail::read_scalar(text, value); }
  else if (kind == "float") { float value{}; detail::read_scalar(text, value); }
  else if (kind == "double") { double value{}; detail::read_scalar(text, value); }
  else { throw std::invalid_argument{"Unsupported XML lexical type"}; }
}

// Validate token-by-token without allocating a second model-sized collection.
void validate_lexical_list(std::string_view text, std::string_view item_kind, const element& scope,
    operation_context* context) {
  text = detail::trim(text);
  while (!text.empty()) {
    const auto end = text.find_first_of(" \t\r\n");
    validate_lexical_value(text.substr(0, end), item_kind, scope, context);
    if (end == std::string_view::npos) { return; }
    text = detail::trim(text.substr(end));
  }
}

namespace {
// Floating list equality follows the target XSD value space, including its distinguished NaN value.
template <typename T> bool floating_lexical_equal(std::string_view first, std::string_view second) {
  T left{}, right{};
  detail::read_scalar(first, left); detail::read_scalar(second, right);
  return left == right || (std::isnan(left) && std::isnan(right));
}
// Compare one validated built-in scalar value without locale or integer-width loss.
bool lexical_equal(std::string_view first, std::string_view second, std::string_view kind,
    const element& scope) {
  if (kind == "float") { return floating_lexical_equal<float>(first, second); }
  if (kind == "double") { return floating_lexical_equal<double>(first, second); }
  if (kind == "boolean") {
    bool left{}, right{}; detail::read_scalar(first, left); detail::read_scalar(second, right);
    return left == right;
  }
  if (kind == "QName") { return resolve_qname(first, scope) == resolve_qname(second, scope); }
  if (kind.empty() || kind == "string" || kind == "normalizedString" || kind == "token" || kind == "anyURI") {
    return first == second;
  }
  validate_lexical_value(first, kind, scope); validate_lexical_value(second, kind, scope);
  if (kind == "language" || kind == "Name" || kind == "NCName" || kind == "ID" ||
      kind == "IDREF" || kind == "ENTITY" || kind == "NMTOKEN") { return first == second; }
  return compare_numeric_values(first, second) == 0;
}
} // namespace

// Token pairs compare in the selected item value space; a length mismatch always differs.
bool lexical_lists_equal(std::string_view first, std::string_view second,
    std::string_view item_kind, const element& scope) {
  first = detail::trim(first); second = detail::trim(second);
  while (!first.empty() && !second.empty()) {
    const auto left_end = first.find_first_of(" \t\r\n");
    const auto right_end = second.find_first_of(" \t\r\n");
    if (!lexical_equal(first.substr(0, left_end), second.substr(0, right_end), item_kind, scope)) { return false; }
    first = left_end == std::string_view::npos ? std::string_view{} : detail::trim(first.substr(left_end));
    second = right_end == std::string_view::npos ? std::string_view{} : detail::trim(second.substr(right_end));
  }
  return first.empty() && second.empty();
}

// Validate manually edited preservation slots with the same namespace/occurrence rules as input.
void validate_extensions(const extension_store& extensions, const name& owner,
    operation_context* context, std::span<const extension_rule> rules) {
  std::vector<std::size_t> counts(rules.size(), 0U);
  const auto check = [&](const name& value, bool attribute, std::size_t position) {
    if (!context || context->unknown_fields != unknown_policy::preserve ||
        value.namespace_uri.empty() || value.namespace_uri == owner.namespace_uri ||
        !context->permits_namespace || !context->permits_namespace(value.namespace_uri)) {
      throw std::invalid_argument{"Preserved XML extension namespace is not permitted"};
    }
    context->consume(operation_resource::work_units, 1U);
    if (rules.empty()) { return; }
    for (std::size_t index{}; index < rules.size(); ++index) {
      const auto& rule = rules[index];
      if (rule.attribute != attribute || rule.strict || (!attribute && rule.before_field != position) ||
          !matches_namespace_rule(value.namespace_uri, rule.namespace_rule, rule.target_namespace)) { continue; }
      if (rule.maximum && counts[index] >= *rule.maximum) {
        throw std::invalid_argument{"Preserved XML wildcard maximum exceeded"};
      }
      ++counts[index]; return;
    }
    throw std::invalid_argument{"Preserved XML extension has no matching wildcard"};
  };
  for (const auto& item : extensions.attributes) { check(item.qualified_name, true, 0U); }
  for (const auto& item : extensions.children) { check(item.value.qualified_name, false, item.before_field); }
  for (const auto& wrapper : extensions.wrappers) {
    validate_extensions(wrapper.extensions, wrapper.qualified_name, context);
  }
  for (std::size_t index{}; index < rules.size(); ++index) {
    if (counts[index] < rules[index].minimum) {
      throw std::invalid_argument{"Required preserved XML wildcard content missing"};
    }
  }
}

// A namespace policy cannot forgive misplaced core children or registered known foreign fields.
bool matches_namespace_rule(std::string_view uri, std::string_view rule,
    std::string_view target_namespace) {
  rule = detail::trim(rule);
  if (rule == "##any") { return true; }
  if (rule == "##other") { return !uri.empty() && uri != target_namespace; }
  while (!rule.empty()) {
    const auto end = rule.find_first_of(" \t\r\n");
    const auto token = rule.substr(0, end);
    if ((token == "##local" && uri.empty()) ||
        (token == "##targetNamespace" && uri == target_namespace) || token == uri) { return true; }
    if (end == std::string_view::npos) { return false; }
    rule = detail::trim(rule.substr(end));
  }
  return false;
}

// A selected wildcard is an additional restriction on the explicit application namespace policy.
bool field_reader::accepts_extension(const name& value, bool child) const {
  if (!context_ || context_->unknown_fields == unknown_policy::reject ||
      value.namespace_uri.empty() || value.namespace_uri == node_.qualified_name.namespace_uri ||
      !context_->permits_namespace || !context_->permits_namespace(value.namespace_uri)) { return false; }
  if (child && std::find(known_children_.begin(), known_children_.end(), value) != known_children_.end()) { return false; }
  if (extension_rules_.empty()) { return true; }
  return std::any_of(extension_rules_.begin(), extension_rules_.end(), [&](const auto& rule) {
    return rule.attribute != child && !rule.strict &&
        (!child || rule.before_field == active_field_) &&
        matches_namespace_rule(value.namespace_uri, rule.namespace_rule, rule.target_namespace);
  });
}

// Matching wildcard counts include skipped values and cannot exceed declared maxima.
void field_reader::record_extension(const name& value, bool child) const {
  for (std::size_t index{}; index < extension_rules_.size(); ++index) {
    const auto& rule = extension_rules_[index];
    if (rule.attribute == child || rule.strict || (child && rule.before_field != active_field_) ||
        !matches_namespace_rule(value.namespace_uri, rule.namespace_rule, rule.target_namespace)) { continue; }
    auto& count = extension_counts_[index];
    if (rule.maximum && count >= *rule.maximum) {
      throw std::invalid_argument{"XML wildcard occurrence maximum exceeded"};
    }
    if (count == std::numeric_limits<std::size_t>::max()) { throw std::length_error{"XML wildcard count overflow"}; }
    ++count;
    return;
  }
}

namespace {
// Sum logical retained storage before copying an extension subtree; reject arithmetic overflow.
std::size_t retained_bytes(const element& node, operation_context* context) {
  std::size_t result{sizeof(element)};
  const auto add = [&](std::size_t amount) {
    consume(result, amount, std::numeric_limits<std::size_t>::max());
  };
  if (context) { context->consume(operation_resource::work_units, 1U); }
  add(node.qualified_name.local.size()); add(node.qualified_name.namespace_uri.size()); add(node.text.size());
  for (const auto& item : node.attributes) {
    add(sizeof(attribute)); add(item.qualified_name.local.size());
    add(item.qualified_name.namespace_uri.size()); add(item.value.size());
  }
  for (const auto& [prefix, uri] : node.namespace_bindings) {
    add(sizeof(std::pair<const std::string, std::string>)); add(prefix.size()); add(uri.size());
  }
  for (const auto& child : node.children) { add(retained_bytes(child, context)); }
  for (const auto& segment : node.text_segments) { add(sizeof(element::text_segment)); add(segment.value.size()); }
  return result;
}
// Charge a previously retained subtree against the generated output's independent limits.
void budget_extension(const element& node, build_budget& budget) {
  const auto depth = budget.enter(node.qualified_name);
  budget.text(node.text.size());
  for (const auto& item : node.attributes) {
    budget.attribute_name(item.qualified_name); budget.text(item.value.size());
  }
  for (const auto& [prefix, uri] : node.namespace_bindings) { budget.namespace_binding(prefix, uri); }
  for (const auto& child : node.children) { budget_extension(child, budget); }
  for (const auto& segment : node.text_segments) {
    budget.retained_storage(sizeof(element::text_segment)); budget.retained_storage(segment.value.size());
  }
}
} // namespace

// Retention requires an explicit per-record slot so preservation never silently loses unknown data.
void field_reader::skip_extensions(const name* requested) const {
  while (next_child_ < node_.children.size()) {
    const auto& child = node_.children[next_child_];
    if ((requested && child.qualified_name == *requested) || !accepts_extension(child.qualified_name, true)) { return; }
    context_->consume(operation_resource::work_units, 1U);
    record_extension(child.qualified_name, true);
    if (context_->unknown_fields == unknown_policy::preserve) {
      auto& extensions = retain_extensions();
      const auto bytes = retained_bytes(child, context_);
      context_->consume(operation_resource::preserved_bytes, bytes);
      context_->consume(operation_resource::allocation_bytes, bytes);
      extensions.children.push_back({known_children_read_, child, active_field_});
    }
    ++next_child_;
  }
}

// Finish consumes only explicitly allowed foreign data; all unknown core names remain errors.
void field_reader::finish() const {
  if (finished_) { return; }
  for (std::size_t index{}; index < used_attributes_.size(); ++index) {
    if (used_attributes_[index]) { continue; }
    const auto& item = node_.attributes[index];
    if (!accepts_extension(item.qualified_name, false)) { throw std::invalid_argument{"Unexpected XML attribute"}; }
    context_->consume(operation_resource::work_units, 1U);
    record_extension(item.qualified_name, false);
    if (context_->unknown_fields == unknown_policy::preserve) {
      auto& extensions = retain_extensions();
      const auto bytes = sizeof(attribute) + item.qualified_name.local.size() +
          item.qualified_name.namespace_uri.size() + item.value.size();
      context_->consume(operation_resource::preserved_bytes, bytes);
      context_->consume(operation_resource::allocation_bytes, bytes);
      extensions.attributes.push_back(item);
    }
  }
  active_field_ = next_field_;
  skip_extensions();
  if (next_child_ != node_.children.size()) { throw std::invalid_argument{"Unexpected XML child"}; }
  for (std::size_t index{}; index < extension_rules_.size(); ++index) {
    if (extension_counts_[index] < extension_rules_[index].minimum) {
      throw std::invalid_argument{"Required XML wildcard content missing"};
    }
  }
  auto* retained = extensions_ ? extensions_ : lazy_extensions_ ?
      lazy_extensions_->get() : nullptr;
  if (retained && !retained->attributes.empty()) {
    for (const auto& [prefix, uri] : node_.namespace_bindings) {
      const auto bytes = sizeof(std::pair<const std::string, std::string>) + prefix.size() + uri.size();
      context_->consume(operation_resource::preserved_bytes, bytes);
      context_->consume(operation_resource::allocation_bytes, bytes);
    }
    retained->namespace_bindings = node_.namespace_bindings;
  }
  finished_ = true;
}

// Reject semantic collisions before copying any opaque data over the generated known fields.
void restore_extensions(element& node, const extension_store& extensions, build_budget& budget) {
  for (const auto& item : extensions.attributes) {
    if (std::any_of(node.attributes.begin(), node.attributes.end(), [&](const auto& known) {
          return known.qualified_name == item.qualified_name;
        })) { throw std::invalid_argument{"Preserved XML attribute collides with a known field"}; }
    budget.attribute_name(item.qualified_name); budget.text(item.value.size());
    node.attributes.push_back(item);
  }
  for (const auto& [prefix, uri] : extensions.namespace_bindings) {
    const auto known = node.namespace_bindings.find(prefix);
    if (known != node.namespace_bindings.end()) {
      if (known->second != uri) { throw std::invalid_argument{"Preserved XML namespace binding conflicts"}; }
    } else {
      budget.namespace_binding(prefix, uri); node.namespace_bindings.emplace(prefix, uri);
    }
  }
  for (const auto& wrapper : extensions.wrappers) {
    const auto selected = std::find_if(node.children.begin(), node.children.end(), [&](const auto& child) {
      return child.qualified_name == wrapper.qualified_name;
    });
    if (selected == node.children.end()) { throw std::invalid_argument{"Preserved XML wrapper no longer exists"}; }
    const auto depth = budget.enter_existing();
    restore_extensions(*selected, wrapper.extensions, budget);
  }
  std::size_t inserted{}, previous_position{};
  const auto known_size = node.children.size();
  for (const auto& slot : extensions.children) {
    if (slot.before_child < previous_position) {
      throw std::invalid_argument{"Preserved XML child positions are unordered"};
    }
    previous_position = slot.before_child;
    budget_extension(slot.value, budget);
    const auto position = std::min(slot.before_child, known_size) + inserted;
    node.children.insert(node.children.begin() + static_cast<std::ptrdiff_t>(position), slot.value);
    ++inserted;
  }
}

bool available() noexcept { return SERIALIZER_HAS_XML != 0; }

element parse(std::string_view document, limits bounds) {
  return parse(document, bounds, nullptr);
}

// Keep ordinary callers on the memory reader; only selected operations install checked I/O.
element parse(std::string_view document, limits bounds, operation_context* context) {
  if (document.size() > bounds.max_input_bytes || document.size() > static_cast<std::size_t>(INT_MAX)) {
    throw std::length_error{"XML input limit exceeded"};
  }
#if SERIALIZER_HAS_XML
  if (context) { context->check(); }
  const auto* encoding = provider_encoding(document, context);
  std::optional<checked_input> input{};
  if (context) { input.emplace(checked_input{document, *context, {}}); }
  const auto reader = std::unique_ptr<xmlTextReader, decltype(&xmlFreeTextReader)>{
      context ? xmlReaderForIO(&checked_input::read, nullptr, &*input, nullptr, encoding,
                         XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING) :
                xmlReaderForMemory(document.data(), static_cast<int>(document.size()), nullptr, encoding,
                         XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING), &xmlFreeTextReader};
  if (input && input->failure) { std::rethrow_exception(input->failure); }
  if (!reader) { throw std::invalid_argument{"Cannot create XML reader"}; }
  element result{};
  std::vector<element*> stack{};
  std::vector<bool> opaque_stack{};
  std::size_t nodes{}, attributes{}, text_bytes{}, name_bytes{}, namespace_bindings{}, tree_bytes{};
  const auto retain = [&](std::size_t amount) {
    consume(tree_bytes, amount, bounds.max_tree_bytes);
    if (context) { context->consume(operation_resource::allocation_bytes, amount); }
  };
  bool root_seen{false};
  int status{};
  while ((status = xmlTextReaderRead(reader.get())) == 1) {
    if (input && input->failure) { std::rethrow_exception(input->failure); }
    if (context) { context->consume(operation_resource::work_units, 1U); }
    if (copy(xmlTextReaderConstXmlVersion(reader.get())) != "1.0") {
      throw std::invalid_argument{"Only XML 1.0 documents are supported"};
    }
    const auto type = xmlTextReaderNodeType(reader.get());
    if (type == XML_READER_TYPE_DOCUMENT_TYPE || type == XML_READER_TYPE_ENTITY ||
        type == XML_READER_TYPE_ENTITY_REFERENCE) {
      throw std::invalid_argument{"XML DTDs and entities are prohibited"};
    }
    if (type == XML_READER_TYPE_ELEMENT) {
      consume(nodes, 1U, bounds.max_nodes);
      retain(sizeof(element));
      if (stack.size() >= bounds.max_depth) { throw std::length_error{"XML depth limit exceeded"}; }
      element* node{};
      if (stack.empty()) {
        if (root_seen) { throw std::invalid_argument{"Multiple XML roots"}; }
        root_seen = true; node = &result;
      } else {
        stack.back()->children.emplace_back(); node = &stack.back()->children.back();
      }
      node->qualified_name = {copy(xmlTextReaderConstLocalName(reader.get())),
                              copy(xmlTextReaderConstNamespaceUri(reader.get()))};
      consume(name_bytes, node->qualified_name.local.size(), bounds.max_name_bytes);
      consume(name_bytes, node->qualified_name.namespace_uri.size(), bounds.max_name_bytes);
      retain(node->qualified_name.local.size());
      retain(node->qualified_name.namespace_uri.size());
      if (!stack.empty()) {
        // Charge inherited maps before copying; compact nested documents cannot multiply metadata unboundedly.
        consume(namespace_bindings, stack.back()->namespace_bindings.size(), bounds.max_namespace_bindings);
        for (const auto& [prefix, uri] : stack.back()->namespace_bindings) {
          consume(name_bytes, prefix.size(), bounds.max_name_bytes);
          consume(name_bytes, uri.size(), bounds.max_name_bytes);
          retain(sizeof(std::pair<const std::string, std::string>));
          retain(prefix.size());
          retain(uri.size());
        }
        node->namespace_bindings = stack.back()->namespace_bindings;
      }
      check_name(node->qualified_name.local);
      const bool opaque = (!opaque_stack.empty() && opaque_stack.back()) ||
          (context && context->allow_mixed_content) ||
          (context && context->unknown_fields != unknown_policy::reject && context->permits_namespace &&
           !node->qualified_name.namespace_uri.empty() &&
           node->qualified_name.namespace_uri != result.qualified_name.namespace_uri &&
           context->permits_namespace(node->qualified_name.namespace_uri));
      if (xmlTextReaderMoveToFirstAttribute(reader.get()) == 1) {
        do {
          if (context) { context->consume(operation_resource::work_units, 1U); }
          if (xmlTextReaderIsNamespaceDecl(reader.get()) == 1) {
            const auto prefix = copy(xmlTextReaderConstPrefix(reader.get())) == "xmlns" ?
                copy(xmlTextReaderConstLocalName(reader.get())) : std::string{};
            const auto uri = copy(xmlTextReaderConstValue(reader.get()));
            consume(namespace_bindings, 1U, bounds.max_namespace_bindings);
            consume(name_bytes, prefix.size(), bounds.max_name_bytes);
            consume(name_bytes, uri.size(), bounds.max_name_bytes);
            retain(sizeof(std::pair<const std::string, std::string>));
            retain(prefix.size());
            retain(uri.size());
            node->namespace_bindings[prefix] = uri;
            continue;
          }
          consume(attributes, 1U, bounds.max_attributes);
          retain(sizeof(attribute));
          attribute item{{copy(xmlTextReaderConstLocalName(reader.get())),
                           copy(xmlTextReaderConstNamespaceUri(reader.get()))},
                          copy(xmlTextReaderConstValue(reader.get()))};
          check_name(item.qualified_name.local);
          consume(text_bytes, item.value.size(), bounds.max_text_bytes);
          consume(name_bytes, item.qualified_name.local.size(), bounds.max_name_bytes);
          consume(name_bytes, item.qualified_name.namespace_uri.size(), bounds.max_name_bytes);
          retain(item.value.size());
          retain(item.qualified_name.local.size());
          retain(item.qualified_name.namespace_uri.size());
          node->attributes.push_back(std::move(item));
        } while (xmlTextReaderMoveToNextAttribute(reader.get()) == 1);
        xmlTextReaderMoveToElement(reader.get());
      }
      if (xmlTextReaderIsEmptyElement(reader.get()) != 1) {
        stack.push_back(node); opaque_stack.push_back(opaque);
      }
    } else if (type == XML_READER_TYPE_END_ELEMENT) {
      if (stack.empty()) { throw std::invalid_argument{"Unexpected XML closing element"}; }
      const auto* node = stack.back();
      if (!node->children.empty() && !detail::trim(node->text).empty() && node->text_segments.empty()) {
        throw std::invalid_argument{"XML mixed content is unsupported"};
      }
      stack.pop_back();
      opaque_stack.pop_back();
    } else if (type == XML_READER_TYPE_TEXT || type == XML_READER_TYPE_CDATA ||
               type == XML_READER_TYPE_WHITESPACE || type == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
      const auto text = copy(xmlTextReaderConstValue(reader.get()));
      consume(text_bytes, text.size(), bounds.max_text_bytes);
      retain(text.size());
      if (!stack.empty()) {
        if (opaque_stack.back()) {
          retain(sizeof(element::text_segment)); retain(text.size());
          stack.back()->text_segments.push_back({stack.back()->children.size(), text});
        }
        stack.back()->text += text;
      }
      else if (!detail::trim(text).empty()) { throw std::invalid_argument{"Text outside XML root"}; }
    }
  }
  if (input && input->failure) { std::rethrow_exception(input->failure); }
  if (context) { context->check(); }
  if (status != 0 || !stack.empty() || !root_seen) { throw std::invalid_argument{"Malformed XML document"}; }
  return result;
#else
  (void)document; (void)bounds; (void)context;
  throw std::runtime_error{"XML support was not enabled in this Serializer runtime"};
#endif
}

std::string write(const element& document, limits bounds) {
  return document_writer{bounds}.encode(document);
}

// Selected package codecs share interruption and resource charges through their caller context.
std::string write(const element& document, limits bounds, operation_context* context) {
  return document_writer{bounds, context}.encode(document);
}

struct prepared_pattern::implementation {
#if SERIALIZER_HAS_XML
  std::unique_ptr<xmlRegexp, decltype(&xmlRegFreeRegexp)> expression{nullptr, &xmlRegFreeRegexp};
#endif
};

// Use the provider's XML Schema grammar, including Unicode categories and class subtraction.
prepared_pattern::prepared_pattern(std::string_view expression, limits bounds,
    operation_context* context) {
#if SERIALIZER_HAS_XML
  if (expression.size() > bounds.max_input_bytes || expression.size() > bounds.max_text_bytes ||
      expression.size() >= static_cast<std::size_t>(INT_MAX)) {
    throw std::length_error{"XML pattern input limit exceeded"};
  }
  check_text(expression, context);
  if (context) {
    context->consume(operation_resource::work_units, expression.size());
    context->consume(operation_resource::allocation_bytes, sizeof(implementation) + expression.size() + 1U);
  }
  const std::string terminated{expression};
  auto prepared = std::make_unique<implementation>();
  prepared->expression.reset(xmlRegexpCompile(reinterpret_cast<const xmlChar*>(terminated.c_str())));
  if (context) { context->check(); }
  if (!prepared->expression) { throw std::invalid_argument{"Invalid XML Schema regular expression"}; }
  implementation_ = std::move(prepared);
#else
  (void)expression; (void)bounds; (void)context;
  throw std::runtime_error{"XML pattern support was not enabled in this Serializer runtime"};
#endif
}

prepared_pattern::~prepared_pattern() = default;
prepared_pattern::prepared_pattern(prepared_pattern&&) noexcept = default;
prepared_pattern& prepared_pattern::operator=(prepared_pattern&&) noexcept = default;

// xmlRegexpExec allocates its execution counters/rollback state per call and reads the automaton.
bool prepared_pattern::matches(std::string_view value, limits bounds,
    operation_context* context) const {
#if SERIALIZER_HAS_XML
  if (!implementation_) { throw std::logic_error{"Moved-from prepared XML pattern"}; }
  if (value.size() > bounds.max_input_bytes || value.size() > bounds.max_text_bytes ||
      value.size() >= static_cast<std::size_t>(INT_MAX)) {
    throw std::length_error{"XML pattern value limit exceeded"};
  }
  check_text(value, context);
  if (context) {
    context->consume(operation_resource::work_units, value.size());
    context->consume(operation_resource::allocation_bytes, value.size() + 1U);
  }
  const std::string terminated{value};
  const auto result = xmlRegexpExec(implementation_->expression.get(),
      reinterpret_cast<const xmlChar*>(terminated.c_str()));
  if (context) { context->check(); }
  if (result < 0) { throw std::runtime_error{"XML Schema regular expression evaluation failed"}; }
  return result == 1;
#else
  (void)value; (void)bounds; (void)context;
  throw std::runtime_error{"XML pattern support was not enabled in this Serializer runtime"};
#endif
}

struct prepared_schema::implementation {
#if SERIALIZER_HAS_XML
  std::unique_ptr<xmlSchema, decltype(&xmlSchemaFree)> schema{nullptr, &xmlSchemaFree};
#endif
};

prepared_schema::prepared_schema(std::string_view xsd, limits bounds) {
#if SERIALIZER_HAS_XML
  operation_context schema_context{};
  schema_context.allow_mixed_content = true;
  const auto tree = parse(xsd, bounds, &schema_context);
  check_schema_references(tree);
  const auto context = std::unique_ptr<xmlSchemaParserCtxt, decltype(&xmlSchemaFreeParserCtxt)>{
      xmlSchemaNewMemParserCtxt(xsd.data(), static_cast<int>(xsd.size())), &xmlSchemaFreeParserCtxt};
  if (!context) { throw std::invalid_argument{"Cannot prepare XSD parser"}; }
  xmlSchemaSetParserErrors(context.get(), &discard_error, &discard_error, nullptr);
  implementation_ = std::make_unique<implementation>();
  implementation_->schema.reset(xmlSchemaParse(context.get()));
  if (!implementation_->schema) { throw std::invalid_argument{"Invalid or unsupported XSD 1.0 schema"}; }
#else
  (void)xsd; (void)bounds;
  throw std::runtime_error{"Runtime XSD support was not enabled in this Serializer runtime"};
#endif
}
prepared_schema::~prepared_schema() = default;
prepared_schema::prepared_schema(prepared_schema&&) noexcept = default;
prepared_schema& prepared_schema::operator=(prepared_schema&&) noexcept = default;

void prepared_schema::validate(std::string_view document, limits bounds) const {
  validate(document, bounds, nullptr);
}

// libxml schema validation has no bounded cancellation hook; guard its complete provider call.
void prepared_schema::validate(std::string_view document, limits bounds, operation_context* operation) const {
#if SERIALIZER_HAS_XML
  (void)parse(document, bounds, operation);
  if (operation) { operation->consume(operation_resource::work_units, document.size()); }
  if (!implementation_) { throw std::logic_error{"Moved-from prepared XML schema"}; }
  const auto tree = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>{
      xmlReadMemory(document.data(), static_cast<int>(document.size()), nullptr,
                    provider_encoding(document, operation),
                    XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING), &xmlFreeDoc};
  const auto context = std::unique_ptr<xmlSchemaValidCtxt, decltype(&xmlSchemaFreeValidCtxt)>{
      xmlSchemaNewValidCtxt(implementation_->schema.get()), &xmlSchemaFreeValidCtxt};
  if (!tree || !context) { throw std::runtime_error{"Cannot prepare XML validation context"}; }
  xmlSchemaSetValidErrors(context.get(), &discard_error, &discard_error, nullptr);
  if (xmlSchemaValidateDoc(context.get(), tree.get()) != 0) {
    throw std::invalid_argument{"XML document does not satisfy the selected XSD"};
  }
  if (operation) { operation->check(); }
#else
  (void)document; (void)bounds; (void)operation;
  throw std::runtime_error{"Runtime XSD support was not enabled in this Serializer runtime"};
#endif
}
namespace {
// Each observer belongs to one exact open owner depth and keeps its immutable metadata borrowed.
struct token_identity_pass {
  std::size_t owner_depth{};
  std::unique_ptr<identity_validator> validator{};
};

// Build borrowed semantic spans only for synchronous identity observers.
void observe_token_identities(std::vector<token_identity_pass>& passes, const token& value,
    operation_context* context) {
  if (passes.empty()) { return; }
  if (context) {
    context->consume(operation_resource::allocation_bytes,
        (value.attributes.size() + value.consumed_attributes.size() + (value.consumed_nil ? 1U : 0U) + (value.consumed_type ? 1U : 0U)) * sizeof(event_attribute) +
        value.namespaces.size() * sizeof(namespace_declaration));
  }
  std::vector<event_attribute> attributes{};
  for (const auto& attribute : value.attributes) {
    attributes.push_back({{attribute.qualified_name.local, attribute.qualified_name.namespace_uri}, attribute.value});
  }
  for (const auto& attribute : value.consumed_attributes) {
    attributes.push_back({{attribute.qualified_name.local, attribute.qualified_name.namespace_uri}, attribute.value});
  }
  if (value.consumed_nil) {
    const auto& attribute = *value.consumed_nil;
    attributes.push_back({{attribute.qualified_name.local, attribute.qualified_name.namespace_uri}, attribute.value});
  }
  if (value.consumed_type) {
    const auto& attribute = *value.consumed_type;
    attributes.push_back({{attribute.qualified_name.local, attribute.qualified_name.namespace_uri}, attribute.value});
  }
  std::vector<namespace_declaration> namespaces{};
  for (const auto& [prefix, uri] : value.namespaces) { namespaces.push_back({prefix, uri}); }
  const event_view event{value.kind, {value.qualified_name.local, value.qualified_name.namespace_uri},
      attributes, namespaces, value.text};
  for (auto& pass : passes) { pass.validator->accept(event); }
}

// Finish local keyrefs at the owner's end tag; document publication remains guarded by byte EOF.
void finish_token_identities(std::vector<token_identity_pass>& passes, std::size_t depth) {
  while (!passes.empty() && passes.back().owner_depth == depth) {
    passes.back().validator->accept({event_kind::end_document});
    passes.back().validator->finish();
    passes.pop_back();
  }
}
} // namespace

// Each output frame owns only its open name, attributes and optional observer start payload.
struct token_writer::implementation {
  struct frame {
    std::string rendered_name{};
    token start{event_kind::start_element};
    std::set<std::pair<std::string, std::string>> attributes{};
    bool pending{true};
    bool retained_namespaces{};
    std::size_t next_prefix{};
  };
  limits bounds;
  operation_context* operation;
  std::string bytes{};
  std::vector<frame> stack{};
  std::vector<token_identity_pass> identities{};
  std::size_t nodes{}, attributes{}, text_bytes{}, name_bytes{}, namespace_bindings{};
  bool root_seen{}, failed{}, completed{};

  // Bind one output pass without retaining caller model data.
  implementation(limits selected, operation_context* context) : bounds{selected}, operation{context} {}
  // Admit escaped output before copying bytes into the publication candidate.
  void append(std::string_view value) {
    if (value.size() > bounds.max_output_bytes - std::min(bytes.size(), bounds.max_output_bytes)) {
      throw std::length_error{"XML token output limit exceeded"};
    }
    if (operation) {
      operation->consume(operation_resource::allocation_bytes, value.size());
      operation->consume(operation_resource::work_units, value.size());
    }
    bytes += value;
  }
  // Escape XML text while retaining carriage returns and normalized attribute whitespace.
  void escaped(std::string_view value, bool is_attribute) {
    check_text(value, operation);
    std::size_t begin{};
    for (std::size_t index{}; index < value.size(); ++index) {
      std::string_view replacement{};
      switch (value[index]) {
      case '&': replacement = "&amp;"; break;
      case '<': replacement = "&lt;"; break;
      case '>': replacement = "&gt;"; break;
      case '"': if (is_attribute) { replacement = "&quot;"; } break;
      case '\r': replacement = "&#13;"; break;
      case '\n': if (is_attribute) { replacement = "&#10;"; } break;
      case '\t': if (is_attribute) { replacement = "&#9;"; } break;
      default: break;
      }
      if (!replacement.empty()) {
        append(value.substr(begin, index - begin));
        append(replacement);
        begin = index + 1U;
      }
    }
    append(value.substr(begin));
  }
  // Check an expanded name and charge its complete logical spelling.
  void inspect_name(const name& selected) {
    check_name(selected.local);
    check_text(selected.namespace_uri, operation);
    if (selected.namespace_uri == "http://www.w3.org/2000/xmlns/") {
      throw std::invalid_argument{"XML namespace declaration is not an ordinary name"};
    }
    consume(name_bytes, selected.local.size(), bounds.max_name_bytes);
    consume(name_bytes, selected.namespace_uri.size(), bounds.max_name_bytes);
  }
  // Close a pending start tag before writing its text or child content.
  void content() {
    if (stack.empty()) { throw std::invalid_argument{"XML text requires an open element"}; }
    if (stack.back().pending) {
      observe_token_identities(identities, stack.back().start, operation);
      append(">"); stack.back().pending = false;
      stack.back().start.attributes.clear();
    }
  }
  // Preserve a terminal failure fence across all public operations.
  template <typename Function> void apply(Function&& function) {
    if (failed || completed) { throw std::logic_error{"XML token writer is closed or failed"}; }
    try {
      if (operation) { operation->check(); }
      function();
    } catch (...) { failed = true; throw; }
  }
};

// Start one bounded output candidate.
token_writer::token_writer(limits bounds, operation_context* context)
    : state_{std::make_unique<implementation>(bounds, context)} {}
// Discard any unpublished candidate.
token_writer::~token_writer() = default;
// Emit a start tag with an element-local prefix, requiring no namespace prescan.
void token_writer::open(const name& selected) {
  static const std::map<std::string, std::string> empty{};
  open(selected, empty);
}
// Choose generated prefixes around retained lexical bindings, leaving ordinary output unchanged.
void token_writer::open(const name& selected, const std::map<std::string, std::string>& namespaces) {
  state_->apply([&] {
    auto& state = *state_;
    if (state.stack.empty() && state.root_seen) { throw std::invalid_argument{"Multiple XML roots"}; }
    if (state.stack.size() >= state.bounds.max_depth) { throw std::length_error{"XML token depth exceeded"}; }
    state.inspect_name(selected);
    consume(state.nodes, 1U, state.bounds.max_nodes);
    if (!state.stack.empty()) { state.content(); }
    implementation::frame frame{};
    if (state.operation) {
      state.operation->consume(operation_resource::allocation_bytes,
          sizeof(implementation::frame) + 2U * selected.local.size() + selected.namespace_uri.size() + std::string_view{"srl:"}.size());
    }
    frame.start.qualified_name = selected;
    const bool reserved = selected.namespace_uri == "http://www.w3.org/XML/1998/namespace";
    std::string element_prefix{"srl"};
    bool existing_binding{};
    for (const auto& [prefix, uri] : namespaces) {
      if (!selected.namespace_uri.empty() && uri == selected.namespace_uri) {
        if (state.operation) { state.operation->consume(operation_resource::allocation_bytes, prefix.size()); }
        element_prefix = prefix; existing_binding = true; break;
      }
    }
    if (!existing_binding) {
      while (namespaces.contains(element_prefix)) { element_prefix = "srl" + std::to_string(frame.next_prefix++); }
    }
    frame.retained_namespaces = !namespaces.empty();
    if (state.operation) { state.operation->consume(operation_resource::allocation_bytes, element_prefix.size()); }
    frame.rendered_name = selected.namespace_uri.empty() || (existing_binding && element_prefix.empty()) ? selected.local :
        (reserved ? std::string{"xml:"} : element_prefix + ':') + selected.local;
    state.append("<"); state.append(frame.rendered_name);
    for (const auto& [prefix, uri] : namespaces) {
      if (prefix == "xml" && uri == "http://www.w3.org/XML/1998/namespace") { continue; }
      if (prefix == "xml" || prefix == "xmlns" || uri == "http://www.w3.org/XML/1998/namespace" ||
          uri == "http://www.w3.org/2000/xmlns/" || (!prefix.empty() && uri.empty()) ||
          (prefix.empty() && !uri.empty() && selected.namespace_uri.empty())) {
        throw std::invalid_argument{"Invalid retained XML namespace declaration"};
      }
      if (!prefix.empty()) { check_name(prefix); }
      check_text(uri, state.operation);
      consume(state.namespace_bindings, 1U, state.bounds.max_namespace_bindings);
      consume(state.name_bytes, prefix.size(), state.bounds.max_name_bytes);
      consume(state.name_bytes, uri.size(), state.bounds.max_name_bytes);
      if (state.operation) {
        state.operation->consume(operation_resource::allocation_bytes,
            sizeof(std::pair<const std::string, std::string>) + prefix.size() + uri.size());
      }
      state.append(" xmlns");
      if (!prefix.empty()) { state.append(":"); state.append(prefix); }
      state.append("=\""); state.escaped(uri, true); state.append("\"");
      frame.start.namespaces.emplace(prefix, uri);
    }
    if (selected.namespace_uri.empty() && !frame.start.namespaces.contains("")) {
      for (auto ancestor = state.stack.rbegin(); ancestor != state.stack.rend(); ++ancestor) {
        const auto inherited = ancestor->start.namespaces.find("");
        if (inherited == ancestor->start.namespaces.end()) { continue; }
        if (!inherited->second.empty()) {
          consume(state.namespace_bindings, 1U, state.bounds.max_namespace_bindings);
          state.append(" xmlns=\"\"");
          frame.start.namespaces.emplace("", "");
        }
        break;
      }
    }
    if (!selected.namespace_uri.empty() && !reserved && !existing_binding) {
      consume(state.namespace_bindings, 1U, state.bounds.max_namespace_bindings);
      consume(state.name_bytes, element_prefix.size(), state.bounds.max_name_bytes);
      consume(state.name_bytes, selected.namespace_uri.size(), state.bounds.max_name_bytes);
      state.append(" xmlns:"); state.append(element_prefix); state.append("=\""); state.escaped(selected.namespace_uri, true); state.append("\"");
      if (state.operation) { state.operation->consume(operation_resource::allocation_bytes, element_prefix.size() + selected.namespace_uri.size()); }
      frame.start.namespaces.emplace(element_prefix, selected.namespace_uri);
    }
    state.stack.push_back(std::move(frame));
    state.root_seen = true;
  });
}
// Write attributes before the start tag is completed, checking expanded duplicates.
void token_writer::attribute_value(const name& selected, std::string_view value) {
  state_->apply([&] {
    auto& state = *state_;
    if (state.stack.empty() || !state.stack.back().pending) {
      throw std::invalid_argument{"XML attributes must precede content"};
    }
    if (selected.namespace_uri.empty() && selected.local == "xmlns") {
      throw std::invalid_argument{"XML namespace declarations are not ordinary attributes"};
    }
    state.inspect_name(selected);
    consume(state.attributes, 1U, state.bounds.max_attributes);
    consume(state.text_bytes, value.size(), state.bounds.max_text_bytes);
    if (state.operation) {
      state.operation->consume(operation_resource::allocation_bytes,
          sizeof(std::pair<std::string, std::string>) + selected.local.size() + selected.namespace_uri.size());
    }
    auto& frame = state.stack.back();
    if (!frame.attributes.emplace(selected.namespace_uri, selected.local).second) {
      throw std::invalid_argument{"Duplicate XML attribute"};
    }
    std::string prefix{};
    if (selected.namespace_uri == "http://www.w3.org/XML/1998/namespace") { prefix = "xml"; }
    else if (!selected.namespace_uri.empty()) {
      if (frame.retained_namespaces) {
        for (const auto& [retained_prefix, uri] : frame.start.namespaces) {
          if (!retained_prefix.empty() && uri == selected.namespace_uri) { prefix = retained_prefix; break; }
        }
      }
      if (prefix.empty()) {
        consume(state.namespace_bindings, 1U, state.bounds.max_namespace_bindings);
        do { prefix = "srl" + std::to_string(frame.next_prefix++); } while (frame.start.namespaces.contains(prefix));
        consume(state.name_bytes, prefix.size(), state.bounds.max_name_bytes);
        consume(state.name_bytes, selected.namespace_uri.size(), state.bounds.max_name_bytes);
        state.append(" xmlns:"); state.append(prefix); state.append("=\"");
        state.escaped(selected.namespace_uri, true); state.append("\"");
        if (state.operation) { state.operation->consume(operation_resource::allocation_bytes, prefix.size() + selected.namespace_uri.size()); }
        frame.start.namespaces.emplace(prefix, selected.namespace_uri);
      }
    }
    state.append(" ");
    if (!prefix.empty()) { state.append(prefix); state.append(":"); }
    state.append(selected.local); state.append("=\""); state.escaped(value, true); state.append("\"");
    if (!state.identities.empty()) {
      if (state.operation) {
        state.operation->consume(operation_resource::allocation_bytes,
            sizeof(attribute) + selected.local.size() + selected.namespace_uri.size() + value.size());
      }
      frame.start.attributes.push_back({selected, std::string{value}});
    }
  });
}
// Bind QName value prefixes locally; prefixes never select a native alternative by themselves.
void token_writer::qname_attribute_value(const name& selected, const name& value) {
  state_->apply([&] {
    auto& state = *state_;
    if (state.stack.empty() || !state.stack.back().pending) {
      throw std::invalid_argument{"XML QName attributes must precede content"};
    }
    state.inspect_name(value);
    std::string lexical{};
    if (value.namespace_uri.empty()) {
      if (state.operation) { state.operation->consume(operation_resource::allocation_bytes, value.local.size()); }
      lexical = value.local;
    } else if (value.namespace_uri == "http://www.w3.org/XML/1998/namespace") {
      if (state.operation) { state.operation->consume(operation_resource::allocation_bytes, std::string_view{"xml:"}.size() + value.local.size()); }
      lexical = "xml:" + value.local;
    }
    else {
      auto& frame = state.stack.back();
      std::string prefix;
      if (frame.retained_namespaces) {
        for (const auto& [retained_prefix, uri] : frame.start.namespaces) {
          if (!retained_prefix.empty() && uri == value.namespace_uri) { prefix = retained_prefix; break; }
        }
      }
      const bool retained = !prefix.empty();
      if (!retained) {
        do { prefix = "srl" + std::to_string(frame.next_prefix++); } while (frame.start.namespaces.contains(prefix));
        consume(state.namespace_bindings, 1U, state.bounds.max_namespace_bindings);
        consume(state.name_bytes, prefix.size(), state.bounds.max_name_bytes);
        consume(state.name_bytes, value.namespace_uri.size(), state.bounds.max_name_bytes);
        if (state.operation) {
          state.operation->consume(operation_resource::allocation_bytes, prefix.size() + value.namespace_uri.size());
        }
        state.append(" xmlns:"); state.append(prefix); state.append("=\"");
        state.escaped(value.namespace_uri, true); state.append("\"");
        frame.start.namespaces.emplace(prefix, value.namespace_uri);
      }
      if (state.operation) { state.operation->consume(operation_resource::allocation_bytes, prefix.size() + 1U + value.local.size()); }
      lexical = prefix + ':' + value.local;
    }
    attribute_value(selected, lexical);
  });
}
// Begin one local identity observer before any of this owner's attributes have been emitted.
void token_writer::begin_identity(const identity_scope& scope) {
  begin_identity(scope, state_->operation && state_->operation->identity_bounds ?
      *state_->operation->identity_bounds : identity_limits{});
}
// Explicit local bounds take precedence over a caller's default identity-index policy.
void token_writer::begin_identity(const identity_scope& scope, identity_limits bounds) {
  state_->apply([&] {
    if (state_->stack.empty() || !state_->stack.back().pending || !state_->stack.back().attributes.empty()) {
      throw std::invalid_argument{"XML identity scope must start before owner attributes or content"};
    }
    if (state_->stack.back().start.qualified_name != scope.owner ||
        (!state_->identities.empty() && state_->identities.back().owner_depth == state_->stack.size())) {
      throw std::invalid_argument{"XML identity scope owner is mismatched or duplicated"};
    }
    std::map<std::string, std::string> inherited{};
    for (std::size_t index{}; index + 1U < state_->stack.size(); ++index) {
      for (const auto& [prefix, uri] : state_->stack[index].start.namespaces) {
        if (state_->operation) {
          state_->operation->consume(operation_resource::allocation_bytes, prefix.size() + uri.size());
          state_->operation->consume(operation_resource::work_units, prefix.size() + uri.size());
        }
        inherited[prefix] = uri;
      }
    }
    auto observer = std::make_unique<identity_validator>(scope, bounds, state_->operation, inherited);
    observer->accept({event_kind::start_document});
    state_->identities.push_back({state_->stack.size(), std::move(observer)});
  });
}
// Emit one checked decoded-text token.
void token_writer::text(std::string_view value) {
  state_->apply([&] {
    consume(state_->text_bytes, value.size(), state_->bounds.max_text_bytes);
    state_->content(); state_->escaped(value, false);
    const event_view semantic{event_kind::text, {}, {}, {}, value};
    for (auto& pass : state_->identities) { pass.validator->accept(semantic); }
  });
}
// Finish one element without retaining its descendants.
void token_writer::close() {
  state_->apply([&] {
    if (state_->stack.empty()) { throw std::invalid_argument{"Unbalanced XML end element"}; }
    const auto& frame = state_->stack.back();
    if (frame.pending) {
      observe_token_identities(state_->identities, frame.start, state_->operation);
      state_->append("/>");
    }
    else { state_->append("</"); state_->append(frame.rendered_name); state_->append(">"); }
    const event_view semantic{event_kind::end_element,
        {frame.start.qualified_name.local, frame.start.qualified_name.namespace_uri}};
    for (auto& pass : state_->identities) { pass.validator->accept(semantic); }
    finish_token_identities(state_->identities, state_->stack.size());
    state_->stack.pop_back();
  });
}
// Move bytes only after the complete output contract succeeds.
std::string token_writer::finish() {
  state_->apply([&] {
    if (!state_->root_seen || !state_->stack.empty() || !state_->identities.empty()) { throw std::invalid_argument{"Incomplete XML token output"}; }
    state_->completed = true;
  });
  return std::move(state_->bytes);
}
// Reuse the caller's cumulative operation budget.
operation_context* token_writer::context() const noexcept { return state_->operation; }
// Expose the immutable limits to generated container adapters.
const limits& token_writer::bounds() const noexcept { return state_->bounds; }

struct token_reader::implementation {
  std::string_view remaining;
  operation_context* operation;
  std::size_t chunk_bytes;
  limits bounds;
  std::deque<token> queue{};
  std::vector<std::map<std::string, std::string>> namespace_stack{};
  std::vector<std::size_t> child_counts{};
  std::vector<token_identity_pass> identities{};
  event_reader producer;
  bool failed{}, completed{};

  // Copy only events produced by one bounded input chunk; no parent/child links are retained.
  implementation(std::string_view input, limits selected_bounds, operation_context* context, std::size_t chunk)
      : remaining{input}, operation{context}, chunk_bytes{std::min(chunk, default_token_chunk_bytes)}, bounds{selected_bounds},
        producer{[this](const event_view& event) {
          if (operation) {
            operation->consume(operation_resource::allocation_bytes,
                sizeof(token) + event.qualified_name.local.size() + event.qualified_name.namespace_uri.size() + event.text.size());
          }
          token value{event.kind, {std::string{event.qualified_name.local},
              std::string{event.qualified_name.namespace_uri}}, {}, std::string{event.text}};
          for (const auto& attribute : event.attributes) {
            if (operation) {
              operation->consume(operation_resource::allocation_bytes,
                  sizeof(xml::attribute) + attribute.qualified_name.local.size() +
                  attribute.qualified_name.namespace_uri.size() + attribute.value.size());
            }
            value.attributes.push_back({{std::string{attribute.qualified_name.local},
                std::string{attribute.qualified_name.namespace_uri}}, std::string{attribute.value}});
          }
          for (const auto& declaration : event.namespaces) {
            if (operation) {
              operation->consume(operation_resource::allocation_bytes,
                  declaration.prefix.size() + declaration.namespace_uri.size());
            }
            value.namespaces.emplace(declaration.prefix, declaration.namespace_uri);
          }
          queue.push_back(std::move(value));
        }, event_limits{selected_bounds}, context} {
    if (!chunk_bytes) { throw std::invalid_argument{"XML token input chunk must be positive"}; }
    if (input.size() > selected_bounds.max_input_bytes) { throw std::length_error{"XML token input limit exceeded"}; }
    fill();
    if (queue.empty() || queue.front().kind != event_kind::start_document) {
      throw std::invalid_argument{"Missing XML start document"};
    }
    queue.pop_front(); fill();
  }
  // Produce another bounded group only after the previous group has been consumed.
  void fill() {
    while (queue.empty() && !producer.complete()) {
      if (remaining.empty()) { producer.finish(); }
      else {
        const auto count = std::min(chunk_bytes, remaining.size());
        producer.feed(remaining.substr(0U, count));
        remaining.remove_prefix(count);
      }
    }
  }
};

// Borrow the exact byte message and enter the normalized token stream.
token_reader::token_reader(std::string_view input, limits bounds,
    operation_context* context, std::size_t chunk_bytes)
    : state_{std::make_unique<implementation>(input, bounds, context, chunk_bytes)} {}
// Destroy provider ownership after all queued token copies.
token_reader::~token_reader() = default;
// Expose the current bounded token only while this pass is valid.
const token& token_reader::current() const {
  if (state_->failed || state_->completed || state_->queue.empty()) {
    throw std::logic_error{"XML token reader is closed or failed"};
  }
  return state_->queue.front();
}
// Advance while preserving any provider or observer failure as a terminal fence.
void token_reader::advance() {
  (void)current();
  try {
    auto& value = state_->queue.front();
    observe_token_identities(state_->identities, value, state_->operation);
    if (value.kind == event_kind::start_element) {
      if (!state_->child_counts.empty()) { ++state_->child_counts.back(); }
      if (state_->operation) { state_->operation->consume(operation_resource::allocation_bytes, sizeof(value.namespaces) + sizeof(std::size_t)); }
      state_->namespace_stack.push_back(std::move(value.namespaces));
      state_->child_counts.push_back(0U);
    } else if (value.kind == event_kind::end_element) {
      finish_token_identities(state_->identities, state_->namespace_stack.size());
      if (state_->namespace_stack.empty()) { throw std::invalid_argument{"Unbalanced XML token namespace scope"}; }
      state_->namespace_stack.pop_back();
      state_->child_counts.pop_back();
    }
    state_->queue.pop_front(); state_->fill();
  }
  catch (...) { state_->failed = true; throw; }
}
// Ignore only XML whitespace outside scalar slots.
void token_reader::skip_space() {
  while (current().kind == event_kind::text && detail::trim(current().text).empty()) { advance(); }
}
// Keep removed foreign attributes visible to identity observers until the owner start is consumed.
void token_reader::consume_attribute(const name& selected) {
  try {
    (void)current();
    auto& value = state_->queue.front();
    if (value.kind != event_kind::start_element) { throw std::invalid_argument{"XML attributes require a start token"}; }
    const auto found = std::find_if(value.attributes.begin(), value.attributes.end(),
        [&](const auto& item) { return item.qualified_name == selected; });
    if (found == value.attributes.end()) { throw std::invalid_argument{"XML attribute is absent"}; }
    if (state_->operation) { state_->operation->consume(operation_resource::allocation_bytes, sizeof(attribute)); }
    value.consumed_attributes.push_back(std::move(*found));
    value.attributes.erase(found);
  } catch (...) { state_->failed = true; throw; }
}
// Copy only namespace context, admitting each owning map insertion before allocation.
std::map<std::string, std::string> token_reader::namespace_scope(build_budget* budget) const {
  try {
    (void)current();
    std::map<std::string, std::string> result;
    const auto adopt = [&](const auto& bindings) {
      for (const auto& [prefix, uri] : bindings) {
        if (budget) { budget->namespace_binding(prefix, uri); }
        else if (state_->operation) {
          state_->operation->consume(operation_resource::allocation_bytes,
              sizeof(std::pair<const std::string, std::string>) + prefix.size() + uri.size());
        }
        result[prefix] = uri;
      }
    };
    for (const auto& bindings : state_->namespace_stack) { adopt(bindings); }
    if (current().kind == event_kind::start_element) { adopt(current().namespaces); }
    return result;
  } catch (...) { state_->failed = true; throw; }
}
// Foreign preservation records positions among known immediate children, independent of nested events.
std::size_t token_reader::child_position() const {
  (void)current();
  if (state_->child_counts.empty()) { throw std::logic_error{"XML child position requires an open owner"}; }
  return state_->child_counts.back();
}
// Recognize xsi:nil by expanded identity, allowing equivalent user-selected prefixes.
bool token_reader::take_nil(bool allowed) {
  try {
    (void)current();
    auto& attributes = state_->queue.front().attributes;
    for (auto selected = attributes.begin(); selected != attributes.end(); ++selected) {
      if (selected->qualified_name != name{"nil", "http://www.w3.org/2001/XMLSchema-instance"}) { continue; }
      if (!allowed) { throw std::invalid_argument{"XML xsi:nil is not allowed by the presence mapping"}; }
      bool value{};
      detail::read_scalar(selected->value, value);
      state_->queue.front().consumed_nil.emplace(std::move(*selected));
      attributes.erase(selected);
      return value;
    }
    return false;
  } catch (...) { state_->failed = true; throw; }
}
// Preserve consumed control metadata for identity observers while hiding it from payload fields.
std::optional<name> token_reader::take_type() {
  try {
    (void)current();
    auto& value = state_->queue.front();
    for (auto selected = value.attributes.begin(); selected != value.attributes.end(); ++selected) {
      if (selected->qualified_name != name{"type", "http://www.w3.org/2001/XMLSchema-instance"}) { continue; }
      element scope{};
      const auto adopt = [&](const auto& declarations) {
        for (const auto& [prefix, uri] : declarations) {
          if (state_->operation) {
            state_->operation->consume(operation_resource::allocation_bytes, prefix.size() + uri.size());
            state_->operation->consume(operation_resource::work_units, prefix.size() + uri.size());
          }
          scope.namespace_bindings[prefix] = uri;
        }
      };
      for (const auto& declarations : state_->namespace_stack) { adopt(declarations); }
      adopt(value.namespaces);
      if (state_->operation) {
        state_->operation->consume(operation_resource::work_units, selected->value.size());
        state_->operation->consume(operation_resource::allocation_bytes, selected->value.size());
        const auto lexical = detail::trim(selected->value);
        const auto colon = lexical.find(':');
        const auto prefix = colon == std::string_view::npos ? std::string_view{} : lexical.substr(0U, colon);
        state_->operation->consume(operation_resource::allocation_bytes, prefix.size());
        if (prefix == "xml") {
          state_->operation->consume(operation_resource::allocation_bytes, std::string_view{"http://www.w3.org/XML/1998/namespace"}.size());
        } else if (const auto binding = scope.namespace_bindings.find(std::string{prefix}); binding != scope.namespace_bindings.end()) {
          state_->operation->consume(operation_resource::allocation_bytes, binding->second.size());
        }
      }
      auto result = resolve_qname(selected->value, scope);
      value.consumed_type.emplace(std::move(*selected));
      value.attributes.erase(selected);
      return result;
    }
    return {};
  } catch (...) { state_->failed = true; throw; }
}
// Register before consuming this owner's start token, independently of producer read-ahead.
void token_reader::begin_identity(const identity_scope& scope) {
  begin_identity(scope, state_->operation && state_->operation->identity_bounds ?
      *state_->operation->identity_bounds : identity_limits{});
}
// Explicit local bounds take precedence over a caller's default identity-index policy.
void token_reader::begin_identity(const identity_scope& scope, identity_limits bounds) {
  try {
    if (current().kind != event_kind::start_element || current().qualified_name != scope.owner ||
        (!state_->identities.empty() && state_->identities.back().owner_depth == state_->namespace_stack.size() + 1U)) {
      throw std::invalid_argument{"XML identity scope requires one matching current owner start token"};
    }
    std::map<std::string, std::string> inherited{};
    for (const auto& declarations : state_->namespace_stack) {
      for (const auto& [prefix, uri] : declarations) {
        if (state_->operation) {
          state_->operation->consume(operation_resource::allocation_bytes, prefix.size() + uri.size());
          state_->operation->consume(operation_resource::work_units, prefix.size() + uri.size());
        }
        inherited[prefix] = uri;
      }
    }
    auto observer = std::make_unique<identity_validator>(scope, bounds, state_->operation, inherited);
    observer->accept({event_kind::start_document});
    state_->identities.push_back({state_->namespace_stack.size() + 1U, std::move(observer)});
  } catch (...) { state_->failed = true; throw; }
}
// Require the producer's checked byte EOF before authorizing candidate publication.
void token_reader::finish() {
  try {
    skip_space();
    if (current().kind != event_kind::end_document || !state_->producer.complete()) {
      throw std::invalid_argument{"Trailing XML tokens"};
    }
    state_->completed = true;
  } catch (...) { state_->failed = true; throw; }
}
// Share all lexical/model admissions with the same caller context.
operation_context* token_reader::context() const noexcept { return state_->operation; }
// Expose immutable limits without creating a separate nested map budget.
const limits& token_reader::bounds() const noexcept { return state_->bounds; }
// Retain just the current record's attributes across recursive child traversal.
namespace {
// Select operation-local extension bounds independently of the absent known-document tree budget.
extension_limits token_extension_bounds(operation_context* context) {
  return context && context->extension_bounds ? *context->extension_bounds : extension_limits{};
}
// Borrow only explicitly understood vocabularies; opaque preservation does not grant capabilities.
std::span<const std::string> token_capabilities(operation_context* context) {
  return context && context->understood_namespaces ? std::span<const std::string>{*context->understood_namespaces} : std::span<const std::string>{};
}
// Match exact position/URI slots and reject unknown core names even when a wildcard is permissive.
std::size_t token_extension_slot(const extension_contract& contract, const name& selected,
    bool attribute, std::size_t field, operation_context* context) {
  if (selected.namespace_uri.empty() || selected.namespace_uri == contract.core_namespace ||
      selected.namespace_uri == model_metadata_namespace ||
      std::find(contract.known_names.begin(), contract.known_names.end(), selected) != contract.known_names.end()) {
    throw std::invalid_argument{"Known, core or reserved XML names cannot enter an extension slot"};
  }
  for (std::size_t index{}; index < contract.slots.size(); ++index) {
    const auto& slot = contract.slots[index];
    if (context) { context->consume(operation_resource::work_units, 1U); }
    if (slot.attribute != attribute || (!attribute && slot.before_field != field)) { continue; }
    for (const auto& uri : slot.namespaces) {
      if (context) { context->consume(operation_resource::work_units, std::min(uri.size(), selected.namespace_uri.size())); }
      if (uri == selected.namespace_uri) {
        if (slot.policy == unknown_policy::reject) { throw std::invalid_argument{"XML extension slot rejects this vocabulary"}; }
        return index;
      }
    }
  }
  throw std::invalid_argument{"XML extension has no permitted position and vocabulary"};
}
// Emit already validated opaque semantic XML without creating a second element tree.
void write_opaque_tokens(token_writer& writer, const element& node) {
  writer.open(node.qualified_name, node.namespace_bindings);
  for (const auto& item : node.attributes) { writer.attribute_value(item.qualified_name, item.value); }
  if (node.text_segments.empty()) {
    writer.text(node.text);
    for (const auto& child : node.children) { write_opaque_tokens(writer, child); }
  } else {
    std::size_t next{};
    for (std::size_t index{}; index <= node.children.size(); ++index) {
      while (next < node.text_segments.size() && node.text_segments[next].before_child == index) { writer.text(node.text_segments[next++].value); }
      if (index < node.children.size()) { write_opaque_tokens(writer, node.children[index]); }
    }
  }
  writer.close();
}
} // namespace

struct token_extension_input::implementation {
  token_reader& reader;
  const extension_contract& contract;
  extension_slot& target;
  extension_limits bounds;
  build_budget budget;
  extension_store candidate{};
  std::vector<std::size_t> counts{};
  std::size_t items{}, unknown_children{}, preserved_bytes{};
  bool failed{}, complete{}, scope_retained{};

  // Admit opaque storage before copies; cumulative operation counters are never refunded.
  void preserved(std::size_t amount) {
    consume(preserved_bytes, amount, bounds.max_preserved_bytes);
    if (reader.context()) { reader.context()->consume(operation_resource::preserved_bytes, amount); }
  }
  // Account retained complete namespace context after its map insertions were individually admitted.
  void preserved_scope(const std::map<std::string, std::string>& scope) {
    for (const auto& [prefix, uri] : scope) { preserved(sizeof(std::pair<const std::string, std::string>) + prefix.size() + uri.size()); }
  }
  // Charge one exact slot occurrence before either skip or retention consumes any tokens.
  unknown_policy admit(const name& selected, bool attribute, std::size_t field) {
    consume(items, 1U, bounds.max_items);
    const auto index = token_extension_slot(contract, selected, attribute, field, reader.context());
    consume(counts[index], 1U, contract.slots[index].maximum.value_or(std::numeric_limits<std::size_t>::max()));
    return contract.slots[index].policy;
  }
  // Capture only a selected foreign subtree; known fields never enter owning XML storage.
  element capture() {
    const auto& start = reader.current();
    const auto depth = budget.enter(start.qualified_name);
    preserved(sizeof(element) + start.qualified_name.local.size() + start.qualified_name.namespace_uri.size());
    element result{};
    result.qualified_name = start.qualified_name;
    result.namespace_bindings = reader.namespace_scope(&budget);
    preserved_scope(result.namespace_bindings);
    for (const auto& item : start.attributes) {
      budget.attribute_name(item.qualified_name); budget.text(item.value.size());
      preserved(sizeof(attribute) + item.qualified_name.local.size() + item.qualified_name.namespace_uri.size() + item.value.size());
      result.attributes.push_back(item);
    }
    reader.advance();
    while (reader.current().kind != event_kind::end_element) {
      if (reader.current().kind == event_kind::start_element) { result.children.push_back(capture()); }
      else if (reader.current().kind == event_kind::text) {
        const auto& text = reader.current().text;
        budget.text(text.size()); budget.retained_storage(text.size());
        preserved(text.size()); preserved(text.size());
        result.text += text;
        if (result.text_segments.empty() || result.text_segments.back().before_child != result.children.size()) {
          budget.retained_storage(sizeof(element::text_segment)); preserved(sizeof(element::text_segment));
          result.text_segments.push_back({result.children.size(), text});
        } else { result.text_segments.back().value += text; }
        reader.advance();
      } else { throw std::invalid_argument{"Incomplete opaque XML subtree"}; }
    }
    reader.advance();
    return result;
  }
  // Discard an allowed foreign subtree with only an integer depth counter.
  void skip() {
    std::size_t depth{};
    do {
      if (reader.current().kind == event_kind::start_element) { ++depth; }
      else if (reader.current().kind == event_kind::end_element) { --depth; }
      else if (reader.current().kind == event_kind::end_document) { throw std::invalid_argument{"Incomplete skipped XML subtree"}; }
      reader.advance();
    } while (depth);
  }
  // Bind immutable metadata and retain only permitted foreign owner attributes.
  implementation(token_reader& source, const extension_contract& selected, extension_slot& output)
      : reader{source}, contract{selected}, target{output}, bounds{token_extension_bounds(source.context())},
        budget{[&] { auto document = bounds.document; document.max_tree_bytes = std::min(document.max_tree_bytes, bounds.max_preserved_bytes); return document; }(), source.context()} {
    validate_extension_contract(contract, bounds);
    if (source.current().kind != event_kind::start_element || source.current().qualified_name.namespace_uri != contract.core_namespace) {
      throw std::invalid_argument{"XML extension owner differs from its contract"};
    }
    element scope{}; scope.qualified_name = source.current().qualified_name;
    validate_required_capabilities(contract, scope, {}, token_capabilities(source.context()), bounds, source.context());
    if (source.context()) { source.context()->consume(operation_resource::allocation_bytes, contract.slots.size() * sizeof(std::size_t)); }
    counts.resize(contract.slots.size());
    target.reset();
    std::size_t index{};
    while (index < reader.current().attributes.size()) {
      const auto& item = reader.current().attributes[index];
      if (std::find(contract.known_names.begin(), contract.known_names.end(), item.qualified_name) != contract.known_names.end() ||
          item.qualified_name.namespace_uri == model_metadata_namespace) { ++index; continue; }
      if (admit(item.qualified_name, true, 0U) == unknown_policy::preserve) {
        budget.attribute_name(item.qualified_name); budget.text(item.value.size());
        preserved(sizeof(attribute) + item.qualified_name.local.size() + item.qualified_name.namespace_uri.size() + item.value.size());
        if (!scope_retained) {
          candidate.namespace_bindings = reader.namespace_scope(&budget);
          preserved_scope(candidate.namespace_bindings); scope_retained = true;
        }
        candidate.attributes.push_back(item);
      }
      reader.consume_attribute(item.qualified_name);
    }
  }
};

// Consume authorized foreign attributes before the ordinary field reader takes the owner start.
token_extension_input::token_extension_input(token_reader& reader, const extension_contract& contract, extension_slot& target)
    : state_{std::make_unique<implementation>(reader, contract, target)} {}
// Release an unpublished preservation candidate without altering caller-visible owning models.
token_extension_input::~token_extension_input() = default;
// Keep the known decoder on the token stream while visiting exact foreign-only boundaries.
void token_extension_input::before_field(std::size_t index) {
  if (state_->failed || state_->complete) { throw std::logic_error{"XML extension input is closed or failed"}; }
  try {
    auto& state = *state_;
    state.reader.skip_space();
    while (state.reader.current().kind == event_kind::start_element &&
        std::find(state.contract.known_names.begin(), state.contract.known_names.end(), state.reader.current().qualified_name) == state.contract.known_names.end()) {
      const auto position = state.reader.child_position() - state.unknown_children;
      if (state.admit(state.reader.current().qualified_name, false, index) == unknown_policy::preserve) {
        constexpr auto metadata_bytes = sizeof(extension_store::child_slot) - sizeof(element);
        state.budget.retained_storage(metadata_bytes); state.preserved(metadata_bytes);
        state.candidate.children.push_back({position, state.capture(), index});
      } else { state.skip(); }
      ++state.unknown_children;
      state.reader.skip_space();
    }
  } catch (...) { state_->failed = true; throw; }
}
// Publish no opaque state until every slot's minimum and final boundary have succeeded.
void token_extension_input::finish(std::size_t end_index) {
  before_field(end_index);
  try {
    for (std::size_t index{}; index < state_->counts.size(); ++index) {
      if (state_->counts[index] < state_->contract.slots[index].minimum) { throw std::invalid_argument{"Required XML extension occurrence is missing"}; }
    }
    if (!state_->candidate.attributes.empty() || !state_->candidate.children.empty()) { state_->target.value(state_->reader.context()) = std::move(state_->candidate); }
    state_->complete = true;
  } catch (...) { state_->failed = true; throw; }
}
// Validate existing opaque storage without cloning its payloads, then emit only its attributes.
token_extension_output::token_extension_output(token_writer& writer, const extension_contract& contract, const extension_store& retained)
    : writer_{writer}, retained_{retained} {
  const auto bounds = token_extension_bounds(writer.context());
  if (retained.attributes.size() > bounds.max_items || retained.children.size() > bounds.max_items - retained.attributes.size()) {
    throw std::length_error{"XML extension item limit exceeded"};
  }
  if (!retained.wrappers.empty()) { throw std::invalid_argument{"Positioned XML extension storage has no nested wrapper slots"}; }
  if (writer.context()) { writer.context()->consume(operation_resource::allocation_bytes, (retained.attributes.size() + retained.children.size()) * sizeof(extension_item_view)); }
  std::vector<extension_item_view> items;
  for (const auto& item : retained.attributes) {
    const auto index = token_extension_slot(contract, item.qualified_name, true, 0U, writer.context());
    if (contract.slots[index].policy != unknown_policy::preserve) { throw std::invalid_argument{"Only preserved XML slots can be emitted"}; }
    items.push_back({&item, nullptr, 0U, 0U});
  }
  for (const auto& item : retained.children) {
    const auto index = token_extension_slot(contract, item.value.qualified_name, false, item.before_field, writer.context());
    if (contract.slots[index].policy != unknown_policy::preserve) { throw std::invalid_argument{"Only preserved XML slots can be emitted"}; }
    items.push_back({nullptr, &item.value, item.before_field, item.before_child});
  }
  element scope{}; scope.qualified_name.namespace_uri = contract.core_namespace;
  std::size_t scope_bytes{};
  if (retained.namespace_bindings.size() > bounds.document.max_namespace_bindings) { throw std::length_error{"XML retained namespace limit exceeded"}; }
  for (const auto& [prefix, uri] : retained.namespace_bindings) {
    consume(scope_bytes, sizeof(std::pair<const std::string, std::string>), bounds.max_preserved_bytes);
    consume(scope_bytes, prefix.size(), bounds.max_preserved_bytes);
    consume(scope_bytes, uri.size(), bounds.max_preserved_bytes);
  }
  if (writer.context()) { writer.context()->consume(operation_resource::allocation_bytes, scope_bytes); }
  scope.namespace_bindings = retained.namespace_bindings;
  validate_extension_slots(contract, scope, items, token_capabilities(writer.context()), bounds, writer.context());
  for (const auto& item : retained.attributes) { writer.attribute_value(item.qualified_name, item.value); }
}
// Replay each already validated foreign child at its exact static field boundary.
void token_extension_output::before_field(std::size_t index) {
  while (next_child_ < retained_.children.size() && retained_.children[next_child_].before_field == index) {
    write_opaque_tokens(writer_, retained_.children[next_child_++].value);
  }
}
// Reject stale positions instead of silently omitting retained extension payloads.
void token_extension_output::finish(std::size_t end_index) {
  before_field(end_index);
  if (next_child_ != retained_.children.size()) { throw std::invalid_argument{"Retained XML extension position no longer exists"}; }
}

// Enter one statically mapped owner while retaining only its authorized ordinary attributes.
token_fields::token_fields(token_reader& reader, const element* lexical_scope) : reader_{reader}, lexical_scope_{lexical_scope} {
  if (reader_.current().kind != event_kind::start_element) {
    throw std::invalid_argument{"XML record requires a start element"};
  }
  if (reader_.context()) {
    reader_.context()->consume(operation_resource::allocation_bytes,
        reader_.current().attributes.size() * sizeof(attribute));
    for (const auto& attribute : reader_.current().attributes) {
      reader_.context()->consume(operation_resource::allocation_bytes,
          attribute.qualified_name.local.size() + attribute.qualified_name.namespace_uri.size() + attribute.value.size());
    }
  }
  attributes_ = reader_.current().attributes;
  used_.resize(attributes_.size());
  reader_.advance();
}
// Reject unknown membership before consuming the record end token.
void token_fields::finish() {
  if (std::find(used_.begin(), used_.end(), false) != used_.end()) {
    throw std::invalid_argument{"Unknown XML attribute"};
  }
  reader_.skip_space();
  if (reader_.current().kind != event_kind::end_element) {
    throw std::invalid_argument{"Unknown or out-of-order XML child or text"};
  }
  reader_.advance();
}
// Retain only field metadata and the owning model while accepting each all-child permutation.
void token_fields::finish_all(bool required_group) {
  bool present{};
  reader_.skip_space();
  while (reader_.current().kind == event_kind::start_element) {
    const auto selected = std::find_if(all_fields_.begin(), all_fields_.end(), [&](const auto& field) {
      return field.selected == reader_.current().qualified_name;
    });
    if (selected == all_fields_.end()) { break; }
    if (selected->seen) { throw std::invalid_argument{"Duplicate XML all child"}; }
    selected->seen = true;
    present = true;
    selected->decode();
    reader_.skip_space();
  }
  if (present || required_group) {
    for (const auto& field : all_fields_) {
      if (field.required && !field.seen) { throw std::invalid_argument{"Missing required XML all child"}; }
    }
  }
}
} // namespace rohit::serializer::xml
