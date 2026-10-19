// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <string>

namespace rohit::serializer::writer::xml_codegen {
// Keep foreign state outside native records and validate the original/final XML around the private known codec.
inline std::string rust_extension_frames(const xml_options& options) {
  std::string result =
      "/// Decode an owning model and its positioned opaque sidecar after complete original XML admission.\n"
      "pub fn decode_xml_document(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>,options:&XmlExtensionOptions)->Result<XmlDocument<Self>,Error> {\n";
  if (!options.runtime_xsd) {
    result += "if validator.is_some(){return Err(Error(\"Runtime XSD not enabled during generation\"));}\n";
  }
  result += "let (known,extensions)=srl_xml_extension_filter(text,limits,options,Self::srl_xml_extension_model())?;\n"
      "Self::srl_xml_identity_check(text,limits,XmlIdentityLimits::default())?;\n";
  if (options.runtime_xsd && options.validation_scope != "output") {
    result += "if let Some(schema)=validator {schema.validate(text)?;}\n";
  } else { result += "let _=validator;\n"; }
  result += "let transport=srl_xml_extension_transport_limits(&known,limits)?;\n"
      "let value=Self::srl_xml_decode_known(&known,transport,None,XmlMapLimits::default(),XmlMixedLimits::default(),XmlDigestLimits::default(),XmlIdentityLimits::default(),false)?;\n"
      "Ok(XmlDocument{value,extensions}) }\n"
      "/// Replay a borrowed sidecar by field/item positions; publish only after final XML/identity/XSD checks.\n"
      "pub fn encode_xml_with_extensions(&self,extensions:&XmlExtensionState,limits:XmlLimits,validator:Option<&dyn XmlValidator>,options:&XmlExtensionOptions)->Result<String,Error> {\n";
  if (!options.runtime_xsd) {
    result += "if validator.is_some(){return Err(Error(\"Runtime XSD not enabled during generation\"));}\n";
  }
  result += "let transport=srl_xml_extension_output_limits(limits)?;\n"
      "let known=self.srl_xml_encode_known(limits,None,XmlMapLimits::default(),XmlMixedLimits::default(),XmlDigestLimits::default(),XmlIdentityLimits::default(),false,Some(transport))?;\n"
      "let text=srl_xml_extension_replay(&known,limits,options,Self::srl_xml_extension_model(),extensions)?;\n"
      "Self::srl_xml_identity_check(&text,limits,XmlIdentityLimits::default())?;\n";
  if (options.runtime_xsd && options.validation_scope != "input") {
    result += "if let Some(schema)=validator {schema.validate(&text)?;}\n";
  } else { result += "let _=validator;\n"; }
  return result + "Ok(text) }\n";
}

// Generate initialized C ownership companions; failed decode/encode preserves both caller outputs exactly.
inline std::string c_extension_frames(const std::string& name, const xml_options& options) {
  const auto validation = options.runtime_xsd ? ",const srl_xml_validation* validation" : "";
  const auto known_validation = options.runtime_xsd ? ",NULL" : "";
  std::string result =
      "/* Own native fields and opaque extension bytes separately; native codecs serialize only value. */\n"
      "typedef struct " + name + "_xml_document {" + name + " value;srl_xml_extension_state* extensions;} " + name + "_xml_document;\n"
      "/* Initialize a document before its first decode; zero state is safe to release after failure. */\n"
      "static inline srl_status " + name + "_xml_document_init(" + name + "_xml_document* value) {if(!value){return srl_invalid;}memset(value,0,sizeof(*value));return " + name + "_init(&value->value);}\n"
      "/* Release native and sidecar storage independently; repeated release is safe. */\n"
      "static inline void " + name + "_xml_document_free(" + name + "_xml_document* value) {if(!value){return;}" + name + "_free(&value->value);srl_xml_extension_state_free(value->extensions);memset(value,0,sizeof(*value));}\n"
      "/* Decode original admitted XML before constructing the native candidate or replacing the initialized document. */\n"
      "static inline srl_status " + name + "_decode_xml_document(" + name + "_xml_document* result,const uint8_t* bytes,size_t size,const srl_limits* limits" + validation + ",const srl_xml_extension_options* options) {\n"
      "if(!result||(size&&!bytes)){return srl_invalid;}const srl_limits bounds=limits?*limits:srl_default_limits();\n"
      "srl_buffer known={0};" + name + "_xml_document candidate={0};srl_limits transport;\n"
      "srl_status status=srl_xml_extension_filter(bytes,size,&bounds,options," + name + "_srl_xml_extension_model(),&known,&candidate.extensions);\n"
      "if(status==srl_ok){status=" + name + "_srl_xml_identity_check(bytes,size,&bounds,NULL);}\n";
  if (options.runtime_xsd && options.validation_scope != "output") {
    result += "if(status==srl_ok&&validation&&validation->validate){status=validation->validate(bytes,size,&bounds,validation->context);}\n";
  } else if (options.runtime_xsd) { result += "(void)validation;\n"; }
  result += "if(status==srl_ok){status=srl_xml_extension_transport_limits(known.size,&bounds,&transport);}\n"
      "if(status==srl_ok){status=" + name + "_srl_xml_decode_known(&candidate.value,known.data,known.size,&bounds" + known_validation + ",NULL,NULL,NULL,NULL,false,&transport);}\n"
      "srl_buffer_free(&known);if(status!=srl_ok){" + name + "_xml_document_free(&candidate);return status;}" + name + "_xml_document_free(result);*result=candidate;return srl_ok;}\n"
      "/* Replay immutable sidecar state, then validate every final byte before replacing output. */\n"
      "static inline srl_status " + name + "_encode_xml_document(const " + name + "_xml_document* value,srl_buffer* result,const srl_limits* limits" + validation + ",const srl_xml_extension_options* options) {\n"
      "if(!value||!result){return srl_invalid;}const srl_limits bounds=limits?*limits:srl_default_limits();srl_buffer known={0},final={0};srl_limits transport;\n"
      "srl_status status=srl_xml_extension_output_limits(&bounds,&transport);\n"
      "if(status==srl_ok){status=" + name + "_srl_xml_encode_known(&value->value,&known,&bounds" + known_validation + ",NULL,NULL,NULL,NULL,false,&transport);}\n"
      "if(status==srl_ok){status=srl_xml_extension_replay(known.data,known.size,&bounds,options," + name + "_srl_xml_extension_model(),value->extensions,&final);}\n"
      "if(status==srl_ok){status=" + name + "_srl_xml_identity_check(final.data,final.size,&bounds,NULL);}\n";
  if (options.runtime_xsd && options.validation_scope != "input") {
    result += "if(status==srl_ok&&validation&&validation->validate){status=validation->validate(final.data,final.size,&bounds,validation->context);}\n";
  } else if (options.runtime_xsd) { result += "(void)validation;\n"; }
  return result + "srl_buffer_free(&known);if(status!=srl_ok){srl_buffer_free(&final);return status;}srl_buffer_free(result);*result=final;return srl_ok;}\n";
}
} // namespace rohit::serializer::writer::xml_codegen
