# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Exercise element-only variant arrays with actual SDKs and independent XML observations."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import tempfile
import xml.etree.ElementTree as etree

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('ordered_xml_runner', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(xml_runner)
sdk = xml_runner.sdk
MODEL = '''serializer version 1.21.0;
namespace ordered {
  class left { public string value(1) xml(text); }
  class right { public uint32 id(1) xml(attribute); }
  class fragment { private variant(left=left, right=right) value(1); }
  class root { public array<fragment> items(1); }
  class fixed { public array[3] fragment items(1); }
}
'''
CONFIG = '''[xml]
enabled=true
[xml.class.ordered::root]
ordered=items
[xml.class.ordered::fixed]
ordered=items
'''


def cases():
    """Distinguish native occurrence budgets from child scalar text and XML formatting."""
    content = '<left>A&amp;B</left><right id="42"/><left>C</left>'
    return [
        ('empty', '<root/>', 0, True),
        ('space', '<root> \t\r\n<![CDATA[ ]]><!--ignored--><?ignored x?></root>', 0, True),
        ('interleaved', '<root>\n' + content + '\n</root>', 3, True),
        ('tight', '<root>' + content + '</root>', 2, False),
        ('zero', '<root><right id="1"/></root>', 0, False),
        ('text', '<root><left>A</left>unexpected<right id="2"/></root>', 3, False),
        ('unicode-space', '<root>\u00a0</root>', 1, False),
        ('unknown', '<root><unknown/></root>', 3, False),
        ('namespace', '<root><left xmlns="urn:wrong">A</left></root>', 3, False),
        ('bad-child', '<root><right id="-1"/></root>', 3, False),
        ('fixed', '<fixed>' + content + '</fixed>', 3, True),
        ('fixed-tight', '<fixed>' + content + '</fixed>', 2, False),
        ('fixed-short', '<fixed><left>A</left></fixed>', 3, False),
        ('fixed-excess', '<fixed>' + content + '<right id="0"/></fixed>', 4, False),
    ]


PYTHON = '''from pathlib import Path
import sys
from schema import *
# Require rejected complete operations to leave no output artifact.
for index in range(1,len(sys.argv),2):
    path=Path(sys.argv[index]); valid=path.name.endswith('.valid.xml')
    kind=OrderedFixed if path.name.startswith('fixed') else OrderedRoot
    limits=Limits(xml_mixed=XmlMixedLimits(max_fragments=int(sys.argv[index+1]),max_text_bytes=0))
    try:
        value=kind.decode_xml(path.read_bytes(),limits)
        assert valid,path
        output=value.encode_xml(limits)
        copy=kind.decode_xml(output,limits)
        for protocol in Protocol:
            assert value.encode(protocol)==copy.encode(protocol)
    except ValueError:
        if valid: raise
        continue
    assert valid,path
    path.with_name(path.name+'.python.xml').write_bytes(output)
'''
JAVASCRIPT = '''import * as M from './schema.mjs';
import {readFileSync,writeFileSync} from 'node:fs';
import assert from 'node:assert/strict';
// Match fixture validity before publishing the completed XML string.
for(let index=2;index<process.argv.length;index+=2){
  const path=process.argv[index],valid=path.endsWith('.valid.xml');
  const Kind=path.split(String.fromCharCode(92)).join('/').split('/').some(name=>name.startsWith('fixed'))?M.OrderedFixed:M.OrderedRoot;
  const limits=new M.XmlLimits({mixed:new M.XmlMixedLimits({maxFragments:Number(process.argv[index+1]),maxTextBytes:0})});
  let value;
  try{value=Kind.decodeXml(readFileSync(path,'utf8'),limits);}catch(failure){if(valid)throw failure;continue;}
  assert.deepEqual(valid,true,path);
  const output=value.encodeXml(limits),copy=Kind.decodeXml(output,limits);
  for(const protocol of Object.values(M.Protocol))assert.deepEqual(value.encode(protocol),copy.encode(protocol));
  writeFileSync(path+'.LANG.xml',output);
}
'''
JAVA = '''import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
/** Exercise private fragment payloads through their owning XML collection. */
public final class Main {
  /** Preserve all native protocols after the XML reader removes formatting. */
  public static void main(String[] args)throws Exception{
    for(int index=0;index<args.length;index+=2){
      var path=Path.of(args[index]);boolean valid=args[index].endsWith(".valid.xml");
      var limits=new Schema.XmlMixedLimits(Integer.parseInt(args[index+1]),0);byte[] output;
      try{BODY}catch(IllegalArgumentException failure){if(valid)throw failure;continue;}
      if(!valid)throw new AssertionError(path);Files.write(Path.of(args[index]+".java.xml"),output);
    }
  }
}'''
CSHARP = '''using System;
using System.IO;
using static SerializerGenerated.Schema;
internal static class Program {
  // Verify exact native payload identity through ordered XML load/save.
  private static void Main(string[] args){
    for(int index=0;index<args.Length;index+=2){
      string path=args[index];bool valid=path.EndsWith(".valid.xml",StringComparison.Ordinal);
      var limits=new XmlMixedLimits(int.Parse(args[index+1],System.Globalization.CultureInfo.InvariantCulture),0);byte[] output;
      try{BODY}catch(FormatException){if(valid)throw;continue;}
      if(!valid)throw new Exception(path);File.WriteAllBytes(path+".csharp.xml",output);
    }
  }
}'''
KOTLIN = '''import java.nio.file.Files
import java.nio.file.Path
// Private variants remain private while the owning array preserves their order.
fun main(args: Array<String>){
  for(index in args.indices step 2){
    val path=Path.of(args[index]);val valid=args[index].endsWith(".valid.xml")
    val limits=XmlMixedLimits(args[index+1].toInt(),0);val output:ByteArray
    try{BODY}catch(failure:IllegalArgumentException){if(valid)throw failure;continue}
    check(valid);Files.write(Path.of(args[index]+".kotlin.xml"),output)
  }
}'''
SWIFT = '''import Foundation
// Distinguish a test expectation from a generated decoder's legitimate rejection.
enum OrderedTestError: Error { case acceptedInvalid }
// Keep expectation failures outside the generated XML rejection catch.
func require(_ value:Bool)throws{if !value{throw SerializerError.invalid("Ordered XML expectation")}}
let args=Array(CommandLine.arguments.dropFirst())
for index in stride(from:0,to:args.count,by:2){
  let path=args[index],valid=path.hasSuffix(".valid.xml")
  let limits=XmlMixedLimits(maxFragments:Int(args[index+1])!,maxTextBytes:0)
  let input=Array(try Data(contentsOf:URL(fileURLWithPath:path)));let output:[UInt8]
  do{BODY}catch{if valid || error is OrderedTestError{throw error};continue}
  try require(valid);try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))
}'''
GO = '''package main
import("bytes";"fmt";"os";"path/filepath";"strconv";"strings")
// Borrow the same decoded native values for XML and ordinary wire output.
func probe(path string,limits XmlMixedLimits)([]byte,error){
  input,err:=os.ReadFile(path);if err!=nil{return nil,err}
  BODY
}
// Publish only complete values matching the independent fixture decision.
func main(){for index:=1;index<len(os.Args);index+=2{
  path:=os.Args[index];count,err:=strconv.Atoi(os.Args[index+1]);if err!=nil{panic(err)}
  output,err:=probe(path,XmlMixedLimits{count,0});valid:=strings.HasSuffix(path,".valid.xml")
  if valid!=(err==nil){panic(fmt.Sprintf("Ordered XML decision %s: %v",path,err))}
  if valid{if err=os.WriteFile(path+".go.xml",output,0600);err!=nil{panic(err)}}
}}
'''
RUST = '''mod schema;
use schema::*;
use std::{env,fs,path::Path};
/// Exercise owning XML collections while comparing every existing native codec.
fn main()->Result<(),Box<dyn std::error::Error>>{
  let args:Vec<String>=env::args().skip(1).collect();
  for args in args.chunks(2){
    let path=&args[0];let valid=path.ends_with(".valid.xml");let input=fs::read_to_string(path)?;
    let limits=XmlMixedLimits{max_fragments:args[1].parse()?,max_text_bytes:0};
    let result=(||->Result<String,Error>{BODY})();
    match result{Ok(output)=>{assert!(valid,"{path}");fs::write(format!("{path}.rust.xml"),output)?;},Err(error)=>{if valid{return Err(error.into());}}}
  }
  Ok(())
}'''
C = '''#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Fail independently of the generated codec status under test. */
static void require(bool value){if(!value){fputs("Ordered XML expectation failed\\n",stderr);exit(EXIT_FAILURE);}}
/* Read bounded test-owned fixtures; the corpus contains only small XML documents. */
static srl_buffer read_file(const char* path){
  FILE* file=fopen(path,"rb");require(file!=NULL);require(fseek(file,0,SEEK_END)==0);long length=ftell(file);
  require(length>=0&&fseek(file,0,SEEK_SET)==0);srl_buffer bytes={0};bytes.size=(size_t)length;
  bytes.data=(uint8_t*)malloc(bytes.size+1u);require(bytes.data!=NULL);bytes.capacity=bytes.size+1u;
  require(fread(bytes.data,1,bytes.size,file)==bytes.size);require(fclose(file)==0);return bytes;
}
CHECKS
/* Verify transactional native decoding before publishing a successful XML result. */
int main(int argc,char** argv){
  for(int index=1;index+1<argc;index+=2){
    const char* path=argv[index];bool valid=strstr(path,".valid.xml")!=NULL;
    const char* filename=strrchr(path,'\\\\');if(!filename){filename=strrchr(path,'/');}filename=filename?filename+1:path;
    srl_buffer input=read_file(path),output={0};srl_xml_mixed_limits limits={(size_t)strtoull(argv[index+1],NULL,10),0};
    srl_status status=strncmp(filename,"fixed",sizeof("fixed")-1u)==0?check_fixed(&input,&limits,&output,valid):check_root(&input,&limits,&output,valid);
    require((status==srl_ok)==valid);
    if(valid){
      const char suffix[]=".c.xml";size_t length=strlen(path);char* destination=(char*)malloc(length+sizeof(suffix));require(destination!=NULL);
      memcpy(destination,path,length);memcpy(destination+length,suffix,sizeof(suffix));FILE* file=fopen(destination,"wb");require(file!=NULL);
      require(fwrite(output.data,1,output.size,file)==output.size);require(fclose(file)==0);free(destination);
    }else{require(output.data==NULL);}
    srl_buffer_free(&input);srl_buffer_free(&output);
  }
  return EXIT_SUCCESS;
}
'''
C_CHECK = '''/* Compare complete candidates and preserve the destination on rejected input. */
static srl_status check_KIND(const srl_buffer* input,const srl_xml_mixed_limits* limits,srl_buffer* output,bool valid){
  ordered_KIND value={0},copy={0};require(ordered_KIND_init(&value)==srl_ok&&ordered_KIND_init(&copy)==srl_ok);
  const ordered_KIND previous=value;
  srl_status status=ordered_KIND_decode_xml_with_mixed_limits(&value,input->data,input->size,NULL,limits);
  if(status!=srl_ok){require(memcmp(&value,&previous,sizeof(value))==0);goto done;}
  require(valid);
  status=ordered_KIND_encode_xml_with_mixed_limits(&value,output,NULL,limits);if(status!=srl_ok){goto done;}
  require(ordered_KIND_decode_xml(&copy,output->data,output->size,NULL)==srl_ok);
  const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
  for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){
    srl_buffer left={0},right={0};require(ordered_KIND_encode(&value,protocols[index],&left)==srl_ok&&ordered_KIND_encode(&copy,protocols[index],&right)==srl_ok);
    require(left.size==right.size&&memcmp(left.data,right.data,left.size)==0);srl_buffer_free(&left);srl_buffer_free(&right);
  }
done:ordered_KIND_free(&value);ordered_KIND_free(&copy);return status;
}
'''


def consumer(language):
    """Generate small typed consumers; each branch uses its public owning facade."""
    bodies = []
    for kind in ('Fixed', 'Root'):
        if language == 'java':
            body = f'''var value=Schema.Ordered.{kind}.decodeXml(Files.readAllBytes(path),Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),limits);
if(!valid)throw new AssertionError(path);
output=value.encodeXml(Schema.Limits.defaults(),null,new Schema.XmlMapLimits(),limits);
var copy=Schema.Ordered.{kind}.decodeXml(output);
for(var protocol:Schema.Protocol.values())if(!Arrays.equals(value.encode(protocol),copy.encode(protocol)))throw new AssertionError("Native wire changed");'''
        elif language == 'csharp':
            body = f'''var value=Ordered{kind}.DecodeXml(File.ReadAllBytes(path),new Limits(),null,new XmlMapLimits(),limits);
if(!valid)throw new Exception(path);
output=value.EncodeXml(new Limits(),null,new XmlMapLimits(),limits);var copy=Ordered{kind}.DecodeXml(output);
foreach(var protocol in Enum.GetValues<Protocol>())if(!value.Encode(protocol).AsSpan().SequenceEqual(copy.Encode(protocol)))throw new Exception("Native wire changed");'''
        elif language == 'kotlin':
            body = f'''val value=Ordered{kind}.decodeXml(Files.readAllBytes(path),mixedLimits=limits)
check(valid)
output=value.encodeXml(mixedLimits=limits);val copy=Ordered{kind}.decodeXml(output)
for(protocol in Protocol.entries)check(value.encode(protocol).contentEquals(copy.encode(protocol)))'''
        elif language == 'swift':
            body = f'''let value=try Ordered{kind}.decodeXml(input,mixedLimits:limits)
if !valid{{throw OrderedTestError.acceptedInvalid}}
output=try value.encodeXml(mixedLimits:limits);let copy=try Ordered{kind}.decodeXml(output)
for protocolValue in Protocol.allCases{{try require(value.encode(protocolValue)==copy.encode(protocolValue))}}'''
        elif language == 'go':
            body = f'''value,err:=DecodeOrdered{kind}XMLWithMixedLimits(input,limits);if err!=nil{{return nil,err}}
if strings.HasSuffix(path,".invalid.xml"){{panic("Invalid ordered input admitted")}}
output,err:=value.EncodeXMLWithMixedLimits(limits);if err!=nil{{return nil,err}};copy,err:=DecodeOrdered{kind}XML(output);if err!=nil{{return nil,err}}
for _,protocol:=range []Protocol{{JSON,BINARY_NONE,BINARY_INTEGER,BINARY_STRING}}{{first,err:=value.Encode(protocol);if err!=nil{{return nil,err}};second,err:=copy.Encode(protocol);if err!=nil||!bytes.Equal(first,second){{panic("Native wire changed")}}}}
return output,nil'''
        elif language == 'rust':
            body = f'''let value=Ordered{kind}::decode_xml_with_mixed_limits(&input,XmlLimits::default(),None,limits)?;
assert!(valid,"Invalid ordered input admitted: {{path}}");
let output=value.encode_xml_with_mixed_limits(XmlLimits::default(),None,limits)?;let copy=Ordered{kind}::decode_xml(&output)?;
for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString]{{assert_eq!(value.encode(protocol)?,copy.encode(protocol)?);}}
Ok(output)'''
        else:
            raise ValueError(language)
        bodies.append(body)
    if language == 'java':
        condition = 'path.getFileName().toString().startsWith("fixed")'
    elif language == 'csharp':
        condition = 'Path.GetFileName(path).StartsWith("fixed",StringComparison.Ordinal)'
    elif language == 'kotlin':
        condition = 'path.fileName.toString().startsWith("fixed")'
    elif language == 'swift':
        condition = 'URL(fileURLWithPath:path).lastPathComponent.hasPrefix("fixed")'
    elif language == 'go':
        condition = 'strings.HasPrefix(filepath.Base(path),"fixed")'
    else:
        condition = 'Path::new(path).file_name().unwrap().to_str().unwrap().starts_with("fixed")'
    if language in ('java', 'csharp', 'kotlin'):
        condition = '(' + condition + ')'
    return globals()[language.upper()].replace('BODY', f'if {condition}{{{bodies[0]}}}else{{{bodies[1]}}}')


def execute(args):
    """Compile selected native SDKs and compare original child order and expanded names."""
    languages = args.language.split(',')
    source = args.build / 'model.serializer'
    source.write_text(MODEL, encoding='utf-8')
    config = args.build / 'options.ini'
    config.write_text(CONFIG, encoding='utf-8')
    command = [args.compiler, '--input', source, '--config', config, '--go.package', 'main', '--kotlin.package=']
    targets = {}
    for language in languages:
        directory = args.build / language
        directory.mkdir()
        targets['js' if language == 'javascript' else language] = directory / sdk.OUTPUTS[language]
    if 'typescript' in languages and 'javascript' not in languages:
        targets['js'] = args.build / 'typescript/schema.mjs'
    command += ['--language', ','.join(targets)]
    for language, path in targets.items():
        command += [f'--{language}.output', path]
    sdk.run(command)
    if 'typescript' in languages and 'javascript' in languages:
        shutil.copyfile(targets['js'], args.build / 'typescript/schema.mjs')
    probes = []
    for name, document, maximum, valid in cases():
        path = args.build / f'{name}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8')
        probes.append((path, maximum, valid))
    runner = sdk.Runner(args)
    for language in languages:
        directory = args.build / language
        source = args.build / 'source' / language
        source.mkdir(parents=True)
        if language in ('javascript', 'typescript'):
            filename = 'main.mts' if language == 'typescript' else 'main.mjs'
            body = JAVASCRIPT.replace('LANG', language)
            for name in ('package.json', 'package-lock.json'):
                shutil.copyfile(ROOT / 'test/resources/xml_shapes' / language / name, source / name)
        elif language == 'rust':
            filename, body = 'main.rs', consumer(language)
            for name in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(ROOT / 'example/xml/rust' / name, source / name)
        elif language == 'python':
            filename, body = 'main.py', PYTHON
        elif language == 'c':
            filename, body = 'main.c', C.replace('CHECKS', ''.join(C_CHECK.replace('KIND', kind) for kind in ('fixed', 'root')))
        else:
            filename = {'java': 'Main.java', 'csharp': 'Program.cs', 'kotlin': 'Main.kt', 'swift': 'main.swift', 'go': 'main.go'}[language]
            body = consumer(language)
        (source / filename).write_text(body, encoding='utf-8')
        launch = xml_runner.build_language(args, runner, language, directory, source_root=source.parent)
        arguments = []
        for path, maximum, _ in probes:
            arguments += [runner.path(language, path), maximum]
        sdk.run([*launch, *arguments])
        for path, _, valid in probes:
            output = Path(str(path) + f'.{language}.xml')
            if valid:
                before, after = etree.parse(path).getroot(), etree.parse(output).getroot()
                assert [(item.tag, item.attrib, item.text or '') for item in before] == [(item.tag, item.attrib, item.text or '') for item in after], output
                assert before.tag == after.tag
            else:
                assert not output.exists(), output
        print(f'PASS: {language}: {len(probes)} ordered decisions and four native protocols', flush=True)


def main():
    """Keep task-created SDK artifacts in an ignored output tree or the system temp directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--language', default='python,javascript,typescript,java,csharp,go,kotlin,swift,rust,c')
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, cpp_executable=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    languages = args.language.split(',')
    if len(languages) != len(set(languages)) or any(item not in sdk.LANGUAGES or item == 'cpp' for item in languages):
        parser.error('Choose distinct supported portable SDKs')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Repository output must be below ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault('RUSTFLAGS', '-D warnings')
    with tempfile.TemporaryDirectory(prefix='ordered-', dir=args.build) as directory:
        args.build = Path(directory)
        execute(args)


if __name__ == '__main__':
    main()
