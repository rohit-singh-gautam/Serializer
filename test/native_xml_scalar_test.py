# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated C/Rust exact scalar, lexical list/union and prepared Unicode pattern contracts."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess

from lxml import etree
import portable_xml_scalar_test as scalars
import portable_xml_facets_test as facets

ROOT, sdk = scalars.ROOT, scalars.sdk
spec = importlib.util.spec_from_file_location('serializer_native_scalar_sdk', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)


def rust_consumer(mode):
    """Keep native value checks independent of emitted metadata and compare all existing codecs."""
    names = ('sample', 'amount', 'words', 'numbers', 'items', 'fixed_list', 'child_list', 'optional_child',
             'list_all', 'plain_default', 'guard') if mode == 'scalar' else ('sample',)
    namespace = 'Scalars' if mode == 'scalar' else 'Facets' if mode == 'numeric' else 'Patterns'
    branches = []
    for name in names:
        target = namespace + ''.join(part.title() for part in name.split('_'))
        checks = ''
        if mode == 'scalar' and name == 'sample':
            checks = '''assert_eq!(value.typed.len(),3);assert!(value.unit.is_some());assert!(value.fixed_flag);
for index in 0..7 {let mut broken=TYPE::decode_xml(&output)?;match index {
0=>broken.decimal_value="1e0".to_owned(),1=>broken.integer_value="1.0".to_owned(),
2=>broken.union_value="unknown".to_owned(),3=>broken.list_value="2 1".to_owned(),
4=>broken.fixed_text="bad".to_owned(),5=>broken.typed=vec![0;2],_=>broken.sign="0".to_owned()}
assert!(broken.encode_xml().is_err());}'''
        elif mode == 'scalar' and name in ('words', 'fixed_list'):
            checks = '''let mut broken=TYPE::decode_xml(&output)?;broken.value=vec!["one".to_owned()];assert!(broken.encode_xml().is_err());'''
        elif mode == 'scalar' and name == 'plain_default':
            checks = '''let mut broken=TYPE::decode_xml(&output)?;broken.value.clear();assert!(broken.encode_xml().is_err());'''
        elif mode == 'scalar' and name == 'items':
            checks = '''let mut broken=TYPE::decode_xml(&output)?;broken.value=vec!["two words".to_owned()];assert!(broken.encode_xml().is_err());'''
        elif mode == 'numeric':
            checks = '''assert!(value.integer>=u64::MAX-2&&value.integer<u64::MAX);assert_eq!(value.choice,u64::MAX);assert_eq!(value.rounded,0.1f32);
for index in 0..7 {let mut broken=TYPE::decode_xml(&output)?;match index {0=>broken.integer=u64::MAX,1=>broken.amount=-1.0,2=>broken.rounded=0.11,3=>broken.text="bad".to_owned(),4=>broken.choice=u64::MAX-1,5=>broken.enabled=false,_=>broken.code="bad".to_owned()}assert!(broken.encode_xml().is_err());}'''
        condition = 'true' if len(names) == 1 else f'filename.starts_with("{name}-")'
        branches.append('''if CONDITION { (||->Result<String,schema::Error>{
let value=schema::TYPE::decode_xml_with_validation(&text,limits,None)?;
let output=value.encode_xml_with_validation(limits,None)?;
let copy=schema::TYPE::decode_xml_with_validation(&output,limits,None)?;
for protocol in [schema::Protocol::Json,schema::Protocol::BinaryNone,schema::Protocol::BinaryInteger,schema::Protocol::BinaryString] {
assert_eq!(value.encode(protocol)?,copy.encode(protocol)?);}
CHECKS
Ok(output)})() }'''.replace('CONDITION', condition).replace('TYPE', target).replace('CHECKS', checks.replace('TYPE', 'schema::' + target)))
    return '''mod schema;
use std::{env,fs,path::Path};
/// Require exact scalar decisions and native wire identity before publishing output to the independent oracle.
fn main()->Result<(),Box<dyn std::error::Error>> {
for path in env::args().skip(1) {let text=fs::read_to_string(&path)?;let filename=Path::new(&path).file_name().unwrap().to_str().unwrap();
let valid=path.ends_with(".valid.xml");let mut limits=schema::XmlLimits::default();
if filename.starts_with("items-limit"){limits.max_nodes=16;}if filename.starts_with("items-text"){limits.max_text_bytes=18;}
let result=BRANCHES else {panic!("Unknown scalar fixture: {path}")};
match result {Ok(output)=>{assert!(valid,"Invalid scalar input published: {path}");fs::write(format!("{path}.rust.xml"),output)?;},
Err(error)=>{if valid{return Err(Box::new(error));}if filename.starts_with("guard-work")||filename.starts_with("work."){assert!(error.0.contains("limit"));}}}
}println!("Rust exact scalar/list/union/default/pattern/native-codec checks pass");Ok(())}
'''.replace('BRANCHES', ' else '.join(branches))


