// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#include <rohit/core_properties.hpp>
#include <rohit/xml.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>

namespace rohit::serializer::package {
namespace {
constexpr std::string_view core_namespace =
    "http://schemas.openxmlformats.org/package/2006/metadata/core-properties";
constexpr std::string_view dc_namespace = "http://purl.org/dc/elements/1.1/";
constexpr std::string_view terms_namespace = "http://purl.org/dc/terms/";
constexpr std::string_view instance_namespace = "http://www.w3.org/2001/XMLSchema-instance";
constexpr std::string_view xml_namespace = "http://www.w3.org/XML/1998/namespace";
enum class date_kind { none, w3cdtf, schema_datetime };
struct property {
  std::string_view local;
  std::string_view uri;
  std::optional<std::string> core_properties::*member;
  date_kind date{date_kind::none};
};
constexpr std::array properties{
    property{"category", core_namespace, &core_properties::category},
    property{"contentStatus", core_namespace, &core_properties::content_status},
    property{"created", terms_namespace, &core_properties::created, date_kind::w3cdtf},
    property{"creator", dc_namespace, &core_properties::creator},
    property{"description", dc_namespace, &core_properties::description},
    property{"identifier", dc_namespace, &core_properties::identifier},
    property{"keywords", core_namespace, nullptr},
    property{"language", dc_namespace, &core_properties::language},
    property{"lastModifiedBy", core_namespace, &core_properties::last_modified_by},
    property{"lastPrinted", core_namespace, &core_properties::last_printed, date_kind::schema_datetime},
    property{"modified", terms_namespace, &core_properties::modified, date_kind::w3cdtf},
    property{"revision", core_namespace, &core_properties::revision},
    property{"subject", dc_namespace, &core_properties::subject},
    property{"title", dc_namespace, &core_properties::title},
    property{"version", core_namespace, &core_properties::version}};

// Keep invalid metadata bounded and located without including application property values.
[[noreturn]] void invalid(std::string_view part_name, std::string_view field, const char* message) {
  throw error{error_code::invalid_metadata, message, std::string{part_name}, 0, 0, 0,
      std::string{field}};
}

// Read a fixed-width decimal component without indexing before its complete extent is checked.
bool digits(std::string_view text, std::size_t offset, std::size_t count, unsigned& value) {
  if (offset > text.size() || count > text.size() - offset) { return false; }
  value = 0U;
  for (const auto character : text.substr(offset, count)) {
    if (character < '0' || character > '9') { return false; }
    value = value * 10U + static_cast<unsigned>(character - '0');
  }
  return true;
}

// Check a terminal XML Schema timezone, including absence and offsets only through 14:00.
bool timezone(std::string_view text, std::size_t offset) {
  if (offset == text.size()) { return true; }
  const auto suffix = text.substr(offset);
  if (suffix == "Z") { return true; }
  unsigned hours{}, minutes{};
  return suffix.size() == 6U && (suffix.front() == '+' || suffix.front() == '-') &&
      suffix[3] == ':' && digits(suffix, 1U, 2U, hours) && digits(suffix, 4U, 2U, minutes) &&
      minutes < 60U && hours <= 14U && (hours != 14U || minutes == 0U);
}

// Apply the imported W3CDTF schema's gYear/gYearMonth/date/dateTime union, or dateTime alone.
// Extended Schema years are reduced modulo 400 for leap-year checks, avoiding integer overflow.
bool valid_date(std::string_view text, date_kind kind) {
  const bool full_datetime = kind == date_kind::schema_datetime;
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) { return false; }
  text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1U);
  const std::size_t year_start = text.starts_with('-') ? 1U : 0U;
  std::size_t year_end = year_start;
  unsigned year_modulo{};
  bool nonzero_year{};
  while (year_end < text.size() && text[year_end] >= '0' && text[year_end] <= '9') {
    const auto digit = static_cast<unsigned>(text[year_end++] - '0');
    year_modulo = (year_modulo * 10U + digit) % 400U;
    nonzero_year = nonzero_year || digit != 0U;
  }
  const auto year_digits = year_end - year_start;
  if (year_digits < 4U || !nonzero_year ||
      (year_digits > 4U && text[year_start] == '0')) { return false; }
  if (timezone(text, year_end)) { return !full_datetime; }
  if (year_end == text.size()) { return false; }
  unsigned month{}, day{};
  if (text[year_end] != '-' || !digits(text, year_end + 1U, 2U, month) || month == 0U || month > 12U) {
    return false;
  }
  const auto month_end = year_end + 3U;
  if (timezone(text, month_end)) { return !full_datetime; }
  if (month_end == text.size()) { return false; }
  if (text[month_end] != '-' || !digits(text, month_end + 1U, 2U, day)) { return false; }
  constexpr std::array month_days{31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
  const bool leap_year = year_modulo % 4U == 0U &&
      (year_modulo % 100U != 0U || year_modulo == 0U);
  const auto maximum_day = month_days[month - 1U] + (month == 2U && leap_year ? 1U : 0U);
  if (day == 0U || day > maximum_day) { return false; }
  const auto date_end = month_end + 3U;
  if (timezone(text, date_end)) { return !full_datetime; }
  if (date_end == text.size()) { return false; }
  unsigned hour{}, minute{}, second{};
  if (text[date_end] != 'T' || !digits(text, date_end + 1U, 2U, hour) ||
      date_end + 3U >= text.size() || text[date_end + 3U] != ':' ||
      !digits(text, date_end + 4U, 2U, minute) || minute >= 60U ||
      hour > 24U) { return false; }
  std::size_t offset = date_end + 6U;
  const bool seconds = offset < text.size() && text[offset] == ':';
  if (!seconds) { return false; }
  if (seconds) {
    if (!digits(text, offset + 1U, 2U, second) || second >= 60U) { return false; }
    offset += 3U;
  }
  bool nonzero_fraction{};
  if (seconds && offset < text.size() && text[offset] == '.') {
    const auto fraction_start = ++offset;
    while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9') {
      nonzero_fraction = nonzero_fraction || text[offset] != '0';
      ++offset;
    }
    if (offset == fraction_start) { return false; }
  }
  return (hour != 24U || (minute == 0U && second == 0U && !nonzero_fraction)) &&
      timezone(text, offset);
}

