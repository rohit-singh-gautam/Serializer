// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_container_contract.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace rohit::serializer::writer::xml_binding {
namespace {
constexpr std::string_view xsi_namespace{"http://www.w3.org/2001/XMLSchema-instance"};
constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};
constexpr std::string_view xml_whitespace{" \t\r\n"};

// Validate the local-name grammar through the existing provider-independent XML writer.
void validate_name(const xml::name& value, std::string_view role) {
  if (value.local.empty()) {
    throw std::invalid_argument{"XML " + std::string{role} + " requires an explicit name"};
  }
  xml::element check{};
  check.qualified_name = value;
  (void)xml::write(check);
}

// Reject contract values that would silently flatten unsupported nested owning containers.
void validate_value_type(const type_name& value, std::string_view role) {
  if (value.is_optional()) {
    throw std::invalid_argument{"XML " + std::string{role} +
        " optional values require an explicit per-value presence contract"};
  }
  if (value.application || value.is_digest() || value.is_managed_reference()) {
    throw std::invalid_argument{"XML " + std::string{role} +
        " requires a supported scalar, enum or owning record"};
  }
  if (value.type == object_type::class_type || value.type == object_type::enum_type) { return; }
  static const std::set<std::string_view> scalars{
      "bool", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64",
      "float", "double", "string"};
  if (value.type != object_type::primitive || !scalars.contains(value.name)) {
    throw std::invalid_argument{"Unsupported XML " + std::string{role} + " type: " + value.name};
  }
}

// Remove only XML whitespace at numeric/boolean boundaries; string keys remain byte-exact.
std::string_view trim_xml_whitespace(std::string_view value) {
  const auto first = value.find_first_not_of(xml_whitespace);
  if (first == std::string_view::npos) { return {}; }
  return value.substr(first, value.find_last_not_of(xml_whitespace) - first + 1U);
}

// Return the declared fixed-width integer range without depending on the host's native long width.
unsigned integer_width(const type_name& value) {
  if (value.name.ends_with("8")) { return 8U; }
  if (value.name.ends_with("16")) { return 16U; }
  if (value.name.ends_with("32")) { return 32U; }
  if (value.name.ends_with("64")) { return 64U; }
  throw std::invalid_argument{"Unsupported XML map integer key width"};
}

// Reject absence/nil metadata for required map entries, keys, values and variant payloads.
void reject_nil(const xml::element& value, std::string_view role,
    xml::operation_context* context = nullptr) {
  for (const auto& attribute : value.attributes) {
    if (context) { context->consume(xml::operation_resource::work_units, 1U); }
    if (attribute.qualified_name != xml::name{"nil", std::string{xsi_namespace}}) { continue; }
    const auto lexical = trim_xml_whitespace(attribute.value);
    if (lexical == "true" || lexical == "1") {
      throw std::invalid_argument{"XML " + std::string{role} + " cannot be nil"};
    }
    if (lexical != "false" && lexical != "0") {
      throw std::invalid_argument{"Invalid XML xsi:nil lexical value"};
    }
  }
}

// Require structural wrappers to contain child elements and formatting whitespace only.
void validate_wrapper_content(const xml::element& value, std::string_view role,
    xml::operation_context* context = nullptr) {
  if (context) { context->consume(xml::operation_resource::work_units, value.text.size()); }
  if (value.text.find_first_not_of(xml_whitespace) != std::string::npos) {
    throw std::invalid_argument{"XML " + std::string{role} + " cannot contain scalar text"};
  }
  for (const auto& segment : value.text_segments) {
    if (context) {
      context->consume(xml::operation_resource::work_units, 1U);
      context->consume(xml::operation_resource::work_units, segment.value.size());
    }
    if (segment.value.find_first_not_of(xml_whitespace) != std::string::npos) {
      throw std::invalid_argument{"XML " + std::string{role} + " cannot contain mixed text"};
    }
  }
  reject_nil(value, role, context);
}

// Preserve normalized scalar text identity when a tree client retains explicit text segments.
void validate_scalar_content(const xml::element& value, xml::operation_context* context) {
  if (!value.children.empty()) {
    throw std::invalid_argument{"XML map scalar cannot contain child elements"};
  }
  std::size_t offset{};
  for (const auto& segment : value.text_segments) {
    if (context) {
      context->consume(xml::operation_resource::work_units, 1U);
      context->consume(xml::operation_resource::work_units, segment.value.size());
    }
    if (segment.before_child != 0U || offset > value.text.size() ||
        !std::string_view{value.text}.substr(offset).starts_with(segment.value)) {
      throw std::invalid_argument{"Invalid XML map scalar text segment"};
    }
    offset += segment.value.size();
  }
  if (!value.text_segments.empty() && offset != value.text.size()) {
    throw std::invalid_argument{"Incomplete XML map scalar text segments"};
  }
}

