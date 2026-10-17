// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_binding {
// Emit bounded native Rust XML helpers; roxmltree is required only when XML is selected.
inline std::string rust_runtime() {
  return R"xml(
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
struct SrlXmlWriter { text:String, limits:XmlLimits, nodes:usize, depth:usize, attributes:usize, text_bytes:usize, namespaces:Vec<String> }
impl SrlXmlWriter {
    /// Keep one cumulative XML output policy across nested generated record calls.
    fn new(limits:XmlLimits) -> Self { Self {text:String::new(),limits,nodes:0,depth:0,attributes:0,text_bytes:0,namespaces:Vec::new()} }
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
        self.put("<")?; self.put(local)?;
        if self.namespaces.last().map(String::as_str).unwrap_or("") != uri {
            self.attributes += 1;
            if self.attributes > self.limits.max_attributes { return Err(Error("XML attribute limit")); }
            self.put(" xmlns=\"")?; self.escaped(uri,true)?; self.put("\"")?;
        }
        self.namespaces.push(uri.to_owned()); Ok(())
    }
    /// Emit a statically named attribute with an explicit deterministic prefix when qualified.
    fn attribute(&mut self,local:&str,uri:&str,value:&str,index:usize) -> Result<(),Error> {
        self.attributes += 1; if self.attributes > self.limits.max_attributes { return Err(Error("XML attribute limit")); }
        self.charge(local)?; self.charge(uri)?;
        self.put(" ")?;
        if !uri.is_empty() {
            self.attributes += 1;
            if self.attributes > self.limits.max_attributes { return Err(Error("XML attribute limit")); }
            let prefix=format!("srl{}",index); self.charge(&prefix)?; self.put("xmlns:")?; self.put(&prefix)?;
            self.put("=\"")?; self.escaped(uri,true)?; self.put("\" ")?; self.put(&prefix)?; self.put(":")?;
        }
        self.put(local)?; self.put("=\"")?; self.escaped(value,true)?; self.put("\"")
    }
    /// Close one generated element after all declared content has been written.
    fn close(&mut self,local:&str) -> Result<(),Error> { self.put("</")?; self.put(local)?; self.put(">")?; self.depth -= 1; self.namespaces.pop(); Ok(()) }
    /// Emit a complete statically scalar child.
    fn child(&mut self,local:&str,uri:&str,value:&str) -> Result<(),Error> {
        self.open(local,uri)?; self.put(">")?; self.escaped(value,false)?; self.close(local)
    }
}
/// Checked XML scalar conversion; Rust numeric parsing enforces exact native widths.
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
/// Ordered field state borrows the bounded parsed tree and publishes no model before finish.
struct SrlXmlFields<'a,'input:'a> { attributes:Vec<roxmltree::Attribute<'a,'input>>, used:Vec<bool>, children:Vec<roxmltree::Node<'a,'input>>, next:usize }
impl<'a,'input:'a> SrlXmlFields<'a,'input> {
    /// Reject record text and collect only direct child elements.
    fn new(node:roxmltree::Node<'a,'input>) -> Result<Self,Error> {
        if node.children().any(|child| child.is_text() && !child.text().unwrap_or("").trim_matches([' ','\t','\r','\n']).is_empty()) { return Err(Error("Unexpected XML record text")); }
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
}
)xml";
}

