"""Qualify flattened XML over private native C/Rust/Kotlin/Swift owning bases."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil

from lxml import etree
from portable_xml_model_test import rejected_shapes

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_native_model_sdk', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)
sdk = xml_runner.sdk
NS = 'urn:serializer:native:inherited'
MODEL = '''serializer version 1.20.0;
namespace inherited {
  using schema "inherited.xsd";
  class child { private string value; }
  class first {
    private uint32 a;
    protected optional<optional<string>> states;
    public uint16 id;
    private array[2] uint8 fixed_values;
  }
  class second { private bool b; protected child nested; }
  class middle : protected first { private bool c; }
  class record : private middle, protected second { public int16 d; }
}
'''
OPTIONS = '[xml]\nenabled=true\n' + ''.join(
    f'[xml.class.inherited::{name}]\npresence.states=absent_or_nil\n'
    for name in ('first', 'middle', 'record'))
FIRST = '''<xs:element name="a" type="xs:unsignedInt"/>
<xs:element name="states" type="xs:string" minOccurs="0" nillable="true"/>
<xs:element name="id" type="xs:unsignedShort"/>
<xs:element name="fixed_values" type="xs:unsignedByte" minOccurs="2" maxOccurs="2"/>'''
SECOND = '<xs:element name="b" type="xs:boolean"/><xs:element name="nested" type="t:childType"/>'
MIDDLE = FIRST + '<xs:element name="c" type="xs:boolean"/>'
RECORD = MIDDLE + SECOND + '<xs:element name="d" type="xs:short"/>'
XSD = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="{NS}" targetNamespace="{NS}" elementFormDefault="qualified">
<xs:complexType name="childType"><xs:sequence><xs:element name="value" type="xs:string"/></xs:sequence></xs:complexType>
<xs:element name="child" type="t:childType"/>
''' + ''.join(f'<xs:element name="{name}"><xs:complexType><xs:sequence>{fields}</xs:sequence></xs:complexType></xs:element>'
              for name, fields in [('first', FIRST), ('second', SECOND), ('middle', MIDDLE), ('record', RECORD)]) + '</xs:schema>'


def cases():
    """Require all inherited fields in base-first order, exact widths and native nil states."""
    start = f'<record xmlns="{NS}" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><a>4294967295</a>'
    tail = '<id>65535</id><fixed_values>255</fixed_values><fixed_values>0</fixed_values><c>true</c><b>false</b><nested><value>private&amp;text</value></nested><d>-32768</d></record>'
    documents = [(start + states + tail, True) for states in [
        '', '<states xsi:nil="true"/>', '<states/>', '<states>hello</states>', '<states xsi:nil="false"/>']]
    base = documents[3][0]
    for before, after in [
        ('<a>4294967295</a>', ''), ('4294967295', '4294967296'), ('<id>65535</id>', '<id>65536</id>'),
        ('<states>hello</states>', '<states xsi:nil="true"> </states>'), ('<states>hello</states>', '<states/><states/>'),
        ('<states>hello</states>', '<states unexpected="1"/>'), ('<fixed_values>255</fixed_values>', ''),
        ('<fixed_values>255</fixed_values>', '<fixed_values>256</fixed_values>'),
        ('<fixed_values>0</fixed_values>', '<fixed_values>0</fixed_values><fixed_values>1</fixed_values>'),
        ('<c>true</c>', ''), ('<b>false</b>', '<b>no</b>'), ('<value>private&amp;text</value>', ''),
        ('<nested>', '<nested xsi:nil="true">'), ('<d>-32768</d>', '<d>-32769</d>'),
        ('<c>true</c><b>false</b>', '<b>false</b><c>true</c>'), ('</record>', '<a>1</a></record>'),
        ('</record>', '</record><second/>'), ('urn:serializer:native:inherited', 'urn:wrong'),
    ]:
        documents.append((base.replace(before, after), False))
    return documents


RUST = r'''mod schema;
use schema::*;
// Compare independently classified XML with complete native owning candidates.
fn main() {
    for argument in std::env::args().skip(1) {
        let text=std::fs::read_to_string(&argument).unwrap(); let valid=argument.ends_with(".valid.xml");
        let result=(|| -> Result<String,Error> {
            let value=InheritedRecord::decode_xml(&text)?; schema::check_private_bases(&value);
            let output=value.encode_xml()?; let copy=InheritedRecord::decode_xml(&output)?;
            for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {
                assert_eq!(value.encode(protocol)?,copy.encode(protocol)?);
            }
            let bounds=XmlLimits {max_depth:1,..XmlLimits::default()};
            assert!(value.encode_xml_with_validation(bounds,None).is_err());
            assert!(InheritedRecord::decode_xml_with_validation(&text,bounds,None).is_err()); Ok(output)
        })();
        match result {
            Ok(output)=>{ assert!(valid,"Invalid inherited candidate published: {argument}"); std::fs::write(argument+".rust.xml",output).unwrap(); }
            Err(error)=>assert!(!valid,"Valid inherited candidate rejected: {argument}: {error:?}"),
        }
    }
    println!("Rust private owning inheritance and native protocols pass");
}
'''
RUST_PROBE = r'''
/// Inspect original private native fields from their existing generated module privacy scope.
pub fn check_private_bases(value:&InheritedRecord) {
    assert_eq!(value.base0.base0.a,u32::MAX); assert_eq!(value.base0.base0.id,u16::MAX);
    assert_eq!(value.base0.base0.fixed_values,vec![255,0]); assert!(value.base0.c); assert!(!value.base1.b);
    assert_eq!(value.base1.nested.value,"private&text"); assert_eq!(value.d,i16::MIN);
}
'''

KOTLIN = r'''import java.nio.file.Files
import java.nio.file.Path
// Require private base storage and complete native values for every independent oracle case.
fun main(arguments: Array<String>) {
    for (argument in arguments) {
        val bytes=Files.readAllBytes(Path.of(argument)); val valid=argument.endsWith(".valid.xml")
        val output=try {
            val value=InheritedRecord.decodeXml(bytes)
            check(value.srlXmlBase0.srlXmlBase0.srlXmlFieldA == UInt.MAX_VALUE)
            check(value.srlXmlBase0.srlXmlBase0.srlXmlFieldId == UShort.MAX_VALUE)
            check(value.srlXmlBase0.srlXmlFieldC && !value.srlXmlBase1.srlXmlFieldB)
            check(value.srlXmlBase1.srlXmlFieldNested.srlXmlFieldValue == "private&text")
            check(value.d == Short.MIN_VALUE)
            val encoded=value.encodeXml(); val copy=InheritedRecord.decodeXml(encoded)
            for (protocol in Protocol.entries) check(value.encode(protocol).contentEquals(copy.encode(protocol)))
            val bounds=XmlLimits(maxDepth=1)
            check(runCatching { value.encodeXml(bounds) }.isFailure)
            check(runCatching { InheritedRecord.decodeXml(bytes,bounds) }.isFailure)
            encoded
        } catch (error: IllegalArgumentException) { if (valid) throw error; continue }
        check(valid) { "Invalid inherited candidate published: $argument" }
        Files.write(Path.of(argument+".kotlin.xml"),output)
    }
    println("Kotlin private owning inheritance and native protocols pass")
}
'''

SWIFT = r'''import Foundation
// Inspect existing private struct storage without exposing it in generated public APIs.
func stored(_ owner: Any, _ name: String) throws -> Any {
    guard let value=Mirror(reflecting: owner).children.first(where: { $0.label == name })?.value else { throw SerializerError.invalid("Missing native field") }
    return value
}
// Compare every independently classified document and all native protocol values.
for argument in CommandLine.arguments.dropFirst() {
    let input=Array(try Data(contentsOf: URL(fileURLWithPath: argument))); let valid=argument.hasSuffix(".valid.xml")
    let output: [UInt8]
    do {
        let value=try InheritedRecord.decodeXml(input)
        let middle=try stored(value,"base0"); let first=try stored(middle,"base0"); let second=try stored(value,"base1")
        let nested=try stored(second,"nested")
        guard try stored(first,"a") as? UInt32 == UInt32.max, try stored(first,"id") as? UInt16 == UInt16.max,
              try stored(first,"fixedValues") as? [UInt8] == [255,0], try stored(middle,"c") as? Bool == true,
              try stored(second,"b") as? Bool == false, try stored(nested,"value") as? String == "private&text",
              value.d == Int16.min else { throw SerializerError.invalid("Private native base values") }
        output=try value.encodeXml(); let copy=try InheritedRecord.decodeXml(output)
        for protocolValue in Protocol.allCases { guard try value.encode(protocolValue) == copy.encode(protocolValue) else { throw SerializerError.invalid("Inherited native identity") } }
        var bounds=XmlLimits(); bounds.maxDepth=1
        var inputRejected=false; var outputRejected=false
        do { _=try value.encodeXml(bounds) } catch { outputRejected=true }
        do { _=try InheritedRecord.decodeXml(input,bounds) } catch { inputRejected=true }
        guard inputRejected && outputRejected else { throw SerializerError.invalid("Inherited resource bound") }
    } catch { if valid { throw error }; continue }
    guard valid else { throw SerializerError.invalid("Invalid inherited candidate published") }
    try Data(output).write(to: URL(fileURLWithPath: argument+".swift.xml"))
}
print("Swift private owning inheritance and native protocols pass")
'''

C = r'''#include "schema.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Require native results independently of generated XML helper names. */
static void require(bool value, const char* message) { if (!value) { fprintf(stderr,"%s\n",message); exit(EXIT_FAILURE); } }
/* Reject malformed candidates atomically and compare all successful native protocol values. */
int main(int argc, char** argv) {
  for (int index=1;index<argc;++index) {
    const char* argument=argv[index]; FILE* file=fopen(argument,"rb"); require(file != NULL,"Open inherited fixture");
    require(fseek(file,0,SEEK_END) == 0,"Seek inherited fixture"); const long length=ftell(file); require(length >= 0,"Inherited fixture size"); rewind(file);
    uint8_t* input=(uint8_t*)malloc((size_t)length+1); require(input != NULL,"Inherited allocation");
    require(fread(input,1,(size_t)length,file) == (size_t)length,"Read inherited fixture"); fclose(file);
    const bool valid=strstr(argument,".valid.xml") != NULL; inherited_record value={0},copy={0};
    require(inherited_record_init(&value) == srl_ok && inherited_record_init(&copy) == srl_ok,"Inherited init");
    const inherited_record previous=value; const srl_status status=inherited_record_decode_xml(&value,input,(size_t)length,NULL);
    srl_buffer output={0};
    if (valid) {
      require(status == srl_ok,argument);
      require(value.base0.base0.a == UINT32_MAX && value.base0.base0.id == UINT16_MAX && value.base0.base0.fixed_values.size == 2 &&
          value.base0.base0.fixed_values.data[0] == 255 && value.base0.base0.fixed_values.data[1] == 0 && value.base0.c && !value.base1.b &&
          strcmp(value.base1.nested.value.data,"private&text") == 0 && value.d == INT16_MIN,"Private inherited values");
      require(inherited_record_encode_xml(&value,&output,NULL) == srl_ok,"Inherited output");
      require(inherited_record_decode_xml(&copy,output.data,output.size,NULL) == srl_ok,"Inherited roundtrip");
      const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
      for (size_t protocol=0;protocol<sizeof(protocols)/sizeof(protocols[0]);++protocol) {
        srl_buffer first={0},second={0}; require(inherited_record_encode(&value,protocols[protocol],&first) == srl_ok &&
            inherited_record_encode(&copy,protocols[protocol],&second) == srl_ok && first.size == second.size && memcmp(first.data,second.data,first.size) == 0,"Inherited native identity");
        srl_buffer_free(&first); srl_buffer_free(&second);
      }
      srl_limits bounds=srl_default_limits(); bounds.max_depth=1;
      require(inherited_record_decode_xml(&copy,input,(size_t)length,&bounds) != srl_ok,"Inherited depth bound");
      const size_t capacity=strlen(argument)+sizeof(".c.xml"); char* path=(char*)malloc(capacity); require(path != NULL,"Output path allocation");
      require(snprintf(path,capacity,"%s.c.xml",argument) >= 0,"Inherited output path"); file=fopen(path,"wb"); require(file != NULL,"Open inherited output"); free(path);
      require(fwrite(output.data,1,output.size,file) == output.size,"Write inherited output"); fclose(file);
    } else { require(status != srl_ok,argument); require(memcmp(&previous,&value,sizeof(value)) == 0,"Partial inherited candidate published"); }
    free(input); srl_buffer_free(&output); inherited_record_free(&value); inherited_record_free(&copy);
  }
  puts("C private owning inheritance and native protocols pass"); return EXIT_SUCCESS;
}
'''


def main():
    """Generate authoritative contracts, compile actual SDKs and independently validate every output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='rust,c,kotlin,swift')
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if len(languages) != len(set(languages)) or any(value not in ('rust', 'c', 'kotlin', 'swift') for value in languages):
        parser.error('Choose distinct languages from rust,c,kotlin,swift')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home'))
    os.environ['RUSTFLAGS'] = '-D warnings'
    (args.build / 'model.serializer').write_text(MODEL, encoding='utf-8')
    (args.build / 'options.ini').write_text(OPTIONS, encoding='utf-8')
    (args.build / 'inherited.xsd').write_text(XSD, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    paths = []
    for index, (document, valid) in enumerate(cases()):
        try:
            accepted = oracle.validate(etree.fromstring(document.encode()))
        except etree.XMLSyntaxError:
            accepted = False
        if accepted != valid:
            raise AssertionError(f'Unexpected independent oracle decision {index}: {oracle.error_log}')
        path = args.build / f'inherited-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8'); paths.append(path)
    command = [args.compiler, '--input', args.build / 'model.serializer', '--config', args.build / 'options.ini',
               '--language', ','.join(languages), '--kotlin.package=']
    sources = {'rust': ('main.rs', RUST), 'c': ('main.c', C), 'kotlin': ('Main.kt', KOTLIN), 'swift': ('main.swift', SWIFT)}
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command)
    runner = sdk.Runner(args)
    for language in languages:
        directory = args.build / language
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        filename, content = sources[language]; (source / filename).write_text(content, encoding='utf-8')
        if language == 'rust':
            with (directory / 'schema.rs').open('a', encoding='utf-8') as output:
                output.write(RUST_PROBE)
            for filename in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(ROOT / 'example/xml/rust' / filename, source / filename)
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        if language in ('rust', 'c'):
            launch = xml_runner.build_language(args, runner, language, directory, source_root=source.parent)
        else:
            launch = runner.build(language, 'xml_inherited', directory, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            output = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(output)))
            elif output.exists():
                raise AssertionError('Invalid inherited candidate published output')
    rejected_shapes(args, languages)
    print(f'PASS: {len(paths)} independent inherited probes and atomic collision gates across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
