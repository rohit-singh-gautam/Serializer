"""Qualify singleton choices and authoritative simpleContent/wrappers across native SDKs."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil

from lxml import etree

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_content_sdk', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xml_runner)
sdk = xml_runner.sdk
NS = 'urn:serializer:content'
MODEL = '''serializer version 1.20.0;
namespace content {
  using schema "content.xsd";
  xml(namespace("urn:serializer:content"));
  class child { public int32 number; }
  class price { public string currency xml(attribute); public uint64 amount xml(text); }
  class sample {
    public string before;
    public optional<int32> left;
    public optional<string> right;
    public optional<child> nested;
    public uint8 after;
  }
  class optional_batch { public optional<int32> left; public optional<string> right; public optional<child> nested; }
  class wrappers { public array price values; }
}
'''
OPTIONS = '[xml]\nenabled=true\n[xml.class.content::wrappers]\nname.values=price\nwrapper.values=values\n'
BRANCHES = '<xs:element name="left" type="xs:int"/><xs:element name="right" type="xs:string"/><xs:element name="nested" type="t:childType"/>'
XSD = f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="{NS}" targetNamespace="{NS}" elementFormDefault="qualified">
<xs:complexType name="childType"><xs:sequence><xs:element name="number" type="xs:int"/></xs:sequence></xs:complexType>
<xs:element name="child" type="t:childType"/>
<xs:complexType name="priceType"><xs:simpleContent><xs:extension base="xs:unsignedLong"><xs:attribute name="currency" type="xs:string" use="required"/></xs:extension></xs:simpleContent></xs:complexType>
<xs:element name="price" type="t:priceType"/>
<xs:element name="sample"><xs:complexType><xs:sequence><xs:element name="before" type="xs:string"/><xs:choice>{BRANCHES}</xs:choice><xs:element name="after" type="xs:unsignedByte"/></xs:sequence></xs:complexType></xs:element>
<xs:element name="optional_batch"><xs:complexType><xs:choice minOccurs="0">{BRANCHES}</xs:choice></xs:complexType></xs:element>
<xs:element name="wrappers"><xs:complexType><xs:sequence><xs:element name="values"><xs:complexType><xs:sequence><xs:element name="price" type="t:priceType" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element>
</xs:schema>'''


def cases():
    """Require exactly one allowed alternative, and validate XSD-derived text/wrapper contracts."""
    branches = ['<left>-2147483648</left>', '<right/>', '<right>A&amp;B</right>', '<nested><number>2147483647</number></nested>']
    documents = [('sample', f'<sample xmlns="{NS}"><before>start</before>{body}<after>255</after></sample>', True) for body in branches]
    documents += [('optional', f'<optional_batch xmlns="{NS}">{body}</optional_batch>', True) for body in ['', *branches]]
    base = documents[0][1]
    for before, after in [
        (branches[0], ''), (branches[0], branches[0] + branches[1]), (branches[0], branches[1] + branches[0]),
        (branches[0], branches[0] * 2), (branches[0], '<left>2147483648</left>'),
        (branches[0], '<nested/>'), (branches[0], '<nested><number>2147483648</number></nested>'),
        (branches[0], '<unknown/>'), ('<before>start</before>', ''), ('<after>255</after>', '<after>256</after>'),
        ('</sample>', '<right>x</right></sample>'), ('<left>', '<left unexpected="1">'),
    ]:
        documents.append(('sample', base.replace(before, after), False))
    for body in [branches[0] + branches[1], branches[1] + branches[0], '<unknown/>', '<left/>']:
        documents.append(('optional', f'<optional_batch xmlns="{NS}">{body}</optional_batch>', False))
    for value in ['0', '18446744073709551615', ' 0007 ']:
        documents.append(('price', f'<price xmlns="{NS}" currency="EUR">{value}</price>', True))
    price = documents[-2][1]
    for before, after in [('18446744073709551615', '18446744073709551616'), (' currency="EUR"', ''),
                          ('18446744073709551615', '<nested/>'), ('currency="EUR"', 'currency="EUR" extra="1"')]:
        documents.append(('price', price.replace(before, after), False))
    for body in ['', '<price currency="USD">7</price>', '<price currency="USD">7</price><price currency="EUR">18446744073709551615</price>']:
        documents.append(('wrappers', f'<wrappers xmlns="{NS}"><values>{body}</values></wrappers>', True))
    wrapped = documents[-1][1]
    for before, after in [('<values>', '<values unexpected="1">'), ('currency="USD"', ''),
                          ('18446744073709551615', '18446744073709551616'), ('<price currency="USD">7</price>', '<unknown/>')]:
        documents.append(('wrappers', wrapped.replace(before, after), False))
    documents.append(('wrappers', f'<wrappers xmlns="{NS}"/>', False))
    return documents


def semantic(node):
    """Compare independent typed scalar values rather than namespace prefixes or numeric aliases."""
    name = etree.QName(node).localname
    text = node.text or ''
    if name in ('price', 'left', 'number', 'after'):
        text = int(text)
    elif len(node):
        text = text.strip()
    return (etree.QName(node).namespace, name, sorted(node.attrib.items()), text, [semantic(child) for child in node])


TYPES = {'sample': 'Sample', 'optional': 'OptionalBatch', 'price': 'Price', 'wrappers': 'Wrappers'}


def rust_consumer():
    """Emit typed Rust probes without introducing a runtime model-reflection adapter."""
    source = 'mod schema;\nuse schema::*;\n'
    for suffix in TYPES.values():
        source += '''/// Compare complete XML and native values for one statically selected root.
fn check_@SUFFIX@(text:&str) -> Result<String,Error> {
    let value=Content@SUFFIX@::decode_xml(text)?; let output=value.encode_xml()?;
    let copy=Content@SUFFIX@::decode_xml(&output)?;
    for protocol in [Protocol::Json,Protocol::BinaryNone,Protocol::BinaryInteger,Protocol::BinaryString] {
        assert_eq!(value.encode(protocol)?,copy.encode(protocol)?);
    }
    let bounds=XmlLimits {max_bytes:0,..XmlLimits::default()};
    assert!(value.encode_xml_with_validation(bounds,None).is_err());
    assert!(Content@SUFFIX@::decode_xml_with_validation(text,bounds,None).is_err()); Ok(output)
}
'''.replace('@SUFFIX@', suffix)
    source += '''/// Reject invalid authored choice states and compare independent oracle decisions.
fn main() {
    let mut invalid=ContentSample::default(); assert!(invalid.encode_xml().is_err());
    invalid.left=Some(1); invalid.right=Some(String::new()); assert!(invalid.encode_xml().is_err());
    let mut optional=ContentOptionalBatch::default(); assert!(optional.encode_xml().is_ok());
    optional.left=Some(1); optional.right=Some(String::new()); assert!(optional.encode_xml().is_err());
    for argument in std::env::args().skip(1) {
        let text=std::fs::read_to_string(&argument).unwrap();
        let name=std::path::Path::new(&argument).file_name().unwrap().to_str().unwrap();
        let result=if name.starts_with("sample-") {check_Sample(&text)} else if name.starts_with("optional-") {check_OptionalBatch(&text)}
            else if name.starts_with("price-") {check_Price(&text)} else {check_Wrappers(&text)};
        match result {
            Ok(output)=>{ assert!(name.ends_with(".valid.xml"),"Invalid content candidate published: {name}"); std::fs::write(argument+".rust.xml",output).unwrap(); }
            Err(error)=>assert!(name.ends_with(".invalid.xml"),"Valid content rejected: {name}: {error:?}"),
        }
    }
    println!("Rust singleton choices and authoritative simpleContent/wrappers pass");
}
'''
    return source.replace('check_OptionalBatch', 'check_optional_batch').replace('check_Wrappers', 'check_wrappers').replace('check_Sample', 'check_sample').replace('check_Price', 'check_price')


def kotlin_consumer():
    """Emit direct Kotlin consumers with explicit native protocol comparisons."""
    source = 'import java.nio.file.Files\nimport java.nio.file.Path\nimport java.util.Optional\n'
    for suffix in TYPES.values():
        source += '''// Require complete values and ordinary native protocol identity for one root.
fun check@SUFFIX@(bytes: ByteArray): ByteArray {
    val value=Content@SUFFIX@.decodeXml(bytes); val output=value.encodeXml(); val copy=Content@SUFFIX@.decodeXml(output)
    for (protocol in Protocol.entries) check(value.encode(protocol).contentEquals(copy.encode(protocol)))
    val bounds=XmlLimits(maxBytes=0)
    check(runCatching { value.encodeXml(bounds) }.isFailure)
    check(runCatching { Content@SUFFIX@.decodeXml(bytes,bounds) }.isFailure); return output
}
'''.replace('@SUFFIX@', suffix)
    source += '''// Validate authored alternatives before publishing any output bytes.
fun main(arguments: Array<String>) {
    val invalid=ContentSample(); check(runCatching { invalid.encodeXml() }.isFailure)
    invalid.left=Optional.of(1); invalid.right=Optional.of(""); check(runCatching { invalid.encodeXml() }.isFailure)
    val optional=ContentOptionalBatch(); optional.encodeXml()
    optional.left=Optional.of(1); optional.right=Optional.of(""); check(runCatching { optional.encodeXml() }.isFailure)
    for (argument in arguments) {
        val bytes=Files.readAllBytes(Path.of(argument)); val name=Path.of(argument).fileName.toString(); val valid=name.endsWith(".valid.xml")
        val output=try { when {
            name.startsWith("sample-")->checkSample(bytes); name.startsWith("optional-")->checkOptionalBatch(bytes)
            name.startsWith("price-")->checkPrice(bytes); else->checkWrappers(bytes)
        } } catch (error: IllegalArgumentException) { if (valid) throw error; continue }
        check(valid) { "Invalid content candidate published: $name" }; Files.write(Path.of(argument+".kotlin.xml"),output)
    }
    println("Kotlin singleton choices and authoritative simpleContent/wrappers pass")
}
'''
    return source


def swift_consumer():
    """Emit real typed Swift calls and retain no test-side XML decoder substitute."""
    source = 'import Foundation\n'
    for suffix in TYPES.values():
        source += '''// Compare exact native values and resource policies after complete XML input.
func check@SUFFIX@(_ bytes: [UInt8]) throws -> [UInt8] {
    let value=try Content@SUFFIX@.decodeXml(bytes); let output=try value.encodeXml(); let copy=try Content@SUFFIX@.decodeXml(output)
    for protocolValue in Protocol.allCases { guard try value.encode(protocolValue) == copy.encode(protocolValue) else { throw SerializerError.invalid("Native content identity") } }
    var bounds=XmlLimits(); bounds.maxBytes=0; var inputRejected=false; var outputRejected=false
    do { _=try Content@SUFFIX@.decodeXml(bytes,bounds) } catch { inputRejected=true }
    do { _=try value.encodeXml(bounds) } catch { outputRejected=true }
    guard inputRejected && outputRejected else { throw SerializerError.invalid("Content resource bound") }; return output
}
'''.replace('@SUFFIX@', suffix)
    source += '''// Require invalid authored choice states to fail before returning an XML message.
var invalid=ContentSample(); var rejected=false
do { _=try invalid.encodeXml() } catch { rejected=true }
guard rejected else { throw SerializerError.invalid("Missing authored choice accepted") }
invalid.left=1; invalid.right=""; rejected=false
do { _=try invalid.encodeXml() } catch { rejected=true }
guard rejected else { throw SerializerError.invalid("Multiple authored choices accepted") }
var optional=ContentOptionalBatch(); _=try optional.encodeXml(); optional.left=1; optional.right=""; rejected=false
do { _=try optional.encodeXml() } catch { rejected=true }
guard rejected else { throw SerializerError.invalid("Multiple optional choices accepted") }
for argument in CommandLine.arguments.dropFirst() {
    let input=Array(try Data(contentsOf: URL(fileURLWithPath: argument))); let name=URL(fileURLWithPath: argument).lastPathComponent
    let valid=name.hasSuffix(".valid.xml"); let output: [UInt8]
    do {
        if name.hasPrefix("sample-") { output=try checkSample(input) }
        else if name.hasPrefix("optional-") { output=try checkOptionalBatch(input) }
        else if name.hasPrefix("price-") { output=try checkPrice(input) }
        else { output=try checkWrappers(input) }
    } catch { if valid { throw error }; continue }
    guard valid else { throw SerializerError.invalid("Invalid content candidate published") }
    try Data(output).write(to: URL(fileURLWithPath: argument+".swift.xml"))
}
print("Swift singleton choices and authoritative simpleContent/wrappers pass")
'''
    return source


def c_consumer():
    """Generate explicitly owned C probes with transactional invalid-input comparisons."""
    source = '''#include "schema.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Require actual native results and expose the failing oracle fixture. */
static void require(bool value, const char* message) { if (!value) { fprintf(stderr,"%s\\n",message); exit(EXIT_FAILURE); } }
'''
    for kind, suffix in TYPES.items():
        native = 'content_' + ('optional_batch' if kind == 'optional' else kind)
        source += '''/* Compare complete XML/native values and preserve destinations on failure. */
static void check_@TYPE@(const uint8_t* bytes, size_t size, bool valid, srl_buffer* output, const char* name) {
  @TYPE@ value={0},copy={0}; require(@TYPE@_init(&value) == srl_ok && @TYPE@_init(&copy) == srl_ok,"Content init");
  const @TYPE@ previous=value; const srl_status status=@TYPE@_decode_xml(&value,bytes,size,NULL);
  if (valid) {
    require(status == srl_ok,name); require(@TYPE@_encode_xml(&value,output,NULL) == srl_ok,"Content output");
    require(@TYPE@_decode_xml(&copy,output->data,output->size,NULL) == srl_ok,"Content roundtrip");
    const srl_protocol protocols[]={srl_json,srl_binary_none,srl_binary_integer,srl_binary_string};
    for (size_t index=0;index<sizeof(protocols)/sizeof(protocols[0]);++index) {
      srl_buffer first={0},second={0}; require(@TYPE@_encode(&value,protocols[index],&first) == srl_ok &&
          @TYPE@_encode(&copy,protocols[index],&second) == srl_ok && first.size == second.size && memcmp(first.data,second.data,first.size) == 0,"Native content identity");
      srl_buffer_free(&first); srl_buffer_free(&second);
    }
    srl_limits bounds=srl_default_limits(); bounds.max_bytes=0;
    require(@TYPE@_decode_xml(&copy,bytes,size,&bounds) != srl_ok,"Content byte bound");
  } else { require(status != srl_ok,name); require(memcmp(&previous,&value,sizeof(value)) == 0,"Partial content candidate published"); }
  @TYPE@_free(&value); @TYPE@_free(&copy);
}
'''.replace('@TYPE@', native)
    source += r'''/* Reject invalid authored states, then execute independent XML decisions. */
int main(int argc, char** argv) {
  content_sample invalid={0}; require(content_sample_init(&invalid) == srl_ok,"Authored sample init"); srl_buffer output={0};
  require(content_sample_encode_xml(&invalid,&output,NULL) != srl_ok,"Missing authored choice accepted");
  invalid.left.has_value=true; invalid.right.has_value=true;
  require(content_sample_encode_xml(&invalid,&output,NULL) != srl_ok,"Multiple authored choices accepted"); content_sample_free(&invalid);
  content_optional_batch optional={0}; require(content_optional_batch_init(&optional) == srl_ok,"Optional sample init");
  require(content_optional_batch_encode_xml(&optional,&output,NULL) == srl_ok,"Empty optional choice rejected"); srl_buffer_free(&output);
  optional.left.has_value=true; optional.right.has_value=true;
  require(content_optional_batch_encode_xml(&optional,&output,NULL) != srl_ok,"Multiple optional choices accepted"); content_optional_batch_free(&optional);
  for (int index=1;index<argc;++index) {
    const char* argument=argv[index]; FILE* file=fopen(argument,"rb"); require(file != NULL,"Open content fixture");
    require(fseek(file,0,SEEK_END) == 0,"Seek content fixture"); const long length=ftell(file); require(length >= 0,"Content fixture size"); rewind(file);
    uint8_t* bytes=(uint8_t*)malloc((size_t)length+1); require(bytes != NULL,"Content fixture allocation");
    require(fread(bytes,1,(size_t)length,file) == (size_t)length,"Read content fixture"); fclose(file);
    const bool valid=strstr(argument,".valid.xml") != NULL;
    if (strstr(argument,"sample-")) { check_content_sample(bytes,(size_t)length,valid,&output,argument); }
    else if (strstr(argument,"optional-")) { check_content_optional_batch(bytes,(size_t)length,valid,&output,argument); }
    else if (strstr(argument,"price-")) { check_content_price(bytes,(size_t)length,valid,&output,argument); }
    else { check_content_wrappers(bytes,(size_t)length,valid,&output,argument); }
    free(bytes);
    if (valid) {
      const size_t capacity=strlen(argument)+sizeof(".c.xml"); char* path=(char*)malloc(capacity); require(path != NULL,"Output path allocation");
      require(snprintf(path,capacity,"%s.c.xml",argument) >= 0,"Content output path"); file=fopen(path,"wb"); require(file != NULL,"Open content output"); free(path);
      require(fwrite(output.data,1,output.size,file) == output.size,"Write content output"); fclose(file);
    }
    srl_buffer_free(&output);
  }
  puts("C singleton choices and authoritative simpleContent/wrappers pass"); return EXIT_SUCCESS;
}
'''
    return source


def main():
    """Compile actual native SDKs and compare each output with the independent XSD and values."""
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
    if not languages or len(languages) != len(set(languages)) or any(value not in ('rust', 'c', 'kotlin', 'swift') for value in languages):
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
    (args.build / 'content.xsd').write_text(XSD, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    paths = []
    for index, (kind, document, valid) in enumerate(cases()):
        try:
            accepted = oracle.validate(etree.fromstring(document.encode()))
        except etree.XMLSyntaxError:
            accepted = False
        if accepted != valid:
            raise AssertionError(f'Unexpected independent oracle decision {index}: {oracle.error_log}')
        path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8'); paths.append(path)
    command = [args.compiler, '--input', args.build / 'model.serializer', '--config', args.build / 'options.ini',
               '--language', ','.join(languages), '--kotlin.package=']
    consumers = {'rust': ('main.rs', rust_consumer), 'c': ('main.c', c_consumer),
                 'kotlin': ('Main.kt', kotlin_consumer), 'swift': ('main.swift', swift_consumer)}
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command)
    runner = sdk.Runner(args)
    for language in languages:
        directory = args.build / language
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        filename, consumer = consumers[language]; (source / filename).write_text(consumer(), encoding='utf-8')
        if language == 'rust':
            for filename in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(ROOT / 'example/xml/rust' / filename, source / filename)
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        if language in ('rust', 'c'):
            launch = xml_runner.build_language(args, runner, language, directory, source_root=source.parent)
        else:
            launch = runner.build(language, 'xml_content', directory, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            output = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(output)))
                if semantic(etree.parse(str(path)).getroot()) != semantic(etree.parse(str(output)).getroot()):
                    raise AssertionError(f'XML scalar/choice/wrapper values changed: {path}')
            elif output.exists():
                raise AssertionError('Invalid content candidate published output')
    print(f'PASS: {len(paths)} independent choice/simpleContent/wrapper probes across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
