// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"
#include "xml_portable_writer.hpp"
#include "xml_native_scalar_identity_writer.hpp"

#include <rohit/xml_pattern.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Derive bounded native scalar work from the shared compiled-pattern admission contract.
inline std::string rust_scalar_constants() {
  const xml::pattern_limits bounds{};
  return "const SRL_XML_SCALAR_MAX_WORK:usize=" + std::to_string(bounds.max_work_units) + ";\n" +
      "const SRL_XML_SCALAR_PATTERN_BYTES:usize=" + std::to_string(bounds.max_input_bytes) + ";\n";
}

// Keep C work and input budgets identical to the approved shared pattern compiler defaults.
inline std::string c_scalar_constants() {
  const xml::pattern_limits bounds{};
  return "static const size_t srl_xml_scalar_max_work=" + std::to_string(bounds.max_work_units) + "u;\n" +
      "static const size_t srl_xml_scalar_pattern_bytes=" + std::to_string(bounds.max_input_bytes) + "u;\n";
}

// Lower immutable Unicode Thompson instructions directly, without target regex parsing or caches.
inline std::string rust_scalar_pattern(const xml::portable_pattern& pattern) {
  constexpr unsigned consumes{1U}, inverted{2U}, dot{4U}, accepting{8U};
  std::string result = "SrlXmlPattern{start:" + std::to_string(pattern.start()) + ",states:&[";
  for (const auto& state : pattern.states()) {
    result += "SrlXmlPatternState{epsilon:&[";
    for (const auto target : state.epsilon) { result += std::to_string(target) + ','; }
    result += "],ranges:&[";
    for (const auto& range : state.ranges) {
      result += '(' + std::to_string(range.first) + ',' + std::to_string(range.last) + "),";
    }
    const auto flags = (state.consumes ? consumes : 0U) | (state.inverted ? inverted : 0U) |
        (state.dot ? dot : 0U) | (state.accepting ? accepting : 0U);
    result += "],next:" + std::to_string(state.next) + ",flags:" + std::to_string(flags) + "},";
  }
  return result + "]}";
}

// Keep independent union members, inherited OR-pattern groups and exact lexical bounds intact.
inline std::string rust_scalar_rule(const xml_binding::field& field,
    const std::function<std::string(std::string_view)>& quote) {
  // Use the native pointer width only for metadata already admitted as a compiler size_t.
  const auto extent = [](const std::optional<std::size_t>& value) {
    return value ? "Some(" + std::to_string(*value) + ")" : "None";
  };
  const auto rule = [&](auto&& self, const xml_binding::scalar_facets& facets,
                        const std::optional<std::string>& fixed,
                        const std::optional<xml_binding::scalar_value_identity>& fixed_identity) -> std::string {
    std::string result = "SrlXmlRule{kind:" + quote(facets.lexical_kind) +
        ",whitespace:" + quote(facets.whitespace) + ",bounds:[";
    for (const auto* bound : {&facets.min_inclusive, &facets.max_inclusive, &facets.min_exclusive, &facets.max_exclusive}) {
      result += quote(bound->value_or("")) + ',';
    }
    result += "],length:" + extent(facets.length) + ",minimum:" + extent(facets.min_length) +
        ",maximum:" + extent(facets.max_length) + ",enumeration:&[";
    if (facets.qname_enumeration.empty() && facets.qname_list_enumeration.empty() &&
        facets.union_enumeration.empty()) {
      for (const auto& value : facets.enumeration) { result += quote(value) + ','; }
    }
    result += "],finite:" + std::string{facets.finite ? "true" : "false"} + ",groups:&[";
    auto groups = facets.pattern_groups;
    if (!facets.patterns.empty()) { groups.push_back(facets.patterns); }
    for (const auto& group : groups) {
      result += "&[";
      for (const auto& expression : group) { result += rust_scalar_pattern(xml::compile_portable_pattern(expression)) + ','; }
      result += "],";
    }
    result += "],list_kind:" + quote(facets.list_item_kind) + ",members:&[";
    for (const auto& member : facets.union_members) { result += self(self, member, {}, {}) + ','; }
    result += "],fixed:" + (fixed && !fixed_identity ? "Some(" + quote(*fixed) + ")" : "None") +
        ",namespace_dependent:" + (native_scalar_uses_namespaces(facets) ? "true" : "false") + ",identities:&[";
    for (const auto& identity : native_scalar_identities(facets)) { result += rust_scalar_identity(identity, quote) + ','; }
    return result + "],fixed_identity:" + (fixed_identity ? "Some(" + rust_scalar_identity(*fixed_identity, quote) + ")" : "None") +
        ",item_rule:" + (facets.list_item_facets.empty() ? "None" : "Some(&" + self(self, facets.list_item_facets.front(), {}, {}) + ')') + ",schema_default:None}";
  };
  auto result = rule(rule, field.facets, field.fixed_value, native_scalar_fixed_identity(field));
  const auto fallback = native_scalar_default_lexical(field);
  if (fallback) { result.replace(result.rfind("schema_default:None}"), std::string_view{"schema_default:None}"}.size(), "schema_default:Some(" + quote(*fallback) + ")}"); }
  return result;
}

