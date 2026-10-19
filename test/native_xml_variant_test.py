"""Qualify C/Rust element and xsi:type variants with 161 shared independent schema probes."""
import argparse
import os
from pathlib import Path
import shutil

from lxml import etree
import native_xml_map_test as native
import portable_xml_variant_test as fixture
import portable_xml_typed_variant_test as typed

ROOT, xml_runner, sdk = native.ROOT, native.xml_runner, native.sdk

RUST = r'''mod schema;
use schema::*;
use std::{env,fs,path::Path};
/// Compare exact native alternatives and fail before publishing any malformed XML candidate.
fn main() -> Result<(),Box<dyn std::error::Error>> {
    for argument in env::args().skip(1) {
        let text=fs::read_to_string(&argument)?; let valid=argument.ends_with(".valid.xml");
        let value=match VariantsSample::decode_xml(&text) {
            Ok(value)=>{ assert!(valid,"Invalid variant accepted: {argument}"); value },
            Err(error)=>{ if valid { return Err(Box::new(error)); } else { continue; } }
        };
        let filename=Path::new(&argument).file_name().unwrap().to_str().unwrap().as_bytes();
        let selected=(filename[1]-b'0') as usize; let primitive=(filename[3]-b'0') as usize;
        assert_eq!(value.value_index,selected); assert_eq!(value.primitive_index,primitive);
        match selected {
            0=>assert_eq!(value.value_code,u32::MAX),
            1=>assert_eq!(value.value_text,"hi\u{1f642}&<>"),
            2=>{ assert_eq!(value.value_record.code,u16::MAX); assert_eq!(value.value_record.label,"nested"); },
            3=>assert_eq!(value.value_state,VariantsState::One), _=>panic!("Invalid independent selection")
        }
        if primitive == 0 { assert!(!value.primitive_flag); } else { assert_eq!(value.primitive_amount,f64::INFINITY); }
        let output=value.encode_xml()?; let copy=VariantsSample::decode_xml(&output)?; assert_eq!(output,copy.encode_xml()?);
        for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {
            if primitive == 1 && protocol == Protocol::Json { assert!(value.encode(protocol).is_err()); assert!(copy.encode(protocol).is_err()); }
            else { assert_eq!(value.encode(protocol)?,copy.encode(protocol)?); }
        }
        fs::write(format!("{argument}.rust.xml"),&output)?;
        let holder=VariantsHolder {value,..VariantsHolder::default()}; let nested=holder.encode_xml()?;
        VariantsHolder::decode_xml(&nested)?;
        let bounds=XmlLimits{max_nodes:1,..XmlLimits::default()};
        assert!(holder.encode_xml_with_validation(bounds,None).is_err());
        assert!(VariantsSample::decode_xml_with_validation(&text,bounds,None).is_err());
        assert!(VariantsHolder::decode_xml_with_validation(&nested,bounds,None).is_err());
        let mut broken=copy; broken.value_index=usize::MAX; assert!(broken.encode_xml().is_err());
        broken.value_index=4; assert!(broken.encode_xml().is_err());
        broken.value_index=selected; broken.primitive_index=2; assert!(broken.encode_xml().is_err());
    }
    println!("Rust exact native variant selection, QName scopes and complete-message failures pass"); Ok(())
}
'''

