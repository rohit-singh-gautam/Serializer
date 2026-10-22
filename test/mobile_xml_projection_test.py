# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify mobile XML output from live native references and scoped translator fields."""
import argparse
from pathlib import Path
from lxml import etree
import mobile_xml_lifecycle_test as shared

ROOT, sdk = shared.ROOT, shared.sdk
MODEL = '''serializer version 1.20.0;
namespace projected {
  xml(namespace("urn:serializer:projected"));
  class payload { public uint32 number; public string label; }
  class source { public payload current;
    public translator payload as project { number=current.number; label=current.label; };
  }
  class envelope { public shared(source.current) value; public borrowed(source.current.label) label xml(attribute); }
  class weak_envelope { public weak(source.current) value; }
  class larger { public array<uint32> values; public payload nested; public string fallback { "default" }; }
  class larger_source { public array<uint32> values; public payload nested;
    public translator larger as project { values=values; nested=nested; };
  }
}'''
NAMES_MODEL='''namespace scoped {
  using schema "names.xsd";
  class names { public string value; public array<string> entry; }
  class source { public names nested;
    public translator names as project { value=nested.value; entry=nested.entry; };
  }
}'''
NAMES_XSD='''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:scoped" targetNamespace="urn:serializer:scoped" elementFormDefault="qualified">
<xs:complexType name="names"><xs:sequence><xs:element name="value" type="xs:QName"/><xs:element name="entry" type="xs:QName" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType>
<xs:element name="names" type="t:names"/><xs:element name="source"><xs:complexType><xs:sequence><xs:element name="nested" type="t:names"/></xs:sequence></xs:complexType></xs:element></xs:schema>'''

