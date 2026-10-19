"""Qualify generated owning Rust input with the legacy tree parser disabled."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_rust_token_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

MAIN = r'''mod schema;
use schema::*;
// Exercise only the generated owning entry points after proving tree parsing is disabled.
fn main() {
    let text="<order xmlns='urn:serializer:xsd' id='4294967295'><customer>A&amp;B&#13;😀</customer><value>-2147483648</value><value>2147483647</value><note/></order>";
    schema::assert_tree_disabled(text);
    let value=XmlProviderFixtureOrder::decode_xml(text).expect("direct Rust input");
    assert_eq!(value.id,u32::MAX); assert_eq!(value.customer,"A&B\r😀");
    assert_eq!(value.values,vec![i32::MIN,i32::MAX]); assert_eq!(value.note.as_deref(),Some(""));
    let encoded=value.encode_xml().unwrap();
    let decoded=XmlProviderFixtureOrder::decode_xml(&encoded).unwrap();
    for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {
        assert_eq!(value.encode(protocol).unwrap(),decoded.encode(protocol).unwrap());
    }
    assert_eq!(XmlProviderFixtureOrder::decode_xml(&text.replace("2147483647","2147<![CDATA[483]]>&#54;47")).unwrap().values[1],i32::MAX);
    assert_eq!(XmlProviderFixtureOrder::decode_xml(&text.replace("<note/>","")).unwrap().note,None);
    let long_reference=text.replace("&#13;","&#000000000000000000000000000000000000013;");
    assert_eq!(XmlProviderFixtureOrder::decode_xml(&long_reference).unwrap().customer,value.customer);
    let limits=XmlLimits {max_bytes:text.len(),..XmlLimits::default()};
    assert_eq!(XmlProviderFixtureOrder::decode_xml_with_validation(text,limits,None).unwrap().id,u32::MAX);
    for bad in [text.to_owned()+"<extra/>",text[..text.len()-1].to_owned(),
        text.replace("urn:serializer:xsd","urn:wrong"),text.replace("<order ","<order extra='1' "),
        text.replace("<value>-2147483648</value>","<value>2147483648</value>"),
        text.replace("<customer>","<customer><nested/>"),text.replace("<note/>","<note extra='1'/>"),
        text.replace("<note/>","<note/><note/>"),text.replace("</order>","</wrong>"),
        "<!DOCTYPE order [<!ENTITY local 'expanded'>]>".to_owned()+text,
        "<?xml version='1.1'?>".to_owned()+text,"<?xml version='1.0' encoding='ISO-8859-1'?>".to_owned()+text,
        text.replace("A&amp;B","&#0;"),text.replace("A&amp;B","&#xD800;"),text.replace("A&amp;B","&unknown;") ] {
        assert!(XmlProviderFixtureOrder::decode_xml(&bad).is_err(),"Invalid candidate published: {}",bad);
    }
    for limits in [XmlLimits {max_bytes:text.len()-1,..XmlLimits::default()},
        XmlLimits {max_nodes:1,..XmlLimits::default()},XmlLimits {max_depth:1,..XmlLimits::default()},
        XmlLimits {max_text_bytes:1,..XmlLimits::default()},XmlLimits {max_attributes:0,..XmlLimits::default()}] {
        assert!(XmlProviderFixtureOrder::decode_xml_with_validation(text,limits,None).is_err());
    }
    assert!(XmlProviderFixtureOrder::decode_xml_bytes(&[0xc0,0x80]).is_err());
    assert_eq!(XmlProviderFixtureOrder::decode_xml_bytes(text.as_bytes()).unwrap().id,u32::MAX);
    println!("Rust direct owning input, split text/entities, exact EOF, namespaces and limits pass");
}
'''


def main():
    """Generate real Rust, instrument its legacy parser, then compile and execute the actual SDK."""
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
    else:
        os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home'))
    os.environ['RUSTFLAGS'] = '-D warnings'
    output = args.build / 'schema.rs'
    sdk.run([args.compiler, '--input', ROOT / 'test/resources/xml_provider.serializer',
             '--config', ROOT / 'test/resources/xml_provider.ini', '--language', 'rust', '--rust.output', output])
    generated = output.read_text(encoding='utf-8')
    signature = "fn srl_xml_parse(text:&str, limits:XmlLimits) -> Result<roxmltree::Document<'_>,Error> {"
    if generated.count(signature) != 1:
        raise AssertionError('Expected the retained-tree parser for the negative control')
    generated = generated.replace(signature, signature + '\n    if SRL_XML_FORBID_TREE.load(std::sync::atomic::Ordering::Relaxed) { return Err(Error("retained XML tree")); }')
    generated += '''
static SRL_XML_FORBID_TREE:std::sync::atomic::AtomicBool=std::sync::atomic::AtomicBool::new(true);
/// Prove a valid document would fail if a generated call selected the old retained-tree parser.
pub fn assert_tree_disabled(text:&str) {
    match srl_xml_parse(text,XmlLimits::default()) { Err(error)=>assert_eq!(error.0,"retained XML tree"),Ok(_)=>panic!("tree negative control failed") }
}
'''
    output.write_text(generated, encoding='utf-8')
    (args.build / 'main.rs').write_text(MAIN, encoding='utf-8')
    for filename in ('Cargo.toml', 'Cargo.lock'):
        shutil.copyfile(ROOT / 'example/xml/rust' / filename, args.build / filename)
    runner = sdk.Runner(args)
    manifest = runner.path('rust', args.build / 'Cargo.toml')
    sdk.run(runner.command('rust', 'cargo', 'run', '--locked', '--release', '--manifest-path', manifest))


if __name__ == '__main__':
    main()
