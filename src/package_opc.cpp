// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/package.hpp>

#include <rohit/xml.hpp>
#include <rohit/utf8.hpp>

#if SERIALIZER_HAS_XML
#include <libxml/tree.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace rohit::serializer::package {
namespace {
constexpr std::string_view content_types_namespace =
    "http://schemas.openxmlformats.org/package/2006/content-types";
constexpr std::string_view relationships_namespace =
    "http://schemas.openxmlformats.org/package/2006/relationships";
constexpr std::string_view relationships_content_type =
    "application/vnd.openxmlformats-package.relationships+xml";
constexpr std::string_view manifest_name = "[Content_Types].xml";
constexpr std::string_view relationships_suffix = ".rels";

// Compare media-type identities without allocating normalized copies for each selected part.
bool equal_identity(std::string_view left, std::string_view right) {
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
      [](char first, char second) {
        if (first >= 'A' && first <= 'Z') { first = static_cast<char>(first - 'A' + 'a'); }
        if (second >= 'A' && second <= 'Z') { second = static_cast<char>(second - 'A' + 'a'); }
        return first == second;
      });
}

// Apply OPC's ASCII case-insensitive comparison without locale or Unicode folding.
std::string identity(std::string_view text) {
  std::string result{text};
  for (auto& value : result) {
    if (value >= 'A' && value <= 'Z') {
      value = static_cast<char>(value - 'A' + 'a');
    }
  }
  return result;
}

// Check exact/derived OPC identities with ancestor lookups and one descendant range lookup.
std::string checked_identity(const std::set<std::string, std::less<>>& names,
                             std::string_view name) {
  auto key = identity(name);
  if (names.contains(key)) {
    throw error{error_code::duplicate_name, "Duplicate OPC part identity", std::string{name}};
  }
  // A lexical neighbor such as /a-1 can hide /a from /a/b; look up each full ancestor.
  for (auto separator = key.find('/', 1); separator != std::string::npos;
       separator = key.find('/', separator + 1)) {
    if (names.contains(std::string_view{key}.substr(0, separator))) {
      throw error{error_code::invalid_name, "OPC part name derives from another part", std::string{name}};
    }
  }
  const auto prefix = key + '/';
  const auto descendant = names.lower_bound(prefix);
  if (descendant != names.end() && descendant->starts_with(prefix)) {
    throw error{error_code::invalid_name, "OPC part name is an ancestor of another part", std::string{name}};
  }
  return key;
}

