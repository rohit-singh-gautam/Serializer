// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_native_extension_writer.hpp"
#include "xml_portable_projection_writer.hpp"
#include "xml_portable_writer.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Retain lexical aliases when restrictions observe their spelling rather than only expanded names.
inline bool namespace_lexical_facets(const xml_binding::scalar_facets& facets) {
  return !facets.patterns.empty() || !facets.pattern_groups.empty() ||
      std::any_of(facets.union_members.begin(), facets.union_members.end(),
          [](const auto& member) { return namespace_lexical_facets(member); }) ||
      std::any_of(facets.list_item_facets.begin(), facets.list_item_facets.end(),
          [](const auto& item) { return namespace_lexical_facets(item); });
}

// Select owner values whose union branch or lexical QName spelling observes the default vocabulary.
inline bool namespace_owner_union(const xml_binding::field& field) {
  return (field.attribute || field.text) && xml_portable::namespace_dependent(field) &&
      (!field.facets.union_members.empty() || !field.facets.list_item_facets.empty() || (!field.prefix_list &&
          !xml_portable::typed_list(field) && namespace_lexical_facets(field.facets)));
}

// Prepare an owner's namespace before any attribute or text can depend on that default.
inline std::string managed_namespace_owner_default(const xml_binding::record& contract,
    const xml_portable::field_name& access, const xml_portable::scalar_codec& scalar,
    const xml_portable::field_name& rule, std::string_view language,
    const xml_portable::field_name& active = {}) {
  const bool java = language == "java";
  std::string result = java ? "String srlOwnerDefault=null;\n" : "string? srlOwnerDefault=null;\n";
  for (std::size_t index{}; index < contract.fields.size(); ++index) {
    const auto& field = contract.fields[index];
    if (!namespace_owner_union(field)) { continue; }
    std::string selected = access(*field.source), condition{};
    const auto* type = &field.source->type_name_list.front();
    if (type->is_optional()) {
      condition = selected + (java ? ".isPresent()" : ".HasValue");
      selected += java ? ".get()" : ".Value.Item1";
      type = &type->arguments.front();
    }
    if (active) {
      const auto enabled = active(*field.source);
      if (!enabled.empty()) { condition = condition.empty() ? enabled : '(' + condition + ")&&(" + enabled + ')'; }
    }
    result += condition.empty() ? "{\n" : "if(" + condition + "){\n";
    result += "var srlPreferredDefault=srlXmlNamespaces." + std::string{java ? "preferredDefault" : "PreferredDefault"} +
        "(output," + std::to_string(index) + ',' + scalar(*type, selected, true) + ',' + rule(*field.source) + ");\n";
    result += "if(srlPreferredDefault!=null){if(srlOwnerDefault!=null&&" +
        std::string{java ? "!srlOwnerDefault.equals(srlPreferredDefault)" : "srlOwnerDefault!=srlPreferredDefault"} +
        ")throw new " + (java ? "IllegalArgumentException" : "System.FormatException") +
        "(\"Conflicting XML union owner defaults\");srlOwnerDefault=srlPreferredDefault;}\n}\n";
  }
  return result;
}

// Select graphs whose namespace-sensitive union or lexical constraints need alias reservation.
inline bool namespace_union_graph(const class_node& source, const xml_binding::schema& bindings) {
  std::set<const class_node*> visited{};
  const auto visit = [&](auto&& self, const class_node& owner) -> bool {
    if (!visited.insert(&owner).second) { return false; }
    if (owner.reference) {
      const auto& payload = xml_portable::projection_payload(owner.reference->value_type);
      return payload.type == object_type::class_type && payload.resolved_node &&
          self(self, static_cast<const class_node&>(*payload.resolved_node));
    }
    const auto contract = bindings.bind(owner);
    for (const auto& field : contract.fields) {
      if (xml_portable::namespace_dependent(field) && (!field.facets.union_members.empty() ||
          namespace_lexical_facets(field.facets) ||
          (field.prefix_list && !field.facets.enumeration.empty()))) { return true; }
      const auto child_type = [&](const type_name& type) {
        const auto& payload = xml_portable::projection_payload(type);
        return payload.type == object_type::class_type && payload.resolved_node &&
            self(self, static_cast<const class_node&>(*payload.resolved_node));
      };
      if (field.map_binding && child_type(field.map_binding->value_type)) { return true; }
      if (field.variant_binding) {
        for (const auto& branch : field.variant_binding->alternatives) {
          if (child_type(branch.native_type)) { return true; }
        }
      }
      if (const auto* child = native_extension_child(field); child && self(self, *child)) { return true; }
    }
    if (contract.mixed) {
      for (const auto& child : contract.mixed->children) {
        if (child.value_type->resolved_node && self(self,
            static_cast<const class_node&>(*child.value_type->resolved_node))) { return true; }
      }
    }
    return false;
  };
  return visit(visit, source);
}

