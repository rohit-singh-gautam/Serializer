// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_schema.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace rohit::serializer::writer::xml_binding {
namespace {
constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};
// Borrow one unqualified XSD attribute, without conflating prefixed attributes.
std::string_view attribute(const xml::element& node, std::string_view key) {
  for (const auto& value : node.attributes) {
    if (value.qualified_name.namespace_uri.empty() && value.qualified_name.local == key) { return value.value; }
  }
  return {};
}
// Resolve QName-valued schema attributes using preserved in-scope namespace declarations.
xml::name resolve(const xml::element& node, std::string_view value) {
  const auto colon = value.find(':');
  const auto prefix = colon == std::string_view::npos ? std::string{} : std::string{value.substr(0, colon)};
  const auto found = node.namespace_bindings.find(prefix);
  return {std::string{value.substr(colon == std::string_view::npos ? 0U : colon + 1U)},
          found == node.namespace_bindings.end() ? std::string{} : found->second};
}
// Reject every construct/attribute whose semantics the generated subset cannot completely enforce.
void inspect(const xml::element& node) {
  if (node.qualified_name.namespace_uri != xsd_namespace) {
    throw std::invalid_argument{"Unsupported foreign node in optimized XSD subset"};
  }
  const auto& kind = node.qualified_name.local;
  const std::map<std::string, std::set<std::string>> accepted{
      {"schema", {"targetNamespace", "elementFormDefault", "attributeFormDefault", "version", "id"}},
      {"element", {"name", "type", "minOccurs", "maxOccurs", "form", "id"}},
      {"attribute", {"name", "type", "use", "form", "id"}},
      {"complexType", {"name", "id"}}, {"sequence", {"id"}}};
  const auto allowed = accepted.find(kind);
  if (allowed == accepted.end()) {
    throw std::invalid_argument{"Unsupported optimized XSD construct: " + kind};
  }
  for (const auto& item : node.attributes) {
    if (!item.qualified_name.namespace_uri.empty() || !allowed->second.contains(item.qualified_name.local)) {
      throw std::invalid_argument{"Unsupported optimized XSD attribute: " + item.qualified_name.local};
    }
  }
  for (const auto& child : node.children) { inspect(child); }
}
// Return whether the member has a supported scalar/record type and unambiguous presence.
void check_type(const type_name& type, bool collection_element = false) {
  if (type.is_optional()) {
    if (collection_element || type.arguments.front().is_optional() || type.arguments.front().is_sequence()) {
      throw std::invalid_argument{"XML nested optional and optional collection mappings require explicit nil/wrapper support"};
    }
    check_type(type.arguments.front());
  } else if (type.is_sequence()) {
    if (collection_element) { throw std::invalid_argument{"XML nested collections require explicit wrapper mapping"}; }
    check_type(type.arguments.front(), true);
  } else if (type.is_associative() || type.is_digest() || type.is_expected() ||
             type.application || type.type == object_type::unresolved) {
    throw std::invalid_argument{"Unsupported XML field type: " + type.name};
  } else if (type.type != object_type::class_type && type.type != object_type::enum_type &&
             type.name != "string" && type.name != "bool" &&
             type.name != "int8" && type.name != "uint8" && type.name != "int16" && type.name != "uint16" &&
             type.name != "int32" && type.name != "uint32" && type.name != "int64" && type.name != "uint64" &&
             type.name != "float" && type.name != "double" && type.name != "float32" && type.name != "float64") {
    throw std::invalid_argument{"Unsupported XML scalar type: " + type.name};
  }
}
// Parse an explicit bounded occurrence count with no permissive partial conversions.
std::size_t occurrence(std::string_view value, std::size_t default_value) {
  if (value.empty()) { return default_value; }
  std::size_t result{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
    throw std::invalid_argument{"Invalid optimized XSD occurrence bound"};
  }
  return result;
}
// Select the scalar/record leaf type beneath one optional or collection modifier.
const type_name& leaf(const member& source) {
  const auto& type = source.type_name_list.front();
  return type.is_optional() || type.is_sequence() ? type.arguments.front() : type;
}
} // namespace

void validate_options(const std::vector<std::unique_ptr<syntax_node>>& statements, const xml_options& options) {
  if (options.validation_scope != "input" && options.validation_scope != "output" && options.validation_scope != "both") {
    throw std::invalid_argument{"XML validation_scope must be input, output or both"};
  }
  if (!options.enabled && (options.runtime_xsd || !options.xsd.empty() || !options.classes.empty())) {
    throw std::invalid_argument{"XML configuration requires xml.enabled = true"};
  }
  if (options.classes.empty()) { return; }
  std::set<std::string> names{};
  const auto collect = [&](auto&& self, const auto& nodes) -> void {
    for (const auto& node : nodes) {
      if (node->type == object_type::namespace_type) {
        self(self, static_cast<const namespace_node&>(*node).statements);
      } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
        names.insert(node->get_full_name());
      }
    }
  };
  collect(collect, statements);
  for (const auto& [class_name, mapping] : options.classes) {
    (void)mapping;
    if (!names.contains(class_name)) { throw std::invalid_argument{"Unknown XML configured class: " + class_name}; }
  }
}

