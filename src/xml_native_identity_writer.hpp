// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <rohit/xml_identity.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::xml_codegen {
struct native_identity_edge {
  xml::name name{};
  std::optional<xml::name> type{};
  std::size_t target{std::numeric_limits<std::size_t>::max()};
  std::string member{};
};
struct native_identity_scalar {
  xml::name name{};
  std::string member{};
  bool attribute{};
  bool text{};
};
struct native_identity_node {
  std::optional<xml::identity_scope> scope{};
  std::vector<native_identity_edge> edges{};
  std::string owner{};
  std::vector<native_identity_scalar> scalars{};
};

// Compile only active typed transitions; recursive records use indexes rather than expanded document paths.
inline std::vector<native_identity_node> native_identity_graph(const class_node& root,
    const xml_binding::record& root_contract, const xml_binding::schema& bindings) {
  std::vector<native_identity_node> result;
  std::map<const class_node*, std::size_t> records;
  std::function<std::size_t(const class_node&, const xml_binding::record*)> record;
  // Scalar/reference-projected leaves have no nested owner scope; record references retain their target contract.
  const auto target = [&](const type_name& original) {
    const auto* leaf = &original;
    while (leaf->is_optional() || leaf->is_sequence()) { leaf = &leaf->arguments.front(); }
    if (leaf->type != object_type::class_type) { return std::numeric_limits<std::size_t>::max(); }
    const auto* owner = static_cast<const class_node*>(leaf->resolved_node);
    if (owner->reference) {
      if (owner->reference->value_type.type != object_type::class_type) { return std::numeric_limits<std::size_t>::max(); }
      owner = static_cast<const class_node*>(owner->reference->value_type.resolved_node);
    }
    return record(*owner, nullptr);
  };
  // Append a finite synthetic wrapper or entry; no runtime object or payload storage is introduced.
  const auto wrapper = [&] {
    const auto index = result.size(); result.emplace_back(); return index;
  };
  record = [&](const class_node& owner, const xml_binding::record* supplied) {
    if (const auto found = records.find(&owner); found != records.end()) { return found->second; }
    const auto index = result.size(); records.emplace(&owner, index); result.emplace_back();
    const auto contract = supplied ? *supplied : bindings.bind(owner);
    result[index].scope = contract.identity;
    result[index].owner = owner.get_full_name();
    for (const auto& field : contract.fields) {
      const auto member = field.source->name;
      if (field.attribute || field.text) {
        result[index].scalars.push_back({field.qualified_name, member, field.attribute, field.text});
        continue;
      }
      if (field.map_binding) {
        const auto& map = *field.map_binding;
        const auto outer = wrapper(), entry = wrapper();
        result[index].edges.push_back({map.wrapper, {}, outer, member});
        result[outer].edges.push_back({map.entry, {}, entry});
        if (map.key_placement == xml_binding::map_key_placement::element) {
          const auto key = target(map.key_type); result[entry].edges.push_back({map.key, {}, key, "key"});
        }
        const auto value = target(map.value_type); result[entry].edges.push_back({map.value, {}, value, "value"});
      } else if (field.variant_binding) {
        const auto& variant = *field.variant_binding;
        const auto outer = wrapper(); result[index].edges.push_back({variant.wrapper, {}, outer, member});
        for (const auto& alternative : variant.alternatives) {
          const auto selected = target(alternative.native_type);
          result[outer].edges.push_back({alternative.tag,
              variant.selection == xml_binding::variant_selection::xsi_type ?
                  std::optional<xml::name>{alternative.type_identity} : std::nullopt, selected});
        }
      } else {
        const auto selected = target(field.source->type_name_list.front());
        if (field.wrapper) {
          const auto outer = wrapper(); result[index].edges.push_back({*field.wrapper, {}, outer, member});
          result[outer].edges.push_back({field.qualified_name, {}, selected});
        } else { result[index].edges.push_back({field.qualified_name, {}, selected, member}); }
      }
    }
    if (contract.mixed) {
      // Mixed text belongs to the native content member even during token lookahead.
      result[index].scalars.push_back({{}, contract.mixed->source->name, false, true});
      for (const auto& child : contract.mixed->children) {
        const auto selected = target(*child.value_type);
        result[index].edges.push_back({child.qualified_name, {}, selected,
            contract.mixed->source->name + '.' + child.branch});
      }
    }
    return index;
  };
  record(root, &root_contract);
  return result;
}