// Emit file-scope C11 immutable tables in dependency order, without compound-literal lifetime assumptions.
inline std::string c_scalar_declaration(const std::string& identifier, const xml_binding::field& field,
    const std::function<std::string(std::string_view)>& quote) {
  std::string declarations;
  const auto extent = [](const std::optional<std::size_t>& value) {
    return value ? std::to_string(*value) + "u" : "SIZE_MAX";
  };
  // Each instruction refers only to tables already declared in this translation unit.
  const auto pattern = [&](const std::string& name, const xml::portable_pattern& compiled) {
    std::string states;
    std::size_t index{};
    for (const auto& state : compiled.states()) {
      const auto stem = name + '_' + std::to_string(index++);
      if (!state.epsilon.empty()) {
        declarations += "static const size_t " + stem + "_epsilon[]={";
        for (const auto target : state.epsilon) { declarations += std::to_string(target) + "u,"; }
        declarations += "};\n";
      }
      if (!state.ranges.empty()) {
        declarations += "static const srl_xml_scalar_range " + stem + "_ranges[]={";
        for (const auto& range : state.ranges) {
          declarations += '{' + std::to_string(range.first) + "u," + std::to_string(range.last) + "u},";
        }
        declarations += "};\n";
      }
      constexpr unsigned consumes{1U}, inverted{2U}, dot{4U}, accepting{8U};
      const auto flags = (state.consumes ? consumes : 0U) | (state.inverted ? inverted : 0U) |
          (state.dot ? dot : 0U) | (state.accepting ? accepting : 0U);
      states += '{' + (state.epsilon.empty() ? std::string{"NULL"} : stem + "_epsilon") + ',' +
          std::to_string(state.epsilon.size()) + "u," + (state.ranges.empty() ? "NULL" : stem + "_ranges") + ',' +
          std::to_string(state.ranges.size()) + "u," + std::to_string(state.next) + "u," + std::to_string(flags) + "u},";
    }
    declarations += "static const srl_xml_scalar_state " + name + "_states[]={" + states + "};\n";
    return '{' + std::to_string(compiled.start()) + "u," + name + "_states," + std::to_string(compiled.states().size()) + "u}";
  };
  // Recursive union members retain their own builtin type, whitespace and restrictions.
  const auto rule = [&](auto&& self, const std::string& name, const xml_binding::scalar_facets& facets,
                        const std::optional<std::string>& fixed,
                        const std::optional<xml_binding::scalar_value_identity>& fixed_identity) -> std::string {
    const bool lexical_enumeration = !facets.enumeration.empty() &&
        facets.qname_enumeration.empty() && facets.qname_list_enumeration.empty() &&
        facets.union_enumeration.empty();
    if (lexical_enumeration) {
      declarations += "static const char* const " + name + "_enumeration[]={";
      for (const auto& value : facets.enumeration) { declarations += quote(value) + ','; }
      declarations += "};\n";
    }
    auto groups = facets.pattern_groups;
    if (!facets.patterns.empty()) { groups.push_back(facets.patterns); }
    std::string group_values;
    for (std::size_t group{}; group < groups.size(); ++group) {
      const auto stem = name + "_group_" + std::to_string(group);
      std::string patterns;
      for (std::size_t index{}; index < groups[group].size(); ++index) {
        patterns += pattern(stem + '_' + std::to_string(index), xml::compile_portable_pattern(groups[group][index])) + ',';
      }
      declarations += "static const srl_xml_scalar_pattern " + stem + "[]={" + patterns + "};\n";
      group_values += '{' + stem + ',' + std::to_string(groups[group].size()) + "u},";
    }
    if (!groups.empty()) { declarations += "static const srl_xml_scalar_group " + name + "_groups[]={" + group_values + "};\n"; }
    if (!facets.union_members.empty()) {
      std::string members;
      for (std::size_t index{}; index < facets.union_members.size(); ++index) {
        members += self(self, name + "_member_" + std::to_string(index), facets.union_members[index], {}, {}) + ',';
      }
      declarations += "static const srl_xml_scalar_rule " + name + "_members[]={" + members + "};\n";
    }
    if (!facets.list_item_facets.empty()) {
      const auto item = self(self, name + "_item", facets.list_item_facets.front(), {}, {});
      declarations += "static const srl_xml_scalar_rule " + name + "_item=" + item + ";\n";
    }
    const auto identities = native_scalar_identities(facets);
    const auto identity_value = [&](auto&& self, const xml_binding::scalar_value_identity& identity, const std::string& stem) -> std::string {
      if (!identity.names.empty()) {
        declarations += "static const srl_xml_scalar_expanded_name " + stem + "[]={";
        for (const auto& item : identity.names) { declarations += '{' + quote(item.namespace_uri) + ',' + quote(item.local) + "},"; }
        declarations += "};\n";
      }
      if (!identity.items.empty()) {
        std::string items;
        for (std::size_t index{}; index < identity.items.size(); ++index) {
          items += self(self, identity.items[index], stem + "_item_" + std::to_string(index)) + ',';
        }
        declarations += "static const srl_xml_scalar_identity " + stem + "_items[]={" + items + "};\n";
      }
      return '{' + quote(identity.lexical_kind) + ',' + quote(identity.list_item_kind) + ',' +
          quote(identity.lexical_value) + ',' + (identity.names.empty() ? "NULL" : stem) + ',' +
          std::to_string(identity.names.size()) + "u," + (identity.items.empty() ? "NULL" : stem + "_items") + ',' +
          std::to_string(identity.items.size()) + "u}";
    };
    if (!identities.empty()) {
      std::string values;
      for (std::size_t index{}; index < identities.size(); ++index) {
        values += identity_value(identity_value, identities[index], name + "_identity_names_" + std::to_string(index)) + ',';
      }
      declarations += "static const srl_xml_scalar_identity " + name + "_identities[]={" + values + "};\n";
    }
    if (fixed_identity) {
      const auto value = identity_value(identity_value, *fixed_identity, name + "_fixed_names");
      declarations += "static const srl_xml_scalar_identity " + name + "_fixed_identity=" + value + ";\n";
    }
    std::string result = '{' + quote(facets.lexical_kind) + ',' + quote(facets.whitespace) + ",{ ";
    for (const auto* bound : {&facets.min_inclusive, &facets.max_inclusive, &facets.min_exclusive, &facets.max_exclusive}) {
      result += quote(bound->value_or("")) + ',';
    }
    result += "}," + extent(facets.length) + ',' + extent(facets.min_length) + ',' + extent(facets.max_length) + ',' +
        (lexical_enumeration ? name + "_enumeration" : "NULL") + ',' + std::to_string(lexical_enumeration ? facets.enumeration.size() : 0U) + "u," +
        (facets.finite ? "true" : "false") + ',' + (groups.empty() ? "NULL" : name + "_groups") + ',' + std::to_string(groups.size()) + "u," +
        quote(facets.list_item_kind) + ',' + (facets.union_members.empty() ? "NULL" : name + "_members") + ',' +
        std::to_string(facets.union_members.size()) + "u," + (fixed && !fixed_identity ? quote(*fixed) : "NULL") + ',' +
        (native_scalar_uses_namespaces(facets) ? "true" : "false") + ',' +
        (identities.empty() ? "NULL" : name + "_identities") + ',' + std::to_string(identities.size()) + "u," +
        (fixed_identity ? '&' + name + "_fixed_identity" : "NULL") + ',' +
        (facets.list_item_facets.empty() ? "NULL" : '&' + name + "_item") + ",NULL}";
    return result;
  };
  auto initializer = rule(rule, identifier, field.facets, field.fixed_value, native_scalar_fixed_identity(field));
  const auto fallback = native_scalar_default_lexical(field);
  if (fallback) { initializer.replace(initializer.size() - std::string_view{"NULL}"}.size(), std::string_view{"NULL}"}.size(), quote(*fallback) + '}'); }
  return declarations + "static const srl_xml_scalar_rule " + identifier + '=' + initializer + ";\n";
}
} // namespace rohit::serializer::writer::xml_codegen