def c_consumer(mode):
    """Generate typed C consumers that preserve initialized destinations and reject edited invalid models."""
    names = ('sample', 'amount', 'words', 'numbers', 'items', 'fixed_list', 'child_list', 'optional_child',
             'list_all', 'plain_default', 'guard') if mode == 'scalar' else ('sample',)
    namespace = 'scalars' if mode == 'scalar' else 'facets' if mode == 'numeric' else 'patterns'
    functions, dispatch = [], []
    for name in names:
        target = namespace + '_' + name
        checks = ''
        if mode == 'scalar' and name == 'sample':
            checks = '''require(value.typed.size==3u&&value.unit.has_value&&value.fixed_flag,"Typed list/default materialization");
for (unsigned index=0;index<7u;++index) { TYPE broken={0};require(TYPE_init(&broken)==srl_ok&&TYPE_decode_xml(&broken,output.data,output.size,NULL)==srl_ok,"Edited scalar setup");
switch(index){case 0:require(srl_string_set_cstr(&broken.decimal_value,"1e0")==srl_ok,"Edit decimal");break;
case 1:require(srl_string_set_cstr(&broken.integer_value,"1.0")==srl_ok,"Edit integer");break;
case 2:require(srl_string_set_cstr(&broken.union_value,"unknown")==srl_ok,"Edit union");break;
case 3:require(srl_string_set_cstr(&broken.list_value,"2 1")==srl_ok,"Edit list");break;
case 4:require(srl_string_set_cstr(&broken.fixed_text,"bad")==srl_ok,"Edit fixed");break;
case 5:broken.typed.size=2u;break;default:require(srl_string_set_cstr(&broken.sign,"0")==srl_ok,"Edit sign");}
const srl_buffer before=again;require(TYPE_encode_xml(&broken,&again,NULL)!=srl_ok&&again.data==before.data&&again.size==before.size,"Edited output transaction");TYPE_free(&broken);}'''
        elif mode == 'scalar' and name in ('words', 'fixed_list'):
            checks = '''require(value.value.size==2u,"Typed text extent");--value.value.size;require(TYPE_encode_xml(&value,&again,NULL)!=srl_ok,"Edited list extent");++value.value.size;'''
        elif mode == 'scalar' and name == 'plain_default':
            checks = '''require(srl_string_set_cstr(&value.value,"")==srl_ok,"Empty native value");require(TYPE_encode_xml(&value,&again,NULL)!=srl_ok,"Unrepresentable empty default");'''
        elif mode == 'numeric':
            checks = '''require(value.integer>=UINT64_MAX-2u&&value.integer<UINT64_MAX&&value.choice==UINT64_MAX&&value.rounded==0.1f,"Exact native widths");
for (unsigned index=0;index<7u;++index){TYPE broken={0};require(TYPE_init(&broken)==srl_ok&&TYPE_decode_xml(&broken,output.data,output.size,NULL)==srl_ok,"Edited facet setup");
switch(index){case 0:broken.integer=UINT64_MAX;break;case 1:broken.amount=-1;break;case 2:broken.rounded=0.11f;break;case 3:require(srl_string_set_cstr(&broken.text,"bad")==srl_ok,"Edit text");break;case 4:broken.choice=UINT64_MAX-1u;break;case 5:broken.enabled=false;break;default:require(srl_string_set_cstr(&broken.code,"bad")==srl_ok,"Edit attr");}
require(TYPE_encode_xml(&broken,&again,NULL)!=srl_ok,"Edited facet rejected");TYPE_free(&broken);}'''
        functions.append('''/* Check one complete initialized native scalar owner. */
static void run_TYPE(const uint8_t* bytes,size_t size,const char* path,bool valid,const srl_limits* limits){
TYPE value={0},copy={0};require(TYPE_init(&value)==srl_ok&&TYPE_init(&copy)==srl_ok,"Initialize scalars");
const TYPE original=value;const srl_status status=TYPE_decode_xml(&value,bytes,size,limits);
if(!valid){require(status!=srl_ok,path);require(memcmp(&value,&original,sizeof(value))==0,"Invalid scalar candidate published");TYPE_free(&value);TYPE_free(&copy);return;}
require(status==srl_ok,path);srl_buffer output={0},again={0};require(TYPE_encode_xml(&value,&output,limits)==srl_ok,"Scalar output");
require(TYPE_decode_xml(&copy,output.data,output.size,limits)==srl_ok,"Scalar direct roundtrip");
const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){srl_buffer first={0},second={0};require(TYPE_encode(&value,protocols[index],&first)==srl_ok&&TYPE_encode(&copy,protocols[index],&second)==srl_ok,"Native scalar controls");require(first.size==second.size&&memcmp(first.data,second.data,first.size)==0,"Native scalar identity");srl_buffer_free(&first);srl_buffer_free(&second);}
write_file(path,&output);CHECKS
srl_buffer_free(&output);srl_buffer_free(&again);TYPE_free(&value);TYPE_free(&copy);}
'''.replace('TYPE', target).replace('CHECKS', checks.replace('TYPE', target)))
        condition = 'true' if len(names) == 1 else f'strncmp(filename,"{name}-",sizeof("{name}-")-1u)==0'
        dispatch.append(f'if({condition}){{run_{target}(bytes,size,path,valid,&limits);}}')
    return '''#include "schema.h"
/* Fail at the first independently specified native contract mismatch. */
static void require(bool good,const char* message){if(!good){fprintf(stderr,"%s\\n",message);exit(EXIT_FAILURE);}}
/* Keep exact UTF8 bytes and a private sentinel for diagnostic dispatch. */
static uint8_t* read_file(const char* path,size_t* size){FILE* file=fopen(path,"rb");require(file!=NULL,"Open scalar");require(fseek(file,0,SEEK_END)==0,"Seek scalar");long length=ftell(file);require(length>=0&&fseek(file,0,SEEK_SET)==0,"Scalar length");*size=(size_t)length;uint8_t* bytes=(uint8_t*)malloc(*size+1u);require(bytes!=NULL,"Scalar allocation");require(fread(bytes,1,*size,file)==*size,"Scalar bytes");fclose(file);bytes[*size]=0;return bytes;}
/* Publish only successfully checked complete output for the separate XSD oracle. */
static void write_file(const char* path,const srl_buffer* bytes){size_t count=strlen(path)+sizeof(".c.xml");char* target=(char*)malloc(count);require(target!=NULL,"Output path");require(snprintf(target,count,"%s.c.xml",path)>=0,"Output name");FILE* file=fopen(target,"wb");free(target);require(file!=NULL,"Output open");require(fwrite(bytes->data,1,bytes->size,file)==bytes->size,"Output bytes");fclose(file);}
FUNCTIONS
/* Select a statically typed consumer by the independently authored fixture name. */
int main(int argc,char** argv){for(int argument=1;argument<argc;++argument){const char* path=argv[argument];const char* filename=strrchr(path,'/');const char* windows=strrchr(path,'\\\\');if(windows&&(!filename||windows>filename)){filename=windows;}filename=filename?filename+1:path;
const bool valid=strstr(filename,".valid.xml")!=NULL;size_t size=0;uint8_t* bytes=read_file(path,&size);srl_limits limits=srl_default_limits();
if(strncmp(filename,"items-limit",sizeof("items-limit")-1u)==0){limits.max_elements=16u;}if(strncmp(filename,"items-text",sizeof("items-text")-1u)==0){limits.max_string_bytes=18u;}
DISPATCH else {require(false,"Unknown scalar fixture");}free(bytes);}puts("C exact scalar/list/union/default/pattern/native-codec checks pass");return EXIT_SUCCESS;}
'''.replace('FUNCTIONS', ''.join(functions)).replace('DISPATCH', ' else '.join(dispatch))


