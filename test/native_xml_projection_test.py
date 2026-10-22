# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify direct C/Rust XML export over native live references and readonly translator getters."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
from lxml import etree
import native_xml_map_test as native
ROOT,xml_runner,sdk=native.ROOT,native.xml_runner,native.sdk
MODEL='serializer version 1.20.0;\nnamespace projected {\n  xml(namespace("urn:serializer:projected"));\n  class payload { public uint32 number; public string label; }\n  class source { public payload current;\n    public translator payload as project { number=current.number; label=current.label; };\n  }\n  class envelope { public shared(source.current) value; public borrowed(source.current.label) label xml(attribute); }\n  class weak_envelope { public weak(source.current) value; }\n}\n'
MODEL=MODEL.replace('  class source {', '  class inherited : public payload { public uint32 extra; }\n  class source {').replace('    public translator payload as project { number=current.number; label=current.label; };', '    public translator payload as project { number=current.number; label=current.label; };\n    public translator inherited as derived { extra=current.number; };')
MODEL=MODEL.rsplit('}',1)[0]+'''class collection { public array<uint16> items; public digest[2] value; }
class collection_source { public collection stored; public translator collection as project { items=stored.items; value=stored.value; }; }
class names { public string value xml(text,qname); public string label xml(attribute,qname); }
class names_source { public names stored; public translator names as project { value=stored.value; label=stored.label; }; }
class names_envelope { public shared(names_source.stored) value; }
class names_weak { public weak(names_source.stored) value; }
class names_borrowed { public borrowed(names_source.stored) value; }
}'''
CONFIG='''[xml.class.projected::names]
extension.foreign.kind=attribute
extension.foreign.namespaces=urn:foreign
extension.foreign.policy=preserve
extension.foreign.maximum=1
'''
NS='urn:serializer:projected'
XSD='''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:projected" targetNamespace="urn:serializer:projected" elementFormDefault="qualified">
<xs:complexType name="payload"><xs:sequence><xs:element name="number" type="xs:unsignedInt"/><xs:element name="label" type="xs:string"/></xs:sequence></xs:complexType>
<xs:element name="payload" type="t:payload"/><xs:element name="envelope"><xs:complexType><xs:sequence><xs:element name="value" type="t:payload"/></xs:sequence><xs:attribute name="label" type="xs:string" use="required"/></xs:complexType></xs:element></xs:schema>'''
XSD=XSD.replace('</xs:schema>','''<xs:element name="collection"><xs:complexType><xs:sequence>
<xs:element name="items" type="xs:unsignedShort" minOccurs="0" maxOccurs="unbounded"/>
<xs:element name="value"><xs:simpleType><xs:restriction base="xs:hexBinary"><xs:length value="2"/></xs:restriction></xs:simpleType></xs:element>
</xs:sequence></xs:complexType></xs:element></xs:schema>''')
RUST=r'''mod schema;
use schema::*;
use std::{cell::RefCell,rc::Rc,path::Path};
/// Borrow source storage through direct XML and each unchanged native wire traversal.
fn main()->Result<(),Box<dyn std::error::Error>> {
 let directory=std::env::args().nth(1).unwrap();let directory=Path::new(&directory);
 let owner=Rc::new(RefCell::new(ProjectedSource::default()));owner.borrow_mut().current.number=7;owner.borrow_mut().current.label="first".to_owned();
 let mut borrowed=ProjectedSource::default();borrowed.current.label="borrowed".to_owned();
 let mut envelope=ProjectedEnvelope::default();envelope.value.bind(&owner)?;envelope.label.bind(&borrowed)?;
 let root=envelope.value.encode_xml()?;assert_eq!(root,owner.borrow().current.encode_xml()?);std::fs::write(directory.join("shared.rust.xml"),&root)?;
 for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] { assert_eq!(envelope.value.encode(protocol)?,owner.borrow().current.encode(protocol)?); }
 assert!(ProjectedEnvelopeValueValueReference::decode_xml(&root).is_err());
 owner.borrow_mut().current.label="second".to_owned();
 let output=envelope.encode_xml()?;assert!(ProjectedEnvelope::decode_xml(&output).is_err());std::fs::write(directory.join("envelope.rust.xml"),output)?;
 let source=owner.borrow();source.with_derived(|view|->Result<(),Error>{let output=view.encode_xml()?;let value=ProjectedInherited::decode_xml(&output)?;assert_eq!(value.extra,7);Ok(())})?;source.with_project(|view|->Result<(),Error> {
   assert!(std::ptr::eq(view.get_label(),&source.current.label));let output=view.encode_xml()?;assert_eq!(output,source.current.encode_xml()?);
   for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {assert_eq!(view.encode(protocol)?,source.current.encode(protocol)?);}
   std::fs::write(directory.join("translated.rust.xml"),output).map_err(|_|Error("Write projection proof"))?;Ok(())
 })?;drop(source);
 let guard=owner.borrow_mut();assert!(envelope.value.encode_xml().is_err());drop(guard);
 assert!(envelope.encode_xml_with_validation(XmlLimits{max_nodes:1,..XmlLimits::default()},None).is_err());
 let mut collection=ProjectedCollectionSource::default();collection.stored.items=vec![1,65535];collection.stored.value=vec![0,255];
 for count in [2usize,25000]{
  if count!=2{collection.stored.items=(0..count).map(|index|if index%2==0{0}else{u16::MAX}).collect();}
  collection.with_project(|view|->Result<(),Error>{
   assert!(std::ptr::eq(view.get_items(),&collection.stored.items));assert!(std::ptr::eq(view.get_value(),&collection.stored.value));
   let output=view.encode_xml()?;let value=ProjectedCollection::decode_xml(&output)?;assert_eq!(value,collection.stored);
   assert_eq!(view.encode_xml_with_validation(XmlLimits{max_bytes:output.len(),..XmlLimits::default()},None)?,output);
   assert_eq!(view.encode_xml_with_validation(XmlLimits{max_depth:2,..XmlLimits::default()},None)?,output);
   assert!(view.encode_xml_with_validation(XmlLimits{max_depth:1,..XmlLimits::default()},None).is_err());
   assert!(view.encode_xml_with_validation(XmlLimits{max_bytes:output.len()-1,..XmlLimits::default()},None).is_err());assert_eq!(view.encode_xml()?,output);
   for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{assert_eq!(view.encode(protocol)?,collection.stored.encode(protocol)?);}
   std::fs::write(directory.join("collection.rust.xml"),output).map_err(|_|Error("Write collection proof"))?;Ok(())
  })?;
 }
 let unbound=ProjectedEnvelope::default();assert!(unbound.encode_xml().is_err());
 let document=ProjectedNames::decode_xml_document("<names xmlns='urn:serializer:projected' xmlns:p='urn:borrowed' xmlns:f='urn:foreign' f:note='opaque' label='p:attribute'>p:payload</names>",XmlLimits::default(),None,&XmlExtensionOptions::default())?;
 let names=ProjectedNamesSource{stored:document.value};
 names.with_project(|view|->Result<(),Error>{
  assert!(std::ptr::eq(view.get_value(),&names.stored.value));assert!(std::ptr::eq(view.get_label(),&names.stored.label));
  let output=view.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&XmlExtensionOptions::default())?;
  assert_eq!(output,names.stored.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&XmlExtensionOptions::default())?);
  assert_eq!(output,view.encode_xml_with_extensions(&document.extensions,XmlLimits{max_bytes:output.len(),..XmlLimits::default()},None,&XmlExtensionOptions::default())?);
  assert!(view.encode_xml_with_extensions(&document.extensions,XmlLimits{max_bytes:output.len()-1,..XmlLimits::default()},None,&XmlExtensionOptions::default()).is_err());
  assert_eq!(output,view.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&XmlExtensionOptions::default())?);
  for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{assert_eq!(view.encode(protocol)?,names.stored.encode(protocol)?);}
  std::fs::write(directory.join("names.rust.xml"),output).map_err(|_|Error("Write QName projection proof"))?;Ok(())
 })?;
 let mut borrowed_names=ProjectedNamesBorrowed::default();borrowed_names.value.bind(&names)?;
 let selected=borrowed_names.value.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&XmlExtensionOptions::default())?;
 assert!(ProjectedNamesBorrowedValueValueReference::decode_xml(&selected).is_err());
 drop(borrowed_names);
 let names_owner=Rc::new(RefCell::new(names));let mut selected_names=ProjectedNamesEnvelope::default();let mut weak_names=ProjectedNamesWeak::default();
 selected_names.value.bind(&names_owner)?;weak_names.value.bind(&names_owner)?;
 selected_names.value.with_value(|selected|{assert!(std::ptr::eq(selected,&names_owner.borrow().stored));})?;
 assert_eq!(selected,selected_names.value.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&XmlExtensionOptions::default())?);
 let envelope_xml="<names_envelope xmlns='urn:serializer:projected'><value xmlns:p='urn:borrowed' xmlns:f='urn:foreign' f:note='opaque' label='p:attribute'>p:payload</value></names_envelope>";
 let presentation=ProjectedNamesEnvelope::prepare_xml_extensions(envelope_xml,XmlLimits::default(),None,&XmlExtensionOptions::default())?;
 let envelope_output=selected_names.encode_xml_with_extensions(&presentation,XmlLimits::default(),None,&XmlExtensionOptions::default())?;
 std::fs::write(directory.join("names-reference.rust.xml"),&envelope_output)?;
 assert!(selected_names.encode_xml_with_extensions(&presentation,XmlLimits{max_bytes:envelope_output.len()-1,..XmlLimits::default()},None,&XmlExtensionOptions::default()).is_err());
 assert_eq!(envelope_output,selected_names.encode_xml_with_extensions(&presentation,XmlLimits{max_bytes:envelope_output.len(),..XmlLimits::default()},None,&XmlExtensionOptions::default())?);
 let guard=names_owner.borrow_mut();assert!(selected_names.value.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&XmlExtensionOptions::default()).is_err());drop(guard);
 drop(names_owner);assert!(weak_names.value.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&XmlExtensionOptions::default()).is_ok());
 selected_names.value.unbind();assert!(weak_names.value.encode_xml_with_extensions(&document.extensions,XmlLimits::default(),None,&XmlExtensionOptions::default()).is_err());
 let transient=Rc::new(RefCell::new(ProjectedSource::default()));let mut weak=ProjectedWeakEnvelope::default();weak.value.bind(&transient)?;assert!(weak.encode_xml().is_ok());drop(transient);assert!(weak.encode_xml().is_err());
 drop(owner);assert!(envelope.value.encode_xml().is_ok());envelope.value.unbind();assert!(envelope.encode_xml().is_err());
 println!("Rust native references and scoped readonly XML borrow live storage");Ok(())
}
'''
C=r'''#include "schema.h"
#include <stdio.h>
/* Fail immediately with a concrete violated native/XML invariant. */
static void require(bool good,const char* message){if(!good){fprintf(stderr,"%s\n",message);exit(1);}}
/* Retain proof output for the independent XSD and value consumer. */
static void write_output(const char* directory,const char* name,const srl_buffer* bytes){char path[4096];require(snprintf(path,sizeof(path),"%s/%s.c.xml",directory,name)>0,"Output path");FILE* file=fopen(path,"wb");require(file!=NULL,"Open output");require(fwrite(bytes->data,1,bytes->size,file)==bytes->size,"Write output");fclose(file);}
static unsigned released=0;
static unsigned names_released=0;
/* Release a selected QName payload only after the last strong native handle drops its lease. */
static void release_names_owner(void* pointer){projected_names_source* source=(projected_names_source*)pointer;projected_names_source_free(source);free(source);++names_released;}
/* Model the application's final native owner release, independently from XML output. */
static void release_owner(void* pointer){projected_source* source=(projected_source*)pointer;projected_source_free(source);free(source);++released;}
typedef struct proof_context {const projected_source* source;const char* directory;} proof_context;
/* Check getter pointer identity and export directly while the native source callback is active. */
static void translated(const projected_source_project_translator* view,void* pointer){const proof_context* context=(const proof_context*)pointer;
 require(projected_source_project_translator_get_label(view)==&context->source->current.label,"Translator copied selected storage");
 srl_buffer output={0},expected={0};require(projected_source_project_translator_encode_xml(view,&output,NULL)==srl_ok && projected_payload_encode_xml(&context->source->current,&expected,NULL)==srl_ok,"Readonly XML export");
 require(output.size==expected.size && memcmp(output.data,expected.data,output.size)==0,"Readonly target XML identity");write_output(context->directory,"translated",&output);srl_buffer_free(&output);srl_buffer_free(&expected);
 const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){
  require(projected_source_project_translator_encode(view,protocols[index],&output)==srl_ok && projected_payload_encode(&context->source->current,protocols[index],&expected)==srl_ok,"Readonly native codecs");require(output.size==expected.size && memcmp(output.data,expected.data,output.size)==0,"Readonly native wire identity");srl_buffer_free(&output);srl_buffer_free(&expected);}
}
/* Borrow inherited defaults and complete non-Copy container descriptors through their native getters. */
static void derived(const projected_source_derived_translator* view,void* context){(void)context;srl_buffer bytes={0};projected_inherited value={0};require(projected_inherited_init(&value)==srl_ok,"Inherited view init");require(projected_source_derived_translator_encode_xml(view,&bytes,NULL)==srl_ok && projected_inherited_decode_xml(&value,bytes.data,bytes.size,NULL)==srl_ok && value.extra==7,"Inherited getter XML");srl_buffer_free(&bytes);projected_inherited_free(&value);}
/* Confirm a readonly native collection/digest descriptor is borrowed, with exact XML and wire identities. */
typedef struct collection_context {const projected_collection_source* source;const char* directory;} collection_context;
/* Borrow the original descriptors through byte limits, recovery and all native formats. */
static void collection_translated(const projected_collection_source_project_translator* view,void* pointer){
 const collection_context* context=(const collection_context*)pointer;const projected_collection_source* source=context->source;
 require(projected_collection_source_project_translator_get_items(view)==&source->stored.items && projected_collection_source_project_translator_get_value(view)==&source->stored.value,"Non-Copy getter identity");
 srl_buffer bytes={0},expected={0};projected_collection value={0};require(projected_collection_init(&value)==srl_ok,"Collection view init");
 require(projected_collection_source_project_translator_encode_xml(view,&bytes,NULL)==srl_ok && projected_collection_decode_xml(&value,bytes.data,bytes.size,NULL)==srl_ok && value.items.size==source->stored.items.size && memcmp(value.items.data,source->stored.items.data,value.items.size*sizeof(*value.items.data))==0 && value.value.size==2 && value.value.data[1]==255,"Collection view XML");
 write_output(context->directory,"collection",&bytes);srl_limits limits=srl_default_limits();limits.max_bytes=bytes.size;
 require(projected_collection_source_project_translator_encode_xml(view,&expected,&limits)==srl_ok && expected.size==bytes.size && memcmp(expected.data,bytes.data,bytes.size)==0,"Exact large projection bytes");srl_buffer_free(&expected);--limits.max_bytes;
 srl_limits depth=srl_default_limits();depth.max_depth=2;require(projected_collection_source_project_translator_encode_xml(view,&expected,&depth)==srl_ok && expected.size==bytes.size,"Exact projection depth");srl_buffer_free(&expected);depth.max_depth=1;require(projected_collection_source_project_translator_encode_xml(view,&expected,&depth)!=srl_ok && !expected.data,"One-tight projection depth");
 require(projected_collection_source_project_translator_encode_xml(view,&expected,&limits)!=srl_ok && !expected.data,"One-tight large projection bytes");
 require(projected_collection_source_project_translator_encode_xml(view,&expected,NULL)==srl_ok && expected.size==bytes.size && memcmp(expected.data,bytes.data,bytes.size)==0,"Fresh large projection recovery");srl_buffer_free(&expected);srl_buffer_free(&bytes);
 const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){require(projected_collection_source_project_translator_encode(view,protocols[index],&bytes)==srl_ok && projected_collection_encode(&source->stored,protocols[index],&expected)==srl_ok && bytes.size==expected.size && memcmp(bytes.data,expected.data,bytes.size)==0,"Collection view native wire");srl_buffer_free(&bytes);srl_buffer_free(&expected);}projected_collection_free(&value);
}
typedef struct names_context {const projected_names_source* source;const srl_xml_extension_state* extensions;const char* directory;} names_context;
/* Borrow both source payload and namespace companion through exact limits and fresh recovery. */
static void names_translated(const projected_names_source_project_translator* view,void* pointer){
 const names_context* context=(const names_context*)pointer;
 require(projected_names_source_project_translator_get_value(view)==&context->source->stored.value && projected_names_source_project_translator_get_label(view)==&context->source->stored.label,"QName projection copied payload");
 srl_buffer output={0},exact={0};require(projected_names_source_project_translator_encode_xml_document(view,context->extensions,&output,NULL,NULL)==srl_ok,"QName projection companion export");
 srl_limits bounds=srl_default_limits();bounds.max_bytes=output.size;
 require(projected_names_source_project_translator_encode_xml_document(view,context->extensions,&exact,&bounds,NULL)==srl_ok && exact.size==output.size && memcmp(exact.data,output.data,output.size)==0,"QName projection exact bytes");
 --bounds.max_bytes;require(projected_names_source_project_translator_encode_xml_document(view,context->extensions,&exact,&bounds,NULL)!=srl_ok && exact.size==output.size && memcmp(exact.data,output.data,output.size)==0,"QName projection failed publication");
 srl_buffer_free(&exact);require(projected_names_source_project_translator_encode_xml_document(view,context->extensions,&exact,NULL,NULL)==srl_ok && exact.size==output.size && memcmp(exact.data,output.data,output.size)==0,"QName projection fresh recovery");
 const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
 for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){srl_buffer native={0},owning={0};require(projected_names_source_project_translator_encode(view,protocols[index],&native)==srl_ok && projected_names_encode(&context->source->stored,protocols[index],&owning)==srl_ok && native.size==owning.size && memcmp(native.data,owning.data,native.size)==0,"QName projection native wire identity");srl_buffer_free(&native);srl_buffer_free(&owning);}
 write_output(context->directory,"names",&output);srl_buffer_free(&output);srl_buffer_free(&exact);
}
/* Preserve initialized bindings on rejected input and hold owners only during direct traversals. */
int main(int argc,char** argv){require(argc==2,"Output directory");
 const char* names_xml="<names xmlns='urn:serializer:projected' xmlns:p='urn:borrowed' xmlns:f='urn:foreign' f:note='opaque' label='p:attribute'>p:payload</names>";
 projected_names_xml_document names_document={0};require(projected_names_xml_document_init(&names_document)==srl_ok && projected_names_decode_xml_document(&names_document,(const uint8_t*)names_xml,strlen(names_xml),NULL,NULL)==srl_ok,"QName source companion");
 projected_names_source names={0};require(projected_names_source_init(&names)==srl_ok,"QName source initialization");projected_names_free(&names.stored);names.stored=names_document.value;memset(&names_document.value,0,sizeof(names_document.value));
 const names_context names_proof={&names,names_document.extensions,argv[1]};require(projected_names_source_with_project(&names,names_translated,(void*)&names_proof)==srl_ok,"QName readonly scope");
 projected_names_source* names_owner=(projected_names_source*)malloc(sizeof(*names_owner));require(names_owner!=NULL,"QName source allocation");*names_owner=names;memset(&names,0,sizeof(names));
 projected_names_envelope selected_names={0};projected_names_weak weak_names={0};projected_names_borrowed borrowed_names={0};require(projected_names_envelope_init(&selected_names)==srl_ok && projected_names_weak_init(&weak_names)==srl_ok && projected_names_borrowed_init(&borrowed_names)==srl_ok,"QName handle init");
 srl_reference_scope* names_scope=projected_names_envelope_value_value_reference_create_scope(names_owner,release_names_owner);require(names_scope!=NULL,"QName reference scope");require(projected_names_envelope_value_value_reference_bind(&selected_names.value,names_scope)==srl_ok && projected_names_weak_value_value_reference_bind(&weak_names.value,names_scope)==srl_ok && projected_names_borrowed_value_value_reference_bind(&borrowed_names.value,names_owner)==srl_ok,"QName native bindings");srl_reference_scope_release(names_scope);
 const projected_names* selected_payload=NULL;require(projected_names_envelope_value_value_reference_read_value(&selected_names.value,&selected_payload)==srl_ok && selected_payload==&names_owner->stored,"QName handle copied payload");
 srl_buffer names_bytes={0};require(projected_names_borrowed_value_value_reference_encode_xml_document(&borrowed_names.value,names_document.extensions,&names_bytes,NULL,NULL)==srl_ok,"Borrowed QName companion export");srl_buffer_free(&names_bytes);
 require(projected_names_envelope_value_value_reference_encode_xml_document(&selected_names.value,names_document.extensions,&names_bytes,NULL,NULL)==srl_ok,"Shared QName companion export");const projected_names_envelope_value_value_reference handle_before=selected_names.value;require(projected_names_envelope_value_value_reference_decode_xml(&selected_names.value,names_bytes.data,names_bytes.size,NULL)!=srl_ok && !memcmp(&handle_before,&selected_names.value,sizeof(handle_before)),"QName reference input changed binding");srl_buffer_free(&names_bytes);
 const char* envelope_xml="<names_envelope xmlns='urn:serializer:projected'><value xmlns:p='urn:borrowed' xmlns:f='urn:foreign' f:note='opaque' label='p:attribute'>p:payload</value></names_envelope>";srl_xml_extension_state* presentation=NULL;
 require(projected_names_envelope_prepare_xml_extensions(&presentation,(const uint8_t*)envelope_xml,strlen(envelope_xml),NULL,NULL)==srl_ok,"QName enclosing presentation");
 require(projected_names_envelope_encode_xml_document(&selected_names,presentation,&names_bytes,NULL,NULL)==srl_ok,"Enclosing QName handle export");write_output(argv[1],"names-reference",&names_bytes);
 srl_limits names_limits=srl_default_limits();names_limits.max_bytes=names_bytes.size-1;require(projected_names_envelope_encode_xml_document(&selected_names,presentation,&names_bytes,&names_limits,NULL)!=srl_ok,"QName handle byte limit");++names_limits.max_bytes;require(projected_names_envelope_encode_xml_document(&selected_names,presentation,&names_bytes,&names_limits,NULL)==srl_ok,"QName handle recovery");srl_buffer_free(&names_bytes);srl_xml_extension_state_free(presentation);
 require(names_released==0,"Shared QName owner released early");projected_names_borrowed_free(&borrowed_names);projected_names_envelope_free(&selected_names);require(names_released==1,"QName owner lease leaked");require(projected_names_weak_value_value_reference_encode_xml_document(&weak_names.value,names_document.extensions,&names_bytes,NULL,NULL)!=srl_ok && !names_bytes.data,"Expired QName weak handle exported");projected_names_weak_free(&weak_names);projected_names_xml_document_free(&names_document);projected_names_source_free(&names);
 projected_source* owner=(projected_source*)calloc(1,sizeof(*owner));require(owner && projected_source_init(owner)==srl_ok,"Owner init");owner->current.number=7;require(srl_string_set(&owner->current.label,"first",5)==srl_ok,"Owner field");
 projected_source borrowed={0};require(projected_source_init(&borrowed)==srl_ok && srl_string_set(&borrowed.current.label,"borrowed",8)==srl_ok,"Borrowed source");
 projected_envelope envelope={0};projected_weak_envelope weak={0};require(projected_envelope_init(&envelope)==srl_ok && projected_weak_envelope_init(&weak)==srl_ok,"Reference init");
 srl_reference_scope* scope=projected_envelope_value_value_reference_create_scope(owner,release_owner);require(scope!=NULL,"Scope allocation");
 require(projected_envelope_value_value_reference_bind(&envelope.value,scope)==srl_ok && projected_envelope_label_value_reference_bind(&envelope.label,&borrowed)==srl_ok && projected_weak_envelope_value_value_reference_bind(&weak.value,scope)==srl_ok,"Reference attachment");srl_reference_scope_release(scope);require(released==0,"Shared owner dropped");
 const projected_payload* selected=NULL;require(projected_envelope_value_value_reference_read_value(&envelope.value,&selected)==srl_ok && selected==&owner->current,"Reference copied target");
 srl_buffer output={0},expected={0};require(projected_envelope_value_value_reference_encode_xml(&envelope.value,&output,NULL)==srl_ok && projected_payload_encode_xml(&owner->current,&expected,NULL)==srl_ok,"Root reference export");require(output.size==expected.size && memcmp(output.data,expected.data,output.size)==0,"Root reference XML identity");write_output(argv[1],"shared",&output);
 const projected_envelope_value_value_reference original=envelope.value;require(projected_envelope_value_value_reference_decode_xml(&envelope.value,output.data,output.size,NULL)==srl_invalid && memcmp(&original,&envelope.value,sizeof(original))==0,"Reference input changed binding");srl_buffer_free(&output);srl_buffer_free(&expected);
 const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){require(projected_envelope_value_value_reference_encode(&envelope.value,protocols[index],&output)==srl_ok && projected_payload_encode(&owner->current,protocols[index],&expected)==srl_ok,"Reference native codecs");require(output.size==expected.size && memcmp(output.data,expected.data,output.size)==0,"Reference native wire identity");srl_buffer_free(&output);srl_buffer_free(&expected);}
 require(srl_string_set(&owner->current.label,"second",6)==srl_ok,"Live source edit");require(projected_envelope_encode_xml(&envelope,&output,NULL)==srl_ok,"Nested reference export");write_output(argv[1],"envelope",&output);
 const projected_envelope before=envelope;require(projected_envelope_decode_xml(&envelope,output.data,output.size,NULL)==srl_invalid && memcmp(&before,&envelope,sizeof(before))==0,"Reference graph input changed bindings");srl_buffer_free(&output);
 require(projected_source_with_derived(owner,derived,NULL)==srl_ok,"Inherited native readonly view");
 const proof_context context={owner,argv[1]};require(projected_source_with_project(owner,translated,(void*)&context)==srl_ok,"Scoped translator");
 projected_collection_source collection={0};require(projected_collection_source_init(&collection)==srl_ok,"Collection source init");collection.stored.items.data=(uint16_t*)malloc(2u*sizeof(uint16_t));require(collection.stored.items.data!=NULL,"Collection memory");collection.stored.items.size=collection.stored.items.capacity=2;collection.stored.items.data[0]=1;collection.stored.items.data[1]=65535;collection.stored.value.data[0]=0;collection.stored.value.data[1]=255;
 const collection_context collection_proof={&collection,argv[1]};require(projected_collection_source_with_project(&collection,collection_translated,(void*)&collection_proof)==srl_ok,"Collection readonly scope");
 const size_t large_values=25000;uint16_t* expanded=(uint16_t*)realloc(collection.stored.items.data,large_values*sizeof(*expanded));require(expanded!=NULL,"Large collection memory");collection.stored.items.data=expanded;collection.stored.items.size=collection.stored.items.capacity=large_values;for(size_t index=0;index<large_values;++index){expanded[index]=index%2?UINT16_MAX:0;}
 require(projected_collection_source_with_project(&collection,collection_translated,(void*)&collection_proof)==srl_ok,"Large collection readonly scope");projected_collection_source_free(&collection);
 srl_limits bounds=srl_default_limits();bounds.max_elements=1;require(projected_envelope_encode_xml(&envelope,&output,&bounds)!=srl_ok && !output.data,"Reference output budgets");
 projected_envelope detached={0};require(projected_envelope_init(&detached)==srl_ok && projected_envelope_encode_xml(&detached,&output,NULL)!=srl_ok && !output.data,"Detached reference export");projected_envelope_free(&detached);
 require(projected_weak_envelope_encode_xml(&weak,&output,NULL)==srl_ok,"Live weak reference");srl_buffer_free(&output);projected_envelope_free(&envelope);require(released==1,"Strong lease leak");require(projected_weak_envelope_encode_xml(&weak,&output,NULL)!=srl_ok && !output.data,"Expired weak reference export");projected_weak_envelope_free(&weak);projected_source_free(&borrowed);require(released==1,"Duplicate owner release");puts("C native owner leases, direct reference XML and scoped getters pass");return 0;
}
'''