// Borrow only mapped XML fields while reserving aliases across nested owner/container graphs.
inline std::string namespace_reservation_body(const xml_binding::record& contract,
    const xml_portable::field_name& access, const xml_portable::field_name& field_name,
    const xml_portable::alternative_name& alternative, std::string_view language,
    const xml_portable::field_name& active = {}) {
  const bool go = language == "go", java = language == "java";
  const auto call = [&](const std::string& expression, bool scalar) {
    const auto method = scalar ? (go || java ? "reserveNamespaceText" : "ReserveNamespaceText") :
        (go ? "srlXmlEncode" : java ? "xmlWrite" : "XmlWrite");
    const auto target = scalar ? "output." + std::string{method} + '(' + expression + ')' :
        expression + '.' + method + (go ? "(output,srlxml.Name{})" : "(output,depth+1)");
    return go ? "if err:=" + target + ";err!=nil{return err}\n" :
        (scalar ? std::string{} : "if(" + expression + "==null)throw new " +
            (java ? "IllegalArgumentException" : "System.FormatException") + "(\"Null XML record\");\n") + target + ";\n";
  };
  std::size_t next_item{};
  const auto traverse = [&](auto&& self, const type_name& type, const std::string& expression,
                            bool namespaced, bool legacy_array = false) -> std::string {
    if (legacy_array || type.is_sequence()) {
      const auto item = "srlNamespaceItem" + std::to_string(next_item++);
      const auto& element = legacy_array ? type : type.arguments.front();
      const auto body = self(self, element, item, namespaced);
      if (body.empty()) { return {}; }
      return std::string{go ? "for _," + item + ":=range " :
          java ? "for(var " + item + ':' : "foreach(var " + item + " in "} + expression +
          (go ? "{\n" : "){\n") + body + "}\n";
    }
    if (type.is_optional()) {
      const auto selected = go ? "(*" + expression + ')' : expression + (java ? ".get()" : ".Value.Item1");
      const auto body = self(self, type.arguments.front(), selected, namespaced);
      if (body.empty()) { return {}; }
      return "if" + std::string{go ? " " + expression + "!=nil{\n" :
          '(' + expression + (java ? ".isPresent()" : ".HasValue") + "){\n"} + body + "}\n";
    }
    if (type.is_digest()) { return {}; }
    if (xml_portable::value_projection(type)) {
      const auto& payload = static_cast<const class_node&>(*type.resolved_node).reference->value_type;
      if (go) {
        const auto selected = "srlNamespaceSelected" + std::to_string(next_item++);
        const auto body = self(self, payload, selected, namespaced);
        if (body.empty()) { return {}; }
        return "{\n" + selected + ",srlNamespaceErr:=" + expression +
            ".ReadValue();if srlNamespaceErr!=nil{return srlNamespaceErr}\n" + body + "}\n";
      }
      return self(self, payload, expression + (java ? ".readValue()" : ".ReadValue()"), namespaced);
    }
    if (type.type == object_type::class_type) { return call(expression, false); }
    return namespaced && type.name == "string" ? call(expression, true) : std::string{};
  };
  std::string result{};
  for (const auto& field : contract.fields) {
    const auto before = result.size();
    const auto selected = access(*field.source);
    if (field.map_binding) {
      const auto& map = *field.map_binding;
      const auto item = "srlNamespaceItem" + std::to_string(next_item++);
      const auto body = traverse(traverse, map.value_type, item, false);
      if (body.empty()) { continue; }
      const auto container = map.optional_container ?
          (go ? "(*" + selected + ')' : selected + (java ? ".get()" : ".Value.Item1")) : selected;
      if (map.optional_container) {
        result += "if" + std::string{go ? " " + selected + "!=nil{\n" :
            '(' + selected + (java ? ".isPresent()" : ".HasValue") + "){\n"};
      }
      result += go ? "for _," + item + ":=range " + container + "{\n" :
          std::string{java ? "for(var " : "foreach(var "} + item + (java ? ":" : " in ") +
              container + (java ? ".values()" : ".Values") + "){\n";
      result += body + "}\n";
      if (map.optional_container) { result += "}\n"; }
    } else if (field.variant_binding) {
      result += std::string{go ? "switch " : "switch("} + selected + "Index" + (go ? "{\n" : "){\n");
      for (std::size_t index{}; index < field.variant_binding->alternatives.size(); ++index) {
        const auto& branch = field.variant_binding->alternatives[index];
        result += "case " + std::to_string(index) + ":{\n" +
            traverse(traverse, branch.native_type, selected + alternative(branch.alias), false) +
            (go ? "}\n" : "break;}\n");
      }
      result += "}\n";
    } else {
      result += traverse(traverse, field.source->type_name_list.front(), selected,
          xml_portable::namespace_dependent(field), field.source->modifier == member::modifier_type::array);
    }
    if (active && result.size() != before) {
      const auto condition = active(*field.source);
      if (!condition.empty()) {
        result.insert(before, go ? "if " + condition + "{\n" : "if(" + condition + "){\n");
        result += "}\n";
      }
    }
  }
  if (contract.mixed) {
    const auto before = result.size();
    const auto& mixed = *contract.mixed;
    const auto item = "srlNamespaceFragment";
    result += go ? "for _," + std::string{item} + ":=range " + access(*mixed.source) + "{\n" :
        std::string{java ? "for(var " : "foreach(var "} + item + (java ? ":" : " in ") + access(*mixed.source) + "){\n";
    result += go ? "if " + std::string{item} + "==nil{return fmt.Errorf(\"Nil XML mixed fragment\")}\n" :
        "if(" + std::string{item} + "==null)throw new " + (java ? "IllegalArgumentException" : "System.FormatException") +
            "(\"Null XML mixed fragment\");\n";
    const auto storage = std::string{item} + '.' + (go || java ? field_name(*mixed.variant) : "SrlXmlMixed");
    result += std::string{go ? "switch " : "switch("} + storage + "Index" + (go ? "{\n" : "){\n");
    for (const auto& child : mixed.children) {
      result += "case " + std::to_string(child.alternative) + ":{\n" +
          traverse(traverse, *child.value_type, storage + alternative(child.branch), false) +
          (go ? "}\n" : "break;}\n");
    }
    result += "}\n}\n";
    if (active) {
      const auto condition = active(*mixed.source);
      if (!condition.empty()) {
        result.insert(before, go ? "if " + condition + "{\n" : "if(" + condition + "){\n");
        result += "}\n";
      }
    }
  }
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
