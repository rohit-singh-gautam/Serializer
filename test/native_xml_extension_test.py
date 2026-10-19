# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify explicit C/Rust XML documents, owned opaque sidecars and bounded token filter/replay."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import sys

from javascript_xml_extension_test import SCHEMA, CONFIG
from python_xml_extension_test import DOCUMENT, NS

ROOT = Path(__file__).resolve().parent.parent
LONG_ATTRIBUTE_NAMESPACE = 'urn:attrs:'+('a'*512)
spec = importlib.util.spec_from_file_location('serializer_native_extension_sdk', ROOT/'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)
sdk = xml_runner.sdk

RUST = r'''mod schema;
use schema::*;
use std::{env,fs,path::Path};
/// A trusted adapter must see original/final opaque XML, never the filtered known transport.
struct FullXml {calls:std::cell::Cell<usize>,reject:bool}
impl XmlValidator for FullXml {
 fn validate(&self,document:&str)->Result<(),Error> {self.calls.set(self.calls.get()+1);assert!(document.contains("inside") && document.contains("p:Thing"));if self.reject {Err(Error("application XML validator marker"))}else{Ok(())}}
}
/// Read one explicit capability inventory without treating permission as understanding.
fn policy(arguments:&[String])->XmlExtensionOptions {
 let understood=if arguments[5].is_empty(){vec![]}else{arguments[5].split(',').map(str::to_owned).collect()};
 XmlExtensionOptions{limits:XmlExtensionLimits{max_slots:arguments[1].parse().unwrap(),max_items:arguments[2].parse().unwrap(),max_preserved_bytes:arguments[3].parse().unwrap()},required_prefixes:arguments[4].clone(),understood_namespaces:understood}
}
/// Match selected complete documents, atomic failures, opaque replay and every native protocol.
fn main()->Result<(),Box<dyn std::error::Error>> {
 let arguments=env::args().skip(1).collect::<Vec<_>>();let limits=XmlLimits::default();
 for arguments in arguments.chunks_exact(6) {
  let path=&arguments[0];let name=Path::new(path).file_name().unwrap().to_str().unwrap();let valid=name.ends_with(".valid.xml");let source=fs::read_to_string(path)?;let options=policy(arguments);
  if name.starts_with("skip") {
   let document=ExtensionOnlySkip::decode_xml_document(&source,limits,None,&options);
   match document {Ok(document)=>{assert!(valid,"Invalid skip accepted: {name}");assert_eq!(document.value.value,1);assert_eq!(document.extensions.preserved_bytes(),0);fs::write(format!("{path}.rust.xml"),document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options)?)?;},Err(error)=>{if valid{return Err(Box::new(error));}}}continue;
  }
  let decoded=ExtensionRoot::decode_xml_document(&source,limits,None,&options);
  let document=match decoded {Ok(document)=>{assert!(valid,"Invalid extension accepted: {name}");document},Err(error)=>{if valid{return Err(Box::new(error));}continue;}};
  assert_eq!((document.value.id,document.value.tail.as_str()),(7,"Z"));
  assert!(ExtensionRoot::decode_xml(&source).is_err());assert!(document.value.encode_xml().is_err());
  let native=ExtensionRoot{id:document.value.id,items:document.value.items.clone(),tail:document.value.tail.clone()};
  let output=document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options)?;
  let copy=ExtensionRoot::decode_xml_document(&output,limits,None,&options)?;
  for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {
   assert_eq!(native.encode(protocol)?,document.value.encode(protocol)?);assert_eq!(document.value.encode(protocol)?,copy.value.encode(protocol)?);
  }
  assert_eq!(output,copy.value.encode_xml_with_extensions(&copy.extensions,limits,None,&options)?);
  fs::write(format!("{path}.rust.xml"),output)?;
 }
 let directory=Path::new(&arguments[0]).parent().unwrap();let source=fs::read_to_string(directory.join("document.xml"))?;
 let mut options=XmlExtensionOptions{understood_namespaces:vec!["urn:vendor".into(),"urn:parent".into()],required_prefixes:"p".into(),..XmlExtensionOptions::default()};
 let mut document=ExtensionRoot::decode_xml_document(&source,limits,None,&options)?;let exact=document.extensions.preserved_bytes();assert!(exact>0);options.limits.max_preserved_bytes=exact;
 let observer=FullXml{calls:std::cell::Cell::new(0),reject:false};let observed=ExtensionRoot::decode_xml_document(&source,limits,Some(&observer),&options)?;observed.value.encode_xml_with_extensions(&observed.extensions,limits,Some(&observer),&options)?;assert_eq!(observer.calls.get(),2);
 let rejected=FullXml{calls:std::cell::Cell::new(0),reject:true};assert!(ExtensionRoot::decode_xml_document(&source,limits,Some(&rejected),&options).is_err());assert!(observed.value.encode_xml_with_extensions(&observed.extensions,limits,Some(&rejected),&options).is_err());
 document=ExtensionRoot::decode_xml_document(&source,limits,None,&options)?;
 let mut previous=String::new();for _ in 0..4 {let encoded=document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options)?;if !previous.is_empty(){assert_eq!(previous,encoded);}previous=encoded;document=ExtensionRoot::decode_xml_document(&previous,limits,None,&options)?;}
 assert!(document.value.encode_xml_with_extensions(&document.extensions,XmlLimits{max_bytes:previous.len()-1,..limits},None,&options).is_err());
 options.limits.max_preserved_bytes=exact-1;assert!(ExtensionRoot::decode_xml_document(&source,limits,None,&options).is_err());assert!(document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options).is_err());options.limits.max_preserved_bytes=exact;
 document.value.items.push(3);let edited=document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options)?;assert_eq!(ExtensionRoot::decode_xml_document(&edited,limits,None,&options)?.value.items,vec![1,2,3]);
 document.value.items.clear();assert!(document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options).is_err());
 let precise=XmlLimits{max_bytes:source.len(),..limits};ExtensionRoot::decode_xml_document(&source,precise,None,&options)?;
 for short in [XmlLimits{max_bytes:source.len()-1,..limits},XmlLimits{max_depth:1,..limits},XmlLimits{max_nodes:1,..limits},XmlLimits{max_text_bytes:0,..limits}] {assert!(ExtensionRoot::decode_xml_document(&source,short,None,&options).is_err());}
 let holder=fs::read_to_string(directory.join("nested.xml"))?;options.limits=XmlExtensionLimits::default();let value=ExtensionHolder::decode_xml_document(&holder,limits,None,&options)?;assert_eq!(value.value.values.len(),2);
 let nested_output=value.value.encode_xml_with_extensions(&value.extensions,limits,None,&options)?;let nested_copy=ExtensionHolder::decode_xml_document(&nested_output,limits,None,&options)?;assert_eq!(nested_output,nested_copy.value.encode_xml_with_extensions(&nested_copy.extensions,limits,None,&options)?);fs::write(directory.join("nested.rust.xml"),nested_output)?;
 options.limits.max_items=9;assert!(ExtensionHolder::decode_xml_document(&holder,limits,None,&options).is_err());options.limits=XmlExtensionLimits::default();
 let tree=fs::read_to_string(directory.join("recursive.xml"))?;let value=ExtensionTree::decode_xml_document(&tree,limits,None,&options)?;fs::write(directory.join("recursive.rust.xml"),value.value.encode_xml_with_extensions(&value.extensions,limits,None,&options)?)?;
 let text=fs::read_to_string(directory.join("textual.xml"))?;let value=ExtensionTextual::decode_xml_document(&text,limits,None,&options)?;assert_eq!(value.value.content,"A<&");fs::write(directory.join("textual.rust.xml"),value.value.encode_xml_with_extensions(&value.extensions,limits,None,&options)?)?;
 macro_rules! framing {($target:ty,$label:literal)=>{{
  let mut policy=XmlExtensionOptions::default();let source=fs::read_to_string(directory.join(concat!($label,".xml")))?;let mut document=<$target>::decode_xml_document(&source,limits,None,&policy)?;policy.limits.max_preserved_bytes=document.extensions.preserved_bytes();let mut output=document.value.encode_xml_with_extensions(&document.extensions,limits,None,&policy)?;let exact=XmlLimits{max_bytes:output.len(),..limits};
  for _ in 0..4 {assert_eq!(output,document.value.encode_xml_with_extensions(&document.extensions,exact,None,&policy)?);document=<$target>::decode_xml_document(&output,limits,None,&policy)?;let next=document.value.encode_xml_with_extensions(&document.extensions,exact,None,&policy)?;assert_eq!(output,next);output=next;}
  assert!(document.value.encode_xml_with_extensions(&document.extensions,XmlLimits{max_bytes:output.len()-1,..limits},None,&policy).is_err());fs::write(directory.join(concat!($label,".rust.xml")),output)?;
 }}}
 framing!(ExtensionDense,"dense");framing!(ExtensionPlain,"plain");
 let long_source=fs::read_to_string(directory.join("dense-long.xml"))?;let long_policy=XmlExtensionOptions::default();let long_document=ExtensionDenseLong::decode_xml_document(&long_source,limits,None,&long_policy)?;assert_eq!(long_document.value.a31,0);fs::write(directory.join("dense-long.rust.xml"),long_document.value.encode_xml_with_extensions(&long_document.extensions,limits,None,&long_policy)?)?;
 println!("Rust explicit XML documents: opaque scope, exact bounds, recursive owners and native controls pass");Ok(())
}'''

RUST_COMPOSITION = r'''
/// Exercise original/final observers directly without reopening the fixed-XSD extension-overlay gate.
fn main()->Result<(),Box<dyn std::error::Error>> {
 static GRAPH:&[SrlXmlIdentityNode]=&[SrlXmlIdentityNode{scope:Some(&SrlXmlIdentityScope{constraints:&[
  SrlXmlIdentityConstraint{kind:0,selector:SrlXmlIdentityPath{steps:&[SrlXmlIdentityName{uri:"urn:serializer:extension-tokens",local:"item"}],descendants:true,attribute:None},
   fields:&[SrlXmlIdentityField{path:SrlXmlIdentityPath{steps:&[],descendants:false,attribute:None},kind:"unsignedInt",nillable:false}],refer:usize::MAX}
 ]}),edges:&[]}];
 let source=std::fs::read_to_string(std::env::args().nth(1).unwrap())?;
 let limits=XmlLimits::default();let options=XmlExtensionOptions{understood_namespaces:vec!["urn:vendor".into(),"urn:parent".into()],..XmlExtensionOptions::default()};
 let (known,state)=srl_xml_extension_filter(&source,limits,&options,ExtensionRoot::srl_xml_extension_model())?;
 srl_xml_identity_validate(&known,limits,XmlIdentityLimits::default(),GRAPH)?;
 assert!(srl_xml_identity_validate(&source,limits,XmlIdentityLimits::default(),GRAPH).is_err());
 let final_document=srl_xml_extension_replay(&known,limits,&options,ExtensionRoot::srl_xml_extension_model(),&state)?;
 assert!(srl_xml_identity_validate(&final_document,limits,XmlIdentityLimits::default(),GRAPH).is_err());
 println!("Rust original/final identity composition rejects a duplicate hidden inside preserved foreign XML");Ok(())
}
'''

C = r'''#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Keep failed operations outside the expected-error path and expose their fixture name. */
static void require(bool condition,const char* message) {if(!condition){fprintf(stderr,"%s\n",message);exit(EXIT_FAILURE);}}
/* Read independently owned source bytes, permitting the caller to free input before replay. */
static uint8_t* read_file(const char* path,size_t* size) {FILE* file=fopen(path,"rb");require(file!=NULL,"Fixture open");require(fseek(file,0,SEEK_END)==0,"Fixture seek");long length=ftell(file);require(length>=0,"Fixture size");rewind(file);*size=(size_t)length;uint8_t* result=(uint8_t*)malloc(*size ? *size : 1);require(result!=NULL,"Fixture allocation");require(fread(result,1,*size,file)==*size,"Fixture read");fclose(file);return result;}
/* Write only completely published output to an independent oracle fixture. */
static void write_file(const char* path,const srl_buffer* value) {FILE* file=fopen(path,"wb");require(file!=NULL && fwrite(value->data,1,value->size,file)==value->size,"Fixture write");fclose(file);}
typedef struct full_xml {size_t calls;bool reject;} full_xml;
/* Search explicit byte extents; callback documents need not have an additional NUL terminator. */
static bool contains(const uint8_t* bytes,size_t size,const char* text) {const size_t length=strlen(text);if(length>size){return false;}for(size_t index=0;index<=size-length;++index){if(!memcmp(bytes+index,text,length)){return true;}}return false;}
/* Require the original/final foreign payload and preserve the exact application status on failure. */
static srl_status full_xml_validate(const uint8_t* bytes,size_t size,const srl_limits* limits,void* context) {full_xml* value=(full_xml*)context;(void)limits;++value->calls;require(contains(bytes,size,"inside") && contains(bytes,size,"p:Thing"),"Validator received filtered transport");return value->reject ? srl_range : srl_ok;}
/* Build bounded derived paths without changing caller-owned argument storage. */
static void path_join(char* result,size_t capacity,const char* directory,const char* name) {int count=snprintf(result,capacity,"%s/%s",directory,name);require(count>=0 && (size_t)count<capacity,"Fixture path bound");}
/* Parse fixture policies; the fixed URI array is a borrowed call-local capability inventory. */
static srl_xml_extension_options policy(char** arguments,char** inventory,const char** understood) {
 srl_xml_extension_options result=srl_xml_default_extension_options();result.limits.max_slots=(size_t)strtoull(arguments[1],NULL,10);result.limits.max_items=(size_t)strtoull(arguments[2],NULL,10);result.limits.max_preserved_bytes=(size_t)strtoull(arguments[3],NULL,10);result.required_prefixes=arguments[4];
 *inventory=(char*)malloc(strlen(arguments[5])+1);require(*inventory!=NULL,"Capability allocation");strcpy(*inventory,arguments[5]);char* current=strtok(*inventory,",");while(current){require(result.understood_count<3,"Capability fixture bound");understood[result.understood_count++]=current;current=strtok(NULL,",");}result.understood_namespaces=understood;return result;
}
/* Prove filtering cannot hide descendant identity tuples, without changing fixed-XSD generation gates. */
static void identity_composition(const char* path) {
 static const srl_xml_identity_name steps[]={{"urn:serializer:extension-tokens","item"}};
 static const srl_xml_identity_field fields[]={{{NULL,0,false,NULL},"unsignedInt",false}};
 static const srl_xml_identity_constraint constraints[]={{0,{steps,1,true,NULL},fields,1,SIZE_MAX}};
 static const srl_xml_identity_scope scope={constraints,1};static const srl_xml_identity_node graph[]={{&scope,NULL,0}};
 const srl_limits limits=srl_default_limits();const srl_xml_identity_limits identity_limits=srl_xml_default_identity_limits();
 const char* const understood[]={"urn:vendor","urn:parent"};srl_xml_extension_options options=srl_xml_default_extension_options();options.understood_namespaces=understood;options.understood_count=2;
 size_t size=0;uint8_t* bytes=read_file(path,&size);srl_buffer known={0},final_document={0};srl_xml_extension_state* state=NULL;
 require(srl_xml_extension_filter(bytes,size,&limits,&options,extension_root_srl_xml_extension_model(),&known,&state)==srl_ok,"Composition filter");
 require(srl_xml_identity_validate(known.data,known.size,&limits,&identity_limits,graph,1)==srl_ok,"Known transport identity");
 require(srl_xml_identity_validate(bytes,size,&limits,&identity_limits,graph,1)!=srl_ok,"Original identity hidden by foreign filter");
 require(srl_xml_extension_replay(known.data,known.size,&limits,&options,extension_root_srl_xml_extension_model(),state,&final_document)==srl_ok,"Composition replay");
 require(srl_xml_identity_validate(final_document.data,final_document.size,&limits,&identity_limits,graph,1)!=srl_ok,"Final identity hidden by foreign replay");
 free(bytes);srl_buffer_free(&known);srl_buffer_free(&final_document);srl_xml_extension_state_free(state);
}
/* Exercise original/final full documents, stable native wire bytes and C transactional ownership. */
int main(int argc,char** argv) {
 const srl_limits limits=srl_default_limits();
 for(int offset=1;offset+5<argc;offset+=6) {
  char** arguments=argv+offset;const char* path=arguments[0];const size_t length=strlen(path);const bool valid=length>=10 && !strcmp(path+length-10,".valid.xml");const char* name=strrchr(path,'/');if(!name){name=strrchr(path,'\\');}name=name?name+1:path;
  char* inventory=NULL;const char* understood[3]={0};srl_xml_extension_options options=policy(arguments,&inventory,understood);size_t size=0;uint8_t* bytes=read_file(path,&size);srl_buffer output={0};srl_status status;
  if(!strncmp(name,"skip",4)) {extension_only_skip_xml_document document={0};require(extension_only_skip_xml_document_init(&document)==srl_ok,"Skip init");status=extension_only_skip_decode_xml_document(&document,bytes,size,&limits,NULL,&options);free(bytes);if(status==srl_ok){require(valid,"Invalid skip accepted");require(document.value.value==1 && srl_xml_extension_preserved_bytes(document.extensions)==0,"Skip retained state");status=extension_only_skip_encode_xml_document(&document,&output,&limits,NULL,&options);}extension_only_skip_xml_document_free(&document);}
  else {
   extension_root_xml_document document={0},copy={0};require(extension_root_xml_document_init(&document)==srl_ok && extension_root_xml_document_init(&copy)==srl_ok,"Document init");document.value.id=99;const extension_root original=document.value;const srl_xml_extension_state* original_state=document.extensions;
   status=extension_root_decode_xml_document(&document,bytes,size,&limits,NULL,&options);free(bytes);
   if(status!=srl_ok){require(memcmp(&document.value,&original,sizeof(original))==0 && document.extensions==original_state,"Failed input changed model/sidecar");}
   else {require(valid,"Invalid extension accepted");require(document.value.id==7 && srl_string_equal(&document.value.tail,"Z"),"Known fields changed");
    require(extension_root_encode_xml(&document.value,&output,&limits,NULL)!=srl_ok,"Ordinary extension graph output accepted");
    status=extension_root_encode_xml_document(&document,&output,&limits,NULL,&options);require(status==srl_ok,"Companion output");
    require(extension_root_decode_xml_document(&copy,output.data,output.size,&limits,NULL,&options)==srl_ok,"Companion replay");
    extension_root native={0};require(extension_root_init(&native)==srl_ok,"Native control init");native.id=document.value.id;native.items.data=(uint32_t*)malloc(document.value.items.size*sizeof(uint32_t));require(native.items.data!=NULL,"Native items");native.items.size=native.items.capacity=document.value.items.size;memcpy(native.items.data,document.value.items.data,native.items.size*sizeof(uint32_t));require(srl_string_set(&native.tail,document.value.tail.data,document.value.tail.size)==srl_ok,"Native text");
    const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){srl_buffer first={0},second={0},third={0};require(extension_root_encode(&native,protocols[index],&first)==srl_ok && extension_root_encode(&document.value,protocols[index],&second)==srl_ok && extension_root_encode(&copy.value,protocols[index],&third)==srl_ok,"Native encode");require(first.size==second.size && first.size==third.size && !memcmp(first.data,second.data,first.size) && !memcmp(first.data,third.data,first.size),"Native wire changed");srl_buffer_free(&first);srl_buffer_free(&second);srl_buffer_free(&third);}extension_root_free(&native);
    srl_buffer again={0};require(extension_root_encode_xml_document(&copy,&again,&limits,NULL,&options)==srl_ok,"Repeat companion output");if(again.size!=output.size || memcmp(again.data,output.data,output.size)){char diagnostic[4096];int count=snprintf(diagnostic,sizeof(diagnostic),"%s.first.xml",path);require(count>=0 && (size_t)count<sizeof(diagnostic),"Diagnostic bound");write_file(diagnostic,&output);count=snprintf(diagnostic,sizeof(diagnostic),"%s.again.xml",path);require(count>=0 && (size_t)count<sizeof(diagnostic),"Diagnostic bound");write_file(diagnostic,&again);require(false,"Unstable opaque replay");}srl_buffer_free(&again);
   }extension_root_xml_document_free(&document);extension_root_xml_document_free(&copy);
  }
  if(status!=srl_ok){require(!valid,path);}else{require(valid,"Invalid output accepted");char destination[4096];int count=snprintf(destination,sizeof(destination),"%s.c.xml",path);require(count>=0 && (size_t)count<sizeof(destination),"Output path bound");write_file(destination,&output);}srl_buffer_free(&output);free(inventory);
 }
 char directory[4096];require(strlen(argv[1])<sizeof(directory),"Directory bound");strcpy(directory,argv[1]);char* slash=strrchr(directory,'/');if(!slash){slash=strrchr(directory,'\\');}require(slash!=NULL,"Fixture directory");*slash=0;
 char path[4096];path_join(path,sizeof(path),directory,"document.xml");size_t size=0;uint8_t* bytes=read_file(path,&size);const char* const understood[]={"urn:vendor","urn:parent"};srl_xml_extension_options options=srl_xml_default_extension_options();options.understood_namespaces=understood;options.understood_count=2;options.required_prefixes="p";
 extension_root_xml_document document={0};require(extension_root_xml_document_init(&document)==srl_ok && extension_root_decode_xml_document(&document,bytes,size,&limits,NULL,&options)==srl_ok,"Exact preparation");const size_t exact=srl_xml_extension_preserved_bytes(document.extensions);require(exact>0,"Preserved byte accounting");options.limits.max_preserved_bytes=exact;
 full_xml observer={0};srl_xml_validation validation={full_xml_validate,&observer};srl_buffer inspected={0};require(extension_root_decode_xml_document(&document,bytes,size,&limits,&validation,&options)==srl_ok && extension_root_encode_xml_document(&document,&inspected,&limits,&validation,&options)==srl_ok && observer.calls==2,"Original/final validator callbacks");const extension_root before_callback=document.value;const srl_xml_extension_state* state_before_callback=document.extensions;uint8_t* inspected_before=inspected.data;observer.reject=true;require(extension_root_decode_xml_document(&document,bytes,size,&limits,&validation,&options)==srl_range && !memcmp(&before_callback,&document.value,sizeof(before_callback)) && document.extensions==state_before_callback,"Validator failure changed document");require(extension_root_encode_xml_document(&document,&inspected,&limits,&validation,&options)==srl_range && inspected.data==inspected_before,"Validator failure changed output");srl_buffer_free(&inspected);
 srl_buffer previous={0};for(size_t pass=0;pass<4;++pass){srl_buffer current={0};require(extension_root_encode_xml_document(&document,&current,&limits,NULL,&options)==srl_ok,"Exact output");if(previous.data){require(previous.size==current.size && !memcmp(previous.data,current.data,current.size),"Canonical replay changed");}require(extension_root_decode_xml_document(&document,current.data,current.size,&limits,NULL,&options)==srl_ok,"Exact replay");srl_buffer_free(&previous);previous=current;}
 srl_limits output_bound=limits;output_bound.max_bytes=previous.size-1;uint8_t* bounded_output=previous.data;require(extension_root_encode_xml_document(&document,&previous,&output_bound,NULL,&options)!=srl_ok && previous.data==bounded_output,"Final XML byte limit ignored or output changed");
 options.limits.max_preserved_bytes=exact-1;const extension_root original=document.value;const srl_xml_extension_state* stored=document.extensions;require(extension_root_decode_xml_document(&document,bytes,size,&limits,NULL,&options)!=srl_ok && !memcmp(&original,&document.value,sizeof(original)) && document.extensions==stored,"Failed exact input changed state");uint8_t* published=previous.data;require(extension_root_encode_xml_document(&document,&previous,&limits,NULL,&options)!=srl_ok && previous.data==published,"Failed output replaced bytes");options.limits.max_preserved_bytes=exact;
 srl_limits short_limit=limits;short_limit.max_bytes=size;require(extension_root_decode_xml_document(&document,bytes,size,&short_limit,NULL,&options)==srl_ok,"Exact original XML byte bound");--short_limit.max_bytes;require(extension_root_decode_xml_document(&document,bytes,size,&short_limit,NULL,&options)!=srl_ok,"Original XML byte limit ignored");
 short_limit=limits;short_limit.max_depth=1;require(extension_root_decode_xml_document(&document,bytes,size,&short_limit,NULL,&options)!=srl_ok,"Original XML depth limit ignored");short_limit=limits;short_limit.max_elements=1;require(extension_root_decode_xml_document(&document,bytes,size,&short_limit,NULL,&options)!=srl_ok,"Original XML node limit ignored");short_limit=limits;short_limit.max_string_bytes=0;require(extension_root_decode_xml_document(&document,bytes,size,&short_limit,NULL,&options)!=srl_ok,"Original XML text limit ignored");free(bytes);
 document.value.items.size=0;require(extension_root_encode_xml_document(&document,&previous,&limits,NULL,&options)!=srl_ok && previous.data==published,"Edited repeated boundary accepted");extension_root_xml_document_free(&document);srl_buffer_free(&previous);
 options.limits=srl_xml_default_extension_options().limits;path_join(path,sizeof(path),directory,"nested.xml");bytes=read_file(path,&size);extension_holder_xml_document holder={0};require(extension_holder_xml_document_init(&holder)==srl_ok && extension_holder_decode_xml_document(&holder,bytes,size,&limits,NULL,&options)==srl_ok,"Nested owner decode");require(holder.value.values.size==2,"Nested owner count");srl_buffer output={0};require(extension_holder_encode_xml_document(&holder,&output,&limits,NULL,&options)==srl_ok,"Nested owner output");path_join(path,sizeof(path),directory,"nested.c.xml");write_file(path,&output);options.limits.max_items=9;require(extension_holder_decode_xml_document(&holder,bytes,size,&limits,NULL,&options)!=srl_ok,"Nested extension budget reset");options.limits=srl_xml_default_extension_options().limits;require(extension_holder_decode_xml_document(&holder,output.data,output.size,&limits,NULL,&options)==srl_ok,"Nested replay decode");srl_buffer nested_again={0};require(extension_holder_encode_xml_document(&holder,&nested_again,&limits,NULL,&options)==srl_ok && output.size==nested_again.size && !memcmp(output.data,nested_again.data,output.size),"Nested namespace context changed");srl_buffer_free(&nested_again);free(bytes);extension_holder_xml_document_free(&holder);srl_buffer_free(&output);
 path_join(path,sizeof(path),directory,"recursive.xml");bytes=read_file(path,&size);extension_tree_xml_document tree={0};require(extension_tree_xml_document_init(&tree)==srl_ok && extension_tree_decode_xml_document(&tree,bytes,size,&limits,NULL,&options)==srl_ok,"Recursive owner decode");require(extension_tree_encode_xml_document(&tree,&output,&limits,NULL,&options)==srl_ok,"Recursive owner output");path_join(path,sizeof(path),directory,"recursive.c.xml");write_file(path,&output);free(bytes);extension_tree_xml_document_free(&tree);srl_buffer_free(&output);
 path_join(path,sizeof(path),directory,"textual.xml");bytes=read_file(path,&size);extension_textual_xml_document text={0};require(extension_textual_xml_document_init(&text)==srl_ok && extension_textual_decode_xml_document(&text,bytes,size,&limits,NULL,&options)==srl_ok,"Textual owner decode");require(srl_string_equal(&text.value.content,"A<&") && extension_textual_encode_xml_document(&text,&output,&limits,NULL,&options)==srl_ok,"Textual owner output");path_join(path,sizeof(path),directory,"textual.c.xml");write_file(path,&output);free(bytes);
 path_join(path,sizeof(path),directory,"fragmented-textual.xml");bytes=read_file(path,&size);short_limit=limits;short_limit.max_string_bytes=sizeof("urn:serializer:extension-tokens")-1;char* previous_text=text.value.content.data;const srl_xml_extension_state* previous_text_state=text.extensions;require(extension_textual_decode_xml_document(&text,bytes,size,&short_limit,NULL,&options)!=srl_ok && text.value.content.data==previous_text && text.extensions==previous_text_state,"Canonical transport relaxed modeled aggregate string bound");free(bytes);extension_textual_xml_document_free(&text);srl_buffer_free(&output);
 path_join(path,sizeof(path),directory,"identity-composition.xml");identity_composition(path);
 check_extension_dense_framing(directory);check_extension_plain_framing(directory);check_extension_dense_long_framing(directory);
 puts("C explicit XML documents: atomic ownership, opaque scope, exact bounds, original/final identity and recursive native controls pass");return EXIT_SUCCESS;
}'''

C_FRAMING = r'''
/* Admit final bytes exactly even when private canonical aliases need a larger bounded transport. */
static void check_TYPE_framing(const char* directory) {
 char path[4096];path_join(path,sizeof(path),directory,"LABEL.xml");size_t size=0;uint8_t* bytes=read_file(path,&size);const srl_limits limits=srl_default_limits();srl_xml_extension_options options=srl_xml_default_extension_options();TYPE_xml_document document={0};require(TYPE_xml_document_init(&document)==srl_ok && TYPE_decode_xml_document(&document,bytes,size,&limits,NULL,&options)==srl_ok,"Framing preparation");free(bytes);options.limits.max_preserved_bytes=srl_xml_extension_preserved_bytes(document.extensions);srl_buffer output={0};require(TYPE_encode_xml_document(&document,&output,&limits,NULL,&options)==srl_ok,"Framing output");srl_limits exact=limits;exact.max_bytes=output.size;
 for(size_t pass=0;pass<4;++pass){srl_buffer next={0};require(TYPE_encode_xml_document(&document,&next,&exact,NULL,&options)==srl_ok && next.size==output.size && !memcmp(next.data,output.data,output.size),"Exact final byte framing");require(TYPE_decode_xml_document(&document,next.data,next.size,&limits,NULL,&options)==srl_ok,"Framing roundtrip");srl_buffer_free(&output);output=next;}
 exact.max_bytes=output.size-1;uint8_t* published=output.data;require(TYPE_encode_xml_document(&document,&output,&exact,NULL,&options)!=srl_ok && output.data==published,"Short final byte bound changed output");path_join(path,sizeof(path),directory,"LABEL.c.xml");write_file(path,&output);srl_buffer_free(&output);TYPE_xml_document_free(&document);
}
'''
C = C.replace('/* Exercise original/final full documents, stable native wire bytes and C transactional ownership. */',
              ''.join(C_FRAMING.replace('TYPE', 'extension_'+name).replace('LABEL', name) for name in ('dense', 'plain'))+
              C_FRAMING.replace('TYPE', 'extension_dense_long').replace('LABEL', 'dense-long').replace('exact.max_bytes=output.size;', 'exact.max_bytes=limits.max_bytes;')+
              '/* Exercise original/final full documents, stable native wire bytes and C transactional ownership. */')


def main():
    """Generate actual companions and use a separate XSD provider for vocabulary and opaque namespace checks."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--language', default='rust,c')
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--python-dependencies', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree
    from portable_xml_extension_test import cases, XSD, check_output
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    languages = args.language.split(',')
    if not languages or len(set(languages)) != len(languages) or any(value not in ('c', 'rust') for value in languages):
        parser.error('Choose distinct languages from c,rust')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Scratch must remain beneath ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault('CARGO_HOME', str(args.build/'cargo-home'))
    os.environ['RUSTFLAGS'] = '-D warnings'
    paths = []
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    vocabulary_cases = [value for value in cases() if not value[0].startswith('text-')]+[('root-first-child-scope', DOCUMENT.decode().replace(' m:label="p:Thing"', ''),
                              (5, 100, 65536, '', 'urn:vendor,urn:parent'), True, True),
                              ('root-model-child-native', DOCUMENT.decode().replace('<e:after>kept</e:after>',
                              '<x:unknown xmlns:x="urn:serializer:xml:model"/>'),
                              (5, 100, 65536, '', 'urn:vendor,urn:parent'), False, False)]
    for label, document, policy, valid, xml_valid in vocabulary_cases:
        try:
            actual = oracle.validate(etree.fromstring(document.encode()))
        except etree.XMLSyntaxError:
            actual = False
        if actual != xml_valid:
            raise AssertionError('Independent vocabulary oracle differs: '+label)
        path = args.build/f'{label}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8')
        paths.append((path, policy))
    model = SCHEMA[:SCHEMA.rfind('}')]+'''class holder {public array<root> values;}
    class tree {public array<holder> children;}
    class textual {public string content xml(text);}
    class plain {public uint32 value;}
    class dense {'''+''.join(f'public uint32 a{index} xml(attribute, namespace("urn:attrs"));' for index in range(32))+'} class dense_long {'+''.join(f'public uint32 a{index} xml(attribute, namespace("{LONG_ATTRIBUTE_NAMESPACE}"));' for index in range(32))+'}}'
    config = CONFIG.replace('enabled=true', 'enabled=true\nruntime_xsd=true\nvalidation_scope=both')
    config = config.replace('extension.attributes.namespaces=urn:metadata',
                            'extension.attributes.namespaces=urn:metadata urn:serializer:xml:model')
    for name, extra in [('holder', 'name.values=root\n'), ('tree', 'name.children=branch\n'),
                        ('textual', 'extension.attributes.kind=attribute\nextension.attributes.namespaces=urn:metadata\nextension.attributes.policy=preserve\n')]:
        config += f'[xml.class.extension::{name}]\nnamespace={NS}\n'+extra
    for name, namespace in [('dense', 'urn:core'), ('dense_long', 'urn:core'), ('plain', '')]:
        config += f'[xml.class.extension::{name}]\n'+(f'namespace={namespace}\n' if namespace else '')+'extension.payload.kind=element\nextension.payload.namespaces=urn:vendor\nextension.payload.policy=preserve\n'
    (args.build/'model.serializer').write_text(model, encoding='utf-8')
    (args.build/'options.ini').write_text(config, encoding='utf-8')
    text = DOCUMENT.decode()
    (args.build/'document.xml').write_bytes(DOCUMENT)
    (args.build/'identity-composition.xml').write_text(text.replace('<v:between/>', '<v:between><item>1</item></v:between>'), encoding='utf-8')
    (args.build/'nested.xml').write_text(f'<holder xmlns="{NS}">'+text+text+'</holder>', encoding='utf-8')
    (args.build/'recursive.xml').write_text(f'<tree xmlns="{NS}"><branch>'+text+'</branch></tree>', encoding='utf-8')
    (args.build/'textual.xml').write_text(f'<textual xmlns="{NS}" xmlns:m="urn:metadata" xmlns:p="urn:parent" m:label="p:Thing">A&lt;&amp;</textual>', encoding='utf-8')
    (args.build/'fragmented-textual.xml').write_text(f'<textual xmlns="{NS}" xmlns:m="urn:metadata" xmlns:p="urn:parent" m:label="p:Thing">'+('A<![CDATA[A]]>'*32)+'</textual>', encoding='utf-8')
    (args.build/'dense.xml').write_text('<dense xmlns="urn:core" xmlns:p="urn:attrs" xmlns:v="urn:vendor" '+
                                      ' '.join(f'p:a{index}="0"' for index in range(32))+'><v:x/></dense>', encoding='utf-8')
    (args.build/'plain.xml').write_text('<plain xmlns:v="urn:vendor"><value>1</value><v:x/></plain>', encoding='utf-8')
    (args.build/'dense-long.xml').write_text(f'<dense_long xmlns="urn:core" xmlns:p="{LONG_ATTRIBUTE_NAMESPACE}" xmlns:v="urn:vendor" '+
                                           ' '.join(f'p:a{index}="0"' for index in range(32))+'><v:x/></dense_long>', encoding='utf-8')
    command = [args.compiler, '--input', args.build/'model.serializer', '--config', args.build/'options.ini', '--language', ','.join(languages)]
    for language in languages:
        directory = args.build/language
        directory.mkdir(exist_ok=True)
        command += [f'--{language}.output', directory/sdk.OUTPUTS[language]]
    sdk.run(command)
    runner = sdk.Runner(args)
    for language in languages:
        source = args.build/'source'/language
        source.mkdir(parents=True, exist_ok=True)
        (source/('main.rs' if language == 'rust' else 'main.c')).write_text(RUST if language == 'rust' else C, encoding='utf-8')
        if language == 'rust':
            for filename in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(ROOT/'example/xml/rust'/filename, source/filename)
            # Compile the unchanged generated runtime at crate scope with one direct private-contract probe.
            # The main SDK consumer still imports the original generated schema as an ordinary module.
            (source/'composition.rs').write_text((args.build/'rust/schema.rs').read_text(encoding='utf-8')+RUST_COMPOSITION, encoding='utf-8')
            with (source/'Cargo.toml').open('a', encoding='utf-8') as manifest:
                manifest.write('\n[[bin]]\nname="serializer-xml-extension-composition"\npath="composition.rs"\n')
        launch = xml_runner.build_language(args, runner, language, args.build/language, source_root=source.parent)
        arguments = []
        for path, policy in paths:
            Path(str(path)+f'.{language}.xml').unlink(missing_ok=True)
            arguments += [runner.path(language, path), *policy]
        sdk.run([*launch, *arguments])
        if language == 'rust':
            executable = args.build/'rust/target/release'/('serializer-xml-extension-composition'+('.exe' if os.name == 'nt' and language not in runner.wsl else ''))
            sdk.run([*runner.launch(language, runner.path(language, executable)), runner.path(language, args.build/'identity-composition.xml')])
        for path, _ in paths:
            output = Path(str(path)+f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(output)))
                if path.name.startswith('root-first-child-scope'):
                    root = etree.parse(str(output)).getroot()
                    assert root.get('{urn:metadata}label') is None and root.nsmap['p'] == 'urn:parent'
                    assert root[0][0].nsmap['p'] == 'urn:inner' and root[0][1].tag == 'plain'
                else:
                    check_output(output.read_bytes())
            elif output.exists():
                raise AssertionError('Invalid input published output: '+str(output))
        nested = etree.parse(str(args.build/f'nested.{language}.xml')).getroot()
        assert len(nested) == 2
        for owner in nested:
            check_output(etree.tostring(owner))
        recursive = etree.parse(str(args.build/f'recursive.{language}.xml')).getroot()
        check_output(etree.tostring(recursive[0][0]))
        textual = etree.parse(str(args.build/f'textual.{language}.xml')).getroot()
        assert textual.text == 'A<&' and textual.get('{urn:metadata}label') == 'p:Thing' and textual.nsmap['p'] == 'urn:parent'
        dense = etree.parse(str(args.build/f'dense.{language}.xml')).getroot()
        assert dense.tag == '{urn:core}dense' and dense.attrib == {f'{{urn:attrs}}a{index}': '0' for index in range(32)} and dense[0].tag == '{urn:vendor}x'
        plain = etree.parse(str(args.build/f'plain.{language}.xml')).getroot()
        assert plain.tag == 'plain' and plain[0].tag == 'value' and plain[0].text == '1' and plain[1].tag == '{urn:vendor}x'
        dense_long = etree.parse(str(args.build/f'dense-long.{language}.xml')).getroot()
        assert dense_long.tag == '{urn:core}dense_long' and dense_long.attrib == {f'{{{LONG_ATTRIBUTE_NAMESPACE}}}a{index}': '0' for index in range(32)} and dense_long[0].tag == '{urn:vendor}x'
    print(f'PASS: {len(paths)} independent XSD/position/admission probes, owned companions, recursive owners, exact policies and native controls in {len(languages)} actual SDKs', flush=True)


if __name__ == '__main__':
    main()