struct native_identity_projection {
  std::size_t node{};
  std::string path{};
};

// Extend a static native member path without inventing segments for XML-only wrappers.
inline std::string native_identity_member_path(const std::string& parent, const std::string& member) {
  return member.empty() ? parent : parent.empty() ? member : parent + '.' + member;
}

// XML-only wrappers retain the enclosing native field instead of exposing unqualified synthetic labels.
inline std::string native_identity_diagnostic_member(const native_identity_node& node,
    const std::string& member) {
  return node.owner.empty() ? std::string{} : native_identity_member_path(node.owner, member);
}

// Resolve finite prepared XML selectors to native paths; recursive types are visited once per search.
inline std::vector<native_identity_projection> native_identity_projections(
    const std::vector<native_identity_node>& graph, std::size_t owner,
    const xml::identity_path& selector, const std::string& prefix) {
  std::vector<native_identity_projection> current{{owner, prefix}};
  if (selector.descendants) {
    std::vector<bool> visited(graph.size());
    if (owner < graph.size()) { visited[owner] = true; }
    for (std::size_t index{}; index < current.size(); ++index) {
      const auto value = current[index];
      if (value.node >= graph.size()) { continue; }
      for (const auto& edge : graph[value.node].edges) {
        if (edge.target >= graph.size() || visited[edge.target]) { continue; }
        visited[edge.target] = true;
        current.push_back({edge.target, native_identity_member_path(value.path, edge.member)});
      }
    }
  }
  for (const auto& step : selector.steps) {
    std::vector<native_identity_projection> next;
    for (const auto& value : current) {
      if (value.node >= graph.size()) { continue; }
      for (const auto& edge : graph[value.node].edges) {
        if (edge.name == step) {
          next.push_back({edge.target, native_identity_member_path(value.path, edge.member)});
        }
      }
    }
    current = std::move(next);
  }
  return current;
}

// Keep diagnostic identity fields in native spelling even when XSD names differ from schema members.
inline std::string native_identity_field_path(const std::vector<native_identity_node>& graph,
    std::size_t owner, const xml::identity_constraint& constraint, const xml::identity_field& field) {
  const auto selected = native_identity_projections(graph, owner, constraint.selector, graph[owner].owner);
  for (const auto& source : selected) {
    for (const auto& value : native_identity_projections(graph, source.node, field.path, source.path)) {
      if (value.node >= graph.size()) {
        if (!field.path.attribute) { return value.path; }
        continue;
      }
      for (const auto& scalar : graph[value.node].scalars) {
        if ((field.path.attribute && scalar.attribute && scalar.name == *field.path.attribute) ||
            (!field.path.attribute && scalar.text)) {
          return native_identity_member_path(value.path, scalar.member);
        }
      }
      if (!field.path.attribute) { return value.path; }
    }
  }
  // An ambiguous or XML-only selector still identifies its exact native constraint owner.
  return native_identity_member_path(graph[owner].owner, constraint.id.local);
}

