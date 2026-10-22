// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <functional>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Alias only compiler-owned runtime identifiers, preserving quoted schema names and lexical values.
inline std::string portable_declaration_alias(std::string_view source, std::string_view language,
    std::string_view prefix) {
  if (prefix.empty()) { return std::string{source}; }
  static constexpr std::pair<std::string_view, std::string_view> csharp_names[]{
      {"SrlXmlDeclarationType", "Type"}, {"SrlXmlDeclaration", "Value"},
      {"SrlXmlDeclarations", "Inventory"}, {"SrlXmlDeclarationInventories", "Inventories"},
      {"SrlXmlDeclarationPolicy", "Policy"}, {"SrlXmlDeclarationRead", "Read"}};
  static constexpr std::pair<std::string_view, std::string_view> go_names[]{
      {"srlXmlDeclarationType", "Type"}, {"srlXmlDeclaredValue", "Value"},
      {"srlXmlDeclarations", "Inventory"}, {"srlXmlDeclarationInventories", "Inventories"},
      {"srlXmlDeclarationPolicy", "Policy"}, {"srlXmlDeclarationRead", "Read"},
      {"srlXmlDeclarationFind", "Find"}, {"srlXmlDeclarationInputPolicy", "InputPolicy"},
      {"srlXmlDeclarationOutputPolicy", "OutputPolicy"}, {"srlXmlDeclarationEmit", "Emit"}};
  const std::span<const std::pair<std::string_view, std::string_view>> names = language == "go"
      ? std::span<const std::pair<std::string_view, std::string_view>>{go_names}
      : std::span<const std::pair<std::string_view, std::string_view>>{csharp_names};
  const auto identifier = [](char value) {
    return value == '_' || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
        (value >= '0' && value <= '9');
  };
  std::string result;
  for (std::size_t offset{}; offset < source.size();) {
    const char value = source[offset];
    if (value == '/' && offset + 1U < source.size() && source[offset + 1U] == '/') {
      const auto newline = source.find('\n', offset);
      const auto end = newline == std::string_view::npos ? source.size() : newline + 1U;
      result += source.substr(offset, end - offset); offset = end; continue;
    }
    if (value == '/' && offset + 1U < source.size() && source[offset + 1U] == '*') {
      const auto close = source.find("*/", offset + 2U);
      const auto end = close == std::string_view::npos ? source.size() : close + 2U;
      result += source.substr(offset, end - offset); offset = end; continue;
    }
    if (value == '"' || value == '\'' || value == '`') {
      const auto quote = value;
      result += source[offset++];
      while (offset < source.size()) {
        const auto current = source[offset++]; result += current;
        if (current == '\\' && quote != '`' && offset < source.size()) { result += source[offset++]; }
        else if (current == quote) { break; }
      }
      continue;
    }
    if (!identifier(value)) { result += value; ++offset; continue; }
    auto end = offset + 1U;
    while (end < source.size() && identifier(source[end])) { ++end; }
    const auto token = source.substr(offset, end - offset);
    const auto found = std::find_if(names.begin(), names.end(), [&](const auto& name) { return name.first == token; });
    if (found == names.end()) { result += token; }
    else { result += prefix; result += found->second; }
    offset = end;
  }
  return result;
}

