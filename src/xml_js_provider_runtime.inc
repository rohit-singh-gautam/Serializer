// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"jsprovider(
const srlXmlSchemaNamespace='http://www.w3.org/2001/XMLSchema';
const srlXmlSchemaSecret=Symbol('prepared XML schema');

// Bound a complete provider input with public SAX events; collect only declared schema roots, never a tree.
function srlXmlProviderScan(source,limits,schema=false,declared=null) {
  if(source instanceof Uint8Array){
    if(source.byteLength>limits.maxBytes)throw new Error('XML provider input byte limit');
    source=new TextDecoder('utf-8',{fatal:true}).decode(source);
  }
  if(typeof source!=='string' || source.length>limits.maxBytes || srlXmlUtf8Length(source)>limits.maxBytes)throw new Error('XML provider input byte limit');
  srlXmlCharacters(source);
  const parser=new SrlXmlSaxes({xmlns:true,fragment:false,defaultXMLVersion:'1.0',forceXMLVersion:true});
  const roots=new Set();let depth=0,nodes=0,attributes=0,text=0,target='',rootSeen=false,complete=false;
  const charge=value=>{const count=srlXmlUtf8Length(value);if(count>limits.maxTextBytes-text)throw new Error('XML provider text limit');text+=count;srlXmlCharacters(value);};
  parser.on('error',error=>{throw error;});
  parser.on('doctype',()=>{throw new Error('XML provider DTD/entities forbidden');});
  parser.on('xmldecl',value=>{
    if(value.version!=='1.0' || value.encoding && !/^utf-?8$/i.test(value.encoding))throw new Error('Only XML1.0 UTF-8 provider input is supported');
  });
  parser.on('opentagstart',()=>{if(++nodes>limits.maxNodes || ++depth>limits.maxDepth)throw new Error('XML provider node/depth limit');});
  parser.on('attribute',attribute=>{
    if(++attributes>limits.maxAttributes)throw new Error('XML provider attribute limit');charge(attribute.value);
    if((attribute.name==='xmlns' || attribute.prefix==='xmlns') && attribute.value!==attribute.value.trim())throw new Error('XML namespace URI whitespace is not permitted');
  });
  parser.on('opentag',node=>{
    charge(node.local);charge(node.uri);const seen=new Set();
    for(const attribute of Object.values(node.attributes)){
      const key=srlXmlTokenName(attribute.uri,attribute.local);if(seen.has(key))throw new Error('Duplicate XML expanded attribute');seen.add(key);
      if(attribute.uri==='http://www.w3.org/2000/xmlns/')charge(attribute.prefix==='xmlns' ? attribute.local : '');
      else {charge(attribute.local);charge(attribute.uri);}
    }
    if(depth===1){
      rootSeen=true;
      if(schema){
        if(node.uri!==srlXmlSchemaNamespace || node.local!=='schema')throw new Error('Runtime XSD requires an XML Schema root');
        target=node.attributes.targetNamespace?.value || '';
      } else if(declared && !declared.has(srlXmlTokenName(node.uri,node.local)))throw new Error('XML root is not declared by the prepared schema');
    }
    if(schema && node.uri===srlXmlSchemaNamespace){
      if(['include','import','redefine','override'].includes(node.local))throw new Error('Runtime XSD external references are unsupported');
      if(depth===2 && node.local==='element' && node.attributes.name){
        roots.add(srlXmlTokenName(target,node.attributes.name.value));
      }
    }
  });
  parser.on('closetag',()=>{--depth;});
  parser.on('text',charge);parser.on('cdata',charge);parser.on('end',()=>{complete=true;});
  parser.write(source).close();
  if(!rootSeen || !complete || depth)throw new Error('XML provider requires one complete document');
  return {source,roots};
}

// Own one real prepared XSD1.0 handle; synchronous calls serialize within an isolate and workers own separate handles.
export class XmlSchema {
  #provider;#schema;#document;#roots;#limits;#disposed=false;
  // Construction is restricted to successful async preparation; native schema/document lifetimes remain paired.
  constructor(secret,provider,schema,document,roots,limits) {
    if(secret!==srlXmlSchemaSecret)throw new Error('Use XmlSchema.prepare to compile a runtime schema');
    this.#provider=provider;this.#schema=schema;this.#document=document;this.#roots=roots;this.#limits=limits;
  }
  // Lazily load the optional pinned provider after bounded no-fetch schema inspection; ordinary XML needs no WASM provider.
  static async prepare(xsd,limits=new XmlLimits()) {
    limits=new XmlLimits(limits);const checked=srlXmlProviderScan(xsd,limits,true);let provider;
    try {provider=await import('libxml2-wasm');}
    catch(error){throw new Error('Runtime XSD validation requires optional libxml2-wasm0.7.1',{cause:error});}
    const option=provider.ParseOption.XML_PARSE_NONET | provider.ParseOption.XML_PARSE_NO_XXE | provider.ParseOption.XML_PARSE_NO_SYS_CATALOG;
    const document=provider.XmlDocument.fromString(checked.source,{option,encoding:'utf-8'});
    try {return new XmlSchema(srlXmlSchemaSecret,provider,provider.XsdValidator.fromDoc(document),document,checked.roots,limits);}
    catch(error){document.dispose();throw error;}
  }
  // Validate changing complete documents after strict bounded preflight and declared-root admission, disposing only call-local trees.
  validate(source,limits=this.#limits) {
    if(this.#disposed)throw new Error('Prepared XML schema is disposed');
    const checked=srlXmlProviderScan(source,new XmlLimits(limits),false,this.#roots),provider=this.#provider;
    const option=provider.ParseOption.XML_PARSE_NONET | provider.ParseOption.XML_PARSE_NO_XXE | provider.ParseOption.XML_PARSE_NO_SYS_CATALOG;
    const document=provider.XmlDocument.fromString(checked.source,{option,encoding:'utf-8'});
    try {this.#schema.validate(document);} finally {document.dispose();}
  }
  // Release compiled native resources deterministically; repeated disposal is harmless and subsequent validation rejects.
  dispose() {
    if(!this.#disposed){this.#schema.dispose();this.#document.dispose();this.#disposed=true;}
  }
}
)jsprovider"