// Emit additive Rust diagnostic tables without changing existing identity metadata struct layouts.
inline std::string rust_identity_diagnostics(const std::vector<native_identity_node>& graph,
    const std::function<std::string(std::string_view)>& quote) {
  std::string result{"&["};
  for (std::size_t index{}; index < graph.size(); ++index) {
    const auto& node = graph[index];
    result += "SrlXmlIdentityDiagnosticNode{owner:" + quote(node.owner) + ",constraints:&[";
    if (node.scope) {
      for (const auto& constraint : node.scope->constraints) {
        result += "SrlXmlIdentityDiagnosticConstraint{name:" +
            quote('{' + constraint.id.namespace_uri + '}' + constraint.id.local) + ",fields:&[";
        for (const auto& field : constraint.fields) {
          result += quote(native_identity_field_path(graph, index, constraint, field)) + ',';
        }
        result += "]},";
      }
    }
    result += "],edges:&[";
    for (const auto& edge : node.edges) {
      result += quote(native_identity_diagnostic_member(node, edge.member)) + ',';
    }
    result += "],scalars:&[";
    for (const auto& scalar : node.scalars) {
      result += "SrlXmlIdentityDiagnosticScalar{uri:" + quote(scalar.name.namespace_uri) +
          ",local:" + quote(scalar.name.local) + ",member:" +
          quote(native_identity_diagnostic_member(node, scalar.member)) +
          ",attribute:" + (scalar.attribute ? "true" : "false") +
          ",text:" + (scalar.text ? "true" : "false") + "},";
    }
    result += "]},";
  }
  return result + ']';
}

// Emit separate C diagnostic arrays, preserving positional initializers of existing identity tables.
inline std::string c_identity_diagnostic_declaration(const std::string& identifier,
    const std::vector<native_identity_node>& graph,
    const std::function<std::string(std::string_view)>& quote) {
  std::string result;
  std::string nodes;
  for (std::size_t index{}; index < graph.size(); ++index) {
    const auto& node = graph[index];
    const auto prefix = identifier + "_diagnostic_" + std::to_string(index);
    std::string constraints;
    if (node.scope) {
      for (std::size_t item{}; item < node.scope->constraints.size(); ++item) {
        const auto& constraint = node.scope->constraints[item];
        const auto fields = prefix + "_fields_" + std::to_string(item);
        result += "static const char* const " + fields + "[]={";
        for (const auto& field : constraint.fields) {
          result += quote(native_identity_field_path(graph, index, constraint, field)) + ',';
        }
        result += "};\n";
        constraints += '{' + quote('{' + constraint.id.namespace_uri + '}' + constraint.id.local) +
            ',' + fields + ',' + std::to_string(constraint.fields.size()) + "u},";
      }
    }
    if (!constraints.empty()) {
      result += "static const srl_xml_identity_diagnostic_constraint " + prefix + "_constraints[]={" + constraints + "};\n";
    }
    if (!node.edges.empty()) {
      result += "static const char* const " + prefix + "_edges[]={";
      for (const auto& edge : node.edges) {
        result += quote(native_identity_diagnostic_member(node, edge.member)) + ',';
      }
      result += "};\n";
    }
    if (!node.scalars.empty()) {
      result += "static const srl_xml_identity_diagnostic_scalar " + prefix + "_scalars[]={";
      for (const auto& scalar : node.scalars) {
        result += '{' + quote(scalar.name.namespace_uri) + ',' + quote(scalar.name.local) + ',' +
            quote(native_identity_diagnostic_member(node, scalar.member)) + ',' +
            (scalar.attribute ? "true" : "false") + ',' +
            (scalar.text ? "true" : "false") + "},";
      }
      result += "};\n";
    }
    nodes += '{' + quote(node.owner) + ',' + (constraints.empty() ? "NULL" : prefix + "_constraints") +
        ',' + (node.edges.empty() ? "NULL" : prefix + "_edges") + ',' +
        (node.scalars.empty() ? "NULL" : prefix + "_scalars") + ',' +
        std::to_string(node.scalars.size()) + "u},";
  }
  return result + "static const srl_xml_identity_diagnostic_node " + identifier + "_diagnostics[]={" + nodes + "};\n";
}