// Recognize ASCII letters independently of the active C locale.
bool letter(char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

// Recognize RFC 3986 unreserved bytes, which OPC names must not percent-encode.
bool unreserved(char value) {
  return letter(value) || (value >= '0' && value <= '9') ||
      value == '-' || value == '.' || value == '_' || value == '~';
}

// Decode one hexadecimal URI digit without accepting locale extensions.
int hex_digit(char value) {
  if (value >= '0' && value <= '9') { return value - '0'; }
  if (value >= 'A' && value <= 'F') { return value - 'A' + 10; }
  if (value >= 'a' && value <= 'f') { return value - 'a' + 10; }
  return -1;
}

// Recognize RFC 3986 sub-delimiters without accepting delimiters from another URI component.
bool sub_delimiter(char value) {
  return std::string_view{"!$&'()*+,;="}.find(value) != std::string_view::npos;
}

// Consume one valid UTF-8 scalar; URI validation never decodes or normalizes its spelling.
std::uint32_t iri_scalar(std::string_view value, std::size_t& index) {
  try {
    const auto bytes = std::span<const std::uint8_t>{
        reinterpret_cast<const std::uint8_t*>(value.data() + index), value.size() - index};
    const auto count = serializer::detail::utf8_sequence_size(bytes);
    std::uint32_t result = count == 1U ? bytes[0] : bytes[0] & (0x7fU >> count);
    for (std::size_t next = 1U; next < count; ++next) {
      result = (result << 6U) | (bytes[next] & 0x3fU);
    }
    index += count;
    return result;
  } catch (const std::invalid_argument&) {
    throw error{error_code::invalid_relationship, "Relationship IRI is not UTF-8"};
  }
}

// RFC 3987 permits these non-ASCII scalars in iunreserved; private use is query-only.
bool iri_character(std::uint32_t value, bool query) {
  const bool ordinary = (value >= 0xa0U && value <= 0xd7ffU) ||
      (value >= 0xf900U && value <= 0xfdcfU) || (value >= 0xfdf0U && value <= 0xffefU) ||
      (value >= 0x10000U && value <= 0xdfffdU && (value & 0xffffU) <= 0xfffdU) ||
      (value >= 0xe1000U && value <= 0xefffdU);
  return ordinary || (query && ((value >= 0xe000U && value <= 0xf8ffU) ||
      (value >= 0xf0000U && value <= 0xffffdU) ||
      (value >= 0x100000U && value <= 0x10fffdU)));
}

// Check a component's exact allowed characters and percent triplets in one bounded traversal.
void validate_uri_component(std::string_view value, std::string_view extra, bool query = false) {
  for (std::size_t index = 0; index < value.size(); ++index) {
    const auto byte = static_cast<unsigned char>(value[index]);
    if (value[index] == '%') {
      if (value.size() - index < 3 || hex_digit(value[index + 1]) < 0 ||
          hex_digit(value[index + 2]) < 0) {
        throw error{error_code::invalid_relationship, "Malformed URI escape"};
      }
      index += 2;
    } else if (byte >= 0x80U) {
      const auto scalar = iri_scalar(value, index);
      --index;
      if (!iri_character(scalar, query)) {
        throw error{error_code::invalid_relationship, "Invalid relationship IRI character"};
      }
    } else if (!unreserved(value[index]) && !sub_delimiter(value[index]) &&
               extra.find(value[index]) == std::string_view::npos) {
      throw error{error_code::invalid_relationship, "Invalid relationship URI character"};
    }
  }
}

// Validate an IPv4 tail inside an IPv6 address without interpreting it as a network endpoint.
bool ipv4_literal(std::string_view value) {
  std::size_t count{};
  for (std::size_t begin{}; begin < value.size();) {
    const auto end = value.find('.', begin);
    const auto piece = value.substr(begin, end == std::string_view::npos ? end : end - begin);
    if (piece.empty() || piece.size() > 3U || (piece.size() > 1U && piece.front() == '0')) { return false; }
    unsigned number{};
    for (const auto digit : piece) {
      if (digit < '0' || digit > '9') { return false; }
      number = number * 10U + static_cast<unsigned>(digit - '0');
    }
    if (number > 255U || ++count > 4U) { return false; }
    if (end == std::string_view::npos) { break; }
    begin = end + 1U;
    if (begin == value.size()) { return false; }
  }
  return count == 4U;
}

// Recognize bracketed RFC 3986 IPv6/IPvFuture syntax; no DNS or network operations occur.
bool ip_literal(std::string_view value) {
  if (value.size() >= 4U && (value.front() == 'v' || value.front() == 'V')) {
    const auto dot = value.find('.');
    if (dot == std::string_view::npos || dot <= 1U || dot + 1U == value.size()) { return false; }
    for (const auto digit : value.substr(1U, dot - 1U)) { if (hex_digit(digit) < 0) { return false; } }
    for (const auto byte : value.substr(dot + 1U)) {
      if (!unreserved(byte) && !sub_delimiter(byte) && byte != ':') { return false; }
    }
    return true;
  }
  const auto compression = value.find("::");
  if (compression != std::string_view::npos && value.find("::", compression + 2U) != std::string_view::npos) {
    return false;
  }
  std::size_t groups{};
  for (std::size_t begin{}; begin < value.size();) {
    if (value[begin] == ':') {
      if (compression == std::string_view::npos ||
          (begin != compression && begin != compression + 1U)) { return false; }
      ++begin;
      continue;
    }
    const auto end = value.find(':', begin);
    const auto piece = value.substr(begin, end == std::string_view::npos ? end : end - begin);
    if (piece.find('.') != std::string_view::npos) {
      if (end != std::string_view::npos || !ipv4_literal(piece)) { return false; }
      groups += 2U;
    } else {
      if (piece.empty() || piece.size() > 4U) { return false; }
      for (const auto digit : piece) { if (hex_digit(digit) < 0) { return false; } }
      ++groups;
    }
    if (end == std::string_view::npos) { break; }
    begin = end + 1U;
    if (begin == value.size() && end != compression + 1U) { return false; }
  }
  return compression == std::string_view::npos ? groups == 8U : groups < 8U;
}

// Separate user-info, host and port so brackets and colons are accepted only in their URI roles.
void validate_authority(std::string_view value) {
  const auto at = value.find('@');
  if (at != std::string_view::npos) {
    validate_uri_component(value.substr(0, at), ":");
    value.remove_prefix(at + 1U);
  }
  std::string_view port;
  if (value.starts_with('[')) {
    const auto close = value.find(']');
    if (close == std::string_view::npos || !ip_literal(value.substr(1U, close - 1U)) ||
        (close + 1U < value.size() && value[close + 1U] != ':')) {
      throw error{error_code::invalid_relationship, "Invalid relationship URI IP literal"};
    }
    if (close + 1U < value.size()) { port = value.substr(close + 2U); }
  } else {
    const auto colon = value.find(':');
    validate_uri_component(value.substr(0, colon), {});
    if (colon != std::string_view::npos) { port = value.substr(colon + 1U); }
  }
  for (const auto digit : port) {
    if (digit < '0' || digit > '9') {
      throw error{error_code::invalid_relationship, "Invalid relationship URI port"};
    }
  }
}

// Validate RFC 3986/3987 references component by component while retaining authored spelling.
void validate_uri(std::string_view value, bool absolute) {
  const auto fragment = value.find('#');
  if (fragment != std::string_view::npos) {
    if (absolute) { throw error{error_code::invalid_relationship, "Relationship type IRI has a fragment"}; }
    validate_uri_component(value.substr(fragment + 1U), ":@/?");
    value = value.substr(0, fragment);
  }
  const auto query = value.find('?');
  if (query != std::string_view::npos) {
    validate_uri_component(value.substr(query + 1U), ":@/?", true);
    value = value.substr(0, query);
  }
  const auto colon = value.find(':');
  const auto slash = value.find('/');
  const bool scheme = colon != std::string_view::npos && (slash == std::string_view::npos || colon < slash);
  if (scheme) {
    if (colon == 0U || !letter(value.front())) {
      throw error{error_code::invalid_relationship, "Invalid URI scheme"};
    }
    for (const auto byte : value.substr(1U, colon - 1U)) {
      if (!letter(byte) && (byte < '0' || byte > '9') && byte != '+' && byte != '-' && byte != '.') {
        throw error{error_code::invalid_relationship, "Invalid URI scheme"};
      }
    }
    value.remove_prefix(colon + 1U);
  } else if (absolute) {
    throw error{error_code::invalid_relationship, "Relationship type requires an absolute IRI"};
  }
  if (value.starts_with("//")) {
    value.remove_prefix(2U);
    const auto end = value.find('/');
    validate_authority(value.substr(0, end));
    value = end == std::string_view::npos ? std::string_view{} : value.substr(end);
  }
  validate_uri_component(value, ":@/");
}

// Validate canonical OPC part URI syntax before comparing or translating it to ZIP storage.
void validate_part_name(std::string_view name) {
  constexpr auto max_name_bytes = std::size_t{1} + std::numeric_limits<std::uint16_t>::max();
  if (name.size() < 2 || name.size() > max_name_bytes || name.front() != '/' || name.back() == '/' ||
      name.find_first_of("?#\\") != std::string_view::npos) {
    throw error{error_code::invalid_name, "Invalid OPC part URI"};
  }
  for (std::size_t begin = 1; begin < name.size();) {
    const auto end = name.find('/', begin);
    const auto segment = name.substr(begin, end == std::string_view::npos ? end : end - begin);
    if (segment.empty() || segment.back() == '.') {
      throw error{error_code::invalid_name, "Invalid OPC part URI segment"};
    }
    for (std::size_t index = 0; index < segment.size(); ++index) {
      const auto value = segment[index];
      if (value == '%') {
        if (segment.size() - index < 3 || hex_digit(segment[index + 1]) < 0 ||
            hex_digit(segment[index + 2]) < 0) {
          throw error{error_code::invalid_name, "Malformed OPC part URI escape"};
        }
        const auto decoded = static_cast<char>((hex_digit(segment[index + 1]) << 4) |
                                               hex_digit(segment[index + 2]));
        // This ASCII subset also refuses escaped non-ASCII octets; I18N part names need
        // a separate Unicode/IRI policy and must not smuggle encoded iunreserved characters.
        if (unreserved(decoded) || decoded == '/' || decoded == '\\' ||
            static_cast<unsigned char>(decoded) >= 0x80) {
          throw error{error_code::invalid_name, "Forbidden OPC part URI escape"};
        }
        index += 2;
      } else if (!unreserved(value) && std::string_view{"!$&'()*+,;=:@"}.find(value) ==
                                           std::string_view::npos) {
        throw error{error_code::invalid_name, "Invalid OPC part URI character"};
      }
    }
    if (end == std::string_view::npos) { break; }
    begin = end + 1;
  }
}

// Keep declared unsupported Unicode candidates distinct from legal physical non-part items.
bool maps_to_supported_part(std::string_view name) {
  for (std::size_t index{}; index < name.size(); ++index) {
    const auto byte = static_cast<unsigned char>(name[index]);
    if (byte >= 0x80U || (byte == '%' && name.size() - index >= 3U &&
        hex_digit(name[index + 1U]) >= 8 && hex_digit(name[index + 2U]) >= 0)) {
      throw error{error_code::unsupported_feature, "Non-ASCII OPC part names are unsupported"};
    }
  }
  try { validate_part_name(name); return true; }
  catch (const error& failure) {
    if (failure.code != error_code::invalid_name) { throw; }
    return false;
  }
}

// Interpret common APPNOTE creator attributes without treating directory/device bytes as parts.
bool physical_file(const zip_entry& entry) {
  if (entry.name.ends_with('/')) { return false; }
  constexpr std::uint8_t dos_system = 0U, unix_system = 3U, ntfs_system = 10U;
  constexpr std::uint8_t vfat_system = 14U, macos_system = 19U;
  constexpr std::uint32_t dos_directory_or_volume = 0x18U;
  if ((entry.creator_system == dos_system || entry.creator_system == ntfs_system ||
       entry.creator_system == vfat_system) &&
      (entry.external_attributes & dos_directory_or_volume) != 0U) { return false; }
  if (entry.creator_system == unix_system || entry.creator_system == macos_system) {
    constexpr std::uint32_t unix_type_mask = 0170000U, unix_regular_file = 0100000U;
    const auto type = (entry.external_attributes >> 16U) & unix_type_mask;
    if (type != 0U && type != unix_regular_file) { return false; }
  }
  return true;
}

// Return the source for a relationship part, or an empty string for a normal payload part.
std::string relationship_source(std::string_view name) {
  if (equal_identity(name, "/_rels/.rels")) { return "/"; }
  const auto separator = name.rfind('/');
  if (separator == std::string_view::npos || separator == 0U ||
      name.size() - separator - 1U < relationships_suffix.size() ||
      !equal_identity(name.substr(name.size() - relationships_suffix.size()), relationships_suffix)) {
    return {};
  }
  const auto parent = name.rfind('/', separator - 1U);
  if (parent == std::string_view::npos ||
      !equal_identity(name.substr(parent + 1U, separator - parent - 1U), "_rels")) {
    return {};
  }
  const auto basename = name.substr(separator + 1U,
      name.size() - separator - 1U - relationships_suffix.size());
  if (basename.empty()) {
    throw error{error_code::invalid_metadata, "Invalid relationship part name"};
  }
  auto result = std::string{name.substr(0, parent + 1U)};
  result += basename;
  validate_part_name(result);
  return result;
}

// Derive standard .rels ZIP names from a package or part relationship source.
std::string relationship_name(std::string_view source) {
  if (source == "/") { return "_rels/.rels"; }
  const auto separator = source.rfind('/');
  return std::string{source.substr(1, separator)} + "_rels/" +
      std::string{source.substr(separator + 1)} + std::string{relationships_suffix};
}

// Reserve all infrastructure identities for generated metadata, including root .rels.
void validate_payload_name(std::string_view name) {
  validate_part_name(name);
  if (identity(name) == "/[content_types].xml" || !relationship_source(name).empty()) {
    throw error{error_code::invalid_name, "OPC infrastructure names are reserved"};
  }
}

// MIME tokens exclude separators and controls; parameters are explicitly parsed below.
bool mime_token(char value) {
  return static_cast<unsigned char>(value) > 0x20 &&
      static_cast<unsigned char>(value) < 0x7f &&
      std::string_view{R"mime(()<>@,;:\"/[]?={})mime"}.find(value) == std::string_view::npos;
}

// Validate media type syntax without imposing application vocabulary semantics.
void validate_content_type(std::string_view value) {
  try { serializer::detail::validate_utf8(value); }
  catch (const std::invalid_argument&) {
    throw error{error_code::invalid_metadata, "OPC content type is not UTF-8"};
  }
  std::size_t position{};
  // Consume a nonempty RFC token.
  const auto token = [&] {
    const auto begin = position;
    while (position < value.size() && mime_token(value[position])) { ++position; }
    return position != begin;
  };
  if (!token() || position == value.size() || value[position++] != '/' || !token()) {
    throw error{error_code::invalid_metadata, "Invalid OPC content type"};
  }
  const auto base = identity(value.substr(0, position));
  constexpr std::string_view opc_types[]{relationships_content_type,
      "application/vnd.openxmlformats-package.core-properties+xml",
      "application/vnd.openxmlformats-package.digital-signature-origin",
      "application/vnd.openxmlformats-package.digital-signature-xmlsignature+xml",
      "application/vnd.openxmlformats-package.digital-signature-certificate"};
  if (position < value.size() && std::find(std::begin(opc_types), std::end(opc_types), base) != std::end(opc_types)) {
    throw error{error_code::invalid_metadata, "OPC-defined media types cannot have parameters"};
  }
  while (position < value.size()) {
    while (position < value.size() && (value[position] == ' ' || value[position] == '\t')) { ++position; }
    if (position == value.size() || value[position++] != ';') {
      throw error{error_code::invalid_metadata, "Invalid OPC media type parameter"};
    }
    while (position < value.size() && (value[position] == ' ' || value[position] == '\t')) { ++position; }
    if (!token() || position == value.size() || value[position++] != '=') {
      throw error{error_code::invalid_metadata, "Invalid OPC media type parameter"};
    }
    if (position < value.size() && value[position] == '"') {
      ++position;
      bool closed{};
      while (position < value.size()) {
        auto byte = static_cast<unsigned char>(value[position++]);
        if (byte == '"') { closed = true; break; }
        if (byte == '\\') {
          if (position == value.size()) { break; }
          byte = static_cast<unsigned char>(value[position++]);
          if (byte != '\t' && (byte < 0x20 || byte >= 0x7f)) {
            throw error{error_code::invalid_metadata, "Invalid media type quoted-pair"};
          }
        } else if (byte >= 0x80U) {
          --position;
          const auto scalar = iri_scalar(value, position);
          if (scalar < 0xa0U || scalar > 0xffU) {
            throw error{error_code::invalid_metadata, "Invalid media type quoted character"};
          }
        } else if (byte != '\t' && (byte < 0x20 || byte == 0x7f)) {
          throw error{error_code::invalid_metadata, "Invalid quoted media type parameter"};
        }
      }
      if (!closed) { throw error{error_code::invalid_metadata, "Unclosed media type parameter"}; }
    } else if (!token()) {
      throw error{error_code::invalid_metadata, "Empty media type parameter"};
    }
  }
}

// Compare declared MIME roles without folding parameter values or allocating for plain media types.
bool equal_media_type(std::string_view first, std::string_view second, budget& shared) {
  shared.consume(resource::work_units, first.size());
  shared.consume(resource::work_units, second.size());
  if (first == second) { return true; }
  const auto first_separator = first.find(';'), second_separator = second.find(';');
  if (!equal_identity(xml::detail::trim(first.substr(0U, first_separator)),
                      xml::detail::trim(second.substr(0U, second_separator)))) { return false; }
  if (first_separator == std::string_view::npos || second_separator == std::string_view::npos) {
    return first_separator == second_separator;
  }
  shared.consume(resource::decoder_allocation_bytes, second.size());
  validate_content_type(second);
  struct parameter { std::string_view name; std::string_view value; bool quoted{}; };
  // Count first, then reserve exact borrowed views; no model-sized normalized strings are retained.
  const auto parameters = [&](std::string_view value) {
    const auto visit = [&](auto visitor) {
      auto remaining = value;
      while (!remaining.empty()) {
        remaining.remove_prefix(1U);
        remaining = xml::detail::trim(remaining);
        const auto equals = remaining.find('=');
        if (equals == std::string_view::npos) { throw error{error_code::invalid_metadata, "Invalid MIME role parameter"}; }
        const auto name = remaining.substr(0U, equals);
        remaining.remove_prefix(equals + 1U);
        const bool quoted = !remaining.empty() && remaining.front() == '"';
        std::size_t end{};
        if (quoted) {
          remaining.remove_prefix(1U);
          while (end < remaining.size() && remaining[end] != '"') {
            if (remaining[end] == '\\') { ++end; }
            ++end;
          }
          if (end >= remaining.size()) { throw error{error_code::invalid_metadata, "Invalid MIME role quotation"}; }
        } else {
          while (end < remaining.size() && mime_token(remaining[end])) { ++end; }
        }
        visitor(parameter{name, remaining.substr(0U, end), quoted});
        remaining.remove_prefix(end + (quoted ? 1U : 0U));
        remaining = xml::detail::trim(remaining);
        if (!remaining.empty() && remaining.front() != ';') {
          throw error{error_code::invalid_metadata, "Invalid MIME role separator"};
        }
      }
    };
    std::size_t count{};
    visit([&](const parameter&) { ++count; });
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(parameter)) {
      throw std::length_error{"MIME role allocation overflow"};
    }
    shared.consume(resource::decoder_allocation_bytes, count * sizeof(parameter));
    shared.consume(resource::work_units, value.size());
    shared.consume(resource::work_units, value.size());
    std::vector<parameter> result;
    result.reserve(count);
    visit([&](parameter item) { result.push_back(item); });
    std::sort(result.begin(), result.end(), [&](const parameter& left, const parameter& right) {
      shared.consume(resource::work_units, left.name.size());
      shared.consume(resource::work_units, right.name.size());
      return std::lexicographical_compare(left.name.begin(), left.name.end(),
          right.name.begin(), right.name.end(), [](unsigned char a, unsigned char b) {
            if (a >= 'A' && a <= 'Z') { a = static_cast<unsigned char>(a - 'A' + 'a'); }
            if (b >= 'A' && b <= 'Z') { b = static_cast<unsigned char>(b - 'A' + 'a'); }
            return a < b;
          });
    });
    return result;
  };
  const auto left = parameters(first.substr(first_separator));
  const auto right = parameters(second.substr(second_separator));
  if (left.size() != right.size()) { return false; }
  for (std::size_t index{}; index < left.size(); ++index) {
    if (!equal_identity(left[index].name, right[index].name) ||
        (index != 0U && (equal_identity(left[index - 1U].name, left[index].name) ||
                        equal_identity(right[index - 1U].name, right[index].name)))) { return false; }
    auto a = left[index].value, b = right[index].value;
    while (!a.empty() && !b.empty()) {
      shared.consume(resource::work_units, 1U);
      if (left[index].quoted && a.front() == '\\') { a.remove_prefix(1U); }
      if (right[index].quoted && b.front() == '\\') { b.remove_prefix(1U); }
      if (a.empty() || b.empty() || a.front() != b.front()) { return false; }
      a.remove_prefix(1U); b.remove_prefix(1U);
    }
    if (!a.empty() || !b.empty()) { return false; }
  }
  return true;
}

