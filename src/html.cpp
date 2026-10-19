// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/html.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::serializer::html {
namespace {
constexpr std::array<std::string_view, 28> tags{
    "div", "p",       "span",    "b",  "i",  "strong", "em",         "a",  "ul", "ol",
    "li",  "section", "article", "h1", "h2", "h3",     "h4",         "h5", "h6", "br",
    "hr",  "img",     "small",   "s",  "u",  "code",   "blockquote", "nav"};

// Recognize the HTML ASCII whitespace set, independent of the host locale.
bool space(char value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f';
}
// Restrict syntax names to ASCII letters/digits/hyphen; Unicode remains ordinary text content.
bool name_character(char value) {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') || value == '-';
}
// HTML tag/attribute matching folds ASCII case, without applying XML name rules.
std::string lower(std::string_view value) {
  std::string result{value};
  for (auto& character : result) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character + ('a' - 'A'));
    }
  }
  return result;
}
// The approved subset contains exactly these HTML void elements.
bool is_void(std::string_view name) {
  return name == "br" || name == "hr" || name == "img";
}
// Phrasing-only containers avoid browser implied paragraph/formatting element reconstruction.
bool phrasing(std::string_view name) {
  return name == "span" || name == "b" || name == "i" || name == "strong" || name == "em" ||
         name == "a" || name == "br" || name == "img" || name == "small" || name == "s" ||
         name == "u" || name == "code";
}
// A closed subset rejects content that an independent HTML parser would implicitly rearrange.
void validate_child(const std::vector<std::string>& open, std::string_view child) {
  if (std::find(tags.begin(), tags.end(), child) == tags.end()) {
    throw std::invalid_argument{"HTML tag is outside the approved fragment subset"};
  }
  if (child == "li" && (open.empty() || (open.back() != "ul" && open.back() != "ol"))) {
    throw std::invalid_argument{"HTML list items require a direct list parent"};
  }
  if (!open.empty()) {
    const auto& parent = open.back();
    if (is_void(parent) || ((parent == "ul" || parent == "ol") && child != "li") ||
        ((phrasing(parent) || parent == "p" || (parent.size() == 2U && parent.front() == 'h')) &&
         !phrasing(child))) {
      throw std::invalid_argument{"HTML child would require unsupported browser recovery"};
    }
  }
  if (child == "a" && std::find(open.begin(), open.end(), "a") != open.end()) {
    throw std::invalid_argument{"Nested HTML anchors are outside the approved subset"};
  }
}
// Only a small explicit attribute vocabulary is accepted; this is not a general HTML sanitizer.
bool attribute_allowed(std::string_view tag, std::string_view name) {
  return name == "id" || name == "class" || name == "title" || name == "lang" || name == "dir" ||
         name == "hidden" || (tag == "a" && (name == "href" || name == "rel")) ||
         (tag == "img" && (name == "src" || name == "alt" || name == "width" || name == "height"));
}
// Check a Unicode scalar before UTF-8 emission; C1 references reject rather than use HTML remapping.
bool scalar_allowed(std::uint32_t value) {
  return value != 0 && value <= 0x10ffffU && !(value >= 0xd800U && value <= 0xdfffU) &&
         !(value < 0x20U && value != '\t' && value != '\n' && value != '\r' && value != '\f') &&
         !(value >= 0x7fU && value <= 0x9fU);
}
// Append one checked scalar using canonical UTF-8.
void append_scalar(std::string& output, std::uint32_t value) {
  if (!scalar_allowed(value)) {
    throw std::invalid_argument{"Unsupported HTML character reference"};
  }
  if (value <= 0x7fU) {
    output += static_cast<char>(value);
  } else if (value <= 0x7ffU) {
    output += static_cast<char>(0xc0U | (value >> 6U));
    output += static_cast<char>(0x80U | (value & 0x3fU));
  } else if (value <= 0xffffU) {
    output += static_cast<char>(0xe0U | (value >> 12U));
    output += static_cast<char>(0x80U | ((value >> 6U) & 0x3fU));
    output += static_cast<char>(0x80U | (value & 0x3fU));
  } else {
    output += static_cast<char>(0xf0U | (value >> 18U));
    output += static_cast<char>(0x80U | ((value >> 12U) & 0x3fU));
    output += static_cast<char>(0x80U | ((value >> 6U) & 0x3fU));
    output += static_cast<char>(0x80U | (value & 0x3fU));
  }
}
// Reject malformed/noncanonical UTF-8 before strings enter the semantic event inventory.
void validate_utf8(std::string_view input) {
  for (std::size_t index = 0; index < input.size();) {
    const auto first = static_cast<unsigned char>(input[index++]);
    if (first < 0x80U) {
      if (!scalar_allowed(first)) {
        throw std::invalid_argument{"Unsupported HTML control character"};
      }
      continue;
    }
    const auto count = first >= 0xc2U && first <= 0xdfU   ? 1U
                       : first >= 0xe0U && first <= 0xefU ? 2U
                       : first >= 0xf0U && first <= 0xf4U ? 3U
                                                          : 4U;
    if (count == 4U || count > input.size() - index) {
      throw std::invalid_argument{"Malformed HTML UTF-8"};
    }
    std::uint32_t value = first & (0x7fU >> count);
    for (unsigned part = 0; part < count; ++part) {
      const auto byte = static_cast<unsigned char>(input[index++]);
      if ((byte & 0xc0U) != 0x80U) {
        throw std::invalid_argument{"Malformed HTML UTF-8 continuation"};
      }
      value = (value << 6U) | (byte & 0x3fU);
    }
    const std::uint32_t minimum = count == 1U ? 0x80U : count == 2U ? 0x800U : 0x10000U;
    if (value < minimum || !scalar_allowed(value)) {
      throw std::invalid_argument{"Invalid HTML Unicode scalar"};
    }
  }
}
// Decode the approved terminated entity subset and normalize HTML source line endings.
std::string decode(std::string_view value) {
  std::string result{};
  result.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '\r') {
      result += '\n';
      if (index + 1U < value.size() && value[index + 1U] == '\n') {
        ++index;
      }
    } else if (value[index] != '&') {
      result += value[index];
    } else {
      const auto end = value.find(';', index + 1U);
      if (end == std::string_view::npos || end - index > 12U) {
        throw std::invalid_argument{
            "HTML entities require a supported semicolon-terminated spelling"};
      }
      auto entity = value.substr(index + 1U, end - index - 1U);
      if (entity == "amp") {
        result += '&';
      } else if (entity == "lt") {
        result += '<';
      } else if (entity == "gt") {
        result += '>';
      } else if (entity == "quot") {
        result += '"';
      } else if (entity == "apos") {
        result += '\'';
      } else if (entity == "nbsp") {
        append_scalar(result, 0xa0U);
      } else if (entity.starts_with('#')) {
        entity.remove_prefix(1);
        int base = 10;
        if (entity.starts_with('x') || entity.starts_with('X')) {
          base = 16;
          entity.remove_prefix(1);
        }
        std::uint32_t scalar{};
        const auto parsed =
            std::from_chars(entity.data(), entity.data() + entity.size(), scalar, base);
        if (entity.empty() || parsed.ec != std::errc{} ||
            parsed.ptr != entity.data() + entity.size()) {
          throw std::invalid_argument{"Malformed HTML numeric reference"};
        }
        append_scalar(result, scalar);
      } else {
        throw std::invalid_argument{"HTML entity is outside the approved subset"};
      }
      index = end;
    }
  }
  return result;
}
// Charge before retaining logical strings/events; provider/container excess capacity is excluded.
void consume(xml::operation_context* context, xml::operation_resource resource, std::size_t count) {
  if (context) {
    context->consume(resource, count);
  }
}
// Append with overflow-safe output bounds and caller-owned shared allocation accounting.
void append(std::string& output, std::string_view value, const limits& bounds,
            xml::operation_context* context) {
  if (value.size() > bounds.max_output_bytes - output.size()) {
    throw std::length_error{"HTML output byte limit exceeded"};
  }
  consume(context, xml::operation_resource::allocation_bytes, value.size());
  output += value;
}
// Escape semantic text/attributes while keeping the output in ordinary HTML syntax.
void escape(std::string& output, std::string_view value, bool attribute_value, const limits& bounds,
            xml::operation_context* context) {
  // Escaping never shrinks UTF-8 bytes, so impossible output must fail before scanning the value.
  if (value.size() > bounds.max_output_bytes - output.size()) {
    throw std::length_error{"HTML output byte limit exceeded"};
  }
  validate_utf8(value);
  for (const auto character : value) {
    if (character == '&') {
      append(output, "&amp;", bounds, context);
    } else if (character == '<') {
      append(output, "&lt;", bounds, context);
    } else if (character == '\r') {
      append(output, "&#13;", bounds, context);
    } else if (attribute_value && character == '"') {
      append(output, "&quot;", bounds, context);
    } else {
      append(output, std::string_view{&character, 1U}, bounds, context);
    }
  }
}
} // namespace