// Emit immutable declaration identities and deferred private codec dispatch after names are fixed.
inline std::string portable_declaration_dispatch(
    std::span<const std::shared_ptr<const xml_binding::declaration_inventory>> inventories,
    const std::function<std::string(const class_node&)>& model_name,
    const std::function<std::string(std::string_view)>& quote, std::string_view language, std::string_view prefix = {}) {
  const bool java = language == "java", go = language == "go";
  std::string result;
  const auto line = [&](const std::string& text) { result += portable_declaration_alias(text, language, prefix) + '\n'; };
  if (go) {
    line("// Immutable lookup contains only declaration identities and private validator indexes.");
    line("var srlXmlDeclarationInventories=[]srlXmlDeclarations{");
  } else {
    line(java ? "/** Immutable XSD declaration inventories; validation remains on private generated codecs. */" :
        "// Immutable XSD declarations dispatch through private generated token codecs.");
    line(std::string{java ? "private static final SrlXmlDeclarations[] srlXmlDeclarationInventories=new SrlXmlDeclarations[]{" :
        "private static readonly SrlXmlDeclarations[] SrlXmlDeclarationInventories=new SrlXmlDeclarations[]{"});
  }
  for (const auto& inventory : inventories) {
    line(go ? "{values:[]srlXmlDeclaredValue{" : "new SrlXmlDeclarations(new SrlXmlDeclaration[]{");
    for (const auto& global : inventory->globals) {
      if (!global.validator_index) { throw std::logic_error{"Missing private XML declaration validator"}; }
      const auto boolean = [](bool value) { return value ? "true" : "false"; };
      const auto name = global.qualified_name.namespace_uri.empty() ? global.qualified_name.local :
          '{' + global.qualified_name.namespace_uri + '}' + global.qualified_name.local;
      const auto index = std::to_string(*global.validator_index);
      const auto& root = inventory->validators.at(*global.validator_index).root;
      const auto root_name = root.namespace_uri.empty() ? root.local : '{' + root.namespace_uri + '}' + root.local;
      const auto nil_index = global.nil_validator_index ? std::to_string(*global.nil_validator_index) : "-1";
      const auto type_name = global.declared_type ? (global.declared_type->namespace_uri.empty() ? global.declared_type->local :
          '{' + global.declared_type->namespace_uri + '}' + global.declared_type->local) : std::string{};
      if (go) {
        std::string types{"[]srlXmlDeclarationType{"};
        for (const auto& type : global.type_validators) {
          const auto type_identity = type.qualified_name.namespace_uri.empty() ? type.qualified_name.local :
              '{' + type.qualified_name.namespace_uri + '}' + type.qualified_name.local;
          types += "{name:" + quote(type_identity) + ",index:" + std::to_string(type.validator_index) +
              ",nilIndex:" + (type.nil_validator_index ? std::to_string(*type.nil_validator_index) : "-1") +
              ",abstract:" + boolean(type.abstract_type) + "},";
        }
        types += '}';
        line("{name:srlxml.Name{Space:" + quote(global.qualified_name.namespace_uri) + ",Local:" + quote(global.qualified_name.local) +
            "},attribute:" + boolean(global.attribute) + ",nillable:" + boolean(global.nillable) +
            ",abstract:" + boolean(global.abstract_element) + ",fixed:" + boolean(global.fixed_value.has_value()) +
            ",index:" + index + ",nilIndex:" + nil_index + ",declaredType:" + quote(type_name) +
            ",root:srlxml.Name{Space:" + quote(root.namespace_uri) + ",Local:" + quote(root.local) + "},abstractType:" +
            boolean(global.abstract_type) + ",types:" + types + "},");
      } else {
        std::string types{"new SrlXmlDeclarationType[]{"};
        for (const auto& type : global.type_validators) {
          const auto type_identity = type.qualified_name.namespace_uri.empty() ? type.qualified_name.local :
              '{' + type.qualified_name.namespace_uri + '}' + type.qualified_name.local;
          types += "new SrlXmlDeclarationType(" + quote(type_identity) + ',' + std::to_string(type.validator_index) + ',' +
              (type.nil_validator_index ? std::to_string(*type.nil_validator_index) : "-1") + ',' + boolean(type.abstract_type) + "),";
        }
        types += '}';
        line("new SrlXmlDeclaration(" + quote(name) + ',' + boolean(global.attribute) + ',' + boolean(global.nillable) + ',' +
            boolean(global.abstract_element) + ',' + boolean(global.fixed_value.has_value()) + ',' + index + ',' + nil_index + ',' +
            quote(type_name) + ',' + quote(root_name) + ',' + boolean(global.abstract_type) + ',' + types + "),");
      }
    }
    line(go ? "}}," : "}),");
  }
  line(go ? "}" : "};");
  line(go ? "// Read one private declaration without installing package root authorizations." :
      java ? "/** Read only a compiler-selected private DTO under the unchanged active operation. */" :
      "// Read a compiler-selected DTO under the same explicit native operation.");
  line(go ? "func srlXmlDeclarationRead(inventory,index int,input *srlXmlInput,name srlxml.Name)error{switch inventory{" :
      std::string{java ? "private static void srlXmlDeclarationRead(int inventory,int index,SrlXmlInput input,String name){switch(inventory){" :
      "private static void SrlXmlDeclarationRead(int inventory,int index,SrlXmlInput input,string name){switch(inventory){"});
  for (std::size_t inventory_index{}; inventory_index < inventories.size(); ++inventory_index) {
    const auto& inventory = *inventories[inventory_index];
    line("case " + std::to_string(inventory_index) + (go ? ":switch index{" : ":switch(index){"));
    for (std::size_t index{}; index < inventory.validators.size(); ++index) {
      const auto found = std::find_if(inventory.validator_indices.begin(), inventory.validator_indices.end(),
          [&](const auto& entry) { return entry.second == index; });
      if (found == inventory.validator_indices.end()) { throw std::logic_error{"Missing private XML validator class"}; }
      const auto* owner = found->first;
      const auto model = model_name(*owner);
      result += "case " + std::to_string(index) + ':' + (go ? "_,err:=srlXmlDecode" + model + "(input,name);return err" :
          model + (java ? ".xmlRead(input,name);return;" : ".XmlRead(input,name);return;")) + '\n';
    }
    line(go ? "default:return fmt.Errorf(\"Unknown private XML validator\")}" :
        std::string{"default:throw new "} + (java ? "IllegalArgumentException" : "System.FormatException") + "(\"Unknown private XML validator\");}");
  }
  line(go ? "default:return fmt.Errorf(\"Unknown XML declaration inventory\")}}" :
      std::string{"default:throw new "} + (java ? "IllegalArgumentException" : "System.FormatException") + "(\"Unknown XML declaration inventory\");}}");
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
