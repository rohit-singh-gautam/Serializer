"""Qualify generated Kotlin nil/absence mappings with an independent XSD oracle."""
import argparse
import importlib.util
from pathlib import Path

from lxml import etree

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:nil'
spec = importlib.util.spec_from_file_location('serializer_kotlin_presence_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

MODEL = '''serializer version 1.20.0;
namespace nil_fixture {
  xml(namespace("urn:serializer:nil"));
  class child { public string value(1); }
  class sample {
    public optional<string> nil_value(1);
    public optional<optional<string>> states(2);
    public optional<optional<array<string>>> entries(3);
    public optional<child> nested(4);
  }
  class holder { public sample value(1); }
}
'''
OPTIONS = '''[xml]
enabled = true
[xml.class.nil_fixture::sample]
presence.nil_value = nil
presence.states = absent_or_nil
presence.entries = absent_or_nil
wrapper.entries = entries
name.entries = item
presence.nested = nil
'''
XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:nil"
targetNamespace="urn:serializer:nil" elementFormDefault="qualified">
<xs:complexType name="childType"><xs:sequence><xs:element name="value" type="xs:string"/></xs:sequence></xs:complexType>
<xs:complexType name="sampleType"><xs:sequence>
<xs:element name="nil_value" type="xs:string" nillable="true"/>
<xs:element name="states" type="xs:string" minOccurs="0" nillable="true"/>
<xs:element name="entries" minOccurs="0" nillable="true"><xs:complexType><xs:sequence>
<xs:element name="item" type="xs:string" minOccurs="0" maxOccurs="unbounded"/>
</xs:sequence></xs:complexType></xs:element>
<xs:element name="nested" type="t:childType" nillable="true"/>
</xs:sequence></xs:complexType>
<xs:element name="sample" type="t:sampleType"/>
<xs:element name="holder"><xs:complexType><xs:sequence><xs:element name="value" type="t:sampleType"/></xs:sequence></xs:complexType></xs:element>
</xs:schema>'''
MAIN = '''import java.nio.file.Files
import java.nio.file.Path
// Distinguish all native optional layers, then compare the independent oracle decisions.
fun main(args: Array<String>) {
    val start = "<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>"
    val end = "<nested xsi:nil='true'/></sample>"
    val absent = NilFixtureSample.decodeXml((start + "<nil_value xsi:nil='true'/>" + end).toByteArray(Charsets.UTF_8))
    val nil = NilFixtureSample.decodeXml((start + "<nil_value/><states xsi:nil='true'/><entries xsi:nil='true'/>" + end).toByteArray(Charsets.UTF_8))
    val empty = NilFixtureSample.decodeXml((start + "<nil_value/><states/><entries/>" + end).toByteArray(Charsets.UTF_8))
    check(!absent.states.isPresent && nil.states.isPresent && !nil.states.get().isPresent && empty.states.get().get() == "")
    check(!absent.entries.isPresent && nil.entries.isPresent && !nil.entries.get().isPresent && empty.entries.get().get().isEmpty())
    check(!absent.nilValue.isPresent && nil.nilValue.isPresent && nil.nilValue.get() == "")
    for (value in listOf(absent, nil, empty)) {
        val encoded = value.encodeXml()
        for (protocol in Protocol.entries) {
            check(value.encode(protocol).contentEquals(NilFixtureSample.decodeXml(encoded).encode(protocol)))
        }
    }
    for (argument in args) {
        val path = Path.of(argument)
        val input = Files.readAllBytes(path)
        val valid = path.fileName.toString().endsWith(".valid.xml")
        val output: ByteArray
        try {
            output = if (path.fileName.toString().startsWith("holder")) {
                val value = NilFixtureHolder.decodeXml(input)
                val result = value.encodeXml()
                check(value.encode(Protocol.BINARY_NONE).contentEquals(NilFixtureHolder.decodeXml(result).encode(Protocol.BINARY_NONE)))
                result
            } else NilFixtureSample.decodeXml(input).encodeXml()
        } catch (failure: IllegalArgumentException) {
            if (valid) throw AssertionError("Rejected valid presence document: " + path, failure)
            continue
        }
        if (!valid) throw AssertionError("Invalid presence candidate published: " + path)
        Files.write(Path.of(argument + ".out.xml"), output)
    }
    println("Kotlin nil, absent, empty, nested and collection presence decisions pass")
}
'''


def main():
    """Require actual generated Kotlin and independently revalidate all emitted documents."""
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
    model, config = args.build / 'model.serializer', args.build / 'options.ini'
    model.write_text(MODEL, encoding='utf-8')
    config.write_text(OPTIONS, encoding='utf-8')
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    start = '<sample xmlns="' + NS + '" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">'
    ending = '<nested xsi:nil="true"/></sample>'
    base = start + '<nil_value xsi:nil="true"/>' + ending
    documents = [(base, True), (base.replace('xsi:nil="true"', 'xsi:nil="1"'), True)]
    for middle in ('', '<states/>', '<states xsi:nil="true"/>', '<states>A&amp;B&#13;😀</states>',
                   '<states xsi:nil="false"/>', '<entries/>', '<entries xsi:nil="true"/>',
                   '<entries><item/><item>A&amp;B</item></entries>'):
        documents.append((start + '<nil_value/>' + middle + ending, True))
    documents.append((base.replace('<nested xsi:nil="true"/>', '<nested xsi:nil="false"><value>child</value></nested>'), True))
    for changed in (base.replace('<nil_value xsi:nil="true"/>', ''),
                    base.replace('xsi:nil="true"', 'xsi:nil="yes"', 1),
                    base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="true"> </nil_value>'),
                    base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="true"><value/></nil_value>'),
                    base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="true" unknown="1"/>'),
                    base.replace('<nested xsi:nil="true"/>', '<entries xsi:nil="true"><item/></entries><nested xsi:nil="true"/>'),
                    base.replace('<nested xsi:nil="true"/>', '<states/><states/><nested xsi:nil="true"/>'),
                    base.replace('<nested xsi:nil="true"/>', '<nested/>')):
        documents.append((changed, False))
    paths = []
    for index, (document, valid) in enumerate(documents):
        if oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent presence decision differs for {index}: {oracle.error_log}')
        path = args.build / f'sample-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8')
        Path(str(path) + '.out.xml').unlink(missing_ok=True)
        paths.append(path)
    holder = args.build / 'holder.valid.xml'
    holder.write_text('<holder xmlns="' + NS + '" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><value>' +
                      '<nil_value xsi:nil="false"/><states xsi:nil="1"/><entries/><nested xsi:nil="true"/>' +
                      '</value></holder>', encoding='utf-8')
    oracle.assertValid(etree.parse(str(holder)))
    paths.append(holder)
    sdk.run([args.compiler, '--input', model, '--config', config, '--language', 'kotlin',
             '--kotlin.package=', '--kotlin.output', args.build / 'Schema.kt'])
    (args.build / 'Main.kt').write_text(MAIN, encoding='utf-8')
    runner = sdk.Runner(args)
    path = lambda name: runner.path('kotlin', args.build / name)
    sdk.run(runner.command('kotlin', 'kotlinc', path('Schema.kt'), path('Main.kt'), '-Werror', '-include-runtime', '-d', path('main.jar')))
    sdk.run([*runner.command('kotlin', 'java', '-jar', path('main.jar')), *(runner.path('kotlin', item) for item in paths)])
    for source in paths:
        output = Path(str(source) + '.out.xml')
        if source.name.endswith('.valid.xml'):
            oracle.assertValid(etree.parse(str(output)))
        elif output.exists():
            raise AssertionError('Invalid presence input published output')
    print(f'PASS: {len(paths)} independent Kotlin presence decisions', flush=True)


if __name__ == '__main__':
    main()
