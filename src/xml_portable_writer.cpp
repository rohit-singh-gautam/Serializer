// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "xml_portable_writer.hpp"
#include "xml_portable_extension_writer.hpp"
#include "native_schema.hpp"
#include "xml_native_model_writer.hpp"
#include "xml_portable_projection_writer.hpp"

#include <rohit/html.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::xml_portable {
namespace {
const std::string_view go_runtime =
#include "xml_go_runtime.inc"
#include "xml_go_map_runtime.inc"
#include "xml_go_scalar_runtime.inc"
#include "xml_go_namespace_runtime.inc"
#include "xml_go_mixed_runtime.inc"
#include "xml_go_identity_runtime.inc"
#include "xml_go_html_runtime.inc"
#include "xml_go_model_runtime.inc"
#include "xml_go_extension_runtime.inc"
    ;
// Retain exact schema strings in generated native source.
std::string quote(std::string_view value) { return native::quote(value); }
// Build an expanded name independently of document prefixes.
std::string qualified(const xml::name& value) {
  return "srlxml.Name{Space:" + quote(value.namespace_uri) + ",Local:" + quote(value.local) + "}";
}
// Select only namespace-dependent scalar strings qualified by the Go direct token path.
bool namespace_scalar(const xml_binding::field& field) {
  return field.qname || field.prefix_list || field.facets.list_item_kind == "QName";
}
// Keep QName, QName-list and namespace-prefix-list semantics distinct during token traversal.
std::string namespace_mode(const xml_binding::field& field) {
  return field.prefix_list ? "2" : field.facets.list_item_kind == "QName" ? "1" : "0";
}
// Select the statically known leaf beneath one supported presence/collection modifier.
const type_name& leaf(const member& value) {
  const auto* selected = &value.type_name_list.front();
  while (selected->is_optional() || selected->is_sequence()) { selected = &selected->arguments.front(); }
  return *selected;
}
// Select the native scalar width without repeating its range in generated checks.
std::string bits(const type_name& type) {
  if (type.name == "float" || type.name == "float32") { return "32"; }
  if (type.name == "double" || type.name == "float64") { return "64"; }
  return type.name.substr(type.name.starts_with("uint") ? 4U : 3U);
}
// Format a statically typed Go scalar; enum spelling remains schema-owned.
std::string encode_scalar(const type_name& type, const std::string& value,
                          const type_name_fn& name, std::size_t projection_depth = 0U) {
  if (value_projection(type)) {
    const auto& owner = static_cast<const class_node&>(*type.resolved_node);
    const auto selected = "srlSelected" + std::to_string(projection_depth);
    const auto error = "srlReferenceErr" + std::to_string(projection_depth);
    return selected + ',' + error + ":=" + value + ".ReadValue()\nif " + error + "!=nil{return nil," + error + "}\n" +
        encode_scalar(owner.reference->value_type, selected, name, projection_depth + 1U);
  }
  if (type.is_digest()) {
    return "srlText,srlDigestErr:=srlXmlDigestText(" + value + ',' + std::to_string(type.digest_extent) + ",output.digestLimits)\nif srlDigestErr!=nil{return nil,srlDigestErr}\n";
  }
  if (type.type == object_type::enum_type) {
    const auto& enumeration = static_cast<const enum_node&>(*type.resolved_node);
    std::string result{"var srlText string\nswitch " + value + " {\n"};
    for (std::size_t index{}; index < enumeration.enum_name_list.size(); ++index) {
      result += "case " + name(type) + "(" + std::to_string(index) + "): srlText=" + quote(enumeration.enum_name_list[index]) + "\n";
    }
    return result + "default:return nil,fmt.Errorf(\"Invalid XML enum\")\n}\n";
  }
  const auto expression = type.name == "string" || type.name.starts_with("version") ? value : type.name == "bool" ? "strconv.FormatBool(" + value + ")" :
      type.name.starts_with("uint") ? "strconv.FormatUint(uint64(" + value + "),10)" :
      type.name.starts_with("int") ? "strconv.FormatInt(int64(" + value + "),10)" :
      "srlXmlFormatFloat(float64(" + value + ")," + bits(type) + ")";
  return "srlText:=" + expression + "\n";
}
// Parse a statically typed Go scalar with exact XML lexical/range checks.
std::string decode_scalar(const type_name& type, const type_name_fn& name) {
  if (type.is_digest()) {
    return "srlValue,srlErr:=srlXmlDigestRead(srlText," + std::to_string(type.digest_extent) + ",input.digestLimits)\nif srlErr!=nil{return nil,srlErr}\n";
  }
  if (type.type == object_type::enum_type) {
    const auto& enumeration = static_cast<const enum_node&>(*type.resolved_node);
    std::string result{"var srlValue " + name(type) + "\nswitch srlText {\n"};
    for (std::size_t index{}; index < enumeration.enum_name_list.size(); ++index) {
      result += "case " + quote(enumeration.enum_name_list[index]) + ":srlValue=" + name(type) + "(" + std::to_string(index) + ")\n";
    }
    return result + "default:return nil,fmt.Errorf(\"Invalid XML enum\")\n}\n";
  }
  if (type.name == "string" || type.name.starts_with("version")) { return "srlValue:=srlText\n"; }
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
// Lower supported scalar facets once for both direct input and output bindings.
std::string scalar_facets(const xml_binding::field& field,
                          const std::string& prepared_rule, bool output = false) {
  if (prepared_rule.empty()) { return {}; }
  const auto& fallback = field.fixed_value ? field.fixed_value : field.default_value;
  std::string result{std::string{output?"output":"input"}+".budget.field="+quote(field.source->name)+"\n"};
  if (!output && fallback && !field.attribute) { result += "if srlText==\"\"{srlText=" + quote(*fallback) + "}\n"; }
  result += "srlChecked,srlFacetErr:=" + prepared_rule;
  if (output) {
    result += ".output(srlText," + quote(fallback.value_or("")) + ',' +
        (fallback && !field.attribute ? "true" : "false") + ",output.budget.limits.MaxNodes,&output.budget)\n";
  } else { result += ".check(srlText,input.budget.limits.MaxNodes,&input.budget)\n"; }
  return result + "if srlFacetErr!=nil{return nil,srlFacetErr}\nsrlText=srlChecked\n";
}
// Lower the shared Thompson program without introducing a host-regex dialect.
std::string pattern_program(const xml::portable_pattern& pattern) {
  std::string result{"{start:" + std::to_string(pattern.start()) +
                     ",states:[]srlXmlPatternState{"};
  for (const auto& state : pattern.states()) {
    result += "{epsilon:[]int{";
    for (const auto target : state.epsilon) { result += std::to_string(target) + ','; }
    result += "},ranges:[][2]rune{";
    for (const auto& range : state.ranges) {
      result += '{' + std::to_string(range.first) + ',' + std::to_string(range.last) + "},";
    }
    result += "},next:" + std::to_string(state.next) + ",consumes:" +
        (state.consumes ? "true" : "false") + ",inverted:" + (state.inverted ? "true" : "false") +
        ",dot:" + (state.dot ? "true" : "false") + ",accepting:" +
        (state.accepting ? "true" : "false") + "},";
  }
  return result + "}}";
}
// Prepare complete recursive scalar contracts once, including independent union pattern programs.
std::string go_scalar_rule(const xml_binding::field& field) {
  const auto count = [](std::optional<std::size_t> extent) { return extent ? std::to_string(*extent) : "-1"; };
  const auto bound = [](const std::optional<std::string>& value) { return quote(value.value_or("")); };
  const auto rule = [&](auto&& self, const xml_binding::scalar_facets& facets,
                        const std::optional<std::string>& fixed) -> std::string {
    std::string result{"srlXmlRule{kind:" + quote(facets.lexical_kind) + ",whitespace:" + quote(facets.whitespace) +
        ",listKind:" + quote(facets.list_item_kind) + ",bounds:[4]string{" + bound(facets.min_inclusive) + ',' +
        bound(facets.max_inclusive) + ',' + bound(facets.min_exclusive) + ',' + bound(facets.max_exclusive) +
        "},length:" + count(facets.length) + ",minimum:" + count(facets.min_length) + ",maximum:" + count(facets.max_length) +
        ",finite:" + (facets.finite ? "true" : "false") + ",enumeration:[]string{"};
    for (const auto& item : facets.enumeration) { result += quote(item) + ','; }
    result += "},patterns:[][]srlXmlPattern{";
    auto groups = facets.pattern_groups;
    if (!facets.patterns.empty()) { groups.push_back(facets.patterns); }
    for (const auto& group : groups) {
      result += '{';
      for (const auto& expression : group) { result += pattern_program(xml::compile_portable_pattern(expression)) + ','; }
      result += "},";
    }
    result += "},members:[]srlXmlRule{";
    for (const auto& branch : facets.union_members) { result += self(self, branch, {}) + ','; }
    return result + "},hasFixed:" + (fixed ? "true" : "false") + ",fixed:" + quote(fixed.value_or("")) + '}';
  };
  return rule(rule, field.facets, field.fixed_value);
}
// Encode the shared instructions as bounded ASCII fragments instead of oversized JVM initializers.
std::string pattern_blob(const xml::portable_pattern& pattern) {
  constexpr unsigned consumes_flag{1U}, inverted_flag{2U}, dot_flag{4U}, accepting_flag{8U};
  std::string result{std::to_string(pattern.start())};
  for (const auto& state : pattern.states()) {
    result += ';';
    for (std::size_t index{}; index < state.epsilon.size(); ++index) {
      if (index != 0U) { result += ','; }
      result += std::to_string(state.epsilon[index]);
    }
    result += '|';
    for (std::size_t index{}; index < state.ranges.size(); ++index) {
      if (index != 0U) { result += ','; }
      result += std::to_string(state.ranges[index].first) + ',' + std::to_string(state.ranges[index].last);
    }
    const unsigned flags = (state.consumes ? consumes_flag : 0U) | (state.inverted ? inverted_flag : 0U) |
        (state.dot ? dot_flag : 0U) | (state.accepting ? accepting_flag : 0U);
    result += '|' + std::to_string(state.next) + '|' + std::to_string(flags);
  }
  return result;
}
// Output helpers return only an error; decoder helpers also return a candidate.
std::string output_errors(std::string source) {
  constexpr std::string_view candidate_return{"return nil,"};
  constexpr std::string_view error_return{"return "};
  std::size_t offset{};
  while ((offset = source.find(candidate_return, offset)) != std::string::npos) {
    source.replace(offset, candidate_return.size(), error_return);
    offset += error_return.size();
  }
  return source;
}
// Traverse a native map in deterministic typed-key order without constructing XML nodes.
std::string encode_map(const xml_binding::map_contract& map, const std::string& access,
                       const type_name_fn& type) {
  const auto selected = map.optional_container ? "(*" + access + ")" : access;
  const auto less = map.key_kind == xml_binding::map_key_kind::boolean ?
      "!srlKeys[i]&&srlKeys[j]" : "srlKeys[i]<srlKeys[j]";
  std::string out{"{\n"};
  if (map.optional_container) { out += "if " + access + "!=nil{\n"; }
  out += "if len(" + selected + ")>output.mapLimits.MaxEntries{return srlXmlLimitError(\"XML map entry limit exceeded\")}\n";
  out += "if err:=output.open(" + qualified(map.wrapper) + ");err!=nil{return err}\n";
  out += "srlKeys:=make([]" + type(map.key_type) + ",0,len(" + selected + "))\nfor srlKey:=range " +
      selected + "{srlKeys=append(srlKeys,srlKey)}\nsort.Slice(srlKeys,func(i,j int)bool{return " + less + "})\nsrlKeyBytes:=0\n";
  out += "for _,srlKey:=range srlKeys{\nsrlItem:=" + selected + "[srlKey]\n";
  out += output_errors(encode_scalar(map.key_type, "srlKey", type));
  out += "if err:=srlXmlMapKeyBytes(srlText,srlText,&srlKeyBytes,output.mapLimits);err!=nil{return err}\n";
  out += "if err:=output.open(" + qualified(map.entry) + ");err!=nil{return err}\n";
  if (map.key_placement == xml_binding::map_key_placement::attribute) {
    out += "if err:=output.attribute(" + qualified(map.key) + ",srlText);err!=nil{return err}\n";
  } else {
    out += "if err:=output.open(" + qualified(map.key) + ");err!=nil{return err}\nif err:=output.text(srlText);err!=nil{return err}\nif err:=output.close();err!=nil{return err}\n";
  }
  if (map.value_type.type == object_type::class_type) {
    out += "if err:=srlItem.srlXmlEncode(output," + qualified(map.value) + ");err!=nil{return err}\n";
  } else {
    out += "{\n" + output_errors(encode_scalar(map.value_type, "srlItem", type));
    out += "if err:=output.open(" + qualified(map.value) + ");err!=nil{return err}\nif err:=output.text(srlText);err!=nil{return err}\nif err:=output.close();err!=nil{return err}\n}\n";
  }
  out += "if err:=output.close();err!=nil{return err}\n}\nif err:=output.close();err!=nil{return err}\n";
  if (map.optional_container) { out += "}\n"; }
  return out + "}\n";
}

// Parse complete private entries, rejecting canonical typed-key duplicates before insertion.
std::string decode_map(const xml_binding::map_contract& map, const std::string& access,
                       const type_name_fn& type) {
  std::string out{"{\nsrlPresent,srlErr:=input.has(" + qualified(map.wrapper) + ");if srlErr!=nil{return nil,srlErr}\n"};
  if (map.optional_container) { out += access + "=nil\n"; }
  else { out += "if !srlPresent{return nil,fmt.Errorf(\"Required XML map wrapper missing\")}\n"; }
  out += "if srlPresent{\nsrlWrapper,srlErr:=input.open(" + qualified(map.wrapper) + ");if srlErr!=nil{return nil,srlErr}\n";
  out += "if len(srlWrapper.attrs)!=0{return nil,fmt.Errorf(\"Unexpected XML map wrapper attribute\")}\n";
  out += "srlMap:=make(map[" + type(map.key_type) + "]" + type(map.value_type) + ")\nsrlKeyBytes,srlEntries:=0,0\n";
  out += "for{\nsrlPresent,srlErr:=input.has(" + qualified(map.entry) + ");if srlErr!=nil{return nil,srlErr};if !srlPresent{break}\n";
  out += "if srlEntries>=input.mapLimits.MaxEntries{return nil,srlXmlLimitError(\"XML map entry limit exceeded\")}\n";
  out += "srlEntry,srlErr:=input.open(" + qualified(map.entry) + ");if srlErr!=nil{return nil,srlErr}\n";
  out += "var srlKey " + type(map.key_type) + "\nvar srlItem " + type(map.value_type) + "\nvar srlKeyLexical string\nsrlHasKey,srlHasValue:=false,false\n";
  // A local scope lets key and value adapters reuse their normal lexical temporary names.
  const auto key = [&]() {
    return decode_scalar(map.key_type, type) + "srlKey=srlValue;srlKeyLexical=srlText;srlHasKey=true\n";
  };
  if (map.key_placement == xml_binding::map_key_placement::attribute) {
    out += "{\nsrlText,_,srlErr:=srlEntry.attribute(" + qualified(map.key) + ",true);if srlErr!=nil{return nil,srlErr}\n" + key() + "}\n";
  }
  out += "for{\nsrlEvent,srlErr:=input.peek();if srlErr!=nil{return nil,srlErr};if srlEvent.kind!='s'{break}\nswitch srlEvent.name{\n";
  if (map.key_placement == xml_binding::map_key_placement::element) {
    out += "case " + qualified(map.key) + ":\nif srlHasKey{return nil,fmt.Errorf(\"Duplicate XML map key member\")}\nsrlText,srlErr:=input.scalar(" + qualified(map.key) + ");if srlErr!=nil{return nil,srlErr}\n" + key();
  }
  out += "case " + qualified(map.value) + ":\nif srlHasValue{return nil,fmt.Errorf(\"Duplicate XML map value member\")}\n";
  if (map.value_type.type == object_type::class_type) {
    out += "srlValue,srlErr:=srlXmlDecode" + record_name(type(map.value_type)) + "(input," + qualified(map.value) + ");if srlErr!=nil{return nil,srlErr}\n";
  } else {
    out += "srlText,srlErr:=input.scalar(" + qualified(map.value) + ");if srlErr!=nil{return nil,srlErr}\n" + decode_scalar(map.value_type, type);
  }
  out += "srlItem=srlValue;srlHasValue=true\ndefault:return nil,fmt.Errorf(\"Unexpected XML map member\")\n}\n}\n";
  out += "if !srlHasKey||!srlHasValue{return nil,fmt.Errorf(\"Incomplete XML map entry\")}\nif srlErr=srlEntry.finish();srlErr!=nil{return nil,srlErr}\n";
  out += "{\n" + encode_scalar(map.key_type, "srlKey", type) +
      "if srlErr:=srlXmlMapKeyBytes(srlKeyLexical,srlText,&srlKeyBytes,input.mapLimits);srlErr!=nil{return nil,srlErr}\n}\n";
  out += "if _,srlDuplicate:=srlMap[srlKey];srlDuplicate{return nil,fmt.Errorf(\"Duplicate canonical XML map key\")}\nsrlMap[srlKey]=srlItem;srlEntries++\n}\n";
  out += "if srlErr=srlWrapper.finish();srlErr!=nil{return nil,srlErr}\n" + access +
      (map.optional_container ? "=&srlMap\n" : "=srlMap\n");
  return out + "}\n}\n";
}
// Encode explicit native presence layers without allocating a second XML representation.
std::string encode_presence(const xml_binding::field& field, const std::string& access,
    const type_name_fn& type, const std::string& patterns) {
  const auto& presence = field.presence;
  const auto& selected = leaf(*field.source);
  std::string out{"{\n"}, payload{access};
  if (presence.allows_absent) { out += "if " + payload + "!=nil{\n"; payload = "(*" + payload + ')'; }
  if (presence.allows_nil) {
    out += "if " + payload + "==nil{\nif err:=output.open(" + qualified(field.wrapper ? *field.wrapper : field.qualified_name) + ");err!=nil{return err}\nif err:=output.attribute(srlxml.Name{Space:\"http://www.w3.org/2001/XMLSchema-instance\",Local:\"nil\"},\"true\");err!=nil{return err}\nif err:=output.close();err!=nil{return err}\n}else{\n";
    payload = "(*" + payload + ')';
  }
  if (field.wrapper) { out += "if err:=output.open(" + qualified(*field.wrapper) + ");err!=nil{return err}\n"; }
  if (presence.collection) {
    const auto minimum = field.source->fixed_extent ? field.source->fixed_extent : field.min_occurs;
    const auto maximum = field.source->fixed_extent ? std::optional<std::size_t>{static_cast<std::size_t>(field.source->fixed_extent)} : field.max_occurs;
    if (minimum != 0U) { out += "if len(" + payload + ")<" + std::to_string(minimum) + "{return fmt.Errorf(\"XML occurrence mismatch\")}\n"; }
    if (maximum) { out += "if len(" + payload + ")>" + std::to_string(*maximum) + "{return fmt.Errorf(\"XML occurrence mismatch\")}\n"; }
    out += "for _,srlItem:=range " + payload + "{\n"; payload = "srlItem";
  }
  if (selected.type == object_type::class_type) {
    out += "if err:=" + payload + ".srlXmlEncode(output," + qualified(field.qualified_name) + ");err!=nil{return err}\n";
  } else {
    out += output_errors(encode_scalar(selected, payload, type)) + output_errors(scalar_facets(field, patterns, true));
    out += "if err:=output.open(" + qualified(field.qualified_name) + ");err!=nil{return err}\nif err:=output.text(srlText);err!=nil{return err}\nif err:=output.close();err!=nil{return err}\n";
  }
  if (presence.collection) { out += "}\n"; }
  if (field.wrapper) { out += "if err:=output.close();err!=nil{return err}\n"; }
  if (presence.allows_nil) { out += "}\n"; }
  if (presence.allows_absent) { out += "}\n"; }
  return out + "}\n";
}

// Decode each complete state before rebuilding the schema's exact pointer optional depth.
std::string decode_presence(const xml_binding::field& field, const std::string& access,
    const type_name_fn& type, const std::string& patterns) {
  const auto& presence = field.presence;
  const auto& selected = leaf(*field.source);
  std::string out{"{\nsrlState,srlErr:=input.presence(" + qualified(field.wrapper ? *field.wrapper : field.qualified_name) + ',' +
      (presence.allows_absent ? "true" : "false") + ',' + (presence.allows_nil ? "true" : "false") + ");if srlErr!=nil{return nil,srlErr}\nswitch srlState{\n"};
  if (presence.allows_absent) { out += "case 'a':" + access + "=nil\n"; }
  if (presence.allows_nil) {
    out += "case 'n':\n";
    out += presence.optional_depth == 2U ? "var srlNil " + type(field.source->type_name_list.front().arguments.front()) + "\n" + access + "=&srlNil\n" : access + "=nil\n";
  }
  out += "case 'v':\n";
  if (field.wrapper) { out += "srlWrapper,srlErr:=input.open(" + qualified(*field.wrapper) + ");if srlErr!=nil{return nil,srlErr}\n"; }
  if (presence.collection) {
    out += "srlPayload:=make([]" + type(selected) + ",0)\nfor{\nsrlPresent,srlErr:=input.has(" + qualified(field.qualified_name) + ");if srlErr!=nil{return nil,srlErr};if !srlPresent{break}\n";
    const auto maximum = field.source->fixed_extent ? std::optional<std::size_t>{static_cast<std::size_t>(field.source->fixed_extent)} : field.max_occurs;
    if (maximum) { out += "if len(srlPayload)>=" + std::to_string(*maximum) + "{return nil,fmt.Errorf(\"XML occurrence mismatch\")}\n"; }
  }
  if (selected.type == object_type::class_type) {
    out += "srlValue,srlErr:=srlXmlDecode" + record_name(type(selected)) + "(input," + qualified(field.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\n";
  } else {
    out += "srlText,srlErr:=input.scalar(" + qualified(field.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\n" + scalar_facets(field, patterns) + decode_scalar(selected, type);
  }
  if (presence.collection) {
    out += "srlPayload=append(srlPayload,srlValue)\n}\n";
    const auto minimum = field.source->fixed_extent ? field.source->fixed_extent : field.min_occurs;
    if (minimum != 0U) { out += "if len(srlPayload)<" + std::to_string(minimum) + "{return nil,fmt.Errorf(\"XML occurrence mismatch\")}\n"; }
  } else { out += "srlPayload:=srlValue\n"; }
  if (field.wrapper) { out += "if srlErr=srlWrapper.finish();srlErr!=nil{return nil,srlErr}\n"; }
  std::string payload{"srlPayload"};
  for (std::size_t depth{}; depth < presence.optional_depth; ++depth) {
    const auto name = "srlPresent" + std::to_string(depth);
    out += name + ":=&" + payload + "\n"; payload = name;
  }
  return out + access + '=' + payload + "\ndefault:return nil,fmt.Errorf(\"Invalid XML presence state\")\n}\n}\n";
}

// Join one native list privately, checking its item/text limits before emitting any field tokens.
std::string encode_list(const xml_binding::field& field, const std::string& access,
    const type_name_fn& type, const std::string& rule, std::size_t field_index, bool readonly) {
  const bool optional = field.source->type_name_list.front().is_optional();
  const auto selected = optional ? "(*" + access + ')' : access;
  std::string out{"{\n"};
  if (optional) { out += "if " + access + "!=nil{\n"; }
  out += "if len(" + selected + ")>output.budget.limits.MaxNodes{return srlXmlLimitError(\"XML list item limit exceeded\")}\n";
  if (field.source->fixed_extent != 0U) {
    out += "if len(" + selected + ")!=" + std::to_string(field.source->fixed_extent) + "{return fmt.Errorf(\"XML fixed list extent mismatch\")}\n";
  }
  out += "var srlJoined strings.Builder\nsrlListBytes:=0\nfor _,srlItem:=range " + selected + "{\n";
  out += output_errors(encode_scalar(leaf(*field.source), "srlItem", type));
  out += "srlTokens,srlErr:=srlXmlTokens(srlText,1);if srlErr!=nil||len(srlTokens)!=1||srlTokens[0]!=srlText{return fmt.Errorf(\"Invalid native XML list item\")}\nsrlListBytes,srlErr=output.listItem(srlText,&srlJoined,srlListBytes);if srlErr!=nil{return srlErr}\n}\nsrlText:=srlJoined.String()\n";
  out += output_errors(scalar_facets(field, rule, true));
  const auto qualify = [&] {
    if (!namespace_scalar(field)) { return std::string{}; }
    std::string result{"srlScope:=map[string]string(nil)\n"};
    if (!readonly) {
      result += "if scopes:=value.srlXmlNamespaces[" + std::to_string(field_index) +
          "];len(scopes)!=0{srlScope=scopes[0]}else if fallback:=value.srlXmlNamespaces[-1];len(fallback)!=0{srlScope=fallback[0]}\n";
    }
    return result + "srlQualified,srlNamespaceErr:=output.namespaceValue(srlText,1,srlScope);if srlNamespaceErr!=nil{return srlNamespaceErr};srlText=srlQualified\n";
  };
  if (field.attribute) {
    out += qualify();
    if (namespace_scalar(field)) { out += "{\n" + output_errors(scalar_facets(field, rule, true)) + "}\n"; }
    out += "if err:=output.attribute(" + qualified(field.qualified_name) + ",srlText);err!=nil{return err}\n";
  } else {
    if (!field.text) { out += "if err:=output.open(" + qualified(field.qualified_name) + ");err!=nil{return err}\n"; }
    out += qualify();
    if (namespace_scalar(field)) { out += "{\n" + output_errors(scalar_facets(field, rule, true)) + "}\n"; }
    out += "if err:=output.text(srlText);err!=nil{return err}\n";
    if (!field.text) { out += "if err:=output.close();err!=nil{return err}\n"; }
  }
  if (optional) { out += "}\n"; }
  return out + "}\n";
}

// Build a complete native list from one already selected scalar before assigning its owner field.
std::string decode_list_value(const xml_binding::field& field, const std::string& access,
    const type_name_fn& type, const std::string& rule, std::size_t field_index) {
  const auto& scalar = leaf(*field.source);
  std::string out{scalar_facets(field, rule)};
  if (namespace_scalar(field)) {
    out += "if value.srlXmlNamespaces==nil{value.srlXmlNamespaces=make(map[int][]map[string]string)}\n";
    out += "value.srlXmlNamespaces[" + std::to_string(field_index) + "]=[]map[string]string{srlScope}\n";
  }
  out += "srlTokens,srlTokenErr:=srlXmlTokens(srlText,input.budget.limits.MaxNodes);if srlTokenErr!=nil{return nil,srlTokenErr}\n";
  if (field.source->fixed_extent != 0U) {
    out += "if len(srlTokens)!=" + std::to_string(field.source->fixed_extent) + "{return nil,fmt.Errorf(\"XML fixed list extent mismatch\")}\n";
  }
  out += "srlValues:=make([]" + type(scalar) + ",0,len(srlTokens))\nfor _,srlText:=range srlTokens{\n" +
      decode_scalar(scalar, type) + "srlValues=append(srlValues,srlValue)\n}\n" + access +
      (field.source->type_name_list.front().is_optional() ? "=&srlValues\n" : "=srlValues\n");
  return out;
}

// Select optional attributes/children or required simple content without retaining XML subtrees.
std::string decode_list(const xml_binding::field& field, const std::string& access,
    const type_name_fn& type, const std::string& rule, std::size_t field_index) {
  const bool optional = field.source->type_name_list.front().is_optional();
  const auto& fallback = field.fixed_value ? field.fixed_value : field.default_value;
  std::string out{"{\n"};
  if (optional) { out += access + "=nil\n"; }
  if (field.attribute) {
    out += "srlText,srlPresent,srlErr:=fields.attribute(" + qualified(field.qualified_name) + ',' +
        (optional || fallback || field.min_occurs == 0U ? "false" : "true") + ");if srlErr!=nil{return nil,srlErr}\n";
    if (fallback) { out += "if !srlPresent{srlText=" + quote(*fallback) + ";srlPresent=true}\n"; }
    out += "if srlPresent{\n";
    if (namespace_scalar(field)) {
      out += "srlScope,srlNamespaceErr:=input.namespaceScope(srlText,1,nil);if srlNamespaceErr!=nil{return nil,srlNamespaceErr}\n";
    }
    out += decode_list_value(field, access, type, rule, field_index) + "}\n";
  } else if (field.text) {
    out += "if fields.used!=len(fields.attrs){return nil,fmt.Errorf(\"Unexpected XML attribute\")}\n";
    out += namespace_scalar(field)
        ? "srlText,srlScope,srlErr:=input.namespaceText(1);if srlErr!=nil{return nil,srlErr}\n"
        : "srlText,srlErr:=input.text();if srlErr!=nil{return nil,srlErr}\n";
    out += decode_list_value(field, access, type, rule, field_index);
  } else {
    if (optional) { out += "srlPresent,srlErr:=input.has(" + qualified(field.qualified_name) + ");if srlErr!=nil{return nil,srlErr};if srlPresent{\n"; }
    out += namespace_scalar(field)
        ? "srlText,srlScope,srlErr:=input.namespaceScalar(" + qualified(field.qualified_name) + ",1);if srlErr!=nil{return nil,srlErr}\n"
        : "srlText,srlErr:=input.scalar(" + qualified(field.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\n";
    out += decode_list_value(field, access, type, rule, field_index);
    if (optional) { out += "}\n"; }
  }
  return out + "}\n";
}

// Emit native mixed variants directly in order, including empty authored text fragments.
std::string encode_mixed(const xml_binding::mixed_contract& mixed, const std::string& access,
    const field_name& field, const alternative_name& alternative) {
  const auto storage = field(*mixed.variant);
  std::string out{"{\nsrlMixed:=srlXmlMixedBudget{limits:output.mixedLimits}\n"};
  if (mixed.source->fixed_extent != 0U) {
    out += "if len(" + access + ")!=" + std::to_string(mixed.source->fixed_extent) + "{return fmt.Errorf(\"XML mixed fixed extent mismatch\")}\n";
  }
  out += "for _,srlFragment:=range " + access + "{\nif srlFragment==nil{return fmt.Errorf(\"Nil XML mixed fragment\")}\nif err:=srlMixed.fragment();err!=nil{return err}\nswitch srlFragment." + storage + "Index{\ncase " +
      std::to_string(mixed.text_alternative) + ":\n";
  const auto text = "srlFragment." + storage + alternative(mixed.text_branch);
  out += "if err:=srlMixed.text(" + text + ");err!=nil{return err}\nif err:=output.text(" + text + ");err!=nil{return err}\n";
  for (const auto& child : mixed.children) {
    const auto selected = "srlFragment." + storage + alternative(child.branch);
    out += "case " + std::to_string(child.alternative) + ":\nif " + selected + "==nil{return fmt.Errorf(\"Nil XML mixed child\")}\nif err:=" +
        selected + ".srlXmlEncode(output," + qualified(child.qualified_name) + ");err!=nil{return err}\n";
  }
  return out + "default:return fmt.Errorf(\"Invalid XML mixed alternative\")\n}\n}\n}\n";
}

// Consume logical text runs/known children into a complete private native fragment sequence.
std::string decode_mixed(const xml_binding::mixed_contract& mixed, const std::string& access,
    const type_name_fn& type, const field_name& field, const alternative_name& alternative) {
  const auto fragment = type(leaf(*mixed.source));
  const auto create = "New" + record_name(fragment) + "()";
  const auto storage = field(*mixed.variant);
  std::string out{"{\nif fields.used!=len(fields.attrs){return nil,fmt.Errorf(\"Unexpected XML mixed attribute\")}\nsrlMixed:=srlXmlMixedBudget{limits:input.mixedLimits}\nsrlMixedValues:=make([]" +
      fragment + ",0)\nfor{\nsrlEvent,srlErr:=input.mixedEvent();if srlErr!=nil{return nil,srlErr}\nif srlEvent.kind=='t'{\nsrlText,srlErr:=input.mixedText(&srlMixed);if srlErr!=nil{return nil,srlErr};if srlText==\"\"{continue}\nsrlFragment:=" + create +
      "\nsrlFragment." + storage + "Index=" + std::to_string(mixed.text_alternative) + "\nsrlFragment." + storage + alternative(mixed.text_branch) +
      "=srlText\nsrlMixedValues=append(srlMixedValues,srlFragment)\ncontinue\n}\nif srlEvent.kind!='s'{break}\nswitch srlEvent.name{\n"};
  for (const auto& child : mixed.children) {
    out += "case " + qualified(child.qualified_name) + ":\nif srlErr=srlMixed.fragment();srlErr!=nil{return nil,srlErr}\nsrlChild,srlErr:=srlXmlDecode" +
        record_name(type(*child.value_type)) + "(input," + qualified(child.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\nsrlFragment:=" + create +
        "\nsrlFragment." + storage + "Index=" + std::to_string(child.alternative) + "\nsrlFragment." + storage + alternative(child.branch) +
        "=srlChild\nsrlMixedValues=append(srlMixedValues,srlFragment)\n";
  }
  out += "default:return nil,fmt.Errorf(\"Unknown XML mixed child\")\n}\n}\n";
  if (mixed.source->fixed_extent != 0U) {
    out += "if len(srlMixedValues)!=" + std::to_string(mixed.source->fixed_extent) + "{return nil,fmt.Errorf(\"XML mixed fixed extent mismatch\")}\n";
  }
  return out + access + "=srlMixedValues\n}\n";
}
} // namespace

bool supports_all(const xml_binding::record& contract) {
  if (contract.all_groups.size() != 1U || contract.all_groups.front().minimum > 1U ||
      !contract.choices.empty() || !contract.wildcards.empty() || contract.mixed ||
      contract.extensions) {
    return false;
  }
  const auto& alternatives = contract.all_groups.front().alternatives;
  std::set<const member*> remaining{alternatives.begin(), alternatives.end()};
  if (remaining.size() != alternatives.size()) { return false; }
  for (const auto& field : contract.fields) {
    if (field.attribute) { continue; }
    if (field.text || field.wrapper || field.repeated || field.min_occurs > 1U ||
        field.max_occurs != std::optional<std::size_t>{1U} || remaining.erase(field.source) != 1U) {
      return false;
    }
  }
  return remaining.empty();
}

bool supports_choices(const xml_binding::record& contract) {
  if (contract.choices.size() != 1U || contract.choices.front().repeated ||
      !contract.all_groups.empty() || !contract.wildcards.empty() || contract.mixed || contract.extensions) {
    return false;
  }
  const auto& choice = contract.choices.front();
  if (choice.minimum > 1U || choice.maximum != 1U || choice.alternatives.empty() ||
      choice.max_repetitions != std::optional<std::size_t>{1U} ||
      choice.branches.size() != choice.alternatives.size()) {
    return false;
  }
  std::set<const member*> remaining{choice.alternatives.begin(), choice.alternatives.end()};
  if (remaining.size() != choice.alternatives.size()) { return false; }
  for (const auto& branch : choice.branches) {
    if (!branch.source || branch.wildcard_index || !remaining.contains(branch.source) ||
        branch.minimum != 1U || branch.maximum != std::optional<std::size_t>{1U}) { return false; }
  }
  for (const auto& field : contract.fields) {
    if (!remaining.contains(field.source)) { continue; }
    const auto& storage = field.source->type_name_list.front();
    if (field.attribute || field.text || field.repeated || field.wrapper || field.list ||
        field.map_binding || field.variant_binding || field.presence.explicit_policy ||
        field.max_occurs != std::optional<std::size_t>{1U} || field.min_occurs != 0U ||
        !storage.is_optional() || storage.arguments.front().is_optional() ||
        storage.arguments.front().is_sequence()) {
      return false;
    }
    remaining.erase(field.source);
  }
  return remaining.empty();
}

std::string choice_check(const xml_binding::record& contract,
                         const field_name& access, std::string_view language) {
  if (contract.choices.empty()) { return {}; }
  if (!supports_choices(contract)) { throw std::invalid_argument{"Portable XML choices require one ordinary optional singleton group"}; }
  const bool java = language == "java", csharp = language == "csharp";
  const auto& choice = contract.choices.front();
  std::string out = java || csharp ? "{int srlChoiceCount=0;\n" : "{srlChoiceCount:=0\n";
  for (const auto* source : choice.alternatives) {
    const auto value = access(*source);
    if (java) {
      out += "if(" + value + "==null)throw new IllegalArgumentException(\"Null XML choice optional\");\n";
    }
    out += "if" + std::string{java || csharp ? "(" : " "} + value +
        (java ? ".isPresent())" : csharp ? ".HasValue)" : "!=nil") + "{srlChoiceCount++" +
        (java || csharp ? ";" : "") + "}\n";
  }
  const auto condition = "srlChoiceCount<" + std::to_string(choice.minimum) +
      "||srlChoiceCount>" + std::to_string(choice.maximum);
  if (java || csharp) {
    out += "if(" + condition + ")throw new " + (java ? "IllegalArgumentException" : "System.FormatException") +
        "(\"XML choice occurrence mismatch\");\n";
  } else { out += "if " + condition + "{return nil,fmt.Errorf(\"XML choice occurrence mismatch\")}\n"; }
  return out + "}\n";
}

bool needs_scalar_rule(const xml_binding::field& field) {
  const auto& facets = field.facets;
  return facets.min_inclusive || facets.max_inclusive || facets.min_exclusive || facets.max_exclusive ||
      facets.length || facets.min_length || facets.max_length || !facets.enumeration.empty() ||
      !facets.whitespace.empty() || facets.finite || !facets.patterns.empty() || !facets.pattern_groups.empty() ||
      !facets.list_item_kind.empty() || !facets.union_members.empty() || field.fixed_value || field.default_value ||
      (!facets.lexical_kind.empty() && facets.lexical_kind != "string");
}

// Share namespace adapter detection with managed record and list emitters.
bool namespace_dependent(const xml_binding::field& field) {
  return namespace_scalar(field);
}

bool supports_map(const xml_binding::field& field) {
  return field.map_binding && field.wrapper && *field.wrapper == field.map_binding->wrapper &&
      !field.attribute && !field.text && !field.repeated && !field.list && !field.qname &&
      !field.prefix_list && !field.value_projection && !field.default_value && !field.fixed_value &&
      !field.presence.explicit_policy && !field.variant_binding && !needs_scalar_rule(field) &&
      field.min_occurs == (field.map_binding->optional_container ? 0U : 1U) &&
      field.max_occurs == std::optional<std::size_t>{1U};
}

bool supports_variant(const xml_binding::field& field) {
  return field.variant_binding && field.wrapper && *field.wrapper == field.variant_binding->wrapper &&
      !field.attribute && !field.text && !field.repeated && !field.list && !field.qname &&
      !field.prefix_list && !field.value_projection && !field.default_value && !field.fixed_value &&
      !field.presence.explicit_policy && !field.map_binding && !needs_scalar_rule(field) &&
      field.min_occurs == 1U && field.max_occurs == std::optional<std::size_t>{1U};
}

bool supports_presence(const xml_binding::field& field) {
  return field.presence.value_type && field.presence.optional_depth <= 2U &&
      !field.list && !field.qname && !field.prefix_list && !field.value_projection &&
      !field.default_value && !field.fixed_value && !field.map_binding && !field.variant_binding &&
      (!field.presence.collection || field.presence.optional_depth == 0U || field.wrapper);
}

bool needs_presence(const xml_binding::field& field) {
  return !field.list && !field.attribute && !field.text && (field.presence.explicit_policy ||
      (field.presence.collection && field.presence.optional_depth != 0U));
}

std::string scalar_rule_name(std::size_t index, std::string_view language) {
  return std::string{language == "java" ? "srlXmlRule" : "SrlXmlRule"} + std::to_string(index);
}

std::string scalar_rule(const xml_binding::field& field, std::size_t index, std::string_view language) {
  if (!needs_scalar_rule(field)) { return {}; }
  if (language != "java" && language != "csharp") {
    throw std::invalid_argument{"Unsupported portable XML scalar-rule backend"};
  }
  const bool java = language == "java";
  const auto& value = leaf(*field.source);
  const bool number = value.type == object_type::primitive &&
      (value.name.starts_with("int") || value.name.starts_with("uint") ||
       value.name == "float" || value.name == "double");
  const auto string_array = java ? "new String[]{" : "new string[]{";
  const auto bound = [](const std::optional<std::string>& text) { return quote(text.value_or("")); };
  const auto extent = [](const std::optional<std::size_t> count) {
    if (count && *count > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      throw std::invalid_argument{"Portable XML facet length exceeds the target signed 64-bit range"};
    }
    return count ? std::to_string(*count) + 'L' : "-1L";
  };
  // Union branches retain independent restrictions and share the operation's bounded evaluator.
  const auto rule = [&](auto&& self, const xml_binding::scalar_facets& facets,
                        const std::optional<std::string>& fixed) -> std::string {
  std::string result = "new SrlXmlRule(" + quote(facets.lexical_kind) + ',' +
      quote(facets.whitespace) + ',' + (number ? bits(value) : "0") + ',' + string_array +
      bound(facets.min_inclusive) + ',' + bound(facets.max_inclusive) + ',' + bound(facets.min_exclusive) + ',' +
      bound(facets.max_exclusive) + "}," + extent(facets.length) + ',' + extent(facets.min_length) + ',' +
      extent(facets.max_length) + ',' + string_array;
  for (const auto& item : facets.enumeration) { result += quote(item) + ','; }
  result += "}," + std::string{facets.finite ? "true" : "false"} + ",new SrlXmlRule.Pattern[][]{";
  auto groups = facets.pattern_groups;
  if (!facets.patterns.empty()) { groups.push_back(facets.patterns); }
  // String fragments keep every JVM constant and its initialization bytecode comfortably bounded.
  constexpr std::size_t maximum_fragment_bytes{10000U};
  for (const auto& group : groups) {
    result += "new SrlXmlRule.Pattern[]{";
    for (const auto& expression : group) {
      const auto blob = pattern_blob(xml::compile_portable_pattern(expression));
      result += std::string{"SrlXmlRule.Pattern."} + (java ? "prepare(" : "Prepare(") + string_array;
      for (std::size_t offset{}; offset < blob.size(); offset += maximum_fragment_bytes) {
        result += quote(std::string_view{blob}.substr(offset, maximum_fragment_bytes)) + ',';
      }
      result += "}),";
    }
    result += "},";
  }
  result += "}," + quote(facets.list_item_kind) + ",new SrlXmlRule[]{";
  for (const auto& branch : facets.union_members) { result += self(self, branch, {}) + ','; }
  return result + "}," + (fixed ? quote(*fixed) : "null") + ')';
  };
  return std::string{java ? "private static final SrlXmlRule " : "private static readonly SrlXmlRule "} +
      scalar_rule_name(index, language) + " = " + rule(rule, field.facets, field.fixed_value) + ';';
}

std::string scalar_check(const xml_binding::field& field, std::size_t index,
                         const std::string& expression, std::string_view language) {
  return needs_scalar_rule(field) ? scalar_rule_name(index, language) +
      (language == "java" ? ".check(" : ".Check(") + expression + ')' : expression;
}

std::string scalar_input(const xml_binding::field& field, std::size_t index,
                         const std::string& expression, std::string_view language) {
  const auto namespaced = [&] {
    if (!namespace_scalar(field) || expression == "srlLexical" || expression == "srlLexical!") {
      return expression;
    }
    const bool java = language == "java";
    const auto prefix = "result.srlXmlNamespaces.";
    if (!field.attribute && !field.text) {
      return std::string{prefix} + (java ? "scalar(input," : "Scalar(input,") + std::to_string(index) + ',' +
          quote(field.qualified_name.namespace_uri.empty() ? field.qualified_name.local :
              '{' + field.qualified_name.namespace_uri + '}' + field.qualified_name.local) + ',' +
          namespace_mode(field) + ')';
    }
    return std::string{prefix} + (java ? "capture(input," : "Capture(input,") + std::to_string(index) +
        ',' + expression + ",srlNamespaceScope," + namespace_mode(field) + ')' + (java ? "" : "!");
  }();
  const auto& fallback = field.fixed_value ? field.fixed_value : field.default_value;
  const auto text = fallback ? std::string{"SrlXmlRule."} +
      (language == "java" ? "defaultText(" : "DefaultText(") + expression + ',' +
      quote(*fallback) + ',' + (field.attribute ? "true" : "false") + ')' : namespaced;
  return needs_scalar_rule(field) ? scalar_rule_name(index, language) +
      (language == "java" ? ".check(" : ".Check(") + text + ',' +
      (language == "java" ? "input.listLimit(),input.budget" : "input.ListLimit,input.Operation") + ')' : text;
}

std::string scalar_output(const xml_binding::field& field, std::size_t index,
                          const std::string& expression, std::string_view language) {
  const auto& fallback = field.fixed_value ? field.fixed_value : field.default_value;
  const bool java = language == "java";
  const auto text = namespace_scalar(field) ? std::string{"srlXmlNamespaces."} +
      (java ? "output(output," : "Output(output,") + std::to_string(index) + ',' +
      (typed_list(field) ? "0" : "srlNamespaceIndex++") + ',' + expression + ',' + namespace_mode(field) + ')' : expression;
  if (!needs_scalar_rule(field)) { return text; }
  const auto limit = java ? "output.listLimit(),output.budget" : "output.ListLimit,output.Operation";
  return scalar_rule_name(index, language) + (fallback && !field.attribute ?
      (java ? ".output(" : ".Output(") + text + ',' + quote(*fallback) + ',' + limit + ')' :
      (java ? ".check(" : ".Check(") + text + ',' + limit + ')');
}

bool typed_list(const xml_binding::field& field) {
  if (!field.list) { return false; }
  const auto* storage = &field.source->type_name_list.front();
  if (storage->is_optional()) { storage = &storage->arguments.front(); }
  return storage->is_sequence() || field.source->modifier == member::modifier_type::array;
}

std::string list_write(const xml_binding::field& field, std::size_t index,
    const std::string& access, const scalar_codec& scalar, std::string_view language) {
  const bool java = language == "java";
  const bool optional = field.source->type_name_list.front().is_optional();
  const auto& selected_type = leaf(*field.source);
  const auto expanded = [](const xml::name& value) { return quote(value.namespace_uri.empty() ? value.local : "{" + value.namespace_uri + "}" + value.local); };
  std::string result{"{\n"}, selected{access};
  if (optional) {
    result += java ? "if(" + access + ".isPresent()){\n" : "var srlOptional=" + access + ";if(srlOptional.HasValue){\n";
    selected = java ? access + ".get()" : "srlOptional.Value.Item1";
  }
  result += java ? "if(" + selected + "==null)throw new IllegalArgumentException(\"Null XML list\");\nvar srlList=new StringBuilder();int srlCount=0,srlListBytes=0;\nfor(var srlItem:" + selected + "){\nif(srlCount++>=output.listLimit())throw SrlXmlRule.quota(\"XML list item limit exceeded\");\n" :
      "if(" + selected + " is null)throw new System.FormatException(\"Null XML list\");\nvar srlList=new System.Text.StringBuilder();int srlCount=0,srlListBytes=0;\nforeach(var srlItem in " + selected + "){\nif(srlCount++>=output.ListLimit)throw SrlXmlBudget.Limit(\"XML list item limit exceeded\",output.Operation);\n";
  result += "var srlToken=" + scalar(selected_type, "srlItem", true) + ";\n";
  // Empty/whitespace-containing native strings cannot represent exactly one lexical list item.
  result += java ? "if(SrlXmlRule.tokens(srlToken).length!=1||!SrlXmlRule.tokens(srlToken)[0].equals(srlToken))throw new IllegalArgumentException(\"Invalid native XML list item\");\nsrlListBytes=output.listItem(srlToken,srlList,srlListBytes);\n}\n" :
      "if(SrlXmlRule.Tokens(srlToken,output.Operation).Length!=1||SrlXmlRule.Tokens(srlToken,output.Operation)[0]!=srlToken)throw new System.FormatException(\"Invalid native XML list item\");\nsrlListBytes=output.ListItem(srlToken,srlList,srlListBytes);\n}\n";
  if (field.source->fixed_extent != 0U) {
    result += "if(srlCount!=" + std::to_string(field.source->fixed_extent) + ")throw new " +
        (java ? "IllegalArgumentException" : "System.FormatException") + "(\"XML fixed list extent mismatch\");\n";
  }
  const auto checked = scalar_output(field, index, java ? "srlList.toString()" : "srlList.ToString()", language);
  if (field.attribute) { result += java ? "output.attribute(" : "output.Attribute("; result += expanded(field.qualified_name) + ',' + checked + ");\n"; }
  else if (field.text) { result += std::string{java ? "output.text(" : "output.Text("} + checked + ");\n"; }
  else {
    result += std::string{java ? "output.open(" : "output.Open("} + expanded(field.qualified_name) + ");\n" +
        (java ? "output.text(" : "output.Text(") + checked + ");\n" + (java ? "output.close();\n" : "output.Close();\n");
  }
  if (optional) { result += "}\n"; }
  return result + "}\n";
}

std::string list_read(const xml_binding::field& field, std::size_t index,
    const std::string& access, const std::string& expression, const type_name_fn& type,
    const scalar_codec& scalar, std::string_view language, const std::string& array_initializer) {
  const bool java = language == "java";
  const bool optional = field.source->type_name_list.front().is_optional();
  const auto& selected_type = leaf(*field.source);
  const auto item_type = type(selected_type);
  const bool defaulted = field.default_value || field.fixed_value;
  const auto lexical = namespace_scalar(field) ? (field.attribute || field.text ?
      std::string{"result.srlXmlNamespaces."} + (java ? "capture(input," : "Capture(input,") +
          std::to_string(index) + ',' + expression + ",srlNamespaceScope," + namespace_mode(field) + ')' :
      std::string{"result.srlXmlNamespaces."} + (java ? "scalar(input," : "Scalar(input,") +
          std::to_string(index) + ',' + quote(field.qualified_name.namespace_uri.empty() ?
              field.qualified_name.local : '{' + field.qualified_name.namespace_uri + '}' + field.qualified_name.local) +
          ',' + namespace_mode(field) + ')') : expression;
  std::string result{"{\nvar srlLexical=" + lexical + ";\n"};
  if (optional && !defaulted) { result += "if(srlLexical==null){" + access + (java ? "=java.util.Optional.empty();" : "=null;") + "}else{\n"; }
  result += "var srlListText=" + scalar_input(field, index, java || defaulted ? "srlLexical" : "srlLexical!", language) + ";\n";
  result += java ? "var srlTokens=SrlXmlRule.tokens(srlListText,input.listLimit());\n" : "var srlTokens=SrlXmlRule.Tokens(srlListText,input.ListLimit,input.Operation);\n";
  if (field.source->fixed_extent != 0U) {
    result += "if(srlTokens." + std::string{java ? "length" : "Length"} + "!=" +
        std::to_string(field.source->fixed_extent) + ")throw new " +
        (java ? "IllegalArgumentException" : "System.FormatException") + "(\"XML fixed list extent mismatch\");\n";
  }
  result += java ? "var srlValues=new java.util.ArrayList<" + item_type + ">();\nfor(String srlToken:srlTokens){\n" :
      "var srlValues=new System.Collections.Generic.List<" + item_type + ">();\nforeach(string srlToken in srlTokens){\n";
  result += std::string{java ? "srlValues.add(" : "srlValues.Add("} + scalar(selected_type, "srlToken", false) + ");\n}\n";
  std::string container{"srlValues"}, container_type{java ? "java.util.List<" + item_type + '>' : "System.Collections.Generic.List<" + item_type + '>'};
  if (java && field.source->modifier == member::modifier_type::array) {
    result += "var srlArray=" + array_initializer + ";\nfor(int srlIndex=0;srlIndex<srlValues.size();++srlIndex)srlArray[srlIndex]=srlValues.get(srlIndex);\n";
    container = "srlArray"; container_type = item_type + "[]";
  }
  result += access + '=' + (optional ? (java ? "java.util.Optional.of(" + container + ')' :
      "new System.ValueTuple<" + container_type + ">(" + container + ')') : container) + ";\n";
  if (optional && !defaulted) { result += "}\n"; }
  return result + "}\n";
}

std::string scalar_runtime(std::string_view language) {
  const xml::pattern_limits limits{};
  const bool java = language == "java";
  if (!java && language != "csharp") {
    throw std::invalid_argument{"Unsupported portable XML scalar runtime"};
  }
  const std::string declaration = java ? "private static final int " : "private const int ";
  const std::string body = java ? std::string{
#include "xml_java_scalar_runtime.inc"
      } : std::string{
#include "xml_csharp_scalar_runtime.inc"
      };
  const std::string names = java ? std::string{
#include "xml_java_namespace_runtime.inc"
      } : std::string{
#include "xml_csharp_namespace_runtime.inc"
      };
  return std::string{java ? "private static final class SrlXmlRule {\n" : "private sealed class SrlXmlRule {\n"} +
      declaration + "MAX_STATES=" + std::to_string(limits.max_states) + ";\n" +
      declaration + "MAX_INPUT_BYTES=" + std::to_string(limits.max_input_bytes) + ";\n" +
      declaration + "MAX_LIST_ITEMS=" + std::to_string(xml::limits{}.max_nodes) + ";\n" +
      declaration + "MAX_WORK_UNITS=" + std::to_string(limits.max_work_units) + ";\n" + body + names;
}

std::string map_runtime(std::string_view language) {
  const xml::limits limits{};
  if (language == "java") {
    return "/** Immutable per-wrapper map admissions, independent of ordinary native limits. */\n"
        "public static final class XmlMapLimits {\npublic final int maxEntries,maxKeyBytes;\n"
        "/** Use the shared XML map defaults. */\npublic XmlMapLimits(){this(" +
        std::to_string(limits.max_map_entries) + ',' + std::to_string(limits.max_map_key_bytes) + ");}\n"
        "/** Zero is a real bound; negative admissions reject before an operation starts. */\n"
        "public XmlMapLimits(int entries,int keyBytes){if(entries<0||keyBytes<0)throw new IllegalArgumentException(\"Negative XML map limit\");maxEntries=entries;maxKeyBytes=keyBytes;}\n"
        "/** Charge lexical aliases before typed insertion discards their original spelling. */\n"
        "int keyBytes(String lexical,String canonical,int used){int count=java.lang.Math.max(utf8(lexical).length,utf8(canonical).length);if(count>maxKeyBytes-used)throw SrlXmlRule.quota(\"XML map key byte limit exceeded\");return used+count;}\n}\n";
  }
  if (language == "csharp") {
    return "// Immutable per-wrapper map admissions, independent of ordinary native limits.\n"
        "public sealed class XmlMapLimits {\npublic readonly int MaxEntries,MaxKeyBytes;\n"
        "// Zero is a real bound; negative admissions reject before an operation starts.\n"
        "public XmlMapLimits(int maxEntries=" + std::to_string(limits.max_map_entries) +
        ",int maxKeyBytes=" + std::to_string(limits.max_map_key_bytes) + "){if(maxEntries<0||maxKeyBytes<0)throw new System.ArgumentOutOfRangeException(nameof(maxEntries),\"Negative XML map limit\");MaxEntries=maxEntries;MaxKeyBytes=maxKeyBytes;}\n"
        "// Charge lexical aliases before typed insertion discards their original spelling.\n"
        "internal int KeyBytes(string lexical,string canonical,int used,Limits.IOperation? operation=null){int count=System.Math.Max(SrlUtf8.GetByteCount(lexical),SrlUtf8.GetByteCount(canonical));if(count>MaxKeyBytes-used)throw SrlXmlBudget.Limit(\"XML map key byte limit exceeded\",operation);return used+count;}\n}\n";
  }
  throw std::invalid_argument{"Unsupported portable XML map runtime"};
}

std::string mixed_runtime(std::string_view language) {
  const xml::limits limits{};
  if (language == "java") {
    return "private static final int SRL_XML_MIXED_MAX_FRAGMENTS=" + std::to_string(limits.max_mixed_fragments) +
        ",SRL_XML_MIXED_MAX_TEXT_BYTES=" + std::to_string(limits.max_mixed_text_bytes) + ";\n" + std::string{
#include "xml_java_mixed_runtime.inc"
        };
  }
  if (language == "csharp") {
    return "private const int SrlXmlMixedMaxFragments=" + std::to_string(limits.max_mixed_fragments) +
        ",SrlXmlMixedMaxTextBytes=" + std::to_string(limits.max_mixed_text_bytes) + ";\n" + std::string{
#include "xml_csharp_mixed_runtime.inc"
        };
  }
  throw std::invalid_argument{"Unsupported portable XML mixed runtime"};
}

std::string identity_metadata(const xml::identity_scope& scope, std::string_view language) {
  const bool go = language == "go";
  if (!go && language != "java" && language != "csharp") {
    throw std::invalid_argument{"Unsupported portable XML identity metadata"};
  }
  const auto name = [](const xml::name& value) {
    return quote(value.namespace_uri.empty() ? value.local : "{" + value.namespace_uri + "}" + value.local);
  };
  const auto path = [&](const xml::identity_path& value) {
    std::string result = go ? "srlXmlIdentityPath{steps:[]string{" : "new SrlXmlIdentityPath(new " +
        std::string{language == "java" ? "String" : "string"} + "[]{";
    for (const auto& step : value.steps) { result += name(step) + ','; }
    result += go ? "},descendants:" : "},";
    result += value.descendants ? "true" : "false";
    result += go ? ",attribute:" : ",";
    result += value.attribute ? name(*value.attribute) : go ? "\"\"" : "null";
    return result + (go ? "}" : ")");
  };
  std::string result = go ? "srlXmlIdentityScope{constraints:[]srlXmlIdentityConstraint{" :
      "new SrlXmlIdentityScope(new SrlXmlIdentityConstraint[]{";
  for (const auto& constraint : scope.constraints) {
    const auto kind = constraint.kind == xml::identity_kind::key ? "key" :
        constraint.kind == xml::identity_kind::unique ? "unique" : "keyref";
    result += go ? "{kind:" : "new SrlXmlIdentityConstraint(";
    result += quote(kind) + (go ? ",selector:" : ",") + path(constraint.selector) +
        (go ? ",fields:[]srlXmlIdentityField{" : ",new SrlXmlIdentityField[]{");
    for (const auto& field : constraint.fields) {
      result += go ? "{path:" : "new SrlXmlIdentityField(";
      result += path(field.path) + (go ? ",kind:" : ",") + quote(field.lexical_kind) +
          (go ? ",nillable:" : ",") + (field.nillable ? "true" : "false") + (go ? "}," : "),");
    }
    std::ptrdiff_t refer{-1};
    if (constraint.refer) {
      const auto target = std::find_if(scope.constraints.begin(), scope.constraints.end(),
          [&](const auto& value) { return value.id == *constraint.refer; });
      if (target == scope.constraints.end()) { throw std::invalid_argument{"XML identity target is outside its owner scope"}; }
      refer = std::distance(scope.constraints.begin(), target);
    }
    result += std::string{go ? "},refer:" : "},"} + std::to_string(refer);
    if (language == "csharp") { result += ',' + quote(constraint.id.local); }
    result += go ? "}," : "),";
  }
  return result + (go ? "}}" : "})");
}

std::string identity_runtime(std::string_view language) {
  const xml::identity_limits limits{};
  const bool java = language == "java", go = language == "go";
  if (!go && !java && language != "csharp") { throw std::invalid_argument{"Unsupported portable XML identity runtime"}; }
  const std::pair<std::string_view, std::size_t> values[]{
      {"Constraints", limits.max_constraints}, {"Fields", limits.max_fields}, {"Depth", limits.max_depth},
      {"SelectedNodes", limits.max_selected_nodes}, {"Tuples", limits.max_tuples},
      {"TupleBytes", limits.max_tuple_bytes}, {"WorkUnits", limits.max_work_units}};
  const std::string_view java_names[]{"CONSTRAINTS", "FIELDS", "DEPTH", "SELECTED_NODES", "TUPLES", "TUPLE_BYTES", "WORK_UNITS"};
  std::string result{};
  for (std::size_t index{}; index < std::size(values); ++index) {
    const auto [suffix, value] = values[index];
    result += go ? "const srlXmlIdentityMax" : java ? "private static final int SRL_XML_IDENTITY_MAX_" : "private const int SrlXmlIdentityMax";
    result += java ? java_names[index] : suffix;
    result += '=' + std::to_string(value) + (go ? "\n" : ";\n");
  }
  if (go) { return result; }
  return result + (java ? std::string{
#include "xml_java_identity_runtime.inc"
      } : std::string{
#include "xml_csharp_identity_runtime.inc"
      });
}

std::string html_runtime(std::string_view language) {
  const html::limits limits{};
  const bool java = language == "java", go = language == "go";
  if (!go && !java && language != "csharp") { throw std::invalid_argument{"Unsupported portable HTML runtime"}; }
  const std::pair<std::string_view, std::size_t> values[]{
      {"InputBytes", limits.max_input_bytes}, {"OutputBytes", limits.max_output_bytes},
      {"Events", limits.max_events}, {"Depth", limits.max_depth},
      {"AttributesPerElement", limits.max_attributes_per_element}, {"TextBytes", limits.max_text_bytes}};
  const std::string_view java_names[]{"INPUT_BYTES", "OUTPUT_BYTES", "EVENTS", "DEPTH", "ATTRIBUTES_PER_ELEMENT", "TEXT_BYTES"};
  std::string result{};
  for (std::size_t index{}; index < std::size(values); ++index) {
    const auto [suffix, value] = values[index];
    result += go ? "const srlHtmlMax" : java ? "private static final int SRL_HTML_MAX_" : "private const int SrlHtmlMax";
    result += java ? java_names[index] : suffix;
    result += '=' + std::to_string(value) + (go ? "\n" : ";\n");
  }
  if (go) { return result; }
  return result + (java ? std::string{
#include "xml_java_html_runtime.inc"
      } : std::string{
#include "xml_csharp_html_runtime.inc"
      });
}

std::string model_runtime(std::string_view language) {
  const auto bound = std::to_string(xml::limits{}.max_digest_bytes);
  if (language == "go") { return "const srlXmlDefaultDigestBytes=" + bound + "\n"; }
  if (language == "java") {
    return "private static final int srlXmlDefaultDigestBytes=" + bound + ";\n" + std::string{
#include "xml_java_model_runtime.inc"
        };
  }
  if (language == "csharp") {
    return "private const int SrlXmlDefaultDigestBytes=" + bound + ";\n" + std::string{
#include "xml_csharp_model_runtime.inc"
        };
  }
  throw std::invalid_argument{"Unsupported portable XML native model runtime"};
}

std::string extension_runtime(std::string_view language) {
  const auto constants = xml_codegen::portable_extension_constants(language);
  if (language == "java") { return constants + std::string{
#include "xml_java_extension_runtime.inc"
      }; }
  if (language == "csharp") { return constants + std::string{
#include "xml_csharp_extension_runtime.inc"
      }; }
  if (language == "go") { return constants; }
  throw std::invalid_argument{"Unsupported portable XML extension runtime"};
}

std::string mixed_write(const xml_binding::mixed_contract& mixed, const std::string& access,
    const field_name& field, const alternative_name& alternative, std::string_view language) {
  const bool java = language == "java";
  const auto expanded = [](const xml::name& name) { return quote(name.namespace_uri.empty() ? name.local : "{" + name.namespace_uri + "}" + name.local); };
  const auto error = java ? "IllegalArgumentException" : "System.FormatException";
  const auto storage = java ? field(*mixed.variant) : "SrlXmlMixed";
  const auto count = access + (java ? mixed.source->modifier == member::modifier_type::array ? ".length" : ".size()" : ".Count");
  std::string out{"{\nif(" + access + (java ? "==null" : " is null") + ")throw new " + error + "(\"Null XML mixed sequence\");\n"};
  if (mixed.source->fixed_extent != 0U) {
    out += "if(" + count + "!=" + std::to_string(mixed.source->fixed_extent) + ")throw new " + error + "(\"XML mixed fixed extent mismatch\");\n";
  }
  out += java ? "var srlMixed=new SrlXmlMixedBudget(output.mixedLimits);\nfor(var srlFragment:" + access + "){\n" :
      "var srlMixed=new SrlXmlMixedBudget(output.MixedLimits,output.Operation);\nforeach(var srlFragment in " + access + "){\n";
  out += "if(srlFragment" + std::string{java ? "==null" : " is null"} + ")throw new " + error + "(\"Null XML mixed fragment\");\nsrlMixed." +
      (java ? "fragment();\n" : "Fragment();\n") + "switch(srlFragment." + storage + "Index){\n";
  const auto text = "srlFragment." + storage + alternative(mixed.text_branch);
  out += "case " + std::to_string(mixed.text_alternative) + ":\nsrlMixed." + (java ? "text(" : "Text(") + text + ");\n" +
      (java ? "output.text(" : "output.Text(") + text + ");break;\n";
  for (const auto& child : mixed.children) {
    const auto selected = "srlFragment." + storage + alternative(child.branch);
    out += "case " + std::to_string(child.alternative) + ":\nif(" + selected + (java ? "==null" : " is null") +
        ")throw new " + error + "(\"Null XML mixed child\");\n" + selected + (java ? ".xmlWrite(output," : ".XmlWrite(output,") +
        expanded(child.qualified_name) + ");break;\n";
  }
  return out + "default:throw new " + error + "(\"Invalid XML mixed alternative\");\n}\n}\n}\n";
}

std::string mixed_read(const xml_binding::mixed_contract& mixed, const std::string& access,
    const type_name_fn& type, const field_name& field, const alternative_name& alternative,
    std::string_view language, const std::string& array_initializer) {
  const bool java = language == "java";
  const auto error = java ? "IllegalArgumentException" : "System.FormatException";
  const auto storage = java ? field(*mixed.variant) : "SrlXmlMixed";
  const auto fragment = type(leaf(*mixed.source));
  const auto expanded = [](const xml::name& name) { return quote(name.namespace_uri.empty() ? name.local : "{" + name.namespace_uri + "}" + name.local); };
  std::string out{"{\n"};
  out += java ? "if(!fields.attributes.isEmpty())throw new IllegalArgumentException(\"Unexpected XML mixed attribute\");\nvar srlMixed=new SrlXmlMixedBudget(input.mixedLimits);\nvar srlMixedValues=new java.util.ArrayList<" + fragment + ">();\n" :
      "if(fields.Attributes.Count!=0)throw new System.FormatException(\"Unexpected XML mixed attribute\");\nvar srlMixed=new SrlXmlMixedBudget(input.MixedLimits,input.Operation);\nvar srlMixedValues=new System.Collections.Generic.List<" + fragment + ">();\n";
  const auto kind = java ? "input.kind" : "input.Kind";
  out += "while(true){\nif(" + std::string{kind} + "=='t'){\nvar srlText=input." + (java ? "mixedText(srlMixed)" : "MixedText(srlMixed)") + ";\n";
  out += java ? "if(srlText.isEmpty())continue;\n" : "if(srlText.Length==0)continue;\n";
  out += "var srlFragment=new " + fragment + "();\nsrlFragment." + storage + "Index=" + std::to_string(mixed.text_alternative) +
      ";\nsrlFragment." + storage + alternative(mixed.text_branch) + "=srlText;\n" +
      (java ? "srlMixedValues.add(srlFragment);\n" : "srlMixedValues.Add(srlFragment);\n") + "continue;\n}\nif(" + kind + "!='s')break;\n";
  out += java ? "switch(input.name()){\n" : "switch(input.Name){\n";
  for (const auto& child : mixed.children) {
    out += "case " + expanded(child.qualified_name) + ":{\nsrlMixed." + (java ? "fragment();\n" : "Fragment();\n") +
        "var srlFragment=new " + fragment + "();\nsrlFragment." + storage + "Index=" + std::to_string(child.alternative) +
        ";\nsrlFragment." + storage + alternative(child.branch) + '=' + type(*child.value_type) +
        (java ? ".xmlRead(input," : ".XmlRead(input,") + expanded(child.qualified_name) + ");\n" +
        (java ? "srlMixedValues.add(srlFragment);\n" : "srlMixedValues.Add(srlFragment);\n") + "break;\n}\n";
  }
  out += "default:throw new " + std::string{error} + "(\"Unknown XML mixed child\");\n}\n}\n";
  const auto count = java ? "srlMixedValues.size()" : "srlMixedValues.Count";
  if (mixed.source->fixed_extent != 0U) {
    out += "if(" + std::string{count} + "!=" + std::to_string(mixed.source->fixed_extent) + ")throw new " + error + "(\"XML mixed fixed extent mismatch\");\n";
  }
  if (java && mixed.source->modifier == member::modifier_type::array) {
    out += access + '=' + array_initializer + ";\nfor(int srlIndex=0;srlIndex<srlMixedValues.size();++srlIndex)" + access + "[srlIndex]=srlMixedValues.get(srlIndex);\n";
  } else { out += access + "=srlMixedValues;\n"; }
  return out + "}\n";
}

std::string map_write(const xml_binding::map_contract& map, const std::string& access,
    const type_name_fn& type, const scalar_codec& scalar, std::string_view language,
    const std::string& key_comparator) {
  const bool java = language == "java";
  const std::string failure = java ? "throw new IllegalArgumentException(" : "throw new System.FormatException(";
  const auto expanded = [](const xml::name& name) { return quote(name.namespace_uri.empty() ? name.local : '{' + name.namespace_uri + '}' + name.local); };
  const std::string output_open = java ? "output.open(" : "output.Open(";
  const std::string output_close = java ? "output.close();\n" : "output.Close();\n";
  const std::string selected = map.optional_container ? access + (java ? ".get()" : ".Value.Item1") : access;
  const std::string limits = java ? "output.mapLimits" : "output.MapLimits";
  const std::string size = selected + (java ? ".size()" : ".Count");
  std::string out{"{\n"};
  if (map.optional_container) { out += "if(" + access + (java ? ".isPresent()" : ".HasValue") + "){\n"; }
  out += "if(" + selected + (java ? "==null)" : " is null)") + failure + "\"Null XML map storage\");\n";
  out += "if(" + size + '>' + limits + (java ? ".maxEntries)throw SrlXmlRule.quota(" : ".MaxEntries)throw SrlXmlBudget.Limit(") + "\"XML map entry limit exceeded\"" + (java ? ");\n" : ",output.Operation);\n");
  out += output_open + expanded(map.wrapper) + ");\n";
  out += "var srlKeys=new " + std::string{java ? "java.util.ArrayList<" : "System.Collections.Generic.List<"} +
      type(map.key_type) + ">(" + selected + (java ? ".keySet()" : ".Keys") + ");\n";
  if (java) {
    out += "for(var srlKey:srlKeys)if(srlKey==null)" + failure + "\"Null XML map key\");\n";
    out += "srlKeys.sort(" + key_comparator + ");\nint srlKeyBytes=0;\nfor(var srlKey:srlKeys){\nvar srlItem=" + selected + ".get(srlKey);\n";
  } else {
    out += "srlKeys.Sort(SrlCompare);\nint srlKeyBytes=0;\nforeach(var srlKey in srlKeys){\nvar srlItem=" + selected + "[srlKey];\n";
  }
  if (java || map.value_type.type == object_type::class_type || map.value_type.name == "string") {
    out += std::string{"if(srlItem"} + (java ? "==null)" : " is null)") + failure + "\"Null XML map value\");\n";
  }
  out += std::string{java ? "String srlKeyText=" : "string srlKeyText="} + scalar(map.key_type, "srlKey", true) + ";\n";
  out += "srlKeyBytes=" + limits + (java ? ".keyBytes(" : ".KeyBytes(") + "srlKeyText,srlKeyText,srlKeyBytes" + (java ? ");\n" : ",output.Operation);\n");
  out += output_open + expanded(map.entry) + ");\n";
  if (map.key_placement == xml_binding::map_key_placement::attribute) {
    out += std::string{java ? "output.attribute(" : "output.Attribute("} + expanded(map.key) + ",srlKeyText);\n";
  } else {
    out += output_open + expanded(map.key) + ");\n" + (java ? "output.text(srlKeyText);\n" : "output.Text(srlKeyText);\n") + output_close;
  }
  if (map.value_type.type == object_type::class_type) {
    out += std::string{"srlItem."} + (java ? "xmlWrite(output," : "XmlWrite(output,") + expanded(map.value) + ");\n";
  } else {
    out += output_open + expanded(map.value) + ");\n" + (java ? "output.text(" : "output.Text(") + scalar(map.value_type, "srlItem", true) + ");\n" + output_close;
  }
  out += output_close + "}\n" + output_close;
  if (map.optional_container) { out += "}\n"; }
  return out + "}\n";
}

std::string map_read(const xml_binding::map_contract& map, const std::string& access,
    const type_name_fn& type, const scalar_codec& scalar, std::string_view language,
    const std::string& initializer) {
  const bool java = language == "java";
  const std::string failure = java ? "throw new IllegalArgumentException(" : "throw new System.FormatException(";
  const auto expanded = [](const xml::name& name) { return quote(name.namespace_uri.empty() ? name.local : '{' + name.namespace_uri + '}' + name.local); };
  const std::string has = java ? "input.has(" : "input.Has(";
  const std::string open = java ? "input.open(" : "input.Open(";
  const std::string scalar_input = java ? "input.scalar(" : "input.Scalar(";
  const std::string finish = java ? ".finish();\n" : ".Finish();\n";
  const std::string limits = java ? "input.mapLimits" : "input.MapLimits";
  std::string out = std::string{java ? "{\nboolean srlPresent=" : "{\nbool srlPresent="} + has + expanded(map.wrapper) + ");\n";
  if (map.optional_container) { out += access + (java ? "=java.util.Optional.empty();\n" : "=null;\n"); }
  else { out += "if(!srlPresent)" + failure + "\"Required XML map wrapper missing\");\n"; }
  out += "if(srlPresent){\nvar srlWrapper=" + open + expanded(map.wrapper) + ");\n";
  out += "if(" + std::string{java ? "!srlWrapper.attributes.isEmpty()" : "srlWrapper.Attributes.Count!=0"} + ')' + failure + "\"Unexpected XML map wrapper attribute\");\n";
  out += std::string{java ? "java.util.NavigableMap<" : "System.Collections.Generic.Dictionary<"} + type(map.key_type) + ',' + type(map.value_type) + "> srlMap=" + initializer + ";\nint srlKeyBytes=0,srlEntries=0;\n";
  out += "while(" + has + expanded(map.entry) + ")){\nif(srlEntries>=" + limits + (java ? ".maxEntries)throw SrlXmlRule.quota(" : ".MaxEntries)throw SrlXmlBudget.Limit(") + "\"XML map entry limit exceeded\"" + (java ? ");\n" : ",input.Operation);\n");
  out += "var srlEntry=" + open + expanded(map.entry) + ");\n" + type(map.key_type) + " srlKey=" + (java ? "null" : "default!") + ";\n" + type(map.value_type) + " srlItem=" + (java ? "null" : "default!") + ";\n";
  out += std::string{java ? "String srlKeyLexical=\"\";\nboolean " : "string srlKeyLexical=\"\";\nbool "} + "srlHasKey=false,srlHasValue=false;\n";
  if (map.key_placement == xml_binding::map_key_placement::attribute) {
    out += "srlKeyLexical=srlEntry." + std::string{java ? "attribute(" : "Attribute("} + expanded(map.key) + ",false)" + (java ? "" : "!") + ";\nsrlKey=" + scalar(map.key_type, "srlKeyLexical", false) + ";srlHasKey=true;\n";
  }
  out += "while(true){\n" + std::string{java ? "input.skipSpace();if(input.kind!='s')break;\nswitch(input.name()){\n" : "input.SkipSpace();if(input.Kind!='s')break;\nswitch(input.Name){\n"};
  if (map.key_placement == xml_binding::map_key_placement::element) {
    out += "case " + expanded(map.key) + ":\nif(srlHasKey)" + failure + "\"Duplicate XML map key member\");\nsrlKeyLexical=" + scalar_input + expanded(map.key) + ");\nsrlKey=" + scalar(map.key_type, "srlKeyLexical", false) + ";srlHasKey=true;break;\n";
  }
  out += "case " + expanded(map.value) + ":\nif(srlHasValue)" + failure + "\"Duplicate XML map value member\");\n";
  out += "srlItem=" + (map.value_type.type == object_type::class_type ? type(map.value_type) +
      (java ? ".xmlRead(input," : ".XmlRead(input,") + expanded(map.value) + ')' :
      scalar(map.value_type, scalar_input + expanded(map.value) + ')', false)) + ";srlHasValue=true;break;\n";
  out += "default:" + failure + "\"Unexpected XML map member\");\n}\n}\nif(!srlHasKey||!srlHasValue)" + failure + "\"Incomplete XML map entry\");\nsrlEntry" + finish;
  out += "srlKeyBytes=" + limits + (java ? ".keyBytes(" : ".KeyBytes(") + "srlKeyLexical," + scalar(map.key_type, "srlKey" + std::string{java ? "" : "!"}, true) + ",srlKeyBytes" + (java ? ");\n" : ",input.Operation);\n");
  out += "if(srlMap." + std::string{java ? "containsKey(srlKey)" : "ContainsKey(srlKey)"} + ')' + failure + "\"Duplicate canonical XML map key\");\n";
  out += java ? "srlMap.put(srlKey,srlItem);\n" : "srlMap.Add(srlKey,srlItem);\n";
  out += "++srlEntries;\n}\nsrlWrapper" + finish;
  out += access + '=' + (map.optional_container ? java ? "java.util.Optional.of(srlMap)" :
      "new System.ValueTuple<System.Collections.Generic.Dictionary<" + type(map.key_type) + ',' + type(map.value_type) + ">>(srlMap)" : "srlMap") + ";\n}\n}\n";
  return out;
}

std::string variant_write(const xml_binding::variant_contract& variant,
    const std::string& access, const alternative_name& alternative, const scalar_codec& scalar,
    std::string_view language) {
  const bool java = language == "java";
  const bool typed = variant.selection == xml_binding::variant_selection::xsi_type;
  const auto expanded = [](const xml::name& name) { return quote(name.namespace_uri.empty() ? name.local : '{' + name.namespace_uri + '}' + name.local); };
  const std::string failure = java ? "throw new IllegalArgumentException(" : "throw new System.FormatException(";
  const std::string open = java ? "output.open(" : "output.Open(";
  const std::string close = java ? "output.close();\n" : "output.Close();\n";
  std::string out{"{\n" + open + expanded(variant.wrapper) + ");\nswitch(" + access + "Index){\n"};
  for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
    const auto& item = variant.alternatives[index];
    const auto& payload_name = typed ? variant.wrapper : item.tag;
    const auto selected = access + alternative(item.alias);
    out += "case " + std::to_string(index) + ":{\n";
    if (item.native_type.type == object_type::class_type || item.native_type.name == "string" ||
        (java && item.native_type.type == object_type::enum_type)) {
      out += "if(" + selected + (java ? "==null)" : " is null)") + failure + "\"Null XML variant payload\");\n";
    }
    if (item.native_type.type == object_type::class_type) {
      out += selected + (java ? ".xmlWrite(output," : ".XmlWrite(output,") + expanded(payload_name) +
          (typed ? ',' + expanded(item.type_identity) : "") + ");\n";
    } else {
      out += open + expanded(payload_name) + ");\n";
      if (typed) { out += std::string{java ? "output.typeName(" : "output.TypeName("} + expanded(item.type_identity) + ");\n"; }
      out += std::string{java ? "output.text(" : "output.Text("} +
          scalar(item.native_type, selected, true) + ");\n" + close;
    }
    out += "break;\n}\n";
  }
  return out + "default:" + failure + "\"Invalid XML variant index\");\n}\n" + close + "}\n";
}

std::string variant_read(const xml_binding::variant_contract& variant,
    const std::string& access, const alternative_name& alternative, const type_name_fn& type,
    const scalar_codec& scalar, std::string_view language) {
  const bool java = language == "java";
  const bool typed = variant.selection == xml_binding::variant_selection::xsi_type;
  const auto expanded = [](const xml::name& name) { return quote(name.namespace_uri.empty() ? name.local : '{' + name.namespace_uri + '}' + name.local); };
  const std::string failure = java ? "throw new IllegalArgumentException(" : "throw new System.FormatException(";
  const std::string finish = java ? ".finish();\n" : ".Finish();\n";
  std::string out{"{\nvar srlWrapper=input." + std::string{java ? "open(" : "Open("} + expanded(variant.wrapper) + ");\n"};
  out += std::string{"if("} + (java ? "!srlWrapper.attributes.isEmpty()" : "srlWrapper.Attributes.Count!=0") + ')' + failure + "\"Unexpected XML variant wrapper attribute\");\n";
  out += std::string{java ? "input.skipSpace();\nif(input.kind!='s')" : "input.SkipSpace();\nif(input.Kind!='s')"} + failure + "\"Missing XML variant payload\");\n";
  if (typed) {
    out += std::string{"if(!input."} + (java ? "has(" : "Has(") + expanded(variant.wrapper) + "))" + failure + "\"Conflicting XML variant payload element\");\n";
    out += java ? "switch(input.typeIdentity(true)){\n" : "switch(input.TypeIdentity(true)){\n";
  } else { out += java ? "switch(input.name()){\n" : "switch(input.Name){\n"; }
  for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
    const auto& item = variant.alternatives[index];
    const auto& payload_name = typed ? variant.wrapper : item.tag;
    out += "case " + expanded(typed ? item.type_identity : item.tag) + ":{\n";
    if (!typed) { out += std::string{"input."} + (java ? "expectType(" : "ExpectType(") + expanded(item.type_identity) + ",false);\n"; }
    const auto decoded = item.native_type.type == object_type::class_type ? type(item.native_type) +
        (java ? ".xmlRead(input," : ".XmlRead(input,") + expanded(payload_name) + ')' :
        scalar(item.native_type, std::string{java ? "input.scalar(" : "input.Scalar("} + expanded(payload_name) + ')', false);
    out += access + alternative(item.alias) + '=' + decoded + ";\n" + access + "Index=" + std::to_string(index) + ";\nbreak;\n}\n";
  }
  return out + "default:" + failure + "\"Unknown XML variant payload\");\n}\nsrlWrapper" + finish + "}\n";
}

std::string presence_write(const xml_binding::field& field, std::size_t rule_index,
    const std::string& access, const scalar_codec& scalar, std::string_view language) {
  const bool java = language == "java";
  const auto& presence = field.presence;
  const auto& selected_type = leaf(*field.source);
  const auto expanded = [](const xml::name& name) { return quote(name.namespace_uri.empty() ? name.local : '{' + name.namespace_uri + '}' + name.local); };
  const std::string failure = java ? "throw new IllegalArgumentException(" : "throw new System.FormatException(";
  const std::string open = java ? "output.open(" : "output.Open(";
  const std::string close = java ? "output.close();\n" : "output.Close();\n";
  const std::string get = java ? ".get()" : ".Value.Item1";
  const auto present = [&](const std::string& value) { return value + (java ? ".isPresent()" : ".HasValue"); };
  std::string out{"{\n"}, payload{access};
  if (!java && presence.optional_depth != 0U) {
    out += "var srlOptional=" + access + ";\n"; payload = "srlOptional";
  }
  if (java && presence.optional_depth != 0U) { out += "if(" + payload + "==null)" + failure + "\"Null XML optional storage\");\n"; }
  if (presence.allows_absent) { out += "if(" + present(payload) + "){\n"; payload += get; }
  if (java && presence.optional_depth == 2U) { out += "if(" + payload + "==null)" + failure + "\"Null XML optional storage\");\n"; }
  if (presence.allows_nil) {
    out += "if(!" + present(payload) + "){\n" + open + expanded(field.wrapper ? *field.wrapper : field.qualified_name) + ");\n";
    out += std::string{java ? "output.attribute(" : "output.Attribute("} + quote("{http://www.w3.org/2001/XMLSchema-instance}nil") + ",\"true\");\n" + close + "}else{\n";
    payload += get;
  }
  if (presence.collection || selected_type.type == object_type::class_type || selected_type.name == "string") {
    out += "if(" + payload + (java ? "==null)" : " is null)") + failure + "\"Null XML presence payload\");\n";
  }
  if (field.wrapper) { out += open + expanded(*field.wrapper) + ");\n"; }
  if (presence.collection) {
    const auto count = payload + (java ? field.source->modifier == member::modifier_type::array ? ".length" : ".size()" : ".Count");
    const auto minimum = field.source->fixed_extent ? field.source->fixed_extent : field.min_occurs;
    const auto maximum = field.source->fixed_extent ? std::optional<std::size_t>{static_cast<std::size_t>(field.source->fixed_extent)} : field.max_occurs;
    if (minimum != 0U) { out += "if(" + count + '<' + std::to_string(minimum) + ')' + failure + "\"XML occurrence mismatch\");\n"; }
    if (maximum) { out += "if(" + count + '>' + std::to_string(*maximum) + ')' + failure + "\"XML occurrence mismatch\");\n"; }
    out += java ? "for(var srlItem:" + payload + "){\n" : "foreach(var srlItem in " + payload + "){\n";
    payload = "srlItem";
    if ((java && (field.source->modifier != member::modifier_type::array ||
            selected_type.type == object_type::enum_type)) ||
        selected_type.type == object_type::class_type || selected_type.name == "string") {
      out += "if(" + payload + (java ? "==null)" : " is null)") + failure + "\"Null XML collection item\");\n";
    }
  }
  if (selected_type.type == object_type::class_type) {
    out += payload + (java ? ".xmlWrite(output," : ".XmlWrite(output,") + expanded(field.qualified_name) + ");\n";
  } else {
    out += open + expanded(field.qualified_name) + ");\n" + (java ? "output.text(" : "output.Text(") +
        scalar_output(field, rule_index, scalar(selected_type, payload, true), language) + ");\n" + close;
  }
  if (presence.collection) { out += "}\n"; }
  if (field.wrapper) { out += close; }
  if (presence.allows_nil) { out += "}\n"; }
  if (presence.allows_absent) { out += "}\n"; }
  return out + "}\n";
}

std::string presence_read(const xml_binding::field& field, std::size_t rule_index,
    const std::string& access, const type_name_fn& type, const scalar_codec& scalar,
    std::string_view language, const std::string& array_initializer) {
  const bool java = language == "java";
  const auto& presence = field.presence;
  const auto& selected_type = leaf(*field.source);
  const auto expanded = [](const xml::name& name) { return quote(name.namespace_uri.empty() ? name.local : '{' + name.namespace_uri + '}' + name.local); };
  const std::string failure = java ? "throw new IllegalArgumentException(" : "throw new System.FormatException(";
  const auto wrap = [&](const std::string& payload, const std::string& native_type) {
    return java ? "java.util.Optional.of(" + payload + ')' : "new System.ValueTuple<" + native_type + ">(" + payload + ')';
  };
  const std::string empty = java ? "java.util.Optional.empty()" : "null";
  const std::string finish = java ? ".finish();\n" : ".Finish();\n";
  std::string out{"{\nswitch(input." + std::string{java ? "presence(" : "Presence("} + expanded(field.wrapper ? *field.wrapper : field.qualified_name) + ',' +
      (presence.allows_absent ? "true" : "false") + ',' + (presence.allows_nil ? "true" : "false") + ")){\n"};
  if (presence.allows_absent) { out += "case 'a':" + access + '=' + empty + ";break;\n"; }
  if (presence.allows_nil) {
    out += "case 'n':" + access + '=' + (presence.optional_depth == 2U ?
        wrap(empty, type(field.source->type_name_list.front().arguments.front())) : empty) + ";break;\n";
  }
  out += "case 'v':{\n";
  if (field.wrapper) { out += std::string{"var srlWrapper=input."} + (java ? "open(" : "Open(") + expanded(*field.wrapper) + ");\n"; }
  const auto decoded = selected_type.type == object_type::class_type ? type(selected_type) +
      (java ? ".xmlRead(input," : ".XmlRead(input,") + expanded(field.qualified_name) + ')' :
      scalar(selected_type, scalar_input(field, rule_index,
          std::string{java ? "input.scalar(" : "input.Scalar("} + expanded(field.qualified_name) + ')', language), false);
  std::string payload{"srlPayload"};
  if (presence.collection) {
    out += "var srlItems=new " + std::string{java ? "java.util.ArrayList<" : "System.Collections.Generic.List<"} + type(selected_type) + ">();\n";
    out += std::string{"while(input."} + (java ? "has(" : "Has(") + expanded(field.qualified_name) + ")){\n";
    const auto size = java ? "srlItems.size()" : "srlItems.Count";
    const auto minimum = field.source->fixed_extent ? field.source->fixed_extent : field.min_occurs;
    const auto maximum = field.source->fixed_extent ? std::optional<std::size_t>{static_cast<std::size_t>(field.source->fixed_extent)} : field.max_occurs;
    if (maximum) { out += "if(" + std::string{size} + ">=" + std::to_string(*maximum) + ')' + failure + "\"XML occurrence mismatch\");\n"; }
    out += std::string{"srlItems."} + (java ? "add(" : "Add(") + decoded + ");\n}\n";
    if (minimum != 0U) { out += "if(" + std::string{size} + '<' + std::to_string(minimum) + ')' + failure + "\"XML occurrence mismatch\");\n"; }
    if (java && field.source->modifier == member::modifier_type::array) {
      out += "var srlPayload=" + array_initializer + ";\nfor(int srlIndex=0;srlIndex<srlItems.size();++srlIndex)srlPayload[srlIndex]=srlItems.get(srlIndex);\n";
    } else { out += "var srlPayload=srlItems;\n"; }
  } else { out += "var srlPayload=" + decoded + ";\n"; }
  if (field.wrapper) { out += "srlWrapper" + finish; }
  auto native_type = type(*presence.value_type);
  for (std::size_t depth{}; depth < presence.optional_depth; ++depth) {
    payload = wrap(payload, native_type);
    if (!java) { native_type = "System.ValueTuple<" + native_type + ">?"; }
  }
  out += access + '=' + payload + ";break;\n}\ndefault:" + failure + "\"Invalid XML presence state\");\n}\n}\n";
  return out;
}

std::string imports(std::string_view language) {
  return language == "go" ? "import (srlxml \"encoding/xml\";srlio \"io\")\n" : "";
}
std::string runtime(std::string_view language) {
  if (language != "go") { throw std::invalid_argument{"Unsupported XML portable runtime"}; }
  const xml::pattern_limits limits{};
  const xml::limits xml_limits{};
  return "const (srlXmlPatternMaxStates=" + std::to_string(limits.max_states) +
      ";srlXmlPatternMaxInputBytes=" + std::to_string(limits.max_input_bytes) +
      ";srlXmlPatternMaxWorkUnits=" + std::to_string(limits.max_work_units) +
      ";srlXmlMapMaxEntries=" + std::to_string(xml_limits.max_map_entries) +
      ";srlXmlMapMaxKeyBytes=" + std::to_string(xml_limits.max_map_key_bytes) +
      ";srlXmlMixedMaxFragments=" + std::to_string(xml_limits.max_mixed_fragments) +
      ";srlXmlMixedMaxTextBytes=" + std::to_string(xml_limits.max_mixed_text_bytes) + ")\n" +
      identity_runtime("go") + html_runtime("go") + model_runtime("go") + extension_runtime("go") + std::string{go_runtime};
}
// Keep reference-specific delegation out of the ordinary owning-record binder and constructors.
std::string reference_methods(const class_node& object, const std::string& identifier,
    const type_name_fn& type, std::string_view language) {
  if (language != "go") { throw std::invalid_argument{"Unsupported XML reference backend"}; }
  return go_reference_methods(object, identifier, [&](const auto& selected, const auto& expression) {
    return output_errors(encode_scalar(selected, expression, type));
  });
}
std::string methods(const class_node& object, const xml_binding::record& contract,
    const std::string& identifier, const field_name& field, const type_name_fn& type,
    const xml_options& options, std::string_view language, const alternative_name& alternative,
    bool readonly, const field_name& projection, std::string_view operation_type,
    std::string_view operation_decoder) {
  if (language != "go") { throw std::invalid_argument{"Unsupported XML portable methods"}; }
  const bool export_graph = readonly || xml_codegen::native_xml_export_graph(object);
  const bool namespace_values = std::any_of(contract.fields.begin(), contract.fields.end(), namespace_scalar);
  const bool all = supports_all(contract);
  if (!contract.all_groups.empty() && !all) {
    throw std::invalid_argument{"Go XML all traversal requires one complete unwrapped singleton group"};
  }
  for (const auto& mapped : contract.fields) {
    if ((mapped.presence.explicit_policy || needs_presence(mapped)) && !supports_presence(mapped)) {
      throw std::invalid_argument{"Go XML presence requires ordinary scalar, record or wrapped sequence storage without lexical overlays"};
    }
    if (mapped.variant_binding && !supports_variant(mapped)) {
      throw std::invalid_argument{"Go XML variants require one complete dedicated wrapper without scalar overlays"};
    }
    if (mapped.map_binding && !supports_map(mapped)) {
      throw std::invalid_argument{"Go XML maps require one complete dedicated wrapper without scalar overlays"};
    }
  }
  for (const auto& value : object.member_list) {
    if (field(value) == "srlXmlNamespaces" || (namespace_values && field(value) == "SetXMLNamespaces")) {
      throw std::invalid_argument{"Go XML namespace state collides with a native field"};
    }
    if (field(value) == "EncodeXML" || field(value) == "EncodeXMLWithMapLimits" ||
        field(value) == "EncodeXMLWithMixedLimits" || field(value) == "EncodeXMLWithContentLimits" ||
        field(value) == "EncodeXMLWithIdentityLimits" || field(value) == "EncodeXMLWithPolicies" ||
        field(value) == "EncodeXMLWithDigestLimits" || field(value) == "EncodeXMLWithAllPolicies" || field(value) == "EncodeXMLWithAllBounds" || field(value) == "EncodeXMLWithExtensionOptions") {
      throw std::invalid_argument{"XML encoder method collides with a generated Go field"};
    }
  }
  // Native Go represents each base as an owned pointer field; XML uses the shared
  // base-first flattened contract without changing that native representation.
  std::map<const member*, std::string> member_paths{};
  std::vector<std::string> base_paths{};
  const auto collect_paths = [&](auto&& self, const class_node& owner,
                                 const std::string& prefix) -> void {
    for (std::size_t index{}; index < owner.parents.size(); ++index) {
      const auto& parent = owner.parents[index];
      member base{};
      base.name = "Base" + std::to_string(index);
      base.access = parent.access;
      const auto path = prefix + (&owner == &object && projection ? projection(base) : field(base));
      base_paths.push_back(path);
      self(self, *parent.parent_class, path + '.');
    }
    for (const auto& value : owner.member_list) {
      member_paths.emplace(&value, prefix + (&owner == &object && projection ? projection(value) : field(value)));
    }
  };
  collect_paths(collect_paths, object, "");
  const auto revision_path = contract.model.revision ? "value." + member_paths.at(contract.model.revision) : std::string{};
  // Close exact lifecycle guards even when specialized field emitters leave their branch early.
  const auto active_scope = [&](const member& item, std::string& generated) {
    const auto condition = xml_codegen::native_xml_active(contract, item, version_language::go, revision_path);
    if (!condition.empty()) { generated += "if " + condition + "{\n"; }
    return xml_codegen::model_emission_scope{condition.empty() ? std::function<void()>{} :
        std::function<void()>{[&generated] { generated += "}\n"; }}};
  };
  std::map<const member*, std::string> patterns{};
  std::string pattern_declarations{};
  if(contract.extensions)for(const auto& source_field:object.member_list)if(field(source_field)=="srlXmlPreserved")throw std::invalid_argument{"Go XML extension state collides with a native field"};
  const auto extension_name="srlXmlExtensionContract"+identifier;
  if(contract.extensions)pattern_declarations+="// Immutable foreign-only permissions and prepared slot lookups.\nvar "+extension_name+'='+xml_codegen::portable_extension_metadata(*contract.extensions,quote,qualified,"go")+"\n";
  const auto identity_name = "srlXmlIdentityScope" + identifier;
  if (contract.identity) {
    pattern_declarations += "// Immutable owner-local tuple paths and exact builtin value spaces.\nvar " +
        identity_name + '=' + identity_metadata(*contract.identity, "go") + "\n";
  }
  for (std::size_t index{}; index < contract.fields.size(); ++index) {
    const auto& mapped = contract.fields[index];
    if (!needs_scalar_rule(mapped)) { continue; }
    const auto name = "srlXmlRule" + identifier + "_" + std::to_string(index);
    patterns.emplace(mapped.source, name);
    pattern_declarations += "// Prepared complete scalar rules are shared by typed input and output.\nvar " +
        name + '=' + go_scalar_rule(mapped) + "\n";
  }
  std::string out{"\n// EncodeXML serializes one complete owning value under its fixed XML mapping.\nfunc (value *" + identifier + ") EncodeXML(policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithMapLimits(DefaultXmlMapLimits(),policies...)}\n"
      "// EncodeXMLWithMapLimits applies explicit map bounds without changing legacy option literals.\nfunc (value *" + identifier + ") EncodeXMLWithMapLimits(maps XmlMapLimits,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithContentLimits(maps,DefaultXmlMixedLimits(),policies...)}\n"
      "// EncodeXMLWithMixedLimits applies explicit ordered-sequence bounds to every descendant.\nfunc (value *" + identifier + ") EncodeXMLWithMixedLimits(mixed XmlMixedLimits,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithContentLimits(DefaultXmlMapLimits(),mixed,policies...)}\n"
      "// EncodeXMLWithContentLimits combines independent map/mixed bounds for the complete graph.\nfunc (value *" + identifier + ") EncodeXMLWithContentLimits(maps XmlMapLimits,mixed XmlMixedLimits,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithPolicies(maps,mixed,DefaultXmlIdentityLimits(),policies...)}\n"
      "// EncodeXMLWithIdentityLimits applies explicit owner-local tuple and selector bounds.\nfunc (value *" + identifier + ") EncodeXMLWithIdentityLimits(identity XmlIdentityLimits,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithPolicies(DefaultXmlMapLimits(),DefaultXmlMixedLimits(),identity,policies...)}\n"
      "// EncodeXMLWithPolicies combines independent map, mixed and identity admission.\nfunc (value *" + identifier + ") EncodeXMLWithPolicies(maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithAllPolicies(maps,mixed,identity,DefaultXmlDigestLimits(),policies...)}\n"
      "// EncodeXMLWithDigestLimits bounds nested owning digests without changing earlier option structs.\nfunc (value *" + identifier + ") EncodeXMLWithDigestLimits(digests XmlDigestLimits,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithAllPolicies(DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),digests,policies...)}\n"
      "// EncodeXMLWithAllPolicies validates every independent policy before graph traversal.\nfunc (value *" + identifier + ") EncodeXMLWithAllPolicies(maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,digests XmlDigestLimits,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithAllBounds(maps,mixed,identity,digests,DefaultXmlExtensionOptions(),policies...)}\n// EncodeXMLWithExtensionOptions carries explicit vocabulary understanding and opaque storage bounds.\nfunc (value *" + identifier + ") EncodeXMLWithExtensionOptions(extensions XmlExtensionOptions,policies ...XmlOptions)([]byte,error){return value.EncodeXMLWithAllBounds(DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits(),extensions,policies...)}\n// EncodeXMLWithAllBounds validates each independent policy before complete owning traversal.\nfunc (value *" + identifier + ") EncodeXMLWithAllBounds(maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,digests XmlDigestLimits,extensions XmlExtensionOptions,policies ...XmlOptions)([]byte,error){\nvar extensionErr error;extensions,extensionErr=srlXmlExtensionOptions(extensions);if extensionErr!=nil{return nil,extensionErr}\nif err:=srlXmlMapPolicy(maps);err!=nil{return nil,err}\nif err:=srlXmlMixedPolicy(mixed);err!=nil{return nil,err}\nif err:=srlXmlIdentityPolicy(identity);err!=nil{return nil,err}\nif err:=srlXmlDigestPolicy(digests);err!=nil{return nil,err}\n"};
  out.insert(0, pattern_declarations);
  if (namespace_values && !readonly) {
    out.insert(0, "// SetXMLNamespaces replaces XML-only field scopes for native-authored QName values.\n"
        "// Bindings are copied; native fields and serialization protocols retain their existing values.\n"
        "func (value *" + identifier + ") SetXMLNamespaces(bindings map[string]string)error{\n"
        "if value==nil{return fmt.Errorf(\"Nil XML record\")}\n"
        "scope,err:=srlXmlNamespaceBindings(bindings);if err!=nil{return err}\n"
        "value.srlXmlNamespaces=map[int][]map[string]string{-1:{scope}};return nil\n}\n");
  }
  // Keep positional extension drains outside lifecycle guards and specialized early branches.
  const auto extension_scope=[&](const xml_binding::field& mapped,bool encoding){
    if(!contract.extensions||mapped.attribute||mapped.text)return xml_codegen::model_emission_scope{{}};
    const auto position=std::to_string(&mapped-contract.fields.data());
    auto count=xml_codegen::portable_extension_field_count(mapped,"value."+member_paths.at(mapped.source),"go");
    const auto condition=xml_codegen::native_xml_active(contract,*mapped.source,version_language::go,revision_path);
    if(!condition.empty())count="func()int{if "+condition+"{return "+count+"};return 0}()";
    const std::string operation=encoding?"emitAt(output,":"readAt(input,",failure=encoding?"return err":"return nil,err";
    out+="{\n";if(encoding&&mapped.repeated&&!mapped.wrapper&&!typed_list(mapped)&&!needs_presence(mapped)&&!mapped.map_binding&&!mapped.variant_binding)out+="srlExtensionItems:=0\n";
    out+="if err:=srlExtensions."+operation+position+",0);err!=nil{"+failure+"}\n";
    return xml_codegen::model_emission_scope{[&,position,count,operation,failure,encoding]{out+="if err:=srlExtensions."+operation+position+','+count+");err!=nil{"+failure+"}\n";if(encoding)out+="if err:=srlExtensions.endPosition("+position+");err!=nil{return err}\n";out+="}\n";}};
  };

  std::string operation_method{"EncodeXMLWithOperation"};
  const auto method_exists=[&](std::string_view candidate){
    if(std::any_of(object.member_list.begin(),object.member_list.end(),[&](const auto& member){return field(member)==candidate;})){return true;}
    return std::any_of(object.functions.begin(),object.functions.end(),[&](const auto& function){member member{};member.name=function.name;member.access=access_type::public_access;return field(member)==candidate;});
  };
  while(method_exists(operation_method)){operation_method+="_2";}
  const auto operation_encode="// "+operation_method+" applies additional admission without changing XmlOptions layout.\nfunc(value *"+identifier+") "+operation_method+"(operation "+std::string{operation_type}+")([]byte,error){return value.EncodeXML(operation.Policy())}\n";
  out += "policy,err:=srlXmlOptions(policies);if err!=nil{return nil,err}\n";
  if (!options.runtime_xsd) { out += "if policy.Validator!=nil{return nil,fmt.Errorf(\"Runtime XSD was not enabled during generation\")}\n"; }
  out += "output:=srlXmlOutputStart(policy.Limits,maps)\noutput.budget.operation=policy.Operation\noutput.mixedLimits=mixed\noutput.identityLimits=identity\noutput.digestLimits=digests\noutput.extensionOptions=extensions\nif err=value.srlXmlEncode(output," + qualified(contract.root) + ");err!=nil{return nil,err}\ndata,err:=output.finish();if err!=nil{return nil,err}\n";
  if (options.runtime_xsd && options.validation_scope != "input") { out += "if policy.Validator!=nil{if err=policy.Validator.ValidateXML(data);err!=nil{return nil,err}}\n"; }
  out += "return data,nil\n}\n";
  out += operation_encode;
  if (!readonly) {
  out += "// Decode" + identifier + "XML returns a fresh value only after complete XML validation.\nfunc Decode" + identifier + "XML(data []byte,policies ...XmlOptions)(*" + identifier + ",error){return Decode" + identifier + "XMLWithMapLimits(data,DefaultXmlMapLimits(),policies...)}\n"
      "// Decode" + identifier + "XMLWithMapLimits applies explicit map bounds before publication.\nfunc Decode" + identifier + "XMLWithMapLimits(data []byte,maps XmlMapLimits,policies ...XmlOptions)(*" + identifier + ",error){return Decode" + identifier + "XMLWithContentLimits(data,maps,DefaultXmlMixedLimits(),policies...)}\n"
      "// Decode" + identifier + "XMLWithMixedLimits applies explicit ordered-sequence bounds.\nfunc Decode" + identifier + "XMLWithMixedLimits(data []byte,mixed XmlMixedLimits,policies ...XmlOptions)(*" + identifier + ",error){return Decode" + identifier + "XMLWithContentLimits(data,DefaultXmlMapLimits(),mixed,policies...)}\n"
      "// Decode" + identifier + "XMLWithContentLimits validates combined map/mixed policies.\nfunc Decode" + identifier + "XMLWithContentLimits(data []byte,maps XmlMapLimits,mixed XmlMixedLimits,policies ...XmlOptions)(*" + identifier + ",error){return Decode" + identifier + "XMLWithPolicies(data,maps,mixed,DefaultXmlIdentityLimits(),policies...)}\n"
      "// Decode" + identifier + "XMLWithIdentityLimits bounds every independently scoped native identity index.\nfunc Decode" + identifier + "XMLWithIdentityLimits(data []byte,identity XmlIdentityLimits,policies ...XmlOptions)(*" + identifier + ",error){return Decode" + identifier + "XMLWithPolicies(data,DefaultXmlMapLimits(),DefaultXmlMixedLimits(),identity,policies...)}\n"
      "// Decode" + identifier + "XMLWithPolicies preserves earlier map/mixed/identity callers.\nfunc Decode" + identifier + "XMLWithPolicies(data []byte,maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,policies ...XmlOptions)(*" + identifier + ",error){return Decode" + identifier + "XMLWithAllPolicies(data,maps,mixed,identity,DefaultXmlDigestLimits(),policies...)}\n"
      "// Decode" + identifier + "XMLWithDigestLimits admits every nested owning byte candidate.\nfunc Decode" + identifier + "XMLWithDigestLimits(data []byte,digests XmlDigestLimits,policies ...XmlOptions)(*" + identifier + ",error){return Decode" + identifier + "XMLWithAllPolicies(data,DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),digests,policies...)}\n"
      "// Decode" + identifier + "XMLWithAllPolicies validates every policy before reading input.\nfunc Decode" + identifier + "XMLWithAllPolicies(data []byte,maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,digests XmlDigestLimits,policies ...XmlOptions)(*" + identifier + ",error){return Decode"+identifier+"XMLWithAllBounds(data,maps,mixed,identity,digests,DefaultXmlExtensionOptions(),policies...)}\n// Decode"+identifier+"XMLWithExtensionOptions requires explicit vocabulary understanding before candidate construction.\nfunc Decode"+identifier+"XMLWithExtensionOptions(data []byte,extensions XmlExtensionOptions,policies ...XmlOptions)(*"+identifier+",error){return Decode"+identifier+"XMLWithAllBounds(data,DefaultXmlMapLimits(),DefaultXmlMixedLimits(),DefaultXmlIdentityLimits(),DefaultXmlDigestLimits(),extensions,policies...)}\n// Decode"+identifier+"XMLWithAllBounds carries independent opaque storage admission through all descendants.\nfunc Decode"+identifier+"XMLWithAllBounds(data []byte,maps XmlMapLimits,mixed XmlMixedLimits,identity XmlIdentityLimits,digests XmlDigestLimits,extensions XmlExtensionOptions,policies ...XmlOptions)(*"+identifier+",error){\n";
  if (export_graph) {
    out += "return nil,fmt.Errorf(\"Readonly XML projections are export-only; decode the owning target\")\n}\n";
  } else {
  out += "var extensionErr error;extensions,extensionErr=srlXmlExtensionOptions(extensions);if extensionErr!=nil{return nil,extensionErr}\nif err:=srlXmlMapPolicy(maps);err!=nil{return nil,err}\nif err:=srlXmlMixedPolicy(mixed);err!=nil{return nil,err}\nif err:=srlXmlIdentityPolicy(identity);err!=nil{return nil,err}\nif err:=srlXmlDigestPolicy(digests);err!=nil{return nil,err}\npolicy,err:=srlXmlOptions(policies);if err!=nil{return nil,err}\n";
  if (!options.runtime_xsd) { out += "if policy.Validator!=nil{return nil,fmt.Errorf(\"Runtime XSD was not enabled during generation\")}\n"; }
  out += "input,err:=srlXmlInputStart(data,policy.Limits,maps);if err!=nil{return nil,err}\ninput.budget.operation=policy.Operation\ninput.mixedLimits=mixed\ninput.identityLimits=identity\ninput.digestLimits=digests\ninput.extensionOptions=extensions\nif policy.ExpectedRoot!=\"\"{event,failure:=input.peek();if failure!=nil{return nil,failure};if srlXmlIdentityName(event.name)!=policy.ExpectedRoot{return nil,fmt.Errorf(\"Unexpected package XML root\")}}\nif policy.ExpectedType!=\"\"{expected:=srlxml.Name{Local:policy.ExpectedType};if strings.HasPrefix(policy.ExpectedType,\"{\"){end:=strings.Index(policy.ExpectedType,\"}\");if end<0{return nil,fmt.Errorf(\"Invalid expected XML type\")};expected.Space=policy.ExpectedType[1:end];expected.Local=policy.ExpectedType[end+1:]};if err:=input.expectType(expected,false);err!=nil{return nil,err}}\nvalue,err:=srlXmlDecode" + identifier + "(input," + qualified(contract.root) + ");if err!=nil{return nil,err}\nif err=input.finish();err!=nil{return nil,err}\n";
  if (options.runtime_xsd && options.validation_scope != "output") { out += "if policy.Validator!=nil{if err=policy.Validator.ValidateXML(data);err!=nil{return nil,err}}\n"; }
  out += "return value,nil\n}\n";
  if(!operation_decoder.empty()){
    out += "// "+std::string{operation_decoder}+" applies additional admission to a fresh candidate.\nfunc "+std::string{operation_decoder}+"(data []byte,operation "+std::string{operation_type}+")(*"+identifier+",error){return Decode"+identifier+"XML(data,operation.Policy())}\n";
  }
  }
  }
  out += "// Emit statically bound tokens while retaining only active ancestors.\nfunc (value *" + identifier + ") srlXmlEncode(output *srlXmlOutput,name srlxml.Name,types ...srlxml.Name)error{\nif value==nil{return fmt.Errorf(\"Nil XML record\")}\nif len(types)>1{return fmt.Errorf(\"Ambiguous XML variant type\")}\n";
  if(contract.extensions){
    out+="srlScope:=map[string]string{}\n";
    if(!readonly)out+="if value.srlXmlPreserved!=nil{srlScope=value.srlXmlPreserved.scope}\n";
    out+="srlExtensions,srlExtensionErr:=srlXmlExtensionTracker(&"+extension_name+",srlScope,output.extensionOptions,"+std::string{readonly?"nil":"value.srlXmlPreserved"}+");if srlExtensionErr!=nil{return srlExtensionErr}\nif err:=output.openScoped(name,srlScope);err!=nil{return err}\n";
  } else if (namespace_values) {
    out += "if err:=output.openScoped(name,map[string]string{\"\":\"\"});err!=nil{return err}\n";
  } else { out += "if err:=output.open(name);err!=nil{return err}\n"; }
  if (contract.identity) {
    out += "srlIdentity,srlIdentityErr:=output.identityScope(&" + identity_name + ");if srlIdentityErr!=nil{return srlIdentityErr}\n";
  }
  out += "if len(types)==1{if err:=output.typeName(types[0]);err!=nil{return err}}\n";
  for (const auto& path : base_paths) {
    out += "if value." + path + "==nil{return fmt.Errorf(\"Nil XML base record\")}\n";
  }
  if (const auto* revision = contract.model.revision) {
    out += "if " + xml_codegen::native_xml_revision_invalid(*revision, version_language::go, revision_path) +
        "{return fmt.Errorf(\"Unsupported XML revision\")}\n{\n" + output_errors(encode_scalar(revision->type_name_list.front(), revision_path, type));
    out += "srlText,srlRevisionErr:=srlXmlRevisionLexical(srlText," + quote(revision->type_name_list.front().name) +
        ");if srlRevisionErr!=nil{return srlRevisionErr}\nif err:=output.attribute(" +
        qualified({"revision", std::string{xml_binding::model_metadata_namespace}}) + ",srlText);err!=nil{return err}\n}\n";
  }
  if (contract.model.magic) {
    out += "if err:=output.attribute(" + qualified({"magic", std::string{xml_binding::model_metadata_namespace}}) + ',' + quote(*contract.model.magic) + ");err!=nil{return err}\n";
  }
  out += output_errors(choice_check(contract,
      [&](const member& source) { return "value." + member_paths.at(&source); }, "go"));
  // Attributes must precede content even when their schema fields follow children.
  for (const bool attributes : {true, false}) {
    if(contract.extensions&&!attributes)out+="if err:=srlExtensions.emitAttributes(output);err!=nil{return err}\n";
    for (const auto& mapped : contract.fields) {
      if (mapped.attribute != attributes) { continue; }
      const auto& source = *mapped.source;
      const auto extensions=extension_scope(mapped,true);
      const auto active = active_scope(source, out);
      if (typed_list(mapped)) {
        out += encode_list(mapped, "value." + member_paths.at(&source), type, patterns.at(&source),
            static_cast<std::size_t>(&mapped - contract.fields.data()), readonly);
        continue;
      }
      if (needs_presence(mapped)) {
        out += encode_presence(mapped, "value." + member_paths.at(&source), type,
            patterns.contains(&source) ? patterns.at(&source) : "");
        continue;
      }
      if (mapped.map_binding) {
        out += encode_map(*mapped.map_binding, "value." + member_paths.at(&source), type);
        continue;
      }
      if (mapped.variant_binding) {
        const auto& variant = *mapped.variant_binding;
        const bool typed = variant.selection == xml_binding::variant_selection::xsi_type;
        const auto access = "value." + member_paths.at(&source);
        out += "{\nif err:=output.open(" + qualified(variant.wrapper) + ");err!=nil{return err}\nswitch " + access + "Index{\n";
        for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
          const auto& item = variant.alternatives[index];
          const auto& payload_name = typed ? variant.wrapper : item.tag;
          const auto selected = access + alternative(item.alias);
          out += "case " + std::to_string(index) + ":\n";
          if (item.native_type.type == object_type::class_type) {
            out += "if err:=" + selected + ".srlXmlEncode(output," + qualified(payload_name) +
                (typed ? ',' + qualified(item.type_identity) : "") + ");err!=nil{return err}\n";
          } else {
            out += output_errors(encode_scalar(item.native_type, selected, type));
            out += "if err:=output.open(" + qualified(payload_name) + ");err!=nil{return err}\n";
            if (typed) { out += "if err:=output.typeName(" + qualified(item.type_identity) + ");err!=nil{return err}\n"; }
            out += "if err:=output.text(srlText);err!=nil{return err}\nif err:=output.close();err!=nil{return err}\n";
          }
        }
        out += "default:return fmt.Errorf(\"Invalid XML variant index\")\n}\nif err:=output.close();err!=nil{return err}\n}\n";
        continue;
      }
      const auto& scalar = leaf(source);
      const auto access = "value." + member_paths.at(&source);
      const bool optional = source.type_name_list.front().is_optional();
      const auto min = source.fixed_extent != 0 ? source.fixed_extent : mapped.min_occurs;
      const auto max = source.fixed_extent != 0 ? std::optional<std::size_t>{static_cast<std::size_t>(source.fixed_extent)} : mapped.max_occurs;
      out += "{\n";
      if (namespace_scalar(mapped)) { out += "srlNamespaceIndex:=0\n"; }
      if (mapped.wrapper) {
        out += "if err:=output.open(" + qualified(*mapped.wrapper) + ");err!=nil{return err}\n";
      }
      auto expression = access;
      if (optional) { out += "if " + access + "!=nil{\n"; expression = "(*" + access + ")"; }
      if (mapped.repeated) {
        out += "if len(" + access + ")<" + std::to_string(min);
        if (max) { out += "||len(" + access + ")>" + std::to_string(*max); }
        out += "{return fmt.Errorf(\"XML collection cardinality mismatch\")}\nfor _,srlItem:=range " + access + "{\n";
        expression = "srlItem";
      }
      if (projection_payload(scalar).type == object_type::class_type) {
        out += "if err:=" + expression + ".srlXmlEncode(output," + qualified(mapped.qualified_name) + ");err!=nil{return err}\n";
      } else {
        out += output_errors(encode_scalar(scalar, expression, type));
        out += output_errors(scalar_facets(mapped,
            patterns.contains(&source) ? patterns.at(&source) : "", true));
        const auto namespace_value = [&] {
          if (!namespace_scalar(mapped)) { return std::string{}; }
          if (readonly) {
            return "srlQualified,srlNamespaceErr:=output.namespaceValue(srlText," + namespace_mode(mapped) +
                ",nil);if srlNamespaceErr!=nil{return srlNamespaceErr};srlText=srlQualified\nsrlNamespaceIndex++\n";
          }
          return "srlScope:=map[string]string(nil)\nsrlScopes:=value.srlXmlNamespaces[" +
              std::to_string(&mapped - contract.fields.data()) + "]\n" +
              "if srlNamespaceIndex<len(srlScopes){srlScope=srlScopes[srlNamespaceIndex]}else if fallback:=value.srlXmlNamespaces[-1];len(fallback)!=0{srlScope=fallback[0]}\n" +
              "srlQualified,srlNamespaceErr:=output.namespaceValue(srlText," + namespace_mode(mapped) +
              ",srlScope);if srlNamespaceErr!=nil{return srlNamespaceErr};srlText=srlQualified\nsrlNamespaceIndex++\n";
        };
        if (mapped.attribute) {
          out += namespace_value();
          if (namespace_scalar(mapped)) {
            out += "{\n" + output_errors(scalar_facets(mapped,
                patterns.contains(&source) ? patterns.at(&source) : "", true)) + "}\n";
          }
          out += "if err:=output.attribute(" + qualified(mapped.qualified_name) + ",srlText);err!=nil{return err}\n";
        } else {
          if (!mapped.text) { out += "if err:=output.open(" + qualified(mapped.qualified_name) + ");err!=nil{return err}\n"; }
          out += namespace_value();
          if (namespace_scalar(mapped)) {
            out += "{\n" + output_errors(scalar_facets(mapped,
                patterns.contains(&source) ? patterns.at(&source) : "", true)) + "}\n";
          }
          out += "if err:=output.text(srlText);err!=nil{return err}\n";
          if (!mapped.text) { out += "if err:=output.close();err!=nil{return err}\n"; }
        }
      }
      if(contract.extensions&&mapped.repeated&&!mapped.wrapper)out+="srlExtensionItems++\nif err:=srlExtensions.emitAt(output,"+std::to_string(&mapped-contract.fields.data())+",srlExtensionItems);err!=nil{return err}\n";
      if (mapped.repeated || optional) { out += "}\n"; }
      if (mapped.wrapper) { out += "if err:=output.close();err!=nil{return err}\n"; }
      out += "}\n";
    }
  }
  if (contract.mixed) {
    out += encode_mixed(*contract.mixed, "value." + member_paths.at(contract.mixed->source), field, alternative);
  }
  if(contract.extensions)out+="if err:=srlExtensions.emitAt(output,"+std::to_string(contract.fields.size())+",0);err!=nil{return err}\nif err:=srlExtensions.finishOutput();err!=nil{return err}\n";
  out += contract.identity ? "if err:=output.close();err!=nil{return err}\nreturn output.identityFinish(srlIdentity)\n" : "return output.close()\n";
  out += "}\n";
  if (readonly) { return out; }
  out += "// Consume typed tokens into a private candidate; no completed XML subtree is retained.\nfunc srlXmlDecode" + identifier + "(input *srlXmlInput,name srlxml.Name)(*" + identifier + ",error){\n";
  if (export_graph) {
    return out + "return nil,fmt.Errorf(\"Readonly XML projections are export-only; decode the owning target\")\n}\n";
  }
  if (contract.identity) {
    out += "srlIdentity,srlIdentityErr:=input.identityScope(&" + identity_name + ");if srlIdentityErr!=nil{return nil,srlIdentityErr}\n";
  }
  if(contract.extensions)out+="if _,err:=input.peek();err!=nil{return nil,err}\nsrlExtensions,srlExtensionErr:=srlXmlExtensionTracker(&"+extension_name+",input.namespaces,input.extensionOptions,nil);if srlExtensionErr!=nil{return nil,srlExtensionErr}\n";
  out += "fields,err:=input.open(name);if err!=nil{return nil,err}\nvalue:=New" + identifier + "()\n";
  if (const auto* revision = contract.model.revision) {
    out += "{\nsrlText,_,srlErr:=fields.attribute(" + qualified({"revision", std::string{xml_binding::model_metadata_namespace}}) +
        ",true);if srlErr!=nil{return nil,srlErr}\nsrlText,srlErr=srlXmlRevisionLexical(srlText," + quote(revision->type_name_list.front().name) +
        ");if srlErr!=nil{return nil,srlErr}\n" + decode_scalar(revision->type_name_list.front(), type) + revision_path + "=srlValue\n}\n";
    out += "if " + xml_codegen::native_xml_revision_invalid(*revision, version_language::go, revision_path) + "{return nil,fmt.Errorf(\"Unsupported XML revision\")}\n";
  }
  if (contract.model.magic) {
    out += "{\nsrlMagic,_,srlErr:=fields.attribute(" + qualified({"magic", std::string{xml_binding::model_metadata_namespace}}) +
        ",true);if srlErr!=nil{return nil,srlErr};if srlMagic!=" + quote(*contract.model.magic) + "{return nil,fmt.Errorf(\"XML magic mismatch\")}\n}\n";
  }
  // Consume attributes independently of their declared field order.
  for (const bool attributes : {true, false}) {
    if (all && !attributes) { continue; }
    if(contract.extensions&&!attributes)out+="for index,attribute:=range fields.attrs{if attribute.Name.Local!=\"\"{if err:=srlExtensions.attribute(attribute);err!=nil{return nil,err};fields.attrs[index].Name=srlxml.Name{};fields.used++}}\n";
    for (const auto& mapped : contract.fields) {
      if (mapped.attribute != attributes) { continue; }
      const auto& source = *mapped.source;
      const auto extensions=extension_scope(mapped,false);
      const auto active = active_scope(source, out);
      if (typed_list(mapped)) {
        out += decode_list(mapped, "value." + member_paths.at(&source), type, patterns.at(&source),
            static_cast<std::size_t>(&mapped - contract.fields.data()));
        continue;
      }
      if (needs_presence(mapped)) {
        out += decode_presence(mapped, "value." + member_paths.at(&source), type,
            patterns.contains(&source) ? patterns.at(&source) : "");
        continue;
      }
      if (mapped.map_binding) {
        out += decode_map(*mapped.map_binding, "value." + member_paths.at(&source), type);
        continue;
      }
      if (mapped.variant_binding) {
        const auto& variant = *mapped.variant_binding;
        const bool typed = variant.selection == xml_binding::variant_selection::xsi_type;
        const auto access = "value." + member_paths.at(&source);
        out += "{\nsrlWrapper,srlErr:=input.open(" + qualified(variant.wrapper) + ");if srlErr!=nil{return nil,srlErr}\nif len(srlWrapper.attrs)!=0{return nil,fmt.Errorf(\"Unexpected XML variant wrapper attribute\")}\nsrlEvent,srlErr:=input.peek();if srlErr!=nil{return nil,srlErr};if srlEvent.kind!='s'{return nil,fmt.Errorf(\"Missing XML variant payload\")}\n";
        if (typed) {
          out += "if srlEvent.name!=(" + qualified(variant.wrapper) + "){return nil,fmt.Errorf(\"Conflicting XML variant payload element\")}\nsrlType,srlErr:=input.typeIdentity(true);if srlErr!=nil{return nil,srlErr}\nswitch srlType{\n";
        } else { out += "switch srlEvent.name{\n"; }
        for (std::size_t index{}; index < variant.alternatives.size(); ++index) {
          const auto& item = variant.alternatives[index];
          const auto& payload_name = typed ? variant.wrapper : item.tag;
          out += "case " + qualified(typed ? item.type_identity : item.tag) + ":\n";
          if (!typed) { out += "if srlErr:=input.expectType(" + qualified(item.type_identity) + ",false);srlErr!=nil{return nil,srlErr}\n"; }
          if (item.native_type.type == object_type::class_type) {
            out += "srlValue,srlErr:=srlXmlDecode" + record_name(type(item.native_type)) + "(input," + qualified(payload_name) + ");if srlErr!=nil{return nil,srlErr}\n";
          } else {
            out += "srlText,srlErr:=input.scalar(" + qualified(payload_name) + ");if srlErr!=nil{return nil,srlErr}\n" + decode_scalar(item.native_type, type);
          }
          out += access + alternative(item.alias) + "=srlValue\n" + access + "Index=" + std::to_string(index) + "\n";
        }
        out += "default:return nil,fmt.Errorf(\"Unknown XML variant payload\")\n}\nif srlErr=srlWrapper.finish();srlErr!=nil{return nil,srlErr}\n}\n";
        continue;
      }
      const auto& scalar = leaf(source);
      const auto access = "value." + member_paths.at(&source);
      const bool optional = source.type_name_list.front().is_optional();
      const auto min = source.fixed_extent != 0 ? source.fixed_extent : mapped.min_occurs;
      const auto max = source.fixed_extent != 0 ? std::optional<std::size_t>{static_cast<std::size_t>(source.fixed_extent)} : mapped.max_occurs;
      out += "{\n";
      if (mapped.repeated && source.fixed_extent == 0) { out += access + "=make([]" + type(scalar) + ",0)\n"; }
      if (optional) { out += access + "=nil\n"; }
      if (mapped.attribute) {
        const auto& fallback = mapped.fixed_value ? mapped.fixed_value : mapped.default_value;
        out += "srlText,srlPresent,srlErr:=fields.attribute(" + qualified(mapped.qualified_name) + "," + (min && !fallback ? "true" : "false") + ");if srlErr!=nil{return nil,srlErr}\n";
        if (fallback) { out += "if !srlPresent{srlText=" + quote(*fallback) + ";srlPresent=true}\n"; }
        out += "if srlPresent{\n";
        if (namespace_scalar(mapped)) {
          out += "srlScope,srlNamespaceErr:=input.namespaceScope(srlText," + namespace_mode(mapped) +
              ",nil);if srlNamespaceErr!=nil{return nil,srlNamespaceErr}\n";
        }
      } else if (mapped.text) {
        out += "if fields.used!=len(fields.attrs){return nil,fmt.Errorf(\"Unexpected XML attribute\")}\n";
        out += namespace_scalar(mapped)
            ? "srlText,srlScope,srlErr:=input.namespaceText(" + namespace_mode(mapped) + ");if srlErr!=nil{return nil,srlErr}\n"
            : "srlText,srlErr:=input.text();if srlErr!=nil{return nil,srlErr}\n";
      } else {
        if (mapped.wrapper) { out += "wrapper,srlErr:=input.open(" + qualified(*mapped.wrapper) + ");if srlErr!=nil{return nil,srlErr}\n"; }
        out += "srlCount:=0\nfor {\nsrlPresent,srlErr:=input.has(" + qualified(mapped.qualified_name) + ");if srlErr!=nil{return nil,srlErr};if !srlPresent{break}\n";
        if (max) { out += "if srlCount>=" + std::to_string(*max) + "{return nil,fmt.Errorf(\"XML child order/cardinality mismatch\")}\n"; }
        if (scalar.type == object_type::class_type) {
          out += "srlValue,srlErr:=srlXmlDecode" + record_name(type(scalar)) + "(input," + qualified(mapped.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\n";
        } else {
          out += namespace_scalar(mapped)
              ? "srlText,srlScope,srlErr:=input.namespaceScalar(" + qualified(mapped.qualified_name) + ',' + namespace_mode(mapped) + ");if srlErr!=nil{return nil,srlErr}\n"
              : "srlText,srlErr:=input.scalar(" + qualified(mapped.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\n";
        }
      }
      if (scalar.type != object_type::class_type) {
        out += scalar_facets(mapped, patterns.contains(&source) ? patterns.at(&source) : "");
        if (namespace_scalar(mapped)) {
          const auto index = std::to_string(&mapped - contract.fields.data());
          out += "if value.srlXmlNamespaces==nil{value.srlXmlNamespaces=make(map[int][]map[string]string)}\n";
          out += "value.srlXmlNamespaces[" + index + "]=append(value.srlXmlNamespaces[" + index + "],srlScope)\n";
        }
        out += decode_scalar(scalar, type);
      }
      out += access + (optional ? "=&srlValue" : mapped.repeated ? source.fixed_extent != 0 ? "[srlCount]=srlValue" : "=append(" + access + ",srlValue)" : "=srlValue") + "\n";
      if (mapped.attribute) { out += "}\n"; }
      else if (!mapped.text) {
        out += "srlCount++\n";
        if(contract.extensions&&!mapped.wrapper)out+="if err:=srlExtensions.readAt(input,"+std::to_string(&mapped-contract.fields.data())+",srlCount);err!=nil{return nil,err}\n";
        out += "}\nif srlCount<" + std::to_string(min) + "{return nil,fmt.Errorf(\"XML child order/cardinality mismatch\")}\n";
        if (mapped.wrapper) { out += "if srlErr=wrapper.finish();srlErr!=nil{return nil,srlErr}\n"; }
      }
      out += "}\n";
    }
  }
  if (all) {
    out += "srlSeen:=[" + std::to_string(contract.fields.size()) + "]bool{}\n_ = srlSeen\nsrlAllCount:=0\n";
    for (const auto& mapped : contract.fields) {
      if (!mapped.attribute && mapped.source->type_name_list.front().is_optional()) {
        out += "value." + member_paths.at(mapped.source) + "=nil\n";
      }
    }
    out += "for {\nsrlEvent,srlErr:=input.peek();if srlErr!=nil{return nil,srlErr};if srlEvent.kind!='s'{break}\nswitch srlEvent.name{\n";
    for (std::size_t index{}; index < contract.fields.size(); ++index) {
      const auto& mapped = contract.fields[index];
      if (mapped.attribute) { continue; }
      const auto& scalar = leaf(*mapped.source);
      const auto seen = "srlSeen[" + std::to_string(index) + "]";
      out += "case " + qualified(mapped.qualified_name) + ":\nif " + seen +
          "{return nil,fmt.Errorf(\"Duplicate XML all child\")}\n";
      if (typed_list(mapped)) {
        out += decode_list(mapped, "value." + member_paths.at(mapped.source), type, patterns.at(mapped.source), index);
        out += seen + "=true;srlAllCount++\n";
        continue;
      }
      if (needs_presence(mapped)) {
        out += decode_presence(mapped, "value." + member_paths.at(mapped.source), type,
            patterns.contains(mapped.source) ? patterns.at(mapped.source) : "");
        out += seen + "=true;srlAllCount++\n";
        continue;
      }
      if (scalar.type == object_type::class_type) {
        out += "srlValue,srlErr:=srlXmlDecode" + record_name(type(scalar)) + "(input," +
            qualified(mapped.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\n";
      } else {
        out += namespace_scalar(mapped)
            ? "srlText,srlScope,srlErr:=input.namespaceScalar(" + qualified(mapped.qualified_name) + ',' + namespace_mode(mapped) + ");if srlErr!=nil{return nil,srlErr}\n"
            : "srlText,srlErr:=input.scalar(" + qualified(mapped.qualified_name) + ");if srlErr!=nil{return nil,srlErr}\n";
        out += scalar_facets(mapped, patterns.contains(mapped.source) ? patterns.at(mapped.source) : "");
        if (namespace_scalar(mapped)) {
          out += "if value.srlXmlNamespaces==nil{value.srlXmlNamespaces=make(map[int][]map[string]string)}\n";
          out += "value.srlXmlNamespaces[" + std::to_string(index) + "]=[]map[string]string{srlScope}\n";
        }
        out += decode_scalar(scalar, type);
      }
      out += "value." + member_paths.at(mapped.source) + '=' +
          (mapped.source->type_name_list.front().is_optional() ? "&srlValue" : "srlValue") +
          "\n" + seen + "=true;srlAllCount++\n";
    }
    out += "default:return nil,fmt.Errorf(\"Unexpected XML all child\")\n}\n}\n";
    if (contract.all_groups.front().minimum == 0U) { out += "if srlAllCount!=0{\n"; }
    else { out += "_ = srlAllCount\n"; }
    for (std::size_t index{}; index < contract.fields.size(); ++index) {
      const auto& mapped = contract.fields[index];
      if (!mapped.attribute && mapped.min_occurs != 0U) {
        out += "if !srlSeen[" + std::to_string(index) +
            "]{return nil,fmt.Errorf(\"Required XML all child missing\")}\n";
      }
    }
    if (contract.all_groups.front().minimum == 0U) { out += "}\n"; }
  }
  const bool has_text = std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; });
  if (contract.mixed) {
    out += decode_mixed(*contract.mixed, "value." + member_paths.at(contract.mixed->source), type, field, alternative);
  }
  if(contract.extensions)out+="if err:=srlExtensions.readAt(input,"+std::to_string(contract.fields.size())+",0);err!=nil{return nil,err}\nif err:=srlExtensions.finish();err!=nil{return nil,err}\nvalue.srlXmlPreserved=srlExtensions.state\n";
  if (!has_text) { out += "if err=fields.finish();err!=nil{return nil,err}\n"; }
  if (contract.identity) { out += "if err=input.identityFinish(srlIdentity);err!=nil{return nil,err}\n"; }
  out += choice_check(contract, [&](const member& source) { return "value." + member_paths.at(&source); }, "go");
  return out + "return value,nil\n}\n";
}
} // namespace rohit::serializer::writer::xml_portable
