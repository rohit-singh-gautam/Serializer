# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify direct mobile foreign-only XML storage, namespace replay and exact slot bounds."""
import argparse
from pathlib import Path
import subprocess
from lxml import etree
import mobile_xml_lifecycle_test as shared
from python_xml_extension_test import DOCUMENT, NS

ROOT, sdk = shared.ROOT, shared.sdk
# Keep these native policy fixtures independent from changes to other backends' public adapters.
SCHEMA='''serializer version 1.20.0; namespace extension {
  class root { public uint32 id(1) xml(attribute); public array<uint32> items(2); public string tail(3); }
  class only_skip { public uint32 value(1); }}'''
CONFIG='''[xml]
enabled=true
[xml.class.extension::root]
namespace='''+NS+'''
name.items=item
extension.attributes.kind=attribute
extension.attributes.namespaces=urn:metadata urn:serializer:xml:model
extension.attributes.policy=preserve
extension.attributes.maximum=1
extension.before.kind=element
extension.before.before=items
extension.before.namespaces=urn:vendor
extension.before.policy=preserve
extension.before.minimum=1
extension.before.maximum=2
extension.end.kind=element
extension.end.namespaces=urn:end urn:serializer:xml:model
extension.end.policy=preserve
extension.end.maximum=1
extension.skip.kind=element
extension.skip.before=tail
extension.skip.namespaces=urn:skip
extension.skip.policy=skip
extension.reject.kind=element
extension.reject.namespaces=urn:reject
extension.reject.policy=reject
extensions.required=urn:vendor
[xml.class.extension::only_skip]
namespace='''+NS+'''
extension.skip.kind=element
extension.skip.namespaces=urn:skip
extension.skip.policy=skip
'''

