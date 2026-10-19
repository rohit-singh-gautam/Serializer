// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_portable_writer.hpp"
#include <array>
#include <cstddef>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace rohit::serializer::writer::xml_portable {
// Resolve only a live handle's selected payload, preserving native optional and container storage.
inline const type_name& projection_payload(const type_name& value) {
  const auto* selected = &value;
  std::set<const class_node*> active{};
  while (selected->type == object_type::class_type && selected->resolved_node) {
    const auto& owner = static_cast<const class_node&>(*selected->resolved_node);
    if (!owner.reference || !owner.reference->value_projection) { break; }
    if (!active.insert(&owner).second) { throw std::invalid_argument{"Cyclic readonly XML value projection"}; }
    selected = &owner.reference->value_type;
  }
  return *selected;
}

// Recognize a native live handle without replacing it with an owning target or copied payload.
inline bool value_projection(const type_name& value) {
  return value.type == object_type::class_type && value.resolved_node &&
      static_cast<const class_node&>(*value.resolved_node).reference &&
      static_cast<const class_node&>(*value.resolved_node).reference->value_projection;
}

// Delegate complete record exports while keeping every standalone scalar export and XML input precise.
inline std::string managed_reference_methods(const class_node& owner, const std::string& name,
    const scalar_codec& scalar, std::string_view language) {
  const bool java = language == "java";
  const auto& payload = owner.reference->value_type;
  if (payload.is_optional() || payload.is_sequence() || payload.is_associative()) {
    throw std::invalid_argument{"Readonly XML selected optional or collection payloads require explicit selected-value traversal"};
  }
  const bool record = projection_payload(payload).type == object_type::class_type;
  const std::string encode = java ? "encodeXml" : "EncodeXml", decode = java ? "decodeXml" : "DecodeXml";
  const std::string read = java ? "readValue()" : "ReadValue()";
  const std::string schema = java ? "XmlSchema" : "XmlSchema?";
  const std::array<std::pair<std::string, std::string>, 7> policies{{
      {"", ""}, {"Limits limits," + schema + " schema", "limits,schema"},
      {"Limits limits," + schema + " schema,XmlMapLimits maps", "limits,schema,maps"},
      {"Limits limits," + schema + " schema,XmlMapLimits maps,XmlMixedLimits mixed", "limits,schema,maps,mixed"},
      {"Limits limits," + schema + " schema,XmlMapLimits maps,XmlMixedLimits mixed,XmlIdentityLimits identity", "limits,schema,maps,mixed,identity"},
      {"Limits limits," + schema + " schema,XmlMapLimits maps,XmlMixedLimits mixed,XmlIdentityLimits identity,XmlDigestLimits digests", "limits,schema,maps,mixed,identity,digests"},
      {"Limits limits," + schema + " schema,XmlMapLimits maps,XmlMixedLimits mixed,XmlIdentityLimits identity,XmlDigestLimits digests,XmlExtensionOptions extensions", "limits,schema,maps,mixed,identity,digests,extensions"}}};
  std::string result{};
  for (const auto& [declaration, arguments] : policies) {
    result += java ? "/** Borrow a live record for XML export without constructing its target. */\n" :
        "// Borrow a live record for XML export without constructing its target.\n";
    result += "public byte[] " + encode + '(' + declaration + ") { ";
    result += record ? "var selected=" + read + ";if(selected==null)throw new " +
        (java ? "IllegalArgumentException" : "System.FormatException") +
        "(\"Null XML selected record\");return selected." + encode + '(' + arguments + "); }\n" :
        std::string{"throw new "} + (java ? "IllegalArgumentException" : "System.FormatException") +
        "(\"Scalar XML references require their owning field mapping\"); }\n";
    result += java ? "/** Readonly XML handles cannot synthesize an external owner on input. */\n" :
        "// Readonly XML handles cannot synthesize an external owner on input.\n";
    result += "public static " + name + ' ' + decode + "(byte[] bytes" +
        (declaration.empty() ? "" : ',' + declaration) + ") { throw new " +
        (java ? "IllegalArgumentException" : "System.FormatException") +
        "(\"Readonly XML projections are export-only; decode the owning target\"); }\n";
  }
  result += java ? "/** Traverse one live selected value beneath its containing field tag. */\nprivate void xmlWrite(SrlXmlOutput output,String name) { xmlWrite(output,name,null); }\n" :
      "// Traverse one live selected value beneath its containing field tag.\ninternal void XmlWrite(SrlXmlOutput output,string name) { XmlWrite(output,name,null); }\n";
  result += java ? "/** Forward typed payload metadata while keeping the selected owner alive. */\nprivate void xmlWrite(SrlXmlOutput output,String name,String typeIdentity) { " :
      "// Forward typed payload metadata while keeping the selected owner alive.\ninternal void XmlWrite(SrlXmlOutput output,string name,string? typeIdentity) { ";
  if (record) {
    result += "var selected=" + read + ";if(selected==null)throw new " +
        (java ? "IllegalArgumentException" : "System.FormatException") +
        "(\"Null XML selected record\");selected" + (java ? ".xmlWrite(output,name,typeIdentity); }\n" : ".XmlWrite(output,name,typeIdentity); }\n");
  }
  else {
    result += java ? "output.open(name);if(typeIdentity!=null)output.typeName(typeIdentity);output.text(" :
        "output.Open(name);if(typeIdentity!=null)output.TypeName(typeIdentity);output.Text(";
    result += scalar(payload, read, true) + (java ? ");output.close(); }\n" : ");output.Close(); }\n");
  }
  result += java ? "/** Refuse nested readonly XML input before allocating any candidate. */\nprivate static " :
      "// Refuse nested readonly XML input before allocating any candidate.\ninternal static ";
  result += name + (java ? " xmlRead(SrlXmlInput input,String name) { throw new IllegalArgumentException" :
      " XmlRead(SrlXmlInput input,string name) { throw new System.FormatException") +
      "(\"Readonly XML projections are export-only; decode the owning target\"); }\n";
  return result;
}

