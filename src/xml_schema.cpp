// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_schema.hpp"
#include "xsd_import.hpp"
#include <rohit/stream.hpp>
#include "xml_portable_writer.hpp"
#include "document_profile_contract.hpp"
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
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace rohit::serializer::writer::xml_binding {
namespace {
constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};
struct expanded_name_less {
  // Order namespace identities before local names for deterministic bundle output.
  bool operator()(const xml::name& left, const xml::name& right) const {
    return std::tie(left.namespace_uri, left.local) < std::tie(right.namespace_uri, right.local);
  }
};
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

// Retain only lexical-token prefixes whose binding or explicit absence affects a union default.
std::map<std::string, std::string> union_literal_scope(std::string_view text, const xml::element& scope) {
  std::map<std::string, std::string> result{};
  const auto retain = [&](std::string_view prefix) {
    if (prefix == "xml") { result.emplace(prefix, "http://www.w3.org/XML/1998/namespace"); return; }
    const auto found = scope.namespace_bindings.find(std::string{prefix});
    result.emplace(prefix, found == scope.namespace_bindings.end() ? "" : found->second);
  };
  retain("");
  for (const auto token : lexical_words(text)) {
    const auto colon = token.find(':');
    if (colon != std::string_view::npos) { retain(token.substr(0U, colon)); }
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
// Bound schema-scope list identity expansion, including repeated long namespace URIs.
std::vector<xml::name> resolve_qname_list(const xml::element& node, std::string_view lexical,
    std::size_t& remaining_bytes) {
  std::vector<xml::name> result{};
  for (const auto token : lexical_words(lexical)) {
    if (sizeof(xml::name) > remaining_bytes) {
      throw std::length_error{"XSD QName list identity expansion byte limit exceeded"};
    }
    remaining_bytes -= sizeof(xml::name);
    auto item = resolve(node, token);
    for (const auto size : {item.local.size(), item.namespace_uri.size()}) {
      if (size > remaining_bytes) {
        throw std::length_error{"XSD QName list identity expansion byte limit exceeded"};
      }
      remaining_bytes -= size;
    }
    result.push_back(std::move(item));
  }
  return result;
}
// Apply the same bounded expansion to each schema default/fixed list value.
std::vector<xml::name> resolve_qname_list(const xml::element& node, std::string_view lexical) {
  constexpr std::size_t maximum_expanded_list_bytes{16U * 1024U * 1024U};
  auto remaining_bytes = maximum_expanded_list_bytes;
  return resolve_qname_list(node, lexical, remaining_bytes);
}
// Native enums use complete authored identifier spellings, never numeric, QName or list value spaces.
bool native_enum_string_kind(std::string_view kind, std::string_view whitespace) {
  return kind == "string" && (whitespace.empty() || whitespace == "preserve");
}
// Parse cardinalities before provider projection so large finite bounds retain exact semantics.
std::size_t occurrence(std::string_view text, std::size_t fallback);
// Reject every construct/attribute whose semantics the generated subset cannot completely enforce.
void inspect(const xml::element& node, bool global_element = false) {
  if (node.qualified_name.namespace_uri != xsd_namespace) {
    throw std::invalid_argument{"Unsupported foreign node in optimized XSD subset"};
  }
  const auto& kind = node.qualified_name.local;
  if (kind == "annotation") { return; }
  if (kind == "element" && !global_element && std::any_of(node.children.begin(), node.children.end(),
      [](const auto& child) {
        return child.qualified_name.local == "key" || child.qualified_name.local == "unique" ||
            child.qualified_name.local == "keyref";
      })) {
    throw std::invalid_argument{"Supported XSD identity constraints require one global element owner scope"};
  }
  if (node.text.find_first_not_of(" \t\r\n") != std::string::npos) {
    throw std::invalid_argument{"Significant text outside XSD annotation is unsupported"};
  }
  for (const auto* key : {"mixed", "abstract", "nillable"}) {
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
      {"element", {"name", "type", "ref", "minOccurs", "maxOccurs", "form", "id", "default", "fixed", "nillable", "abstract", "block", "final"}},
      {"attribute", {"name", "type", "ref", "use", "form", "id", "default", "fixed"}},
      {"simpleType", {"name", "id", "final"}}, {"restriction", {"base", "id"}},
      {"complexType", {"name", "id", "mixed", "abstract", "block", "final"}},
      {"sequence", {"id", "minOccurs", "maxOccurs"}},
      {"choice", {"id", "minOccurs", "maxOccurs"}},
      {"all", {"id", "minOccurs", "maxOccurs"}},
      {"key", {"id", "name"}}, {"unique", {"id", "name"}},
      {"keyref", {"id", "name", "refer"}},
      {"selector", {"id", "xpath"}}, {"field", {"id", "xpath"}},
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
  for (const auto& child : node.children) {
    inspect(child, kind == "schema" && child.qualified_name.local == "element");
  }
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
    if ((global || source.qualified_name.local == "key" || source.qualified_name.local == "unique" ||
        source.qualified_name.local == "keyref") && !attribute(source, "name").empty()) {
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
    for (const auto* key : {"type", "base", "ref", "itemType", "refer"}) {
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
  // A chameleon source has a separate component instance in every adopted namespace.
  // Insert each instance before following edges so finite include/import cycles deduplicate.
  using instance_key = std::pair<std::filesystem::path, std::string>;
  std::set<instance_key> loaded{};
  std::map<std::filesystem::path, std::string> declared_targets{};
  const auto load = [&](auto&& self, const std::filesystem::path& file,
                        std::string_view required_namespace, std::size_t depth,
                        bool include = false) -> void {
    if (const auto found = declared_targets.find(file); found != declared_targets.end()) {
      const auto& declared = found->second;
      const auto effective = include && declared.empty() ? std::string{required_namespace} : declared;
      if (depth != 0U && effective != required_namespace) {
        throw std::invalid_argument{"XSD include/import targetNamespace mismatch"};
      }
      if (loaded.contains({file, effective})) { return; }
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
    const auto declared_target = std::string{attribute(tree, "targetNamespace")};
    declared_targets.emplace(file, declared_target);
    const auto target = include && declared_target.empty()
        ? std::string{required_namespace} : declared_target;
    if (depth != 0U && target != required_namespace) {
      throw std::invalid_argument{"XSD include/import targetNamespace mismatch"};
    }
    if (!loaded.emplace(file, target).second) { return; }
    if (declared_target.empty() && !target.empty()) {
      // Change component-reference QNames only. QName-valued defaults and facets retain
      // their authored namespace contexts and therefore their original value identities.
      const auto adopt = [&](auto&& visit, xml::element& node) -> void {
        std::string prefix{"serializer_adopted"};
        while (node.namespace_bindings.contains(prefix)) { prefix += '_'; }
        const auto adopted_name = [&](std::string_view lexical) {
          const auto name = resolve(node, lexical);
          return name.namespace_uri.empty() ? prefix + ':' + name.local : std::string{lexical};
        };
        for (const auto* key : {"type", "base", "ref", "itemType", "refer"}) {
          if (!attribute(node, key).empty()) {
            set_attribute(node, key, adopted_name(attribute(node, key)));
          }
        }
        if (!attribute(node, "memberTypes").empty()) {
          std::string members;
          for (const auto lexical : lexical_words(attribute(node, "memberTypes"))) {
            if (!members.empty()) { members += ' '; }
            members += adopted_name(lexical);
          }
          set_attribute(node, "memberTypes", std::move(members));
        }
        node.namespace_bindings[prefix] = target;
        for (auto& child : node.children) { visit(visit, child); }
      };
      adopt(adopt, tree);
      set_attribute(tree, "targetNamespace", target);
    }
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
      // XML character data is UTF-8, independent of the embedding process's native code page.
      const std::filesystem::path relative{std::u8string{location.begin(), location.end()}};
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
      self(self, child, imported_namespace, depth + 1U, kind == "include");
    }
  };
  load(load, root_path, {}, 0U);
  std::sort(result->documents.begin(), result->documents.end(), [&](const auto& first, const auto& second) {
    return std::pair{relative_path_text(first.path, root_directory), first.target} <
        std::pair{relative_path_text(second.path, root_directory), second.target};
  });
  std::string identity_input{"Serializer XSD bundle 1\n"};
  for (const auto& document : result->documents) {
    const auto relative = relative_path_text(document.path, root_directory);
    // Length prefixes distinguish arbitrary path/content delimiters without relying on escaping.
    identity_input += std::to_string(relative.size()) + ":" + relative;
    identity_input += std::to_string(document.bytes.size()) + ":" + document.bytes;
    if (std::count_if(result->documents.begin(), result->documents.end(), [&](const auto& other) {
          return other.path == document.path;
        }) > 1) {
      identity_input += std::to_string(document.target.size()) + ":" + document.target;
    }
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
    std::size_t identity_count{};
    constexpr auto maximum_identity_constraints = xml::identity_limits{}.max_constraints;
    for (const auto& document : result->documents) {
      for (const auto& node : document.root.children) {
        const auto constraints = static_cast<std::size_t>(std::count_if(node.children.begin(), node.children.end(),
            [](const auto& child) {
              return child.qualified_name.local == "key" || child.qualified_name.local == "unique" ||
                  child.qualified_name.local == "keyref";
            }));
        if (constraints > maximum_identity_constraints - identity_count) {
          throw std::length_error{"XSD bundle aggregate identity-constraint limit exceeded"};
        }
        identity_count += constraints;
        if (node.qualified_name.local == "element" && std::any_of(node.children.begin(), node.children.end(),
            [](const auto& child) {
              return child.qualified_name.local == "key" || child.qualified_name.local == "unique" ||
                  child.qualified_name.local == "keyref";
            })) {
          (void)bind_identity_constraints(*result, node, document.target);
        }
      }
    }
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
  } else if (type.is_digest()) {
    // The model contract retains the digest extent for shared hexadecimal validation.
    return;
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

// Keep configured URI/IRI references free of characters that require escaping.
void check_namespace(std::string_view uri) {
  for (const auto character : uri) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte <= 0x20U || byte == 0x7fU || std::string_view{"\"<>^`{}|\\"}.find(character) != std::string_view::npos) {
      throw std::invalid_argument{"XML namespaces require URI/IRI references with escaped reserved characters"};
    }
  }
}

// Parse an expanded configured name without depending on document prefix bindings.
xml::name contract_name(std::string_view value, std::string_view default_namespace) {
  xml::name result{std::string{value}, std::string{default_namespace}};
  if (value.starts_with('{')) {
    const auto close = value.find('}');
    if (close == std::string_view::npos) {
      throw std::invalid_argument{"XML contract expanded name requires {namespace}local"};
    }
    result = {std::string{value.substr(close + 1U)}, std::string{value.substr(1U, close - 1U)}};
  }
  check_namespace(result.namespace_uri);
  xml::element check{};
  check.qualified_name = result;
  (void)xml::write(check);
  return result;
}

// Split explicit vocabulary URI lists on XML whitespace; URI commas retain their meaning.
std::vector<std::string> contract_namespaces(std::string_view value) {
  std::vector<std::string> result{};
  for (const auto word : lexical_words(value)) { result.emplace_back(word); }
  return result;
}

// Bind independent, explicitly positioned unknown-vocabulary slots into owning runtime metadata.
xml::extension_contract bind_extension_slots(const record& owner,
    const std::map<std::string, std::string>& options) {
  xml::extension_contract result{};
  result.core_namespace = owner.root.namespace_uri;
  result.distinct_known_kinds = true;
  for (const auto& field : owner.fields) {
    if (field.text) { continue; }
    (field.attribute ? result.known_attributes : result.known_names).push_back(
        field.wrapper ? *field.wrapper : field.qualified_name);
    if (!field.attribute) { result.known_positions.push_back(static_cast<std::size_t>(&field - owner.fields.data())); }
  }
  if (owner.mixed) {
    for (const auto& child : owner.mixed->children) {
      result.known_names.push_back(child.qualified_name);
      result.known_positions.push_back(std::numeric_limits<std::size_t>::max());
    }
  }
  std::map<std::string, std::map<std::string, std::string>> slots{};
  for (const auto& [key, value] : options) {
    if (key == "extensions.required") { result.required_namespaces = contract_namespaces(value); }
    if (!key.starts_with("extension.")) { continue; }
    const auto tail = std::string_view{key}.substr(std::string_view{"extension."}.size());
    const auto separator = tail.find('.');
    if (separator == std::string_view::npos) {
      throw std::invalid_argument{"XML extension slot property is missing"};
    }
    slots[std::string{tail.substr(0U, separator)}].emplace(tail.substr(separator + 1U), value);
  }
  for (const auto& [id, properties] : slots) {
    if (!properties.contains("kind") || !properties.contains("namespaces") ||
        !properties.contains("policy")) {
      throw std::invalid_argument{"XML extension slot requires kind, namespaces and policy"};
    }
    xml::extension_slot_contract slot{};
    slot.id = id;
    slot.attribute = properties.at("kind") == "attribute";
    slot.namespaces = contract_namespaces(properties.at("namespaces"));
    const auto& policy = properties.at("policy");
    slot.policy = policy == "preserve" ? xml::unknown_policy::preserve :
        policy == "skip" ? xml::unknown_policy::skip : xml::unknown_policy::reject;
    if (properties.contains("minimum")) { slot.minimum = occurrence(properties.at("minimum"), 0U); }
    if (properties.contains("maximum") && properties.at("maximum") != "unbounded") {
      slot.maximum = occurrence(properties.at("maximum"), 0U);
    }
    slot.before_field = owner.fields.size() + (owner.mixed ? owner.mixed->children.size() : 0U);
    if (slot.attribute && properties.contains("before")) {
      throw std::invalid_argument{"XML attribute extension slots have no field position"};
    }
    if (properties.contains("before") && properties.at("before") != "$end") {
      if (slot.attribute) { throw std::invalid_argument{"XML attribute extension slots have no field position"}; }
      const auto found = std::find_if(owner.fields.begin(), owner.fields.end(), [&](const auto& field) {
        return field.source->name == properties.at("before");
      });
      if (found == owner.fields.end() || found->attribute || found->text) {
        throw std::invalid_argument{"XML extension before selector requires an element member"};
      }
      slot.before_field = static_cast<std::size_t>(found - owner.fields.begin());
    }
    result.slots.push_back(std::move(slot));
  }
  xml::validate_extension_contract(result);
  return result;
}
// Lower XSD wildcard predicates independently of their semantic declaration and retention policies.
void bind_skip_wildcard_slots(record& owner) {
  std::size_t ordinal{};
  for (const auto& wildcard : owner.wildcards) {
    const auto index = ordinal++;
    if (!owner.extensions) { owner.extensions = bind_extension_slots(owner, {}); }
    xml::extension_slot_contract slot{};
    slot.id = "srl_xsd_wildcard_" + std::to_string(index);
    while (std::any_of(owner.extensions->slots.begin(), owner.extensions->slots.end(),
        [&](const auto& previous) { return previous.id == slot.id; })) { slot.id += '_'; }
    slot.attribute = wildcard.attribute;
    slot.policy = owner.unknown == xml_unknown_policy::skip ? xml::unknown_policy::skip : xml::unknown_policy::preserve;
    slot.process_contents = wildcard.process_contents == "skip" ? xml::wildcard_process_contents::skip :
        wildcard.process_contents == "lax" ? xml::wildcard_process_contents::lax : xml::wildcard_process_contents::strict;
    slot.minimum = wildcard.minimum;
    slot.maximum = wildcard.maximum;
    slot.before_field = owner.fields.size();
    std::size_t children{};
    for (std::size_t field{}; field < owner.fields.size(); ++field) {
      if (owner.fields[field].attribute || owner.fields[field].text) { continue; }
      if (children++ == wildcard.before_field) { slot.before_field = field; break; }
    }
    const auto target = wildcard.target_namespace.value_or(owner.root.namespace_uri);
    if (wildcard.namespace_rule == "##any") {
      slot.namespace_match = xml::extension_namespace_match::any;
    } else if (wildcard.namespace_rule == "##other") {
      slot.namespace_match = xml::extension_namespace_match::except;
      slot.namespaces = {"", target};
      if (target.empty()) { slot.namespaces.resize(1U); }
    } else {
      slot.namespace_match = xml::extension_namespace_match::one_of;
      for (const auto& token : lexical_words(wildcard.namespace_rule)) {
        std::string uri = token == "##local" ? "" : token == "##targetNamespace" ? target : std::string{token};
        if (uri.starts_with("##")) { throw std::invalid_argument{"Invalid XSD wildcard namespace token"}; }
        if (std::find(slot.namespaces.begin(), slot.namespaces.end(), uri) == slot.namespaces.end()) {
          slot.namespaces.push_back(std::move(uri));
        }
      }
    }
    owner.extensions->slots.push_back(std::move(slot));
  }
  if (owner.unknown && *owner.unknown != xml_unknown_policy::reject) {
    for (const bool attribute : {false, true}) {
      if (std::any_of(owner.wildcards.begin(), owner.wildcards.end(),
          [&](const auto& wildcard) { return wildcard.attribute == attribute; })) { continue; }
      if (!owner.extensions) { owner.extensions = bind_extension_slots(owner, {}); }
      xml::extension_slot_contract slot{};
      slot.id = attribute ? "srl_unknown_attributes" : "srl_unknown_children";
      while (std::any_of(owner.extensions->slots.begin(), owner.extensions->slots.end(),
          [&](const auto& previous) { return previous.id == slot.id; })) { slot.id += '_'; }
      slot.attribute = attribute;
      slot.before_field = attribute || std::any_of(owner.fields.begin(), owner.fields.end(),
          [](const auto& field) { return field.text; }) || owner.mixed || !owner.all_groups.empty() ? owner.fields.size() +
          (owner.mixed ? owner.mixed->children.size() : 0U) : std::numeric_limits<std::size_t>::max();
      slot.namespace_match = xml::extension_namespace_match::foreign_fallback;
      slot.policy = *owner.unknown == xml_unknown_policy::skip ? xml::unknown_policy::skip : xml::unknown_policy::preserve;
      slot.maximum.reset();
      slot.namespaces = {""};
      if (!owner.root.namespace_uri.empty()) { slot.namespaces.push_back(owner.root.namespace_uri); }
      for (const auto& previous : owner.extensions->slots) {
        if (previous.attribute != attribute) { continue; }
        for (const auto& uri : previous.namespaces) {
          if (std::find(slot.namespaces.begin(), slot.namespaces.end(), uri) == slot.namespaces.end()) {
            slot.namespaces.push_back(uri);
          }
        }
      }
      owner.extensions->slots.push_back(std::move(slot));
    }
  }
  if (owner.extensions) { xml::validate_extension_contract(*owner.extensions); }
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
      result.facets.qname_list_enumeration.clear();
      result.facets.union_enumeration.clear();
      result.facets.union_enumeration_scopes.clear();
      if (!result.facets.union_members.empty() || !result.facets.list_item_facets.empty()) {
        scalar_facets selection{};
        selection.union_members = result.facets.union_members;
        selection.lexical_kind = result.facets.lexical_kind;
        selection.whitespace = result.facets.whitespace;
        selection.list_item_kind = result.facets.list_item_kind;
        selection.list_item_facets = result.facets.list_item_facets;
        const auto prepared = compile_scalar_contract(selection, false);
        for (const auto& facet : restriction.children) {
          if (facet.qualified_name.local == "enumeration") {
            result.facets.union_enumeration.push_back(prepared.identify(attribute(facet, "value"), facet));
            result.facets.union_enumeration_scopes.push_back(union_literal_scope(attribute(facet, "value"), facet));
          }
        }
      }
      if (result.primitive.local == "QName") {
        for (const auto& facet : restriction.children) {
          if (facet.qualified_name.local == "enumeration") {
            result.facets.qname_enumeration.push_back(resolve(facet, attribute(facet, "value")));
          }
        }
      }
      if (result.facets.list_item_kind == "QName") {
        for (const auto& facet : restriction.children) {
          if (facet.qualified_name.local != "enumeration") { continue; }
          result.facets.qname_list_enumeration.push_back(
              resolve_qname_list(facet, attribute(facet, "value"), remaining_facet_bytes));
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
        const auto type = attribute(child, "itemType");
        const auto selected_item = type.empty() ? std::optional<xml::name>{} : resolve(child, type);
        auto item = self(self, child, selected_item, depth + 1U);
        const auto atomic = [&](auto&& visit, const scalar_facets& facets) -> bool {
          return facets.list_item_kind.empty() && facets.list_item_facets.empty() &&
              std::all_of(facets.union_members.begin(), facets.union_members.end(),
                  [&](const auto& member) { return visit(visit, member); });
        };
        if (!atomic(atomic, item.facets)) { throw std::invalid_argument{"XSD list items must be atomic values or unions of atomic values"}; }
        scalar_resolution result{{"string", std::string{xsd_namespace}}, {}};
        result.facets.list_item_kind = item.primitive.local;
        if (!selected_item || selected_item->namespace_uri != xsd_namespace) {
          result.facets.list_item_facets.push_back(std::move(item.facets));
        }
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
// Peel native optional and sequence layers before binding the scalar or record contract.
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
  (void)compile_scalar_contract(result.facets, false);
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
    if (compositor || (child.qualified_name.local != "sequence" && child.qualified_name.local != "choice" &&
        child.qualified_name.local != "all")) {
      throw std::invalid_argument{"XSD model groups require one supported sequence, choice or all"};
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
    return value.namespace_uri || value.root || value.ordered || value.scalar_placement || value.unknown || value.schema;
  };
  const auto scan = [&](auto&& self, const auto& nodes) -> void {
    for (const auto& node : nodes) {
      if (node->type == object_type::namespace_type) {
        const auto& space = static_cast<const namespace_node&>(*node);
        authored = authored || declared(space.xml);
        self(self, space.statements);
      } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
        const auto& owner = static_cast<const class_node&>(*node);
        if (owner.xml.ordered) {
          auto& contracts = options.classes[owner.get_full_name()].contracts;
          const auto selected = contracts.find("ordered");
          if (contracts.contains("mixed") ||
              (selected != contracts.end() && selected->second != *owner.xml.ordered)) {
            throw std::invalid_argument{"Configured XML content conflicts with schema-local ordered selection"};
          }
          contracts["ordered"] = *owner.xml.ordered;
          for (const auto& [branch, name] : owner.xml.ordered_children) {
            const auto key = "ordered.child." + branch;
            if (contracts.contains(key) && contracts.at(key) != name) {
              throw std::invalid_argument{"Configured XML child conflicts with schema-local ordered selection"};
            }
            contracts[key] = name;
          }
        }
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
  package_codegen::validate_package_options(statements, options);
  if (options.validation_scope != "input" && options.validation_scope != "output" && options.validation_scope != "both") {
    throw std::invalid_argument{"XML validation_scope must be input, output or both"};
  }
  if (!options.enabled && (options.runtime_xsd || !options.xsd.empty() || !options.classes.empty())) {
    throw std::invalid_argument{"XML configuration requires xml.enabled = true"};
  }
  if (options.enabled) {
    const schema bindings{options};
    // A mixed sequence's fragment is native storage for its parent's ordered content,
    // rather than an independently mapped XML record. Explicit class mappings still win.
    std::set<const class_node*> mixed_fragments{};
    if (language == "python" || language == "cpp" || language == "js" || language == "typescript" ||
        language == "java" || language == "csharp" || language == "go" || language == "kotlin" || language == "swift" ||
        language == "c" || language == "rust" || language == "xsd") {
      const auto collect_fragments = [&](auto&& self, const auto& nodes) -> void {
        for (const auto& node : nodes) {
          if (node->type == object_type::namespace_type) {
            self(self, static_cast<const namespace_node&>(*node).statements);
          } else if (node->type == object_type::class_type) {
            const auto& owner = static_cast<const class_node&>(*node);
            const auto configured = options.classes.find(owner.get_full_name());
            if (configured != options.classes.end() && (configured->second.contracts.contains("mixed") ||
                configured->second.contracts.contains("ordered"))) {
              const auto contract = bindings.bind(owner);
              if (contract.mixed && !options.classes.contains(contract.mixed->fragment->get_full_name())) {
                mixed_fragments.insert(contract.mixed->fragment);
              }
            }
          }
        }
      };
      collect_fragments(collect_fragments, statements);
    }
    const auto validate_references = [&](auto&& self, const auto& nodes) -> void {
      for (const auto& node : nodes) {
        if (node->type == object_type::namespace_type) {
          self(self, static_cast<const namespace_node&>(*node).statements);
        } else if (node->type == object_type::class_type || node->type == object_type::generic_definition) {
          const auto& owner = static_cast<const class_node&>(*node);
          const auto configured = options.classes.find(owner.get_full_name());
          if (configured != options.classes.end() && !configured->second.contracts.empty() &&
              (node->type == object_type::generic_definition || owner.reference)) {
            throw std::invalid_argument{
                "Shared XML content contracts require ordinary owning records; generic and reference bindings are not implemented"};
          }
          if (owner.reference && (!owner.reference->value_projection ||
              (language != "cpp" && language != "python" && language != "c" && language != "rust" &&
               language != "js" && language != "typescript" && language != "kotlin" && language != "swift" &&
               language != "java" && language != "csharp" && language != "go"))) {
            throw std::invalid_argument{
                "XML value projections are export-only for supported backends; durable field addresses have no XML binding"};
          }
          if (node->type == object_type::class_type && !owner.reference) {
            if (mixed_fragments.contains(&owner)) { continue; }
            const auto contract = bindings.bind(owner);
            // Helpers cannot become ordinary XML payloads through maps, variants or nested wrappers.
            const auto uses_mixed_fragment = [&](auto&& self, const type_name& type) -> bool {
              if (type.type == object_type::class_type && type.resolved_node &&
                  mixed_fragments.contains(static_cast<const class_node*>(type.resolved_node))) {
                return true;
              }
              return std::any_of(type.arguments.begin(), type.arguments.end(),
                  [&](const auto& argument) { return self(self, argument); });
            };
            for (const auto& field : contract.fields) {
              if (std::any_of(field.source->type_name_list.begin(), field.source->type_name_list.end(),
                      [&](const auto& type) { return uses_mixed_fragment(uses_mixed_fragment, type); }) ||
                  (field.source->key_type && uses_mixed_fragment(uses_mixed_fragment, *field.source->key_type))) {
                throw std::invalid_argument{"XML mixed fragment storage requires its owning mixed mapping: " +
                                            field.source->name};
              }
            }
            if (language != "xsd" || (contract.mixed && contract.mixed->element_only)) {
              validate_backend(contract, language);
            }
          }
        } else if (node->type == object_type::package_type && language != "cpp" && language != "python" &&
                   language != "js" && language != "typescript" && language != "java" && language != "go" &&
                     language != "csharp" && language != "kotlin" && language != "rust" && language != "swift" && language != "c") {
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

// Reuse the collector's immutable inventory rather than importing a second private AST graph.
schema::schema(const xml_options& options,
    std::span<const std::shared_ptr<const declaration_inventory>> inventories) : schema{options} {
  for (const auto& inventory : inventories) { register_inventory(inventory); }
}
// Ownership remains local to this writer; no process-global mutable schema registry is used.
void schema::register_inventory(std::shared_ptr<const declaration_inventory> inventory) {
  if (inventory && inventory->owner) { registered_inventories_.insert_or_assign(inventory->owner, std::move(inventory)); }
}

record schema::bind(const class_node& source, bool include_declarations) const {
  if (source.prepared_xml_validation) { return *source.prepared_xml_validation; }
  const auto model = bind_model_contract(source);
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
    mapping.contracts = selected.contracts;
  }
  if (source.xml.ordered) {
    const auto selected = mapping.contracts.find("ordered");
    if (mapping.contracts.contains("mixed") ||
        (selected != mapping.contracts.end() && selected->second != *source.xml.ordered)) {
      throw std::invalid_argument{"Configured XML content conflicts with schema-local ordered selection"};
    }
    mapping.contracts["ordered"] = *source.xml.ordered;
    for (const auto& [branch, name] : source.xml.ordered_children) {
      const auto key = "ordered.child." + branch;
      if (mapping.contracts.contains(key) && mapping.contracts.at(key) != name) {
        throw std::invalid_argument{"Configured XML child conflicts with schema-local ordered selection"};
      }
      mapping.contracts[key] = name;
    }
  }
  validate_xml_contract_options(mapping);
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
    fixed.options_.xsd = canonical;
    fixed.declaration_cache_ = declaration_cache_;
    fixed.registered_inventories_ = registered_inventories_;
    return fixed.bind(source, include_declarations);
  }
  record result{{mapping.root.empty() ? source.name : mapping.root, mapping.namespace_uri}, {}};
  result.model = model;
  if (bundle_ && std::any_of(mapping.contracts.begin(), mapping.contracts.end(), [](const auto& item) {
        return !item.first.starts_with("presence.") && item.first != "ordered" &&
            !item.first.starts_with("ordered.") && !item.first.starts_with("extension.") &&
            item.first != "extensions.required";
      })) {
    throw std::invalid_argument{"Shared XML content contracts cannot override a fixed XSD binding"};
  }
  if (mapping.contracts.contains("mixed") && mapping.contracts.contains("ordered")) {
    throw std::invalid_argument{"XML mixed and ordered content mappings are mutually exclusive"};
  }
  if (mapping.contracts.contains("mixed") || mapping.contracts.contains("ordered")) {
    mixed_options content{};
    content.element_only = mapping.contracts.contains("ordered");
    const std::string prefix = content.element_only ? "ordered" : "mixed";
    content.field = mapping.contracts.at(prefix);
    if (mapping.contracts.contains(prefix + ".variant")) { content.variant_member = mapping.contracts.at(prefix + ".variant"); }
    if (mapping.contracts.contains("mixed.text")) {
      if (content.element_only) { throw std::invalid_argument{"XML ordered content cannot select a mixed text branch"}; }
      content.text_branch = mapping.contracts.at("mixed.text");
    }
    for (const auto& [key, value] : mapping.contracts) {
      if (key.starts_with(prefix + ".child.")) {
        content.children.emplace(key.substr(prefix.size() + std::string_view{".child."}.size()),
            contract_name(value, mapping.namespace_uri));
      } else if (key.starts_with(content.element_only ? "mixed." : "ordered.")) {
        throw std::invalid_argument{"XML content properties must match the selected mixed or ordered mapping"};
      }
    }
    result.mixed = bind_mixed_content(source, content, !mapping.text.empty(), mapping.namespace_uri);
  } else if (std::any_of(mapping.contracts.begin(), mapping.contracts.end(), [](const auto& item) {
        return item.first.starts_with("mixed.") || item.first.starts_with("ordered.");
      })) {
    if (std::any_of(mapping.contracts.begin(), mapping.contracts.end(), [](const auto& item) {
          return item.first.starts_with("mixed.");
        })) {
      throw std::invalid_argument{"XML mixed properties require mixed = source_member"};
    }
    throw std::invalid_argument{"XML ordered properties require ordered = source_member"};
  }
  for (const auto* options : authored) {
    if (options->unknown) { result.unknown = options->unknown; }
  }
  std::set<std::string> member_names{};
  for (const auto* source_field_pointer : model.flattened_fields) {
    const auto& source_field = *source_field_pointer;
    if (source_field.omits("xml")) { continue; }
    if (source_field.version) {
      if (source_field.xml || mapping.names.contains(source_field.name) ||
          mapping.wrappers.contains(source_field.name) || mapping.text == source_field.name ||
          std::any_of(mapping.contracts.begin(), mapping.contracts.end(), [&](const auto& item) {
            return item.first == "presence." + source_field.name ||
                item.first == "map." + source_field.name || item.first.starts_with("map." + source_field.name + ".") ||
                item.first == "variant." + source_field.name || item.first.starts_with("variant." + source_field.name + ".");
          }) ||
          std::find(mapping.attributes.begin(), mapping.attributes.end(), source_field.name) != mapping.attributes.end()) {
        throw std::invalid_argument{"XML revision metadata cannot have an ordinary field mapping"};
      }
      member_names.insert(source_field.name);
      continue;
    }
    if (result.mixed && result.mixed->source == &source_field) {
      if (source_field.xml || mapping.names.contains(source_field.name) ||
          mapping.wrappers.contains(source_field.name) ||
          std::any_of(mapping.contracts.begin(), mapping.contracts.end(), [&](const auto& item) {
            return item.first == "presence." + source_field.name ||
                item.first == "map." + source_field.name || item.first.starts_with("map." + source_field.name + ".") ||
                item.first == "variant." + source_field.name || item.first.starts_with("variant." + source_field.name + ".");
          }) ||
          std::find(mapping.attributes.begin(), mapping.attributes.end(), source_field.name) != mapping.attributes.end()) {
        throw std::invalid_argument{"Mixed XML storage cannot also have a regular field mapping"};
      }
      member_names.insert(source_field.name);
      continue;
    }
    if (std::ranges::any_of(source_field.type_name_list,
            [](const auto& type) { return type.contains_managed_reference(); }) ||
        (source_field.key_type && source_field.key_type->contains_managed_reference())) {
      throw std::invalid_argument{"Managed entity references have no XML binding: " + source_field.name};
    }
    const auto map_key = "map." + source_field.name;
    const auto variant_key = "variant." + source_field.name;
    const bool map_binding = mapping.contracts.contains(map_key);
    const bool variant_binding = mapping.contracts.contains(variant_key);
    const bool presence_binding = mapping.contracts.contains("presence." + source_field.name);
    if (source_field.wire_only ||
        (source_field.modifier == member::modifier_type::map && !map_binding) ||
        (source_field.modifier == member::modifier_type::variant && !variant_binding)) {
      throw std::invalid_argument{"Unsupported XML map, variant, managed or derived-size field: " + source_field.name};
    }
    const bool list = source_field.xml && source_field.xml->list;
    const auto& source_type = source_field.type_name_list.front();
    const auto* storage_type = &source_type;
    if (presence_binding) {
      while (storage_type->is_optional()) { storage_type = &storage_type->arguments.at(0U); }
    } else if (storage_type->is_optional()) { storage_type = &storage_type->arguments.at(0U); }
    const auto& list_container = *storage_type;
    if (map_binding || variant_binding || presence_binding) {
      // Their dedicated binders validate storage before any backend selects an emitter.
    } else if (list) {
      if (list_container.is_sequence()) { check_type(list_container.arguments.front(), true); }
      else { check_type(source_type, source_field.modifier == member::modifier_type::array); }
    } else { check_type(source_type, source_field.modifier == member::modifier_type::array); }
    const bool collection = source_field.modifier == member::modifier_type::array || list_container.is_sequence();
    const bool repeated = collection && !list;
    const auto renamed = mapping.names.find(source_field.name);
    auto local_name = renamed == mapping.names.end() ? source_field.name : renamed->second;
    bool is_attribute = std::find(mapping.attributes.begin(), mapping.attributes.end(), source_field.name) != mapping.attributes.end() ||
        (scalar_placement == xml_field_placement::attribute && !repeated && !map_binding && !variant_binding &&
         (presence_binding ? storage_type->type : leaf(source_field).type) != object_type::class_type);
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
    if (is_attribute && (repeated || (presence_binding ? storage_type->type : leaf(source_field).type) == object_type::class_type || text)) {
      throw std::invalid_argument{"XML attributes require scalar fields: " + source_field.name};
    }
    if (text && (repeated || (presence_binding ? storage_type->type : leaf(source_field).type) == object_type::class_type ||
        source_field.type_name_list.front().is_optional())) {
      throw std::invalid_argument{"XML text requires one nonoptional scalar field: " + source_field.name};
    }
    const auto wrapper = mapping.wrappers.find(source_field.name);
    const auto wrapper_name = authored_wrapper ? *authored_wrapper :
        wrapper != mapping.wrappers.end() ? wrapper->second : std::string{};
    if ((authored_wrapper || wrapper != mapping.wrappers.end()) &&
        ((!repeated && !map_binding && !variant_binding && !presence_binding) || is_attribute || text || wrapper_name.empty())) {
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
      result.fields.back().wrapper = contract_name(wrapper_name, mapping.namespace_uri);
    }
    auto& bound_field = result.fields.back();
    if (map_binding && variant_binding) {
      throw std::invalid_argument{"An XML member cannot be both map and variant"};
    }
    if (map_binding || variant_binding) {
      if (is_attribute || text || list || presence_binding || mapping.names.contains(source_field.name) ||
          (source_field.xml && (source_field.xml->name || source_field.xml->namespace_uri ||
              source_field.xml->qname || source_field.xml->prefix_list))) {
        throw std::invalid_argument{"XML container contracts require dedicated wrapper and payload names without scalar mappings"};
      }
      if (map_binding) {
        const auto selected = [&](std::string_view suffix, std::string_view fallback,
            std::string_view namespace_uri) {
          const auto found = mapping.contracts.find(map_key + std::string{suffix});
          return contract_name(found == mapping.contracts.end() ? fallback : found->second, namespace_uri);
        };
        const auto placement = mapping.contracts.find(map_key + ".key_placement");
        const bool key_attribute = placement != mapping.contracts.end() && placement->second == "attribute";
        bound_field.map_binding = bind_map_contract(source_field, bound_field.wrapper,
            contract_name(mapping.contracts.at(map_key), mapping.namespace_uri),
            selected(".key", "key", key_attribute ? std::string_view{} : mapping.namespace_uri),
            selected(".value", "value", mapping.namespace_uri),
            key_attribute ? map_key_placement::attribute : map_key_placement::element);
      } else {
        std::map<std::string, variant_tag_override> overrides{};
        for (const auto& [key, value] : mapping.contracts) {
          if (key.starts_with(variant_key + ".tag.")) {
            const auto alias = key.substr(variant_key.size() + std::string_view{".tag."}.size());
            overrides[alias].alternative = alias;
            overrides[alias].tag = contract_name(value, mapping.namespace_uri);
          } else if (key.starts_with(variant_key + ".type.")) {
            const auto alias = key.substr(variant_key.size() + std::string_view{".type."}.size());
            overrides[alias].alternative = alias;
            overrides[alias].type_identity = contract_name(value, mapping.namespace_uri);
          }
        }
        std::vector<variant_tag_override> tags{};
        for (auto& [alias, value] : overrides) { (void)alias; tags.push_back(std::move(value)); }
        bound_field.variant_binding = bind_variant_contract(source_field, bound_field.wrapper,
            mapping.contracts.at(variant_key) == "element" ? variant_selection::element :
                variant_selection::xsi_type, tags);
      }
      bound_field.min_occurs = bound_field.map_binding && bound_field.map_binding->optional_container ? 0U : 1U;
      bound_field.max_occurs = 1U;
      bound_field.repeated = false;
    } else {
      const auto presence = presence_binding ? std::optional{parse_presence_policy(
          mapping.contracts.at("presence." + source_field.name))} : std::nullopt;
      // An optimized collection may derive its wrapper below; publication requires that wrapper.
      bound_field.presence = bind_presence(source_field, presence, is_attribute, text,
          bound_field.wrapper.has_value() || (bundle_ && presence_binding && collection), list);
      if (presence_binding) {
        check_type(*bound_field.presence.value_type);
        bound_field.min_occurs = bound_field.presence.collection ? 0U :
            bound_field.presence.allows_absent ? 0U : 1U;
      }
    }
    if (result.mixed && !bound_field.attribute) {
      throw std::invalid_argument{"Ordered XML mixed content cannot coexist with separate element/text fields"};
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
  for (const auto& [key, value] : mapping.contracts) {
    (void)value;
    std::string_view tail{key};
    if (tail.starts_with("presence.")) { tail.remove_prefix(std::string_view{"presence."}.size()); }
    else if (tail.starts_with("map.")) { tail.remove_prefix(std::string_view{"map."}.size()); }
    else if (tail.starts_with("variant.")) { tail.remove_prefix(std::string_view{"variant."}.size()); }
    else { continue; }
    const auto selector = tail.substr(0U, tail.find('.'));
    if (!member_names.contains(std::string{selector})) {
      throw std::invalid_argument{"Unknown XML contract member: " + std::string{selector}};
    }
    if (key.starts_with("map.") && !mapping.contracts.contains("map." + std::string{selector})) {
      throw std::invalid_argument{"XML map properties require an entry mapping"};
    }
    if (key.starts_with("variant.") && !mapping.contracts.contains("variant." + std::string{selector})) {
      throw std::invalid_argument{"XML variant properties require a selection mapping"};
    }
  }
  const bool extensions_requested = std::any_of(mapping.contracts.begin(), mapping.contracts.end(), [](const auto& item) {
        return item.first.starts_with("extension.") || item.first == "extensions.required";
      });
  if (bundle_) {
    const auto [complex_document, complex] = record_type(*bundle_, result.root);
    if (true_attribute(*complex, "abstract")) {
      throw std::invalid_argument{"Abstract XSD complex types require dynamic type selection"};
    }
    const auto content = expand_complex_type(*bundle_, *complex_document, *complex);
    // Existing sequence/singleton-choice codecs retain their original compile path and
    // resource behavior. The complete particle tree supplies the newly supported shapes.
    const auto needs_particles = [&](auto&& self, const xml::element& node,
                                     const schema_bundle::document* owner,
                                     std::size_t depth) -> bool {
      if (depth > xml::limits{}.max_depth) { throw std::length_error{"XSD particle inspection depth exceeded"}; }
      const auto& kind = node.qualified_name.local;
      if (kind == "all") { return true; }
      if (kind == "group") {
        if (occurrence(attribute(node, "minOccurs"), 1U) != 1U ||
            attribute(node, "maxOccurs") == "unbounded" || occurrence(attribute(node, "maxOccurs"), 1U) != 1U) { return true; }
        const auto selected = resolve_model_group(*bundle_, *owner, node);
        return self(self, *selected.node, selected.owner, depth + 1U);
      }
      if (kind != "sequence" && kind != "choice") { return false; }
      const auto known = std::count_if(node.children.begin(), node.children.end(),
          [](const auto& child) { return child.qualified_name.local == "element"; });
      const bool repeated = attribute(node, "maxOccurs") == "unbounded" ||
          occurrence(attribute(node, "maxOccurs"), 1U) != 1U ||
          std::any_of(node.children.begin(), node.children.end(), [](const auto& child) {
            return attribute(child, "maxOccurs") == "unbounded" || occurrence(attribute(child, "maxOccurs"), 1U) != 1U;
          });
      if ((kind == "sequence" && (occurrence(attribute(node, "minOccurs"), 1U) != 1U ||
          attribute(node, "maxOccurs") == "unbounded" || occurrence(attribute(node, "maxOccurs"), 1U) != 1U)) ||
          (kind == "choice" && ((repeated && known > 1) ||
              std::any_of(node.children.begin(), node.children.end(), [](const auto& child) {
                return child.qualified_name.local != "annotation" && child.qualified_name.local != "element" &&
                    child.qualified_name.local != "any";
              })))) { return true; }
      return std::any_of(node.children.begin(), node.children.end(),
          [&](const auto& child) { return self(self, child, owner, depth + 1U); });
    };
    if (std::any_of(content.children.begin(), content.children.end(), [&](const auto& component) {
          return needs_particles(needs_particles, *component.node, component.owner, 0U);
        })) {
      result.particles = compile_particle_contract(*bundle_, content.children);
    }
    if (result.mixed) {
      if (!result.mixed->element_only || content.mixed) {
        throw std::invalid_argument{"Ordered XSD bindings require element-only content"};
      }
      result.particles = compile_particle_contract(*bundle_, content.children);
      std::vector<xml::name> symbols;
      for (const auto& child : result.mixed->children) { symbols.push_back(child.qualified_name); }
      result.mixed->grammar = compile_ordered_automaton(*result.particles, symbols);
    }
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
    std::set<const member*> bound_wrappers{};
    const auto bind_field = [&](const xml::element& occurrence_node, bool is_attribute,
                                bool in_choice = false,
                                const schema_bundle::document* defining_document = nullptr,
                                bool wrapper_item = false) {
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
      if (field.source->xml) {
        const auto& authored_field = *field.source->xml;
        if ((authored_field.placement && (*authored_field.placement == xml_field_placement::text ||
            (*authored_field.placement == xml_field_placement::attribute) != is_attribute)) ||
            (authored_field.namespace_uri && *authored_field.namespace_uri != uri)) {
          throw std::invalid_argument{"Schema-authored XML placement or namespace conflicts with XSD"};
        }
      }
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
        const bool native_enum = source_type.type == object_type::enum_type;
        if (native_enum && (field.list || !resolved.facets.list_item_kind.empty() ||
            !native_enum_string_kind(resolved.primitive.local, resolved.facets.whitespace))) {
          throw std::invalid_argument{"XSD native enum bindings require xs:string with preserve whitespace"};
        }
        if (primitive == primitive_types.end() ||
            (!native_enum && source_type.name != primitive->second &&
             !(source_type.name == "float32" && primitive->second == "float") &&
             !(source_type.name == "float64" && primitive->second == "double"))) {
          throw std::invalid_argument{"Optimized XSD field type is unsupported or differs from Serializer: " + field.source->name};
        }
        field.facets = resolved.facets;
        field.qname = field.qname || resolved.primitive.local == "QName";
      }
      if (has_attribute(node, "default")) { field.default_value = std::string{attribute(node, "default")}; }
      if (has_attribute(node, "fixed")) { field.fixed_value = std::string{attribute(node, "fixed")}; }
      if (!is_attribute) {
        // A wrapper owns the field's nil state; each repeated item remains a nonnil value.
        const bool nillable = true_attribute(node, "nillable");
        if (nillable != (!wrapper_item && field.presence.allows_nil)) {
          throw std::invalid_argument{"XSD nillability differs from XML presence policy: " + field.source->name};
        }
        if (nillable && field.fixed_value) {
          throw std::invalid_argument{"XSD fixed values with nil states require an explicit nil/fixed constraint: " + field.source->name};
        }
      }
      if (field.qname) {
        if (field.default_value) { field.qname_default = resolve(node, *field.default_value); }
        if (field.fixed_value) { field.qname_fixed = resolve(node, *field.fixed_value); }
      }
      if (field.facets.list_item_kind == "QName") {
        if (field.default_value) { field.qname_list_default = resolve_qname_list(node, *field.default_value); }
        if (field.fixed_value) { field.qname_list_fixed = resolve_qname_list(node, *field.fixed_value); }
      }
      if (!field.facets.union_members.empty() || !field.facets.list_item_facets.empty()) {
        scalar_facets selection{};
        selection.union_members = field.facets.union_members;
        selection.lexical_kind = field.facets.lexical_kind;
        selection.whitespace = field.facets.whitespace;
        selection.list_item_kind = field.facets.list_item_kind;
        selection.list_item_facets = field.facets.list_item_facets;
        const auto prepared = compile_scalar_contract(selection, false);
        if (field.default_value) {
          field.union_default = prepared.identify(*field.default_value, node);
          field.union_default_scope = union_literal_scope(*field.default_value, node);
        }
        if (field.fixed_value) {
          field.union_fixed = prepared.identify(*field.fixed_value, node);
          field.union_fixed_scope = union_literal_scope(*field.fixed_value, node);
        }
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
      // An optional XSD attribute may fill required native storage through its default/fixed value.
      const auto expected_minimum = field.presence.explicit_policy ?
          (field.presence.allows_absent ? 0U : 1U) :
          (optional || (is_attribute && (field.default_value || field.fixed_value) &&
              field.min_occurs == 0U) ? 0U : 1U);
      if (!field.repeated && (field.max_occurs != std::optional<std::size_t>{1U} ||
          field.min_occurs != expected_minimum)) {
        throw std::invalid_argument{"XSD presence/cardinality differs from Serializer member: " + field.source->name};
      }
      if (field.presence.explicit_policy && field.source->fixed_extent != 0U &&
          (field.min_occurs != field.source->fixed_extent ||
              field.max_occurs != std::optional<std::size_t>{static_cast<std::size_t>(field.source->fixed_extent)})) {
        throw std::invalid_argument{"XSD cardinality differs from fixed array extent: " + field.source->name};
      }
      ordered.push_back(std::move(field));
    };
    // Recognize only an exact singleton wrapper around one repeated element. Other
    // complex particles remain ordinary nested records rather than guessed wrappers.
    const auto bind_element = [&](const xml::element& node,
                                  const schema_bundle::document* owner) {
      const auto local = attribute(node, "name");
      const auto candidate = std::find_if(result.fields.begin(), result.fields.end(), [&](const auto& value) {
        return value.repeated && !value.attribute &&
            (value.source->name == local || (value.wrapper && value.wrapper->local == local));
      });
      if (candidate == result.fields.end() || !attribute(node, "ref").empty()) {
        bind_field(node, false, false, owner); return;
      }
      if (attribute(node, "maxOccurs") == "unbounded" ||
          occurrence(attribute(node, "maxOccurs"), 1U) != 1U) {
        bind_field(node, false, false, owner); return;
      }
      const auto inline_complex = std::find_if(node.children.begin(), node.children.end(), [](const auto& child) {
        return child.qualified_name.local == "complexType";
      });
      if (inline_complex == node.children.end() && attribute(node, "type").empty()) {
        bind_field(node, false, false, owner); return;
      }
      if (inline_complex == node.children.end() &&
          resolve(node, attribute(node, "type")).namespace_uri == xsd_namespace) {
        bind_field(node, false, false, owner); return;
      }
      const auto selected = complex_type(*bundle_, *owner, node);
      const auto& native_type = leaf(*candidate->source);
      if (native_type.type == object_type::class_type && native_type.resolved_node) {
        const auto nested = bind(static_cast<const class_node&>(*native_type.resolved_node));
        if (record_type(*bundle_, nested.root).second == selected.second) {
          // A bounded array of genuine records remains a repeated record field even
          // when that record happens to contain one repeated element of its own.
          bind_field(node, false, false, owner);
          return;
        }
      }
      const auto expanded = expand_complex_type(*bundle_, *selected.first, *selected.second);
      if (expanded.mixed || expanded.children.size() != 1U ||
          expanded.children.front().node->qualified_name.local != "sequence") {
        bind_field(node, false, false, owner); return;
      }
      const auto& sequence = *expanded.children.front().node;
      std::vector<const xml::element*> items;
      for (const auto& child : sequence.children) {
        if (child.qualified_name.local != "annotation") { items.push_back(&child); }
      }
      if (items.size() != 1U || items.front()->qualified_name.local != "element" ||
          occurrence(attribute(node, "minOccurs"), 1U) !=
              (candidate->presence.explicit_policy && candidate->presence.allows_absent ? 0U : 1U) ||
          occurrence(attribute(node, "maxOccurs"), 1U) != 1U ||
          occurrence(attribute(sequence, "minOccurs"), 1U) != 1U ||
          occurrence(attribute(sequence, "maxOccurs"), 1U) != 1U) {
        throw std::invalid_argument{"XSD collection wrapper requires one mandatory singleton sequence"};
      }
      if (attribute(*items.front(), "maxOccurs") != "unbounded" &&
          occurrence(attribute(*items.front(), "maxOccurs"), 1U) <= 1U) {
        bind_field(node, false, false, owner); return;
      }
      const auto inner_local = attribute(*items.front(), "name");
      if (inner_local.empty()) {
        throw std::invalid_argument{"XSD collection wrapper requires a named local item"};
      }
      auto* field = &*candidate;
      if (true_attribute(node, "nillable") != field->presence.allows_nil) {
        throw std::invalid_argument{"XSD wrapper nillability differs from XML presence policy: " + field->source->name};
      }
      if (has_attribute(node, "fixed") || has_attribute(node, "default")) {
        throw std::invalid_argument{"XSD collection wrapper value constraints require scalar content"};
      }
      const auto outer_form = attribute(node, "form");
      const auto outer_uri = outer_form == "qualified" || (outer_form.empty() &&
          attribute(owner->root, "elementFormDefault") == "qualified") ? owner->target : std::string{};
      const xml::name wrapper{std::string{local}, outer_uri};
      if (field->wrapper && *field->wrapper != wrapper) {
        throw std::invalid_argument{"Configured XML wrapper conflicts with XSD"};
      }
      const auto original_name = field->qualified_name;
      field->qualified_name.local = inner_local;
      field->wrapper = wrapper;
      try { bind_field(*items.front(), false, false, expanded.children.front().owner, true); }
      catch (...) { field->qualified_name = original_name; field->wrapper.reset(); throw; }
      bound_wrappers.insert(field->source);
    };
    // XSD all is a bounded unordered set; the native fields retain their normal storage.
    const auto bind_all = [&](const xml::element& node, const schema_bundle::document* owner) {
      if (occurrence(attribute(node, "maxOccurs"), 1U) != 1U ||
          occurrence(attribute(node, "minOccurs"), 1U) > 1U) {
        throw std::invalid_argument{"XSD all requires a singleton optional or required group"};
      }
      all_group group{};
      group.minimum = occurrence(attribute(node, "minOccurs"), 1U);
      for (const auto& child : node.children) {
        if (child.qualified_name.local == "annotation") { continue; }
        if (child.qualified_name.local != "element" ||
            occurrence(attribute(child, "maxOccurs"), 1U) != 1U) {
          throw std::invalid_argument{"XSD all permits only singleton element branches"};
        }
        bind_field(child, false, false, owner);
        group.alternatives.push_back(ordered.back().source);
      }
      result.all_groups.push_back(std::move(group));
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
    // Shared particle IR preserves group order even when native collections do not yet
    // provide an emitted ordered representation. Never send this flattened view to a codec.
    const auto bind_advanced = [&](auto&& self, const xml::element& node,
                                   const schema_bundle::document* owner,
                                   bool repeated_group, std::size_t depth) -> void {
      constexpr std::size_t maximum_particle_depth{64U};
      if (depth > maximum_particle_depth) { throw std::length_error{"XSD grouped binding depth exceeded"}; }
      const auto maximum = attribute(node, "maxOccurs");
      const bool repeated = repeated_group || maximum == "unbounded" || occurrence(maximum, 1U) > 1U;
      const auto& kind = node.qualified_name.local;
      if (kind == "element") {
        bind_field(node, false, true, owner);
        auto& value = ordered.back();
        if (repeated && !value.repeated) {
          throw std::invalid_argument{"Repeated grouped XSD particles require collection storage for each known member"};
        }
        value.min_occurs = 0U;
        if (value.repeated) { value.max_occurs.reset(); }
      } else if (kind == "group") {
        const auto selected = resolve_model_group(*bundle_, *owner, node);
        self(self, *selected.node, selected.owner, repeated, depth + 1U);
      } else if (kind == "any") { add_wildcard(node, false, owner); }
      else if (kind == "sequence" || kind == "choice" || kind == "all") {
        for (const auto& child : node.children) {
          if (child.qualified_name.local != "annotation") { self(self, child, owner, repeated, depth + 1U); }
        }
      } else { throw std::invalid_argument{"Unsupported grouped XSD particle"}; }
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
        if (child.qualified_name.local == "element") { bind_element(child, owner); }
        else if (child.qualified_name.local == "sequence") { self(self, child, owner); }
        else if (child.qualified_name.local == "choice") { bind_choice(child, owner); }
        else if (child.qualified_name.local == "group") {
          if (occurrence(attribute(child, "minOccurs"), 1U) != 1U ||
              occurrence(attribute(child, "maxOccurs"), 1U) != 1U) {
            throw std::invalid_argument{"Optional or repeated XSD model groups require grouped occurrence storage"};
          }
          const auto selected = resolve_model_group(*bundle_, *owner, child);
          if (selected.node->qualified_name.local == "choice") { bind_choice(*selected.node, selected.owner); }
          else if (selected.node->qualified_name.local == "all") { bind_all(*selected.node, selected.owner); }
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
      auto text = std::find_if(result.fields.begin(), result.fields.end(), [](const auto& value) { return value.text; });
      if (text == result.fields.end()) {
        const auto inferred = [&](const field& value) {
          return !value.attribute && !value.repeated && leaf(*value.source).name == "string" &&
              !value.source->type_name_list.front().is_optional() &&
              std::none_of(content.children.begin(), content.children.end(), [&](const auto& component) {
                return component.node->qualified_name.local == "attribute" &&
                    attribute(*component.node, "name") == value.qualified_name.local;
              });
        };
        if (std::count_if(result.fields.begin(), result.fields.end(), inferred) == 1) {
          text = std::find_if(result.fields.begin(), result.fields.end(), inferred);
        }
      }
      if (text == result.fields.end() || leaf(*text->source).name != "string" || !bound.insert(text->source).second) {
        throw std::invalid_argument{"Text-only mixed XSD content requires one unambiguous string text field"};
      }
      auto value = *text;
      if (value.source->xml && value.source->xml->placement &&
          *value.source->xml->placement != xml_field_placement::text) {
        throw std::invalid_argument{"Explicit XML placement conflicts with XSD text content"};
      }
      value.text = true;
      value.qualified_name = {};
      value.facets.lexical_kind = "string";
      ordered.push_back(std::move(value));
    }
    for (const auto& component : content.children) {
      const auto& node = *component.node;
      const auto* owner = component.owner;
      if (result.mixed && (node.qualified_name.local == "element" ||
          node.qualified_name.local == "sequence" || node.qualified_name.local == "choice" ||
          node.qualified_name.local == "group")) {
        // The array is the only source of order; each payload still binds its authoritative XSD type.
        const auto check_ordered = [&](auto&& self, const xml::element& particle,
                                       const schema_bundle::document* document,
                                       std::size_t depth) -> void {
          if (depth > xml::limits{}.max_depth) { throw std::length_error{"Ordered XSD binding depth exceeded"}; }
          if (particle.qualified_name.local == "group") {
            const auto selected = resolve_model_group(*bundle_, *document, particle);
            self(self, *selected.node, selected.owner, depth + 1U);
          } else if (particle.qualified_name.local == "element") {
            const auto* declaration_node = &particle;
            const auto referenced = attribute(particle, "ref");
            if (!referenced.empty()) {
              const auto selected = declaration(*bundle_, "element", resolve(particle, referenced));
              document = selected.first; declaration_node = selected.second;
            }
            const auto form = attribute(*declaration_node, "form");
            const bool qualified = !referenced.empty() || form == "qualified" ||
                (form.empty() && attribute(document->root, "elementFormDefault") == "qualified");
            const xml::name name{std::string{attribute(*declaration_node, "name")},
                qualified ? document->target : std::string{}};
            const auto branch = std::find_if(result.mixed->children.begin(), result.mixed->children.end(),
                [&](const auto& child) { return child.qualified_name == name; });
            if (branch == result.mixed->children.end() || true_attribute(*declaration_node, "nillable")) {
              throw std::invalid_argument{"Ordered XSD alternatives require mapped nonnil record children"};
            }
            const auto child = bind(static_cast<const class_node&>(*branch->value_type->resolved_node));
            if (record_type(*bundle_, child.root).second != complex_type(*bundle_, *document, *declaration_node).second) {
              throw std::invalid_argument{"Ordered XSD alternative differs from its declared complex type"};
            }
          } else {
            for (const auto& child : particle.children) {
              if (child.qualified_name.local != "annotation") { self(self, child, document, depth + 1U); }
            }
          }
        };
        check_ordered(check_ordered, node, owner, 0U);
        continue;
      }
      if (result.particles && result.particles->advanced && (node.qualified_name.local == "sequence" ||
          node.qualified_name.local == "choice" || node.qualified_name.local == "all" ||
          node.qualified_name.local == "group")) {
        bind_advanced(bind_advanced, node, owner, false, 0U);
        continue;
      }
      if (node.qualified_name.local == "sequence") { bind_sequence(bind_sequence, node, owner); }
      else if (node.qualified_name.local == "choice") { bind_choice(node, owner); }
      else if (node.qualified_name.local == "all") { bind_all(node, owner); }
      else if (node.qualified_name.local == "group") {
        if (occurrence(attribute(node, "minOccurs"), 1U) != 1U || occurrence(attribute(node, "maxOccurs"), 1U) != 1U) {
          throw std::invalid_argument{"Optional or repeated XSD model groups require grouped occurrence storage"};
        }
        const auto selected = resolve_model_group(*bundle_, *owner, node);
        if (selected.node->qualified_name.local == "choice") { bind_choice(*selected.node, selected.owner); }
        else if (selected.node->qualified_name.local == "all") { bind_all(*selected.node, selected.owner); }
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
          if (text == result.fields.end()) {
            const auto extension_attributes = expand_complex_type(*bundle_, *owner, extension);
            const auto inferred = [&](const field& value) {
              return !value.attribute && !value.repeated && !bound.contains(value.source) &&
                  leaf(*value.source).type != object_type::class_type &&
                  std::none_of(extension_attributes.children.begin(), extension_attributes.children.end(),
                      [&](const auto& item) {
                        return item.node->qualified_name.local == "attribute" &&
                            attribute(*item.node, "name") == value.qualified_name.local;
                      });
            };
            if (std::count_if(result.fields.begin(), result.fields.end(), inferred) == 1) {
              text = std::find_if(result.fields.begin(), result.fields.end(), inferred);
            }
          }
          if (text == result.fields.end() || !bound.insert(text->source).second) {
            throw std::invalid_argument{"XSD simpleContent text association is missing or ambiguous"};
          }
          auto scalar = extension;
          set_attribute(scalar, "type", std::string{attribute(extension, "base")});
          const auto resolved = resolve_scalar(*bundle_, scalar);
          auto value = *text;
          const std::map<std::string, std::string> text_types{{"string", "string"},
              {"normalizedString", "string"}, {"token", "string"}, {"decimal", "string"}, {"QName", "string"},
              {"integer", "string"}, {"nonNegativeInteger", "string"}, {"positiveInteger", "string"},
              {"nonPositiveInteger", "string"}, {"negativeInteger", "string"}, {"anyURI", "string"},
              {"language", "string"}, {"Name", "string"}, {"NCName", "string"}, {"NMTOKEN", "string"},
              {"boolean", "bool"}, {"byte", "int8"},
              {"unsignedByte", "uint8"}, {"short", "int16"}, {"unsignedShort", "uint16"},
              {"int", "int32"}, {"unsignedInt", "uint32"}, {"long", "int64"},
              {"unsignedLong", "uint64"}, {"float", "float"}, {"double", "double"}};
          const auto native = text_types.find(value.list && !resolved.facets.list_item_kind.empty() ?
              resolved.facets.list_item_kind : resolved.primitive.local);
          const auto& stored = leaf(*value.source).name;
          const bool native_enum = leaf(*value.source).type == object_type::enum_type;
          if (native_enum && (value.list || !resolved.facets.list_item_kind.empty() ||
              !native_enum_string_kind(resolved.primitive.local, resolved.facets.whitespace))) {
            throw std::invalid_argument{"XSD native enum bindings require xs:string with preserve whitespace"};
          }
          if (native == text_types.end() || (stored != native->second &&
              !native_enum &&
              !(stored == "float32" && native->second == "float") &&
              !(stored == "float64" && native->second == "double"))) {
            throw std::invalid_argument{"XSD simpleContent type differs from its native text member"};
          }
          if (value.source->type_name_list.front().is_optional() || value.repeated ||
              (value.source->xml && value.source->xml->placement &&
               *value.source->xml->placement != xml_field_placement::text)) {
            throw std::invalid_argument{"Explicit XML placement or optional storage conflicts with XSD simpleContent"};
          }
          value.text = true;
          value.qualified_name = {};
          value.facets = resolved.facets;
          value.qname = value.qname || resolved.primitive.local == "QName";
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
    for (const auto& document : bundle_->documents) {
      if (document.target != result.root.namespace_uri) { continue; }
      for (const auto& node : document.root.children) {
        if (node.qualified_name.local != "element" || attribute(node, "name") != result.root.local) { continue; }
        if (true_attribute(node, "abstract")) {
          throw std::invalid_argument{"Abstract XSD roots require dynamic declaration selection"};
        }
        if (true_attribute(node, "nillable")) {
          throw std::invalid_argument{"Nillable XSD roots require an optional document-root contract"};
        }
        // Global element defaults/fixed values belong to simpleContent text, not its complex type.
        const auto text = std::find_if(result.fields.begin(), result.fields.end(),
            [](const auto& field) { return field.text; });
        if (text != result.fields.end()) {
          if (has_attribute(node, "default")) { text->default_value = std::string{attribute(node, "default")}; }
          if (has_attribute(node, "fixed")) { text->fixed_value = std::string{attribute(node, "fixed")}; }
          if (text->qname) {
            if (text->default_value) { text->qname_default = resolve(node, *text->default_value); }
            if (text->fixed_value) { text->qname_fixed = resolve(node, *text->fixed_value); }
          }
          if (text->facets.list_item_kind == "QName") {
            if (text->default_value) { text->qname_list_default = resolve_qname_list(node, *text->default_value); }
            if (text->fixed_value) { text->qname_list_fixed = resolve_qname_list(node, *text->fixed_value); }
          }
          if (!text->facets.union_members.empty() || !text->facets.list_item_facets.empty()) {
            scalar_facets selection{};
            selection.union_members = text->facets.union_members;
            selection.lexical_kind = text->facets.lexical_kind;
            selection.whitespace = text->facets.whitespace;
            selection.list_item_kind = text->facets.list_item_kind;
            selection.list_item_facets = text->facets.list_item_facets;
            const auto prepared = compile_scalar_contract(selection, false);
            if (text->default_value) {
              text->union_default = prepared.identify(*text->default_value, node);
              text->union_default_scope = union_literal_scope(*text->default_value, node);
            }
            if (text->fixed_value) {
              text->union_fixed = prepared.identify(*text->fixed_value, node);
              text->union_fixed_scope = union_literal_scope(*text->fixed_value, node);
            }
          }
        }
        if (std::none_of(node.children.begin(), node.children.end(), [](const auto& child) {
              return child.qualified_name.local == "key" || child.qualified_name.local == "unique" ||
                  child.qualified_name.local == "keyref";
            })) { continue; }
        const auto identity = bind_identity_constraints(*bundle_, node, document.target);
        if (!identity.constraints.empty()) { result.identity = identity; }
      }
    }
    for (const auto& value : result.fields) {
      if (!mapping.text.empty() && value.source->name == mapping.text && !value.text) {
        throw std::invalid_argument{"Configured XML text conflicts with XSD"};
      }
      if ((value.wrapper && !bound_wrappers.contains(value.source)) ||
          (value.presence.explicit_policy && value.presence.collection && !value.wrapper)) {
        throw std::invalid_argument{"Configured XML wrapper conflicts with XSD"};
      }
    }
  }
  std::set<std::pair<bool, std::pair<std::string, std::string>>> names{};
  check_namespace(result.root.namespace_uri);
  for (const auto& value : result.fields) {
    check_namespace(value.qualified_name.namespace_uri);
    if (value.wrapper) { check_namespace(value.wrapper->namespace_uri); }
    if (value.text) { continue; }
    if (result.model.advanced && value.attribute && value.qualified_name.namespace_uri == model_metadata_namespace &&
        (value.qualified_name.local == "revision" || value.qualified_name.local == "magic")) {
      throw std::invalid_argument{"XML field collides with reserved model lifecycle metadata"};
    }
    const auto& name = value.wrapper ? *value.wrapper : value.qualified_name;
    if (!names.emplace(value.attribute, std::pair{name.namespace_uri, name.local}).second) {
      throw std::invalid_argument{"Ambiguous XML member mapping"};
    }
  }
  // Positions and known expanded names follow the final authoritative XSD order,
  // including inferred attributes/wrappers, rather than the authored approximation.
  if (extensions_requested) { result.extensions = bind_extension_slots(result, mapping.contracts); }
  bind_skip_wildcard_slots(result);
  for (auto& value : result.fields) {
    const auto& stored = leaf(*value.source);
    if (stored.type != object_type::class_type) {
      value.compiled_facets = compile_scalar_contract(value.facets, false);
    }
    if (stored.type == object_type::enum_type && !value.list) {
      // Validate the finite wire inventory before any backend can publish an unusable enum codec.
      // Schema defaults/fixed values must convert through the same native names as explicit XML.
      std::vector<std::string_view> permitted_names;
      for (const auto& name : static_cast<const enum_node&>(*stored.resolved_node).enum_name_list) {
        try {
          value.compiled_facets->validate(name);
          permitted_names.push_back(name);
        } catch (const std::invalid_argument&) {
          // Scalar restrictions may exclude some, but never every, native wire name.
        }
      }
      if (permitted_names.empty()) {
        throw std::invalid_argument{"XSD native enum binding has no permitted wire names"};
      }
      for (const auto* constrained : {&value.default_value, &value.fixed_value}) {
        if (*constrained && std::find(permitted_names.begin(), permitted_names.end(), **constrained) ==
            permitted_names.end()) {
          throw std::invalid_argument{"XSD native enum default/fixed value is not a permitted wire name"};
        }
      }
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
  if (include_declarations && bundle_ && std::any_of(result.wildcards.begin(), result.wildcards.end(),
      [](const auto& slot) { return slot.process_contents != "skip"; })) {
    const auto key = std::pair{bundle_.get(), &source};
    auto found = declaration_cache_->find(key);
    if (found == declaration_cache_->end()) {
      const auto registered = registered_inventories_.find(&source);
      auto inventory = registered == registered_inventories_.end()
          ? compile_declaration_inventory(bundle_, options_.xsd, source, result, options_)
          : registered->second;
      found = declaration_cache_->emplace(key, std::move(inventory)).first;
    }
    result.declarations = found->second;
  }
  return result;
}

// Own imported validator declarations and the immutable schema nodes used to derive them.
struct declaration_inventory_storage {
  std::shared_ptr<const schema_bundle> bundle{};
  std::vector<std::vector<std::unique_ptr<syntax_node>>> statements{};
  std::vector<std::shared_ptr<const declaration_inventory>> dependencies{};
};

// Keep imported model ownership in the inventory while allowing backend collectors to borrow declarations.
const std::vector<std::vector<std::unique_ptr<syntax_node>>>& declaration_inventory::statements() const {
  static const std::vector<std::vector<std::unique_ptr<syntax_node>>> empty;
  return storage ? storage->statements : empty;
}

// Compile exactly the global declarations reachable through this record's validating wildcard slots.
std::shared_ptr<const declaration_inventory> compile_declaration_inventory(
    std::shared_ptr<const schema_bundle> bundle, const std::filesystem::path& entry_path,
    const class_node& owner, const record& owner_contract, const xml_options& options, bool include_derived, std::optional<xml::name> specialized_root) {
  if (!bundle || bundle->documents.empty()) {
    throw std::invalid_argument{"Validating XML wildcards require a pinned fixed XSD bundle"};
  }
  auto result = std::make_shared<declaration_inventory>();
  result->owner = &owner;
  auto storage = std::make_shared<declaration_inventory_storage>();
  storage->bundle = bundle;
  result->storage = storage;
  std::vector<std::pair<wildcard, std::string>> reachable_slots;
  if (!specialized_root) {
    for (const auto& slot : owner_contract.wildcards) { reachable_slots.emplace_back(slot, owner_contract.root.namespace_uri); }
  }
  const auto matches = [&](const auto& selected, bool is_attribute, std::string_view uri) {
    const auto& [slot, root_namespace] = selected;
    if (slot.attribute != is_attribute || slot.process_contents == "skip") { return false; }
    const auto& target = slot.target_namespace ? *slot.target_namespace : root_namespace;
    for (const auto token : lexical_words(slot.namespace_rule)) {
      if (token == "##any" || (token == "##other" && !uri.empty() && uri != target) ||
          (token == "##targetNamespace" && uri == target) || (token == "##local" && uri.empty()) || token == uri) {
        return true;
      }
    }
    return false;
  };
  auto detached = options;
  detached.enabled = true;
  detached.xsd.clear();
  detached.bundle_sha256.clear();
  schema binder{detached};
  binder.options_ = options;
  binder.options_.enabled = true;
  binder.options_.xsd = entry_path;
  auto private_bundle = std::make_shared<schema_bundle>(*bundle);
  const auto permit_private_abstract_root = [&](auto&& self, xml::element& node) -> void {
    if (node.qualified_name.local == "complexType") {
      std::erase_if(node.attributes, [](const auto& value) {
        return value.qualified_name.namespace_uri.empty() && value.qualified_name.local == "abstract";
      });
    }
    for (auto& child : node.children) { self(self, child); }
  };
  for (auto& document : private_bundle->documents) { permit_private_abstract_root(permit_private_abstract_root, document.root); }
  binder.bundle_ = private_bundle;
  std::optional<xml::name> private_root;
  // Native owner records already have exact bound fields; register their reachable classes before recursion.
  const auto register_native = [&](auto&& self, const class_node& source, const record* bound = nullptr) -> std::size_t {
    if (const auto found = result->validator_indices.find(&source); found != result->validator_indices.end()) {
      return found->second;
    }
    const auto index = result->validators.size();
    result->validator_indices.emplace(&source, index);
    auto contract = bound ? *bound : binder.bind(source, false);
    contract.declarations.reset();
    if (private_root && contract.root != *private_root) {
      const auto [document, complex] = record_type(*bundle, contract.root);
      (void)document;
      if (true_attribute(*complex, "abstract")) {
        throw std::invalid_argument{"Nested abstract XSD fields require an explicit dynamic variant mapping"};
      }
    }
    std::vector<const class_node*> children;
    const auto collect_type = [&](auto&& visit, const type_name& stored) -> void {
      if (stored.type == object_type::class_type) { children.push_back(static_cast<const class_node*>(stored.resolved_node)); }
      for (const auto& argument : stored.arguments) { visit(visit, argument); }
    };
    for (const auto& value : contract.fields) {
      for (const auto& stored : value.source->type_name_list) { collect_type(collect_type, stored); }
    }
    result->validators.push_back(std::move(contract));
    for (const auto* child : children) { self(self, *child); }
    return index;
  };
  std::optional<std::size_t> owner_index;
  std::set<std::tuple<bool, std::string, std::string>> names;
  std::size_t imported_count{};
  bool expanded;
  std::size_t expanded_validators{};
  do {
    expanded = false;
  for (const auto& document : bundle->documents) {
    for (const auto& node : document.root.children) {
      const bool is_attribute = node.qualified_name.local == "attribute";
      if (!is_attribute && node.qualified_name.local != "element") { continue; }
      const xml::name name{std::string{attribute(node, "name")}, document.target};
      const bool selected_root = specialized_root && expanded_validators == 0U && !is_attribute && name == *specialized_root;
      if (name.local.empty() || (!selected_root && std::none_of(reachable_slots.begin(), reachable_slots.end(),
          [&](const auto& slot) { return matches(slot, is_attribute, name.namespace_uri); }))) { continue; }
      if (!names.emplace(is_attribute, name.namespace_uri, name.local).second) { continue; }
      expanded = true;
      global_declaration global{};
      global.qualified_name = name;
      global.attribute = is_attribute;
      global.nillable = !is_attribute && true_attribute(node, "nillable");
      global.abstract_element = !is_attribute && true_attribute(node, "abstract");
      if (has_attribute(node, "fixed")) { global.fixed_value = attribute(node, "fixed"); }
      if (has_attribute(node, "type")) { global.declared_type = resolve(node, attribute(node, "type")); }
      if (!is_attribute && name == owner_contract.root) {
        if (!owner_index) { owner_index = register_native(register_native, owner, &owner_contract); }
        global.validator_index = *owner_index;
        result->globals.push_back(std::move(global));
        continue;
      }
      bool complex = std::any_of(node.children.begin(), node.children.end(), [](const auto& child) {
        return child.qualified_name.local == "complexType";
      });
      if (!complex && !attribute(node, "type").empty()) {
        const auto type = resolve(node, attribute(node, "type"));
        complex = std::any_of(bundle->documents.begin(), bundle->documents.end(), [&](const auto& candidate) {
          return candidate.target == type.namespace_uri && std::any_of(candidate.root.children.begin(), candidate.root.children.end(),
              [&](const auto& child) { return child.qualified_name.local == "complexType" && attribute(child, "name") == type.local; });
        });
      }
      const auto private_identity = owner.get_full_name() + ':' + bundle->identity + ':' +
          name.namespace_uri + ':' + name.local + ':' + std::to_string(imported_count++);
      const auto private_namespace = "xml_decl_validator_" +
          rohit::digest_to_hex(rohit::make_digest<rohit::digest_algorithm::sha256>(private_identity));
      if (complex) {
        if (is_attribute) { throw std::invalid_argument{"A declared wildcard attribute requires a scalar type"}; }
        import_options import_settings{};
        import_settings.namespace_name = private_namespace;
        import_settings.root_name = '{' + name.namespace_uri + '}' + name.local;
        import_settings.schema_location = std::filesystem::absolute(entry_path).generic_string();
        auto selected_bundle = std::make_shared<schema_bundle>(*private_bundle);
        for (auto& candidate : selected_bundle->documents) {
          if (candidate.target != name.namespace_uri) { continue; }
          for (auto& declaration : candidate.root.children) {
            if (declaration.qualified_name.local == "element" && attribute(declaration, "name") == name.local) {
              std::erase_if(declaration.attributes, [](const auto& value) {
                return value.qualified_name.namespace_uri.empty() &&
                    (value.qualified_name.local == "abstract" || value.qualified_name.local == "nillable");
              });
            }
          }
        }
        binder.bundle_ = selected_bundle;
        const auto imported = import_xsd(*selected_bundle, import_settings);
        auto parsed = parser::parse(rohit::make_constant_stream(imported.schema_text.data(), imported.schema_text.size()));
        std::vector<const class_node*> classes;
        const auto collect = [&](auto&& self, const auto& statements) -> void {
          for (const auto& statement : statements) {
            if (statement->type == object_type::namespace_type) {
              self(self, static_cast<const namespace_node&>(*statement).statements);
            } else if (statement->type == object_type::class_type) {
              classes.push_back(static_cast<const class_node*>(statement.get()));
            }
          }
        };
        const auto set_paths = [&](auto&& self, auto& statements) -> void {
          for (auto& statement : statements) {
            statement->source_path = owner.source_path;
            statement->output_source_path = owner.output_source_path;
            if (statement->type == object_type::namespace_type) {
              self(self, static_cast<namespace_node&>(*statement).statements);
            }
          }
        };
        set_paths(set_paths, parsed);
        collect(collect, parsed);
        storage->statements.push_back(std::move(parsed));
        private_root = name;
        for (const auto* source : classes) {
          const auto index = register_native(register_native, *source);
          if (result->validators[index].root == name) {
            if (global.validator_index && *global.validator_index != index) {
              throw std::invalid_argument{"Ambiguous validating wildcard root: " + name.local};
            }
            global.validator_index = index;
          }
        }
        private_root.reset();
        binder.bundle_ = private_bundle;
        if (!global.validator_index) { throw std::invalid_argument{"Missing validating wildcard root: " + name.local}; }
      } else {
        const auto resolved = resolve_scalar_type(*bundle, node);
        field value{};
        value.qualified_name = name;
        value.attribute = is_attribute;
        value.text = !is_attribute;
        value.facets = resolved.facets;
        value.qname = resolved.primitive.local == "QName";
        if (has_attribute(node, "default")) { value.default_value = attribute(node, "default"); }
        if (has_attribute(node, "fixed")) { value.fixed_value = attribute(node, "fixed"); }
        value.compiled_facets = compile_scalar_contract(value.facets, false);
        const bool specialized_literal = specialized_root && name == *specialized_root;
        if (!specialized_literal && value.default_value) { value.compiled_facets->validate(*value.default_value, node); }
        if (!specialized_literal && value.fixed_value) { value.compiled_facets->validate(*value.fixed_value, node); }
        if (value.qname) {
          if (value.default_value) { value.qname_default = resolve(node, *value.default_value); }
          if (value.fixed_value) { value.qname_fixed = resolve(node, *value.fixed_value); }
        }
        if (value.facets.list_item_kind == "QName") {
          if (value.default_value) { value.qname_list_default = resolve_qname_list(node, *value.default_value); }
          if (value.fixed_value) { value.qname_list_fixed = resolve_qname_list(node, *value.fixed_value); }
        }
        if (!value.facets.union_members.empty() || !value.facets.list_item_facets.empty()) {
          auto selection = value.facets;
          selection.enumeration.clear(); selection.union_enumeration.clear();
          selection.patterns.clear(); selection.pattern_groups.clear();
          const auto prepared = compile_scalar_contract(selection, false);
          if (value.default_value) {
            try { value.union_default = prepared.identify(*value.default_value, node); }
            catch (const std::invalid_argument&) { if (!specialized_literal) { throw; } }
            value.union_default_scope = union_literal_scope(*value.default_value, node);
          }
          if (value.fixed_value) {
            try { value.union_fixed = prepared.identify(*value.fixed_value, node); }
            catch (const std::invalid_argument&) { if (!specialized_literal) { throw; } }
            value.union_fixed_scope = union_literal_scope(*value.fixed_value, node);
          }
        }
        global.scalar = value;
        const auto wrapper_text = "serializer version 1; namespace " + private_namespace +
            " { class scalar { public string value; } }";
        auto parsed = parser::parse(rohit::make_constant_stream(wrapper_text.data(), wrapper_text.size()));
        auto& space = static_cast<namespace_node&>(*parsed.front());
        auto& source = static_cast<class_node&>(*space.statements.front());
        space.source_path = source.source_path = owner.source_path;
        space.output_source_path = source.output_source_path = owner.output_source_path;
        value.source = &source.member_list.front();
        value.attribute = is_attribute;
        value.text = !is_attribute;
        value.qualified_name = name;
        record contract{};
        contract.root = is_attribute ? xml::name{"attribute", "http://www.w3.org/XML/1998/namespace"} : name;
        contract.model = bind_model_contract(source);
        contract.fields.push_back(std::move(value));
        const auto index = result->validators.size();
        result->validator_indices.emplace(&source, index);
        result->validators.push_back(std::move(contract));
        global.validator_index = index;
        storage->statements.push_back(std::move(parsed));
      }
      result->globals.push_back(std::move(global));
    }
  }
    while (expanded_validators < result->validators.size()) {
      const auto& contract = result->validators[expanded_validators++];
      for (const auto& slot : contract.wildcards) {
        reachable_slots.emplace_back(slot, contract.root.namespace_uri);
      }
    }
  } while (expanded);
  for (auto& global : result->globals) {
    if (!global.nillable || !global.validator_index) { continue; }
    const auto original = result->validators.at(*global.validator_index);
    std::vector<field> attributes;
    for (const auto& value : original.fields) { if (value.attribute) { attributes.push_back(value); } }
    const auto identity = owner.get_full_name() + ':' + bundle->identity + ':' +
        global.qualified_name.namespace_uri + ':' + global.qualified_name.local + ":nil";
    const auto private_name = "xml_decl_validator_" +
        rohit::digest_to_hex(rohit::make_digest<rohit::digest_algorithm::sha256>(identity));
    auto text = "serializer version 1; namespace " + private_name + " { class nil_value { ";
    for (std::size_t index = 0; index < attributes.size(); ++index) {
      text += "public string attribute_" + std::to_string(index) + "; ";
    }
    text += "} }";
    auto parsed = parser::parse(rohit::make_constant_stream(text.data(), text.size()));
    auto& space = static_cast<namespace_node&>(*parsed.front());
    auto& source = static_cast<class_node&>(*space.statements.front());
    space.source_path = source.source_path = owner.source_path;
    space.output_source_path = source.output_source_path = owner.output_source_path;
    auto contract = original;
    contract.fields = std::move(attributes);
    contract.model = bind_model_contract(source);
    for (std::size_t index = 0; index < contract.fields.size(); ++index) {
      contract.fields[index].source = &source.member_list[index];
    }
    contract.choices.clear(); contract.all_groups.clear(); contract.particles.reset(); contract.mixed.reset();
    contract.identity.reset(); contract.declarations.reset();
    std::erase_if(contract.wildcards, [](const auto& slot) { return !slot.attribute; });
    if (contract.extensions) {
      contract.extensions->known_names.clear(); contract.extensions->known_positions.clear();
      std::erase_if(contract.extensions->slots, [](const auto& slot) { return !slot.attribute; });
      for (auto& slot : contract.extensions->slots) { slot.before_field = 0; }
    }
    global.nil_validator_index = result->validators.size();
    result->validator_indices.emplace(&source, result->validators.size());
    result->validators.push_back(std::move(contract));
    storage->statements.push_back(std::move(parsed));
  }
  if (include_derived) {
    struct named_type {
      xml::name name{};
      const schema_bundle::document* document{};
      const xml::element* node{};
    };
    std::vector<named_type> types;
    for (const auto& document : bundle->documents) {
      for (const auto& node : document.root.children) {
        if ((node.qualified_name.local == "complexType" || node.qualified_name.local == "simpleType") &&
            !attribute(node, "name").empty()) {
          types.push_back({{std::string{attribute(node, "name")}, document.target}, &document, &node});
        }
      }
    }
    const std::map<std::string_view, std::string_view> builtin_bases{
        {"string", "anySimpleType"}, {"boolean", "anySimpleType"}, {"decimal", "anySimpleType"},
        {"float", "anySimpleType"}, {"double", "anySimpleType"}, {"duration", "anySimpleType"},
        {"dateTime", "anySimpleType"}, {"time", "anySimpleType"}, {"date", "anySimpleType"},
        {"gYearMonth", "anySimpleType"}, {"gYear", "anySimpleType"}, {"gMonthDay", "anySimpleType"},
        {"gDay", "anySimpleType"}, {"gMonth", "anySimpleType"}, {"hexBinary", "anySimpleType"},
        {"base64Binary", "anySimpleType"}, {"anyURI", "anySimpleType"}, {"QName", "anySimpleType"},
        {"NOTATION", "anySimpleType"}, {"normalizedString", "string"}, {"token", "normalizedString"},
        {"language", "token"}, {"Name", "token"}, {"NCName", "Name"}, {"NMTOKEN", "token"},
        {"ID", "NCName"}, {"IDREF", "NCName"}, {"ENTITY", "NCName"}, {"integer", "decimal"},
        {"nonPositiveInteger", "integer"}, {"negativeInteger", "nonPositiveInteger"},
        {"long", "integer"}, {"int", "long"}, {"short", "int"}, {"byte", "short"},
        {"nonNegativeInteger", "integer"}, {"unsignedLong", "nonNegativeInteger"},
        {"unsignedInt", "unsignedLong"}, {"unsignedShort", "unsignedInt"}, {"unsignedByte", "unsignedShort"},
        {"positiveInteger", "nonNegativeInteger"}, {"NMTOKENS", "anySimpleType"},
        {"IDREFS", "anySimpleType"}, {"ENTITIES", "anySimpleType"}, {"anySimpleType", "anyType"}};
    const auto find_type = [&](const xml::name& name) -> const named_type* {
      const auto found = std::find_if(types.begin(), types.end(), [&](const auto& type) { return type.name == name; });
      return found == types.end() ? nullptr : &*found;
    };
    const auto forbids = [](std::string_view rule, std::string_view method) {
      const auto words = lexical_words(rule);
      return std::find(words.begin(), words.end(), "#all") != words.end() ||
          std::find(words.begin(), words.end(), method) != words.end();
    };
    for (std::size_t global_index = 0; global_index < result->globals.size(); ++global_index) {
      auto& global = result->globals[global_index];
      std::vector<global_declaration> discovered;
      if (global.attribute || !global.declared_type) { continue; }
      const xml::element* declaration{};
      const schema_bundle::document* declaration_document{};
      for (const auto& document : bundle->documents) {
        if (document.target != global.qualified_name.namespace_uri) { continue; }
        for (const auto& node : document.root.children) {
          if (node.qualified_name.local == "element" && attribute(node, "name") == global.qualified_name.local) {
            declaration = &node; declaration_document = &document;
          }
        }
      }
      if (!declaration) { throw std::invalid_argument{"Missing pinned wildcard declaration"}; }
      if (const auto* declared = find_type(*global.declared_type)) {
        global.abstract_type = true_attribute(*declared->node, "abstract");
      }
      const auto block = has_attribute(*declaration, "block") ? attribute(*declaration, "block") :
          attribute(declaration_document->root, "blockDefault");
      std::vector<xml::name> candidates;
      for (const auto& type : types) { candidates.push_back(type.name); }
      if (global.declared_type->namespace_uri == xsd_namespace) {
        for (const auto& [name, base] : builtin_bases) { (void)base; candidates.push_back({std::string{name}, std::string{xsd_namespace}}); }
      }
      for (const auto& candidate : candidates) {
        if (candidate == *global.declared_type) { continue; }
        auto current = candidate;
        bool admitted{}, rejected{};
        std::set<std::pair<std::string, std::string>> visited;
        while (visited.emplace(current.namespace_uri, current.local).second) {
          if (current == *global.declared_type) { admitted = true; break; }
          const auto* type = find_type(current);
          std::optional<xml::name> base;
          std::string method{"restriction"};
          if (type) {
            const auto derivation = [&](auto&& self, const xml::element& node) -> void {
              for (const auto& child : node.children) {
                const auto& kind = child.qualified_name.local;
                if ((kind == "extension" || kind == "restriction") && has_attribute(child, "base")) {
                  if (base) { throw std::invalid_argument{"Ambiguous XML type derivation"}; }
                  base = resolve(child, attribute(child, "base")); method = kind;
                } else if (kind == "complexContent" || kind == "simpleContent") { self(self, child); }
                else if (kind == "list" || kind == "union") {
                  base = xml::name{"anySimpleType", std::string{xsd_namespace}}; method = kind;
                }
              }
            };
            derivation(derivation, *type->node);
            if (!base && type->node->qualified_name.local == "complexType") {
              base = xml::name{"anyType", std::string{xsd_namespace}};
            }
          } else if (current.namespace_uri == xsd_namespace) {
            const auto found = builtin_bases.find(current.local);
            if (found != builtin_bases.end()) { base = xml::name{std::string{found->second}, std::string{xsd_namespace}}; }
          }
          if (!base || forbids(block, method)) { rejected = true; break; }
          if (const auto* parent = find_type(*base)) {
            const auto final_rule = has_attribute(*parent->node, "final") ? attribute(*parent->node, "final") :
                attribute(parent->document->root, "finalDefault");
            if (forbids(final_rule, method)) { rejected = true; break; }
          }
          current = *base;
        }
        if (!admitted || rejected) { continue; }
        constexpr std::size_t max_type_validators = 1024U;
        if (global.type_validators.size() >= max_type_validators) {
          throw std::invalid_argument{"XML wildcard type-validator limit"};
        }
        auto specialized = std::make_shared<schema_bundle>(*bundle);
        const auto type_identity = candidate.namespace_uri + ':' + candidate.local;
        specialized->identity = rohit::digest_to_hex(rohit::make_digest<rohit::digest_algorithm::sha256>(
            bundle->identity + ':' + type_identity));
        for (auto& document : specialized->documents) {
          if (document.target != global.qualified_name.namespace_uri) { continue; }
          for (auto& node : document.root.children) {
            if (node.qualified_name.local != "element" || attribute(node, "name") != global.qualified_name.local) { continue; }
            std::string prefix{"decl_type"};
            while (node.namespace_bindings.contains(prefix) ||
                attribute(node, "default").find(prefix + ':') != std::string_view::npos ||
                attribute(node, "fixed").find(prefix + ':') != std::string_view::npos) { prefix += '_'; }
            node.namespace_bindings.emplace(prefix, candidate.namespace_uri);
            std::erase_if(node.attributes, [](const auto& value) { return value.qualified_name.namespace_uri.empty() && value.qualified_name.local == "type"; });
            node.attributes.push_back({{"type", ""}, prefix + ':' + candidate.local});
          }
        }
        auto selected_owner = owner_contract;
        selected_owner.root.local += "_private_type_inventory";
        const auto compiled = compile_declaration_inventory(specialized, entry_path, owner, selected_owner, options, false, global.qualified_name);
        const auto selected = std::find_if(compiled->globals.begin(), compiled->globals.end(), [&](const auto& value) {
          return !value.attribute && value.qualified_name == global.qualified_name;
        });
        if (selected == compiled->globals.end() || !selected->validator_index) {
          throw std::invalid_argument{"Missing derived XML wildcard validator"};
        }
        std::map<std::size_t, std::size_t> remap;
        for (const auto& [source, old_index] : compiled->validator_indices) {
          const auto found = result->validator_indices.find(source);
          if (found != result->validator_indices.end()) { remap.emplace(old_index, found->second); continue; }
          const auto index = result->validators.size();
          remap.emplace(old_index, index); result->validator_indices.emplace(source, index);
          result->validators.push_back(compiled->validators.at(old_index));
        }
        for (const auto& discovered_global : compiled->globals) {
          if (!names.emplace(discovered_global.attribute, discovered_global.qualified_name.namespace_uri, discovered_global.qualified_name.local).second) { continue; }
          auto added = discovered_global;
          if (added.validator_index) { added.validator_index = remap.at(*added.validator_index); }
          if (added.nil_validator_index) { added.nil_validator_index = remap.at(*added.nil_validator_index); }
          discovered.push_back(std::move(added));
        }
        const auto* selected_type = find_type(candidate);
        global.type_validators.push_back({candidate, remap.at(*selected->validator_index),
            selected->nil_validator_index ? std::optional{remap.at(*selected->nil_validator_index)} : std::nullopt,
            selected_type && true_attribute(*selected_type->node, "abstract")});
        auto imported_storage = std::const_pointer_cast<declaration_inventory_storage>(compiled->storage);
        for (auto& group : imported_storage->statements) { storage->statements.push_back(std::move(group)); }
        imported_storage->statements.clear();
        storage->dependencies.push_back(compiled);
      }
      for (auto& discovered_global : discovered) { result->globals.push_back(std::move(discovered_global)); }
    }
  }
  for (const auto& group : storage->statements) {
    const auto prepare = [&](auto&& self, const auto& statements) -> void {
      for (const auto& statement : statements) {
        if (statement->type == object_type::namespace_type) {
          self(self, static_cast<const namespace_node&>(*statement).statements);
        } else if (statement->type == object_type::class_type) {
          auto& source = static_cast<class_node&>(*statement);
          source.prepared_xml_validation = std::make_shared<const record>(
              result->validators.at(result->validator_indices.at(&source)));
        }
      }
    };
    prepare(prepare, group);
  }
  return result;
}

// Enumerate the exact pinned local inputs used by fixed and inline schema bindings.
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
  // Export owns its separate representability checks and publishes a verified bundle.
  if (language == "xsd") { return; }
  if (contract.particles && contract.particles->advanced &&
      !(contract.mixed && contract.mixed->grammar)) {
    throw std::invalid_argument{"Grouped or repeated heterogeneous XSD particles are compiled; ordered storage generation is not implemented for " +
                                std::string{language}};
  }
  if (contract.identity && language != "python" && language != "cpp" && language != "js" && language != "typescript" &&
      language != "java" && language != "csharp" && language != "go" && language != "kotlin" && language != "swift" && language != "c" && language != "rust") {
    throw std::invalid_argument{"XSD identity constraints are compiled; generated tuple validation is not implemented for " +
                                std::string{language}};
  }
  const bool concrete_owning_instance = contract.model.concrete_generic &&
      !contract.model.inherited && !contract.model.digest && !contract.model.revision &&
      !contract.model.magic && contract.model.projection == projection_kind::owning;
  const bool flattened_owning_model = (language == "go" || language == "python" || language == "cpp" ||
      language == "js" || language == "typescript" || language == "java" || language == "csharp" || language == "rust" ||
      language == "c" || language == "kotlin" || language == "swift") && !contract.model.digest &&
      !contract.model.revision && !contract.model.magic &&
      contract.model.projection == projection_kind::owning;
  const bool python_owning_model = (language == "python" || language == "cpp" || language == "js" || language == "typescript") &&
      contract.model.projection == projection_kind::owning;
  const bool native_owning_model = (language == "c" || language == "rust" || language == "kotlin" || language == "swift" ||
      language == "java" || language == "csharp" || language == "go") &&
      contract.model.projection == projection_kind::owning;
  if (native_owning_model && contract.model.revision &&
      (!contract.all_groups.empty() || !contract.choices.empty()) &&
      std::any_of(contract.model.flattened_fields.begin(), contract.model.flattened_fields.end(),
          [](const auto* value) { return !value->created_version.empty() || !value->obsolete_version.empty(); })) {
    throw std::invalid_argument{"XML lifecycle fields in all/choice groups require active particle layouts for " + std::string{language}};
  }
  if ((language == "kotlin" || language == "swift" || language == "java" || language == "csharp" || language == "go") && contract.model.revision &&
      (contract.mixed || std::any_of(contract.fields.begin(), contract.fields.end(),
          [](const auto& field) { return field.text && (!field.source->created_version.empty() || !field.source->obsolete_version.empty()); }))) {
    throw std::invalid_argument{"XML lifecycle text/mixed fields require active text layouts for " + std::string{language}};
  }
  if (contract.model.advanced && !concrete_owning_instance && !flattened_owning_model && !python_owning_model && !native_owning_model) {
    throw std::invalid_argument{"XML native model contract is compiled; lifecycle/projection generation is not implemented for " +
                                std::string{language}};
  }
  const bool portable_all = (language == "java" || language == "csharp" || language == "go" || language == "kotlin" || language == "swift" || language == "rust" || language == "c") &&
      xml_portable::supports_all(contract);
  const bool javascript_backend = language == "js" || language == "typescript";
  const bool mobile_managed_extensions = language == "kotlin" || language == "swift" ||
      language == "java" || language == "csharp" || language == "go";
  if (contract.extensions && mobile_managed_extensions) {
    // Reject unrepresentable metadata before publishing target source with overflowing integer literals.
    // Swift uses 64-bit Int; the other portable counters retain their 32-bit contract across hosts.
    const auto counter_maximum = language == "swift" ?
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) :
        static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    for (const auto& slot : contract.extensions->slots) {
      if ((slot.before_field != std::numeric_limits<std::size_t>::max() && slot.before_field > counter_maximum) || slot.minimum > counter_maximum ||
          (slot.maximum && *slot.maximum > counter_maximum)) {
        throw std::invalid_argument{"XML extension occurrence/position exceeds native counter range for " + std::string{language}};
      }
    }
  }
  if (!contract.all_groups.empty() && language != "python" && language != "cpp" && !javascript_backend && !portable_all) {
    throw std::invalid_argument{"XSD all is compiled; generated unordered-child decoding is not implemented for " +
                                std::string{language}};
  }
  if (language == "cpp" && contract.extensions && contract.mixed &&
      std::any_of(contract.extensions->slots.begin(), contract.extensions->slots.end(),
          [&](const auto& slot) { return !slot.attribute && slot.before_field !=
              contract.fields.size() + contract.mixed->children.size(); })) {
    throw std::invalid_argument{"Positioned XML child slots with mixed/all storage require additional ordered positions for cpp"};
  }
  if (contract.extensions && ((language == "cpp" && !contract.all_groups.empty() &&
      std::any_of(contract.extensions->slots.begin(), contract.extensions->slots.end(),
          [&](const auto& slot) { return !slot.attribute && slot.before_field != contract.fields.size(); })) ||
      ((language == "python" || javascript_backend || language == "kotlin" || language == "swift" ||
          language == "java" || language == "csharp" || language == "go") && contract.mixed &&
          std::any_of(contract.extensions->slots.begin(), contract.extensions->slots.end(),
              [&](const auto& slot) { return !slot.attribute && slot.before_field !=
                  contract.fields.size() + contract.mixed->children.size(); })) ||
      ((language == "c" || language == "rust") && (contract.mixed || !contract.all_groups.empty()) &&
          std::any_of(contract.extensions->slots.begin(), contract.extensions->slots.end(),
              [&](const auto& slot) { return !slot.attribute && slot.before_field !=
                  contract.fields.size() + (contract.mixed ? contract.mixed->children.size() : 0U); })))) {
    throw std::invalid_argument{"Positioned XML extension slots with mixed/all storage are not implemented for " + std::string{language}};
  }
  if (contract.extensions && (language == "kotlin" || language == "swift" || language == "java" || language == "csharp" || language == "go" || language == "c" || language == "rust") &&
      std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; }) &&
      std::any_of(contract.extensions->slots.begin(), contract.extensions->slots.end(), [&](const auto& slot) { return !slot.attribute && slot.before_field != contract.fields.size(); })) {
    throw std::invalid_argument{"Positioned XML child slots with scalar text require ordered mixed storage for " + std::string{language}};
  }
  const bool portable_mixed = language == "java" || language == "csharp" || language == "go" || language == "kotlin" || language == "swift" ||
      language == "c" || language == "rust";
  if ((contract.mixed && language != "python" && language != "cpp" && !javascript_backend && !portable_mixed) ||
      (contract.extensions && language != "python" && language != "cpp" && language != "kotlin" && language != "swift" &&
          language != "java" && language != "csharp" && language != "go" && language != "c" && language != "rust" && !javascript_backend) ||
      std::any_of(contract.fields.begin(), contract.fields.end(), [&](const auto& field) {
        const bool native_presence = (language == "swift" || language == "rust" || language == "c") &&
            (!field.presence.collection || field.wrapper);
        const bool portable_presence = (language == "go" || language == "java" || language == "csharp") &&
            xml_portable::supports_presence(field);
        return (field.presence.explicit_policy && language != "cpp" && language != "python" && language != "kotlin" &&
                !javascript_backend && !native_presence && !portable_presence) ||
            (field.map_binding && language != "cpp" && language != "python" && !javascript_backend &&
                ((language != "go" && language != "java" && language != "csharp" && language != "kotlin" && language != "swift" &&
                    language != "rust" && language != "c") ||
                    !xml_portable::supports_map(field))) ||
            (field.variant_binding && language != "python" && language != "cpp" && !javascript_backend &&
                ((language != "go" && language != "java" && language != "csharp" && language != "kotlin" && language != "swift" &&
                    language != "c" && language != "rust") ||
                    !xml_portable::supports_variant(field)));
      })) {
    throw std::invalid_argument{"Shared XML presence/mixed/map/variant/extension contracts are bound; "
        "generated token codec support is not implemented for " + std::string{language}};
  }
  const bool expanded_namespace_backend = language == "go" || language == "java" || language == "csharp";
  const bool native_namespace_backend = language == "c" || language == "rust";
  const bool namespace_default_backend = expanded_namespace_backend || language == "kotlin" || language == "swift" ||
      language == "python" || language == "cpp" || javascript_backend || native_namespace_backend;
  const bool namespace_union_backend = expanded_namespace_backend || language == "kotlin" || language == "swift" ||
      language == "c" || language == "rust";
  const bool namespace_literal_backend = namespace_union_backend || language == "cpp" || language == "python" || javascript_backend;
  const bool primitive_union_backend = expanded_namespace_backend || language == "kotlin" || language == "swift" ||
      language == "rust" || language == "c" || language == "cpp" || javascript_backend;
  const bool expanded_enumeration_backend = expanded_namespace_backend || language == "kotlin" || language == "swift" ||
      language == "c" || language == "rust";
  const auto namespace_value = [&](auto&& self, const scalar_facets& facets) -> bool {
    return facets.lexical_kind == "QName" || facets.list_item_kind == "QName" ||
        !facets.qname_enumeration.empty() ||
        std::any_of(facets.union_members.begin(), facets.union_members.end(),
            [&](const auto& member) { return self(self, member); }) ||
        std::any_of(facets.list_item_facets.begin(), facets.list_item_facets.end(),
            [&](const auto& item) { return self(self, item); });
  };
  const auto check_unions = [&](auto&& self, const scalar_facets& facets) -> void {
    if (language != "python" && language != "cpp" && !expanded_enumeration_backend && !javascript_backend &&
        facets.list_item_kind == "QName" && !facets.enumeration.empty()) {
      throw std::invalid_argument{"XSD QName list enumeration requires expanded item identities"};
    }
    if (!facets.union_members.empty() &&
        (!facets.enumeration.empty() || !facets.qname_enumeration.empty()) &&
        ((!primitive_union_backend && language != "python") ||
            (namespace_value(namespace_value, facets) && !namespace_literal_backend))) {
      throw std::invalid_argument{"XSD union enumeration requires selected-member value identities"};
    }
    for (const auto& alternative : facets.union_members) { self(self, alternative); }
    for (const auto& item : facets.list_item_facets) { self(self, item); }
  };
  for (const auto& field : contract.fields) {
    if (language != "python" && language != "cpp" && !namespace_union_backend && !javascript_backend && field.facets.list_item_kind == "QName" && field.default_value &&
        !lexical_words(*field.default_value).empty()) {
      throw std::invalid_argument{"XSD QName list defaults require expanded schema-scope item identities"};
    }
    check_unions(check_unions, field.facets);
    if (!field.facets.union_members.empty() && field.fixed_value &&
        ((!primitive_union_backend && language != "python") ||
            (namespace_value(namespace_value, field.facets) && !namespace_default_backend))) {
      throw std::invalid_argument{"XSD union fixed values require selected-member value identities"};
    }
    if (!field.facets.union_members.empty() && field.default_value &&
        namespace_value(namespace_value, field.facets) && !namespace_default_backend) {
      throw std::invalid_argument{"XSD QName union defaults require expanded schema-scope identities"};
    }
  }
  if (language == "cpp") {
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
      for (const auto& item : facets.list_item_facets) { self(self, item); }
    };
    for (const auto& value : contract.fields) {
      check(check, value.facets);
    }
    return;
  }
  const bool portable_choices = (language == "kotlin" || language == "swift" || language == "rust" || language == "c" ||
      language == "java" || language == "csharp" || language == "go") &&
      xml_portable::supports_choices(contract);
  const bool unsupported_choices = std::any_of(contract.choices.begin(), contract.choices.end(),
      [&](const auto& choice) { return (!javascript_backend && !portable_choices) || choice.repeated; });
  const bool declaration_dispatch = javascript_backend || language == "swift" || language == "kotlin" ||
      language == "csharp" || language == "go" || language == "java" || language == "c" || language == "rust";
  if (unsupported_choices || std::any_of(contract.wildcards.begin(), contract.wildcards.end(),
      [&](const auto& wildcard) { return wildcard.process_contents != "skip" && !declaration_dispatch; }) ||
      (contract.unknown && *contract.unknown != xml_unknown_policy::reject && !contract.extensions)) {
    throw std::invalid_argument{"XML choices, wildcards and extension policies are not implemented for " +
                                std::string{language}};
  }
  const std::set<std::string> primitives{"", "string", "boolean", "byte", "unsignedByte", "short",
      "unsignedShort", "int", "unsignedInt", "long", "unsignedLong", "float", "double"};
  const bool managed_scalar = language == "go" || language == "java" || language == "csharp" || language == "kotlin" || language == "swift" || language == "rust" || language == "c";
  const std::set<std::string> managed_kinds{"", "string", "boolean", "byte", "unsignedByte", "short",
      "unsignedShort", "int", "unsignedInt", "long", "unsignedLong", "float", "double",
      "normalizedString", "token", "anyURI", "decimal", "integer", "positiveInteger", "negativeInteger",
      "nonPositiveInteger", "nonNegativeInteger", "language", "Name", "NCName", "NMTOKEN", "ID", "IDREF", "ENTITY"};
  const bool namespace_backend = language == "go" || language == "c" || language == "rust" ||
      language == "kotlin" || language == "swift" || language == "java" || language == "csharp";
  const auto managed_facets = [&](auto&& self, const scalar_facets& facets) -> bool {
    const auto supported_kind = [&](const std::string& kind) {
      return managed_kinds.contains(kind) || (namespace_backend && kind == "QName");
    };
    return supported_kind(facets.lexical_kind) && supported_kind(facets.list_item_kind) &&
        (expanded_enumeration_backend || facets.qname_enumeration.empty()) &&
        std::all_of(facets.list_item_facets.begin(), facets.list_item_facets.end(),
            [&](const auto& item) { return self(self, item); }) && std::all_of(facets.union_members.begin(), facets.union_members.end(),
            [&](const auto& branch) { return self(self, branch); });
  };
  for (const auto& value : contract.fields) {
    const auto& facets = value.facets;
    const bool expanded_enumeration = expanded_enumeration_backend &&
        (!facets.qname_enumeration.empty() || !facets.qname_list_enumeration.empty() ||
            !facets.union_enumeration.empty() || value.prefix_list);
    if (namespace_backend && (value.qname || value.prefix_list || namespace_value(namespace_value, facets)) &&
        ((!namespace_union_backend && !facets.union_members.empty()) ||
            ((!namespace_default_backend || value.prefix_list) && (value.default_value || value.fixed_value)) ||
            (!expanded_enumeration && (!facets.enumeration.empty() || !facets.qname_enumeration.empty())))) {
      throw std::invalid_argument{std::string{language} +
          " XML QName unions/defaults/fixed/enumeration require expanded value identities: " + value.source->name};
    }
    const bool portable_scalar_facets = managed_scalar || javascript_backend;
    if (portable_scalar_facets && value.compiled_facets &&
        !value.compiled_facets->patterns_portable()) {
      throw std::invalid_argument{std::string{language} + " XML patterns require the approved portable XSD regex subset: " +
                                  value.source->name};
    }
    if (javascript_backend) {
      const std::set<std::string> kinds{"", "string", "normalizedString", "token", "anyURI", "boolean",
          "byte", "unsignedByte", "short", "unsignedShort", "int", "unsignedInt", "long", "unsignedLong",
          "integer", "positiveInteger", "negativeInteger", "nonPositiveInteger", "nonNegativeInteger",
          "float", "double", "decimal", "QName", "language", "Name", "NCName", "NMTOKEN", "ID", "IDREF", "ENTITY"};
      // Recursive lexical adapters preserve native arrays and per-field QName scopes on the direct token path.
      const auto supported = [&](auto&& self, const scalar_facets& selected) -> bool {
        return kinds.contains(selected.lexical_kind) && kinds.contains(selected.list_item_kind) &&
            std::all_of(selected.list_item_facets.begin(), selected.list_item_facets.end(),
                [&](const auto& item) { return self(self, item); }) &&
            std::all_of(selected.union_members.begin(), selected.union_members.end(),
                [&](const auto& alternative) { return self(self, alternative); });
      };
      if (!supported(supported, facets)) {
        throw std::invalid_argument{"XML scalar adapter is not implemented for " + std::string{language} + ": " + value.source->name};
      }
      continue;
    }
    const bool whitespace_changes_value = !facets.whitespace.empty() &&
        (facets.lexical_kind == "string" || facets.lexical_kind.empty());
    const bool javascript_lexical = javascript_backend &&
        (facets.lexical_kind == "normalizedString" || facets.lexical_kind == "token" || facets.lexical_kind == "anyURI" ||
         facets.lexical_kind == "decimal" || facets.lexical_kind == "integer" || facets.lexical_kind == "positiveInteger" ||
         facets.lexical_kind == "negativeInteger" || facets.lexical_kind == "nonPositiveInteger" || facets.lexical_kind == "nonNegativeInteger");
    if ((!managed_scalar && value.list) || ((value.qname || value.prefix_list) && !namespace_backend) ||
        (value.value_projection && language != "c" && language != "rust" && language != "kotlin" && language != "swift" &&
            language != "java" && language != "csharp" && language != "go") ||
        (!javascript_backend && !managed_scalar && (value.default_value || value.fixed_value)) ||
        (javascript_backend && !facets.union_members.empty()) ||
        (!portable_scalar_facets && (facets.min_inclusive || facets.max_inclusive || facets.min_exclusive || facets.max_exclusive ||
        facets.length || facets.min_length || facets.max_length || !facets.enumeration.empty() || whitespace_changes_value)) ||
        (!portable_scalar_facets && (!facets.patterns.empty() || !facets.pattern_groups.empty())) ||
        (!managed_scalar && !facets.list_item_kind.empty()) ||
        (managed_scalar && !managed_facets(managed_facets, facets)) ||
        (!managed_scalar && !primitives.contains(facets.lexical_kind) && !javascript_lexical)) {
      throw std::invalid_argument{"XML scalar restrictions or lexical adapters are not implemented for " +
                                  std::string{language} + ": " + value.source->name};
    }
  }
}

// Validate bounded known-child content without building an additional XML representation.
void validate_content_model(const record& contract, const xml::element& input) {
  if (input.qualified_name != contract.root) {
    throw std::invalid_argument{"XML content root differs from its compiled model"};
  }
  if (contract.mixed && contract.mixed->element_only) {
    const auto items = observe_mixed_content(*contract.mixed, input);
    if (contract.mixed->source->fixed_extent != 0U &&
        items.size() != contract.mixed->source->fixed_extent) {
      throw std::invalid_argument{"Ordered XML fixed storage cardinality mismatch"};
    }
    if (!contract.mixed->grammar) { return; }
  }
  if (contract.particles && (contract.particles->advanced ||
      (contract.mixed && contract.mixed->grammar))) {
    std::vector<xml::name> trace;
    trace.reserve(input.children.size());
    for (const auto& child : input.children) { trace.push_back(child.qualified_name); }
    validate_particle_trace(*contract.particles, trace);
    return;
  }
  if (!contract.wildcards.empty()) {
    throw std::invalid_argument{"Shared content-model validation requires explicit wildcard dispatch"};
  }
  std::vector<std::size_t> counts(contract.fields.size());
  std::vector<bool> wrappers_seen(contract.fields.size());
  std::vector<std::size_t> ranks(contract.fields.size());
  for (std::size_t index{}; index < ranks.size(); ++index) { ranks[index] = index; }
  const auto index_of = [&](const member* source) {
    const auto found = std::find_if(contract.fields.begin(), contract.fields.end(),
        [&](const auto& value) { return value.source == source; });
    if (found == contract.fields.end()) { throw std::invalid_argument{"Dangling XML content-model field"}; }
    return static_cast<std::size_t>(found - contract.fields.begin());
  };
  const auto merge_ranks = [&](const auto& alternatives) {
    std::size_t first = ranks.size();
    for (const auto* source : alternatives) { first = std::min(first, index_of(source)); }
    for (const auto* source : alternatives) { ranks[index_of(source)] = first; }
  };
  for (const auto& group : contract.all_groups) { merge_ranks(group.alternatives); }
  for (const auto& group : contract.choices) { merge_ranks(group.alternatives); }
  std::optional<std::size_t> last;
  for (const auto& child : input.children) {
    const auto found = std::find_if(contract.fields.begin(), contract.fields.end(), [&](const auto& value) {
      return !value.attribute && !value.text &&
          (value.wrapper ? *value.wrapper : value.qualified_name) == child.qualified_name;
    });
    if (found == contract.fields.end()) { throw std::invalid_argument{"Unknown XML child in compiled content model"}; }
    const auto index = static_cast<std::size_t>(found - contract.fields.begin());
    if (last && ranks[index] < *last) { throw std::invalid_argument{"XML child sequence order differs from its model"}; }
    last = ranks[index];
    if (found->wrapper) {
      if (wrappers_seen[index] || !child.attributes.empty() ||
          child.text.find_first_not_of(" \t\r\n") != std::string::npos) {
        throw std::invalid_argument{"Invalid or duplicate XML collection wrapper"};
      }
      for (const auto& item : child.children) {
        if (item.qualified_name != found->qualified_name) {
          throw std::invalid_argument{"Unknown XML collection wrapper item"};
        }
      }
      wrappers_seen[index] = true;
      counts[index] = child.children.size();
    } else { ++counts[index]; }
    if (found->max_occurs && counts[index] > *found->max_occurs) {
      throw std::invalid_argument{"XML field exceeds its compiled occurrence maximum"};
    }
  }
  std::set<const member*> grouped;
  for (const auto& group : contract.all_groups) {
    const bool present = std::any_of(group.alternatives.begin(), group.alternatives.end(),
        [&](const auto* source) { return counts[index_of(source)] != 0U; });
    for (const auto* source : group.alternatives) {
      grouped.insert(source);
      const auto index = index_of(source);
      if ((present || group.minimum != 0U) && counts[index] < contract.fields[index].min_occurs) {
        throw std::invalid_argument{"XML all group is missing a required child"};
      }
    }
  }
  for (const auto& group : contract.choices) {
    std::size_t selected{};
    std::size_t repetitions{};
    for (const auto& branch : group.branches) {
      if (!branch.source) { throw std::invalid_argument{"Shared choice validation requires typed branches"}; }
      grouped.insert(branch.source);
      const auto count = counts[index_of(branch.source)];
      if (count == 0U) { continue; }
      ++selected;
      if (!group.repeated && (count < branch.minimum || (branch.maximum && count > *branch.maximum))) {
        throw std::invalid_argument{"XML choice branch occurrence differs from its model"};
      }
      if (group.repeated && (branch.minimum != 1U || branch.maximum != std::optional<std::size_t>{1U})) {
        throw std::invalid_argument{"Shared repeated choice validation requires singleton branches"};
      }
      repetitions += group.repeated ? count : 1U;
    }
    const bool nullable = std::any_of(group.branches.begin(), group.branches.end(),
        [](const auto& branch) { return branch.minimum == 0U; });
    if ((!group.repeated && selected > 1U) || (repetitions < group.minimum && !nullable) ||
        (group.max_repetitions && repetitions > *group.max_repetitions)) {
      throw std::invalid_argument{"XML choice selection or occurrence differs from its model"};
    }
  }
  for (std::size_t index{}; index < contract.fields.size(); ++index) {
    const auto& value = contract.fields[index];
    if (!value.attribute && !value.text && !grouped.contains(value.source) &&
        (counts[index] < value.min_occurs || (value.wrapper && !wrappers_seen[index]))) {
      throw std::invalid_argument{"XML content is missing a required child"};
    }
  }
}

std::vector<compatibility_diagnostic> compare_contracts(const record& previous, const record& current) {
  const auto shared_content = [](const record& value) {
    return (value.mixed && !value.mixed->element_only) || value.extensions ||
        std::any_of(value.fields.begin(), value.fields.end(), [](const auto& field) {
          return field.presence.explicit_policy || field.map_binding || field.variant_binding;
        });
  };
  if (shared_content(previous) || shared_content(current)) {
    throw std::invalid_argument{"XML shared-content compatibility comparison is not implemented"};
  }
  std::vector<compatibility_diagnostic> result{};
  if (previous.mixed || current.mixed) {
    if (!previous.mixed || !current.mixed) {
      result.push_back({{}, "Ordered element storage was added or removed"});
    } else {
      // Reader-direction compatibility is language inclusion, independent of native branch numbering.
      const auto automaton = [](const mixed_contract& mixed) {
        if (mixed.grammar) { return *mixed.grammar; }
        particle_contract grammar;
        grammar.root.kind = particle_kind::choice;
        grammar.root.minimum = mixed.source->fixed_extent;
        grammar.root.maximum = mixed.source->fixed_extent ? std::optional{mixed.source->fixed_extent} : std::nullopt;
        std::vector<xml::name> symbols;
        for (const auto& child : mixed.children) {
          content_particle value;
          value.kind = particle_kind::element;
          value.name = child.qualified_name;
          grammar.root.children.push_back(std::move(value));
          symbols.push_back(child.qualified_name);
        }
        return compile_ordered_automaton(grammar, symbols);
      };
      const auto before = automaton(*previous.mixed), after = automaton(*current.mixed);
      std::vector<std::pair<std::size_t, std::size_t>> pending{{1U, 1U}};
      std::set<std::pair<std::size_t, std::size_t>> visited{{1U, 1U}};
      std::size_t remaining = particle_limits{}.max_work_units;
      for (std::size_t cursor{}; cursor < pending.size(); ++cursor) {
        const auto [old_state, new_state] = pending[cursor];
        if (before.accepting[old_state] && !after.accepting[new_state]) {
          result.push_back({{}, "Ordered XML reader rejects a previously accepted child sequence"});
          break;
        }
        for (std::size_t symbol{}; symbol < previous.mixed->children.size(); ++symbol) {
          if (remaining == 0U) { throw std::length_error{"Ordered XML compatibility work limit exceeded"}; }
          --remaining;
          const auto old_target = before.transitions[old_state][symbol];
          if (old_target == 0U) { continue; }
          const auto& branch = previous.mixed->children[symbol];
          const auto found = std::find_if(current.mixed->children.begin(), current.mixed->children.end(),
              [&](const auto& child) { return child.qualified_name == branch.qualified_name; });
          const auto new_target = found == current.mixed->children.end() ? 0U :
              after.transitions[new_state][static_cast<std::size_t>(found - current.mixed->children.begin())];
          const auto pair = std::pair{old_target, new_target};
          if (visited.insert(pair).second) {
            if (visited.size() > particle_limits{}.max_states) {
              throw std::length_error{"Ordered XML compatibility state limit exceeded"};
            }
            pending.push_back(pair);
          }
        }
      }
      if (current.mixed->source->fixed_extent != 0U &&
          previous.mixed->source->fixed_extent != current.mixed->source->fixed_extent) {
        result.push_back({{}, "Ordered XML fixed storage cardinality narrowed"});
      }
    }
  }
  if (previous.root != current.root) { result.push_back({{}, "Root expanded name changed"}); }
  if (previous.identity != current.identity) {
    result.push_back({{}, "Identity selector, tuple type or key-reference contract changed"});
  }
  if (!(previous.mixed && current.mixed) && ((previous.particles && previous.particles->advanced) ||
       (current.particles && current.particles->advanced)) && previous.particles != current.particles) {
    result.push_back({{}, "Grouped or repeated heterogeneous particle contract changed"});
  }
  const auto revision_contract = [](const model_contract& model) {
    return model.revision ? std::tuple{model.revision->name, model.revision->type_name_list.front().name,
        model.revision->default_value, model.revision->compatibility_version,
        model.revision->resolved_compatibility_version} :
        std::tuple{std::string{}, std::string{}, std::string{}, std::string{}, std::string{}};
  };
  if (previous.model.concrete_identity != current.model.concrete_identity ||
      previous.model.projection != current.model.projection || previous.model.magic != current.model.magic ||
      revision_contract(previous.model) != revision_contract(current.model)) {
    result.push_back({{}, "Concrete model identity, projection, revision or immutable magic changed"});
  }
  std::map<std::string, const field*> previous_fields{};
  const auto normalized_facets = [](const field& value) {
    auto result = value.facets;
    const auto& stored = leaf(*value.source);
    if (stored.type == object_type::enum_type && !value.list &&
        (result.lexical_kind.empty() || native_enum_string_kind(result.lexical_kind, result.whitespace))) {
      // Implicit native enum conversion and an explicit exported string enumeration accept the same finite names.
      // Keep patterns, lengths and default/fixed metadata distinct so narrower bound contracts still diagnose.
      result.enumeration.clear();
      for (const auto& name : static_cast<const enum_node&>(*stored.resolved_node).enum_name_list) {
        try {
          if (value.compiled_facets) { value.compiled_facets->validate(name); }
          result.enumeration.push_back(name);
        } catch (const std::invalid_argument&) {
          // The complete scalar contract excludes this otherwise declared native wire name.
        }
      }
      std::sort(result.enumeration.begin(), result.enumeration.end());
      result.lexical_kind.clear();
      if (result.whitespace == "preserve") { result.whitespace.clear(); }
      return result;
    }
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
    if (found == previous_fields.end()) {
      if (value.min_occurs != 0U && !value.default_value && !value.fixed_value) {
        result.push_back({value.source->name, "Current reader requires an added field"});
      }
      continue;
    }
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
    if (old.source->created_version != value.source->created_version ||
        old.source->obsolete_version != value.source->obsolete_version) {
      result.push_back({value.source->name, "Field creation or retirement revision changed"});
    }
    if (normalized_facets(old) != normalized_facets(value) || old.fixed_value != value.fixed_value ||
        old.default_value != value.default_value || old.qname_default != value.qname_default ||
        old.qname_fixed != value.qname_fixed || old.qname_list_default != value.qname_list_default ||
        old.qname_list_fixed != value.qname_list_fixed) {
      result.push_back({value.source->name, "Scalar restrictions or default/fixed semantics changed"});
    }
    previous_fields.erase(found);
  }
  for (const auto& [key, value] : previous_fields) {
    (void)value;
    // Unknown policies require a caller whitelist and never forgive unknown core names.
    // A static contract comparison therefore cannot prove retired-name acceptance.
    result.push_back({key, "Current contract removes a field"});
  }
  // Optional additions do not disturb the order of fields that existed in both contracts.
  std::erase_if(current_elements, [&](const auto& name) {
    return std::find(previous_elements.begin(), previous_elements.end(), name) == previous_elements.end();
  });
  std::erase_if(previous_elements, [&](const auto& name) {
    return std::find(current_elements.begin(), current_elements.end(), name) == current_elements.end();
  });
  const auto all_names = [](const record& value) {
    std::vector<std::pair<std::size_t, std::set<std::string>>> groups;
    for (const auto& group : value.all_groups) {
      std::set<std::string> names;
      for (const auto* field : group.alternatives) { names.insert(field->name); }
      groups.emplace_back(group.minimum, std::move(names));
    }
    return groups;
  };
  const bool sequence_widened_to_all = previous.all_groups.empty() && current.all_groups.size() == 1U &&
      current.choices.empty() && current.all_groups.front().alternatives.size() == current_elements.size();
  if (all_names(previous) != all_names(current) && !sequence_widened_to_all) {
    result.push_back({{}, "Unordered all-group membership or occurrence changed"});
  // An all reader accepts every permitted sequence order; its declaration order is immaterial.
  } else if (!sequence_widened_to_all && previous.all_groups.empty() &&
             previous_elements != current_elements) {
    result.push_back({{}, "Element sequence order changed"});
  }
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
  auto bundle = export_xsd_bundle(sources);
  if (bundle.documents.size() != 1U) {
    throw std::invalid_argument{"XSD export requires export_xsd_bundle for multiple namespaces"};
  }
  const xml::prepared_schema verified{bundle.documents.front().bytes};
  return bundle.documents.front().bytes;
}

// Build every document in memory first; callers publish only a complete pinned bundle.
schema_bundle schema::export_xsd_bundle(const std::vector<const class_node*>& sources) const {
  if (sources.empty()) { throw std::invalid_argument{"XSD export requires at least one record"}; }
  std::set<const class_node*> fragments;
  for (const auto* source : sources) {
    if (!source) { throw std::invalid_argument{"XSD export received an empty record"}; }
    const auto configured = options_.classes.find(source->get_full_name());
    if (configured != options_.classes.end() && configured->second.contracts.contains("ordered")) {
      const auto bound = bind(*source);
      if (bound.mixed && !options_.classes.contains(bound.mixed->fragment->get_full_name())) {
        fragments.insert(bound.mixed->fragment);
      }
    }
  }
  std::map<const syntax_node*, record> records;
  std::map<xml::name, const class_node*, expanded_name_less> roots;
  for (const auto* source : sources) {
    if (!source) { throw std::invalid_argument{"XSD export received an empty record"}; }
    if (fragments.contains(source)) { continue; }
    auto bound = bind(*source);
    if (bound.model.advanced || (bound.mixed && !bound.mixed->element_only) || bound.extensions || std::any_of(bound.fields.begin(), bound.fields.end(),
        [](const auto& value) {
          return value.presence.explicit_policy || value.map_binding || value.variant_binding;
        })) {
      throw std::invalid_argument{"XSD export of shared XML content contracts is not implemented"};
    }
    if ((bound.particles && bound.particles->advanced && !(bound.mixed && bound.mixed->grammar)) ||
        !bound.choices.empty() || !bound.wildcards.empty()) {
      throw std::invalid_argument{"XSD choice/wildcard export requires ordered content-model storage"};
    }
    if (!roots.emplace(bound.root, source).second || !records.emplace(source, std::move(bound)).second) {
      throw std::invalid_argument{"XSD export has duplicate roots or records"};
    }
  }
  const auto node = [](std::string_view kind) {
    xml::element value{};
    value.qualified_name = {std::string{kind}, std::string{xsd_namespace}};
    return value;
  };
  std::map<std::string, xml::element> documents;
  for (const auto& [source, bound] : records) {
    (void)source;
    documents.try_emplace(bound.root.namespace_uri, node("schema"));
    if (bound.mixed) {
      for (const auto& child : bound.mixed->children) {
        documents.try_emplace(child.qualified_name.namespace_uri, node("schema"));
      }
    }
    for (const auto& value : bound.fields) {
      if (!value.text && !value.qualified_name.namespace_uri.empty()) {
        documents.try_emplace(value.qualified_name.namespace_uri, node("schema"));
      }
      if (value.wrapper && !value.wrapper->namespace_uri.empty()) {
        documents.try_emplace(value.wrapper->namespace_uri, node("schema"));
      }
    }
  }
  // Generated component aliases must never bind a prefix that a source union literal left unbound.
  std::set<std::string> literal_prefixes;
  const auto reserve_literal = [&](std::string_view text) {
    for (const auto token : lexical_words(text)) {
      const auto colon = token.find(':');
      if (colon != std::string_view::npos) { literal_prefixes.emplace(token.substr(0U, colon)); }
    }
  };
  const auto reserve_facets = [&](auto&& self, const scalar_facets& facets) -> void {
    for (const auto& text : facets.enumeration) { reserve_literal(text); }
    for (const auto& member : facets.union_members) { self(self, member); }
    for (const auto& item : facets.list_item_facets) { self(self, item); }
  };
  for (const auto& [source, bound] : records) {
    (void)source;
    for (const auto& value : bound.fields) {
      reserve_facets(reserve_facets, value.facets);
      if (value.default_value) { reserve_literal(*value.default_value); }
      if (value.fixed_value) { reserve_literal(*value.fixed_value); }
    }
  }
  std::string schema_prefix{"xs"};
  for (std::size_t index{}; literal_prefixes.contains(schema_prefix); ++index) { schema_prefix = "schema" + std::to_string(index); }
  std::map<std::string, std::string> prefixes, filenames;
  for (auto& [uri, document] : documents) {
    const auto ordinal = std::to_string(prefixes.size());
    std::string prefix = "n" + ordinal;
    while (literal_prefixes.contains(prefix) || prefix == schema_prefix) { prefix += '_'; }
    prefixes.emplace(uri, std::move(prefix));
    filenames.emplace(uri, "namespace_" + ordinal + ".xsd");
    if (!uri.empty()) { set_attribute(document, "targetNamespace", uri); }
  }
  for (auto& [uri, document] : documents) {
    document.namespace_bindings[schema_prefix] = std::string{xsd_namespace};
    for (const auto& [other, prefix] : prefixes) {
      if (!other.empty()) { document.namespace_bindings[prefix] = other; }
      if (other == uri) { continue; }
      auto imported = node("import");
      if (!other.empty()) { set_attribute(imported, "namespace", other); }
      set_attribute(imported, "schemaLocation", filenames.at(other));
      document.children.push_back(std::move(imported));
    }
  }
  const auto qname = [&](const xml::name& name) {
    return name.namespace_uri.empty() ? name.local : prefixes.at(name.namespace_uri) + ':' + name.local;
  };
  const auto identity_xpath = [&](const xml::identity_path& path) {
    std::string expression = path.descendants ? ".//" : "";
    for (const auto& step : path.steps) {
      if (!expression.empty() && expression.back() != '/') { expression += '/'; }
      expression += qname(step);
    }
    if (path.attribute) {
      if (!expression.empty() && expression.back() != '/') { expression += '/'; }
      expression += '@' + qname(*path.attribute);
    }
    return expression.empty() ? std::string{"."} : expression;
  };
  std::map<const syntax_node*, xml::name> types;
  for (const auto& [root, source] : roots) {
    types.emplace(source, xml::name{"record_" + std::to_string(types.size()), root.namespace_uri});
  }
  const std::map<std::string, std::string> primitives{{"string", "string"}, {"bool", "boolean"},
      {"int8", "byte"}, {"uint8", "unsignedByte"}, {"int16", "short"}, {"uint16", "unsignedShort"},
      {"int32", "int"}, {"uint32", "unsignedInt"}, {"int64", "long"}, {"uint64", "unsignedLong"},
      {"float", "float"}, {"double", "double"}, {"float32", "float"}, {"float64", "double"}};
  const auto name_bindings = [&](std::string_view lexical, const std::vector<xml::name>& names) {
    std::map<std::string, std::string> result{};
    const auto tokens = lexical_words(lexical);
    if (tokens.size() != names.size()) { throw std::invalid_argument{"Exported QName literal has inconsistent item identities"}; }
    for (std::size_t index{}; index < tokens.size(); ++index) {
      const auto colon = tokens[index].find(':');
      result.emplace(colon == std::string_view::npos ? "" : std::string{tokens[index].substr(0U, colon)}, names[index].namespace_uri);
    }
    return result;
  };
  // Reconstruct complete simple-type intersections, retaining union member order and inherited OR-pattern groups.
  const auto scalar_declaration = [&](auto&& self, const scalar_facets& facets, std::string_view fallback) -> xml::element {
    const auto lexical = facets.lexical_kind.empty() ? std::string{fallback} : facets.lexical_kind;
    auto simple = node("simpleType");
    simple.namespace_bindings[schema_prefix] = std::string{xsd_namespace};
    auto restriction = node("restriction");
    if (!facets.union_members.empty()) {
      auto base = node("simpleType");
      auto alternatives = node("union");
      for (const auto& branch : facets.union_members) { alternatives.children.push_back(self(self, branch, "string")); }
      base.children.push_back(std::move(alternatives));
      restriction.children.push_back(std::move(base));
    } else if (!facets.list_item_kind.empty()) {
      auto base = node("simpleType");
      auto list = node("list");
      if (facets.list_item_facets.empty()) { set_attribute(list, "itemType", schema_prefix + ':' + facets.list_item_kind); }
      else { list.children.push_back(self(self, facets.list_item_facets.front(), facets.list_item_kind)); }
      base.children.push_back(std::move(list));
      restriction.children.push_back(std::move(base));
    } else { set_attribute(restriction, "base", schema_prefix + ':' + lexical); }
    for (const auto& group : facets.pattern_groups) {
      auto base = node("simpleType");
      base.children.push_back(std::move(restriction));
      restriction = node("restriction");
      restriction.children.push_back(std::move(base));
      for (const auto& expression : group) {
        auto pattern = node("pattern"); set_attribute(pattern, "value", expression);
        restriction.children.push_back(std::move(pattern));
      }
    }
    // Place current restrictions outside inherited pattern steps so their pattern alternatives remain conjunctive.
    if (!facets.pattern_groups.empty()) {
      auto base = node("simpleType"); base.children.push_back(std::move(restriction));
      restriction = node("restriction"); restriction.children.push_back(std::move(base));
    }
    const auto append = [&](std::string_view kind, std::string text) -> xml::element& {
      auto facet = node(kind); set_attribute(facet, "value", std::move(text));
      restriction.children.push_back(std::move(facet)); return restriction.children.back();
    };
    if (facets.min_inclusive) { append("minInclusive", *facets.min_inclusive); }
    if (facets.max_inclusive) { append("maxInclusive", *facets.max_inclusive); }
    if (facets.min_exclusive) { append("minExclusive", *facets.min_exclusive); }
    if (facets.max_exclusive) { append("maxExclusive", *facets.max_exclusive); }
    if (facets.length) { append("length", std::to_string(*facets.length)); }
    if (facets.min_length) { append("minLength", std::to_string(*facets.min_length)); }
    if (facets.max_length) { append("maxLength", std::to_string(*facets.max_length)); }
    for (std::size_t index{}; index < facets.enumeration.size(); ++index) {
      auto& facet = append("enumeration", facets.enumeration[index]);
      if (!facets.union_enumeration_scopes.empty()) {
        for (const auto& [prefix, uri] : facets.union_enumeration_scopes.at(index)) {
          if (prefix.empty() || !uri.empty()) { facet.namespace_bindings.emplace(prefix, uri); }
        }
      }
      else if (!facets.qname_enumeration.empty()) { facet.namespace_bindings = name_bindings(facets.enumeration[index], {facets.qname_enumeration.at(index)}); }
      else if (!facets.qname_list_enumeration.empty()) { facet.namespace_bindings = name_bindings(facets.enumeration[index], facets.qname_list_enumeration.at(index)); }
    }
    for (const auto& expression : facets.patterns) { append("pattern", expression); }
    // Union whiteSpace is fixed by XSD and cannot be redeclared as a restriction facet.
    if (facets.union_members.empty() && !facets.whitespace.empty()) { append("whiteSpace", facets.whitespace); }
    simple.children.push_back(std::move(restriction));
    return simple;
  };
  const auto assign_type = [&](xml::element& declaration, const field& value) {
    if (value.prefix_list) {
      throw std::invalid_argument{"XSD export of bound prefix-list namespaces requires an explicit external capability contract"};
    }
    const auto& type = leaf(*value.source);
    if (type.type == object_type::class_type) {
      const auto found = types.find(type.resolved_node);
      if (found == types.end()) { throw std::invalid_argument{"XSD export requires every nested record"}; }
      set_attribute(declaration, "type", qname(found->second));
      return;
    }
    const bool native_enum = type.type == object_type::enum_type;
    const auto primitive = primitives.find(type.name);
    if (!native_enum && primitive == primitives.end()) {
      throw std::invalid_argument{"Unsupported XSD export scalar: " + type.name};
    }
    const auto& facets = value.facets;
    const auto lexical = facets.lexical_kind.empty() ? (value.qname ? "QName" :
        native_enum ? std::string{"string"} : primitive->second) : facets.lexical_kind;
    std::vector<std::string> enum_names;
    if (native_enum) {
      if (value.list || !facets.list_item_kind.empty()) {
        throw std::invalid_argument{"XSD native enum list export requires restricted item types"};
      }
      if (!native_enum_string_kind(lexical, facets.whitespace)) {
        throw std::invalid_argument{"XSD native enum export requires xs:string with preserve whitespace"};
      }
      const auto& enumeration = static_cast<const enum_node&>(*type.resolved_node);
      // Native enum conversion uses authored wire names, independently of generated identifier casing.
      // Restrict that finite inventory with the already compiled XSD contract instead of widening a bound enum.
      for (const auto& name : enumeration.enum_name_list) {
        try {
          if (value.compiled_facets) { value.compiled_facets->validate(name); }
          enum_names.push_back(name);
        } catch (const std::invalid_argument&) {
          // A declared name outside a scalar restriction remains unavailable in the exported XML contract.
        }
      }
      if (enum_names.empty()) {
        throw std::invalid_argument{"XSD native enum export has no permitted wire names"};
      }
      const auto whitespace = facets.whitespace == "collapse" ? xml::whitespace_policy::collapse :
          facets.whitespace == "replace" ? xml::whitespace_policy::replace : xml::whitespace_policy::preserve;
      for (const auto* constrained : {&value.default_value, &value.fixed_value}) {
        if (*constrained && std::find(enum_names.begin(), enum_names.end(),
            xml::normalize_whitespace(**constrained, whitespace)) == enum_names.end()) {
          throw std::invalid_argument{"XSD native enum default/fixed value is not a permitted wire name"};
        }
      }
    }
    if (!facets.union_members.empty() || !facets.list_item_facets.empty() || !facets.pattern_groups.empty() ||
        !facets.qname_enumeration.empty() || !facets.qname_list_enumeration.empty()) {
      auto selected = facets;
      if (native_enum) { selected.enumeration = enum_names; }
      declaration.children.push_back(scalar_declaration(scalar_declaration, selected, lexical));
      return;
    }
    const bool restricted = native_enum || facets.min_inclusive || facets.max_inclusive || facets.min_exclusive ||
        facets.max_exclusive || facets.length || facets.min_length || facets.max_length ||
        !facets.enumeration.empty() || !facets.patterns.empty() || !facets.whitespace.empty() ||
        !facets.list_item_kind.empty();
    if (!restricted) { set_attribute(declaration, "type", schema_prefix + ':' + lexical); return; }
    auto simple = node("simpleType");
    auto restriction = node("restriction");
    if (facets.list_item_kind.empty()) { set_attribute(restriction, "base", schema_prefix + ':' + lexical); }
    else {
      auto base = node("simpleType");
      auto list = node("list");
      set_attribute(list, "itemType", schema_prefix + ':' + facets.list_item_kind);
      base.children.push_back(std::move(list));
      restriction.children.push_back(std::move(base));
    }
    const auto append = [&](std::string_view kind, std::string text) {
      auto facet = node(kind);
      set_attribute(facet, "value", std::move(text));
      restriction.children.push_back(std::move(facet));
    };
    if (facets.min_inclusive) { append("minInclusive", *facets.min_inclusive); }
    if (facets.max_inclusive) { append("maxInclusive", *facets.max_inclusive); }
    if (facets.min_exclusive) { append("minExclusive", *facets.min_exclusive); }
    if (facets.max_exclusive) { append("maxExclusive", *facets.max_exclusive); }
    if (facets.length) { append("length", std::to_string(*facets.length)); }
    if (facets.min_length) { append("minLength", std::to_string(*facets.min_length)); }
    if (facets.max_length) { append("maxLength", std::to_string(*facets.max_length)); }
    for (const auto& item : native_enum ? enum_names : facets.enumeration) { append("enumeration", item); }
    for (const auto& item : facets.patterns) { append("pattern", item); }
    if (!facets.whitespace.empty()) { append("whiteSpace", facets.whitespace); }
    simple.children.push_back(std::move(restriction));
    declaration.children.push_back(std::move(simple));
  };
  // Preserve schema literal scopes independently of generated component-reference aliases on the same declaration.
  const auto assign_value_constraints = [&](xml::element& declaration, const field& value) {
    if (value.default_value) { set_attribute(declaration, "default", *value.default_value); }
    if (value.fixed_value) { set_attribute(declaration, "fixed", *value.fixed_value); }
    const auto& lexical = value.fixed_value ? value.fixed_value : value.default_value;
    if (!lexical) { return; }
    std::map<std::string, std::string> bindings;
    if (value.union_fixed) { bindings = value.union_fixed_scope; }
    else if (value.union_default) { bindings = value.union_default_scope; }
    else if (value.qname_fixed || value.qname_default) {
      bindings = name_bindings(*lexical, {value.qname_fixed ? *value.qname_fixed : *value.qname_default});
    } else if (value.qname_list_fixed || value.qname_list_default) {
      bindings = name_bindings(*lexical, value.qname_list_fixed ? *value.qname_list_fixed : *value.qname_list_default);
    }
    std::map<std::string, std::string> components{{schema_prefix, std::string{xsd_namespace}}};
    for (const auto& [uri, prefix] : prefixes) { if (!uri.empty()) { components.emplace(prefix, uri); } }
    for (auto& entry : declaration.attributes) {
      const auto& name = entry.qualified_name;
      auto& text = entry.value;
      if (name.local != "type" && name.local != "ref" && name.local != "base") { continue; }
      const auto colon = text.find(':');
      if (colon == std::string::npos) {
        if (bindings.contains("") && !bindings.at("").empty()) {
          throw std::invalid_argument{"XSD export of an unqualified component beneath a nonempty QName literal default requires inline type placement"};
        }
        continue;
      }
      const auto prefix = text.substr(0U, colon);
      const auto original = components.find(prefix);
      const auto replaced = bindings.find(prefix);
      if (original == components.end() || replaced == bindings.end() || original->second == replaced->second) { continue; }
      std::string alias;
      for (std::size_t index{};; ++index) {
        alias = "component" + std::to_string(index);
        if (!components.contains(alias) && !bindings.contains(alias) && !declaration.namespace_bindings.contains(alias)) { break; }
      }
      declaration.namespace_bindings[alias] = original->second;
      text = alias + text.substr(colon);
    }
    for (const auto& [prefix, uri] : bindings) {
      if (prefix.empty() || !uri.empty()) { declaration.namespace_bindings[prefix] = uri; }
    }
  };
  std::map<std::tuple<bool, std::string, std::string>, std::string> global_contracts;
  const auto localize = [&](xml::element declaration, const xml::name& name, bool attribute,
                            const std::string& owner) {
    set_attribute(declaration, "name", name.local);
    if (name.namespace_uri.empty() || name.namespace_uri == owner) {
      set_attribute(declaration, "form", name.namespace_uri.empty() ? "unqualified" : "qualified");
      return declaration;
    }
    const auto bytes = xml::write(declaration);
    const auto key = std::tuple{attribute, name.namespace_uri, name.local};
    const auto [found, inserted] = global_contracts.emplace(key, bytes);
    if (!inserted && found->second != bytes) {
      throw std::invalid_argument{"Conflicting exported global XML field declarations"};
    }
    if (inserted) { documents.at(name.namespace_uri).children.push_back(std::move(declaration)); }
    auto reference = node(attribute ? "attribute" : "element");
    set_attribute(reference, "ref", qname(name));
    return reference;
  };
  for (const auto& [root_name, source] : roots) {
    const auto& contract = records.at(source);
    auto root = node("element");
    set_attribute(root, "name", root_name.local);
    // A no-namespace type cannot be referenced beneath a literal's nonempty default namespace.
    // Inline its anonymous equivalent on that root while keeping the reusable global declaration.
    const bool inline_root_type = types.at(source).namespace_uri.empty() &&
        std::any_of(contract.fields.begin(), contract.fields.end(), [&](const auto& value) {
          if (!value.text) { return false; }
          const auto& literal = value.fixed_value ? value.fixed_value : value.default_value;
          if (!literal) { return false; }
          const auto& scope = value.union_fixed ? value.union_fixed_scope : value.union_default_scope;
          if (const auto found = scope.find(""); found != scope.end()) { return !found->second.empty(); }
          if (value.qname_fixed || value.qname_default) {
            return literal->find(':') == std::string::npos &&
                !(value.qname_fixed ? value.qname_fixed : value.qname_default)->namespace_uri.empty();
          }
          if (value.qname_list_fixed || value.qname_list_default) {
            const auto bindings = name_bindings(*literal, value.qname_list_fixed ? *value.qname_list_fixed : *value.qname_list_default);
            const auto found = bindings.find("");
            return found != bindings.end() && !found->second.empty();
          }
          return false;
        });
    if (!inline_root_type) { set_attribute(root, "type", qname(types.at(source))); }
    for (const auto& value : contract.fields) {
      if (value.text) {
        // Simple-content value constraints belong to the global element, never its extension/type declaration.
        assign_value_constraints(root, value);
      }
    }
    if (contract.identity) {
      for (const auto& constraint : contract.identity->constraints) {
        auto identity = node(constraint.kind == xml::identity_kind::key ? "key" :
            constraint.kind == xml::identity_kind::unique ? "unique" : "keyref");
        for (const auto& [uri, prefix] : prefixes) { if (!uri.empty()) { identity.namespace_bindings[prefix] = uri; } }
        set_attribute(identity, "name", constraint.id.local);
        if (constraint.refer) { set_attribute(identity, "refer", qname(*constraint.refer)); }
        auto selector = node("selector");
        set_attribute(selector, "xpath", identity_xpath(constraint.selector));
        identity.children.push_back(std::move(selector));
        for (const auto& value : constraint.fields) {
          auto projection = node("field");
          set_attribute(projection, "xpath", identity_xpath(value.path));
          identity.children.push_back(std::move(projection));
        }
        root.children.push_back(std::move(identity));
      }
    }
    const auto root_index = documents.at(root_name.namespace_uri).children.size();
    documents.at(root_name.namespace_uri).children.push_back(std::move(root));
    auto complex = node("complexType");
    set_attribute(complex, "name", types.at(source).local);
    auto sequence = node(contract.mixed ? "choice" : contract.all_groups.empty() ? "sequence" : "all");
    if (contract.mixed) {
      const auto extent = contract.mixed->source->fixed_extent;
      set_attribute(sequence, "minOccurs", std::to_string(extent));
      set_attribute(sequence, "maxOccurs", extent ? std::to_string(extent) : "unbounded");
      const auto child_declaration = [&](const mixed_child_branch& child) {
        const auto found = types.find(child.value_type->resolved_node);
        if (found == types.end()) {
          throw std::invalid_argument{"XSD ordered export requires every typed child record"};
        }
        auto declaration = node("element");
        set_attribute(declaration, "type", qname(found->second));
        const auto existing = roots.find(child.qualified_name);
        if (child.qualified_name.namespace_uri != root_name.namespace_uri &&
            existing != roots.end()) {
          if (existing->second != child.value_type->resolved_node) {
            throw std::invalid_argument{"XSD ordered child conflicts with an exported root"};
          }
          declaration = node("element");
          set_attribute(declaration, "ref", qname(child.qualified_name));
        } else {
          declaration = localize(std::move(declaration), child.qualified_name, false,
              root_name.namespace_uri);
        }
        return declaration;
      };
      if (contract.mixed->grammar) {
        const auto export_particle = [&](auto&& self, const content_particle& value) -> xml::element {
          auto declaration = node(value.kind == particle_kind::choice ? "choice" : "sequence");
          if (value.kind == particle_kind::element) {
            const auto child = std::find_if(contract.mixed->children.begin(), contract.mixed->children.end(),
                [&](const auto& branch) { return branch.qualified_name == value.name; });
            if (child == contract.mixed->children.end()) {
              throw std::invalid_argument{"Exported ordered grammar has no typed alternative"};
            }
            declaration = child_declaration(*child);
          } else {
            for (const auto& child : value.children) { declaration.children.push_back(self(self, child)); }
          }
          set_attribute(declaration, "minOccurs", std::to_string(value.minimum));
          set_attribute(declaration, "maxOccurs", value.maximum ? std::to_string(*value.maximum) : "unbounded");
          return declaration;
        };
        if (extent != 0U) {
          // Fixed native storage and the XSD grammar must have the same complete length language.
          std::set<std::size_t> current{1U};
          std::size_t remaining = particle_limits{}.max_work_units;
          for (std::size_t count{};; ++count) {
            std::set<std::size_t> next;
            for (const auto state : current) {
              if (count != extent && contract.mixed->grammar->accepting[state]) {
                throw std::invalid_argument{"Fixed ordered XSD export requires an exact matching grammar extent"};
              }
              for (const auto target : contract.mixed->grammar->transitions[state]) {
                if (remaining == 0U) { throw std::length_error{"Fixed ordered export work limit exceeded"}; }
                --remaining;
                if (target != 0U) { next.insert(target); }
              }
            }
            current = std::move(next);
            if (count == extent || current.empty()) { break; }
          }
          if (!current.empty()) {
            throw std::invalid_argument{"Fixed ordered XSD export grammar permits excess content"};
          }
        }
        sequence = export_particle(export_particle, contract.particles->root);
      } else {
        for (const auto& child : contract.mixed->children) {
          sequence.children.push_back(child_declaration(child));
        }
      }
    }
    if (contract.all_groups.size() > 1U || (!contract.all_groups.empty() &&
        contract.all_groups.front().alternatives.size() != static_cast<std::size_t>(std::count_if(
            contract.fields.begin(), contract.fields.end(),
            [](const auto& value) { return !value.attribute && !value.text; })))) {
      throw std::invalid_argument{"XSD export requires one complete all compositor"};
    }
    if (!contract.all_groups.empty()) {
      set_attribute(sequence, "minOccurs", std::to_string(contract.all_groups.front().minimum));
    }
    std::vector<xml::element> attributes;
    std::optional<xml::element> text_extension;
    for (const auto& value : contract.fields) {
      auto declaration = node(value.attribute ? "attribute" : "element");
      assign_type(declaration, value);
      if (value.text) {
        auto extension = node("extension");
        if (!declaration.children.empty()) {
          auto simple = std::move(declaration.children.front());
          const xml::name name{"text_" + types.at(source).local, root_name.namespace_uri};
          set_attribute(simple, "name", name.local);
          documents.at(root_name.namespace_uri).children.push_back(std::move(simple));
          set_attribute(extension, "base", qname(name));
        } else { set_attribute(extension, "base", std::string{attribute(declaration, "type")}); }
        text_extension = std::move(extension);
        continue;
      }
      assign_value_constraints(declaration, value);
      auto member_node = localize(std::move(declaration), value.qualified_name, value.attribute,
                                  value.wrapper ? value.wrapper->namespace_uri : root_name.namespace_uri);
      if (value.attribute) {
        set_attribute(member_node, "use", value.min_occurs == 0U ? "optional" : "required");
        attributes.push_back(std::move(member_node));
        continue;
      }
      auto minimum = value.min_occurs;
      auto maximum = value.max_occurs;
      if (value.source->fixed_extent != 0U) {
        const auto extent = value.source->fixed_extent;
        if (extent < minimum || (maximum && extent > *maximum)) {
          throw std::invalid_argument{"XSD occurrence bounds differ from fixed array extent"};
        }
        minimum = extent; maximum = extent;
      }
      set_attribute(member_node, "minOccurs", std::to_string(minimum));
      set_attribute(member_node, "maxOccurs", maximum ? std::to_string(*maximum) : "unbounded");
      if (value.wrapper) {
        auto wrapper = node("element");
        auto wrapper_type = node("complexType");
        auto items = node("sequence");
        items.children.push_back(std::move(member_node));
        wrapper_type.children.push_back(std::move(items));
        wrapper.children.push_back(std::move(wrapper_type));
        member_node = localize(std::move(wrapper), *value.wrapper, false, root_name.namespace_uri);
      }
      sequence.children.push_back(std::move(member_node));
    }
    if (text_extension) {
      if (!sequence.children.empty()) { throw std::invalid_argument{"XSD text cannot accompany child particles"}; }
      for (auto& value : attributes) { text_extension->children.push_back(std::move(value)); }
      auto content = node("simpleContent");
      content.children.push_back(std::move(*text_extension));
      complex.children.push_back(std::move(content));
    } else {
      if (!sequence.children.empty()) { complex.children.push_back(std::move(sequence)); }
      for (auto& value : attributes) { complex.children.push_back(std::move(value)); }
    }
    if (inline_root_type) {
      auto anonymous = complex;
      anonymous.namespace_bindings[""] = "";
      std::erase_if(anonymous.attributes, [](const auto& item) { return item.qualified_name.local == "name"; });
      auto& declaration = documents.at(root_name.namespace_uri).children.at(root_index);
      declaration.children.insert(declaration.children.begin(), std::move(anonymous));
    }
    documents.at(root_name.namespace_uri).children.push_back(std::move(complex));
  }
  schema_bundle result;
  std::string identity_input{"Serializer XSD bundle 1\n"};
  for (auto& [target, document] : documents) {
    const auto path = filenames.at(target);
    const auto bytes = xml::write(document);
    identity_input += std::to_string(path.size()) + ':' + path;
    identity_input += std::to_string(bytes.size()) + ':' + bytes;
    result.documents.push_back({path, bytes, target, xml::parse(bytes)});
  }
  result.identity = rohit::digest_to_hex(rohit::make_digest<rohit::digest_algorithm::sha256>(identity_input));
  validate_bundle(result);
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
