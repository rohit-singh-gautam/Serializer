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
    const std::function<std::string(std::string_view)>& literal) {
  const auto parts = ordered(package);
  std::set<std::string> fields{"extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  std::map<std::string, std::size_t> role_indices;
  for (std::size_t index{}; index < parts.size(); ++index) {
    const auto* part = parts[index];
    require_native_package_graph(*part, xml, "C");
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
    if (part->value_type && part->codec != "xml") {
      throw std::invalid_argument{"C typed package roles require XML"};
    }
  }
  std::string output;
  const auto line = [&](const std::string& text) { output += text + '\n'; };
  const auto api = [&](std::string_view suffix) { return helper + "_" + std::string{suffix}; };
  const auto selected_api = helper + '_' + name;
  const auto role = [&](std::size_t index) { return "&" + name + "_profiles[" + std::to_string(index) + "]"; };
  const auto optional = [&](const auto& value) { return value ? literal(*value) : "NULL"; };
  line("/* Serializer package facade: " + package.get_full_name() + " " + name + " */");
  line("/* Generated C selected-codec prefix: " + selected_api + " */");
  for (const auto* part : parts) {
    if (!part->value_type || singleton(*part)) { continue; }
    const auto member = render_field(part->name);
    line("/* Own one typed occurrence and its discovered source-routing metadata. */");
    line("typedef struct " + name + "_" + member + "_part {" + render_type(*part->value_type) +
        " value; char* name; char* content_type; size_t source_index; char* link;} " + name + "_" + member + "_part;");
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
    line("} " + owner + ";");
  }
  line("/* Release only resources owned by an initialized decoded aggregate. */");
  line("static inline void " + name + "_free(" + name + "_value* value) {");
  for (const auto* part : parts) {
    const auto member = render_field(part->name);
    if (singleton(*part)) {
      line("  if(value->" + member + "){ " + render_type(*part->value_type) + "_free(value->" + member + "); free(value->" + member + "); } free(value->" + member + "_path); free(value->" + member + "_link);");
    } else {
      line("  for(size_t index=0;index<value->" + member + "_size;++index){");
      if (part->value_type) {
        line("    " + render_type(*part->value_type) + "_free(&value->" + member + "[index].value); free(value->" + member + "[index].name);free(value->" + member + "[index].content_type);free(value->" + member + "[index].link);");
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
  line("static inline int " + selected_api + "_encode_mode(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size,bool canonical){ " + api("composer") + " composer={0};srl_buffer bytes={0};srl_limits limits=" + api("native_limits") + "(budget);");
  line("  if(!value||!result){return " + api("fail") + "(budget," + api("invalid_options") + ",NULL,NULL,NULL);}");
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
      line("    if(" + api("encode_selected") + "(codecs,codecs_size," + selected + ",srl_xml_operation_codec_" + render_type(*part->value_type) + "(),value->" + member + ",&bytes,&limits,path,link,budget)||" + api("typed") + "(" + selected + ",bytes.data,bytes.size,path,link,budget)||" + api("compose") + "(&composer," + selected + "," + std::to_string(index) + ",path,NULL,0,&bytes)){goto done;}");
    } else {
      line("  for(size_t index=0;index<value->" + member + "_size;++index){const char* path=value->" + member + "[index].name;");
      if (part->value_type) {
        line("    const char* link=value->" + member + "[index].link?value->" + member + "[index].link:" + literal(part->relationship_id.value_or(part->name)) + ";");
        line("    if(" + api("encode_selected") + "(codecs,codecs_size," + selected + ",srl_xml_operation_codec_" + render_type(*part->value_type) + "(),&value->" + member + "[index].value,&bytes,&limits,path,link,budget)||" + api("typed") + "(" + selected + ",bytes.data,bytes.size,path,link,budget)){goto done;}");
      } else {
        line("    if(" + api("append") + "(budget,&bytes,value->" + member + "[index].bytes.data,value->" + member + "[index].bytes.size,budget->max_part_bytes)){goto done;}");
      }
      line("    if(" + api("compose") + "(&composer," + selected + "," + std::to_string(index) + ",path,value->" + member + "[index].content_type,value->" + member + "[index].source_index,&bytes)){goto done;}");
    }
    line("  }");
  }
  line("  if(canonical){" + api("compose_finish_relocated") + "(&composer,value->extensions,relocations,relocation_count,result);}else{" + api("compose_finish") + "(&composer,value->extensions,result);}goto done;done:srl_buffer_free(&bytes);" + api("composer_free") + "(&composer);(void)limits;return budget->status;\n}");
  line("/* Preserve ordinary discovered paths and existing selected-codec entrypoint signatures. */");
  line("static inline int " + selected_api + "_encode_with_codecs(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size){return " + selected_api + "_encode_mode(value,result,budget,codecs,codecs_size,false);}");
  line("/* Canonicalize only fixed singleton paths while borrowing every typed payload unchanged. */");
  line("static inline int " + selected_api + "_encode_canonical_with_codecs(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size){return " + selected_api + "_encode_mode(value,result,budget,codecs,codecs_size,true);}");
  line("/* Select native codecs for an additive canonical-path archive operation. */");
  line("static inline int " + selected_api + "_encode_canonical(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget){return " + selected_api + "_encode_canonical_with_codecs(value,result,budget,NULL,0);}");
  line("/* Preserve the original generated encoder with default operation-aware native codecs. */");
  line("static inline int " + name + "_encode(const " + name + "_input* value,srl_buffer* result," + api("budget") + "* budget){return " + selected_api + "_encode_with_codecs(value,result,budget,NULL,0);}");
  line("/* Decode into a private aggregate; failure leaves the initialized destination unchanged. */");
  line("static inline int " + selected_api + "_load_with_codecs(" + name + "_value* result,const uint8_t* bytes,size_t size," + api("policy") + " policy," + api("budget") + "* budget,const " + api("selected_codec") + "* codecs,size_t codecs_size){ " + name + "_value fresh={0};" + api("loader") + " loader={0};" + api("selection") + "* selected=NULL;size_t count=0;srl_buffer payload={0};srl_limits limits=" + api("native_limits") + "(budget);");
  line("  if(!result){return " + api("fail") + "(budget," + api("invalid_options") + ",NULL,NULL,NULL);}if(" + api("validate_codecs") + "(codecs,codecs_size," + name + "_profiles,sizeof(" + name + "_profiles)/sizeof(" + name + "_profiles[0]),budget)||" + api("profiles") + "(" + name + "_profiles,sizeof(" + name + "_profiles)/sizeof(" + name + "_profiles[0]),budget)||" + api("loader_init") + "(&loader,bytes,size," + std::to_string(parts.size()) + ",budget)){goto done;}");
  for (std::size_t index{}; index < parts.size(); ++index) {
    const auto* part = parts[index];
    const auto member = render_field(part->name), metadata = role(index);
    line("  if(" + api("select") + "(&loader," + metadata + "," + std::to_string(index) + ",&selected,&count)){goto done;}");
    if (!singleton(*part)) {
      line("  if(count){if(count>SIZE_MAX/sizeof(*fresh." + member + ")||" + api("consume") + "(budget," + api("allocation_bytes") + ",count*sizeof(*fresh." + member + "),NULL," + literal(part->name) + ",NULL)){ " + api("fail") + "(budget," + api("resource_limit") + ",NULL," + literal(part->name) + ",NULL);goto done;}fresh." + member + "=(" + (part->value_type ? name + "_" + member + "_part" : api("binary_part")) + "*)calloc(count,sizeof(*fresh." + member + "));if(!fresh." + member + "){ " + api("fail") + "(budget," + api("allocation") + ",NULL," + literal(part->name) + ",NULL);goto done;}fresh." + member + "_size=count;}");
    }
    line("  for(size_t index=0;index<count;++index){const " + api("catalog_part") + "* part=&loader.catalog.parts[selected[index].part];const " + api("relationship") + "* link=&loader.catalog.links[selected[index].link];if(" + api("archive_read") + "(&loader.archive,part->entry,&payload,budget)){goto done;}");
    if (part->value_type) {
      const auto native = render_type(*part->value_type);
      const auto value = singleton(*part) ? "fresh." + member : "&fresh." + member + "[index].value";
      if (singleton(*part)) {
        line("    if(" + api("consume") + "(budget," + api("allocation_bytes") + ",sizeof(*fresh." + member + "),part->value.name," + literal(part->name) + ",link->id)){goto done;}fresh." + member + "=(" + native + "*)calloc(1,sizeof(*fresh." + member + "));if(!fresh." + member + "){ " + api("fail") + "(budget," + api("allocation") + ",part->value.name," + literal(part->name) + ",link->id);goto done;}");
      }
      line("    if(" + api("native_at") + "(budget," + native + "_init(" + value + "),part->value.name," + literal(part->name) + ",link->id)||" + api("typed") + "(" + metadata + ",payload.data,payload.size,part->value.name,link->id,budget)||" + api("decode_selected") + "(codecs,codecs_size," + metadata + ",srl_xml_operation_codec_" + native + "()," + value + ",payload.data,payload.size,&limits,part->value.name,link->id,budget)){goto done;}");
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
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
