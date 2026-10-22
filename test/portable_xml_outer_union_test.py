# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify outer union restrictions using first-selected primitive value identities."""
import argparse
from pathlib import Path
import shutil
import tempfile

import native_xml_namespace_test as shared

ROOT, sdk, runner_module = shared.ROOT, shared.sdk, shared.xml_runner
USE_JAXP = False
CHECK_OUTPUT = lambda source, output: None
MODEL = '''serializer version 1.21.0; namespace outer { using schema "union.xsd";
class probe { public string boolean_value; public string numeric_value; public string list_value;
public string preserved; public string fixed_value; public string default_value; }
}'''
XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
<xs:simpleType name="BooleanFirst"><xs:union memberTypes="xs:boolean xs:unsignedShort"/></xs:simpleType>
<xs:simpleType name="NumberFirst"><xs:union memberTypes="xs:unsignedShort xs:boolean"/></xs:simpleType>
<xs:simpleType name="Boolean"><xs:restriction base="BooleanFirst"><xs:enumeration value="1"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Number"><xs:restriction base="NumberFirst"><xs:enumeration value="1"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Numbers"><xs:list itemType="xs:unsignedShort"/></xs:simpleType>
<xs:simpleType name="ListChoice"><xs:union memberTypes="Numbers xs:token"/></xs:simpleType>
<xs:simpleType name="List"><xs:restriction base="ListChoice"><xs:enumeration value="1 2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="PreserveChoice"><xs:union><xs:simpleType><xs:restriction base="xs:string"><xs:pattern value=" raw "/></xs:restriction></xs:simpleType><xs:simpleType><xs:restriction base="xs:token"/></xs:simpleType></xs:union></xs:simpleType>
<xs:simpleType name="Preserve"><xs:restriction base="PreserveChoice"><xs:enumeration value=" raw "/></xs:restriction></xs:simpleType>
<xs:element name="probe"><xs:complexType><xs:sequence>
<xs:element name="boolean_value" type="Boolean"/><xs:element name="numeric_value" type="Number"/>
<xs:element name="list_value" type="List"/><xs:element name="preserved" type="Preserve"/>
<xs:element name="fixed_value" type="BooleanFirst" fixed="1"/>
<xs:element name="default_value" type="NumberFirst" default="1"/>
</xs:sequence></xs:complexType></xs:element></xs:schema>'''


def cases():
    """Specify isolated type-family, lexical-alias and selected-whitespace boundaries."""
    baseline = dict(boolean_value='true', numeric_value='01', list_value='001 02', preserved=' raw ', fixed_value='true', default_value='')
    result = []
    for field, alternatives in {
        'boolean_value': [('true', True), ('1', True), (' true ', True), ('01', False), ('false', False), ('2', False)],
        'numeric_value': [('1', True), ('01', True), ('+001', True), ('true', False), ('0', False), ('65536', False)],
        'list_value': [('1 2', True), ('001 02', True), ('1\t2', True), ('2 1', False), ('one two', False), ('1 2 3', False)],
        'preserved': [(' raw ', True), ('raw', False), ('  raw  ', False), ('other', False)],
        'fixed_value': [('true', True), ('1', True), ('01', False), ('false', False)],
        'default_value': [('', True), ('true', True), ('001', True)],
    }.items():
        for text, valid in alternatives:
            values = dict(baseline, **{field: text})
            document = '<probe>' + ''.join(f'<{key}>{value}</{key}>' for key, value in values.items()) + '</probe>'
            result.append((document, valid))
    return result


CPP = r'''#include "schema.hpp"
#include <rohit/xml.hpp>
#include <rohit/xml_tokens.hpp>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
// Compare the ordinary facade's exact binary identity independently of XML presentation state.
std::string native_bytes(const outer::probe& value){rohit::full_stream_auto_alloc output{};value.serialize_out<rohit::serializer::binary_none>(output);return {reinterpret_cast<const char*>(output.begin()),output.current_offset()};}
// Exercise tree and direct-token facades using the same independent source decisions.
int main(int argc,char** argv){for(int index=1;index<argc;++index){const std::string path=argv[index];const bool valid=path.ends_with(".valid.xml");std::ifstream file{path,std::ios::binary};const std::string input{std::istreambuf_iterator<char>{file},{}};
for(bool token:{false,true}){outer::probe value{};try{value=token?rohit::serializer::xml::deserialize_tokens<outer::probe>(input):rohit::serializer::xml::deserialize<outer::probe>(input);}catch(const std::invalid_argument&){if(valid)throw;continue;}if(!valid)throw std::runtime_error{"Invalid union admitted"};if(value.preserved!=" raw ")throw std::runtime_error{"Union spelling changed"};
const auto output=token?rohit::serializer::xml::serialize_tokens(value):rohit::serializer::xml::serialize(value);const auto copy=rohit::serializer::xml::deserialize_tokens<outer::probe>(output);if(native_bytes(value)!=native_bytes(copy))throw std::runtime_error{"Native identity changed: "+path};
value.boolean_value="01";bool rejected=false;try{(void)(token?rohit::serializer::xml::serialize_tokens(value):rohit::serializer::xml::serialize(value));}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error{"Edited union published"};std::ofstream destination{path+".cpp.xml",std::ios::binary};destination.write(output.data(),static_cast<std::streamsize>(output.size()));}}}
'''


JAVA = '''import java.nio.file.*;import java.util.Arrays;
public class Main {
  /** Compare selected value families against independent expected decisions and native round trips. */
  public static void main(String[] paths)throws Exception{for(String path:paths){boolean valid=path.endsWith(".valid.xml");
  Schema.Outer.Probe value;try{value=Schema.Outer.Probe.decodeXml(Files.readAllBytes(Path.of(path)));}catch(IllegalArgumentException error){if(valid)throw error;continue;}
  if(!valid)throw new AssertionError(path);if(!value.preserved.equals(" raw "))throw new AssertionError("Union spelling changed");
  byte[] before=value.encode(Schema.Protocol.BINARY_NONE);byte[] output=value.encodeXml();var copy=Schema.Outer.Probe.decodeXml(output);
  if(!Arrays.equals(before,copy.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Native identity changed");
  value.booleanValue="01";boolean rejected=false;try{value.encodeXml();}catch(IllegalArgumentException error){rejected=true;}if(!rejected)throw new AssertionError("Edited union published");
  Files.write(Path.of(path+".java.xml"),output);}}
}'''
CSHARP = '''using System;using System.IO;using static SerializerGenerated.Schema;
class Program {
  // Require native wire identity, selected whitespace and failure before output publication.
  static void Main(string[] paths){foreach(string path in paths){bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);OuterProbe value;
  try{value=OuterProbe.DecodeXml(File.ReadAllBytes(path));}catch(FormatException){if(valid)throw;continue;}
  if(!valid||value.Preserved!=" raw ")throw new Exception(path);byte[] before=value.Encode(Protocol.BINARY_NONE);byte[] output=value.EncodeXml();var copy=OuterProbe.DecodeXml(output);
  if(!before.AsSpan().SequenceEqual(copy.Encode(Protocol.BINARY_NONE)))throw new Exception("Native identity changed");
  value.BooleanValue="01";bool rejected=false;try{value.EncodeXml();}catch(FormatException){rejected=true;}if(!rejected)throw new Exception("Edited union published");File.WriteAllBytes(path+".csharp.xml",output);}}
}'''
GO = '''package main
import("bytes";"fmt";"os";"strings")
// Verify complete scalar identities before serializing ordinary native fields.
func main(){for _,path:=range os.Args[1:]{input,err:=os.ReadFile(path);if err!=nil{panic(err)};valid:=strings.HasSuffix(path,".valid.xml");value,err:=DecodeOuterProbeXML(input);if (err==nil)!=valid{panic(fmt.Sprintf("%s: %v",path,err))};if !valid{continue};if value.Preserved!=" raw "{panic("Union spelling changed")};before,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};output,err:=value.EncodeXML();if err!=nil{panic(err)};copy,err:=DecodeOuterProbeXML(output);if err!=nil{panic(err)};after,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(before,after){panic("Native identity changed")};value.BooleanValue="01";if output,err:=value.EncodeXML();err==nil||output!=nil{panic("Edited union published")};if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}}}
'''
KOTLIN = '''import java.nio.file.Files
import java.nio.file.Path
// Preserve first-member spelling and reject edits before returning output bytes.
fun main(paths: Array<String>){for(path in paths){val valid=path.endsWith(".valid.xml")
val value=try{OuterProbe.decodeXml(Files.readAllBytes(Path.of(path)))}catch(error:IllegalArgumentException){if(valid)throw error;continue}
check(valid && value.preserved==" raw ");val before=value.encode(Protocol.BINARY_NONE);val output=value.encodeXml();val copy=OuterProbe.decodeXml(output)
check(before.contentEquals(copy.encode(Protocol.BINARY_NONE)));value.booleanValue="01";var rejected=false;try{value.encodeXml()}catch(_:IllegalArgumentException){rejected=true};check(rejected)
Files.write(Path.of(path+".kotlin.xml"),output)}}'''
SWIFT = '''import Foundation
// Keep expected candidate failures separate from assertion failures.
func require(_ value: Bool) throws {guard value else {throw SerializerError.invalid("Union expectation failed")}}
for path in CommandLine.arguments.dropFirst(){let valid=path.hasSuffix(".valid.xml");var value:OuterProbe
do{value=try OuterProbe.decodeXml(Array(Data(contentsOf:URL(fileURLWithPath:path))))}catch{if valid{throw error};continue}
try require(valid && value.preserved==" raw ");let before=try value.encode(Protocol.BINARY_NONE);let output=try value.encodeXml();let copy=try OuterProbe.decodeXml(output)
try require(before == copy.encode(Protocol.BINARY_NONE));value.booleanValue="01";var rejected=false;do{_=try value.encodeXml()}catch{rejected=true};try require(rejected)
try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))}'''
RUST = '''mod schema;use schema::*;
/// Require the ordinary native facade to retain selected whitespace and reject edited families.
fn main()->Result<(),Box<dyn std::error::Error>>{for path in std::env::args().skip(1){let valid=path.ends_with(".valid.xml");let input=std::fs::read_to_string(&path)?;
let mut value=match OuterProbe::decode_xml(&input){Ok(value)=>{assert!(valid,"{path}");value},Err(error)=>{if valid{return Err(error.into())};continue}};
assert_eq!(value.preserved," raw ");let output=value.encode_xml()?;let copy=OuterProbe::decode_xml(&output)?;
for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{assert_eq!(value.encode(protocol)?,copy.encode(protocol)?);}
value.boolean_value="01".to_owned();assert!(value.encode_xml().is_err());std::fs::write(format!("{path}.rust.xml"),output)?;}Ok(())}'''
C = shared.C.split('int main(')[0] + '''
/* Require the complete ordinary native facade to enforce selected primitive identities transactionally. */
int main(int argc,char** argv){for(int index=1;index<argc;++index){const char* path=argv[index];const bool valid=strstr(path,".valid.xml")!=NULL;
size_t size=0;uint8_t* input=read_file(path,&size);outer_probe value={0},copy={0};require(outer_probe_init(&value)==srl_ok&&outer_probe_init(&copy)==srl_ok,"Init");
const outer_probe previous=value;srl_status status=outer_probe_decode_xml(&value,input,size,NULL);free(input);require((status==srl_ok)==valid,path);
if(!valid){require(!memcmp(&previous,&value,sizeof(value)),"Atomic native input");}else{
require(value.preserved.size==5u&&!memcmp(value.preserved.data," raw ",5u),"Selected whitespace");srl_buffer output={0};require(outer_probe_encode_xml(&value,&output,NULL)==srl_ok,"Output");require(outer_probe_decode_xml(&copy,output.data,output.size,NULL)==srl_ok,"Replay");
const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};for(size_t protocol=0;protocol<sizeof(protocols)/sizeof(protocols[0]);++protocol){srl_buffer first={0},second={0};require(outer_probe_encode(&value,protocols[protocol],&first)==srl_ok&&outer_probe_encode(&copy,protocols[protocol],&second)==srl_ok,"Native encode");require(first.size==second.size&&!memcmp(first.data,second.data,first.size),"Native identity");srl_buffer_free(&first);srl_buffer_free(&second);}
require(srl_string_set_cstr(&value.boolean_value,"01")==srl_ok,"Edit");srl_buffer rejected={0};require(outer_probe_encode_xml(&value,&rejected,NULL)!=srl_ok&&!rejected.data,"Edited output");
char* destination=(char*)malloc(strlen(path)+sizeof(".c.xml"));require(destination!=NULL,"Path");strcpy(destination,path);strcat(destination,".c.xml");FILE* file=fopen(destination,"wb");require(file!=NULL&&fwrite(output.data,1,output.size,file)==output.size&&fclose(file)==0,"Write");free(destination);srl_buffer_free(&output);}
outer_probe_free(&value);outer_probe_free(&copy);}return EXIT_SUCCESS;}
'''
PYTHON = '''from pathlib import Path
import sys
from schema import *
# Exercise selected primitive identities and all ordinary native protocols.
for argument in sys.argv[1:]:
    path=Path(argument);valid=path.name.endswith('.valid.xml')
    try: value=OuterProbe.decode_xml(path.read_bytes())
    except ValueError:
        if valid: raise
        continue
    assert valid and value.preserved==' raw ',path
    output=value.encode_xml();copy=OuterProbe.decode_xml(output)
    for protocol in Protocol: assert value.encode(protocol)==copy.encode(protocol)
    value.boolean_value='01';rejected=False
    try: value.encode_xml()
    except ValueError: rejected=True
    assert rejected,path
    Path(str(path)+'.python.xml').write_bytes(output)
