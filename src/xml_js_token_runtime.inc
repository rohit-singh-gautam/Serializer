// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jstoken(
import {SaxesParser as SrlXmlSaxes} from 'saxes';

// Frame lookup uses a separator forbidden in XML names and namespace text.
function srlXmlTokenName(uri,local) { return uri+'\0'+local; }
const srlXmlTokenNil=Symbol('XML nil');
const srlXmlTokenNilName=srlXmlTokenName('http://www.w3.org/2001/XMLSchema-instance','nil');

// Resolve authored optional layers once; empty owning collections remain present XML wrappers.
function srlXmlTokenSelection(owner,field) {
  let value=field.get(owner);
  if (field.optional) {
    if (!Array.isArray(value) || value.length>1) throw new Error('Invalid XML optional');
    if (!value.length) return {present:field.presence==='nil',nil:field.presence==='nil',values:[]};
    value=value[0];
    if (field.presence==='absent_or_nil') {
      if (!Array.isArray(value) || value.length>1) throw new Error('Invalid XML optional');
      if (!value.length) return {present:true,nil:true,values:[]};
      value=value[0];
    }
  }
  const values=field.repeated ? value : [value];
  if (!Array.isArray(values) || values.length<field.minimum ||
      field.maximum!==null && values.length>field.maximum || field.extent && values.length!==field.extent) {
    throw new Error('XML field cardinality');
  }
  return {present:true,nil:false,values};
}

// Traverse typed owning members directly through lexical callbacks; no intermediate XML tree is built.
function srlXmlTokenWrite(owner,meta,output,local,uri,variantType=null) {
  const model=srlXmlModelOutput(owner,meta);
  const selected=meta.fields.map((field,index)=>model.active[index] ? srlXmlTokenSelection(owner,field) : {present:false,nil:false,values:[]});
  for (const choice of meta.choices) {
    const count=choice.fields.reduce((total,index)=>total+(selected[index].present ? 1 : 0),0);
    if (count<choice.minimum || count>choice.maximum) throw new Error('XML choice cardinality');
  }
  const identity=meta.identity ? new SrlXmlIdentity(meta.identity,output.limits.identity) : null;
  if(identity)output.identities.push(identity);
  const extension=srlXmlExtensionOutput(owner,meta.extensions,output);
  const prepared=new Map(),bindings=new Map(extension ? extension.bindings : []);
  for(let index=0;index<meta.fields.length;++index){
    const field=meta.fields[index],item=selected[index];
    if(item.present && (field.mode==='attribute' || field.mode==='text')){
      const scalar=srlXmlPrepareField(owner,field,item.values[0],output.limits);
      prepared.set(index,scalar);srlXmlMergeBindings(bindings,scalar.bindings);
    }
  }
  let attributeIndex=srlXmlVariantStart(output,local,uri,variantType,bindings);
  for(const [name,value] of model.attributes)output.attribute(name.slice(name.indexOf('\0')+1),srlXmlModelNamespace,value,attributeIndex++);
  for (let index=0;index<meta.fields.length;++index) {
    const field=meta.fields[index],item=selected[index];
    if (field.mode==='attribute' && item.present) {
      if(variantType!==null && field.name===srlXmlTokenTypeName) throw new Error('XML variant type metadata conflicts with mapped attribute');
      output.attribute(field.local,field.uri,prepared.get(index).text,attributeIndex++);
    }
  }
  if(extension && extension.state)for(const [name,value] of extension.state.attributes){
    const [uri,local]=srlXmlExtensionName(name);output.attribute(local,uri,value,attributeIndex++);
  }
  // All generated attribute/type aliases now exist; compare against the scope actually being published.
  for(const [index,scalar] of prepared)srlXmlCheckPreparedNamespace(meta.fields[index],scalar,output);
  output.put('>');
  if(meta.mixed)srlXmlMixedWrite(owner,meta.mixed,output);
  for (let index=0;index<meta.fields.length;++index) {
    const field=meta.fields[index],item=selected[index];
    srlXmlExtensionAt(extension,output,index);
    if (field.mode==='attribute' || !item.present) {srlXmlExtensionPositionEnd(extension,index);continue;}
    if(field.map) {srlXmlMapWrite(field,item.values[0],output);srlXmlExtensionPositionEnd(extension,index);continue;}
    if(field.variant) {srlXmlVariantWrite(field,item.values[0],output);srlXmlExtensionPositionEnd(extension,index);continue;}
    const local=field.wrapper!==null ? field.wrapperLocal : field.local;
    const uri=field.wrapper!==null ? field.wrapperUri : field.uri;
    if (item.nil) {
      output.open(local,uri);output.attribute('nil','http://www.w3.org/2001/XMLSchema-instance','true',0);
      output.put('>');output.close(local);srlXmlExtensionPositionEnd(extension,index);continue;
    }
    if (field.wrapper!==null) {output.open(local,uri);output.put('>');}
    for (let itemIndex=0;itemIndex<item.values.length;++itemIndex) {
      if(field.repeated && field.wrapper===null)srlXmlExtensionAt(extension,output,index,itemIndex);
      const value=item.values[itemIndex];
      if (field.mode==='text') output.escaped(prepared.get(index).text,false);
      else if (field.model) (field.projection ? value.readValue() : value)._srlXmlWrite(output,field.local,field.uri);
      else {const scalar=srlXmlPrepareField(owner,field,value,output.limits,itemIndex);
        srlXmlVariantStart(output,field.local,field.uri,null,scalar.bindings);
        srlXmlCheckPreparedNamespace(field,scalar,output);output.put('>');
        output.escaped(scalar.text,false);output.close(field.local);}
    }
    if (field.wrapper!==null) output.close(local);
    if(field.repeated && field.wrapper===null)srlXmlExtensionAt(extension,output,index,item.values.length);
    srlXmlExtensionPositionEnd(extension,index);
  }
  srlXmlExtensionAt(extension,output,meta.fields.length);srlXmlExtensionPositionEnd(extension,meta.fields.length);
  if(extension)extension.tracker.finish();
  output.close(local);
  if(identity){output.identities.pop();identity.finish();}
}

)jstoken"
R"jstoken(

