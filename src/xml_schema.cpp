// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_schema.hpp"
#include <rohit/digest.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace rohit::serializer::writer::xml_binding {
// Preserve each fixed input's namespace/form defaults and exact content for bundle identity.
struct schema_bundle {
  struct document {
    std::filesystem::path path{};
    std::string bytes{};
    std::string target{};
    xml::element root{};
  };
  std::vector<document> documents{};
  std::string identity{};
};
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
  if (value.empty() || colon == 0U || (colon != std::string_view::npos &&
      (colon + 1U == value.size() || found == node.namespace_bindings.end() ||
       value.find(':', colon + 1U) != std::string_view::npos))) {
    throw std::invalid_argument{"Invalid or unbound XSD QName: " + std::string{value}};
  }
  xml::name result{std::string{value.substr(colon == std::string_view::npos ? 0U : colon + 1U)},
                   found == node.namespace_bindings.end() ? std::string{} : found->second};
  xml::element check{}; check.qualified_name = {result.local, ""};
  (void)xml::write(check);
  return result;
}
// Reject every construct/attribute whose semantics the generated subset cannot completely enforce.
void inspect(const xml::element& node) {
  if (node.qualified_name.namespace_uri != xsd_namespace) {
    throw std::invalid_argument{"Unsupported foreign node in optimized XSD subset"};
  }
  const auto& kind = node.qualified_name.local;
  if (kind == "annotation") { return; }
  if (kind == "schema") {
    for (const auto* key : {"elementFormDefault", "attributeFormDefault"}) {
      const auto form = attribute(node, key);
      if (!form.empty() && form != "qualified" && form != "unqualified") {
        throw std::invalid_argument{"Invalid XSD qualification default"};
      }
    }
  }
  const std::map<std::string, std::set<std::string>> accepted{
      {"schema", {"targetNamespace", "elementFormDefault", "attributeFormDefault", "version", "id"}},
      {"include", {"schemaLocation", "id"}}, {"import", {"namespace", "schemaLocation", "id"}},
      {"element", {"name", "type", "ref", "minOccurs", "maxOccurs", "form", "id"}},
      {"attribute", {"name", "type", "ref", "use", "form", "id"}},
      {"simpleType", {"name", "id"}}, {"restriction", {"base", "id"}},
      {"complexType", {"name", "id"}}, {"sequence", {"id"}}};
  const auto allowed = accepted.find(kind);
  if (allowed == accepted.end()) {
    throw std::invalid_argument{"Unsupported optimized XSD construct: " + kind};
  }
  for (const auto& item : node.attributes) {
    if (!item.qualified_name.namespace_uri.empty() || !allowed->second.contains(item.qualified_name.local)) {
      throw std::invalid_argument{"Unsupported optimized XSD attribute: " + item.qualified_name.local};
    }
    if (item.qualified_name.local == "name") {
      xml::element check{}; check.qualified_name = {item.value, ""};
      (void)xml::write(check);
    }
    if (item.qualified_name.local == "form" && item.value != "qualified" && item.value != "unqualified") {
      throw std::invalid_argument{"Invalid XSD declaration qualification"};
    }
  }
  for (const auto& child : node.children) { inspect(child); }
}
// Replace or append one unqualified attribute during closed-schema projection and export.
void set_attribute(xml::element& node, std::string_view key, std::string value) {
  for (auto& item : node.attributes) {
    if (item.qualified_name.namespace_uri.empty() && item.qualified_name.local == key) {
      item.value = std::move(value); return;
    }
  }
  node.attributes.push_back({{std::string{key}, ""}, std::move(value)});
}
// Resolve a unique global declaration by original namespace and local name.
std::pair<const schema_bundle::document*, const xml::element*> declaration(
    const schema_bundle& bundle, std::string_view kind, const xml::name& name) {
  std::pair<const schema_bundle::document*, const xml::element*> result{};
  for (const auto& document : bundle.documents) {
    if (document.target != name.namespace_uri) { continue; }
    for (const auto& node : document.root.children) {
      if (node.qualified_name.local == kind && attribute(node, "name") == name.local) {
        if (result.second) { throw std::invalid_argument{"Duplicate XSD declaration: " + name.local}; }
        result = {&document, &node};
      }
    }
  }
  if (!result.second) { throw std::invalid_argument{"Unresolved XSD " + std::string{kind} + ": " + name.local}; }
  return result;
}
// Select a root's inline or named complex type while retaining the defining document's defaults.
std::pair<const schema_bundle::document*, const xml::element*> complex_type(
    const schema_bundle& bundle, const schema_bundle::document& document, const xml::element& node) {
  for (const auto& child : node.children) {
    if (child.qualified_name.local == "complexType") { return {&document, &child}; }
  }
  return declaration(bundle, "complexType", resolve(node, attribute(node, "type")));
}
// Read one fixed input with an aggregate byte budget checked before allocating its contents.
std::string read_schema(const std::filesystem::path& path, std::size_t& remaining_bytes) {
  std::ifstream input{path, std::ios::binary};
  if (!input) { throw std::invalid_argument{"Cannot open XML schema: " + path.string()}; }
  input.seekg(0, std::ios::end);
  const auto size = input.tellg();
  if (size < 0 || static_cast<std::uintmax_t>(size) > remaining_bytes) {
    throw std::length_error{"XSD bundle exceeds aggregate schema byte limit"};
  }
  input.seekg(0);
  std::string bytes(static_cast<std::size_t>(size), '\0');
  if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
    throw std::invalid_argument{"Cannot read XML schema: " + path.string()};
  }
  remaining_bytes -= bytes.size();
  return bytes;
}
// Compile a closed projection of all supported declarations, without installing any external loader.
void validate_bundle(const schema_bundle& bundle) {
  constexpr std::string_view closed_namespace{"urn:serializer:closed-xsd-bundle"};
  xml::element closed{};
  closed.qualified_name = {"schema", std::string{xsd_namespace}};
  closed.namespace_bindings = {{"xs", std::string{xsd_namespace}}, {"b", std::string{closed_namespace}}};
  set_attribute(closed, "targetNamespace", std::string{closed_namespace});
  std::map<std::string, std::string> prefixes{};
  prefixes.emplace("", "");
  for (const auto& document : bundle.documents) { prefixes.emplace(document.target, ""); }
  std::size_t next_prefix{};
  for (auto& [uri, prefix] : prefixes) { (void)uri; prefix = "s" + std::to_string(next_prefix++) + "_"; }
  const auto rewrite = [&](auto&& self, const schema_bundle::document& document,
                           const xml::element& source, bool global) -> xml::element {
    auto node = source;
    node.children.clear();
    if (node.qualified_name.local == "annotation") { return node; }
    node.namespace_bindings["xs"] = std::string{xsd_namespace};
    node.namespace_bindings["b"] = std::string{closed_namespace};
    if (global && !attribute(source, "name").empty()) {
      set_attribute(node, "name", prefixes.at(document.target) + std::string{attribute(source, "name")});
    }
    for (const auto* key : {"type", "base", "ref"}) {
      if (attribute(source, key).empty()) { continue; }
      const auto name = resolve(source, attribute(source, key));
      if (name.namespace_uri == xsd_namespace) { set_attribute(node, key, "xs:" + name.local); }
      else {
        const bool imported = std::any_of(document.root.children.begin(), document.root.children.end(), [&](const auto& child) {
          return child.qualified_name.local == "import" && attribute(child, "namespace") == name.namespace_uri;
        });
        if (name.namespace_uri != document.target && !imported) {
          throw std::invalid_argument{"XSD QName namespace requires an explicit import in its source document"};
        }
        const auto prefix = prefixes.find(name.namespace_uri);
        if (prefix == prefixes.end()) { throw std::invalid_argument{"XSD QName references an unbundled namespace"}; }
        set_attribute(node, key, "b:" + prefix->second + name.local);
      }
    }
    const bool is_attribute = source.qualified_name.local == "attribute";
    if (!global && (is_attribute || source.qualified_name.local == "element") &&
        attribute(source, "ref").empty()) {
      if (attribute(source, "name").empty()) { throw std::invalid_argument{"XSD local declarations require a name or ref"}; }
      auto form = attribute(source, "form");
      if (form.empty()) {
        form = attribute(document.root, is_attribute ? "attributeFormDefault" : "elementFormDefault");
      }
      const auto& uri = form == "qualified" ? document.target : std::string{};
      // Rename local identities as well as globals, preserving particle collisions across refs.
      set_attribute(node, "name", prefixes.at(uri) + std::string{attribute(source, "name")});
      set_attribute(node, "form", "qualified");
    }
    for (const auto& child : source.children) { node.children.push_back(self(self, document, child, false)); }
    return node;
  };
  for (const auto& document : bundle.documents) {
    for (const auto& node : document.root.children) {
      const auto& kind = node.qualified_name.local;
      if (kind == "import" || kind == "include" || kind == "annotation") { continue; }
      closed.children.push_back(rewrite(rewrite, document, node, true));
    }
  }
  // Only global identities change; grammar, declaration links, and primitive types stay intact.
  const xml::prepared_schema prepared{xml::write(closed)};
}
// Encode identity paths as UTF-8 on every host, independently of the Windows active code page.
std::string relative_path_text(const std::filesystem::path& path,
                               const std::filesystem::path& root) {
  const auto utf8 = path.lexically_relative(root).generic_u8string();
  return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}
