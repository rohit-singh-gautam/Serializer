# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify C/Rust QName document companions against independent expanded namespace identities."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_namespace_sdk', ROOT/'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)
sdk = xml_runner.sdk
NS = 'urn:serializer:names'
XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:names"
targetNamespace="urn:serializer:names" elementFormDefault="qualified">
<xs:simpleType name="Names"><xs:list itemType="xs:QName"/></xs:simpleType>
<xs:complexType name="entry"><xs:simpleContent><xs:extension base="xs:QName">
<xs:attribute name="name" type="xs:QName" use="required"/></xs:extension></xs:simpleContent></xs:complexType>
<xs:element name="record"><xs:complexType><xs:sequence>
<xs:element name="value" type="xs:QName" minOccurs="0" maxOccurs="unbounded"/>
<xs:element name="lexical" type="t:Names"/><xs:element name="child_names" type="t:Names"/>
<xs:element name="optional_names" type="t:Names" minOccurs="0"/>
<xs:element name="prefixes" type="xs:string"/><xs:element name="child" type="t:entry"/>
</xs:sequence><xs:attribute name="name" type="xs:QName" use="required"/>
<xs:attribute name="names" type="t:Names" use="required"/>
<xs:attribute name="required" type="xs:string" use="required"/></xs:complexType></xs:element></xs:schema>'''
MODEL = '''serializer version 1.20.0; namespace names {
xml(namespace("urn:serializer:names")); using schema "names.xsd";
class entry { public string value xml(text,qname); public string name xml(attribute,qname); }
class record {
 public string name xml(attribute,qname); public array[2] string names xml(attribute,list);
 public string required xml(attribute,prefix_list); public array string values xml(name("value"));
 public string lexical; public array[2] string child_names xml(list);
 public optional<array<string>> optional_names xml(list); public string prefixes xml(prefix_list);
 public entry child;
} }
'''
DOCUMENT = ('<n:record xmlns:n="'+NS+'" xmlns:p="urn:outer" xmlns:别名="urn:outer" '
            'name="p:名称" names="p:one 别名:two" required="p 别名 xml">'
            '<n:value xmlns:p="urn:first">p:名称</n:value>'
            '<n:value xmlns:p="urn:second">p:名称</n:value>'
            '<n:value xmlns="urn:default-value">名称</n:value><n:value xmlns="">local</n:value>'
            '<n:lexical xmlns:p="urn:list">p:one xml:lang</n:lexical>'
            '<n:child_names xmlns:p="urn:child-list">p:one p:two</n:child_names>'
            '<n:optional_names xmlns:p="urn:optional-list">p:one p:two</n:optional_names>'
            '<n:prefixes xmlns:p="urn:prefix-child">p xml</n:prefixes>'
            '<n:child xmlns:p="urn:nested" name="p:attribute">p:text</n:child></n:record>')
RUST = r'''mod schema;
use schema::*;
use std::{env,fs};
/// Exercise independently owned scopes, stable native bytes and exact companion resource admission.
fn main()->Result<(),Box<dyn std::error::Error>> {
 for path in env::args().skip(1) {
  let source=fs::read_to_string(&path)?;let valid=path.ends_with(".valid.xml");let limits=XmlLimits::default();let options=XmlExtensionOptions::default();
  let loaded=NamesRecord::decode_xml_document(&source,limits,None,&options);
  let mut document=match loaded {Ok(value)=>{assert!(valid,"Invalid QName admitted: {path}");value},Err(error)=>{if valid{return Err(Box::new(error));}continue;}};drop(source);
  let before=document.value.encode(Protocol::BinaryNone)?;
  let output=document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options)?;
  assert_eq!(before,document.value.encode(Protocol::BinaryNone)?);
  let copy=NamesRecord::decode_xml_document(&output,limits,None,&options)?;
  assert_eq!(before,copy.value.encode(Protocol::BinaryNone)?);
  assert_eq!(output,copy.value.encode_xml_with_extensions(&copy.extensions,limits,None,&options)?);
  let exact=document.extensions.preserved_bytes();assert!(exact>0);
  let mut bounded=options.clone();bounded.limits.max_preserved_bytes=exact;
  document.value.encode_xml_with_extensions(&document.extensions,limits,None,&bounded)?;
  bounded.limits.max_preserved_bytes=exact-1;
  assert!(document.value.encode_xml_with_extensions(&document.extensions,limits,None,&bounded).is_err());
  assert!(NamesRecord::decode_xml_document(&output,limits,None,&bounded).is_err());
  document.value.name="missing:item".to_owned();assert!(document.value.encode_xml_with_extensions(&document.extensions,limits,None,&options).is_err());
  fs::write(format!("{path}.rust.xml"),output)?;
 }
 println!("Rust QName companions retain local/default/shadowed scopes and bounded native identity");Ok(())
}'''
RUST_ACCOUNTING = r'''
/// Independently count every retained scope entry, including default resets and ownership paths.
fn main()->Result<(),Box<dyn std::error::Error>> {
 let text=std::fs::read_to_string(std::env::args().nth(1).unwrap())?;
 let document=NamesRecord::decode_xml_document(&text,XmlLimits::default(),None,&XmlExtensionOptions::default())?;
 let mut expected=0usize;
 for owner in &document.extensions.owners {
  expected+=64+owner.path.len()*2*std::mem::size_of::<usize>();
  assert!(owner.attributes.is_empty()&&owner.children.is_empty());
  assert!(!owner.scope.iter().any(|(prefix,_)|prefix=="xml"));
  for (prefix,uri) in &owner.scope {expected+=64+prefix.len()+uri.len();}
 }
 assert_eq!(document.extensions.preserved_bytes(),expected);
 println!("Rust retained namespace entries match independently counted admission");Ok(())
}'''
C = r'''#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Keep fixture failures distinct from the malformed XML decisions under test. */
static void require(bool value,const char* message) {if(!value){fprintf(stderr,"%s\n",message);exit(EXIT_FAILURE);}}
/* Own the input independently, then release it before namespace replay. */
static uint8_t* read_file(const char* path,size_t* size) {FILE* file=fopen(path,"rb");require(file!=NULL,"Input open");require(fseek(file,0,SEEK_END)==0,"Input seek");long count=ftell(file);require(count>=0,"Input size");rewind(file);*size=(size_t)count;uint8_t* bytes=(uint8_t*)malloc(*size?*size:1u);require(bytes!=NULL,"Input allocation");require(fread(bytes,1,*size,file)==*size,"Input read");fclose(file);return bytes;}
/* Verify exact output before giving it to the independent namespace oracle. */
int main(int argc,char** argv) {
 const srl_limits limits=srl_default_limits();
 for(int index=1;index<argc;++index) {
  const char* path=argv[index];const size_t length=strlen(path);const bool valid=length>=10u&&!strcmp(path+length-10u,".valid.xml");size_t size=0;uint8_t* bytes=read_file(path,&size);
  srl_xml_extension_options options=srl_xml_default_extension_options();names_record_xml_document document={0},copy={0};require(names_record_xml_document_init(&document)==srl_ok&&names_record_xml_document_init(&copy)==srl_ok,"Document init");
  const names_record previous=document.value;const srl_xml_extension_state* previous_state=document.extensions;
  const srl_status status=names_record_decode_xml_document(&document,bytes,size,&limits,&options);free(bytes);
  if(status!=srl_ok){require(!valid,path);require(!memcmp(&previous,&document.value,sizeof(previous))&&document.extensions==previous_state,"Failed decode published state");names_record_xml_document_free(&document);names_record_xml_document_free(&copy);continue;}
  require(valid,"Invalid QName admitted");srl_buffer native={0},after={0},output={0},again={0};require(names_record_encode(&document.value,srl_binary_none,&native)==srl_ok,"Native encode");
  require(names_record_encode_xml_document(&document,&output,&limits,&options)==srl_ok,"QName encode");
  require(names_record_decode_xml_document(&copy,output.data,output.size,&limits,&options)==srl_ok,"QName reload");require(names_record_encode(&copy.value,srl_binary_none,&after)==srl_ok&&native.size==after.size&&!memcmp(native.data,after.data,native.size),"Native bytes changed");
  require(names_record_encode_xml_document(&copy,&again,&limits,&options)==srl_ok&&output.size==again.size&&!memcmp(output.data,again.data,output.size),"Namespace replay unstable");
  const size_t exact=srl_xml_extension_preserved_bytes(document.extensions);require(exact>0,"Missing namespace admission");size_t expected=0;for(size_t owner_index=0;owner_index<document.extensions->owner_count;++owner_index){const srl_ext_owner* owner=&document.extensions->owners[owner_index];require(!owner->attributes.size&&!owner->child_count,"Unexpected opaque namespace data");expected+=64u+owner->path.size;for(size_t binding=0;binding<owner->scope.size;++binding){require(strcmp(owner->scope.data[binding].prefix,"xml")!=0,"Implicit xml storage retained");expected+=64u+strlen(owner->scope.data[binding].prefix)+strlen(owner->scope.data[binding].uri);}}require(expected==exact,"Namespace storage admission undercounted");options.limits.max_preserved_bytes=exact;require(names_record_encode_xml_document(&document,&again,&limits,&options)==srl_ok,"Exact namespace bound");options.limits.max_preserved_bytes=exact-1u;uint8_t* previous_output=again.data;require(names_record_encode_xml_document(&document,&again,&limits,&options)!=srl_ok&&again.data==previous_output,"Failed output replaced bytes");
  options=srl_xml_default_extension_options();require(srl_string_set(&document.value.name,"missing:item",12u)==srl_ok,"Mutation allocation");require(names_record_encode_xml_document(&document,&again,&limits,&options)!=srl_ok,"Unbound QName output admitted");
  char destination[4096];const int count=snprintf(destination,sizeof(destination),"%s.c.xml",path);require(count>=0&&(size_t)count<sizeof(destination),"Output path bound");FILE* file=fopen(destination,"wb");require(file!=NULL&&fwrite(output.data,1,output.size,file)==output.size,"Output write");fclose(file);
  srl_buffer_free(&native);srl_buffer_free(&after);srl_buffer_free(&output);srl_buffer_free(&again);names_record_xml_document_free(&document);names_record_xml_document_free(&copy);
 }
 puts("C QName companions retain local/default/shadowed scopes and transactional output");return 0;
}'''


def identities(root):
    """Resolve QName values with lxml's independent per-element in-scope namespace inventories."""
    result = []
    for node in root.iter():
        if not isinstance(node.tag, str):
            continue
        for key, value in node.attrib.items():
            result.append((node.tag, key, expanded(node, value, key == 'required')))
        if node.text and node.text.strip():
            result.append((node.tag, '#text', expanded(node, node.text, node.tag.endswith('}prefixes'))))
    return result


def expanded(node, text, prefixes):
    """Keep the implicit xml binding separate from the nullable default namespace key."""
    result = []
    for token in text.split():
        prefix, local = (token, '') if prefixes else token.split(':', 1) if ':' in token else ('', token)
        uri = 'http://www.w3.org/XML/1998/namespace' if prefix == 'xml' else node.nsmap.get(prefix or None, '')
        result.append((uri, local))
    return result


def verify_namespace_contract_bindings(args, languages):
    """Admit native scoped union/default/fixed bindings qualified by the dedicated SDK corpora."""
    fixture = args.build/'unsupported.serializer'
    fixture.write_text('serializer version 1.20.0; namespace unsupported { using schema "unsupported.xsd"; '
                       'class probe { public string value xml(text); } }', encoding='utf-8')
    head = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:q="urn:types">'
    for label, type_declaration, base, extra in [
        ('union', '<xs:simpleType name="Choice"><xs:union memberTypes="xs:QName xs:string"/></xs:simpleType>', 'Choice', ''),
        ('default', '', 'xs:QName', ' default="q:item"'),
        ('fixed', '', 'xs:QName', ' fixed="q:item"'),
        ]:
        (args.build/'unsupported.xsd').write_text(head+type_declaration+'<xs:element name="probe"'+extra+
            '><xs:complexType><xs:simpleContent><xs:extension base="'+base+'"/></xs:simpleContent></xs:complexType></xs:element></xs:schema>', encoding='utf-8')
        for language in languages:
            output = args.build/f'unsupported-{label}-{language}.{sdk.OUTPUTS[language].split(".")[-1]}'
            output.unlink(missing_ok=True)
            result = subprocess.run([str(args.compiler), '--input', str(fixture), '--language', language,
                                     f'--{language}.output', str(output)], capture_output=True, text=True)
            if result.returncode != 0 or not output.exists():
                raise AssertionError(f'Qualified QName {label} binding rejected for {language}: '
                                     f'{result.stdout}{result.stderr}')


def main():
    """Generate and compile real C/Rust consumers, then compare complete XML and value identities."""
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
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath ignored out/')
    languages = args.language.split(',')
    if not languages or len(set(languages)) != len(languages) or any(value not in ('c', 'rust') for value in languages):
        parser.error('Choose distinct languages from c,rust')
    args.build.mkdir(parents=True, exist_ok=True)
    verify_namespace_contract_bindings(args, languages)
    (args.build/'names.xsd').write_text(XSD, encoding='utf-8')
    (args.build/'names.serializer').write_text(MODEL, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    probes = [('base', DOCUMENT, True), ('empty-list', DOCUMENT.replace('p:one xml:lang', ''), True),
              ('empty-optional-list', DOCUMENT.replace('p:one p:two</n:optional_names>', '</n:optional_names>'), True),
              ('absent-optional-list', DOCUMENT.replace('<n:optional_names xmlns:p="urn:optional-list">p:one p:two</n:optional_names>', ''), True),
              ('empty-prefix-list', DOCUMENT.replace('required="p 别名 xml"', 'required=""').replace('>p xml</n:prefixes>', '></n:prefixes>'), True),
              ('split-text', DOCUMENT.replace('p:名称</n:value>', 'p:<![CDATA[名称]]><!-- ignored --></n:value>', 1), True),
              ('unbound', DOCUMENT.replace('name="p:名称"', 'name="missing:名称"'), False),
              ('bad-local', DOCUMENT.replace('p:名称</n:value>', 'p:1name</n:value>', 1), False),
              ('extra-colon', DOCUMENT.replace('p:名称</n:value>', 'p:other:name</n:value>', 1), False),
              ('empty-qname', DOCUMENT.replace('p:名称</n:value>', '</n:value>', 1), False),
              ('bad-prefix-list', DOCUMENT.replace('required="p 别名 xml"', 'required="p missing"'), False),
              ('bad-child-prefix-list', DOCUMENT.replace('>p xml</n:prefixes>', '>missing</n:prefixes>'), False),
              ('list-cardinality', DOCUMENT.replace('names="p:one 别名:two"', 'names="p:one"'), False)]
    paths = []
    for label, document, valid in probes:
        node = etree.fromstring(document.encode())
        if label not in ('bad-prefix-list', 'bad-child-prefix-list', 'list-cardinality') and oracle.validate(node) != valid:
            raise AssertionError(f'Independent XSD differs: {label}: {oracle.error_log}')
        path = args.build/f'{label}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8'); paths.append(path)
    command = [args.compiler, '--input', args.build/'names.serializer', '--language', ','.join(languages), '--protocols', 'binary_none']
    for language in languages:
        directory = args.build/language; directory.mkdir(exist_ok=True)
        command += [f'--{language}.output', directory/sdk.OUTPUTS[language]]
    sdk.run(command)
    os.environ['RUSTFLAGS'] = '-D warnings'
    runner = sdk.Runner(args)
    for language in languages:
        source = args.build/'source'/language; source.mkdir(parents=True, exist_ok=True)
        (source/('main.rs' if language == 'rust' else 'main.c')).write_text(RUST if language == 'rust' else C, encoding='utf-8')
        if language == 'rust':
            for filename in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(ROOT/'example/xml/rust'/filename, source/filename)
            (source/'accounting.rs').write_text((args.build/'rust/schema.rs').read_text(encoding='utf-8')+RUST_ACCOUNTING, encoding='utf-8')
            with (source/'Cargo.toml').open('a', encoding='utf-8') as manifest:
                manifest.write('\n[[bin]]\nname="serializer-namespace-accounting"\npath="accounting.rs"\n')
        launch = xml_runner.build_language(args, runner, language, args.build/language, source_root=source.parent)
        sdk.run([*launch, *[runner.path(language, path) for path in paths]])
        if language == 'rust':
            executable = args.build/'rust/target/release'/('serializer-namespace-accounting'+('.exe' if os.name == 'nt' and language not in runner.wsl else ''))
            sdk.run([*runner.launch(language, runner.path(language, executable)), runner.path(language, paths[0])])
        for (label, document, valid), path in zip(probes, paths):
            emitted = Path(str(path)+f'.{language}.xml')
            if not valid:
                if emitted.exists():
                    raise AssertionError(f'Invalid candidate published: {path}')
                continue
            node = etree.parse(str(emitted)).getroot()
            if not oracle.validate(node) or identities(etree.fromstring(document.encode())) != identities(node):
                raise AssertionError(f'Expanded QName identities changed: {language}/{label}: {oracle.error_log}')
    print(f'PASS: {len(probes)} namespace companion probes in {",".join(languages)}')


if __name__ == '__main__':
    main()
