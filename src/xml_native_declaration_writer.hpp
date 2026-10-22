// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_native_extension_writer.hpp"
#include "xml_native_identity_writer.hpp"

#include <algorithm>
#include <set>
#include <optional>
#include <stdexcept>
#include <string>

namespace rohit::serializer::writer::xml_codegen {
// Select a retained validator AST without exposing inventory storage ownership to target runtimes.
inline const class_node& declaration_validator(const xml_binding::declaration_inventory& inventory,
    std::size_t index) {
  const auto selected = std::find_if(inventory.validator_indices.begin(), inventory.validator_indices.end(),
      [index](const auto& value) { return value.second == index; });
  if (selected == inventory.validator_indices.end()) { throw std::logic_error{"Missing private XML declaration codec"}; }
  return *selected->first;
}

// Dispatchers exist only where processContents requests semantic declaration checks.
inline bool validates_declarations(const xml_binding::record& contract) {
  return contract.extensions && std::any_of(contract.extensions->slots.begin(), contract.extensions->slots.end(),
      [](const auto& slot) { return slot.process_contents != xml::wildcard_process_contents::skip; });
}

// Decode and discard private C DTOs after all original declaration, scope and resource checks.
inline std::string c_declaration_dispatch(const native::schema& model, const xml_binding::schema& bindings,
    bool prototypes) {
  std::string result;
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const bool operations = !model.packages.empty();
  std::set<const class_node*> validators;
  for (const auto& inventory : model.xml_inventories) {
    for (const auto& [source, index] : inventory->validator_indices) { (void)index; validators.insert(source); }
  }
  for (const auto* source : validators) {
    const auto name = native::snake(model.names.at(source));
    const auto helper = "srl_xml_declaration_decode_" + name;
    const auto signature = "static inline srl_status " + helper +
        "(const uint8_t* bytes,size_t size,const srl_limits* limits,srl_xml_operation* operation)";
    if (prototypes) { result += signature + ";\n"; continue; }
    result += "/* Discard private presentation while preserving the original operation and complete declaration checks. */\n" + signature + "{\n";
    result += "srl_buffer known={0};srl_xml_extension_state* extensions=NULL;srl_limits transport;"
        "const srl_xml_operation_policies* policies=operation?operation->policies:NULL;"
        "const uint8_t* selected=bytes;size_t selected_size=size;srl_status status=srl_ok;" + name + " candidate={0};\n";
    if (native_extension_graph(*source, bindings)) {
      if (operations) {
        result += "srl_limits framing;status=srl_xml_extension_output_limits(limits,&framing);srl_native_operation nested={operation,status==srl_ok?framing.max_bytes:0};"
            "if(status==srl_ok){status=srl_extension_operation_srl_xml_extension_filter_validation(bytes,size,limits," + name +
            "_srl_xml_extension_model(),&known,&extensions,&nested);}";
      } else {
        result += "status=srl_xml_extension_filter_validation(bytes,size,limits," + name + "_srl_xml_extension_model(),&known,&extensions);";
      }
      result += "if(status==srl_ok){status=srl_xml_operation_identity_check_" + name + "(bytes,size,limits,policies?policies->identity:NULL,operation);}"
          "selected=known.data;selected_size=known.size;";
    }
    result += "if(status==srl_ok){status=srl_xml_extension_transport_limits(selected_size,limits,&transport);}"
        "if(status==srl_ok){status=srl_xml_operation_decode_" + name + "(&candidate,selected,selected_size,limits" +
        (model.xml.runtime_xsd ? ",NULL" : "") +
        ",policies?policies->map:NULL,policies?policies->mixed:NULL,policies?policies->digest:NULL,policies?policies->identity:NULL," +
        (native_extension_graph(*source, bindings) ? "false" : "true") + ",&transport,operation,false);}"
        + name + "_free(&candidate);srl_buffer_free(&known);srl_xml_extension_state_free(extensions);return status;}\n";
  }
  for (const auto* node : model.nodes) {
    if (node->type != object_type::class_type) { continue; }
    const auto& source = static_cast<const class_node&>(*node);
    // Unmapped mixed-fragment storage has no standalone XML contract or declaration dispatcher.
    const auto* inventory = model.inventory_for(&source);
    if (!inventory || !validates_declarations(bindings.bind(source))) { continue; }
    const auto helper = "srl_xml_declaration_dispatch_" + native::snake(model.names.at(node));
    const auto signature = "static inline srl_status " + helper +
        "(const char* uri,const char* local,const char* value,const srl_ext_scope* scope,const uint8_t* bytes,size_t size,const srl_limits* limits,srl_xml_operation* operation,bool* found)";
    if (prototypes) { result += signature + ";\n"; continue; }
    result += "/* Resolve only immutable declarations and legal derived types from the original pinned bundle. */\n" + signature +
        "{*found=false;(void)uri;(void)local;(void)value;(void)scope;(void)bytes;(void)size;(void)limits;(void)operation;\n";
    for (const auto& global : inventory->globals) {
      result += "if(" + std::string{global.attribute ? "value" : "!value"} + "&&!strcmp(uri," +
          quote(global.qualified_name.namespace_uri) + ")&&!strcmp(local," + quote(global.qualified_name.local) + ")){*found=true;";
      if (global.abstract_element) { result += "return srl_invalid;}\n"; continue; }
      if (!global.validator_index) { throw std::logic_error{"Missing C declaration validator"}; }
      result += "if(!value&&!bytes){return srl_ok;}srl_buffer prepared={0};bool nil=false;srl_status status;"
          "const srl_xml_package_contract* previous=operation?operation->package_contract:NULL;const char* previous_field=operation?operation->field:NULL;"
          "if(operation){operation->package_contract=NULL;}";
      if (operations) { result += "srl_native_operation nested={operation,limits->max_bytes};"; }
      const auto prefix = operations ? "srl_extension_operation_" : "";
      if (global.attribute) {
        const auto& root = inventory->validators.at(*global.validator_index).root;
        result += "(void)nil;status=" + std::string{prefix} + "srl_ext_declaration_attribute(" + quote(root.namespace_uri) + ',' +
            quote(root.local) + ",uri,local,value,scope,limits->max_bytes,&prepared" +
            (operations ? ",&nested" : "") + ");";
      } else {
        result += "static const srl_xml_declaration_type types[]={ {" +
            (global.declared_type ? quote(global.declared_type->namespace_uri) : "NULL") + ',' +
            (global.declared_type ? quote(global.declared_type->local) : "NULL") + ',' +
            (global.abstract_type ? "true" : "false") + "},";
        for (const auto& type : global.type_validators) {
          result += '{' + quote(type.qualified_name.namespace_uri) + ',' + quote(type.qualified_name.local) + ',' +
              (type.abstract_type ? "true" : "false") + "},";
        }
        result += "};size_t selected=0;status=" + std::string{prefix} +
            "srl_ext_declaration_type(bytes,size,limits,types,sizeof(types)/sizeof(types[0]),&selected" +
            (operations ? ",&nested" : "") + ");if(status==srl_ok){status=" + std::string{prefix} +
            "srl_ext_declaration_element(bytes,size,limits," + (global.nillable ? "true" : "false") + ',' +
            (global.fixed_value ? "true" : "false") + ",types[selected].uri,types[selected].local,&prepared,&nil" +
            (operations ? ",&nested" : "") + ");}";
      }
      // Every selected type retains its complete owning codec; nil uses that type's attributes-only codec.
      const auto decode = [&](std::size_t index, std::optional<std::size_t> nil_index) {
        std::string call;
        if (nil_index) {
          call += "if(nil){status=srl_xml_declaration_decode_" + native::snake(model.names.at(
              &declaration_validator(*inventory, *nil_index))) + "(prepared.data,prepared.size,limits,operation);}else{";
        }
        call += "status=srl_xml_declaration_decode_" + native::snake(model.names.at(
            &declaration_validator(*inventory, index))) + "(prepared.data,prepared.size,limits,operation);";
        if (nil_index) { call += '}'; }
        return call;
      };
      result += "if(status==srl_ok){";
      if (global.attribute) { result += decode(*global.validator_index, {}); }
      else {
        result += "switch(selected){case 0:" + decode(*global.validator_index, global.nil_validator_index) + "break;";
        for (std::size_t index{}; index<global.type_validators.size(); ++index) {
          const auto& type = global.type_validators[index];
          result += "case " + std::to_string(index+1U) + ':' + decode(type.validator_index, type.nil_validator_index) + "break;";
        }
        result += "default:status=srl_invalid;break;}";
      }
      result += "}if(operation){operation->package_contract=previous;if(status==srl_ok){operation->field=previous_field;}}srl_buffer_free(&prepared);return status;}\n";
    }
    result += "return srl_ok;}\n";
  }
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
