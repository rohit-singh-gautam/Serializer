// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xsd_import.hpp"
#include "xml_schema.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace rohit::serializer::writer::xml_binding {
namespace {
constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};

// Borrow one unqualified schema attribute without inserting missing keys.
std::string_view attribute(const xml::element& node, std::string_view key) {
  for (const auto& value : node.attributes) {
    if (value.qualified_name.namespace_uri.empty() && value.qualified_name.local == key) {
      return value.value;
    }
  }
  return {};
}

// Apply the XSD boolean whitespace and lexical rules to type metadata.
bool true_attribute(const xml::element& node, std::string_view key) {
  auto value = attribute(node, key);
  constexpr std::string_view whitespace{" \t\r\n"};
  const auto first = value.find_first_not_of(whitespace);
  if (first == std::string_view::npos) { return false; }
  value = value.substr(first, value.find_last_not_of(whitespace) - first + 1U);
  return value == "true" || value == "1";
}

// Quote a schema literal without allowing source text to inject declarations.
std::string quote(std::string_view value) {
  std::string result{"\""};
  for (const auto byte : value) {
    switch (byte) {
    case '\\': result += "\\\\"; break;
    case '"': result += "\\\""; break;
    case '\n': result += "\\n"; break;
    case '\r': result += "\\r"; break;
    case '\t': result += "\\t"; break;
    default:
      if (static_cast<unsigned char>(byte) < 0x20U) {
        throw std::invalid_argument{"XSD import cannot represent a control byte in a schema literal"};
      }
      result += byte;
    }
  }
  return result + '"';
}

// Keep generated identifiers portable, deterministic and independent of declaration order.
std::string identifier(std::string_view name) {
  std::string result;
  for (const auto byte : name) {
    if ((byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9')) {
      result += byte;
    } else if (byte >= 'A' && byte <= 'Z') {
      if (!result.empty() && result.back() != '_') { result += '_'; }
      result += static_cast<char>(byte - 'A' + 'a');
    } else if (result.empty() || result.back() != '_') {
      result += '_';
    }
  }
  if (result.empty() || (result.front() >= '0' && result.front() <= '9')) {
    result.insert(0, "value_");
  }
  // Prefixing protects current and future language keywords without maintaining a second lexer.
  return "xsd_" + result;
}

// Assign explicit stable wire IDs from expanded XML identity, detecting rather than hiding collisions.
std::uint32_t stable_id(std::string_view identity) {
  constexpr std::uint32_t fnv_prime{16777619U};
  constexpr std::uint32_t fnv_offset{2166136261U};
  constexpr std::uint32_t maximum_id{0x1fffffffU};
  std::uint32_t value{fnv_offset};
  for (const auto byte : identity) {
    value = (value ^ static_cast<unsigned char>(byte)) * fnv_prime;
  }
  value &= maximum_id;
  return value == 0U ? 1U : value;
}

// Resolve QName-valued declarations using the source node's namespace scope.
xml::name resolve(const xml::element& node, std::string_view value) {
  const auto colon = value.find(':');
  const auto prefix = colon == std::string_view::npos ? std::string{} : std::string{value.substr(0, colon)};
  const auto found = node.namespace_bindings.find(prefix);
  const bool predefined = prefix == "xml";
  if (value.empty() || colon == 0U || (colon != std::string_view::npos &&
      (colon + 1U == value.size() || value.find(':', colon + 1U) != std::string_view::npos ||
       (!predefined && found == node.namespace_bindings.end())))) {
    throw std::invalid_argument{"Invalid or unbound imported XSD QName"};
  }
  return {std::string{value.substr(colon == std::string_view::npos ? 0U : colon + 1U)},
          predefined ? "http://www.w3.org/XML/1998/namespace" :
              found == node.namespace_bindings.end() ? std::string{} : found->second};
}

// Read an occurrence bound exactly, refusing partial and overflowing conversions.
std::size_t count(std::string_view text, std::size_t fallback = 1U) {
  if (text.empty()) { return fallback; }
  std::size_t value{};
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    throw std::invalid_argument{"Invalid imported XSD occurrence bound"};
  }
  return value;
}

using declaration = std::pair<const schema_bundle::document*, const xml::element*>;
using declaration_key = std::pair<std::string, std::string>;

