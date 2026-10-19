// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Derive generated admission defaults from the shared XML extension policy source of truth.
inline std::string native_extension_constants(bool rust) {
  const xml::extension_limits bounds{};
  const auto constant = [rust](std::string_view c_name, std::string_view rust_name, std::size_t value) {
    return rust ? "const " + std::string{rust_name} + ":usize=" + std::to_string(value) + ";\n" :
        "static const size_t " + std::string{c_name} + '=' + std::to_string(value) + ";\n";
  };
  return constant("srl_xml_extension_max_slots", "SRL_XML_EXTENSION_MAX_SLOTS", bounds.max_slots) +
      constant("srl_xml_extension_max_items", "SRL_XML_EXTENSION_MAX_ITEMS", bounds.max_items) +
      constant("srl_xml_extension_max_preserved_bytes", "SRL_XML_EXTENSION_MAX_PRESERVED_BYTES", bounds.max_preserved_bytes);
}
// Locate an ordinary field's owning item after native collection/optional storage adapters.
inline const class_node* native_extension_child(const xml_binding::field& field) {
  if (field.map_binding || field.variant_binding || field.source->type_name_list.empty()) { return nullptr; }
  const auto* type = &field.source->type_name_list.front();
  while ((type->is_sequence() || type->is_optional()) && !type->arguments.empty()) { type = &type->arguments.front(); }
  if (type->type != object_type::class_type || !type->resolved_node) { return nullptr; }
  const auto* child = static_cast<const class_node*>(type->resolved_node);
  if (!child->reference) { return child; }
  const auto& selected = child->reference->value_type;
  return selected.type == object_type::class_type ? static_cast<const class_node*>(selected.resolved_node) : nullptr;
}

// Detect extension-bearing ordinary owner graphs before selecting an XML entrypoint.
inline bool native_extension_graph(const class_node& source, const xml_binding::schema& bindings) {
  std::set<const class_node*> visited{};
  const auto visit = [&](auto&& self, const class_node& owner) -> bool {
    if (!visited.insert(&owner).second) { return false; }
    if (owner.reference) {
      const auto& selected = owner.reference->value_type;
      return selected.type == object_type::class_type && self(self, static_cast<const class_node&>(*selected.resolved_node));
    }
    const auto contract = bindings.bind(owner);
    if (contract.extensions) { return true; }
    return std::any_of(contract.fields.begin(), contract.fields.end(), [&](const auto& field) {
      if (field.map_binding) {
        auto selected = field; selected.map_binding.reset();
        member value{}; value.type_name_list.push_back(field.map_binding->value_type); selected.source = &value;
        const auto* child = native_extension_child(selected); return child && self(self, *child);
      }
      if (field.variant_binding) {
        return std::any_of(field.variant_binding->alternatives.begin(), field.variant_binding->alternatives.end(), [&](const auto& alternative) {
          auto selected = field; selected.variant_binding.reset();
          member value{}; value.type_name_list.push_back(alternative.native_type); selected.source = &value;
          const auto* child = native_extension_child(selected); return child && self(self, *child);
        });
      }
      const auto* child = native_extension_child(field);
      return child && self(self, *child);
    });
  };
  return visit(visit, source);
}

// Fence combinations whose native sidecar needs additional positions instead of silently dropping nested state.
inline void validate_native_extension_graph(const class_node& source, const xml_binding::schema& bindings,
    std::string_view language) {
  if (!native_extension_graph(source, bindings)) { return; }
  std::set<const class_node*> visited;
  const auto visit = [&](auto&& self, const class_node& owner) -> void {
    if (!visited.insert(&owner).second) { return; }
    const auto contract = bindings.bind(owner);
    if (contract.mixed || !contract.all_groups.empty()) {
      throw std::invalid_argument{"Native positioned XML companions with mixed/all graphs are not implemented for " + std::string{language}};
    }
    if (contract.model.revision || contract.model.magic) {
      throw std::invalid_argument{"Native positioned XML companions with revision/magic metadata require additional known attribute adapters for " + std::string{language}};
    }
    for (const auto& field : contract.fields) {
      if (field.map_binding || field.variant_binding) {
        const auto nested = [&](const type_name& type) {
          auto selected = field; selected.map_binding.reset(); selected.variant_binding.reset();
          member value{}; value.type_name_list.push_back(type); selected.source = &value;
          const auto* child = native_extension_child(selected); return child && native_extension_graph(*child, bindings);
        };
        if ((field.map_binding && nested(field.map_binding->value_type)) ||
            (field.variant_binding && std::any_of(field.variant_binding->alternatives.begin(), field.variant_binding->alternatives.end(),
                [&](const auto& alternative) { return nested(alternative.native_type); }))) {
          throw std::invalid_argument{"Native positioned XML companions in map/variant payloads are not implemented for " + std::string{language}};
        }
      }
      if (const auto* child = native_extension_child(field); child && native_extension_graph(*child, bindings)) {
        if (field.wrapper || field.presence.allows_nil) {
          throw std::invalid_argument{"Native positioned XML companions in wrapped or nil record fields are not implemented for " + std::string{language}};
        }
        self(self, *child);
      }
    }
  };
  visit(visit, source);
}

