// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsnamespace(
const srlXmlNamespaceState=new WeakMap();
const srlXmlNamespaceRules=new WeakMap();

// Cache whether a complete rule can select a namespace-dependent primitive or list member.
function srlXmlNeedsNamespace(rule) {
  if(srlXmlNamespaceRules.has(rule))return srlXmlNamespaceRules.get(rule);
  const selected=rule.kind==='QName' || rule.listItem==='QName' || rule.prefixList || rule.union.some(srlXmlNeedsNamespace);
  srlXmlNamespaceRules.set(rule,selected);return selected;
}

// Recognize complete XML names using the pinned provider's public Unicode character rules.
function srlXmlCompleteName(expression,value) {
  const match=expression.exec(value);return match!==null && match[0].length===value.length;
}

// Resolve one lexical QName and retain only the declaration needed by this value.
function srlXmlScalarQName(text,scope,bindings=null) {
  const colon=text.indexOf(':'),prefix=colon<0 ? '' : text.slice(0,colon),local=text.slice(colon+1);
  if(!srlXmlCompleteName(SrlXmlNcName,local) || colon>=0 && (!srlXmlCompleteName(SrlXmlNcName,prefix) || text.indexOf(':',colon+1)>=0)) {
    throw new SrlXmlLexicalError('Invalid XML QName scalar');
  }
  let uri=prefix==='xml' ? 'http://www.w3.org/XML/1998/namespace' : null;
  for(let current=scope;uri===null && current;current=current.parent) {
    if(current.bindings.has(prefix))uri=current.bindings.get(prefix);
  }
  if(uri===null){if(prefix)throw new SrlXmlLexicalError('Undeclared XML QName prefix');uri='';}
  if(bindings!==null)bindings.set(prefix,uri);
  return srlXmlTokenName(uri,local);
}

// Persist only used field declarations, avoiding provider scopes or retained known-element objects.
function srlXmlRememberNamespaces(owner,field,index,bindings) {
  if(!bindings || !bindings.size)return;
  let state=srlXmlNamespaceState.get(owner);
  if(!state){state={authored:new Map(),fields:new Map()};srlXmlNamespaceState.set(owner,state);}
  let values=state.fields.get(field.native);
  if(!values){values=[];state.fields.set(field.native,values);}
  values[index]=new Map(bindings);
}

// Bind lexical QName/prefix-list vocabulary in XML-only state without changing native codecs.
function srlXmlBindNamespace(owner,prefix,uri) {
  if(typeof prefix!=='string' || typeof uri!=='string' || prefix && !srlXmlCompleteName(SrlXmlNcName,prefix) ||
      prefix==='xmlns' || prefix==='xml' && uri!=='http://www.w3.org/XML/1998/namespace' ||
      prefix!=='xml' && uri==='http://www.w3.org/XML/1998/namespace' || uri==='http://www.w3.org/2000/xmlns/' ||
      prefix && !uri || uri.trim()!==uri)throw new Error('Invalid XML namespace binding');
  srlXmlCharacters(uri);
  let state=srlXmlNamespaceState.get(owner);
  if(!state){state={authored:new Map(),fields:new Map()};srlXmlNamespaceState.set(owner,state);}
  state.authored.set(prefix,uri);return owner;
}

// Reconstruct a value's minimal presentation scope, allowing explicit authored bindings to override saved ones.
function srlXmlFieldScope(owner,field,index) {
  const state=srlXmlNamespaceState.get(owner);
  if(!state)return null;
  const values=state.fields.get(field.native),saved=values && values[index];
  return {parent:saved ? {parent:null,bindings:saved} : null,bindings:state.authored};
}

// Resolve schema-default QName spellings in schema scope rather than the input document's vocabulary.
function srlXmlReadField(field,text,limits,scope,bindings,defaulted=false) {
  if(defaulted && field.defaultName!==null && field.defaultName!==undefined){
    const colon=text.indexOf(':'),prefix=colon<0 ? '' : text.slice(0,colon),separator=field.defaultName.indexOf('\0');
    scope={parent:scope,bindings:new Map([[prefix,field.defaultName.slice(0,separator)]])};
  }
  return field.read(text,limits,field,scope,bindings);
}

// Validate authored output in each field's own minimal namespace scope before opening its element head.
function srlXmlPrepareField(owner,field,value,limits,index=0) {
  const bindings=new Map();bindings.srlXmlTrackSelection=srlXmlNeedsNamespace(field.rule);
  const text=field.write(value,limits,field,srlXmlFieldScope(owner,field,index),bindings);
  return {text,bindings,selection:bindings.srlXmlSelection};
}

// Reject generated presentation aliases that change the first selected leaf or its expanded value identity.
function srlXmlCheckPreparedNamespace(field,scalar,output) {
  if(!scalar.selection)return;
  let scope=null;for(const bindings of output.prefixes)scope={parent:scope,bindings};
  const checked=new Map();checked.srlXmlTrackSelection=true;
  srlXmlCheckedText(scalar.text,field.rule,output.limits,scope,checked);
  const selected=checked.srlXmlSelection;
  if(selected.rule!==scalar.selection.rule || !srlXmlScalarEqual(selected.value,scalar.selection.value)) {
    throw new Error('XML scalar namespace presentation changes its selected member or value identity');
  }
}

// Merge declarations needed by attributes/simple content; incompatible authored vocabularies fail atomically.
function srlXmlMergeBindings(target,source) {
  for(const [prefix,uri] of source){
    if(target.has(prefix) && target.get(prefix)!==uri)throw new Error('Conflicting XML scalar namespace bindings');
    target.set(prefix,uri);
  }
}
)jsnamespace"
