// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native_schema.hpp"
#include "xml_js_container_writer.hpp"
#include "xml_js_variant_writer.hpp"
#include "xml_js_model_writer.hpp"
#include "xml_js_scalar_writer.hpp"
#include "xml_js_mixed_writer.hpp"
#include "xml_js_identity_writer.hpp"
#include "xml_js_extension_writer.hpp"
#include "xml_js_lifecycle_writer.hpp"
#include "xml_js_projection_writer.hpp"
#include "xml_schema.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_binding {

inline constexpr std::string_view js_token_runtime =
#include "xml_js_token_runtime.inc"
;
inline constexpr std::string_view js_container_runtime =
#include "xml_js_container_runtime.inc"
;
inline constexpr std::string_view js_variant_runtime =
#include "xml_js_variant_runtime.inc"
;
inline constexpr std::string_view js_scalar_runtime =
#include "xml_js_scalar_runtime.inc"
;
inline constexpr std::string_view js_namespace_runtime =
#include "xml_js_namespace_runtime.inc"
;
inline constexpr std::string_view js_mixed_runtime =
#include "xml_js_mixed_runtime.inc"
;
inline constexpr std::string_view js_identity_runtime =
#include "xml_js_identity_runtime.inc"
;
inline constexpr std::string_view js_extension_runtime =
#include "xml_js_extension_runtime.inc"
;
inline constexpr std::string_view js_html_runtime =
#include "xml_js_html_runtime.inc"
;
inline constexpr std::string_view js_lifecycle_runtime =
#include "xml_js_lifecycle_runtime.inc"
;
inline constexpr std::string_view js_provider_runtime =
#include "xml_js_provider_runtime.inc"
;
inline constexpr std::string_view js_projection_runtime =
#include "xml_js_projection_runtime.inc"
;

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
    this.map=new XmlMapLimits(this.map || {});
    this.pattern=new XmlPatternLimits(this.pattern || {});this.patternWork=0;
    this.mixed=new XmlMixedLimits(this.mixed || {});
    this.identity=new XmlIdentityLimits(this.identity || {});
    this.extensions=new XmlExtensionOptions(this.extensions || {});
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
      if (++nodes > limits.maxNodes || ++depth > limits.maxDepth) throw srlXmlQuotaError('XML node/depth limit');
      text += srlXmlUtf8Length(node.localName) + srlXmlUtf8Length(node.namespaceURI || '');
      const expanded = new Set();
      for (const attr of Array.from(node.attributes)) {
        if (++attributes > limits.maxAttributes) throw srlXmlQuotaError('XML attribute limit');
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
    if (text > limits.maxTextBytes) throw srlXmlQuotaError('XML text limit');
    for (const child of Array.from(node.childNodes || [])) visit(child, depth);
  };
  visit(document, 0);
}
// Parse one bounded XML document using the pinned native parser; no resource retrieval hooks exist.
function srlXmlParse(text, limits) {
  if (typeof text !== 'string') { throw new Error('XML input byte limit'); }
  if (text.length > limits.maxBytes || srlXmlUtf8Length(text) > limits.maxBytes) {
    throw srlXmlQuotaError('XML input byte limit');
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
)xml"
R"xml(
// Write bounded XML directly; XMLSerializer normalizes literal CR/attribute whitespace on reread.
class SrlXmlWriter {
  constructor(limits) { this.limits=limits; this.text=''; this.bytes=0; this.nodes=0; this.depth=0; this.attributes=0; this.textBytes=0; this.namespaces=[]; this.presentation=[]; this.prefixes=[]; this.identities=[]; this.identityScopes=[]; this.head=null; }
  put(text) {
    const bytes=srlXmlUtf8Length(text); if (bytes > this.limits.maxBytes-this.bytes) throw srlXmlQuotaError('XML output byte limit');
    if(text==='>' && this.head!==null){const head=this.head;this.head=null;for(const observer of this.identities)observer.start(head.name,head.attributes,head.scope);}
    this.bytes+=bytes; this.text+=text;
  }
  charge(text) {
    if(this.limits.operation){this.limits.operation("work",srlXmlUtf8Length(text),this.field||"");this.limits.operation("allocation",2*text.length,this.field||"");}
    this.textBytes+=srlXmlUtf8Length(text); if (this.textBytes > this.limits.maxTextBytes) throw srlXmlQuotaError('XML text limit');
  }
  escaped(text,attribute) {
    this.charge(text);
    if(!attribute)for(const observer of this.identities)observer.text(text);
    srlXmlCharacters(text); let batch='';
    for (const item of text) {
      batch+=item === '&' ? '&amp;' : item === '<' ? '&lt;' : item === '>' ? '&gt;' :
        item === '"' && attribute ? '&quot;' : item === '\r' ? '&#13;' :
        item === '\n' && attribute ? '&#10;' : item === '\t' && attribute ? '&#9;' : item;
      if (batch.length >= 4096) { this.put(batch); batch=''; }
    }
    if (batch) this.put(batch);
  }
  open(local,uri,emptyDefault=false,defaultOverride=null,reserved=null) {
    this.field=local;if(this.limits.operation)this.limits.operation("allocation",128,local);
    if (++this.nodes > this.limits.maxNodes || ++this.depth > this.limits.maxDepth) throw srlXmlQuotaError('XML node/depth limit');
    this.charge(local); this.charge(uri);
    const defaultUri=defaultOverride===null ? emptyDefault ? '' : uri : defaultOverride;
    if(!uri && defaultUri)throw new Error('Default QName namespace conflicts with unnamespaced XML element');
    let prefix='srlv';
    if(reserved)for(const [bound,namespace] of reserved)if(bound && namespace===uri){prefix=bound;break;}
    while(reserved && reserved.has(prefix) && reserved.get(prefix)!==uri)prefix+='v';
    const qualified=defaultUri!==uri && uri ? prefix+':'+local : local;
    this.put('<'+qualified);
    const bindings=new Map();bindings.set('',defaultUri);this.prefixes.push(bindings);
    let scope=null;
    if(this.identities.length){
      let parent=this.identityScopes[this.identityScopes.length-1] || null;
      if(parent===null)for(let index=0;index+1<this.prefixes.length;++index)parent={parent,bindings:this.prefixes[index]};
      scope={parent,bindings};this.head={name:srlXmlTokenName(uri,local),attributes:new Map(),scope};
    }
    this.identityScopes.push(scope);
    if ((this.namespaces[this.namespaces.length-1] || '') !== defaultUri) {
      if (++this.attributes > this.limits.maxAttributes) throw srlXmlQuotaError('XML attribute limit');
      this.put(' xmlns="'); this.escaped(defaultUri,true); this.put('"');
    }
    if(defaultUri!==uri && uri) this.namespace(prefix,uri);
    this.namespaces.push(defaultUri);this.presentation.push(qualified);
  }
  // Declare one generated trusted prefix while charging the same logical attribute/text quantities.
  namespace(prefix,uri) {
    const selected=this.prefixes[this.prefixes.length-1];
    if(selected.has(prefix)){if(selected.get(prefix)!==uri)throw new Error('Conflicting XML namespace declaration');return;}
    selected.set(prefix,uri);
    if(++this.attributes>this.limits.maxAttributes) throw srlXmlQuotaError('XML attribute limit');
    this.charge(prefix);this.put(' xmlns:'+prefix+'="');this.escaped(uri,true);this.put('"');
  }
  attribute(local,uri,text,index) {
    this.field=local;if(this.limits.operation)this.limits.operation("allocation",96,local);
    if (++this.attributes > this.limits.maxAttributes) throw srlXmlQuotaError('XML attribute limit');
    this.charge(local); this.charge(uri);
    if(this.head)this.head.attributes.set(srlXmlTokenName(uri,local),text);
    if (uri === 'http://www.w3.org/XML/1998/namespace') this.put(' xml:'+local+'="');
    else if (uri) {
      const selected=this.prefixes[this.prefixes.length-1];
      let prefix='srl'+index;
      for(const [bound,namespace] of selected)if(bound && namespace===uri){prefix=bound;break;}
      while(selected.has(prefix) && selected.get(prefix)!==uri)prefix+='a';
      this.namespace(prefix,uri);this.put(' '+prefix+':'+local+'="');
    }
    else this.put(' '+local+'="'); this.escaped(text,true); this.put('"');
  }
  close(local) { this.put('</'+this.presentation.pop()+'>');for(const observer of this.identities)observer.end(); --this.depth; this.namespaces.pop(); this.prefixes.pop(); this.identityScopes.pop(); }
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
    const bits = Number(kind.replace(/^[a-z]+/, ''));
    const unsigned = kind.startsWith('u');
    const minimum = unsigned ? 0n : -(1n << BigInt(bits - 1));
    const maximum = (1n << BigInt(unsigned ? bits : bits - 1)) - 1n;
    const digits=lexical.replace(/^[+-]?0*/,'') || '0';
    if(digits.length>maximum.toString().length) throw new Error('XML integer range');
    const value=BigInt((lexical[0]==='-' ? '-' : '')+digits);
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
  return kind === 'float' || kind === 'float32' ? srlXmlSingle(text,value) : value;
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
)xml" + std::string{js_token_runtime} + std::string{js_container_runtime} + std::string{js_variant_runtime} +
      std::string{js_scalar_runtime} + std::string{js_namespace_runtime} + std::string{js_mixed_runtime} +
      std::string{js_identity_runtime} + std::string{js_extension_runtime} + std::string{js_html_runtime} +
      std::string{js_lifecycle_runtime} + std::string{js_provider_runtime} + std::string{js_projection_runtime};
}