// Derive deterministic type QNames from the exact native schema identity, never output naming style.
xml::name native_xml_type(const type_name& value) {
  static const std::map<std::string_view, std::string_view> primitive_types{
      {"bool", "boolean"}, {"int8", "byte"}, {"uint8", "unsignedByte"},
      {"int16", "short"}, {"uint16", "unsignedShort"}, {"int32", "int"},
      {"uint32", "unsignedInt"}, {"int64", "long"}, {"uint64", "unsignedLong"},
      {"float", "float"}, {"double", "double"}, {"string", "string"}};
  if (value.type == object_type::primitive) {
    const auto found = primitive_types.find(value.name);
    if (found == primitive_types.end()) {
      throw std::invalid_argument{"Unsupported XML variant scalar type: " + value.name};
    }
    return {std::string{found->second}, std::string{xsd_namespace}};
  }
  const auto full_name = value.get_full_name();
  const auto separator = full_name.rfind("::");
  return {separator == std::string::npos ? full_name : full_name.substr(separator + 2U),
      "urn:serializer:native:" +
          (separator == std::string::npos ? std::string{} : full_name.substr(0U, separator))};
}

// Provide a sortable expanded-name identity without delimiter collisions in namespace URI strings.
auto name_identity(const xml::name& value) {
  return std::pair{value.namespace_uri, value.local};
}
} // namespace

// Resolve native container syntax once, then validate every key/value placement before publication.
map_contract bind_map_contract(const member& source, const std::optional<xml::name>& wrapper,
    const xml::name& entry, const xml::name& key, const xml::name& value,
    map_key_placement key_placement) {
  if (!wrapper) { throw std::invalid_argument{"XML map requires an explicit collection wrapper"}; }
  validate_name(*wrapper, "map wrapper");
  validate_name(entry, "map entry");
  validate_name(key, "map key");
  validate_name(value, "map value");
  if (key_placement == map_key_placement::attribute &&
      ((key.namespace_uri.empty() && key.local == "xmlns") ||
       key.namespace_uri == "http://www.w3.org/2000/xmlns/" ||
       key.namespace_uri == xsi_namespace)) {
    throw std::invalid_argument{"XML map keys cannot replace namespace or xsi metadata attributes"};
  }
  if (key_placement == map_key_placement::element && key == value) {
    throw std::invalid_argument{"XML map key and value child names must be distinct"};
  }
  map_contract result{};
  result.wrapper = *wrapper;
  result.entry = entry;
  result.key = key;
  result.value = value;
  result.key_placement = key_placement;
  if (source.type_name_list.size() != 1U) {
    throw std::invalid_argument{"XML map field requires one native map value type"};
  }
  if (source.modifier == member::modifier_type::map) {
    result.value_type = source.type_name_list.front();
    if (source.key_type) {
      result.key_type = *source.key_type;
    } else {
      result.key_type = type_name{std::string{source.key}, nullptr};
      result.key_type.type = source.key_node ? source.key_node->type : object_type::primitive;
      result.key_type.resolved_node = source.key_node;
    }
  } else {
    const auto* native_map = &source.type_name_list.front();
    if (native_map->is_optional()) {
      if (native_map->arguments.size() != 1U) {
        throw std::invalid_argument{"Malformed optional XML map type"};
      }
      result.optional_container = true;
      native_map = &native_map->arguments.front();
    }
    if (source.modifier != member::modifier_type::none || !native_map->is_associative() ||
        native_map->arguments.size() != 2U) {
      throw std::invalid_argument{"XML map mapping requires a native map field"};
    }
    result.key_type = native_map->arguments.front();
    result.value_type = native_map->arguments.back();
  }
  validate_value_type(result.key_type, "map key");
  validate_value_type(result.value_type, "map value");
  if (result.key_type.type == object_type::enum_type && result.key_type.resolved_node) {
    result.key_kind = map_key_kind::enumeration;
    result.key_enumerators = static_cast<const enum_node&>(*result.key_type.resolved_node).enum_name_list;
  } else if (result.key_type.type != object_type::primitive) {
    throw std::invalid_argument{"XML map keys require bool, fixed-width integer, string or enum identity"};
  } else if (result.key_type.name == "bool") {
    result.key_kind = map_key_kind::boolean;
  } else if (result.key_type.name == "string") {
    result.key_kind = map_key_kind::string;
  } else if (result.key_type.name.starts_with("uint")) {
    result.key_kind = map_key_kind::unsigned_integer;
  } else if (result.key_type.name.starts_with("int")) {
    result.key_kind = map_key_kind::signed_integer;
  } else {
    throw std::invalid_argument{"XML map keys require bool, fixed-width integer, string or enum identity"};
  }
  return result;
}