// Hold one unpublished model, collection wrapper or scalar lexical value, never XML siblings.
class SrlXmlTokenFrame {
  constructor(meta,name,attrs,item=null,wrapper=false,limits=null,scope=null) {
    this.limits=limits;this.scope=scope;this.bindings=wrapper ? [] : new Map();
    if(item && item.variantPayload && attrs.has(srlXmlTokenTypeName)) {attrs=new Map(attrs);attrs.delete(srlXmlTokenTypeName);}
    this.nil=false;
    if (attrs.has(srlXmlTokenNilName)) {
      const policy=item && (wrapper || item.wrapper===null) ? item.presence : null;
      if (policy!=='nil' && policy!=='absent_or_nil') throw new Error('XML xsi:nil is not permitted by presence policy');
      const value=srlXmlTrim(attrs.get(srlXmlTokenNilName));
      if (!['true','1','false','0'].includes(value)) throw new Error('Invalid XML xsi:nil boolean value');
      attrs=new Map(attrs);attrs.delete(srlXmlTokenNilName);
      this.nil=value==='true' || value==='1';if(this.nil)meta=null;
    }
    this.meta=meta; this.name=name; this.item=item; this.wrapper=wrapper;
    const model=meta ? srlXmlModelInput(meta,attrs) : null;
    if(model)attrs=model.attributes;
    this.active=model ? model.active : [];
    this.identity=meta && meta.identity ? new SrlXmlIdentity(meta.identity,limits.identity) : null;
    this.extensions=meta && meta.extensions ? new SrlXmlExtensions(meta.extensions,scope,limits.extensions) : null;
    this.value=meta ? meta.create() : null; this.fields=meta ? meta.fields : [];
    if(meta && meta.model.revision)meta.model.revision.set(this.value,model.value);
    this.mixed=meta ? meta.mixed : null;this.mixedText='';this.mixedTextBytes=0;
    if(this.mixed)this.mixed.set(this.value,[]);
    this.counts=new Array(this.fields.length).fill(0); this.position=0;
    this.text=''; this.values=[];
    if (!meta) {
      if (attrs.size) throw new Error('XML scalar/wrapper contains unexpected attributes');
      return;
    }
    const known=new Set();
    for (let index=0;index<this.fields.length;++index) {
      const field=this.fields[index];
      if(!this.active[index])continue;
      if (field.repeated && !field.optional) field.set(this.value,[]);
      if (field.mode!=='attribute') continue;
      known.add(field.name);
      if(!attrs.has(field.name) && field.minimum) throw new Error('Required XML attribute missing');
      const lexical=attrs.has(field.name) ? attrs.get(field.name) : field.default;
      if (lexical===null || lexical===undefined) {
        if (!field.optional) throw new Error('Required XML attribute missing');
      } else {const bindings=new Map();this.store(index,srlXmlReadField(field,lexical,limits,scope,bindings,!attrs.has(field.name)),false,bindings);}
    }
    for (const [key,value] of attrs) if (!known.has(key)) {
      if(!this.extensions)throw new Error('Unexpected XML attribute');this.extensions.attribute(key,value);
    }
  }
  // Resolve one permitted opaque root at an exact static-field/repeated-item boundary before traversing it.
  foreign(name,position,item=0) {
    if(!this.extensions || this.extensions.slot(name,false,position)<0)return null;
    const preserve=this.extensions.admit(name,false,position);
    return [{foreign:{tracker:this.extensions,preserve}},{extension:true,position,item},false];
  }
  // Select one statically bound field in sequence and reject unknown or duplicate siblings immediately.
  child(name) {
    if(this.mixed){
      srlXmlMixedFlush(this);const child=this.mixed.children.find(child=>child.payload.name===name);
      if(!child)throw new Error('Unknown XML mixed child');return [child.payload,-child.alternative-1,false];
    }
    if (!this.meta) {
      if (this.nil) throw new Error('Nil XML element cannot contain text or child elements');
      if (this.wrapper && name===this.item.name) return [this.item,null,false];
      throw new Error('XML scalar contains child elements');
    }
    while (this.position<this.fields.length) {
      const index=this.position, field=this.fields[index];
      if(!this.active[index]){++this.position;continue;}
      if (field.mode!=='child') {
        const foreign=this.foreign(name,index);if(foreign)return foreign;++this.position;continue;
      }
      const group=this.meta.all.find(group=>group.fields.includes(index));
      if (group) {
        for (const selected of group.fields) {
          const candidate=this.fields[selected];
          if (this.active[selected] && (candidate.wrapper!==null ? candidate.wrapper : candidate.name)===name) {
            if (this.counts[selected]) throw new Error('XML field occurrence maximum');
            return [candidate,selected,candidate.wrapper!==null];
          }
        }
        if ((group.minimum || group.fields.some(selected=>this.counts[selected])) &&
            group.fields.some(selected=>this.active[selected] && this.counts[selected]<this.fields[selected].minimum)) {
          throw new Error('Required XML all member missing');
        }
        this.position=Math.max(...group.fields)+1;continue;
      }
      const wrapped=field.wrapper!==null, expected=wrapped ? field.wrapper : field.name;
      if (expected===name) {
        const maximum=wrapped ? 1 : field.maximum;
        if (maximum!==null && this.counts[index]>=maximum) throw new Error('XML field occurrence maximum');
        return [field,index,wrapped];
      }
      if(!field.repeated && this.counts[index] || wrapped && this.counts[index]){++this.position;continue;}
      const foreign=this.foreign(name,index,field.repeated && !wrapped ? this.counts[index] : 0);
      if(foreign)return foreign;
      const minimum=wrapped ? (field.presence==='absent' || field.presence==='absent_or_nil' || field.optional && field.presence===null ? 0 : 1) : field.minimum;
      if (this.counts[index]<minimum) throw new Error('Required XML child missing or out of order');
      ++this.position;
    }
    const foreign=this.foreign(name,this.fields.length);if(foreign)return foreign;
    throw new Error('Unexpected XML child');
  }
  // Attach a completed native value only to the private candidate's statically generated setter.
  store(index,value,wrapper=false,bindings=null) {
    const field=this.fields[index];
    if(wrapper && Array.isArray(bindings))bindings.forEach((scope,item)=>srlXmlRememberNamespaces(this.value,field,item,scope));
    else srlXmlRememberNamespaces(this.value,field,this.counts[index],bindings);
    ++this.counts[index];
    if (value===srlXmlTokenNil) field.set(this.value,field.presence==='absent_or_nil' ? [[]] : []);
    else if (field.repeated && !wrapper) field.get(this.value).push(value);
    else field.set(this.value,field.presence==='absent_or_nil' ? [[value]] : field.optional ? [value] : value);
  }
  // Admit a completed child through the owning field or current native collection wrapper.
  accept(index,value,wrapper=false,bindings=null) {
    if(index && index.extension){this.extensions.child(index.position,index.item,value);return;}
    if(this.mixed){srlXmlMixedAppend(this,-index-1,value);return;}
    if(this.wrapper) {
      if(this.item.maximum!==null && this.values.length>=this.item.maximum) throw new Error('XML wrapped field occurrence maximum');
      this.values.push(value);this.bindings.push(bindings);
    } else this.store(index,value,wrapper,bindings);
  }
  // Retain only scalar/simple-content text; ordinary record indentation is discarded immediately.
  characters(value) {
    if (this.nil && value.length) throw new Error('Nil XML element cannot contain text or child elements');
    if(this.mixed){srlXmlMixedCharacters(this,value);return;}
    if (!this.meta || this.fields.some(field=>field.mode==='text')) this.text+=value;
    else if (srlXmlTrim(value)!=='') throw new Error('Unexpected XML record text');
  }
  // Require complete content before returning a scalar or owning value to the parent frame.
  finish() {
    if (!this.meta) {
      if (this.nil) return srlXmlTokenNil;
      if (!this.wrapper) return srlXmlReadField(this.item,this.text || this.item.default || '',this.limits,this.scope,this.bindings,!this.text);
      if (srlXmlTrim(this.text)!=='') throw new Error('Unexpected XML wrapper text');
      if (this.values.length<this.item.minimum || this.item.maximum!==null && this.values.length>this.item.maximum ||
          this.item.extent && this.values.length!==this.item.extent) throw new Error('XML wrapped field cardinality');
      return this.values;
    }
    if(this.mixed)srlXmlMixedFlush(this);
    for (let index=0;index<this.fields.length;++index) {
      const field=this.fields[index];
      if(!this.active[index])continue;
      if (field.mode==='text') {const bindings=new Map();this.store(index,
        srlXmlReadField(field,this.text || field.default || '',this.limits,this.scope,bindings,!this.text),false,bindings);}
      let minimum=field.wrapper!==null ? (field.presence==='absent' || field.presence==='absent_or_nil' || field.optional && field.presence===null ? 0 : 1) : field.minimum;
      const group=this.meta.all.find(group=>group.fields.includes(index));
      if (group && !group.minimum && !group.fields.some(selected=>this.counts[selected])) minimum=0;
      if (this.counts[index]<minimum || field.maximum!==null && field.wrapper===null && this.counts[index]>field.maximum ||
          field.extent && field.wrapper===null && this.counts[index]!==field.extent) throw new Error('XML field cardinality');
    }
    for (const choice of this.meta.choices) {
      const count=choice.fields.reduce((total,index)=>total+(this.counts[index] ? 1 : 0),0);
      if (count<choice.minimum || count>choice.maximum) throw new Error('XML choice cardinality');
    }
    if(this.extensions){this.extensions.finish();if(this.extensions.state)srlXmlExtensionState.set(this.value,this.extensions.state);}
    return this.value;
  }
}