KOTLIN = r'''@file:OptIn(ExperimentalUnsignedTypes::class)
import java.nio.file.Files
import java.nio.file.Path
// Require live-reference and export-only input failures outside their expected-error catch.
fun rejected(action: () -> Unit) { try { action() } catch (_: RuntimeException) { return }; error("Invalid projection accepted") }
// Read the original nested/array objects through target getters without constructing a target.
fun main(arguments: Array<String>) {
 val directory=Path.of(arguments[0]); val source=ProjectedSource(); source.current.number=7u; source.current.label="A<&\""
 val envelope=ProjectedEnvelope(); envelope.value.bind(source); envelope.label.bind(source)
 check(envelope.value.readValue() === source.current)
 val before=Protocol.entries.map { envelope.encode(it) }; val direct=envelope.value.encodeXml()
 check(direct.contentEquals(source.current.encodeXml()))
 Files.write(directory.resolve("reference.kotlin.xml"),envelope.encodeXml())
 for((index,protocol) in Protocol.entries.withIndex()) {
  check(before[index].contentEquals(envelope.encode(protocol)))
  check(source.withProject { it.encode(protocol) }.contentEquals(source.current.encode(protocol)))
 }
 check(source.withProject { it.encodeXml() }.contentEquals(source.current.encodeXml()))
 // A successful provider that signals cancellation cannot publish readonly projection bytes.
 var completed=false; val canceled=IllegalStateException("Canceled inside projection validator")
 val operationLimits=XmlLimits().also { it.srlPackageContext=SrlXmlBudget.Operation({if(completed)throw canceled},{_,_,_->}) }
 val validator=XmlValidator { data, limits ->
  val candidate=ProjectedPayload.decodeXml(data,limits);check(candidate.number==7u && candidate.label==source.current.label);completed=true
 }
 var failure:Throwable?=null
 try { source.withProject { it.encodeXml(operationLimits,validator=validator) } } catch(error:Throwable){failure=error}
 check(completed && failure===canceled)
 source.current.number=0u; source.current.label="updated"
 check(ProjectedPayload.decodeXml(envelope.value.encodeXml()).number == 0u)
 rejected { ProjectedEnvelope.decodeXml(envelope.encodeXml()) }
 rejected { ProjectedEnvelope().encodeXml() }; envelope.value.unbind(); rejected { envelope.encodeXml() }
 val large=ProjectedLargerSource(); large.values=mutableListOf(1u,2u); large.nested.number=3u
 val output=large.withProject { view ->
  check(view.getValues() === large.values); check(view.getNested() === large.nested)
  view.encodeXml()
 }
 val copy=ProjectedLarger.decodeXml(output)
 check(copy.values == large.values && copy.nested.number == 3u && copy.fallback == "default")
 Files.write(directory.resolve("translator.kotlin.xml"),output)
 large.values[0]=9u; large.nested.number=5u
 check(ProjectedLarger.decodeXml(large.withProject { it.encodeXml() }).nested.number == 5u)
 val stress=ProjectedLargerSource(); stress.values=(0 until 25000).map { it.toUInt() }.toMutableList(); stress.nested.number=17u
 val values=stress.values; val nested=stress.nested; val native=Protocol.entries.map { stress.encode(it) }
 val stressed=stress.withProject { view -> check(view.getValues() === values && view.getNested() === nested); view.encodeXml() }
 check(stressed.contentEquals(stress.withProject { it.encodeXml(XmlLimits(maxBytes=stressed.size)) }))
 check(stressed.contentEquals(stress.withProject { it.encodeXml(XmlLimits(maxDepth=3)) }))
 rejected { stress.withProject { it.encodeXml(XmlLimits(maxDepth=2)) } }
 rejected { stress.withProject { it.encodeXml(XmlLimits(maxBytes=stressed.size-1)) } }
 val marker=IllegalStateException("Large projection interrupted"); var polls=0
 val canceledLimits=XmlLimits().withCancellation { if(++polls==100)throw marker }
 val sentinel=byteArrayOf(17); var published=sentinel; var interrupted:Throwable?=null
 try { published=stress.withProject { it.encodeXml(canceledLimits) } } catch(error:Throwable){interrupted=error}
 check(interrupted===marker && polls==100 && published===sentinel)
 check(stressed.contentEquals(stress.withProject { it.encodeXml() }))
 for((index,protocol) in Protocol.entries.withIndex())check(native[index].contentEquals(stress.encode(protocol)))
 Files.write(directory.resolve("large.kotlin.xml"),stressed)
 val names=ScopedSource(); names.nested=ScopedNames.decodeXml("<names xmlns='urn:serializer:scoped' xmlns:p='urn:outer'><value>p:root</value><entry xmlns:p='urn:first'>p:one</entry><entry xmlns:p='urn:second'>p:two</entry></names>".toByteArray())
 val namesNative=Protocol.entries.map { names.encode(it) }; val entries=names.nested.entry
 val namesOutput=names.withProject { view -> check(view.getEntry() === entries); view.encodeXml() }
 check(namesOutput.contentEquals(names.withProject { it.encodeXml(XmlLimits(maxBytes=namesOutput.size)) }))
 rejected { names.withProject { it.encodeXml(XmlLimits(maxBytes=namesOutput.size-1)) } }
 check(namesOutput.contentEquals(names.withProject { it.encodeXml() }))
 for ((index,protocol) in Protocol.entries.withIndex()) check(namesNative[index].contentEquals(names.encode(protocol)))
 Files.write(directory.resolve("names.kotlin.xml"),namesOutput)
 println("Kotlin live XML references, scoped getters, arrays and native wire identities pass")
}'''

