# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify mobile expanded QName enumeration with shared independent XSD decisions."""
import argparse
from pathlib import Path
import sys
import tempfile

import go_xml_qname_constraints_test as shared

MODEL = '''serializer version 1.20.0;
namespace names {
  using schema "names.xsd";
  class record {
    public string kind (1) xml(attribute, qname);
    public array[2] string attribute_names (2) xml(attribute, name("names"), list);
    public array<string> values (3) xml(name("value"));
    public array[2] string names (4) xml(name("child_names"), list);
    public string lexical (5);
  }
  class patterned {
    public string value (1) xml(attribute);
    public string prefixes (2) xml(attribute, prefix_list);
    public array[2] string names (3) xml(list);
  }
}
'''
KOTLIN = '''import java.nio.file.Files
import java.nio.file.Path
// Require failure outside the expected-error catch so successful publication cannot pass.
fun rejected(action: () -> Unit) { try { action() } catch (_: IllegalArgumentException) { return }; error("Invalid QName output published") }
// Compare all native protocols and require byte-stable native fields through each XML round trip.
fun main(arguments: Array<String>) {
  for(path in arguments) {
    val valid=path.endsWith(".valid.xml"); val bytes=Files.readAllBytes(Path.of(path)); val output: ByteArray
    try {
      if(Path.of(path).fileName.toString().startsWith("pattern-")) {
        val value=NamesPatterned.decodeXml(bytes); val native=Protocol.entries.map { value.encode(it) }
        output=value.encodeXml(); val decoded=NamesPatterned.decodeXml(output)
        for((index,protocol) in Protocol.entries.withIndex()) {
          check(native[index].contentEquals(value.encode(protocol)))
          check(native[index].contentEquals(decoded.encode(protocol)))
        }
        value.value="p:wrong"; rejected { value.encodeXml() }
      } else {
        val value=NamesRecord.decodeXml(bytes); val native=Protocol.entries.map { value.encode(it) }
        output=value.encodeXml(); val decoded=NamesRecord.decodeXml(output)
        for((index,protocol) in Protocol.entries.withIndex()) {
          check(native[index].contentEquals(value.encode(protocol)))
          check(native[index].contentEquals(decoded.encode(protocol)))
        }
        val saved=value.kind; value.kind="p:wrong"; rejected { value.encodeXml() }; value.kind=saved
        value.lexical="p:two p:one"; rejected { value.encodeXml() }
      }
    } catch(failure: IllegalArgumentException) { if(valid)throw failure; continue }
    check(valid); Files.write(Path.of(path+".kotlin.xml"),output)
  }
  println("Kotlin expanded QName enumeration, ordered lists, patterns and four native protocols pass")
}
'''
SWIFT = '''import Foundation
// Keep failed expectations separate from catches for rejected XML values.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("QName constraint expectation failed") } }
// Require failed complete output without accepting an assertion thrown by the test itself.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid QName output published") }
// Retain exact native bytes through scoped namespace output, including every supported native protocol.
for path in CommandLine.arguments.dropFirst() {
  let valid=path.hasSuffix(".valid.xml"), bytes=Array(try Data(contentsOf:URL(fileURLWithPath:path)))
  let output: [UInt8]
  do {
    if URL(fileURLWithPath:path).lastPathComponent.hasPrefix("pattern-") {
      var value=try NamesPatterned.decodeXml(bytes); let native=try Protocol.allCases.map { try value.encode($0) }
      output=try value.encodeXml(); let decoded=try NamesPatterned.decodeXml(output)
      for (index,protocolValue) in Protocol.allCases.enumerated() {
        try require(native[index] == value.encode(protocolValue))
        try require(native[index] == decoded.encode(protocolValue))
      }
      value.value="p:wrong"; try rejected { _=try value.encodeXml() }
    } else {
      var value=try NamesRecord.decodeXml(bytes); let native=try Protocol.allCases.map { try value.encode($0) }
      output=try value.encodeXml(); let decoded=try NamesRecord.decodeXml(output)
      for (index,protocolValue) in Protocol.allCases.enumerated() {
        try require(native[index] == value.encode(protocolValue))
        try require(native[index] == decoded.encode(protocolValue))
      }
      let saved=value.kind; value.kind="p:wrong"; try rejected { _=try value.encodeXml() }; value.kind=saved
      value.lexical="p:two p:one"; try rejected { _=try value.encodeXml() }
    }
  } catch { if valid { throw error }; continue }
  try require(valid); try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))
}
print("Swift expanded QName enumeration, ordered lists, patterns and four native protocols pass")
'''


