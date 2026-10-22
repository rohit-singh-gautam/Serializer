// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsscalar(
import {NAME_RE as SrlXmlNameRule,NMTOKEN_RE as SrlXmlTokenRule} from 'xmlchars/xml/1.0/ed5.js';
// Distinguish a failed lexical alternative from resource/provider/caller failures that unions must propagate.
class SrlXmlLexicalError extends Error {}
// Runtime-owned local quota failures remain distinct from malformed data and caller errors.
class SrlXmlLimitError extends Error {}
let srlXmlPackageQuotaDepth=0;
// Native codecs are synchronous; scoped tagging never spans an asynchronous provider or caller await.
function srlXmlWithPackageQuota(action){++srlXmlPackageQuotaDepth;try{return action();}finally{--srlXmlPackageQuotaDepth;}}
// Preserve ordinary Error instances and messages outside package-owned codec invocation.
function srlXmlQuotaError(message){return srlXmlPackageQuotaDepth?new SrlXmlLimitError(message):new Error(message);}
// Explicit portable-pattern execution bounds; work is cumulative across one encode/decode operation.
export class XmlPatternLimits {
  constructor(values={}) {
    Object.assign(this,{maxStates:16384,maxInputBytes:8388608,maxWorkUnits:67108864},values);
    for(const key of ['maxStates','maxInputBytes','maxWorkUnits']) {
      if(!Number.isSafeInteger(this[key]) || this[key]<0) throw new Error('Invalid XML pattern limits');
    }
  }
}
const srlXmlScalarCache=new WeakMap();
const srlXmlListRules=new WeakMap();
const srlXmlSingleBits=new DataView(new ArrayBuffer(4));
const srlXmlIntegerKinds=new Set(['byte','unsignedByte','short','unsignedShort','int','unsignedInt','long','unsignedLong',
  'integer','positiveInteger','negativeInteger','nonPositiveInteger','nonNegativeInteger']);
const srlXmlIntegerWidths=new Map([['byte',[8,true]],['unsignedByte',[8,false]],['short',[16,true]],
  ['unsignedShort',[16,false]],['int',[32,true]],['unsignedInt',[32,false]],['long',[64,true]],['unsignedLong',[64,false]]]);

// Apply only XML whitespace rules; non-XML Unicode spaces remain significant scalar data.
function srlXmlScalarWhitespace(text,rule) {
  let mode=rule.whitespace;
  if(!mode) mode=rule.kind==='string' ? 'preserve' : rule.kind==='normalizedString' ? 'replace' : 'collapse';
  if(mode==='replace' || mode==='collapse') text=text.replace(/[\t\r\n]/g,' ');
  return mode==='collapse' ? srlXmlTrim(text).replace(/ +/g,' ') : text;
}

// Normalize arbitrary-precision decimal digits without converting untrusted long lexemes to BigInt/Number.
function srlXmlDecimal(text,integer=false) {
  if(!(integer ? /^[+-]?[0-9]+$/ : /^[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)$/).test(text)) {
    throw new SrlXmlLexicalError('XML exact numeric lexical value');
  }
  const negative=text[0]==='-',unsigned=text[0]==='-' || text[0]==='+' ? text.slice(1) : text;
  const dot=unsigned.indexOf('.');
  const whole=(dot<0 ? unsigned : unsigned.slice(0,dot)).replace(/^0+/,''),fraction=(dot<0 ? '' : unsigned.slice(dot+1)).replace(/0+$/,'');
  return {negative:negative && (whole!=='' || fraction!==''),whole,fraction};
}

// Compare normalized decimal value spaces exactly, padding fractional digits logically rather than allocating them.
function srlXmlDecimalCompare(left,right) {
  if(left.negative!==right.negative) return left.negative ? -1 : 1;
  let result=left.whole.length===right.whole.length ? left.whole<right.whole ? -1 : left.whole>right.whole ? 1 : 0 :
    left.whole.length<right.whole.length ? -1 : 1;
  for(let index=0;!result && index<Math.max(left.fraction.length,right.fraction.length);++index) {
    const a=index<left.fraction.length ? left.fraction.charCodeAt(index) : 48;
    const b=index<right.fraction.length ? right.fraction.charCodeAt(index) : 48;
    if(a!==b) result=a<b ? -1 : 1;
  }
  return left.negative ? -result : result;
}

