// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_portable_writer.hpp"
#include "native_schema.hpp"

#include <stdexcept>

namespace rohit::serializer::writer::xml_portable {
namespace {
const std::string_view go_runtime =
#include "xml_go_runtime.inc"
    ;
// Retain exact schema strings in generated native source.
std::string quote(std::string_view value) { return native::quote(value); }
// Build an expanded name independently of document prefixes.
std::string qualified(const xml::name& value) {
  return "srlxml.Name{Space:" + quote(value.namespace_uri) + ",Local:" + quote(value.local) + "}";
}
// Select the statically known leaf beneath one supported presence/collection modifier.
const type_name& leaf(const member& value) {
  const auto& type = value.type_name_list.front();
  return type.is_optional() || type.is_sequence() ? type.arguments.front() : type;
}
// Select the native scalar width without repeating its range in generated checks.
std::string bits(const type_name& type) {
  if (type.name == "float" || type.name == "float32") { return "32"; }
  if (type.name == "double" || type.name == "float64") { return "64"; }
  return type.name.substr(type.name.starts_with("uint") ? 4U : 3U);
}
// Format a statically typed Go scalar; enum spelling remains schema-owned.
std::string encode_scalar(const type_name& type, const std::string& value,
                          const type_name_fn& name) {
  if (type.type == object_type::enum_type) {
    const auto& enumeration = static_cast<const enum_node&>(*type.resolved_node);
    std::string result{"var srlText string\nswitch " + value + " {\n"};
    for (std::size_t index{}; index < enumeration.enum_name_list.size(); ++index) {
      result += "case " + name(type) + "(" + std::to_string(index) + "): srlText=" + quote(enumeration.enum_name_list[index]) + "\n";
    }
    return result + "default:return nil,fmt.Errorf(\"Invalid XML enum\")\n}\n";
  }
  const auto expression = type.name == "string" ? value : type.name == "bool" ? "strconv.FormatBool(" + value + ")" :
      type.name.starts_with("uint") ? "strconv.FormatUint(uint64(" + value + "),10)" :
      type.name.starts_with("int") ? "strconv.FormatInt(int64(" + value + "),10)" :
      "srlXmlFormatFloat(float64(" + value + ")," + bits(type) + ")";
  return "srlText:=" + expression + "\n";
}
// Parse a statically typed Go scalar with exact XML lexical/range checks.
std::string decode_scalar(const type_name& type, const type_name_fn& name) {
  if (type.type == object_type::enum_type) {
    const auto& enumeration = static_cast<const enum_node&>(*type.resolved_node);
    std::string result{"var srlValue " + name(type) + "\nswitch srlText {\n"};
    for (std::size_t index{}; index < enumeration.enum_name_list.size(); ++index) {
      result += "case " + quote(enumeration.enum_name_list[index]) + ":srlValue=" + name(type) + "(" + std::to_string(index) + ")\n";
    }
    return result + "default:return nil,fmt.Errorf(\"Invalid XML enum\")\n}\n";
  }
  if (type.name == "string") { return "srlValue:=srlText\n"; }
  const auto parser = type.name == "bool" ? "srlXmlBool(srlText)" :
      type.name.starts_with("uint") ? "srlXmlUint(srlText," + bits(type) + ")" :
      type.name.starts_with("int") ? "srlXmlInt(srlText," + bits(type) + ")" :
      "srlXmlDouble(srlText," + bits(type) + ")";
  return "srlParsed,srlErr:=" + parser + "\nif srlErr!=nil{return nil,srlErr}\nsrlValue:=" + name(type) + "(srlParsed)\n";
}
// Drop the Go pointer decoration only when naming a generated record helper.
std::string record_name(std::string value) {
  if (!value.empty() && value.front() == '*') { value.erase(0, 1U); }
  return value;
}
} // namespace

std::string imports(std::string_view language) {
  return language == "go" ? "import (srlxml \"encoding/xml\";srlio \"io\")\n" : "";
}
std::string runtime(std::string_view language) {
  if (language != "go") { throw std::invalid_argument{"Unsupported XML portable runtime"}; }
  return std::string{go_runtime};
}
std::string methods(const class_node& object, const xml_binding::record& contract,
    const std::string& identifier, const field_name& field, const type_name_fn& type,
    const xml_options& options, std::string_view language) {
  if (language != "go") { throw std::invalid_argument{"Unsupported XML portable methods"}; }
  for (const auto& value : object.member_list) {
    if (field(value) == "EncodeXML") {
      throw std::invalid_argument{"XML encoder method collides with a generated Go field"};
    }
  }
  std::string out{"\n// EncodeXML serializes one complete owning value under its fixed XML mapping.\nfunc (value *" + identifier + ") EncodeXML(policies ...XmlOptions)([]byte,error){\n"};
  out += "policy,err:=srlXmlOptions(policies);if err!=nil{return nil,err}\n";
  if (!options.runtime_xsd) { out += "if policy.Validator!=nil{return nil,fmt.Errorf(\"Runtime XSD was not enabled during generation\")}\n"; }
  out += "budget:=srlXmlBudget{limits:policy.Limits}\nnode,err:=value.srlXmlNode(&budget);if err!=nil{return nil,err}\ndata,err:=srlXmlWrite(node,policy.Limits);if err!=nil{return nil,err}\n";
  if (options.runtime_xsd && options.validation_scope != "input") { out += "if policy.Validator!=nil{if err=policy.Validator.ValidateXML(data);err!=nil{return nil,err}}\n"; }
  out += "return data,nil\n}\n// Decode" + identifier + "XML returns a fresh value only after complete XML validation.\nfunc Decode" + identifier + "XML(data []byte,policies ...XmlOptions)(*" + identifier + ",error){\npolicy,err:=srlXmlOptions(policies);if err!=nil{return nil,err}\n";
  if (!options.runtime_xsd) { out += "if policy.Validator!=nil{return nil,fmt.Errorf(\"Runtime XSD was not enabled during generation\")}\n"; }
  out += "node,err:=srlXmlParse(data,policy.Limits);if err!=nil{return nil,err}\n";
  if (options.runtime_xsd && options.validation_scope != "output") { out += "if policy.Validator!=nil{if err=policy.Validator.ValidateXML(data);err!=nil{return nil,err}}\n"; }
  out += "if node.name!=(" + qualified(contract.root) + "){return nil,fmt.Errorf(\"Unexpected XML root\")}\nreturn srlXmlDecode" + identifier + "(node)\n}\n";
  out += "// Construct one bounded tree using generated field access without reflection.\nfunc (value *" + identifier + ") srlXmlNode(b *srlXmlBudget)(*srlXmlNode,error){\nif value==nil{return nil,fmt.Errorf(\"Nil XML record\")}\nif b.depth>=b.limits.MaxDepth{return nil,fmt.Errorf(\"XML depth limit exceeded\")}\nb.depth++;defer func(){b.depth--}()\nnode,err:=b.node(" + qualified(contract.root) + ");if err!=nil{return nil,err}\n";
  for (const auto& mapped : contract.fields) {
    const auto& source = *mapped.source;
    const auto& scalar = leaf(source);
    const auto access = "value." + field(source);
    const bool optional = source.type_name_list.front().is_optional();
    const auto min = source.fixed_extent != 0 ? source.fixed_extent : mapped.min_occurs;
    const auto max = source.fixed_extent != 0 ? std::optional<std::size_t>{static_cast<std::size_t>(source.fixed_extent)} : mapped.max_occurs;
    out += "{\n";
    auto expression = access;
    if (optional) { out += "if " + access + "!=nil{\n"; expression = "(*" + access + ")"; }
    if (mapped.repeated) {
      out += "if len(" + access + ")<" + std::to_string(min);
      if (max) { out += "||len(" + access + ")>" + std::to_string(*max); }
      out += "{return nil,fmt.Errorf(\"XML collection cardinality mismatch\")}\nfor _,srlItem:=range " + access + "{\n";
      expression = "srlItem";
    }
    if (scalar.type == object_type::class_type) {
      out += "child,srlErr:=" + expression + ".srlXmlNode(b);if srlErr!=nil{return nil,srlErr}\nchild.name=" + qualified(mapped.qualified_name) + "\nnode.children=append(node.children,child)\n";
    } else {
      out += encode_scalar(scalar, expression, type);
      if (mapped.attribute) {
        out += "if srlErr:=srlXmlAttribute(node," + qualified(mapped.qualified_name) + ",srlText,b);srlErr!=nil{return nil,srlErr}\n";
      } else {
        out += "child,srlErr:=b.node(" + qualified(mapped.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\nif srlErr=b.textValue(srlText);srlErr!=nil{return nil,srlErr}\nchild.text=srlText;node.children=append(node.children,child)\n";
      }
    }
    if (mapped.repeated || optional) { out += "}\n"; }
    out += "}\n";
  }
  out += "return node,nil\n}\n// Decode each statically known field into a private owning candidate.\nfunc srlXmlDecode" + identifier + "(node *srlXmlNode)(*" + identifier + ",error){\nreader,err:=srlXmlReader(node);if err!=nil{return nil,err}\nvalue:=New" + identifier + "()\n";
  for (const auto& mapped : contract.fields) {
    const auto& source = *mapped.source;
    const auto& scalar = leaf(source);
    const auto access = "value." + field(source);
    const bool optional = source.type_name_list.front().is_optional();
    const auto min = source.fixed_extent != 0 ? source.fixed_extent : mapped.min_occurs;
    const auto max = source.fixed_extent != 0 ? std::optional<std::size_t>{static_cast<std::size_t>(source.fixed_extent)} : mapped.max_occurs;
    out += "{\nitems,srlErr:=reader.";
    out += mapped.attribute ? "attribute(" + qualified(mapped.qualified_name) + "," + (min ? "true" : "false") + ")" :
        "children(" + qualified(mapped.qualified_name) + "," + std::to_string(min) + "," + (max ? std::to_string(*max) : "-1") + ")";
    out += "\nif srlErr!=nil{return nil,srlErr}\n";
    if (mapped.repeated && source.fixed_extent == 0) { out += access + "=make([]" + type(scalar) + ",0,len(items))\n"; }
    if (optional) { out += access + "=nil\n"; }
    out += "for ";
    out += mapped.repeated && source.fixed_extent != 0 ? "srlIndex" : "_";
    out += ",srlChild:=range items{\n";
    if (scalar.type == object_type::class_type) {
      out += "srlValue,srlErr:=srlXmlDecode" + record_name(type(scalar)) + "(srlChild);if srlErr!=nil{return nil,srlErr}\n";
    } else {
      out += "srlText,srlErr:=srlXmlText(srlChild);if srlErr!=nil{return nil,srlErr}\n" + decode_scalar(scalar, type);
    }
    out += access + (optional ? "=&srlValue" : mapped.repeated ? source.fixed_extent != 0 ? "[srlIndex]=srlValue" : "=append(" + access + ",srlValue)" : "=srlValue") + "\n}\n}\n";
  }
  return out + "if err=reader.finish();err!=nil{return nil,err}\nreturn value,nil\n}\n";
}
} // namespace rohit::serializer::writer::xml_portable