def execute(args, etree):
    """Compile actual SDKs and validate their unmodified emitted XML with independent providers."""
    sdk = shared.sdk
    (args.build / 'names.xsd').write_text(shared.XSD, encoding='utf-8')
    (args.build / 'model.serializer').write_text(MODEL, encoding='utf-8')
    languages = args.language.split(',')
    command = [args.compiler, '--input', args.build / 'model.serializer', '--language', args.language]
    for language in languages:
        directory = args.build / language
        directory.mkdir()
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command)
    schema = etree.XMLSchema(etree.fromstring(shared.XSD.encode()))
    probes = [probe for probe in shared.documents()
              if not probe[0].startswith(('defaults-', 'unions-', 'owner-'))]
    paths = []
    for name, text, valid in probes:
        assert schema.validate(etree.fromstring(text.encode())) == valid, (name, schema.error_log)
        path = args.build / f'{name}.{"valid" if valid else "invalid"}.xml'
        path.write_text(text, encoding='utf-8')
        paths.append(path)
    runner = sdk.Runner(args)
    outputs = []
    for language in languages:
        source = args.build / f'source-{language}'
        source.mkdir()
        filename, body = ('Main.kt', KOTLIN) if language == 'kotlin' else ('main.swift', SWIFT)
        (source / filename).write_text(body, encoding='utf-8')
        launch = runner.build(language, 'mobile_qname_constraints', args.build / language, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for (name, text, valid), path in zip(probes, paths):
            emitted = Path(str(path) + f'.{language}.xml')
            if valid:
                outputs.append(emitted)
                node = etree.parse(str(emitted)).getroot()
                # lxml misclassifies no-namespace QName enumeration after repeated xmlns="".
                # Compare actual expanded names here; JAXP validates every original output below.
                original = etree.fromstring(text.encode())
                if name.startswith('pattern-'):
                    assert node.get('value') == original.get('value')
                    assert shared.expanded(node, node.get('value')) == shared.expanded(original, original.get('value'))
                    assert node.get('prefixes') == 'p srl'
                    assert [node.nsmap[prefix] for prefix in ('p', 'srl')] == ['urn:allowed', 'urn:foreign']
                    assert node[0].text == 'p:one srl:two'
                else:
                    assert shared.identities(node) == shared.identities(original)
            else:
                assert not emitted.exists()
    # JAXP checks the original fixture and original emitted documents separately from lxml.
    oracle = args.build / 'oracle'
    oracle.mkdir()
    (oracle / 'Main.java').write_text(shared.JAVA_ORACLE, encoding='utf-8')
    sdk.run(runner.command('java', 'javac', '--release', '17', '-encoding', 'UTF-8', '-Xlint:all',
                           '-Werror', '-d', runner.path('java', oracle), runner.path('java', oracle / 'Main.java')))
    sdk.run(runner.command('java', 'java', '-cp', runner.path('java', oracle), 'Main',
                           runner.path('java', args.build / 'names.xsd'),
                           *(runner.path('java', path) for path in [*paths, *outputs])))
    print(f'PASS: {len(probes)} QName enumeration/pattern probes across {len(languages)} actual mobile SDKs')


def main():
    """Use ignored temporary generation directories while preserving a caller's verification logs."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--python-dependencies', type=Path)
    parser.add_argument('--language', default='kotlin,swift')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    languages = args.language.split(',')
    if len(languages) != len(set(languages)) or any(language not in ('kotlin', 'swift') for language in languages):
        parser.error('Select distinct names from kotlin,swift')
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.build == shared.ROOT or shared.ROOT in args.build.parents and 'out' not in args.build.relative_to(shared.ROOT).parts:
        parser.error('In-repository output must be beneath ignored out/')
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree
    args.build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mobile-qname-constraints-', dir=args.build) as directory:
        args.build = Path(directory)
        execute(args, etree)


if __name__ == '__main__':
    main()