C = native.C.split('/* Compare typed aliases, native wire identities and independent per-wrapper map policies. */')[0] + r'''
/* Require exact typed alternatives while retaining destination bytes across all failed loads. */
int main(int argc,char** argv) {
  for (int argument=1; argument < argc; ++argument) {
    const char* path=argv[argument]; const bool valid=strstr(path,".valid.xml") != NULL;
    size_t size=0; uint8_t* bytes=read_file(path,&size); variants_sample value={0},copy={0};
    require(variants_sample_init(&value) == srl_ok && variants_sample_init(&copy) == srl_ok,"Initialize variants");
    const variants_sample original=value; const srl_status status=variants_sample_decode_xml(&value,bytes,size,NULL);
    if (!valid) { require(status != srl_ok,path); require(memcmp(&value,&original,sizeof(value)) == 0,"Invalid variant published candidate"); free(bytes); variants_sample_free(&value); variants_sample_free(&copy); continue; }
    require(status == srl_ok,path);
    const char* filename=strrchr(path,'\\'); if (!filename) { filename=strrchr(path,'/'); } filename=filename ? filename+1 : path;
    const uint32_t selected=(uint32_t)(filename[1]-'0'),primitive=(uint32_t)(filename[3]-'0');
    require(value.value_index == selected && value.primitive_index == primitive,"Exact variant discriminator");
    switch (selected) {
      case 0u: require(value.value_code == UINT32_MAX,"Exact unsigned variant"); break;
      case 1u: require(srl_string_equal(&value.value_text,"hi\xF0\x9F\x99\x82&<>"),"Exact UTF8 variant"); break;
      case 2u: require(value.value_record.code == UINT16_MAX && srl_string_equal(&value.value_record.label,"nested"),"Exact nested record variant"); break;
      case 3u: require((unsigned)value.value_state == 1u,"Exact enum variant"); break;
      default: require(false,"Invalid independent selection");
    }
    require(primitive == 0u ? !value.primitive_flag : isinf(value.primitive_amount) && value.primitive_amount > 0,"Exact floating alternative");
    srl_buffer output={0},again={0}; require(variants_sample_encode_xml(&value,&output,NULL) == srl_ok,"Variant output");
    require(variants_sample_decode_xml(&copy,output.data,output.size,NULL) == srl_ok && variants_sample_encode_xml(&copy,&again,NULL) == srl_ok,"Variant direct roundtrip");
    require(output.size == again.size && memcmp(output.data,again.data,output.size) == 0,"Stable variant output");
    const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
    for (size_t index=0; index < sizeof(protocols)/sizeof(protocols[0]); ++index) {
      srl_buffer first={0},second={0}; const srl_status one=variants_sample_encode(&value,protocols[index],&first),two=variants_sample_encode(&copy,protocols[index],&second);
      if (primitive == 1u && protocols[index] == srl_json) { require(one != srl_ok && two != srl_ok,"Native JSON nonfinite restriction"); }
      else { require(one == srl_ok && two == srl_ok && first.size == second.size && memcmp(first.data,second.data,first.size) == 0,"Native variant value identity"); }
      srl_buffer_free(&first); srl_buffer_free(&second);
    }
    write_file(path,".c.xml",&output);
    variants_holder holder={0},nested_copy={0}; require(variants_holder_init(&holder) == srl_ok && variants_holder_init(&nested_copy) == srl_ok,"Initialize nested variants");
    variants_sample_free(&holder.value); holder.value=value; memset(&value,0,sizeof(value));
    srl_buffer nested={0}; require(variants_holder_encode_xml(&holder,&nested,NULL) == srl_ok && variants_holder_decode_xml(&nested_copy,nested.data,nested.size,NULL) == srl_ok,"Nested typed variant");
    srl_limits bounds=srl_default_limits(); bounds.max_elements=1u;
    const variants_sample previous=copy;
    require(variants_holder_encode_xml(&holder,&again,&bounds) != srl_ok && variants_sample_decode_xml(&copy,bytes,size,&bounds) != srl_ok && memcmp(&copy,&previous,sizeof(copy)) == 0,"Bounded transactional variant");
    require(variants_holder_decode_xml(&nested_copy,nested.data,nested.size,&bounds) != srl_ok,"Nested variant input bound");
    copy.value_index=UINT32_MAX; require(variants_sample_encode_xml(&copy,&again,NULL) != srl_ok,"Unknown edited native discriminator");
    copy.value_index=4u; require(variants_sample_encode_xml(&copy,&again,NULL) != srl_ok,"Out of range native discriminator");
    copy.value_index=selected; copy.primitive_index=2u; require(variants_sample_encode_xml(&copy,&again,NULL) != srl_ok,"Unknown raw union discriminator");
    free(bytes); srl_buffer_free(&output); srl_buffer_free(&again); srl_buffer_free(&nested); variants_sample_free(&value); variants_sample_free(&copy); variants_holder_free(&holder); variants_holder_free(&nested_copy);
  }
  puts("C exact native variant selection, QName scopes and transactional failures pass"); return EXIT_SUCCESS;
}
'''


def main():
    """Compile real native consumers and validate every completed element/type namespace mode."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='rust,c')
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('rust', 'c') for value in languages):
        parser.error('Choose distinct languages from rust,c')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home'))
    os.environ['RUSTFLAGS'] = '-D warnings'
    runner = sdk.Runner(args); count = 0
    for mode in ['element', 'typed', 'typed-empty']:
        directory = args.build / mode; directory.mkdir(parents=True, exist_ok=True)
        empty = mode == 'typed-empty'; is_typed = mode != 'element'
        oracle = typed.prepare_oracle(directory, empty) if is_typed else etree.XMLSchema(etree.fromstring(fixture.XSD.encode()))
        probes = typed.cases(empty) if is_typed else fixture.cases()
        normalize = typed.normalized_namespaces if is_typed else lambda value: value
        paths = []
        for index, (selected, primitive, document, valid) in enumerate(probes):
            try:
                accepted = oracle.validate(normalize(etree.fromstring(document.encode())))
            except ValueError:
                accepted = False
            if accepted != valid:
                raise AssertionError(f'Independent native variant decision differs: {mode}/{index}: {oracle.error_log}')
            path = directory / f'v{selected}u{primitive}-{index}.{"valid" if valid else "invalid"}.xml'
            path.write_text(document, encoding='utf-8'); paths.append(path)
        model = directory / 'model.serializer'; model.write_text(typed.MODEL if is_typed else fixture.MODEL, encoding='utf-8')
        options = directory / 'variants.ini'; options.write_text(typed.config(empty) if is_typed else fixture.CONFIG, encoding='utf-8')
        command = [args.compiler, '--input', model, '--config', options, '--language', ','.join(languages)]
        for language in languages:
            target = directory / language; target.mkdir(parents=True, exist_ok=True)
            command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
        sdk.run(command)
        for language in languages:
            source = directory / 'source' / language; source.mkdir(parents=True, exist_ok=True)
            filename, content = ('main.rs', RUST) if language == 'rust' else ('main.c', C)
            (source / filename).write_text(content, encoding='utf-8')
            if language == 'rust':
                for filename in ('Cargo.toml', 'Cargo.lock'):
                    shutil.copyfile(ROOT / 'example/xml/rust' / filename, source / filename)
            for path in paths:
                Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            launch = xml_runner.build_language(args, runner, language, directory / language, source_root=source.parent)
            sdk.run([*launch, *(runner.path(language, path) for path in paths)])
            for path in paths:
                emitted = Path(str(path) + f'.{language}.xml')
                if path.name.endswith('.valid.xml'):
                    oracle.assertValid(normalize(etree.parse(str(emitted)).getroot()))
                elif emitted.exists():
                    raise AssertionError(f'Invalid native variant output published: {emitted}')
        count += len(paths)
    print(f'PASS: {count} independent native variant probes across {len(languages)} SDKs and exact empty/prefixed type scopes', flush=True)


if __name__ == '__main__':
    main()
