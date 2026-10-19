# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated C/Rust streaming owner-local identities with independent XSD and native controls."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil

from lxml import etree
import portable_xml_identity_test as fixture
import native_xml_scalar_test as native_helpers

ROOT, sdk = fixture.ROOT, fixture.sdk
spec = importlib.util.spec_from_file_location('serializer_native_identity_sdk', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec); spec.loader.exec_module(xml_runner)

OWNERS = [('scalar-nullable-keys', 'NullableKeys', 'nullable_keys'), ('scalar-key-nil', 'NilKeyed', 'nil_keyed'),
          ('scalar-key', 'Keyed', 'keyed'), ('scalar-unique', 'UniqueValue', 'unique_value'),
          ('owners', 'Registries', 'registries'), ('', 'Registry', 'registry')]


def rust_consumer():
    """Check complete native values and output edits without optional XSD callbacks."""
    branches = []
    for prefix, rust_name, _ in OWNERS:
        target = 'schema::IdentityXml' + rust_name
        checks = ''
        if not prefix:
            checks = '''if filename.starts_with("forward") {
let mut edited=TYPE::decode_xml(&output)?;edited.requests[0].id=Some(8);assert!(edited.encode_xml().is_err());
edited.requests[0].id=Some(7);edited.entries.push(edited.entries[0].clone());assert!(edited.encode_xml().is_err());
edited.entries.pop();edited.entries[0].id=None;assert!(edited.encode_xml().is_err());}'''
        elif prefix == 'scalar-nullable-keys':
            checks = '''let mut edited=TYPE::decode_xml(&output)?;edited.items=Some(Some("".to_owned()));assert!(edited.encode_xml().is_err());'''
        condition = f'filename.starts_with("{prefix}")' if prefix else 'true'
        branches.append('''if CONDITION {(||->Result<String,schema::Error>{
let value=TYPE::decode_xml_with_identity_limits(&text,schema::XmlLimits::default(),None,limits)?;
let output=value.encode_xml_with_identity_limits(schema::XmlLimits::default(),None,limits)?;
let copy=TYPE::decode_xml(&output)?;
for protocol in [schema::Protocol::Json,schema::Protocol::BinaryNone,schema::Protocol::BinaryInteger,schema::Protocol::BinaryString]{assert_eq!(value.encode(protocol)?,copy.encode(protocol)?);}
CHECKS
Ok(output)})()}'''.replace('CONDITION', condition).replace('TYPE', target).replace('CHECKS', checks.replace('TYPE', target)))
    return '''mod schema;
use std::{env,fs,path::Path};
/// Compare all independent identity/admission decisions before publishing complete SDK output.
fn main()->Result<(),Box<dyn std::error::Error>>{
let arguments=env::args().skip(1).collect::<Vec<_>>();assert_eq!(arguments.len()%8,0);
for arguments in arguments.chunks_exact(8){let path=&arguments[0];let text=fs::read_to_string(path)?;let filename=Path::new(path).file_name().unwrap().to_str().unwrap();let valid=path.ends_with(".valid.xml");
let limits=schema::XmlIdentityLimits{max_constraints:arguments[1].parse()?,max_fields:arguments[2].parse()?,max_depth:arguments[3].parse()?,max_selected_nodes:arguments[4].parse()?,max_tuples:arguments[5].parse()?,max_tuple_bytes:arguments[6].parse()?,max_work_units:arguments[7].parse()?};
let result=BRANCHES else {unreachable!()};match result{Ok(output)=>{assert!(valid,"Invalid identity published: {path}");fs::write(format!("{path}.rust.xml"),output)?;},Err(error)=>{if valid{return Err(Box::new(error));}}}}
println!("Rust bounded owner-local key/unique/keyref and native-control probes pass");Ok(())}
'''.replace('BRANCHES', ' else '.join(branches))