// Normalize scientific decimal magnitude as significant digits/order without allocating exponent-sized padding.
function srlXmlScientificDigits(text) {
  if(text[0]==='-' || text[0]==='+')text=text.slice(1);
  const e=text.search(/[eE]/),exponent=e<0 ? 0 : Number(text.slice(e+1)),mantissa=e<0 ? text : text.slice(0,e);
  const dot=mantissa.indexOf('.'),whole=dot<0 ? mantissa.length : dot;
  let digits=dot<0 ? mantissa : mantissa.slice(0,dot)+mantissa.slice(dot+1);
  const leading=digits.length-digits.replace(/^0+/,'').length;
  digits=digits.slice(leading).replace(/0+$/,'');
  return {digits,order:whole+exponent-leading};
}

// Compute an adjacent binary32 midpoint exactly; only bounded trusted IEEE mantissas enter BigInt arithmetic.
function srlXmlSingleMidpoint(lower,upper) {
  const binary=bits=>{const exponent=(bits>>>23)&255;return [BigInt((bits&8388607)+(exponent ? 8388608 : 0)),exponent ? exponent-150 : -149];};
  const a=binary(lower),b=binary(upper),common=Math.min(a[1],b[1]),power=common-1;
  const sum=(a[0]<<BigInt(a[1]-common))+(b[0]<<BigInt(b[1]-common));
  const digits=(power<0 ? sum*5n**BigInt(-power) : sum<<BigInt(power)).toString();
  return {digits,order:digits.length+Math.min(power,0)};
}

// Avoid decimal-to-double-to-single double rounding only at exact double representations of binary32 midpoints.
function srlXmlSingle(text,value) {
  const negative=text[0]==='-',magnitude=Math.abs(value),rounded=Math.fround(magnitude);
  const signed=selected=>negative ? -selected : selected;
  if(rounded===magnitude)return signed(rounded);
  srlXmlSingleBits.setFloat32(0,rounded);const bits=srlXmlSingleBits.getUint32(0),neighbor=bits+(magnitude>rounded ? 1 : -1);
  srlXmlSingleBits.setUint32(0,neighbor);const adjacent=srlXmlSingleBits.getFloat32(0);
  if(magnitude!==(rounded+adjacent)/2)return signed(rounded);
  const lower=Math.min(bits,neighbor),upper=Math.max(bits,neighbor),left=srlXmlScientificDigits(text),right=srlXmlSingleMidpoint(lower,upper);
  let comparison=left.order<right.order ? -1 : left.order>right.order ? 1 : 0;
  for(let index=0;!comparison && index<Math.max(left.digits.length,right.digits.length);++index){
    const a=index<left.digits.length ? left.digits.charCodeAt(index) : 48,b=index<right.digits.length ? right.digits.charCodeAt(index) : 48;
    if(a!==b)comparison=a<b ? -1 : 1;
  }
  const selected=comparison<0 ? lower : comparison>0 ? upper : lower%2===0 ? lower : upper;
  srlXmlSingleBits.setUint32(0,selected);return signed(srlXmlSingleBits.getFloat32(0));
}

// Classify one normalized lexical value in its exact XSD value space, including native float32 rounding.
function srlXmlScalarValue(text,rule,scope=null,bindings=null) {
  const kind=rule.kind;
  if(rule.listItem)return srlXmlLexicalTokens(text).map(item=>srlXmlScalarValue(item,srlXmlListRule(rule),scope,bindings));
  if(kind==='QName')return srlXmlScalarQName(text,scope,bindings);
  if(kind==='language' && !/^[A-Za-z]{1,8}(?:-[A-Za-z0-9]{1,8})*$/.test(text) ||
      kind==='Name' && !srlXmlCompleteName(SrlXmlNameRule,text) ||
      kind==='NMTOKEN' && !srlXmlCompleteName(SrlXmlTokenRule,text) ||
      ['NCName','ID','IDREF','ENTITY'].includes(kind) && !srlXmlCompleteName(SrlXmlNcName,text)) {
    throw new SrlXmlLexicalError('XML lexical name restriction');
  }
  if(srlXmlIntegerKinds.has(kind)) {
    const value=srlXmlDecimal(text,true),width=srlXmlIntegerWidths.get(kind);
    if(width) {
      const bits=BigInt(width[0]),minimum=width[1] ? -(1n<<(bits-1n)) : 0n;
      const maximum=(1n<<(width[1] ? bits-1n : bits))-1n;
      const limit=srlXmlDecimal((value.negative ? minimum : maximum).toString(),true);
      if(value.negative && !width[1] && value.whole!=='' || srlXmlDecimalCompare(value,limit)*(value.negative ? -1 : 1)>0) {
        throw new SrlXmlLexicalError('XML integer value-space range');
      }
    }
    const zero=value.whole==='' && value.fraction==='';
    if(kind==='positiveInteger' && (zero || value.negative) || kind==='negativeInteger' && (zero || !value.negative) ||
        kind==='nonPositiveInteger' && !zero && !value.negative || kind==='nonNegativeInteger' && value.negative) {
      throw new SrlXmlLexicalError('XML integer sign restriction');
    }
    return value;
  }
  if(kind==='decimal') return srlXmlDecimal(text);
  if(kind==='float' || kind==='double' || kind==='boolean') {
    try{return srlXmlScalar(text,kind==='boolean' ? 'bool' : kind);}
    catch(error){if(error instanceof Error && error.message.startsWith('XML '))throw new SrlXmlLexicalError(error.message);throw error;}
  }
  return text;
}