KOTLIN = r'''@file:OptIn(ExperimentalUnsignedTypes::class)
import java.nio.file.Files
import java.nio.file.Path
// Require a policy or malformed document to fail outside the expected-error catch.
fun rejected(action: () -> Unit) { try { action() } catch (_: RuntimeException) { return }; error("Invalid extension accepted") }
// Declare understanding independently of permission to retain foreign vocabulary.
fun policy(limits: XmlExtensionLimits = XmlExtensionLimits(), prefixes: String = "") =
 XmlExtensionOptions(listOf("urn:vendor","urn:parent"),prefixes,limits)
// Exercise actual owning input/output and every native wire protocol without a retained known tree.
fun main(arguments: Array<String>) {
 val directory=Path.of(arguments[0]); val document=Files.readAllBytes(directory.resolve("document.xml"))
 val options=policy(); val value=ExtensionRoot.decodeXml(document,extensionOptions=options)
 check(value.id == 7u && value.items == listOf(1u,2u) && value.tail == "Z")
 val native=ExtensionRoot(); native.id=7u; native.items=mutableListOf(1u,2u); native.tail="Z"
 val before=Protocol.entries.map { value.encode(it) }
 for((index,protocol) in Protocol.entries.withIndex()) check(before[index].contentEquals(native.encode(protocol)))
 val output=value.encodeXml(extensionOptions=options); Files.write(directory.resolve("result.kotlin.xml"),output)
 for((index,protocol) in Protocol.entries.withIndex()) check(before[index].contentEquals(value.encode(protocol)))
 rejected { ExtensionRoot.decodeXml(document) }
 for(path in Files.list(directory).use { it.filter { it.fileName.toString().endsWith(".invalid.xml") }.toList() }) {
  rejected { ExtensionRoot.decodeXml(Files.readAllBytes(path),extensionOptions=options) }
 }
 rejected { ExtensionRoot.decodeXml(document,extensionOptions=policy(prefixes="missing")) }
 rejected { ExtensionRoot.decodeXml(document,extensionOptions=XmlExtensionOptions(listOf("urn:vendor"),"p")) }
 val required=policy(prefixes="p\r\np"); val accepted=ExtensionRoot.decodeXml(document,extensionOptions=required)
 ExtensionRoot.decodeXml(accepted.encodeXml(extensionOptions=required),extensionOptions=required)
 val skip=Files.readAllBytes(directory.resolve("skip.xml"))
 val stripped=ExtensionOnlySkip.decodeXml(skip,extensionOptions=policy(XmlExtensionLimits(maxPreservedBytes=0)))
 check(stripped.value == 1u && !String(stripped.encodeXml()).contains("discard"))
 var low=0L; var high=65536L
 while(low<high) { val middle=(low+high)/2; try { ExtensionRoot.decodeXml(document,extensionOptions=policy(XmlExtensionLimits(maxPreservedBytes=middle))); high=middle } catch(_: RuntimeException) { low=middle+1 } }
 check(low>0)
 val exact=policy(XmlExtensionLimits(maxSlots=5,maxItems=5,maxPreservedBytes=low))
 var current=ExtensionRoot.decodeXml(document,extensionOptions=exact); var previous: ByteArray?=null
 repeat(4) {
  val encoded=current.encodeXml(extensionOptions=exact)
  if(previous != null) check(previous!!.contentEquals(encoded)); previous=encoded
  current=ExtensionRoot.decodeXml(encoded,extensionOptions=exact)
 }
 rejected { ExtensionRoot.decodeXml(document,extensionOptions=policy(XmlExtensionLimits(maxItems=4))) }
 // Output contains four retained foreign roots; the fifth input root was intentionally skipped.
 for(bounds in listOf(XmlExtensionLimits(maxSlots=4),XmlExtensionLimits(maxItems=3),XmlExtensionLimits(maxPreservedBytes=low-1),XmlExtensionLimits(maxPreservedBytes=0))) {
  rejected { ExtensionRoot.decodeXml(document,extensionOptions=policy(bounds)) }
  rejected { value.encodeXml(extensionOptions=policy(bounds)) }
 }
 current.items.add(3u); check(ExtensionRoot.decodeXml(current.encodeXml(extensionOptions=options),extensionOptions=options).items == listOf(1u,2u,3u))
 current.items.clear(); rejected { current.encodeXml(extensionOptions=options) }
 rejected { XmlExtensionLimits(maxItems=-1) }
 rejected { XmlExtensionOptions(requiredPrefixes="p".repeat(8388609)) }
 val unicode=Files.readAllBytes(directory.resolve("unicode.xml")); val unicodeValue=ExtensionRoot.decodeXml(unicode,extensionOptions=options)
 Files.write(directory.resolve("unicode.kotlin.xml"),unicodeValue.encodeXml(extensionOptions=options))
 val nested=ExtensionEnvelope.decodeXml(Files.readAllBytes(directory.resolve("nested.xml")),extensionOptions=policy(XmlExtensionLimits(maxItems=5)))
 check(nested.first.items == listOf(1u,2u) && nested.second.id == 7u)
 Files.write(directory.resolve("nested.kotlin.xml"),nested.encodeXml(extensionOptions=policy(XmlExtensionLimits(maxItems=5))))
 val textual=ExtensionTextual.decodeXml(Files.readAllBytes(directory.resolve("textual.xml")),extensionOptions=options)
 check(textual.content == "A<&")
 Files.write(directory.resolve("textual.kotlin.xml"),textual.encodeXml(extensionOptions=options))
 println("Kotlin foreign-only slots, capabilities, native protocols and exact limits pass")
}'''

