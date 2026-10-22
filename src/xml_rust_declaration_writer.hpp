// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_native_declaration_writer.hpp"

#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_codegen {
// Validate unpublished native DTOs using the immutable inventory and the caller's existing ledger.
inline std::string rust_declaration_dispatch(const native::schema& model,
    const xml_binding::schema& bindings, std::string_view operation_module) {
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const std::string api{operation_module};
  std::string result;
  std::set<const class_node*> validators;
  for (const auto& inventory : model.xml_inventories) {
    for (const auto& [source, index] : inventory->validator_indices) {
      (void)index;
      validators.insert(source);
    }
  }
  const auto decoder = [&](const xml_binding::declaration_inventory& inventory, std::size_t index) {
    return "srl_xml_declaration_decode_" + native::snake(model.names.at(&declaration_validator(inventory, index)));
  };
  for (const auto* source : validators) {
    const auto& contract = bindings.bind(*source);
    const auto name = model.names.at(source);
    const auto graph = native_identity_graph(*source, contract, bindings);
    result += "/// Decode one private declaration without publishing native values or opaque presentation.\n"
        "fn srl_xml_declaration_decode_" + native::snake(name) +
        "(text:&str,limits:XmlLimits,operation:Option<" + api + "::State<'_>>)->Result<(),Error>{\n";
    result += "use " + api + "::{SrlXmlIdentityDiagnosticNode,SrlXmlIdentityDiagnosticConstraint,SrlXmlIdentityDiagnosticScalar};\n"
        "const GRAPH:&[SrlXmlIdentityNode]=" + rust_identity_graph(graph, quote) + ";\n"
        "const DIAGNOSTICS:&[SrlXmlIdentityDiagnosticNode]=" + rust_identity_diagnostics(graph, quote) + ";\n"
        "let policies=operation.as_ref().map(" + api + "::State::policies).unwrap_or_default();\n"
        "srl_xml_identity_validate_with_operation(text,limits,policies.identity,GRAPH,operation.as_ref().map(" +
        api + "::State::secondary),DIAGNOSTICS)?;\n";
    if (native_extension_graph(*source, bindings)) {
      result += "let (known,_)=srl_xml_extension_filter_validation(text,limits,&XmlExtensionOptions::default()," +
          name + "::srl_xml_extension_model(),operation.as_ref().map(" + api + "::State::secondary))?;let selected=known.as_str();\n";
    } else {
      result += "let selected=text;\n";
    }
    result += "let transport=srl_xml_extension_transport_limits(selected,limits)?;let mut input=SrlXmlInput::new_with_operation(selected,transport,operation.as_ref().map(" +
        api + "::State::secondary))?;input.map_limits=policies.map;input.mixed_limits=policies.mixed;input.digest_limits=policies.digest;let _candidate=" + name + "::srl_xml_read_tokens(&mut input," +
        quote(contract.root.namespace_uri) + ',' + quote(contract.root.local) +
        ")?;input.finish_document()?;Ok(())}\n";
  }
  for (const auto* node : model.nodes) {
    if (node->type != object_type::class_type) { continue; }
    const auto& owner = static_cast<const class_node&>(*node);
    // Unmapped mixed-fragment storage has no standalone XML contract or declaration dispatcher.
    const auto* inventory = model.inventory_for(&owner);
    if (!inventory || !validates_declarations(bindings.bind(owner))) { continue; }
    result += "/// Dispatch expanded names against the original pinned declaration inventory.\nfn " +
        native_declaration_dispatch_name(owner) + "(uri:&str,local:&str,value:Option<&str>,scope:&[(String,String)],text:&str,limits:XmlLimits,operation:Option<" +
        api + "::State<'_>>)->Result<bool,Error>{let _=(uri,local,value,scope,text,limits,&operation);\n";
    for (const auto& global : inventory->globals) {
      result += "if value.is_" + std::string{global.attribute ? "some" : "none"} + "()&&uri==" +
          quote(global.qualified_name.namespace_uri) + "&&local==" + quote(global.qualified_name.local) + "{\n";
      if (global.abstract_element) {
        result += "return Err(Error(\"Abstract XML declaration cannot appear directly\"));}\n";
        continue;
      }
      if (!global.validator_index) { throw std::logic_error{"Missing Rust declaration validator"}; }
      if (global.attribute) {
        const auto& root = inventory->validators.at(*global.validator_index).root;
        result += "let prepared=srl_xml_declaration_attribute(" + quote(root.namespace_uri) + ',' +
            quote(root.local) + ",uri,local,value.unwrap(),scope,limits,operation.as_ref().map(" + api +
            "::State::secondary))?;" + decoder(*inventory, *global.validator_index) +
            "(&prepared,limits,operation)?;return Ok(true);}\n";
        continue;
      }
      const std::string mutable_selection = global.type_validators.empty() ? "" : "mut ";
      result += "type Decoder=for<'a> fn(&str,XmlLimits,Option<" + api + "::State<'a>>)->Result<(),Error>;"
          "let " + mutable_selection + "selected:Decoder=" + decoder(*inventory, *global.validator_index) + ";let " + mutable_selection + "nil_selected:Option<Decoder>=" +
          (global.nil_validator_index ? "Some(" + decoder(*inventory, *global.nil_validator_index) + ')' : "None") +
          ";let " + mutable_selection + "abstract_type=" + (global.abstract_type ? "true" : "false") +
          ";let actual=srl_xml_declaration_type(text,limits,operation.as_ref().map(" + api + "::State::secondary))?;\n";
      if (global.declared_type) {
        result += "if let Some((type_uri,type_local))=actual.as_ref(){if type_uri==" +
            quote(global.declared_type->namespace_uri) + "&&type_local==" + quote(global.declared_type->local) + "{}";
        for (const auto& alternative : global.type_validators) {
          result += "else if type_uri==" + quote(alternative.qualified_name.namespace_uri) + "&&type_local==" +
              quote(alternative.qualified_name.local) + "{selected=" + decoder(*inventory, alternative.validator_index) +
              ";nil_selected=" + (alternative.nil_validator_index ? "Some(" + decoder(*inventory, *alternative.nil_validator_index) + ')' : "None") +
              ";abstract_type=" + (alternative.abstract_type ? "true" : "false") + ";}";
        }
        result += "else{return Err(Error(\"XML type is not an admitted derivation\"));}}\n";
      } else {
        result += "if actual.is_some(){return Err(Error(\"Anonymous XML type cannot be named\"));}\n";
      }
      result += "if abstract_type{return Err(Error(\"Abstract XML type cannot appear directly\"));}"
          "let expected=actual.as_ref().map(|(uri,local)|(uri.as_str(),local.as_str()));"
          "let(prepared,nil)=srl_xml_declaration_element(text,limits," + std::string{global.nillable ? "true" : "false"} + ',' +
          (global.fixed_value ? "true" : "false") + ",expected,operation.as_ref().map(" + api + "::State::secondary))?;"
          "if nil{nil_selected.ok_or(Error(\"XML nil validator missing\"))?(&prepared,limits,operation)?;}else{selected(&prepared,limits,operation)?;}return Ok(true);}\n";
    }
    result += "Ok(false)}\n";
  }
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