// Relationships parts own their reserved media type; payload names cannot impersonate them.
void validate_payload_content_type(std::string_view value) {
  validate_content_type(value);
  if (identity(value) == relationships_content_type) {
    throw error{error_code::invalid_metadata, "Relationship media type requires a relationship part name"};
  }
}

// Relationship identifiers use the complete XML NCName lexical space, including Unicode names.
void validate_id(std::string_view value) {
#if SERIALIZER_HAS_XML
  if (value.find('\0') != std::string_view::npos) {
    throw error{error_code::invalid_relationship, "Relationship ID contains a zero byte"};
  }
  const std::string terminated{value};
  if (xmlValidateNCName(reinterpret_cast<const xmlChar*>(terminated.c_str()), 0) != 0) {
    throw error{error_code::invalid_relationship, "Relationship ID is not an XML NCName"};
  }
#else
  if (value.empty() || (!letter(value.front()) && value.front() != '_')) {
    throw error{error_code::invalid_relationship, "Relationship ID is not an ASCII NCName"};
  }
  for (const auto byte : value) {
    if (!letter(byte) && (byte < '0' || byte > '9') && byte != '_' && byte != '.' && byte != '-') {
      throw error{error_code::invalid_relationship, "Relationship ID is not an ASCII NCName"};
    }
  }
#endif
}

// Validate one link independently of target existence so forward references are supported.
void validate_relationship(const relationship& link) {
  if (link.source != "/") {
    validate_payload_name(link.source);
  }
  validate_id(link.id);
  validate_uri(link.type, true);
  if (link.mode == target_mode::internal) {
    static_cast<void>(resolve_target(link.source, link.target));
  } else {
    validate_uri(link.target, false);
  }
}

// Find OPC parts using their specified ASCII case-insensitive identity.
const part* find_part(const std::vector<part>& parts, std::string_view name) {
  const auto key = identity(name);
  const auto found = std::find_if(parts.begin(), parts.end(), [&key](const auto& item) {
    return identity(item.name) == key;
  });
  return found == parts.end() ? nullptr : &*found;
}

// Validate reference existence after all declarations, including forward links, are known.
void validate_links(const std::vector<part>& parts, const std::vector<relationship>& links,
                    budget& shared) {
  std::set<std::pair<std::string, std::string>> ids;
  for (const auto& link : links) {
    shared.check_cancelled();
    validate_relationship(link);
    if (!ids.emplace(identity(link.source), link.id).second) {
      throw error{error_code::invalid_relationship, "Duplicate source-local relationship ID",
                  link.source};
    }
    if (link.source != "/" && !find_part(parts, link.source)) {
      throw error{error_code::missing_part, "Relationship source part does not exist", link.source};
    }
    if (link.mode == target_mode::internal) {
      const auto target = resolve_target(link.source, link.target);
      if (!find_part(parts, target)) {
        throw error{error_code::missing_part, "Relationship target part does not exist", target};
      }
    }
  }
}

