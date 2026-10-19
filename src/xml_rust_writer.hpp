// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "behavior_writer.hpp"
#include "xml_schema.hpp"
#include "xml_native_choice_writer.hpp"
#include "xml_rust_container_writer.hpp"
#include "xml_rust_variant_writer.hpp"
#include "xml_rust_mixed_writer.hpp"
#include "xml_native_model_writer.hpp"
#include "xml_native_scalar_writer.hpp"
#include "xml_native_identity_writer.hpp"
#include "xml_native_extension_writer.hpp"
#include "xml_native_extension_frames.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_binding {
// Emit bounded native Rust XML helpers; roxmltree is required only when XML is selected.
inline std::string rust_runtime() {
  return xml_codegen::rust_scalar_constants() + xml_codegen::native_identity_constants(true) + xml_codegen::native_extension_constants(true) + R"xml(
/// Complete-document XML budgets; native parser storage is bounded by input/nodes.
#[derive(Clone, Copy, Debug)]
pub struct XmlLimits { pub max_bytes:usize, pub max_text_bytes:usize, pub max_nodes:usize,
    pub max_attributes:usize, pub max_depth:usize }
impl Default for XmlLimits {
    /// Apply independent conservative complete-message defaults.
    fn default() -> Self { Self { max_bytes:16*1024*1024,max_text_bytes:8*1024*1024,
        max_nodes:100000,max_attributes:100000,max_depth:128 } }
}
/// A caller-owned trusted prepared XSD adapter; generated mapping checks always remain mandatory.
pub trait XmlValidator { fn validate(&self, document:&str) -> Result<(),Error>; }
/// Compare expanded names, independently of prefix spelling.
fn srl_xml_name(node:roxmltree::Node<'_, '_>, uri:&str, local:&str) -> bool {
    node.is_element() && node.tag_name().name() == local && node.tag_name().namespace().unwrap_or("") == uri
}
/// Refuse declaration tokens without confusing literal CDATA/comment content with a DTD.
fn srl_xml_reject_dtd(text:&str) -> Result<(),Error> {
    let mut remaining=text;
    while let Some(position)=remaining.find('<') {
        remaining=&remaining[position..];
        let end=if remaining.starts_with("<!--") { Some("-->") }
            else if remaining.starts_with("<![CDATA[") { Some("]]>") }
            else if remaining.starts_with("<?") { Some("?>") }
            else if remaining.starts_with("<!DOCTYPE") || remaining.starts_with("<!ENTITY") { return Err(Error("XML DTD/entities forbidden")); }
            else { None };
        if let Some(end)=end {
            if let Some(position)=remaining[2..].find(end) { remaining=&remaining[position+2+end.len()..]; }
            else { return Ok(()); }
        } else { remaining=&remaining[1..]; }
    }
    Ok(())
}
/// Enforce an XML 1.0/UTF-8 header for an already UTF-8 native string.
fn srl_xml_header(text:&str) -> Result<(),Error> {
    let text=text.strip_prefix('\u{feff}').unwrap_or(text);
    if !text.starts_with("<?xml") || !text.as_bytes().get(5).is_some_and(u8::is_ascii_whitespace) { return Ok(()); }
    let end=text.find("?>").ok_or(Error("Malformed XML declaration"))?;
    let mut body=&text[5..end]; let mut previous=None;
    while !body.trim_matches([' ','\t','\r','\n']).is_empty() {
        let trimmed=body.trim_start_matches([' ','\t','\r','\n']);
        if trimmed.len() == body.len() { return Err(Error("Malformed XML declaration")); } body=trimmed;
        let length=body.bytes().take_while(u8::is_ascii_alphabetic).count();
        let key=&body[..length]; body=body[length..].trim_start_matches([' ','\t','\r','\n']);
        body=body.strip_prefix('=').ok_or(Error("Malformed XML declaration"))?.trim_start_matches([' ','\t','\r','\n']);
        let quote=body.chars().next().ok_or(Error("Malformed XML declaration"))?;
        if quote != '\'' && quote != '"' { return Err(Error("Malformed XML declaration")); }
        body=&body[1..]; let length=body.find(quote).ok_or(Error("Malformed XML declaration"))?;
        let value=&body[..length]; body=&body[length+1..];
        let slot=match key { "version" if value == "1.0" => 0,
            "encoding" if value.eq_ignore_ascii_case("UTF-8") => 1,
            "standalone" if value == "yes" || value == "no" => 2,
            _ => return Err(Error("Only XML 1.0 UTF-8 declarations supported")) };
        if previous.is_none() && slot != 0 || previous.is_some_and(|old| old >= slot) { return Err(Error("Malformed XML declaration")); }
        previous=Some(slot);
    }
    if previous.is_none() { return Err(Error("Missing XML declaration version")); } Ok(())
}
/// Parse exactly one bounded UTF-8 XML document, with DTD/entities/resolvers disabled.
fn srl_xml_parse(text:&str, limits:XmlLimits) -> Result<roxmltree::Document<'_>,Error> {
    if text.len() > limits.max_bytes { return Err(Error("XML input byte limit")); }
    srl_xml_header(text)?;
    srl_xml_reject_dtd(text)?;
    let document = roxmltree::Document::parse_with_options(text,roxmltree::ParsingOptions {
        allow_dtd:false, nodes_limit:limits.max_bytes.saturating_add(1).min(u32::MAX as usize) as u32, entity_resolver:None
    }).map_err(|_| Error("Malformed XML"))?;
    let mut attributes = 0usize; let mut text_bytes = 0usize; let mut elements=0usize;
    for node in document.descendants() {
        if node.is_element() {
            elements += 1;
            if elements > limits.max_nodes || node.ancestors().filter(|item| item.is_element()).count() > limits.max_depth { return Err(Error("XML node/depth limit")); }
            text_bytes = text_bytes.checked_add(node.tag_name().name().len()+node.tag_name().namespace().unwrap_or("").len()).ok_or(Error("XML text limit"))?;
            for namespace in node.namespaces() {
                if node.parent().and_then(|parent| parent.lookup_namespace_uri(namespace.name())) != Some(namespace.uri()) {
                    attributes = attributes.checked_add(1).ok_or(Error("XML attribute limit"))?;
                    text_bytes = text_bytes.checked_add(namespace.name().unwrap_or("").len()+namespace.uri().len()).ok_or(Error("XML text limit"))?;
                }
            }
        }
        if let Some(value) = node.text().filter(|_| node.is_text()) {
            text_bytes = text_bytes.checked_add(value.len()).ok_or(Error("XML text limit"))?;
        }
        for attribute in node.attributes() {
            attributes = attributes.checked_add(1).ok_or(Error("XML attribute limit"))?;
            text_bytes = text_bytes.checked_add(attribute.value().len()+attribute.name().len()+attribute.namespace().unwrap_or("").len()).ok_or(Error("XML text limit"))?;
        }
        if attributes > limits.max_attributes || text_bytes > limits.max_text_bytes { return Err(Error("XML metadata limit")); }
        if node.children().any(|child| child.is_element()) && node.children().any(|child|
            child.is_text() && !child.text().unwrap_or("").trim_matches([' ','\t','\r','\n']).is_empty()) {
            return Err(Error("Mixed XML content unsupported"));
        }
    }
    Ok(document)
}
/// Encode directly to bounded escaped text without an intermediate XML tree.
struct SrlXmlWriter { text:String, limits:XmlLimits, value_limits:XmlLimits, map_limits:XmlMapLimits, mixed_limits:XmlMixedLimits, digest_limits:XmlDigestLimits, nodes:usize, depth:usize, attributes:usize, text_bytes:usize, namespaces:Vec<String>, pending_type:Option<(&'static str,&'static str)>, prefixed_type_depths:Vec<usize> }
impl SrlXmlWriter {
    /// Keep one cumulative XML output policy across nested generated record calls.
    fn new(limits:XmlLimits) -> Self { Self {text:String::new(),limits,value_limits:limits,map_limits:XmlMapLimits::default(),mixed_limits:XmlMixedLimits::default(),digest_limits:XmlDigestLimits::default(),nodes:0,depth:0,attributes:0,text_bytes:0,namespaces:Vec::new(),pending_type:None,prefixed_type_depths:Vec::new()} }
    /// Bound output before allocation and append one trusted framing fragment.
    fn put(&mut self,value:&str) -> Result<(),Error> {
        if value.len() > self.limits.max_bytes.saturating_sub(self.text.len()) { return Err(Error("XML output byte limit")); }
        self.text.try_reserve(value.len()).map_err(|_| Error("XML output allocation"))?;
        self.text.push_str(value); Ok(())
    }
    /// Charge the same retained name/namespace/value bytes as the native decoder.
    fn charge(&mut self,value:&str) -> Result<(),Error> {
        self.text_bytes = self.text_bytes.checked_add(value.len()).ok_or(Error("XML text limit"))?;
        if self.text_bytes > self.limits.max_text_bytes { return Err(Error("XML text limit")); }
        Ok(())
    }
    /// Escape scalar characters for XML text or attributes, preserving normalized whitespace.
    fn escaped(&mut self,value:&str,attribute:bool) -> Result<(),Error> {
        self.charge(value)?;
        for c in value.chars() {
            let number = c as u32;
            if !(number == 9 || number == 10 || number == 13 || (32..=0xd7ff).contains(&number) ||
                (0xe000..=0xfffd).contains(&number) || (0x10000..=0x10ffff).contains(&number)) {
                return Err(Error("Illegal XML character"));
            }
            match c { '&' => self.put("&amp;")?, '<' => self.put("&lt;")?, '>' => self.put("&gt;")?,
                '"' if attribute => self.put("&quot;")?, '\r' => self.put("&#13;")?,
                '\n' if attribute => self.put("&#10;")?, '\t' if attribute => self.put("&#9;")?,
                _ => { let mut bytes=[0u8;4]; self.put(c.encode_utf8(&mut bytes))?; } }
        }
        Ok(())
    }
    /// Begin a statically named element and its local default namespace binding.
    fn open(&mut self,local:&str,uri:&str) -> Result<(),Error> {
        self.nodes += 1; self.depth += 1;
        if self.nodes > self.limits.max_nodes || self.depth > self.limits.max_depth { return Err(Error("XML node/depth limit")); }
        self.charge(local)?; self.charge(uri)?;
        let pending=self.pending_type.take();
        let prefixed=pending.is_some_and(|(namespace,_)| namespace.is_empty() && !uri.is_empty());
        self.put("<")?; if prefixed { self.put("srlvalue:")?; } self.put(local)?;
        let default_uri=if prefixed {""} else {uri};
        if prefixed {
            self.attributes+=1; if self.attributes > self.limits.max_attributes { return Err(Error("XML attribute limit")); }
            self.put(" xmlns:srlvalue=\"")?; self.escaped(uri,true)?; self.put("\"")?;
            self.prefixed_type_depths.push(self.depth);
        }
        if self.namespaces.last().map(String::as_str).unwrap_or("") != default_uri {
            self.attributes += 1;
            if self.attributes > self.limits.max_attributes { return Err(Error("XML attribute limit")); }
            self.put(" xmlns=\"")?; self.escaped(default_uri,true)?; self.put("\"")?;
        }
        self.namespaces.push(default_uri.to_owned());
        if let Some((namespace,identity))=pending {
            let lexical=if namespace.is_empty() {identity.to_owned()} else {
                self.attributes+=1; if self.attributes > self.limits.max_attributes { return Err(Error("XML attribute limit")); }
                self.put(" xmlns:srltype=\"")?; self.escaped(namespace,true)?; self.put("\"")?;
                format!("srltype:{identity}")
            };
            self.attribute("type","http://www.w3.org/2001/XMLSchema-instance",&lexical,usize::MAX)?;
        }
        Ok(())
    }
    /// Emit a statically named attribute with an explicit deterministic prefix when qualified.
    fn attribute(&mut self,local:&str,uri:&str,value:&str,index:usize) -> Result<(),Error> {
        self.attributes += 1; if self.attributes > self.limits.max_attributes { return Err(Error("XML attribute limit")); }
        self.charge(local)?; self.charge(uri)?;
        self.put(" ")?;
        if uri == "http://www.w3.org/XML/1998/namespace" {
            self.put("xml:")?;
        } else if !uri.is_empty() {
            self.attributes += 1;
            if self.attributes > self.limits.max_attributes { return Err(Error("XML attribute limit")); }
            let prefix=format!("srl{}",index); self.charge(&prefix)?; self.put("xmlns:")?; self.put(&prefix)?;
            self.put("=\"")?; self.escaped(uri,true)?; self.put("\" ")?; self.put(&prefix)?; self.put(":")?;
        }
        self.put(local)?; self.put("=\"")?; self.escaped(value,true)?; self.put("\"")
    }
    /// Close one generated element after all declared content has been written.
    fn close(&mut self,local:&str) -> Result<(),Error> {
        self.put("</")?;
        if self.prefixed_type_depths.last() == Some(&self.depth) { self.put("srlvalue:")?; self.prefixed_type_depths.pop(); }
        self.put(local)?; self.put(">")?; self.depth -= 1; self.namespaces.pop(); Ok(())
    }
    /// Emit a complete statically scalar child.
    fn child(&mut self,local:&str,uri:&str,value:&str) -> Result<(),Error> {
        self.open(local,uri)?; self.put(">")?; self.escaped(value,false)?; self.close(local)
    }
}
)xml"
R"xml(/// Checked XML scalar conversion; Rust numeric parsing enforces exact native widths.
fn srl_xml_number<T:std::str::FromStr>(text:&str) -> Result<T,Error> {
    let mut text=text.trim_matches([' ','\t','\r','\n']);
    if text.is_empty() { return Err(Error("Empty XML scalar")); }
    if text.len()>1 && text.starts_with('-') && text[1..].bytes().all(|byte| byte == b'0') { text="0"; }
    text.parse().map_err(|_| Error("Invalid or overflowing XML scalar"))
}
/// Accept precisely the XML Boolean lexical alternatives.
fn srl_xml_bool(text:&str) -> Result<bool,Error> {
    match text.trim_matches([' ','\t','\r','\n']) { "true"|"1" => Ok(true), "false"|"0" => Ok(false), _ => Err(Error("Invalid XML Boolean")) }
}
/// Keep XML nonfinite spelling separate from Rust's accepted floating spellings.
fn srl_xml_float(text:&str) -> Result<f64,Error> {
    let text=text.trim_matches([' ','\t','\r','\n']);
    match text { "INF" => return Ok(f64::INFINITY), "-INF" => return Ok(f64::NEG_INFINITY), "NaN" => return Ok(f64::NAN), _ => {} }
    if !text.bytes().all(|b| b.is_ascii_digit() || b"+-.eE".contains(&b)) { return Err(Error("Invalid XML float")); }
    let value=text.parse::<f64>().map_err(|_| Error("Invalid XML float"))?;
    if !value.is_finite() { return Err(Error("XML float range")); } Ok(value)
}
/// Return scalar text while rejecting child markup and undeclared scalar attributes.
fn srl_xml_text<'a,'input:'a>(node:roxmltree::Node<'a,'input>) -> Result<String,Error> {
    if node.attributes().len() != 0 || node.children().any(|child| child.is_element()) { return Err(Error("XML scalar contains markup")); }
    let mut result=String::new(); for child in node.children().filter(|child| child.is_text()) { result.push_str(child.text().unwrap_or("")); } Ok(result)
}
/// Read simple content while retaining separately checked record attributes.
fn srl_xml_simple_text(node:roxmltree::Node<'_, '_>) -> Result<String,Error> {
    if node.children().any(|child| child.is_element()) { return Err(Error("XML simple content contains children")); }
    Ok(node.children().filter(|child| child.is_text()).filter_map(|child| child.text()).collect())
}
/// Ordered field state borrows the bounded parsed tree and publishes no model before finish.
struct SrlXmlFields<'a,'input:'a> { attributes:Vec<roxmltree::Attribute<'a,'input>>, used:Vec<bool>, children:Vec<roxmltree::Node<'a,'input>>, next:usize }
impl<'a,'input:'a> SrlXmlFields<'a,'input> {
    /// Reject record text and collect only direct child elements.
    fn new(node:roxmltree::Node<'a,'input>,has_text:bool) -> Result<Self,Error> {
        if !has_text && node.children().any(|child| child.is_text() && !child.text().unwrap_or("").trim_matches([' ','\t','\r','\n']).is_empty()) { return Err(Error("Unexpected XML record text")); }
        let attributes:Vec<_>=node.attributes().collect(); let used=vec![false;attributes.len()];
        Ok(Self {attributes,used,children:node.children().filter(|child| child.is_element()).collect(),next:0})
    }
    /// Locate one exact expanded attribute and retain required/optional presence.
    fn attribute(&mut self,uri:&str,local:&str,optional:bool) -> Result<Option<&'a str>,Error> {
        for (index,attribute) in self.attributes.iter().enumerate() {
            if attribute.namespace().unwrap_or("") == uri && attribute.name() == local { self.used[index]=true; return Ok(Some(attribute.value())); }
        }
        if optional { Ok(None) } else { Err(Error("Required XML attribute missing")) }
    }
    /// Consume one ordered contiguous child group and enforce its generated occurrence bounds.
    fn children(&mut self,uri:&str,local:&str,minimum:usize,maximum:Option<usize>) -> Result<Vec<roxmltree::Node<'a,'input>>,Error> {
        let begin=self.next; while self.next < self.children.len() && srl_xml_name(self.children[self.next],uri,local) { self.next += 1; }
        let count=self.next-begin; if count < minimum || maximum.is_some_and(|bound| count > bound) { return Err(Error("XML child order/cardinality")); }
        Ok(self.children[begin..self.next].to_vec())
    }
    /// Require every direct attribute/child to have one declared consumer.
    fn finish(&self) -> Result<(),Error> { if self.used.iter().any(|used| !used) || self.next != self.children.len() { Err(Error("Unexpected XML member")) } else { Ok(()) } }
    /// Require one wrapper and consume all of its item content.
    fn wrapped(&mut self,wrapper_uri:&str,wrapper_local:&str,uri:&str,local:&str,minimum:usize,maximum:Option<usize>) -> Result<Vec<roxmltree::Node<'a,'input>>,Error> {
        let wrapper=self.children(wrapper_uri,wrapper_local,1,Some(1))?[0];
        let mut fields=Self::new(wrapper,false)?;
        let result=fields.children(uri,local,minimum,maximum)?; fields.finish()?; Ok(result)
    }
}
)xml"
#include "xml_rust_map_runtime.inc"
#include "xml_rust_tokens_runtime.inc"
#include "xml_rust_variant_runtime.inc"
#include "xml_rust_mixed_runtime.inc"
#include "xml_rust_model_runtime.inc"
#include "xml_rust_html_runtime.inc"
#include "xml_rust_scalar_runtime.inc"
#include "xml_rust_identity_runtime.inc"
#include "xml_rust_extension_runtime.inc"
;
}

