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
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace rohit::serializer::writer::xml_binding {
namespace {
constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};
// Split XML Schema list-valued attributes using only XML whitespace characters.
std::vector<std::string_view> lexical_words(std::string_view text) {
  constexpr std::string_view whitespace{" \t\r\n"};
  std::vector<std::string_view> result;
  auto start = text.find_first_not_of(whitespace);
  while (start != std::string_view::npos) {
    const auto end = text.find_first_of(whitespace, start);
    result.push_back(text.substr(start, end == std::string_view::npos ? text.size() - start : end - start));
    start = end == std::string_view::npos ? end : text.find_first_not_of(whitespace, end);
  }
  return result;
}
// Borrow one unqualified XSD attribute, without conflating prefixed attributes.
std::string_view attribute(const xml::element& node, std::string_view key) {
  for (const auto& value : node.attributes) {
    if (value.qualified_name.namespace_uri.empty() && value.qualified_name.local == key) { return value.value; }
  }
  return {};
}
// Distinguish a present empty schema default/fixed value from an absent declaration.
bool has_attribute(const xml::element& node, std::string_view key) {
  return std::any_of(node.attributes.begin(), node.attributes.end(), [&](const auto& value) {
    return value.qualified_name.namespace_uri.empty() && value.qualified_name.local == key;
  });
}
// XML Schema boolean metadata accepts both standardized true lexical spellings.
bool true_attribute(const xml::element& node, std::string_view key) {
  auto value = attribute(node, key);
  constexpr std::string_view whitespace{" \t\r\n"};
  const auto first = value.find_first_not_of(whitespace);
  if (first == std::string_view::npos) { return false; }
  value = value.substr(first, value.find_last_not_of(whitespace) - first + 1U);
  return value == "true" || value == "1";
}
// Resolve QName-valued schema attributes using preserved in-scope namespace declarations.
xml::name resolve(const xml::element& node, std::string_view value) {
  const auto colon = value.find(':');
  const auto prefix = colon == std::string_view::npos ? std::string{} : std::string{value.substr(0, colon)};
  const auto found = node.namespace_bindings.find(prefix);
  const bool predefined = prefix == "xml";
  if (value.empty() || colon == 0U || (colon != std::string_view::npos &&
      (colon + 1U == value.size() || (!predefined && found == node.namespace_bindings.end()) ||
       value.find(':', colon + 1U) != std::string_view::npos))) {
    throw std::invalid_argument{"Invalid or unbound XSD QName: " + std::string{value}};
  }
  xml::name result{std::string{value.substr(colon == std::string_view::npos ? 0U : colon + 1U)},
                   predefined ? "http://www.w3.org/XML/1998/namespace" :
                       found == node.namespace_bindings.end() ? std::string{} : found->second};
  xml::element check{}; check.qualified_name = {result.local, ""};
  (void)xml::write(check);
  return result;
}
// Parse cardinalities before provider projection so large finite bounds retain exact semantics.
std::size_t occurrence(std::string_view text, std::size_t fallback);
// Reject every construct/attribute whose semantics the generated subset cannot completely enforce.
void inspect(const xml::element& node) {
  if (node.qualified_name.namespace_uri != xsd_namespace) {
    throw std::invalid_argument{"Unsupported foreign node in optimized XSD subset"};
  }
  const auto& kind = node.qualified_name.local;
  if (kind == "annotation") { return; }
  if (node.text.find_first_not_of(" \t\r\n") != std::string::npos) {
    throw std::invalid_argument{"Significant text outside XSD annotation is unsupported"};
  }
  for (const auto* key : {"mixed", "abstract"}) {
    if (!has_attribute(node, key)) { continue; }
    const auto words = lexical_words(attribute(node, key));
    if (words.size() != 1U || (words.front() != "true" && words.front() != "false" &&
        words.front() != "1" && words.front() != "0")) {
      throw std::invalid_argument{"Invalid XSD boolean metadata"};
    }
  }
  if (has_attribute(node, "minOccurs") || has_attribute(node, "maxOccurs")) {
    const auto minimum = occurrence(attribute(node, "minOccurs"), 1U);
    const auto maximum = attribute(node, "maxOccurs");
    if (maximum != "unbounded" && minimum > occurrence(maximum, 1U)) {
      throw std::invalid_argument{"XSD occurrence interval is empty"};
    }
  }
  if (kind == "schema") {
    for (const auto* key : {"elementFormDefault", "attributeFormDefault"}) {
      const auto form = attribute(node, key);
      if (!form.empty() && form != "qualified" && form != "unqualified") {
        throw std::invalid_argument{"Invalid XSD qualification default"};
      }
    }
    for (const auto* key : {"blockDefault", "finalDefault"}) {
      const auto words = lexical_words(attribute(node, key));
      for (const auto word : words) {
        const bool final = key == std::string_view{"finalDefault"};
        if ((word == "#all" && words.size() == 1U) || word == "extension" || word == "restriction" ||
            (final && (word == "list" || word == "union")) || (!final && word == "substitution")) { continue; }
        throw std::invalid_argument{"Invalid XSD schema derivation default"};
      }
    }
  }
  const std::map<std::string, std::set<std::string>> accepted{
      {"schema", {"targetNamespace", "elementFormDefault", "attributeFormDefault", "version", "id", "blockDefault", "finalDefault"}},
      {"include", {"schemaLocation", "id"}}, {"import", {"namespace", "schemaLocation", "id"}},
      {"element", {"name", "type", "ref", "minOccurs", "maxOccurs", "form", "id", "default", "fixed"}},
      {"attribute", {"name", "type", "ref", "use", "form", "id", "default", "fixed"}},
      {"simpleType", {"name", "id"}}, {"restriction", {"base", "id"}},
      {"complexType", {"name", "id", "mixed", "abstract", "block", "final"}},
      {"sequence", {"id", "minOccurs", "maxOccurs"}},
      {"choice", {"id", "minOccurs", "maxOccurs"}},
      {"simpleContent", {"id"}}, {"complexContent", {"id", "mixed"}},
      {"extension", {"base", "id"}},
      {"group", {"id", "name", "ref", "minOccurs", "maxOccurs"}},
      {"attributeGroup", {"id", "name", "ref"}},
      {"any", {"id", "namespace", "processContents", "minOccurs", "maxOccurs"}},
      {"anyAttribute", {"id", "namespace", "processContents"}},
      {"list", {"id", "itemType"}}, {"union", {"id", "memberTypes"}},
      {"pattern", {"id", "value", "fixed"}},
      {"enumeration", {"id", "value"}}, {"minInclusive", {"id", "value", "fixed"}},
      {"maxInclusive", {"id", "value", "fixed"}}, {"minExclusive", {"id", "value", "fixed"}},
      {"maxExclusive", {"id", "value", "fixed"}}, {"length", {"id", "value", "fixed"}},
      {"minLength", {"id", "value", "fixed"}}, {"maxLength", {"id", "value", "fixed"}},
      {"whiteSpace", {"id", "value", "fixed"}}};
  const auto allowed = accepted.find(kind);
  if (allowed == accepted.end()) {
    throw std::invalid_argument{"Unsupported optimized XSD construct: " + kind};
  }
  for (const auto& item : node.attributes) {
    if (kind == "schema" && item.qualified_name ==
        xml::name{"lang", "http://www.w3.org/XML/1998/namespace"}) { continue; }
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
// Match a global root, a named reusable type, or a unique anonymous local record in that order.
std::pair<const schema_bundle::document*, const xml::element*> record_type(
    const schema_bundle& bundle, const xml::name& root) {
  using selected_type = std::pair<const schema_bundle::document*, const xml::element*>;
  for (const auto kind : {"element", "complexType"}) {
    selected_type selected{};
    for (const auto& document : bundle.documents) {
      if (!root.namespace_uri.empty() && document.target != root.namespace_uri) { continue; }
      for (const auto& node : document.root.children) {
        if (node.qualified_name.local != kind || attribute(node, "name") != root.local) { continue; }
        const auto found = kind == std::string_view{"complexType"}
            ? selected_type{&document, &node} : complex_type(bundle, document, node);
        if (selected.second && selected != found) {
          throw std::invalid_argument{"Ambiguous XSD record; configure its namespace or unique root"};
        }
        selected = found;
      }
    }
    if (selected.second) { return selected; }
  }
  selected_type selected{};
  for (const auto& document : bundle.documents) {
    if (!root.namespace_uri.empty() && document.target != root.namespace_uri) { continue; }
    const auto visit = [&](auto&& self, const xml::element& node) -> void {
      if (node.qualified_name.local == "element" && attribute(node, "name") == root.local) {
        for (const auto& child : node.children) {
          if (child.qualified_name.local == "complexType") {
            if (selected.second && selected.second != &child) {
              throw std::invalid_argument{"Anonymous XSD record name is ambiguous"};
            }
            selected = {&document, &child};
          }
        }
      }
      for (const auto& child : node.children) { self(self, child); }
    };
    visit(visit, document.root);
  }
  if (!selected.second) { throw std::invalid_argument{"No XSD record matches: " + root.local}; }
  return selected;
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
    // Grammar inspection has already rejected significant structural text. The projection
    // rebuilds children and omits annotation bodies, so source mixed-text slots cannot survive.
    node.text.clear();
    node.text_segments.clear();
    if (node.qualified_name.local == "annotation") { return node; }
    // libxml2 reserves INT_MAX internally and rejects that valid finite XSD maximum.
    // This projection checks grammar only; binding and generated checks retain original bounds.
    constexpr auto maximum_provider_occurrence =
        static_cast<std::size_t>(std::numeric_limits<int>::max()) - 1U;
    const auto maximum = attribute(source, "maxOccurs");
    if (!maximum.empty() && maximum != "unbounded" && occurrence(maximum, 1U) > maximum_provider_occurrence) {
      set_attribute(node, "maxOccurs", "unbounded");
    }
    // Canonical spellings avoid a provider rejection of otherwise legal whitespace-collapsed booleans.
    for (const auto* key : {"mixed", "abstract"}) {
      if (has_attribute(source, key)) { set_attribute(node, key, true_attribute(source, key) ? "true" : "false"); }
    }
    node.namespace_bindings["xs"] = std::string{xsd_namespace};
    node.namespace_bindings["b"] = std::string{closed_namespace};
    if (global && !attribute(source, "name").empty()) {
      set_attribute(node, "name", prefixes.at(document.target) + std::string{attribute(source, "name")});
    }
    if (global && (source.qualified_name.local == "complexType" || source.qualified_name.local == "simpleType") &&
        !has_attribute(source, "final") && !attribute(document.root, "finalDefault").empty()) {
      std::string derivations;
      for (const auto value : lexical_words(attribute(document.root, "finalDefault"))) {
        const bool complex = source.qualified_name.local == "complexType";
        if (value == "#all" || value == "restriction" || (complex && value == "extension") ||
            (!complex && (value == "list" || value == "union"))) {
          if (!derivations.empty()) { derivations += ' '; }
          derivations += value;
        }
      }
      set_attribute(node, "final", std::move(derivations));
    }
    const auto rewrite_name = [&](const xml::name& name) {
      if (name.namespace_uri == xsd_namespace) { return "xs:" + name.local; }
      {
        const bool imported = std::any_of(document.root.children.begin(), document.root.children.end(), [&](const auto& child) {
          return child.qualified_name.local == "import" && attribute(child, "namespace") == name.namespace_uri;
        });
        if (name.namespace_uri != document.target && !imported) {
          throw std::invalid_argument{"XSD QName namespace requires an explicit import in its source document"};
        }
        const auto prefix = prefixes.find(name.namespace_uri);
        if (prefix == prefixes.end()) { throw std::invalid_argument{"XSD QName references an unbundled namespace"}; }
        return "b:" + prefix->second + name.local;
      }
    };
    for (const auto* key : {"type", "base", "ref", "itemType"}) {
      if (attribute(source, key).empty()) { continue; }
      set_attribute(node, key, rewrite_name(resolve(source, attribute(source, key))));
    }
    if (!attribute(source, "memberTypes").empty()) {
      std::string members;
      for (const auto value : lexical_words(attribute(source, "memberTypes"))) {
        if (!members.empty()) { members += ' '; }
        members += rewrite_name(resolve(source, value));
      }
      set_attribute(node, "memberTypes", std::move(members));
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
std::shared_ptr<const schema_bundle> load_bundle(const std::filesystem::path& path,
                                               bool optimized_subset) {
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
    // Documentation may interleave text and markup; grammar inspection still rejects significant
    // text and foreign nodes outside annotations, while the parser retains all byte budgets.
    xml::operation_context annotation_context;
    annotation_context.allow_mixed_content = true;
    annotation_context.unknown_fields = xml::unknown_policy::preserve;
    annotation_context.permits_namespace = [](std::string_view uri) { return uri != xsd_namespace; };
    auto tree = xml::parse(bytes, {}, &annotation_context);
    if (optimized_subset) { inspect(tree); }
    if (tree.qualified_name != xml::name{"schema", std::string{xsd_namespace}}) {
      throw std::invalid_argument{"XSD root must be xs:schema"};
    }
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
  if (optimized_subset) {
    // Check alias fan-out before handing unions to the provider, which may eagerly expand them.
    const auto check_unions = [&](auto&& self, const xml::element& node) -> void {
      if (node.qualified_name.local == "annotation") { return; }
      if (node.qualified_name.local == "simpleType" && std::any_of(node.children.begin(), node.children.end(),
          [](const auto& child) { return child.qualified_name.local == "union"; })) {
        (void)resolve_scalar_type(*result, node);
      }
      for (const auto& child : node.children) { self(self, child); }
    };
    for (const auto& document : result->documents) { check_unions(check_unions, document.root); }
    validate_bundle(*result);
  }
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
  } else if (type.type == object_type::class_type && type.resolved_node &&
             static_cast<const class_node&>(*type.resolved_node).reference) {
    const auto& reference = *static_cast<const class_node&>(*type.resolved_node).reference;
    if (!reference.value_projection) {
      throw std::invalid_argument{"XML durable field-address references require an explicit wire contract"};
    }
    check_type(reference.value_type, collection_element);
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
// Preserve inherited restrictions in base-to-derived order after libxml2 checks their legality.
struct scalar_resolution {
  xml::name primitive{};
  scalar_facets facets{};
};

// Resolve bounded aliases, inline simple types and list item types without fetching any schema.
scalar_resolution resolve_scalar(const schema_bundle& bundle, const xml::element& source) {
  constexpr std::size_t maximum_type_depth{64U};
  constexpr std::size_t maximum_union_members{1024U};
  constexpr std::size_t maximum_facet_bytes{16U * 1024U * 1024U};
  std::size_t remaining_union_members{maximum_union_members};
  std::size_t remaining_facet_bytes{maximum_facet_bytes};
  std::set<std::pair<std::string, std::string>> active;
  const auto apply = [&](scalar_resolution& result, const xml::element& restriction) {
    std::vector<std::string> enumeration;
    std::vector<std::string> patterns;
    for (const auto& facet : restriction.children) {
      const auto& kind = facet.qualified_name.local;
      const auto raw_value = attribute(facet, "value");
      if (raw_value.size() > remaining_facet_bytes) {
        throw std::length_error{"XSD scalar facet expansion byte limit exceeded"};
      }
      remaining_facet_bytes -= raw_value.size();
      const auto value = std::string{raw_value};
      if (kind == "minInclusive") { result.facets.min_inclusive = value; }
      else if (kind == "maxInclusive") { result.facets.max_inclusive = value; }
      else if (kind == "minExclusive") { result.facets.min_exclusive = value; }
      else if (kind == "maxExclusive") { result.facets.max_exclusive = value; }
      else if (kind == "length") { result.facets.length = occurrence(value, 0U); }
      else if (kind == "minLength") { result.facets.min_length = occurrence(value, 0U); }
      else if (kind == "maxLength") { result.facets.max_length = occurrence(value, 0U); }
      else if (kind == "whiteSpace") { result.facets.whitespace = value; }
      else if (kind == "enumeration") { enumeration.push_back(value); }
      else if (kind == "pattern") { patterns.push_back(value); }
      else if (kind != "annotation" && kind != "simpleType") {
        throw std::invalid_argument{"Unsupported optimized XSD scalar facet: " + kind};
      }
    }
    if (!patterns.empty()) {
      if (!result.facets.patterns.empty()) {
        result.facets.pattern_groups.push_back(std::move(result.facets.patterns));
      }
      result.facets.patterns = std::move(patterns);
    }
    if (!enumeration.empty()) {
      result.facets.enumeration = std::move(enumeration);
      result.facets.qname_enumeration.clear();
      if (result.primitive.local == "QName") {
        for (const auto& facet : restriction.children) {
          if (facet.qualified_name.local == "enumeration") {
            result.facets.qname_enumeration.push_back(resolve(facet, attribute(facet, "value")));
          }
        }
      }
    }
  };
  const auto walk = [&](auto&& self, const xml::element& node,
                        std::optional<xml::name> selected, std::size_t depth) -> scalar_resolution {
    if (depth > maximum_type_depth) { throw std::length_error{"XSD simple type nesting limit exceeded"}; }
    const xml::element* simple{};
    if (!selected && node.qualified_name.local == "simpleType") { simple = &node; }
    else if (!selected) {
      for (const auto& child : node.children) {
        if (child.qualified_name.local == "simpleType") { simple = &child; }
      }
    }
    if (!simple) {
      const auto type = selected ? *selected : resolve(node, attribute(node, "type"));
      if (type.namespace_uri == xsd_namespace) {
        scalar_resolution result{type, {}};
        result.facets.lexical_kind = type.local;
        if (type.local == "NMTOKENS") {
          result.primitive.local = "string";
          result.facets.lexical_kind = "string";
          result.facets.list_item_kind = "NMTOKEN";
          result.facets.min_length = 1U;
        }
        if (type.local != "string") {
          result.facets.whitespace = type.local == "normalizedString" ? "replace" : "collapse";
        }
        return result;
      }
      if (!active.emplace(type.namespace_uri, type.local).second) {
        throw std::invalid_argument{"Cyclic XSD simple type restriction"};
      }
      const auto* definition = declaration(bundle, "simpleType", type).second;
      auto result = self(self, *definition, std::nullopt, depth + 1U);
      active.erase({type.namespace_uri, type.local});
      return result;
    }
    for (const auto& child : simple->children) {
      const auto& kind = child.qualified_name.local;
      if (kind == "restriction") {
        const auto base = attribute(child, "base");
        auto result = self(self, child, base.empty() ? std::optional<xml::name>{} : resolve(child, base), depth + 1U);
        apply(result, child);
        return result;
      }
      if (kind == "list") {
        const auto item = resolve(child, attribute(child, "itemType"));
        if (item.namespace_uri != xsd_namespace) {
          throw std::invalid_argument{"XSD list item restrictions need separate per-item facet metadata"};
        }
        auto result = self(self, child, item, depth + 1U);
        result.facets.list_item_kind = result.primitive.local;
        result.primitive = {"string", std::string{xsd_namespace}};
        result.facets.whitespace = "collapse";
        result.facets.lexical_kind = "string";
        return result;
      }
      if (kind == "union") {
        scalar_resolution result{{"string", std::string{xsd_namespace}}, {}};
        result.facets.lexical_kind = "string";
        result.facets.whitespace = "collapse";
        const auto append = [&](scalar_resolution branch) {
          if (remaining_union_members == 0U) {
            throw std::length_error{"XSD union expansion member limit exceeded"};
          }
          --remaining_union_members;
          result.facets.union_members.push_back(std::move(branch.facets));
        };
        for (const auto type : lexical_words(attribute(child, "memberTypes"))) {
          append(self(self, child, resolve(child, type), depth + 1U));
        }
        for (const auto& member : child.children) {
          if (member.qualified_name.local == "simpleType") {
            append(self(self, member, std::nullopt, depth + 1U));
          }
        }
        if (result.facets.union_members.empty()) {
          throw std::invalid_argument{"XSD union requires at least one simple member type"};
        }
        return result;
      }
    }
    throw std::invalid_argument{"XSD simple types require supported restriction, list or union content"};
  };
  return walk(walk, source, std::nullopt, 0U);
}
// Select the scalar/record leaf type beneath one optional or collection modifier.
const type_name& leaf(const member& source) {
  const auto* selected_type = &source.type_name_list.front();
  while (selected_type->is_optional() || selected_type->is_sequence()) {
    selected_type = &selected_type->arguments.front();
  }
  const auto& selected = *selected_type;
  if (selected.type == object_type::class_type && selected.resolved_node) {
    const auto& owner = static_cast<const class_node&>(*selected.resolved_node);
    if (owner.reference && owner.reference->value_projection) { return owner.reference->value_type; }
  }
  return selected;
}
} // namespace

// Keep resolver policy identical for generation and import; neither mode fetches remote input.
std::shared_ptr<const schema_bundle> read_schema_bundle(const std::filesystem::path& path,
                                                       bool optimized_subset) {
  return load_bundle(path, optimized_subset);
}

// Share the complete lexical contract with the importer instead of resolving aliases twice.
scalar_type resolve_scalar_type(const schema_bundle& bundle, const xml::element& source) {
  auto result = resolve_scalar(bundle, source);
  return {std::move(result.primitive), std::move(result.facets)};
}

// Retain the model group's defining namespace defaults when expanding a local reference.
schema_component resolve_model_group(const schema_bundle& bundle,
    const schema_bundle::document& owner, const xml::element& reference) {
  const auto name = attribute(reference, "ref");
  const auto selected = name.empty()
      ? std::pair{&owner, &reference} : declaration(bundle, "group", resolve(reference, name));
  const xml::element* compositor{};
  for (const auto& child : selected.second->children) {
    if (child.qualified_name.local == "annotation") { continue; }
    if (compositor || (child.qualified_name.local != "sequence" && child.qualified_name.local != "choice")) {
      throw std::invalid_argument{"XSD model groups require one supported sequence or choice"};
    }
    compositor = &child;
  }
  if (!compositor) { throw std::invalid_argument{"XSD model group has no compositor"}; }
  return {selected.first, compositor};
}

// Expand static inheritance and attribute groups while keeping original nodes alive in the bundle.
complex_content expand_complex_type(const schema_bundle& bundle,
    const schema_bundle::document& owner, const xml::element& source) {
  complex_content result;
  std::set<const xml::element*> active;
  constexpr std::size_t maximum_expansion_depth{64U};
  const auto expand = [&](auto&& self, const schema_bundle::document& document,
                          const xml::element& node, std::size_t depth) -> void {
    if (depth > maximum_expansion_depth || !active.insert(&node).second) {
      throw std::invalid_argument{"Cyclic or excessively nested XSD complex content"};
    }
    const auto& kind = node.qualified_name.local;
    if ((kind == "complexType" || kind == "complexContent") && true_attribute(node, "mixed")) {
      result.mixed = true;
    }
    for (const auto& child : node.children) {
      const auto& child_kind = child.qualified_name.local;
      if (child_kind == "annotation") { continue; }
      if (child_kind == "attributeGroup") {
        const auto reference = attribute(child, "ref");
        if (reference.empty()) { throw std::invalid_argument{"Local XSD attribute groups require ref"}; }
        const auto selected = declaration(bundle, "attributeGroup", resolve(child, reference));
        self(self, *selected.first, *selected.second, depth + 1U);
      } else if (child_kind == "complexContent") {
        self(self, document, child, depth + 1U);
      } else if (kind == "complexContent" && child_kind == "extension") {
        const auto selected = declaration(bundle, "complexType", resolve(child, attribute(child, "base")));
        self(self, *selected.first, *selected.second, depth + 1U);
        self(self, document, child, depth + 1U);
      } else if (kind == "complexContent" && child_kind == "restriction") {
        throw std::invalid_argument{"XSD complex-content restrictions require an explicit projected model"};
      } else {
        result.children.push_back({&document, &child});
      }
    }
    active.erase(&node);
  };
  expand(expand, owner, source, 0U);
  return result;
}

// Scan compiler metadata only; schemas without XML declarations keep the original codec selection.
xml_options effective_options(const std::vector<std::unique_ptr<syntax_node>>& statements,
                              xml_options options) {
  bool authored{};
  bool package{};
  const auto declared = [](const xml_authoring_options& value) {
    return value.namespace_uri || value.root || value.scalar_placement || value.unknown || value.schema;
  };
  const auto scan = [&](auto&& self, const auto& nodes) -> void {
    for (const auto& node : nodes) {
      if (node->type == object_type::namespace_type) {
        const auto& space = static_cast<const namespace_node&>(*node);
        authored = authored || declared(space.xml);
        self(self, space.statements);
      } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
        const auto& owner = static_cast<const class_node&>(*node);
        authored = authored || declared(owner.xml) || std::any_of(owner.member_list.begin(),
            owner.member_list.end(), [](const auto& field) { return field.xml.has_value(); });
      } else if (node->type == object_type::package_type) { package = true; }
    }
  };
  scan(scan, statements);
  if (options.enabled_selection == false && package) {
    throw std::invalid_argument{"Schema package declarations require XML; explicit XML disable conflicts"};
  }
  if (options.enabled_selection) { options.enabled = *options.enabled_selection; }
  else if (authored || package) { options.enabled = true; }
  return options;
}

void validate_options(const std::vector<std::unique_ptr<syntax_node>>& statements,
                      const xml_options& options, std::string_view language) {
  if (options.validation_scope != "input" && options.validation_scope != "output" && options.validation_scope != "both") {
    throw std::invalid_argument{"XML validation_scope must be input, output or both"};
  }
  if (!options.enabled && (options.runtime_xsd || !options.xsd.empty() || !options.classes.empty())) {
    throw std::invalid_argument{"XML configuration requires xml.enabled = true"};
  }
  if (options.enabled) {
    const schema bindings{options};
    const auto validate_references = [&](auto&& self, const auto& nodes) -> void {
      for (const auto& node : nodes) {
        if (node->type == object_type::namespace_type) {
          self(self, static_cast<const namespace_node&>(*node).statements);
        } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
          const auto& owner = static_cast<const class_node&>(*node);
          if (owner.reference && (!owner.reference->value_projection || language != "cpp")) {
            throw std::invalid_argument{
                "XML value projections are C++ export-only; durable field addresses have no XML binding"};
          }
          if (node->type == object_type::class_type && !owner.reference) {
            validate_backend(bindings.bind(owner), language);
          }
        } else if (node->type == object_type::package_type && language != "cpp" && language != "python") {
          throw std::invalid_argument{"Generated OPC package profiles are not implemented for " +
                                      std::string{language}};
        }
      }
    };
    validate_references(validate_references, statements);
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
  bundle_ = read_schema_bundle(options_.xsd);
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
  xml_class_options mapping{};
  mapping.namespace_uri = options_.namespace_uri;
  std::vector<const xml_authoring_options*> authored;
  for (auto* space = source.parent_namespace; space; space = space->parent_namespace) {
    authored.push_back(&space->xml);
  }
  std::reverse(authored.begin(), authored.end());
  authored.push_back(&source.xml);
  std::optional<xml_field_placement> scalar_placement = options_.scalar_attributes
      ? std::optional{xml_field_placement::attribute} : std::nullopt;
  std::optional<xml_schema_binding> declared_schema;
  for (const auto* options : authored) {
    if (options->namespace_uri) { mapping.namespace_uri = *options->namespace_uri; }
    if (options->root) { mapping.root = *options->root; }
    if (options->scalar_placement) { scalar_placement = options->scalar_placement; }
    if (options->schema) { declared_schema = options->schema; }
  }
  if (configured != options_.classes.end()) {
    // Legacy configuration is sparse when combined with schema-authored defaults.
    const auto& selected = configured->second;
    if (!selected.root.empty()) { mapping.root = selected.root; }
    if (!selected.namespace_uri.empty()) { mapping.namespace_uri = selected.namespace_uri; }
    mapping.attributes = selected.attributes;
    mapping.names = selected.names;
    mapping.text = selected.text;
    mapping.wrappers = selected.wrappers;
  }
  if (!bundle_ && declared_schema) {
    auto selected = options_;
    const auto base = std::filesystem::path{declared_schema->source_path}.parent_path();
    selected.xsd = declared_schema->path.is_absolute() ? declared_schema->path
                                                     : base / declared_schema->path;
    const auto canonical = std::filesystem::canonical(selected.xsd);
    auto found = inline_bundles_.find(canonical);
    if (found == inline_bundles_.end()) {
      found = inline_bundles_.emplace(canonical, read_schema_bundle(canonical)).first;
    }
    schema fixed{options_};
    fixed.bundle_ = found->second;
    return fixed.bind(source);
  }
  record result{{mapping.root.empty() ? source.name : mapping.root, mapping.namespace_uri}, {}};
  for (const auto* options : authored) {
    if (options->unknown) { result.unknown = options->unknown; }
  }
  std::set<std::string> member_names{};
  for (const auto& source_field : source.member_list) {
    if (source_field.omits("xml")) { continue; }
    if (std::ranges::any_of(source_field.type_name_list,
            [](const auto& type) { return type.contains_managed_reference(); }) ||
        (source_field.key_type && source_field.key_type->contains_managed_reference())) {
      throw std::invalid_argument{"Managed entity references have no XML binding: " + source_field.name};
    }
    if (source_field.modifier == member::modifier_type::map || source_field.modifier == member::modifier_type::variant ||
        source_field.managed || source_field.wire_only) {
      throw std::invalid_argument{"Unsupported XML map, variant, managed or derived-size field: " + source_field.name};
    }
    const bool list = source_field.xml && source_field.xml->list;
    const auto& source_type = source_field.type_name_list.front();
    const auto& list_container = source_type.is_optional() ? source_type.arguments.front() : source_type;
    if (list) {
      if (list_container.is_sequence()) { check_type(list_container.arguments.front(), true); }
      else { check_type(source_type, source_field.modifier == member::modifier_type::array); }
    } else { check_type(source_type, source_field.modifier == member::modifier_type::array); }
    const bool collection = source_field.modifier == member::modifier_type::array || list_container.is_sequence();
    const bool repeated = collection && !list;
    const auto renamed = mapping.names.find(source_field.name);
    auto local_name = renamed == mapping.names.end() ? source_field.name : renamed->second;
    bool is_attribute = std::find(mapping.attributes.begin(), mapping.attributes.end(), source_field.name) != mapping.attributes.end() ||
        (scalar_placement == xml_field_placement::attribute && !repeated &&
         leaf(source_field).type != object_type::class_type);
    bool text = source_field.name == mapping.text;
    auto field_namespace = is_attribute ? std::string{} : mapping.namespace_uri;
    std::optional<std::string> authored_wrapper;
    if (source_field.xml) {
      const auto& field_options = *source_field.xml;
      if (field_options.placement) {
        is_attribute = *field_options.placement == xml_field_placement::attribute;
        text = *field_options.placement == xml_field_placement::text;
        field_namespace = is_attribute ? std::string{} : mapping.namespace_uri;
      }
      if (field_options.name) { local_name = *field_options.name; }
      if (field_options.namespace_uri) { field_namespace = *field_options.namespace_uri; }
      authored_wrapper = field_options.wrapper;
    }
    if (list && (!collection || leaf(source_field).type == object_type::class_type)) {
      throw std::invalid_argument{"XML list mapping requires a collection of nonoptional scalar items"};
    }
    if (source_field.xml && (source_field.xml->qname || source_field.xml->prefix_list) &&
        (collection || leaf(source_field).name != "string" ||
         (source_field.xml->qname && source_field.xml->prefix_list))) {
      throw std::invalid_argument{"XML QName/prefix-list mappings require one string field"};
    }
    if (is_attribute && (repeated || leaf(source_field).type == object_type::class_type || text)) {
      throw std::invalid_argument{"XML attributes require scalar fields: " + source_field.name};
    }
    if (text && (repeated || leaf(source_field).type == object_type::class_type ||
        source_field.type_name_list.front().is_optional())) {
      throw std::invalid_argument{"XML text requires one nonoptional scalar field: " + source_field.name};
    }
    const auto wrapper = mapping.wrappers.find(source_field.name);
    const auto wrapper_name = authored_wrapper ? *authored_wrapper :
        wrapper != mapping.wrappers.end() ? wrapper->second : std::string{};
    if ((authored_wrapper || wrapper != mapping.wrappers.end()) &&
        (!repeated || is_attribute || text || wrapper_name.empty())) {
      throw std::invalid_argument{"XML wrappers require repeated element fields and a nonempty wrapper name"};
    }
    result.fields.push_back({&source_field,
        {local_name, field_namespace},
        is_attribute, source_field.type_name_list.front().is_optional() || repeated ? 0U : 1U,
        repeated ? std::optional<std::size_t>{} : std::optional<std::size_t>{1U}, repeated, text, {}});
    result.fields.back().list = list;
    const auto& declared_type = source_field.type_name_list.front();
    const auto& declared_leaf = declared_type.is_optional() || declared_type.is_sequence()
        ? declared_type.arguments.front() : declared_type;
    if (declared_leaf.type == object_type::class_type && declared_leaf.resolved_node) {
      const auto& selected_owner = static_cast<const class_node&>(*declared_leaf.resolved_node);
      result.fields.back().value_projection = selected_owner.reference &&
          selected_owner.reference->value_projection;
    }
    result.fields.back().qname = source_field.xml && source_field.xml->qname;
    result.fields.back().prefix_list = source_field.xml && source_field.xml->prefix_list;
    if (list) {
      const std::map<std::string, std::string> primitives{
          {"string", "string"}, {"bool", "boolean"}, {"int8", "byte"},
          {"uint8", "unsignedByte"}, {"int16", "short"}, {"uint16", "unsignedShort"},
          {"int32", "int"}, {"uint32", "unsignedInt"}, {"int64", "long"},
          {"uint64", "unsignedLong"}, {"float", "float"}, {"double", "double"},
          {"float32", "float"}, {"float64", "double"}};
      const auto item = primitives.find(leaf(source_field).name);
      if (item == primitives.end()) { throw std::invalid_argument{"Unsupported XML list scalar type"}; }
      result.fields.back().facets.list_item_kind = item->second;
      result.fields.back().facets.lexical_kind = "string";
      result.fields.back().facets.whitespace = "collapse";
      if (source_field.fixed_extent != 0U) {
        result.fields.back().facets.length = static_cast<std::size_t>(source_field.fixed_extent);
      }
    }
    if (result.fields.back().qname) { result.fields.back().facets.lexical_kind = "QName"; }
    if (!wrapper_name.empty()) {
      result.fields.back().wrapper = xml::name{wrapper_name, mapping.namespace_uri};
    }
    member_names.insert(source_field.name);
  }
  for (const auto& key : mapping.attributes) {
    if (!member_names.contains(key)) { throw std::invalid_argument{"Unknown XML attribute member: " + key}; }
  }
  if (!mapping.text.empty() && !member_names.contains(mapping.text)) {
    throw std::invalid_argument{"Unknown XML text member: " + mapping.text};
  }
  const auto text_count = std::count_if(result.fields.begin(), result.fields.end(),
      [](const auto& value) { return value.text; });
  if (text_count > 1 || (text_count != 0 && std::any_of(result.fields.begin(), result.fields.end(), [](const auto& value) {
      return !value.attribute && !value.text;
    }))) { throw std::invalid_argument{"XML text requires exactly one text field and no child elements"}; }
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
    const auto [complex_document, complex] = record_type(*bundle_, result.root);
    if (true_attribute(*complex, "abstract")) {
      throw std::invalid_argument{"Abstract XSD complex types require dynamic type selection"};
    }
    const auto content = expand_complex_type(*bundle_, *complex_document, *complex);
    const auto& target = complex_document->target;
    if (result.root.namespace_uri.empty()) {
      std::optional<std::string> root_namespace;
      for (const auto& document : bundle_->documents) {
        for (const auto& node : document.root.children) {
          if (node.qualified_name.local != "element" || attribute(node, "name") != result.root.local) { continue; }
          if (root_namespace && *root_namespace != document.target) {
            throw std::invalid_argument{"XSD root element namespace is ambiguous"};
          }
          root_namespace = document.target;
        }
      }
      // The global element's namespace is independent of the namespace defining its complex type.
      result.root.namespace_uri = root_namespace.value_or(target);
    }
    std::vector<field> ordered{};
    std::set<const member*> bound{};
    const auto bind_field = [&](const xml::element& occurrence_node, bool is_attribute,
                                bool in_choice = false,
                                const schema_bundle::document* defining_document = nullptr) {
      auto* owner = defining_document ? defining_document : complex_document;
      auto* selected = &occurrence_node;
      const bool referenced = !attribute(*selected, "ref").empty();
      if (referenced) {
        const auto found = declaration(*bundle_, is_attribute ? "attribute" : "element",
                                       resolve(*selected, attribute(*selected, "ref")));
        owner = found.first; selected = found.second;
      }
      const auto& node = *selected;
      const auto local = attribute(node, "name");
      const auto form = attribute(node, "form");
      const bool qualified = referenced || form == "qualified" ||
          (form.empty() && attribute(owner->root, is_attribute ? "attributeFormDefault" : "elementFormDefault") == "qualified");
      const auto uri = qualified ? owner->target : std::string{};
      const auto local_matches = std::count_if(result.fields.begin(), result.fields.end(), [&](const auto& value) {
        return !value.text && value.qualified_name.local == local;
      });
      const auto matches = [&](const auto& value) {
        return !value.text && value.qualified_name.local == local &&
            (local_matches == 1 || (value.qualified_name.namespace_uri == uri && value.attribute == is_attribute));
      };
      const auto candidate = std::find_if(result.fields.begin(), result.fields.end(), matches);
      if (candidate == result.fields.end() || !bound.insert(candidate->source).second) {
        throw std::invalid_argument{"XSD member binding is missing or ambiguous: " + std::string{local}};
      }
      auto field = *candidate;
      if (std::find(mapping.attributes.begin(), mapping.attributes.end(), field.source->name) != mapping.attributes.end() && !is_attribute) {
        throw std::invalid_argument{"Configured XML attribute conflicts with XSD"};
      }
      field.attribute = is_attribute;
      if (std::count_if(result.fields.begin(), result.fields.end(), matches) != 1) {
        throw std::invalid_argument{"XSD expanded member identity is ambiguous: " + std::string{local}};
      }
      field.qualified_name.namespace_uri = uri;
      const auto& source_type = leaf(*field.source);
      const std::map<std::string, std::string> primitive_types{{"string", "string"}, {"boolean", "bool"},
          {"byte", "int8"}, {"unsignedByte", "uint8"}, {"short", "int16"}, {"unsignedShort", "uint16"},
          {"int", "int32"}, {"unsignedInt", "uint32"}, {"long", "int64"}, {"unsignedLong", "uint64"},
          {"float", "float"}, {"double", "double"}, {"normalizedString", "string"},
          {"token", "string"}, {"language", "string"}, {"Name", "string"}, {"NCName", "string"},
          {"NMTOKEN", "string"}, {"anyURI", "string"}, {"QName", "string"}, {"decimal", "string"},
          {"integer", "string"}, {"nonNegativeInteger", "string"}, {"positiveInteger", "string"},
          {"nonPositiveInteger", "string"}, {"negativeInteger", "string"}};
      if (source_type.type == object_type::class_type) {
        if (is_attribute || !source_type.resolved_node) { throw std::invalid_argument{"XSD records require element bindings"}; }
        const auto& nested = static_cast<const class_node&>(*source_type.resolved_node);
        const auto nested_configured = options_.classes.find(nested.get_full_name());
        const auto& nested_mapping = nested_configured == options_.classes.end() ? defaults : nested_configured->second;
        const auto expected = complex_type(*bundle_, *owner, node);
        const auto nested_contract = bind(nested);
        if (record_type(*bundle_, nested_contract.root).second != expected.second) {
          throw std::invalid_argument{"Nested XSD record differs from its declared complex type"};
        }
        (void)nested_mapping;
      } else {
        const auto resolved = resolve_scalar(*bundle_, node);
        const auto primitive = primitive_types.find(field.list && !resolved.facets.list_item_kind.empty()
            ? resolved.facets.list_item_kind : resolved.primitive.local);
        if (primitive == primitive_types.end() ||
            (source_type.name != primitive->second &&
             !(source_type.name == "float32" && primitive->second == "float") &&
             !(source_type.name == "float64" && primitive->second == "double"))) {
          throw std::invalid_argument{"Optimized XSD field type is unsupported or differs from Serializer: " + field.source->name};
        }
        field.facets = resolved.facets;
        field.qname = field.qname || resolved.primitive.local == "QName";
      }
      if (has_attribute(node, "default")) { field.default_value = std::string{attribute(node, "default")}; }
      if (has_attribute(node, "fixed")) { field.fixed_value = std::string{attribute(node, "fixed")}; }
      if (field.qname) {
        if (field.default_value) { field.qname_default = resolve(node, *field.default_value); }
        if (field.fixed_value) { field.qname_fixed = resolve(node, *field.fixed_value); }
      }
      if (is_attribute) {
        if (field.repeated) { throw std::invalid_argument{"XSD attributes cannot bind collections"}; }
        const auto use = attribute(occurrence_node, "use");
        if (use == "prohibited") { throw std::invalid_argument{"Prohibited XSD attribute cannot bind a stored field"}; }
        field.min_occurs = use == "required" ? 1U : 0U;
        field.max_occurs = 1U;
      } else {
        field.min_occurs = in_choice ? 0U : occurrence(attribute(occurrence_node, "minOccurs"), 1U);
        const auto maximum = attribute(occurrence_node, "maxOccurs");
        field.max_occurs = maximum == "unbounded" ? std::optional<std::size_t>{} :
            std::optional<std::size_t>{occurrence(maximum, 1U)};
      }
      if (field.max_occurs && field.min_occurs > *field.max_occurs) {
        throw std::invalid_argument{"XSD occurrence interval is empty"};
      }
      const bool optional = field.source->type_name_list.front().is_optional();
      if (!field.repeated && (field.max_occurs != std::optional<std::size_t>{1U} ||
          field.min_occurs != (optional ? 0U : 1U))) {
        throw std::invalid_argument{"XSD presence/cardinality differs from Serializer member: " + field.source->name};
      }
      ordered.push_back(std::move(field));
    };
    const auto add_wildcard = [&](const xml::element& node, bool is_attribute,
                                  const schema_bundle::document* owner) {
      wildcard value;
      value.attribute = is_attribute;
      value.target_namespace = owner->target;
      if (!attribute(node, "namespace").empty()) { value.namespace_rule = attribute(node, "namespace"); }
      if (!attribute(node, "processContents").empty()) { value.process_contents = attribute(node, "processContents"); }
      value.minimum = is_attribute ? 0U : occurrence(attribute(node, "minOccurs"), 1U);
      const auto maximum = attribute(node, "maxOccurs");
      value.maximum = is_attribute || maximum == "unbounded" ? std::optional<std::size_t>{}
          : std::optional{occurrence(maximum, 1U)};
      value.before_field = static_cast<std::size_t>(std::count_if(ordered.begin(), ordered.end(),
          [](const auto& field) { return !field.attribute && !field.text; }));
      result.wildcards.push_back(std::move(value));
    };
    const auto aggregate_maximum = [](std::optional<std::size_t> branch,
                                       std::optional<std::size_t> repetitions) {
      if (!branch || !repetitions || (*branch != 0U &&
          *repetitions > std::numeric_limits<std::size_t>::max() / *branch)) {
        return std::optional<std::size_t>{};
      }
      return std::optional<std::size_t>{*branch * *repetitions};
    };
    const auto bind_choice = [&](const xml::element& node, const schema_bundle::document* owner) {
      choice_group group;
      group.minimum = occurrence(attribute(node, "minOccurs"), 1U);
      const auto maximum = attribute(node, "maxOccurs");
      group.max_repetitions = maximum == "unbounded" ? std::optional<std::size_t>{}
          : std::optional{occurrence(maximum, 1U)};
      group.maximum = group.max_repetitions.value_or(std::numeric_limits<std::size_t>::max());
      group.repeated = group.max_repetitions != std::optional<std::size_t>{1U};
      group.before_field = static_cast<std::size_t>(std::count_if(ordered.begin(), ordered.end(),
          [](const auto& value) { return !value.attribute && !value.text; }));
      const auto known_branches = std::count_if(node.children.begin(), node.children.end(),
          [](const auto& value) { return value.qualified_name.local == "element"; });
      for (const auto& child : node.children) {
        const auto child_maximum = attribute(child, "maxOccurs");
        if (child.qualified_name.local == "any" || child_maximum == "unbounded" ||
            occurrence(child_maximum, 1U) != 1U) { group.repeated = true; }
      }
      if (group.repeated && known_branches > 1) {
        throw std::invalid_argument{"Repeated XSD choices with multiple known branches require ordered variant storage"};
      }
      for (const auto& child : node.children) {
        if (child.qualified_name.local == "annotation") { continue; }
        choice_branch branch;
        branch.minimum = occurrence(attribute(child, "minOccurs"), 1U);
        const auto branch_maximum = attribute(child, "maxOccurs");
        branch.maximum = branch_maximum == "unbounded" ? std::optional<std::size_t>{}
            : std::optional{occurrence(branch_maximum, 1U)};
        if (child.qualified_name.local == "any") {
          branch.wildcard_index = result.wildcards.size();
          add_wildcard(child, false, owner);
          auto& wildcard = result.wildcards.back();
          wildcard.minimum = 0U;
          wildcard.maximum = aggregate_maximum(branch.maximum, group.max_repetitions);
          wildcard.before_field = group.before_field;
        } else if (child.qualified_name.local == "element") {
          bind_field(child, false, true, owner);
          auto& value = ordered.back();
          if (group.repeated && !value.repeated) {
            throw std::invalid_argument{"Repeated XSD choices require collection storage for their known branch"};
          }
          branch.source = value.source;
          if (group.repeated) {
            value.min_occurs = 0U;
            value.max_occurs = aggregate_maximum(branch.maximum, group.max_repetitions);
          } else if (value.repeated || value.max_occurs != std::optional<std::size_t>{1U}) {
            throw std::invalid_argument{"Singleton XSD choices require singleton optional fields"};
          }
          group.alternatives.push_back(value.source);
        } else {
          throw std::invalid_argument{"XSD choices require direct element or wildcard branches"};
        }
        group.branches.push_back(std::move(branch));
      }
      result.choices.push_back(std::move(group));
    };
    const auto bind_sequence = [&](auto&& self, const xml::element& sequence,
                                   const schema_bundle::document* owner) -> void {
      if (occurrence(attribute(sequence, "minOccurs"), 1U) != 1U ||
          occurrence(attribute(sequence, "maxOccurs"), 1U) != 1U) {
        throw std::invalid_argument{"Optional or repeated XSD sequences require grouped occurrence storage"};
      }
      for (const auto& child : sequence.children) {
        if (child.qualified_name.local == "element") { bind_field(child, false, false, owner); }
        else if (child.qualified_name.local == "sequence") { self(self, child, owner); }
        else if (child.qualified_name.local == "choice") { bind_choice(child, owner); }
        else if (child.qualified_name.local == "group") {
          if (occurrence(attribute(child, "minOccurs"), 1U) != 1U ||
              occurrence(attribute(child, "maxOccurs"), 1U) != 1U) {
            throw std::invalid_argument{"Optional or repeated XSD model groups require grouped occurrence storage"};
          }
          const auto selected = resolve_model_group(*bundle_, *owner, child);
          if (selected.node->qualified_name.local == "choice") { bind_choice(*selected.node, selected.owner); }
          else { self(self, *selected.node, selected.owner); }
        }
        else if (child.qualified_name.local == "any") { add_wildcard(child, false, owner); }
        else if (child.qualified_name.local != "annotation") { throw std::invalid_argument{"Unsupported optimized XSD sequence"}; }
      }
    };
    if (content.mixed) {
      if (std::any_of(content.children.begin(), content.children.end(), [](const auto& component) {
            const auto& kind = component.node->qualified_name.local;
            return kind != "attribute" && kind != "anyAttribute";
          })) { throw std::invalid_argument{"Mixed XSD content with child particles requires ordered text storage"}; }
      const auto text = std::find_if(result.fields.begin(), result.fields.end(), [](const auto& value) { return value.text; });
      if (text == result.fields.end() || leaf(*text->source).name != "string" || !bound.insert(text->source).second) {
        throw std::invalid_argument{"Text-only mixed XSD content requires one explicitly mapped string text field"};
      }
      auto value = *text;
      value.facets.lexical_kind = "string";
      ordered.push_back(std::move(value));
    }
    for (const auto& component : content.children) {
      const auto& node = *component.node;
      const auto* owner = component.owner;
      if (node.qualified_name.local == "sequence") { bind_sequence(bind_sequence, node, owner); }
      else if (node.qualified_name.local == "choice") { bind_choice(node, owner); }
      else if (node.qualified_name.local == "group") {
        if (occurrence(attribute(node, "minOccurs"), 1U) != 1U || occurrence(attribute(node, "maxOccurs"), 1U) != 1U) {
          throw std::invalid_argument{"Optional or repeated XSD model groups require grouped occurrence storage"};
        }
        const auto selected = resolve_model_group(*bundle_, *owner, node);
        if (selected.node->qualified_name.local == "choice") { bind_choice(*selected.node, selected.owner); }
        else { bind_sequence(bind_sequence, *selected.node, selected.owner); }
      }
      else if (node.qualified_name.local == "attribute") { bind_field(node, true, false, owner); }
      else if (node.qualified_name.local == "anyAttribute") { add_wildcard(node, true, owner); }
      else if (node.qualified_name.local == "simpleContent") {
        for (const auto& extension : node.children) {
          if (extension.qualified_name.local == "annotation") { continue; }
          if (extension.qualified_name.local != "extension") {
            throw std::invalid_argument{"XSD simpleContent requires a supported extension"};
          }
          auto text = std::find_if(result.fields.begin(), result.fields.end(), [](const auto& value) { return value.text; });
          if (text == result.fields.end() || !bound.insert(text->source).second) {
            throw std::invalid_argument{"XSD simpleContent requires one explicitly mapped text field"};
          }
          auto scalar = extension;
          set_attribute(scalar, "type", std::string{attribute(extension, "base")});
          const auto resolved = resolve_scalar(*bundle_, scalar);
          auto value = *text;
          value.facets = resolved.facets;
          ordered.push_back(std::move(value));
          const auto attributes = expand_complex_type(*bundle_, *owner, extension);
          for (const auto& item : attributes.children) {
            const auto& child = *item.node;
            if (child.qualified_name.local == "attribute") { bind_field(child, true, false, item.owner); }
            else if (child.qualified_name.local == "anyAttribute") { add_wildcard(child, true, item.owner); }
            else if (child.qualified_name.local != "annotation") {
              throw std::invalid_argument{"Unsupported XSD simpleContent extension child"};
            }
          }
        }
      }
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
  for (const auto& [path, bundle] : inline_bundles_) {
    (void)path;
    for (const auto& document : bundle->documents) { result.push_back(document.path); }
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

std::string schema::bundle_identity() const {
  return bundle_ ? bundle_->identity : std::string{};
}

// Reject unsupported value-space contracts before selecting a backend's implemented XML subset.
void validate_backend(const record& contract, std::string_view language) {
  const auto namespace_value = [&](auto&& self, const scalar_facets& facets) -> bool {
    return facets.lexical_kind == "QName" || facets.list_item_kind == "QName" ||
        !facets.qname_enumeration.empty() ||
        std::any_of(facets.union_members.begin(), facets.union_members.end(),
            [&](const auto& member) { return self(self, member); });
  };
  const auto check_unions = [&](auto&& self, const scalar_facets& facets) -> void {
    if (facets.list_item_kind == "QName" && !facets.enumeration.empty()) {
      throw std::invalid_argument{"XSD QName list enumeration requires expanded item identities"};
    }
    if (!facets.union_members.empty() &&
        (!facets.enumeration.empty() || !facets.qname_enumeration.empty())) {
      throw std::invalid_argument{"XSD union enumeration requires selected-member value identities"};
    }
    for (const auto& alternative : facets.union_members) { self(self, alternative); }
  };
  for (const auto& field : contract.fields) {
    check_unions(check_unions, field.facets);
    if (!field.facets.union_members.empty() && field.fixed_value) {
      throw std::invalid_argument{"XSD union fixed values require selected-member value identities"};
    }
    if (!field.facets.union_members.empty() && field.default_value &&
        namespace_value(namespace_value, field.facets)) {
      throw std::invalid_argument{"XSD QName union defaults require expanded schema-scope identities"};
    }
  }
  if (language == "cpp") {
    for (const auto& field : contract.fields) {
      if (!field.attribute && !field.text && !field.facets.union_members.empty() &&
          namespace_value(namespace_value, field.facets)) {
        throw std::invalid_argument{"C++ QName child values require per-field expanded namespace identities"};
      }
    }
    return;
  }
  if (language == "python") {
    for (const auto& choice : contract.choices) {
      if (choice.repeated) {
        if (choice.alternatives.size() != 1U) {
          throw std::invalid_argument{"Repeated XML choices require one typed branch for python"};
        }
        const auto selected = std::find_if(contract.fields.begin(), contract.fields.end(), [&](const auto& field) {
          return field.source == choice.alternatives.front();
        });
        if (selected == contract.fields.end() || !selected->repeated || selected->attribute ||
            selected->text || selected->wrapper) {
          throw std::invalid_argument{"Repeated XML choices require one unwrapped repeated field for python"};
        }
      }
    }
    for (const auto& wildcard : contract.wildcards) {
      if (wildcard.attribute && wildcard.process_contents == "strict") {
        throw std::invalid_argument{"Strict XML attribute wildcards are not implemented for python"};
      }
    }
    const std::set<std::string> kinds{"", "string", "normalizedString", "token", "anyURI", "boolean",
        "byte", "unsignedByte", "short", "unsignedShort", "int", "unsignedInt", "long", "unsignedLong",
        "integer", "positiveInteger", "negativeInteger", "nonPositiveInteger", "nonNegativeInteger",
        "float", "double", "decimal", "QName", "language", "Name", "NCName", "NMTOKEN", "NMTOKENS",
        "ID", "IDREF", "ENTITY", "IDREFS", "ENTITIES"};
    const auto check = [&](auto&& self, const scalar_facets& facets) -> void {
      if (!kinds.contains(facets.lexical_kind) || !kinds.contains(facets.list_item_kind)) {
        throw std::invalid_argument{"XML lexical type is not implemented for python"};
      }
      for (const auto& alternative : facets.union_members) { self(self, alternative); }
    };
    for (const auto& value : contract.fields) {
      if (value.value_projection) {
        throw std::invalid_argument{"XML value projections are not implemented for python"};
      }
      if (value.facets.list_item_kind == "QName" &&
          (value.fixed_value || !value.facets.enumeration.empty())) {
        throw std::invalid_argument{"QName list fixed/enumeration values require expanded item identities"};
      }
      check(check, value.facets);
    }
    return;
  }
  if (!contract.choices.empty() || !contract.wildcards.empty() ||
      (contract.unknown && *contract.unknown != xml_unknown_policy::reject)) {
    throw std::invalid_argument{"XML choices, wildcards and extension policies are not implemented for " +
                                std::string{language}};
  }
  const std::set<std::string> primitives{"", "string", "boolean", "byte", "unsignedByte", "short",
      "unsignedShort", "int", "unsignedInt", "long", "unsignedLong", "float", "double"};
  for (const auto& value : contract.fields) {
    const auto& facets = value.facets;
    const bool whitespace_changes_value = !facets.whitespace.empty() &&
        (facets.lexical_kind == "string" || facets.lexical_kind.empty());
    if (value.list || value.qname || value.prefix_list || value.value_projection || value.default_value || value.fixed_value ||
        facets.min_inclusive || facets.max_inclusive || facets.min_exclusive || facets.max_exclusive ||
        facets.length || facets.min_length || facets.max_length || !facets.enumeration.empty() ||
        !facets.patterns.empty() || !facets.list_item_kind.empty() || whitespace_changes_value ||
        !primitives.contains(facets.lexical_kind)) {
      throw std::invalid_argument{"XML scalar restrictions or lexical adapters are not implemented for " +
                                  std::string{language} + ": " + value.source->name};
    }
  }
}

std::vector<compatibility_diagnostic> compare_contracts(const record& previous, const record& current) {
  std::vector<compatibility_diagnostic> result{};
  if (previous.root != current.root) { result.push_back({{}, "Root expanded name changed"}); }
  std::map<std::string, const field*> previous_fields{};
  const auto normalized_facets = [](const field& value) {
    auto result = value.facets;
    const std::map<std::string, std::string> primitives{{"string", "string"}, {"bool", "boolean"},
        {"int8", "byte"}, {"uint8", "unsignedByte"}, {"int16", "short"}, {"uint16", "unsignedShort"},
        {"int32", "int"}, {"uint32", "unsignedInt"}, {"int64", "long"}, {"uint64", "unsignedLong"},
        {"float", "float"}, {"double", "double"}, {"float32", "float"}, {"float64", "double"}};
    const auto primitive = primitives.find(leaf(*value.source).name);
    if (primitive != primitives.end() && result.lexical_kind == primitive->second && !value.list) {
      if (primitive->second != "string" && result.whitespace == "collapse") { result.whitespace.clear(); }
      result.lexical_kind.clear();
    }
    return result;
  };
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
    if (normalized_facets(old) != normalized_facets(value) || old.fixed_value != value.fixed_value ||
        old.default_value != value.default_value) {
      result.push_back({value.source->name, "Scalar restrictions or default/fixed semantics changed"});
    }
    previous_fields.erase(found);
  }
  for (const auto& [key, value] : previous_fields) {
    (void)value; result.push_back({key, "Current contract removes a field"});
  }
  if (previous_elements != current_elements) { result.push_back({{}, "Element sequence order changed"}); }
  const auto same_choices = [&] {
    if (previous.choices.size() != current.choices.size()) { return false; }
    for (std::size_t index{}; index < previous.choices.size(); ++index) {
      const auto& before = previous.choices[index];
      const auto& after = current.choices[index];
      if (before.minimum != after.minimum || before.maximum != after.maximum ||
          before.repeated != after.repeated || before.before_field != after.before_field ||
          before.max_repetitions != after.max_repetitions || before.branches.size() != after.branches.size() ||
          before.alternatives.size() != after.alternatives.size()) { return false; }
      for (std::size_t branch{}; branch < before.branches.size(); ++branch) {
        const auto& left = before.branches[branch];
        const auto& right = after.branches[branch];
        if (left.minimum != right.minimum || left.maximum != right.maximum ||
            left.wildcard_index != right.wildcard_index || (left.source == nullptr) != (right.source == nullptr) ||
            (left.source && left.source->name != right.source->name)) { return false; }
      }
      for (std::size_t alternative{}; alternative < before.alternatives.size(); ++alternative) {
        if (before.alternatives[alternative]->name != after.alternatives[alternative]->name) { return false; }
      }
    }
    return true;
  };
  if (!same_choices()) { result.push_back({{}, "Choice occurrence or branch contract changed"}); }
  const auto same_wildcards = [&] {
    if (previous.wildcards.size() != current.wildcards.size()) { return false; }
    for (std::size_t index{}; index < previous.wildcards.size(); ++index) {
      const auto& before = previous.wildcards[index];
      const auto& after = current.wildcards[index];
      if (before.attribute != after.attribute || before.namespace_rule != after.namespace_rule ||
          before.process_contents != after.process_contents || before.minimum != after.minimum ||
          before.maximum != after.maximum || before.before_field != after.before_field ||
          before.target_namespace.value_or(previous.root.namespace_uri) !=
              after.target_namespace.value_or(current.root.namespace_uri)) { return false; }
    }
    return true;
  };
  if (!same_wildcards() || previous.unknown != current.unknown) {
    result.push_back({{}, "Wildcard or unknown-extension policy changed"});
  }
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
    if (!records[index].choices.empty() || !records[index].wildcards.empty()) {
      throw std::invalid_argument{"XSD export of choice and wildcard contracts requires content-model export"};
    }
    auto root = node("element");
    set_attribute(root, "name", records[index].root.local);
    set_attribute(root, "type", type_qname(type_names.at(sources[index])));
    document.children.push_back(std::move(root));
    auto complex = node("complexType");
    set_attribute(complex, "name", type_names.at(sources[index]));
    auto sequence = node("sequence");
    std::vector<xml::element> attributes{};
    for (const auto& value : records[index].fields) {
      if (!value.facets.union_members.empty() || !value.facets.pattern_groups.empty()) {
        throw std::invalid_argument{"XSD export of union or inherited pattern contracts requires nested simple-type export"};
      }
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
        const auto xsd_type = value.facets.lexical_kind.empty() ? primitive->second
                                                               : value.facets.lexical_kind;
        const auto& facets = value.facets;
        const bool restricted = facets.min_inclusive || facets.max_inclusive || facets.min_exclusive ||
            facets.max_exclusive || facets.length || facets.min_length || facets.max_length ||
            !facets.enumeration.empty() || !facets.patterns.empty() || !facets.whitespace.empty() ||
            !facets.list_item_kind.empty();
        if (!restricted) { set_attribute(member_node, "type", "xs:" + xsd_type); }
        else {
          auto simple = node("simpleType");
          auto restriction = node("restriction");
          if (!facets.list_item_kind.empty()) {
            auto base = node("simpleType");
            auto list = node("list");
            set_attribute(list, "itemType", "xs:" + facets.list_item_kind);
            base.children.push_back(std::move(list));
            restriction.children.push_back(std::move(base));
          } else { set_attribute(restriction, "base", "xs:" + xsd_type); }
          const auto append = [&](std::string_view kind, std::string facet_value) {
            auto facet = node(kind);
            set_attribute(facet, "value", std::move(facet_value));
            restriction.children.push_back(std::move(facet));
          };
          if (facets.min_inclusive) { append("minInclusive", *facets.min_inclusive); }
          if (facets.max_inclusive) { append("maxInclusive", *facets.max_inclusive); }
          if (facets.min_exclusive) { append("minExclusive", *facets.min_exclusive); }
          if (facets.max_exclusive) { append("maxExclusive", *facets.max_exclusive); }
          if (facets.length) { append("length", std::to_string(*facets.length)); }
          if (facets.min_length) { append("minLength", std::to_string(*facets.min_length)); }
          if (facets.max_length) { append("maxLength", std::to_string(*facets.max_length)); }
          for (const auto& item : facets.enumeration) { append("enumeration", item); }
          for (const auto& item : facets.patterns) { append("pattern", item); }
          if (!facets.whitespace.empty()) { append("whiteSpace", facets.whitespace); }
          simple.children.push_back(std::move(restriction));
          member_node.children.push_back(std::move(simple));
        }
      }
      if (value.default_value) { set_attribute(member_node, "default", *value.default_value); }
      if (value.fixed_value) { set_attribute(member_node, "fixed", *value.fixed_value); }
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