// Bound metadata XML through the same operation policy, with no package-supplied resolver.
xml::limits xml_bounds(const budget& shared) {
  const auto& policy = shared.configured();
  const auto maximum = static_cast<std::size_t>(std::min({policy.max_part_bytes,
      shared.remaining(resource::metadata_bytes), policy.max_buffer_bytes,
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())}));
  xml::limits result{};
  result.max_input_bytes = maximum;
  result.max_output_bytes = maximum;
  result.max_text_bytes = maximum;
  result.max_nodes = maximum / sizeof(xml::element);
  result.max_attributes = maximum / sizeof(xml::attribute);
  result.max_depth = 8;
  result.max_tree_bytes = maximum;
  return result;
}

// Require optional XML support before reading or emitting package infrastructure.
void require_xml() {
  if (!xml::available()) {
    throw error{error_code::unavailable, "OPC metadata requires XML support in this build"};
  }
}

// Construct namespace-aware metadata elements without maintaining a second XML implementation.
xml::element node(std::string_view local, std::string_view namespace_uri) {
  xml::element result{};
  result.qualified_name = {std::string{local}, std::string{namespace_uri}};
  return result;
}

// Add one unqualified infrastructure attribute; the shared XML writer performs escaping.
void attribute(xml::element& element, std::string_view name, std::string_view value) {
  element.attributes.push_back({{std::string{name}, {}}, std::string{value}});
}

// Require the exact expanded root/child name rather than a document prefix spelling.
void require_name(const xml::element& element, std::string_view local, std::string_view uri) {
  if (element.qualified_name.local != local || element.qualified_name.namespace_uri != uri) {
    throw error{error_code::invalid_metadata, "Unexpected OPC metadata expanded name"};
  }
}

// Reject undeclared attributes and nested/text content in flat OPC metadata records.
std::map<std::string, std::string> attributes(
    const xml::element& element, std::initializer_list<std::string_view> permitted,
    bool permits_text = false) {
  if (!element.children.empty() ||
      (!permits_text && element.text.find_first_not_of(" \t\r\n") != std::string::npos)) {
    throw error{error_code::invalid_metadata, "Unexpected OPC metadata content"};
  }
  std::map<std::string, std::string> result;
  for (const auto& item : element.attributes) {
    if (!item.qualified_name.namespace_uri.empty() ||
        std::find(permitted.begin(), permitted.end(), item.qualified_name.local) == permitted.end() ||
        !result.emplace(item.qualified_name.local, item.value).second) {
      throw error{error_code::invalid_metadata, "Unexpected or duplicate OPC metadata attribute"};
    }
  }
  return result;
}

// anyURI permits an empty value; required means that the attribute itself must exist.
const std::string& present(const std::map<std::string, std::string>& values,
                          std::string_view key) {
  const auto found = values.find(std::string{key});
  if (found == values.end()) {
    throw error{error_code::invalid_metadata, "Required OPC metadata attribute missing"};
  }
  return found->second;
}

// Read one required nonempty metadata attribute with no map insertion side effect.
const std::string& required(const std::map<std::string, std::string>& values,
                            std::string_view key) {
  const auto found = values.find(std::string{key});
  if (found == values.end() || found->second.empty()) {
    throw error{error_code::invalid_metadata, "Required OPC metadata attribute missing"};
  }
  return found->second;
}