SWIFT = r'''import Foundation
// Validate the actual projected fields before making cancellation observable to the caller.
final class ProjectionValidator: XmlValidator {
 var completed=false
 // Keep validation synchronous and use the caller's operation policy for the nested checks.
 func validate(_ bytes:[UInt8],_ limits:XmlLimits) throws {
  let value=try ProjectedPayload.decodeXml(bytes,limits)
  guard value.number==7 && value.label=="A<&\"" else { throw SerializerError.invalid("Projected provider fields differ") }
  completed=true
 }
}
enum ProjectionCancellation: Error { case canceled }
// Reject unexpectedly successful operations outside the catch for expected native failures.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid projection accepted") }
// Preserve throwing checks without nonthrowing assertion autoclosures.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Projection expectation failed") } }
// Keep owner identity stable while value fields and copy-on-write arrays are borrowed synchronously.
let directory=URL(fileURLWithPath:CommandLine.arguments[1])
let source=SrlReferenceOwner(ProjectedSource()); source.value.current.number=7; source.value.current.label="A<&\""
var envelope=ProjectedEnvelope(); envelope.value.bind(source); envelope.label.bind(source)
let before=try Protocol.allCases.map { try envelope.encode($0) }
let direct=try envelope.value.encodeXml(); try require(direct == source.value.current.encodeXml())
try Data(envelope.encodeXml()).write(to:directory.appendingPathComponent("reference.swift.xml"))
for (index,protocolValue) in Protocol.allCases.enumerated() {
 try require(before[index] == envelope.encode(protocolValue))
 try require(source.value.withProject { try $0.encode(protocolValue) } == source.value.current.encode(protocolValue))
}
try require(source.value.withProject { try $0.encodeXml() } == source.value.current.encodeXml())
let projectionValidator=ProjectionValidator();var operationLimits=XmlLimits()
operationLimits.srlPackageCheck={if projectionValidator.completed {throw ProjectionCancellation.canceled}}
var canceled=false
do { _=try source.value.withProject {try $0.encodeXml(operationLimits,validator:projectionValidator)} }
catch ProjectionCancellation.canceled {canceled=true}
try require(projectionValidator.completed && canceled)
source.value.current.number=0; source.value.current.label="updated"
try require(ProjectedPayload.decodeXml(envelope.value.encodeXml()).number == 0)
try rejected { _=try ProjectedEnvelope.decodeXml(envelope.encodeXml()) }
try rejected { _=try ProjectedEnvelope().encodeXml() }; envelope.value.unbind(); try rejected { _=try envelope.encodeXml() }
var weakOwner: SrlReferenceOwner<ProjectedSource>?=SrlReferenceOwner(ProjectedSource())
var weakEnvelope=ProjectedWeakEnvelope(); weakEnvelope.value.bind(weakOwner!); _=try weakEnvelope.encodeXml()
weakOwner=nil; try rejected { _=try weakEnvelope.encodeXml() }
var large=ProjectedLargerSource(); large.values=[1,2]; large.nested.number=3
let output=try large.withProject { try $0.encodeXml() }; let copy=try ProjectedLarger.decodeXml(output)
try require(copy.values == [1,2] && copy.nested.number == 3 && copy.fallback == "default")
try Data(output).write(to:directory.appendingPathComponent("translator.swift.xml"))
large.values[0]=9; large.nested.number=5
try require(ProjectedLarger.decodeXml(large.withProject { try $0.encodeXml() }).nested.number == 5)
var stress=ProjectedLargerSource(); stress.values=(0..<25000).map { UInt32($0) }; stress.nested.number=17
let nativeStress=try Protocol.allCases.map { try stress.encode($0) }
let stressStorageAddress=stress.values.withUnsafeBufferPointer { UInt(bitPattern:UnsafeRawPointer($0.baseAddress!)) }
let stressed=try stress.withProject { view in
 try view.getValues().withUnsafeBufferPointer { projected in
  try require(UInt(bitPattern:UnsafeRawPointer(projected.baseAddress!)) == stressStorageAddress)
  return try view.encodeXml()
 }
}
var exactStress=XmlLimits(); exactStress.maxBytes=stressed.count
try require(stressed == stress.withProject { try $0.encodeXml(exactStress) })
var depthStress=XmlLimits(); depthStress.maxDepth=3
try require(stressed == stress.withProject { try $0.encodeXml(depthStress) })
depthStress.maxDepth=2; try rejected { _=try stress.withProject { try $0.encodeXml(depthStress) } }
exactStress.maxBytes-=1; try rejected { _=try stress.withProject { try $0.encodeXml(exactStress) } }
var stressPolls=0
let canceledStress=XmlLimits().withCancellation { stressPolls+=1; if stressPolls==100 { throw ProjectionCancellation.canceled } }
let sentinelStress:[UInt8]=[17]; var publishedStress=sentinelStress; var interruptedStress=false
do { publishedStress=try stress.withProject { try $0.encodeXml(canceledStress) } } catch ProjectionCancellation.canceled { interruptedStress=true }
try require(interruptedStress && stressPolls==100 && publishedStress==sentinelStress)
try require(stressed == stress.withProject { try $0.encodeXml() })
for (index,protocolValue) in Protocol.allCases.enumerated() { try require(nativeStress[index] == stress.encode(protocolValue)) }
try Data(stressed).write(to:directory.appendingPathComponent("large.swift.xml"))
var names=ScopedSource()
names.nested=try ScopedNames.decodeXml(Array("<names xmlns='urn:serializer:scoped' xmlns:p='urn:outer'><value>p:root</value><entry xmlns:p='urn:first'>p:one</entry><entry xmlns:p='urn:second'>p:two</entry></names>".utf8))
let namesNative=try Protocol.allCases.map { try names.encode($0) }
let namesStorage=names.nested.entry.withUnsafeBufferPointer { UInt(bitPattern:UnsafeRawPointer($0.baseAddress!)) }
let namesOutput=try names.withProject { view in
 try view.getEntry().withUnsafeBufferPointer { values in
  try require(UInt(bitPattern:UnsafeRawPointer(values.baseAddress!)) == namesStorage)
  return try view.encodeXml()
 }
}
var exactNames=XmlLimits(); exactNames.maxBytes=namesOutput.count
try require(namesOutput == names.withProject { try $0.encodeXml(exactNames) })
exactNames.maxBytes-=1; try rejected { _=try names.withProject { try $0.encodeXml(exactNames) } }
try require(namesOutput == names.withProject { try $0.encodeXml() })
for (index,protocolValue) in Protocol.allCases.enumerated() { try require(namesNative[index] == names.encode(protocolValue)) }
try Data(namesOutput).write(to:directory.appendingPathComponent("names.swift.xml"))
print("Swift live XML owners, weak expiry, scoped array getters and native identities pass")
'''