schema::schema(const xml_options& options) : options_{options} {
  if (options_.validation_scope != "input" && options_.validation_scope != "output" && options_.validation_scope != "both") {
    throw std::invalid_argument{"XML validation_scope must be input, output or both"};
  }
  if (!options_.enabled && (options_.runtime_xsd || !options_.xsd.empty() || !options_.classes.empty())) {
    throw std::invalid_argument{"XML configuration requires xml.enabled = true"};
  }
  if (options_.enabled && !xml::available()) {
    throw std::invalid_argument{"XML generation requires Serializer built with SERIALIZER_WITH_XML=ON"};
  }
  if (options_.xsd.empty()) { return; }
  std::ifstream input{options_.xsd, std::ios::binary};
  if (!input) { throw std::invalid_argument{"Cannot open XML schema: " + options_.xsd.string()}; }
  input.seekg(0, std::ios::end);
  const auto size = input.tellg();
  constexpr std::size_t maximum_schema_bytes{16U * 1024U * 1024U};
  if (size < 0 || static_cast<std::uintmax_t>(size) > maximum_schema_bytes) {
    throw std::length_error{"XSD build input exceeds schema byte limit"};
  }
  input.seekg(0);
  std::string bytes(static_cast<std::size_t>(size), '\0');
  if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
    throw std::invalid_argument{"Cannot read XML schema"};
  }
  // Compile once to reject malformed XSD/QNames, then bind only fully supported optimized constructs.
  const xml::prepared_schema prepared{bytes};
  document_ = xml::parse(bytes);
  inspect(*document_);
}