// Emit matching declarations for the optional JavaScript XML runtime.
inline std::string js_declarations() {
  return "export declare class XmlLimits { maxBytes:number; maxTextBytes:number; maxNodes:number; "
      "maxAttributes:number; maxDepth:number; operation?:((resource:string,count:number,field:string)=>void)|null; map:XmlMapLimits; pattern:XmlPatternLimits; mixed:XmlMixedLimits; identity:XmlIdentityLimits; extensions:XmlExtensionOptions; constructor(values?:Partial<XmlLimits>); }\n"
      "export declare class XmlMapLimits { maxEntries:number; maxKeyBytes:number; constructor(values?:Partial<XmlMapLimits>); }\n"
      "export declare class XmlPatternLimits { maxStates:number; maxInputBytes:number; maxWorkUnits:number; constructor(values?:Partial<XmlPatternLimits>); }\n"
      "export declare class XmlMixedLimits { maxFragments:number; maxTextBytes:number; constructor(values?:Partial<XmlMixedLimits>); }\n"
      "export declare class XmlIdentityLimits { maxConstraints:number; maxFields:number; maxDepth:number; maxSelectedNodes:number; maxTuples:number; maxTupleBytes:number; maxWorkUnits:number; constructor(values?:Partial<XmlIdentityLimits>); }\n"
      "export declare class XmlExtensionLimits { maxSlots:number; maxItems:number; maxPreservedBytes:number; constructor(values?:Partial<XmlExtensionLimits>); }\n"
      "export declare class XmlExtensionOptions { limits:XmlExtensionLimits; understoodNamespaces:readonly string[]; requiredPrefixes:string; constructor(values?:Partial<XmlExtensionOptions>); }\n"
      "export declare class HtmlLimits { maxInputBytes:number; maxOutputBytes:number; maxEvents:number; maxDepth:number; maxAttributesPerElement:number; maxTextBytes:number; constructor(values?:Partial<HtmlLimits>); }\n"
      "export declare class HtmlAttribute { name:string; value:string; boolean:boolean; constructor(name:string,value?:string,boolean?:boolean); }\n"
      "export declare class HtmlEvent { kind:string; name:string; attributes:HtmlAttribute[]; text:string; constructor(kind:string,name?:string,attributes?:Iterable<HtmlAttribute>,text?:string); }\n"
      "export declare class Html { static supportedSubset():string; static parseFragment(data:Uint8Array,limits?:HtmlLimits):HtmlEvent[]; static writeFragment(events:HtmlEvent[],limits?:HtmlLimits):string; }\n"
      "export interface XmlValidator { validate(document:string):void; }\n"
      "export declare class XmlSchema implements XmlValidator { private constructor(); static prepare(xsd:string|Uint8Array,limits?:XmlLimits):Promise<XmlSchema>; validate(document:string|Uint8Array,limits?:XmlLimits):void; dispose():void; }\n";
}

