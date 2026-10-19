"""Qualify native Kotlin/Swift maps using the shared independent typed-key XSD corpus."""
import argparse
import importlib.util
from pathlib import Path

from lxml import etree
import go_xml_map_test as fixture

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_mobile_maps_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

KOTLIN = r'''import java.nio.file.Files
import java.nio.file.Path
import java.util.Optional
// Require invalid input/output policies to fail before publishing a complete result.
fun rejected(action: () -> Unit) {
    try { action() } catch (_: IllegalArgumentException) { return }
    error("Invalid map operation accepted")
}
// Compare independent schema decisions, native values, deterministic order and nested policies.
fun main(arguments: Array<String>) {
    val exact=XmlMapLimits(2,20)
    for (argument in arguments) {
        val input=Files.readAllBytes(Path.of(argument)); val valid=argument.endsWith(".valid.xml")
        val value=try { MapsSample.decodeXml(input) } catch (error: IllegalArgumentException) { if (valid) throw error; continue }
        check(valid) { "Invalid map candidate published: $argument" }
        val text=input.toString(Charsets.UTF_8); val key=if (text.contains("<key> </key>")) " " else "A&B"
        check(value.integers.size == 2 && value.integers[UInt.MAX_VALUE] == "max" && value.integers[1u] == "one")
        check(value.booleans[true] == Short.MIN_VALUE && value.booleans[false] == Short.MAX_VALUE)
        check(value.records[key]!!.code == UShort.MAX_VALUE && value.records[key]!!.label == "nested")
        check(value.enumerated[MapsKey.Alpha] == "first" && value.signedValues[Byte.MIN_VALUE] == "negative")
        check(value.optionalValues.isPresent == text.contains("optional_values"))
        val output=value.encodeXml(); val copy=MapsSample.decodeXml(output)
        check(output.contentEquals(copy.encodeXml()))
        for (protocol in Protocol.entries) check(value.encode(protocol).contentEquals(copy.encode(protocol)))
        Files.write(Path.of(argument+".kotlin.xml"),output)
        value.encodeXml(mapLimits=exact); MapsSample.decodeXml(input,mapLimits=exact)
        val holder=MapsHolder(); holder.value=value; val nested=holder.encodeXml(mapLimits=exact)
        MapsHolder.decodeXml(nested,mapLimits=exact)
        for (bounds in listOf(XmlMapLimits(1,20),XmlMapLimits(2,9),XmlMapLimits(0,0))) {
            rejected { value.encodeXml(mapLimits=bounds) }; rejected { MapsSample.decodeXml(input,mapLimits=bounds) }
            rejected { MapsHolder.decodeXml(nested,mapLimits=bounds) }
        }
        rejected { XmlMapLimits(-1,20) }; rejected { XmlMapLimits(2,-1) }
        val empty=MapsSample.decodeXml(output)
        empty.integers.clear(); empty.booleans.clear(); empty.records.clear(); empty.enumerated.clear(); empty.signedValues.clear()
        empty.optionalValues=Optional.of(linkedMapOf<ULong,String>())
        val emptyOutput=empty.encodeXml(mapLimits=XmlMapLimits(0,0)); val emptyCopy=MapsSample.decodeXml(emptyOutput,mapLimits=XmlMapLimits(0,0))
        check(emptyCopy.optionalValues.isPresent && emptyCopy.optionalValues.get().isEmpty())
        Files.write(Path.of(argument+".empty.kotlin.xml"),emptyOutput)
        val lexical=text.replace("id=\"00001\"","id=\"0000000000000000000000000000000000001\"").toByteArray(Charsets.UTF_8)
        if (!input.contentEquals(lexical)) rejected { MapsSample.decodeXml(lexical,mapLimits=exact) }
        val many=MapsSample.decodeXml(output); many.integers.clear()
        for (index in 0..1024) many.integers[index.toUInt()]="x"
        rejected { many.encodeXml() }; val expanded=many.encodeXml(mapLimits=XmlMapLimits(1025,1 shl 20))
        rejected { MapsSample.decodeXml(expanded) }; MapsSample.decodeXml(expanded,mapLimits=XmlMapLimits(1025,1 shl 20))
        val large=MapsSample.decodeXml(output); large.records.clear(); large.records["x".repeat((1 shl 20)+1)]=MapsChild()
        rejected { large.encodeXml() }; val largeOutput=large.encodeXml(mapLimits=XmlMapLimits(2,(1 shl 20)+1))
        rejected { MapsSample.decodeXml(largeOutput) }; MapsSample.decodeXml(largeOutput,mapLimits=XmlMapLimits(2,(1 shl 20)+1))
    }
    println("Kotlin typed maps, canonical aliases, optional wrappers and independent nested limits pass")
}
'''