// Derive XML limits from unspent caller capacity rather than reset the package's counters.
xml::limits bounds(const budget& shared) {
  const auto maximum_bytes = static_cast<std::size_t>(std::min({shared.configured().max_part_bytes,
      shared.available_buffer_bytes(),
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())}));
  const auto maximum_tree_bytes = static_cast<std::size_t>(std::min({
      shared.available_buffer_bytes(), shared.remaining(resource::decoder_allocation_bytes),
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())}));
  xml::limits result{};
  result.max_input_bytes = maximum_bytes;
  result.max_output_bytes = maximum_bytes;
  result.max_text_bytes = maximum_bytes;
  result.max_tree_bytes = maximum_tree_bytes;
  result.max_depth = 3U;
  result.max_nodes = maximum_tree_bytes / sizeof(xml::element);
  result.max_attributes = maximum_tree_bytes / sizeof(xml::attribute);
  result.max_mixed_text_bytes = maximum_bytes;
  result.max_mixed_fragments = maximum_tree_bytes / sizeof(xml::element::text_segment);
  return result;
}

// Admit manually assembled output-tree storage before copying any application text.
void charge_node(xml::operation_context& context, const property& selected, std::string_view text) {
  context.consume(xml::operation_resource::allocation_bytes, sizeof(xml::element));
  context.consume(xml::operation_resource::allocation_bytes, selected.local.size());
  context.consume(xml::operation_resource::allocation_bytes, selected.uri.size());
  context.consume(xml::operation_resource::allocation_bytes, text.size());
}

// Check the XML language datatype, retaining its permitted surrounding whitespace spelling.
bool valid_language(std::string_view value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) { return false; }
  value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1U);
  std::size_t segment_size{};
  bool primary = true;
  for (const auto character : value) {
    if (character == '-') {
      if (segment_size == 0U) { return false; }
      primary = false;
      segment_size = 0U;
      continue;
    }
    const bool letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    if ((!letter && (primary || character < '0' || character > '9')) || ++segment_size > 8U) {
      return false;
    }
  }
  return segment_size != 0U;
}

// Admit and add the sole optional attribute allowed on keyword/value elements.
void write_language(xml::element& node, const std::optional<std::string>& language,
    xml::operation_context& context, std::string_view part_name) {
  if (!language) { return; }
  context.consume(xml::operation_resource::work_units, language->size());
  if (!valid_language(*language)) { invalid(part_name, "keywords", "Invalid keyword language value"); }
  context.consume(xml::operation_resource::allocation_bytes,
      sizeof(xml::attribute) + xml_namespace.size() + std::string_view{"lang"}.size());
  context.consume(xml::operation_resource::allocation_bytes, language->size());
  node.attributes.push_back({{"lang", std::string{xml_namespace}}, *language});
}

