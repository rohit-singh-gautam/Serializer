// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_identity_contract.hpp"

#include "xml_schema.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::xml_binding {
namespace {
constexpr std::string_view xsd_namespace{"http://www.w3.org/2001/XMLSchema"};

// Read only unqualified schema metadata, which is separate from instance XML attributes.
std::string_view attribute(const xml::element& node, std::string_view local) {
  for (const auto& value : node.attributes) {
    if (value.qualified_name.namespace_uri.empty() && value.qualified_name.local == local) { return value.value; }
  }
  return {};
}

// Identity reference checks consume explicit normalized values, without schema-default augmentation.
void reject_augmented_identity_field(const xml::element& node) {
  if (std::any_of(node.attributes.begin(), node.attributes.end(), [](const auto& value) {
        return value.qualified_name.namespace_uri.empty() &&
            (value.qualified_name.local == "default" || value.qualified_name.local == "fixed");
      })) {
    throw std::invalid_argument{
        "Portable XSD identity fields do not support schema default/fixed value augmentation"};
  }
}

struct component {
  const schema_bundle::document* owner{};
  const xml::element* node{};
  // Compare declaration identities without selecting MSVC STL's scalar-size vectorized find.
  bool operator==(const component& other) const noexcept {
    return owner == other.owner && node == other.node;
  }
};

// Bind strict selectors against actual XSD declarations instead of guessing field types from XML text.
class identity_binder {
  const schema_bundle& bundle_;
  const xml::identity_limits& limits_;
  std::size_t remaining_work_{};
  std::map<std::tuple<std::string, std::string, std::string>, component> globals_{};
  std::map<const xml::element*, const schema_bundle::document*> owners_{};
  // Bound graph searches and expansion using the same deterministic work policy as runtime checks.
  void work(std::size_t amount = 1U) {
    if (amount > remaining_work_) { throw std::length_error{"XSD identity binding work limit exceeded"}; }
    remaining_work_ -= amount;
  }
  // Locate a qualified global declaration without accidentally selecting a different namespace's type.
  std::optional<component> global(std::string_view kind, const xml::name& name) {
    work(kind.size() + name.local.size() + name.namespace_uri.size());
    const auto found = globals_.find({std::string{kind}, name.namespace_uri, name.local});
    return found == globals_.end() ? std::optional<component>{} : found->second;
  }
  // Require a previously pinned global declaration; no URI lookup or provider resolution occurs.
  component require_global(std::string_view kind, const xml::name& name) {
    const auto found = global(kind, name);
    if (!found) { throw std::invalid_argument{"XSD identity path references an unknown " + std::string{kind}}; }
    return *found;
  }
  // Resolve element/attribute refs before assigning their globally qualified instance name.
  component resolve_reference(component value, bool is_attribute) {
    const auto reference = attribute(*value.node, "ref");
    return reference.empty() ? value : require_global(is_attribute ? "attribute" : "element",
        xml::resolve_qname(reference, *value.node));
  }
  // Determine each original declaration's instance identity using its own form defaults.
  xml::name instance_name(component value, bool is_attribute) {
    const bool referenced = !attribute(*value.node, "ref").empty();
    value = resolve_reference(value, is_attribute);
    bool is_global{};
    for (const auto& child : value.owner->root.children) { if (&child == value.node) { is_global = true; break; } }
    const auto form = attribute(*value.node, "form");
    const bool qualified = referenced || is_global || form == "qualified" ||
        (form.empty() && attribute(value.owner->root,
            is_attribute ? "attributeFormDefault" : "elementFormDefault") == "qualified");
    return {std::string{attribute(*value.node, "name")}, qualified ? value.owner->target : std::string{}};
  }
  // Resolve only actual complex types, leaving built-in/simple declarations available to scalar binding.
  std::optional<component> complex(component value) {
    value = resolve_reference(value, false);
    if (value.node->qualified_name.local == "complexType") { return value; }
    for (const auto& child : value.node->children) {
      if (child.qualified_name.local == "complexType") { return component{value.owner, &child}; }
    }
    const auto type = attribute(*value.node, "type");
    if (type.empty()) { return {}; }
    const auto name = xml::resolve_qname(type, *value.node);
    return name.namespace_uri == xsd_namespace ? std::optional<component>{} : global("complexType", name);
  }
  // Flatten only static compositors/groups; all member names retain their original defining document.
  void collect(component value, bool attributes, std::vector<component>& result,
      std::set<const xml::element*>& active, std::size_t depth) {
    work();
    if (depth > limits_.max_depth || !active.insert(value.node).second) {
      throw std::invalid_argument{"Cyclic or excessively nested XSD identity particle projection"};
    }
    const auto& kind = value.node->qualified_name.local;
    if (kind == (attributes ? "anyAttribute" : "any")) {
      throw std::invalid_argument{"Portable XSD identity projections do not support wildcard declarations"};
    }
    if (kind == (attributes ? "attribute" : "element")) {
      if (!attributes || attribute(*value.node, "use") != "prohibited") {
        if (result.size() >= limits_.max_selected_nodes) { throw std::length_error{"XSD identity declaration selection limit exceeded"}; }
        result.push_back(value);
      }
    } else if (kind == "group") {
      const auto selected = resolve_model_group(bundle_, *value.owner, *value.node);
      collect({selected.owner, selected.node}, attributes, result, active, depth + 1U);
    } else if (kind == "sequence" || kind == "choice" || kind == "all" || kind == "simpleContent" ||
        kind == "extension" || kind == "restriction") {
      for (const auto& child : value.node->children) { collect({value.owner, &child}, attributes, result, active, depth + 1U); }
    }
    active.erase(value.node);
  }
  // Gather direct instance children or attributes after static complex inheritance expansion.
  std::vector<component> members(component value, bool attributes) {
    std::vector<component> result{};
    const auto selected = complex(value);
    if (!selected) { return result; }
    const auto content = expand_complex_type(bundle_, *selected->owner, *selected->node);
    std::set<const xml::element*> active{};
    for (const auto& child : content.children) { collect({child.owner, child.node}, attributes, result, active, 0U); }
    return result;
  }
  // Advance exact expanded child steps, rejecting a statically unresolved selector/field.
  std::vector<component> path(std::vector<component> current, const xml::identity_path& projection) {
    for (const auto& step : projection.steps) {
      std::vector<component> next{};
      for (const auto value : current) {
        for (auto child : members(value, false)) {
          work(step.local.size() + step.namespace_uri.size() + 1U);
          if (instance_name(child, false) != step) { continue; }
          child = resolve_reference(child, false);
          if (std::find(next.begin(), next.end(), child) == next.end()) { next.push_back(child); }
        }
      }
      current = std::move(next);
    }
    return current;
  }
  // Traverse a finite declaration/type graph once for optional descendant selector prefixes.
  std::vector<component> selector(component root, const xml::identity_path& projection) {
    if (!projection.descendants) { return path({root}, projection); }
    std::vector<component> pending{root}, contexts{};
    std::set<const xml::element*> visited{};
    while (!pending.empty()) {
      work();
      const auto value = pending.back();
      pending.pop_back();
      if (!visited.insert(value.node).second) { continue; }
      if (contexts.size() >= limits_.max_selected_nodes) { throw std::length_error{"XSD identity descendant declaration limit exceeded"}; }
      contexts.push_back(value);
      for (const auto child : members(value, false)) { pending.push_back(resolve_reference(child, false)); }
    }
    auto result = path(std::move(contexts), projection);
    return result;
  }
  // Resolve scalar simpleContent and primitive/derived simple types with their actual value spaces.
  std::string scalar_kind(component value, std::size_t depth = 0U) {
    work();
    if (depth > limits_.max_depth) { throw std::length_error{"XSD identity scalar type depth exceeded"}; }
    if (const auto selected = complex(value)) {
      for (const auto& content : selected->node->children) {
        if (content.qualified_name.local != "simpleContent") { continue; }
        for (const auto& derivation : content.children) {
          if (derivation.qualified_name.local != "extension" && derivation.qualified_name.local != "restriction") { continue; }
          const auto base = xml::resolve_qname(attribute(derivation, "base"), derivation);
          if (base.namespace_uri != xsd_namespace) {
            if (const auto complex_base = global("complexType", base)) { return scalar_kind(*complex_base, depth + 1U); }
          }
          xml::element projected{};
          projected.qualified_name = {"element", std::string{xsd_namespace}};
          projected.namespace_bindings = derivation.namespace_bindings;
          projected.attributes.push_back({{"type", {}}, std::string{attribute(derivation, "base")}});
          const auto resolved = resolve_scalar_type(bundle_, projected);
          if (!resolved.facets.list_item_kind.empty() || !resolved.facets.union_members.empty()) {
            throw std::invalid_argument{"XSD identity fields do not support list/union value spaces"};
          }
          return resolved.facets.lexical_kind.empty() ? resolved.primitive.local : resolved.facets.lexical_kind;
        }
      }
      throw std::invalid_argument{"XSD identity field selects complex element content"};
    }
    const auto resolved = resolve_scalar_type(bundle_, *value.node);
    if (!resolved.facets.list_item_kind.empty() || !resolved.facets.union_members.empty()) {
      throw std::invalid_argument{"XSD identity fields do not support list/union value spaces"};
    }
    return resolved.facets.lexical_kind.empty() ? resolved.primitive.local : resolved.facets.lexical_kind;
  }
  // Require one unambiguous built-in field contract across every selected static declaration.
  std::pair<std::string, bool> field_kind(const std::vector<component>& selected, const xml::identity_path& projection) {
    auto values = path(selected, projection);
    if (projection.attribute) {
      std::vector<component> attributes{};
      for (const auto value : values) {
        for (auto field : members(value, true)) {
          if (instance_name(field, true) == *projection.attribute) {
            reject_augmented_identity_field(*field.node);
            field = resolve_reference(field, true);
            if (std::find(attributes.begin(), attributes.end(), field) == attributes.end()) { attributes.push_back(field); }
          }
        }
      }
      values = std::move(attributes);
    }
    if (values.empty()) { throw std::invalid_argument{"XSD identity field cannot be resolved to a declared scalar"}; }
    std::optional<std::string> kind{};
    std::optional<bool> nillable{};
    for (const auto value : values) {
      reject_augmented_identity_field(*value.node);
      auto candidate = scalar_kind(value);
      if (kind && *kind != candidate) { throw std::invalid_argument{"XSD identity field has ambiguous declared scalar types"}; }
      const auto marker = projection.attribute ? std::string_view{} : attribute(*value.node, "nillable");
      const bool candidate_nillable = marker == "true" || marker == "1";
      if (nillable && *nillable != candidate_nillable) {
        throw std::invalid_argument{"XSD identity field has ambiguous declared element nillability"};
      }
      kind = std::move(candidate);
      nillable = candidate_nillable;
    }
    return {*kind, *nillable};
  }
public:
  // Index pinned declarations once; each document retains its own namespace and form defaults.
  identity_binder(const schema_bundle& bundle, const xml::identity_limits& limits)
      : bundle_{bundle}, limits_{limits}, remaining_work_{limits.max_work_units} {
    for (const auto& document : bundle.documents) {
      const auto scan = [&](auto&& self, const xml::element& node, std::size_t depth) -> void {
        work();
        if (depth > limits.max_depth) { throw std::length_error{"XSD identity schema depth exceeded"}; }
        owners_.emplace(&node, &document);
        for (const auto& child : node.children) { self(self, child, depth + 1U); }
      };
      scan(scan, document.root, 0U);
      for (const auto& node : document.root.children) {
        const auto name = attribute(node, "name");
        if (name.empty()) { continue; }
        if (!globals_.emplace(std::tuple{node.qualified_name.local, document.target, std::string{name}},
            component{&document, &node}).second) { throw std::invalid_argument{"Ambiguous global XSD identity binding declaration"}; }
      }
    }
  }
  // Compile and type every field before checking local-scope keyref targets.
  xml::identity_scope bind(const xml::element& declaration, std::string_view target_namespace) {
    const auto found = owners_.find(&declaration);
    if (found == owners_.end()) { throw std::invalid_argument{"XSD identity declaration is not owned by this bundle"}; }
    xml::identity_scope result{};
    result.owner = {std::string{attribute(declaration, "name")}, std::string{target_namespace}};
    for (const auto& child : declaration.children) {
      const auto& kind = child.qualified_name.local;
      if (kind != "key" && kind != "unique" && kind != "keyref") { continue; }
      if (result.constraints.size() >= limits_.max_constraints) { throw std::length_error{"XSD identity constraint-count limit exceeded"}; }
      xml::identity_constraint constraint{};
      constraint.id = {std::string{attribute(child, "name")}, std::string{target_namespace}};
      constraint.kind = kind == "key" ? xml::identity_kind::key : kind == "unique" ?
          xml::identity_kind::unique : xml::identity_kind::keyref;
      if (kind == "keyref") { constraint.refer = xml::resolve_qname(attribute(child, "refer"), child); }
      const xml::element* selector_node{};
      std::vector<const xml::element*> fields{};
      for (const auto& item : child.children) {
        if (item.qualified_name.local == "selector") {
          if (selector_node) { throw std::invalid_argument{"XSD identity requires exactly one selector"}; }
          selector_node = &item;
        } else if (item.qualified_name.local == "field") { fields.push_back(&item); }
        else if (item.qualified_name.local != "annotation") { throw std::invalid_argument{"Unsupported XSD identity declaration child"}; }
      }
      if (!selector_node || fields.empty()) { throw std::invalid_argument{"XSD identity requires a selector and at least one field"}; }
      if (fields.size() > limits_.max_fields) { throw std::length_error{"XSD identity field-count limit exceeded"}; }
      constraint.selector = xml::compile_identity_path(attribute(*selector_node, "xpath"), *selector_node, true, limits_);
      const auto selected = selector({found->second, &declaration}, constraint.selector);
      if (selected.empty()) { throw std::invalid_argument{"XSD identity selector has no statically declared target"}; }
      for (const auto* field : fields) {
        auto projection = xml::compile_identity_path(attribute(*field, "xpath"), *field, false, limits_);
        const auto [type, nillable] = field_kind(selected, projection);
        constraint.fields.push_back({std::move(projection), type, nillable});
      }
      result.constraints.push_back(std::move(constraint));
    }
    xml::validate_identity_scope_contract(result, limits_);
    return result;
  }
};
} // namespace

// Keep bound metadata independent of the bundle's pointers and namespace-prefix spellings.
xml::identity_scope bind_identity_constraints(const schema_bundle& bundle,
    const xml::element& declaration, std::string_view target_namespace, const xml::identity_limits& limits) {
  return identity_binder{bundle, limits}.bind(declaration, target_namespace);
}
} // namespace rohit::serializer::writer::xml_binding