// Emit immutable expanded-name/ownership metadata; child callbacks name existing generated models.
// Rust returns an associated method body; C returns a complete static getter definition.
inline std::string native_extension_metadata(const xml_binding::record& contract,
    std::string_view identity, std::string_view c_getter,
    const std::function<std::string(const class_node&)>& child_getter,
    const std::function<std::string(std::string_view)>& quote, bool rust) {
  std::string result = rust ?
      "/// Return immutable XML-only ownership metadata without constructing a native model.\nfn srl_xml_extension_model()->&'static SrlXmlExtensionModel {\n" :
      "/* Return immutable XML-only ownership metadata; all strings have static lifetime. */\nstatic inline const srl_xml_extension_model* " + std::string{c_getter} + "(void) {\n";
  const auto count = [](std::size_t value) { return std::to_string(value); };
  if (!contract.fields.empty()) {
    result += rust ? "static FIELDS:&[SrlXmlExtensionField]=&[\n" : "static const srl_xml_extension_field fields[]={\n";
    for (const auto& field : contract.fields) {
      const auto& name = field.wrapper ? *field.wrapper : field.qualified_name;
      const auto* child = field.wrapper ? nullptr : native_extension_child(field);
      const auto callback = child ? child_getter(*child) : std::string{};
      const auto boolean = [](bool value) { return value ? "true" : "false"; };
      if (rust) {
        result += "SrlXmlExtensionField {uri:" + quote(name.namespace_uri) + ",local:" + quote(name.local) +
            ",attribute:" + boolean(field.attribute) + ",repeated:" + boolean(field.repeated && !field.wrapper) +
            ",child:" + (callback.empty() ? "None" : "Some(" + callback + ')') + "},\n";
      } else {
        result += '{' + quote(name.namespace_uri) + ',' + quote(name.local) + ',' + boolean(field.attribute) + ',' +
            boolean(field.repeated && !field.wrapper) + ',' + (callback.empty() ? "NULL" : callback) + "},\n";
      }
    }
    result += rust ? "];\n" : "};\n";
  }
  if (contract.extensions) {
    const auto& extensions = *contract.extensions;
    for (std::size_t index = 0; index < extensions.slots.size(); ++index) {
      result += rust ? "static URIS" + count(index) + ":&[&str]=&[" : "static const char* const uris" + count(index) + "[]={";
      for (const auto& uri : extensions.slots[index].namespaces) { result += quote(uri) + ','; }
      result += rust ? "];\n" : "};\n";
    }
    if (!extensions.required_namespaces.empty()) {
      result += rust ? "static REQUIRED:&[&str]=&[" : "static const char* const required[]={";
      for (const auto& uri : extensions.required_namespaces) { result += quote(uri) + ','; }
      result += rust ? "];\n" : "};\n";
    }
    if (!extensions.slots.empty()) {
      result += rust ? "static SLOTS:&[SrlXmlExtensionSlot]=&[\n" : "static const srl_xml_extension_slot slots[]={\n";
      for (std::size_t index = 0; index < extensions.slots.size(); ++index) {
        const auto& slot = extensions.slots[index];
        const auto policy = slot.policy == xml::unknown_policy::preserve ? "2" : slot.policy == xml::unknown_policy::skip ? "1" : "0";
        if (rust) {
          result += "SrlXmlExtensionSlot {attribute:" + std::string{slot.attribute ? "true" : "false"} +
              ",position:" + count(slot.before_field) + ",uris:URIS" + count(index) + ",policy:" + policy +
              ",minimum:" + count(slot.minimum) + ",maximum:" +
              (slot.maximum ? "Some(" + count(*slot.maximum) + ')' : "None") + "},\n";
        } else {
          result += '{' + std::string{slot.attribute ? "true" : "false"} + ',' + count(slot.before_field) + ",uris" + count(index) +
              ',' + count(slot.namespaces.size()) + ',' + policy + ',' + count(slot.minimum) + ',' +
              (slot.maximum ? count(*slot.maximum) : "SIZE_MAX") + "},\n";
        }
      }
      result += rust ? "];\n" : "};\n";
    }
    if (rust) {
      result += "static CONTRACT:SrlXmlExtensionContract=SrlXmlExtensionContract {core:" + quote(extensions.core_namespace) +
          ",required:" + (extensions.required_namespaces.empty() ? "&[]" : "REQUIRED") + ",slots:" +
          (extensions.slots.empty() ? "&[]" : "SLOTS") + "};\n";
    } else {
      result += "static const srl_xml_extension_contract contract={" + quote(extensions.core_namespace) + ',' +
          (extensions.required_namespaces.empty() ? "NULL" : "required") + ',' + count(extensions.required_namespaces.size()) + ',' +
          (extensions.slots.empty() ? "NULL" : "slots") + ',' + count(extensions.slots.size()) + "};\n";
    }
  }
  if (rust) {
    result += "static MODEL:SrlXmlExtensionModel=SrlXmlExtensionModel {identity:" + quote(identity) + ",fields:" +
        (contract.fields.empty() ? "&[]" : "FIELDS") + ",contract:" + (contract.extensions ? "Some(&CONTRACT)" : "None") + "}; &MODEL\n}\n";
  } else {
    result += "static const srl_xml_extension_model model={" + quote(identity) + ',' +
        (contract.fields.empty() ? "NULL" : "fields") + ',' + count(contract.fields.size()) + ',' +
        (contract.extensions ? "&contract" : "NULL") + "}; return &model;\n}\n";
  }
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