def main():
    """Generate the enabled/native controls and require strict actual SDK export consumers."""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--language',default='rust,c');parser.add_argument('--libxml2-prefix',type=Path);parser.add_argument('--wsl-languages',default='');parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args();args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if len(languages)!=len(set(languages)) or any(language not in ['rust','c'] for language in languages):parser.error('Choose distinct languages rust,c')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:parser.error('Scratch must remain under ignored out/')
    args.build.mkdir(parents=True,exist_ok=True)
    if args.libxml2_prefix:args.libxml2_prefix=args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME',str(args.build/'cargo-home'));os.environ['RUSTFLAGS']='-D warnings'
    (args.build/'model.serializer').write_text(MODEL,encoding='utf-8');command=[args.compiler,'--input',args.build/'model.serializer','--language',','.join(languages)]
    (args.build/'options.ini').write_text(CONFIG,encoding='utf-8')
    for language in languages:
        directory=args.build/language;directory.mkdir(parents=True,exist_ok=True);command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run([*command,'--config',args.build/'options.ini','--xml.enabled=true']);sdk.run([args.compiler,'--input',args.build/'model.serializer','--language',','.join(languages),'--xml.enabled=false',*[argument for language in languages for argument in [f'--{language}.output',args.build/('native.h' if language=='c' else 'native.rs')]]])
    runner=sdk.Runner(args);oracle=etree.XMLSchema(etree.fromstring(XSD.encode()))
    for language in languages:
        source=args.build/'source'/language;source.mkdir(parents=True,exist_ok=True);(source/('main.rs' if language=='rust' else 'main.c')).write_text(RUST if language=='rust' else C,encoding='utf-8')
        if language=='rust':
            for name in ['Cargo.toml','Cargo.lock']:shutil.copyfile(ROOT/'example/xml/rust'/name,source/name)
        launch=xml_runner.build_language(args,runner,language,args.build/language,source_root=source.parent);sdk.run([*launch,runner.path(language,args.build)])
        for kind in ['shared','envelope','translated']:
            node=etree.parse(str(args.build/(kind+'.'+language+'.xml'))).getroot();oracle.assertValid(node)
            payload=node[0] if kind=='envelope' else node
            assert payload.find('{'+NS+'}number').text=='7';assert payload.find('{'+NS+'}label').text==('first' if kind=='shared' else 'second')
            if kind=='envelope':assert node.get('label')=='borrowed'
        collection=etree.parse(str(args.build/f'collection.{language}.xml'));oracle.assertValid(collection)
        assert [int(child.text) for child in collection.getroot().findall('{'+NS+'}items')]==[0,65535]*12500
        assert bytes.fromhex(collection.getroot().find('{'+NS+'}value').text)==bytes((0,255))
        names=etree.parse(str(args.build/f'names.{language}.xml')).getroot()
        for text,local in [(names.text,'payload'),(names.get('label'),'attribute')]:
            prefix,value=text.split(':');assert names.nsmap[prefix]=='urn:borrowed' and value==local
        assert names.get('{urn:foreign}note')=='opaque'
        selected=etree.parse(str(args.build/f'names-reference.{language}.xml')).getroot()[0]
        for text,local in [(selected.text,'payload'),(selected.get('label'),'attribute')]:
            prefix,value=text.split(':');assert selected.nsmap[prefix]=='urn:borrowed' and value==local
        assert selected.get('{urn:foreign}note')=='opaque'
    private=args.build/'private.serializer';private.write_text(MODEL.replace('inherited : public payload','inherited : private payload'),encoding='utf-8')
    for enabled in ['true','false']:
        target=args.build/('private-'+enabled+'.sentinel');target.write_bytes(b'existing output')
        result=subprocess.run([str(args.compiler),'--input',str(private),'--language',languages[0],'--'+languages[0]+'.output',str(target),'--xml.enabled='+enabled],capture_output=True,text=True,encoding='utf-8')
        assert result.returncode!=0 and 'Borrowed values require public generated bases' in result.stderr+result.stdout
        assert target.read_bytes()==b'existing output'
    print(f'PASS: native live/scoped XML references and readonly getters across {len(languages)} SDKs, independent XSD and four native formats',flush=True)


if __name__=='__main__':main()