std::string_view supported_subset() noexcept {
  return "UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void "
         "tags; "
         "case-insensitive ASCII names; quoted/unquoted and hidden boolean attributes; "
         "amp/lt/gt/quot/apos/nbsp "
         "and valid numeric references; comments omitted; no doctype, implied closes, raw text, "
         "foreign content or browser recovery";
}

std::string write_fragment(std::span<const event> events, const limits& bounds,
                           xml::operation_context* context) {
  if (events.size() > bounds.max_events || events.size() < 2U ||
      events.front().kind != event_kind::start_fragment ||
      events.back().kind != event_kind::end_fragment) {
    throw std::invalid_argument{"HTML event trace requires bounded explicit fragment boundaries"};
  }
  std::string output{};
  std::vector<std::string> open{};
  std::size_t text_bytes{};
  for (std::size_t index = 0; index < events.size(); ++index) {
    const auto& item = events[index];
    consume(context, xml::operation_resource::work_units, 1U);
    consume(context, xml::operation_resource::work_units, item.name.size());
    consume(context, xml::operation_resource::work_units, item.text.size());
    if (item.kind == event_kind::start_fragment || item.kind == event_kind::end_fragment) {
      if ((index != 0 && index + 1U != events.size()) || !open.empty() || !item.name.empty() ||
          !item.attributes.empty() || !item.text.empty()) {
        throw std::invalid_argument{"Malformed HTML fragment boundary"};
      }
    } else if (item.kind == event_kind::start_element) {
      validate_child(open, item.name);
      if (!item.text.empty() || open.size() >= bounds.max_depth ||
          item.attributes.size() > bounds.max_attributes_per_element) {
        throw std::invalid_argument{"HTML element payload/depth/attribute limit exceeded"};
      }
      append(output, "<", bounds, context);
      append(output, item.name, bounds, context);
      std::set<std::string> attributes{};
      for (const auto& attribute : item.attributes) {
        if (!attribute_allowed(item.name, attribute.name) ||
            !attributes.insert(attribute.name).second || attribute.name != lower(attribute.name) ||
            (attribute.boolean != (attribute.name == "hidden")) ||
            (attribute.boolean && !attribute.value.empty())) {
          throw std::invalid_argument{"Unsupported or duplicate HTML attribute"};
        }
        consume(context, xml::operation_resource::work_units, attribute.name.size());
        consume(context, xml::operation_resource::work_units, attribute.value.size());
        append(output, " ", bounds, context);
        append(output, attribute.name, bounds, context);
        if (!attribute.boolean) {
          append(output, "=\"", bounds, context);
          escape(output, attribute.value, true, bounds, context);
          append(output, "\"", bounds, context);
        }
      }
      append(output, ">", bounds, context);
      consume(context, xml::operation_resource::allocation_bytes,
              sizeof(std::string) + item.name.size());
      open.push_back(item.name);
    } else if (item.kind == event_kind::end_element) {
      if (open.empty() || open.back() != item.name || !item.attributes.empty() ||
          !item.text.empty()) {
        throw std::invalid_argument{"Unbalanced HTML element close"};
      }
      if (!is_void(item.name)) {
        append(output, "</", bounds, context);
        append(output, item.name, bounds, context);
        append(output, ">", bounds, context);
      }
      open.pop_back();
    } else if (item.kind == event_kind::text) {
      if (!item.name.empty() || !item.attributes.empty() ||
          (!open.empty() && is_void(open.back())) ||
          item.text.size() > bounds.max_text_bytes - text_bytes) {
        throw std::invalid_argument{"Invalid HTML text event or cumulative text bound"};
      }
      if (!open.empty() && (open.back() == "ul" || open.back() == "ol") &&
          !std::all_of(item.text.begin(), item.text.end(), space)) {
        throw std::invalid_argument{"HTML lists permit only whitespace between list items"};
      }
      text_bytes += item.text.size();
      escape(output, item.text, false, bounds, context);
    } else {
      throw std::invalid_argument{"Unknown HTML event kind"};
    }
  }
  return output;
}