// Emit direct model-member mapping; parsing remains a separate mature XML dependency.
inline std::string js_methods(const class_node& owner, const record& contract,
    const std::string& generated_name,
    const std::function<std::string(const member&)>& field_name,
    const std::function<std::string(const type_name&)>& type_identifier,
    const xml_options& options, bool typescript = false) {
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const std::set<std::string> generated_methods{"encodeXml", "decodeXml", "decodeXmlChunks", "xmlBindNamespace", "_srlXmlToken", "_srlXmlMixed", "_srlXmlRead", "_srlXmlWrite"};
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
        "static decodeXml(text:string,limits?:XmlLimits,validator?:XmlValidator):" + generated_name + ";\n"
        "// Consume arbitrary UTF-8 slices and return only after complete document EOF.\n"
        "static decodeXmlChunks(chunks:Iterable<Uint8Array>,limits?:XmlLimits):" + generated_name + ";\n"
        "// Bind XML-only QName vocabulary for this owning value; native codecs ignore these declarations.\n"
        "xmlBindNamespace(prefix:string,uri:string):this;\n";
  }
  std::string result;
  // Append one generated JavaScript statement without relying on emitter indentation state.
  const auto line = [&result](const std::string& text) { result += text + "\n"; };
  // Render scalar or nested-record reading without JSON conversion.
  const auto read = [&](const type_name& type, const std::string& expression, bool attribute) {
    if (type.type == object_type::class_type) { return type_identifier(type) + "._srlXmlRead(" + expression + ")"; }
    const auto text = attribute ? expression : "srlXmlText(" + expression + ")";
    if (type.is_digest()) {
      return "srlXmlDigest(" + text + ',' + std::to_string(type.digest_extent) + ')';
    }
    if (type.type == object_type::enum_type) {
      return "(() => { const value = " + type_identifier(type) + "Names.indexOf(" + text +
          "); if (value < 0) throw new Error('Unknown XML enum'); return value; })()";
    }
    return "srlXmlScalar(" + text + "," + quote(type.name) + ")";
  };
  // Render statically checked scalar output; enum spellings retain original schema names.
  const auto scalar = [&](const type_name& type, const std::string& expression) {
    if (type.is_digest()) {
      return "srlXmlDigest(" + expression + ',' + std::to_string(type.digest_extent) + ",true)";
    }
    if (type.type == object_type::enum_type) {
      return "(() => { if (!Number.isInteger(" + expression + ")) throw new Error('Unknown XML enum'); const encoded = " +
          type_identifier(type) + "Names[" + expression +
          "]; if (encoded === undefined) throw new Error('Unknown XML enum'); return encoded; })()";
    }
    return "srlXmlScalar(" + expression + "," + quote(type.name) + ",true)";
  };
  // Lower immutable field callbacks inside the declaring class so private native fields stay accessible.
  line("static _srlXmlToken=Object.freeze({root:srlXmlTokenName(" + quote(contract.root.namespace_uri) + ',' +
      quote(contract.root.local) + "),create:()=>new " + generated_name + "(),mixed:" +
      (contract.mixed ? js_mixed_metadata(owner, *contract.mixed, field_name, type_identifier) : "null") +
      ",identity:" + (contract.identity ? js_identity_metadata(*contract.identity) : "null") +
      ",extensions:" + (contract.extensions ? js_extension_metadata(*contract.extensions, contract.fields.size()) : "null") +
      ",model:" + js_lifecycle_metadata(owner, contract, field_name, type_identifier) +
      ",fields:Object.freeze([");
  for (const auto& field : contract.fields) {
    const auto& source = *field.source;
    const auto& original = source.type_name_list.front();
    const bool optional = original.is_optional();
    const bool projection = js_value_projection(original);
    const auto* leaf = &js_projected_leaf(original);
    std::string native_reader;
    if (leaf->type == object_type::enum_type) {
      native_reader = "text=>{const value=" + type_identifier(*leaf) +
          "Names.indexOf(text);if(value<0)throw new Error('Unknown XML enum');return value;}";
    } else if (leaf->is_digest()) {
      native_reader = "text=>srlXmlDigest(text," + std::to_string(leaf->digest_extent) + ')';
    } else { native_reader = "text=>srlXmlScalar(text," + quote(leaf->name) + ")"; }
    const auto checked = "srlXmlCheckedText(text,field.rule,limits,scope,bindings)";
    const std::string reader = "(text,limits,field,scope,bindings)=>" +
        (field.list ? "srlXmlListRead(" + std::string{checked} + ',' + native_reader + ')' :
            '(' + native_reader + ")(" + checked + ')');
    const auto expression = projection ? "value.readValue()" : "value";
    const auto encoded = field.list ? "srlXmlListWrite(" + std::string{expression} + ",item=>" + scalar(*leaf, "item") + ')' : scalar(*leaf, expression);
    const auto qualified = [](const xml::name& name) {
      return "srlXmlTokenName(" + native::quote(name.namespace_uri) + ',' + native::quote(name.local) + ')';
    };
    const auto minimum = source.fixed_extent && !field.list ? source.fixed_extent : field.min_occurs;
    const auto maximum = source.fixed_extent && !field.list ? std::optional<std::size_t>{source.fixed_extent} : field.max_occurs;
    const auto inherited = js_inherited_access(owner, source, field_name, type_identifier);
    const auto access = inherited ? *inherited : field.variant_binding ? js_variant_access(source, field_name) :
        js_variant_callbacks{"value=>value." + field_name(source), "(owner,value)=>{owner." + field_name(source) + "=value;}"};
    line("Object.freeze({native:" + quote(js_native_field_identity(owner, source)) +
        ",created:" + (source.created_version.empty() ? "null" : js_revision_literal(*contract.model.revision, source.created_version)) +
        ",obsolete:" + (source.obsolete_version.empty() ? "null" : js_revision_literal(*contract.model.revision, source.obsolete_version)) +
        ",name:" + qualified(field.qualified_name) + ",local:" + quote(field.qualified_name.local) +
        ",uri:" + quote(field.qualified_name.namespace_uri) + ",mode:" +
        quote(field.attribute ? "attribute" : field.text ? "text" : "child") +
        ",wrapper:" + (field.wrapper ? qualified(*field.wrapper) : "null") +
        ",wrapperLocal:" + (field.wrapper ? quote(field.wrapper->local) : "null") +
        ",wrapperUri:" + (field.wrapper ? quote(field.wrapper->namespace_uri) : "null") +
        ",minimum:" + std::to_string(minimum) + ",maximum:" + (maximum ? std::to_string(*maximum) : "null") +
        ",extent:" + std::to_string(field.list ? 0U : source.fixed_extent) + ",optional:" + (optional ? "true" : "false") +
        ",projection:" + (projection ? "true" : "false") + ",presence:" + (field.presence.explicit_policy ? quote(
            field.presence.policy == presence_policy::required ? "required" :
            field.presence.policy == presence_policy::absent ? "absent" :
            field.presence.policy == presence_policy::nil ? "nil" : "absent_or_nil") : "null") +
        ",map:" + (field.map_binding ? js_map_metadata(*field.map_binding, type_identifier) : "null") +
        ",variant:" + (field.variant_binding ? js_variant_metadata(*field.variant_binding, type_identifier) : "null") +
        ",default:" + (field.fixed_value ? quote(*field.fixed_value) : field.default_value ? quote(*field.default_value) : "null") +
        ",defaultName:" + (field.qname_fixed ? qualified(*field.qname_fixed) : field.qname_default ? qualified(*field.qname_default) : "null") +
        ",rule:" + (leaf->type == object_type::class_type || field.map_binding || field.variant_binding ? "null" : js_scalar_metadata(field, *leaf)) +
        ",repeated:" + (field.repeated ? "true" : "false") + ",get:" + access.get +
        ",set:" + access.set + ",model:" +
        (leaf->type == object_type::class_type && !field.map_binding && !field.variant_binding ? "()=>" + type_identifier(*leaf) + "._srlXmlToken" : "null") +
        ",read:" + (leaf->type == object_type::class_type || field.map_binding || field.variant_binding ? "null" : reader) +
        ",write:" + (leaf->type == object_type::class_type || field.map_binding || field.variant_binding ? "null" :
            "(value,limits,field,scope,bindings)=>srlXmlCheckedText(" + encoded + ",field.rule,limits,scope,bindings)") + "}),");
  }
  line("]),all:Object.freeze([");
  // Store field indexes rather than generated member spellings for namespace-safe content selection.
  const auto group_indexes = [&](const auto& alternatives) {
    std::string indexes{"Object.freeze(["};
    for (const auto* alternative : alternatives) {
      const auto found = std::find_if(contract.fields.begin(), contract.fields.end(),
          [&](const auto& selected) { return selected.source == alternative; });
      indexes += std::to_string(std::distance(contract.fields.begin(), found)) + ',';
    }
    return indexes + "])";
  };
  for (const auto& group : contract.all_groups) {
    line("Object.freeze({fields:" + group_indexes(group.alternatives) + ",minimum:" + std::to_string(group.minimum) + "}),");
  }
  line("]),choices:Object.freeze([");
  for (const auto& group : contract.choices) {
    line("Object.freeze({fields:" + group_indexes(group.alternatives) + ",minimum:" + std::to_string(group.minimum) +
        ",maximum:" + std::to_string(group.maximum) + "}),");
  }
  line("])});");
  line("// Retain only this value's QName vocabulary until the owning object is collected; native codecs ignore it.");
  line("xmlBindNamespace(prefix,uri) { return srlXmlBindNamespace(this,prefix,uri); }");
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
  line("srlXmlProjectionInput(this._srlXmlToken);");
  line("limits = new XmlLimits(limits);");
  line("const value = srlXmlTokenDecode(this._srlXmlToken,[text],limits,true);");
  if (options.runtime_xsd && options.validation_scope != "output") {
    line("if (validator) validator.validate(text);");
  } else if (!options.runtime_xsd) {
    line("if (validator) throw new Error('Runtime XSD was not enabled during generation');");
  }
  line("return value; }");
  line("// Decode byte slices without retaining a document tree or publishing before final EOF.");
  line("static decodeXmlChunks(chunks,limits=new XmlLimits(),validator=null) {");
  line("srlXmlProjectionInput(this._srlXmlToken);");
  line("if(validator)throw new Error('Chunked XML cannot run a whole-document validator');");
  line("return srlXmlTokenDecode(this._srlXmlToken,chunks,new XmlLimits(limits)); }");
  const bool direct_only = contract.model.inherited || contract.mixed || !contract.choices.empty() || !contract.all_groups.empty() ||
      std::any_of(contract.fields.begin(), contract.fields.end(), [](const auto& field) {
        return field.value_projection || field.presence.explicit_policy || field.map_binding || field.variant_binding || field.list || field.qname ||
            field.prefix_list || !field.facets.union_members.empty() || !field.facets.list_item_kind.empty();
      });
  if (direct_only) {
    line("// Advanced mappings require the public token decoder and never fall back to a DOM reader.");
    line("static _srlXmlRead(node) { throw new Error('This XML mapping requires decodeXml or decodeXmlChunks'); }");
  } else {
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
  }
  line("// Write one caller-named element directly from typed native model fields.");
  line("_srlXmlWrite(output,local,uri,variantType=null) { srlXmlTokenWrite(this," + generated_name + "._srlXmlToken,output,local,uri,variantType); }");
  static_cast<void>(owner);
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