// Preserve numeric enum/fixed equality, including decimal aliases, NaN identity and signed floating zero.
function srlXmlScalarEqual(left,right) {
  if(Array.isArray(left))return Array.isArray(right) && left.length===right.length && left.every((value,index)=>srlXmlScalarEqual(value,right[index]));
  if(left!==null && typeof left==='object') return right!==null && typeof right==='object' && srlXmlDecimalCompare(left,right)===0;
  return left===right || typeof left==='number' && typeof right==='number' && Number.isNaN(left) && Number.isNaN(right);
}

// Compile trusted static facet literals once, sharing read-only results between subsequent operations.
function srlXmlScalarRule(rule) {
  let compiled=srlXmlScalarCache.get(rule);
  if(compiled) return compiled;
  const parse=text=>srlXmlScalarValue(srlXmlScalarWhitespace(text,rule),rule);
  compiled={enumeration:rule.unionValues&&rule.unionValues.length ? [] : rule.qnames.length ? rule.qnames : rule.qnameLists.length ? rule.qnameLists : rule.enumeration.map(parse),
    fixed:rule.fixedUnion ? null : rule.fixedName!==null ? rule.fixedName : rule.fixedNames!==null ? rule.fixedNames : rule.fixed===null ? null : parse(rule.fixed),bounds:[]};
  for(const [name,minimum,inclusive] of [['minInclusive',true,true],['maxInclusive',false,true],['minExclusive',true,false],['maxExclusive',false,false]]) {
    if(rule[name]!==null) compiled.bounds.push([parse(rule[name]),minimum,inclusive]);
  }
  srlXmlScalarCache.set(rule,compiled);return compiled;
}

// Split only XML whitespace; empty lexical lists contain zero native items.
function srlXmlLexicalTokens(text) {const trimmed=srlXmlTrim(text);return trimmed ? trimmed.split(/[ \t\r\n]+/) : [];}

// Cache one immutable item contract so list facets apply to the list rather than to each item.
function srlXmlListRule(rule) {
  if(rule.itemRule)return rule.itemRule;
  let item=srlXmlListRules.get(rule);
  if(!item){item=Object.freeze({...rule,kind:rule.listItem,listItem:'',length:null,minLength:null,maxLength:null,
    enumeration:Object.freeze([]),qnames:Object.freeze([]),qnameLists:Object.freeze([]),union:Object.freeze([]),unionValues:Object.freeze([]),fixedUnion:null,itemRule:null,fixed:null,fixedName:null,fixedNames:null,patterns:Object.freeze([]),prefixList:false});
    srlXmlListRules.set(rule,item);}
  return item;
}

// Convert one checked lexical list to native scalar items without accepting empty or whitespace-bearing items.
function srlXmlListRead(text,read) {return srlXmlLexicalTokens(text).map(read);}

// Preserve one-to-one native item identities before checking complete lexical list restrictions.
function srlXmlListWrite(values,write) {
  if(!Array.isArray(values))throw new Error('XML list requires a native array');
  return values.map(value=>{const text=write(value);if(!text || /[ \t\r\n]/.test(text))throw new Error('XML list item contains whitespace');return text;}).join(' ');
}

)jsscalar"
R"jsscalar(

