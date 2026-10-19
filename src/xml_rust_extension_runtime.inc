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
pub struct XmlExtensionState {owners:Vec<SrlXmlExtensionOwner>,bytes:usize,items:usize}
impl XmlExtensionState {
    /// Report charged logical preservation storage for exact application admission policies.
    pub fn preserved_bytes(&self)->usize {self.bytes}
}
/// Static field edges describe ordinary native ownership without reading or copying native values.
struct SrlXmlExtensionField {uri:&'static str,local:&'static str,attribute:bool,repeated:bool,child:Option<fn()->&'static SrlXmlExtensionModel>}
struct SrlXmlExtensionSlot {attribute:bool,position:usize,uris:&'static[&'static str],policy:u8,minimum:usize,maximum:Option<usize>}
struct SrlXmlExtensionContract {core:&'static str,required:&'static[&'static str],slots:&'static[SrlXmlExtensionSlot]}
struct SrlXmlExtensionModel {identity:&'static str,fields:&'static[SrlXmlExtensionField],contract:Option<&'static SrlXmlExtensionContract>}
/// A graph path uses static field/repeated-item positions, never unstable native object addresses.
struct SrlXmlExtensionOwner {path:Vec<(usize,usize)>,identity:&'static str,scope:Vec<(String,String)>,attributes:Vec<(String,String,String)>,children:Vec<SrlXmlExtensionChild>}
struct SrlXmlExtensionChild {position:usize,item:usize,uri:String,xml:String}
/// Charge operation-wide preservation before making any corresponding owning allocation.
fn srl_xml_extension_charge(state:&mut XmlExtensionState,limits:XmlExtensionLimits,count:usize)->Result<(),Error> {
    state.bytes=state.bytes.checked_add(count).filter(|value|*value<=limits.max_preserved_bytes).ok_or(Error("XML extension preserved-byte limit"))?;Ok(())
}
/// Append bounded canonical transport bytes; this buffer contains no retained known XML tree.
fn srl_xml_extension_put(output:&mut String,value:&str,maximum:usize)->Result<(),Error> {
    if value.len()>maximum.saturating_sub(output.len()) {return Err(Error("XML extension transport byte limit"));}
    output.try_reserve(value.len()).map_err(|_|Error("XML extension transport allocation"))?;output.push_str(value);Ok(())
}
/// Escape semantic XML values, preserving referenced whitespace and carriage returns.
fn srl_xml_extension_escape(output:&mut String,value:&str,attribute:bool,maximum:usize)->Result<(),Error> {
    for value in value.chars() {let mut bytes=[0;4];let text=match value {'&'=>"&amp;",'<'=>"&lt;",'>'=>"&gt;",'"' if attribute=>"&quot;",'\r'=>"&#13;",'\n' if attribute=>"&#10;",'\t' if attribute=>"&#9;",_=>value.encode_utf8(&mut bytes)};srl_xml_extension_put(output,text,maximum)?;}Ok(())
}
/// Flatten only an admitted original scope, preserving unused aliases and nearest shadowing.
fn srl_xml_extension_scope(input:&SrlXmlInput<'_>)->Vec<(String,String)> {
    let mut result=std::collections::BTreeMap::new();result.insert("xml".to_owned(),SRL_XML_NAMESPACE.to_owned());result.insert(String::new(),String::new());
    for (prefix,uri) in &input.bindings {result.insert((*prefix).to_owned(),uri.clone());}result.into_iter().collect()
}
/// Check semantic capabilities against original namespace bindings before any typed model is published.
fn srl_xml_extension_capabilities(contract:&SrlXmlExtensionContract,scope:&[(String,String)],options:&XmlExtensionOptions)->Result<(),Error> {
    if contract.slots.len()>options.limits.max_slots || options.understood_namespaces.len()>options.limits.max_items {return Err(Error("XML extension slot/capability limit"));}
    let understood=|uri:&str|uri==contract.core || options.understood_namespaces.iter().any(|value|value==uri);
    for uri in contract.required {if !understood(uri) {return Err(Error("Required XML capability is not understood"));}}
    let mut count=0;for prefix in options.required_prefixes.split([' ','\t','\r','\n']).filter(|value|!value.is_empty()) {
        count+=1;if count>options.limits.max_items || prefix=="xmlns" || !srl_xml_extension_prefix(prefix) {return Err(Error("Invalid required XML capability prefix"));}
        let uri=scope.iter().rev().find(|(name,_)|name==prefix).map(|(_,uri)|uri.as_str()).filter(|uri|!uri.is_empty()).ok_or(Error("Unbound required XML capability prefix"))?;
        if !understood(uri) {return Err(Error("Required XML capability is not understood"));}
    }Ok(())
}
/// Prefix inventories accept the same Unicode NCName ranges as the generated XML tokenizer.
fn srl_xml_extension_prefix(value:&str)->bool {
    let mut chars=value.chars();let start=|c:char|c=='_' || c.is_alphabetic() || matches!(c as u32,0xc0..=0xd6|0xd8..=0xf6|0xf8..=0x2ff|0x370..=0x37d|0x37f..=0x1fff|0x200c..=0x200d|0x2070..=0x218f|0x2c00..=0x2fef|0x3001..=0xd7ff|0xf900..=0xfdcf|0xfdf0..=0xfffd|0x10000..=0xeffff);
    chars.next().is_some_and(start) && chars.all(|c|start(c)||c.is_ascii_digit()||matches!(c,'-'|'.'|'\u{b7}'|'\u{300}'..='\u{36f}'|'\u{203f}'..='\u{2040}'))
}
/// Select an exact foreign URI at a reachable field boundary, with closed core namespaces.
fn srl_xml_extension_slot(model:&SrlXmlExtensionModel,uri:&str,local:&str,attribute:bool,position:usize)->Option<usize> {
    let contract=model.contract?;
    if uri.is_empty() || uri==contract.core || uri=="urn:serializer:xml:model" || model.fields.iter().any(|field|field.uri==uri && field.local==local) {return None;}
    contract.slots.iter().position(|slot|slot.attribute==attribute && (attribute||slot.position==position) && slot.uris.contains(&uri))
}
/// Admit every preserve/skip root cumulatively, including skip payloads that retain no opaque state.
fn srl_xml_extension_admit(state:&mut XmlExtensionState,model:&SrlXmlExtensionModel,counts:&mut[usize],slot:usize,options:&XmlExtensionOptions)->Result<bool,Error> {
    let selected=&model.contract.ok_or(Error("XML extension has no permitted slot"))?.slots[slot];
    if selected.policy==0 {return Err(Error("XML extension has a reject slot"));}
    state.items=state.items.checked_add(1).filter(|value|*value<=options.limits.max_items).ok_or(Error("XML extension item limit"))?;
    counts[slot]=counts[slot].checked_add(1).filter(|value|selected.maximum.is_none_or(|maximum|*value<=maximum)).ok_or(Error("XML extension occurrence limit"))?;Ok(selected.policy==2)
}
)rustext"
R"rustext(
/// Merge nearest lexical bindings without changing unknown values that merely resemble QNames.
fn srl_xml_extension_merge(scope:&mut Vec<(String,String)>,values:&[(String,String)]) {
    for (prefix,uri) in values {if let Some((_,value))=scope.iter_mut().find(|(name,_)|name==prefix) {*value=uri.clone();}else{scope.push((prefix.clone(),uri.clone()));}}
}
/// Revalidate a retained item and place it at its exact static-field/repeated-item boundary.
fn srl_xml_extension_replay_children(output:&mut String,owner:Option<&SrlXmlExtensionOwner>,cursor:&mut usize,index:usize,items:&[usize],model:&SrlXmlExtensionModel,counts:&mut[usize],admission:&mut XmlExtensionState,options:&XmlExtensionOptions,maximum:usize)->Result<(),Error> {
    let Some(owner)=owner else{return Ok(())};
    while let Some(child)=owner.children.get(*cursor) {
        if child.position>index {break;}
        let count=items.get(child.position).copied().unwrap_or(0);
        if child.position==index && child.item>count {break;}
        if child.item!=count {return Err(Error("Preserved XML extension has an edited field position"));}
        let slot=srl_xml_extension_slot(model,&child.uri,"",false,child.position).ok_or(Error("Preserved XML extension has no permitted slot"))?;
        if !srl_xml_extension_admit(admission,model,counts,slot,options)? {return Err(Error("Preserved XML extension no longer has a preserve slot"));}
        srl_xml_extension_put(output,&child.xml,maximum)?;*cursor+=1;
    }Ok(())
}
/// Replay borrowed sidecar items while traversing only the generated codec's known transport tokens.
fn srl_xml_extension_replay_owner(input:&mut SrlXmlInput<'_>,output:&mut String,model:&'static SrlXmlExtensionModel,path:&mut Vec<(usize,usize)>,state:&XmlExtensionState,used:&mut[bool],admission:&mut XmlExtensionState,options:&XmlExtensionOptions,inherited:&[(String,String)],maximum:usize)->Result<(),Error> {
    let matched=state.owners.iter().enumerate().find(|(_,owner)|owner.path==*path);
    let owner=matched.map(|(index,owner)|{used[index]=true;owner});
    if owner.is_some_and(|owner|owner.identity!=model.identity) {return Err(Error("Preserved XML extension owner type changed"));}
    // Retained contexts are complete original scopes; canonical writer aliases must not accumulate on replay.
    let mut scope=owner.map_or_else(||inherited.to_vec(),|owner|owner.scope.clone());if owner.is_none() {srl_xml_extension_merge(&mut scope,&srl_xml_extension_scope(input));}
    if let Some(contract)=model.contract {srl_xml_extension_capabilities(contract,&scope,options)?;}
    let mut counts=vec![0;model.contract.map_or(0,|contract|contract.slots.len())];
    let mut attributes=match std::mem::replace(&mut input.current,SrlXmlToken::Eof) {SrlXmlToken::Start(name,attributes)=>{input.current=SrlXmlToken::Start(name,Vec::new());attributes},_=>return Err(Error("Expected typed XML start"))};
    if let Some(owner)=owner {for (uri,local,value) in &owner.attributes {
        let slot=srl_xml_extension_slot(model,uri,local,true,0).ok_or(Error("Preserved XML attribute has no permitted slot"))?;
        if !srl_xml_extension_admit(admission,model,&mut counts,slot,options)? {return Err(Error("Preserved XML attribute no longer has a preserve slot"));}
        attributes.push((SrlXmlTokenName {uri:uri.clone(),local},value.clone()));
    }}
    let lexical=srl_xml_extension_start(output,input,&attributes,Some(&scope),maximum)?;input.advance()?;
    let mut items=vec![0usize;model.fields.len()];let mut cursor=0;
    loop {match &input.current {
        SrlXmlToken::Start(name,_)=>{
            let index=model.fields.iter().position(|field|!field.attribute && field.uri==name.uri && field.local==name.local).ok_or(Error("Unexpected known XML transport child"))?;
            srl_xml_extension_replay_children(output,owner,&mut cursor,index,&items,model,&mut counts,admission,options,maximum)?;
            let item=items[index];items[index]+=1;
            if let Some(child)=model.fields[index].child {path.push((index,item));srl_xml_extension_replay_owner(input,output,child(),path,state,used,admission,options,&scope,maximum)?;path.pop();}
            else {srl_xml_extension_foreign(input,Some(output),maximum,false)?;}
        },
        SrlXmlToken::Text(value)=>{srl_xml_extension_escape(output,value,false,maximum)?;input.advance()?;},
        SrlXmlToken::End(_)=>{
            srl_xml_extension_replay_children(output,owner,&mut cursor,model.fields.len(),&items,model,&mut counts,admission,options,maximum)?;
            if owner.is_some_and(|owner|cursor!=owner.children.len()) {return Err(Error("Preserved XML extension has an edited field position"));}
            if let Some(contract)=model.contract {for (slot,count) in contract.slots.iter().zip(counts) {if count<slot.minimum {return Err(Error("Required XML extension occurrence missing"));}}}
            srl_xml_extension_end(output,&lexical,maximum)?;input.advance()?;return Ok(());
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
fn srl_xml_extension_filter(text:&str,limits:XmlLimits,options:&XmlExtensionOptions,model:&'static SrlXmlExtensionModel)->Result<(String,XmlExtensionState),Error> {
    options.validate()?;
    // Canonical names can expand framing. This explicit buffer bound is additional to, and never replaces, original XML admission.
    let maximum=srl_xml_extension_output_limits(limits)?.max_bytes;
    let mut input=SrlXmlInput::new(text,limits)?;input.skip_space()?;let mut output=String::new();let mut state=XmlExtensionState::default();
    srl_xml_extension_filter_owner(&mut input,&mut output,model,&mut Vec::new(),&mut state,options,maximum)?;input.finish_document()?;Ok((output,state))
}
/// Replay an immutable borrowed sidecar and re-admit the complete final document before returning bytes.
fn srl_xml_extension_replay(text:&str,limits:XmlLimits,options:&XmlExtensionOptions,model:&'static SrlXmlExtensionModel,state:&XmlExtensionState)->Result<String,Error> {
    options.validate()?;
    if state.bytes>options.limits.max_preserved_bytes {return Err(Error("XML extension preserved-byte limit"));}
    let framing=srl_xml_extension_output_limits(limits)?;if text.len()>framing.max_bytes {return Err(Error("XML extension transport byte limit"));}
    let mut input=SrlXmlInput::new(text,srl_xml_extension_transport_limits(text,limits)?)?;input.skip_space()?;let mut output=String::new();let mut used=vec![false;state.owners.len()];let mut admission=XmlExtensionState::default();
    srl_xml_extension_replay_owner(&mut input,&mut output,model,&mut Vec::new(),state,&mut used,&mut admission,options,&[],limits.max_bytes)?;input.finish_document()?;
    if used.iter().any(|value|!*value) {return Err(Error("Preserved XML extension owner has an edited field position"));}
    let mut checked=SrlXmlInput::new(&output,limits)?;while !matches!(checked.current,SrlXmlToken::Eof) {checked.advance()?;}checked.finish_document()?;Ok(output)
}
)rustext"
R"rustext(
/// Reuse a compatible original binding; allocate a fresh prefix only for a genuine framing collision.
fn srl_xml_extension_select_prefix(declarations:&mut std::collections::BTreeMap<String,String>,input:&SrlXmlInput<'_>,uri:&str,preferred:&str,attribute:bool)->String {
    if uri.is_empty() {if !attribute {declarations.insert(String::new(),String::new());}return String::new();}
    if uri==SRL_XML_NAMESPACE {return "xml".to_owned();}
    let effective=|prefix:&str|declarations.get(prefix).map(String::as_str).or_else(||input.bindings.iter().rev().find(|(value,_)|*value==prefix).map(|(_,value)|value.as_str()));
    if (!attribute||!preferred.is_empty()) && effective(preferred)==Some(uri) {let result=preferred.to_owned();declarations.insert(result.clone(),uri.to_owned());return result;}
    if let Some((prefix,_))=declarations.iter().find(|(prefix,value)|(!attribute||!prefix.is_empty()) && value.as_str()==uri) {return prefix.clone();}
    if let Some((prefix,_))=input.bindings.iter().rev().find(|(prefix,value)|(!attribute||!prefix.is_empty()) && value.as_str()==uri && effective(prefix)==Some(uri)) {let result=(*prefix).to_owned();declarations.insert(result.clone(),uri.to_owned());return result;}
    let mut serial=0usize;loop {let prefix=format!("srlopaque{serial}");serial+=1;if !declarations.contains_key(&prefix) && !input.bindings.iter().any(|(value,_)|*value==prefix) {declarations.insert(prefix.clone(),uri.to_owned());return prefix;}}
}
/// Serialize expanded names while leaving every opaque lexical binding intact and replay stable.
fn srl_xml_extension_start(output:&mut String,input:&SrlXmlInput<'_>,attributes:&[(SrlXmlTokenName<'_>,String)],scope:Option<&[(String,String)]>,maximum:usize)->Result<String,Error> {
    let (name,_) = match &input.current {SrlXmlToken::Start(name,values)=>(name,values),_=>return Err(Error("Expected XML extension start"))};
    let mut declarations=std::collections::BTreeMap::<String,String>::new();
    if let Some(scope)=scope {for (prefix,uri) in scope {declarations.insert(prefix.clone(),uri.clone());}}
    else {let rollback=input.ancestors.last().ok_or(Error("XML extension scope"))?.bindings;for (prefix,uri) in &input.bindings[rollback..] {declarations.insert((*prefix).to_owned(),uri.clone());}}
    let preferred=input.ancestors.last().ok_or(Error("XML extension scope"))?.prefix;
    let prefix=srl_xml_extension_select_prefix(&mut declarations,input,&name.uri,preferred,false);
    let lexical=if prefix.is_empty() {name.local.to_owned()} else {format!("{prefix}:{}",name.local)};
    let mut values=Vec::new();for (name,value) in attributes {let prefix=srl_xml_extension_select_prefix(&mut declarations,input,&name.uri,"",true);let lexical=if prefix.is_empty() {name.local.to_owned()} else {format!("{prefix}:{}",name.local)};values.push((lexical,value));}
    srl_xml_extension_put(output,"<",maximum)?;srl_xml_extension_put(output,&lexical,maximum)?;
    for (prefix,uri) in declarations {if prefix=="xml" {continue;}srl_xml_extension_put(output," xmlns",maximum)?;if !prefix.is_empty() {srl_xml_extension_put(output,":",maximum)?;srl_xml_extension_put(output,&prefix,maximum)?;}srl_xml_extension_put(output,"=\"",maximum)?;srl_xml_extension_escape(output,&uri,true,maximum)?;srl_xml_extension_put(output,"\"",maximum)?;}
    for (name,value) in values {srl_xml_extension_put(output," ",maximum)?;srl_xml_extension_put(output,&name,maximum)?;srl_xml_extension_put(output,"=\"",maximum)?;srl_xml_extension_escape(output,value,true,maximum)?;srl_xml_extension_put(output,"\"",maximum)?;}
    srl_xml_extension_put(output,">",maximum)?;Ok(lexical)
}
/// End one independently prefixed transport element.
fn srl_xml_extension_end(output:&mut String,name:&str,maximum:usize)->Result<(),Error> {srl_xml_extension_put(output,"</",maximum)?;srl_xml_extension_put(output,name,maximum)?;srl_xml_extension_put(output,">",maximum)}
/// Traverse an opaque foreign subtree directly; skip retains no completed token or payload.
fn srl_xml_extension_foreign(input:&mut SrlXmlInput<'_>,mut output:Option<&mut String>,maximum:usize,full_scope:bool)->Result<(),Error> {
    let attributes=match &input.current {SrlXmlToken::Start(_,attributes)=>attributes,_=>return Err(Error("Expected foreign XML start"))};
    let name=if let Some(target)=output.as_deref_mut() {let scope=full_scope.then(||srl_xml_extension_scope(input));srl_xml_extension_start(target,input,attributes,scope.as_deref(),maximum)?}else{String::new()};
    input.advance()?;
    loop {match &input.current {
        SrlXmlToken::Start(..)=>srl_xml_extension_foreign(input,output.as_deref_mut(),maximum,false)?,
        SrlXmlToken::Text(value)=>{if let Some(target)=output.as_deref_mut() {srl_xml_extension_escape(target,value,false,maximum)?;}input.advance()?;},
        SrlXmlToken::End(_)=>{if let Some(target)=output {srl_xml_extension_end(target,&name,maximum)?;}input.advance()?;return Ok(());},
        SrlXmlToken::Eof=>return Err(Error("Unterminated foreign XML")),
    }}
}
/// Retain an original owner scope lazily, after its complete logical size has been admitted.
fn srl_xml_extension_owner(state:&mut XmlExtensionState,model:&SrlXmlExtensionModel,path:&[(usize,usize)],input:&SrlXmlInput<'_>,binding_end:usize,options:&XmlExtensionOptions)->Result<usize,Error> {
    if let Some(index)=state.owners.iter().position(|owner|owner.path==path) {return Ok(index);}
    let mut size=64usize.checked_add(path.len().checked_mul(2*std::mem::size_of::<usize>()).ok_or(Error("XML extension path limit"))?).ok_or(Error("XML extension path limit"))?;
    // Charge all active declarations conservatively before flattening; shadowed bindings may cost more, never less.
    let mut prefixes=std::collections::BTreeSet::new();for (prefix,uri) in input.bindings[..binding_end].iter().rev() {if prefixes.insert(*prefix) {size=size.checked_add(64+prefix.len()+uri.len()).ok_or(Error("XML extension scope limit"))?;}}
    if !prefixes.contains("") {size=size.checked_add(64).ok_or(Error("XML extension scope limit"))?;}
    srl_xml_extension_charge(state,options.limits,size)?;let mut scope=std::collections::BTreeMap::new();scope.insert("xml".to_owned(),SRL_XML_NAMESPACE.to_owned());scope.insert(String::new(),String::new());for (prefix,uri) in &input.bindings[..binding_end] {scope.insert((*prefix).to_owned(),uri.clone());}
    let index=state.owners.len();state.owners.push(SrlXmlExtensionOwner {path:path.to_vec(),identity:model.identity,scope:scope.into_iter().collect(),attributes:Vec::new(),children:Vec::new()});Ok(index)
}
/// Filter one typed ordinary owner, preserving only explicitly admitted foreign roots and attributes.
fn srl_xml_extension_filter_owner(input:&mut SrlXmlInput<'_>,output:&mut String,model:&'static SrlXmlExtensionModel,path:&mut Vec<(usize,usize)>,state:&mut XmlExtensionState,options:&XmlExtensionOptions,maximum:usize)->Result<(),Error> {
    let original_owners=state.owners.len();let binding_end=input.bindings.len();if let Some(contract)=model.contract {srl_xml_extension_capabilities(contract,&srl_xml_extension_scope(input),options)?;}
    let mut counts=vec![0;model.contract.map_or(0,|contract|contract.slots.len())];let mut known=Vec::new();
    let attributes=match std::mem::replace(&mut input.current,SrlXmlToken::Eof) {SrlXmlToken::Start(name,values)=>{input.current=SrlXmlToken::Start(name,Vec::new());values},_=>return Err(Error("Expected typed XML start"))};
    for (name,value) in attributes {
        if model.fields.iter().any(|field|field.attribute && field.uri==name.uri && field.local==name.local) || name.uri=="urn:serializer:xml:model" {known.push((name,value));continue;}
        let slot=srl_xml_extension_slot(model,&name.uri,name.local,true,0).ok_or(Error("XML extension attribute has no permitted slot"))?;
        if srl_xml_extension_admit(state,model,&mut counts,slot,options)? {let index=srl_xml_extension_owner(state,model,path,input,binding_end,options)?;srl_xml_extension_charge(state,options.limits,48+name.uri.len()+name.local.len()+value.len())?;state.owners[index].attributes.push((name.uri,name.local.to_owned(),value));}
    }
    let lexical=srl_xml_extension_start(output,input,&known,None,maximum)?;input.advance()?;
    let mut position=0usize;let mut items=vec![0usize;model.fields.len()];
    loop {match &input.current {
        SrlXmlToken::Start(name,_)=>{
            if let Some(index)=model.fields.iter().enumerate().find(|(index,field)|*index>=position && !field.attribute && field.uri==name.uri && field.local==name.local).map(|(index,_)|index) {
                let field=&model.fields[index];let item=items[index];items[index]+=1;position=if field.repeated {index}else{index+1};
                if let Some(child)=field.child {path.push((index,item));srl_xml_extension_filter_owner(input,output,child(),path,state,options,maximum)?;path.pop();}
                else {srl_xml_extension_foreign(input,Some(output),maximum,false)?;}
            }else{
                let candidate=(position..=model.fields.len()).filter(|index|*index==model.fields.len() || !model.fields[*index].attribute).find_map(|index|srl_xml_extension_slot(model,&name.uri,name.local,false,index).map(|slot|(index,slot))).ok_or(Error("XML extension element has no permitted slot"))?;
                position=candidate.0;let preserve=srl_xml_extension_admit(state,model,&mut counts,candidate.1,options)?;
                if preserve {let index=srl_xml_extension_owner(state,model,path,input,binding_end,options)?;let uri=name.uri.clone();srl_xml_extension_charge(state,options.limits,24+uri.len())?;let mut xml=String::new();let remaining=options.limits.max_preserved_bytes.saturating_sub(state.bytes);srl_xml_extension_foreign(input,Some(&mut xml),remaining,true)?;srl_xml_extension_charge(state,options.limits,xml.len())?;state.owners[index].children.push(SrlXmlExtensionChild {position,item:items.get(position).copied().unwrap_or(0),uri,xml});}
                else {srl_xml_extension_foreign(input,None,maximum,false)?;}
            }
        },
        SrlXmlToken::Text(value)=>{srl_xml_extension_escape(output,value,false,maximum)?;input.advance()?;},
        SrlXmlToken::End(_)=>{if state.owners.len()>original_owners {srl_xml_extension_owner(state,model,path,input,binding_end,options)?;}if let Some(contract)=model.contract {for (slot,count) in contract.slots.iter().zip(counts) {if count<slot.minimum {return Err(Error("Required XML extension occurrence missing"));}}}srl_xml_extension_end(output,&lexical,maximum)?;input.advance()?;return Ok(());},
        SrlXmlToken::Eof=>return Err(Error("Unterminated typed XML owner")),
    }}
}
)rustext"
