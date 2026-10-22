// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "package_writer.hpp"
#include "xml_schema.hpp"
#include "native_package_contract.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::package_codegen {
// Emit explicit C ownership and statically selected role codecs without runtime model reflection.
inline std::string c(const package_node& package, const std::string& name,
    const std::string& helper, const xml_options& xml,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal, bool binary_none_only = false) {
  const auto configured_package = configure_package(package, xml);
  const auto parts = ordered(configured_package);
  const auto profile_contract = compile_package_profile(package, xml);
  std::set<std::string> fields{"extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  std::map<std::string, std::size_t> role_indices;
  std::map<const package_part_declaration*, std::string> companion_fields{};
  for (std::size_t index{}; index < parts.size(); ++index) {
    const auto* part = parts[index];
    if (binary_none_only && part->value_type && part->codec == "json") { throw std::invalid_argument{"JSON package profile requires the JSON native protocol"}; }
    require_native_package_graph(*part, xml, "C", true);
    role_indices.emplace(part->name, index);
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"C package role/member collision: " + part->name};
    }
    if (!fields.insert(render_field(part->name) + (singleton(*part) ? "_path" : "_size")).second) {
      throw std::invalid_argument{"C package metadata member collision: " + part->name};
    }
    if (singleton(*part) && !fields.insert(render_field(part->name) + "_link").second) {
      throw std::invalid_argument{"C package diagnostic member collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated C package roles require dynamic names and relationship IDs"};
    }
  }
  for (const auto* part : parts) {
    if (!native_package_companion_required(*part, xml)) { continue; }
    auto field = "_srl_xml_" + render_field(part->name);
    while (fields.contains(field)) { field += "_2"; }
    fields.insert(field); companion_fields.emplace(part, std::move(field));
  }
  const auto companion = [&](const package_part_declaration* part) { return companion_fields.contains(part); };
  const auto document_codec = [&](const package_part_declaration* part) {
    return companion(part) ? "srl_xml_package_document_" + render_type(*part->value_type) + "()" : "NULL";
  };
  std::string output;
  const auto line = [&](const std::string& text) { output += text + '\n'; };
  const auto api = [&](std::string_view suffix) { return helper + "_" + std::string{suffix}; };
  const auto selected_api = helper + '_' + name;
  const auto role = [&](std::size_t index) { return "&pipeline_profiles[" + std::to_string(index) + "].role"; };
  const auto profile = [&](std::size_t index) { return "&pipeline_profiles[" + std::to_string(index) + "]"; };
  const auto contracts = "srl_package_pipeline_" + name + "_contracts";
  const auto profile_count = std::to_string(parts.size());
  const auto profile_extent = std::to_string(std::max<std::size_t>(parts.size(), 1U));
  const auto optional = [&](const auto& value) { return value ? literal(*value) : "NULL"; };
  line("/* Serializer package facade: " + package.get_full_name() + " " + name + " */");
  line("/* Generated C selected-codec prefix: " + selected_api + " */");
  for (const auto* part : parts) {
    if (!part->value_type || singleton(*part)) { continue; }
    const auto member = render_field(part->name);
    line("/* Own one typed occurrence and its discovered source-routing metadata. */");
    line("typedef struct " + name + "_" + member + "_part {" + render_type(*part->value_type) +
        " value; char* name; char* content_type; size_t source_index; char* link;" +
        (companion(part) ? "srl_xml_extension_state* xml_state;" : "") + "} " + name + "_" + member + "_part;");
  }
  for (const auto input : {true, false}) {
    const auto owner = name + (input ? "_input" : "_value");
    line(std::string{input ? "/* Borrow model and resource ownership for one synchronous operation. */" :
                            "/* Own a complete transactional aggregate; zero initialization is valid. */"});
    line("typedef struct " + owner + " {");
    for (const auto* part : parts) {
      const auto member = render_field(part->name);
      if (singleton(*part)) {
        line("  " + std::string{input ? "const " : ""} + render_type(*part->value_type) + "* " + member + "; " +
            (input ? "const " : "") + "char* " + member + "_path; " +
            (input ? "const " : "") + "char* " + member + "_link;");
      } else {
        const auto item = part->value_type ? name + "_" + member + "_part" : api("binary_part");
        line("  " + std::string{input ? "const " : ""} + item + "* " + member + "; size_t " + member + "_size;");
      }
    }
    line("  " + std::string{input ? "const " : ""} + api("extensions") + (input ? "* extensions;" : " extensions;"));
    for (const auto* part : parts) {
      if (singleton(*part) && companion(part)) {
        line("  " + std::string{input ? "const " : ""} + "srl_xml_extension_state* " + companion_fields.at(part) + ";");
      }
    }
    line("} " + owner + ";");
  }
  line("/* Release only resources owned by an initialized decoded aggregate. */");
  line("static inline void " + name + "_free(" + name + "_value* value) {");
  for (const auto* part : parts) {
    const auto member = render_field(part->name);
    if (singleton(*part)) {
      line("  if(value->" + member + "){ " + render_type(*part->value_type) + "_free(value->" + member + "); free(value->" + member + "); } free(value->" + member + "_path); free(value->" + member + "_link);");
      if (companion(part)) { line("  srl_xml_extension_state_free(value->" + companion_fields.at(part) + ");"); }
    } else {
      line("  for(size_t index=0;index<value->" + member + "_size;++index){");
      if (part->value_type) {
        line("    " + render_type(*part->value_type) + "_free(&value->" + member + "[index].value); free(value->" + member + "[index].name);free(value->" + member + "[index].content_type);free(value->" + member + "[index].link);");
        if (companion(part)) { line("    srl_xml_extension_state_free(value->" + member + "[index].xml_state);"); }
      } else {
        line("    " + api("binary_part_free") + "(&value->" + member + "[index]);");
      }
      line("  } free(value->" + member + ");");
    }
  }
  line("  " + api("extensions_free") + "(&value->extensions);memset(value,0,sizeof(*value));\n}");
  line("/* Borrow a loaded aggregate for edit/save without changing its owning lifecycle. */");
  line("static inline " + name + "_input " + name + "_as_input(const " + name + "_value* value){ " + name + "_input input={0};");
  for (const auto* part : parts) {
    const auto member = render_field(part->name);
    line("  input." + member + "=value->" + member + "; input." + member +
        (singleton(*part) ? "_path=value->" + member + "_path;" : "_size=value->" + member + "_size;"));
    if (singleton(*part)) { line("  input." + member + "_link=value->" + member + "_link;"); }
    if (singleton(*part) && companion(part)) { line("  input." + companion_fields.at(part) + "=value->" + companion_fields.at(part) + ";"); }
  }
  line("  input.extensions=&value->extensions;return input;\n}");
  line("/* Immutable generated model/envelope descriptors never select a decoder by a prefix. */");
  line("static const " + api("role") + " " + name + "_profiles[]={");
  for (const auto* part : parts) {
    std::string expected = "NULL,NULL,NULL,NULL";
    if (part->value_type) {
      const auto& owner = static_cast<const class_node&>(*part->value_type->resolved_node);
      const auto contract = xml_binding::schema{xml}.bind(owner);
      expected = literal(contract.root.namespace_uri) + "," + literal(contract.root.local) + "," +
          literal("urn:serializer:native:" + (owner.parent_namespace ? get_full_name_for_namespace(owner.parent_namespace) : std::string{})) + "," + literal(owner.name);
      // XML definitions follow the C native model definitions in the existing generation pipeline.
    }
    line("  {" + literal(part->name) + "," + optional(part->part_name) + "," + optional(part->content_type) + "," +
        (part->source == "/" ? "SIZE_MAX" : std::to_string(role_indices.at(part->source))) + "," +
        literal(part->relationship) + "," + optional(part->relationship_id) + "," +
        (part->compression == "deflate" ? "8" : "0") + "," + std::to_string(part->min_occurs) + "," +
        (part->max_occurs ? std::to_string(*part->max_occurs) : "SIZE_MAX") + "," +
        (part->profile_policy == "fixed" ? "true" : "false") + "," + expected + "},");
  }
  line("};");
  std::set<std::string> native_adapters{};
  for (const auto* part : parts) {
    if (!part->value_type) { continue; }
    const auto native = render_type(*part->value_type);
    if (!native_adapters.insert(native).second) { continue; }
    const auto prefix = "srl_package_native_" + native;
    line("#ifndef "+prefix+"_included\n#define "+prefix+"_included");
    line("/* Traverse the borrowed native payload under separately owned operation admission. */");
    line("static inline srl_status "+prefix+"_encode(const void* model,srl_buffer* result,srl_protocol protocol,const srl_limits* limits,srl_xml_operation* admission){if(!model||!result||(protocol!=srl_json&&protocol!=srl_binary_none)){return srl_invalid;}const srl_limits bounds=limits?*limits:srl_default_limits();srl_native_operation operation={admission,bounds.max_bytes};srl_writer output={0};output.protocol=protocol;srl_native_operation_"+native+"_srl_write(&output,(const "+native+"*)model,&operation);srl_status status=output.status;if(status==srl_ok&&!srl_xml_operation_check(admission)){status=srl_invalid;}if(status==srl_ok){srl_buffer_free(result);*result=output.buffer;memset(&output.buffer,0,sizeof(output.buffer));}srl_buffer_free(&output.buffer);return srl_xml_operation_fail(admission,status);}");
    line("/* Decode one complete private native candidate before replacing initialized model ownership. */");
    line("static inline srl_status "+prefix+"_decode(void* model,const uint8_t* bytes,size_t size,srl_protocol protocol,const srl_limits* limits,srl_xml_operation* admission){if(!model||(size&&!bytes)||(protocol!=srl_json&&protocol!=srl_binary_none)){return srl_invalid;}const srl_limits bounds=limits?*limits:srl_default_limits();if(size>bounds.max_bytes||bounds.read_policy>srl_read_flexible){return srl_xml_operation_fail(admission,size>bounds.max_bytes?srl_limit:srl_invalid);}srl_native_operation operation={admission,bounds.max_bytes};"+native+" candidate={0};srl_status status=srl_xml_operation_init_"+native+"(&candidate,admission);if(status==srl_ok){srl_reader input={0};input.data=bytes;input.size=size;input.protocol=protocol;input.limits=bounds;srl_native_operation_"+native+"_srl_read(&input,&candidate,&operation);srl_finish_with_operation(&input,&operation);status=input.status;}if(status==srl_ok&&!srl_xml_operation_check(admission)){status=srl_invalid;}if(status==srl_ok){"+native+"_free(("+native+"*)model);*("+native+"*)model=candidate;memset(&candidate,0,sizeof(candidate));}"+native+"_free(&candidate);return srl_xml_operation_fail(admission,status);}");
    line("/* Preserve selected-decoder default bindings with the existing operation-aware initializer. */");
    line("static inline srl_status "+prefix+"_initialize(void* model,srl_xml_operation* operation){return srl_xml_operation_init_"+native+"(("+native+"*)model,operation);}");
    line("static const srl_native_operation_codec "+prefix+"={"+prefix+"_encode,"+prefix+"_decode,"+prefix+"_initialize};");
    line("#endif");
  }
  for (std::size_t index{}; index < parts.size(); ++index) {
    const auto metadata = std::find_if(profile_contract.parts.begin(), profile_contract.parts.end(), [&](const auto& item) { return item.role == parts[index]->name; });
    if (!metadata->decoder || metadata->decoder->vocabularies.empty()) { continue; }
    std::string values{};
    for (const auto& uri : metadata->decoder->vocabularies) { values += literal(uri) + ','; }
    line("static const char* const "+contracts+"_vocabularies_"+std::to_string(index)+"[]={"+values+"};");
  }
  line("static const "+api("pipeline_contract")+' '+contracts+"[]={");
  for (std::size_t index{}; index < parts.size(); ++index) {
    const auto* part = parts[index];
    const auto metadata = std::find_if(profile_contract.parts.begin(), profile_contract.parts.end(), [&](const auto& item) { return item.role == part->name; });
    const auto vocabulary = metadata->decoder && !metadata->decoder->vocabularies.empty() ? contracts+"_vocabularies_"+std::to_string(index) : "NULL";
    line("{&"+name+"_profiles["+std::to_string(index)+"],"+api("payload_"+(part->value_type?part->codec:std::string{"bytes"}))+","+api("target_"+part->target_mode)+",{"+(part->requires_type?"true":"false")+","+vocabulary+","+(metadata->decoder?std::to_string(metadata->decoder->vocabularies.size()):"0")+"},"+(binary_none_only?"false":"true")+"},");
  }
  line("};");
  line("/* Collect schema-declared keys and references against the whole candidate before publication. */");
  line("static inline int " + name + "_check_references(const " + name + "_input* value," + api("budget") + "* budget){ " + api("key_index") + " keys={0}; (void)value;");
  const auto model_index = [&](const package_part_declaration& part) {
    const auto found = std::find_if(parts.begin(), parts.end(), [&](const auto* candidate) {
      return candidate->value_type && candidate->value_type->resolved_node == part.value_type->resolved_node;
    });
    return std::to_string(std::distance(parts.begin(), found));
  };
  for (const auto* part : parts) {
    if (part->keys.empty() && part->references.empty()) { continue; }
    const auto member = render_field(part->name);
    if (singleton(*part)) {
      line("  if(value->" + member + "){ const " + render_type(*part->value_type) + "* model=value->" + member + ";const char* path=value->" + member + "_path?value->" + member + "_path:" + literal(*part->part_name) + ";");
    } else {
      line("  for(size_t index=0;index<value->" + member + "_size;++index){ const " + render_type(*part->value_type) + "* model=&value->" + member + "[index].value;const char* path=value->" + member + "[index].name;");
    }
    const auto link = singleton(*part) ? "value->" + member + "_link" : "value->" + member + "[index].link";
    line("    const char* link=" + link + "?" + link + ":" + literal(part->relationship_id.value_or(part->name)) + ";");
    const auto selector = [&](std::string_view scope, std::string_view field_name,
                              const package_part_declaration& target, bool reference) {
      const auto& selected = selector_member(*part, field_name);
      const auto& scalar = selected.type_name_list.front().name;
      const auto native_field = "model->" + render_field(field_name);
      line("    {");
      if (scalar == "string") {
        line("      const char* bytes=" + native_field + ".data; const size_t size=" + native_field + ".size;");
      } else {
        const auto is_unsigned = scalar.starts_with("uint");
        line("      char bytes[32];const int written=snprintf(bytes,sizeof(bytes),\"%\" " +
            std::string{is_unsigned ? "PRIu64" : "PRId64"} + ",(" +
            (is_unsigned ? "uint64_t" : "int64_t") + ")" + native_field + ");const size_t size=written>0?(size_t)written:0;");
      }
      line("      if(" + api("key_add") + "(&keys," + literal(scope) + "," + model_index(target) + ",bytes,size," +
          (reference ? "true" : "false") + ",path," + literal(field_name) + ",link,budget)){goto done;}\n    }");
    };
    for (const auto& key : part->keys) { selector(key.scope, key.field, *part, false); }
    for (const auto& reference : part->references) {
      selector(reference.scope, reference.field, *parts[role_indices.at(reference.target_role)], true);
    }
    line("  }");
  }
  line("  " + api("key_finish") + "(&keys,budget);goto done;done:" + api("key_index_free") + "(&keys);return budget->status;\n}");
  line("/* Assemble a complete archive privately; failed encoding retains the initialized output buffer. */");
  line("static inline int " + selected_api + "_encode_pipeline_mode(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size,bool canonical,const " + api("part_rename") + "* renames,size_t renames_size,const "+api("pipeline_override")+"* overrides,size_t overrides_size,const "+api("pipeline_profile")+"* resolved){ " + api("composer") + " composer={0};srl_buffer bytes={0};srl_limits limits=" + api("native_limits") + "(budget);" + api("relocation") + "* dynamic_relocations=NULL;const " + api("relocation") + "* selected_relocations=NULL;size_t selected_relocations_size=0;"+api("pipeline_profile")+" local_profiles["+profile_extent+"]={0};const "+api("pipeline_profile")+"* pipeline_profiles=resolved?resolved:local_profiles;");
  line("  if(!value||!result){return " + api("fail") + "(budget," + api("invalid_options") + ",NULL,NULL,NULL);}");
  line("  if((!resolved&&"+api("resolve_pipeline")+"("+name+"_profiles,"+contracts+","+profile_count+",overrides,overrides_size,local_profiles,budget))||"+api("validate_codecs")+"(codecs,codecs_size,"+name+"_profiles,"+profile_count+",budget)||"+api("pipeline_codecs")+"(pipeline_profiles,"+profile_count+",codecs,codecs_size,budget)){return budget->status;}");
  line("  " + name + "_input routed=*value;" + api("relocation") + " relocations[" + std::to_string(std::max<std::size_t>(parts.size(), 1U)) + "]={0};size_t relocation_count=0;");
  line("  if(canonical){");
  for (const auto* part : parts) {
    if (!singleton(*part)) { continue; }
    const auto member = render_field(part->name);
    line("    if(value->" + member + "_path&&strcmp(value->" + member + "_path," + literal(*part->part_name) + ")){if(" + api("consume") + "(budget," + api("metadata_bytes") + ",sizeof(relocations[0]),value->" + member + "_path,\"relocations\",value->" + member + "_link)){return budget->status;}relocations[relocation_count++]=(" + api("relocation") + "){value->" + member + "_path," + literal(*part->part_name) + ",value->" + member + "!=NULL};}routed." + member + "_path=" + literal(*part->part_name) + ";");
  }
  line("    value=&routed;if(" + api("relocations_check") + "(relocations,relocation_count,budget)){return budget->status;}\n  }");
  for (const auto* part : parts) {
    if (singleton(*part)) { continue; }
    const auto member = render_field(part->name);
    line("  if(value->" + member + "_size&&!value->" + member + "){return " + api("fail") + "(budget," + api("invalid_options") + ",NULL," + literal(part->name) + ",NULL);}");
  }
  line("  selected_relocations=relocations;selected_relocations_size=relocation_count;if(renames_size){if(!renames||renames_size>SIZE_MAX/sizeof(*dynamic_relocations)-relocation_count){return " + api("fail") + "(budget," + api("invalid_options") + ",NULL,\"relocations\",NULL);}const size_t bytes_size=(renames_size+relocation_count)*sizeof(*dynamic_relocations);if(" + api("consume") + "(budget," + api("allocation_bytes") + ",bytes_size,NULL,\"relocations\",NULL)){return budget->status;}dynamic_relocations=(" + api("relocation") + "*)calloc(renames_size+relocation_count,sizeof(*dynamic_relocations));if(!dynamic_relocations){return " + api("fail") + "(budget," + api("allocation") + ",NULL,\"relocations\",NULL);}memcpy(dynamic_relocations,relocations,relocation_count*sizeof(*relocations));");
  line("    for(size_t rename_index=0;rename_index<renames_size;++rename_index){const " + api("part_rename") + "* rename=&renames[rename_index];bool known=false;bool present=false;if(!rename->role||!rename->original||!rename->destination){" + api("fail") + "(budget," + api("invalid_options") + ",NULL,\"relocations\",NULL);goto done;}");
  for (const auto* part : parts) {
    if (part->part_name) { continue; }
    const auto member = render_field(part->name);
      line("      if(!strcmp(rename->role," + literal(part->name) + ")){known=true;for(size_t occurrence=0;occurrence<value->" + member + "_size;++occurrence){"+std::string{part->value_type?"":"if(value->"+member+"[occurrence].content_type&&!*value->"+member+"[occurrence].content_type){continue;}"}+"const char* item_name=value->" + member + "[occurrence].name;if(item_name&&!" + api("compare") + "(item_name,rename->destination)){present=true;}}}");
  }
  line("      if(!known){" + api("fail") + "(budget," + api("invalid_options") + ",rename->original,\"relocations\",NULL);goto done;}dynamic_relocations[relocation_count+rename_index]=(" + api("relocation") + "){rename->original,rename->destination,present};}selected_relocations=dynamic_relocations;selected_relocations_size=relocation_count+renames_size;if(" + api("relocations_check") + "(selected_relocations,selected_relocations_size,budget)){goto done;}} ");
  line("  if(" + api("validate_codecs") + "(codecs,codecs_size," + name + "_profiles,sizeof(" + name + "_profiles)/sizeof(" + name + "_profiles[0]),budget)||" + api("profiles") + "(" + name + "_profiles,sizeof(" + name + "_profiles)/sizeof(" + name + "_profiles[0]),budget)||" + name + "_check_references(value,budget)||" + api("composer_init") + "(&composer," + std::to_string(parts.size()) + ",budget)){goto done;}");
  for (std::size_t index{}; index < parts.size(); ++index) {
    const auto* part = parts[index];
    const auto member = render_field(part->name), selected = role(index);
    const auto count = singleton(*part) ? "value->" + member + "?1:0" : "value->" + member + "_size";
    if (!singleton(*part)) {
      line("  if(value->" + member + "_size&&!value->" + member + "){ " + api("fail") + "(budget," + api("invalid_options") + ",NULL," + literal(part->name) + ",NULL);goto done;}");
    }
    line("  if(" + api("occurs") + "(" + selected + "," + count + ",budget)){goto done;}");
    if (singleton(*part)) {
      line("  if(value->" + member + "){const char* path=value->" + member + "_path?value->" + member + "_path:" + literal(*part->part_name) + ";");
      line("    const char* link=value->" + member + "_link?value->" + member + "_link:" + literal(part->relationship_id.value_or(part->name)) + ";");
      line("    if(" + api("encode_pipeline") + "(codecs,codecs_size," + profile(index) + ",srl_xml_operation_codec_" + render_type(*part->value_type) + "(),&srl_package_native_"+render_type(*part->value_type)+","+document_codec(part)+","+(companion(part)?"value->"+companion_fields.at(part):"NULL")+",value->" + member + ",&bytes,&limits,path,link,budget)||" + api("typed_pipeline") + "(" + profile(index) + ",bytes.data,bytes.size,path,link,budget)||" + api("compose_pipeline") + "(&composer," + profile(index) + "," + std::to_string(index) + ",path,NULL,0,&bytes)){goto done;}");
    } else {
      line("  for(size_t index=0;index<value->" + member + "_size;++index){const char* path=value->" + member + "[index].name;");
      if (part->value_type) {
        line("    const char* link=value->" + member + "[index].link?value->" + member + "[index].link:" + literal(part->relationship_id.value_or(part->name)) + ";");
        line("    if(" + api("encode_pipeline") + "(codecs,codecs_size," + profile(index) + ",srl_xml_operation_codec_" + render_type(*part->value_type) + "(),&srl_package_native_"+render_type(*part->value_type)+","+document_codec(part)+","+(companion(part)?"value->"+member+"[index].xml_state":"NULL")+",&value->" + member + "[index].value,&bytes,&limits,path,link,budget)||" + api("typed_pipeline") + "(" + profile(index) + ",bytes.data,bytes.size,path,link,budget)){goto done;}");
      } else {
        line("    if(" + api("append") + "(budget,&bytes,value->" + member + "[index].bytes.data,value->" + member + "[index].bytes.size,budget->max_part_bytes)){goto done;}");
      }
      line("    if(" + api("compose_pipeline") + "(&composer," + profile(index) + "," + std::to_string(index) + ",path,value->" + member + "[index].content_type,value->" + member + "[index].source_index,&bytes)){goto done;}");
    }
    line("  }");
  }
  line("  if(canonical||renames_size){" + api("compose_finish_relocated") + "(&composer,value->extensions,selected_relocations,selected_relocations_size,result);}else{" + api("compose_finish") + "(&composer,value->extensions,result);}goto done;done:free(dynamic_relocations);srl_buffer_free(&bytes);" + api("composer_free") + "(&composer);(void)limits;return budget->status;\n}");
  line("/* Preserve the complete original mode entry point and public aggregate layouts. */");
  line("static inline int "+selected_api+"_encode_mode(const "+name+"_input* value,srl_buffer* result,"+api("budget")+"* budget,const "+api("selected_codec")+"* codecs,size_t codecs_size,bool canonical,const "+api("part_rename")+"* renames,size_t renames_size){return "+selected_api+"_encode_pipeline_mode(value,result,budget,codecs,codecs_size,canonical,renames,renames_size,NULL,0,NULL);}");
  line("/* Resolve native/XML codec and storage envelopes without replacing bound model identities. */");
  line("static inline int "+selected_api+"_encode_with_profiles(const "+name+"_input* value,srl_buffer* result,"+api("budget")+"* budget,const "+api("pipeline_override")+"* profiles,size_t count){return "+selected_api+"_encode_pipeline_mode(value,result,budget,NULL,0,false,NULL,0,profiles,count,NULL);}");
  line("/* Compose selected envelopes and explicit occurrence moves through the same borrowed traversal. */");
  line("static inline int "+selected_api+"_encode_renamed_with_profiles(const "+name+"_input* value,srl_buffer* result,"+api("budget")+"* budget,const "+api("part_rename")+"* renames,size_t renames_size,const "+api("pipeline_override")+"* profiles,size_t count){return "+selected_api+"_encode_pipeline_mode(value,result,budget,NULL,0,true,renames,renames_size,profiles,count,NULL);}");
  line("/* Preserve ordinary discovered paths and existing selected-codec entrypoint signatures. */");
  line("static inline int " + selected_api + "_encode_with_codecs(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size){return " + selected_api + "_encode_mode(value,result,budget,codecs,codecs_size,false,NULL,0);}");
  line("/* Canonicalize only fixed singleton paths while borrowing every typed payload unchanged. */");
  line("static inline int " + selected_api + "_encode_canonical_with_codecs(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size){return " + selected_api + "_encode_mode(value,result,budget,codecs,codecs_size,true,NULL,0);}");
  line("/* Apply explicit dynamic occurrence identities without changing legacy input or extension layouts. */");
  line("static inline int " + selected_api + "_encode_renamed(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget,const " + api("part_rename") + "* renames,size_t renames_size){return " + selected_api + "_encode_mode(value,result,budget,NULL,0,true,renames,renames_size);}");
  line("/* Select native codecs for an additive canonical-path archive operation. */");
  line("static inline int " + selected_api + "_encode_canonical(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget){return " + selected_api + "_encode_canonical_with_codecs(value,result,budget,NULL,0);}");
  line("/* Preserve the original generated encoder with default operation-aware native codecs. */");
  line("static inline int " + name + "_encode(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget){return " + selected_api + "_encode_with_codecs(value,result,budget,NULL,0);}");
  line("/* Decode into a private aggregate; failure leaves the initialized destination unchanged. */");
  line("static inline int " + selected_api + "_load_pipeline_mode(" + name + "_value* result,const uint8_t* bytes,size_t size," + api("policy") + " policy," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size,const "+api("pipeline_override")+"* overrides,size_t overrides_size,const "+api("pipeline_profile")+"* resolved){ " + name + "_value fresh={0};" + api("loader") + " loader={0};" + api("selection") + "* selected=NULL;size_t count=0;srl_buffer payload={0};srl_limits limits=" + api("native_limits") + "(budget);"+api("pipeline_profile")+" local_profiles["+profile_extent+"]={0};const "+api("pipeline_profile")+"* pipeline_profiles=resolved?resolved:local_profiles;");
  line("  if((!resolved&&"+api("resolve_pipeline")+"("+name+"_profiles,"+contracts+","+profile_count+",overrides,overrides_size,local_profiles,budget))||"+api("validate_codecs")+"(codecs,codecs_size,"+name+"_profiles,"+profile_count+",budget)||"+api("pipeline_codecs")+"(pipeline_profiles,"+profile_count+",codecs,codecs_size,budget)){return budget->status;}");
  line("  if(!result){return " + api("fail") + "(budget," + api("invalid_options") + ",NULL,NULL,NULL);}if(" + api("validate_codecs") + "(codecs,codecs_size," + name + "_profiles,sizeof(" + name + "_profiles)/sizeof(" + name + "_profiles[0]),budget)||" + api("profiles") + "(" + name + "_profiles,sizeof(" + name + "_profiles)/sizeof(" + name + "_profiles[0]),budget)||" + api("loader_init") + "(&loader,bytes,size," + std::to_string(parts.size()) + ",budget)){goto done;}");
  for (std::size_t index{}; index < parts.size(); ++index) {
    const auto* part = parts[index];
    const auto member = render_field(part->name), metadata = role(index);
    line("  if(" + api("select_mode") + "(&loader," + metadata + "," + std::to_string(index) + ",(unsigned)pipeline_profiles["+std::to_string(index)+"].contract->mode,&selected,&count)){goto done;}");
    if (!singleton(*part)) {
      line("  if(count){if(count>SIZE_MAX/sizeof(*fresh." + member + ")||" + api("consume") + "(budget," + api("allocation_bytes") + ",count*sizeof(*fresh." + member + "),NULL," + literal(part->name) + ",NULL)){ " + api("fail") + "(budget," + api("resource_limit") + ",NULL," + literal(part->name) + ",NULL);goto done;}fresh." + member + "=(" + (part->value_type ? name + "_" + member + "_part" : api("binary_part")) + "*)calloc(count,sizeof(*fresh." + member + "));if(!fresh." + member + "){ " + api("fail") + "(budget," + api("allocation") + ",NULL," + literal(part->name) + ",NULL);goto done;}fresh." + member + "_size=count;}");
    }
    line("  for(size_t index=0;index<count;++index){const " + api("relationship") + "* link=&loader.catalog.links[selected[index].link];");
    if (!part->value_type) {
      line("    if(selected[index].part==SIZE_MAX){fresh."+member+"[index].name="+api("string_at")+"(budget,link->target,strlen(link->target),link->source,"+literal(part->name)+",link->id);fresh."+member+"[index].content_type="+api("string_at")+"(budget,\"\",0,link->source,"+literal(part->name)+",link->id);fresh."+member+"[index].source_index=selected[index].source_index;fresh."+member+"[index].method=pipeline_profiles["+std::to_string(index)+"].role.method;if(!fresh."+member+"[index].name||!fresh."+member+"[index].content_type){goto done;}continue;}");
    }
    line("    const " + api("catalog_part") + "* part=&loader.catalog.parts[selected[index].part];if(" + api("archive_read") + "(&loader.archive,part->entry,&payload,budget)){goto done;}");
    if (part->value_type) {
      const auto native = render_type(*part->value_type);
      const auto value = singleton(*part) ? "fresh." + member : "&fresh." + member + "[index].value";
      if (singleton(*part)) {
        line("    if(" + api("consume") + "(budget," + api("allocation_bytes") + ",sizeof(*fresh." + member + "),part->value.name," + literal(part->name) + ",link->id)){goto done;}fresh." + member + "=(" + native + "*)calloc(1,sizeof(*fresh." + member + "));if(!fresh." + member + "){ " + api("fail") + "(budget," + api("allocation") + ",part->value.name," + literal(part->name) + ",link->id);goto done;}");
      }
      line("    if(" + api("typed_pipeline") + "(" + profile(index) + ",payload.data,payload.size,part->value.name,link->id,budget)||" + api("decode_pipeline") + "(codecs,codecs_size," + profile(index) + ",srl_xml_operation_codec_" + native + "(),&srl_package_native_"+native+","+document_codec(part)+","+(companion(part)?(singleton(*part)?"&fresh."+companion_fields.at(part):"&fresh."+member+"[index].xml_state"):"NULL")+"," + value + ",payload.data,payload.size,&limits,part->value.name,link->id,budget)){goto done;}");
      if (singleton(*part)) {
        line("    fresh." + member + "_path=" + api("string_at") + "(budget,part->value.name,strlen(part->value.name),part->value.name," + literal(part->name) + ",link->id);fresh." + member + "_link=" + api("string_at") + "(budget,link->id,strlen(link->id),part->value.name," + literal(part->name) + ",link->id);if(!fresh." + member + "_path||!fresh." + member + "_link){goto done;}");
      } else {
        line("    fresh." + member + "[index].name=" + api("string_at") + "(budget,part->value.name,strlen(part->value.name),part->value.name," + literal(part->name) + ",link->id);fresh." + member + "[index].content_type=" + api("string_at") + "(budget,part->value.content_type,strlen(part->value.content_type),part->value.name," + literal(part->name) + ",link->id);fresh." + member + "[index].link=" + api("string_at") + "(budget,link->id,strlen(link->id),part->value.name," + literal(part->name) + ",link->id);fresh." + member + "[index].source_index=selected[index].source_index;if(!fresh." + member + "[index].name||!fresh." + member + "[index].content_type||!fresh." + member + "[index].link){goto done;}");
      }
    } else {
      line("    fresh." + member + "[index].name=" + api("string") + "(budget,part->value.name,strlen(part->value.name));fresh." + member + "[index].content_type=" + api("string") + "(budget,part->value.content_type,strlen(part->value.content_type));fresh." + member + "[index].source_index=selected[index].source_index;fresh." + member + "[index].method=part->value.method;fresh." + member + "[index].bytes=payload;memset(&payload,0,sizeof(payload));if(!fresh." + member + "[index].name||!fresh." + member + "[index].content_type){goto done;}(void)link;");
    }
    line("    srl_buffer_free(&payload);\n  }free(selected);selected=NULL;count=0;");
  }
  line("  if(" + api("load_finish") + "(&loader,policy,&fresh.extensions)){goto done;} {const " + name + "_input input=" + name + "_as_input(&fresh);if(" + name + "_check_references(&input,budget)){goto done;}} " + name + "_free(result);*result=fresh;memset(&fresh,0,sizeof(fresh));goto done;done:srl_buffer_free(&payload);free(selected);" + api("loader_free") + "(&loader);" + name + "_free(&fresh);(void)limits;return budget->status;\n}");
  line("/* Preserve the complete selected-decoder signature with generated envelope defaults. */");
  line("static inline int "+selected_api+"_load_with_codecs("+name+"_value* result,const uint8_t* bytes,size_t size,"+api("policy")+" policy,"+api("budget")+"* budget,const "+api("selected_codec")+"* codecs,size_t codecs_size){return "+selected_api+"_load_pipeline_mode(result,bytes,size,policy,budget,codecs,codecs_size,NULL,0,NULL);}");
  line("/* Select native/XML envelopes while retaining transactional aggregate replacement. */");
  line("static inline int "+selected_api+"_load_with_profiles("+name+"_value* result,const uint8_t* bytes,size_t size,"+api("policy")+" policy,"+api("budget")+"* budget,const "+api("pipeline_override")+"* profiles,size_t count){return "+selected_api+"_load_pipeline_mode(result,bytes,size,policy,budget,NULL,0,profiles,count,NULL);}");
  line("/* Snapshot the selected envelope once and revalidate it before atomic filesystem publication. */");
  line("static inline int "+selected_api+"_save_with_profiles(const char* path,const "+name+"_input* value,"+api("budget")+"* budget,"+api("fault")+" fault,void* context,const "+api("pipeline_override")+"* profiles,size_t count){"+api("pipeline_profile")+" resolved["+profile_extent+"]={0};if("+api("resolve_pipeline")+"("+name+"_profiles,"+contracts+","+profile_count+",profiles,count,resolved,budget)){return budget->status;}srl_buffer bytes={0};"+name+"_value candidate={0};int status="+selected_api+"_encode_pipeline_mode(value,&bytes,budget,NULL,0,false,NULL,0,NULL,0,resolved);if(!status){status="+selected_api+"_load_pipeline_mode(&candidate,bytes.data,bytes.size,"+api("preserve_policy")+"(),budget,NULL,0,NULL,0,resolved);}if(!status){status="+api("save_file")+"(path,bytes.data,bytes.size,budget,fault,context);}"+name+"_free(&candidate);srl_buffer_free(&bytes);return status;}");
  line("/* Revalidate the selected envelope after all explicit moves before atomic publication. */");
  line("static inline int "+selected_api+"_save_renamed_with_profiles(const char* path,const "+name+"_input* value,"+api("budget")+"* budget,const "+api("part_rename")+"* renames,size_t renames_size,const "+api("pipeline_override")+"* profiles,size_t count,"+api("fault")+" fault,void* context){"+api("pipeline_profile")+" resolved["+profile_extent+"]={0};if("+api("resolve_pipeline")+"("+name+"_profiles,"+contracts+","+profile_count+",profiles,count,resolved,budget)){return budget->status;}srl_buffer bytes={0};"+name+"_value candidate={0};int status="+selected_api+"_encode_pipeline_mode(value,&bytes,budget,NULL,0,true,renames,renames_size,NULL,0,resolved);if(!status){status="+selected_api+"_load_pipeline_mode(&candidate,bytes.data,bytes.size,"+api("preserve_policy")+"(),budget,NULL,0,NULL,0,resolved);}if(!status){status="+api("save_file")+"(path,bytes.data,bytes.size,budget,fault,context);}"+name+"_free(&candidate);srl_buffer_free(&bytes);return status;}");
  line("/* Preserve the original generated loader with default operation-aware native codecs. */");
  line("static inline int " + name + "_load(" + name + "_value* result,const uint8_t* bytes,size_t size," + api("policy") + " policy," + api("budget") + "* budget){return " + selected_api + "_load_with_codecs(result,bytes,size,policy,budget,NULL,0);}");
  line("/* Bounded private memory spooling preserves source ownership and original callback statuses. */");
  line("static inline int " + name + "_read(" + name + "_value* result," + api("source") + " source,void* context," + api("policy") + " policy," + api("budget") + "* budget){srl_buffer bytes={0};int status=" + api("read_source") + "(source,context,&bytes,budget);if(!status){status=" + name + "_load(result,bytes.data,bytes.size,policy,budget);}srl_buffer_free(&bytes);return status;}");
  line("/* Prepare a complete archive before borrowing a sink; a transport failure can retain a prefix. */");
  line("static inline int " + name + "_write(const " + name + "_input* value," + api("sink") + " sink,void* context," + api("budget") + "* budget){srl_buffer bytes={0};int status=" + name + "_encode(value,&bytes,budget);if(!status){status=" + api("write_sink") + "(sink,context,bytes.data,bytes.size,budget);}srl_buffer_free(&bytes);return status;}");
  line("/* Validate the complete generated candidate before atomically replacing a destination path. */");
  line("static inline int " + name + "_save(const char* path,const " + name + "_input* value," + api("budget") + "* budget," + api("fault") + " fault,void* context){srl_buffer bytes={0};" + name + "_value candidate={0};int status=" + name + "_encode(value,&bytes,budget);if(!status){status=" + name + "_load(&candidate,bytes.data,bytes.size," + api("preserve_policy") + "(),budget);}if(!status){status=" + api("save_file") + "(path,bytes.data,bytes.size,budget,fault,context);}" + name + "_free(&candidate);srl_buffer_free(&bytes);return status;}");
  line("/* Read with caller-selected synchronous codecs and the same cumulative source/XML ledger. */");
  line("static inline int " + selected_api + "_read_with_codecs(" + name + "_value* result," + api("source") + " source,void* context," + api("policy") + " policy," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size){srl_buffer bytes={0};int status=" + api("read_source") + "(source,context,&bytes,budget);if(!status){status=" + selected_api + "_load_with_codecs(result,bytes.data,bytes.size,policy,budget,codecs,codecs_size);}srl_buffer_free(&bytes);return status;}");
  line("/* Prepare all selected codecs before publishing any bytes to the borrowed sink. */");
  line("static inline int " + selected_api + "_write_with_codecs(const " + name + "_input* value," + api("sink") + " sink,void* context," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size){srl_buffer bytes={0};int status=" + selected_api + "_encode_with_codecs(value,&bytes,budget,codecs,codecs_size);if(!status){status=" + api("write_sink") + "(sink,context,bytes.data,bytes.size,budget);}srl_buffer_free(&bytes);return status;}");
  line("/* Run selected encode/decode validation before same-directory atomic file publication. */");
  line("static inline int " + selected_api + "_save_with_codecs(const char* path,const " + name + "_input* value," + api("budget") + "* budget," + api("fault") + " fault,void* context,const " + api("selected_codec") + "* codecs,size_t codecs_size){srl_buffer bytes={0};" + name + "_value candidate={0};int status=" + selected_api + "_encode_with_codecs(value,&bytes,budget,codecs,codecs_size);if(!status){status=" + selected_api + "_load_with_codecs(&candidate,bytes.data,bytes.size," + api("preserve_policy") + "(),budget,codecs,codecs_size);}if(!status){status=" + api("save_file") + "(path,bytes.data,bytes.size,budget,fault,context);}" + name + "_free(&candidate);srl_buffer_free(&bytes);return status;}");
  line("/* Stage canonical fixed paths before borrowing a sink; input ownership remains untouched. */");
  line("static inline int " + selected_api + "_write_canonical_with_codecs(const " + name + "_input* value," + api("sink") + " sink,void* context," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size){srl_buffer bytes={0};int status=" + selected_api + "_encode_canonical_with_codecs(value,&bytes,budget,codecs,codecs_size);if(!status){status=" + api("write_sink") + "(sink,context,bytes.data,bytes.size,budget);}srl_buffer_free(&bytes);return status;}");
  line("/* Use default native codecs when writing canonical fixed-role paths. */");
  line("static inline int " + selected_api + "_write_canonical(const " + name + "_input* value," + api("sink") + " sink,void* context," + api("budget") + "* budget){return " + selected_api + "_write_canonical_with_codecs(value,sink,context,budget,NULL,0);}");
  line("/* Revalidate canonical output and preserved closure before atomic same-directory replacement. */");
  line("static inline int " + selected_api + "_save_canonical_with_codecs(const char* path,const " + name + "_input* value," + api("budget") + "* budget," + api("fault") + " fault,void* context,const " + api("selected_codec") + "* codecs,size_t codecs_size){srl_buffer bytes={0};" + name + "_value candidate={0};int status=" + selected_api + "_encode_canonical_with_codecs(value,&bytes,budget,codecs,codecs_size);if(!status){status=" + selected_api + "_load_with_codecs(&candidate,bytes.data,bytes.size," + api("preserve_policy") + "(),budget,codecs,codecs_size);}if(!status){status=" + api("save_file") + "(path,bytes.data,bytes.size,budget,fault,context);}" + name + "_free(&candidate);srl_buffer_free(&bytes);return status;}");
  line("/* Select native codecs for canonical fixed-role Save without altering the old Save API. */");
  line("static inline int " + selected_api + "_save_canonical(const char* path,const " + name + "_input* value," + api("budget") + "* budget," + api("fault") + " fault,void* context){return " + selected_api + "_save_canonical_with_codecs(path,value,budget,fault,context,NULL,0);}");
  line("/* Revalidate all edited role identities and retained references before dynamic-rename publication. */");
  line("static inline int " + selected_api + "_save_renamed(const char* path,const " + name + "_input* value," + api("budget") + "* budget,const " + api("part_rename") + "* renames,size_t renames_size," + api("fault") + " fault,void* context){srl_buffer bytes={0};" + name + "_value candidate={0};int status=" + selected_api + "_encode_renamed(value,&bytes,budget,renames,renames_size);if(!status){status=" + selected_api + "_load_with_codecs(&candidate,bytes.data,bytes.size," + api("preserve_policy") + "(),budget,NULL,0);}if(!status){status=" + api("save_file") + "(path,bytes.data,bytes.size,budget,fault,context);}" + name + "_free(&candidate);srl_buffer_free(&bytes);return status;}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