// Borrow a live Go payload synchronously; native owner validation and error behavior remain unchanged.
inline std::string go_reference_methods(const class_node& owner, const std::string& name,
    const std::function<std::string(const type_name&, const std::string&)>& scalar) {
  const auto& payload = owner.reference->value_type;
  if (payload.is_optional() || payload.is_sequence() || payload.is_associative()) {
    throw std::invalid_argument{"Readonly XML selected optional or collection payloads require explicit selected-value traversal"};
  }
  const bool record = projection_payload(payload).type == object_type::class_type;
  const std::array<std::pair<std::string, std::string>, 7> policies{{
      {"", ""}, {"WithMapLimits", "maps XmlMapLimits,"}, {"WithMixedLimits", "mixed XmlMixedLimits,"},
      {"WithContentLimits", "maps XmlMapLimits,mixed XmlMixedLimits,"},
      {"WithIdentityLimits", "identity XmlIdentityLimits,"},
      {"WithPolicies", "maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,"},
      {"WithDigestLimits", "digests XmlDigestLimits,"}}};
  const std::array<std::string, 7> arguments{{"DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits()",
      "maps,DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits()",
      "DefaultXmlMapLimits(),mixed,DefaultXmlIdentityLimits(),DefaultXmlDigestLimits()",
      "maps,mixed,DefaultXmlIdentityLimits(),DefaultXmlDigestLimits()",
      "DefaultXmlMapLimits(),DefaultXmlMixedLimits(),identity,DefaultXmlDigestLimits()",
      "maps,mixed,identity,DefaultXmlDigestLimits()",
      "DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),digests"}};
  std::string result{};
  for (std::size_t index{}; index < policies.size(); ++index) {
    const auto& [suffix, declarations] = policies[index];
    result += "// Export live selected record storage using existing independent policies.\nfunc (value *" + name + ") EncodeXML" + suffix + '(' + declarations +
        "policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithAllPolicies(" + arguments[index] + ",policies...)}\n";
    result += "// Readonly handles cannot create or recover external ownership from XML.\nfunc Decode" + name + "XML" + suffix + "(data []byte," + declarations +
        "policies ...XmlOptions)(*" + name + ",error){return nil,fmt.Errorf(\"Readonly XML projections are export-only; decode the owning target\")}\n";
  }
  result += "// Borrow the selected target only for this complete export operation.\nfunc (value *" + name + ") EncodeXMLWithAllPolicies(maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,digests XmlDigestLimits,policies ...XmlOptions)([]byte,error){";
  result += record ? "selected,err:=value.ReadValue();if err!=nil{return nil,err};return selected.EncodeXMLWithAllPolicies(maps,mixed,identity,digests,policies...)}\n" :
      "return nil,fmt.Errorf(\"Scalar XML references require their owning field mapping\")}\n";
  result += "// Refuse complete readonly input before provider invocation or native allocation.\nfunc Decode" + name + "XMLWithAllPolicies(data []byte,maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,digests XmlDigestLimits,policies ...XmlOptions)(*" + name +
      ",error){return nil,fmt.Errorf(\"Readonly XML projections are export-only; decode the owning target\")}\n";
  result += "// Forward explicit opaque policies to the original live selected record.\nfunc (value *"+name+") EncodeXMLWithAllBounds(maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,digests XmlDigestLimits,extensions XmlExtensionOptions,policies ...XmlOptions)([]byte,error){";
  result += record ? "selected,err:=value.ReadValue();if err!=nil{return nil,err};return selected.EncodeXMLWithAllBounds(maps,mixed,identity,digests,extensions,policies...)}\n" :
      "return nil,fmt.Errorf(\"Scalar XML references require their owning field mapping\")}\n";
  result += "// Select extension vocabulary understanding independently from earlier policies.\nfunc(value *"+name+") EncodeXMLWithExtensionOptions(extensions XmlExtensionOptions,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithAllBounds(DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits(),extensions,policies...)}\n";
  result += "// Keep readonly graph input closed before provider or native candidate construction.\nfunc Decode"+name+"XMLWithAllBounds(data []byte,maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,digests XmlDigestLimits,extensions XmlExtensionOptions,policies ...XmlOptions)(*"+name+",error){return nil,fmt.Errorf(\"Readonly XML projections are export-only; decode the owning target\")}\n";
  result += "// Refuse explicit extension input for a graph with borrowed ownership.\nfunc Decode"+name+"XMLWithExtensionOptions(data []byte,extensions XmlExtensionOptions,policies ...XmlOptions)(*"+name+",error){return nil,fmt.Errorf(\"Readonly XML projections are export-only; decode the owning target\")}\n";
  result += "// Traverse the original selected payload beneath a known owning field tag.\nfunc (value *" + name + ") srlXmlEncode(output *srlXmlOutput,name srlxml.Name,types ...srlxml.Name)error{selected,err:=value.ReadValue();if err!=nil{return err};";
  result += record ? "return selected.srlXmlEncode(output,name,types...)}\n" :
      "if len(types)>1{return fmt.Errorf(\"Ambiguous XML variant type\")};if err=output.open(name);err!=nil{return err};if len(types)==1{if err=output.typeName(types[0]);err!=nil{return err}};\n" +
      scalar(payload, "selected") + "if err=output.text(srlText);err!=nil{return err};return output.close()}\n";
  result += "// Refuse nested readonly input without constructing any external-owner candidate.\nfunc srlXmlDecode" + name + "(input *srlXmlInput,name srlxml.Name)(*" + name +
      ",error){return nil,fmt.Errorf(\"Readonly XML projections are export-only; decode the owning target\")}\n";
  return result;
}
} // namespace rohit::serializer::writer::xml_portable
