"""Qualify Kotlin/Swift mixed fragments against the shared independent XSD and resource corpus."""
import argparse
from pathlib import Path

from lxml import etree
import portable_xml_mixed_test as fixture

ROOT, sdk = fixture.ROOT, fixture.sdk

KOTLIN = r'''import java.nio.file.Files
import java.nio.file.Path
import java.lang.reflect.Modifier
// Reject an operation without treating its successful return as a valid failure path.
fun rejected(action: () -> Unit) { try { action() } catch (_: IllegalArgumentException) { return }; error("Invalid mixed operation accepted") }
// Inspect unchanged native private fields independently of generated XML accessors.
fun main(arguments: Array<String>) {
    val index=JsMixedFragment::class.java.getDeclaredField("valueIndex"); val text=JsMixedFragment::class.java.getDeclaredField("valueText")
    check(Modifier.isPrivate(index.modifiers) && Modifier.isPrivate(text.modifiers)); index.isAccessible=true; text.isAccessible=true
    rejected { XmlMixedLimits(-1,0) }
    for (argument in arguments.indices step 3) {
        val path=arguments[argument]; val valid=path.endsWith(".valid.xml"); val input=Files.readAllBytes(Path.of(path))
        val limits=XmlMixedLimits(arguments[argument+1].toInt(),arguments[argument+2].toInt())
        val output: ByteArray
        try {
            if (Path.of(path).fileName.toString().startsWith("holder")) {
                val value=JsMixedHolder.decodeXml(input,mixedLimits=limits); output=value.encodeXml(mixedLimits=limits)
            } else {
                val value=JsMixedParagraph.decodeXml(input,mixedLimits=limits); output=value.encodeXml(mixedLimits=limits)
                val copy=JsMixedParagraph.decodeXml(output)
                for (protocol in Protocol.entries) check(value.encode(protocol).contentEquals(copy.encode(protocol)))
                if (Path.of(path).fileName.toString().startsWith("document")) {
                    check(value.content.size == 5 && text.get(value.content[0]) == "head&😀")
                    check(value.content.map { index.getInt(it) } == listOf(0,1,0,1,0))
                    val blank=JsMixedFragment(); val adjacent=JsMixedFragment(); text.set(adjacent,"adjacent")
                    value.content.add(1,blank); value.content.add(2,adjacent)
                    rejected { value.encodeXml(mixedLimits=XmlMixedLimits(6,100)) }
                    check(value.encodeXml().toString(Charsets.UTF_8).contains("head&amp;😀adjacent<"))
                }
                val invalid=JsMixedFragment(); index.setInt(invalid,999); value.content.add(invalid)
                rejected { value.encodeXml() }
            }
        } catch (failure: IllegalArgumentException) { if (valid) throw IllegalArgumentException(path,failure); continue }
        check(valid) { "Invalid mixed candidate published: $path" }
        Files.write(Path.of(path+".kotlin.xml"),output)
    }
    println("Kotlin private ordered fragments, logical text, native formats and nested mixed policies pass")
}
'''

SWIFT_PROBE = r'''
extension JsMixedFragment {
    // Inspect authored private native storage without exposing it in production output.
    func testNativeMixedIndex() -> Int { valueIndex }
    // Inspect the native text payload independently of XML namespace or parser metadata.
    func testNativeMixedText() -> String { valueText }
    // Create an authored native fragment for empty/adjacent/invalid variant controls.
    static func testNativeMixed(_ index: Int, _ text: String) -> JsMixedFragment {
        var value=JsMixedFragment(); value.valueIndex=index; value.valueText=text; return value
    }
}
'''