SWIFT = r'''import Foundation
// Require a policy or malformed document to fail outside the expected-error catch.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid extension accepted") }
// Preserve throwing checks without nonthrowing assertion autoclosures.
func require(_ condition: Bool) throws { guard condition else { throw SerializerError.invalid("Extension expectation failed") } }
// Declare understanding independently of permission to retain foreign vocabulary.
func policy(_ limits: XmlExtensionLimits = XmlExtensionLimits(), _ prefixes: String = "") -> XmlExtensionOptions {
 XmlExtensionOptions(understoodNamespaces:["urn:vendor","urn:parent"],requiredPrefixes:prefixes,limits:limits)
}
// Exercise actual owning input/output and every native wire protocol without a retained known tree.
let directory=URL(fileURLWithPath:CommandLine.arguments[1])
let document=[UInt8](try Data(contentsOf:directory.appendingPathComponent("document.xml")))
let options=policy(); let value=try ExtensionRoot.decodeXml(document,extensionOptions:options)
try require(value.id == 7 && value.items == [1,2] && value.tail == "Z")
var native=ExtensionRoot(); native.id=7; native.items=[1,2]; native.tail="Z"
let before=try Protocol.allCases.map { try value.encode($0) }
for(index,protocolValue) in Protocol.allCases.enumerated() { try require(before[index] == native.encode(protocolValue)) }
let output=try value.encodeXml(extensionOptions:options); try Data(output).write(to:directory.appendingPathComponent("result.swift.xml"))
for(index,protocolValue) in Protocol.allCases.enumerated() { try require(before[index] == value.encode(protocolValue)) }
try rejected { _=try ExtensionRoot.decodeXml(document) }
for path in try FileManager.default.contentsOfDirectory(at:directory,includingPropertiesForKeys:nil) where path.lastPathComponent.hasSuffix(".invalid.xml") {
 try rejected { _=try ExtensionRoot.decodeXml([UInt8](Data(contentsOf:path)),extensionOptions:options) }
}
try rejected { _=try ExtensionRoot.decodeXml(document,extensionOptions:policy(XmlExtensionLimits(),"missing")) }
try rejected { _=try ExtensionRoot.decodeXml(document,extensionOptions:XmlExtensionOptions(understoodNamespaces:["urn:vendor"],requiredPrefixes:"p")) }
let required=policy(XmlExtensionLimits(),"p\r\np"); let accepted=try ExtensionRoot.decodeXml(document,extensionOptions:required)
_=try ExtensionRoot.decodeXml(accepted.encodeXml(extensionOptions:required),extensionOptions:required)
let skip=[UInt8](try Data(contentsOf:directory.appendingPathComponent("skip.xml")))
let stripped=try ExtensionOnlySkip.decodeXml(skip,extensionOptions:policy(XmlExtensionLimits(maxPreservedBytes:0)))
try require(stripped.value == 1 && !String(decoding:stripped.encodeXml(),as:UTF8.self).contains("discard"))
var low=0,high=65536
while low<high {
 let middle=(low+high)/2
 do { _=try ExtensionRoot.decodeXml(document,extensionOptions:policy(XmlExtensionLimits(maxPreservedBytes:middle))); high=middle }
 catch { low=middle+1 }
}
try require(low>0)
let exact=policy(XmlExtensionLimits(maxSlots:5,maxItems:5,maxPreservedBytes:low))
var current=try ExtensionRoot.decodeXml(document,extensionOptions:exact); var previous: [UInt8]?=nil
for _ in 0..<4 {
 let encoded=try current.encodeXml(extensionOptions:exact)
 if let previous=previous { try require(previous == encoded) }; previous=encoded
 current=try ExtensionRoot.decodeXml(encoded,extensionOptions:exact)
}
try rejected { _=try ExtensionRoot.decodeXml(document,extensionOptions:policy(XmlExtensionLimits(maxItems:4))) }
// Output contains four retained foreign roots; the fifth input root was intentionally skipped.
for bounds in [XmlExtensionLimits(maxSlots:4),XmlExtensionLimits(maxItems:3),XmlExtensionLimits(maxPreservedBytes:low-1),XmlExtensionLimits(maxPreservedBytes:0)] {
 try rejected { _=try ExtensionRoot.decodeXml(document,extensionOptions:policy(bounds)) }
 try rejected { _=try value.encodeXml(extensionOptions:policy(bounds)) }
}
current.items.append(3); try require(ExtensionRoot.decodeXml(current.encodeXml(extensionOptions:options),extensionOptions:options).items == [1,2,3])
current.items=[]; try rejected { _=try current.encodeXml(extensionOptions:options) }
try rejected { _=try value.encodeXml(extensionOptions:policy(XmlExtensionLimits(maxItems:-1))) }
try rejected { _=try value.encodeXml(extensionOptions:XmlExtensionOptions(requiredPrefixes:String(repeating:"p",count:8388609))) }
let unicode=[UInt8](try Data(contentsOf:directory.appendingPathComponent("unicode.xml")))
try Data(ExtensionRoot.decodeXml(unicode,extensionOptions:options).encodeXml(extensionOptions:options)).write(to:directory.appendingPathComponent("unicode.swift.xml"))
let nested=try ExtensionEnvelope.decodeXml([UInt8](Data(contentsOf:directory.appendingPathComponent("nested.xml"))),extensionOptions:policy(XmlExtensionLimits(maxItems:5)))
try require(nested.first.items == [1,2] && nested.second.id == 7)
try Data(nested.encodeXml(extensionOptions:policy(XmlExtensionLimits(maxItems:5)))).write(to:directory.appendingPathComponent("nested.swift.xml"))
let textual=try ExtensionTextual.decodeXml([UInt8](Data(contentsOf:directory.appendingPathComponent("textual.xml"))),extensionOptions:options)
try require(textual.content == "A<&")
try Data(textual.encodeXml(extensionOptions:options)).write(to:directory.appendingPathComponent("textual.swift.xml"))
print("Swift foreign-only slots, capabilities, native protocols and exact limits pass")
'''


