// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jslifecycle(
const srlXmlModelNamespace='urn:serializer:xml:model';
const srlXmlModelPrefix=srlXmlModelNamespace+'\0';
const srlXmlModelRevisionName=srlXmlModelPrefix+'revision',srlXmlModelMagicName=srlXmlModelPrefix+'magic';
const srlXmlDigestMaxBytes=1048576;

// Parse the shared canonical revision with exact integers/dotted components and native IEEE precision.
function srlXmlModelRevision(lexical,kind) {
  if(typeof lexical!=='string')throw new Error('XML revision requires text');
  if(kind.startsWith('version')){srlVersionParts(lexical,Number(kind.slice(-1)));return lexical;}
  lexical=srlXmlTrim(lexical);
  if(!/^(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?$/.test(lexical))throw new Error('Invalid canonical XML revision');
  if(kind.startsWith('uint')){
    if(!/^(0|[1-9][0-9]*)$/.test(lexical))throw new Error('Invalid XML integer revision');return srlXmlScalar(lexical,kind);
  }
  const value=srlXmlScalar(lexical,kind);
  if(!Number.isFinite(value) || value===0 && /[1-9]/.test(lexical.split(/[eE]/)[0]))throw new Error('XML floating revision overflows or underflows native width');
  return value;
}

// Compare native revisions without floating conversion of integers or lexical ordering of dotted components.
function srlXmlModelCompare(left,right,kind) {
  return kind.startsWith('version') ? srlVersionCompare(left,right,Number(kind.slice(-1))) : left<right ? -1 : left>right ? 1 : 0;
}

// Select one supported historical/current layout before native construction or XML output is published.
function srlXmlModelSelect(meta,lexical) {
  const revision=meta.model.revision;
  if(!revision){if(lexical!==null)throw new Error('Unversioned XML model has revision metadata');return {value:null,active:meta.fields.map(()=>true)};}
  if(lexical===null)throw new Error('XML revision metadata is required');
  const value=srlXmlModelRevision(lexical,revision.kind),compare=bound=>srlXmlModelCompare(value,bound,revision.kind);
  if(compare(revision.minimum)<0 || compare(revision.current)>0)throw new Error('XML revision outside accepted historical/current interval');
  return {value,active:meta.fields.map(field=>(field.created===null || compare(field.created)>=0) && (field.obsolete===null || compare(field.obsolete)<0))};
}

// Check immutable reserved metadata before constructing a candidate; ordinary namespace attributes remain intact.
function srlXmlModelInput(meta,attributes) {
  for(const key of attributes.keys())if(key.startsWith(srlXmlModelPrefix) && key!==srlXmlModelRevisionName && key!==srlXmlModelMagicName)throw new Error('Unknown reserved XML model metadata');
  const magic=attributes.has(srlXmlModelMagicName) ? attributes.get(srlXmlModelMagicName) : null;
  if(magic!==meta.model.magic)throw new Error('XML immutable model magic missing or incorrect');
  const selected=srlXmlModelSelect(meta,attributes.has(srlXmlModelRevisionName) ? attributes.get(srlXmlModelRevisionName) : null);
  if(attributes.has(srlXmlModelRevisionName) || attributes.has(srlXmlModelMagicName)){
    attributes=new Map(attributes);attributes.delete(srlXmlModelRevisionName);attributes.delete(srlXmlModelMagicName);
  }
  return {...selected,attributes};
}

// Read the existing native discriminator and stage exact expanded metadata without mutating the model.
function srlXmlModelOutput(owner,meta) {
  const revision=meta.model.revision,attributes=new Map();let lexical=null;
  if(revision){const value=revision.get(owner);lexical=revision.kind.startsWith('version') ? value : srlXmlScalar(value,revision.kind,true);attributes.set(srlXmlModelRevisionName,lexical);}
  const selected=srlXmlModelSelect(meta,lexical);
  if(meta.model.magic!==null)attributes.set(srlXmlModelMagicName,meta.model.magic);
  return {...selected,attributes};
}

// Project exact owning bytes as hex without hashing, whitespace acceptance or lossy native conversion.
function srlXmlDigest(value,extent,encoding=false) {
  if(encoding){
    if(!Array.isArray(value) || value.length>srlXmlDigestMaxBytes || extent && value.length!==extent)throw new Error('XML digest extent or byte bound');
    let result='';for(const byte of value){if(!Number.isInteger(byte) || byte<0 || byte>255)throw new Error('XML digest requires exact byte values');result+=byte.toString(16).padStart(2,'0');}return result;
  }
  if(typeof value!=='string' || value.length%2 || value.length/2>srlXmlDigestMaxBytes || extent && value.length/2!==extent || /[^0-9a-fA-F]/.test(value))throw new Error('XML digest requires bounded exact hexadecimal byte pairs');
  const result=new Array(value.length/2);for(let index=0;index<result.length;++index)result[index]=Number.parseInt(value.slice(index*2,index*2+2),16);return result;
}
)jslifecycle"