// Use the provider's public incremental interface and expose a candidate only after strict final EOF.
function srlXmlTokenDecode(meta,chunks,limits,strings=false) {
  const stack=[],identities=[]; let result=null,rootSeen=false,complete=false,bytes=0,nodes=0,attributes=0,textBytes=0;
  const parser=new SrlXmlSaxes({xmlns:true,fragment:false,defaultXMLVersion:'1.0',forceXMLVersion:true});
  const charge=value=>{
    textBytes+=srlXmlUtf8Length(value);
    if (textBytes>limits.maxTextBytes) throw new Error('XML text limit');
    srlXmlCharacters(value);
  };
  parser.on('error',error=>{throw error;});
  parser.on('doctype',()=>{throw new Error('XML DTD/entities forbidden');});
  parser.on('xmldecl',declaration=>{
    if (declaration.version!=='1.0' || declaration.encoding && !/^utf-?8$/i.test(declaration.encoding)) {
      throw new Error('Only XML 1.0 UTF-8 declarations are supported');
    }
  });
  parser.on('opentagstart',()=>{
    if (++nodes>limits.maxNodes || stack.length+1>limits.maxDepth) throw new Error('XML node/depth limit');
  });
  parser.on('attribute',attribute=>{
    if (++attributes>limits.maxAttributes) throw new Error('XML attribute limit');
    charge(attribute.value);
    if ((attribute.name==='xmlns' || attribute.prefix==='xmlns') && attribute.value!==attribute.value.trim()) {
      throw new Error('XML namespace URI whitespace is not permitted');
    }
  });
  parser.on('opentag',node=>{
    charge(node.local); charge(node.uri);
    const name=srlXmlTokenName(node.uri,node.local),attrs=new Map(),seen=new Set();
    const scope={parent:stack.length ? stack[stack.length-1][0].scope : null,bindings:new Map(Object.entries(node.ns))};
    for (const attribute of Object.values(node.attributes)) {
      const key=srlXmlTokenName(attribute.uri,attribute.local);
      if (seen.has(key)) throw new Error('Duplicate XML expanded attribute');
      seen.add(key);
      if (attribute.uri==='http://www.w3.org/2000/xmlns/') charge(attribute.prefix==='xmlns' ? attribute.local : '');
      else { charge(attribute.local); charge(attribute.uri); attrs.set(key,attribute.value); }
    }
    let frame,index=null;
    if (!stack.length) {
      if (rootSeen || name!==meta.root) throw new Error('Unexpected XML root');
      rootSeen=true; frame=new SrlXmlTokenFrame(meta,name,attrs,null,false,limits,scope);
    } else {
      const parent=stack[stack.length-1][0],selected=parent.child(name,attrs,scope),field=selected[0];
      index=selected[1];
      frame=field.foreign ? new SrlXmlForeignFrame(name,attrs,scope,field.foreign.tracker,field.foreign.preserve) :
        field.map ? new SrlXmlMapFrame(field,name,attrs,limits) :
        field.variant ? new SrlXmlVariantFrame(field,name,attrs) :
        new SrlXmlTokenFrame(!selected[2] && field.model ? field.model() : null,name,attrs,field,selected[2],limits,scope);
    }
    frame.scope=scope;
    if(frame.identity)identities.push(frame.identity);
    for(const observer of identities)observer.start(name,attrs,scope);
    stack.push([frame,index]);
  });
  const characters=value=>{
    charge(value);
    for(const observer of identities)observer.text(value);
    if (stack.length) stack[stack.length-1][0].characters(value);
    else if (srlXmlTrim(value)!=='') throw new Error('XML text outside root');
  };
  parser.on('text',characters); parser.on('cdata',characters);
  parser.on('closetag',()=>{
    const [frame,index]=stack.pop(),value=frame.finish();
    for(const observer of identities)observer.end();
    if(frame.identity){identities.pop();frame.identity.finish();}
    if (!stack.length) result=value;
    else {
      const parent=stack[stack.length-1][0];
      parent.accept(index,value,frame.wrapper,frame.bindings);
    }
  });
  parser.on('end',()=>{complete=true;});
  const decoder=strings ? null : new TextDecoder('utf-8',{fatal:true});
  for (const chunk of chunks) {
    let source;
    if (strings) {
      if (typeof chunk!=='string' || chunk.length>limits.maxBytes-bytes) throw new Error('XML input byte limit');
      const count=srlXmlUtf8Length(chunk);
      if (count>limits.maxBytes-bytes) throw new Error('XML input byte limit');
      bytes+=count; srlXmlCharacters(chunk); source=chunk;
    } else {
      if (!(chunk instanceof Uint8Array)) throw new Error('XML chunks must be UTF-8 byte arrays');
      if (chunk.byteLength>limits.maxBytes-bytes) throw new Error('XML input byte limit');
      bytes+=chunk.byteLength; source=decoder.decode(chunk,{stream:true});
    }
    parser.write(source);
  }
  if (decoder) parser.write(decoder.decode());
  parser.close();
  if (!complete || stack.length || result===null) throw new Error('XML requires one complete document');
  return result;
}
)jstoken"