// Derive all native policy defaults from the immutable shared identity contract.
inline std::string native_identity_constants(bool rust) {
  const xml::identity_limits limits{};
  std::string result;
  for (const auto& [name, value] : {
      std::pair{"constraints", limits.max_constraints}, {"fields", limits.max_fields},
      {"depth", limits.max_depth}, {"selected", limits.max_selected_nodes}, {"tuples", limits.max_tuples},
      {"bytes", limits.max_tuple_bytes}, {"work", limits.max_work_units}}) {
    std::string identifier = name;
    if (rust) {
      std::transform(identifier.begin(), identifier.end(), identifier.begin(), [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
      result += "const SRL_XML_IDENTITY_" + identifier + ":usize=" + std::to_string(value) + ";\n";
    } else { result += "static const size_t srl_xml_identity_" + identifier + '=' + std::to_string(value) + "u;\n"; }
  }
  return result;
}

// Lower prepared names, owner-local target indexes and graph edges to immutable native Rust metadata.
inline std::string rust_identity_graph(const std::vector<native_identity_node>& graph,
    const std::function<std::string(std::string_view)>& quote) {
  const auto name = [&](const xml::name& value) { return "SrlXmlIdentityName{uri:" + quote(value.namespace_uri) + ",local:" + quote(value.local) + '}'; };
  const auto path = [&](const xml::identity_path& value) {
    std::string result = "SrlXmlIdentityPath{steps:&[";
    for (const auto& step : value.steps) { result += name(step) + ','; }
    return result + "],descendants:" + (value.descendants ? "true" : "false") +
        ",attribute:" + (value.attribute ? "Some(" + name(*value.attribute) + ")" : "None") + '}';
  };
  std::string result = "&[";
  for (const auto& node : graph) {
    result += "SrlXmlIdentityNode{scope:";
    if (node.scope) {
      result += "Some(&SrlXmlIdentityScope{constraints:&[";
      for (const auto& constraint : node.scope->constraints) {
        result += "SrlXmlIdentityConstraint{kind:" + std::to_string(constraint.kind == xml::identity_kind::key ? 0U : constraint.kind == xml::identity_kind::unique ? 1U : 2U) +
            ",selector:" + path(constraint.selector) + ",fields:&[";
        for (const auto& field : constraint.fields) {
          result += "SrlXmlIdentityField{path:" + path(field.path) + ",kind:" + quote(field.lexical_kind) +
              ",nillable:" + (field.nillable ? "true" : "false") + "},";
        }
        result += "],refer:";
        if (constraint.refer) {
          const auto target = std::find_if(node.scope->constraints.begin(), node.scope->constraints.end(),
              [&](const auto& value) { return value.id == *constraint.refer; });
          if (target == node.scope->constraints.end()) { throw std::invalid_argument{"XML keyref target outside owner scope"}; }
          result += std::to_string(std::distance(node.scope->constraints.begin(), target));
        } else { result += "usize::MAX"; }
        result += "},";
      }
      result += "]})";
    } else { result += "None"; }
    result += ",edges:&[";
    for (const auto& edge : node.edges) {
      result += "SrlXmlIdentityEdge{name:" + name(edge.name) + ",type_name:" +
          (edge.type ? "Some(" + name(*edge.type) + ")" : "None") + ",target:" +
          (edge.target == std::numeric_limits<std::size_t>::max() ? "usize::MAX" : std::to_string(edge.target)) + "},";
    }
    result += "]},";
  }
  return result + ']';
}

// Emit immutable C11 tables in dependency order, keeping recursive owner transitions as checked indexes.
inline std::string c_identity_declaration(const std::string& identifier,
    const std::vector<native_identity_node>& graph,
    const std::function<std::string(std::string_view)>& quote) {
  std::string declarations;
  const auto name = [&](const xml::name& value) { return '{' + quote(value.namespace_uri) + ',' + quote(value.local) + '}'; };
  // XPath syntax has already been checked; only prepared expanded steps reach the target runtime.
  const auto path = [&](const std::string& prefix, const xml::identity_path& value) {
    if (!value.steps.empty()) {
      declarations += "static const srl_xml_identity_name " + prefix + "_steps[]={";
      for (const auto& step : value.steps) { declarations += name(step) + ','; }
      declarations += "};\n";
    }
    if (value.attribute) { declarations += "static const srl_xml_identity_name " + prefix + "_attribute=" + name(*value.attribute) + ";\n"; }
    return '{' + (value.steps.empty() ? std::string{"NULL"} : prefix + "_steps") + ',' +
        std::to_string(value.steps.size()) + "u," + (value.descendants ? "true" : "false") + ',' +
        (value.attribute ? '&' + prefix + "_attribute" : "NULL") + '}';
  };
  std::string nodes;
  for (std::size_t node_index{}; node_index < graph.size(); ++node_index) {
    const auto& node = graph[node_index];
    const auto prefix = identifier + '_' + std::to_string(node_index);
    if (node.scope) {
      std::string constraints;
      for (std::size_t constraint_index{}; constraint_index < node.scope->constraints.size(); ++constraint_index) {
        const auto& constraint = node.scope->constraints[constraint_index];
        const auto stem = prefix + '_' + std::to_string(constraint_index);
        const auto selector = path(stem + "_selector", constraint.selector);
        std::string fields;
        for (std::size_t field_index{}; field_index < constraint.fields.size(); ++field_index) {
          const auto& field = constraint.fields[field_index];
          fields += '{' + path(stem + "_field_" + std::to_string(field_index), field.path) + ',' +
              quote(field.lexical_kind) + ',' + (field.nillable ? "true" : "false") + "},";
        }
        declarations += "static const srl_xml_identity_field " + stem + "_fields[]={" + fields + "};\n";
        std::string target = "SIZE_MAX";
        if (constraint.refer) {
          const auto found = std::find_if(node.scope->constraints.begin(), node.scope->constraints.end(),
              [&](const auto& value) { return value.id == *constraint.refer; });
          if (found == node.scope->constraints.end()) { throw std::invalid_argument{"XML keyref target outside owner scope"}; }
          target = std::to_string(std::distance(node.scope->constraints.begin(), found)) + 'u';
        }
        constraints += '{' + std::to_string(constraint.kind == xml::identity_kind::key ? 0U : constraint.kind == xml::identity_kind::unique ? 1U : 2U) +
            "u," + selector + ',' + stem + "_fields," + std::to_string(constraint.fields.size()) + "u," + target + "},";
      }
      declarations += "static const srl_xml_identity_constraint " + prefix + "_constraints[]={" + constraints + "};\n";
      declarations += "static const srl_xml_identity_scope " + prefix + "_scope={" + prefix + "_constraints," +
          std::to_string(node.scope->constraints.size()) + "u};\n";
    }
    std::string edges;
    for (std::size_t edge_index{}; edge_index < node.edges.size(); ++edge_index) {
      const auto& edge = node.edges[edge_index];
      const auto stem = prefix + "_edge_" + std::to_string(edge_index);
      if (edge.type) { declarations += "static const srl_xml_identity_name " + stem + "_type=" + name(*edge.type) + ";\n"; }
      edges += '{' + name(edge.name) + ',' + (edge.type ? '&' + stem + "_type" : "NULL") + ',' +
          (edge.target == std::numeric_limits<std::size_t>::max() ? "SIZE_MAX" : std::to_string(edge.target) + 'u') + "},";
    }
    if (!node.edges.empty()) { declarations += "static const srl_xml_identity_edge " + prefix + "_edges[]={" + edges + "};\n"; }
    nodes += '{' + (node.scope ? '&' + prefix + "_scope" : "NULL") + ',' +
        (node.edges.empty() ? "NULL" : prefix + "_edges") + ',' + std::to_string(node.edges.size()) + "u},";
  }
  return declarations + "static const srl_xml_identity_node " + identifier + "[]={" + nodes + "};\n";
}
} // namespace rohit::serializer::writer::xml_codegen
