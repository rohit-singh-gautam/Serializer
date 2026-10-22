// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsvariant(
import {NC_NAME_RE as SrlXmlNcName} from 'xmlchars/xmlns/1.0/ed3.js';
const srlXmlTokenTypeName=srlXmlTokenName('http://www.w3.org/2001/XMLSchema-instance','type');

// Resolve one QName through only the active linked provider scopes, including the default namespace.
function srlXmlVariantQName(text,scope) {
  const lexical=srlXmlTrim(text),colon=lexical.indexOf(':');
  if(colon>=0 && lexical.indexOf(':',colon+1)>=0) throw new Error('Invalid XML xsi:type QName');
  const prefix=colon>=0 ? lexical.slice(0,colon) : '',local=lexical.slice(colon+1);
  const valid=value=>{const match=SrlXmlNcName.exec(value);return match!==null && match[0].length===value.length;};
  if(!valid(local) || colon>=0 && !valid(prefix)) throw new Error('Invalid XML xsi:type QName');
  if(prefix==='xml') return srlXmlTokenName('http://www.w3.org/XML/1998/namespace',local);
  for(let current=scope;current;current=current.parent) {
    if(current.bindings.has(prefix)) return srlXmlTokenName(current.bindings.get(prefix),local);
  }
  if(prefix) throw new Error('Undeclared XML xsi:type prefix');
  return srlXmlTokenName('',local);
}

// Keep exactly one selected typed payload private until the complete required wrapper closes.
class SrlXmlVariantFrame {
  constructor(field,name,attributes) {
    if(attributes.size) throw new Error('Unexpected XML variant wrapper attribute');
    this.field=field;this.name=name;this.wrapper=true;this.selected=null;this.value=null;this.hasValue=false;
  }
  // Resolve complete payload tag/type agreement before constructing its scalar or owning model frame.
  child(name,attributes,scope) {
    if(this.selected!==null || attributes.has(srlXmlTokenNilName)) throw new Error('XML variant requires exactly one nonnil payload');
    const type=attributes.has(srlXmlTokenTypeName) ? srlXmlVariantQName(attributes.get(srlXmlTokenTypeName),scope) : null;
    const meta=this.field.variant;
    for(let index=0;index<meta.alternatives.length;++index) {
      const alternative=meta.alternatives[index];
      if(meta.selection==='element' ? name!==alternative.payload.name : type!==alternative.type) continue;
      if(type!==null && type!==alternative.type || meta.selection==='xsi_type' && name!==this.field.wrapper) {
        throw new Error('Conflicting XML variant tag/type identity');
      }
      this.selected=index;
      return [{...alternative.payload,name,variantPayload:true},index,false];
    }
    throw new Error('Unknown or missing XML variant tag/type identity');
  }
  // Install one fully checked payload without changing the unpublished parent's native selector yet.
  accept(index,value) {
    if(this.hasValue) throw new Error('XML variant requires exactly one payload');
    this.value=value;this.hasValue=true;
  }
  // Ignore only XML formatting whitespace between wrapper and payload.
  characters(text) { if(srlXmlTrim(text)!=='') throw new Error('Unexpected XML variant wrapper text'); }
  // Expose one normalized native selection only after wrapper completeness is known.
  finish() {
    if(!this.hasValue) throw new Error('XML variant requires exactly one payload');
    return [this.selected,this.value];
  }
}

// Open a payload with a QName marker; an empty type namespace requires an empty default binding.
function srlXmlVariantStart(output,local,uri,type=null,bindings=null) {
  const defaultUri=bindings ? bindings.get('') || '' : null;
  if(type!==null && type.typeUri==='' && defaultUri)throw new Error('QName default conflicts with empty xsi:type namespace');
  output.open(local,uri,type!==null && type.typeUri==='',defaultUri,bindings);
  if(bindings)for(const [prefix,namespace] of bindings)if(prefix && prefix!=='xml')output.namespace(prefix,namespace);
  if(type===null) return 0;
  let lexical=type.typeLocal;
  if(type.typeUri) {let prefix='srlt';while(bindings && bindings.has(prefix) && bindings.get(prefix)!==type.typeUri)prefix+='t';
    output.namespace(prefix,type.typeUri);lexical=prefix+':'+lexical;}
  output.attribute('type','http://www.w3.org/2001/XMLSchema-instance',lexical,0);
  return 1;
}

// Emit the selected native payload directly with its element tag or complete xsi:type inventory.
function srlXmlVariantWrite(field,selected,output) {
  const meta=field.variant,index=selected[0];
  if(!Number.isInteger(index) || index<0 || index>=meta.alternatives.length) throw new Error('Unknown native XML variant');
  const alternative=meta.alternatives[index],payload=alternative.payload,value=selected[1];
  const typed=meta.selection==='xsi_type',local=typed ? field.wrapperLocal : payload.local;
  const uri=typed ? field.wrapperUri : payload.uri,type=typed ? alternative : null;
  output.open(field.wrapperLocal,field.wrapperUri);output.put('>');
  if(payload.model) (payload.projection ? value.readValue() : value)._srlXmlWrite(output,local,uri,type);
  else {srlXmlVariantStart(output,local,uri,type);output.put('>');output.escaped(payload.write(value),false);output.close(local);}
  output.close(field.wrapperLocal);
}
)jsvariant"
