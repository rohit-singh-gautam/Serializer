// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_schema.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_binding {

// Emit optional native JavaScript XML support; ordinary generated modules retain no XML imports.
inline std::string js_runtime() {
  return R"xml(
import {DOMParser as SrlXmlParser} from '@xmldom/xmldom';
// Complete-document XML policy, independent of binary/JSON decoder options.
export class XmlLimits {
  constructor(values = {}) {
    Object.assign(this, {maxBytes:16777216,maxTextBytes:8388608,maxNodes:100000,
      maxAttributes:100000,maxDepth:128}, values);
    for (const key of ['maxBytes','maxTextBytes','maxNodes','maxAttributes','maxDepth']) {
      if (!Number.isSafeInteger(this[key]) || this[key] < 0) throw new Error('Invalid XML limits');
    }
  }
}
// Count UTF-8 bytes without allocating an encoded copy of an over-limit input string.
function srlXmlUtf8Length(value) {
  let result = 0;
  for (const item of value) {
    const c = item.codePointAt(0); result += c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
  }
  return result;
}
// XML whitespace is SP/TAB/CR/LF; other Unicode spaces remain significant data.
function srlXmlTrim(value) { return value.replace(/^[ \t\r\n]+|[ \t\r\n]+$/g,''); }
// Reject invalid XML scalar code points before a DOM writer can preserve them literally.
function srlXmlCharacters(value) {
  for (const item of value) {
    const c = item.codePointAt(0);
    if (!(c === 9 || c === 10 || c === 13 || (c >= 32 && c <= 0xd7ff) ||
          (c >= 0xe000 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff))) {
      throw new Error('Illegal XML character');
    }
  }
  return value;
}
// Compare expanded names; namespace prefixes are never contract identities.
function srlXmlName(node, uri, local) {
  return node.nodeType === 1 && (node.namespaceURI || '') === uri && node.localName === local;
}
// Refuse declarations before parsing while allowing the same literal text inside CDATA/comments.
function srlXmlRejectDtd(text) {
  let position=0;
  while ((position=text.indexOf('<',position)) >= 0) {
    let end=null;
    if (text.startsWith('<!--',position)) end='-->';
    else if (text.startsWith('<![CDATA[',position)) end=']]>';
    else if (text.startsWith('<?',position)) end='?>';
    else if (text.startsWith('<!DOCTYPE',position) || text.startsWith('<!ENTITY',position)) throw new Error('XML DTD/entities forbidden');
    if (end) { const next=text.indexOf(end,position+2); if (next < 0) return; position=next+end.length; }
    else ++position;
  }
}
// Bound a complete native DOM and reject mixed content, DTDs and duplicate expanded attributes.
function srlXmlCheck(document, limits) {
  let nodes = 0, attributes = 0, text = 0;
  const visit = (node, depth) => {
    if (node.nodeType === 10 || node.nodeType === 5) throw new Error('XML DTD/entities forbidden');
    if (node.nodeType === 1) {
      if (++nodes > limits.maxNodes || ++depth > limits.maxDepth) throw new Error('XML node/depth limit');
      text += srlXmlUtf8Length(node.localName) + srlXmlUtf8Length(node.namespaceURI || '');
      const expanded = new Set();
      for (const attr of Array.from(node.attributes)) {
        if (++attributes > limits.maxAttributes) throw new Error('XML attribute limit');
        const key = (attr.namespaceURI || '') + '\0' + attr.localName;
        if (expanded.has(key)) throw new Error('Duplicate XML expanded attribute');
        expanded.add(key); text += srlXmlUtf8Length(attr.value);
        if (attr.namespaceURI === 'http://www.w3.org/2000/xmlns/') {
          text += srlXmlUtf8Length(attr.prefix === 'xmlns' ? attr.localName : '');
        } else text += srlXmlUtf8Length(attr.localName) + srlXmlUtf8Length(attr.namespaceURI || '');
        srlXmlCharacters(attr.value);
      }
      const children = Array.from(node.childNodes);
      if (children.some(child => child.nodeType === 1) && children.some(child =>
          (child.nodeType === 3 || child.nodeType === 4) && srlXmlTrim(child.data) !== '')) {
        throw new Error('Mixed XML content unsupported');
      }
    } else if (node.nodeType === 3 || node.nodeType === 4) {
      text += srlXmlUtf8Length(node.data); srlXmlCharacters(node.data);
    }
    if (text > limits.maxTextBytes) throw new Error('XML text limit');
    for (const child of Array.from(node.childNodes || [])) visit(child, depth);
  };
  visit(document, 0);
}
// Parse one bounded XML document using the pinned native parser; no resource retrieval hooks exist.
function srlXmlParse(text, limits) {
  if (typeof text !== 'string' || text.length > limits.maxBytes || srlXmlUtf8Length(text) > limits.maxBytes) {
    throw new Error('XML input byte limit');
  }
  srlXmlRejectDtd(text);
  const declaration=text.match(/^\uFEFF?<\?xml[ \t\r\n]+([^?]*)\?>/);
  if (declaration && !/^version[ \t\r\n]*=[ \t\r\n]*(?:"1\.0"|'1\.0')(?:[ \t\r\n]+encoding[ \t\r\n]*=[ \t\r\n]*(?:"[Uu][Tt][Ff]-8"|'[Uu][Tt][Ff]-8'))?(?:[ \t\r\n]+standalone[ \t\r\n]*=[ \t\r\n]*(?:"(?:yes|no)"|'(?:yes|no)'))?[ \t\r\n]*$/.test(declaration[1])) {
    throw new Error('Only XML 1.0 UTF-8 declarations are supported');
  }
  const document = new SrlXmlParser({onError() { throw new Error('Malformed XML'); }})
    .parseFromString(text, 'application/xml');
  srlXmlCheck(document, limits);
  if (!document.documentElement) throw new Error('Missing XML root');
  return document.documentElement;
}
// Write bounded XML directly; XMLSerializer normalizes literal CR/attribute whitespace on reread.
class SrlXmlWriter {
  constructor(limits) { this.limits=limits; this.text=''; this.bytes=0; this.nodes=0; this.depth=0; this.attributes=0; this.textBytes=0; this.namespaces=[]; }
  put(text) {
    const bytes=srlXmlUtf8Length(text); if (bytes > this.limits.maxBytes-this.bytes) throw new Error('XML output byte limit');
    this.bytes+=bytes; this.text+=text;
  }
  charge(text) {
    this.textBytes+=srlXmlUtf8Length(text); if (this.textBytes > this.limits.maxTextBytes) throw new Error('XML text limit');
  }
  escaped(text,attribute) {
    this.charge(text);
    srlXmlCharacters(text); let batch='';
    for (const item of text) {
      batch+=item === '&' ? '&amp;' : item === '<' ? '&lt;' : item === '>' ? '&gt;' :
        item === '"' && attribute ? '&quot;' : item === '\r' ? '&#13;' :
        item === '\n' && attribute ? '&#10;' : item === '\t' && attribute ? '&#9;' : item;
      if (batch.length >= 4096) { this.put(batch); batch=''; }
    }
    if (batch) this.put(batch);
  }
  open(local,uri) {
    if (++this.nodes > this.limits.maxNodes || ++this.depth > this.limits.maxDepth) throw new Error('XML node/depth limit');
    this.charge(local); this.charge(uri);
    this.put('<'+local);
    if ((this.namespaces[this.namespaces.length-1] || '') !== uri) {
      if (++this.attributes > this.limits.maxAttributes) throw new Error('XML attribute limit');
      this.put(' xmlns="'); this.escaped(uri,true); this.put('"');
    }
    this.namespaces.push(uri);
  }
  attribute(local,uri,text,index) {
    if (++this.attributes > this.limits.maxAttributes) throw new Error('XML attribute limit');
    this.charge(local); this.charge(uri);
    if (uri) {
      if (++this.attributes > this.limits.maxAttributes) throw new Error('XML attribute limit');
      const prefix='srl'+index; this.charge(prefix); this.put(' xmlns:'+prefix+'="'); this.escaped(uri,true); this.put('" '+prefix+':'+local+'="');
    }
    else this.put(' '+local+'="'); this.escaped(text,true); this.put('"');
  }
  close(local) { this.put('</'+local+'>'); --this.depth; this.namespaces.pop(); }
  child(local,uri,text) { this.open(local,uri); this.put('>'); this.escaped(text,false); this.close(local); }
}
// Check numeric/Boolean lexemes and exact declared ranges before assigning native scalar values.
function srlXmlScalar(text, kind, encoding = false) {
  if (kind === 'string') {
    if (typeof text !== 'string') throw new Error('XML string type');
    return encoding ? srlXmlCharacters(text) : text;
  }
  if (kind === 'bool') {
    if (encoding) { if (typeof text !== 'boolean') throw new Error('XML Boolean type'); return text ? 'true' : 'false'; }
    text = srlXmlTrim(text); if (text === 'true' || text === '1') return true;
    if (text === 'false' || text === '0') return false; throw new Error('XML Boolean lexical value');
  }
  if (kind.startsWith('int') || kind.startsWith('uint')) {
    if (encoding && typeof text !== (kind.endsWith('64') ? 'bigint' : 'number')) throw new Error('XML integer type');
    const lexical = srlXmlTrim(String(text));
    if (!/^[+-]?[0-9]+$/.test(lexical)) throw new Error('XML integer lexical value');
    const value = BigInt(lexical); const bits = Number(kind.replace(/^[a-z]+/, ''));
    const unsigned = kind.startsWith('u');
    const minimum = unsigned ? 0n : -(1n << BigInt(bits - 1));
    const maximum = (1n << BigInt(unsigned ? bits : bits - 1)) - 1n;
    if (value < minimum || value > maximum) throw new Error('XML integer range');
    return encoding ? value.toString() : bits === 64 ? value : Number(value);
  }
  if (encoding) {
    if (typeof text !== 'number') throw new Error('XML floating type');
    if (kind === 'float' || kind === 'float32') {
      const value=Math.fround(text); if (Number.isFinite(text) && !Number.isFinite(value)) throw new Error('XML float32 range'); text=value;
    }
    if (Object.is(text,-0)) return '-0';
    return Number.isNaN(text) ? 'NaN' : text === Infinity ? 'INF' : text === -Infinity ? '-INF' : String(text);
  }
  text = srlXmlTrim(text); if (text === 'INF') return Infinity; if (text === '-INF') return -Infinity;
  if (text === 'NaN') return NaN;
  if (!/^[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?$/.test(text)) throw new Error('XML floating lexical value');
  const value = Number(text); if (!Number.isFinite(value)) throw new Error('XML floating range');
  if ((kind === 'float' || kind === 'float32') && !Number.isFinite(Math.fround(value))) throw new Error('XML float32 range');
  return kind === 'float' || kind === 'float32' ? Math.fround(value) : value;
}
// Track exact attribute membership and ordered repeated child consumption for a generated record.
class SrlXmlFields {
  constructor(node, hasText = false) {
    this.node = node;
    this.attributes = Array.from(node.attributes).filter(attr => attr.namespaceURI !== 'http://www.w3.org/2000/xmlns/');
    this.used = new Set(); this.children = Array.from(node.childNodes).filter(child => child.nodeType === 1); this.next = 0;
    for (const child of Array.from(node.childNodes)) {
      if (!hasText && (child.nodeType === 3 || child.nodeType === 4) && srlXmlTrim(child.data)) throw new Error('Unexpected XML record text');
    }
  }
  // Read simple content without accepting nested elements.
  text() { if (this.children.length) throw new Error('XML simple content contains children'); return this.node.textContent || ''; }
  attribute(uri, local, optional) {
    const index = this.attributes.findIndex(attr => (attr.namespaceURI || '') === uri && attr.localName === local);
    if (index < 0) { if (optional) return null; throw new Error('Required XML attribute missing'); }
    this.used.add(index); return this.attributes[index].value;
  }
  childrenFor(uri, local, minimum, maximum) {
    const result = []; while (this.next < this.children.length && srlXmlName(this.children[this.next], uri, local)) {
      result.push(this.children[this.next++]);
      if (maximum !== null && result.length > maximum) throw new Error('XML occurrence maximum');
    }
    if (result.length < minimum) throw new Error('Required XML child missing or out of order'); return result;
  }
  finish() { if (this.used.size !== this.attributes.length || this.next !== this.children.length) throw new Error('Unexpected XML member'); }
  // Require one wrapper and consume its entire item sequence.
  wrapped(wrapperUri, wrapperLocal, uri, local, minimum, maximum) { const fields = new SrlXmlFields(this.childrenFor(wrapperUri, wrapperLocal, 1, 1)[0]); const result = fields.childrenFor(uri, local, minimum, maximum); fields.finish(); return result; }
}
// Reject nested markup/attributes in statically scalar fields.
function srlXmlText(node) {
  if (Array.from(node.attributes).some(attr => attr.namespaceURI !== 'http://www.w3.org/2000/xmlns/') ||
      Array.from(node.childNodes).some(child => child.nodeType === 1)) throw new Error('XML scalar contains markup');
  return node.textContent || '';
}
)xml";
}

