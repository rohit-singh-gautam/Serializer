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
}'''
NS='urn:serializer:projected'
XSD='''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:projected" targetNamespace="urn:serializer:projected" elementFormDefault="qualified">
<xs:complexType name="payload"><xs:sequence><xs:element name="number" type="xs:unsignedInt"/><xs:element name="label" type="xs:string"/></xs:sequence></xs:complexType>
<xs:element name="payload" type="t:payload"/><xs:element name="envelope"><xs:complexType><xs:sequence><xs:element name="value" type="t:payload"/></xs:sequence><xs:attribute name="label" type="xs:string" use="required"/></xs:complexType></xs:element></xs:schema>'''
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
 collection.with_project(|view|->Result<(),Error>{assert!(std::ptr::eq(view.get_items(),&collection.stored.items));assert!(std::ptr::eq(view.get_value(),&collection.stored.value));let output=view.encode_xml()?;let value=ProjectedCollection::decode_xml(&output)?;assert_eq!(value,collection.stored);for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{assert_eq!(view.encode(protocol)?,collection.stored.encode(protocol)?);}Ok(())})?;
 let unbound=ProjectedEnvelope::default();assert!(unbound.encode_xml().is_err());
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
static void collection_translated(const projected_collection_source_project_translator* view,void* pointer){const projected_collection_source* source=(const projected_collection_source*)pointer;require(projected_collection_source_project_translator_get_items(view)==&source->stored.items && projected_collection_source_project_translator_get_value(view)==&source->stored.value,"Non-Copy getter identity");srl_buffer bytes={0},expected={0};projected_collection value={0};require(projected_collection_init(&value)==srl_ok,"Collection view init");require(projected_collection_source_project_translator_encode_xml(view,&bytes,NULL)==srl_ok && projected_collection_decode_xml(&value,bytes.data,bytes.size,NULL)==srl_ok && value.items.size==2 && value.items.data[1]==65535 && value.value.size==2 && value.value.data[1]==255,"Collection view XML");srl_buffer_free(&bytes);const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){require(projected_collection_source_project_translator_encode(view,protocols[index],&bytes)==srl_ok && projected_collection_encode(&source->stored,protocols[index],&expected)==srl_ok && bytes.size==expected.size && memcmp(bytes.data,expected.data,bytes.size)==0,"Collection view native wire");srl_buffer_free(&bytes);srl_buffer_free(&expected);}projected_collection_free(&value);}
/* Preserve initialized bindings on rejected input and hold owners only during direct traversals. */
int main(int argc,char** argv){require(argc==2,"Output directory");
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
 projected_collection_source collection={0};require(projected_collection_source_init(&collection)==srl_ok,"Collection source init");collection.stored.items.data=(uint16_t*)malloc(2u*sizeof(uint16_t));require(collection.stored.items.data!=NULL,"Collection memory");collection.stored.items.size=collection.stored.items.capacity=2;collection.stored.items.data[0]=1;collection.stored.items.data[1]=65535;collection.stored.value.data[0]=0;collection.stored.value.data[1]=255;require(projected_collection_source_with_project(&collection,collection_translated,&collection)==srl_ok,"Collection readonly scope");projected_collection_source_free(&collection);
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
    for language in languages:
        directory=args.build/language;directory.mkdir(parents=True,exist_ok=True);command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run([*command,'--xml.enabled=true']);sdk.run([args.compiler,'--input',args.build/'model.serializer','--language',','.join(languages),'--xml.enabled=false',*[argument for language in languages for argument in [f'--{language}.output',args.build/('native.h' if language=='c' else 'native.rs')]]])
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
    private=args.build/'private.serializer';private.write_text(MODEL.replace('inherited : public payload','inherited : private payload'),encoding='utf-8')
    for enabled in ['true','false']:
        target=args.build/('private-'+enabled+'.sentinel');target.write_bytes(b'existing output')
        result=subprocess.run([str(args.compiler),'--input',str(private),'--language',languages[0],'--'+languages[0]+'.output',str(target),'--xml.enabled='+enabled],capture_output=True,text=True,encoding='utf-8')
        assert result.returncode!=0 and 'Borrowed values require public generated bases' in result.stderr+result.stdout
        assert target.read_bytes()==b'existing output'
    print(f'PASS: native live/scoped XML references and readonly getters across {len(languages)} SDKs, independent XSD and four native formats',flush=True)


if __name__=='__main__':main()
