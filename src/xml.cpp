// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/xml.hpp>

#include <algorithm>
#include <climits>
#include <cstdint>
#include <exception>
#include <map>
#include <set>
#include <string_view>

#ifndef SERIALIZER_HAS_XML
#define SERIALIZER_HAS_XML 0
#endif
#if SERIALIZER_HAS_XML
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
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
    if (!node.children.empty() && !detail::trim(node.text).empty() && node.text_segments.empty()) {
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
  if (kind == "string" || kind == "normalizedString" || kind == "token" || kind == "anyURI") {
    return first == second;
  }
  validate_lexical_value(first, kind, scope); validate_lexical_value(second, kind, scope);
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
  check_encoding(document);
  std::optional<checked_input> input{};
  if (context) { input.emplace(checked_input{document, *context, {}}); }
  const auto reader = std::unique_ptr<xmlTextReader, decltype(&xmlFreeTextReader)>{
      context ? xmlReaderForIO(&checked_input::read, nullptr, &*input, nullptr, "UTF-8",
                         XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING) :
                xmlReaderForMemory(document.data(), static_cast<int>(document.size()), nullptr, "UTF-8",
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

struct prepared_schema::implementation {
#if SERIALIZER_HAS_XML
  std::unique_ptr<xmlSchema, decltype(&xmlSchemaFree)> schema{nullptr, &xmlSchemaFree};
#endif
};

prepared_schema::prepared_schema(std::string_view xsd, limits bounds) {
#if SERIALIZER_HAS_XML
  const auto tree = parse(xsd, bounds);
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
      xmlReadMemory(document.data(), static_cast<int>(document.size()), nullptr, "UTF-8",
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
} // namespace rohit::serializer::xml