def c_consumer():
    """Check C destination/output transactionality and four native wire identities."""
    preamble = native_helpers.c_consumer('numeric').split('/* Check one complete initialized native scalar owner. */', 1)[0]
    functions, dispatch = [], []
    for prefix, _, c_name in OWNERS:
        target = 'identity_xml_' + c_name
        checks = ''
        if not prefix:
            checks = '''if(strncmp(filename,"forward",7u)==0){
value.requests.data[0].id.value=8u;const srl_buffer previous=output;
require(TYPE_encode_xml(&value,&output,NULL)!=srl_ok&&output.data==previous.data&&output.size==previous.size,"Invalid identity output transaction");
value.requests.data[0].id.value=7u;value.entries.data[0].id.has_value=false;
require(TYPE_encode_xml(&value,&output,NULL)!=srl_ok,"Missing edited key rejected");value.entries.data[0].id.has_value=true;
}'''
        elif prefix == 'scalar-nullable-keys':
            checks = '''value.items.has_value=true;value.items.value.has_value=true;require(srl_string_set_cstr(&value.items.value.value,"")==srl_ok,"Initialize edited nullable key");require(TYPE_encode_xml(&value,&again,NULL)!=srl_ok,"Nillable declaration key rejected");'''
        functions.append('''/* Validate one owning C identity scope before publishing any native candidate or output. */
static void check_TYPE(const uint8_t* bytes,size_t size,const char* path,const char* filename,bool valid,const srl_xml_identity_limits* limits){
TYPE value={0},copy={0};require(TYPE_init(&value)==srl_ok&&TYPE_init(&copy)==srl_ok,"Initialize identities");const TYPE original=value;
const srl_status status=TYPE_decode_xml_with_identity_limits(&value,bytes,size,NULL,limits);
if(!valid){require(status!=srl_ok,path);require(memcmp(&original,&value,sizeof(value))==0,"Invalid identity input changed destination");TYPE_free(&value);TYPE_free(&copy);return;}
require(status==srl_ok,path);srl_buffer output={0},again={0};require(TYPE_encode_xml_with_identity_limits(&value,&output,NULL,limits)==srl_ok,"Identity output");
require(TYPE_decode_xml(&copy,output.data,output.size,NULL)==srl_ok,"Identity direct roundtrip");
const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
for(size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index){srl_buffer first={0},second={0};require(TYPE_encode(&value,protocols[index],&first)==srl_ok&&TYPE_encode(&copy,protocols[index],&second)==srl_ok,"Native identity controls");require(first.size==second.size&&memcmp(first.data,second.data,first.size)==0,"Identity native wire bytes");srl_buffer_free(&first);srl_buffer_free(&second);}
write_file(path,&output);(void)filename;CHECKS
srl_buffer_free(&output);srl_buffer_free(&again);TYPE_free(&value);TYPE_free(&copy);}
'''.replace('TYPE', target).replace('CHECKS', checks.replace('TYPE', target)))
        condition = f'strncmp(filename,"{prefix}",sizeof("{prefix}")-1u)==0' if prefix else 'true'
        dispatch.append(f'if({condition}){{check_{target}(bytes,size,path,filename,valid,&limits);}}')
    return preamble + ''.join(functions) + '''
/* Exercise exact policy endpoints through the initialized public C owner entrypoints. */
int main(int argc,char** argv){require((argc-1)%8==0,"Identity argument inventory");for(int argument=1;argument<argc;argument+=8){
const char* path=argv[argument];const char* filename=strrchr(path,'/');const char* windows=strrchr(path,'\\\\');if(windows&&(!filename||windows>filename)){filename=windows;}filename=filename?filename+1:path;
const bool valid=strstr(filename,".valid.xml")!=NULL;size_t size=0;uint8_t* bytes=read_file(path,&size);
const srl_xml_identity_limits limits={(size_t)strtoull(argv[argument+1],NULL,10),(size_t)strtoull(argv[argument+2],NULL,10),(size_t)strtoull(argv[argument+3],NULL,10),(size_t)strtoull(argv[argument+4],NULL,10),(size_t)strtoull(argv[argument+5],NULL,10),(size_t)strtoull(argv[argument+6],NULL,10),(size_t)strtoull(argv[argument+7],NULL,10)};
DISPATCH else{require(false,"Unknown identity fixture");}free(bytes);}puts("C bounded owner-local key/unique/keyref and native-control probes pass");return EXIT_SUCCESS;}
'''.replace('DISPATCH', ' else '.join(dispatch))


def main():
    """Generate real owning bindings and compare all 45 probes with raw JDK/lxml XSD oracles."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True); parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='rust,c'); parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default=''); parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('rust', 'c') for value in languages): parser.error('Choose distinct languages from rust,c')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts: parser.error('Repository scratch must be beneath ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    if args.libxml2_prefix: args.libxml2_prefix = args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home')); os.environ['RUSTFLAGS'] = '-D warnings'
    xsd = args.build / 'identity.xsd'; xsd.write_text(fixture.XSD, encoding='utf-8'); oracle = etree.XMLSchema(etree.parse(str(xsd)))
    probes = fixture.cases(); paths = []
    for label, document, limits, valid, _ in probes:
        path = args.build / (label + ('.valid.xml' if valid else '.invalid.xml')); path.write_text(document, encoding='utf-8'); paths.append((path, limits))
    provider = args.build / 'oracle'; provider.mkdir(exist_ok=True); (provider / 'Oracle.java').write_text(fixture.ORACLE, encoding='utf-8')
    sdk.run(['javac', '--release', '17', '-Xlint:all', '-Werror', provider / 'Oracle.java'])
    arguments = []
    for (path, _), (_, _, _, _, valid) in zip(paths, probes): arguments += [path, str(valid).lower()]
    sdk.run(['java', '-cp', provider, 'Oracle', xsd, *arguments])
    model = args.build / 'model.serializer'; model.write_text(fixture.MODEL, encoding='utf-8')
    config = args.build / 'identity.ini'; config.write_text(fixture.OPTIONS, encoding='utf-8')
    command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages)]
    for language in languages:
        target = args.build / language; target.mkdir(exist_ok=True); command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        (source / ('main.rs' if language == 'rust' else 'main.c')).write_text(rust_consumer() if language == 'rust' else c_consumer(), encoding='utf-8')
        if language == 'rust':
            for name in ('Cargo.toml', 'Cargo.lock'): shutil.copyfile(ROOT / 'example/xml/rust' / name, source / name)
        for path, _ in paths: Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        launch = xml_runner.build_language(args, runner, language, args.build / language, source_root=source.parent); arguments = []
        for path, limits in paths: arguments += [runner.path(language, path), *limits]
        sdk.run([*launch, *arguments]); emitted_arguments = []
        for path, _ in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                valid = oracle.validate(etree.parse(str(emitted)))
                if not valid and not (path.name.startswith('scalar-unique-nil') and 'No precomputed value' in str(oracle.error_log)): raise AssertionError(str(oracle.error_log))
                emitted_arguments += [emitted, 'true']
            elif emitted.exists(): raise AssertionError('Invalid identity output published: ' + str(emitted))
        sdk.run(['java', '-cp', provider, 'Oracle', xsd, *emitted_arguments])
    fixture.rejected_bindings(args, languages)
    print(f'PASS: {len(paths)} independent XSD/admission identity probes +5 atomic binding fences across {len(languages)} native SDKs', flush=True)


if __name__ == '__main__':
    main()
