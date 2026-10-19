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
};
struct native_identity_node {
  std::optional<xml::identity_scope> scope{};
  std::vector<native_identity_edge> edges{};
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
    for (const auto& field : contract.fields) {
      if (field.attribute || field.text) { continue; }
      if (field.map_binding) {
        const auto& map = *field.map_binding;
        const auto outer = wrapper(), entry = wrapper();
        result[index].edges.push_back({map.wrapper, {}, outer});
        result[outer].edges.push_back({map.entry, {}, entry});
        if (map.key_placement == xml_binding::map_key_placement::element) {
          const auto key = target(map.key_type); result[entry].edges.push_back({map.key, {}, key});
        }
        const auto value = target(map.value_type); result[entry].edges.push_back({map.value, {}, value});
      } else if (field.variant_binding) {
        const auto& variant = *field.variant_binding;
        const auto outer = wrapper(); result[index].edges.push_back({variant.wrapper, {}, outer});
        for (const auto& alternative : variant.alternatives) {
          const auto selected = target(alternative.native_type);
          result[outer].edges.push_back({alternative.tag,
              variant.selection == xml_binding::variant_selection::xsi_type ?
                  std::optional<xml::name>{alternative.type_identity} : std::nullopt, selected});
        }
      } else {
        const auto selected = target(field.source->type_name_list.front());
        if (field.wrapper) {
          const auto outer = wrapper(); result[index].edges.push_back({*field.wrapper, {}, outer});
          result[outer].edges.push_back({field.qualified_name, {}, selected});
        } else { result[index].edges.push_back({field.qualified_name, {}, selected}); }
      }
    }
    if (contract.mixed) {
      for (const auto& child : contract.mixed->children) {
        const auto selected = target(*child.value_type);
        result[index].edges.push_back({child.qualified_name, {}, selected});
      }
    }
    return index;
  };
  record(root, &root_contract);
  return result;
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