// Execute immutable Thompson instructions with reusable closure inventories and a complete-operation work fence.
function srlXmlPortablePattern(text,program,limits) {
  const bounds=limits.pattern,states=program[1],count=states.length;
  if(count>bounds.maxStates || srlXmlUtf8Length(text)>bounds.maxInputBytes) throw srlXmlQuotaError('XML pattern state/input limit');
  const charge=()=>{if(limits.operation)limits.operation("work",1,limits.operationField||"");if(limits.patternWork>=bounds.maxWorkUnits)throw srlXmlQuotaError('XML pattern work limit');++limits.patternWork;};
  const seen=new Uint32Array(count),pending=[],seeds=[];let generation=0,current=[],following=[];
  const closure=(output,input)=>{
    output.length=0;pending.length=0;
    if(++generation===4294967296){seen.fill(0);generation=1;}
    for(const index of input) pending.push(index);
    while(pending.length){
      const index=pending.pop();charge();if(seen[index]===generation)continue;seen[index]=generation;
      const state=states[index];if(state[3] || state[6])output.push(index);
      for(const target of state[0]){charge();pending.push(target);}
    }
  };
  closure(current,[program[0]]);
  for(const character of text){
    const codepoint=character.codePointAt(0);seeds.length=0;
    for(const index of current){
      charge();const state=states[index];if(!state[3])continue;
      let matches=state[5] ? codepoint!==10 && codepoint!==13 : false;
      if(!state[5])for(const range of state[1]){charge();if(codepoint>=range[0] && codepoint<=range[1]){matches=true;break;}}
      if(state[4])matches=!matches;if(matches)seeds.push(state[2]);
    }
    closure(following,seeds);const previous=current;current=following;following=previous;
    if(!current.length)return false;
  }
  for(const index of current){charge();if(states[index][6])return true;}return false;
}

// Validate complete normalized scalar restrictions before native conversion or returning any encoded XML bytes.
function srlXmlCheckedText(text,rule,limits,scope=null,bindings=null) {
  const original=text;
  if(!rule.union.length)text=srlXmlScalarWhitespace(text,rule);
  const track=bindings!==null && bindings.srlXmlTrackSelection;
  let unionSelection=null;
  if(rule.union.length){
    let matched=false;
    for(const alternative of rule.union){
      const selected=new Map();
      selected.srlXmlTrackSelection=true;
      try{srlXmlCheckedText(text,alternative,limits,scope,selected);unionSelection=selected.srlXmlSelection;text=unionSelection.text;if(bindings){srlXmlMergeBindings(bindings,selected);if(track)bindings.srlXmlSelection=unionSelection;}matched=true;break;}
      catch(error){if(!(error instanceof SrlXmlLexicalError))throw error;}
    }
    if(!matched)throw new SrlXmlLexicalError('XML value matches no union member');
  }
  let value=srlXmlScalarValue(text,rule,scope,bindings);const compiled=srlXmlScalarRule(rule);
  let listItems=null;
  if(rule.listItem&&(rule.itemRule||track)){
    const tokens=srlXmlLexicalTokens(text);if(limits.operation)limits.operation("allocation",tokens.length*128,limits.operationField||"");
    listItems=[];for(const token of tokens){const selected=new Map();selected.srlXmlTrackSelection=true;srlXmlCheckedText(token,srlXmlListRule(rule),limits,scope,selected);if(bindings)srlXmlMergeBindings(bindings,selected);listItems.push(selected.srlXmlSelection);}
    value=listItems.map(item=>item.value);
  }
  const prefixValues=track && rule.prefixList ? [] : null;
  if(rule.prefixList)for(const prefix of srlXmlLexicalTokens(text)){
    const name=srlXmlScalarQName(prefix+':item',scope,bindings);if(prefixValues)prefixValues.push(name);
  }
  if(rule.finite && typeof value==='number' && !Number.isFinite(value))throw new SrlXmlLexicalError('XML finite scalar restriction');
  for(const [bound,minimum,inclusive] of compiled.bounds){
    if(typeof value==='number' && (Number.isNaN(value) || Number.isNaN(bound))){
      if(inclusive && Number.isNaN(value) && Number.isNaN(bound))continue;
      throw new SrlXmlLexicalError('XML scalar unordered numeric bound');
    }
    const comparison=value!==null && typeof value==='object' ? srlXmlDecimalCompare(value,bound) :
      typeof value==='number' && Number.isNaN(value) ? NaN : value<bound ? -1 : value>bound ? 1 : value===bound ? 0 : NaN;
    if(Number.isNaN(comparison) || (minimum ? comparison<0 || !inclusive && comparison===0 : comparison>0 || !inclusive && comparison===0)) {
      throw new SrlXmlLexicalError('XML scalar numeric bound');
    }
  }
  if(rule.union.length||rule.itemRule){
    const actual=unionSelection||{rule,value,items:listItems,text};
    if(rule.enumeration.length && !(rule.unionValues && rule.unionValues.length ? rule.unionValues.some(item=>srlXmlUnionConstantEqual(actual,item)) : rule.enumeration.some(item=>srlXmlUnionEqual(actual,item,rule,limits,scope))))throw new SrlXmlLexicalError('XML scalar enumeration');
    if(rule.fixed!==null && !(rule.fixedUnion ? srlXmlUnionConstantEqual(actual,rule.fixedUnion) : srlXmlUnionEqual(actual,rule.fixed,rule,limits,scope)))throw new SrlXmlLexicalError('XML fixed scalar value');
  }else{
    if(compiled.enumeration.length && !compiled.enumeration.some(item=>srlXmlScalarEqual(value,item)))throw new SrlXmlLexicalError('XML scalar enumeration');
    if(rule.fixed!==null && !srlXmlScalarEqual(value,compiled.fixed))throw new SrlXmlLexicalError('XML fixed scalar value');
  }
  if(rule.length!==null || rule.minLength!==null || rule.maxLength!==null){
    let length=rule.listItem ? srlXmlLexicalTokens(text).length : 0;if(!rule.listItem)for(const character of text)++length;
    if(rule.length!==null && length!==rule.length || rule.minLength!==null && length<rule.minLength ||
        rule.maxLength!==null && length>rule.maxLength)throw new SrlXmlLexicalError('XML scalar code-point/list length');
  }
  for(const group of rule.patterns){
    if(!group.some(program=>srlXmlPortablePattern(text,program,limits)))throw new SrlXmlLexicalError('XML scalar portable pattern');
  }
  if(track && !rule.union.length)bindings.srlXmlSelection={rule,value:prefixValues || value,items:listItems,text};
  return rule.union.length ? original : text;
}

