// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsidentity(
// Bound one owner-local identity index, including transient selections and deferred forward references.
export class XmlIdentityLimits {
  // Validate explicit nonnegative admission policies; zero limits forbid the corresponding resource.
  constructor(values={}) {
    Object.assign(this,{maxConstraints:128,maxFields:32,maxDepth:128,maxSelectedNodes:100000,
      maxTuples:100000,maxTupleBytes:8388608,maxWorkUnits:67108864},values);
    for(const key of ['maxConstraints','maxFields','maxDepth','maxSelectedNodes','maxTuples','maxTupleBytes','maxWorkUnits']) {
      if(!Number.isSafeInteger(this[key]) || this[key]<0)throw new Error('Invalid XML identity limits');
    }
  }
}

// Retain only bounded tuples/open scalar captures; no known-element objects or sibling trees are constructed.
class SrlXmlIdentity {
  // Borrow immutable metadata through one complete traversal; all retained indexes are owner-local.
  constructor(contract,limits) {
    this.contract=contract;this.limits=limits;this.path=[];this.active=[];this.selected=0;this.tuples=0;
    this.tupleBytes=0;this.workUnits=0;this.started=false;this.complete=false;this.failed=false;
    const constraints=contract.constraints;
    if(constraints.length>limits.maxConstraints)throw new Error('XML identity constraint limit');
    this.tables=constraints.map(()=>new Set());this.targets=new Map(constraints.map((item,index)=>[item.id,index]));
    for(const item of constraints){
      if(!item.fields.length || item.fields.length>limits.maxFields)throw new Error('XML identity field limit');
      if(item.kind==='keyref' && !this.targets.has(item.refer))throw new Error('XML keyref target is outside owner scope');
    }
  }
  // Admit matching work before processing another input portion; zero is a real bound.
  work(count=1) {
    if(count>this.limits.maxWorkUnits-this.workUnits)throw new Error('XML identity work limit');
    this.workUnits+=count;
  }
  // Charge complete expanded-name comparisons, including large common namespace URI prefixes.
  matches(actual,expected) {
    this.work();if(actual.length!==expected.length)return false;
    for(let index=0;index<actual.length;++index){
      this.work(Math.min(srlXmlUtf8Length(actual[index]),srlXmlUtf8Length(expected[index]))+1);
      if(actual[index]!==expected[index])return false;
    }
    return true;
  }
  // Canonicalize scalar value spaces without unsafe integer conversion or presentation-prefix equality.
  key(lexical,field,scope) {
    const size=srlXmlUtf8Length(lexical);if(size>this.limits.maxTupleBytes)throw new Error('XML identity field-byte limit');
    this.work(size+1);
    const kind=field.kind,rule={kind,whitespace:kind==='string' ? 'preserve' : kind==='normalizedString' ? 'replace' : 'collapse'};
    const normalized=srlXmlScalarWhitespace(lexical,rule),value=srlXmlScalarValue(normalized,rule,scope);
    if(kind==='boolean')return ['boolean',value ? 'true' : 'false'];
    if(value!==null && typeof value==='object')return ['decimal',(value.negative ? '-' : '')+(value.whole || '0')+(value.fraction ? '.'+value.fraction : '')];
    if(kind==='QName'){const split=value.indexOf('\0');return ['QName',value.slice(0,split),value.slice(split+1)];}
    if(kind==='float' || kind==='double')throw new Error('Floating identity fields are outside the shared value-space contract');
    return [kind==='anyURI' ? 'uri' : 'text',value];
  }
  // Bound an incomplete candidate tuple as each selected scalar arrives.
  store(candidate,index,value) {
    const bytes=value.slice(1).reduce((total,item)=>total+srlXmlUtf8Length(item),0);
    if(bytes>this.limits.maxTupleBytes-candidate.bytes)throw new Error('XML identity candidate tuple-byte limit');
    candidate.bytes+=bytes;candidate.values[index]=value;
  }
  // Select current nodes/attributes through compiled expanded paths and retain only open scalar captures.
  start(name,attributes,scope) {
    try {
      if(this.failed || this.complete)throw new Error('XML identity observer is not active');
      this.work(srlXmlUtf8Length(name)+1);
      if(this.path.length>=this.limits.maxDepth)throw new Error('XML identity depth limit');
      if(!this.started){if(name!==this.contract.owner)throw new Error('XML identity owner mismatch');this.started=true;}
      this.path.push(name);const relative=this.path.slice(1);
      for(const candidate of this.active)if(candidate.captures.some(item=>item!==null))throw new Error('XML identity field has complex child content');
      for(let index=0;index<this.contract.constraints.length;++index){
        const constraint=this.contract.constraints[index],selector=constraint.selector,steps=selector.steps;
        this.work(relative.length+steps.length+1);
        const matches=selector.descendants ? !steps.length || this.matches(relative.slice(-steps.length),steps) : this.matches(relative,steps);
        if(matches){
          if(this.selected>=this.limits.maxSelectedNodes)throw new Error('XML identity selected-node limit');
          ++this.selected;const count=constraint.fields.length;
          this.active.push({index,depth:this.path.length,values:new Array(count).fill(null),seen:new Array(count).fill(false),captures:new Array(count).fill(null),bytes:0});
        }
      }
      for(const candidate of this.active){
        const relative=this.path.slice(candidate.depth),constraint=this.contract.constraints[candidate.index];
        for(let index=0;index<constraint.fields.length;++index){
          const field=constraint.fields[index],path=field.path;this.work(relative.length+path.steps.length+1);
          if(!this.matches(relative,path.steps) || path.attribute!==null && !attributes.has(path.attribute))continue;
          if(constraint.kind==='key' && field.nillable)throw new Error('XSD key field declaration must not be nillable');
          if(candidate.seen[index])throw new Error('XML identity field selects multiple values');candidate.seen[index]=true;
          if(path.attribute!==null)this.store(candidate,index,this.key(attributes.get(path.attribute),field,scope));
          else{
            const nil=srlXmlTrim(attributes.get(srlXmlTokenNilName) || 'false');
            if(!['true','1','false','0'].includes(nil))throw new Error('Malformed XML identity nil');
            candidate.captures[index]={text:'',bytes:0,scope,nil:nil==='true'||nil==='1',depth:this.path.length};
          }
        }
      }
    } catch(error){this.failed=true;throw error;}
  }
  // Charge only logical UTF-8 text work, making scalar capture admission independent of parser chunk splits.
  text(value) {
    try {
      if(this.failed || this.complete || !this.path.length)throw new Error('XML identity observer is not active');
      const bytes=srlXmlUtf8Length(value);this.work(bytes);
      for(const candidate of this.active)for(const capture of candidate.captures){
        if(capture===null)continue;this.work(bytes);
        if(capture.nil && value.length)throw new Error('Nil XML identity field contains text');
        if(bytes>this.limits.maxTupleBytes-capture.bytes)throw new Error('XML identity field-byte limit');
        capture.bytes+=bytes;capture.text+=value;
      }
    } catch(error){this.failed=true;throw error;}
  }
  // Close scalar captures, enforce key completeness/uniqueness, and retain complete forward-reference tuples.
  end() {
    try {
      if(this.failed || this.complete || !this.path.length)throw new Error('Unbalanced XML identity token');this.work();
      for(const candidate of this.active){
        const constraint=this.contract.constraints[candidate.index];
        for(let index=0;index<candidate.captures.length;++index){
          const capture=candidate.captures[index];this.work();
          if(capture!==null && capture.depth===this.path.length){
            if(!capture.nil)this.store(candidate,index,this.key(capture.text,constraint.fields[index],capture.scope));
            candidate.captures[index]=null;
          }
        }
      }
      const retained=[];
      for(const candidate of this.active){
        if(candidate.depth!==this.path.length){retained.push(candidate);continue;}
        const constraint=this.contract.constraints[candidate.index];
        if(candidate.values.some(value=>value===null)){if(constraint.kind==='key')throw new Error('XSD key tuple is incomplete or nil');continue;}
        if(this.tuples>=this.limits.maxTuples)throw new Error('XML identity tuple-count limit');
        if(candidate.bytes>this.limits.maxTupleBytes-this.tupleBytes)throw new Error('XML identity tuple-byte limit');
        ++this.tuples;this.tupleBytes+=candidate.bytes;this.work(candidate.bytes+1);
        const table=this.tables[candidate.index],key=JSON.stringify(candidate.values);
        if(table.has(key) && constraint.kind!=='keyref')throw new Error('Duplicate XSD key/unique tuple');table.add(key);
      }
      this.active=retained;this.path.pop();if(!this.path.length)this.complete=true;
    } catch(error){this.failed=true;throw error;}
  }
  // Resolve every complete keyref only after the owner scope is closed and all target tables exist.
  finish() {
    if(this.failed || !this.complete || this.path.length || this.active.length)throw new Error('Incomplete or failed XML identity scope');
    try {
      this.work();
      for(let index=0;index<this.contract.constraints.length;++index){
        const constraint=this.contract.constraints[index];if(constraint.kind!=='keyref')continue;
        const target=this.tables[this.targets.get(constraint.refer)];
        for(const key of this.tables[index]){this.work(srlXmlUtf8Length(key)+1);if(!target.has(key))throw new Error('Unresolved XSD keyref tuple');}
      }
    } catch(error){this.failed=true;throw error;}
  }
}
)jsidentity"