// Generate native Rust static XML methods using the same frozen binding IR as C++.
inline std::string rust_methods(const class_node& owner, const record& contract,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier,
    const xml_options& options, bool output_only = false,
    const std::function<std::string(const member&)>& projected_reference = {},
    const std::function<std::string()>& custom_output = {}) {
  std::string result;
  std::size_t index{};
  const auto line = [&result](const std::string& text) { result += text + "\n"; };
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const auto identity_graph = xml_codegen::native_identity_graph(owner, contract, xml_binding::schema{options});
  const bool identity_enabled = std::any_of(identity_graph.begin(), identity_graph.end(),
      [](const auto& node) { return node.scope.has_value(); });
  const xml_binding::schema extension_bindings{options};
  const bool extension_graph = xml_codegen::native_extension_graph(owner, extension_bindings);
  if (extension_graph) { xml_codegen::validate_native_extension_graph(owner, extension_bindings, "rust"); }
  if ((extension_graph || contract.extensions) && (output_only || xml_codegen::native_xml_export_graph(owner))) {
    throw std::invalid_argument{"Native XML extension companions require an owning graph; export/reference extension adapters are not implemented for rust"};
  }
  // Getter adapters lend existing target-shaped fields without constructing or copying a target record.
  const auto output_reference = [&](const member& field) {
    return projected_reference ? projected_reference(field) : "(&self." + field_name(field) + ")";
  };
  // Resolve mixed fragments through their original native item type, independently of ordinary fields.
  const auto mixed_method = [&](bool input) {
    const auto& mixed = *contract.mixed;
    const auto* item = &mixed.source->type_name_list.front();
    while (item->is_sequence()) { item = &item->arguments.front(); }
    return rust_mixed_method(mixed, (input ? "value." + field_name(*mixed.source) : output_reference(*mixed.source)),
        type_identifier(*item), input, type_identifier);
  };
  const auto revision_path = [&](bool input) {
    if (!contract.model.revision) { return std::string{}; }
    if (!input && projected_reference) {
      const auto selected = output_reference(*contract.model.revision);
      return contract.model.revision->type_name_list.front().name.starts_with("version") ? selected : "(*" + selected + ")";
    }
    return std::string{input ? "value." : "self."} + field_name(*contract.model.revision);
  };
  // Lifecycle guards close at field-emitter scope exit, including container early returns.
  const auto active_scope = [&](const member& field, bool input) -> xml_codegen::model_emission_scope {
    const auto condition = xml_codegen::native_xml_active(contract, field, version_language::rust, revision_path(input));
    if (!condition.empty()) { line("if " + behavior::ungroup_expression(condition) + " {"); }
    return xml_codegen::model_emission_scope{condition.empty() ? std::function<void()>{} : std::function<void()>{[&] { line("}"); }}};
  };
  // Omit inferred lifetime arguments from Rust associated-function expression paths.
  const auto associated_type = [&](const type_name& type) {
    auto name = type_identifier(type);
    if (name.ends_with("<'a>")) { name.resize(name.size()-std::string_view{"<'a>"}.size()); }
    return name;
  };
  // Select one statically known native scalar lexical conversion.
  const auto read_scalar = [&](const type_name& type, const std::string& text) {
    if (type.type == object_type::enum_type) {
      std::string expression = "match " + text + ".as_str() { ";
      const auto& values = static_cast<const enum_node&>(*type.resolved_node).enum_name_list;
      for (const auto& value : values) { expression += quote(value) + " => " + type_identifier(type) + "::" + native::pascal(value) + ","; }
      return expression + "_ => return Err(Error(\"Unknown XML enum\")) }";
    }
    if (type.is_digest()) { return "srl_xml_digest_read(&" + text + "," + std::to_string(type.digest_extent) + ",input.digest_limits)?"; }
    if (type.name == "string" || type.name.starts_with("version")) { return text; }
    if (type.name == "bool") { return "srl_xml_bool(&" + text + ")?"; }
    if (type.name == "float" || type.name == "float32") {
      return "srl_xml_float32(&" + text + ")?";
    }
    if (type.name == "double" || type.name == "float64") { return "srl_xml_float(&" + text + ")?"; }
    return "srl_xml_number::<" + type_identifier(type) + ">(&" + text + ")?";
  };
  // Borrow string/enum spellings and only format primitive numeric temporaries.
  const auto scalar = [&](const type_name& type, const std::string& expression) {
    if (type.is_digest()) { return "&srl_xml_digest_text(" + behavior::ungroup_expression(expression) + "," + std::to_string(type.digest_extent) + ",output.digest_limits)?"; }
    if (type.name == "string" || type.name.starts_with("version")) { return "(" + expression + ").as_str()"; }
    if (type.type == object_type::enum_type) { return type_identifier(type) + "::NAMES[*" + expression + " as usize]"; }
    if (type.name == "float" || type.name == "float32" || type.name == "double" || type.name == "float64") {
      return "&{ let value=*" + expression + "; if value.is_nan() { \"NaN\".to_owned() } "
          "else if value == " + type_identifier(type) + "::INFINITY { \"INF\".to_owned() } else if value == " +
          type_identifier(type) + "::NEG_INFINITY { \"-INF\".to_owned() } else { value.to_string() } }";
    }
    return "&(" + expression + ").to_string()";
  };
  // Validate the complete lexical value before allocating native list storage or converting one scalar.
  const auto read_field_scalar = [&](const field& selected, const type_name& type, const std::string& raw) {
    if (!xml_portable::needs_scalar_rule(selected)) { return read_scalar(type, raw); }
    std::string expression = "{ const RULE:SrlXmlRule=" + xml_codegen::rust_scalar_rule(selected, quote) +
        "; let raw=" + raw + "; ";
    const auto fallback = selected.fixed_value ? selected.fixed_value : selected.default_value;
    if (fallback && !selected.attribute) {
      expression += "let raw=if raw.is_empty(){" + quote(*fallback) + ".to_owned()}else{raw}; ";
    }
    expression += "let text=RULE.check(&raw,input.limits.max_nodes)?; ";
    if (selected.list) {
      expression += "let tokens=srl_xml_scalar_tokens(&text,input.limits.max_nodes)?; ";
      if (selected.source->fixed_extent != 0U) {
        expression += "if tokens.len()!=" + std::to_string(selected.source->fixed_extent) +
            " {return Err(Error(\"XML native list extent\"));} ";
      }
      expression += "let mut values=Vec::new(); values.try_reserve_exact(tokens.len()).map_err(|_|Error(\"XML scalar allocation\"))?; "
          "for item in tokens {values.push(" + read_scalar(type, "item.to_owned()") + ");} values }";
    } else { expression += read_scalar(type, "text") + " }"; }
    return expression;
  };
  // Join typed lists without mutating native values, then check their complete lexical contract once.
  const auto write_field_scalar = [&](const field& selected, const type_name& type, const std::string& value) {
    if (!xml_portable::needs_scalar_rule(selected)) { return scalar(type, value); }
    std::string expression = "&{ const RULE:SrlXmlRule=" + xml_codegen::rust_scalar_rule(selected, quote) + "; ";
    if (selected.list) {
      expression += "let items=" + behavior::ungroup_expression(value) + "; if items.len()>output.value_limits.max_nodes {return Err(Error(\"XML list item limit\"));} ";
      if (selected.source->fixed_extent != 0U) {
        expression += "if items.len()!=" + std::to_string(selected.source->fixed_extent) +
            " {return Err(Error(\"XML native list extent\"));} ";
      }
      expression += "let mut joined=String::new(); for item in items.iter(){srl_xml_scalar_append(&mut joined," +
          scalar(type, "item") + ",output.value_limits.max_text_bytes)?;} ";
    } else { expression += "let joined=" + scalar(type, value) + "; if joined.len()>output.value_limits.max_text_bytes {return Err(Error(\"XML scalar text limit\"));} "; }
    const auto fallback = selected.fixed_value ? selected.fixed_value : selected.default_value;
    return expression + "RULE.output(" + (selected.list ? "&joined" : "joined") + ',' +
        (fallback && !selected.attribute ? "Some(" + quote(*fallback) + ")" : "None") + ",output.value_limits.max_nodes)? }";
  };
  // Keep explicit XML states aligned with nested native Option layers and wrapped collections.
  const auto controlled_presence = [](const field& value) {
    return !value.attribute && !value.list && (value.presence.explicit_policy ||
        (value.presence.collection && value.presence.optional_depth != 0U));
  };
  // Decode one controlled field entirely before assigning its owner's private candidate slot.
  const auto read_presence = [&](const field& value, const std::string& destination) {
    const auto& presence = value.presence;
    const auto& element = value.wrapper ? *value.wrapper : value.qualified_name;
    const auto uri = quote(element.namespace_uri), local = quote(element.local);
    const auto* leaf = &value.source->type_name_list.front();
    while (leaf->is_optional() || leaf->is_sequence()) { leaf = &leaf->arguments.front(); }
    const auto item_uri = quote(value.qualified_name.namespace_uri), item_local = quote(value.qualified_name.local);
    const auto conversion = leaf->type == object_type::class_type ?
        associated_type(*leaf) + "::srl_xml_read_tokens(input," + item_uri + "," + item_local + ")?" :
        read_field_scalar(value, *leaf, "input.scalar(" + item_uri + "," + item_local + ")?");
    line(destination + "=match input.presence(" + uri + "," + local + "," +
        (presence.allows_absent ? "true" : "false") + "," + (presence.allows_nil ? "true" : "false") + ")? {");
    if (presence.allows_absent) { line("'a'=>None,"); }
    if (presence.allows_nil) {
      line("'n'=>" + std::string{presence.optional_depth == 2U ? "Some(None)" : "None"} + ",");
    }
    line("'v'=>{");
    std::string payload = conversion;
    if (presence.collection) {
      line("input.open(" + uri + "," + local + ")?.finish()?; let mut items=Vec::new();");
      line("while input.has(" + item_uri + "," + item_local + ")? {");
      const auto maximum = value.source->fixed_extent != 0U ?
          std::optional<std::size_t>{value.source->fixed_extent} : value.max_occurs;
      if (maximum) { line("if items.len() >= " + std::to_string(*maximum) + " { return Err(Error(\"XML collection occurrence bound\")); }"); }
      line("items.try_reserve(1).map_err(|_| Error(\"XML collection allocation\"))?; items.push(" + conversion + "); }");
      const auto minimum = value.source->fixed_extent != 0U ? value.source->fixed_extent : value.min_occurs;
      if (minimum > 0U) { line("if items.len() < " + std::to_string(minimum) + " { return Err(Error(\"XML collection occurrence bound\")); }"); }
      line("input.close(" + uri + "," + local + ")?;");
      payload = "items";
    }
    for (std::size_t depth{}; depth < presence.optional_depth; ++depth) { payload = "Some(" + payload + ")"; }
    line(payload + "}, _=>return Err(Error(\"Invalid XML presence state\")) };");
  };
  // Write an already-unwrapped scalar, record or collection under its declared XML names.
  const auto write_present = [&](const field& value, std::string expression) {
    const auto* leaf = &value.source->type_name_list.front();
    while (leaf->is_optional() || leaf->is_sequence()) { leaf = &leaf->arguments.front(); }
    if (value.wrapper) { line("output.open(" + quote(value.wrapper->local) + "," + quote(value.wrapper->namespace_uri) + ")?; output.put(\">\")?;"); }
    if (value.presence.collection) {
      const auto minimum = value.source->fixed_extent != 0U ? value.source->fixed_extent : value.min_occurs;
      const auto maximum = value.source->fixed_extent != 0U ? std::optional<std::size_t>{value.source->fixed_extent} : value.max_occurs;
      if (minimum > 0U) { line("if " + expression + ".len() < " + std::to_string(minimum) + " { return Err(Error(\"XML collection occurrence bound\")); }"); }
      if (maximum) { line("if " + expression + ".len() > " + std::to_string(*maximum) + " { return Err(Error(\"XML collection occurrence bound\")); }"); }
      line("for item in " + expression + ".iter() {"); expression = "item";
    }
    if (leaf->type == object_type::class_type) {
      line(expression + ".srl_xml_write(output," + quote(value.qualified_name.local) + "," + quote(value.qualified_name.namespace_uri) + ")?;");
    } else {
      line("output.child(" + quote(value.qualified_name.local) + "," + quote(value.qualified_name.namespace_uri) + "," + write_field_scalar(value, *leaf, expression) + ")?;");
    }
    if (value.presence.collection) { line("}"); }
    if (value.wrapper) { line("output.close(" + quote(value.wrapper->local) + ")?;"); }
  };
  if (extension_graph) {
    line(xml_codegen::native_extension_metadata(contract, owner.get_full_name(), {},
        [&](const class_node& child) {
          if (!xml_codegen::native_extension_graph(child, extension_bindings)) { return std::string{}; }
          type_name selected{std::string{child.name}, child.parent_namespace};
          selected.type = object_type::class_type; selected.resolved_node = &child;
          return associated_type(selected) + "::srl_xml_extension_model";
        }, quote, true));
  }
  line("/// Check original or final XML with bounded owner-local token indexes before publication.");
  if (identity_enabled) {
    line("fn srl_xml_identity_check(text:&str,limits:XmlLimits,identity_limits:XmlIdentityLimits)->Result<(),Error> {");
    line("const GRAPH:&[SrlXmlIdentityNode]=" + xml_codegen::rust_identity_graph(identity_graph, quote) + ";");
    line("srl_xml_identity_validate(text,limits,identity_limits,GRAPH) }");
  } else {
    line("fn srl_xml_identity_check(_text:&str,_limits:XmlLimits,_identity_limits:XmlIdentityLimits)->Result<(),Error> {Ok(())}");
  }
  line("/// Encode the fixed XML contract with no runtime schema interpretation.");
  line("pub fn encode_xml(&self) -> Result<String,Error> { self.encode_xml_with_validation(XmlLimits::default(),None) }");
  line("/// Bound one complete XML document and optionally apply a trusted prepared validator.");
  line("pub fn encode_xml_with_validation(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>) -> Result<String,Error> {");
  line("self.encode_xml_with_map_limits(limits,validator,XmlMapLimits::default()) }");
  line("/// Apply the same independent map policy to every nested wrapper without changing XML limits.");
  line("pub fn encode_xml_with_map_limits(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>,map_limits:XmlMapLimits) -> Result<String,Error> {");
  line("self.encode_xml_with_policies(limits,validator,map_limits,XmlMixedLimits::default()) }");
  line("/// Select an independent mixed-content policy with the default map policy.");
  line("pub fn encode_xml_with_mixed_limits(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>,mixed_limits:XmlMixedLimits) -> Result<String,Error> {");
  line("self.encode_xml_with_policies(limits,validator,XmlMapLimits::default(),mixed_limits) }");
  line("/// Carry independent map and mixed-content policies through the same complete typed operation.");
  line("pub fn encode_xml_with_policies(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>,map_limits:XmlMapLimits,mixed_limits:XmlMixedLimits) -> Result<String,Error> {");
  line("self.encode_xml_with_all_policies(limits,validator,map_limits,mixed_limits,XmlDigestLimits::default()) }");
  line("/// Select an independent native digest byte policy while preserving previous call forms.");
  line("pub fn encode_xml_with_digest_limits(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>,digest_limits:XmlDigestLimits) -> Result<String,Error> {");
  line("self.encode_xml_with_all_policies(limits,validator,XmlMapLimits::default(),XmlMixedLimits::default(),digest_limits) }");
  line("/// Carry independent map, mixed and digest admission through every nested typed operation.");
  line("pub fn encode_xml_with_all_policies(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>,map_limits:XmlMapLimits,mixed_limits:XmlMixedLimits,digest_limits:XmlDigestLimits) -> Result<String,Error> {");
  if (extension_graph) { line("let _=(limits,validator,map_limits,mixed_limits,digest_limits);Err(Error(\"XML extension document companion required\")) }"); }
  else { line("self.srl_xml_encode_known(limits,validator,map_limits,mixed_limits,digest_limits,XmlIdentityLimits::default(),true,None) }"); }
  line("/// Select independent identity indexes without changing existing complete XML policy layouts.");
  line("pub fn encode_xml_with_identity_limits(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>,identity_limits:XmlIdentityLimits)->Result<String,Error> {");
  if (extension_graph) { line("let _=(limits,validator,identity_limits);Err(Error(\"XML extension document companion required\")) }"); }
  else { line("self.srl_xml_encode_known(limits,validator,XmlMapLimits::default(),XmlMixedLimits::default(),XmlDigestLimits::default(),identity_limits,true,None) }"); }
  line("/// Keep typed known output private so extension companions can validate their final replay separately.");
  line("fn srl_xml_encode_known(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>,map_limits:XmlMapLimits,mixed_limits:XmlMixedLimits,digest_limits:XmlDigestLimits,identity_limits:XmlIdentityLimits,check_identity:bool,transport_limits:Option<XmlLimits>)->Result<String,Error> {");
  line("let mut output=SrlXmlWriter::new(limits); if let Some(transport)=transport_limits {output.limits=transport;} output.map_limits=map_limits; output.mixed_limits=mixed_limits; output.digest_limits=digest_limits; self.srl_xml_write(&mut output," + quote(contract.root.local) + "," + quote(contract.root.namespace_uri) + ")?;");
  line("if check_identity {Self::srl_xml_identity_check(&output.text,limits,identity_limits)?;}");
  if (!options.runtime_xsd) { line("if validator.is_some() { return Err(Error(\"Runtime XSD not enabled during generation\")); }"); }
  else if (options.validation_scope != "input") { line("if let Some(schema)=validator { schema.validate(&output.text)?; }"); }
  else { line("let _=validator;"); }
  line("Ok(output.text) }");
  if (!output_only && !xml_codegen::native_xml_export_graph(owner)) {
  line("/// Decode a fresh owning model after complete XML and mapping checks.");
  line("pub fn decode_xml(text:&str) -> Result<Self,Error> { Self::decode_xml_with_validation(text,XmlLimits::default(),None) }");
  line("/// Validate an externally supplied UTF8 byte message before selecting the direct typed reader.");
  line("pub fn decode_xml_bytes(bytes:&[u8]) -> Result<Self,Error> { Self::decode_xml(std::str::from_utf8(bytes).map_err(|_| Error(\"Invalid XML UTF8\"))?) }");
  line("/// Apply caller limits and optional trusted XSD while retaining generated checks.");
  line("pub fn decode_xml_with_validation(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>) -> Result<Self,Error> {");
  line("Self::decode_xml_with_map_limits(text,limits,validator,XmlMapLimits::default()) }");
  line("/// Retain lexical key accounting and per-wrapper admission through all nested typed readers.");
  line("pub fn decode_xml_with_map_limits(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>,map_limits:XmlMapLimits) -> Result<Self,Error> {");
  line("Self::decode_xml_with_policies(text,limits,validator,map_limits,XmlMixedLimits::default()) }");
  line("/// Select an independent mixed-content policy without changing existing complete XML limits.");
  line("pub fn decode_xml_with_mixed_limits(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>,mixed_limits:XmlMixedLimits) -> Result<Self,Error> {");
  line("Self::decode_xml_with_policies(text,limits,validator,XmlMapLimits::default(),mixed_limits) }");
  line("/// Apply independently bounded typed containers before publishing the owning candidate.");
  line("pub fn decode_xml_with_policies(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>,map_limits:XmlMapLimits,mixed_limits:XmlMixedLimits) -> Result<Self,Error> {");
  line("Self::decode_xml_with_all_policies(text,limits,validator,map_limits,mixed_limits,XmlDigestLimits::default()) }");
  line("/// Select an independent native digest byte policy while preserving previous call forms.");
  line("pub fn decode_xml_with_digest_limits(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>,digest_limits:XmlDigestLimits) -> Result<Self,Error> {");
  line("Self::decode_xml_with_all_policies(text,limits,validator,XmlMapLimits::default(),XmlMixedLimits::default(),digest_limits) }");
  line("/// Carry independent map, mixed and digest admission through every nested typed operation.");
  line("pub fn decode_xml_with_all_policies(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>,map_limits:XmlMapLimits,mixed_limits:XmlMixedLimits,digest_limits:XmlDigestLimits) -> Result<Self,Error> {");
  if (extension_graph) { line("let _=(text,limits,validator,map_limits,mixed_limits,digest_limits);Err(Error(\"XML extension document companion required\")) }"); }
  else { line("Self::srl_xml_decode_known(text,limits,validator,map_limits,mixed_limits,digest_limits,XmlIdentityLimits::default(),true) }"); }
  line("/// Resolve bounded owner-local keyrefs before constructing or publishing a native candidate.");
  line("pub fn decode_xml_with_identity_limits(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>,identity_limits:XmlIdentityLimits)->Result<Self,Error> {");
  if (extension_graph) { line("let _=(text,limits,validator,identity_limits);Err(Error(\"XML extension document companion required\")) }"); }
  else { line("Self::srl_xml_decode_known(text,limits,validator,XmlMapLimits::default(),XmlMixedLimits::default(),XmlDigestLimits::default(),identity_limits,true) }"); }
  line("/// Decode only a bounded known representation; extension companions separately check the original XML.");
  line("fn srl_xml_decode_known(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>,map_limits:XmlMapLimits,mixed_limits:XmlMixedLimits,digest_limits:XmlDigestLimits,identity_limits:XmlIdentityLimits,check_identity:bool)->Result<Self,Error> {");
  line("if check_identity {Self::srl_xml_identity_check(text,limits,identity_limits)?;}");
  line("let mut input=SrlXmlInput::new(text,limits)?; input.map_limits=map_limits; input.mixed_limits=mixed_limits; input.digest_limits=digest_limits;");
  if (!options.runtime_xsd) { line("if validator.is_some() { return Err(Error(\"Runtime XSD not enabled during generation\")); }"); }
  else if (options.validation_scope != "output") { line("if let Some(schema)=validator { schema.validate(text)?; }"); }
  else { line("let _=validator;"); }
  line("let value=Self::srl_xml_read_tokens(&mut input," + quote(contract.root.namespace_uri) + "," + quote(contract.root.local) + ")?; input.finish_document()?; Ok(value) }");
  line("/// Read generated field assignments directly, preserving transactionality.");
  const bool token_presence_only = contract.model.revision || contract.model.magic || contract.model.digest || contract.mixed || std::any_of(contract.fields.begin(), contract.fields.end(),
      [&](const auto& field) { return controlled_presence(field) || field.map_binding || field.variant_binding || xml_portable::needs_scalar_rule(field); });
  if (token_presence_only) {
    line("fn srl_xml_read(_node:roxmltree::Node<'_, '_>) -> Result<Self,Error> { Err(Error(\"Explicit XML presence requires the direct token codec\")) }");
  } else {
  const bool consumes_fields = std::any_of(contract.fields.begin(), contract.fields.end(),
      [](const auto& field) { return !field.text; });
  line("fn srl_xml_read(node:roxmltree::Node<'_, '_>) -> Result<Self,Error> { let " +
      std::string{consumes_fields ? "mut " : ""} + "fields=SrlXmlFields::new(node," + std::string{std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; }) ? "true" : "false"} + ")?; let " +
      (contract.fields.empty() ? "" : "mut ") + "value=Self::default();");
  for (const auto& field : contract.fields) {
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto& leaf = optional || original.is_sequence() ? original.arguments.front() : original;
    const auto local = quote(field.qualified_name.local);
    const auto uri = quote(field.qualified_name.namespace_uri);
    const auto temporary = "field_" + std::to_string(index++);
    const auto destination = "value." + field_name(source);
    if (field.text) {
      line(destination + " = " + read_scalar(leaf, "srl_xml_simple_text(node)?") + ";");
    } else if (field.attribute) {
      line("let " + temporary + "=fields.attribute(" + uri + "," + local + "," + (optional ? "true" : "false") + ")?;");
      const auto conversion = read_scalar(leaf, "text.to_owned()");
      line(destination + "=" + (optional ? "match " + temporary + " { Some(text) => Some(" + conversion + "),None=>None }"
          : "{ let text=" + temporary + ".ok_or(Error(\"Required XML attribute missing\"))?; " + conversion + " }") + ";");
    } else {
      const auto minimum = source.fixed_extent != 0 ? source.fixed_extent : field.min_occurs;
      const auto maximum = source.fixed_extent != 0 ? std::optional<std::size_t>{source.fixed_extent} : field.max_occurs;
      line("let " + temporary + "=fields." + std::string{field.wrapper ? "wrapped(" + quote(field.wrapper->namespace_uri) + "," + quote(field.wrapper->local) + "," : "children("} + uri + "," + local + "," + std::to_string(minimum) + "," +
          (maximum ? "Some(" + std::to_string(*maximum) + ")" : "None") + ")?;");
      const auto conversion = leaf.type == object_type::class_type ? associated_type(leaf) + "::srl_xml_read(child)?"
          : read_scalar(leaf, "srl_xml_text(child)?");
      if (field.repeated) {
        line(destination + "= { let mut result=Vec::new(); for child in " + temporary + " { result.push(" + conversion + "); } result }; ");
      } else if (optional) {
        line(destination + "=match " + temporary + ".first() { Some(child)=> { let child=*child; Some(" + conversion + ") },None=>None }; ");
      } else {
        line(destination + "={ let child=" + temporary + "[0]; " + conversion + " }; ");
      }
    }
  }
  line("fields.finish()?; Ok(value) }");
  }
  line("/// Read one statically mapped owning candidate without a retained XML node.");
  line("fn srl_xml_read_tokens(input:&mut SrlXmlInput<'_>,uri:&str,local:&str) -> Result<Self,Error> {");
  const bool has_attributes = contract.model.revision || contract.model.magic || std::any_of(contract.fields.begin(), contract.fields.end(),
      [](const auto& field) { return field.attribute; });
  line("let " + std::string{has_attributes ? "mut " : ""} + "fields=input.open(uri,local)?; let " +
      (contract.fields.empty() && !contract.mixed && !contract.model.revision ? "" : "mut ") + "value=Self::default();");
  if (contract.model.revision) {
    const auto& revision = *contract.model.revision;
    const auto& type = revision.type_name_list.front();
    line("let revision=fields.attribute(" + quote(model_metadata_namespace) + ",\"revision\",false)?.ok_or(Error(\"Missing XML revision\"))?;");
    if (type.name.starts_with("version")) {
      line("srl_version_parts(&revision," + std::string{type.name.back()} + ")?;");
    } else {
      line("srl_xml_revision_number(&revision," + std::string{type.name == "float" || type.name == "double" ? "true" : "false"} + ")?;");
    }
    const auto converted = type.name == "float" ? "revision.parse::<f32>().map_err(|_| Error(\"XML revision float32\"))?" : read_scalar(type, "revision");
    line(revision_path(true) + "=" + converted + ";");
    line("if " + xml_codegen::native_xml_revision_invalid(revision, version_language::rust, revision_path(true)) + " { return Err(Error(\"Unsupported XML revision\")); }");
  }
  if (contract.model.magic) {
    line("if fields.attribute(" + quote(model_metadata_namespace) + ",\"magic\",false)?.as_deref() != Some(" + quote(*contract.model.magic) + ") { return Err(Error(\"XML magic mismatch\")); }");
  }
  index = 0U;
  for (const auto& field : contract.fields) {
    if (!contract.all_groups.empty() && !field.attribute) { continue; }
    const auto lifecycle = active_scope(*field.source, true);
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto* leaf = &original;
    while (leaf->is_optional() || leaf->is_sequence()) { leaf = &leaf->arguments.front(); }
    const auto local = quote(field.qualified_name.local);
    const auto uri = quote(field.qualified_name.namespace_uri);
    const auto temporary = "field_" + std::to_string(index++);
    const auto destination = "value." + field_name(source);
    if (field.map_binding) {
      line(rust_map_method(field, destination, true, type_identifier, read_scalar, scalar)); continue;
    }
    if (field.variant_binding) {
      line(rust_variant_method(field, destination, true, type_identifier, read_scalar, scalar)); continue;
    }
    if (controlled_presence(field)) { read_presence(field, destination); continue; }
    const auto conversion = leaf->type == object_type::class_type ?
        associated_type(*leaf) + "::srl_xml_read_tokens(input," + uri + "," + local + ")?" :
        read_field_scalar(field, *leaf, "input.scalar(" + uri + "," + local + ")?");
    if (field.attribute) {
      const auto fallback = field.fixed_value ? field.fixed_value : field.default_value;
      const bool defaultable = fallback && field.min_occurs == 0U;
      line("let " + temporary + "=fields.attribute(" + uri + "," + local + "," + (optional || defaultable ? "true" : "false") + ")?" +
          (defaultable ? ".or_else(||Some(" + quote(*fallback) + ".to_owned()))" : "") + ";");
      const auto attribute_value = read_field_scalar(field, *leaf, "text");
      line(destination + "=" + (optional ? "match " + temporary + " {Some(text)=>Some(" + attribute_value + "),None=>None}" :
          "{let text=" + temporary + ".ok_or(Error(\"Required XML attribute missing\"))?; " + attribute_value + "}") + ";");
    } else if (field.text) {
      line(destination + "=" + read_field_scalar(field, *leaf, "input.text()?") + ";");
    } else if (field.repeated && !field.list) {
      if (field.wrapper) {
        line("input.open(" + quote(field.wrapper->namespace_uri) + "," + quote(field.wrapper->local) + ")?.finish()?;");
      }
      const auto minimum = source.fixed_extent != 0U ? source.fixed_extent : field.min_occurs;
      const auto maximum = source.fixed_extent != 0U ? std::optional<std::size_t>{source.fixed_extent} : field.max_occurs;
      line("let mut " + temporary + "=Vec::new();");
      line("while input.has(" + uri + "," + local + ")? {");
      if (maximum) {
        line("if " + temporary + ".len() >= " + std::to_string(*maximum) + " { return Err(Error(\"XML collection occurrence bound\")); }");
      }
      line(temporary + ".try_reserve(1).map_err(|_| Error(\"XML collection allocation\"))?;");
      line(temporary + ".push(" + conversion + "); }");
      if (minimum > 0U) {
        line("if " + temporary + ".len() < " + std::to_string(minimum) + " { return Err(Error(\"XML collection occurrence bound\")); }");
      }
      if (field.wrapper) {
        line("input.close(" + quote(field.wrapper->namespace_uri) + "," + quote(field.wrapper->local) + ")?;");
      }
      line(destination + "=" + temporary + ";");
    } else if (optional) {
      line(destination + "=if input.has(" + uri + "," + local + ")? {Some(" + conversion + ")} else {None};");
    } else { line(destination + "=" + conversion + ";"); }
  }
  if (!contract.all_groups.empty() && std::any_of(contract.fields.begin(), contract.fields.end(),
          [](const auto& field) { return !field.attribute; })) {
    line("let mut seen=[false;" + std::to_string(contract.fields.size()) + "];");
    for (const auto& field : contract.fields) {
      if (!field.attribute && field.source->type_name_list.front().is_optional()) {
        line("value." + field_name(*field.source) + "=None;");
      }
    }
    line("loop {");
    for (std::size_t field_index{}; field_index < contract.fields.size(); ++field_index) {
      const auto& field = contract.fields[field_index];
      if (field.attribute) { continue; }
      const auto& original = field.source->type_name_list.front();
      const bool optional = original.is_optional();
      const auto* value_type = &original;
      while (value_type->is_optional() || value_type->is_sequence()) { value_type = &value_type->arguments.front(); }
      const auto local = quote(field.qualified_name.local);
      const auto uri = quote(field.qualified_name.namespace_uri);
      const auto position = std::to_string(field_index);
      line("if input.has(" + uri + "," + local + ")? {");
      line("if seen[" + position + "] { return Err(Error(\"Duplicate XML all child\")); } seen[" + position + "]=true;");
      if (controlled_presence(field)) {
        read_presence(field, "value." + field_name(*field.source));
        line("continue; }"); continue;
      }
      const auto conversion = value_type->type == object_type::class_type ?
          associated_type(*value_type) + "::srl_xml_read_tokens(input," + uri + "," + local + ")?" :
          read_field_scalar(field, *value_type, "input.scalar(" + uri + "," + local + ")?");
      line("value." + field_name(*field.source) + "=" + (optional ? "Some(" + conversion + ")" : conversion) + "; continue; }");
    }
    line("break; }");
    const bool optional_group = contract.all_groups.front().minimum == 0U;
    if (optional_group) { line("if seen.iter().any(|value| *value) {"); }
    for (std::size_t field_index{}; field_index < contract.fields.size(); ++field_index) {
      const auto& field = contract.fields[field_index];
      if (!field.attribute && field.min_occurs != 0U) {
        line("if !seen[" + std::to_string(field_index) + "] { return Err(Error(\"Required XML all child missing\")); }");
      }
    }
    if (optional_group) { line("}"); }
  }
  line(xml_codegen::native_choice_checks(contract, [&](const member& field) { return "value." + field_name(field); }, "rust"));
  if (contract.mixed) { const auto lifecycle = active_scope(*contract.mixed->source, true); line(mixed_method(true)); }
  line("fields.finish()?; input.close(uri,local)?; Ok(value) }");
  } else {
    line("/// Export-only views and live references never construct an owning candidate from XML input.");
    line("pub fn decode_xml(_text:&str)->Result<Self,Error> {Err(Error(\"Export-only XML projection\"))}");
    line("/// Reject byte input before parsing or borrowing its source owner.");
    line("pub fn decode_xml_bytes(_bytes:&[u8])->Result<Self,Error> {Err(Error(\"Export-only XML projection\"))}");
    line("/// Reject legacy node input without materializing a selected payload.");
    line("fn srl_xml_read(_node:roxmltree::Node<'_, '_>)->Result<Self,Error> {Err(Error(\"Export-only XML projection\"))}");
    line("/// Reject nested input before accessing source storage.");
    line("fn srl_xml_read_tokens(_input:&mut SrlXmlInput<'_>,_uri:&str,_local:&str)->Result<Self,Error> {Err(Error(\"Export-only XML projection\"))}");
  }
  if (extension_graph) { line(xml_codegen::rust_extension_frames(options)); }
  if (custom_output) { line(custom_output()); return result; }
  line("/// Emit a nested owning value under its caller-selected static expanded element name.");
  line("fn srl_xml_write(&self,output:&mut SrlXmlWriter,local:&str,uri:&str) -> Result<(),Error> { output.open(local,uri)?;");
  line(xml_codegen::native_choice_checks(contract, [&](const member& field) { return output_reference(field); }, "rust"));
  if (contract.model.revision) {
    const auto& revision = *contract.model.revision;
    const auto& type = revision.type_name_list.front();
    line("if " + xml_codegen::native_xml_revision_invalid(revision, version_language::rust, revision_path(false)) + " { return Err(Error(\"Unsupported XML revision\")); }");
    if (type.name == "float" || type.name == "double") {
      line("if " + revision_path(false) + ".is_sign_negative() || !" + revision_path(false) + ".is_finite() { return Err(Error(\"Noncanonical XML revision\")); }");
    }
    line("output.attribute(\"revision\"," + quote(model_metadata_namespace) + "," + scalar(type, projected_reference ? output_reference(revision) : "(&" + revision_path(false) + ")") + ",usize::MAX-1)?;");
  }
  if (contract.model.magic) {
    line("output.attribute(\"magic\"," + quote(model_metadata_namespace) + "," + quote(*contract.model.magic) + ",usize::MAX-2)?;");
  }
  index = 0;
  for (const auto& field : contract.fields) {
    if (!field.attribute) { continue; }
    const auto lifecycle = active_scope(*field.source, false);
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto* leaf = &original;
    while (leaf->is_optional() || leaf->is_sequence()) { leaf = &leaf->arguments.front(); }
    auto expression = output_reference(source);
    if (optional) { line("if let Some(item)=" + expression + ".as_ref() {"); expression = "item"; }
    const auto* reference = leaf->type == object_type::class_type ? static_cast<const class_node*>(leaf->resolved_node) : nullptr;
    if (reference && reference->reference && reference->reference->value_projection) {
      line(expression + ".with_value(|selected| { output.attribute(" + quote(field.qualified_name.local) + "," + quote(field.qualified_name.namespace_uri) + "," + scalar(reference->reference->value_type, "selected") + "," + std::to_string(index++) + ")?; Ok::<(),Error>(()) })??;");
    } else {
      line("output.attribute(" + quote(field.qualified_name.local) + "," + quote(field.qualified_name.namespace_uri) + "," + write_field_scalar(field, *leaf, expression) + "," + std::to_string(index++) + ")?;");
    }
    if (optional) { line("}"); }
  }
  line("output.put(\">\")?;");
  for (const auto& field : contract.fields) {
    if (field.attribute) { continue; }
    const auto lifecycle = active_scope(*field.source, false);
    if (field.map_binding) {
      line(rust_map_method(field, projected_reference ? "(*" + output_reference(*field.source) + ")" : "self." + field_name(*field.source), false, type_identifier, read_scalar, scalar)); continue;
    }
    if (field.variant_binding) {
      line(rust_variant_method(field, "self." + field_name(*field.source), false, type_identifier, read_scalar, scalar)); continue;
    }
    if (controlled_presence(field)) {
      const auto& presence = field.presence;
      auto expression = output_reference(*field.source);
      line("{");
      if (presence.allows_absent) { line("if let Some(present)=" + expression + ".as_ref() {"); expression = "present"; }
      if (presence.allows_nil) {
        line("if let Some(item)=" + expression + ".as_ref() {");
        write_present(field, "item"); line("} else {");
        const auto& element = field.wrapper ? *field.wrapper : field.qualified_name;
        line("output.open(" + quote(element.local) + "," + quote(element.namespace_uri) + ")?;");
        line("output.attribute(\"nil\",\"http://www.w3.org/2001/XMLSchema-instance\",\"true\",0)?; output.put(\">\")?;");
        line("output.close(" + quote(element.local) + ")?; }");
      } else { write_present(field, expression); }
      if (presence.allows_absent) { line("}"); }
      line("}"); continue;
    }
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto* leaf = &original;
    while (leaf->is_optional() || leaf->is_sequence()) { leaf = &leaf->arguments.front(); }
    auto expression = output_reference(source);
    if (optional) { line("if let Some(item)=" + expression + ".as_ref() {"); expression = "item"; }
    if (field.wrapper) { line("output.open(" + quote(field.wrapper->local) + "," + quote(field.wrapper->namespace_uri) + ")?; output.put(\">\")?;"); }
    if (field.repeated && !field.list) {
      const auto minimum = source.fixed_extent != 0 ? source.fixed_extent : field.min_occurs;
      const auto maximum = source.fixed_extent != 0 ? std::optional<std::size_t>{source.fixed_extent} : field.max_occurs;
      std::string condition;
      if (minimum != 0) { condition = expression + ".len() < " + std::to_string(minimum); }
      if (maximum) { condition += (condition.empty() ? "" : " || ") + expression + ".len() > " + std::to_string(*maximum); }
      if (!condition.empty()) { line("if " + condition + " { return Err(Error(\"XML array cardinality\")); }"); }
      line("for item in " + expression + ".iter() { "); expression = "item";
    }
    if (field.text) {
      const auto* reference = leaf->type == object_type::class_type ? static_cast<const class_node*>(leaf->resolved_node) : nullptr;
      if (reference && reference->reference && reference->reference->value_projection) {
        line(expression + ".with_value(|selected| { output.escaped(" + scalar(reference->reference->value_type, "selected") + ",false)?; Ok::<(),Error>(()) })??;");
      } else { line("output.escaped(" + write_field_scalar(field, *leaf, expression) + ",false)?;"); }
    } else if (leaf->type == object_type::class_type) {
      line(expression + ".srl_xml_write(output," + quote(field.qualified_name.local) + "," + quote(field.qualified_name.namespace_uri) + ")?;");
    } else {
      line("output.child(" + quote(field.qualified_name.local) + "," + quote(field.qualified_name.namespace_uri) + "," + write_field_scalar(field, *leaf, expression) + ")?;");
    }
    if (field.repeated && !field.list) { line("}"); }
    if (field.wrapper) { line("output.close(" + quote(field.wrapper->local) + ")?;"); }
    if (optional) { line("}"); }
  }
  if (contract.mixed) { const auto lifecycle = active_scope(*contract.mixed->source, false); line(mixed_method(false)); }
  line("output.close(local) }");
  static_cast<void>(owner);
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