// Compile one finite graph into ordinary owning declarations; no native runtime data is retained.
class importer {
  const schema_bundle& bundle_;
  const import_options& options_;
  std::map<std::pair<std::string, declaration_key>, declaration> declarations_{};
  struct model {
    declaration declaration_{};
    xml::name root{};
    std::string name{};
    std::string text{};
    unsigned state{};
  };
  std::map<const xml::element*, model> models_{};
  std::set<std::string> class_names_{};
  std::vector<const xml::element*> order_{};

  // Resolve one global declaration uniquely across the pinned bundle.
  declaration find(std::string_view kind, const xml::name& name) const {
    const auto found = declarations_.find({std::string{kind}, {name.namespace_uri, name.local}});
    if (found == declarations_.end()) {
      throw std::invalid_argument{"Unresolved imported XSD " + std::string{kind} + ": " + name.local};
    }
    return found->second;
  }

  // Select an element's local complex type or its named complex declaration.
  declaration complex(const schema_bundle::document& owner, const xml::element& node) const {
    for (const auto& child : node.children) {
      if (child.qualified_name.local == "complexType") { return {&owner, &child}; }
      if (child.qualified_name.local == "simpleType") { return {}; }
    }
    const auto type = resolve(node, attribute(node, "type"));
    if (type.namespace_uri == xsd_namespace) { return {}; }
    const auto found = declarations_.find({"complexType", {type.namespace_uri, type.local}});
    return found == declarations_.end() ? declaration{} : found->second;
  }

  // Preserve the exact primitive representation through facet-free or constrained aliases.
  std::string scalar(const xml::element& node) const {
    const auto type = resolve_scalar_type(bundle_, node).primitive;
    const std::map<std::string, std::string> primitives{
        {"string", "string"}, {"boolean", "bool"}, {"byte", "int8"},
        {"unsignedByte", "uint8"}, {"short", "int16"}, {"unsignedShort", "uint16"},
        {"int", "int32"}, {"unsignedInt", "uint32"}, {"long", "int64"},
        {"unsignedLong", "uint64"}, {"float", "float"}, {"double", "double"},
        {"normalizedString", "string"}, {"token", "string"}, {"language", "string"},
        {"Name", "string"}, {"NCName", "string"}, {"NMTOKEN", "string"},
        {"anyURI", "string"}, {"QName", "string"}, {"decimal", "string"},
        {"integer", "string"}, {"nonNegativeInteger", "string"}, {"positiveInteger", "string"},
        {"nonPositiveInteger", "string"}, {"negativeInteger", "string"}};
    const auto found = primitives.find(type.local);
    if (found == primitives.end()) {
      throw std::invalid_argument{"Unsupported imported XSD primitive: " + type.local};
    }
    return found->second;
  }

  // Detect a lexical scalar list separately from repeated XML element occurrences.
  std::optional<std::pair<std::string, std::optional<std::size_t>>> list_type(
      const xml::element& source) const {
    using result_type = std::optional<std::pair<std::string, std::optional<std::size_t>>>;
    std::set<declaration_key> active;
    const auto visit = [&](auto&& self, const xml::element& node) -> result_type {
      const auto& kind = node.qualified_name.local;
      if (kind == "list") {
        const auto item = resolve(node, attribute(node, "itemType"));
        if (item.namespace_uri != xsd_namespace || item.local == "QName") {
          throw std::invalid_argument{"Imported typed lists require builtin non-QName scalar items"};
        }
        auto item_node = node;
        item_node.attributes = {{{"type", ""}, "xs:" + item.local}};
        item_node.namespace_bindings["xs"] = std::string{xsd_namespace};
        item_node.children.clear();
        return std::pair{scalar(item_node), std::optional<std::size_t>{}};
      }
      result_type result;
      for (const auto& child : node.children) {
        if (child.qualified_name.local == "simpleType" || child.qualified_name.local == "restriction" ||
            child.qualified_name.local == "list") { result = self(self, child); }
      }
      if (!result) {
        const auto type_text = attribute(node, kind == "restriction" ? "base" : "type");
        if (!type_text.empty()) {
          const auto type = resolve(node, type_text);
          if (type.namespace_uri == xsd_namespace && type.local == "NMTOKENS") {
            result = std::pair{std::string{"string"}, std::optional<std::size_t>{}};
          } else if (type.namespace_uri != xsd_namespace) {
            if (!active.emplace(type.namespace_uri, type.local).second) {
              throw std::invalid_argument{"Cyclic imported XSD list alias"};
            }
            result = self(self, *find("simpleType", type).second);
          }
        }
      }
      if (result && kind == "restriction") {
        for (const auto& child : node.children) {
          if (child.qualified_name.local == "length") {
            result->second = count(attribute(child, "value"), 0U);
          }
        }
      }
      return result;
    };
    return visit(visit, source);
  }

