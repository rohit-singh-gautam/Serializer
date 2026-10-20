# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify real Kotlin/Swift QName, lexical-list and prefix-list namespace round trips."""
import argparse
import importlib.util
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_namespace_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
NAMESPACE = 'urn:serializer:mobile-namespaces'
MODEL = '''serializer version 1.20.0; namespace names {
  using schema "names.xsd";
  class document {
    public string value;
    public array<string> names xml(list);
    public optional<string> optional;
    public array<string> entry;
    public optional<string> qualified xml(attribute, namespace("urn:serializer:mobile-namespaces"), qname);
    public optional<string> prefixes xml(attribute, prefix_list);
  }
  class text { public string value xml(text, qname); }
}'''
XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="'''+NAMESPACE+'''"
 targetNamespace="'''+NAMESPACE+'''" elementFormDefault="qualified">
 <xs:simpleType name="Names"><xs:list itemType="xs:QName"/></xs:simpleType>
 <xs:element name="document"><xs:complexType><xs:sequence>
   <xs:element name="value" type="xs:QName"/><xs:element name="names" type="t:Names"/>
   <xs:element name="optional" type="xs:QName" minOccurs="0"/>
   <xs:element name="entry" type="xs:QName" minOccurs="0" maxOccurs="unbounded"/>
 </xs:sequence><xs:attribute name="qualified" type="xs:QName" form="qualified"/>
 <xs:attribute name="prefixes" type="xs:string"/></xs:complexType></xs:element>
 <xs:element name="text"><xs:complexType><xs:simpleContent><xs:extension base="xs:QName"/>
 </xs:simpleContent></xs:complexType></xs:element>