// Convert validated metadata output to the codec-independent ZIP byte interface.
void write_metadata(zip_writer& archive, std::string_view name, const xml::element& document) {
  auto& shared = *archive.shared_budget();
  shared.check_cancelled();
  auto context = shared.xml_context(name);
  const auto text = xml::write(document, xml_bounds(shared), &context);
  shared.consume(resource::metadata_bytes, text.size(), name);
  archive.add(name, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

constexpr std::string_view compatibility_namespace{
    "http://schemas.openxmlformats.org/markup-compatibility/2006"};
constexpr std::string_view xml_namespace{"http://www.w3.org/XML/1998/namespace"};

// Borrow inherited expanded-name rules only while the owning input attributes remain intact.
struct compatibility_scope {
  struct content_name { std::string_view uri; std::string_view local; };
  const compatibility_scope* parent{};
  std::vector<std::string_view> ignorable{};
  std::vector<content_name> process_content{};
  // Link one lexical scope without copying ancestor sets or retaining it beyond recursion.
  explicit compatibility_scope(const compatibility_scope* previous) : parent{previous} {}
  // Scope chains are stack-local and cannot be copied away from their borrowed input.
  compatibility_scope(const compatibility_scope&) = delete;
  // Replacing a live scope would invalidate the inherited rule chain.
  compatibility_scope& operator=(const compatibility_scope&) = delete;
};

// Apply ECMA-376 Part 3 with only Relationships understood and no extension-element roots.
class relationships_compatibility {
  enum class disposition { keep, discard, unwrap };
  xml::operation_context& operation_;

  // Charge complete auxiliary reservations before any vector/string allocation.
  template <typename Value> void allocation(std::size_t count) {
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(Value)) {
      throw std::length_error{"OPC compatibility allocation overflow"};
    }
    operation_.consume(xml::operation_resource::allocation_bytes, count * sizeof(Value));
  }
  // Grow scoped rule vectors geometrically with explicit cumulative allocation charges.
  template <typename Value> void append(std::vector<Value>& values, Value value) {
    if (values.size() == values.capacity()) {
      if (values.capacity() > values.max_size() / 2U) {
        throw std::length_error{"OPC compatibility rule limit exceeded"};
      }
      const auto capacity = values.empty() ? 1U : values.capacity() * 2U;
      allocation<Value>(capacity);
      values.reserve(capacity);
    }
    values.push_back(std::move(value));
  }
  // Visit XML-whitespace-delimited tokens without constructing an intermediate list.
  template <typename Visitor> void tokens(std::string_view value, Visitor visitor) {
    value = xml::detail::trim(value);
    while (!value.empty()) {
      const auto end = value.find_first_of(" \t\r\n");
      const auto token = value.substr(0U, end);
      operation_.consume(xml::operation_resource::work_units, token.size() + 1U);
      visitor(token);
      if (end == std::string_view::npos) { return; }
      value = xml::detail::trim(value.substr(end));
    }
  }
  // Resolve an NCName prefix in its original lexical scope, including the predefined XML binding.
  std::string_view prefix(const xml::element& node, std::string_view value) {
    xml::validate_lexical_value(value, "NCName", node, &operation_);
    if (value == "xml") { return xml_namespace; }
    allocation<char>(value.size() + 1U);
    const auto found = node.namespace_bindings.find(std::string{value});
    if (found == node.namespace_bindings.end() || found->second.empty() ||
        found->second == compatibility_namespace) {
      throw std::invalid_argument{"Invalid OPC compatibility namespace prefix"};
    }
    return found->second;
  }
  // Infrastructure does not implicitly claim support for an application's foreign vocabulary.
  static bool understood(std::string_view uri) {
    return uri == relationships_namespace;
  }
  // Search borrowed ancestor rules, charging each comparison so deep scopes remain bounded.
  bool ignorable(const compatibility_scope& scope, std::string_view uri) {
    for (auto current = &scope; current; current = current->parent) {
      for (const auto candidate : current->ignorable) {
        operation_.consume(xml::operation_resource::work_units, 1U);
        if (candidate == uri) { return true; }
      }
    }
    return false;
  }
  // ProcessContent matches expanded identity, independent of prefix spelling or rebinding.
  bool process_content(const compatibility_scope& scope, const xml::name& name) {
    for (auto current = &scope; current; current = current->parent) {
      for (const auto& candidate : current->process_content) {
        operation_.consume(xml::operation_resource::work_units, 1U);
        if (candidate.uri == name.namespace_uri &&
            (candidate.local == "*" || candidate.local == name.local)) { return true; }
      }
    }
    return false;
  }
  // Parse local declarations before interpreting any element/attribute governed by that scope.
  void declarations(const xml::element& node, compatibility_scope& scope) {
    for (const auto& attribute : node.attributes) {
      operation_.consume(xml::operation_resource::work_units, 1U);
      if (attribute.qualified_name.namespace_uri == compatibility_namespace &&
          attribute.qualified_name.local == "Ignorable") {
        tokens(attribute.value, [&](std::string_view token) {
          append(scope.ignorable, prefix(node, token));
        });
      }
    }
    for (const auto& attribute : node.attributes) {
      if (attribute.qualified_name.namespace_uri != compatibility_namespace) { continue; }
      const auto& local = attribute.qualified_name.local;
      if (local == "ProcessContent") {
        tokens(attribute.value, [&](std::string_view token) {
          const auto colon = token.find(':');
          if (colon == std::string_view::npos || colon == 0U || colon + 1U == token.size()) {
            throw std::invalid_argument{"Invalid OPC ProcessContent name"};
          }
          const auto uri = prefix(node, token.substr(0U, colon));
          const auto name = token.substr(colon + 1U);
          if (name != "*") { xml::validate_lexical_value(name, "NCName", node, &operation_); }
          if (!ignorable(scope, uri)) {
            throw std::invalid_argument{"OPC ProcessContent namespace is not ignorable"};
          }
          append(scope.process_content, compatibility_scope::content_name{uri, name});
        });
      } else if (local == "MustUnderstand") {
        tokens(attribute.value, [&](std::string_view token) { (void)prefix(node, token); });
      } else if (local != "Ignorable") {
        throw std::invalid_argument{"Unknown OPC compatibility attribute"};
      }
    }
  }
  // Check capability requirements only for content retained by the reference processing model.
  void must_understand(const xml::element& node) {
    for (const auto& attribute : node.attributes) {
      if (attribute.qualified_name.namespace_uri == compatibility_namespace &&
          attribute.qualified_name.local == "MustUnderstand") {
        tokens(attribute.value, [&](std::string_view token) {
          if (!understood(prefix(node, token))) {
            throw std::invalid_argument{"Unsupported required OPC compatibility namespace"};
          }
        });
      }
    }
  }
  // Validate wrapper attributes even on unselected choices, without evaluating MustUnderstand.
  bool wrapper(const xml::element& node, const compatibility_scope& scope, bool choice) {
    const xml::attribute* requires_attribute{};
    for (const auto& attribute : node.attributes) {
      const auto& name = attribute.qualified_name;
      if (name.namespace_uri.empty()) {
        if (!choice || name.local != "Requires" || requires_attribute) {
          throw std::invalid_argument{"Unexpected OPC compatibility wrapper attribute"};
        }
        requires_attribute = &attribute;
      } else if (name.namespace_uri != compatibility_namespace &&
                 (name.namespace_uri == xml_namespace || !ignorable(scope, name.namespace_uri))) {
        throw std::invalid_argument{"Invalid qualified OPC compatibility wrapper attribute"};
      }
    }
    if (!choice) { return false; }
    if (!requires_attribute || xml::detail::trim(requires_attribute->value).empty()) {
      throw std::invalid_argument{"OPC compatibility Choice requires namespace prefixes"};
    }
    bool supported = true;
    tokens(requires_attribute->value, [&](std::string_view token) {
      if (!understood(prefix(node, token))) { supported = false; }
    });
    return supported;
  }
  // Flatten processed fragments in linear time, retaining ordered text and moving complete subtrees.
  void children(xml::element& node, const compatibility_scope& scope) {
    if (node.children.empty()) { return; }
    allocation<disposition>(node.children.size());
    std::vector<disposition> actions;
    actions.reserve(node.children.size());
    bool changed{};
    std::size_t child_count{}, segment_count = node.text_segments.size(), text_bytes = node.text.size();
    const auto add = [](std::size_t& value, std::size_t amount) {
      if (amount > std::numeric_limits<std::size_t>::max() - value) {
        throw std::length_error{"OPC compatibility fragment overflow"};
      }
      value += amount;
    };
    for (auto& child : node.children) {
      const auto action = process(child, &scope, false);
      actions.push_back(action);
      changed = changed || action != disposition::keep;
      if (action == disposition::keep) { add(child_count, 1U); }
      else if (action == disposition::unwrap) {
        add(child_count, child.children.size());
        add(segment_count, child.text_segments.size());
        add(text_bytes, child.text.size());
      }
    }
    if (!changed) { return; }
    allocation<xml::element>(child_count);
    allocation<xml::element::text_segment>(segment_count);
    allocation<char>(text_bytes + 1U);
    std::vector<xml::element> output_children;
    std::vector<xml::element::text_segment> output_segments;
    std::string output_text;
    output_children.reserve(child_count);
    output_segments.reserve(segment_count);
    output_text.reserve(text_bytes);
    const auto text = [&](xml::element::text_segment segment, std::size_t offset) {
      operation_.consume(xml::operation_resource::work_units, segment.value.size() + 1U);
      output_text += segment.value;
      segment.before_child += offset;
      output_segments.push_back(std::move(segment));
    };
    std::size_t next_segment{};
    for (std::size_t index{}; index <= node.children.size(); ++index) {
      while (next_segment < node.text_segments.size() &&
             node.text_segments[next_segment].before_child == index) {
        auto segment = std::move(node.text_segments[next_segment++]);
        segment.before_child = output_children.size();
        text(std::move(segment), 0U);
      }
      if (index == node.children.size()) { break; }
      auto& child = node.children[index];
      if (actions[index] == disposition::keep) { output_children.push_back(std::move(child)); }
      else if (actions[index] == disposition::unwrap) {
        const auto offset = output_children.size();
        for (auto& segment : child.text_segments) { text(std::move(segment), offset); }
        for (auto& descendant : child.children) { output_children.push_back(std::move(descendant)); }
      }
    }
    node.children = std::move(output_children);
    node.text_segments = std::move(output_segments);
    node.text = std::move(output_text);
  }
  // Select the first understood Choice, or its Fallback, after checking complete sibling ordering.
  void alternate(xml::element& node, const compatibility_scope& scope) {
    (void)wrapper(node, scope, false);
    if (!xml::detail::trim(node.text).empty()) {
      throw std::invalid_argument{"Text in OPC AlternateContent wrapper"};
    }
    std::size_t choices{}, selected = node.children.size();
    bool fallback{};
    for (std::size_t index{}; index < node.children.size(); ++index) {
      const auto& child = node.children[index];
      compatibility_scope child_scope{&scope};
      declarations(child, child_scope);
      const auto& name = child.qualified_name;
      if (!understood(name.namespace_uri) && ignorable(child_scope, name.namespace_uri) &&
          !process_content(child_scope, name)) { continue; }
      if (name.namespace_uri != compatibility_namespace) {
        throw std::invalid_argument{"Unexpected OPC AlternateContent child"};
      }
      if (name.local == "Choice" && !fallback) {
        ++choices;
        const auto supported = wrapper(child, child_scope, true);
        if (selected == node.children.size() && supported) { selected = index; }
      } else if (name.local == "Fallback" && choices != 0U && !fallback) {
        fallback = true;
        (void)wrapper(child, child_scope, false);
        if (selected == node.children.size()) { selected = index; }
      } else { throw std::invalid_argument{"Invalid OPC Choice/Fallback ordering"}; }
    }
    if (choices == 0U) { throw std::invalid_argument{"OPC AlternateContent has no Choice"}; }
    must_understand(node);
    if (selected == node.children.size()) {
      node.children.clear(); node.text.clear(); node.text_segments.clear();
    } else {
      auto branch = std::move(node.children[selected]);
      (void)process(branch, &scope, true);
      node.children = std::move(branch.children);
      node.text = std::move(branch.text);
      node.text_segments = std::move(branch.text_segments);
    }
  }
  // Apply inherited rules while keeping their borrowed attribute storage alive through descendants.
  disposition process(xml::element& node, const compatibility_scope* parent, bool selected_branch) {
    operation_.consume(xml::operation_resource::work_units, 1U);
    compatibility_scope scope{parent};
    declarations(node, scope);
    const auto& name = node.qualified_name;
    if (!understood(name.namespace_uri) && ignorable(scope, name.namespace_uri)) {
      if (!process_content(scope, name)) { return disposition::discard; }
      for (const auto& attribute : node.attributes) {
        if (attribute.qualified_name.namespace_uri == xml_namespace &&
            (attribute.qualified_name.local == "base" || attribute.qualified_name.local == "lang" ||
             attribute.qualified_name.local == "space")) {
          throw std::invalid_argument{"XML context attribute on unwrapped OPC element"};
        }
      }
      must_understand(node);
      children(node, scope);
      return disposition::unwrap;
    }
    if (name.namespace_uri == compatibility_namespace) {
      if (name.local == "AlternateContent") { alternate(node, scope); return disposition::unwrap; }
      if (!selected_branch || (name.local != "Choice" && name.local != "Fallback")) {
        throw std::invalid_argument{"Unexpected OPC compatibility element"};
      }
      (void)wrapper(node, scope, name.local == "Choice");
      must_understand(node);
      children(node, scope);
      return disposition::unwrap;
    }
    must_understand(node);
    children(node, scope);
    std::erase_if(node.attributes, [&](const xml::attribute& attribute) {
      const auto& uri = attribute.qualified_name.namespace_uri;
      return uri == compatibility_namespace || (!understood(uri) && ignorable(scope, uri));
    });
    return disposition::keep;
  }
public:
  // Borrow the one operation ledger through the complete preprocessing pass.
  explicit relationships_compatibility(xml::operation_context& operation) : operation_{operation} {}
  // Permit AlternateContent at the document root, requiring exactly one resulting XML root.
  void apply(xml::element& document) {
    xml::element container{};
    allocation<xml::element>(1U);
    container.children.push_back(std::move(document));
    compatibility_scope root{nullptr};
    children(container, root);
    if (container.children.size() != 1U || !xml::detail::trim(container.text).empty()) {
      throw std::invalid_argument{"OPC compatibility output must have one root element"};
    }
    document = std::move(container.children.front());
  }
};

// Parse only a selected infrastructure entry and charge bounded tree/index storage conservatively.
xml::element read_metadata(zip_reader& archive, std::string_view name, bool relationships = false) {
  auto& shared = *archive.shared_budget();
  const auto* entry = archive.find(name);
  if (!entry || !physical_file(*entry)) {
    throw error{error_code::missing_part, "OPC metadata file is absent", std::string{name}};
  }
  shared.consume(resource::metadata_bytes, entry->expanded_bytes, name);
  const auto bytes = archive.read(name);
  try {
    auto context = shared.xml_context(name);
    context.input_encoding = xml::input_encoding_policy::utf8_or_utf16;
    context.allow_mixed_content = relationships;
    auto document = xml::parse({reinterpret_cast<const char*>(bytes.data()), bytes.size()},
                               xml_bounds(shared), &context);
    shared.check_cancelled();
    // Each retained tree item is bounded independently of its compressed input length.
    std::uint64_t storage{};
    bool compatibility{};
    // Charge tree storage, including separately allocated strings and attributes.
    const auto measure = [&storage, &compatibility](const auto& self, const xml::element& element) -> void {
      compatibility = compatibility || element.qualified_name.namespace_uri == compatibility_namespace;
      storage += sizeof(xml::element) + element.qualified_name.local.size() +
          element.qualified_name.namespace_uri.size() + element.text.size();
      for (const auto& [prefix, uri] : element.namespace_bindings) {
        storage += sizeof(typename decltype(element.namespace_bindings)::value_type) + prefix.size() + uri.size();
      }
      for (const auto& item : element.attributes) {
        compatibility = compatibility || item.qualified_name.namespace_uri == compatibility_namespace;
        storage += sizeof(xml::attribute) + item.qualified_name.local.size() +
            item.qualified_name.namespace_uri.size() + item.value.size();
      }
      for (const auto& segment : element.text_segments) {
        storage += sizeof(xml::element::text_segment) + segment.value.size();
      }
      for (const auto& child : element.children) { self(self, child); }
    };
    measure(measure, document);
    shared.consume(resource::metadata_bytes, storage, name);
    if (relationships && compatibility) { relationships_compatibility{context}.apply(document); }
    return document;
  } catch (const error&) {
    throw;
  } catch (const std::exception&) {
    throw error{error_code::invalid_metadata, "Invalid OPC metadata XML", std::string{name}};
  }
}

// Validate an OPC extension default and use case-insensitive extension matching.
std::string extension_identity(std::string_view extension) {
  if (extension.empty()) {
    throw error{error_code::invalid_metadata, "Invalid OPC content-type extension"};
  }
  for (std::size_t index{}; index < extension.size(); ++index) {
    const auto byte = extension[index];
    if (byte == '%') {
      if (extension.size() - index < 3U || hex_digit(extension[index + 1U]) < 0 ||
          hex_digit(extension[index + 2U]) < 0) {
        throw error{error_code::invalid_metadata, "Invalid OPC extension percent escape"};
      }
      index += 2U;
    } else if ((byte == '.' || !unreserved(byte)) &&
               std::string_view{"!$&'()*+,:=@"}.find(byte) == std::string_view::npos) {
      throw error{error_code::invalid_metadata, "Invalid OPC content-type extension"};
    }
  }
  return identity(extension);
}

// Return a part's final extension, excluding directory names.
std::string extension_of(std::string_view name) {
  const auto dot = name.rfind('.');
  const auto slash = name.rfind('/');
  return dot == std::string_view::npos || dot < slash ? std::string{} : identity(name.substr(dot + 1));
}
} // namespace