// Keep primitive family identity separate from lexical aliases and authored member names.
function srlXmlUnionFamily(rule){
  if(rule.listItem)return 'list:'+srlXmlUnionFamily(srlXmlListRule(rule));
  if(srlXmlIntegerKinds.has(rule.kind) || rule.kind==='decimal')return 'decimal';
  return ['boolean','float','double','anyURI','QName'].includes(rule.kind) ? rule.kind : 'string';
}

// Prepare selected schema identities without retaining provider scopes or reparsing literal aliases.
function srlXmlSchemaIdentity(kind,listItem,text,names,items=[]){
  const rule=Object.freeze({kind,listItem});
  return Object.freeze({rule,value:listItem&&(items.length||!text) ? items.map(item=>item.value) : kind==='QName' ? names[0] : listItem==='QName' ? names : srlXmlScalarValue(text,rule),items});
}

// Keep primitive families distinct even when lexical strings or numeric representations coincide.
function srlXmlUnionConstantEqual(actual,expected){
  if(Boolean(actual.rule.listItem)!==Boolean(expected.rule.listItem))return false;
  if(actual.rule.listItem&&actual.items&&expected.items)return actual.items.length===expected.items.length&&actual.items.every((item,index)=>srlXmlUnionConstantEqual(item,expected.items[index]));
  return srlXmlUnionFamily(actual.rule)===srlXmlUnionFamily(expected.rule) && srlXmlScalarEqual(actual.value,expected.value);
}

// Select schema literals through the same ordered independent members as instance values.
function srlXmlUnionEqual(actual,allowed,rule,limits,scope){
  for(const member of rule.union){
    const bindings=new Map();bindings.srlXmlTrackSelection=true;
    try{srlXmlCheckedText(allowed,member,limits,scope,bindings);}
    catch(error){if(error instanceof SrlXmlLexicalError)continue;throw error;}
    const expected=bindings.srlXmlSelection;
    return srlXmlUnionFamily(actual.rule)===srlXmlUnionFamily(expected.rule) && srlXmlScalarEqual(actual.value,expected.value);
  }
  return false;
}
)jsscalar"
