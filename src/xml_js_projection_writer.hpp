// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_js_model_writer.hpp"
#include "xml_js_projection_types.hpp"
#include "translator_writer.hpp"

#include <cstddef>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>

namespace rohit::serializer::writer::xml_binding {

// Export a record handle through the original owning target; a scalar handle needs its containing field's root mapping.
inline std::string js_reference_methods(const class_node& owner, const std::string& generated_name, bool typescript) {
  const bool record = js_projected_leaf(owner.reference->value_type).type == object_type::class_type;
  if (typescript) {
    return "// Borrow the selected owning value; XML input cannot create an external owner binding.\n"
        "encodeXml(limits?:XmlLimits,validator?:XmlValidator):string;\n"
        "static decodeXml(text:string,limits?:XmlLimits,validator?:XmlValidator):never;\n"
        "static decodeXmlChunks(chunks:Iterable<Uint8Array>,limits?:XmlLimits):never;\n" +
        std::string{record ? "xmlBindNamespace(prefix:string,uri:string):this;\n" : ""};
  }
  static_cast<void>(generated_name);
  std::string result = "static _srlXmlToken=Object.freeze({projection:true,fields:Object.freeze([]),mixed:null});\n"
      "// Borrow the original owning target without constructing or copying any of its native values.\n"
      "encodeXml(limits,validator) { ";
  result += record ? "return this.readValue().encodeXml(limits,validator); }\n" :
      "throw new Error('Scalar XML references require their owning field mapping'); }\n";
  if (record) {
    result += "// Keep explicitly authored XML namespace presentation on the selected value.\n"
        "xmlBindNamespace(prefix,uri) { this.readValue().xmlBindNamespace(prefix,uri);return this; }\n"
        "// Borrow one selected child while its original typed fields are written.\n"
        "_srlXmlWrite(output,local,uri,variantType=null) { this.readValue()._srlXmlWrite(output,local,uri,variantType); }\n";
  }
  for (const auto* method : {"decodeXml()", "decodeXmlChunks()", "_srlXmlRead()"}) {
    result += "// Readonly XML projections are export-only; deserialize the owning target instead.\nstatic " +
        std::string{method} + " { throw new Error('Readonly XML projections are export-only; decode the owning target'); }\n";
  }
  return result;
}

// Resolve one target field through the scoped view's own public getter or its existing composed parent default.
inline std::string js_projection_getter(const class_node& target, const member& source,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier, bool revision = false) {
  const auto getter = [&](const std::string& name) {
    member naming{};
    naming.name = "get_" + name;
    naming.access = access_type::public_access;
    return field_name(naming);
  };
  for (const auto& field : target.member_list) {
    if (&field == &source) { return "owner=>owner." + getter(source.name) + "()"; }
  }
  for (std::size_t index{}; index < target.parents.size(); ++index) {
    const auto& parent = *target.parents[index].parent_class;
    if (!js_declaring_record(parent, source)) { continue; }
    type_name parent_type{std::string{parent.name}, nullptr};
    parent_type.type = object_type::class_type;
    parent_type.resolved_node = &parent;
    const auto descriptor = type_identifier(parent_type) + "._srlXmlToken" +
        (revision ? ".model.revision" : ".fields.find(field=>field.native===" +
            native::quote(js_native_field_identity(target, source)) + ')');
    return "owner=>" + descriptor + ".get(owner." + getter("base" + std::to_string(index)) + "())";
  }
  throw std::logic_error{"XML translator target field has no borrowed getter"};
}

// Add scoped XML export methods with cached target metadata and private-safe readonly getter callbacks.
inline std::string js_translator_xml_methods(const class_node& target, const record& contract,
    const std::string& target_name, const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier, const xml_options& options) {
  std::string result = "// Cache XML callbacks only after all generated target classes have initialized.\n"
      "static _srlXmlProjectionMeta() { return srlXmlProjectionContract(this,()=>({target:" + target_name +
      "._srlXmlToken,getters:new Map([";
  for (const auto& field : contract.fields) {
    result += '[' + native::quote(js_native_field_identity(target, *field.source)) + ',' +
        js_projection_getter(target, *field.source, field_name, type_identifier) + "],";
  }
  result += "]),mixed:" + (contract.mixed ? js_projection_getter(target, *contract.mixed->source, field_name, type_identifier) : "null") + ",revision:" + (contract.model.revision ?
      js_projection_getter(target, *contract.model.revision, field_name, type_identifier, true) : "null") + "})); }\n"
      "// Bind XML-only QName presentation on this scoped view without changing its source's native fields.\n"
      "xmlBindNamespace(prefix,uri) { return srlXmlBindNamespace(this,prefix,uri); }\n"
      "// Borrow target getter values directly; no owning target or nested value copy is constructed.\n"
      "encodeXml(limits=new XmlLimits(),validator=null) { limits=new XmlLimits(limits);const meta=this.constructor._srlXmlProjectionMeta();"
      "const output=new SrlXmlWriter(limits);const split=meta.root.indexOf(String.fromCharCode(0));"
      "srlXmlTokenWrite(this,meta,output,meta.root.slice(split+1),meta.root.slice(0,split));";
  if (!options.runtime_xsd) { result += "if(validator)throw new Error('Runtime XSD was not enabled during generation');"; }
  else if (options.validation_scope != "input") { result += "if(validator)validator.validate(output.text);"; }
  return result + "return output.text; }\n";
}

} // namespace rohit::serializer::writer::xml_binding
