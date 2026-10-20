// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsextension(
const srlXmlExtensionState=new WeakMap();
const srlXmlExtensionMetadataBytes=8388608;

// Bound positioned foreign roots and logical retained semantic bytes independently of known model storage.
export class XmlExtensionLimits {
  constructor(values={}) {
    Object.assign(this,{maxSlots:128,maxItems:100000,maxPreservedBytes:8388608},values);
    for(const key of ['maxSlots','maxItems','maxPreservedBytes'])if(!Number.isSafeInteger(this[key]) || this[key]<0)throw new Error('Invalid XML extension limits');
  }
}

// Declare semantic understanding separately from opaque preservation; own the URI inventory for this policy.
export class XmlExtensionOptions {
  constructor(values={}) {
    if(values===null || typeof values!=='object')throw new Error('Invalid XML extension options');
    this.limits=new XmlExtensionLimits(values.limits || {});
    const understood=values.understoodNamespaces ?? [],required=values.requiredPrefixes ?? '';
    if(!Array.isArray(understood) || understood.length>this.limits.maxItems || typeof required!=='string')throw new Error('Invalid XML extension capability inventory');
    let bytes=0;
    for(const value of [...understood,required]){
      if(typeof value!=='string')throw new Error('XML capabilities require URI and prefix text');
      bytes+=srlXmlUtf8Length(value);if(bytes>srlXmlExtensionMetadataBytes)throw srlXmlQuotaError('XML capability metadata byte limit');srlXmlCharacters(value);
    }
    this.understoodNamespaces=Object.freeze([...understood]);this.requiredPrefixes=required;
  }
}

// Flatten only provider namespace declarations, preserving nearest shadowing and the implicit XML binding.
function srlXmlExtensionScope(scope) {
  if(scope instanceof Map)return scope;
  const scopes=[],result=new Map([['xml','http://www.w3.org/XML/1998/namespace']]);
  for(let current=scope;current;current=current.parent)scopes.push(current.bindings);
  for(let index=scopes.length-1;index>=0;--index)for(const item of scopes[index])result.set(...item);
  return result;
}

// Validate opaque names without ever interpreting unknown vocabulary as a known model field.
function srlXmlExtensionName(name) {
  if(typeof name!=='string')throw new Error('Invalid XML extension name');
  const split=name.indexOf('\0'),uri=name.slice(0,split),local=name.slice(split+1);
  if(split<0 || !srlXmlCompleteName(SrlXmlNcName,local) || uri==='http://www.w3.org/2000/xmlns/')throw new Error('Invalid XML extension expanded name');
  srlXmlCharacters(uri);return [uri,local];
}

// Reject reserved/invalid edited namespace context before opaque QName meanings can be emitted.
function srlXmlExtensionBindings(bindings) {
  if(!(bindings instanceof Map))throw new Error('Invalid XML extension namespace context');
  for(const [prefix,uri] of bindings){
    if(typeof prefix!=='string' || typeof uri!=='string' || prefix && !srlXmlCompleteName(SrlXmlNcName,prefix) ||
        prefix==='xmlns' || (prefix==='xml')!==(uri==='http://www.w3.org/XML/1998/namespace') ||
        prefix && !uri || uri==='http://www.w3.org/2000/xmlns/' || uri.trim()!==uri)throw new Error('Invalid XML extension namespace binding');
    srlXmlCharacters(uri);
  }
}