'''
JAVASCRIPT = '''import * as M from './schema.mjs';
import {readFileSync,writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
// Preserve raw union member spelling and prevent edited primitive family publication.
for(const path of process.argv.slice(2)){const valid=path.endsWith('.valid.xml');let value;
try{value=M.OuterProbe.decodeXml(readFileSync(path,'utf8'));}catch(error){if(valid)throw error;continue;}
assert.deepEqual(valid,true,path);assert.deepEqual(value.preserved,' raw ');const output=value.encodeXml(),copy=M.OuterProbe.decodeXml(output);
for(const protocol of Object.values(M.Protocol))assert.deepEqual(value.encode(protocol),copy.encode(protocol));
value.booleanValue='01';assert.throws(()=>value.encodeXml());writeFileSync(path+'.LANG.xml',output);}
'''


def main():
    """Build every requested SDK from generated source and revalidate emitted XML independently."""
    from lxml import etree
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--language', default='python,javascript,typescript,java,csharp,go,kotlin,swift,rust,c')
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--cpp-library', type=Path)
    parser.add_argument('--skip-export', action='store_true', help='Isolate runtime diagnostics while exported contract qualification is run separately')
    parser.set_defaults(wsl_languages='', cpp_executable=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    languages = args.language.split(',')
    supported = ('cpp', 'python', 'javascript', 'typescript', 'java', 'csharp', 'go', 'kotlin', 'swift', 'rust', 'c')
    if not languages or len(set(languages)) != len(languages) or any(value not in supported for value in languages):
        parser.error('Select distinct supported SDK consumers')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Output must be under ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='unions-', dir=args.build) as directory:
        args.build = Path(directory)
        (args.build / 'union.xsd').write_text(XSD, encoding='utf-8')
        (args.build / 'model.serializer').write_text(MODEL, encoding='utf-8')
        oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
        paths = []
        for index, (document, valid) in enumerate(cases()):
            # Preserve separate known provider boundaries: '+' on unsigned forms
            # and boolean aliases in union fixed values are compared lexically.
            control = document.replace('+001', '001').replace('<fixed_value>true</fixed_value>', '<fixed_value>1</fixed_value>')
            if not USE_JAXP and oracle.validate(etree.fromstring(control.encode())) != valid:
                raise AssertionError(f'Independent oracle disagrees at {index}: {oracle.error_log}')
            path = args.build / f'{index}.{"valid" if valid else "invalid"}.xml'
            path.write_text(document, encoding='utf-8'); paths.append(path)
        targets = {}
        for language in languages:
            target = args.build / language; target.mkdir()
            targets['js' if language == 'javascript' else language] = target / ('schema.hpp' if language == 'cpp' else sdk.OUTPUTS[language])
        if 'typescript' in languages and 'javascript' not in languages:
            targets['js'] = args.build / 'typescript/schema.mjs'
        command = [args.compiler, '--input', args.build / 'model.serializer', '--language', ','.join(targets),
                   '--go.package', 'main', '--csharp.namespace', 'SerializerGenerated']
        if not args.skip_export:
            command += ['--xml.export-xsd', args.build / 'exported.xsd']
        for language, target in targets.items():
            command += [f'--{language}.output', target]
        command += ['--cpp.format', 'false']
        runner = sdk.Runner(args)
        oracle_command = None
        if USE_JAXP:
            import go_xml_qname_constraints_test as qnames
            oracle_directory = args.build / 'oracle'; oracle_directory.mkdir()
            (oracle_directory / 'Main.java').write_text(qnames.JAVA_ORACLE, encoding='utf-8')
            sdk.run(runner.command('java', 'javac', '--release', '17', '-encoding', 'UTF-8', '-Xlint:all', '-Werror',
                                   '-d', runner.path('java', oracle_directory), runner.path('java', oracle_directory / 'Main.java')))
            oracle_command = runner.command('java', 'java', '-cp', runner.path('java', oracle_directory), 'Main',
                                            runner.path('java', args.build / 'union.xsd'))
            sdk.run([*oracle_command, *[runner.path('java', path) for path in paths]])
        sdk.run(command)
        if 'typescript' in languages and 'javascript' in languages:
            shutil.copyfile(args.build / 'javascript/schema.mjs', args.build / 'typescript/schema.mjs')
        exported_oracle = oracle if args.skip_export else etree.XMLSchema(etree.parse(str(args.build / 'exported.xsd')))
        exported_command = None
        if oracle_command is not None:
            exported_command = oracle_command if args.skip_export else [*oracle_command[:-1], runner.path('java', args.build / 'exported.xsd')]
            sdk.run([*exported_command, *[runner.path('java', path) for path in paths]])
        else:
            for (document, valid), path in zip(cases(), paths):
                control = document.replace('+001', '001').replace('<fixed_value>true</fixed_value>', '<fixed_value>1</fixed_value>')
                if exported_oracle.validate(etree.fromstring(control.encode())) != valid:
                    raise AssertionError(f'Exported contract disagrees at {path}: {exported_oracle.error_log}')
        outputs = []
        consumers = [('cpp', 'main.cpp', CPP), ('python', 'main.py', PYTHON), ('javascript', 'main.mjs', JAVASCRIPT.replace('LANG', 'javascript')),
                     ('typescript', 'main.mts', JAVASCRIPT.replace('LANG', 'typescript')),
                     ('java', 'Main.java', JAVA), ('csharp', 'Program.cs', CSHARP), ('go', 'main.go', GO),
                     ('kotlin', 'Main.kt', KOTLIN), ('swift', 'main.swift', SWIFT), ('rust', 'main.rs', RUST), ('c', 'main.c', C)]
        for language, filename, text in consumers:
            if language not in languages:
                continue
            source = args.build / 'source' / language; source.mkdir(parents=True)
            (source / filename).write_text(text, encoding='utf-8')
            if language == 'rust':
                for name in ('Cargo.toml', 'Cargo.lock'):
                    shutil.copyfile(ROOT / 'example/xml/rust' / name, source / name)
            if language in ('javascript', 'typescript'):
                for name in ('package.json', 'package-lock.json'):
                    shutil.copyfile(ROOT / 'test/resources/xml_shapes' / language / name, source / name)
            if language == 'cpp':
                import os
                target = args.build / language
                shutil.copyfile(source / filename, target / filename)
                executable = target / ('main.exe' if os.name == 'nt' else 'main')
                library = args.cpp_library or args.compiler.parent / ('serializer_runtime.lib' if os.name == 'nt' else 'libserializer_runtime.a')
                prefix = args.libxml2_prefix or args.compiler.parent / 'vcpkg_installed/x64-windows'
                if os.name == 'nt':
                    sdk.run(runner.command('cpp', 'cl', '/nologo', '/std:c++20', '/EHsc', '/MD', '/O2', '/W4', '/WX', '/utf-8',
                        '/I'+str(ROOT / 'include'), '/I'+str(target), str(target / filename), str(library),
                        *[str(prefix / 'lib' / name) for name in ('zstd.lib','lz4.lib','z.lib','libxml2.lib')], '/Fe:'+str(executable)), cwd=target)
                    for dll in (prefix / 'bin').glob('*.dll'): shutil.copyfile(dll, target / dll.name)
                else:
                    sdk.run(runner.command('cpp', 'c++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                        '-I'+str(ROOT / 'include'), '-I'+str(target), str(target / filename), str(library),
                        '-lxml2', '-lzstd', '-llz4', '-lz', '-o', str(executable)), cwd=target)
                launch = [str(executable)]
            else:
                launch = runner_module.build_language(args, runner, language, args.build / language, source_root=source.parent)
            sdk.run([*launch, *[runner.path(language, path) for path in paths]])
            for (_, valid), path in zip(cases(), paths):
                output = Path(str(path) + f'.{language}.xml')
                if valid and not USE_JAXP and not oracle.validate(etree.fromstring(output.read_bytes().replace(b'+001', b'001').replace(b'<fixed_value>true</fixed_value>', b'<fixed_value>1</fixed_value>'))):
                    raise AssertionError(f'Emitted union violates XSD: {oracle.error_log}')
                if valid and not USE_JAXP and not exported_oracle.validate(etree.fromstring(output.read_bytes().replace(b'+001', b'001').replace(b'<fixed_value>true</fixed_value>', b'<fixed_value>1</fixed_value>'))):
                    raise AssertionError(f'Emitted union violates exported XSD: {exported_oracle.error_log}')
                if valid:
                    CHECK_OUTPUT(path.read_bytes(), output.read_bytes())
                    outputs.append(output)
                if not valid and output.exists():
                    raise AssertionError(f'Invalid candidate published: {path}')
            print(f'PASS: {language}: {len(paths)} outer union decisions, edited output and native identity', flush=True)
        if oracle_command is not None:
            sdk.run([*oracle_command, *[runner.path('java', path) for path in outputs]])
            sdk.run([*exported_command, *[runner.path('java', path) for path in outputs]])


if __name__ == '__main__':
    main()
