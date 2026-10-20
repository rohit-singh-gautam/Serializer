// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"
#include "xml_portable_writer.hpp"
#include "xml_native_model_writer.hpp"
#include "xml_portable_projection_writer.hpp"
#include "xml_portable_extension_writer.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rohit::serializer::writer::xml_csharp {

// Supply whole-object XML runtime helpers inside the generated outer Schema container.
inline std::string runtime() {
  static constexpr std::string_view source =
#include "xml_csharp_runtime.inc"
      ;
  return std::string{source} + xml_portable::scalar_runtime("csharp") + xml_portable::map_runtime("csharp") +
      xml_portable::mixed_runtime("csharp") + xml_portable::identity_runtime("csharp") +
      xml_portable::html_runtime("csharp") + xml_portable::model_runtime("csharp") + xml_portable::extension_runtime("csharp");
}

// Escape a checked contract spelling for a C# string literal without changing wire names.
inline std::string quote(std::string_view value) {
  std::string result{"\""};
  for (const auto character : value) {
    switch (character) {
    case '\\': result += "\\\\"; break;
    case '"': result += "\\\""; break;
    case '\n': result += "\\n"; break;
    case '\r': result += "\\r"; break;
    case '\t': result += "\\t"; break;
    default: result += character; break;
    }
  }
  return result + '"';
}

// Resolve the scalar or record stored beneath a supported optional or repeated field.
inline const type_name& leaf(const member& value) {
  const auto* selected = &value.type_name_list.front();
  while (selected->is_optional() || selected->is_sequence()) { selected = &selected->arguments.front(); }
  return *selected;
}

// Convert one native scalar or synchronously borrowed scalar reference without target construction.
inline std::string scalar_value(const type_name& type, const std::string& expression, bool encoding,
    const xml_portable::type_name_fn& native_type) {
  if (xml_portable::value_projection(type)) {
    if (!encoding) { throw std::invalid_argument{"Readonly XML projection input requires the owning target"}; }
    const auto& owner = static_cast<const class_node&>(*type.resolved_node);
    return scalar_value(owner.reference->value_type, expression + ".ReadValue()", true, native_type);
  }
  if (type.is_digest()) {
    return std::string{encoding ? "SrlXmlDigestText(" : "SrlXmlDigestRead("} + expression + ',' +
        std::to_string(type.digest_extent) + (encoding ? ",output.DigestLimits)" : ",input.DigestLimits)");
  }
  if (type.type == object_type::enum_type) {
    std::string result = "(" + (encoding ? "((int)" + expression + ")" : expression) + " switch { ";
    const auto& values = static_cast<const enum_node&>(*type.resolved_node).enum_name_list;
    for (std::size_t index{}; index < values.size(); ++index) {
      result += (encoding ? std::to_string(index) : quote(values[index])) + " => " +
          (encoding ? quote(values[index]) : "(" + native_type(type) + ")" + std::to_string(index)) + ", ";
    }
    return result + "_ => throw new System.FormatException(\"Invalid XML enum\") })";
  }
  if (type.name == "string" || type.name.starts_with("version")) { return expression; }
  if (type.name == "bool") { return encoding ? "(" + expression + " ? \"true\" : \"false\")" : "SrlXmlBoolean(" + expression + ")"; }
  if (type.name == "float" || type.name == "double") {
    const auto single = type.name == "float" ? "true" : "false";
    return encoding ? "SrlXmlFloatText(" + expression + ", " + single + ")" :
        "(" + native_type(type) + ")SrlXmlFloat(" + expression + ", " + single + ")";
  }
  if (encoding) { return expression + ".ToString(System.Globalization.CultureInfo.InvariantCulture)"; }
  const auto bits = type.name.ends_with("64") ? 64 : type.name.ends_with("32") ? 32 : type.name.ends_with("16") ? 16 : 8;
  return "(" + native_type(type) + ")SrlXmlInteger(" + expression + ", " + std::to_string(bits) + ", " +
      (type.name.starts_with('u') ? "true" : "false") + ")";
}

// Emit exact static mappings inside one generated C# owning class; callbacks retain naming profiles.
// Bridge only the original private native variant storage needed by an owning mixed parent.
inline std::string mixed_fragment_methods(const class_node& object, const member& variant,
    const xml_portable::field_name& field_name, const xml_portable::type_name_fn& native_type,
    const xml_portable::alternative_name& alternative) {
  std::set<std::string> names{};
  for (const auto& item : object.member_list) {
    const auto name = field_name(item);
    if (item.modifier == member::modifier_type::variant) {
      names.insert(name + "Index");
      for (const auto& choice : item.type_name_list) { names.insert(name + alternative(choice.enum_name)); }
    } else { names.insert(name); }
  }
  for (const auto& item : object.functions) {
    member method{}; method.name = item.name; method.access = item.access; names.insert(field_name(method));
  }
  std::string out{};
  const auto bridge = [&](const std::string& name, const std::string& type, const std::string& storage) {
    if (!names.insert(name).second) { throw std::invalid_argument{"C# XML mixed helper collides with native storage: " + name}; }
    out += "    // Keep original native visibility while allowing static XML fragment traversal.\n    internal " +
        type + ' ' + name + " { get => this." + storage + "; set => this." + storage + " = value; }\n";
  };
  const auto storage = field_name(variant);
  bridge("SrlXmlMixedIndex", "int", storage + "Index");
  for (const auto& choice : variant.type_name_list) {
    const auto suffix = alternative(choice.enum_name);
    bridge("SrlXmlMixed" + suffix, native_type(choice), storage + suffix);
  }
  return out;
}