record schema::bind(const class_node& source) const {
  if (!source.parents.empty() || source.supports_managed() || source.multiple_modes() ||
      !source.generic_parameters.empty() || source.version_member() || source.has_magic()) {
    throw std::invalid_argument{"XML generation currently requires ordinary owning records without inheritance, generics, magic or revision lifecycles"};
  }
  const auto configured = options_.classes.find(source.get_full_name());
  const xml_class_options defaults{};
  const auto& mapping = configured == options_.classes.end() ? defaults : configured->second;
  record result{{mapping.root.empty() ? source.name : mapping.root, mapping.namespace_uri}, {}};
  std::set<std::string> member_names{};
  for (const auto& source_field : source.member_list) {
    if (source_field.omits("xml")) { continue; }
    if (source_field.modifier == member::modifier_type::map || source_field.modifier == member::modifier_type::variant ||
        source_field.managed || source_field.wire_only) {
      throw std::invalid_argument{"Unsupported XML map, variant, managed or derived-size field: " + source_field.name};
    }
    check_type(source_field.type_name_list.front(), source_field.modifier == member::modifier_type::array);
    const auto renamed = mapping.names.find(source_field.name);
    const bool is_attribute = std::find(mapping.attributes.begin(), mapping.attributes.end(), source_field.name) != mapping.attributes.end();
    const bool repeated = source_field.modifier == member::modifier_type::array || source_field.type_name_list.front().is_sequence();
    if (is_attribute && (repeated || leaf(source_field).type == object_type::class_type)) {
      throw std::invalid_argument{"XML attributes require scalar fields: " + source_field.name};
    }
    result.fields.push_back({&source_field,
        {renamed == mapping.names.end() ? source_field.name : renamed->second, is_attribute ? "" : mapping.namespace_uri},
        is_attribute, source_field.type_name_list.front().is_optional() || repeated ? 0U : 1U,
        repeated ? std::optional<std::size_t>{} : std::optional<std::size_t>{1U}, repeated});
    member_names.insert(source_field.name);
  }
  for (const auto& key : mapping.attributes) {
    if (!member_names.contains(key)) { throw std::invalid_argument{"Unknown XML attribute member: " + key}; }
  }
  for (const auto& [key, value] : mapping.names) {
    (void)value;
    if (!member_names.contains(key)) { throw std::invalid_argument{"Unknown XML renamed member: " + key}; }
  }
  if (document_) {
    const auto target = std::string{attribute(*document_, "targetNamespace")};
    const xml::element* declaration{};
    for (const auto& node : document_->children) {
      if (node.qualified_name.local == "element" && attribute(node, "name") == result.root.local) {
        declaration = &node; break;
      }
    }
    if (!declaration) { throw std::invalid_argument{"No XSD root matches Serializer class: " + source.get_full_name()}; }
    if (!mapping.namespace_uri.empty() && mapping.namespace_uri != target) {
      throw std::invalid_argument{"Configured XML namespace conflicts with XSD"};
    }
    result.root.namespace_uri = target;
    const xml::element* complex{};
    for (const auto& node : declaration->children) {
      if (node.qualified_name.local == "complexType") { complex = &node; }
    }
    if (!complex) {
      const auto type = resolve(*declaration, attribute(*declaration, "type"));
      for (const auto& node : document_->children) {
        if (node.qualified_name.local == "complexType" && attribute(node, "name") == type.local &&
            type.namespace_uri == target) { complex = &node; break; }
      }
    }
    if (!complex) { throw std::invalid_argument{"Optimized XSD roots require a declared complexType"}; }
    std::vector<field> ordered{};
    std::set<const member*> bound{};
    const auto bind_field = [&](const xml::element& node, bool is_attribute) {
      const auto local = attribute(node, "name");
      const auto candidate = std::find_if(result.fields.begin(), result.fields.end(), [&](const auto& value) {
        return value.qualified_name.local == local;
      });
      if (candidate == result.fields.end() || !bound.insert(candidate->source).second) {
        throw std::invalid_argument{"XSD member binding is missing or ambiguous: " + std::string{local}};
      }
      auto field = *candidate;
      if (std::find(mapping.attributes.begin(), mapping.attributes.end(), field.source->name) != mapping.attributes.end() && !is_attribute) {
        throw std::invalid_argument{"Configured XML attribute conflicts with XSD"};
      }
      field.attribute = is_attribute;
      const auto form = attribute(node, "form");
      const bool qualified = form == "qualified" ||
          (form.empty() && attribute(*document_, is_attribute ? "attributeFormDefault" : "elementFormDefault") == "qualified");
      field.qualified_name.namespace_uri = qualified ? target : "";
      const auto declared_type = resolve(node, attribute(node, "type"));
      const auto& source_type = leaf(*field.source);
      const std::map<std::string, std::string> primitive_types{{"string", "string"}, {"boolean", "bool"},
          {"byte", "int8"}, {"unsignedByte", "uint8"}, {"short", "int16"}, {"unsignedShort", "uint16"},
          {"int", "int32"}, {"unsignedInt", "uint32"}, {"long", "int64"}, {"unsignedLong", "uint64"},
          {"float", "float"}, {"double", "double"}};
      const auto primitive = primitive_types.find(declared_type.local);
      if (declared_type.namespace_uri != xsd_namespace || primitive == primitive_types.end() ||
          (source_type.name != primitive->second &&
           !(source_type.name == "float32" && primitive->second == "float") &&
           !(source_type.name == "float64" && primitive->second == "double"))) {
        throw std::invalid_argument{"Optimized XSD field type is unsupported or differs from Serializer: " + field.source->name};
      }
      if (is_attribute) {
        if (field.repeated) { throw std::invalid_argument{"XSD attributes cannot bind collections"}; }
        const auto use = attribute(node, "use");
        if (use == "prohibited") { throw std::invalid_argument{"Prohibited XSD attribute cannot bind a stored field"}; }
        field.min_occurs = use == "required" ? 1U : 0U;
        field.max_occurs = 1U;
      } else {
        field.min_occurs = occurrence(attribute(node, "minOccurs"), 1U);
        const auto maximum = attribute(node, "maxOccurs");
        field.max_occurs = maximum == "unbounded" ? std::optional<std::size_t>{} :
            std::optional<std::size_t>{occurrence(maximum, 1U)};
      }
      const bool optional = field.source->type_name_list.front().is_optional();
      if (!field.repeated && (field.max_occurs != std::optional<std::size_t>{1U} || field.min_occurs != (optional ? 0U : 1U))) {
        throw std::invalid_argument{"XSD presence/cardinality differs from Serializer member: " + field.source->name};
      }
      ordered.push_back(std::move(field));
    };
    for (const auto& node : complex->children) {
      if (node.qualified_name.local == "sequence") {
        for (const auto& child : node.children) {
          if (child.qualified_name.local != "element") { throw std::invalid_argument{"Unsupported optimized XSD sequence"}; }
          bind_field(child, false);
        }
      } else if (node.qualified_name.local == "attribute") { bind_field(node, true); }
      else { throw std::invalid_argument{"Unsupported optimized XSD complexType child"}; }
    }
    if (bound.size() != result.fields.size()) { throw std::invalid_argument{"Serializer member is absent from XSD contract"}; }
    result.fields = std::move(ordered);
  }
  std::set<std::pair<bool, std::pair<std::string, std::string>>> names{};
  const auto check_namespace = [](std::string_view uri) {
    for (const auto character : uri) {
      const auto byte = static_cast<unsigned char>(character);
      if (byte <= 0x20U || byte == 0x7fU || std::string_view{"\"<>^`{}|\\"}.find(character) != std::string_view::npos) {
        throw std::invalid_argument{"XML namespaces require URI/IRI references with escaped reserved characters"};
      }
    }
  };
  check_namespace(result.root.namespace_uri);
  for (const auto& value : result.fields) {
    check_namespace(value.qualified_name.namespace_uri);
    if (!names.emplace(value.attribute, std::pair{value.qualified_name.namespace_uri, value.qualified_name.local}).second) {
      throw std::invalid_argument{"Ambiguous XML member mapping"};
    }
  }
  // Reuse writer name checks at generation time, including empty records.
  xml::element check{};
  check.qualified_name = result.root;
  for (const auto& value : result.fields) {
    if (value.attribute) { check.attributes.push_back({value.qualified_name, ""}); }
    else { xml::element child{}; child.qualified_name = value.qualified_name; check.children.push_back(std::move(child)); }
  }
  (void)xml::write(check);
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