// Enforce one owner-local slot inventory and retain only explicitly preserved foreign payloads.
class SrlXmlExtensions {
  constructor(meta,scope,options) {
    this.meta=meta;this.scope=scope;this.options=options;this.bounds=options.limits;
    if(meta.slots.length>this.bounds.maxSlots || meta.known.length>this.bounds.maxItems || meta.required.length>this.bounds.maxItems)throw srlXmlQuotaError('XML extension slot limit');
    this.counts=meta.slots.map(()=>0);this.items=0;this.bytes=0;this.state=null;this.known=new Set(meta.known);
    const understood=new Set(options.understoodNamespaces),required=[...meta.required];let count=0;
    for(const matched of options.requiredPrefixes.matchAll(/[^ \t\r\n]+/g)){
      if(count>=this.bounds.maxItems)throw srlXmlQuotaError('XML required-vocabulary item limit');++count;
      const prefix=matched[0];if(prefix==='xmlns' || !srlXmlCompleteName(SrlXmlNcName,prefix))throw new Error('Invalid required XML capability prefix');
      const name=srlXmlScalarQName(prefix+':item',scope),uri=name.slice(0,name.indexOf('\0'));
      if(!uri)throw new Error('Unbound required XML capability prefix');required.push(uri);
    }
    for(const uri of required)if(uri!==meta.core && !understood.has(uri))throw new Error('Required XML capability is not understood');
  }
  // Admit logical retained bytes before corresponding owning payload storage is created.
  charge(size) {
    if(size>this.bounds.maxPreservedBytes-this.bytes)throw srlXmlQuotaError('XML extension preserved-byte limit');this.bytes+=size;
  }
  // Locate an exact foreign URI/field boundary; core and statically known expanded names remain closed.
  slot(name,attribute,position=null) {
    const uri=name.slice(0,name.indexOf('\0'));
    if(!uri || uri===this.meta.core || uri==='urn:serializer:xml:model' || this.known.has(name))return -1;
    return this.meta.slots.findIndex(slot=>slot.attribute===attribute && (attribute || slot.position===position) && slot.namespaces.includes(uri));
  }
  // Enforce policy, root-item admission and occurrence counts before traversing an unknown owner item.
  admit(name,attribute,position=null) {
    const index=this.slot(name,attribute,position),slot=this.meta.slots[index];
    if(index<0 || slot.policy==='reject')throw new Error('XML extension has no permitted slot');
    if(this.items>=this.bounds.maxItems)throw srlXmlQuotaError('XML extension item/occurrence limit');
    if(slot.maximum!==null && this.counts[index]>=slot.maximum)throw new Error('XML extension item/occurrence limit');
    ++this.items;++this.counts[index];return slot.policy==='preserve';
  }
  // Account one complete namespace context, including declarations needed only by opaque lexical values.
  scopeSize(scope) {
    srlXmlExtensionBindings(scope);let size=0;
    for(const [prefix,uri] of scope)size+=64+srlXmlUtf8Length(prefix)+srlXmlUtf8Length(uri);return size;
  }
  // Create XML-only owner storage lazily; a skip-only traversal allocates no preserved state.
  storage() {
    if(this.state===null){const scope=srlXmlExtensionScope(this.scope);this.charge(64+this.scopeSize(scope));
      this.state={scope:new Map(scope),attributes:new Map(),children:[]};}
    return this.state;
  }
  // Preserve only attributes admitted by their explicit foreign-vocabulary slot.
  attribute(name,value) {
    if(this.admit(name,true)){this.charge(48+srlXmlUtf8Length(name)+srlXmlUtf8Length(value));this.storage().attributes.set(name,value);}
  }
  // Attach a completely captured foreign root at its original static-field/repeated-child boundary.
  child(position,item,node) {
    if(node!==null){this.charge(24);this.storage().children.push({position,item,node});}
  }
  // Measure one foreign node before copying provider maps into owning extension storage.
  measure(name,attributes,scope) {
    srlXmlExtensionName(name);const bindings=srlXmlExtensionScope(scope);let size=64+srlXmlUtf8Length(name)+this.scopeSize(bindings);
    for(const [key,value] of attributes){srlXmlExtensionName(key);if(typeof value!=='string')throw new Error('Invalid XML extension attribute value');
      srlXmlCharacters(value);size+=48+srlXmlUtf8Length(key)+srlXmlUtf8Length(value);}
    this.charge(size);return bindings;
  }
  // Copy one bounded foreign element, never a statically known model member.
  node(name,attributes,scope) {
    const bindings=this.measure(name,attributes,scope);return {name,attributes:new Map(attributes),scope:new Map(bindings),content:[]};
  }
  // Require all declared minima before publishing the owning candidate or encoded bytes.
  finish() {
    for(let index=0;index<this.counts.length;++index)if(this.counts[index]<this.meta.slots[index].minimum)throw new Error('Required XML extension occurrence missing');
  }
}

