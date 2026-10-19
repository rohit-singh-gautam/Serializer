"""Qualify Kotlin/Swift variants with shared independent element and xsi:type XSD corpora."""
import argparse
from pathlib import Path

from lxml import etree
import portable_xml_variant_test as fixture
import portable_xml_typed_variant_test as typed

ROOT, sdk = fixture.ROOT, fixture.sdk

KOTLIN = r'''import java.nio.file.Files
import java.nio.file.Path
// Require a failed selection or resource policy to publish no result.
fun rejected(action: () -> Unit) {
    try { action() } catch (_: IllegalArgumentException) { return } catch (_: SerializerException) { return }
    error("Invalid variant operation accepted")
}
// Compare typed native alternatives with independent schema decisions and native wire formats.
fun main(arguments: Array<String>) {
    for (argument in arguments) {
        val input=Files.readAllBytes(Path.of(argument)); val valid=argument.endsWith(".valid.xml")
        val value=try { VariantsSample.decodeXml(input) } catch (error: IllegalArgumentException) { if (valid) throw IllegalArgumentException(argument,error); continue }
        check(valid) { "Invalid variant accepted: $argument" }
        val file=Path.of(argument).fileName.toString(); val selected=file[1]-'0'; val primitive=file[3]-'0'
        check(value.valueIndex == selected && value.primitiveIndex == primitive)
        when (selected) {
            0 -> check(value.valueCode == UInt.MAX_VALUE)
            1 -> check(value.valueText == "hi🙂&<>")
            2 -> check(value.valueRecord.code == UShort.MAX_VALUE && value.valueRecord.label == "nested")
            3 -> check(value.valueState == VariantsState.One)
        }
        check(if (primitive == 0) !value.primitiveFlag else value.primitiveAmount == Double.POSITIVE_INFINITY)
        val output=value.encodeXml(); val copy=VariantsSample.decodeXml(output)
        check(output.contentEquals(copy.encodeXml()))
        for (protocol in Protocol.entries) {
            if (primitive == 1 && protocol == Protocol.JSON) {
                rejected { value.encode(protocol) }; rejected { copy.encode(protocol) }
            } else { check(value.encode(protocol).contentEquals(copy.encode(protocol))) }
        }
        Files.write(Path.of(argument+".kotlin.xml"),output)
        val holder=VariantsHolder(); holder.value=value; val nested=holder.encodeXml(); VariantsHolder.decodeXml(nested)
        val limits=XmlLimits(maxNodes=1)
        rejected { value.encodeXml(limits) }; rejected { VariantsSample.decodeXml(input,limits) }
        rejected { VariantsHolder.decodeXml(nested,limits) }
        val broken=VariantsSample.decodeXml(output)
        broken.valueIndex=-1; rejected { broken.encodeXml() }
        broken.valueIndex=4; rejected { broken.encodeXml() }
        broken.valueIndex=selected; broken.primitiveIndex=2; rejected { broken.encodeXml() }
    }
    println("Kotlin exact variant identities, native alternatives and nested resource policies pass")
}
'''

