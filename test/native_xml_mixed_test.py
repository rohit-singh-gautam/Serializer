"""Qualify C/Rust ordered native mixed fragments with the shared 21 independent probes."""
import argparse
import os
from pathlib import Path
import shutil

from lxml import etree
import native_xml_map_test as native
import portable_xml_mixed_test as fixture

ROOT, xml_runner, sdk = native.ROOT, native.xml_runner, native.sdk

RUST_PROBE = r'''
/// Inspect only the native private fragment discriminator from its existing module privacy scope.
pub fn srl_test_break_paragraph(value:&mut JsMixedParagraph) -> bool {
    if let Some(fragment)=value.content.first_mut() { fragment.value_index=usize::MAX; true } else { false }
}
/// Apply the same private-storage check through an ordinary nested owning record.
pub fn srl_test_break_holder(value:&mut JsMixedHolder) -> bool { srl_test_break_paragraph(&mut value.value) }
'''
RUST = r'''mod schema;
use schema::*;
use std::{env,fs,path::Path};
/// Match independent validity/resource policies while checking existing native codec identities.
fn main() -> Result<(),Box<dyn std::error::Error>> {
    let arguments:Vec<String>=env::args().skip(1).collect();
    for argument in arguments.chunks(3) {
        let path=&argument[0]; let text=fs::read_to_string(path)?; let valid=path.ends_with(".valid.xml");
        let policy=XmlMixedLimits{max_fragments:argument[1].parse()?,max_text_bytes:argument[2].parse()?};
        macro_rules! probe { ($kind:ty,$break:path) => { (|| -> Result<String,Error> {
            let mut value=match <$kind>::decode_xml_with_mixed_limits(&text,XmlLimits::default(),None,policy) {
                Ok(value)=>value,Err(error)=>{
                    if let Ok(value)=<$kind>::decode_xml(&text) { assert!(value.encode_xml_with_mixed_limits(XmlLimits::default(),None,policy).is_err()); }
                    return Err(error);
                }
            };
            let output=value.encode_xml_with_mixed_limits(XmlLimits::default(),None,policy)?;
            let copy=<$kind>::decode_xml_with_mixed_limits(&output,XmlLimits::default(),None,policy)?;
            assert_eq!(output,copy.encode_xml_with_mixed_limits(XmlLimits::default(),None,policy)?);
            for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {
                assert_eq!(value.encode(protocol)?,copy.encode(protocol)?);
            }
            if $break(&mut value) { assert!(value.encode_xml().is_err()); }
            Ok(output)
        })() }; }
        let result=if Path::new(path).file_name().unwrap().to_str().unwrap().starts_with("holder") {
            probe!(JsMixedHolder,schema::srl_test_break_holder)
        } else { probe!(JsMixedParagraph,schema::srl_test_break_paragraph) };
        match result {
            Ok(output)=>{ assert!(valid,"Invalid mixed candidate published: {path}"); fs::write(format!("{path}.rust.xml"),output)?; },
            Err(error)=>{ if valid { return Err(Box::new(error)); } }
        }
    }
    println!("Rust private ordered fragments, exact logical text and independent nested limits pass"); Ok(())
}
'''

C_CHECK = r'''
/* Compare one complete owning mixed candidate and all native protocols without changing storage. */
static srl_status check_NAME(const uint8_t* bytes,size_t size,const srl_xml_mixed_limits* policy,srl_buffer* output) {
  TYPE value={0},copy={0}; require(TYPE_init(&value) == srl_ok && TYPE_init(&copy) == srl_ok,"Initialize mixed candidates");
  const TYPE original=value; srl_status status=TYPE_decode_xml_with_mixed_limits(&value,bytes,size,NULL,policy);
  if (status != srl_ok) {
    require(memcmp(&value,&original,sizeof(value)) == 0,"Partial mixed candidate published");
    if (TYPE_decode_xml(&copy,bytes,size,NULL) == srl_ok) {
      srl_buffer limited={0}; require(TYPE_encode_xml_with_mixed_limits(&copy,&limited,NULL,policy) != srl_ok && limited.data == NULL,"Mixed output resource admission"); srl_buffer_free(&limited);
    }
    TYPE_free(&value); TYPE_free(&copy); return status;
  }
  status=TYPE_encode_xml_with_mixed_limits(&value,output,NULL,policy);
  if (status != srl_ok) { TYPE_free(&value); TYPE_free(&copy); return status; }
  srl_buffer again={0}; require(TYPE_decode_xml_with_mixed_limits(&copy,output->data,output->size,NULL,policy) == srl_ok && TYPE_encode_xml_with_mixed_limits(&copy,&again,NULL,policy) == srl_ok,"Mixed direct roundtrip");
  require(output->size == again.size && memcmp(output->data,again.data,output->size) == 0,"Stable ordered mixed output");
  const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
  for (size_t index=0; index < sizeof(protocols)/sizeof(protocols[0]); ++index) {
    srl_buffer first={0},second={0}; require(TYPE_encode(&value,protocols[index],&first) == srl_ok && TYPE_encode(&copy,protocols[index],&second) == srl_ok,"Native mixed controls");
    require(first.size == second.size && memcmp(first.data,second.data,first.size) == 0,"Native mixed fragment identity"); srl_buffer_free(&first); srl_buffer_free(&second);
  }
  if (value.CONTENT.size) {
    value.CONTENT.data[0].value_index=UINT32_MAX; srl_buffer invalid={0};
    require(TYPE_encode_xml(&value,&invalid,NULL) != srl_ok && invalid.data == NULL,"Unknown edited mixed discriminator"); srl_buffer_free(&invalid);
  }
  srl_buffer_free(&again); TYPE_free(&value); TYPE_free(&copy); return srl_ok;
}
'''
C = native.C.split('/* Compare typed aliases, native wire identities and independent per-wrapper map policies. */')[0]
for name, owner, content in [('paragraph', 'js_mixed_paragraph', 'content'), ('holder', 'js_mixed_holder', 'value.content')]:
    C += C_CHECK.replace('NAME', name).replace('TYPE', owner).replace('CONTENT', content)