// Emit matching declarations for the optional JavaScript XML runtime.
inline std::string js_declarations() {
  return "export declare class XmlLimits { maxBytes:number; maxTextBytes:number; maxNodes:number; "
      "maxAttributes:number; maxDepth:number; constructor(values?:Partial<XmlLimits>); }\n"
      "export interface XmlValidator { validate(document:string):void; }\n";
}

// Emit direct model-member mapping; parsing remains a separate mature XML dependency.
inline std::string js_methods(const class_node& owner, const record& contract,
    const std::string& generated_name,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier,
    const xml_options& options, bool typescript = false) {
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const std::set<std::string> generated_methods{"encodeXml", "decodeXml", "_srlXmlRead", "_srlXmlWrite"};
  // Apply the emitter's actual naming/access profile to every authored class API.
  const auto check_identifier = [&](std::string_view authored_name, access_type access) {
    member naming_probe{};
    naming_probe.name = authored_name;
    naming_probe.access = access;
    auto identifier = field_name(naming_probe);
    if (identifier.starts_with('#')) { identifier.erase(0, 1); }
    if (generated_methods.contains(identifier)) {
      throw std::invalid_argument{"XML method name collision: " + std::string{authored_name}};
    }
  };
  for (const auto& item : owner.member_list) {
    check_identifier(item.name, item.access);
  }
  for (const auto& method : owner.functions) { check_identifier(method.name, method.access); }
  for (const auto& factory : owner.constructions) {
    check_identifier(factory.constructor ? "create" : factory.name, factory.access);
  }
  for (const auto& conversion : owner.conversions) { check_identifier(conversion.name, conversion.access); }
  if (typescript) {
    return "// Encode one complete XML document using the fixed generated mapping.\n"
        "encodeXml(limits?:XmlLimits,validator?:XmlValidator):string;\n"
        "// Decode a fresh owning value after complete contract validation.\n"
        "static decodeXml(text:string,limits?:XmlLimits,validator?:XmlValidator):" + generated_name + ";\n";
  }
  std::string result;
  // Append one generated JavaScript statement without relying on emitter indentation state.
  const auto line = [&result](const std::string& text) { result += text + "\n"; };
  // Render scalar or nested-record reading without JSON conversion.
  const auto read = [&](const type_name& type, const std::string& expression, bool attribute) {
    if (type.type == object_type::class_type) { return type_identifier(type) + "._srlXmlRead(" + expression + ")"; }
    const auto text = attribute ? expression : "srlXmlText(" + expression + ")";
    if (type.type == object_type::enum_type) {
      return "(() => { const value = " + type_identifier(type) + "Names.indexOf(" + text +
          "); if (value < 0) throw new Error('Unknown XML enum'); return value; })()";
    }
    return "srlXmlScalar(" + text + "," + quote(type.name) + ")";
  };
  // Render statically checked scalar output; enum spellings retain original schema names.
  const auto scalar = [&](const type_name& type, const std::string& expression) {
    if (type.type == object_type::enum_type) {
      return "(() => { const value = " + type_identifier(type) + "Names[" + expression +
          "]; if (value === undefined) throw new Error('Unknown XML enum'); return value; })()";
    }
    return "srlXmlScalar(" + expression + "," + quote(type.name) + ",true)";
  };
  line("// Emit one complete XML document; no schema interpreter runs on the default path.");
  line("encodeXml(limits = new XmlLimits(), validator = null) {");
  line("limits = new XmlLimits(limits); const output=new SrlXmlWriter(limits);");
  line("this._srlXmlWrite(output," + quote(contract.root.local) + "," + quote(contract.root.namespace_uri) + "); const text=output.text;");
  if (options.runtime_xsd && options.validation_scope != "input") {
    line("if (validator) validator.validate(text);");
  } else if (!options.runtime_xsd) {
    line("if (validator) throw new Error('Runtime XSD was not enabled during generation');");
  }
  line("return text; }");
  line("// Decode into a fresh candidate; no partial model escapes on any contract error.");
  line("static decodeXml(text, limits = new XmlLimits(), validator = null) {");
  line("limits = new XmlLimits(limits);");
  line("const root = srlXmlParse(text,limits);");
  if (options.runtime_xsd && options.validation_scope != "output") {
    line("if (validator) validator.validate(text);");
  } else if (!options.runtime_xsd) {
    line("if (validator) throw new Error('Runtime XSD was not enabled during generation');");
  }
  line("if (!srlXmlName(root," +
      quote(contract.root.namespace_uri) + "," + quote(contract.root.local) +
      ")) throw new Error('Unexpected XML root'); return this._srlXmlRead(root); }");
  line("// Read statically bound attributes and children in contract order.");
  line("static _srlXmlRead(node) { const fields = new SrlXmlFields(node," + std::string{std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) { return field.text; }) ? "true" : "false"} + "); const value = new " + generated_name + "();");
  std::size_t index{};
  for (const auto& field : contract.fields) {
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto& leaf = optional || original.is_sequence() ? original.arguments.front() : original;
    const auto local = quote(field.qualified_name.local);
    const auto uri = quote(field.qualified_name.namespace_uri);
    const auto temporary = "field" + std::to_string(index++);
    const auto destination = "value." + field_name(source);
    if (field.text) {
      line(destination + " = " + read(leaf, "fields.text()", true) + ";");
    } else if (field.attribute) {
      line("const " + temporary + " = fields.attribute(" + uri + "," + local + "," +
          (optional ? "true" : "false") + ");");
      line(destination + " = " + (optional ? temporary + " === null ? [] : [" : "") +
          read(leaf, temporary, true) + (optional ? "]" : "") + ";");
    } else {
      auto minimum = field.min_occurs;
      auto maximum = field.max_occurs;
      if (source.fixed_extent != 0) { minimum = source.fixed_extent; maximum = source.fixed_extent; }
      line("const " + temporary + " = fields." + std::string{field.wrapper ? "wrapped(" + quote(field.wrapper->namespace_uri) + "," + quote(field.wrapper->local) + "," : "childrenFor("} + uri + "," + local + "," +
          std::to_string(minimum) + "," + (maximum ? std::to_string(*maximum) : "null") + ");");
      if (field.repeated) {
        line(destination + " = " + temporary + ".map(child => " + read(leaf, "child", false) + ");");
      } else {
        line(destination + " = " + (optional ? temporary + ".length === 0 ? [] : [" : "") +
            read(leaf, temporary + "[0]", false) + (optional ? "]" : "") + ";");
      }
    }
  }
  line("fields.finish(); return value; }");
  line("// Write one caller-named element directly from typed native model fields.");
  line("_srlXmlWrite(output,local,uri) { output.open(local,uri);");
  index = 0;
  for (const auto& field : contract.fields) {
    if (!field.attribute) { continue; }
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto& leaf = optional ? original.arguments.front() : original;
    auto expression = "this." + field_name(source);
    if (optional) {
      line("if (!Array.isArray(" + expression + ") || " + expression + ".length > 1) throw new Error('Invalid XML optional');");
      line("if (" + expression + ".length === 1) {");
      expression += "[0]";
    }
    line("output.attribute(" + quote(field.qualified_name.local) + "," +
        quote(field.qualified_name.namespace_uri) + "," + scalar(leaf, expression) + "," + std::to_string(index++) + ");");
    if (optional) { line("}"); }
  }
  line("output.put('>');");
  index = 0;
  for (const auto& field : contract.fields) {
    if (field.attribute) { continue; }
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const auto& leaf = optional || original.is_sequence() ? original.arguments.front() : original;
    const auto local = quote(field.qualified_name.local);
    const auto uri = quote(field.qualified_name.namespace_uri);
    auto expression = "this." + field_name(source);
    if (optional) {
      line("if (!Array.isArray(" + expression + ") || " + expression + ".length > 1) throw new Error('Invalid XML optional');");
      line("if (" + expression + ".length === 1) {");
      expression += "[0]";
    }
    if (field.wrapper) { line("output.open(" + quote(field.wrapper->local) + "," + quote(field.wrapper->namespace_uri) + "); output.put('>');"); }
    if (field.repeated) {
      const auto minimum = source.fixed_extent != 0 ? source.fixed_extent : field.min_occurs;
      const auto maximum = source.fixed_extent != 0 ? std::optional<std::size_t>{source.fixed_extent} : field.max_occurs;
      line("if (!Array.isArray(" + expression + ") || " + expression + ".length < " + std::to_string(minimum) +
          (maximum ? " || " + expression + ".length > " + std::to_string(*maximum) : "") +
          ") throw new Error('XML array cardinality');");
      line("for (const item of " + expression + ") {");
    }
    const auto item = field.repeated ? std::string{"item"} : expression;
    if (field.text) {
      line("output.escaped(" + scalar(leaf, item) + ",false);");
    } else if (leaf.type == object_type::class_type) {
      line(item + "._srlXmlWrite(output," + local + "," + uri + ");");
    } else {
      line("output.child(" + local + "," + uri + "," + scalar(leaf, item) + ");");
    }
    if (field.repeated) { line("}"); }
    if (field.wrapper) { line("output.close(" + quote(field.wrapper->local) + ");"); }
    if (optional) { line("}"); }
    ++index;
  }
  line("output.close(local); }");
  static_cast<void>(owner);
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