def binding_gates(args,languages,model,config):
    """Reject impossible native counters and scalar/child interleaving before replacing target files."""
    directory=args.build/'binding-gates'; directory.mkdir(exist_ok=True)
    for language in languages:
        maximum=str(1 << (63 if language=='swift' else 31))
        for label,options,diagnostic in [
            ('maximum',config.replace('extension.before.maximum=2','extension.before.maximum='+maximum),'native counter range'),
            ('minimum',config.replace('extension.before.minimum=1','extension.before.minimum='+maximum).replace('extension.before.maximum=2','extension.before.maximum='+maximum),'native counter range'),
            ('text-child',config+'extension.child.kind=element\nextension.child.namespaces=urn:vendor\nextension.child.policy=preserve\n','scalar text')]:
            schema=directory/(label+'.serializer'); schema.write_text(model,encoding='utf-8')
            ini=directory/(label+'.ini'); ini.write_text(options,encoding='utf-8')
            output=directory/(label+('.swift' if language=='swift' else '.kt')); output.write_bytes(b'preserved target\n')
            result=subprocess.run([str(args.compiler),'--input',str(schema),'--config',str(ini),'--language',language,
                                   f'--{language}.output',str(output),'--kotlin.package='],capture_output=True,text=True,encoding='utf-8')
            assert result.returncode and diagnostic in result.stdout+result.stderr,(language,label,result.stdout,result.stderr)
            assert output.read_bytes()==b'preserved target\n'