// Generate native Rust static XML methods using the same frozen binding IR as C++.
inline std::string rust_methods(const class_node& owner, const record& contract,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier,
    const xml_options& options) {
  std::string result;
  const auto line = [&result](const std::string& text) { result += text + "\n"; };
  const auto quote = [](std::string_view value) { return native::quote(value); };
  // Select one statically known native scalar lexical conversion.
  const auto read_scalar = [&](const type_name& type, const std::string& text) {
    if (type.type == object_type::enum_type) {
      std::string expression = "match " + text + ".as_str() { ";
      const auto& values = static_cast<const enum_node&>(*type.resolved_node).enum_name_list;
      for (const auto& value : values) { expression += quote(value) + " => " + type_identifier(type) + "::" + native::pascal(value) + ","; }
      return expression + "_ => return Err(Error(\"Unknown XML enum\")) }";
    }
    if (type.name == "string") { return text; }
    if (type.name == "bool") { return "srl_xml_bool(&" + text + ")?"; }
    if (type.name == "float" || type.name == "float32") {
      return "{ let parsed=srl_xml_float(&" + text + ")?; let narrow=parsed as f32; "
          "if parsed.is_finite() && !narrow.is_finite() { return Err(Error(\"XML float32 range\")); } narrow }";
    }
    if (type.name == "double" || type.name == "float64") { return "srl_xml_float(&" + text + ")?"; }
    return "srl_xml_number::<" + type_identifier(type) + ">(&" + text + ")?";
  };
  // Borrow string/enum spellings and only format primitive numeric temporaries.
  const auto scalar = [&](const type_name& type, const std::string& expression) {
    if (type.name == "string") { return "(" + expression + ").as_str()"; }
    if (type.type == object_type::enum_type) { return type_identifier(type) + "::NAMES[*" + expression + " as usize]"; }
    if (type.name == "float" || type.name == "float32" || type.name == "double" || type.name == "float64") {
      return "&{ let value=*" + expression + "; if value.is_nan() { \"NaN\".to_owned() } "
          "else if value == " + type_identifier(type) + "::INFINITY { \"INF\".to_owned() } else if value == " +
          type_identifier(type) + "::NEG_INFINITY { \"-INF\".to_owned() } else { value.to_string() } }";
    }
    return "&(" + expression + ").to_string()";
  };
  line("/// Encode the fixed XML contract with no runtime schema interpretation.");
  line("pub fn encode_xml(&self) -> Result<String,Error> { self.encode_xml_with_validation(XmlLimits::default(),None) }");
  line("/// Bound one complete XML document and optionally apply a trusted prepared validator.");
  line("pub fn encode_xml_with_validation(&self,limits:XmlLimits,validator:Option<&dyn XmlValidator>) -> Result<String,Error> {");
  line("let mut output=SrlXmlWriter::new(limits); self.srl_xml_write(&mut output," + quote(contract.root.local) + "," + quote(contract.root.namespace_uri) + ")?;");
  if (!options.runtime_xsd) { line("if validator.is_some() { return Err(Error(\"Runtime XSD not enabled during generation\")); }"); }
  else if (options.validation_scope != "input") { line("if let Some(schema)=validator { schema.validate(&output.text)?; }"); }
  else { line("let _=validator;"); }
  line("Ok(output.text) }");
  line("/// Decode a fresh owning model after complete XML and mapping checks.");
  line("pub fn decode_xml(text:&str) -> Result<Self,Error> { Self::decode_xml_with_validation(text,XmlLimits::default(),None) }");
  line("/// Apply caller limits and optional trusted XSD while retaining generated checks.");
  line("pub fn decode_xml_with_validation(text:&str,limits:XmlLimits,validator:Option<&dyn XmlValidator>) -> Result<Self,Error> {");
  line("let document=srl_xml_parse(text,limits)?;");
  if (!options.runtime_xsd) { line("if validator.is_some() { return Err(Error(\"Runtime XSD not enabled during generation\")); }"); }
  else if (options.validation_scope != "output") { line("if let Some(schema)=validator { schema.validate(text)?; }"); }
  else { line("let _=validator;"); }
  line("let node=document.root_element(); if !srl_xml_name(node," + quote(contract.root.namespace_uri) + "," + quote(contract.root.local) + ") { return Err(Error(\"Unexpected XML root\")); } Self::srl_xml_read(node) }");
  line("/// Read generated field assignments directly, preserving transactionality.");
  line("fn srl_xml_read(node:roxmltree::Node<'_, '_>) -> Result<Self,Error> { let mut fields=SrlXmlFields::new(node)?; let mut value=Self::default();");
  std::size_t index{};
  for (const auto& field : contract.fields) {
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto& leaf = optional || original.is_sequence() ? original.arguments.front() : original;
    const auto local = quote(field.qualified_name.local);
    const auto uri = quote(field.qualified_name.namespace_uri);
    const auto temporary = "field_" + std::to_string(index++);
    const auto destination = "value." + field_name(source);
    if (field.attribute) {
      line("let " + temporary + "=fields.attribute(" + uri + "," + local + "," + (optional ? "true" : "false") + ")?;");
      const auto conversion = read_scalar(leaf, "text.to_owned()");
      line(destination + "=" + (optional ? "match " + temporary + " { Some(text) => Some(" + conversion + "),None=>None }"
          : "{ let text=" + temporary + ".ok_or(Error(\"Required XML attribute missing\"))?; " + conversion + " }") + ";");
    } else {
      const auto minimum = source.fixed_extent != 0 ? source.fixed_extent : field.min_occurs;
      const auto maximum = source.fixed_extent != 0 ? std::optional<std::size_t>{source.fixed_extent} : field.max_occurs;
      line("let " + temporary + "=fields.children(" + uri + "," + local + "," + std::to_string(minimum) + "," +
          (maximum ? "Some(" + std::to_string(*maximum) + ")" : "None") + ")?;");
      const auto conversion = leaf.type == object_type::class_type ? type_identifier(leaf) + "::srl_xml_read(child)?"
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
  line("/// Emit a nested owning value under its caller-selected static expanded element name.");
  line("fn srl_xml_write(&self,output:&mut SrlXmlWriter,local:&str,uri:&str) -> Result<(),Error> { output.open(local,uri)?;");
  index = 0;
  for (const auto& field : contract.fields) {
    if (!field.attribute) { continue; }
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto& leaf = optional ? original.arguments.front() : original;
    auto expression = "(&self." + field_name(source) + ")";
    if (optional) { line("if let Some(item)=" + expression + ".as_ref() {"); expression = "item"; }
    line("output.attribute(" + quote(field.qualified_name.local) + "," + quote(field.qualified_name.namespace_uri) + "," + scalar(leaf, expression) + "," + std::to_string(index++) + ")?;");
    if (optional) { line("}"); }
  }
  line("output.put(\">\")?;");
  for (const auto& field : contract.fields) {
    if (field.attribute) { continue; }
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto& leaf = optional || original.is_sequence() ? original.arguments.front() : original;
    auto expression = "(&self." + field_name(source) + ")";
    if (optional) { line("if let Some(item)=" + expression + ".as_ref() {"); expression = "item"; }
    if (field.repeated) {
      const auto minimum = source.fixed_extent != 0 ? source.fixed_extent : field.min_occurs;
      const auto maximum = source.fixed_extent != 0 ? std::optional<std::size_t>{source.fixed_extent} : field.max_occurs;
      std::string condition;
      if (minimum != 0) { condition = expression + ".len() < " + std::to_string(minimum); }
      if (maximum) { condition += (condition.empty() ? "" : " || ") + expression + ".len() > " + std::to_string(*maximum); }
      if (!condition.empty()) { line("if " + condition + " { return Err(Error(\"XML array cardinality\")); }"); }
      line("for item in " + expression + ".iter() { "); expression = "item";
    }
    if (leaf.type == object_type::class_type) {
      line(expression + ".srl_xml_write(output," + quote(field.qualified_name.local) + "," + quote(field.qualified_name.namespace_uri) + ")?;");
    } else {
      line("output.child(" + quote(field.qualified_name.local) + "," + quote(field.qualified_name.namespace_uri) + "," + scalar(leaf, expression) + ")?;");
    }
    if (field.repeated) { line("}"); }
    if (optional) { line("}"); }
  }
  line("output.close(local) }");
  static_cast<void>(owner);
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