  // Register a deterministic record identity before traversing its dependencies.
  model& register_model(declaration value, xml::name root) {
    auto [found, inserted] = models_.try_emplace(value.second);
    if (inserted) {
      auto name = identifier(attribute(*value.second, "name").empty() ? root.local
                                                                    : attribute(*value.second, "name"));
      // Namespace identity participates from the first import, so a later namespace cannot rename it.
      if (!root.namespace_uri.empty()) {
        name += '_' + std::to_string(stable_id(root.namespace_uri));
      }
      if (!class_names_.insert(name).second) {
        name += '_' + std::to_string(stable_id(root.namespace_uri + ":" + root.local));
        if (!class_names_.insert(name).second) {
          throw std::invalid_argument{"Imported XSD type names collide after portable normalization"};
        }
      }
      found->second = {value, std::move(root), std::move(name), {}, 0U};
    }
    return found->second;
  }

  // Emit one model after all owning dependencies, rejecting recursive owning storage.
  void compile(model& value) {
    if (value.state == 2U) { return; }
    if (value.state == 1U) { throw std::invalid_argument{"Recursive XSD owning records are unsupported"}; }
    value.state = 1U;
    const auto& [owner, complex_node] = value.declaration_;
    if (true_attribute(*complex_node, "abstract")) {
      throw std::invalid_argument{"Abstract imported XSD types require dynamic type selection"};
    }
    const auto content = expand_complex_type(bundle_, *owner, *complex_node);
    std::set<std::string> names;
    std::set<std::uint32_t> ids;
    std::string fields;
    const auto emit = [&](const xml::element& occurrence, bool is_attribute, bool in_choice,
                          bool text, const schema_bundle::document* defining_owner,
                          bool repeated_choice = false) {
      auto selected = declaration{defining_owner, &occurrence};
      const bool reference = !attribute(occurrence, "ref").empty();
      if (reference) {
        selected = find(is_attribute ? "attribute" : "element",
                        resolve(occurrence, attribute(occurrence, "ref")));
      }
      const auto& node = *selected.second;
      const auto local = attribute(node, "name");
      if (local.empty()) { throw std::invalid_argument{"Imported XSD field requires a name"}; }
      const auto form = attribute(node, "form");
      const bool qualified = reference || form == "qualified" || (form.empty() &&
          attribute(selected.first->root, is_attribute ? "attributeFormDefault" :
                                                         "elementFormDefault") == "qualified");
      const auto uri = qualified ? selected.first->target : std::string{};
      auto name = identifier(local);
      if (!names.insert(name).second) {
        name += is_attribute ? "_attribute" : "_element";
        if (!names.insert(name).second) {
          name += '_' + std::to_string(stable_id('{' + uri + '}' + std::string{local}));
          if (!names.insert(name).second) { throw std::invalid_argument{"Duplicate imported XML field identity"}; }
        }
      }
      const auto id = stable_id(std::string{is_attribute ? "@" : ""} + '{' + uri + '}' + std::string{local});
      if (!ids.insert(id).second) { throw std::invalid_argument{"Imported XML field stable-ID collision"}; }
      const auto minimum = is_attribute ? (attribute(occurrence, "use") == "required" ? 1U : 0U)
                                        : count(attribute(occurrence, "minOccurs"));
      const auto maximum = is_attribute ? std::string_view{"1"} : attribute(occurrence, "maxOccurs");
      const bool repeated = repeated_choice || maximum == "unbounded" || count(maximum) > 1U;
      if (is_attribute && attribute(occurrence, "use") == "prohibited") {
        throw std::invalid_argument{"Prohibited XSD attributes cannot become stored fields"};
      }
      if (maximum != "unbounded" && (count(maximum) == 0U || minimum > count(maximum))) {
        throw std::invalid_argument{"Empty imported XSD occurrence interval"};
      }
      std::string type;
      bool lexical_list{};
      if (const auto nested = complex(*selected.first, node); nested.second) {
        auto& child = register_model(nested,
            {attribute(*nested.second, "name").empty() ? std::string{local} :
                std::string{attribute(*nested.second, "name")}, nested.first->target});
        compile(child);
        type = child.name;
      } else if (const auto list = list_type(node)) {
        if (repeated || in_choice) {
          throw std::invalid_argument{"Repeated imported XML lists need nested collection mapping"};
        }
        lexical_list = true;
        if (minimum == 0U) { type = "optional<array<" + list->first + ">>"; }
        else {
          type = "array" + (list->second && *list->second != 0U
              ? '[' + std::to_string(*list->second) + ']' : std::string{}) +
              ' ' + list->first;
        }
      } else { type = scalar(node); }
      fields += "    public ";
      if (repeated) { fields += "array "; }
      if (!repeated && !lexical_list && (minimum == 0U || in_choice)) { type = "optional<" + type + '>'; }
      fields += type + ' ' + name + " (" + std::to_string(id) + ") xml(" +
          (text ? "text" : is_attribute ? "attribute" : "element") +
          (lexical_list ? ", list" : "");
      if (!text) { fields += ", name(" + quote(local) + "), namespace(" + quote(uri) + ')'; }
      fields += ");\n";
    };
    const auto particles = [&](auto&& self, const xml::element& group, bool in_choice,
                                const schema_bundle::document* defining_owner) -> void {
      const bool choice = group.qualified_name.local == "choice";
      const auto maximum = attribute(group, "maxOccurs");
      bool repeated_choice = choice && (maximum == "unbounded" || count(maximum) != 1U);
      if (!choice && (count(attribute(group, "minOccurs")) != 1U ||
                      maximum == "unbounded" || count(maximum) != 1U)) {
        throw std::invalid_argument{"Optional or repeated imported XSD sequences require grouped occurrence storage"};
      }
      if (choice) {
        for (const auto& child : group.children) {
          const auto child_maximum = attribute(child, "maxOccurs");
          if (child.qualified_name.local == "any" || child_maximum == "unbounded" ||
              count(child_maximum) != 1U) { repeated_choice = true; }
        }
        if (repeated_choice && std::count_if(group.children.begin(), group.children.end(),
            [](const auto& child) { return child.qualified_name.local == "element"; }) > 1) {
          throw std::invalid_argument{"Repeated imported XSD choices with multiple known branches require ordered variant storage"};
        }
      }
      for (const auto& child : group.children) {
        const auto& kind = child.qualified_name.local;
        if (kind == "element") { emit(child, false, in_choice || choice, false, defining_owner, repeated_choice); }
        else if (kind == "sequence" || kind == "choice" || kind == "all") {
          if (choice || in_choice) {
            throw std::invalid_argument{"Grouped imported XSD choice branches are unsupported"};
          }
          self(self, child, false, defining_owner);
        } else if (kind == "group") {
          if (choice || in_choice || count(attribute(child, "minOccurs")) != 1U ||
              count(attribute(child, "maxOccurs")) != 1U) {
            throw std::invalid_argument{"Grouped imported XSD occurrences require ordered grouped storage"};
          }
          const auto selected = resolve_model_group(bundle_, *defining_owner, child);
          self(self, *selected.node, false, selected.owner);
        } else if (kind != "annotation" && kind != "any") {
          throw std::invalid_argument{"Unsupported imported XSD particle: " + kind};
        }
      }
    };
    if (content.mixed) {
      if (std::any_of(content.children.begin(), content.children.end(), [](const auto& component) {
            const auto& kind = component.node->qualified_name.local;
            return kind != "attribute" && kind != "anyAttribute";
          })) { throw std::invalid_argument{"Imported mixed child/text content requires ordered text storage"}; }
      xml::element text;
      text.qualified_name = {"element", std::string{xsd_namespace}};
      text.namespace_bindings["xs"] = std::string{xsd_namespace};
      text.attributes = {{{"name", ""}, "value"}, {{"type", ""}, "xs:string"}};
      emit(text, false, false, true, owner);
    }
    for (const auto& component : content.children) {
      const auto& child = *component.node;
      const auto* defining_owner = component.owner;
      const auto& kind = child.qualified_name.local;
      if (kind == "sequence" || kind == "choice" || kind == "all") {
        particles(particles, child, false, defining_owner);
      }
      else if (kind == "group") {
        if (count(attribute(child, "minOccurs")) != 1U || count(attribute(child, "maxOccurs")) != 1U) {
          throw std::invalid_argument{"Optional or repeated imported model groups require grouped occurrence storage"};
        }
        const auto selected = resolve_model_group(bundle_, *defining_owner, child);
        particles(particles, *selected.node, false, selected.owner);
      }
      else if (kind == "attribute") { emit(child, true, false, false, defining_owner); }
      else if (kind == "simpleContent") {
        for (const auto& extension : child.children) {
          if (extension.qualified_name.local == "annotation") { continue; }
          if (extension.qualified_name.local != "extension") {
            throw std::invalid_argument{"XSD import simpleContent requires a supported extension"};
          }
          xml::element text_content = extension;
          text_content.qualified_name.local = "element";
          text_content.attributes = {{{"name", ""}, "value"},
                                {{"type", ""}, std::string{attribute(extension, "base")}}};
          text_content.children.clear();
          emit(text_content, false, false, true, defining_owner);
          const auto attributes = expand_complex_type(bundle_, *defining_owner, extension);
          for (const auto& item_component : attributes.children) {
            const auto& item = *item_component.node;
            if (item.qualified_name.local == "attribute") { emit(item, true, false, false, item_component.owner); }
            else if (item.qualified_name.local != "annotation" && item.qualified_name.local != "anyAttribute") {
              throw std::invalid_argument{"Unsupported imported XSD simpleContent extension"};
            }
          }
        }
      } else if (kind != "annotation" && kind != "anyAttribute") {
        throw std::invalid_argument{"Unsupported imported XSD complex content: " + kind};
      }
    }
    value.text = "  class " + value.name + " stable_ids {\n    xml(root(" + quote(value.root.local) +
        "), namespace(" + quote(value.root.namespace_uri) + "));\n" + fields + "  }\n";
    value.state = 2U;
    order_.push_back(complex_node);
  }

public:
  // Index global identities exactly once before resolving aliases and owning dependencies.
  importer(const schema_bundle& bundle, const import_options& options)
      : bundle_{bundle}, options_{options} {
    for (const auto& document : bundle_.documents) {
      for (const auto& node : document.root.children) {
        const auto& kind = node.qualified_name.local;
        if (kind == "annotation" || kind == "include" || kind == "import") { continue; }
        const auto name = attribute(node, "name");
        if (name.empty() || !declarations_.emplace(
            std::pair{kind, declaration_key{document.target, std::string{name}}},
            declaration{&document, &node}).second) {
          throw std::invalid_argument{"Duplicate or unnamed imported XSD global declaration"};
        }
      }
    }
  }