def main():
    """Compile both actual SDKs and independently inspect opaque namespaces and mixed content."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path); parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--language',default='kotlin,swift'); parser.add_argument('--wsl-languages',default='')
    parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args(); args.compiler=args.compiler.resolve(); args.build=args.build.resolve()
    languages=args.language.split(',')
    if not languages or len(languages)!=len(set(languages)) or any(value not in ('kotlin','swift') for value in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Scratch must remain beneath ignored out/')
    args.build.mkdir(parents=True,exist_ok=True)
    model=SCHEMA[:SCHEMA.rfind('}')]+'''class envelope { public root first; public root second; }
    class textual { public string content xml(text); }}'''
    (args.build/'model.serializer').write_text(model,encoding='utf-8')
    # Even an explicitly allowed reserved metadata URI must remain closed to foreign capture.
    config=CONFIG
    config+='[xml.class.extension::envelope]\nnamespace='+NS+'\n'
    config+='[xml.class.extension::textual]\nnamespace='+NS+'\nextension.attributes.kind=attribute\nextension.attributes.namespaces=urn:metadata\nextension.attributes.policy=preserve\n'
    (args.build/'options.ini').write_text(config,encoding='utf-8')
    text=DOCUMENT.decode(); (args.build/'document.xml').write_bytes(DOCUMENT)
    nested='<envelope xmlns="'+NS+'">'+text.replace('<root ','<first ',1).replace('</root>','</first>')+text.replace('<root ','<second ',1).replace('</root>','</second>')+'</envelope>'
    (args.build/'nested.xml').write_text(nested,encoding='utf-8')
    (args.build/'textual.xml').write_text('<textual xmlns="'+NS+'" xmlns:m="urn:metadata" xmlns:p="urn:parent" m:label="p:Thing">A&lt;&amp;</textual>',encoding='utf-8')
    (args.build/'unicode.xml').write_text(text.replace('head','headé😀'),encoding='utf-8')
    (args.build/'skip.xml').write_text('<only_skip xmlns="'+NS+'" xmlns:s="urn:skip"><value>1</value><s:discard>'+('discarded'*4096)+'</s:discard></only_skip>',encoding='utf-8')
    without=text[text.index('<v:before'):text.index('</v:before>')+len('</v:before>')]
    invalid=[text.replace('<v:between/>','<intruder/>'),text.replace('<v:between/>','<q:extra xmlns:q="urn:unlisted"/>'),
             text.replace('<v:between/>','<v:one/><v:two/>'),text.replace(without,'').replace('<v:between/>',''),
             text.replace('</root>','<r:denied xmlns:r="urn:reject"/></root>'),
             text.replace('<tail>Z</tail>','<tail>Z</tail><v:late/>'),text.replace('id="7"','id="7" unknown="x"'),
             text.replace('id="7"','id="7" z:revision="1" xmlns:z="urn:serializer:xml:model"'),
             text.replace('<e:after>kept</e:after>','<z:future xmlns:z="urn:serializer:xml:model"/>'),text+'<extra/>']
    for index,source in enumerate(invalid): (args.build/f'{index}.invalid.xml').write_text(source,encoding='utf-8')
    command=[args.compiler,'--input',args.build/'model.serializer','--config',args.build/'options.ini','--language',','.join(languages),'--kotlin.package=']
    for language in languages:
        directory=args.build/language; directory.mkdir(exist_ok=True)
        command += [f'--{language}.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command); runner=sdk.Runner(args)
    oracle=etree.XMLSchema(etree.fromstring(('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="'''+NS+'''" elementFormDefault="qualified">
    <xs:element name="root"><xs:complexType><xs:sequence><xs:choice minOccurs="0" maxOccurs="unbounded">
    <xs:any namespace="urn:vendor" processContents="skip"/><xs:element name="item" type="xs:unsignedInt"/>
    </xs:choice><xs:element name="tail" type="xs:string"/><xs:any namespace="urn:end" processContents="skip" minOccurs="0" maxOccurs="1"/>
    </xs:sequence><xs:attribute name="id" type="xs:unsignedInt" use="required"/><xs:anyAttribute namespace="urn:metadata" processContents="skip"/>
    </xs:complexType></xs:element></xs:schema>''').encode()))
    for language in languages:
        source=args.build/'source'/language; source.mkdir(parents=True,exist_ok=True)
        (source/('main.swift' if language=='swift' else 'Main.kt')).write_text(SWIFT if language=='swift' else KOTLIN,encoding='utf-8')
        launch=runner.build(language,'xml_extension',args.build/language,source)
        sdk.run([*launch,runner.path(language,args.build)])
        for prefix in ('result','unicode'):
            root=etree.parse(str(args.build/f'{prefix}.{language}.xml')).getroot()
            assert oracle.validate(root),str(oracle.error_log)
            assert root.attrib['{urn:metadata}label']=='p:Thing' and root.nsmap['p']=='urn:parent'
            assert [child.tag for child in root]==['{urn:vendor}before','{'+NS+'}item','{urn:vendor}between','{'+NS+'}item','{'+NS+'}tail','{urn:end}after']
            foreign=root[0]
            assert (foreign.text,foreign[0].tail,foreign[1].tail)==('head' if prefix=='result' else 'headé😀','tail','last')
            assert foreign.nsmap['n1']=='urn:another' and foreign.attrib['{urn:metadata}again']=='n1:Thing'
            assert foreign[0].tag=='{urn:inner}nest' and foreign[0].nsmap['p']=='urn:inner' and foreign[0].attrib['name']=='p:Thing'
            assert foreign[1].tag=='plain'
        nested=etree.parse(str(args.build/f'nested.{language}.xml')).getroot()
        assert [child.tag for child in nested]==['{'+NS+'}first','{'+NS+'}second']
        for owner in nested:
            assert owner.attrib['{urn:metadata}label']=='p:Thing' and owner[0].nsmap['p']=='urn:inner'
        textual=etree.parse(str(args.build/f'textual.{language}.xml')).getroot()
        assert textual.text=='A<&' and textual.attrib['{urn:metadata}label']=='p:Thing' and textual.nsmap['p']=='urn:parent'
    binding_gates(args,languages,model,config)
    print(f'PASS: positioned foreign-only XML, independent XSD, capabilities, {len(invalid)} rejection probes, three binding fences, exact bounds and native controls in {len(languages)} mobile SDKs',flush=True)


if __name__=='__main__':
    main()