// Convert all accepted lexical aliases into exactly one key for native duplicate detection.
std::string canonical_map_key(const map_contract& contract, std::string_view lexical) {
  if (contract.key_kind == map_key_kind::string) { return std::string{lexical}; }
  if (contract.key_kind == map_key_kind::enumeration) {
    if (std::find(contract.key_enumerators.begin(), contract.key_enumerators.end(), lexical) ==
        contract.key_enumerators.end()) {
      throw std::invalid_argument{"Unknown XML map enum key"};
    }
    return std::string{lexical};
  }
  lexical = trim_xml_whitespace(lexical);
  if (contract.key_kind == map_key_kind::boolean) {
    if (lexical == "true" || lexical == "1") { return "true"; }
    if (lexical == "false" || lexical == "0") { return "false"; }
    throw std::invalid_argument{"Invalid XML map boolean key"};
  }
  const bool negative = !lexical.empty() && lexical.front() == '-';
  if (!lexical.empty() && (lexical.front() == '+' || negative)) { lexical.remove_prefix(1U); }
  if (lexical.empty() || lexical.find_first_not_of("0123456789") != std::string_view::npos) {
    throw std::invalid_argument{"Invalid XML map integer key"};
  }
  std::uint64_t magnitude{};
  const auto converted = std::from_chars(lexical.data(), lexical.data() + lexical.size(), magnitude);
  if (converted.ec != std::errc{} || converted.ptr != lexical.data() + lexical.size()) {
    throw std::out_of_range{"XML map integer key exceeds native range"};
  }
  const auto width = integer_width(contract.key_type);
  if (contract.key_kind == map_key_kind::unsigned_integer) {
    const auto maximum = std::numeric_limits<std::uint64_t>::max() >>
        (std::numeric_limits<std::uint64_t>::digits - width);
    if ((negative && magnitude != 0U) || magnitude > maximum) {
      throw std::out_of_range{"XML map unsigned key exceeds native range"};
    }
  } else {
    const auto minimum_magnitude = std::uint64_t{1U} << (width - 1U);
    if (magnitude > minimum_magnitude || (!negative && magnitude == minimum_magnitude)) {
      throw std::out_of_range{"XML map signed key exceeds native range"};
    }
  }
  return (negative && magnitude != 0U ? "-" : "") + std::to_string(magnitude);
}