std::vector<event> parse_fragment(std::string_view bytes, const limits& bounds,
                                  xml::operation_context* context) {
  if (bytes.size() > bounds.max_input_bytes) {
    throw std::length_error{"HTML input byte limit exceeded"};
  }
  consume(context, xml::operation_resource::work_units, bytes.size());
  validate_utf8(bytes);
  std::vector<event> result{};
  std::vector<std::string> open{};
  const auto add = [&](event value) {
    if (result.size() >= bounds.max_events) {
      throw std::length_error{"HTML event count limit exceeded"};
    }
    consume(context, xml::operation_resource::allocation_bytes,
            sizeof(event) + value.name.size() + value.text.size());
    result.push_back(std::move(value));
  };
  add({event_kind::start_fragment, {}, {}, {}});
  std::size_t offset{};
  while (offset < bytes.size()) {
    if (bytes[offset] != '<') {
      const auto end = bytes.find('<', offset);
      const auto count = end == std::string_view::npos ? bytes.size() - offset : end - offset;
      consume(context, xml::operation_resource::allocation_bytes, count);
      add({event_kind::text, {}, {}, decode(bytes.substr(offset, count))});
      offset += count;
      continue;
    }
    if (bytes.substr(offset).starts_with("<!--")) {
      const auto end = bytes.find("-->", offset + 4U);
      if (end == std::string_view::npos ||
          bytes.substr(offset + 4U, end - offset - 4U).find("--") != std::string_view::npos) {
        throw std::invalid_argument{"Malformed HTML comment"};
      }
      offset = end + 3U;
      continue;
    }
    ++offset;
    const bool closing = offset < bytes.size() && bytes[offset] == '/';
    if (closing) {
      ++offset;
    }
    const auto begin = offset;
    while (offset < bytes.size() && name_character(bytes[offset])) {
      ++offset;
    }
    if (offset == begin) {
      throw std::invalid_argument{"Unsupported HTML declaration or tag syntax"};
    }
    auto name = lower(bytes.substr(begin, offset - begin));
    if (closing) {
      while (offset < bytes.size() && space(bytes[offset])) {
        ++offset;
      }
      if (offset >= bytes.size() || bytes[offset++] != '>' || is_void(name) || open.empty() ||
          open.back() != name) {
        throw std::invalid_argument{"HTML close requires an explicit matching nonvoid start"};
      }
      open.pop_back();
      add({event_kind::end_element, std::move(name), {}, {}});
      continue;
    }
    validate_child(open, name);
    if (open.size() >= bounds.max_depth) {
      throw std::length_error{"HTML nesting limit exceeded"};
    }
    event start{event_kind::start_element, name, {}, {}};
    bool self_closing{};
    while (true) {
      const auto separator = offset;
      while (offset < bytes.size() && space(bytes[offset])) {
        ++offset;
      }
      if (offset >= bytes.size()) {
        throw std::invalid_argument{"Truncated HTML start tag"};
      }
      if (bytes[offset] == '>') {
        ++offset;
        break;
      }
      if (bytes[offset] == '/' && offset + 1U < bytes.size() && bytes[offset + 1U] == '>') {
        self_closing = true;
        offset += 2U;
        break;
      }
      if (separator == offset || start.attributes.size() >= bounds.max_attributes_per_element) {
        throw std::invalid_argument{"HTML attributes require separation and bounded cardinality"};
      }
      const auto key_begin = offset;
      while (offset < bytes.size() && name_character(bytes[offset])) {
        ++offset;
      }
      if (key_begin == offset) {
        throw std::invalid_argument{"Unsupported HTML attribute name"};
      }
      attribute value{lower(bytes.substr(key_begin, offset - key_begin)), {}, false};
      const auto key_end = offset;
      while (offset < bytes.size() && space(bytes[offset])) {
        ++offset;
      }
      const bool assigned = offset < bytes.size() && bytes[offset] == '=';
      if (assigned) {
        ++offset;
        while (offset < bytes.size() && space(bytes[offset])) {
          ++offset;
        }
        if (offset >= bytes.size()) {
          throw std::invalid_argument{"Truncated HTML attribute"};
        }
        const auto quote = bytes[offset] == '\'' || bytes[offset] == '"' ? bytes[offset++] : '\0';
        const auto value_begin = offset;
        if (quote) {
          while (offset < bytes.size() && bytes[offset] != quote) {
            ++offset;
          }
          if (offset >= bytes.size()) {
            throw std::invalid_argument{"Unterminated HTML attribute"};
          }
        } else {
          while (offset < bytes.size() && !space(bytes[offset]) && bytes[offset] != '>') {
            if (bytes[offset] == '"' || bytes[offset] == '\'' || bytes[offset] == '<' ||
                bytes[offset] == '=' || bytes[offset] == '`') {
              throw std::invalid_argument{"Invalid unquoted HTML attribute"};
            }
            ++offset;
          }
          if (offset == value_begin) {
            throw std::invalid_argument{"Empty unquoted HTML attribute"};
          }
        }
        consume(context, xml::operation_resource::allocation_bytes, offset - value_begin);
        value.value = decode(bytes.substr(value_begin, offset - value_begin));
        if (quote) {
          ++offset;
        }
      } else {
        offset = key_end;
      }
      if (value.name == "hidden") {
        if (assigned && !value.value.empty() && lower(value.value) != "hidden") {
          throw std::invalid_argument{"HTML hidden requires a boolean spelling"};
        }
        value.boolean = true;
        value.value.clear();
      } else if (!assigned) {
        throw std::invalid_argument{"Only approved boolean HTML attributes may omit a value"};
      }
      consume(context, xml::operation_resource::allocation_bytes,
              sizeof(attribute) + value.name.size() + value.value.size());
      start.attributes.push_back(std::move(value));
    }
    if (self_closing && !is_void(name)) {
      throw std::invalid_argument{"HTML self-closing nonvoid elements require browser recovery"};
    }
    add(std::move(start));
    if (is_void(name)) {
      add({event_kind::end_element, std::move(name), {}, {}});
    } else {
      open.push_back(std::move(name));
    }
  }
  if (!open.empty()) {
    throw std::invalid_argument{"HTML fragment has unclosed elements"};
  }
  add({event_kind::end_fragment, {}, {}, {}});
  // One validator governs parser and writer acceptance, preventing divergent content models.
  (void)write_fragment(result, bounds, context);
  return result;
}
} // namespace rohit::serializer::html
