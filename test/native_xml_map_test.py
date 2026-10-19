"""Qualify generated C/Rust typed maps against the shared independent 33-case oracle."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil

from lxml import etree
import go_xml_map_test as fixture

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_native_map_sdk', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)
sdk = xml_runner.sdk

RUST = r'''mod schema;
use schema::{MapsSample, MapsHolder, XmlLimits, XmlMapLimits, Protocol};
use std::{env,fs};
/// Compare direct typed maps, native wire values and independent nested resource policies.
fn main() -> Result<(),Box<dyn std::error::Error>> {
    let exact=XmlMapLimits {max_entries:2,max_key_bytes:20};
    for argument in env::args().skip(1) {
        let text=fs::read_to_string(&argument)?; let valid=argument.ends_with(".valid.xml");
        let value=match MapsSample::decode_xml(&text) {
            Ok(value)=>{ assert!(valid,"Invalid map accepted: {}",argument); value },
            Err(error)=>{ if valid { return Err(Box::new(error)); } else { continue; } }
        };
        assert_eq!(value.integers.len(),2); assert_eq!(value.integers[&u32::MAX],"max");
        assert_eq!(value.integers[&1],"one"); assert_eq!(value.booleans[&true],i16::MIN);
        assert_eq!(value.booleans[&false],i16::MAX); assert_eq!(value.signed_values[&i8::MIN],"negative");
        let key=if text.contains("<key> </key>") {" "} else {"A&B"};
        assert_eq!(value.records[key].code,u16::MAX); assert_eq!(value.records[key].label,"nested");
        assert_eq!(value.optional_values.is_some(),text.contains("optional_values"));
        let encoded=value.encode_xml()?; let copy=MapsSample::decode_xml(&encoded)?;
        assert_eq!(encoded,copy.encode_xml()?);
        for protocol in [Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString,Protocol::Json] {
            assert_eq!(value.encode(protocol)?,copy.encode(protocol)?);
        }
        fs::write(format!("{}.rust.xml",argument),&encoded)?;
        value.encode_xml_with_map_limits(XmlLimits::default(),None,exact)?;
        MapsSample::decode_xml_with_map_limits(&text,XmlLimits::default(),None,exact)?;
        let holder=MapsHolder {value,..MapsHolder::default()};
        let nested=holder.encode_xml_with_map_limits(XmlLimits::default(),None,exact)?;
        MapsHolder::decode_xml_with_map_limits(&nested,XmlLimits::default(),None,exact)?;
        for bound in [XmlMapLimits{max_entries:1,max_key_bytes:20},XmlMapLimits{max_entries:2,max_key_bytes:9},XmlMapLimits{max_entries:0,max_key_bytes:0}] {
            assert!(holder.encode_xml_with_map_limits(XmlLimits::default(),None,bound).is_err());
            assert!(MapsSample::decode_xml_with_map_limits(&text,XmlLimits::default(),None,bound).is_err());
            assert!(MapsHolder::decode_xml_with_map_limits(&nested,XmlLimits::default(),None,bound).is_err());
        }
        let lexical=text.replace("id=\"00001\"","id=\"0000000000000000000000000000000000001\"");
        if lexical != text { assert!(MapsSample::decode_xml_with_map_limits(&lexical,XmlLimits::default(),None,exact).is_err()); }
        let mut empty=holder.value;
        empty.integers.clear(); empty.booleans.clear(); empty.records.clear(); empty.enumerated.clear(); empty.signed_values.clear();
        empty.optional_values=Some(std::collections::BTreeMap::new());
        let zero=XmlMapLimits{max_entries:0,max_key_bytes:0};
        let empty_bytes=empty.encode_xml_with_map_limits(XmlLimits::default(),None,zero)?;
        let empty_copy=MapsSample::decode_xml_with_map_limits(&empty_bytes,XmlLimits::default(),None,zero)?;
        assert!(empty_copy.optional_values.as_ref().is_some_and(|map| map.is_empty()));
        fs::write(format!("{}.empty.rust.xml",argument),empty_bytes)?;
        for key in 0..1025 { empty.integers.insert(key,"x".to_owned()); }
        assert!(empty.encode_xml().is_err());
        let expanded=XmlMapLimits{max_entries:1025,max_key_bytes:1<<20};
        let many=empty.encode_xml_with_map_limits(XmlLimits::default(),None,expanded)?;
        assert!(MapsSample::decode_xml(&many).is_err());
        MapsSample::decode_xml_with_map_limits(&many,XmlLimits::default(),None,expanded)?;
        empty.integers.clear(); empty.records.insert("x".repeat((1<<20)+1),schema::MapsChild::default());
        assert!(empty.encode_xml().is_err());
        let expanded=XmlMapLimits{max_entries:2,max_key_bytes:(1<<20)+1};
        let large=empty.encode_xml_with_map_limits(XmlLimits::default(),None,expanded)?;
        assert!(MapsSample::decode_xml(&large).is_err());
        MapsSample::decode_xml_with_map_limits(&large,XmlLimits::default(),None,expanded)?;
    }
    println!("Rust typed map aliases, optional wrappers, canonical identities and per-wrapper limits pass"); Ok(())
}
'''

C = r'''#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Abort on an SDK/oracle disagreement, retaining the original transactional destination. */
static void require(bool condition,const char* message) {
  if (!condition) { fprintf(stderr,"%s\n",message); exit(EXIT_FAILURE); }
}
/* Read exact binary XML bytes without an implicit character encoding conversion. */
static uint8_t* read_file(const char* path,size_t* size) {
  FILE* file=fopen(path,"rb"); require(file != NULL,"Open map fixture");
  require(fseek(file,0,SEEK_END) == 0,"Seek map fixture"); const long length=ftell(file);
  require(length >= 0 && fseek(file,0,SEEK_SET) == 0,"Measure map fixture");
  *size=(size_t)length; uint8_t* bytes=(uint8_t*)malloc(*size+1u); require(bytes != NULL,"Read map allocation");
  require(fread(bytes,1,*size,file) == *size,"Read map bytes"); fclose(file); bytes[*size]=0; return bytes;
}
/* Publish a successful SDK output for independent schema validation. */
static void write_file(const char* path,const char* suffix,const srl_buffer* bytes) {
  const size_t size=strlen(path)+strlen(suffix)+1u; char* output=(char*)malloc(size); require(output != NULL,"Output path allocation");
  require(snprintf(output,size,"%s%s",path,suffix) >= 0,"Map output path"); FILE* file=fopen(output,"wb"); free(output);
  require(file != NULL,"Open map output"); require(fwrite(bytes->data,1,bytes->size,file) == bytes->size,"Write map output"); fclose(file);
}
/* Compare typed aliases, native wire identities and independent per-wrapper map policies. */
int main(int argc,char** argv) {
  const srl_xml_map_limits exact={2u,20u};
  for (int argument=1; argument < argc; ++argument) {
    const char* path=argv[argument]; const bool valid=strstr(path,".valid.xml") != NULL;
    size_t size=0; uint8_t* bytes=read_file(path,&size); maps_sample value={0},copy={0};
    require(maps_sample_init(&value) == srl_ok && maps_sample_init(&copy) == srl_ok,"Initialize maps");
    const maps_sample original=value; const srl_status status=maps_sample_decode_xml(&value,bytes,size,NULL);
    if (!valid) { require(status != srl_ok,path); require(memcmp(&value,&original,sizeof(value)) == 0,"Invalid map published candidate"); free(bytes); maps_sample_free(&value); maps_sample_free(&copy); continue; }
    require(status == srl_ok,path);
    require(value.integers.size == 2u && value.integers.data[0].key == 1u && value.integers.data[1].key == UINT32_MAX,"Exact unsigned map keys");
    require(srl_string_equal(&value.integers.data[0].value,"one") && srl_string_equal(&value.integers.data[1].value,"max"),"Exact map values");
    require(value.booleans.size == 2u && !value.booleans.data[0].key && value.booleans.data[0].value == INT16_MAX && value.booleans.data[1].key && value.booleans.data[1].value == INT16_MIN,"Boolean canonical map keys");
    require(value.records.size == 2u && value.records.data[1].value.code == UINT16_MAX && srl_string_equal(&value.records.data[1].value.label,"nested"),"Nested map record");
    require(value.optional_values.has_value == (strstr((char*)bytes,"optional_values") != NULL),"Optional map presence");
    require(value.signed_values.size == 2u && value.signed_values.data[0].key == INT8_MIN,"Signed map width");
    srl_buffer encoded={0},again={0}; require(maps_sample_encode_xml(&value,&encoded,NULL) == srl_ok,"Map output");
    require(maps_sample_decode_xml(&copy,encoded.data,encoded.size,NULL) == srl_ok && maps_sample_encode_xml(&copy,&again,NULL) == srl_ok,"Map direct roundtrip");
    require(encoded.size == again.size && memcmp(encoded.data,again.data,encoded.size) == 0,"Deterministic map output");
    const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
    for (size_t index=0; index < sizeof(protocols)/sizeof(protocols[0]); ++index) {
      srl_buffer first={0},second={0}; require(maps_sample_encode(&value,protocols[index],&first) == srl_ok && maps_sample_encode(&copy,protocols[index],&second) == srl_ok,"Native map controls");
      require(first.size == second.size && memcmp(first.data,second.data,first.size) == 0,"Native map identity"); srl_buffer_free(&first); srl_buffer_free(&second);
    }
    write_file(path,".c.xml",&encoded);
    require(maps_sample_encode_xml_with_map_limits(&value,&again,NULL,&exact) == srl_ok && maps_sample_decode_xml_with_map_limits(&copy,bytes,size,NULL,&exact) == srl_ok,"Exact map resource bounds");
    maps_holder holder={0},nested_copy={0}; require(maps_holder_init(&holder) == srl_ok && maps_holder_init(&nested_copy) == srl_ok,"Initialize nested maps");
    maps_sample_free(&holder.value); holder.value=value; memset(&value,0,sizeof(value));
    srl_buffer nested={0}; require(maps_holder_encode_xml_with_map_limits(&holder,&nested,NULL,&exact) == srl_ok && maps_holder_decode_xml_with_map_limits(&nested_copy,nested.data,nested.size,NULL,&exact) == srl_ok,"Nested map bounds");
    const srl_xml_map_limits rejected[]={{1u,20u},{2u,9u},{0u,0u}};
    for (size_t index=0; index < sizeof(rejected)/sizeof(rejected[0]); ++index) {
      const maps_sample previous=copy; require(maps_holder_encode_xml_with_map_limits(&holder,&again,NULL,&rejected[index]) != srl_ok,"Map output bound");
      require(maps_sample_decode_xml_with_map_limits(&copy,bytes,size,NULL,&rejected[index]) != srl_ok && memcmp(&copy,&previous,sizeof(copy)) == 0,"Transactional map input bound");
      require(maps_holder_decode_xml_with_map_limits(&nested_copy,nested.data,nested.size,NULL,&rejected[index]) != srl_ok,"Nested input map bound");
    }
    const char* alias=strstr((char*)bytes,"id=\"00001\"");
    if (alias) {
      const char replacement[]="id=\"0000000000000000000000000000000000001\""; const size_t before=(size_t)(alias-(char*)bytes);
      const size_t original_size=sizeof("id=\"00001\"")-1u; const size_t replacement_size=sizeof(replacement)-1u;
      const size_t expanded_size=size-original_size+replacement_size; uint8_t* expanded_bytes=(uint8_t*)malloc(expanded_size);
      require(expanded_bytes != NULL,"Lexical alias allocation"); memcpy(expanded_bytes,bytes,before); memcpy(expanded_bytes+before,replacement,replacement_size);
      memcpy(expanded_bytes+before+replacement_size,bytes+before+original_size,size-before-original_size);
      require(maps_sample_decode_xml_with_map_limits(&copy,expanded_bytes,expanded_size,NULL,&exact) != srl_ok,"Original lexical map key byte bound"); free(expanded_bytes);
    }
    if (copy.integers.capacity == copy.integers.size) {
      const size_t capacity=copy.integers.size+1u; void* allocation=realloc(copy.integers.data,capacity*sizeof(copy.integers.data[0])); require(allocation != NULL,"Edited map allocation");
      copy.integers.data=(maps_sample_integers_map_entry*)allocation; memset(&copy.integers.data[copy.integers.size],0,sizeof(copy.integers.data[0])); copy.integers.capacity=capacity;
    }
    copy.integers.data[copy.integers.size].key=copy.integers.data[0].key; ++copy.integers.size;
    require(maps_sample_encode_xml(&copy,&again,NULL) != srl_ok,"Duplicate edited native map output"); --copy.integers.size;
    maps_sample_free(&copy); require(maps_sample_init(&copy) == srl_ok,"Reset empty maps"); copy.optional_values.has_value=true;
    const srl_xml_map_limits zero={0u,0u}; require(maps_sample_encode_xml_with_map_limits(&copy,&again,NULL,&zero) == srl_ok,"Empty zero-bound maps");
    require(maps_sample_decode_xml_with_map_limits(&value,again.data,again.size,NULL,&zero) == srl_ok && value.optional_values.has_value && value.optional_values.value.size == 0u,"Empty optional wrapper");
    write_file(path,".empty.c.xml",&again);
    copy.integers.data=(maps_sample_integers_map_entry*)calloc(1025u,sizeof(*copy.integers.data)); require(copy.integers.data != NULL,"Large map allocation"); copy.integers.size=1025u; copy.integers.capacity=1025u;
    for (size_t index=0; index < copy.integers.size; ++index) { copy.integers.data[index].key=(uint32_t)index; }
    require(maps_sample_encode_xml(&copy,&again,NULL) != srl_ok,"Default map entry output bound");
    const srl_xml_map_limits expanded={1025u,1024u*1024u}; require(maps_sample_encode_xml_with_map_limits(&copy,&again,NULL,&expanded) == srl_ok,"Explicit expanded map limit");
    require(maps_sample_decode_xml(&value,again.data,again.size,NULL) != srl_ok && maps_sample_decode_xml_with_map_limits(&value,again.data,again.size,NULL,&expanded) == srl_ok,"Default map entry input bound");
    maps_sample_integers_map_clear(&copy.integers);
    copy.records.data=(maps_sample_records_map_entry*)calloc(1u,sizeof(*copy.records.data)); require(copy.records.data != NULL,"Large key map allocation"); copy.records.size=1u; copy.records.capacity=1u;
    const size_t large_key_bytes=1024u*1024u+1u; char* large_key=(char*)malloc(large_key_bytes); require(large_key != NULL,"Large key allocation"); memset(large_key,'x',large_key_bytes);
    require(srl_string_set(&copy.records.data[0].key,large_key,large_key_bytes) == srl_ok && maps_child_init(&copy.records.data[0].value) == srl_ok,"Large key owning fields"); free(large_key);
    require(maps_sample_encode_xml(&copy,&again,NULL) != srl_ok,"Default key byte output bound");
    const srl_xml_map_limits large_bound={2u,large_key_bytes}; require(maps_sample_encode_xml_with_map_limits(&copy,&again,NULL,&large_bound) == srl_ok,"Expanded key byte policy");
    require(maps_sample_decode_xml(&value,again.data,again.size,NULL) != srl_ok && maps_sample_decode_xml_with_map_limits(&value,again.data,again.size,NULL,&large_bound) == srl_ok,"Default key byte input bound");
    free(bytes); srl_buffer_free(&encoded); srl_buffer_free(&again); srl_buffer_free(&nested); maps_sample_free(&value); maps_sample_free(&copy); maps_holder_free(&holder); maps_holder_free(&nested_copy);
  }
  puts("C typed map aliases, optional wrappers, canonical identities and nested transactional limits pass"); return EXIT_SUCCESS;
}
'''


def main():
    """Generate owning models, run actual SDK consumers and validate all successful outputs."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='rust,c')
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if len(languages) != len(set(languages)) or any(value not in ('rust', 'c') for value in languages):
        parser.error('Choose distinct languages from rust,c')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home'))
    os.environ['RUSTFLAGS'] = '-D warnings'
    (args.build / 'model.serializer').write_text(fixture.MODEL, encoding='utf-8')
    (args.build / 'options.ini').write_text(fixture.CONFIG, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(fixture.xsd().encode()))
    paths = []
    for index, (document, valid) in enumerate(fixture.cases()):
        if oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent map decision differs for {index}: {oracle.error_log}')
        path = args.build / f'maps-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8'); paths.append(path)
    command = [args.compiler, '--input', args.build / 'model.serializer', '--config', args.build / 'options.ini',
               '--language', ','.join(languages)]
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language in languages:
        directory = args.build / language
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        (source / ('main.rs' if language == 'rust' else 'main.c')).write_text(RUST if language == 'rust' else C, encoding='utf-8')
        if language == 'rust':
            for filename in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(ROOT / 'example/xml/rust' / filename, source / filename)
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            Path(str(path) + f'.empty.{language}.xml').unlink(missing_ok=True)
        launch = xml_runner.build_language(args, runner, language, directory, source_root=source.parent)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            output = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(output)))
                oracle.assertValid(etree.parse(str(path) + f'.empty.{language}.xml'))
            elif output.exists():
                raise AssertionError('Invalid native map candidate published output')
    print(f'PASS: {len(paths)} independent typed map probes and resource boundaries across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