def main():
    """Run the real compiler, strict native SDKs and two independent scalar/pattern oracles."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True); parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='rust,c'); parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default=''); parser.add_argument('--modes', default='scalar,numeric,pattern,large')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('rust', 'c') for value in languages):
        parser.error('Choose distinct languages from rust,c')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath ignored out/')
    if args.libxml2_prefix: args.libxml2_prefix = args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home')); os.environ['RUSTFLAGS'] = '-D warnings'
    total = 0
    for mode in args.modes.split(','):
        if mode not in ('scalar', 'numeric', 'pattern', 'large'): parser.error('Unknown scalar mode')
        directory = args.build / mode; directory.mkdir(parents=True, exist_ok=True)
        if mode == 'scalar':
            xsd, model, probes = scalars.XSD, scalars.MODEL, scalars.cases(); oracle = scalars.oracle_schema()
            validate = lambda data: scalars.independent(data, oracle)
        else:
            xsd, model, probes = facets.numeric_cases(directory) if mode == 'numeric' else facets.pattern_cases(mode == 'large')
            oracle = etree.XMLSchema(etree.fromstring(xsd.encode())); validate = lambda data: oracle.validate(etree.fromstring(data))
        paths = []
        for index, probe in enumerate(probes):
            if mode == 'scalar':
                kind, text, valid = probe; name, document = f'{kind}-{index}', text.encode()
                skip_oracle = kind in ('guard-work', 'items-limit', 'items-text')
            else:
                name, document, _, valid = probe; skip_oracle = name in ('work', 'bytes')
            if not skip_oracle and validate(document) != valid: raise AssertionError(f'Independent oracle differs: {mode}/{name}')
            path = directory / f'{name}.{"valid" if valid else "invalid"}.xml'; path.write_bytes(document); paths.append(path)
        (directory / ('scalars.xsd' if mode == 'scalar' else 'sample.xsd')).write_text(xsd, encoding='utf-8')
        model_path = directory / 'model.serializer'; model_path.write_text(model, encoding='utf-8')
        command = [args.compiler, '--input', model_path, '--language', ','.join(languages)]
        for language in languages:
            target = directory / language; target.mkdir(parents=True, exist_ok=True)
            command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
        sdk.run(command); originals = {language: (directory / language / sdk.OUTPUTS[language]).read_bytes() for language in languages}
        for language in languages:
            source = directory / 'source' / language; source.mkdir(parents=True, exist_ok=True)
            (source / ('main.rs' if language == 'rust' else 'main.c')).write_text(rust_consumer(mode) if language == 'rust' else c_consumer(mode), encoding='utf-8')
            if language == 'rust':
                for name in ('Cargo.toml', 'Cargo.lock'): shutil.copyfile(ROOT / 'example/xml/rust' / name, source / name)
            run_args = argparse.Namespace(**vars(args)); run_args.build = directory
            runner = sdk.Runner(run_args)
            for path in paths: Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            launch = xml_runner.build_language(run_args, runner, language, directory / language, source_root=source.parent)
            sdk.run([*launch, *(runner.path(language, path) for path in paths)])
            for path in paths:
                emitted = Path(str(path) + f'.{language}.xml')
                if path.name.endswith('.valid.xml'):
                    if not validate(emitted.read_bytes()): raise AssertionError(f'Independent output differs: {emitted}: {oracle.error_log}')
                elif emitted.exists(): raise AssertionError(f'Invalid scalar output published: {emitted}')
        if mode == 'pattern':
            (directory / 'sample.xsd').write_text(xsd.replace('A[😀-🙏]{1,2}|B[0-9]+|^$|C[^0-9]{1,2}|D(a?)*', r'\p{L}+'), encoding='utf-8')
            rejected = subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=60)
            if rejected.returncode == 0 or 'approved portable XSD regex subset' not in rejected.stdout + rejected.stderr:
                raise AssertionError('Unsupported native pattern requires its portable-subset diagnostic')
            if any((directory / language / sdk.OUTPUTS[language]).read_bytes() != originals[language] for language in languages):
                raise AssertionError('Rejected pattern replaced generated source')
        total += len(paths)
    scalars.rejected_bindings(args, languages)
    print(f'PASS: {total} independent exact scalar/list/union/default/pattern probes and 6 atomic binding gates across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