// Load only a bounded relative local graph, then hash sorted relative paths and exact bytes.
std::shared_ptr<const schema_bundle> load_bundle(const std::filesystem::path& path) {
  auto result = std::make_shared<schema_bundle>();
  const auto root_path = std::filesystem::canonical(path);
  const auto root_directory = root_path.parent_path();
  constexpr std::size_t maximum_schema_bytes{16U * 1024U * 1024U};
  constexpr std::size_t maximum_schema_documents{64U};
  constexpr std::size_t maximum_schema_depth{32U};
  std::size_t remaining_bytes{maximum_schema_bytes};
  std::map<std::filesystem::path, std::string> loaded{};
  std::set<std::filesystem::path> active{};
  const auto load = [&](auto&& self, const std::filesystem::path& file,
                        std::string_view required_namespace, std::size_t depth) -> void {
    if (active.contains(file)) { throw std::invalid_argument{"Cyclic XSD bundle references are unsupported"}; }
    if (const auto found = loaded.find(file); found != loaded.end()) {
      if (found->second != required_namespace) { throw std::invalid_argument{"XSD document has conflicting bundle namespaces"}; }
      return;
    }
    if (depth > maximum_schema_depth || result->documents.size() >= maximum_schema_documents) {
      throw std::length_error{"XSD bundle document/depth limit exceeded"};
    }
    auto bytes = read_schema(file, remaining_bytes);
    auto tree = xml::parse(bytes);
    inspect(tree);
    if (tree.qualified_name.local != "schema") { throw std::invalid_argument{"XSD root must be xs:schema"}; }
    const auto target = std::string{attribute(tree, "targetNamespace")};
    if (depth != 0U && target != required_namespace) {
      throw std::invalid_argument{"XSD include/import targetNamespace mismatch; chameleon includes are unsupported"};
    }
    loaded.emplace(file, target);
    active.insert(file);
    result->documents.push_back({file, std::move(bytes), target, std::move(tree)});
    // Recursive loads relocate the document vector, so keep an independent small reference list.
    const auto references = result->documents.back().root.children;
    for (const auto& reference : references) {
      const auto& kind = reference.qualified_name.local;
      if (kind != "include" && kind != "import") { continue; }
      const auto location = attribute(reference, "schemaLocation");
      if (location.empty() || location.find_first_of(":\\?#%") != std::string_view::npos) {
        throw std::invalid_argument{"XSD references require explicit relative local schemaLocation paths"};
      }
      const std::filesystem::path relative{std::string{location}};
      if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
        throw std::invalid_argument{"Absolute XSD schemaLocation is prohibited"};
      }
      const auto child = std::filesystem::canonical(file.parent_path() / relative);
      const auto within = child.lexically_relative(root_directory);
      if (within.empty() || within.is_absolute() || *within.begin() == "..") {
        throw std::invalid_argument{"XSD schemaLocation escapes its pinned bundle directory"};
      }
      const auto imported_namespace = kind == "include" ? target : std::string{attribute(reference, "namespace")};
      if (kind == "import" && imported_namespace == target) {
        throw std::invalid_argument{"XSD import must select another namespace"};
      }
      self(self, child, imported_namespace, depth + 1U);
    }
    active.erase(file);
  };
  load(load, root_path, {}, 0U);
  std::sort(result->documents.begin(), result->documents.end(), [&](const auto& first, const auto& second) {
    return relative_path_text(first.path, root_directory) < relative_path_text(second.path, root_directory);
  });
  std::string identity_input{"Serializer XSD bundle 1\n"};
  for (const auto& document : result->documents) {
    const auto relative = relative_path_text(document.path, root_directory);
    // Length prefixes distinguish arbitrary path/content delimiters without relying on escaping.
    identity_input += std::to_string(relative.size()) + ":" + relative;
    identity_input += std::to_string(document.bytes.size()) + ":" + document.bytes;
  }
  result->identity = rohit::digest_to_hex(rohit::make_digest<rohit::digest_algorithm::sha256>(identity_input));
  validate_bundle(*result);
  return result;
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
  if (!options_.bundle_sha256.empty() && (options_.xsd.empty() ||
      options_.bundle_sha256.size() != 64U ||
      options_.bundle_sha256.find_first_not_of("0123456789abcdef") != std::string::npos)) {
    throw std::invalid_argument{"XML bundle_sha256 requires an XSD and 64 lowercase hexadecimal digits"};
  }
  if (options_.xsd.empty()) { return; }
  bundle_ = load_bundle(options_.xsd);
  if (!options_.bundle_sha256.empty() && options_.bundle_sha256 != bundle_->identity) {
    throw std::invalid_argument{"XML XSD bundle identity mismatch: expected " + options_.bundle_sha256 +
                                ", actual " + bundle_->identity};
  }
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
    const bool text = source_field.name == mapping.text;
    if (is_attribute && (repeated || leaf(source_field).type == object_type::class_type || text)) {
      throw std::invalid_argument{"XML attributes require scalar fields: " + source_field.name};
    }
    if (text && (repeated || leaf(source_field).type == object_type::class_type ||
        source_field.type_name_list.front().is_optional())) {
      throw std::invalid_argument{"XML text requires one nonoptional scalar field: " + source_field.name};
    }
    const auto wrapper = mapping.wrappers.find(source_field.name);
    if (wrapper != mapping.wrappers.end() && (!repeated || is_attribute || text || wrapper->second.empty())) {
      throw std::invalid_argument{"XML wrappers require repeated element fields and a nonempty wrapper name"};
    }
    result.fields.push_back({&source_field,
        {renamed == mapping.names.end() ? source_field.name : renamed->second, is_attribute ? "" : mapping.namespace_uri},
        is_attribute, source_field.type_name_list.front().is_optional() || repeated ? 0U : 1U,
        repeated ? std::optional<std::size_t>{} : std::optional<std::size_t>{1U}, repeated, text, {}});
    if (wrapper != mapping.wrappers.end()) {
      result.fields.back().wrapper = xml::name{wrapper->second, mapping.namespace_uri};
    }
    member_names.insert(source_field.name);
  }
  for (const auto& key : mapping.attributes) {
    if (!member_names.contains(key)) { throw std::invalid_argument{"Unknown XML attribute member: " + key}; }
  }
  if (!mapping.text.empty() && !member_names.contains(mapping.text)) {
    throw std::invalid_argument{"Unknown XML text member: " + mapping.text};
  }
  if (!mapping.text.empty() && std::any_of(result.fields.begin(), result.fields.end(), [](const auto& value) {
      return !value.attribute && !value.text;
    })) { throw std::invalid_argument{"XML text with child elements requires ordered mixed-content support"}; }
  for (const auto& [key, value] : mapping.wrappers) {
    (void)value;
    if (!member_names.contains(key)) { throw std::invalid_argument{"Unknown XML wrapped member: " + key}; }
  }
  for (const auto& [key, value] : mapping.names) {
    (void)value;
    if (!member_names.contains(key)) { throw std::invalid_argument{"Unknown XML renamed member: " + key}; }
  }
  if (bundle_) {
    if (!mapping.text.empty() || !mapping.wrappers.empty()) {
      throw std::invalid_argument{"Explicit text/wrapper mappings cannot override a fixed XSD contract"};
    }
    const schema_bundle::document* root_document{};
    const xml::element* root_declaration{};
    for (const auto& document : bundle_->documents) {
      if (!mapping.namespace_uri.empty() && mapping.namespace_uri != document.target) { continue; }
      for (const auto& node : document.root.children) {
        if (node.qualified_name.local == "element" && attribute(node, "name") == result.root.local) {
          if (root_declaration) { throw std::invalid_argument{"Ambiguous XSD root; configure its namespace"}; }
          root_document = &document; root_declaration = &node;
        }
      }
    }
    if (!root_declaration) { throw std::invalid_argument{"No XSD root matches Serializer class: " + source.get_full_name()}; }
    const auto& target = root_document->target;
    result.root.namespace_uri = target;
    const auto [complex_document, complex] = complex_type(*bundle_, *root_document, *root_declaration);
    std::vector<field> ordered{};
    std::set<const member*> bound{};
    const auto bind_field = [&](const xml::element& occurrence_node, bool is_attribute) {
      auto* owner = complex_document;
      auto* selected = &occurrence_node;
      const bool referenced = !attribute(*selected, "ref").empty();
      if (referenced) {
        const auto found = declaration(*bundle_, is_attribute ? "attribute" : "element",
                                       resolve(*selected, attribute(*selected, "ref")));
        owner = found.first; selected = found.second;
      }
      const auto& node = *selected;
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
      const bool qualified = referenced || form == "qualified" ||
          (form.empty() && attribute(owner->root, is_attribute ? "attributeFormDefault" : "elementFormDefault") == "qualified");
      field.qualified_name.namespace_uri = qualified ? owner->target : "";
      const auto& source_type = leaf(*field.source);
      const std::map<std::string, std::string> primitive_types{{"string", "string"}, {"boolean", "bool"},
          {"byte", "int8"}, {"unsignedByte", "uint8"}, {"short", "int16"}, {"unsignedShort", "uint16"},
          {"int", "int32"}, {"unsignedInt", "uint32"}, {"long", "int64"}, {"unsignedLong", "uint64"},
          {"float", "float"}, {"double", "double"}};
      if (source_type.type == object_type::class_type) {
        if (is_attribute || !source_type.resolved_node) { throw std::invalid_argument{"XSD records require element bindings"}; }
        const auto& nested = static_cast<const class_node&>(*source_type.resolved_node);
        const auto nested_configured = options_.classes.find(nested.get_full_name());
        const auto& nested_mapping = nested_configured == options_.classes.end() ? defaults : nested_configured->second;
        const auto [nested_owner, nested_root] = declaration(*bundle_, "element",
            {nested_mapping.root.empty() ? nested.name : nested_mapping.root,
             nested_mapping.namespace_uri.empty() ? target : nested_mapping.namespace_uri});
        if (complex_type(*bundle_, *owner, node).second != complex_type(*bundle_, *nested_owner, *nested_root).second) {
          throw std::invalid_argument{"Nested XSD record must share its globally declared complex type: " + field.source->name};
        }
      } else {
        auto declared_type = resolve(node, attribute(node, "type"));
        std::set<std::pair<std::string, std::string>> aliases{};
        while (declared_type.namespace_uri != xsd_namespace) {
          if (!aliases.emplace(declared_type.namespace_uri, declared_type.local).second) {
            throw std::invalid_argument{"Cyclic XSD simple type restriction"};
          }
          const auto [simple_owner, simple] = declaration(*bundle_, "simpleType", declared_type);
          (void)simple_owner;
          const xml::element* restriction{};
          for (const auto& child : simple->children) {
            if (child.qualified_name.local == "restriction") { restriction = &child; }
          }
          if (!restriction) { throw std::invalid_argument{"XSD simple aliases require a restriction base"}; }
          declared_type = resolve(*restriction, attribute(*restriction, "base"));
        }
        const auto primitive = primitive_types.find(declared_type.local);
        if (primitive == primitive_types.end() ||
            (source_type.name != primitive->second &&
             !(source_type.name == "float32" && primitive->second == "float") &&
             !(source_type.name == "float64" && primitive->second == "double"))) {
          throw std::invalid_argument{"Optimized XSD field type is unsupported or differs from Serializer: " + field.source->name};
        }
      }
      if (is_attribute) {
        if (field.repeated) { throw std::invalid_argument{"XSD attributes cannot bind collections"}; }
        const auto use = attribute(occurrence_node, "use");
        if (use == "prohibited") { throw std::invalid_argument{"Prohibited XSD attribute cannot bind a stored field"}; }
        field.min_occurs = use == "required" ? 1U : 0U;
        field.max_occurs = 1U;
      } else {
        field.min_occurs = occurrence(attribute(occurrence_node, "minOccurs"), 1U);
        const auto maximum = attribute(occurrence_node, "maxOccurs");
        field.max_occurs = maximum == "unbounded" ? std::optional<std::size_t>{} :
            std::optional<std::size_t>{occurrence(maximum, 1U)};
      }
      if (field.max_occurs && field.min_occurs > *field.max_occurs) {
        throw std::invalid_argument{"XSD occurrence interval is empty"};
      }
      const bool optional = field.source->type_name_list.front().is_optional();
      if (!field.repeated && (field.max_occurs != std::optional<std::size_t>{1U} || field.min_occurs != (optional ? 0U : 1U))) {
        throw std::invalid_argument{"XSD presence/cardinality differs from Serializer member: " + field.source->name};
      }
      ordered.push_back(std::move(field));
    };
    const auto bind_sequence = [&](auto&& self, const xml::element& sequence) -> void {
      for (const auto& child : sequence.children) {
        if (child.qualified_name.local == "element") { bind_field(child, false); }
        else if (child.qualified_name.local == "sequence") { self(self, child); }
        else if (child.qualified_name.local != "annotation") { throw std::invalid_argument{"Unsupported optimized XSD sequence"}; }
      }
    };
    for (const auto& node : complex->children) {
      if (node.qualified_name.local == "sequence") { bind_sequence(bind_sequence, node); }
      else if (node.qualified_name.local == "attribute") { bind_field(node, true); }
      else if (node.qualified_name.local != "annotation") { throw std::invalid_argument{"Unsupported optimized XSD complexType child"}; }
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
    if (value.text) { continue; }
    const auto& name = value.wrapper ? *value.wrapper : value.qualified_name;
    if (!names.emplace(value.attribute, std::pair{name.namespace_uri, name.local}).second) {
      throw std::invalid_argument{"Ambiguous XML member mapping"};
    }
  }
  // Reuse writer name checks at generation time, including empty records.
  xml::element check{};
  check.qualified_name = result.root;
  for (const auto& value : result.fields) {
    if (value.text) { continue; }
    if (value.attribute) { check.attributes.push_back({value.qualified_name, ""}); }
    else {
      xml::element child{}; child.qualified_name = value.wrapper ? *value.wrapper : value.qualified_name;
      if (value.wrapper) { xml::element item{}; item.qualified_name = value.qualified_name; child.children.push_back(std::move(item)); }
      check.children.push_back(std::move(child));
    }
  }
  (void)xml::write(check);
  return result;
}

