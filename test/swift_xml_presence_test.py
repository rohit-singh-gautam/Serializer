"""Qualify native Swift Optional states with the shared independent presence corpus."""
import argparse
import importlib.util
from pathlib import Path

from lxml import etree
from portable_xml_presence_test import MODEL, OPTIONS, XSD, BOUND_XSD, RICH_MODEL, RICH_OPTIONS, RICH_XSD, cases, rich_cases

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_swift_presence_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

MAIN = '''import Foundation
// Assert native Optional layers independently of empty lexical and collection payloads.
let start = "<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>"
let end = "<nested xsi:nil='true'/></sample>"
let absent = try NilFixtureSample.decodeXml(Array((start + "<nil_value xsi:nil='true'/>" + end).utf8))
let nilValue = try NilFixtureSample.decodeXml(Array((start + "<nil_value/><states xsi:nil='true'/><entries xsi:nil='true'/>" + end).utf8))
let empty = try NilFixtureSample.decodeXml(Array((start + "<nil_value/><states/><entries/>" + end).utf8))
guard absent.states == nil && nilValue.states != nil && nilValue.states! == nil && empty.states!! == "" &&
      absent.entries == nil && nilValue.entries != nil && nilValue.entries! == nil && empty.entries!!.isEmpty &&
      absent.nilValue == nil && nilValue.nilValue == "" else { throw SerializerError.invalid("Collapsed native presence state") }
for value in [absent, nilValue, empty] {
    for protocolValue in Protocol.allCases {
        guard try value.encode(protocolValue) == NilFixtureSample.decodeXml(value.encodeXml()).encode(protocolValue) else { throw SerializerError.invalid("Presence native protocol identity") }
    }
}
// Publish only complete values matching each separately produced schema-oracle decision.
for argument in CommandLine.arguments.dropFirst() {
    let input = Array(try Data(contentsOf: URL(fileURLWithPath: argument)))
    let name = URL(fileURLWithPath: argument).lastPathComponent
    let valid = name.hasSuffix(".valid.xml")
    let output: [UInt8]
    do {
        if name.hasPrefix("rich") {
            let value = try NilShapesSample.decodeXml(input)
            output = try value.encodeXml()
            guard try value.encode(.BINARY_NONE) == NilShapesSample.decodeXml(output).encode(.BINARY_NONE) else { throw SerializerError.invalid("Record/enum collection native equality") }
        } else if name.hasPrefix("holder") {
            let value = try NilFixtureHolder.decodeXml(input)
            output = try value.encodeXml()
            guard try value.encode(.BINARY_NONE) == NilFixtureHolder.decodeXml(output).encode(.BINARY_NONE) else { throw SerializerError.invalid("Nested presence native equality") }
        } else { output = try NilFixtureSample.decodeXml(input).encodeXml() }
    } catch {
        if valid { throw error }
        continue
    }
    guard valid else { throw SerializerError.invalid("Invalid presence candidate published") }
    try Data(output).write(to: URL(fileURLWithPath: argument + ".swift.xml"))
}
print("Swift absent, nil, empty, nested and collection states pass")
'''


def main():
    """Compile real generated Swift and independently validate every successful output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    source = args.build / 'model.serializer'
    config = args.build / 'options.ini'
    optimized_model = MODEL.replace('namespace nil_fixture {', 'namespace nil_fixture { using schema "presence.xsd";')
    source.write_text(optimized_model + RICH_MODEL, encoding='utf-8')
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
        Path(str(path) + '.swift.xml').unlink(missing_ok=True)
        paths.append(path)
    sdk.run([args.compiler, '--input', source, '--config', config, '--language', 'swift',
             '--swift.output', args.build / 'Schema.swift'])
    (args.build / 'main.swift').write_text(MAIN, encoding='utf-8')
    runner = sdk.Runner(args)
    path = lambda name: runner.path('swift', args.build / name)
    sdk.run(runner.command('swift', 'swiftc', '-O', '-warnings-as-errors', path('Schema.swift'), path('main.swift'), '-o', path('main.exe')))
    sdk.run([*runner.command('swift', path('main.exe')), *(runner.path('swift', item) for item in paths)])
    for source in paths:
        output = Path(str(source) + '.swift.xml')
        if source.name.endswith('.valid.xml'):
            (rich_oracle if source.name.startswith('rich') else oracle).assertValid(etree.parse(str(output)))
        elif output.exists():
            raise AssertionError('Invalid presence candidate published output')
    print(f'PASS: {len(paths)} independent Swift presence decisions', flush=True)


if __name__ == '__main__':
    main()