  // Publish only complete source after every reachable declaration has passed its subset checks.
  std::string generate(std::string_view schema_location) {
    if (options_.namespace_name.empty() || identifier(options_.namespace_name) !=
        "xsd_" + options_.namespace_name ||
        std::set<std::string>{"namespace", "class", "enum", "include", "using", "package", "serializer", "xml"}
            .contains(options_.namespace_name)) {
      throw std::invalid_argument{"XSD import namespace must be a lowercase snake_case identifier"};
    }
    std::vector<model*> roots;
    for (const auto& [key, declaration] : declarations_) {
      if (key.first != "element" || (!options_.root_name.empty() &&
          options_.root_name != key.second.second &&
          options_.root_name != '{' + key.second.first + '}' + key.second.second)) { continue; }
      const auto type = complex(*declaration.first, *declaration.second);
      if (!type.second) {
        throw std::invalid_argument{"XSD import requires complex global roots"};
      }
      auto& value = register_model(type, {key.second.second, key.second.first});
      if (value.root != xml::name{key.second.second, key.second.first}) {
        throw std::invalid_argument{
            "Multiple XSD global roots share one complex type; select one root explicitly"};
      }
      roots.push_back(&value);
    }
    if (roots.empty()) { throw std::invalid_argument{"No selected XSD global complex root"}; }
    for (auto* value : roots) { compile(*value); }
    std::string result{"// Generated from a checked local XSD bundle; regenerate after contract changes.\n"};
    result += "serializer version 1.20.0;\n\nnamespace " + options_.namespace_name + " {\n";
    result += "  using schema " + quote(schema_location) + ";\n\n";
    for (const auto* node : order_) { result += models_.at(node).text + '\n'; }
    return result + "}\n";
  }
};
} // namespace

// Preserve the pinned dependency graph and identity alongside the generated declarative source.
import_result import_xsd(const std::filesystem::path& input, const import_options& options) {
  const auto bundle = read_schema_bundle(input);
  importer compiler{*bundle, options};
  import_result result;
  result.schema_text = compiler.generate(options.schema_location.empty()
      ? std::filesystem::absolute(input).generic_string() : options.schema_location);
  result.bundle_identity = bundle->identity;
  for (const auto& document : bundle->documents) { result.dependencies.push_back(document.path); }
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