C += r'''
/* Feed independent per-owner fragment and UTF8 text limits to each complete native operation. */
int main(int argc,char** argv) {
  for (int argument=1; argument+2 < argc; argument+=3) {
    const char* path=argv[argument]; const bool valid=strstr(path,".valid.xml") != NULL; size_t size=0; uint8_t* bytes=read_file(path,&size);
    const srl_xml_mixed_limits policy={(size_t)strtoull(argv[argument+1],NULL,10),(size_t)strtoull(argv[argument+2],NULL,10)};
    const char* filename=strrchr(path,'\\'); if (!filename) { filename=strrchr(path,'/'); } filename=filename ? filename+1 : path;
    srl_buffer output={0}; const srl_status status=strncmp(filename,"holder",sizeof("holder")-1u) == 0 ? check_holder(bytes,size,&policy,&output) : check_paragraph(bytes,size,&policy,&output);
    require((status == srl_ok) == valid,path); if (valid) { write_file(path,".c.xml",&output); } else { require(output.data == NULL,"Invalid mixed output published"); }
    free(bytes); srl_buffer_free(&output);
  }
  puts("C private ordered fragments, exact logical text and independent nested limits pass"); return EXIT_SUCCESS;
}
'''


def main():
    """Compile real native consumers and compare every successful text/tail position independently."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True); parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='rust,c'); parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('rust', 'c') for value in languages):
        parser.error('Choose distinct languages from rust,c')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home')); os.environ['RUSTFLAGS'] = '-D warnings'
    (args.build / 'format.xsd').write_text(fixture.FORMAT_XSD, encoding='utf-8')
    schema = args.build / 'mixed.xsd'; schema.write_text(fixture.XSD, encoding='utf-8')
    oracle = etree.XMLSchema(etree.parse(str(schema))); paths = []
    controls = {'fragments', 'bytes', 'child-limit', 'text-limit', 'large-limit', 'holder-limit'}
    for label, document, fragments, text_bytes, valid in fixture.cases():
        if oracle.validate(etree.fromstring(document.encode())) != (valid or label in controls):
            raise AssertionError(f'Independent native mixed decision differs: {label}: {oracle.error_log}')
        path = args.build / f'{label}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8'); paths.append((path, fragments, text_bytes))
    model = args.build / 'model.serializer'; model.write_text(fixture.MODEL, encoding='utf-8')
    config = args.build / 'mixed.ini'; config.write_text(fixture.OPTIONS, encoding='utf-8')
    command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages)]
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        filename, content = ('main.rs', RUST) if language == 'rust' else ('main.c', C)
        (source / filename).write_text(content, encoding='utf-8')
        if language == 'rust':
            for filename in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(ROOT / 'example/xml/rust' / filename, source / filename)
            with (args.build / language / sdk.OUTPUTS[language]).open('a', encoding='utf-8') as generated:
                generated.write(RUST_PROBE)
        for path, _, _ in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        launch = xml_runner.build_language(args, runner, language, args.build / language, source_root=source.parent); arguments = []
        for path, fragments, text_bytes in paths:
            arguments += [runner.path(language, path), fragments, text_bytes]
        sdk.run([*launch, *arguments])
        for path, _, _ in paths:
            output = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(output)))
                original = etree.parse(str(path), etree.XMLParser(remove_comments=True, remove_pis=True)).getroot()
                if fixture.semantic(original) != fixture.semantic(etree.parse(str(output)).getroot()):
                    raise AssertionError(f'Native mixed text or child position changed: {output}')
            elif output.exists():
                raise AssertionError(f'Invalid native mixed output published: {output}')
    fixture.rejected_bindings(args, languages)
    print(f'PASS: {len(paths)} independent ordered native mixed probes and 3 atomic helper gates across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