std::vector<std::filesystem::path> schema::dependencies() const {
  std::vector<std::filesystem::path> result{};
  if (bundle_) {
    for (const auto& document : bundle_->documents) { result.push_back(document.path); }
  }
  return result;
}

std::string schema::bundle_identity() const {
  return bundle_ ? bundle_->identity : std::string{};
}

std::vector<compatibility_diagnostic> compare_contracts(const record& previous, const record& current) {
  std::vector<compatibility_diagnostic> result{};
  if (previous.root != current.root) { result.push_back({{}, "Root expanded name changed"}); }
  std::map<std::string, const field*> previous_fields{};
  std::vector<std::string> previous_elements{}, current_elements{};
  for (const auto& value : previous.fields) {
    previous_fields.emplace(value.source->name, &value);
    if (!value.attribute && !value.text) { previous_elements.push_back(value.source->name); }
  }
  for (const auto& value : current.fields) {
    if (!value.attribute && !value.text) { current_elements.push_back(value.source->name); }
    const auto found = previous_fields.find(value.source->name);
    if (found == previous_fields.end()) { result.push_back({value.source->name, "Current contract adds a field"}); continue; }
    const auto& old = *found->second;
    if (old.qualified_name != value.qualified_name || old.attribute != value.attribute ||
        old.text != value.text || old.wrapper != value.wrapper) {
      result.push_back({value.source->name, "Expanded name or attribute/element/text/wrapper placement changed"});
    }
    if (value.min_occurs > old.min_occurs ||
        (value.max_occurs && (!old.max_occurs || *value.max_occurs < *old.max_occurs))) {
      result.push_back({value.source->name, "Current reader narrows the accepted occurrence interval"});
    }
    if (old.repeated != value.repeated || leaf(*old.source).name != leaf(*value.source).name) {
      result.push_back({value.source->name, "Stored scalar or collection representation changed"});
    }
    previous_fields.erase(found);
  }
  for (const auto& [key, value] : previous_fields) {
    (void)value; result.push_back({key, "Current contract removes a field"});
  }
  if (previous_elements != current_elements) { result.push_back({{}, "Element sequence order changed"}); }
  return result;
}