// Check each entry before retaining its semantic key, with cumulative caller-selected limits.
void validate_map_entries(const map_contract& contract, const xml::element& wrapper,
    const map_validation_limits& limits, xml::operation_context* context) {
  if (context) { context->check(); }
  if (wrapper.qualified_name != contract.wrapper) {
    throw std::invalid_argument{"Unexpected XML map wrapper expanded name"};
  }
  validate_wrapper_content(wrapper, "map wrapper", context);
  if (!wrapper.attributes.empty()) { throw std::invalid_argument{"Unexpected XML map wrapper attribute"}; }
  if (wrapper.children.size() > limits.max_entries) { throw std::length_error{"XML map entry limit exceeded"}; }
  std::set<std::string> keys{};
  std::size_t retained_key_bytes{};
  for (const auto& entry : wrapper.children) {
    if (context) { context->consume(xml::operation_resource::work_units, 1U); }
    if (entry.qualified_name != contract.entry) { throw std::invalid_argument{"Unexpected XML map entry"}; }
    validate_wrapper_content(entry, "map entry", context);
    const xml::element* key_element{};
    const xml::element* value_element{};
    std::optional<std::string_view> lexical{};
    for (const auto& attribute : entry.attributes) {
      if (context) { context->consume(xml::operation_resource::work_units, 1U); }
      if (contract.key_placement != map_key_placement::attribute || attribute.qualified_name != contract.key || lexical) {
        throw std::invalid_argument{"Unexpected or duplicate XML map entry attribute"};
      }
      lexical = attribute.value;
    }
    for (const auto& child : entry.children) {
      if (context) { context->consume(xml::operation_resource::work_units, 1U); }
      if (contract.key_placement == map_key_placement::element && child.qualified_name == contract.key) {
        if (key_element) { throw std::invalid_argument{"Duplicate XML map entry key"}; }
        key_element = &child;
      } else if (child.qualified_name == contract.value) {
        if (value_element) { throw std::invalid_argument{"Duplicate XML map entry value"}; }
        value_element = &child;
      } else { throw std::invalid_argument{"Unexpected XML map entry child"}; }
    }
    if (key_element) {
      reject_nil(*key_element, "map key", context);
      if (!key_element->attributes.empty()) {
        throw std::invalid_argument{"XML map key requires scalar text only"};
      }
      validate_scalar_content(*key_element, context);
      lexical = key_element->text;
    }
    if (!lexical || !value_element) {
      throw std::invalid_argument{"XML map entry requires exactly one present key and value"};
    }
    reject_nil(*value_element, "map value", context);
    if (contract.value_type.type != object_type::class_type) {
      validate_scalar_content(*value_element, context);
    }
    if (lexical->size() > limits.max_key_bytes - retained_key_bytes) {
      throw std::length_error{"XML map retained key byte limit exceeded"};
    }
    if (context) {
      context->consume(xml::operation_resource::work_units, lexical->size());
      // Boolean aliases may expand from one byte to the canonical words true/false.
      constexpr std::size_t maximum_boolean_key_bytes{std::string_view{"false"}.size()};
      const auto allocation_bytes = contract.key_kind == map_key_kind::boolean
          ? std::max(lexical->size(), maximum_boolean_key_bytes) : lexical->size();
      context->consume(xml::operation_resource::allocation_bytes, allocation_bytes);
      context->consume(xml::operation_resource::allocation_bytes, sizeof(std::string));
    }
    auto canonical = canonical_map_key(contract, *lexical);
    const auto key_bytes = std::max(lexical->size(), canonical.size());
    if (key_bytes > limits.max_key_bytes - retained_key_bytes) {
      throw std::length_error{"XML map retained key byte limit exceeded"};
    }
    retained_key_bytes += key_bytes;
    if (!keys.insert(std::move(canonical)).second) {
      throw std::invalid_argument{"Duplicate XML map key by native value identity"};
    }
  }
}

// Bind tag inventories against native alternative aliases and reject every ambiguous identity.
variant_contract bind_variant_contract(const member& source, const std::optional<xml::name>& wrapper,
    variant_selection selection, const std::vector<variant_tag_override>& overrides) {
  if (!wrapper) { throw std::invalid_argument{"XML variant requires an explicit payload wrapper"}; }
  validate_name(*wrapper, "variant wrapper");
  if (source.modifier != member::modifier_type::variant || source.type_name_list.empty()) {
    throw std::invalid_argument{"XML variant mapping requires a native union or owning variant"};
  }
  std::map<std::string, const variant_tag_override*> configured{};
  for (const auto& override : overrides) {
    if (!configured.emplace(override.alternative, &override).second) {
      throw std::invalid_argument{"Duplicate XML variant tag override: " + override.alternative};
    }
  }
  variant_contract result{*wrapper, selection, {}};
  std::set<std::string> aliases{};
  std::set<std::pair<std::string, std::string>> tags{}, type_identities{};
  for (const auto& native_type : source.type_name_list) {
    validate_value_type(native_type, "variant alternative");
    if (native_type.enum_name.empty() || !aliases.insert(native_type.enum_name).second) {
      throw std::invalid_argument{"XML variant alternative aliases must be nonempty and unique"};
    }
    variant_alternative alternative{};
    alternative.alias = native_type.enum_name;
    alternative.native_identity = native_type.get_full_name();
    alternative.native_type = native_type;
    alternative.type_identity = native_xml_type(native_type);
    const auto found = configured.find(alternative.alias);
    if (selection == variant_selection::xsi_type && found == configured.end()) {
      throw std::invalid_argument{"XML xsi:type variant requires a complete explicit tag inventory"};
    }
    alternative.tag = {alternative.alias, wrapper->namespace_uri};
    if (found != configured.end()) {
      // A type-only override retains the stable element alias; xsi:type still needs explicit tags.
      if (!found->second->tag.local.empty() || !found->second->tag.namespace_uri.empty() ||
          selection == variant_selection::xsi_type) {
        alternative.tag = found->second->tag;
      }
      if (found->second->type_identity) { alternative.type_identity = *found->second->type_identity; }
      if (selection == variant_selection::xsi_type) {
        if (found->second->type_identity && alternative.tag != alternative.type_identity) {
          throw std::invalid_argument{"Conflicting XML variant tag and xsi:type identity"};
        }
        alternative.type_identity = alternative.tag;
      }
      configured.erase(found);
    }
    validate_name(alternative.tag, "variant tag");
    validate_name(alternative.type_identity, "variant type identity");
    if (!tags.insert(name_identity(alternative.tag)).second ||
        (selection == variant_selection::xsi_type &&
            !type_identities.insert(name_identity(alternative.type_identity)).second)) {
      throw std::invalid_argument{"Ambiguous XML variant tag or native/type identity"};
    }
    result.alternatives.push_back(std::move(alternative));
  }
  if (!configured.empty()) {
    throw std::invalid_argument{"Unknown XML variant alternative alias: " + configured.begin()->first};
  }
  return result;
}