SWIFT = r'''import Foundation
// Keep throwing assertions outside nonthrowing autoclosures.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Mixed expectation failed") } }
// Reject invalid operations without catching the assertion after a successful return.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid mixed operation accepted") }
// Verify direct ordered content, private native values and exact resource decisions.
let arguments=Array(CommandLine.arguments.dropFirst())
for argument in stride(from:0,to:arguments.count,by:3) {
    let path=arguments[argument]; let valid=path.hasSuffix(".valid.xml"); let input=Array(try Data(contentsOf:URL(fileURLWithPath:path)))
    let limits=XmlMixedLimits(maxFragments:Int(arguments[argument+1])!,maxTextBytes:Int(arguments[argument+2])!)
    let output: [UInt8]
    do {
        if URL(fileURLWithPath:path).lastPathComponent.hasPrefix("holder") {
            let value=try JsMixedHolder.decodeXml(input,mixedLimits:limits); output=try value.encodeXml(mixedLimits:limits)
        } else {
            var value=try JsMixedParagraph.decodeXml(input,mixedLimits:limits); output=try value.encodeXml(mixedLimits:limits)
            let copy=try JsMixedParagraph.decodeXml(output)
            for protocolValue in Protocol.allCases { try require(value.encode(protocolValue) == copy.encode(protocolValue)) }
            if URL(fileURLWithPath:path).lastPathComponent.hasPrefix("document") {
                try require(value.content.count == 5 && value.content[0].testNativeMixedText() == "head&😀")
                try require(value.content.map { $0.testNativeMixedIndex() } == [0,1,0,1,0])
                let labels=Set(Mirror(reflecting:value.content[0]).children.compactMap(\.label))
                try require(labels.contains("valueIndex") && labels.contains("valueText"))
                value.content.insert(JsMixedFragment.testNativeMixed(0,""),at:1)
                value.content.insert(JsMixedFragment.testNativeMixed(0,"adjacent"),at:2)
                try rejected { _=try value.encodeXml(mixedLimits:XmlMixedLimits(maxFragments:6,maxTextBytes:100)) }
                try require(String(decoding:value.encodeXml(),as:UTF8.self).contains("head&amp;😀adjacent<"))
            }
            value.content.append(JsMixedFragment.testNativeMixed(999,""))
            try rejected { _=try value.encodeXml() }
            try rejected { _=try copy.encodeXml(mixedLimits:XmlMixedLimits(maxFragments:-1,maxTextBytes:0)) }
        }
    } catch { if valid { print(path); throw error }; continue }
    try require(valid)
    try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))
}
print("Swift private ordered fragments, logical text, native formats and nested mixed policies pass")
'''


def main():
    """Execute native SDKs and compare every emitted text/tail position with an independent parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True); parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='kotlin,swift'); parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('kotlin', 'swift') for value in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    (args.build / 'format.xsd').write_text(fixture.FORMAT_XSD, encoding='utf-8')
    schema = args.build / 'mixed.xsd'; schema.write_text(fixture.XSD, encoding='utf-8')
    oracle = etree.XMLSchema(etree.parse(str(schema))); paths = []
    controls = {'fragments', 'bytes', 'child-limit', 'text-limit', 'large-limit', 'holder-limit'}
    for label, document, fragments, text_bytes, valid in fixture.cases():
        if oracle.validate(etree.fromstring(document.encode())) != (valid or label in controls):
            raise AssertionError(f'Independent mixed oracle differs: {label}: {oracle.error_log}')
        path = args.build / f'{label}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8'); paths.append((path, fragments, text_bytes))
    model = args.build / 'model.serializer'; model.write_text(fixture.MODEL, encoding='utf-8')
    config = args.build / 'mixed.ini'; config.write_text(fixture.OPTIONS, encoding='utf-8')
    command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages), '--kotlin.package=']
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language in languages:
        directory = args.build / 'source' / language; directory.mkdir(parents=True, exist_ok=True)
        filename, source = ('Main.kt', KOTLIN) if language == 'kotlin' else ('main.swift', SWIFT)
        (directory / filename).write_text(source, encoding='utf-8')
        if language == 'swift':
            with (args.build / language / sdk.OUTPUTS[language]).open('a', encoding='utf-8') as generated:
                generated.write(SWIFT_PROBE)
        for path, _, _ in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_mixed', args.build / language, directory); arguments = []
        for path, fragments, text_bytes in paths:
            arguments += [runner.path(language, path), fragments, text_bytes]
        sdk.run([*launch, *arguments])
        for path, _, _ in paths:
            output = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(output)))
                original = etree.parse(str(path), etree.XMLParser(remove_comments=True, remove_pis=True)).getroot()
                if fixture.semantic(original) != fixture.semantic(etree.parse(str(output)).getroot()):
                    raise AssertionError(f'Ordered mixed value changed: {output}')
            elif output.exists():
                raise AssertionError(f'Invalid mixed output published: {output}')
    fixture.rejected_bindings(args, languages)
    print(f'PASS: {len(paths)} independent mixed probes and 3 atomic helper gates across {len(languages)} native SDKs', flush=True)


if __name__ == '__main__':
    main()
