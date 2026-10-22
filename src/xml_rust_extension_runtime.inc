// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustext(
/// Bound positioned foreign items independently of complete-document XML admission.
#[derive(Clone,Copy,Debug)]
pub struct XmlExtensionLimits { pub max_slots:usize,pub max_items:usize,pub max_preserved_bytes:usize }
/// Canonical XML transport never replaces complete original/final XML admission.
const SRL_XML_EXTENSION_TRANSPORT_FRAMING_EXPANSION:usize=16;
impl Default for XmlExtensionLimits {
    /// Apply the shared conservative preservation policy.
    fn default()->Self {Self {max_slots:SRL_XML_EXTENSION_MAX_SLOTS,max_items:SRL_XML_EXTENSION_MAX_ITEMS,max_preserved_bytes:SRL_XML_EXTENSION_MAX_PRESERVED_BYTES}}
}
/// Declare understood vocabulary separately from permission to preserve opaque XML.
#[derive(Clone,Debug,Default)]
pub struct XmlExtensionOptions {pub limits:XmlExtensionLimits,pub understood_namespaces:Vec<String>,pub required_prefixes:String}
impl XmlExtensionOptions {
    /// Bound external capability inventories without allocating copies of their URI or prefix strings.
    fn validate(&self)->Result<(),Error> {
        if self.understood_namespaces.len()>self.limits.max_items {return Err(Error("XML capability item limit"));}
        let mut bytes=self.required_prefixes.len();if bytes>SRL_XML_EXTENSION_MAX_PRESERVED_BYTES {return Err(Error("XML capability metadata byte limit"));}
        for value in &self.understood_namespaces {bytes=bytes.checked_add(value.len()).filter(|value|*value<=SRL_XML_EXTENSION_MAX_PRESERVED_BYTES).ok_or(Error("XML capability metadata byte limit"))?;}Ok(())
    }
}
/// Own an XML model and its independent opaque sidecar. Native codecs serialize only `value`.
pub struct XmlDocument<T> {pub value:T,pub extensions:XmlExtensionState}
/// Opaque positioned storage owns every retained byte and never borrows the source XML or model.
#[derive(Default)]
pub struct XmlExtensionState {owners:Vec<SrlXmlExtensionOwner>,bytes:usize,items:usize,validation_only:bool}
impl XmlExtensionState {
    /// Report charged logical preservation storage for exact application admission policies.
    pub fn preserved_bytes(&self)->usize {self.bytes}
}
/// Static field edges describe ordinary native ownership without reading or copying native values.
struct SrlXmlExtensionField {uri:&'static str,local:&'static str,attribute:bool,repeated:bool,child:Option<fn()->&'static SrlXmlExtensionModel>,namespace_kind:u8,namespace_enumeration:&'static [&'static [(&'static str,&'static str)]],type_identity:Option<(&'static str,&'static str)>,scalar_rule:Option<&'static SrlXmlRule>}
struct SrlXmlExtensionSlot {attribute:bool,position:usize,uris:&'static[&'static str],policy:u8,minimum:usize,maximum:Option<usize>,namespace_match:u8,process_contents:u8}
type SrlXmlDeclarationValidator=for<'a> fn(&str,&str,Option<&str>,&[(String,String)],&str,XmlLimits,Option<@xml_operation@::State<'a>>)->Result<bool,Error>;
struct SrlXmlExtensionContract {core:&'static str,required:&'static[&'static str],slots:&'static[SrlXmlExtensionSlot],validate:Option<SrlXmlDeclarationValidator>}
struct SrlXmlExtensionModel {identity:&'static str,fields:&'static[SrlXmlExtensionField],contract:Option<&'static SrlXmlExtensionContract>,namespace_text:u8,namespace_enumeration:&'static [&'static [(&'static str,&'static str)]],flexible_children:bool,mixed_text:bool,map_key_kind:u8,scalar_rule:Option<&'static SrlXmlRule>}
/// Nil has no QName text value; its lexical/content validity stays with the generated owning reader.
fn srl_xml_extension_nil(attributes:&[(SrlXmlTokenName<'_>,String)])->bool {
    attributes.iter().any(|(name,value)|name.uri=="http://www.w3.org/2001/XMLSchema-instance"&&name.local=="nil"&&matches!(value.trim_matches([' ','\t','\r','\n']),"true"|"1"))
}
/// Select typed variant edges by the instance's expanded xsi:type without changing reader state.
fn srl_xml_extension_typed_field(input:&SrlXmlInput<'_,'_>,field:&SrlXmlExtensionField)->bool {
    let Some((uri,local))=field.type_identity else{return true};
    let SrlXmlToken::Start(_,attributes)=&input.current else{return false};
    let Some((_,value))=attributes.iter().find(|(name,_)|name.uri=="http://www.w3.org/2001/XMLSchema-instance"&&name.local=="type") else{return false};
    let value=value.trim_matches([' ','\t','\r','\n']);let (prefix,name)=value.split_once(':').unwrap_or(("",value));
    let scope=&input.bindings;
    let actual=if prefix=="xml" {"http://www.w3.org/XML/1998/namespace"}else{scope.iter().rev().find(|(name,_)|*name==prefix).map_or("",|(_,uri)|uri.as_str())};
    actual==uri&&name==local
}
/// Give QName scalar occurrences independent namespace scopes without adding native record fields.
fn srl_xml_namespace_model_1()->&'static SrlXmlExtensionModel {
    static MODEL:SrlXmlExtensionModel=SrlXmlExtensionModel{identity:"serializer:QName",fields:&[],contract:None,namespace_text:1,namespace_enumeration:&[],flexible_children:false,mixed_text:false,map_key_kind:0,scalar_rule:None};&MODEL
}
/// Keep each QName-list child bound to its original local namespace declarations.
fn srl_xml_namespace_model_2()->&'static SrlXmlExtensionModel {
    static MODEL:SrlXmlExtensionModel=SrlXmlExtensionModel{identity:"serializer:QName-list",fields:&[],contract:None,namespace_text:2,namespace_enumeration:&[],flexible_children:false,mixed_text:false,map_key_kind:0,scalar_rule:None};&MODEL
}
/// Retain expanded prefix-list identities independently of the typed owner's namespace scope.
fn srl_xml_namespace_model_3()->&'static SrlXmlExtensionModel {
    static MODEL:SrlXmlExtensionModel=SrlXmlExtensionModel{identity:"serializer:prefix-list",fields:&[],contract:None,namespace_text:3,namespace_enumeration:&[],flexible_children:false,mixed_text:false,map_key_kind:0,scalar_rule:None};&MODEL
}
/// Use XML fifth-edition NCNames rather than locale-dependent alphabetic classifications.
fn srl_xml_namespace_name(value:&str)->bool {
    !value.is_empty()&&value.chars().enumerate().all(|(index,value)|srl_xml_scalar_name_character(value as u32,index==0,false))
}
/// Validate the exact QName/prefix-list tokens against a complete saved namespace scope.
fn srl_xml_namespace_tokens(raw:&str,kind:u8,scope:&[(String,String)],maximum:usize,enumeration:&[&[(&str,&str)]])->Result<(),Error> {
    if kind==0 {return Ok(());}let mut count=0usize;
    let bound=|prefix:&str|prefix=="xml"||scope.iter().rev().any(|(name,uri)|name==prefix&&!uri.is_empty());
    for value in raw.split([' ','\t','\r','\n']).filter(|value|!value.is_empty()) {
        if count==maximum {return Err(Error("XML namespace token limit"));}count+=1;
        if kind==3 {
            if value=="xmlns"||!srl_xml_namespace_name(value)||!bound(value) {return Err(Error("Invalid or unbound XML prefix-list token"));}
        } else if let Some((prefix,local))=value.split_once(':') {
            if prefix=="xmlns"||!srl_xml_namespace_name(prefix)||!srl_xml_namespace_name(local)||!bound(prefix) {return Err(Error("Invalid or unbound XML QName"));}
        } else if !srl_xml_namespace_name(value) {return Err(Error("Invalid XML QName"));}
    }
    if kind==1&&count!=1 {return Err(Error("XML QName requires one token"));}
    if !enumeration.is_empty() && !enumeration.iter().any(|allowed| {
        allowed.len()==count && raw.split([' ','\t','\r','\n']).filter(|value|!value.is_empty()).zip(allowed.iter()).all(|(value,(uri,local))| {
            let (prefix,name)=value.split_once(':').unwrap_or(("",value));
            let actual=if prefix=="xml" {"http://www.w3.org/XML/1998/namespace"}else{scope.iter().rev().find(|(name,_)|name==prefix).map(|(_,uri)|uri.as_str()).unwrap_or("")};
            actual==*uri && name==*local
        })
    }) {return Err(Error("XML QName enumeration facet failed"));}Ok(())
}
/// Resolve a QName with the nearest occurrence binding, including implicit xml and empty defaults.
fn srl_xml_scalar_expanded<'a>(value:&'a str,scope:&'a [(String,String)])->(&'a str,&'a str) {
    let (prefix,local)=value.split_once(':').unwrap_or(("",value));
    let uri=if prefix=="xml"{"http://www.w3.org/XML/1998/namespace"}else{scope.iter().rev().find(|(name,_)|name==prefix).map_or("",|(_,uri)|uri.as_str())};(uri,local)
}
impl SrlXmlRule {
 /// Compare schema-selected identities without reinterpreting their authored prefixes in instance scope.
 fn scoped_identity(&self,text:&str,identity:SrlXmlScalarIdentity,scope:&[(String,String)],work:&mut SrlXmlScalarWork)->Result<bool,Error> {
    work.spend(text.len().saturating_add(1))?;let mut expected=Self::item(identity.kind);expected.list_kind=identity.list_kind;
    if self.value_family()!=expected.value_family(){return Ok(false);}
    if !self.list_kind.is_empty()&&(!identity.items.is_empty()||identity.names.is_empty()){
        let values=srl_xml_scalar_tokens_with_operation(text,work.max_items,work.operation.as_ref())?;
        if values.len()!=identity.items.len(){return Ok(false);}let builtin=Self::item(self.list_kind);
        for (value,expected) in values.into_iter().zip(identity.items){let item=if let Some(item)=self.item_rule{item}else{&builtin};let(text,leaf)=item.check_scoped(value,scope,work)?;
            if !leaf.scoped_identity(&text,*expected,scope,work)?{return Ok(false);}}
        return Ok(true);
    }
    if self.kind=="QName"||self.list_kind=="QName" {
        if self.list_kind.is_empty()!=identity.list_kind.is_empty(){return Ok(false);}
        let values=srl_xml_scalar_tokens_with_operation(text,work.max_items,work.operation.as_ref())?;
        return Ok(values.len()==identity.names.len()&&values.into_iter().zip(identity.names).all(|(value,name)|srl_xml_scalar_expanded(value,scope)==*name));
    }
    self.identity_equal(text,&expected,identity.lexical,work)
 }
 /// Namespace binding is part of member validity; resource failures never become union fallback.
 fn check_scoped<'s>(&'s self,raw:&str,scope:&[(String,String)],work:&mut SrlXmlScalarWork)->Result<(String,&'s Self),Error> {
    let mut selected=self;let mut candidate=None;
    if !self.members.is_empty(){for member in self.members{match member.check_scoped(raw,scope,work){
        Ok((text,leaf))=>{candidate=Some(text);selected=leaf;break;},
        Err(error) if matches!(error.0,"XML operation failed"|"XML scalar work limit"|"XML scalar allocation"|"XML pattern input limit"|"XML list item limit")=>return Err(error),Err(_)=>{}
    }}if candidate.is_none(){return Err(Error("XML union has no member"));}}
    let mut outer=*self;if !self.members.is_empty(){outer.kind="";outer.whitespace="preserve";outer.members=&[];}
    if !self.identities.is_empty(){outer.enumeration=&[];}if self.fixed_identity.is_some(){outer.fixed=None;}
    outer.identities=&[];outer.fixed_identity=None;outer.item_rule=None;
    let (text,_)=outer.check_selected(candidate.as_deref().unwrap_or(raw),work)?;
    if let Some(item)=self.item_rule{for token in srl_xml_scalar_tokens_with_operation(&text,work.max_items,work.operation.as_ref())?{item.check_scoped(token,scope,work)?;}}
    if self.members.is_empty()&&(self.kind=="QName"||self.list_kind=="QName"){
        srl_xml_namespace_tokens(&text,if self.list_kind.is_empty(){1}else{2},scope,work.max_items,&[])?;
    }
    if !self.identities.is_empty(){let mut matched=false;for identity in self.identities{if selected.scoped_identity(&text,*identity,scope,work)?{matched=true;break;}}if !matched{return Err(Error("XML enumeration facet"));}}
    if let Some(identity)=self.fixed_identity{if !selected.scoped_identity(&text,identity,scope,work)?{return Err(Error("XML fixed value"));}}
    Ok((text,selected))
 }
}
/// Complete scoped scalar admission precedes transactional publication of the native document.
fn srl_xml_namespace_value(raw:&str,kind:u8,scope:&[(String,String)],maximum:usize,enumeration:&[&[(&str,&str)]],rule:Option<&SrlXmlRule>,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error> {
    let operation=operation.cloned();
    if kind==0{return Ok(());}@xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::WorkUnits,raw.len().saturating_mul(scope.len().saturating_add(1)))?;if let Some(rule)=rule{let mut work=SrlXmlScalarWork{remaining:SRL_XML_SCALAR_MAX_WORK,max_items:maximum,operation:operation.clone()};rule.check_scoped(raw,scope,&mut work)?;}
    if kind==4{Ok(())}else{srl_xml_namespace_tokens(raw,kind,scope,maximum,enumeration)}
}
/// A graph path uses static field/repeated-item positions, never unstable native object addresses.
struct SrlXmlExtensionOwner {path:Vec<(usize,SrlXmlExtensionIndex)>,identity:&'static str,scope:Vec<(String,String)>,attributes:Vec<(String,String,String)>,children:Vec<SrlXmlExtensionChild>,defaults:Vec<usize>}
/// An unchanged schema-origin literal is emitted as absence/empty text, independently of instance aliases.
fn srl_xml_extension_default_value(owner:Option<&SrlXmlExtensionOwner>,field:usize,rule:Option<&SrlXmlRule>,value:&str)->bool {
    owner.is_some_and(|owner|owner.defaults.contains(&field))&&rule.is_some_and(|rule|rule.schema_default==Some(value))
}
/// Map entries follow canonical typed keys; sequences retain ordinary occurrence positions.
#[derive(Clone,PartialEq,Eq)]
enum SrlXmlExtensionIndex {Ordinal(usize),Key(String)}
const SRL_XML_MAP_LEXICAL_KEY:u8=1;
const SRL_XML_MAP_BOOLEAN_KEY:u8=3;
/// Canonicalize supported map keys without hashes, preserving string and enum byte identities.
fn srl_xml_extension_map_path(path:&mut[(usize,SrlXmlExtensionIndex)],text:&str,kind:u8,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error> {
    let operation=operation.cloned();
    let value=if kind==SRL_XML_MAP_LEXICAL_KEY {srl_xml_extension_owned(text,operation.as_ref())?}else{
        let text=text.trim_matches([' ','\t','\r','\n']);
        if kind==SRL_XML_MAP_BOOLEAN_KEY {match text {"true"|"1"=>srl_xml_extension_owned("true",operation.as_ref())?,"false"|"0"=>srl_xml_extension_owned("false",operation.as_ref())?,_=>return Err(Error("Invalid XML map boolean key"))}})rustext"
R"rustext(
        else {let negative=text.starts_with('-');let digits=text.strip_prefix('+').or_else(||text.strip_prefix('-')).unwrap_or(text);
            if digits.is_empty()||!digits.bytes().all(|value|value.is_ascii_digit()){return Err(Error("Invalid XML map integer key"));}
            let digits=digits.trim_start_matches('0');if digits.is_empty(){srl_xml_extension_owned("0",operation.as_ref())?}else if negative{srl_xml_extension_join("-",digits,operation.as_ref())?}else{srl_xml_extension_owned(digits,operation.as_ref())?}}
    };
    path.last_mut().ok_or(Error("XML map key has no ownership path"))?.1=SrlXmlExtensionIndex::Key(value);Ok(())
}
/// Copy one scalar key into transport while borrowing no completed XML subtree.
fn srl_xml_extension_map_scalar(input:&mut SrlXmlInput<'_,'_>,output:&mut String,field:&SrlXmlExtensionField,maximum:usize)->Result<String,Error>{
    let operation=input.operation.clone();
    let lexical=srl_xml_extension_start(output,input,&[],None,maximum)?;
    let value=input.scalar(field.uri,field.local)?;srl_xml_extension_escape(output,&value,false,maximum,operation.as_ref())?;
    srl_xml_extension_end(output,&lexical,maximum,operation.as_ref())?;Ok(value)
}
struct SrlXmlExtensionChild {position:usize,item:usize,uri:String,xml:String}
/// Charge operation-wide preservation before making any corresponding owning allocation.
fn srl_xml_extension_charge(state:&mut XmlExtensionState,limits:XmlExtensionLimits,count:usize,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error> {
    let bytes=state.bytes.checked_add(count).filter(|value|*value<=limits.max_preserved_bytes).ok_or(Error("XML extension preserved-byte limit"))?;
    if let Some(operation)=operation{operation.preserve_bytes(count)?;}state.bytes=bytes;Ok(())
}
/// Append bounded canonical transport bytes; this buffer contains no retained known XML tree.
fn srl_xml_extension_put(output:&mut String,value:&str,maximum:usize,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error> {
    let operation=operation.cloned();
    if value.len()>maximum.saturating_sub(output.len()) {return Err(Error("XML extension transport byte limit"));}
    @xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::WorkUnits,value.len().saturating_add(1))?;
    let required=output.len().checked_add(value.len()).ok_or(Error("XML extension transport byte limit"))?;
    if required>output.capacity(){let capacity=required.max(output.capacity().checked_mul(2).unwrap_or(required)).min(maximum);@xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,capacity-output.capacity())?;output.try_reserve_exact(capacity-output.len()).map_err(|_|@xml_operation@::quota(operation.as_ref(),"XML extension transport allocation"))?;}output.push_str(value);Ok(())
}
/// Escape semantic XML values, preserving referenced whitespace and carriage returns.
fn srl_xml_extension_escape(output:&mut String,value:&str,attribute:bool,maximum:usize,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error> {
    let operation=operation.cloned();
    for value in value.chars() {let mut bytes=[0;4];let text=match value {'&'=>"&amp;",'<'=>"&lt;",'>'=>"&gt;",'"' if attribute=>"&quot;",'\r'=>"&#13;",'\n' if attribute=>"&#10;",'\t' if attribute=>"&#9;",_=>value.encode_utf8(&mut bytes)};srl_xml_extension_put(output,text,maximum,operation.as_ref())?;}Ok(())
}
/// Flatten only an admitted original scope, preserving unused aliases and nearest shadowing.
fn srl_xml_extension_scope(input:&SrlXmlInput<'_,'_>)->Result<Vec<(String,String)>,Error> {
    let operation=input.operation.as_ref();let mut result=std::collections::BTreeMap::new();
    srl_xml_extension_binding(&mut result,"xml",SRL_XML_NAMESPACE,operation)?;srl_xml_extension_binding(&mut result,"","",operation)?;
    for(prefix,uri)in &input.bindings{srl_xml_extension_binding(&mut result,prefix,uri,operation)?;}
    let mut values=Vec::new();srl_xml_extension_reserve(&mut values,result.len(),operation)?;values.extend(result);Ok(values)
}
/// Check semantic capabilities against original namespace bindings before any typed model is published.
fn srl_xml_extension_capabilities(contract:&SrlXmlExtensionContract,scope:&[(String,String)],options:&XmlExtensionOptions)->Result<(),Error> {
    if contract.slots.len()>options.limits.max_slots || options.understood_namespaces.len()>options.limits.max_items {return Err(Error("XML extension slot/capability limit"));}
    let understood=|uri:&str|uri==contract.core || options.understood_namespaces.iter().any(|value|value==uri);
    for uri in contract.required {if !understood(uri) {return Err(Error("Required XML capability is not understood"));}}
    let mut count=0;for prefix in options.required_prefixes.split([' ','\t','\r','\n']).filter(|value|!value.is_empty()) {
        count+=1;if count>options.limits.max_items || prefix=="xmlns" || !srl_xml_extension_prefix(prefix) {return Err(Error("Invalid required XML capability prefix"));}
        let uri=if prefix=="xml" {SRL_XML_NAMESPACE}else{scope.iter().rev().find(|(name,_)|name==prefix).map(|(_,uri)|uri.as_str()).filter(|uri|!uri.is_empty()).ok_or(Error("Unbound required XML capability prefix"))?};
        if !understood(uri) {return Err(Error("Required XML capability is not understood"));}
    }Ok(())
}
/// Prefix inventories accept the same Unicode NCName ranges as the generated XML tokenizer.
fn srl_xml_extension_prefix(value:&str)->bool {
    let mut chars=value.chars();let start=|c:char|c=='_' || c.is_alphabetic() || matches!(c as u32,0xc0..=0xd6|0xd8..=0xf6|0xf8..=0x2ff|0x370..=0x37d|0x37f..=0x1fff|0x200c..=0x200d|0x2070..=0x218f|0x2c00..=0x2fef|0x3001..=0xd7ff|0xf900..=0xfdcf|0xfdf0..=0xfffd|0x10000..=0xeffff);
    chars.next().is_some_and(start) && chars.all(|c|start(c)||c.is_ascii_digit()||matches!(c,'-'|'.'|'\u{b7}'|'\u{300}'..='\u{36f}'|'\u{203f}'..='\u{2040}'))
}
/// Match generated namespace predicates without broadening authored foreign-only slots.
fn srl_xml_extension_namespace(contract:&SrlXmlExtensionContract,slot:&SrlXmlExtensionSlot,uri:&str)->bool {
    if matches!(slot.namespace_match,0|4) && (uri.is_empty()||uri==contract.core) {return false;}
    match slot.namespace_match {1=>true,3|4=>!slot.uris.contains(&uri),_=>slot.uris.contains(&uri)}
}
/// Select an exact wildcard boundary after kind-aware known-field dispatch.
fn srl_xml_extension_slot(model:&SrlXmlExtensionModel,uri:&str,local:&str,attribute:bool,position:usize)->Option<usize> {
    let contract=model.contract?;
    if uri=="urn:serializer:xml:model" {return None;}
    let known=model.fields.iter().any(|field|field.attribute==attribute && field.uri==uri && field.local==local);
    contract.slots.iter().position(|slot|slot.attribute==attribute && (attribute||slot.position==usize::MAX||slot.position==position) &&
        (!known || !attribute&&!matches!(slot.namespace_match,0|4)) && srl_xml_extension_namespace(contract,slot,uri))
}
/// Consume a required fixed leading wildcard before dispatching the next owning native field.
fn srl_xml_extension_required_wildcard(model:&SrlXmlExtensionModel,uri:&str,position:usize,field:usize,counts:&[usize])->bool {
    model.contract.is_some_and(|contract|contract.slots.iter().enumerate().any(|(index,slot)|
        !slot.attribute&&slot.namespace_match!=0&&slot.position>=position&&slot.position<=field&&
        counts[index]<slot.minimum&&srl_xml_extension_namespace(contract,slot,uri)))
}
/// Admit every preserve/skip root cumulatively, including skip payloads that retain no opaque state.
fn srl_xml_extension_admit(state:&mut XmlExtensionState,model:&SrlXmlExtensionModel,counts:&mut[usize],slot:usize,options:&XmlExtensionOptions)->Result<bool,Error> {
    let selected=&model.contract.ok_or(Error("XML extension has no permitted slot"))?.slots[slot];
    if selected.policy==0 {return Err(Error("XML extension has a reject slot"));}
    state.items=state.items.checked_add(1).filter(|value|*value<=options.limits.max_items).ok_or(Error("XML extension item limit"))?;
    counts[slot]=counts[slot].checked_add(1).filter(|value|selected.maximum.is_none_or(|maximum|*value<=maximum)).ok_or(Error("XML extension occurrence limit"))?;Ok(selected.policy==2&&!state.validation_only)
}
)rustext"
R"rustext(
/// Merge nearest lexical bindings without changing unknown values that merely resemble QNames.
fn srl_xml_extension_merge(scope:&mut Vec<(String,String)>,values:&[(String,String)],operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error> {
    for(prefix,uri)in values{@xml_operation@::consume(operation,@xml_operation@::Resource::WorkUnits,scope.len().saturating_add(1))?;
        if let Some((_,value))=scope.iter_mut().find(|(name,_)|name==prefix){*value=srl_xml_extension_owned(uri,operation)?;}
        else{srl_xml_extension_reserve(scope,1,operation)?;scope.push((srl_xml_extension_owned(prefix,operation)?,srl_xml_extension_owned(uri,operation)?));}}
    Ok(())
}
/// Revalidate a retained item and place it at its exact static-field/repeated-item boundary.
fn srl_xml_extension_replay_children(output:&mut String,owner:Option<&SrlXmlExtensionOwner>,cursor:&mut usize,index:usize,items:&[usize],model:&SrlXmlExtensionModel,counts:&mut[usize],admission:&mut XmlExtensionState,options:&XmlExtensionOptions,maximum:usize,limits:XmlLimits,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error> {
    let operation=operation.cloned();
    let Some(owner)=owner else{return Ok(())};
    while let Some(child)=owner.children.get(*cursor) {
        if child.position>index {break;}
        let count=items.get(child.position).copied().unwrap_or(0);
        if child.position==index && child.item>count {break;}
        if child.item!=count {return Err(Error("Preserved XML extension has an edited field position"));}
        let slot=srl_xml_extension_slot(model,&child.uri,"",false,child.position).ok_or(Error("Preserved XML extension has no permitted slot"))?;
        srl_xml_extension_validate(model,slot,"","",None,&[],&child.xml,limits,operation.as_ref().map(@xml_operation@::State::secondary))?;
        if !srl_xml_extension_admit(admission,model,counts,slot,options)? {return Err(Error("Preserved XML extension no longer has a preserve slot"));}
        srl_xml_extension_put(output,&child.xml,maximum,operation.as_ref())?;*cursor+=1;
    }Ok(())
}
/// Replay borrowed sidecar items while traversing only the generated codec's known transport tokens.
fn srl_xml_extension_replay_owner(input:&mut SrlXmlInput<'_,'_>,output:&mut String,model:&'static SrlXmlExtensionModel,path:&mut Vec<(usize,SrlXmlExtensionIndex)>,state:&XmlExtensionState,used:&mut[bool],admission:&mut XmlExtensionState,options:&XmlExtensionOptions,inherited:&[(String,String)],maximum:usize,limits:XmlLimits)->Result<(),Error> {
    let operation=input.operation.clone();
    let matched=state.owners.iter().enumerate().find(|(_,owner)|model.map_key_kind==0&&owner.path==*path);
    let owner=matched.map(|(index,owner)|{used[index]=true;owner});
    if owner.is_some_and(|owner|owner.identity!=model.identity) {return Err(Error("Preserved XML extension owner type changed"));}
    // Retained contexts are complete original scopes; canonical writer aliases must not accumulate on replay.
    let mut scope=srl_xml_extension_scope_copy(owner.map_or(inherited,|owner|&owner.scope),operation.as_ref())?;if owner.is_none() {srl_xml_extension_merge(&mut scope,&srl_xml_extension_scope(input)?,operation.as_ref())?;}
    if let Some(contract)=model.contract {srl_xml_extension_capabilities(contract,&scope,options)?;}
    let mut counts=srl_xml_extension_zeros::<usize>(model.contract.map_or(0,|contract|contract.slots.len()),operation.as_ref())?;
    let mut attributes=match std::mem::replace(&mut input.current,SrlXmlToken::Eof) {SrlXmlToken::Start(name,attributes)=>{input.current=SrlXmlToken::Start(name,Vec::new());attributes},_=>return Err(Error("Expected typed XML start"))};
    attributes.retain(|(name,value)|!model.fields.iter().enumerate().any(|(index,field)|field.attribute&&field.uri==name.uri&&field.local==name.local&&srl_xml_extension_default_value(owner,index,field.scalar_rule,value)));
    // Canonical type aliases are private writer choices; keep opaque and QName payload aliases intact.
    for (name,value) in &mut attributes {
        if name.uri!="http://www.w3.org/2001/XMLSchema-instance"||name.local!="type" {continue;}
        let source=srl_xml_extension_scope(input)?;let (prefix,local)=value.split_once(':').unwrap_or(("",value.as_str()));
        let uri=if prefix=="xml" {"http://www.w3.org/XML/1998/namespace"}else{source.iter().rev().find(|(name,_)|name==prefix).map_or("",|(_,uri)|uri.as_str())};
        if uri.is_empty() {
            if scope.iter().rev().any(|(prefix,uri)|prefix.is_empty()&&!uri.is_empty()) {return Err(Error("Edited XML type conflicts with retained default namespace"));}
            *value=srl_xml_extension_owned(local,operation.as_ref())?;continue;
        }
        let alias=if let Some((prefix,_))=scope.iter().find(|(_,value)|value==uri) {srl_xml_extension_owned(prefix,operation.as_ref())?}else{
            let mut ordinal=0usize;let selected=loop{let candidate=srl_xml_extension_alias("srl_type",ordinal,operation.as_ref())?;if !scope.iter().any(|(name,_)|name==&candidate){break candidate;}ordinal=ordinal.checked_add(1).ok_or(Error("XML type alias limit"))?;};
            srl_xml_extension_reserve(&mut scope,1,operation.as_ref())?;scope.push((srl_xml_extension_owned(&selected,operation.as_ref())?,srl_xml_extension_owned(uri,operation.as_ref())?));selected
        };
        *value=srl_xml_extension_lexical(&alias,local,operation.as_ref())?;
    }
    let namespace_kind=if srl_xml_extension_nil(&attributes) {0}else{model.namespace_text};
    let mut map_key_ready=model.map_key_kind==0;
    if model.map_key_kind!=0&&model.fields[0].attribute {
        let key=&model.fields[0];let value=attributes.iter().find(|(name,_)|name.uri==key.uri&&name.local==key.local).ok_or(Error("XML map key missing"))?;
        srl_xml_extension_map_path(path,&value.1,model.map_key_kind,operation.as_ref())?;map_key_ready=true;
    }
    if let Some(owner)=owner {for (uri,local,value) in &owner.attributes {
        let slot=srl_xml_extension_slot(model,uri,local,true,0).ok_or(Error("Preserved XML attribute has no permitted slot"))?;
        srl_xml_extension_validate(model,slot,uri,local,Some(value),&scope,"",limits,operation.as_ref().map(@xml_operation@::State::secondary))?;
        if !srl_xml_extension_admit(admission,model,&mut counts,slot,options)? {return Err(Error("Preserved XML attribute no longer has a preserve slot"));}
        srl_xml_extension_reserve(&mut attributes,1,operation.as_ref())?;attributes.push((SrlXmlTokenName {uri:srl_xml_extension_owned(uri,operation.as_ref())?,local},srl_xml_extension_owned(value,operation.as_ref())?));
    }}
    for (name,value) in &attributes {if let Some(field)=model.fields.iter().find(|field|field.attribute&&field.uri==name.uri&&field.local==name.local) {srl_xml_namespace_value(value,field.namespace_kind,&scope,input.limits.max_nodes,field.namespace_enumeration,field.scalar_rule,operation.as_ref())?;}}
    let lexical=srl_xml_extension_start(output,input,&attributes,Some(&scope),maximum)?;input.advance()?;
    let mut namespace_value=String::new();
    let mut items=srl_xml_extension_zeros::<usize>(model.fields.len(),operation.as_ref())?;let mut cursor=0;
    loop {match &input.current {
        SrlXmlToken::Start(name,_)=>{
            let index=model.fields.iter().position(|field|!field.attribute && field.uri==name.uri && field.local==name.local && srl_xml_extension_typed_field(input,field)).ok_or(Error("Unexpected known XML transport child"))?;
            srl_xml_extension_replay_children(output,owner,&mut cursor,index,&items,model,&mut counts,admission,options,maximum,limits,operation.as_ref())?;
            let item=items[index];items[index]+=1;
            if model.map_key_kind!=0&&index==0&&!model.fields[0].attribute {
                if map_key_ready{return Err(Error("Duplicate XML map key"));}
                let value=srl_xml_extension_map_scalar(input,output,&model.fields[0],maximum)?;
                srl_xml_extension_map_path(path,&value,model.map_key_kind,operation.as_ref())?;map_key_ready=true;continue;
            }
            if !map_key_ready{return Err(Error("XML map value precedes its key"));}
            if let Some(child)=model.fields[index].child {srl_xml_extension_reserve(path,1,operation.as_ref())?;path.push((index,SrlXmlExtensionIndex::Ordinal(item)));srl_xml_extension_replay_owner(input,output,child(),path,state,used,admission,options,&scope,maximum,limits)?;path.pop();}
            else {srl_xml_extension_foreign(input,Some(output),maximum,false)?;}
        },
        SrlXmlToken::Text(value)=>{if model.namespace_text!=0 {srl_xml_extension_put(&mut namespace_value,value,input.limits.max_text_bytes,operation.as_ref())?;}if model.scalar_rule.is_none_or(|rule|rule.schema_default.is_none()){srl_xml_extension_escape(output,value,false,maximum,operation.as_ref())?;}input.advance()?;},
        SrlXmlToken::End(_)=>{
            if !map_key_ready{return Err(Error("XML map key missing"));}
            if !srl_xml_extension_default_value(owner,usize::MAX,model.scalar_rule,&namespace_value){
                srl_xml_namespace_value(&namespace_value,namespace_kind,&scope,input.limits.max_nodes,model.namespace_enumeration,model.scalar_rule,operation.as_ref())?;
                if model.scalar_rule.is_some_and(|rule|rule.schema_default.is_some()){srl_xml_extension_escape(output,&namespace_value,false,maximum,operation.as_ref())?;}
            }
            srl_xml_extension_replay_children(output,owner,&mut cursor,model.fields.len(),&items,model,&mut counts,admission,options,maximum,limits,operation.as_ref())?;
            if owner.is_some_and(|owner|cursor!=owner.children.len()) {return Err(Error("Preserved XML extension has an edited field position"));}
            if let Some(contract)=model.contract {for (slot,count) in contract.slots.iter().zip(counts) {if count<slot.minimum {return Err(Error("Required XML extension occurrence missing"));}}}
            srl_xml_extension_end(output,&lexical,maximum,operation.as_ref())?;input.advance()?;return Ok(());
        },
        SrlXmlToken::Eof=>return Err(Error("Unterminated known XML transport owner")),
    }}
}
/// Fence metadata transport separately after original XML has already passed its complete logical limits.
fn srl_xml_extension_transport_limits(text:&str,limits:XmlLimits)->Result<XmlLimits,Error> {
    let mut result=limits;result.max_bytes=text.len();result.max_attributes=text.len();result.max_text_bytes=limits.max_text_bytes.max(text.len().checked_mul(2).ok_or(Error("XML extension transport limit"))?);Ok(result)
}
/// Allow bounded canonical aliases while retaining native node/depth admission for modeled values.
fn srl_xml_extension_output_limits(limits:XmlLimits)->Result<XmlLimits,Error> {
    let maximum=limits.max_bytes.checked_mul(SRL_XML_EXTENSION_TRANSPORT_FRAMING_EXPANSION).ok_or(Error("XML extension transport limit"))?;
    Ok(XmlLimits{max_bytes:maximum,max_attributes:maximum,max_text_bytes:limits.max_text_bytes.max(maximum.checked_mul(2).ok_or(Error("XML extension transport limit"))?),..limits})
}
/// Filter the original bounded document before any native constructor; keep the sidecar private on failure.
fn srl_xml_extension_filter(text:&str,limits:XmlLimits,options:&XmlExtensionOptions,model:&'static SrlXmlExtensionModel)->Result<(String,XmlExtensionState),Error>{srl_xml_extension_filter_with_operation(text,limits,options,model,None,None)}
fn srl_xml_extension_filter_with_operation(text:&str,limits:XmlLimits,options:&XmlExtensionOptions,model:&'static SrlXmlExtensionModel,operation:Option<@xml_operation@::State<'_>>,package:Option<@xml_operation@::PackageContract>)->Result<(String,XmlExtensionState),Error> {
    srl_xml_extension_filter_mode(text,limits,options,model,operation,package,false)
}
/// Temporary declaration DTOs validate recursively without retaining opaque presentation.
fn srl_xml_extension_filter_validation(text:&str,limits:XmlLimits,options:&XmlExtensionOptions,model:&'static SrlXmlExtensionModel,operation:Option<@xml_operation@::State<'_>>)->Result<(String,XmlExtensionState),Error>{
    srl_xml_extension_filter_mode(text,limits,options,model,operation,None,true)
}
/// Select private validation storage independently of the authored wildcard retention policy.
fn srl_xml_extension_filter_mode(text:&str,limits:XmlLimits,options:&XmlExtensionOptions,model:&'static SrlXmlExtensionModel,operation:Option<@xml_operation@::State<'_>>,package:Option<@xml_operation@::PackageContract>,validation_only:bool)->Result<(String,XmlExtensionState),Error> {
    options.validate()?;
    // Canonical names can expand framing. This explicit buffer bound is additional to, and never replaces, original XML admission.
    let maximum=srl_xml_extension_output_limits(limits)?.max_bytes;
    let mut input=SrlXmlInput::new_with_operation(text,limits,operation.clone())?;input.skip_space()?;srl_xml_extension_package_capabilities(&mut input,package)?;let mut output=String::new();let mut state=XmlExtensionState{validation_only,..XmlExtensionState::default()};
    srl_xml_extension_filter_owner(&mut input,&mut output,model,&mut Vec::new(),&mut state,options,maximum)?;input.finish_document()?;Ok((output,state))
}
/// Replay an immutable borrowed sidecar and re-admit the complete final document before returning bytes.
fn srl_xml_extension_replay(text:&str,limits:XmlLimits,options:&XmlExtensionOptions,model:&'static SrlXmlExtensionModel,state:&XmlExtensionState)->Result<String,Error>{srl_xml_extension_replay_with_operation(text,limits,options,model,state,None)}
fn srl_xml_extension_replay_with_operation(text:&str,limits:XmlLimits,options:&XmlExtensionOptions,model:&'static SrlXmlExtensionModel,state:&XmlExtensionState,operation:Option<@xml_operation@::State<'_>>)->Result<String,Error> {
    options.validate()?;
    if state.bytes>options.limits.max_preserved_bytes {return Err(Error("XML extension preserved-byte limit"));}
    let framing=srl_xml_extension_output_limits(limits)?;if text.len()>framing.max_bytes {return Err(Error("XML extension transport byte limit"));}
    let mut input=SrlXmlInput::new_with_operation(text,srl_xml_extension_transport_limits(text,limits)?,operation.as_ref().map(@xml_operation@::State::secondary))?;input.skip_space()?;let mut output=String::new();let mut used=srl_xml_extension_zeros::<bool>(state.owners.len(),operation.as_ref())?;let mut admission=XmlExtensionState::default();
    srl_xml_extension_replay_owner(&mut input,&mut output,model,&mut Vec::new(),state,&mut used,&mut admission,options,&[],limits.max_bytes,limits)?;input.finish_document()?;
    if used.iter().any(|value|!*value) {return Err(Error("Preserved XML extension owner has an edited field position"));}
    let mut checked=SrlXmlInput::new_with_operation(&output,limits,operation)?;while !matches!(checked.current,SrlXmlToken::Eof) {checked.advance()?;}checked.finish_document()?;Ok(output)
}
)rustext"
R"rustext(
/// Reuse a compatible original binding; allocate a fresh prefix only for a genuine framing collision.
fn srl_xml_extension_select_prefix(declarations:&mut std::collections::BTreeMap<String,String>,input:&SrlXmlInput<'_,'_>,uri:&str,preferred:&str,attribute:bool)->Result<String,Error> {
    let operation=input.operation.as_ref();
    if uri.is_empty(){if !attribute{srl_xml_extension_binding(declarations,"","",operation)?;}return Ok(String::new());}
    if uri==SRL_XML_NAMESPACE{return srl_xml_extension_owned("xml",operation);}
    let effective=|prefix:&str|declarations.get(prefix).map(String::as_str).or_else(||input.bindings.iter().rev().find(|(value,_)|*value==prefix).map(|(_,value)|value.as_str()));
    @xml_operation@::consume(operation,@xml_operation@::Resource::WorkUnits,declarations.len().saturating_add(input.bindings.len()).saturating_add(1))?;
    let selected=if (!attribute||!preferred.is_empty())&&effective(preferred)==Some(uri){Some(preferred)}
        else if let Some((prefix,_))=declarations.iter().find(|(prefix,value)|(!attribute||!prefix.is_empty())&&value.as_str()==uri){Some(prefix.as_str())}
        else{input.bindings.iter().rev().find(|(prefix,value)|(!attribute||!prefix.is_empty())&&value.as_str()==uri&&effective(prefix)==Some(uri)).map(|(prefix,_)|*prefix)};
    if let Some(prefix)=selected{let result=srl_xml_extension_owned(prefix,operation)?;srl_xml_extension_binding(declarations,&result,uri,operation)?;return Ok(result);}
    let mut serial=0usize;loop{let prefix=srl_xml_extension_alias("srlopaque",serial,operation)?;serial=serial.checked_add(1).ok_or(Error("XML extension alias limit"))?;
        @xml_operation@::consume(operation,@xml_operation@::Resource::WorkUnits,declarations.len().saturating_add(input.bindings.len()).saturating_add(1))?;
        if !declarations.contains_key(&prefix)&&!input.bindings.iter().any(|(value,_)|*value==prefix){srl_xml_extension_binding(declarations,&prefix,uri,operation)?;return Ok(prefix);}}
}
/// Serialize expanded names while leaving every opaque lexical binding intact and replay stable.
fn srl_xml_extension_start(output:&mut String,input:&SrlXmlInput<'_,'_>,attributes:&[(SrlXmlTokenName<'_>,String)],scope:Option<&[(String,String)]>,maximum:usize)->Result<String,Error> {
    let operation=input.operation.clone();
    let (name,_) = match &input.current {SrlXmlToken::Start(name,values)=>(name,values),_=>return Err(Error("Expected XML extension start"))};
    let mut declarations=std::collections::BTreeMap::<String,String>::new();
    if let Some(scope)=scope {for (prefix,uri) in scope {srl_xml_extension_binding(&mut declarations,prefix,uri,operation.as_ref())?;}}
    else {let rollback=input.ancestors.last().ok_or(Error("XML extension scope"))?.bindings;for (prefix,uri) in &input.bindings[rollback..] {srl_xml_extension_binding(&mut declarations,prefix,uri,operation.as_ref())?;}}
    let preferred=input.ancestors.last().ok_or(Error("XML extension scope"))?.prefix;
    let prefix=srl_xml_extension_select_prefix(&mut declarations,input,&name.uri,preferred,false)?;
    let lexical=srl_xml_extension_lexical(&prefix,name.local,operation.as_ref())?;
    let mut values=Vec::new();for (name,value) in attributes {let prefix=srl_xml_extension_select_prefix(&mut declarations,input,&name.uri,"",true)?;let lexical=srl_xml_extension_lexical(&prefix,name.local,operation.as_ref())?;srl_xml_extension_reserve(&mut values,1,operation.as_ref())?;values.push((lexical,value));}
    srl_xml_extension_put(output,"<",maximum,operation.as_ref())?;srl_xml_extension_put(output,&lexical,maximum,operation.as_ref())?;
    for (prefix,uri) in declarations {if prefix=="xml" {continue;}srl_xml_extension_put(output," xmlns",maximum,operation.as_ref())?;if !prefix.is_empty() {srl_xml_extension_put(output,":",maximum,operation.as_ref())?;srl_xml_extension_put(output,&prefix,maximum,operation.as_ref())?;}srl_xml_extension_put(output,"=\"",maximum,operation.as_ref())?;srl_xml_extension_escape(output,&uri,true,maximum,operation.as_ref())?;srl_xml_extension_put(output,"\"",maximum,operation.as_ref())?;}
    for (name,value) in values {srl_xml_extension_put(output," ",maximum,operation.as_ref())?;srl_xml_extension_put(output,&name,maximum,operation.as_ref())?;srl_xml_extension_put(output,"=\"",maximum,operation.as_ref())?;srl_xml_extension_escape(output,value,true,maximum,operation.as_ref())?;srl_xml_extension_put(output,"\"",maximum,operation.as_ref())?;}
    srl_xml_extension_put(output,">",maximum,operation.as_ref())?;Ok(lexical)
}
/// End one independently prefixed transport element.
fn srl_xml_extension_end(output:&mut String,name:&str,maximum:usize,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error> {
    let operation=operation.cloned();srl_xml_extension_put(output,"</",maximum,operation.as_ref())?;srl_xml_extension_put(output,name,maximum,operation.as_ref())?;srl_xml_extension_put(output,">",maximum,operation.as_ref())}
/// Traverse an opaque foreign subtree directly; skip retains no completed token or payload.
fn srl_xml_extension_foreign(input:&mut SrlXmlInput<'_,'_>,mut output:Option<&mut String>,maximum:usize,full_scope:bool)->Result<(),Error> {
    let operation=input.operation.clone();
    let attributes=match &input.current {SrlXmlToken::Start(_,attributes)=>attributes,_=>return Err(Error("Expected foreign XML start"))};
    let name=if let Some(target)=output.as_deref_mut() {let scope=if full_scope{Some(srl_xml_extension_scope(input)?)}else{None};srl_xml_extension_start(target,input,attributes,scope.as_deref(),maximum)?}else{String::new()};
    input.advance()?;
    loop {match &input.current {
        SrlXmlToken::Start(..)=>srl_xml_extension_foreign(input,output.as_deref_mut(),maximum,false)?,
        SrlXmlToken::Text(value)=>{if let Some(target)=output.as_deref_mut() {srl_xml_extension_escape(target,value,false,maximum,operation.as_ref())?;}input.advance()?;},
        SrlXmlToken::End(_)=>{if let Some(target)=output {srl_xml_extension_end(target,&name,maximum,operation.as_ref())?;}input.advance()?;return Ok(());},
        SrlXmlToken::Eof=>return Err(Error("Unterminated foreign XML")),
    }}
}
/// Retain an original owner scope lazily, after its complete logical size has been admitted.
fn srl_xml_extension_owner(state:&mut XmlExtensionState,model:&SrlXmlExtensionModel,path:&[(usize,SrlXmlExtensionIndex)],input:&SrlXmlInput<'_,'_>,binding_end:usize,options:&XmlExtensionOptions)->Result<usize,Error> {
    let operation=input.operation.clone();
    if let Some(index)=state.owners.iter().position(|owner|owner.path==path) {return Ok(index);}
    let mut size=64usize.checked_add(path.len().checked_mul(std::mem::size_of::<(usize,SrlXmlExtensionIndex)>()).ok_or(Error("XML extension path limit"))?).ok_or(Error("XML extension path limit"))?;
    for (_,component) in path {if let SrlXmlExtensionIndex::Key(value)=component{size=size.checked_add(value.len()).ok_or(Error("XML extension path limit"))?;}}
    // Charge all active declarations conservatively before flattening; shadowed bindings may cost more, never less.
    for(index,(prefix,uri))in input.bindings[..binding_end].iter().enumerate(){
        @xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::WorkUnits,binding_end-index)?;
        if !input.bindings[index+1..binding_end].iter().any(|(name,_)|name==prefix){size=size.checked_add(64+prefix.len()+uri.len()).ok_or(Error("XML extension scope limit"))?;}}
    if !input.bindings[..binding_end].iter().any(|(prefix,_)|prefix.is_empty()){size=size.checked_add(64).ok_or(Error("XML extension scope limit"))?;}
    srl_xml_extension_charge(state,options.limits,size,operation.as_ref())?;let mut scope=std::collections::BTreeMap::new();srl_xml_extension_binding(&mut scope,"","",operation.as_ref())?;
    for(prefix,uri)in &input.bindings[..binding_end]{srl_xml_extension_binding(&mut scope,prefix,uri,operation.as_ref())?;}
    let mut values=Vec::new();srl_xml_extension_reserve(&mut values,scope.len(),operation.as_ref())?;values.extend(scope);
    let retained_path=srl_xml_extension_path_copy(path,operation.as_ref())?;srl_xml_extension_reserve(&mut state.owners,1,operation.as_ref())?;
    let index=state.owners.len();state.owners.push(SrlXmlExtensionOwner{path:retained_path,identity:model.identity,scope:values,attributes:Vec::new(),children:Vec::new(),defaults:Vec::new()});Ok(index)
}
/// Retain only bounded schema-origin occurrence flags; no fallback prefix is rebound into instance scope.
fn srl_xml_extension_default_add(state:&mut XmlExtensionState,model:&SrlXmlExtensionModel,path:&[(usize,SrlXmlExtensionIndex)],input:&SrlXmlInput<'_,'_>,binding_end:usize,options:&XmlExtensionOptions,field:usize)->Result<(),Error> {
    if state.validation_only{return Ok(());}
    let operation=input.operation.clone();
    let index=srl_xml_extension_owner(state,model,path,input,binding_end,options)?;
    srl_xml_extension_charge(state,options.limits,std::mem::size_of::<usize>(),operation.as_ref())?;
    srl_xml_extension_reserve(&mut state.owners[index].defaults,1,operation.as_ref())?;
    state.owners[index].defaults.push(field);Ok(())
}
/// Filter one typed ordinary owner, preserving only explicitly admitted foreign roots and attributes.
fn srl_xml_extension_filter_owner(input:&mut SrlXmlInput<'_,'_>,output:&mut String,model:&'static SrlXmlExtensionModel,path:&mut Vec<(usize,SrlXmlExtensionIndex)>,state:&mut XmlExtensionState,options:&XmlExtensionOptions,maximum:usize)->Result<(),Error> {
    let operation=input.operation.clone();
    let original_owners=state.owners.len();let binding_end=input.bindings.len();if let Some(contract)=model.contract {srl_xml_extension_capabilities(contract,&srl_xml_extension_scope(input)?,options)?;}
    let mut namespace_owner=model.namespace_text!=0;let mut namespace_value=String::new();
    let mut counts=srl_xml_extension_zeros::<usize>(model.contract.map_or(0,|contract|contract.slots.len()),operation.as_ref())?;let mut known=Vec::new();
    let attributes=match std::mem::replace(&mut input.current,SrlXmlToken::Eof) {SrlXmlToken::Start(name,values)=>{input.current=SrlXmlToken::Start(name,Vec::new());values},_=>return Err(Error("Expected typed XML start"))};
    let namespace_kind=if srl_xml_extension_nil(&attributes) {0}else{model.namespace_text};
    for (name,value) in attributes {
        if let Some(field)=model.fields.iter().find(|field|field.attribute && field.uri==name.uri && field.local==name.local) {
            if field.namespace_kind!=0 {namespace_owner=true;srl_xml_namespace_value(&value,field.namespace_kind,&srl_xml_extension_scope(input)?,input.limits.max_nodes,field.namespace_enumeration,field.scalar_rule,operation.as_ref())?;}srl_xml_extension_reserve(&mut known,1,operation.as_ref())?;known.push((name,value));continue;
        }
        if name.uri=="urn:serializer:xml:model" || (name.uri=="http://www.w3.org/2001/XMLSchema-instance" && matches!(name.local,"nil"|"type")) {srl_xml_extension_reserve(&mut known,1,operation.as_ref())?;known.push((name,value));continue;}
        let slot=srl_xml_extension_slot(model,&name.uri,name.local,true,0).ok_or(Error("XML extension attribute has no permitted slot"))?;
        srl_xml_extension_validate(model,slot,&name.uri,name.local,Some(&value),&srl_xml_extension_scope(input)?,"",input.limits,operation.as_ref().map(@xml_operation@::State::secondary))?;
        if srl_xml_extension_admit(state,model,&mut counts,slot,options)? {let index=srl_xml_extension_owner(state,model,path,input,binding_end,options)?;srl_xml_extension_charge(state,options.limits,48+name.uri.len()+name.local.len()+value.len(),operation.as_ref())?;srl_xml_extension_reserve(&mut state.owners[index].attributes,1,operation.as_ref())?;state.owners[index].attributes.push((name.uri,srl_xml_extension_owned(name.local,operation.as_ref())?,value));}
    }
    let mut map_key_ready=model.map_key_kind==0;
    for (index,field) in model.fields.iter().enumerate(){
        if field.attribute&&field.scalar_rule.is_some_and(|rule|rule.schema_default.is_some())&&!known.iter().any(|(name,_)|field.uri==name.uri&&field.local==name.local){
            srl_xml_extension_default_add(state,model,path,input,binding_end,options,index)?;
        }
    }
    if model.map_key_kind!=0&&model.fields[0].attribute {
        let key=&model.fields[0];let value=known.iter().find(|(name,_)|name.uri==key.uri&&name.local==key.local).ok_or(Error("XML map key missing"))?;
        srl_xml_extension_map_path(path,&value.1,model.map_key_kind,operation.as_ref())?;map_key_ready=true;
    }
    if namespace_owner&&!state.validation_only {srl_xml_extension_owner(state,model,path,input,binding_end,options)?;}
    let namespace_scope=if model.namespace_text!=0 {srl_xml_extension_scope(input)?}else{Vec::new()};
    let lexical=srl_xml_extension_start(output,input,&known,None,maximum)?;input.advance()?;
    // Attribute permissions do not constrain the owning decoder's mixed/all element grammar.
    let positioned=!model.flexible_children&&model.contract.is_some_and(|contract|contract.slots.iter().any(|slot|!slot.attribute));
    let mut closed_children=false;let mut position=0usize;let mut items=srl_xml_extension_zeros::<usize>(model.fields.len(),operation.as_ref())?;
    loop {match &input.current {
        SrlXmlToken::Start(name,_)=>{
            if let Some(index)=model.fields.iter().enumerate().find(|(index,field)|(!positioned || *index>=position) && !field.attribute && field.uri==name.uri && field.local==name.local && srl_xml_extension_typed_field(input,field)).map(|(index,_)|index)
                .filter(|index|!srl_xml_extension_required_wildcard(model,&name.uri,position,*index,&counts)) {
                if closed_children {return Err(Error("Known XML content follows an end extension slot"));}
                let field=&model.fields[index];let item=items[index];items[index]+=1;position=if field.repeated {index}else{index+1};)rustext"
R"rustext(
                if model.map_key_kind!=0&&index==0&&!field.attribute {
                    if map_key_ready{return Err(Error("Duplicate XML map key"));}
                    let value=srl_xml_extension_map_scalar(input,output,field,maximum)?;
                    srl_xml_extension_map_path(path,&value,model.map_key_kind,operation.as_ref())?;map_key_ready=true;continue;
                }
                if !map_key_ready{return Err(Error("XML map value precedes its key"));}
                if let Some(child)=field.child {srl_xml_extension_reserve(path,1,operation.as_ref())?;path.push((index,SrlXmlExtensionIndex::Ordinal(item)));srl_xml_extension_filter_owner(input,output,child(),path,state,options,maximum)?;path.pop();}
                else {srl_xml_extension_foreign(input,Some(output),maximum,false)?;}
            }else{
                let boundary=if model.flexible_children {model.fields.len()}else{position};
                let candidate=(boundary..=model.fields.len()).filter(|index|*index==model.fields.len() || !model.fields[*index].attribute).find_map(|index|srl_xml_extension_slot(model,&name.uri,name.local,false,index).map(|slot|(index,slot))).ok_or(Error("XML extension element has no permitted slot"))?;
                closed_children=model.flexible_children;
                position=candidate.0;let preserve=srl_xml_extension_admit(state,model,&mut counts,candidate.1,options)?;
                let validate=model.contract.is_some_and(|contract|contract.slots[candidate.1].process_contents!=0);
                if preserve||validate {
                    let owner_index=if preserve{Some(srl_xml_extension_owner(state,model,path,input,binding_end,options)?)}else{None};
                    let uri=if preserve{srl_xml_extension_owned(&name.uri,operation.as_ref())?}else{String::new()};
                    if preserve{srl_xml_extension_charge(state,options.limits,24+uri.len(),operation.as_ref())?;}
                    let capture_maximum=if preserve{maximum.min(options.limits.max_preserved_bytes.saturating_sub(state.bytes))}else{maximum};
                    let mut xml=String::new();srl_xml_extension_foreign(input,Some(&mut xml),capture_maximum,true)?;
                    srl_xml_extension_validate(model,candidate.1,"","",None,&[],&xml,input.limits,operation.as_ref().map(@xml_operation@::State::secondary))?;
                    if let Some(index)=owner_index{srl_xml_extension_charge(state,options.limits,xml.len(),operation.as_ref())?;srl_xml_extension_reserve(&mut state.owners[index].children,1,operation.as_ref())?;state.owners[index].children.push(SrlXmlExtensionChild{position,item:items.get(position).copied().unwrap_or(0),uri,xml});}
                }else{srl_xml_extension_foreign(input,None,maximum,false)?;}
            }
        },
        SrlXmlToken::Text(value)=>{if closed_children&&model.mixed_text&&!value.is_empty(){return Err(Error("Mixed XML text follows an end extension slot"));}if model.namespace_text!=0 {srl_xml_extension_put(&mut namespace_value,value,input.limits.max_text_bytes,operation.as_ref())?;}srl_xml_extension_escape(output,value,false,maximum,operation.as_ref())?;input.advance()?;},
        SrlXmlToken::End(_)=>{if !map_key_ready{return Err(Error("XML map key missing"));}if namespace_kind!=0&&namespace_value.is_empty()&&model.scalar_rule.is_some_and(|rule|rule.schema_default.is_some()){srl_xml_extension_default_add(state,model,path,input,binding_end,options,usize::MAX)?;}else{srl_xml_namespace_value(&namespace_value,namespace_kind,&namespace_scope,input.limits.max_nodes,model.namespace_enumeration,model.scalar_rule,operation.as_ref())?;}if model.map_key_kind==0&&state.owners.len()>original_owners {srl_xml_extension_owner(state,model,path,input,binding_end,options)?;}if let Some(contract)=model.contract {for (slot,count) in contract.slots.iter().zip(counts) {if count<slot.minimum {return Err(Error("Required XML extension occurrence missing"));}}}srl_xml_extension_end(output,&lexical,maximum,operation.as_ref())?;input.advance()?;return Ok(());},
        SrlXmlToken::Eof=>return Err(Error("Unterminated typed XML owner")),
    }}
}
)rustext"
