"""Qualify generated unordered XML traversal against an independent XSD oracle."""
import argparse
import importlib.util
import itertools
import os
from pathlib import Path
import re
import shutil

from lxml import etree


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_all_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
provider_spec = importlib.util.spec_from_file_location('serializer_all_provider_sdk', ROOT / 'example/xml/run.py')
xml_runner = importlib.util.module_from_spec(provider_spec)
provider_spec.loader.exec_module(xml_runner)

XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:all"
targetNamespace="urn:serializer:all" elementFormDefault="qualified">
<xs:complexType name="detailType"><xs:all>
  <xs:element name="number" type="xs:unsignedInt"/>
  <xs:element name="note" type="xs:string" minOccurs="0"/>
</xs:all></xs:complexType>
<xs:element name="detail" type="t:detailType"/>
<xs:element name="sample"><xs:complexType><xs:all>
  <xs:element name="count" type="xs:int"/>
  <xs:element name="text" type="xs:string"/>
  <xs:element name="child" type="t:detailType"/>
  <xs:element name="extra" type="xs:boolean" minOccurs="0"/>
</xs:all><xs:attribute name="id" type="xs:unsignedInt" use="required"/>
<xs:attribute name="tag" type="xs:string"/></xs:complexType></xs:element>
<xs:element name="optional_batch"><xs:complexType><xs:all minOccurs="0">
  <xs:element name="value" type="xs:int"/>
  <xs:element name="extra" type="xs:string" minOccurs="0"/>