def main():
    """Compile actual mobile outputs and inspect exported expanded names with an independent XML parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path); parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--language', default='kotlin,swift'); parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('kotlin', 'swift') for value in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Scratch must be beneath ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    model = args.build / 'model.serializer'; model.write_text(MODEL+NAMES_MODEL, encoding='utf-8')
    (args.build / 'names.xsd').write_text(NAMES_XSD,encoding='utf-8')
    command = [args.compiler, '--input', model, '--language', ','.join(languages), '--kotlin.package=',
               '--xml.runtime_xsd', 'true', '--xml.validation_scope', 'both']
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        (source / ('main.swift' if language == 'swift' else 'Main.kt')).write_text(SWIFT if language == 'swift' else KOTLIN, encoding='utf-8')
        launch = runner.build(language, 'xml_projection', args.build / language, source)
        sdk.run([*launch, runner.path(language, args.build)])
        names=etree.parse(str(args.build / f'names.{language}.xml')).getroot()
        identities=[]
        for child in names:
            prefix,local=child.text.split(':'); identities.append((child.nsmap[prefix],local))
        assert identities==[('urn:outer','root'),('urn:first','one'),('urn:second','two')]
        node = etree.parse(str(args.build / f'reference.{language}.xml')).getroot()
        assert node.tag == '{urn:serializer:projected}envelope' and node.get('label') == 'A<&"'
        assert [child.tag for child in node] == ['{urn:serializer:projected}value']
        assert [child.text for child in node[0]] == ['7', 'A<&"']
        node = etree.parse(str(args.build / f'translator.{language}.xml')).getroot()
        assert node.tag == '{urn:serializer:projected}larger'
        assert [child.text for child in node.findall('{urn:serializer:projected}values')] == ['1', '2']
        assert node.find('{urn:serializer:projected}fallback').text == 'default'
        node = etree.parse(str(args.build / f'large.{language}.xml')).getroot()
        assert node.tag == '{urn:serializer:projected}larger'
        assert [int(child.text) for child in node.findall('{urn:serializer:projected}values')] == list(range(25000))
        assert node.find('{urn:serializer:projected}nested')[0].text == '17'
        assert node.find('{urn:serializer:projected}fallback').text == 'default'
    print(f'PASS: live reference/scoped translator XML exports and native controls across {len(languages)} mobile SDKs', flush=True)


if __name__ == '__main__':
    main()