SWIFT = r'''import Foundation
// Keep throwing assertions outside nonthrowing autoclosures and report actual map failures.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Map expectation failed") } }
// Reject invalid operations without treating a successful return as a valid failure path.
func rejected(_ action: () throws -> Void) throws {
    do { try action() } catch { return }
    throw SerializerError.invalid("Invalid map operation accepted")
}
// Execute independent oracle decisions using unchanged typed native dictionaries and optionals.
let exact=XmlMapLimits(maxEntries:2,maxKeyBytes:20)
for argument in CommandLine.arguments.dropFirst() {
    let input=Array(try Data(contentsOf: URL(fileURLWithPath: argument))); let valid=argument.hasSuffix(".valid.xml")
    let value: MapsSample
    do { value=try MapsSample.decodeXml(input) } catch { if valid { throw error }; continue }
    try require(valid)
    let text=String(decoding:input,as:UTF8.self); let key=WireString(text.contains("<key> </key>") ? " " : "A&B")
    try require(value.integers.count == 2 && value.integers[UInt32.max] == "max" && value.integers[1] == "one")
    try require(value.booleans[true] == Int16.min && value.booleans[false] == Int16.max)
    try require(value.records[key]?.code == UInt16.max && value.records[key]?.label == "nested")
    try require(value.enumerated[MapsKey.Alpha] == "first" && value.signedValues[Int8.min] == "negative")
    try require((value.optionalValues != nil) == text.contains("optional_values"))
    let output=try value.encodeXml(); let copy=try MapsSample.decodeXml(output)
    try require(output == copy.encodeXml())
    for protocolValue in Protocol.allCases { try require(value.encode(protocolValue) == copy.encode(protocolValue)) }
    try Data(output).write(to: URL(fileURLWithPath: argument+".swift.xml"))
    _=try value.encodeXml(mapLimits:exact); _=try MapsSample.decodeXml(input,mapLimits:exact)
    var holder=MapsHolder(); holder.value=value; let nested=try holder.encodeXml(mapLimits:exact)
    _=try MapsHolder.decodeXml(nested,mapLimits:exact)
    for bounds in [XmlMapLimits(maxEntries:1,maxKeyBytes:20),XmlMapLimits(maxEntries:2,maxKeyBytes:9),XmlMapLimits(maxEntries:0,maxKeyBytes:0),XmlMapLimits(maxEntries:-1,maxKeyBytes:20),XmlMapLimits(maxEntries:2,maxKeyBytes:-1)] {
        try rejected { _=try value.encodeXml(mapLimits:bounds) }; try rejected { _=try MapsSample.decodeXml(input,mapLimits:bounds) }
        try rejected { _=try MapsHolder.decodeXml(nested,mapLimits:bounds) }
    }
    var empty=try MapsSample.decodeXml(output)
    empty.integers=[:]; empty.booleans=[:]; empty.records=[:]; empty.enumerated=[:]; empty.signedValues=[:]; empty.optionalValues=[:]
    let zero=XmlMapLimits(maxEntries:0,maxKeyBytes:0); let emptyOutput=try empty.encodeXml(mapLimits:zero)
    let emptyCopy=try MapsSample.decodeXml(emptyOutput,mapLimits:zero)
    try require(emptyCopy.optionalValues != nil && emptyCopy.optionalValues!.isEmpty)
    try Data(emptyOutput).write(to: URL(fileURLWithPath: argument+".empty.swift.xml"))
    let lexical=Array(text.replacingOccurrences(of:"id=\"00001\"",with:"id=\"0000000000000000000000000000000000001\"").utf8)
    if lexical != input { try rejected { _=try MapsSample.decodeXml(lexical,mapLimits:exact) } }
    var many=try MapsSample.decodeXml(output); many.integers=[:]
    for index in 0...1024 { many.integers[UInt32(index)]="x" }
    let expanded=XmlMapLimits(maxEntries:1025,maxKeyBytes:1 << 20)
    try rejected { _=try many.encodeXml() }; let manyOutput=try many.encodeXml(mapLimits:expanded)
    try rejected { _=try MapsSample.decodeXml(manyOutput) }; _=try MapsSample.decodeXml(manyOutput,mapLimits:expanded)
    var large=try MapsSample.decodeXml(output); large.records=[WireString(String(repeating:"x",count:(1 << 20)+1)):MapsChild()]
    let largeBounds=XmlMapLimits(maxEntries:2,maxKeyBytes:(1 << 20)+1)
    try rejected { _=try large.encodeXml() }; let largeOutput=try large.encodeXml(mapLimits:largeBounds)
    try rejected { _=try MapsSample.decodeXml(largeOutput) }; _=try MapsSample.decodeXml(largeOutput,mapLimits:largeBounds)
}
print("Swift typed maps, canonical aliases, optional wrappers and independent nested limits pass")
'''


def main():
    """Compile actual emitted SDKs and independently validate regular and empty map output."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='kotlin,swift')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('kotlin', 'swift') for value in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    oracle = etree.XMLSchema(etree.fromstring(fixture.xsd().encode()))
    paths = []
    for index, (document, valid) in enumerate(fixture.cases()):
        if oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent map decision differs: {index}: {oracle.error_log}')
        path = args.build / f'{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8'); paths.append(path)
    (args.build / 'model.serializer').write_text(fixture.MODEL, encoding='utf-8')
    (args.build / 'maps.ini').write_text(fixture.CONFIG, encoding='utf-8')
    command = [args.compiler, '--input', args.build / 'model.serializer', '--config', args.build / 'maps.ini',
               '--language', ','.join(languages), '--kotlin.package=']
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command)
    runner = sdk.Runner(args)
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        filename, content = ('Main.kt', KOTLIN) if language == 'kotlin' else ('main.swift', SWIFT)
        (source / filename).write_text(content, encoding='utf-8')
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            Path(str(path) + f'.empty.{language}.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_maps', args.build / language, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            output = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(output)))
                oracle.assertValid(etree.parse(str(path) + f'.empty.{language}.xml'))
            elif output.exists():
                raise AssertionError('Invalid map candidate published output')
    print(f'PASS: {len(paths)} independent maps and native/resource checks across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