// Capture one permitted foreign subtree, or discard skip payloads while retaining only parser frames.
class SrlXmlForeignFrame {
  constructor(name,attributes,scope,tracker,preserve) {
    this.name=name;this.scope=scope;this.tracker=tracker;this.preserve=preserve;this.wrapper=false;this.identity=null;
    this.value=preserve ? tracker.node(name,attributes,scope) : null;this.pending='';
  }
  // Finish only semantic text runs so provider byte/chunk boundaries do not change preserved storage.
  flush() {
    if(this.pending.length){this.tracker.charge(24);this.value.content.push(this.pending);this.pending='';}
  }
  // Descendants stay opaque within the already admitted foreign root.
  child() {
    if(this.preserve)this.flush();return [{foreign:{tracker:this.tracker,preserve:this.preserve}},null,false];
  }
  // Retain only explicitly preserved text; skipped characters remain transient parser data.
  characters(value) {
    if(this.preserve){this.tracker.charge(srlXmlUtf8Length(value));this.pending+=value;}
  }
  // Attach a completed foreign descendant without touching known owning fields.
  accept(index,value) {
    if(this.preserve){this.tracker.charge(24);this.value.content.push(value);}
  }
  // Return one complete semantic foreign node, or no retained value for skip.
  finish() {
    if(this.preserve)this.flush();return this.value;
  }
}

)jsextension"
R"jsextension(
// Borrow and revalidate XML-only owner state before any known output head is opened.
function srlXmlExtensionOutput(owner,meta,output) {
  if(!meta)return null;
  let inherited=null;for(const bindings of output.prefixes)inherited={parent:inherited,bindings};
  const state=srlXmlExtensionState.get(owner) || null,scope=new Map(srlXmlExtensionScope(inherited));
  if(state)for(const item of state.scope)scope.set(...item);
  const authored=srlXmlNamespaceState.get(owner);
  if(authored)for(const [prefix,uri] of authored.authored){
    if(state && scope.has(prefix) && scope.get(prefix)!==uri)throw new Error('Authored namespace conflicts with preserved XML vocabulary');scope.set(prefix,uri);
  }
  const tracker=new SrlXmlExtensions(meta,{parent:null,bindings:scope},output.limits.extensions);
  if(state){
    tracker.charge(64+tracker.scopeSize(state.scope));
    for(const [name,value] of state.attributes){
      srlXmlExtensionName(name);srlXmlCharacters(value);
      if(!tracker.admit(name,true))throw new Error('Preserved XML attribute no longer has a preserve slot');
      tracker.charge(48+srlXmlUtf8Length(name)+srlXmlUtf8Length(value));
    }
    let position=-1,item=-1;
    for(const entry of state.children){
      if(!Number.isSafeInteger(entry.position) || !Number.isSafeInteger(entry.item) || entry.position<0 || entry.item<0 || entry.position>meta.fieldCount ||
          entry.position<position || entry.position===position && entry.item<item)throw new Error('Invalid preserved XML extension order');
      position=entry.position;item=entry.item;
    }
  }
  return {tracker,state,bindings:scope,cursor:0};
}

// Revalidate and emit one borrowed opaque subtree; all known values continue to use generated typed callbacks.
function srlXmlExtensionEmit(output,tracker,node) {
  if(!node || !(node.attributes instanceof Map) || !(node.scope instanceof Map) || !Array.isArray(node.content))throw new Error('Invalid preserved XML extension node');
  tracker.measure(node.name,node.attributes,node.scope);const [uri,local]=srlXmlExtensionName(node.name);
  srlXmlVariantStart(output,local,uri,null,node.scope);let attribute=0;
  for(const [name,value] of node.attributes){const [uri,local]=srlXmlExtensionName(name);output.attribute(local,uri,value,attribute++);}
  output.put('>');
  for(const value of node.content){
    tracker.charge(24);
    if(typeof value==='string'){srlXmlCharacters(value);tracker.charge(srlXmlUtf8Length(value));output.escaped(value,false);}
    else srlXmlExtensionEmit(output,tracker,value);
  }
  output.close(local);
}

// Emit only extensions belonging to the current exact known field and repeated-item boundary.
function srlXmlExtensionAt(extension,output,position,item=0) {
  if(!extension || !extension.state)return;
  const {tracker,state}=extension;
  while(extension.cursor<state.children.length){
    const entry=state.children[extension.cursor];
    if(entry.position<position || entry.position===position && entry.item<item)throw new Error('Preserved XML extension no longer fits edited field position');
    if(entry.position!==position || entry.item!==item)return;
    if(!tracker.admit(entry.node.name,false,position))throw new Error('Preserved XML child no longer has a preserve slot');
    tracker.charge(24);srlXmlExtensionEmit(output,tracker,entry.node);++extension.cursor;
  }
}

// Reject retained boundaries that no longer exist after the owning native collection was edited.
function srlXmlExtensionPositionEnd(extension,position) {
  if(extension && extension.state && extension.cursor<extension.state.children.length && extension.state.children[extension.cursor].position===position) {
    throw new Error('Preserved XML extension no longer fits edited field position');
  }
}
)jsextension"