SWIFT = r'''import Foundation
// Keep throwing assertions outside nonthrowing autoclosures.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Variant expectation failed") } }
// Reject invalid operations without catching the assertion that detects a successful return.
func rejected(_ action: () throws -> Void) throws {
    do { try action() } catch { return }
    throw SerializerError.invalid("Invalid variant operation accepted")
}
// Compare native alternatives, exact QName identities and transactional failure behavior.
for argument in CommandLine.arguments.dropFirst() {
    let input=Array(try Data(contentsOf: URL(fileURLWithPath:argument))); let valid=argument.hasSuffix(".valid.xml")
    let value: VariantsSample
    do { value=try VariantsSample.decodeXml(input) } catch { if valid { print(argument); throw error }; continue }
    try require(valid)
    let file=Array(URL(fileURLWithPath:argument).lastPathComponent.utf8); let selected=Int(file[1]-48); let primitive=Int(file[3]-48)
    try require(value.valueIndex == selected && value.primitiveIndex == primitive)
    switch selected {
    case 0: try require(value.valueCode == UInt32.max)
    case 1: try require(value.valueText == "hi🙂&<>")
    case 2: try require(value.valueRecord.code == UInt16.max && value.valueRecord.label == "nested")
    case 3: try require(value.valueState == VariantsState.One)
    default: throw SerializerError.invalid("Unexpected independent alternative")
    }
    try require(primitive == 0 ? !value.primitiveFlag : value.primitiveAmount == Double.infinity)
    let output=try value.encodeXml(); let copy=try VariantsSample.decodeXml(output)
    try require(output == copy.encodeXml())
    for protocolValue in Protocol.allCases {
        if primitive == 1 && protocolValue == Protocol.JSON {
            try rejected { _=try value.encode(protocolValue) }; try rejected { _=try copy.encode(protocolValue) }
        } else { try require(value.encode(protocolValue) == copy.encode(protocolValue)) }
    }
    try Data(output).write(to: URL(fileURLWithPath:argument+".swift.xml"))
    var holder=VariantsHolder(); holder.value=value; let nested=try holder.encodeXml(); _=try VariantsHolder.decodeXml(nested)
    var limits=XmlLimits(); limits.maxNodes=1
    try rejected { _=try value.encodeXml(limits) }; try rejected { _=try VariantsSample.decodeXml(input,limits) }
    try rejected { _=try VariantsHolder.decodeXml(nested,limits) }
    var broken=try VariantsSample.decodeXml(output)
    broken.valueIndex = -1; try rejected { _=try broken.encodeXml() }
    broken.valueIndex = 4; try rejected { _=try broken.encodeXml() }
    broken.valueIndex=selected; broken.primitiveIndex=2; try rejected { _=try broken.encodeXml() }
}
print("Swift exact variant identities, native alternatives and nested resource policies pass")
'''


def main():
    """Execute actual native SDKs, then independently validate every emitted complete document."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='kotlin,swift')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('kotlin', 'swift') for value in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    runner = sdk.Runner(args); count = 0
    for mode in ['element', 'typed', 'typed-empty']:
        directory = args.build / mode; directory.mkdir(parents=True, exist_ok=True)
        empty = mode == 'typed-empty'; is_typed = mode != 'element'
        oracle = typed.prepare_oracle(directory, empty) if is_typed else etree.XMLSchema(etree.fromstring(fixture.XSD.encode()))
        probes = typed.cases(empty) if is_typed else fixture.cases()
        normalize = typed.normalized_namespaces if is_typed else lambda value: value
        paths = []
        for index, (selected, primitive, document, valid) in enumerate(probes):
            try:
                accepted = oracle.validate(normalize(etree.fromstring(document.encode())))
            except ValueError:
                accepted = False
            if accepted != valid:
                raise AssertionError(f'Independent variant decision differs: {mode}/{index}: {oracle.error_log}')
            path = directory / f'v{selected}u{primitive}-{index}.{"valid" if valid else "invalid"}.xml'
            path.write_text(document, encoding='utf-8'); paths.append(path)
        model = directory / 'model.serializer'; model.write_text(typed.MODEL if is_typed else fixture.MODEL, encoding='utf-8')
        options = directory / 'variants.ini'; options.write_text(typed.config(empty) if is_typed else fixture.CONFIG, encoding='utf-8')
        command = [args.compiler, '--input', model, '--config', options, '--language', ','.join(languages), '--kotlin.package=']
        for language in languages:
            target = directory / language; target.mkdir(parents=True, exist_ok=True)
            command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
        sdk.run(command)
        for language in languages:
            source = directory / 'source' / language; source.mkdir(parents=True, exist_ok=True)
            filename, content = ('Main.kt', KOTLIN) if language == 'kotlin' else ('main.swift', SWIFT)
            (source / filename).write_text(content, encoding='utf-8')
            for path in paths:
                Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            launch = runner.build(language, 'xml_variants', directory / language, source)
            sdk.run([*launch, *(runner.path(language, path) for path in paths)])
            for path in paths:
                emitted = Path(str(path) + f'.{language}.xml')
                if path.name.endswith('.valid.xml'):
                    oracle.assertValid(normalize(etree.parse(str(emitted)).getroot()))
                elif emitted.exists():
                    raise AssertionError(f'Invalid variant output published: {emitted}')
        count += len(paths)
    print(f'PASS: {count} independent variant probes across {len(languages)} SDKs, both selection modes and exact empty/prefixed type scopes', flush=True)


if __name__ == '__main__':
    main()