// Validate the structural selection transaction before exposing an alternative index to a decoder.
std::size_t select_variant_alternative(const variant_contract& contract, const xml::element& wrapper,
    xml::operation_context* context) {
  if (context) { context->consume(xml::operation_resource::work_units, 1U); }
  if (wrapper.qualified_name != contract.wrapper) {
    throw std::invalid_argument{"Unexpected XML variant wrapper expanded name"};
  }
  validate_wrapper_content(wrapper, "variant wrapper", context);
  if (!wrapper.attributes.empty() || wrapper.children.size() != 1U) {
    throw std::invalid_argument{"XML variant wrapper requires exactly one payload child"};
  }
  const auto& payload = wrapper.children.front();
  reject_nil(payload, "variant payload", context);
  std::optional<xml::name> xsi_type{};
  for (const auto& attribute : payload.attributes) {
    if (context) { context->consume(xml::operation_resource::work_units, 1U); }
    if (attribute.qualified_name == xml::name{"type", std::string{xsi_namespace}}) {
      if (xsi_type) { throw std::invalid_argument{"Duplicate XML variant xsi:type"}; }
      if (context) {
        context->consume(xml::operation_resource::work_units, attribute.value.size());
        context->consume(xml::operation_resource::allocation_bytes, attribute.value.size());
        const auto lexical = trim_xml_whitespace(attribute.value);
        const auto colon = lexical.find(':');
        const auto prefix = colon == std::string_view::npos ? std::string_view{} : lexical.substr(0U, colon);
        if (prefix == "xml") {
          constexpr std::string_view xml_namespace{"http://www.w3.org/XML/1998/namespace"};
          context->consume(xml::operation_resource::allocation_bytes, xml_namespace.size());
        } else {
          for (const auto& [binding_prefix, uri] : payload.namespace_bindings) {
            context->consume(xml::operation_resource::work_units, 1U);
            if (binding_prefix == prefix) {
              context->consume(xml::operation_resource::allocation_bytes, uri.size());
              break;
            }
          }
        }
      }
      xsi_type = xml::resolve_qname(attribute.value, payload);
    }
  }
  std::optional<std::size_t> selected{};
  for (std::size_t index{}; index < contract.alternatives.size(); ++index) {
    if (context) { context->consume(xml::operation_resource::work_units, 1U); }
    const auto& alternative = contract.alternatives[index];
    const bool match = contract.selection == variant_selection::element
        ? payload.qualified_name == alternative.tag
        : xsi_type && *xsi_type == alternative.type_identity;
    if (!match) { continue; }
    if (selected) { throw std::invalid_argument{"Ambiguous XML variant payload selection"}; }
    if (xsi_type && *xsi_type != alternative.type_identity) {
      throw std::invalid_argument{"Conflicting XML variant element tag and xsi:type"};
    }
    // xsi:type mode uses the field wrapper's expanded name for its single typed payload.
    if (contract.selection == variant_selection::xsi_type && payload.qualified_name != contract.wrapper) {
      throw std::invalid_argument{"Conflicting XML xsi:type variant payload element name"};
    }
    selected = index;
  }
  if (!selected) { throw std::invalid_argument{"Unknown or missing XML variant tag/type identity"}; }
  return *selected;
}

} // namespace rohit::serializer::writer::xml_binding