// Adapt only selected XML operations; native binary/JSON codecs retain their existing fast paths.
xml::operation_context budget::xml_context(std::string_view part) {
  xml::operation_context result{};
  result.is_cancelled = [this] { check_cancelled(); return false; };
  result.charge = [this, source = std::string{part}](xml::operation_resource selected,
      std::size_t amount) {
    switch (selected) {
    case xml::operation_resource::allocation_bytes:
      consume(resource::decoder_allocation_bytes, amount, source); break;
    case xml::operation_resource::work_units:
      consume(resource::work_units, amount, source); break;
    case xml::operation_resource::preserved_bytes:
      consume(resource::preserved_bytes, amount, source); break;
    }
  };
  return result;
}

// Resolve a relative IRI through RFC 3986, returning the whole addressed part without its selector.
std::string resolve_target(std::string_view source, std::string_view target) {
  if (source != "/") { validate_payload_name(source); }
  validate_uri(target, false);
  target = target.substr(0, target.find_first_of("?#"));
  const auto colon = target.find(':');
  const auto slash = target.find('/');
  if ((colon != std::string_view::npos && (slash == std::string_view::npos || colon < slash)) ||
      target.starts_with("//")) {
    throw error{error_code::invalid_relationship, "Unsupported internal OPC target URI"};
  }
  std::string path;
  if (target.empty()) {
    path = source;
  } else if (target.front() == '/') {
    path = target;
  } else {
    path = std::string{source.substr(0, source.rfind('/') + 1)} + std::string{target};
  }
  std::vector<std::string_view> segments;
  for (std::size_t begin = 1; begin <= path.size();) {
    const auto end = path.find('/', begin);
    const auto segment = std::string_view{path}.substr(
        begin, end == std::string::npos ? end : end - begin);
    if (segment == "..") {
      if (!segments.empty()) { segments.pop_back(); }
    } else if (segment != ".") {
      if (segment.empty()) {
        throw error{error_code::invalid_relationship, "Empty OPC target segment"};
      }
      segments.push_back(segment);
    }
    if (end == std::string::npos) { break; }
    begin = end + 1;
  }
  std::string result;
  for (const auto segment : segments) {
    result += '/';
    result += segment;
  }
  if (path.ends_with("/.") || path.ends_with("/..")) { result += '/'; }
  validate_payload_name(result);
  return result;
}

// Check optional XML dependencies before the first package byte is written.
opc_writer::opc_writer(std::ostream& output, std::shared_ptr<budget> shared)
    : archive_{output, std::move(shared), true} { require_xml(); }

// Defaults supplement explicit overrides; duplicate declarations never silently replace each other.
void opc_writer::content_type_default(std::string_view extension, std::string_view content_type) {
  if (finished_ || failed_) { throw error{error_code::invalid_state, "OPC writer is finished or failed"}; }
  const auto key = extension_identity(extension);
  validate_content_type(content_type);
  for (const auto& item : defaults_) {
    if (item.first == key) { throw error{error_code::invalid_metadata, "Duplicate content-type default"}; }
  }
  archive_.shared_budget()->consume(resource::metadata_bytes,
      sizeof(decltype(defaults_)::value_type) + key.size() + content_type.size());
  defaults_.emplace_back(key, content_type);
}

// Keep the public part model and physical ZIP entry in lockstep; failed writes cannot finalize.
void opc_writer::add_part(std::string_view name, std::string_view content_type,
                          std::span<const std::uint8_t> bytes, entry_options options) {
  if (finished_ || failed_) { throw error{error_code::invalid_state, "OPC writer is finished or failed"}; }
  validate_payload_name(name);
  validate_payload_content_type(content_type);
  auto key = checked_identity(identities_, name);
  archive_.shared_budget()->consume(resource::metadata_bytes,
      sizeof(part) + sizeof(std::string) + name.size() + key.size() + content_type.size(), name);
  try {
    identities_.insert(std::move(key));
    parts_.push_back({std::string{name}, std::string{content_type}, bytes.size()});
    archive_.add(name.substr(1), bytes, options);
  } catch (...) { failed_ = true; throw; }
}

// Compose an existing whole-object serializer with one bounded OPC part stream.
void opc_writer::write_part(std::string_view name, std::string_view content_type,
                            const std::function<void(std::ostream&)>& producer,
                            entry_options options) {
  if (finished_ || failed_) { throw error{error_code::invalid_state, "OPC writer is finished or failed"}; }
  validate_payload_name(name);
  validate_payload_content_type(content_type);
  auto key = checked_identity(identities_, name);
  archive_.shared_budget()->consume(resource::metadata_bytes,
      sizeof(part) + sizeof(std::string) + name.size() + key.size() + content_type.size(), name);
  try {
    identities_.insert(std::move(key));
    archive_.write(name.substr(1), producer, options);
    parts_.push_back({std::string{name}, std::string{content_type}, 0});
  } catch (...) { failed_ = true; throw; }
}

// Preserve authored IDs/targets while enforcing source-local uniqueness immediately.
void opc_writer::add_relationship(relationship link) {
  if (finished_ || failed_) { throw error{error_code::invalid_state, "OPC writer is finished or failed"}; }
  validate_relationship(link);
  for (const auto& existing : relationships_) {
    if (identity(existing.source) == identity(link.source) && existing.id == link.id) {
      throw error{error_code::invalid_relationship, "Duplicate source-local relationship ID", link.source};
    }
  }
  auto& shared = *archive_.shared_budget();
  shared.consume(resource::relationships, 1, link.source);
  shared.consume(resource::metadata_bytes, sizeof(relationship) + link.source.size() +
      link.id.size() + link.type.size() + link.target.size(), link.source);
  relationships_.push_back(std::move(link));
}