// Retain the XML namespace's language attribute and reject additional attributes or duplicates.
std::optional<std::string> read_language(const xml::element& node, xml::operation_context& context,
    std::string_view part_name) {
  if (node.attributes.empty()) { return {}; }
  if (node.attributes.size() != 1U || node.attributes.front().qualified_name !=
      xml::name{"lang", std::string{xml_namespace}}) {
    invalid(part_name, "keywords", "Unexpected keyword attribute");
  }
  context.consume(xml::operation_resource::work_units, node.attributes.front().value.size());
  if (!valid_language(node.attributes.front().value)) {
    invalid(part_name, "keywords", "Invalid keyword language value");
  }
  context.consume(xml::operation_resource::allocation_bytes, node.attributes.front().value.size());
  return node.attributes.front().value;
}

// Materialize ordered mixed keywords using the existing raw XML tree's child-relative text positions.
xml::element encode_keywords(const core_property_keywords& value, xml::operation_context& context,
    std::string_view part_name) {
  const property selected{"keywords", core_namespace, nullptr};
  charge_node(context, selected, {});
  xml::element result{};
  result.qualified_name = {"keywords", std::string{core_namespace}};
  write_language(result, value.language, context, part_name);
  for (const auto& fragment : value.content) {
    context.check();
    if (const auto* text = std::get_if<std::string>(&fragment)) {
      context.consume(xml::operation_resource::allocation_bytes, sizeof(xml::element::text_segment));
      context.consume(xml::operation_resource::allocation_bytes, text->size());
      context.consume(xml::operation_resource::allocation_bytes, text->size());
      result.text_segments.push_back({result.children.size(), *text});
      result.text += *text;
    } else {
      const auto& keyword = std::get<core_property_keyword>(fragment);
      const property item{"value", core_namespace, nullptr};
      charge_node(context, item, keyword.value);
      xml::element child{};
      child.qualified_name = {"value", std::string{core_namespace}};
      child.text = keyword.value;
      write_language(child, keyword.language, context, part_name);
      result.children.push_back(std::move(child));
    }
  }
  return result;
}

// Decode mixed keywords without concatenating localized values or losing interleaving text.
core_property_keywords decode_keywords(const xml::element& node, xml::operation_context& context,
    std::string_view part_name) {
  core_property_keywords result{};
  result.language = read_language(node, context, part_name);
  auto segment = node.text_segments.begin();
  for (std::size_t index{}; index <= node.children.size(); ++index) {
    context.check();
    while (segment != node.text_segments.end() && segment->before_child == index) {
      context.consume(xml::operation_resource::allocation_bytes, sizeof(decltype(result.content)::value_type));
      context.consume(xml::operation_resource::allocation_bytes, segment->value.size());
      result.content.emplace_back(segment->value);
      ++segment;
    }
    if (index == node.children.size()) { break; }
    const auto& child = node.children[index];
    if (child.qualified_name != xml::name{"value", std::string{core_namespace}} || !child.children.empty()) {
      invalid(part_name, "keywords", "Unexpected keyword child");
    }
    context.consume(xml::operation_resource::allocation_bytes, sizeof(decltype(result.content)::value_type));
    context.consume(xml::operation_resource::allocation_bytes, child.text.size());
    result.content.emplace_back(core_property_keyword{child.text, read_language(child, context, part_name)});
  }
  if (node.text_segments.empty() && !node.text.empty()) {
    context.consume(xml::operation_resource::allocation_bytes, sizeof(decltype(result.content)::value_type));
    context.consume(xml::operation_resource::allocation_bytes, node.text.size());
    result.content.emplace_back(node.text);
  }
  return result;
}

// Reject dates outside the selected lexical contract before XML or archive publication.
void validate_value(const property& selected, std::string_view text, budget& shared,
    std::string_view part_name) {
  shared.consume_at(resource::work_units, text.size(), {part_name, selected.local});
  if (selected.date != date_kind::none && !valid_date(text, selected.date)) {
    invalid(part_name, selected.local, "Invalid core property date value");
  }
}
} // namespace

