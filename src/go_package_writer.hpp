// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "package_writer.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::package_codegen {
// Emit flat Go owning facades over checked schema roles, preserving existing model/XML APIs.
inline std::string go(const package_node& package, const std::string& name,
    const std::string& helper,
    const std::function<std::string(const type_name&)>& render_type,
    const std::function<std::string(std::string_view)>& render_field,
    const std::function<std::string(std::string_view)>& literal, const xml_options& xml,
    const std::string& selected_prefix) {
  const auto parts = ordered(package);
  const auto contract = compile_package_profile(package,xml);
  std::set<std::string> fields{"Extensions"};
  std::set<std::pair<std::string, std::string>> dispatch;
  for (const auto* part : parts) {
    if (!fields.insert(render_field(part->name)).second ||
        !dispatch.emplace(part->source, part->relationship).second) {
      throw std::invalid_argument{"Go package field or role dispatch collision: " + part->name};
    }
    if ((part->part_name || part->relationship_id) && (!part->max_occurs || *part->max_occurs > 1U)) {
      throw std::invalid_argument{"Repeated package roles require dynamic names and generated relationship IDs"};
    }
    if (part->min_occurs > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        (part->max_occurs && *part->max_occurs > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))) {
      throw std::invalid_argument{"Go package cardinality exceeds the signed native range"};
    }
    if (part->value_type) {
      const auto& type = *part->value_type;
      if (part->codec != "xml" || type.type != object_type::class_type || !type.resolved_node ||
          static_cast<const class_node&>(*type.resolved_node).reference ||
          !render_type(type).starts_with('*')) {
        throw std::invalid_argument{"Go typed package roles require ordinary owning XML records"};
      }
    }
  }
  std::string output;
  const auto line = [&](const std::string& text = {}) { output += text + '\n'; };
  const auto api = [&](std::string_view suffix) { return helper + std::string{suffix}; };
  const auto role = [&](std::size_t index) { return "srlPackageRole" + name + std::to_string(index); };
  // Typed callback arguments reuse public XmlOptions without extending old positional option structs.
  const auto codec_parameters = [&](bool encode, const std::string& prefix) {
    std::string result;
    for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
      const auto* part = parts[role_index];
      if (!part->value_type) { continue; }
      const auto type = render_type(*part->value_type);
      result += ',' + prefix + std::to_string(role_index) + " func(" +
          (encode ? type : "[]byte") + ",XmlOptions,string,string)(" +
          (encode ? "[]byte" : type) + ",error)";
    }
    return result;
  };
  // Forward callbacks by role order, or select the existing native direction with nil.
  const auto codec_arguments = [&](const std::string& prefix) {
    std::string result;
    for (std::size_t role_index{}; role_index < parts.size(); ++role_index) {
      if (parts[role_index]->value_type) {
        result += ',' + (prefix.empty() ? std::string{"nil"} : prefix + std::to_string(role_index));
      }
    }
    return result;
  };
  const auto role_type = [&](const package_part_declaration& part) {
    if (singleton(part)) { return render_type(*part.value_type); }
    return "[]" + (part.value_type ? api("ModelPart") + '[' + render_type(*part.value_type) + ']' : api("BinaryPart"));
  };
  line("// " + name + " binds complete declared package roles and independently retained extensions.");
  line("// Generated Go selected-codec prefix: " + selected_prefix);
  line("type " + name + " struct {");
  for (const auto* part : parts) { line("  " + render_field(part->name) + ' ' + role_type(*part)); }
  line("  Extensions " + api("Extensions"));
  line("}");
  line("// " + name + "Input borrows application values only for synchronous private composition.");
  line("type " + name + "Input = " + name);
  line("// " + name + "Value owns a fresh complete decoded aggregate; it never aliases the input model.");
  line("type " + name + "Value = " + name);
  line("// " + name + "LoadOptions is copied before source access; callbacks return promptly and errors retain identity.");
  line("type " + name + "LoadOptions struct { Budget *" + api("Budget") + "; ExtensionPolicy " + api("ExtensionPolicy") + "; Validate func(*" + name + "Value,*" + api("Budget") + ")error }");
  line("// " + name + "SaveOptions validates a fresh round-trip before any filesystem publication effects.");
  line("type " + name + "SaveOptions struct { Budget *" + api("Budget") + "; Validate func(*" + name + "Value,*" + api("Budget") + ")error; Fault " + api("SaveFault") + " }");
  std::size_t index{};
  for (const auto* part : parts) {
    const auto metadata=std::find_if(contract.parts.begin(),contract.parts.end(),[&](const auto& item){return item.role==part->name;});
    const auto expanded=[&](const auto& item){return item.namespace_uri.empty()?item.local:"{"+item.namespace_uri+"}"+item.local;};
    const auto expected=metadata->decoder?",expectedRoot:"+literal(expanded(metadata->decoder->root))+",expectedType:"+literal(expanded(*metadata->decoder->type)):"";
    const auto text = [&](const auto& value) { return value ? literal(*value) : "\"\""; };
    line("var " + role(index++) + " = srlPackageRole{field:" + literal(render_field(part->name)) + ",role:"+literal(part->name)+
        ",name:" + text(part->part_name) + ",contentType:" + text(part->content_type) +
        ",source:" + literal(part->source == "/" ? "/" : render_field(part->source)) +
        ",relationship:" + literal(part->relationship) + ",id:" + text(part->relationship_id) +
        ",compression:" + literal(part->compression) + ",minimum:" + std::to_string(part->min_occurs) +
        ",maximum:" + (part->max_occurs ? std::to_string(*part->max_occurs) : "-1") + ",policy:"+literal(part->profile_policy)+expected+"}");
  }
  line("// srlPackageReferences"+name+" validates all declared scalar keys and forward references.");
  line("func srlPackageReferences"+name+"(value *"+name+",budget *"+api("Budget")+",loaders ...*srlPackageLoader)error{index:=srlPackageReferences{budget:budget}");
  for(const auto* part:parts){
    if(part->keys.empty()&&part->references.empty()){continue;}
    const auto field=render_field(part->name);
    line(singleton(*part)?"  if value."+field+"!=nil{occurrence:=0;model:=value."+field:"  for occurrence,item:=range value."+field+"{model:=item.Value");
    const auto path=singleton(*part)?literal(*part->part_name):"item.Name";
    const auto link=part->relationship_id?literal(*part->relationship_id):literal("srl_"+part->name+"_")+"+fmt.Sprint(occurrence)";
    line("    index.part,index.id="+path+","+link+";if len(loaders)!=0{location:=loaders[0].referenceLocations["+literal(field)+"][occurrence];index.part,index.id=location.metadata.Name,location.id}");
    const auto lexical=[&](std::string_view field){const auto& member=selector_member(*part,field);return literal(member.type_name_list.front().name=="string"?"s:":"i:")+"+fmt.Sprint(model."+render_field(member.name)+")";};
    const auto model_identity=literal(part->value_type->resolved_node->get_full_name());
    for(const auto& key:part->keys){line("    if err:=index.key("+literal(key.scope)+","+lexical(key.field)+","+literal(field)+","+literal(key.field)+","+model_identity+");err!=nil{return err}");}
    for(const auto& reference:part->references){const auto target=std::find_if(parts.begin(),parts.end(),[&](const auto* item){return item->name==reference.target_role;});const auto expected=literal((*target)->value_type->resolved_node->get_full_name());line("    if err:=index.reference("+literal(reference.scope)+","+lexical(reference.field)+","+expected+","+literal(field)+","+literal(reference.field)+");err!=nil{return err}");}
    line("  }");
  }
  line("  return index.finish()}");
  line("// " + name + "Encode returns owning canonical bytes only after complete schema-role/graph validation.");
  line("func " + name + "Encode(value *" + name + "Input,budgets ...*" + api("Budget") + ")([]byte,error){");
  line("  if value==nil{return nil,srlPackageFailure(" + api("InvalidOptions") + ",\"Missing package input\",\"\")}");
  line("  budget,err:=srlPackageBudget(budgets);if err!=nil{return nil,err};return "+name+"EncodeWithProfiles(value,nil,budget)}");
  line("// "+name+"EncodeWithProfiles resolves closed per-role compression defaults.");
  line("func "+name+"EncodeWithProfiles(value *"+name+"Input,profiles map[string]string,budgets ...*"+api("Budget")+")([]byte,error){budget,err:=srlPackageBudget(budgets);if err!=nil{return nil,err};return "+selected_prefix+"Encode(value,profiles,budget"+codec_arguments("")+")}");
  line("// "+selected_prefix+"Encode shares bridged native XML policies with caller-selected typed encoders.");
  line("func "+selected_prefix+"Encode(value *"+name+"Input,profiles map[string]string,selectedBudget *"+api("Budget")+codec_parameters(true,"codec")+")([]byte,error){if value==nil{return nil,srlPackageFailure("+api("InvalidOptions")+",\"Missing package input\",\"\")}");
  std::string known;
  for(const auto* part:parts){if(!known.empty()){known+="&&";}known+="key!="+literal(part->name);}
  line("  for key:=range profiles{if "+(known.empty()?"true":known)+"{return nil,srlPackageFailure("+api("InvalidOptions")+",\"Unknown package profile role\",key)}}");
  line("  budget,err:=srlPackageBudget([]*"+api("Budget")+"{selectedBudget});if err!=nil{return nil,err};composer:=srlPackageNewComposer(budget);defer composer.close()");
  line("  if err=srlPackageReferences"+name+"(value,budget);err!=nil{return nil,err}");
  index = 0;
  for (const auto* part : parts) {
    const auto selected = index++;
    line("  "+role(selected)+",err:=srlPackageSelectProfile("+role(selected)+",profiles);if err!=nil{return nil,err}");
    const auto field = "value." + render_field(part->name);
    if (!part->value_type) {
      line("  if err=composer.binaries(" + role(selected) + ',' + field + ");err!=nil{return nil,err}");
      continue;
    }
    const auto type = render_type(*part->value_type);
    const auto values = "srlModels" + std::to_string(selected);
    const auto native = "srlNative" + std::to_string(selected);
    const auto count = "srlCount" + std::to_string(selected);
    line("  " + native + ":=" + field);
    if (singleton(*part)) {
      line("  " + count + ":=0;if " + native + "!=nil{" + count + "=1}");
    } else {
      line("  " + count + ":=len(" + native + ")");
    }
    line("  " + values + ",err:=composer.inputs(" + role(selected) + ',' + count + ");if err!=nil{return nil,err}");
    if (singleton(*part)) {
      line("  if " + native + "!=nil{" + values + "=append(" + values + ",srlPackageModelInput{value:" + native + "})}");
    } else {
      line("  for _,item:=range " + native + '{' + values + "=append(" + values + ",srlPackageModelInput{value:item.Value,name:item.Name,contentType:item.ContentType,parent:item.SourceIndex})}");
    }
    const auto encoder = "srlEncoder" + std::to_string(selected);
    line("  var " + encoder + " any=func(native any,xmlOptions XmlOptions)([]byte,error){typed,ok:=native.(" + type + ");if !ok||typed==nil{return nil,srlPackageFailure(" + api("Codec") + ",\"Missing typed package value\",\"\")};return typed.EncodeXML(xmlOptions)}");
    line("  if codec" + std::to_string(selected) + "!=nil{" + encoder + "=func(native any,xmlOptions XmlOptions,part,id string)([]byte,error){typed,ok:=native.(" + type + ");if !ok||typed==nil{return nil,srlPackageFailure(" + api("Codec") + ",\"Missing typed package value\",part)};return codec" + std::to_string(selected) + "(typed,xmlOptions,part,id)}}");
    line("  if err=composer.models(" + role(selected) + ',' + values + ',' + encoder + ");err!=nil{return nil,err}");
  }
  line("  return composer.finish(value.Extensions)");
  line("}");
  line("// " + name + "Write borrows a sink only after complete staging; an error may leave an accepted prefix.");
  line("func " + name + "Write(destination srlPackageIO.Writer,value *" + name + "Input,budgets ...*" + api("Budget") + ")(int64,error){");
  line("  if destination==nil{return 0,srlPackageFailure(" + api("InvalidOptions") + ",\"Missing package sink\",\"\")};budget,err:=srlPackageBudget(budgets);if err!=nil{return 0,err};data,err:=" + name + "Encode(value,budget);if err!=nil{return 0,err};return " + api("Write") + "(destination,data,budget)");
  line("}");
  line("// " + name + "Load publishes only a complete fresh aggregate after native codecs, extensions and domain validation.");
  line("func " + name + "Load(source any,values ..." + name + "LoadOptions)(*" + name + "Value,error){");
  line("  if len(values)>1{return nil,srlPackageFailure(" + api("InvalidOptions") + ",\"Too many package load options\",\"\")};options:=" + name + "LoadOptions{};if len(values)==1{options=values[0]}");
  line("  return " + selected_prefix + "Load(source,options" + codec_arguments("") + ")}");
  line("// " + selected_prefix + "Load composes selected decoders with the same located native operation policy.");
  line("func " + selected_prefix + "Load(source any,options " + name + "LoadOptions" + codec_parameters(false,"codec") + ")(*" + name + "Value,error){");
  line("  if options.ExtensionPolicy.Parts>" + api("Preserve") + "||options.ExtensionPolicy.Relationships>" + api("Preserve") + "{return nil,srlPackageFailure(" + api("InvalidOptions") + ",\"Invalid package extension policy\",\"\")}");
  line("  budget,err:=srlPackageBudget([]*" + api("Budget") + "{options.Budget});if err!=nil{return nil,err};loader,err:=srlPackageNewLoader(source,budget);if err!=nil{return nil,err};defer loader.close();value:=&" + name + "Value{}");
  index = 0;
  for (const auto* part : parts) {
    const auto selected = index++;
    const auto field = "value." + render_field(part->name);
    if (!part->value_type) {
      line("  " + field + ",err=loader.binaries(" + role(selected) + ");if err!=nil{return nil,err}");
      continue;
    }
    const auto type = render_type(*part->value_type);
    const auto values = "srlValues" + std::to_string(selected);
    const auto decoder = "srlDecoder" + std::to_string(selected);
    line("  var " + decoder + " any=func(data []byte,xmlOptions XmlOptions)(any,error){native,err:=Decode" + type.substr(1) + "XML(data,xmlOptions);if err!=nil{return nil,err};return native,nil}");
    line("  if codec" + std::to_string(selected) + "!=nil{" + decoder + "=func(data []byte,xmlOptions XmlOptions,part,id string)(any,error){native,err:=codec" + std::to_string(selected) + "(data,xmlOptions,part,id);if err!=nil{return nil,err};if native==nil{return nil,srlPackageFailure(" + api("Codec") + ",\"Missing decoded typed package value\",part)};return native,nil}}");
    line("  " + values + ",err:=loader.models(" + role(selected) + ',' + decoder + ");if err!=nil{return nil,err}");
    if (singleton(*part)) {
      line("  if len(" + values + ")!=0{" + field + '=' + values + "[0].value.(" + type + ")}");
    } else {
      line("  for _,item:=range " + values + '{' + field + "=append(" + field + ',' + api("ModelPart") + '[' + type + "]{Value:item.value.(" + type + "),Name:item.name,ContentType:item.contentType,SourceIndex:item.parent})}");
    }
  }
  line("  if err=srlPackageReferences"+name+"(value,budget,loader);err!=nil{return nil,err}");
  line("  var validate func(*" + api("Budget") + ")error;if options.Validate!=nil{validate=func(budget *" + api("Budget") + ")error{return options.Validate(value,budget)}}");
  line("  if err=loader.finish(options.ExtensionPolicy,func(extensions " + api("Extensions") + "){value.Extensions=extensions},validate);err!=nil{return nil,err};return value,nil");
  line("}");
  line("// " + selected_prefix + "Write stages selected encoders before writing to the borrowed sink.");
  line("func " + selected_prefix + "Write(destination srlPackageIO.Writer,value *" + name + "Input,budget *" + api("Budget") + codec_parameters(true,"codec") + ")(int64,error){if destination==nil{return 0,srlPackageFailure(" + api("InvalidOptions") + ",\"Missing package sink\",\"\")};data,err:=" + selected_prefix + "Encode(value,nil,budget" + codec_arguments("codec") + ");if err!=nil{return 0,err};return " + api("Write") + "(destination,data,budget)}");
  line("// " + selected_prefix + "Save validates selected encode/decode callbacks before final file publication.");
  line("func " + selected_prefix + "Save(path string,value *" + name + "Input,options " + name + "SaveOptions" + codec_parameters(true,"encode") + codec_parameters(false,"decode") + ")error{if path==\"\"{return srlPackageFailure(" + api("InvalidOptions") + ",\"Missing save path\",\"\")};budget,err:=srlPackageBudget([]*" + api("Budget") + "{options.Budget});if err!=nil{return err};data,err:=" + selected_prefix + "Encode(value,nil,budget" + codec_arguments("encode") + ");if err!=nil{return err};_,err=" + selected_prefix + "Load(" + api("Source") + "{Reader:srlPackageBytes.NewReader(data),SizeBytes:int64(len(data))}," + name + "LoadOptions{Budget:budget,ExtensionPolicy:" + api("ExtensionPolicy") + "{Parts:" + api("Preserve") + ",Relationships:" + api("Preserve") + "},Validate:options.Validate}" + codec_arguments("decode") + ");if err!=nil{return err};return " + api("SaveBytes") + "(path,data,budget,options.Fault)}");
  line("// " + name + "Save validates a fresh round-trip before same-directory platform-specific Save publication.");
  line("func " + name + "Save(path string,value *" + name + "Input,values ..." + name + "SaveOptions)error{");
  line("  if path==\"\"||len(values)>1{return srlPackageFailure(" + api("InvalidOptions") + ",\"Invalid package save options\",\"\")};options:=" + name + "SaveOptions{};if len(values)==1{options=values[0]};budget,err:=srlPackageBudget([]*" + api("Budget") + "{options.Budget});if err!=nil{return err}");
  line("  data,err:=" + name + "Encode(value,budget);if err!=nil{return err};_,err=" + name + "Load(" + api("Source") + "{Reader:srlPackageBytes.NewReader(data),SizeBytes:int64(len(data))}," + name + "LoadOptions{Budget:budget,ExtensionPolicy:" + api("ExtensionPolicy") + "{Parts:" + api("Preserve") + ",Relationships:" + api("Preserve") + "},Validate:options.Validate});if err!=nil{return err};return " + api("SaveBytes") + "(path,data,budget,options.Fault)");
  line("}");
  return output;
}
} // namespace rohit::serializer::writer::package_codegen
