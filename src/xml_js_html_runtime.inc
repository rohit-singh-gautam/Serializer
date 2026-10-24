// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jshtml(
// Bound the shared explicitly balanced HTML fragment subset independently of XML policy and providers.
export class HtmlLimits {
  constructor(values={}) {
    Object.assign(this,{maxInputBytes:8388608,maxOutputBytes:16777216,maxEvents:200000,
      maxDepth:128,maxAttributesPerElement:256,maxTextBytes:8388608},values);
    for(const key of ['maxInputBytes','maxOutputBytes','maxEvents','maxDepth','maxAttributesPerElement','maxTextBytes']){
      if(!Number.isSafeInteger(this[key]) || this[key]<0)throw new Error('Invalid HTML limit');
    }
  }
}

// Own one lowercase approved HTML attribute; hidden has Boolean presence without an assigned value.
export class HtmlAttribute {
  constructor(name,value='',boolean=false) { this.name=name;this.value=value;this.boolean=boolean; }
}

// Own one semantic fragment/start/text/end event, without XML namespaces or xsi metadata.
export class HtmlEvent {
  constructor(kind,name='',attributes=[],text='') { this.kind=kind;this.name=name;this.attributes=[...attributes];this.text=text; }
}

// Parse and write the approved HTML contract; neither operation implements general HTML5 recovery or URL sanitization.
export class Html {
  static #tags=new Set('div p span b i strong em a ul ol li section article h1 h2 h3 h4 h5 h6 br hr img small s u code blockquote nav sub sup'.split(' '));
  static #voids=new Set(['br','hr','img']);
  static #phrasing=new Set('span b i strong em a br img small s u code sub sup'.split(' '));
  static #entities=new Map([['amp','&'],['lt','<'],['gt','>'],['quot','"'],['apos',"'"],['nbsp','\u00a0']]);
  // Describe the supported syntax without advertising a general browser parser or sanitizer.
  static supportedSubset() {
    return 'UTF-8 HTML fragments; explicit balanced approved flow/phrasing/list tags; br/hr/img void tags; ASCII case-insensitive names; quoted/unquoted and hidden Boolean attributes; amp/lt/gt/quot/apos/nbsp and numeric references; comments omitted; no doctype, implied closes, raw text, foreign content or browser recovery';
  }
  // Accept Unicode scalars and the shared HTML whitespace set, rejecting unsupported controls.
  static text(value) {
    if(typeof value!=='string')throw new Error('HTML requires text');
    for(const item of value){const code=item.codePointAt(0);
      if(!code || code>=0xd800 && code<=0xdfff || code>=0x7f && code<=0x9f || code<0x20 && !'\t\n\r\f'.includes(item))throw new Error('Unsupported HTML control character');
    }
    return value;
  }
  // Decode only approved terminated character references and normalize HTML source line endings.
  static decode(value) {
    value=value.replace(/\r\n?/g,'\n');let result='',position=0;
    while(position<value.length){
      if(value[position]!=='&'){result+=value[position++];continue;}
      const end=value.indexOf(';',position+1);
      if(end<0 || end-position>12)throw new Error('HTML entities require a supported terminated spelling');
      const entity=value.slice(position+1,end);let decoded=Html.#entities.get(entity);
      if(decoded===undefined && entity.startsWith('#')){
        const hexadecimal=entity[1]==='x' || entity[1]==='X',digits=entity.slice(hexadecimal ? 2 : 1);
        if(!(hexadecimal ? /^[0-9a-fA-F]+$/ : /^[0-9]+$/).test(digits))throw new Error('Malformed HTML numeric reference');
        const code=Number.parseInt(digits,hexadecimal ? 16 : 10);
        if(code>0x10ffff)throw new Error('Unsupported HTML character reference');decoded=String.fromCodePoint(code);
      }
      if(decoded===undefined)throw new Error('HTML entity is outside the approved subset');
      result+=Html.text(decoded);position=end+1;
    }
    return Html.text(result);
  }
  // Reject parent/child relationships that an independent HTML parser would implicitly rearrange.
  static child(open,child) {
    if(!Html.#tags.has(child))throw new Error('HTML tag outside the approved subset');
    const parent=open[open.length-1];
    if(child==='li' && parent!=='ul' && parent!=='ol')throw new Error('HTML list items require a direct list parent');
    if(parent && (Html.#voids.has(parent) || (parent==='ul' || parent==='ol') && child!=='li' ||
        (Html.#phrasing.has(parent) || parent==='p' || /^h[1-6]$/.test(parent)) && !Html.#phrasing.has(child)) || child==='a' && open.includes('a')){
      throw new Error('HTML child requires unsupported browser recovery');
    }
  }
  // Validate an owning trace transactionally and return deterministic ordinary HTML, without an XML provider.
  static writeFragment(events,values={}) {
    const limits=new HtmlLimits(values);
    if(!Array.isArray(events) || events.length>limits.maxEvents || events.length<2 || events[0].kind!=='start_fragment' || events[events.length-1].kind!=='end_fragment'){
      throw new Error('HTML events require bounded explicit fragment boundaries');
    }
    const parts=[],open=[];let bytes=0,textBytes=0;
    const append=value=>{const count=srlXmlUtf8Length(value);if(count>limits.maxOutputBytes-bytes)throw new Error('HTML output byte limit');bytes+=count;parts.push(value);};
    const escape=(value,attribute=false)=>{
      Html.text(value);let batch='';
      for(const item of value){batch+=item==='&' ? '&amp;' : item==='<' ? '&lt;' : item==='>' ? '&gt;' :
        item==='"' && attribute ? '&quot;' : item==='\r' ? '&#13;' : item;
        if(batch.length>=4096){append(batch);batch='';}}
      if(batch)append(batch);
    };
    for(let index=0;index<events.length;++index){
      const event=events[index];
      if(!event || typeof event.name!=='string' || typeof event.text!=='string' || !Array.isArray(event.attributes))throw new Error('Invalid HTML event');
      if(event.kind==='start_fragment' || event.kind==='end_fragment'){
        if(index!==0 && index!==events.length-1 || open.length || event.name || event.attributes.length || event.text)throw new Error('Malformed HTML fragment boundary');
      } else if(event.kind==='start_element'){
        Html.child(open,event.name);
        if(event.text || open.length>=limits.maxDepth || event.attributes.length>limits.maxAttributesPerElement)throw new Error('HTML element payload/depth/attribute limit');
        append('<'+event.name);const names=new Set();
        for(const attribute of event.attributes){
          const key=attribute.name,allowed=['id','class','title','lang','dir','hidden'].includes(key) || event.name==='a' && ['href','rel'].includes(key) ||
            event.name==='img' && ['src','alt','width','height'].includes(key);
          if(!allowed || names.has(key) || typeof attribute.boolean!=='boolean' || attribute.boolean!==(key==='hidden') || attribute.boolean && attribute.value)throw new Error('Unsupported or duplicate HTML attribute');
          Html.text(attribute.value);names.add(key);append(' '+key);
          if(!attribute.boolean){append('="');escape(attribute.value,true);append('"');}
        }
        append('>');open.push(event.name);
      } else if(event.kind==='end_element'){
        if(!open.length || open[open.length-1]!==event.name || event.attributes.length || event.text)throw new Error('Unbalanced HTML element close');
        if(!Html.#voids.has(event.name))append('</'+event.name+'>');open.pop();
      } else if(event.kind==='text'){
        const count=srlXmlUtf8Length(event.text),parent=open[open.length-1];
        if(event.name || event.attributes.length || Html.#voids.has(parent) || count>limits.maxTextBytes-textBytes ||
            (parent==='ul' || parent==='ol') && /[^ \t\r\n\f]/.test(event.text))throw new Error('Invalid HTML text event or cumulative text bound');
        textBytes+=count;escape(event.text);
      } else throw new Error('Unknown HTML event kind');
    }
    return parts.join('');
  }

)jshtml"
R"jshtml(
  // Parse fatal UTF-8 with explicit nesting and return the event trace only after complete validated EOF.
  static parseFragment(data,values={}) {
    const limits=new HtmlLimits(values);
    if(!(data instanceof Uint8Array) || data.byteLength>limits.maxInputBytes)throw new Error('HTML input byte limit');
    const source=Html.text(new TextDecoder('utf-8',{fatal:true}).decode(data)),events=[],open=[],names=/[A-Za-z0-9-]+/y;let position=0,textBytes=0;
    const add=event=>{
      if(events.length>=limits.maxEvents)throw new Error('HTML event count limit');
      if(event.kind==='text'){const count=srlXmlUtf8Length(event.text);if(count>limits.maxTextBytes-textBytes)throw new Error('HTML text byte limit');textBytes+=count;}
      events.push(event);
    };
    const space=offset=>{while(offset<source.length && ' \t\r\n\f'.includes(source[offset]))++offset;return offset;};
    const name=offset=>{names.lastIndex=offset;const found=names.exec(source);if(!found)throw new Error('Unsupported HTML tag/attribute syntax');return found[0];};
    add(new HtmlEvent('start_fragment'));
    while(position<source.length){
      if(source[position]!=='<'){
        let end=source.indexOf('<',position);if(end<0)end=source.length;
        add(new HtmlEvent('text','',[],Html.decode(source.slice(position,end))));position=end;continue;
      }
      if(source.startsWith('<!--',position)){
        const end=source.indexOf('-->',position+4);
        if(end<0 || source.slice(position+4,end).includes('--'))throw new Error('Malformed HTML comment');position=end+3;continue;
      }
      ++position;const closing=source[position]==='/';if(closing)++position;
      const original=name(position),tag=original.toLowerCase();position+=original.length;
      if(closing){
        position=space(position);
        if(source[position]!=='>' || Html.#voids.has(tag) || open[open.length-1]!==tag)throw new Error('HTML close requires an explicit matching nonvoid start');
        ++position;open.pop();add(new HtmlEvent('end_element',tag));continue;
      }
      Html.child(open,tag);if(open.length>=limits.maxDepth)throw new Error('HTML nesting limit');
      const attributes=[];let selfClosing=false;
      while(true){
        const separator=position;position=space(position);
        if(position>=source.length)throw new Error('Truncated HTML start tag');
        if(source[position]==='>'){++position;break;}
        if(source.startsWith('/>',position)){selfClosing=true;position+=2;break;}
        if(separator===position || attributes.length>=limits.maxAttributesPerElement)throw new Error('HTML attributes require separation and bounded cardinality');
        const original=name(position),key=original.toLowerCase();position+=original.length;
        const keyEnd=position;position=space(position);const assigned=source[position]==='=';let value='';
        if(assigned){
          position=space(position+1);if(position>=source.length)throw new Error('Truncated HTML attribute');
          const quote=['"',"'"].includes(source[position]) ? source[position++] : null;let end;
          if(quote){end=source.indexOf(quote,position);if(end<0)throw new Error('Unterminated HTML attribute');}
          else {end=position;while(end<source.length && !' \t\r\n\f>'.includes(source[end])){
            if('\"\'< =`'.includes(source[end]))throw new Error('Invalid unquoted HTML attribute');++end;
          }if(end===position)throw new Error('Empty unquoted HTML attribute');}
          value=Html.decode(source.slice(position,end));position=end+(quote ? 1 : 0);
        } else position=keyEnd;
        if(key==='hidden'){if(assigned && value && value.toLowerCase()!=='hidden')throw new Error('HTML hidden requires Boolean spelling');value='';}
        else if(!assigned)throw new Error('Only approved Boolean HTML attributes may omit a value');
        attributes.push(new HtmlAttribute(key,value,key==='hidden'));
      }
      if(selfClosing && !Html.#voids.has(tag))throw new Error('HTML self-closing nonvoid tag requires browser recovery');
      add(new HtmlEvent('start_element',tag,attributes));
      if(Html.#voids.has(tag))add(new HtmlEvent('end_element',tag));else open.push(tag);
    }
    if(open.length)throw new Error('HTML fragment has unclosed elements');add(new HtmlEvent('end_fragment'));
    Html.writeFragment(events,limits);return events;
  }
}
)jshtml"
