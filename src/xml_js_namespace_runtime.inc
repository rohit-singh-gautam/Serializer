// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsnamespace(
const srlXmlNamespaceState=new WeakMap();
const srlXmlNamespaceRules=new WeakMap();

// Cache whether a complete rule can select a namespace-dependent primitive or list member.
function srlXmlNeedsNamespace(rule) {
  if(srlXmlNamespaceRules.has(rule))return srlXmlNamespaceRules.get(rule);
  const selected=rule.kind==='QName' || rule.listItem==='QName' || rule.prefixList || rule.union.some(srlXmlNeedsNamespace) || rule.itemRule && srlXmlNeedsNamespace(rule.itemRule);
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
function srlXmlRememberNamespaces(owner,field,index,bindings,limits=null) {
  if(!bindings || !bindings.size)return;
  if(limits&&limits.operation){let bytes=128;for(const[prefix,uri]of bindings)bytes+=128+2*(prefix.length+uri.length);limits.operation("allocation",bytes,field.native);}
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
  limits.operationField=field.native;
  if(defaulted && field.defaultScope!==null && field.defaultScope!==undefined){
    if(limits.operation)limits.operation("allocation",128+field.defaultScope.length*128,field.native);
    scope={parent:null,bindings:new Map(field.defaultScope)};
  }
  if(defaulted && field.defaultName!==null && field.defaultName!==undefined){
    const colon=text.indexOf(':'),prefix=colon<0 ? '' : text.slice(0,colon),separator=field.defaultName.indexOf('\0');
    scope={parent:scope,bindings:new Map([[prefix,field.defaultName.slice(0,separator)]])};
  }
  if(defaulted && field.defaultNames!==null && field.defaultNames!==undefined){
    const tokens=srlXmlLexicalTokens(text),schemaBindings=new Map();
    if(tokens.length!==field.defaultNames.length)throw new Error('Invalid generated QName list default');
    for(let index=0;index<tokens.length;++index){
      const colon=tokens[index].indexOf(':'),prefix=colon<0 ? '' : tokens[index].slice(0,colon);
      const name=field.defaultNames[index];schemaBindings.set(prefix,name.slice(0,name.indexOf('\0')));
    }
    scope={parent:scope,bindings:schemaBindings};
  }
  return field.read(text,limits,field,scope,bindings);
}

// Validate authored output in each field's own minimal namespace scope before opening its element head.
function srlXmlPrepareField(owner,field,value,limits,index=0) {
  limits.operationField=field.native;
  const bindings=new Map();bindings.srlXmlTrackSelection=srlXmlNeedsNamespace(field.rule);
  const text=field.write(value,limits,field,srlXmlFieldScope(owner,field,index),bindings);
  if(field.defaultScope!==null && field.defaultScope!==undefined && text==='' && field.default!=='')throw new Error('Empty XML union would activate a different schema default');
  let omitDefault=false;
  if(bindings.srlXmlSelection && text===field.default && (field.mode!=='attribute' || field.minimum===0)){
    const expected=new Map();expected.srlXmlTrackSelection=true;
    srlXmlReadField(field,text,limits,null,expected,true);
    omitDefault=srlXmlUnionConstantEqual(bindings.srlXmlSelection,expected.srlXmlSelection);
  }
  if(omitDefault)bindings.clear();
  return {text,bindings,selection:bindings.srlXmlSelection,omitDefault};
}

// Reject generated presentation aliases that change the first selected leaf or its expanded value identity.
function srlXmlCheckPreparedNamespace(field,scalar,output) {
  if(!scalar.selection || scalar.omitDefault)return;
  let scope=null;for(const bindings of output.prefixes)scope={parent:scope,bindings};
  const checked=new Map();checked.srlXmlTrackSelection=true;
  srlXmlCheckedText(scalar.text,field.rule,output.limits,scope,checked);
  const selected=checked.srlXmlSelection;
  if(selected.rule!==scalar.selection.rule || !srlXmlUnionConstantEqual(selected,scalar.selection)) {
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

// Reconcile conflicting field-private aliases only when the old shared-head projection cannot represent them.
// Native lexical values and their first selected union members remain unchanged.
function srlXmlReconcilePrepared(prepared,bindings,limits) {
  const charge=(resource,amount)=>{if(limits.operation)limits.operation(resource,amount,limits.operationField||'');};
  let conflict=false;
  charge('allocation',128+bindings.size*128);
  const combined=new Map(bindings);
  for(const scalar of prepared.values())for(const [prefix,uri] of scalar.bindings){
    charge('work',prefix.length+uri.length+1);
    if(combined.has(prefix) && combined.get(prefix)!==uri)conflict=true;
    else if(!combined.has(prefix)){charge('allocation',128);combined.set(prefix,uri);}
  }
  if(!conflict){srlXmlMergeBindings(bindings,combined);return;}
  const reserved=new Set();
  for(const scalar of prepared.values()){
    charge('work',scalar.text.length);
    for(const match of scalar.text.matchAll(/(q[0-9]+):/g))if(!reserved.has(match[1])){
      charge('allocation',128+2*match[1].length);reserved.add(match[1]);
    }
  }
  const values=scalar=>Array.isArray(scalar.selection.value) ? scalar.selection.value : [scalar.selection.value];
  const namespaceSelection=selection=>selection && (selection.rule.kind==='QName' ||
    selection.rule.listItem==='QName' || selection.rule.prefixList ||
    selection.items && selection.items.some(namespaceSelection));
  const namespaced=scalar=>!scalar.omitDefault && namespaceSelection(scalar.selection);
  const namespaceNames=selection=>selection.items ? selection.items.flatMap(namespaceNames) :
    selection.rule.kind==='QName' || selection.rule.listItem==='QName' ?
      (Array.isArray(selection.value)?selection.value:[selection.value]) : [];
  for(const scalar of prepared.values())if(namespaced(scalar) && !scalar.selection.rule.prefixList &&
      namespaceNames(scalar.selection).some(name=>name.startsWith('\0'))){
    if(bindings.has('') && bindings.get('')!=='')throw new Error('Opaque default namespace conflicts with QName value');
    bindings.set('','');
  }
  for(const scalar of prepared.values()){
    if(!namespaced(scalar)){srlXmlMergeBindings(bindings,scalar.bindings);continue;}
    const prefixes=scalar.selection.rule.prefixList;
    charge('allocation',128+2*scalar.text.length+values(scalar).length*128);
    const renderName=(name,prefixes)=>{
      charge('work',name.length);
      const separator=name.indexOf('\0'),uri=name.slice(0,separator),local=name.slice(separator+1);
      if(!uri){if(prefixes)throw new Error('XML prefix list requires a namespace');return local;}
      let selected=null;
      for(const [prefix,bound] of bindings)if(bound===uri && (!prefixes || prefix)){selected=prefix;break;}
      if(selected===null){let index=0;do{charge('work',1);selected='q'+index++;}while(bindings.has(selected) || reserved.has(selected));charge('allocation',128+2*(selected.length+uri.length));bindings.set(selected,uri);}
      return selected ? selected+(prefixes ? '' : ':'+local) : local;
    };
    // Atomic union items keep ordinary strings and qualify only selected QName values.
    const render=(selection,text)=>{
      if(selection.items){let index=0;return text.replace(/[^ \t\r\n]+/g,token=>render(selection.items[index++],token));}
      if(selection.rule.kind==='QName')return renderName(selection.value,false);
      if(selection.rule.listItem==='QName' || selection.rule.prefixList)return selection.value.map(name=>renderName(name,selection.rule.prefixList)).join(' ');
      return text;
    };
    scalar.text=render(scalar.selection,scalar.text);
  }
}
)jsnamespace"
