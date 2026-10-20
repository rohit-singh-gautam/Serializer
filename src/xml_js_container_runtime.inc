// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jscontainer(
// Bound native map entries and the cumulative larger lexical/canonical UTF-8 key representation.
export class XmlMapLimits {
  constructor(values={}) {
    Object.assign(this,{maxEntries:1024,maxKeyBytes:1048576},values);
    for(const key of ['maxEntries','maxKeyBytes']) {
      if(!Number.isSafeInteger(this[key]) || this[key]<0) throw new Error('Invalid XML map limits');
    }
  }
}

// Build only a destination Map and one pending typed entry, never a list of XML entry nodes.
class SrlXmlMapFrame {
  constructor(field,name,attributes,limits) {
    if(attributes.size) throw new Error('Unexpected XML map wrapper attribute');
    this.field=field;this.name=name;this.wrapper=true;this.values=new Map();this.keyBytes=0;
    this.bounds=limits.map;const meta=field.map;
    const key={...meta.key,get:value=>value.key,set:(owner,value)=>{owner.key=value;},read:text=>{
      const value=meta.key.read(text),canonical=meta.key.write(value);
      const bytes=Math.max(srlXmlUtf8Length(text),srlXmlUtf8Length(canonical));
      if(bytes>this.bounds.maxKeyBytes-this.keyBytes) throw srlXmlQuotaError('XML map key-byte limit');
      return {value,bytes};
    }};
    const value={...meta.value,get:value=>value.value,set:(owner,value)=>{owner.value=value;}};
    this.entry={name:meta.entry,wrapper:null,presence:null,model:()=>({
      create:()=>Object.create(null),fields:[key,value],all:[],choices:[],
      mixed:null,identity:null,extensions:null,model:Object.freeze({revision:null,magic:null})
    })};
  }
  // Reject excess entries before allocating the next pending key/value pair.
  child(name) {
    if(name!==this.field.map.entry) throw new Error('Unexpected XML map entry');
    if(this.values.size>=this.bounds.maxEntries) throw srlXmlQuotaError('XML map entry limit');
    return [this.entry,null,false];
  }
  // Commit one complete entry after exact native-key duplicate and cumulative byte validation.
  accept(index,entry) {
    if(this.values.has(entry.key.value)) throw new Error('Duplicate XML map key');
    this.keyBytes+=entry.key.bytes;this.values.set(entry.key.value,entry.value);
  }
  // Collection indentation is insignificant; mixed content is never accepted by this contract.
  characters(text) { if(srlXmlTrim(text)!=='') throw new Error('Unexpected XML map wrapper text'); }
  // Return the unpublished native container only after its complete owning wrapper closes.
  finish() { return this.values; }
}

// Write native entries in iteration order through their exact statically checked key/value codecs.
function srlXmlMapWrite(field,value,output) {
  const meta=field.map,bounds=output.limits.map;
  if(!(value instanceof Map)) throw new Error('XML map entry limit or native type');
  if(value.size>bounds.maxEntries) throw srlXmlQuotaError('XML map entry limit or native type');
  output.open(field.wrapperLocal,field.wrapperUri);output.put('>');
  let count=0,keyBytes=0;const selectedKeys=meta.key.projection ? new Set() : null;
  for(const [key,item] of value) {
    if(++count>bounds.maxEntries) throw srlXmlQuotaError('XML map entry limit');
    const text=meta.key.write(key),size=srlXmlUtf8Length(text);
    if(selectedKeys){const selected=meta.key.read(text);if(selectedKeys.has(selected))throw new Error('Duplicate XML projected map key');selectedKeys.add(selected);}
    if(size>bounds.maxKeyBytes-keyBytes) throw srlXmlQuotaError('XML map key-byte limit');
    keyBytes+=size;output.open(meta.entryLocal,meta.entryUri);
    if(meta.key.mode==='attribute') output.attribute(meta.key.local,meta.key.uri,text,0);
    output.put('>');
    if(meta.key.mode!=='attribute') output.child(meta.key.local,meta.key.uri,text);
    if(meta.value.model) (meta.value.projection ? item.readValue() : item)._srlXmlWrite(output,meta.value.local,meta.value.uri);
    else output.child(meta.value.local,meta.value.uri,meta.value.write(item));
    output.close(meta.entryLocal);
  }
  output.close(field.wrapperLocal);
}
)jscontainer"