std::string schema::export_xsd(const std::vector<const class_node*>& sources) const {
  if (sources.empty()) { throw std::invalid_argument{"XSD export requires at least one record"}; }
  std::vector<record> records{};
  std::map<const syntax_node*, std::string> type_names{};
  std::set<std::string> roots{};
  std::string target{};
  for (const auto* source : sources) {
    if (!source) { throw std::invalid_argument{"XSD export received an empty record"}; }
    records.push_back(bind(*source));
    if (records.size() == 1U) { target = records.back().root.namespace_uri; }
    if (records.back().root.namespace_uri != target) { throw std::invalid_argument{"XSD export currently requires one root namespace"}; }
    if (!roots.insert(records.back().root.local).second) { throw std::invalid_argument{"XSD export has duplicate root names"}; }
    type_names.emplace(source, "record_" + std::to_string(type_names.size()));
  }
  xml::element document{};
  document.qualified_name = {"schema", std::string{xsd_namespace}};
  document.namespace_bindings["xs"] = std::string{xsd_namespace};
  if (!target.empty()) {
    set_attribute(document, "targetNamespace", target);
    document.namespace_bindings["t"] = target;
  }
  const auto type_qname = [&](const std::string& local) { return target.empty() ? local : "t:" + local; };
  const auto node = [](std::string_view kind) {
    xml::element value{}; value.qualified_name = {std::string{kind}, std::string{xsd_namespace}}; return value;
  };
  const std::map<std::string, std::string> primitive_types{{"string", "string"}, {"bool", "boolean"},
      {"int8", "byte"}, {"uint8", "unsignedByte"}, {"int16", "short"}, {"uint16", "unsignedShort"},
      {"int32", "int"}, {"uint32", "unsignedInt"}, {"int64", "long"}, {"uint64", "unsignedLong"},
      {"float", "float"}, {"double", "double"}, {"float32", "float"}, {"float64", "double"}};
  for (std::size_t index{}; index < sources.size(); ++index) {
    auto root = node("element");
    set_attribute(root, "name", records[index].root.local);
    set_attribute(root, "type", type_qname(type_names.at(sources[index])));
    document.children.push_back(std::move(root));
    auto complex = node("complexType");
    set_attribute(complex, "name", type_names.at(sources[index]));
    auto sequence = node("sequence");
    std::vector<xml::element> attributes{};
    for (const auto& value : records[index].fields) {
      if (value.text || value.wrapper) { throw std::invalid_argument{"XSD export of text/wrapper mappings is not yet supported"}; }
      if (!value.qualified_name.namespace_uri.empty() && value.qualified_name.namespace_uri != target) {
        throw std::invalid_argument{"XSD export of imported field namespaces requires a separate bundle"};
      }
      auto member_node = node(value.attribute ? "attribute" : "element");
      set_attribute(member_node, "name", value.qualified_name.local);
      set_attribute(member_node, "form", value.qualified_name.namespace_uri.empty() ? "unqualified" : "qualified");
      const auto& type = leaf(*value.source);
      if (type.type == object_type::class_type) {
        const auto found = type_names.find(type.resolved_node);
        if (found == type_names.end()) { throw std::invalid_argument{"XSD export requires every nested record in its source list"}; }
        set_attribute(member_node, "type", type_qname(found->second));
      } else {
        const auto primitive = primitive_types.find(type.name);
        if (type.type == object_type::enum_type || primitive == primitive_types.end()) {
          throw std::invalid_argument{"XSD export scalar is unsupported: " + type.name};
        }
        set_attribute(member_node, "type", "xs:" + primitive->second);
      }
      if (value.attribute) {
        set_attribute(member_node, "use", value.min_occurs == 0U ? "optional" : "required");
        attributes.push_back(std::move(member_node));
      } else {
        if (value.source->fixed_extent != 0U) {
          const auto extent = value.source->fixed_extent;
          if (extent < value.min_occurs || (value.max_occurs && extent > *value.max_occurs)) {
            throw std::invalid_argument{"XSD occurrence bounds cannot represent the model's fixed array extent"};
          }
          set_attribute(member_node, "minOccurs", std::to_string(extent));
          set_attribute(member_node, "maxOccurs", std::to_string(extent));
        } else {
          set_attribute(member_node, "minOccurs", std::to_string(value.min_occurs));
          set_attribute(member_node, "maxOccurs", value.max_occurs ? std::to_string(*value.max_occurs) : "unbounded");
        }
        sequence.children.push_back(std::move(member_node));
      }
    }
    if (!sequence.children.empty()) { complex.children.push_back(std::move(sequence)); }
    for (auto& value : attributes) { complex.children.push_back(std::move(value)); }
    document.children.push_back(std::move(complex));
  }
  const auto result = xml::write(document);
  const xml::prepared_schema verified{result};
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
