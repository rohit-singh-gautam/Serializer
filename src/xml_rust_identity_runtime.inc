// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustidentity(
/// Operation-local indexes are bounded independently of complete XML/native message policies.
#[derive(Clone,Copy,Debug)]
pub struct XmlIdentityLimits {pub max_constraints:usize,pub max_fields:usize,pub max_depth:usize,
 pub max_selected_nodes:usize,pub max_tuples:usize,pub max_tuple_bytes:usize,pub max_work_units:usize}
impl Default for XmlIdentityLimits {
 /// Apply compiler-derived shared identity defaults without sharing mutable operation state.
 fn default()->Self {Self{max_constraints:SRL_XML_IDENTITY_CONSTRAINTS,max_fields:SRL_XML_IDENTITY_FIELDS,
  max_depth:SRL_XML_IDENTITY_DEPTH,max_selected_nodes:SRL_XML_IDENTITY_SELECTED,max_tuples:SRL_XML_IDENTITY_TUPLES,
  max_tuple_bytes:SRL_XML_IDENTITY_BYTES,max_work_units:SRL_XML_IDENTITY_WORK}}
}
/// Expanded compiler-owned identities and paths do not interpret XPath or target regex text at runtime.
struct SrlXmlIdentityName {uri:&'static str,local:&'static str}
struct SrlXmlIdentityPath {steps:&'static [SrlXmlIdentityName],descendants:bool,attribute:Option<SrlXmlIdentityName>}
struct SrlXmlIdentityField {path:SrlXmlIdentityPath,kind:&'static str,nillable:bool}
struct SrlXmlIdentityConstraint {kind:u8,selector:SrlXmlIdentityPath,fields:&'static [SrlXmlIdentityField],refer:usize}
struct SrlXmlIdentityScope {constraints:&'static [SrlXmlIdentityConstraint]}
/// Typed graph transitions distinguish owner scopes without retaining completed siblings or document nodes.
struct SrlXmlIdentityEdge {name:SrlXmlIdentityName,type_name:Option<SrlXmlIdentityName>,target:usize}
struct SrlXmlIdentityNode {scope:Option<&'static SrlXmlIdentityScope>,edges:&'static [SrlXmlIdentityEdge]}
struct SrlXmlIdentityCapture {depth:usize,nil:bool,text:String}
struct SrlXmlIdentityCandidate {constraint:usize,depth:usize,values:Vec<Option<String>>,seen:Vec<bool>,captures:Vec<Option<SrlXmlIdentityCapture>>,bytes:usize}
struct SrlXmlIdentity<'a> {
 operation:Option<@xml_operation@::State<'a>>,diagnostics:&'static [@xml_operation@::SrlXmlIdentityDiagnosticConstraint],
 scope:&'static SrlXmlIdentityScope,limits:XmlIdentityLimits,path:Vec<(String,String)>,active:Vec<SrlXmlIdentityCandidate>,
 tables:Vec<std::collections::BTreeSet<String>>,selected:usize,tuples:usize,bytes:usize,work:usize,complete:bool,
}
impl<'a> SrlXmlIdentity<'a> {
 /// Validate static indexes and allocate only the bounded operation-local table inventory.
 fn new(scope:&'static SrlXmlIdentityScope,limits:XmlIdentityLimits)->Result<Self,Error>{Self::new_with_operation(scope,limits,None,&[])}
 /// Keep caller admission and diagnostic projections alongside each owner-local identity observer.
 fn new_with_operation(scope:&'static SrlXmlIdentityScope,limits:XmlIdentityLimits,operation:Option<@xml_operation@::State<'a>>,diagnostics:&'static [@xml_operation@::SrlXmlIdentityDiagnosticConstraint])->Result<Self,Error>{
  if scope.constraints.len()>limits.max_constraints{return Err(@xml_operation@::quota(operation.as_ref(),"XML identity constraint limit"));}
  @xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,scope.constraints.len().saturating_mul(std::mem::size_of::<std::collections::BTreeSet<String>>()))?;
  let mut tables=Vec::new();tables.try_reserve_exact(scope.constraints.len()).map_err(|_|@xml_operation@::quota(operation.as_ref(),"XML identity allocation"))?;
  for constraint in scope.constraints{if constraint.fields.is_empty()||constraint.fields.len()>limits.max_fields{return Err(@xml_operation@::quota(operation.as_ref(),"XML identity field limit"));}
   if constraint.kind==2&&(constraint.refer>=scope.constraints.len()||scope.constraints[constraint.refer].kind==2){return Err(Error("XML identity keyref scope"));}tables.push(std::collections::BTreeSet::new());}
  Ok(Self{operation,diagnostics,scope,limits,path:Vec::new(),active:Vec::new(),tables,selected:0,tuples:0,bytes:0,work:0,complete:false})
 }
 /// Charge before comparisons, scans and retained tuple work, using subtraction to preserve exact zero limits.
 fn spend(&mut self,count:usize)->Result<(),Error>{if count>self.limits.max_work_units.saturating_sub(self.work){return Err(@xml_operation@::quota(self.operation.as_ref(),"XML identity work limit"));}@xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::WorkUnits,count)?;self.work+=count;Ok(())}
 /// Compare the prepared suffix directly against an active path without allocating temporary slices.
 fn matches(path:&[(String,String)],first:usize,expected:&SrlXmlIdentityPath)->(bool,usize){
  let count=path.len().saturating_sub(first);let mut work=count.saturating_add(expected.steps.len()).saturating_add(1);
  if if expected.descendants{count<expected.steps.len()}else{count!=expected.steps.len()}{return (false,work);}
  let offset=if expected.descendants{path.len()-expected.steps.len()}else{first};
  for (actual,wanted) in path[offset..].iter().zip(expected.steps){work=work.saturating_add((actual.0.len()+actual.1.len()+2).min(wanted.uri.len()+wanted.local.len()+2)+1);
   if actual.0!=wanted.uri||actual.1!=wanted.local{return(false,work);}}(true,work)
 }
 /// Canonicalize exact builtin values independently of application widths and namespace prefix spellings.
 fn key(raw:&str,field:&SrlXmlIdentityField,maximum:usize,resolve:&impl Fn(&str)->Result<String,Error>,operation:Option<&@xml_operation@::State<'_>>)->Result<String,Error>{
  @xml_operation@::consume(operation,@xml_operation@::Resource::DecoderAllocationBytes,raw.len().saturating_add(2))?;
  if raw.len()>maximum{return Err(@xml_operation@::quota(operation,"XML identity field-byte limit"));}
  let mut rule=SrlXmlRule::item(field.kind);rule.whitespace=match field.kind{"string"=>"preserve","normalizedString"=>"replace",_=>"collapse"};
  let value=rule.check_with_operation(raw,maximum,operation.cloned())?;let mut key=String::new();
  if field.kind=="QName"{let parts=value.split(':').collect::<Vec<_>>();if parts.is_empty()||parts.len()>2{return Err(Error("Invalid XML identity QName"));}
   for part in &parts{SrlXmlRule::item("NCName").check_with_operation(part,maximum,operation.cloned())?;}
   let namespace=resolve(if parts.len()==2{parts[0]}else{""})?;key.try_reserve(namespace.len()+value.len()+2).map_err(|_|@xml_operation@::quota(operation,"XML identity allocation"))?;
   key.push('q');key.push_str(&namespace);key.push('\0');key.push_str(parts[parts.len()-1]);
  }else if field.kind=="boolean"{key.push_str(if srl_xml_bool(&value)?{"btrue"}else{"bfalse"});}
  else if rule.integer_kind()||field.kind=="decimal"{
   let negative=value.starts_with('-');let digits=value.strip_prefix(['+','-']).unwrap_or(&value);let (whole,fraction)=digits.split_once('.').unwrap_or((digits,""));
   let whole=whole.trim_start_matches('0');let fraction=fraction.trim_end_matches('0');key.try_reserve(value.len()+2).map_err(|_|@xml_operation@::quota(operation,"XML identity allocation"))?;key.push('d');
   if negative&&(!whole.is_empty()||!fraction.is_empty()){key.push('-');}key.push_str(if whole.is_empty(){"0"}else{whole});if !fraction.is_empty(){key.push('.');key.push_str(fraction);}
  }else{if field.kind=="float"||field.kind=="double"{return Err(Error("Floating XML identity fields unsupported"));}
   key.try_reserve(value.len()+1).map_err(|_|@xml_operation@::quota(operation,"XML identity allocation"))?;key.push(if field.kind=="anyURI"{'u'}else{'s'});key.push_str(&value);}
  Ok(key)
 }
 /// Admit canonical value bytes before retaining a field inside its incomplete candidate tuple.
 fn store(candidate:&mut SrlXmlIdentityCandidate,index:usize,value:String,maximum:usize,operation:Option<&@xml_operation@::State<'_>>)->Result<(),Error>{
  let bytes=value.len()-if value.starts_with('q'){2}else{1};if bytes>maximum.saturating_sub(candidate.bytes){return Err(@xml_operation@::quota(operation,"XML identity candidate-byte limit"));}
  candidate.bytes+=bytes;candidate.values[index]=Some(value);Ok(())
 }
 /// Observe full original attributes before generated nil/type or ordinary field callbacks consume them.
 fn start(&mut self,name:&SrlXmlTokenName<'_>,attributes:&[(SrlXmlTokenName<'_>,String)],resolve:&impl Fn(&str)->Result<String,Error>)->Result<(),Error>{
  if self.complete{return Err(Error("XML identity scope complete"));}self.spend(name.uri.len()+name.local.len()+3)?;
  if self.path.len()>=self.limits.max_depth{return Err(@xml_operation@::quota(self.operation.as_ref(),"XML identity depth limit"));}
  if self.active.iter().any(|candidate|candidate.captures.iter().any(Option::is_some)){return Err(Error("XML identity scalar has complex child"));}
  @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,std::mem::size_of::<(String,String)>().saturating_add(name.uri.len()).saturating_add(name.local.len()))?;self.path.try_reserve(1).map_err(|_|@xml_operation@::quota(self.operation.as_ref(),"XML identity allocation"))?;self.path.push((name.uri.clone(),name.local.to_owned()));
  for (index,constraint) in self.scope.constraints.iter().enumerate(){let (matched,work)=Self::matches(&self.path,1,&constraint.selector);self.spend(work)?;
   if matched{if self.selected>=self.limits.max_selected_nodes{return Err(@xml_operation@::quota(self.operation.as_ref(),"XML identity selected-node limit"));}self.selected+=1;
    let count=constraint.fields.len();@xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,count.saturating_mul(std::mem::size_of::<Option<String>>()+std::mem::size_of::<bool>()+std::mem::size_of::<Option<SrlXmlIdentityCapture>>()).saturating_add(std::mem::size_of::<SrlXmlIdentityCandidate>()))?;let mut values=Vec::new();let mut seen=Vec::new();let mut captures=Vec::new();
    values.try_reserve_exact(count).map_err(|_|@xml_operation@::quota(self.operation.as_ref(),"XML identity allocation"))?;seen.try_reserve_exact(count).map_err(|_|@xml_operation@::quota(self.operation.as_ref(),"XML identity allocation"))?;captures.try_reserve_exact(count).map_err(|_|@xml_operation@::quota(self.operation.as_ref(),"XML identity allocation"))?;
    values.resize_with(count,||None);seen.resize(count,false);captures.resize_with(count,||None);self.active.try_reserve(1).map_err(|_|@xml_operation@::quota(self.operation.as_ref(),"XML identity allocation"))?;
    self.active.push(SrlXmlIdentityCandidate{constraint:index,depth:self.path.len(),values,seen,captures,bytes:0});}}
  for candidate_index in 0..self.active.len(){let constraint=&self.scope.constraints[self.active[candidate_index].constraint];
   for (index,field) in constraint.fields.iter().enumerate(){let(matched,work)=Self::matches(&self.path,self.active[candidate_index].depth,&field.path);self.spend(work)?;if !matched{continue;}
    let attribute=field.path.attribute.as_ref().and_then(|wanted|attributes.iter().find(|(name,_)|name.uri==wanted.uri&&name.local==wanted.local));
    if field.path.attribute.is_some()&&attribute.is_none(){continue;}if constraint.kind==0&&field.nillable{return Err(Error("XSD key field declaration nillable"));}
    let candidate=&mut self.active[candidate_index];if candidate.seen[index]{return Err(Error("XML identity field selects multiple values"));}candidate.seen[index]=true;
    if let Some((_,value))=attribute{let key=Self::key(value,field,self.limits.max_tuple_bytes,resolve,self.operation.as_ref())?;Self::store(candidate,index,key,self.limits.max_tuple_bytes,self.operation.as_ref())?;}
    else{let nil=attributes.iter().find(|(name,_)|name.uri=="http://www.w3.org/2001/XMLSchema-instance"&&name.local=="nil").map(|(_,value)|srl_xml_bool(value)).transpose()?.unwrap_or(false);
     candidate.captures[index]=Some(SrlXmlIdentityCapture{depth:self.path.len(),nil,text:String::new()});}
   }}Ok(())
 }
 /// Join active scalar captures across arbitrary provider text, CDATA, comment and entity boundaries.
 fn text(&mut self,value:&str)->Result<(),Error>{self.spend(value.len().saturating_mul(1+self.active.iter().map(|candidate|candidate.captures.iter().filter(|capture|capture.is_some()).count()).sum::<usize>()))?;
  for candidate in &mut self.active{for capture in candidate.captures.iter_mut().flatten(){if capture.nil&&!value.is_empty(){return Err(Error("Nil XML identity text"));}
   if value.len()>self.limits.max_tuple_bytes.saturating_sub(capture.text.len()){return Err(@xml_operation@::quota(self.operation.as_ref(),"XML identity field-byte limit"));}
   @xml_operation@::consume(self.operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,value.len())?;capture.text.try_reserve(value.len()).map_err(|_|@xml_operation@::quota(self.operation.as_ref(),"XML identity allocation"))?;capture.text.push_str(value);}}
  Ok(())
 }
 /// Finish scalar captures under their original namespace scope, then index complete owner-local tuples.
 fn end(&mut self,resolve:&impl Fn(&str)->Result<String,Error>)->Result<(),Error>{
  if self.path.is_empty()||self.complete{return Err(Error("Unbalanced XML identity end"));}self.spend(1)?;
  let mut position=0;while position<self.active.len(){let constraint=&self.scope.constraints[self.active[position].constraint];
   for index in 0..constraint.fields.len(){if self.active[position].captures[index].as_ref().is_some_and(|capture|capture.depth==self.path.len()){
    let capture=self.active[position].captures[index].take().unwrap();if !capture.nil{let key=Self::key(&capture.text,&constraint.fields[index],self.limits.max_tuple_bytes,resolve,self.operation.as_ref())?;
     Self::store(&mut self.active[position],index,key,self.limits.max_tuple_bytes,self.operation.as_ref())?;}}}
   if self.active[position].depth!=self.path.len(){position+=1;continue;}let candidate=self.active.remove(position);
   if candidate.values.iter().any(Option::is_none){if constraint.kind==0{return Err(Error("XSD key tuple incomplete or nil"));}continue;}
   if let Some(operation)=&self.operation{if let Some(diagnostic)=self.diagnostics.get(candidate.constraint){operation.field(diagnostic.fields.first().copied().unwrap_or(diagnostic.name));}operation.consume(@xml_operation@::Resource::References,1)?;operation.consume(@xml_operation@::Resource::DecoderAllocationBytes,candidate.bytes.saturating_add(std::mem::size_of::<String>()+6*std::mem::size_of::<usize>()))?;}
   if self.tuples>=self.limits.max_tuples{return Err(@xml_operation@::quota(self.operation.as_ref(),"XML identity tuple-count limit"));}if candidate.bytes>self.limits.max_tuple_bytes.saturating_sub(self.bytes){return Err(@xml_operation@::quota(self.operation.as_ref(),"XML identity tuple-byte limit"));}
   self.tuples+=1;self.bytes+=candidate.bytes;self.spend(candidate.bytes+1)?;let mut tuple=String::new();
   for value in candidate.values.into_iter().flatten(){let prefix=value.len().to_string();tuple.try_reserve(prefix.len()+value.len()+1).map_err(|_|@xml_operation@::quota(self.operation.as_ref(),"XML identity allocation"))?;tuple.push_str(&prefix);tuple.push(':');tuple.push_str(&value);}
   if !self.tables[candidate.constraint].insert(tuple)&&constraint.kind!=2{return Err(Error("Duplicate XSD key/unique tuple"));}
  }self.path.pop();self.complete=self.path.is_empty();Ok(())
 }
 /// Resolve deferred forward references only after this exact owner's key/unique indexes are complete.
 fn finish(&mut self)->Result<(),Error>{if !self.complete||!self.path.is_empty()||!self.active.is_empty(){return Err(Error("Incomplete XML identity scope"));}self.spend(1)?;
  for (index,constraint) in self.scope.constraints.iter().enumerate(){if constraint.kind==2{for tuple in &self.tables[index]{if let Some(operation)=&self.operation{if let Some(diagnostic)=self.diagnostics.get(index){operation.field(diagnostic.fields.first().copied().unwrap_or(diagnostic.name));}operation.consume(@xml_operation@::Resource::WorkUnits,tuple.len().saturating_add(1))?;}if !self.tables[constraint.refer].contains(tuple){return Err(Error("Unresolved XSD keyref tuple"));}}}}Ok(())
 }
}
)rustidentity"
R"rustidentity(
/// Select a statically prepared typed transition, including expanded xsi:type dispatch under variant wrappers.
fn srl_xml_identity_edge(node:&SrlXmlIdentityNode,name:&SrlXmlTokenName<'_>,attributes:&[(SrlXmlTokenName<'_>,String)],
 resolve:&impl Fn(&str)->Result<String,Error>)->Result<usize,Error>{
 for edge in node.edges{if edge.name.uri!=name.uri||edge.name.local!=name.local{continue;}
  if let Some(expected)=&edge.type_name{
   let lexical=attributes.iter().find(|(name,_)|name.uri=="http://www.w3.org/2001/XMLSchema-instance"&&name.local=="type").map(|(_,value)|value.as_str()).ok_or(Error("Missing XML identity xsi:type"))?;
   let text=lexical.trim_matches(srl_xml_scalar_space);let mut parts=text.split(':');let first=parts.next().unwrap_or("");let next=parts.next();
   let (prefix,local)=if let Some(local)=next{(first,local)}else{("",first)};
   if parts.next().is_some(){return Err(Error("Invalid XML identity xsi:type"));}SrlXmlRule::item("NCName").check(local,usize::MAX)?;
   if !prefix.is_empty(){SrlXmlRule::item("NCName").check(prefix,usize::MAX)?;}
   if local!=expected.local||resolve(prefix)?!=expected.uri{continue;}
  }return Ok(edge.target);
 }Ok(usize::MAX)
}
/// Check exact owner scopes in one bounded token pass; only active paths, captures and tuple indexes survive.
fn srl_xml_identity_validate(text:&str,limits:XmlLimits,identity_limits:XmlIdentityLimits,graph:&'static [SrlXmlIdentityNode])->Result<(),Error>{srl_xml_identity_validate_with_operation(text,limits,identity_limits,graph,None,&[])}
/// Run secondary identity parsing against the exact caller without repeating logical XML counts.
fn srl_xml_identity_validate_with_operation<'a,'operation>(text:&'a str,limits:XmlLimits,identity_limits:XmlIdentityLimits,graph:&'static [SrlXmlIdentityNode],operation:Option<@xml_operation@::State<'operation>>,diagnostics:&'static [@xml_operation@::SrlXmlIdentityDiagnosticNode])->Result<(),Error>{
 if graph.iter().all(|node|node.scope.is_none()){return Ok(());}
 let mut input=SrlXmlInput::new_with_operation(text,limits,operation.clone())?;let mut path=Vec::new();let mut scopes:Vec<(usize,SrlXmlIdentity<'_>)>=Vec::new();
 loop{
  {let resolve=|prefix:&str|input.namespace(prefix,false);
   match &input.current{
    SrlXmlToken::Start(name,attributes)=>{
     let current=if let Some(parent)=path.last().copied(){if parent==usize::MAX{usize::MAX}else{srl_xml_identity_edge(&graph[parent],name,attributes,&resolve)?}}else{0};
     @xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::WorkUnits,1)?;@xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,std::mem::size_of::<usize>())?;path.try_reserve(1).map_err(|_|@xml_operation@::quota(operation.as_ref(),"XML identity allocation"))?;path.push(current);
     if current!=usize::MAX{if let Some(scope)=graph[current].scope{let descriptor=diagnostics.get(current);if let(Some(operation),Some(descriptor))=(&operation,descriptor){operation.field(descriptor.owner);}@xml_operation@::consume(operation.as_ref(),@xml_operation@::Resource::DecoderAllocationBytes,std::mem::size_of::<(usize,SrlXmlIdentity<'_>)>())?;scopes.try_reserve(1).map_err(|_|@xml_operation@::quota(operation.as_ref(),"XML identity allocation"))?;scopes.push((path.len(),SrlXmlIdentity::new_with_operation(scope,identity_limits,operation.clone(),descriptor.map_or(&[],|item|item.constraints))?));}}
     for (_,scope) in &mut scopes{scope.start(name,attributes,&resolve)?;}
    },
    SrlXmlToken::Text(value)=>{for (_,scope) in &mut scopes{scope.text(value)?;}},
    SrlXmlToken::End(_)=>{
     for (_,scope) in &mut scopes{scope.end(&resolve)?;}
     if scopes.last().is_some_and(|(depth,_)|*depth==path.len()){let (_,mut scope)=scopes.pop().unwrap();scope.finish()?;}
     if path.pop().is_none(){return Err(Error("Unbalanced XML identity graph"));}
    },
    SrlXmlToken::Eof=>{if !path.is_empty()||!scopes.is_empty(){return Err(Error("Incomplete XML identity graph"));}break;}
   }
  }input.advance()?;
 }Ok(())
}
)rustidentity"