</xs:all></xs:complexType></xs:element>
<xs:element name="empty"><xs:complexType><xs:all/>
<xs:attribute name="id" type="xs:unsignedInt" use="required"/></xs:complexType></xs:element>
</xs:schema>'''

MODEL = '''serializer version 1.20.0;
namespace unordered {
  using schema "all.xsd";
  class detail { public uint32 number; public optional<string> note; }
  class sample {
    public int32 count;
    public string text;
    public detail child;
    public optional<bool> extra;
    public uint32 id;
    public optional<string> tag;
  }
  class optional_batch { public int32 value; public optional<string> extra; }
  class empty { public uint32 id; }
}
'''

JAVA = '''import java.nio.file.Files;
import java.nio.file.Path;
/** Check each oracle decision and preserve all decoded singleton values. */
public final class Main {
  /** Publish only valid candidates and retain deterministic declaration-order output. */
  public static void main(String[] args) throws Exception {
    for (String argument : args) {
      Path path = Path.of(argument); byte[] input = Files.readAllBytes(path);
      String name = path.getFileName().toString(); boolean valid = name.endsWith(".valid.xml");
      byte[] output;
      try {
        if (name.startsWith("sample")) {
          var value = Schema.Unordered.Sample.decodeXml(input);
          if (value.count != -7 || !value.text.equals("A&B") || value.child.number != -1 || !value.child.note.orElse("").equals("nested") || value.id != 19 || value.tag.isEmpty() || !value.tag.get().isEmpty()) throw new AssertionError("Decoded all values");
          boolean extra = new String(input, java.nio.charset.StandardCharsets.UTF_8).contains("<extra>");
          if (value.extra.isPresent() != extra || extra && !value.extra.get()) throw new AssertionError("Optional all branch presence");
          output = value.encodeXml();
          var defaults = Schema.Limits.defaults();
          var limited = new Schema.Limits(defaults.maxBytes, defaults.maxStringBytes, 1, defaults.maxDepth);
          try { value.encodeXml(limited, null); throw new AssertionError("Oversized all output published"); } catch (IllegalArgumentException expected) { }
          try { Schema.Unordered.Sample.decodeXml(input, limited, null); throw new AssertionError("Oversized all input published"); } catch (IllegalArgumentException expected) { }
        } else if (name.startsWith("optional")) output = Schema.Unordered.OptionalBatch.decodeXml(input).encodeXml();
        else output = Schema.Unordered.Empty.decodeXml(input).encodeXml();
      } catch (IllegalArgumentException expected) {
        if (valid) throw new AssertionError("Rejected valid all document: " + path, expected);
        continue;
      }
      if (!valid) throw new AssertionError("Invalid all candidate published: " + path);
      Files.write(Path.of(argument + ".java.xml"), output);
    }
    System.out.println("Java all permutations, optional groups, namespaces and transactional rejection pass");
  }
}
'''

CSHARP = '''using System;
using System.IO;
using static SerializerGenerated.Schema;
// Check each independent oracle decision and preserve typed singleton model values.
internal static class Program {
  // Write only complete canonical output from successfully decoded private candidates.
  private static void Main(string[] args) {
    foreach (string path in args) {
      byte[] input = File.ReadAllBytes(path); string name = Path.GetFileName(path);
      bool valid = name.EndsWith(".valid.xml", StringComparison.Ordinal); byte[] output;
      try {
        if (name.StartsWith("sample", StringComparison.Ordinal)) {
          var value = UnorderedSample.DecodeXml(input);
          if (value.Count != -7 || value.Text != "A&B" || value.Child.Number != uint.MaxValue || value.Child.Note?.Item1 != "nested" || value.Id != 19 || value.Tag?.Item1 != "") throw new Exception("Decoded all values");
          bool extra = System.Text.Encoding.UTF8.GetString(input).Contains("<extra>", StringComparison.Ordinal);
          if (value.Extra.HasValue != extra || extra && !value.Extra!.Value.Item1) throw new Exception("Optional all branch presence");
          output = value.EncodeXml();
          var limited = new Limits(maxElements: 1);
          try { value.EncodeXml(limited); throw new Exception("Oversized all output published"); } catch (FormatException) { }
          try { UnorderedSample.DecodeXml(input, limited); throw new Exception("Oversized all input published"); } catch (FormatException) { }
        } else if (name.StartsWith("optional", StringComparison.Ordinal)) output = UnorderedOptionalBatch.DecodeXml(input).EncodeXml();
        else output = UnorderedEmpty.DecodeXml(input).EncodeXml();
      } catch (FormatException expected) {
        if (valid) throw new Exception("Rejected valid all document: " + path, expected);
        continue;
      }
      if (!valid) throw new Exception("Invalid all candidate published: " + path);
      File.WriteAllBytes(path + ".csharp.xml", output);
    }
    Console.WriteLine("C# all permutations, optional groups, namespaces and transactional rejection pass");
  }
}
'''

GO = '''package main
import ("fmt"; "os"; "path/filepath"; "strings")
// Compare oracle decisions while requiring failed decodes to return no model candidate.
func main() {
  for _, path := range os.Args[1:] {
    input, err := os.ReadFile(path); if err != nil { panic(err) }
    name := filepath.Base(path); valid := strings.HasSuffix(name, ".valid.xml"); var output []byte
    if strings.HasPrefix(name, "sample") {
      value, failure := DecodeUnorderedSampleXML(input); err = failure
      if err != nil && value != nil { panic("Partial all candidate published") }
      if err == nil {
        if value.Count != -7 || value.Text != "A&B" || value.Child.Number != 4294967295 || value.Child.Note == nil || *value.Child.Note != "nested" || value.Id != 19 || value.Tag == nil || *value.Tag != "" { panic("Decoded all values") }
        extra := strings.Contains(string(input), "<extra>")
        if (value.Extra != nil) != extra || extra && !*value.Extra { panic("Optional all branch presence") }
        output, err = value.EncodeXML()
        limited := DefaultXmlOptions(); limited.Limits.MaxNodes = 1
        if partial, failure := value.EncodeXML(limited); partial != nil || failure == nil { panic("Oversized all output published") }
        if partial, failure := DecodeUnorderedSampleXML(input, limited); partial != nil || failure == nil { panic("Oversized all input published") }
      }
    } else if strings.HasPrefix(name, "optional") {
      value, failure := DecodeUnorderedOptionalBatchXML(input); err = failure
      if err != nil && value != nil { panic("Partial optional all candidate published") }
      if err == nil { output, err = value.EncodeXML() }
    } else {
      value, failure := DecodeUnorderedEmptyXML(input); err = failure
      if err != nil && value != nil { panic("Partial empty all candidate published") }
      if err == nil { output, err = value.EncodeXML() }
    }
    if valid != (err == nil) { panic(fmt.Sprintf("Oracle differs: %s: %v", path, err)) }
    if valid { if err := os.WriteFile(path + ".go.xml", output, 0600); err != nil { panic(err) } }
  }
  fmt.Println("Go all permutations, optional groups, namespaces and transactional rejection pass")
}
'''

KOTLIN = '''import java.nio.file.Files
import java.nio.file.Path
// Check independent oracle decisions through Kotlin's direct provider-token traversal.
fun main(args: Array<String>) {
    for (argument in args) {
        val path = Path.of(argument)
        val input = Files.readAllBytes(path)
        val name = path.fileName.toString()
        val valid = name.endsWith(".valid.xml")
        val output: ByteArray
        try {
            output = if (name.startsWith("sample")) {
                val value = UnorderedSample.decodeXml(input)
                if (value.count != -7 || value.text != "A&B" || value.child.number != UInt.MAX_VALUE ||
                    value.child.note.orElse("") != "nested" || value.id != 19u || !value.tag.isPresent || value.tag.get() != "") throw AssertionError("Decoded all values")
                val extra = input.toString(Charsets.UTF_8).contains("<extra>")
                if (value.extra.isPresent != extra || extra && !value.extra.get()) throw AssertionError("Optional all presence")
                if (runCatching { value.encodeXml(XmlLimits(maxNodes = 1)) }.isSuccess ||
                    runCatching { UnorderedSample.decodeXml(input, XmlLimits(maxNodes = 1)) }.isSuccess) throw AssertionError("All resource bound")
                value.encodeXml()
            } else if (name.startsWith("optional")) UnorderedOptionalBatch.decodeXml(input).encodeXml()
            else UnorderedEmpty.decodeXml(input).encodeXml()
        } catch (failure: Exception) {
            if (valid) throw AssertionError("Rejected valid all document: " + path, failure)
            continue
        }
        if (!valid) throw AssertionError("Invalid all candidate published: " + path)
        Files.write(Path.of(argument + ".kotlin.xml"), output)
    }
    println("Kotlin all permutations, optional groups, namespaces and transactional rejection pass")
}
'''


SWIFT = '''import Foundation
// Compare independent unordered-content decisions through direct owning Swift frames.
for argument in CommandLine.arguments.dropFirst() {
    let input = Array(try Data(contentsOf: URL(fileURLWithPath: argument)))
    let name = URL(fileURLWithPath: argument).lastPathComponent
    let valid = name.hasSuffix(".valid.xml")
    var output: [UInt8]? = nil
    do {
        if name.hasPrefix("sample") {
            let value = try UnorderedSample.decodeXml(input)
            guard value.count == -7 && value.text == "A&B" && value.child.number == UInt32.max &&
                  value.child.note == "nested" && value.id == 19 && value.tag == "" else { throw SerializerError.invalid("Decoded all values") }
            let extra = String(decoding: input, as: UTF8.self).contains("<extra>")
            guard (value.extra != nil) == extra && (!extra || value.extra!) else { throw SerializerError.invalid("Optional all presence") }
            var limited = XmlLimits(); limited.maxNodes = 1
            var rejectedInput = false; var rejectedOutput = false
            do { _ = try value.encodeXml(limited) } catch { rejectedOutput = true }
            do { _ = try UnorderedSample.decodeXml(input, limited) } catch { rejectedInput = true }
            guard rejectedInput && rejectedOutput else { throw SerializerError.invalid("All resource bound") }
            output = try value.encodeXml()
            guard try value.encode(.BINARY_NONE) == UnorderedSample.decodeXml(output!).encode(.BINARY_NONE) else { throw SerializerError.invalid("All native equality") }
        } else if name.hasPrefix("optional") {
            output = try UnorderedOptionalBatch.decodeXml(input).encodeXml()
        } else { output = try UnorderedEmpty.decodeXml(input).encodeXml() }
    } catch {
        if valid { throw error }
        continue
    }
    guard valid, let bytes = output else { throw SerializerError.invalid("Invalid all candidate published") }
    try Data(bytes).write(to: URL(fileURLWithPath: argument + ".swift.xml"))
}
print("Swift all permutations, optional groups, namespaces and transactional rejection pass")
'''


RUST = '''mod schema;
use schema::*;
/// Compare the independent all-group oracle with complete native Rust candidates.
fn main() -> Result<(),Box<dyn std::error::Error>> {
    for argument in std::env::args().skip(1) {
        let input=std::fs::read_to_string(&argument)?;
        let name=std::path::Path::new(&argument).file_name().unwrap().to_str().unwrap();
        let valid=name.ends_with(".valid.xml");
        let result=(|| -> Result<String,Error> {
            if name.starts_with("sample") {
                let value=UnorderedSample::decode_xml(&input)?;
                assert_eq!((value.count,value.text.as_str(),value.child.number,value.child.note.as_deref(),value.id,value.tag.as_deref()),(-7,"A&B",u32::MAX,Some("nested"),19,Some("")));
                let extra=input.contains("<extra>"); assert_eq!(value.extra.is_some(),extra); assert!(!extra || value.extra.unwrap());
                let limited=XmlLimits {max_nodes:1,..XmlLimits::default()};
                assert!(value.encode_xml_with_validation(limited,None).is_err());
                assert!(UnorderedSample::decode_xml_with_validation(&input,limited,None).is_err());
                let output=value.encode_xml()?;
                assert_eq!(value.encode(Protocol::BinaryNone)?,UnorderedSample::decode_xml(&output)?.encode(Protocol::BinaryNone)?);
                Ok(output)
            } else if name.starts_with("optional") { UnorderedOptionalBatch::decode_xml(&input)?.encode_xml() }
            else { UnorderedEmpty::decode_xml(&input)?.encode_xml() }
        })();
        match result {
            Ok(output)=> { assert!(valid,"Invalid all candidate published: {}",name); std::fs::write(argument+".rust.xml",output)?; },
            Err(error)=>assert!(!valid,"Rejected valid all document: {}: {:?}",name,error),
        }
    }
    println!("Rust all permutations, optional groups, namespaces and transactional rejection pass"); Ok(())
}
'''

C = r'''#include "schema.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Require actual typed/native results rather than treating every decoder error as a valid rejection. */
static void require(bool value, const char* message) { if (!value) { fprintf(stderr,"%s\n",message); exit(EXIT_FAILURE); } }
/* Compare each independent oracle decision and write only complete canonical candidates. */
int main(int argc, char** argv) {
  for (int index=1;index<argc;++index) {
    const char* argument=argv[index];
    FILE* file=fopen(argument,"rb"); require(file != NULL,"Open all fixture");
    require(fseek(file,0,SEEK_END) == 0,"Seek all fixture"); const long length=ftell(file); require(length >= 0,"All fixture size"); rewind(file);
    char* input=(char*)malloc((size_t)length+1); require(input != NULL,"All fixture allocation");
    require(fread(input,1,(size_t)length,file) == (size_t)length,"Read all fixture"); fclose(file); input[length]=0;
    const bool valid=strstr(argument,".valid.xml") != NULL; srl_buffer output={0}; srl_status status;
    if (strstr(argument,"sample-")) {
      unordered_sample value={0},copy={0}; require(unordered_sample_init(&value) == srl_ok && unordered_sample_init(&copy) == srl_ok,"All init");
      status=unordered_sample_decode_xml(&value,(const uint8_t*)input,(size_t)length,NULL);
      if (status == srl_ok) {
        require(value.count == -7 && strcmp(value.text.data,"A&B") == 0 && value.child.number == UINT32_MAX && value.child.note.has_value && strcmp(value.child.note.value.data,"nested") == 0 && value.id == 19 && value.tag.has_value && value.tag.value.size == 0,"Decoded all values");
        const bool extra=strstr(input,"<extra>") != NULL; require(value.extra.has_value == extra && (!extra || value.extra.value),"Optional all presence");
        srl_limits limited=srl_default_limits(); limited.max_elements=1; srl_buffer partial={0};
        require(unordered_sample_encode_xml(&value,&partial,&limited) != srl_ok && partial.size == 0,"All output bound"); srl_buffer_free(&partial);
        require(unordered_sample_decode_xml(&copy,(const uint8_t*)input,(size_t)length,&limited) != srl_ok,"All input bound");
        status=unordered_sample_encode_xml(&value,&output,NULL);
        require(status == srl_ok && unordered_sample_decode_xml(&copy,output.data,output.size,NULL) == srl_ok,"All roundtrip");
        srl_buffer first={0},second={0}; require(unordered_sample_encode(&value,srl_binary_none,&first) == srl_ok && unordered_sample_encode(&copy,srl_binary_none,&second) == srl_ok && first.size == second.size && memcmp(first.data,second.data,first.size) == 0,"All native equality"); srl_buffer_free(&first); srl_buffer_free(&second);
      }
      unordered_sample_free(&value); unordered_sample_free(&copy);
    } else if (strstr(argument,"optional-")) {
      unordered_optional_batch value={0}; require(unordered_optional_batch_init(&value) == srl_ok,"Optional all init");
      status=unordered_optional_batch_decode_xml(&value,(const uint8_t*)input,(size_t)length,NULL);
      if (status == srl_ok) { status=unordered_optional_batch_encode_xml(&value,&output,NULL); }
      unordered_optional_batch_free(&value);
    } else {
      unordered_empty value={0}; require(unordered_empty_init(&value) == srl_ok,"Empty all init");
      status=unordered_empty_decode_xml(&value,(const uint8_t*)input,(size_t)length,NULL);
      if (status == srl_ok) { status=unordered_empty_encode_xml(&value,&output,NULL); }
      unordered_empty_free(&value);
    }
    free(input); require(valid == (status == srl_ok),argument);
    if (valid) {
      const char suffix[]=".c.xml"; char* path=(char*)malloc(strlen(argument)+sizeof(suffix)); require(path != NULL,"All output path"); strcpy(path,argument); strcat(path,suffix);
      file=fopen(path,"wb"); require(file != NULL && fwrite(output.data,1,output.size,file) == output.size,"Write all output"); fclose(file); free(path);
    }
    srl_buffer_free(&output);
  }
  puts("C all permutations, optional groups, namespaces and transactional rejection pass"); return EXIT_SUCCESS;
}
'''


def oracle_cases(directory):
    """Pin permutations and malformed shapes independently of generated decoders."""
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    pieces = ['<count>-7</count>', '<text>A<![CDATA[&]]>B</text>',
              '<child><note>nested</note><number>4294967295</number></child>']
    start = '<sample xmlns="urn:serializer:all" id="19" tag="">'
    documents = [('sample', start + '\n'.join(order) + '</sample>', True)
                 for order in itertools.permutations(pieces)]
    documents.extend(('sample', start + ''.join(order) + '</sample>', True)
                     for order in itertools.permutations([*pieces, '<extra>1</extra>']))
    base = documents[0][1]
    documents.append(('sample', re.sub(r'<(/?)([a-z_]+)', r'<\1q:\2', base).replace(
        'xmlns="urn:serializer:all"', 'xmlns:q="urn:serializer:all"'), True))
    for changed in (
        base.replace(pieces[0], ''), base.replace(pieces[1], ''), base.replace(pieces[2], ''),
        base.replace('</sample>', '<extra>true</extra><extra>false</extra></sample>'),
        base.replace('</sample>', '<count>1</count></sample>'),
        base.replace('</sample>', '<unknown/></sample>'),
        base.replace('<count>', '<count xmlns="">'),
        base.replace('<count>', '<count attr="x">'), base.replace('-7', 'broken'),
        base.replace('<text>', '<text><unknown/>'),
        base.replace('<number>4294967295</number>', ''),
        base.replace('</child>', '<number>1</number></child>'),
        base.replace('id="19"', ''), base.replace('tag=""', 'unknown="x"'),
        base.replace('id="19"', 'id="19" id="20"'),
        base.replace('</sample>', 'unexpected</sample>'), base + '<second/>',
    ):
        documents.append(('sample', changed, False))
    documents += [
        ('optional', '<optional_batch xmlns="urn:serializer:all"/>', True),
        ('optional', '<optional_batch xmlns="urn:serializer:all"><value>4</value></optional_batch>', True),
        ('optional', '<optional_batch xmlns="urn:serializer:all"><extra/><value>4</value></optional_batch>', True),
        ('optional', '<optional_batch xmlns="urn:serializer:all"><extra/></optional_batch>', False),
        ('optional', '<optional_batch xmlns="urn:serializer:all"><value>4</value><value>5</value></optional_batch>', False),
        ('empty', '<empty xmlns="urn:serializer:all" id="1"/>', True),
        ('empty', '<empty xmlns="urn:serializer:all"/>', False),
        ('empty', '<empty xmlns="urn:serializer:all" id="1"><unknown/></empty>', False),
    ]
    paths = []
    for index, (kind, document, expected) in enumerate(documents):
        try:
            valid = oracle.validate(etree.fromstring(document.encode()))
        except etree.XMLSyntaxError:
            valid = False
        if valid != expected:
            raise AssertionError(f'Unexpected independent oracle decision for {index}: {oracle.error_log}')
        path = directory / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8')
        paths.append(path)
    return oracle, paths


def main():
    """Generate through the real compiler, require selected SDKs, and revalidate emitted documents."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,go,kotlin,swift,rust,c')
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(
            language not in ('java', 'csharp', 'go', 'kotlin', 'swift', 'rust', 'c') for language in languages):
        parser.error('Select distinct language names from java,csharp,go,kotlin,swift,rust,c')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve()
    os.environ.setdefault('CARGO_HOME', str(args.build / 'cargo-home'))
    (args.build / 'all.xsd').write_text(XSD, encoding='utf-8')
    model = args.build / 'all.serializer'
    model.write_text(MODEL, encoding='utf-8')
    oracle, paths = oracle_cases(args.build)
    runner = sdk.Runner(args)
    sources = {'java': ('Main.java', JAVA), 'csharp': ('Program.cs', CSHARP), 'go': ('main.go', GO),
               'kotlin': ('Main.kt', KOTLIN), 'swift': ('main.swift', SWIFT),
               'rust': ('main.rs', RUST), 'c': ('main.c', C)}
    command = [args.compiler, '--input', model, '--language', ','.join(languages),
               '--protocols', 'binary_none', '--go.package', 'main', '--kotlin.package=']
    for language in languages:
        directory = args.build / language
        directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command)
    for language in languages:
        source = args.build / 'source' / language
        source.mkdir(parents=True, exist_ok=True)
        filename, content = sources[language]
        (source / filename).write_text(content, encoding='utf-8')
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        if language == 'rust':
            for filename in ('Cargo.toml', 'Cargo.lock'):
                shutil.copyfile(ROOT / 'example/xml/rust' / filename, source / filename)
        if language in ('rust', 'c'):
            launch = xml_runner.build_language(args, runner, language, args.build / language, source_root=source.parent)
        else:
            launch = runner.build(language, 'xml_all', args.build / language, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if not path.name.endswith('.valid.xml'):
                if emitted.exists():
                    raise AssertionError(f'Invalid input published output: {emitted}')
                continue
            oracle.assertValid(etree.parse(str(emitted)))
            children = [etree.QName(node).localname for node in etree.parse(str(emitted)).getroot()]
            if path.name.startswith('sample') and children not in (
                    ['count', 'text', 'child'], ['count', 'text', 'child', 'extra']):
                raise AssertionError(f'Declaration order changed: {children}')
    print(f'PASS: {len(paths)} independent all-group probes across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
