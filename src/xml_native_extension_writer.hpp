// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"
#include "xml_native_scalar_writer.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Keep deferred declaration callbacks stable across collector order and flattened native names.
inline std::string native_declaration_dispatch_name(const class_node& owner) {
  return "srl_xml_declaration_dispatch_" + rohit::digest_to_hex(
      rohit::make_digest<rohit::digest_algorithm::sha256>(owner.get_full_name() + ':' + owner.source_path));
}
// Select namespace-dependent scalar storage without changing its native string/list representation.
inline unsigned native_namespace_kind(const xml_binding::field& field) {
  if (field.prefix_list) { return 3U; }
  if (field.facets.list_item_kind == "QName") { return 2U; }
  if ((!field.facets.union_members.empty() || !field.facets.list_item_facets.empty()) &&
      native_scalar_uses_namespaces(field.facets)) { return 4U; }
  return field.qname || field.facets.lexical_kind == "QName" ? 1U : 0U;
}

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

// Detect namespace/foreign companions, optionally restricting discovery to foreign preservation slots.
inline bool native_extension_graph(const class_node& source, const xml_binding::schema& bindings,
    bool foreign_only = false) {
  std::set<const class_node*> visited{};
  const auto visit = [&](auto&& self, const class_node& owner) -> bool {
    if (!visited.insert(&owner).second) { return false; }
    if (owner.reference) {
      const auto& selected = owner.reference->value_type;
      return selected.type == object_type::class_type && self(self, static_cast<const class_node&>(*selected.resolved_node));
    }
    const auto contract = bindings.bind(owner);
    if (contract.extensions || (!foreign_only && std::any_of(contract.fields.begin(), contract.fields.end(),
        [](const auto& field) { return native_namespace_kind(field) != 0U; }))) { return true; }
    if (contract.mixed && std::any_of(contract.mixed->children.begin(), contract.mixed->children.end(),
        [&](const auto& child) { return self(self, static_cast<const class_node&>(*child.value_type->resolved_node)); })) {
      return true;
    }
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
    if ((contract.mixed || !contract.all_groups.empty()) && contract.extensions &&
        std::any_of(contract.extensions->slots.begin(), contract.extensions->slots.end(),
            [&](const auto& slot) { return !slot.attribute && slot.before_field !=
                contract.fields.size() + (contract.mixed ? contract.mixed->children.size() : 0U); })) {
      throw std::invalid_argument{"Native foreign XML child slots with mixed/all storage require additional position adapters for " + std::string{language}};
    }
    if (contract.mixed) {
      for (const auto& child : contract.mixed->children) {
        self(self, static_cast<const class_node&>(*child.value_type->resolved_node));
      }
    }
    for (const auto& field : contract.fields) {
      if (field.map_binding || field.variant_binding) {
        const auto nested = [&](const type_name& type) {
          auto selected = field; selected.map_binding.reset(); selected.variant_binding.reset();
          member value{}; value.type_name_list.push_back(type); selected.source = &value;
          if (const auto* child = native_extension_child(selected)) { self(self, *child); }
        };
        if (field.map_binding) { nested(field.map_binding->value_type); }
        if (field.variant_binding) {
          for (const auto& alternative : field.variant_binding->alternatives) { nested(alternative.native_type); }
        }
      }
      if (const auto* child = native_extension_child(field); child && native_extension_graph(*child, bindings)) {
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
    const std::function<std::string(std::string_view)>& quote, bool rust,
    const std::map<const member*, std::string>& child_overrides = {},
    const std::map<const member*, xml::name>& type_overrides = {}, unsigned map_key_kind = 0U,
    std::string_view declaration_dispatch = {}) {
  std::string prelude;
  std::map<const xml_binding::field*, std::string> wrappers;
  std::map<const xml_binding::field*, std::string> scalars;
  // Synthetic ownership frames follow actual XML wrappers without adding fields to native models.
  const auto emit_frame = [&](const xml_binding::record& selected, const std::string& callback,
      const std::string& suffix, const std::map<const member*, std::string>& overrides = {},
      const std::map<const member*, xml::name>& types = {}, unsigned key_kind = 0U) {
    auto declaration = native_extension_metadata(selected, std::string{identity} + suffix,
        callback, child_getter, quote, rust, overrides, types, key_kind);
    if (rust) {
      const auto begin = declaration.rfind("fn srl_xml_extension_model()");
      if (begin == std::string::npos) { throw std::logic_error{"Missing container ownership metadata"}; }
      declaration.replace(begin, std::string_view{"fn srl_xml_extension_model"}.size(), "fn " + callback);
    }
    prelude += declaration;
  };
  for (const auto& field : contract.fields) {
    if (!field.map_binding && !field.variant_binding) { continue; }
    const auto callback = (rust ? "srl_xml_container_" : "srl_xml_container_" + std::string{c_getter} + '_') +
        std::to_string(&field - contract.fields.data());
    xml_binding::record wrapper;
    if (field.map_binding) {
      const auto& map = *field.map_binding;
      member key_source{}, value_source{}, entry_source{};
      key_source.name = "key"; key_source.type_name_list.push_back(map.key_type);
      value_source.name = "value"; value_source.type_name_list.push_back(map.value_type);
      entry_source.name = "entry"; entry_source.type_name_list.push_back(map.key_type);
      xml_binding::field key{}, value{}, entry{};
      key.source = &key_source; key.qualified_name = map.key;
      key.attribute = map.key_placement == xml_binding::map_key_placement::attribute;
      value.source = &value_source; value.qualified_name = map.value;
      xml_binding::record item; item.fields = {key, value};
      const auto item_callback = callback + "_entry";
      constexpr unsigned lexical_key_kind = 1U;
      constexpr unsigned integer_key_kind = 2U;
      constexpr unsigned boolean_key_kind = 3U;
      const auto key_kind = map.key_kind == xml_binding::map_key_kind::boolean ? boolean_key_kind :
          map.key_kind == xml_binding::map_key_kind::signed_integer ||
          map.key_kind == xml_binding::map_key_kind::unsigned_integer ? integer_key_kind : lexical_key_kind;
      emit_frame(item, item_callback, ":map-entry:" + field.source->name, {}, {}, key_kind);
      entry.source = &entry_source; entry.qualified_name = map.entry; entry.repeated = true;
      wrapper.fields.push_back(entry);
      emit_frame(wrapper, callback, ":map-wrapper:" + field.source->name, {{&entry_source, item_callback}});
    } else {
      // Value-initialize aggregate descriptors without instantiating an implicit default constructor.
      std::vector<member> sources(field.variant_binding->alternatives.size(), member{});
      std::map<const member*, xml::name> types;
      for (std::size_t index{}; index < sources.size(); ++index) {
        const auto& alternative = field.variant_binding->alternatives[index];
        sources[index].name = alternative.alias;
        sources[index].type_name_list.push_back(alternative.native_type);
        xml_binding::field payload;
        payload.source = &sources[index];
        payload.qualified_name = field.variant_binding->selection == xml_binding::variant_selection::xsi_type ?
            field.variant_binding->wrapper : alternative.tag;
        if (field.variant_binding->selection == xml_binding::variant_selection::xsi_type) {
          types.emplace(&sources[index], alternative.type_identity);
        }
        wrapper.fields.push_back(std::move(payload));
      }
      emit_frame(wrapper, callback, ":variant-wrapper:" + field.source->name, {}, types);
    }
    wrappers.emplace(&field, callback);
  }
  for (const auto& field : contract.fields) {
    if (field.wrapper || field.attribute || field.text || native_namespace_kind(field) == 0U) { continue; }
    const auto callback = (rust ? "srl_xml_scalar_" : "srl_xml_scalar_" + std::string{c_getter} + '_') +
        std::to_string(&field - contract.fields.data());
    auto nested = field;
    nested.text = true;
    nested.repeated = false;
    xml_binding::record scalar;
    scalar.fields.push_back(std::move(nested));
    auto declaration = native_extension_metadata(scalar, std::string{identity} + ":scalar:" + field.source->name,
        callback, child_getter, quote, rust);
    if (rust) {
      const auto begin = declaration.rfind("fn srl_xml_extension_model()");
      if (begin == std::string::npos) { throw std::logic_error{"Missing constrained scalar namespace metadata"}; }
      declaration.replace(begin, std::string_view{"fn srl_xml_extension_model"}.size(), "fn " + callback);
    }
    prelude += declaration;
    scalars.emplace(&field, callback);
  }
  for (const auto& field : contract.fields) {
    if (!field.wrapper || wrappers.contains(&field)) { continue; }
    const auto callback = (rust ? "srl_xml_wrapper_" : "srl_xml_wrapper_" + std::string{c_getter} + '_') +
        std::to_string(&field - contract.fields.data());
    auto nested = field;
    nested.wrapper.reset();
    nested.attribute = false;
    xml_binding::record wrapper;
    wrapper.fields.push_back(std::move(nested));
    prelude += native_extension_metadata(wrapper, std::string{identity} + ":wrapper:" + field.source->name,
        callback, child_getter, quote, rust);
    if (rust) {
      const auto declaration = prelude.rfind("fn srl_xml_extension_model()");
      if (declaration == std::string::npos) { throw std::logic_error{"Missing wrapper namespace metadata"}; }
      prelude.replace(declaration, std::string_view{"fn srl_xml_extension_model"}.size(), "fn " + callback);
    }
    wrappers.emplace(&field, callback);
  }
  std::string result = rust ?
      "/// Return immutable XML-only ownership metadata without constructing a native model.\nfn srl_xml_extension_model()->&'static SrlXmlExtensionModel {\n" :
      "/* Return immutable XML-only ownership metadata; all strings have static lifetime. */\nstatic inline const srl_xml_extension_model* " + std::string{c_getter} + "(void) {\n";
  if (rust) { result += prelude; } else { result = prelude + result; }
  const auto count = [](std::size_t value) { return std::to_string(value); };
  std::map<const xml_binding::field*, std::pair<std::string, std::size_t>> enumerations;
  for (const auto& field : contract.fields) {
    auto lists = field.facets.qname_list_enumeration;
    for (const auto& name : field.facets.qname_enumeration) { lists.push_back({name}); }
    if (lists.empty()) { continue; }
    const auto stem = std::string{rust ? "NAMESPACE_" : "namespace_"} + count(&field - contract.fields.data());
    if (rust) {
      result += "static " + stem + ":&[&[(&str,&str)]]=&[";
      for (const auto& list : lists) {
        result += "&[";
        for (const auto& name : list) { result += '(' + quote(name.namespace_uri) + ',' + quote(name.local) + "),"; }
        result += "],";
      }
      result += "];\n";
    } else {
      std::string values;
      for (std::size_t index{}; index < lists.size(); ++index) {
        const auto items = stem + '_' + count(index);
        if (!lists[index].empty()) {
          result += "static const srl_xml_namespace_name " + items + "[]={";
          for (const auto& name : lists[index]) { result += '{' + quote(name.namespace_uri) + ',' + quote(name.local) + "},"; }
          result += "};\n";
        }
        values += '{' + (lists[index].empty() ? "NULL" : items) + ',' + count(lists[index].size()) + "},";
      }
      result += "static const srl_xml_namespace_list " + stem + "[]={" + values + "};\n";
    }
    enumerations.emplace(&field, std::pair{stem, lists.size()});
  }
  const auto enumeration = [&](const xml_binding::field* field) {
    const auto found = enumerations.find(field);
    if (found == enumerations.end()) { return std::string{rust ? "&[]" : "NULL,0"}; }
    return found->second.first + (rust ? "" : ',' + count(found->second.second));
  };
  std::map<const xml_binding::field*, std::string> scalar_rules;
  for (const auto& field : contract.fields) {
    if (!native_scalar_uses_namespaces(field.facets) || field.wrapper) { continue; }
    const auto stem = std::string{rust ? "SCALAR_" : "scalar_"} + count(&field - contract.fields.data());
    result += rust ? "static " + stem + ":SrlXmlRule=" + rust_scalar_rule(field, quote) + ";\n" :
        c_scalar_declaration(stem, field, quote);
    scalar_rules.emplace(&field, stem);
  }
  // Rule descriptors belong to immutable generated metadata, never to a temporary native value.
  const auto scalar_rule = [&](const xml_binding::field* field) {
    const auto found = scalar_rules.find(field);
    return found == scalar_rules.end() ? std::string{rust ? "None" : "NULL"} :
        rust ? "Some(&" + found->second + ')' : '&' + found->second;
  };
  if (!contract.fields.empty() || contract.mixed) {
    result += rust ? "static FIELDS:&[SrlXmlExtensionField]=&[\n" : "static const srl_xml_extension_field fields[]={\n";
    for (const auto& field : contract.fields) {
      const auto& name = field.wrapper ? *field.wrapper : field.qualified_name;
      const auto* child = field.wrapper ? nullptr : native_extension_child(field);
      const auto namespace_kind = field.wrapper ? 0U : native_namespace_kind(field);
      const auto callback = child_overrides.contains(field.source) ? child_overrides.at(field.source) :
          wrappers.contains(&field) ? wrappers.at(&field) : scalars.contains(&field) ? scalars.at(&field) : child ? child_getter(*child) :
          namespace_kind && !field.attribute && !field.text && !field.wrapper ?
          "srl_xml_namespace_model_" + std::to_string(namespace_kind) : std::string{};
      const auto boolean = [](bool value) { return value ? "true" : "false"; };
      const auto selected_type = type_overrides.find(field.source);
      const auto type_identity = selected_type == type_overrides.end() ?
          std::string{rust ? "None" : "NULL,NULL"} : rust ?
          "Some((" + quote(selected_type->second.namespace_uri) + ',' + quote(selected_type->second.local) + "))" :
          quote(selected_type->second.namespace_uri) + ',' + quote(selected_type->second.local);
      if (rust) {
        result += "SrlXmlExtensionField {uri:" + quote(name.namespace_uri) + ",local:" + quote(name.local) +
            ",attribute:" + boolean(field.attribute) + ",repeated:" + boolean(field.repeated && !field.wrapper) +
            ",child:" + (callback.empty() ? "None" : "Some(" + callback + ')') +
            ",namespace_kind:" + count(namespace_kind) + ",namespace_enumeration:" + enumeration(&field) +
            ",type_identity:" + type_identity + ",scalar_rule:" + scalar_rule(&field) + "},\n";
      } else {
        result += '{' + quote(name.namespace_uri) + ',' + quote(name.local) + ',' + boolean(field.attribute) + ',' +
            boolean(field.repeated && !field.wrapper) + ',' + (callback.empty() ? "NULL" : callback) + ',' +
            count(namespace_kind) + ',' + enumeration(&field) + ',' + type_identity + ',' + scalar_rule(&field) + "},\n";
      }
    }
    if (contract.mixed) {
      for (const auto& child : contract.mixed->children) {
        const auto callback = child_getter(static_cast<const class_node&>(*child.value_type->resolved_node));
        if (rust) {
          result += "SrlXmlExtensionField {uri:" + quote(child.qualified_name.namespace_uri) + ",local:" +
              quote(child.qualified_name.local) + ",attribute:false,repeated:true,child:" +
              (callback.empty() ? "None" : "Some(" + callback + ')') + ",namespace_kind:0,namespace_enumeration:&[],type_identity:None,scalar_rule:None},\n";
        } else {
          result += '{' + quote(child.qualified_name.namespace_uri) + ',' + quote(child.qualified_name.local) +
              ",false,true," + (callback.empty() ? "NULL" : callback) + ",0,NULL,0,NULL,NULL,NULL},\n";
        }
      }
    }
    result += rust ? "];\n" : "};\n";
  }
  if (contract.extensions) {
    const auto& extensions = *contract.extensions;
    for (std::size_t index = 0; index < extensions.slots.size(); ++index) {
      result += rust ? "static URIS" + count(index) + ":&[&str]=&[" : "static const char* const uris" + count(index) + "[]={";
      for (const auto& uri : extensions.slots[index].namespaces) { result += quote(uri) + ','; }
      if (!rust && extensions.slots[index].namespaces.empty()) { result += "NULL"; }
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
        const auto position = slot.before_field == std::numeric_limits<std::size_t>::max() ?
            (rust ? std::string{"usize::MAX"} : std::string{"SIZE_MAX"}) : count(slot.before_field);
        if (rust) {
          result += "SrlXmlExtensionSlot {attribute:" + std::string{slot.attribute ? "true" : "false"} +
              ",position:" + position + ",uris:URIS" + count(index) + ",policy:" + policy +
              ",minimum:" + count(slot.minimum) + ",maximum:" +
              (slot.maximum ? "Some(" + count(*slot.maximum) + ')' : "None") +
              ",namespace_match:" + count(static_cast<unsigned>(slot.namespace_match)) +
              ",process_contents:" + count(static_cast<unsigned>(slot.process_contents)) + "},\n";
        } else {
          result += '{' + std::string{slot.attribute ? "true" : "false"} + ',' + position + ",uris" + count(index) +
              ',' + count(slot.namespaces.size()) + ',' + policy + ',' + count(slot.minimum) + ',' +
              (slot.maximum ? count(*slot.maximum) : "SIZE_MAX") + ',' +
              count(static_cast<unsigned>(slot.namespace_match)) + ',' +
              count(static_cast<unsigned>(slot.process_contents)) + "},\n";
        }
      }
      result += rust ? "];\n" : "};\n";
    }
    if (rust) {
      result += "static CONTRACT:SrlXmlExtensionContract=SrlXmlExtensionContract {core:" + quote(extensions.core_namespace) +
          ",required:" + (extensions.required_namespaces.empty() ? "&[]" : "REQUIRED") + ",slots:" +
          (extensions.slots.empty() ? "&[]" : "SLOTS") + ",validate:" +
          (declaration_dispatch.empty() ? "None" : "Some(" + std::string{declaration_dispatch} + ')') + "};\n";
    } else {
      result += "static const srl_xml_extension_contract contract={" + quote(extensions.core_namespace) + ',' +
          (extensions.required_namespaces.empty() ? "NULL" : "required") + ',' + count(extensions.required_namespaces.size()) + ',' +
          (extensions.slots.empty() ? "NULL" : "slots") + ',' + count(extensions.slots.size()) + ',' +
          (declaration_dispatch.empty() ? "NULL" : std::string{declaration_dispatch}) + "};\n";
    }
  }
  const auto text = std::find_if(contract.fields.begin(), contract.fields.end(),
      [](const auto& field) { return field.text; });
  const auto text_kind = text == contract.fields.end() ? 0U : native_namespace_kind(*text);
  const auto flexible = contract.mixed || !contract.all_groups.empty() || text != contract.fields.end();
  const auto mixed_text = (contract.mixed && !contract.mixed->element_only) || text != contract.fields.end();
  if (rust) {
    result += "static MODEL:SrlXmlExtensionModel=SrlXmlExtensionModel {identity:" + quote(identity) + ",fields:" +
        (contract.fields.empty() && !contract.mixed ? "&[]" : "FIELDS") + ",contract:" + (contract.extensions ? "Some(&CONTRACT)" : "None") +
        ",namespace_text:" + count(text_kind) + ",namespace_enumeration:" + enumeration(text == contract.fields.end() ? nullptr : &*text) +
        ",flexible_children:" + (flexible ? "true" : "false") + ",mixed_text:" + (mixed_text ? "true" : "false") +
        ",map_key_kind:" + count(map_key_kind) + ",scalar_rule:" + scalar_rule(text == contract.fields.end() ? nullptr : &*text) + "}; &MODEL\n}\n";
  } else {
    result += "static const srl_xml_extension_model model={" + quote(identity) + ',' +
        (contract.fields.empty() && !contract.mixed ? "NULL" : "fields") + ',' +
        count(contract.fields.size() + (contract.mixed ? contract.mixed->children.size() : 0U)) + ',' +
        (contract.extensions ? "&contract" : "NULL") + ',' + count(text_kind) + ',' + enumeration(text == contract.fields.end() ? nullptr : &*text) +
        ',' + (flexible ? "true" : "false") + ',' + (mixed_text ? "true" : "false") + ',' + count(map_key_kind) +
        ',' + scalar_rule(text == contract.fields.end() ? nullptr : &*text) + "}; return &model;\n}\n";
  }
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