// Generate standard namespace-aware infrastructure without any private Serializer envelope.
void opc_writer::finish() {
  if (failed_) { throw error{error_code::invalid_state, "Cannot finalize failed OPC writer"}; }
  if (finished_) { return; }
  try {
    validate_links(parts_, relationships_, *archive_.shared_budget());
    auto manifest = node("Types", content_types_namespace);
    for (const auto& item : defaults_) {
      auto declaration = node("Default", content_types_namespace);
      attribute(declaration, "Extension", item.first);
      attribute(declaration, "ContentType", item.second);
      manifest.children.push_back(std::move(declaration));
    }
    for (const auto& item : parts_) {
      auto declaration = node("Override", content_types_namespace);
      attribute(declaration, "PartName", item.name);
      attribute(declaration, "ContentType", item.content_type);
      manifest.children.push_back(std::move(declaration));
    }
    std::map<std::string, std::vector<const relationship*>> sets;
    for (const auto& link : relationships_) { sets[identity(link.source)].push_back(&link); }
    for (const auto& [source, links] : sets) {
      auto document = node("Relationships", relationships_namespace);
      for (const auto* link : links) {
        auto declaration = node("Relationship", relationships_namespace);
        attribute(declaration, "Id", link->id);
        attribute(declaration, "Type", link->type);
        attribute(declaration, "Target", link->target);
        if (link->mode == target_mode::external) { attribute(declaration, "TargetMode", "External"); }
        document.children.push_back(std::move(declaration));
      }
      // Use an authored source spelling so relationship names track its actual part spelling.
      const auto rel_name = relationship_name(links.front()->source);
      auto key = checked_identity(identities_, "/" + rel_name);
      archive_.shared_budget()->consume(resource::metadata_bytes,
          sizeof(std::string) + key.size(), rel_name);
      identities_.insert(std::move(key));
      write_metadata(archive_, rel_name, document);
      auto declaration = node("Override", content_types_namespace);
      attribute(declaration, "PartName", "/" + rel_name);
      attribute(declaration, "ContentType", relationships_content_type);
      manifest.children.push_back(std::move(declaration));
      static_cast<void>(source);
    }
    if (manifest.children.empty()) {
      auto declaration = node("Default", content_types_namespace);
      attribute(declaration, "Extension", "rels");
      attribute(declaration, "ContentType", relationships_content_type);
      manifest.children.push_back(std::move(declaration));
    }
    write_metadata(archive_, manifest_name, manifest);
    archive_.finish();
    finished_ = true;
  } catch (...) { failed_ = true; throw; }
}

// Return the underlying archive's cumulative operation budget.
const std::shared_ptr<budget>& opc_writer::shared_budget() const noexcept {
  return archive_.shared_budget();
}

// Read OPC manifests/rels through the shared XML provider; defer only payload decompression.
opc_reader::opc_reader(std::istream& input, std::shared_ptr<budget> shared, input_options options)
    : archive_{input, std::move(shared), std::move(options), true} {
  require_xml();
  const auto manifest = read_metadata(archive_, manifest_name);
  require_name(manifest, "Types", content_types_namespace);
  if (manifest.children.empty() || !manifest.attributes.empty() ||
      manifest.text.find_first_not_of(" \t\r\n") != std::string::npos) {
    throw error{error_code::invalid_metadata, "Invalid content-types root content"};
  }
  std::map<std::string, std::string> defaults;
  std::map<std::string, std::pair<std::string, std::string>> overrides;
  for (const auto& child : manifest.children) {
    archive_.shared_budget()->check_cancelled();
    if (child.qualified_name.local == "Default") {
      require_name(child, "Default", content_types_namespace);
      const auto values = attributes(child, {"Extension", "ContentType"});
      const auto key = extension_identity(required(values, "Extension"));
      const auto& type = required(values, "ContentType");
      validate_content_type(type);
      if (!defaults.emplace(key, type).second) {
        throw error{error_code::invalid_metadata, "Duplicate content-type default"};
      }
    } else {
      require_name(child, "Override", content_types_namespace);
      const auto values = attributes(child, {"PartName", "ContentType"});
      const auto name = xml::normalize_whitespace(required(values, "PartName"), xml::whitespace_policy::collapse);
      const auto& type = required(values, "ContentType");
      validate_part_name(name);
      validate_content_type(type);
      if (!overrides.emplace(identity(name), std::pair{name, type}).second) {
        throw error{error_code::invalid_metadata, "Duplicate content-type override"};
      }
    }
  }
  std::set<std::string, std::less<>> identities;
  std::vector<std::pair<std::string, std::string>> relationship_parts;
  for (const auto& entry : archive_.entries()) {
    if (entry.name == manifest_name) { continue; }
    const auto name = "/" + entry.name;
    if (!physical_file(entry) || !maps_to_supported_part(name)) {
      // Excluded physical items never become logical parts, but their bytes still pass every bound/CRC.
      (void)archive_.read(entry.name);
      continue;
    }
    auto key = checked_identity(identities, name);
    archive_.shared_budget()->consume(resource::metadata_bytes,
        sizeof(std::string) + key.size(), name);
    identities.insert(std::move(key));
    const auto override = overrides.find(identity(name));
    const auto fallback = defaults.find(extension_of(name));
    const auto type = override != overrides.end() ? override->second.second
        : fallback != defaults.end() ? fallback->second : std::string{};
    if (type.empty()) { throw error{error_code::invalid_metadata, "OPC part lacks a content type", name}; }
    const auto source = relationship_source(name);
    if (!source.empty()) {
      if (identity(type) != relationships_content_type) {
        throw error{error_code::invalid_metadata, "Relationship part has wrong content type", name};
      }
      relationship_parts.emplace_back(entry.name, source);
    } else {
      validate_payload_content_type(type);
      archive_.shared_budget()->consume(resource::metadata_bytes,
          sizeof(part) + name.size() + type.size(), name);
      parts_.push_back({name, type, entry.expanded_bytes});
    }
  }
  for (const auto& [key, declaration] : overrides) {
    if (!identities.contains(key)) {
      throw error{error_code::invalid_metadata, "Content-type override names an absent part",
                  declaration.first};
    }
  }
  for (const auto& [name, source] : relationship_parts) {
    if (source != "/" && !find_part(parts_, source)) {
      throw error{error_code::missing_part, "Relationship source part does not exist", source};
    }
    const auto document = read_metadata(archive_, name, true);
    require_name(document, "Relationships", relationships_namespace);
    if (!document.attributes.empty() ||
        document.text.find_first_not_of(" \t\r\n") != std::string::npos) {
      throw error{error_code::invalid_metadata, "Invalid relationships root content", name};
    }
    for (const auto& child : document.children) {
      require_name(child, "Relationship", relationships_namespace);
      const auto values = attributes(child, {"Id", "Type", "Target", "TargetMode"}, true);
      relationship link{source,
          xml::normalize_whitespace(required(values, "Id"), xml::whitespace_policy::collapse),
          xml::normalize_whitespace(required(values, "Type"), xml::whitespace_policy::collapse),
          xml::normalize_whitespace(present(values, "Target"), xml::whitespace_policy::collapse),
          target_mode::internal};
      const auto mode = values.find("TargetMode");
      if (mode != values.end()) {
        if (mode->second == "External") { link.mode = target_mode::external; }
        else if (mode->second != "Internal") {
          throw error{error_code::invalid_relationship, "Invalid relationship TargetMode", name};
        }
      }
      validate_relationship(link);
      archive_.shared_budget()->consume(resource::relationships, 1, source);
      archive_.shared_budget()->consume(resource::metadata_bytes, sizeof(relationship) +
          link.source.size() + link.id.size() + link.type.size() + link.target.size(), source);
      relationships_.push_back(std::move(link));
    }
  }
  validate_links(parts_, relationships_, *archive_.shared_budget());
}

// Return metadata for user payloads only; infrastructure stays in the archive layer.
const std::vector<part>& opc_reader::parts() const noexcept { return parts_; }

// Return authored relationship metadata without fetching external targets.
const std::vector<relationship>& opc_reader::relationships() const noexcept { return relationships_; }

// Discover typed roles without hardcoded payload filenames or implicit cardinality policy.
std::vector<const relationship*> opc_reader::find_relationships(
    std::string_view source, std::string_view type) const {
  std::vector<const relationship*> result;
  for (const auto& link : relationships_) {
    archive_.shared_budget()->check_cancelled();
    if (identity(link.source) == identity(source) && link.type == type) { result.push_back(&link); }
  }
  return result;
}

// Resolve OPC identity consistently with duplicate detection and URI target resolution.
const part* opc_reader::find(std::string_view name) const {
  validate_payload_name(name);
  return find_part(parts_, name);
}

// Decode no unrelated typed payload when the caller opens one selected part.
std::vector<std::uint8_t> opc_reader::read(std::string_view name) {
  const auto* selected = find(name);
  if (!selected) { throw error{error_code::missing_part, "OPC part is absent", std::string{name}}; }
  return archive_.read(std::string_view{selected->name}.substr(1));
}

// Let callers explicitly opt into complete archive integrity validation before publication.
void opc_reader::validate_all() { archive_.validate_all(); }

// Preserve one cumulative operation budget when a profile invokes additional typed codecs.
const std::shared_ptr<budget>& opc_reader::shared_budget() const noexcept {
  return archive_.shared_budget();
}

// Keep aggregate selection and extension handling on the archive's original budget.
profile_loader::profile_loader(opc_reader& reader, extension_policy policy)
    : reader_{reader}, policy_{std::move(policy)} {}

// Resolve one expected role, retaining precise source diagnostics across caller codec failures.
void profile_loader::read_related(std::string_view source, std::string_view relationship_type,
    std::string_view expected_content_type,
    const std::function<void(std::span<const std::uint8_t>, const part&, budget&)>& decode,
    std::size_t minimum, std::size_t maximum) {
  read_related(source, relationship_type, expected_content_type, decode, minimum, maximum, false);
}