// Construct standard expanded identities; XML emission supplies escaping and namespace declarations.
std::string encode_core_properties(const core_properties& value, budget& shared, std::string_view part_name) {
  if (!xml::available()) { throw error{error_code::unavailable, "Core properties require XML support"}; }
  auto context = shared.xml_context(part_name);
  context.allow_mixed_content = true;
  const auto selected_bounds = bounds(shared);
  shared.check_cancelled();
  const property root_property{"coreProperties", core_namespace, nullptr};
  charge_node(context, root_property, {});
  xml::element root{};
  root.qualified_name = {std::string{root_property.local}, std::string{core_namespace}};
  for (const auto& selected : properties) {
    if (!selected.member) {
      if (value.keywords) { root.children.push_back(encode_keywords(*value.keywords, context, part_name)); }
      continue;
    }
    const auto& text = value.*selected.member;
    if (!text) { continue; }
    validate_value(selected, *text, shared, part_name);
    charge_node(context, selected, *text);
    xml::element child{};
    child.qualified_name = {std::string{selected.local}, std::string{selected.uri}};
    child.text = *text;
    if (selected.date == date_kind::w3cdtf) {
      constexpr std::string_view prefix = "dcterms";
      constexpr std::string_view qualified_type = "dcterms:W3CDTF";
      context.consume(xml::operation_resource::allocation_bytes, sizeof(xml::attribute) +
          instance_namespace.size() + std::string_view{"type"}.size() + qualified_type.size());
      context.consume(xml::operation_resource::allocation_bytes,
          sizeof(decltype(child.namespace_bindings)::value_type) + prefix.size() + terms_namespace.size());
      child.namespace_bindings.emplace(prefix, terms_namespace);
      child.attributes.push_back({{"type", std::string{instance_namespace}}, std::string{qualified_type}});
    }
    root.children.push_back(std::move(child));
  }
  return xml::write(root, selected_bounds, &context);
}

// Resolve type QNames in each property's own scope, preserving prefix-equivalent independent input.
core_properties decode_core_properties(std::string_view bytes, budget& shared, std::string_view part_name) {
  if (!xml::available()) { throw error{error_code::unavailable, "Core properties require XML support"}; }
  auto context = shared.xml_context(part_name);
  context.input_encoding = xml::input_encoding_policy::utf8_or_utf16;
  context.allow_mixed_content = true;
  const auto root = xml::parse(bytes, bounds(shared), &context);
  if (root.qualified_name != xml::name{"coreProperties", std::string{core_namespace}} ||
      !root.attributes.empty() || root.text.find_first_not_of(" \t\r\n") != std::string::npos) {
    invalid(part_name, {}, "Invalid core properties root");
  }
  core_properties result{};
  for (const auto& child : root.children) {
    shared.check_cancelled();
    const auto found = std::find_if(properties.begin(), properties.end(), [&](const auto& selected) {
      return child.qualified_name.local == selected.local && child.qualified_name.namespace_uri == selected.uri;
    });
    if (found == properties.end()) {
      invalid(part_name, child.qualified_name.local, "Unknown or nested core property");
    }
    if (!found->member) {
      if (result.keywords) { invalid(part_name, found->local, "Duplicate core property"); }
      result.keywords = decode_keywords(child, context, part_name);
      continue;
    }
    if (!child.children.empty()) { invalid(part_name, found->local, "Nested core property"); }
    auto& destination = result.*found->member;
    if (destination) { invalid(part_name, found->local, "Duplicate core property"); }
    if (found->date == date_kind::w3cdtf) {
      if (child.attributes.size() != 1U || child.attributes.front().qualified_name !=
          xml::name{"type", std::string{instance_namespace}}) {
        invalid(part_name, found->local, "Core property date requires only xsi:type");
      }
      if (xml::resolve_qname(child.attributes.front().value, child) !=
          xml::name{"W3CDTF", std::string{terms_namespace}}) {
        invalid(part_name, found->local, "Core property date requires the W3CDTF expanded type");
      }
    } else if (!child.attributes.empty()) {
      invalid(part_name, found->local, "Unexpected core property attribute");
    }
    validate_value(*found, child.text, shared, part_name);
    context.consume(xml::operation_resource::allocation_bytes, child.text.size());
    destination = child.text;
  }
  return result;
}

// Finish property validation and XML construction before adding the part to a fallible stream writer.
void write_core_properties(opc_writer& writer, const core_properties& value, std::string_view part_name,
    std::string_view relationship_id, entry_options storage) {
  const auto bytes = encode_core_properties(value, *writer.shared_budget(), part_name);
  writer.add_part(part_name, core_properties_content_type,
      {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}, storage);
  writer.add_relationship({"/", std::string{relationship_id}, std::string{core_properties_relationship_type},
      std::string{part_name}});
}

// Reuse the profile loader's mode/type/cardinality checks and cumulative decoding context.
std::optional<core_properties> read_core_properties(profile_loader& loader) {
  std::optional<core_properties> result{};
  loader.read_related("/", core_properties_relationship_type, core_properties_content_type,
      [&](std::span<const std::uint8_t> bytes, const part& metadata, budget& shared) {
        result = decode_core_properties(
            {reinterpret_cast<const char*>(bytes.data()), bytes.size()}, shared, metadata.name);
      }, 0U, 1U);
  return result;
}
} // namespace rohit::serializer::package
