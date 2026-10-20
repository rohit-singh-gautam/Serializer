// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"rustoperation(
/// Additive synchronous XML operations; existing native limits, errors and entry points are unchanged.
pub mod @xml_operation@ {
 use std::cell::RefCell;
 use std::rc::Rc;
 use std::sync::Arc;
 /// Retain caller-owned errors without converting or replacing their concrete identity.
 pub type Failure=Arc<dyn std::error::Error+Send+Sync>;
 /// Return an owning result only after the complete operation succeeds.
 pub type Result<T>=std::result::Result<T,Failure>;
 /// Logical admissions shared with the selected package or application operation.
 #[derive(Clone,Copy,Debug,PartialEq,Eq)]
 pub enum Resource {WorkUnits,DecoderAllocationBytes,XmlNodes,XmlAttributes,ScalarBytes,References}
 /// Borrow one caller-owned ledger for the entire synchronous native codec invocation.
 pub trait Operation {
  fn consume(&mut self,resource:Resource,count:usize,field:&str)->Result<()>;
  fn poll(&mut self,field:&str)->Result<()>;
  fn limit(&mut self,message:&'static str,field:&str)->Failure;
  /// Preserve native semantic failures with the last selected owner/member projection.
  fn codec(&mut self,error:super::Error,field:&str)->Failure {let _=field;Arc::new(error)}
 }
 /// Select existing independent native policies without extending any old public struct.
 pub struct Options<'a> {
  pub limits:super::XmlLimits,pub map_limits:super::XmlMapLimits,
  pub mixed_limits:super::XmlMixedLimits,pub digest_limits:super::XmlDigestLimits,
  pub identity_limits:super::XmlIdentityLimits,pub validator:Option<&'a dyn super::XmlValidator>,
 }
 impl Default for Options<'_> {
  /// Preserve all ordinary native defaults unless the caller explicitly selects another policy.
  fn default()->Self {Self{limits:Default::default(),map_limits:Default::default(),mixed_limits:Default::default(),digest_limits:Default::default(),identity_limits:Default::default(),validator:None}}
 }
 /// Qualified trait methods cannot collide with existing schema methods or native model names.
 pub trait Codec:Sized {
  fn encode_xml_with_operation(&self,options:Options<'_>,operation:&mut dyn Operation)->Result<String>;
  fn decode_xml_with_operation(text:&str,options:Options<'_>,operation:&mut dyn Operation)->Result<Self>;
  fn decode_package_with_operation(text:&str,options:Options<'_>,expected_type:Option<(&str,&str)>,operation:&mut dyn Operation)->Result<Self>;
 }
 /// Diagnostic metadata is parallel to the unchanged native identity descriptors.
 pub(super) struct SrlXmlIdentityDiagnosticConstraint {pub(super) name:&'static str,pub(super) fields:&'static [&'static str]}
 pub(super) struct SrlXmlIdentityDiagnosticScalar {pub(super) uri:&'static str,pub(super) local:&'static str,pub(super) member:&'static str,pub(super) attribute:bool,pub(super) text:bool}
 pub(super) struct SrlXmlIdentityDiagnosticNode {pub(super) owner:&'static str,pub(super) constraints:&'static [SrlXmlIdentityDiagnosticConstraint],pub(super) edges:&'static [&'static str],pub(super) scalars:&'static [SrlXmlIdentityDiagnosticScalar]}
 /// Only an active operation allocates shared synchronous state; no thread-local or global context exists.
 struct Core<'a> {operation:&'a mut dyn Operation,first:Option<Failure>,field:&'static str,
  graph:&'static [super::SrlXmlIdentityNode],diagnostics:&'static [SrlXmlIdentityDiagnosticNode],path:Vec<(usize,&'static str)>}
 #[derive(Clone)]
 pub(super) struct State<'a> {core:Rc<RefCell<Core<'a>>>,primary:bool}
 impl<'a> State<'a> {
  /// Borrow the exact caller and retain one first-failure latch for this invocation only.
  pub(super) fn new(operation:&'a mut dyn Operation,graph:&'static [super::SrlXmlIdentityNode],diagnostics:&'static [SrlXmlIdentityDiagnosticNode])->Result<Self> {
   let field=diagnostics.first().map_or("",|item|item.owner);
   operation.poll(field)?;operation.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<Core<'_>>().saturating_add(2*std::mem::size_of::<usize>()),field)?;
   Ok(Self{core:Rc::new(RefCell::new(Core{operation,first:None,field,graph,diagnostics,path:Vec::new()})),primary:true})
  }
  /// Identity reparsing spends real work and allocation but never repeats primary XML admissions.
  pub(super) fn secondary(&self)->Self {Self{core:self.core.clone(),primary:false}}
  /// Update a compiler-owned native projection before work whose logical member is already selected.
  pub(super) fn field(&self,field:&'static str) {self.core.borrow_mut().field=field;}
  /// Poll and preserve the original caller failure before any further work or admission.
  pub(super) fn poll(&self)->std::result::Result<(),super::Error> {
   let mut core=self.core.borrow_mut();if core.first.is_some(){return Err(super::Error("XML operation failed"));}
   let field=core.field;match core.operation.poll(field){Ok(())=>Ok(()),Err(error)=>{core.first=Some(error);Err(super::Error("XML operation failed"))}}
  }
  /// Charge the selected caller ledger; a failed invocation cannot call it again or clear its error.
  pub(super) fn consume(&self,resource:Resource,count:usize)->std::result::Result<(),super::Error> {
   if !self.primary&&matches!(resource,Resource::XmlNodes|Resource::XmlAttributes|Resource::ScalarBytes){return self.poll();}
   let mut core=self.core.borrow_mut();if core.first.is_some(){return Err(super::Error("XML operation failed"));}
   let field=core.field;match core.operation.consume(resource,count,field){Ok(())=>Ok(()),Err(error)=>{core.first=Some(error);Err(super::Error("XML operation failed"))}}
  }
  /// Classify only an explicit native resource branch, independently of its diagnostic spelling.
  pub(super) fn quota(&self,message:&'static str)->super::Error {
   let mut core=self.core.borrow_mut();if core.first.is_none(){let field=core.field;let error=core.operation.limit(message,field);core.first=Some(error);}super::Error(message)
  }
  /// Prefer the first caller error even when a legacy native conversion catches a local Error.
  pub(super) fn finish<T>(&self,result:std::result::Result<T,super::Error>)->Result<T> {
   if let Some(error)=self.core.borrow().first.as_ref(){return Err(error.clone());}
   result.map_err(|error|{let mut core=self.core.borrow_mut();let field=core.field;core.operation.codec(error,field)})
  }
  /// Select an unambiguous lexical member before its namespace declarations are available.
  pub(super) fn begin(&self,local:&str)->std::result::Result<(),super::Error> {
   self.consume(Resource::DecoderAllocationBytes,std::mem::size_of::<(usize,&str)>())?;
   let mut core=self.core.borrow_mut();let mut node=0;let mut field=core.diagnostics.first().map_or("",|item|item.owner);
   if let Some((parent,parent_field))=core.path.last().copied(){node=usize::MAX;field=parent_field;if parent<core.graph.len(){let mut matches=core.graph[parent].edges.iter().enumerate().filter(|(_,edge)|edge.name.local==local);if let Some((index,edge))=matches.next(){let unique=matches.all(|(_,other)|other.name.uri==edge.name.uri&&other.target==edge.target);if unique{node=edge.target;let selected=core.diagnostics[parent].edges.get(index).copied().unwrap_or("");if !selected.is_empty(){field=selected;}}}}}
   if core.path.try_reserve(1).is_err(){drop(core);return Err(self.quota("XML operation allocation"));}core.path.push((node,field));core.field=field;Ok(())
  }
  /// Refine the pending lexical owner using exact expanded names and prepared variant identities.
  pub(super) fn expanded(&self,name:&super::SrlXmlTokenName<'_>,attributes:&[(super::SrlXmlTokenName<'_>,String)],resolve:&impl Fn(&str)->std::result::Result<String,super::Error>)->std::result::Result<(),super::Error> {
   let (parent,graph,diagnostics,previous)={let core=self.core.borrow();(if core.path.len()>1{Some(core.path[core.path.len()-2].0)}else{None},core.graph,core.diagnostics,core.field)};
   let (node,field)=if let Some(parent)=parent {if parent<graph.len(){let target=super::srl_xml_identity_edge(&graph[parent],name,attributes,resolve)?;let selected=graph[parent].edges.iter().enumerate().find(|(_,edge)|edge.name.uri==name.uri&&edge.name.local==name.local&&edge.target==target).and_then(|(index,_)|diagnostics[parent].edges.get(index)).copied().filter(|field|!field.is_empty()).unwrap_or(previous);(target,selected)}else{(usize::MAX,previous)}}else{(0,diagnostics.first().map_or(previous,|item|item.owner))};
   let mut core=self.core.borrow_mut();if let Some(current)=core.path.last_mut(){*current=(node,field);}core.field=field;Ok(())
  }
  /// Resolve known unambiguous attribute aliases and keep namespace declarations at their enclosing owner.
  pub(super) fn attribute(&self,local:&str) {
   let mut core=self.core.borrow_mut();if let Some((node,owner))=core.path.last().copied(){core.field=owner;if node<core.diagnostics.len(){let mut values=core.diagnostics[node].scalars.iter().filter(|item|item.attribute&&item.local==local);if let Some(item)=values.next(){if values.all(|other|other.member==item.member){core.field=item.member;}}}}
  }
  /// Select one prepared text member before lexical storage; formatting-only records keep their owner.
  pub(super) fn text(&self) {
   let mut core=self.core.borrow_mut();if let Some((node,owner))=core.path.last().copied(){core.field=owner;if node<core.diagnostics.len(){let mut values=core.diagnostics[node].scalars.iter().filter(|item|item.text);if let Some(item)=values.next(){if values.all(|other|other.member==item.member){core.field=item.member;}}}}
  }
  /// Restore the enclosing native projection when provider lookahead leaves a lexical scope.
  pub(super) fn end(&self) {let mut core=self.core.borrow_mut();core.path.pop();if let Some((_,field))=core.path.last().copied(){core.field=field;}}

 }
 /// Preserve ordinary Error values while adding typed resource classification only to active operations.
 pub(super) fn quota(operation:Option<&State<'_>>,message:&'static str)->super::Error {operation.map_or(super::Error(message),|operation|operation.quota(message))}
 /// Admit only when the caller selected the additive operation API.
 pub(super) fn consume(operation:Option<&State<'_>>,resource:Resource,count:usize)->std::result::Result<(),super::Error> {if let Some(operation)=operation{operation.consume(resource,count)?;}Ok(())}
}
)rustoperation"
