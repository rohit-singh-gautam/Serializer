"""Qualify native Rust Option states with the shared independent presence corpus."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil

from lxml import etree
from portable_xml_presence_test import MODEL, OPTIONS, XSD, BOUND_XSD, RICH_MODEL, RICH_OPTIONS, RICH_XSD, cases, rich_cases

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_rust_presence_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

MAIN = r'''mod schema;
use schema::*;
use std::{env,fs};

// Require native nested Option layers to preserve absent, nil and present empty values.
fn main() {
    let start="<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>";
    let end="<nested xsi:nil='true'/></sample>";
    let absent=NilFixtureSample::decode_xml(&format!("{start}<nil_value xsi:nil='true'/>{end}")).unwrap();
    let nil_value=NilFixtureSample::decode_xml(&format!("{start}<nil_value/><states xsi:nil='true'/><entries xsi:nil='true'/>{end}")).unwrap();
    let empty=NilFixtureSample::decode_xml(&format!("{start}<nil_value/><states/><entries/>{end}")).unwrap();
    assert_eq!(absent.states,None); assert_eq!(nil_value.states,Some(None)); assert_eq!(empty.states,Some(Some(String::new())));
    assert_eq!(absent.entries,None); assert_eq!(nil_value.entries,Some(None)); assert_eq!(empty.entries,Some(Some(Vec::new())));
    assert_eq!(absent.nil_value,None); assert_eq!(nil_value.nil_value,Some(String::new()));
    for value in [absent,nil_value,empty] {
        let decoded=NilFixtureSample::decode_xml(&value.encode_xml().unwrap()).unwrap();
        for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {
            assert_eq!(value.encode(protocol).unwrap(),decoded.encode(protocol).unwrap());
        }
    }
    for argument in env::args().skip(1) {
        let input=fs::read_to_string(&argument).unwrap();
        let name=std::path::Path::new(&argument).file_name().unwrap().to_str().unwrap();
        let valid=name.ends_with(".valid.xml");
        let result=(|| -> Result<String,Error> {
            if name.starts_with("rich") {
                let value=NilShapesSample::decode_xml(&input)?; let output=value.encode_xml()?;
                assert_eq!(value.encode(Protocol::BinaryNone)?,NilShapesSample::decode_xml(&output)?.encode(Protocol::BinaryNone)?);
                Ok(output)
            } else if name.starts_with("holder") {
                let value=NilFixtureHolder::decode_xml(&input)?; let output=value.encode_xml()?;
                assert_eq!(value.encode(Protocol::BinaryNone)?,NilFixtureHolder::decode_xml(&output)?.encode(Protocol::BinaryNone)?);
                Ok(output)
            } else { NilFixtureSample::decode_xml(&input)?.encode_xml() }
        })();
        match result {
            Ok(output)=>{ assert!(valid,"Invalid presence candidate published: {name}"); fs::write(argument+".rust.xml",output).unwrap(); }
            Err(error)=>assert!(!valid,"Valid presence candidate rejected: {name}: {error:?}"),
        }
    }
    println!("Rust absent, nil, empty, nested and collection states pass");
}
'''


def main():
    """Compile actual generated Rust and independently validate every successful output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--cargo-home', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    if args.cargo_home:
        os.environ['CARGO_HOME'] = str(args.cargo_home.resolve())
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home'))
    os.environ['RUSTFLAGS'] = '-D warnings'
    source, config = args.build / 'model.serializer', args.build / 'options.ini'
    source.write_text(MODEL.replace('namespace nil_fixture {', 'namespace nil_fixture { using schema "presence.xsd";') + RICH_MODEL, encoding='utf-8')
    config.write_text(OPTIONS + RICH_OPTIONS, encoding='utf-8')
    (args.build / 'presence.xsd').write_text(BOUND_XSD, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    rich_oracle = etree.XMLSchema(etree.fromstring(RICH_XSD.encode()))
    paths = []
    for index, (kind, document, valid) in enumerate(cases() + rich_cases()):
        selected_oracle = rich_oracle if kind == 'rich' else oracle
        if selected_oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent presence decision differs for {index}: {selected_oracle.error_log}')
        path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8')
        Path(str(path) + '.rust.xml').unlink(missing_ok=True)
        paths.append(path)
    sdk.run([args.compiler, '--input', source, '--config', config, '--language', 'rust', '--rust.output', args.build / 'schema.rs'])
    (args.build / 'main.rs').write_text(MAIN, encoding='utf-8')
    for filename in ('Cargo.toml', 'Cargo.lock'):
        shutil.copyfile(ROOT / 'example/xml/rust' / filename, args.build / filename)
    runner = sdk.Runner(args)
    sdk.run(runner.command('rust', 'cargo', 'build', '--locked', '--release', '--manifest-path', runner.path('rust', args.build / 'Cargo.toml')))
    executable = args.build / 'target/release' / ('serializer-xml-example.exe' if os.name == 'nt' else 'serializer-xml-example')
    sdk.run([*runner.command('rust', runner.path('rust', executable)), *(runner.path('rust', item) for item in paths)])
    for path in paths:
        output = Path(str(path) + '.rust.xml')
        if path.name.endswith('.valid.xml'):
            (rich_oracle if path.name.startswith('rich') else oracle).assertValid(etree.parse(str(output)))
        elif output.exists():
            raise AssertionError('Invalid presence candidate published output')
    print(f'PASS: {len(paths)} independent Rust presence decisions', flush=True)


if __name__ == '__main__':
    main()