// A declared dynamic MIME role still requires existing internal targets and validated metadata.
void profile_loader::read_related(std::string_view source, std::string_view relationship_type,
    std::string_view expected_content_type,
    const std::function<void(std::span<const std::uint8_t>, const part&, budget&)>& decode,
    std::size_t minimum, std::size_t maximum, bool allow_any_content_type) {
  if (finished_ || failed_ || minimum > maximum || !decode) {
    throw error{error_code::invalid_state, "Invalid or finished package profile"};
  }
  struct failure_guard {
    bool& failed;
    bool complete{};
    // Any unsuccessful selected-part operation permanently fences this candidate loader.
    ~failure_guard() { if (!complete) { failed = true; } }
  } guard{failed_};
  auto& shared = shared_budget();
  const auto links = reader_.find_relationships(source, relationship_type);
  if (links.size() < minimum || links.size() > maximum) {
    throw error{error_code::invalid_relationship, "Profile relationship cardinality mismatch",
                std::string{source}, 0, links.size(), maximum, {}, {},
                std::string{relationship_type}};
  }
  for (const auto* link : links) {
    shared.consume(resource::work_units, 1, source);
    if (link->mode != target_mode::internal) {
      throw error{error_code::invalid_relationship, "Typed profile requires an internal target",
                  link->source, 0, 0, 0, {}, link->target, link->id};
    }
    const auto name = resolve_target(link->source, link->target);
    const auto* selected = reader_.find(name);
    if (!selected) {
      throw error{error_code::missing_part, "Profile target is absent", name,
                  0, 0, 0, {}, link->target, link->id};
    }
    if (!allow_any_content_type && !equal_media_type(selected->content_type, expected_content_type, shared)) {
      throw error{error_code::invalid_metadata, "Profile target has an unexpected content type",
                  selected->name, 0, 0, 0, {}, {}, link->id};
    }
    if (decoded_.contains(selected->name)) {
      throw error{error_code::invalid_state, "Profile selects a typed part more than once",
                  selected->name, 0, 0, 0, {}, {}, link->id};
    }
    shared.consume(resource::metadata_bytes,
        sizeof(std::string) + selected->name.size() + sizeof(link), selected->name);
    decoded_.insert(selected->name);
    selected_.insert(link);
    const auto bytes = reader_.read(selected->name);
    try {
      decode(bytes, *selected, shared);
    } catch (const error& failure) {
      throw error{failure.code, failure.what(), failure.part.empty() ? selected->name : failure.part,
                  failure.offset_bytes, failure.required, failure.permitted, failure.field_path,
                  failure.reference, failure.relationship_id.empty() ? link->id : failure.relationship_id};
    } catch (const std::exception&) {
      throw error{error_code::codec, "Profile part decoding failed", selected->name,
                  0, 0, 0, {}, {}, link->id};
    }
    shared.check_cancelled();
  }
  guard.complete = true;
}

// Finish full aggregate validation without hiding corrupt or required unknown capabilities.
preserved_extensions profile_loader::finish() {
  if (finished_ || failed_) {
    throw error{error_code::invalid_state, "Package profile is finished or failed"};
  }
  struct failure_guard {
    bool& failed;
    bool complete{};
    // A failed integrity/preservation pass cannot be retried on the same candidate loader.
    ~failure_guard() { if (!complete) { failed = true; } }
  } guard{failed_};
  preserved_extensions extensions;
  auto& shared = shared_budget();
  for (const auto& item : reader_.parts()) {
    shared.consume(resource::work_units, 1, item.name);
    if (decoded_.contains(item.name)) { continue; }
    if ((policy_.is_required_part && policy_.is_required_part(item)) ||
        policy_.parts == unknown_policy::reject) {
      throw error{error_code::unsupported_feature, "Profile does not support this part", item.name};
    }
    // Charge retention before expanding, so a small preservation bound cannot stage a huge part.
    if (policy_.parts == unknown_policy::preserve) {
      shared.consume(resource::preserved_bytes, item.expanded_bytes, item.name);
      shared.consume(resource::metadata_bytes,
          sizeof(opaque_part) + item.name.size() + item.content_type.size(), item.name);
    }
    auto bytes = reader_.read(item.name);
    if (policy_.parts == unknown_policy::preserve) {
      extensions.parts.push_back({item, std::move(bytes), {}});
    }
  }
  for (const auto& link : reader_.relationships()) {
    shared.consume(resource::work_units, 1, link.source);
    if (selected_.contains(&link)) { continue; }
    if ((policy_.is_required_relationship && policy_.is_required_relationship(link)) ||
        policy_.relationships == unknown_policy::reject) {
      throw error{error_code::unsupported_feature, "Profile does not support this relationship",
                  link.source, 0, 0, 0, {}, link.target, link.id};
    }
    if (policy_.relationships == unknown_policy::preserve) {
      const auto bytes = sizeof(relationship) + link.source.size() + link.id.size() +
          link.type.size() + link.target.size();
      shared.consume(resource::preserved_bytes, bytes, link.source);
      shared.consume(resource::metadata_bytes, bytes, link.source);
      extensions.relationships.push_back(link);
    }
  }
  shared.check_cancelled();
  finished_ = true;
  guard.complete = true;
  return extensions;
}

// Return the single budget used by archive, metadata, typed callbacks and references.
budget& profile_loader::shared_budget() const noexcept { return *reader_.shared_budget(); }

// Retain opaque bytes and authored links without introducing a second XML representation.
void write_preserved(opc_writer& writer, const preserved_extensions& extensions) {
  for (const auto& item : extensions.parts) {
    writer.add_part(item.metadata.name, item.metadata.content_type, item.bytes, item.storage);
  }
  for (const auto& link : extensions.relationships) { writer.add_relationship(link); }
}

// Sort borrowed part pointers, bounding auxiliary storage independently of caller-owned payloads.
void write_canonical_zip(std::ostream& output, std::span<const opaque_part> parts,
                          std::shared_ptr<budget> shared) {
  if (!shared) { shared = std::make_shared<budget>(); }
  if (parts.size() > shared->available_buffer_bytes() / sizeof(const opaque_part*)) {
    throw error{error_code::resource_limit, "Canonical ZIP index exceeds scratch limit"};
  }
  shared->consume(resource::metadata_bytes, parts.size() * sizeof(const opaque_part*));
  buffer_reservation scratch{*shared, parts.size() * sizeof(const opaque_part*)};
  std::vector<const opaque_part*> ordered;
  ordered.reserve(parts.size());
  for (const auto& item : parts) { shared->check_cancelled(); ordered.push_back(&item); }
  std::sort(ordered.begin(), ordered.end(), [&](const auto* left, const auto* right) {
    shared->check_cancelled();
    return left->metadata.name < right->metadata.name;
  });
  zip_writer writer{output, shared};
  for (const auto* item : ordered) { writer.add(item->metadata.name, item->bytes, item->storage); }
  writer.finish();
}

// Canonical OPC uses fixed ZIP metadata, sorted logical names and source-local relationship IDs.
void write_canonical_opc(std::ostream& output, std::span<const opaque_part> parts,
                          std::span<const relationship> relationships,
                          std::shared_ptr<budget> shared) {
  if (!shared) { shared = std::make_shared<budget>(); }
  constexpr auto part_pointer_bytes = sizeof(const opaque_part*);
  constexpr auto link_pointer_bytes = sizeof(const relationship*);
  const auto maximum = shared->available_buffer_bytes();
  if (parts.size() > maximum / part_pointer_bytes ||
      relationships.size() > (maximum - parts.size() * part_pointer_bytes) / link_pointer_bytes) {
    throw error{error_code::resource_limit, "Canonical OPC index exceeds scratch limit"};
  }
  shared->consume(resource::metadata_bytes,
      parts.size() * part_pointer_bytes + relationships.size() * link_pointer_bytes);
  buffer_reservation scratch{*shared,
      parts.size() * part_pointer_bytes + relationships.size() * link_pointer_bytes};
  std::vector<const opaque_part*> ordered_parts;
  std::vector<const relationship*> ordered_links;
  ordered_parts.reserve(parts.size());
  ordered_links.reserve(relationships.size());
  for (const auto& item : parts) { shared->check_cancelled(); ordered_parts.push_back(&item); }
  for (const auto& link : relationships) { shared->check_cancelled(); ordered_links.push_back(&link); }
  std::sort(ordered_parts.begin(), ordered_parts.end(), [&](const auto* left, const auto* right) {
    shared->check_cancelled();
    return left->metadata.name < right->metadata.name;
  });
  std::sort(ordered_links.begin(), ordered_links.end(), [&](const auto* left, const auto* right) {
    shared->check_cancelled();
    return std::tie(left->source, left->id) < std::tie(right->source, right->id);
  });
  opc_writer writer{output, shared};
  for (const auto* item : ordered_parts) {
    writer.add_part(item->metadata.name, item->metadata.content_type, item->bytes, item->storage);
  }
  for (const auto* link : ordered_links) { writer.add_relationship(*link); }
  writer.finish();
}
} // namespace rohit::serializer::package