</xs:schema>'''

KOTLIN = '''import java.nio.file.Files
import java.nio.file.Path
// Assert failed XML operations separately from successful-publication assertions.
fun rejected(action: () -> Unit) { try { action() } catch (_: IllegalArgumentException) { return }; error("Invalid namespace output published") }
// Preserve native payload data while generated XML-only sidecars retain each lexical namespace scope.
fun main(arguments: Array<String>) {
  for (path in arguments) {
    val input=Files.readAllBytes(Path.of(path)); val valid=path.endsWith(".valid.xml")
    if (Path.of(path).fileName.toString().startsWith("text")) {
      val value=try { NamesText.decodeXml(input) } catch (error: IllegalArgumentException) { if (valid) throw error; continue }
      check(valid); val native=value.encode(Protocol.BINARY_NONE); val output=value.encodeXml()
      check(native.contentEquals(NamesText.decodeXml(output).encode(Protocol.BINARY_NONE)))
      Files.write(Path.of(path+".kotlin.xml"),output)
      continue
    }
    val value=try { NamesDocument.decodeXml(input) } catch (error: IllegalArgumentException) { if (valid) throw error; continue }
    check(valid); val native=value.encode(Protocol.BINARY_NONE); val output=value.encodeXml()
    check(output.contentEquals(value.encodeXml()))
    check(native.contentEquals(NamesDocument.decodeXml(output).encode(Protocol.BINARY_NONE)))
    Files.write(Path.of(path+".kotlin.xml"),output)
    val before=value.value; value.value="unbound:item"; rejected { value.encodeXml() }; value.value=before
    rejected { value.xmlBindNamespace("xml","urn:wrong") }
    check(output.contentEquals(value.encodeXml()))
    rejected { NamesDocument.decodeXml(input,XmlLimits(maxNodes=1)) }
    if (Path.of(path).fileName.toString().startsWith("copies")) {
      rejected { NamesDocument.decodeXml(input,XmlLimits(maxNodes=90)) }
    }
    if (Path.of(path).fileName.toString().startsWith("absence")) {
      value.xmlBindNamespace("","urn:edited-parent")
      check(native.contentEquals(value.encode(Protocol.BINARY_NONE)))
      Files.write(Path.of(path+".kotlin.xml"),value.encodeXml())
    }
  }
  val authored=NamesDocument(); authored.xmlBindNamespace("p","urn:authored")
  authored.value="p:item"; authored.names=mutableListOf("p:first","p:second")
  check(NamesDocument.decodeXml(authored.encodeXml()).value == "p:item")
  println("Kotlin QName, QName-list, prefix-list and namespace sidecar checks pass")
}'''

SWIFT = '''import Foundation
// Keep throwing assertions independent of catches that expect an invalid XML operation.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Namespace expectation failed") } }
// Refuse successful publication when lexical namespace validation should fail.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid namespace output published") }
// Exercise source-scope preservation with actual Foundation input and generated token output.
for path in CommandLine.arguments.dropFirst() {
  let input=Array(try Data(contentsOf:URL(fileURLWithPath:path))); let valid=path.hasSuffix(".valid.xml")
  if URL(fileURLWithPath:path).lastPathComponent.hasPrefix("text") {
    let value: NamesText
    do { value=try NamesText.decodeXml(input) } catch { if valid { throw error }; continue }
    try require(valid); let native=try value.encode(.BINARY_NONE), output=try value.encodeXml()
    try require(native == NamesText.decodeXml(output).encode(.BINARY_NONE))
    try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml")); continue
  }
  var value: NamesDocument
  do { value=try NamesDocument.decodeXml(input) } catch { if valid { throw error }; continue }
  try require(valid); let native=try value.encode(.BINARY_NONE), output=try value.encodeXml()
  try require(output == value.encodeXml())
  try require(native == NamesDocument.decodeXml(output).encode(.BINARY_NONE))
  try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))
  let before=value.value; value.value="unbound:item"; try rejected { _=try value.encodeXml() }; value.value=before
  try rejected { try value.xmlBindNamespace("xml","urn:wrong") }
  try require(output == value.encodeXml())
  var limits=XmlLimits(); limits.maxNodes=1; try rejected { _=try NamesDocument.decodeXml(input,limits) }
  if URL(fileURLWithPath:path).lastPathComponent.hasPrefix("copies") {
    limits.maxNodes=90; try rejected { _=try NamesDocument.decodeXml(input,limits) }
  }
  if URL(fileURLWithPath:path).lastPathComponent.hasPrefix("absence") {
    try value.xmlBindNamespace("","urn:edited-parent")
    try require(native == value.encode(.BINARY_NONE))
    try Data(value.encodeXml()).write(to:URL(fileURLWithPath:path+".swift.xml"))
  }
}
var authored=NamesDocument(); try authored.xmlBindNamespace("p","urn:authored")
authored.value="p:item"; authored.names=["p:first","p:second"]
let authoredXml=try authored.encodeXml(); try require(NamesDocument.decodeXml(authoredXml).value == "p:item")
print("Swift QName, QName-list, prefix-list and namespace sidecar checks pass")'''


def expanded(value, scope):
    """Resolve an independent provider's lexical QName without comparing presentation prefixes."""
    pieces = value.strip(' \t\r\n').split(':')
    return (scope.get(pieces[0], ''), pieces[1]) if len(pieces) == 2 else (scope.get(None, ''), pieces[0])


def compare(source, output):
    """Compare every typed scalar and qualified attribute in the original and regenerated scopes."""
    first, second = etree.fromstring(source), etree.fromstring(output)
    if etree.QName(first).localname == 'text':
        if expanded(first.text, first.nsmap) != expanded(second.text, second.nsmap):
            raise AssertionError('Simple-content QName identity changed')
        return
    for left, right in zip(first, second):
        if left.tag != right.tag:
            raise AssertionError('Expanded element name changed')
        left_values, right_values = (left.text or '').split(), (right.text or '').split()
        if [expanded(value, left.nsmap) for value in left_values] != [expanded(value, right.nsmap) for value in right_values]:
            raise AssertionError('Child QName/list identities changed')
    if len(first) != len(second):
        raise AssertionError('Child count changed')
    name = '{' + NAMESPACE + '}qualified'
    if name in first.attrib and expanded(first.attrib[name], first.nsmap) != expanded(second.attrib[name], second.nsmap):
        raise AssertionError('Qualified attribute QName identity changed')
    if 'prefixes' in first.attrib:
        if [first.nsmap[prefix] for prefix in first.attrib['prefixes'].split()] != [second.nsmap[prefix] for prefix in second.attrib['prefixes'].split()]:
            raise AssertionError('Prefix-list identities changed')


def execute(args):
    """Use a disposable admitted directory for the actual generated SDKs and independent corpus."""
    languages = args.language.split(',')
    schema = args.build / 'names.xsd'
    schema.write_text(XSD, encoding='utf-8')
    model = args.build / 'names.serializer'
    model.write_text(MODEL, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    texts = (
        '<document xmlns="'+NAMESPACE+'" xmlns:a="'+NAMESPACE+'" xmlns:p="urn:parent" a:qualified="p:root" prefixes="p a">\n <value xmlns:p="urn:left">p:item</value>\n <names xmlns:p="urn:list">p:first p:second</names>\n <optional xmlns:p="urn:optional">p:item</optional><entry xmlns:p="urn:first">p:item</entry><entry xmlns:p="urn:second">p:item</entry></document>',
        '<t:document xmlns:t="'+NAMESPACE+'" xmlns="urn:parent"><t:value xmlns="urn:child">item</t:value><t:names xmlns="">first second</t:names><t:entry xmlns:名="urn:unicode">名:項目</t:entry></t:document>',
        '<document xmlns="'+NAMESPACE+'"><value xmlns:srl="urn:collision">srl:item</value><names xmlns:q="urn:same" xmlns:alias="urn:same">q:first alias:second</names></document>',
        '<text xmlns="'+NAMESPACE+'" xmlns:名="urn:unicode"> 名:項目 </text>',
    )
    probes = [(str(index), text.encode(), True) for index, text in enumerate(texts)]
    probes[-1] = ('text', texts[-1].encode(), True)
    probes += [('copies', ('<document xmlns="'+NAMESPACE+'" xmlns:p="urn:parent"><value>p:item</value><names>p:a p:b</names>' + '<entry>p:item</entry>' * 20 + '</document>').encode(), True)]
    probes += [('absence', ('<t:document xmlns:t="'+NAMESPACE+'"><t:value>local</t:value><t:names>first second</t:names></t:document>').encode(), True)]
    probes += [('unbound', texts[0].replace('p:item</value>', 'missing:item</value>').encode(), False),
               ('prefixes', texts[0].replace('prefixes="p a"', 'prefixes="missing"').encode(), False),
               ('list', texts[0].replace('p:first p:second', 'p:first missing:second').encode(), False),
               ('text-unbound', ('<text xmlns="'+NAMESPACE+'">missing:item</text>').encode(), False)]
    paths = []
    for name, data, valid in probes:
        if name != 'prefixes' and oracle.validate(etree.fromstring(data)) != valid:
            raise AssertionError('Independent XSD oracle disagrees: '+name)
        path = args.build / (name + ('.valid.xml' if valid else '.invalid.xml'))
        path.write_bytes(data)
        paths.append(path)
    command = [args.compiler, '--input', model, '--language', ','.join(languages), '--protocols', 'binary_none', '--kotlin.package=']
    for language in languages:
        target = args.build / language
        target.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
    sdk.run(command)
    runner = sdk.Runner(args)
    for language in languages:
        source = args.build / 'source' / language
        source.mkdir(parents=True, exist_ok=True)
        (source / ('main.swift' if language == 'swift' else 'Main.kt')).write_text(SWIFT if language == 'swift' else KOTLIN, encoding='utf-8')
        for path in paths:
            Path(str(path) + '.' + language + '.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_namespaces', args.build / language, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path, (_, original, valid) in zip(paths, probes):
            emitted = Path(str(path) + '.' + language + '.xml')
            if valid:
                encoded = emitted.read_bytes()
                if not oracle.validate(etree.fromstring(encoded)):
                    raise AssertionError('Independent output validation failed: '+str(emitted))
                compare(original, encoded)
            elif emitted.exists():
                raise AssertionError('Invalid input published output: '+str(emitted))
    print(f'PASS: {len(probes)} namespace probes across {len(languages)} real SDKs', flush=True)


def main():
    """Generate and exercise the SDKs with explicit providers, then remove disposable build artifacts."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='kotlin,swift')
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--python-dependencies', type=Path)
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(item not in ('kotlin', 'swift') for item in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    global etree
    from lxml import etree
    with tempfile.TemporaryDirectory(prefix='mobile-xml-namespaces-', dir=args.build) as temporary:
        args.build = Path(temporary)
        execute(args)


if __name__ == '__main__':
    main()