// Emit exact static mappings inside one generated C# owning class; callbacks retain naming profiles.
inline std::string methods(const class_node& object, const xml_binding::record& contract,
                           const xml_options& options, std::string_view model_name,
                           const std::function<std::string(const member&)>& field_name,
                           const std::function<std::string(const type_name&)>& native_type,
                           const xml_portable::alternative_name& alternative,
                           bool needs_base_bridges, bool readonly = false,
                           const xml_portable::field_name& projection = {}) {
  const bool export_graph = readonly || xml_codegen::native_xml_export_graph(object);
  const bool namespace_values = std::any_of(contract.fields.begin(), contract.fields.end(), xml_portable::namespace_dependent);
  const bool all = xml_portable::supports_all(contract);
  if (!contract.all_groups.empty() && !all) {
    throw std::invalid_argument{"C# XML all traversal requires one complete unwrapped singleton group"};
  }
  for (const auto& item : contract.fields) {
    if ((item.presence.explicit_policy || xml_portable::needs_presence(item)) && !xml_portable::supports_presence(item)) {
      throw std::invalid_argument{"C# XML presence requires ordinary scalar, record or wrapped sequence storage without lexical overlays"};
    }
    if (item.variant_binding && !xml_portable::supports_variant(item)) {
      throw std::invalid_argument{"C# XML variants require one complete dedicated wrapper without scalar overlays"};
    }
    if (item.map_binding && !xml_portable::supports_map(item)) {
      throw std::invalid_argument{"C# XML maps require one complete dedicated wrapper without scalar overlays"};
    }
  }
  std::map<const member*, std::string> member_paths{};
  std::vector<std::string> base_paths{};
  // Composed bases keep native ownership; internal properties bridge only inaccessible base storage.
  const auto collect_paths = [&](auto&& self, const class_node& owner, const std::string& prefix) -> void {
    for (std::size_t index{}; index < owner.parents.size(); ++index) {
      const auto& parent = owner.parents[index]; member base{};
      base.name = "base" + std::to_string(index); base.access = parent.access;
      const auto path = prefix + (&owner == &object && projection ? projection(base) :
          &owner != &object && parent.access != access_type::public_access ?
          "SrlXmlBase" + std::to_string(index) : field_name(base));
      base_paths.push_back(path); self(self, *parent.parent_class, path + '.');
    }
    for (std::size_t index{}; index < owner.member_list.size(); ++index) {
      const auto& item = owner.member_list[index];
      member_paths.emplace(&item, prefix + (&owner == &object && projection ? projection(item) :
          &owner != &object && item.access != access_type::public_access ?
          "SrlXmlField" + std::to_string(index) : field_name(item)));
    }
  };
  collect_paths(collect_paths, object, "");
  const auto mapped_field = [&](const member& item) { return member_paths.at(&item); };
  std::string source;
  std::size_t indent{2};
  // Retain a short native statement at the current class nesting level.
  const auto line = [&](const std::string& text) {
    source.append(indent * 2, ' '); source += text + '\n';
  };
  // Preserve exact native predicates while closing guards across specialized field branches.
  const auto revision_path = [&](const std::string& owner) {
    return contract.model.revision ? owner + mapped_field(*contract.model.revision) : std::string{};
  };
  const auto active_scope = [&](const member& item, const std::string& owner) {
    const auto condition = xml_codegen::native_xml_active(contract, item, version_language::csharp, revision_path(owner));
    if (!condition.empty()) { line("if (" + condition + ") {"); ++indent; }
    return xml_codegen::model_emission_scope{condition.empty() ? std::function<void()>{} :
        std::function<void()>{[&] { --indent; line("}"); }}};
  };
  std::set<std::string> native_names{};
  for (const auto& item : object.member_list) {
    const auto identifier = field_name(item);
    if (item.modifier == member::modifier_type::variant) {
      native_names.insert(identifier + "Index");
      for (const auto& choice : item.type_name_list) { native_names.insert(identifier + alternative(choice.enum_name)); }
    } else { native_names.insert(identifier); }
  }
  for (const auto& item : object.functions) {
    member method{}; method.name = item.name; method.access = item.access;
    native_names.insert(field_name(method));
  }
  for (const auto& item : object.constructions) {
    member method{}; method.name = item.constructor ? "create" : item.name;
    native_names.insert(field_name(method));
  }
  for (const auto& item : object.conversions) {
    member method{}; method.name = item.name; native_names.insert(field_name(method));
  }
  // Keep XML-only bridge names out of authored fields and behaviors before publishing generated code.
  const auto bridge = [&](const std::string& helper, const std::string& type, const std::string& storage) {
    if (!native_names.insert(helper).second) {
      throw std::invalid_argument{"C# XML inherited storage helper collides with a declared member: " + helper};
    }
    line("// Bridge native nonpublic storage for statically bound flattened XML descendants.");
    line("internal " + type + ' ' + helper + " { get => this." + storage + "; set => this." + storage + " = value; }");
  };
  for (std::size_t index{}; index < object.parents.size(); ++index) {
    if (!needs_base_bridges || readonly) { break; }
    const auto& parent = object.parents[index];
    member base{}; base.name = "base" + std::to_string(index); base.access = parent.access;
    native_names.insert(field_name(base));
    if (parent.access == access_type::public_access) { continue; }
    type_name parent_type{std::string{}, nullptr}; parent_type.type = object_type::class_type;
    parent_type.resolved_node = parent.parent_class;
    bridge("SrlXmlBase" + std::to_string(index), native_type(parent_type), field_name(base));
  }
  for (std::size_t index{}; index < object.member_list.size(); ++index) {
    if (!needs_base_bridges || readonly) { break; }
    const auto& item = object.member_list[index];
    if (item.access == access_type::public_access || item.omits("xml") || item.wire_only) { continue; }
    const auto helper = "SrlXmlField" + std::to_string(index), storage = field_name(item);
    if (item.modifier == member::modifier_type::variant) {
      bridge(helper + "Index", "int", storage + "Index");
      for (const auto& choice : item.type_name_list) {
        bridge(helper + alternative(choice.enum_name), native_type(choice), storage + alternative(choice.enum_name));
      }
    } else if (item.modifier == member::modifier_type::array) {
      bridge(helper, "System.Collections.Generic.List<" + native_type(item.type_name_list.front()) + '>', storage);
    } else if (item.modifier == member::modifier_type::map) {
      type_name key{std::string{item.key}, nullptr}; key.resolved_node = item.key_node;
      key.type = item.key_node ? object_type::enum_type : object_type::primitive;
      bridge(helper, "System.Collections.Generic.Dictionary<" + native_type(key) + ',' + native_type(item.type_name_list.front()) + '>', storage);
    } else { bridge(helper, native_type(item.type_name_list.front()), storage); }
  }
  // Select a C# lexical conversion with fixed integer and enum constraints.
  const auto scalar = [&](const rohit::serializer::type_name& type,
                          const std::string& expression, bool encoding) {
    return scalar_value(type, expression, encoding, native_type);
  };
  // Select stable expanded-name spelling independently of XML document prefixes.
  const auto expanded = [](const xml::name& name) {
    return quote(name.namespace_uri.empty() ? name.local : "{" + name.namespace_uri + "}" + name.local);
  };
  // Surround every known element with its owner-level foreign-root positions.
  const auto extension_scope = [&](const xml_binding::field& item,const std::string& owner,bool encoding) {
    if(!contract.extensions||item.attribute||item.text)return xml_codegen::model_emission_scope{{}};
    const auto position=std::to_string(&item-contract.fields.data());
    auto count=xml_codegen::portable_extension_field_count(item,owner+mapped_field(*item.source),"csharp");
    const auto condition=xml_codegen::native_xml_active(contract,*item.source,version_language::csharp,revision_path(owner));
    if(!condition.empty())count="("+condition+"?"+count+":0)";
    line("{");++indent;if(encoding&&item.repeated&&!item.wrapper)line("int srlExtensionItems=0;");
    line("srlExtensions."+std::string{encoding?"EmitAt(output,":"ReadAt(input,"}+position+",0);");
    return xml_codegen::model_emission_scope{[&,position,count,encoding]{line("srlExtensions."+std::string{encoding?"EmitAt(output,":"ReadAt(input,"}+position+','+count+");");if(encoding)line("srlExtensions.EndPosition("+position+");");--indent;line("}");}};
  };
  if(contract.extensions){
    for(const auto& source_field:object.member_list)if(field_name(source_field)=="SrlXmlPreservedState"||field_name(source_field)=="SrlXmlExtensionMetadata")throw std::invalid_argument{"C# XML extension state collides with a native field"};
    line("// Immutable foreign-only permissions and XML-only preservation state.");
    line("private static readonly SrlXmlExtensionContract SrlXmlExtensionMetadata="+xml_codegen::portable_extension_metadata(*contract.extensions,quote,expanded,"csharp")+';');
    if(!readonly)line("private SrlXmlPreserved? SrlXmlPreservedState;");
  }
  if (namespace_values) {
    if (native_names.contains("srlXmlNamespaces") || native_names.contains("SetXmlNamespaces")) {
      throw std::invalid_argument{"C# XML namespace metadata collides with a native field"};
    }
    line("// XML-only occurrence scopes stay separate from native field codecs.");
    line("private readonly SrlXmlNamespaceValues srlXmlNamespaces=new();");
    line("// Replace QName scopes using copied validated bindings for native-authored values.");
    line("public void SetXmlNamespaces(System.Collections.Generic.IDictionary<string,string> bindings)=>srlXmlNamespaces.Set(bindings);");
  }
  std::map<const member*, std::size_t> rule_indices{};
  for (std::size_t index{}; index < contract.fields.size(); ++index) {
    const auto& item = contract.fields[index]; rule_indices.emplace(item.source, index);
    if (!xml_portable::needs_scalar_rule(item)) { continue; }
    line("// Prepare immutable facets and portable programs once for both XML directions.");
    line(xml_portable::scalar_rule(item, index, "csharp"));
  }
  if (contract.identity) {
    line("// Immutable owner-local key/unique/keyref projections used by both token directions.");
    line("private static readonly SrlXmlIdentityScope SrlXmlIdentityMetadata=" +
        xml_portable::identity_metadata(*contract.identity, "csharp") + ';');
  }
  // The portable emitter validates normalized field, function, factory and conversion names.
  line("// Encode one complete XML document using its fixed generated mapping.");
  line("public byte[] EncodeXml() { return EncodeXml(new Limits(), null); }");
  line("// Bound output and optionally reuse an explicitly prepared runtime XSD.");
  line("public byte[] EncodeXml(Limits limits, XmlSchema? schema = null) {"); ++indent;
  line("return EncodeXml(limits, schema, new XmlMapLimits());"); --indent; line("}");
  line("// Carry explicit per-wrapper map bounds through the complete output graph.");
  line("public byte[] EncodeXml(Limits limits, XmlSchema? schema, XmlMapLimits maps) {"); ++indent;
  line("return EncodeXml(limits, schema, maps, new XmlMixedLimits());"); --indent; line("}");
  line("// Carry explicit map and ordered mixed limits through every descendant.");
  line("public byte[] EncodeXml(Limits limits, XmlSchema? schema, XmlMapLimits maps, XmlMixedLimits mixed) {"); ++indent;
  line("return EncodeXml(limits,schema,maps,mixed,new XmlIdentityLimits());"); --indent; line("}");
  line("// Carry explicit identity tuple/selector admission without changing earlier overloads.");
  line("public byte[] EncodeXml(Limits limits, XmlSchema? schema, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity) {"); ++indent;
  line("return EncodeXml(limits,schema,maps,mixed,identity,new XmlDigestLimits());"); --indent; line("}");
  line("// Bound owning digest bytes through the complete graph without changing earlier overloads.");
  line("public byte[] EncodeXml(Limits limits, XmlSchema? schema, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests) {"); ++indent;
  line("return EncodeXml(limits,schema,maps,mixed,identity,digests,new XmlExtensionOptions());");--indent;line("}");
  line("// Carry explicit vocabulary understanding and opaque preservation admission.");
  line("public byte[] EncodeXml(Limits limits, XmlSchema? schema, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests, XmlExtensionOptions extensions) {"); ++indent;
  line("var output = new SrlXmlOutput(limits, maps, mixed, identity, digests, extensions);");
  line("XmlWrite(output, " + expanded(contract.root) + ");");
  line("byte[] bytes = output.Finish();");
  if (!options.runtime_xsd) { line("if (schema != null) throw new System.InvalidOperationException(\"Runtime XSD was not enabled during generation\");"); }
  else if (options.validation_scope != "input") { line("if (schema != null) schema.Validate(bytes, limits);"); }
  line("return bytes;"); --indent; line("}");
  if (!readonly) {
  line("// Decode exactly one XML document into a fresh unpublished owning value.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes) { return DecodeXml(bytes, new Limits(), null); }");
  line("// Enforce static shape, bounded parsing and selected optional runtime XSD validation.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes, Limits limits, XmlSchema? schema = null) {"); ++indent;
  line("return DecodeXml(bytes, limits, schema, new XmlMapLimits());"); --indent; line("}");
  line("// Carry explicit per-wrapper map bounds through the complete input graph.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes, Limits limits, XmlSchema? schema, XmlMapLimits maps) {"); ++indent;
  line("return DecodeXml(bytes, limits, schema, maps, new XmlMixedLimits());"); --indent; line("}");
  line("// Validate complete input with explicit map and ordered mixed admission policies.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes, Limits limits, XmlSchema? schema, XmlMapLimits maps, XmlMixedLimits mixed) {"); ++indent;
  line("return DecodeXml(bytes,limits,schema,maps,mixed,new XmlIdentityLimits());"); --indent; line("}");
  line("// Validate every completed owner-local scope under explicit identity admission.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes, Limits limits, XmlSchema? schema, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity) {"); ++indent;
  line("return DecodeXml(bytes,limits,schema,maps,mixed,identity,new XmlDigestLimits());"); --indent; line("}");
  line("// Admit each owning digest before allocating its unpublished byte candidate.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes, Limits limits, XmlSchema? schema, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests) {"); ++indent;
  line("return DecodeXml(bytes,limits,schema,maps,mixed,identity,digests,new XmlExtensionOptions());");--indent;line("}");
  line("// Carry explicit vocabulary understanding and opaque preservation admission.");
  line("public static " + std::string{model_name} + " DecodeXml(byte[] bytes, Limits limits, XmlSchema? schema, XmlMapLimits maps, XmlMixedLimits mixed, XmlIdentityLimits identity, XmlDigestLimits digests, XmlExtensionOptions extensions) {"); ++indent;
  if (export_graph) {
    line("throw new System.FormatException(\"Readonly XML projections are export-only; decode the owning target\");");
  } else {
  if (!options.runtime_xsd) { line("if (schema != null) throw new System.InvalidOperationException(\"Runtime XSD was not enabled during generation\");"); }
  else if (options.validation_scope != "output") { line("if (schema != null) schema.Validate(bytes, limits);"); }
  line("using var input = new SrlXmlInput(bytes, limits, maps, mixed, identity, digests, extensions);");
  line("var value = XmlRead(input, " + expanded(contract.root) + ");");
  line("input.Finish();"); line("return value;");
  }
  --indent; line("}");
  }
  line("// Emit direct typed tokens while retaining only active ancestor state.");
  line("internal void XmlWrite(SrlXmlOutput output, string name) {"); ++indent;
  line("XmlWrite(output, name, null);"); --indent; line("}");
  line("// Add an explicit type before native attributes without changing ordinary callers.");
  line("internal void XmlWrite(SrlXmlOutput output, string name, string? typeIdentity) {"); ++indent;
  line("using var srlOperationOwner=output.FieldOwner();");
  if(contract.extensions){
    line(readonly?"var srlScope=new System.Collections.Generic.Dictionary<string,string>();":"var srlScope=SrlXmlPreservedState?.Scope??new System.Collections.Generic.Dictionary<string,string>();");
    line("var srlExtensions=new SrlXmlExtensions(SrlXmlExtensionMetadata,srlScope,output.ExtensionOptions,"+std::string{readonly?"null":"SrlXmlPreservedState"}+");");
    line("output.Open(name,srlScope);");
  } else if (namespace_values) {
    line("output.Open(name,new System.Collections.Generic.Dictionary<string,string>{{\"\",\"\"}});");
  } else { line("output.Open(name);"); }
  if (contract.identity) { line("var srlIdentity=output.IdentityScope(SrlXmlIdentityMetadata);"); }
  line("if (typeIdentity != null) output.TypeName(typeIdentity);");
  for (const auto& path : base_paths) {
    line("if (this." + path + " is null) throw new System.FormatException(\"Null XML base record\");");
  }
  if (const auto* revision = contract.model.revision) {
    const auto access = revision_path("this.");
    line("if (" + xml_codegen::native_xml_revision_invalid(*revision, version_language::csharp, access) +
        ") throw new System.FormatException(\"Unsupported XML revision\");");
    line("output.Attribute(" + expanded({"revision", std::string{xml_binding::model_metadata_namespace}}) +
        ",SrlXmlRevisionLexical(" + scalar(revision->type_name_list.front(), access, true) + ',' + quote(revision->type_name_list.front().name) + "));");
  }
  if (contract.model.magic) {
    line("output.Attribute(" + expanded({"magic", std::string{xml_binding::model_metadata_namespace}}) + ',' + quote(*contract.model.magic) + ");");
  }
  line(xml_portable::choice_check(contract,
      [&](const member& item) { return "this." + mapped_field(item); }, "csharp"));
  for (const bool attributes : {true, false}) {
  if(contract.extensions&&!attributes)line("srlExtensions.EmitAttributes(output);");
  for (const auto& item : contract.fields) {
    if (item.attribute != attributes) { continue; }
    line("output.Field(" + quote(item.source->name) + ");");
    const auto extensions=extension_scope(item,"this.",true);
    const auto active = active_scope(*item.source, "this.");
    if (xml_portable::typed_list(item)) {
      line(xml_portable::list_write(item, rule_indices.at(item.source),
          "this." + mapped_field(*item.source), scalar, "csharp"));
      continue;
    }
    if (xml_portable::needs_presence(item)) {
      line(xml_portable::presence_write(item, rule_indices.at(item.source),
          "this." + mapped_field(*item.source), scalar, "csharp"));
      continue;
    }
    if (item.variant_binding) {
      line(xml_portable::variant_write(*item.variant_binding, "this." + mapped_field(*item.source),
          alternative, scalar, "csharp"));
      continue;
    }
    if (item.map_binding) {
      line(xml_portable::map_write(*item.map_binding, "this." + mapped_field(*item.source),
          native_type, scalar, "csharp"));
      continue;
    }
    const auto& value = *item.source;
    const auto& type = leaf(value);
    const auto checked = [&](const std::string& expression) {
      return xml_portable::scalar_output(item, rule_indices.at(item.source), expression, "csharp");
    };
    const auto access = "this." + mapped_field(value);
    const bool optional = value.type_name_list.front().is_optional();
    std::string selected = access;
    line("{"); ++indent;
    if (xml_portable::namespace_dependent(item)) { line("int srlNamespaceIndex=0;"); }
    if (item.wrapper) {
      line("output.Open(" + expanded(*item.wrapper) + ");");
    }
    if (optional) { line("var srlOptional=" + access + ";if (srlOptional.HasValue) {"); ++indent; selected = "srlOptional.Value.Item1"; }
    if (item.repeated) {
      line("if (" + access + ".Count < " + std::to_string(item.min_occurs) +
           (item.max_occurs ? " || " + access + ".Count > " + std::to_string(*item.max_occurs) : "") +
           ") throw new System.FormatException(\"XML occurrence mismatch\");");
      if (value.fixed_extent) { line("if (" + access + ".Count != " + std::to_string(value.fixed_extent) + ") throw new System.FormatException(\"XML fixed extent mismatch\");"); }
      line("foreach (var item in " + access + ") {"); ++indent; selected = "item";
    }
    if (item.text) { line("output.Text(" + checked(scalar(type, selected, true)) + ");"); }
    else if (item.attribute) { line("output.Attribute(" + expanded(item.qualified_name) + ", " + checked(scalar(type, selected, true)) + ");"); }
    else if (xml_portable::projection_payload(type).type == object_type::class_type) {
      line("if(" + selected + " is null)throw new System.FormatException(\"Null XML record\");");
      line(selected + ".XmlWrite(output, " + expanded(item.qualified_name) + ");");
    }
    else {
      line("output.Open(" + expanded(item.qualified_name) + ");");
      line("output.Text(" + checked(scalar(type, selected, true)) + ");");
      line("output.Close();");
    }
    if(contract.extensions&&item.repeated&&!item.wrapper)line("srlExtensions.EmitAt(output,"+std::to_string(&item-contract.fields.data())+",++srlExtensionItems);");
    if (item.repeated) { --indent; line("}"); }
    if (optional) { --indent; line("}"); }
    if (item.wrapper) { line("output.Close();"); }
    --indent; line("}");
  }
  }
  if (contract.mixed) {
    line("output.Field(" + quote(contract.mixed->source->name) + ");");
    line(xml_portable::mixed_write(*contract.mixed, "this." + mapped_field(*contract.mixed->source),
        field_name, alternative, "csharp"));
  }
  line("output.Field(\"\");");
  if(contract.extensions){line("srlExtensions.EmitAt(output,"+std::to_string(contract.fields.size())+",0);");line("srlExtensions.FinishOutput();");}
  line("output.Close();");
  if (contract.identity) { line("output.IdentityFinish(srlIdentity);"); }
  --indent; line("}");
  if (readonly) { return source; }
  line("// Decode typed tokens into a fresh candidate without retaining XML descendants.");
  line("internal static " + std::string{model_name} + " XmlRead(SrlXmlInput input, string name) {"); ++indent;
  if (export_graph) {
    line("throw new System.FormatException(\"Readonly XML projections are export-only; decode the owning target\");");
    --indent; line("}"); return source;
  }
  line("using var srlOperationOwner=input.FieldOwner();");
  if (contract.identity) { line("var srlIdentity=input.IdentityScope(SrlXmlIdentityMetadata);"); }
  if(contract.extensions)line("var srlExtensions=new SrlXmlExtensions(SrlXmlExtensionMetadata,input.NamespaceScope(),input.ExtensionOptions,null);");
  if (std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& item) {
        return (item.attribute || item.text) && xml_portable::namespace_dependent(item);
      })) { line("var srlNamespaceScope=input.NamespaceScope();"); }
  line("var fields = input.Open(name);"); line("var result = new " + std::string{model_name} + "();");
  if (const auto* revision = contract.model.revision) {
    const auto text = "SrlXmlRevisionLexical(fields.Attribute(" +
        expanded({"revision", std::string{xml_binding::model_metadata_namespace}}) + ",false)!," + quote(revision->type_name_list.front().name) + ')';
    line(revision_path("result.") + '=' + scalar(revision->type_name_list.front(), text, false) + ';');
    line("if (" + xml_codegen::native_xml_revision_invalid(*revision, version_language::csharp, revision_path("result.")) +
        ") throw new System.FormatException(\"Unsupported XML revision\");");
  }
  if (contract.model.magic) {
    line("if (fields.Attribute(" + expanded({"magic", std::string{xml_binding::model_metadata_namespace}}) + ",false)!=" +
        quote(*contract.model.magic) + ") throw new System.FormatException(\"XML magic mismatch\");");
  }
  for (const bool attributes : {true, false}) {
    if (all && !attributes) { continue; }
    if(contract.extensions&&!attributes)line("foreach(var entry in fields.Attributes)srlExtensions.Attribute(entry.Key,entry.Value);fields.Attributes.Clear();");
    for (const auto& item : contract.fields) {
      if (item.attribute != attributes) { continue; }
      line("input.Field(" + quote(item.source->name) + ");");
      const auto extensions=extension_scope(item,"result.",false);
      const auto active = active_scope(*item.source, "result.");
      if (xml_portable::typed_list(item)) {
        const auto access = "result." + mapped_field(*item.source);
        const bool optional = item.source->type_name_list.front().is_optional();
        const auto text = item.attribute ? "fields.Attribute(" + expanded(item.qualified_name) + ',' +
            (optional || item.default_value || item.fixed_value ? "true" : "false") + ')' :
            item.text ? "fields.Text()" : "input.Scalar(" + expanded(item.qualified_name) + ')';
        if (!item.attribute && !item.text && optional) { line("if(input.Has(" + expanded(item.qualified_name) + ")) {"); ++indent; }
        line(xml_portable::list_read(item, rule_indices.at(item.source), access, text, native_type, scalar, "csharp"));
        if (!item.attribute && !item.text && optional) { --indent; line("}else{" + access + "=null;}"); }
        continue;
      }
      if (xml_portable::needs_presence(item)) {
        line(xml_portable::presence_read(item, rule_indices.at(item.source),
            "result." + mapped_field(*item.source), native_type, scalar, "csharp"));
        continue;
      }
      if (item.variant_binding) {
        line(xml_portable::variant_read(*item.variant_binding, "result." + mapped_field(*item.source),
            alternative, native_type, scalar, "csharp"));
        continue;
      }
      if (item.map_binding) {
        const auto& map = *item.map_binding;
        line(xml_portable::map_read(map, "result." + mapped_field(*item.source), native_type, scalar,
            "csharp", "new System.Collections.Generic.Dictionary<" + native_type(map.key_type) + ',' + native_type(map.value_type) + ">()"));
        continue;
      }
      if (item.min_occurs > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
          (item.max_occurs && *item.max_occurs > static_cast<std::size_t>(std::numeric_limits<int>::max()))) {
        throw std::invalid_argument{"C# XML cardinality exceeds supported collection size"};
      }
      const auto& value = *item.source;
      const auto& type = leaf(value);
      const bool optional = value.type_name_list.front().is_optional();
      const auto access = "result." + mapped_field(value);
      const auto read_value = [&](const std::string& text) {
        return type.type == object_type::class_type ? native_type(type) + ".XmlRead(input, " + expanded(item.qualified_name) + ")" :
            scalar(type, xml_portable::scalar_input(item, rule_indices.at(item.source), text, "csharp"), false);
      };
      line("{"); ++indent;
      if (item.text) {
        line(access + " = " + read_value("fields.Text()") + ";");
      } else if (item.attribute) {
        line("var text = fields.Attribute(" + expanded(item.qualified_name) + ", " + (optional || item.default_value || item.fixed_value ? "true" : "false") + ");");
        line(access + " = " + (optional ? (item.default_value || item.fixed_value ? "new System.ValueTuple<" + native_type(type) + ">(" + read_value("text") + ")" : "text == null ? null : new System.ValueTuple<" + native_type(type) + ">(" + read_value("text") + ")") : read_value("text!")) + ";");
      } else {
        if (item.wrapper) { line("var wrapper = input.Open(" + expanded(*item.wrapper) + ");"); }
        line("int count = 0;");
        if (item.repeated) { line(access + " = new System.Collections.Generic.List<" + native_type(type) + ">();"); }
        if (optional) { line(access + " = null;"); }
        line("while (input.Has(" + expanded(item.qualified_name) + ")) {"); ++indent;
        const auto maximum = value.fixed_extent ? std::optional<std::size_t>{static_cast<std::size_t>(value.fixed_extent)} : item.max_occurs;
        if (maximum) { line("if (count >= " + std::to_string(*maximum) + ") throw new System.FormatException(\"XML occurrence mismatch\");"); }
        const auto decoded = read_value("input.Scalar(" + expanded(item.qualified_name) + ")");
        line(item.repeated ? access + ".Add(" + decoded + ");" : access + " = " + (optional ? "new System.ValueTuple<" + native_type(type) + ">(" + decoded + ")" : decoded) + ";");
        line("++count;");
        if(contract.extensions&&item.repeated&&!item.wrapper)line("srlExtensions.ReadAt(input,"+std::to_string(&item-contract.fields.data())+",count);");
        --indent; line("}");
        const auto minimum = value.fixed_extent ? static_cast<std::size_t>(value.fixed_extent) : item.min_occurs;
        line("if (count < " + std::to_string(minimum) + ") throw new System.FormatException(\"XML occurrence mismatch\");");
        if (item.wrapper) { line("wrapper.Finish();"); }
      }
      --indent; line("}");
    }
  }
  if (all) {
    line("var seen = new bool[" + std::to_string(contract.fields.size()) + "];");
    line("int allCount = 0;");
    for (const auto& item : contract.fields) {
      if (!item.attribute && item.source->type_name_list.front().is_optional()) {
        line("result." + mapped_field(*item.source) + " = null;");
      }
    }
    line("while (true) {"); ++indent;
    line("input.SkipSpace();"); line("if (input.Kind != 's') break;");
    line("switch (input.Name) {"); ++indent;
    for (std::size_t index{}; index < contract.fields.size(); ++index) {
      const auto& item = contract.fields[index];
      if (item.attribute) { continue; }
      const auto& type = leaf(*item.source);
      const auto seen = "seen[" + std::to_string(index) + "]";
      line("case " + expanded(item.qualified_name) + ": {"); ++indent;
      line("input.Field(" + quote(item.source->name) + ");");
      line("if (" + seen + ") throw new System.FormatException(\"Duplicate XML all child\");");
      if (xml_portable::typed_list(item)) {
        line(xml_portable::list_read(item, rule_indices.at(item.source), "result." + mapped_field(*item.source),
            "input.Scalar(" + expanded(item.qualified_name) + ')', native_type, scalar, "csharp"));
        line(seen + " = true;"); line("++allCount;"); line("break;"); --indent; line("}"); continue;
      }
      if (xml_portable::needs_presence(item)) {
        line(xml_portable::presence_read(item, rule_indices.at(item.source),
            "result." + mapped_field(*item.source), native_type, scalar, "csharp"));
        line(seen + " = true;"); line("++allCount;"); line("break;");
        --indent; line("}");
        continue;
      }
      const auto decoded = type.type == object_type::class_type ?
          native_type(type) + ".XmlRead(input, " + expanded(item.qualified_name) + ")" :
          scalar(type, xml_portable::scalar_input(item, rule_indices.at(item.source),
              "input.Scalar(" + expanded(item.qualified_name) + ")", "csharp"), false);
      line("result." + mapped_field(*item.source) + " = " +
          (item.source->type_name_list.front().is_optional() ? "new System.ValueTuple<" + native_type(type) + ">(" + decoded + ")" : decoded) + ";");
      line(seen + " = true;"); line("++allCount;"); line("break;");
      --indent; line("}");
    }
    line("default: throw new System.FormatException(\"Unexpected XML all child\");");
    --indent; line("}"); --indent; line("}");
    const bool optional_group = contract.all_groups.front().minimum == 0U;
    if (optional_group) { line("if (allCount != 0) {"); ++indent; }
    else { line("_ = allCount;"); }
    for (std::size_t index{}; index < contract.fields.size(); ++index) {
      const auto& item = contract.fields[index];
      if (!item.attribute && item.min_occurs != 0U) {
        line("if (!seen[" + std::to_string(index) + "]) throw new System.FormatException(\"Required XML all child missing\");");
      }
    }
    if (optional_group) { --indent; line("}"); }
  }
  if (contract.mixed) {
    line("input.Field(" + quote(contract.mixed->source->name) + ");");
    line(xml_portable::mixed_read(*contract.mixed, "result." + mapped_field(*contract.mixed->source),
        native_type, field_name, alternative, "csharp"));
  }
  line("input.Field(\"\");");
  if(contract.extensions){line("srlExtensions.ReadAt(input,"+std::to_string(contract.fields.size())+",0);");line("srlExtensions.Finish();");line("result.SrlXmlPreservedState=srlExtensions.State;");}
  line("fields.Finish();");
  if (contract.identity) { line("input.IdentityFinish(srlIdentity);"); }
  line(xml_portable::choice_check(contract,
      [&](const member& item) { return "result." + mapped_field(item); }, "csharp"));
  line("return result;"); --indent; line("}");
  return source;
}
} // namespace rohit::serializer::writer::xml_csharp
