// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <string>

namespace rohit::serializer::writer::xml_codegen {
// Borrow native payload/state independently and publish owning decoded pairs only after every stage succeeds.
inline std::string c_package_document_support(const std::string& name, const xml_options& options) {
  const auto prefix = "srl_xml_package_document_" + name;
  const auto validation = options.runtime_xsd ? ",validation" : "";
  const auto known_validation = options.runtime_xsd ? ",NULL" : "";
  std::string result =
      "/* Borrow the original model and XML state without constructing or copying an owning document. */\n"
      "static inline srl_status " + prefix + "_encode(const void* model,const srl_xml_extension_state* state,srl_buffer* result,const srl_limits* limits,const srl_xml_validation* validation,srl_xml_operation* operation){\n"
      "(void)validation;if(!model||!result){return srl_xml_operation_fail(operation,srl_invalid);}if(!srl_xml_operation_preserve(operation,srl_xml_extension_preserved_bytes(state))){return srl_xml_operation_fail(operation,srl_invalid);}const srl_limits bounds=limits?*limits:srl_default_limits();srl_limits transport;"
      "srl_buffer known={0},final={0};const srl_xml_operation_policies* policies=operation?operation->policies:NULL;"
      "srl_status status=srl_xml_extension_output_limits(&bounds,&transport);srl_native_operation native={operation,status==srl_ok?transport.max_bytes:0};\n"
      "if(status==srl_ok){status=srl_xml_operation_encode_" + name + "((const " + name + "*)model,&known,&bounds" + known_validation +
      ",policies?policies->map:NULL,policies?policies->mixed:NULL,policies?policies->digest:NULL,policies?policies->identity:NULL,false,&transport,operation);}\n"
      "if(status==srl_ok){status=srl_extension_operation_srl_xml_extension_replay(known.data,known.size,&bounds,NULL," + name + "_srl_xml_extension_model(),state,&final,&native);}\n"
      "if(status==srl_ok){status=srl_xml_operation_identity_check_" + name + "(final.data,final.size,&bounds,policies?policies->identity:NULL,operation);}\n";
  if (options.runtime_xsd && options.validation_scope != "input") {
    result += "if(status==srl_ok&&validation&&validation->validate){status=validation->validate(final.data,final.size,&bounds,validation->context);}\n";
  }
  result += "srl_buffer_free(&known);if(status==srl_ok&&!srl_xml_operation_check(operation)){status=srl_invalid;}if(status!=srl_ok){srl_buffer_free(&final);return srl_xml_operation_fail(operation,status);}srl_buffer_free(result);*result=final;return srl_ok;}\n"
      "/* Decode original XML and its companion transactionally under the same package operation. */\n"
      "static inline srl_status " + prefix + "_decode(void* model,srl_xml_extension_state** state,const uint8_t* bytes,size_t size,const srl_limits* limits,const srl_xml_validation* validation,srl_xml_operation* operation){\n"
      "(void)validation;if(!model||!state||(size&&!bytes)){return srl_xml_operation_fail(operation,srl_invalid);}const srl_limits bounds=limits?*limits:srl_default_limits();"
      "srl_limits transport;srl_status status=srl_xml_extension_output_limits(&bounds,&transport);srl_native_operation native={operation,status==srl_ok?transport.max_bytes:0};"
      "srl_buffer known={0};srl_xml_extension_state* retained=NULL;" + name + " candidate={0};const srl_xml_operation_policies* policies=operation?operation->policies:NULL;\n"
      "if(status==srl_ok){status=srl_extension_operation_srl_xml_extension_filter(bytes,size,&bounds,NULL," + name + "_srl_xml_extension_model(),&known,&retained,&native);}\n"
      "if(status==srl_ok){status=srl_xml_operation_identity_check_" + name + "(bytes,size,&bounds,policies?policies->identity:NULL,operation);}\n";
  if (options.runtime_xsd && options.validation_scope != "output") {
    result += "if(status==srl_ok&&validation&&validation->validate){status=validation->validate(bytes,size,&bounds,validation->context);}\n";
  }
  result += "if(status==srl_ok){status=srl_xml_extension_transport_limits(known.size,&bounds,&transport);}\n"
      "if(status==srl_ok){status=srl_xml_operation_decode_" + name + "(&candidate,known.data,known.size,&bounds" + known_validation +
      ",policies?policies->map:NULL,policies?policies->mixed:NULL,policies?policies->digest:NULL,policies?policies->identity:NULL,false,&transport,operation,true);}\n"
      "srl_buffer_free(&known);if(status==srl_ok&&!srl_xml_operation_check(operation)){status=srl_invalid;}"
      "if(status!=srl_ok){" + name + "_free(&candidate);srl_xml_extension_state_free(retained);return srl_xml_operation_fail(operation,status);}"
      + name + "_free((" + name + "*)model);*(" + name + "*)model=candidate;srl_xml_extension_state_free(*state);*state=retained;return srl_ok;}\n"
      "/* Capture presentation for a selected decoder without constructing another native payload. */\n"
      "static inline srl_status " + prefix + "_prepare(srl_xml_extension_state** state,const uint8_t* bytes,size_t size,const srl_limits* limits,const srl_xml_validation* validation,srl_xml_operation* operation){\n"
      "(void)validation;if(!state||(size&&!bytes)){return srl_xml_operation_fail(operation,srl_invalid);}const srl_limits bounds=limits?*limits:srl_default_limits();"
      "srl_limits transport;srl_status status=srl_xml_extension_output_limits(&bounds,&transport);srl_native_operation native={operation,status==srl_ok?transport.max_bytes:0};"
      "srl_buffer known={0};srl_xml_extension_state* retained=NULL;const srl_xml_operation_policies* policies=operation?operation->policies:NULL;\n"
      "if(status==srl_ok){status=srl_extension_operation_srl_xml_extension_filter(bytes,size,&bounds,NULL," + name + "_srl_xml_extension_model(),&known,&retained,&native);}\n"
      "if(status==srl_ok){status=srl_xml_operation_identity_check_" + name + "(bytes,size,&bounds,policies?policies->identity:NULL,operation);}\n";
  if (options.runtime_xsd && options.validation_scope != "output") {
    result += "if(status==srl_ok&&validation&&validation->validate){status=validation->validate(bytes,size,&bounds,validation->context);}\n";
  }
  result += "srl_buffer_free(&known);if(status==srl_ok&&!srl_xml_operation_check(operation)){status=srl_invalid;}if(status!=srl_ok){srl_xml_extension_state_free(retained);return srl_xml_operation_fail(operation,status);}srl_xml_extension_state_free(*state);*state=retained;return srl_ok;}\n"
      "/* Immutable descriptor keeps native and XML-only ownership separate. */\n"
      "static inline const srl_xml_package_document_codec* " + prefix + "(void){static const srl_xml_package_document_codec codec={" + prefix + "_encode," + prefix + "_decode," + prefix + "_prepare};return &codec;}\n";
  (void)validation;
  return result;
}
} // namespace rohit::serializer::writer::xml_codegen
