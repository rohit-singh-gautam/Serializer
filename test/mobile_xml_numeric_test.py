"""Check direct Kotlin/Swift decimal-to-binary32 rounding against exact rational arithmetic."""
import argparse
from pathlib import Path
import struct

from javascript_xml_numeric_test import rounded_single
import mobile_xml_scalar_test as shared

ROOT, sdk = shared.ROOT, shared.sdk
VALUES = ['1.0000000596046448', '1.0000000596046447', '1.000000059604644775390625',
          '1.0000001788139343', '-1.0000000596046448', '-1.0000000596046447',
          '7.0064923216240854e-46', '7.0064923216240853e-46', '1e-999', '-1e-999', '0', '-0']
KOTLIN = '''import java.nio.file.Files
import java.nio.file.Path
// Preserve exact binary32 rounding, signed zero and native protocol bit patterns.
fun main(arguments: Array<String>) {
 for(line in Files.readAllLines(Path.of(arguments[0]))) {
  val parts=line.split('\t'); val lexical=parts[0]; val expected=parts[1].toUInt()
  val value=RoundingSample.decodeXml(("<sample value=\\\""+lexical+"\\\"/>").toByteArray(Charsets.UTF_8))
  check(value.value.toRawBits().toUInt() == expected) { lexical }
  check(RoundingSample.decodeXml(value.encodeXml()).value.toRawBits().toUInt() == expected)
  for(protocol in Protocol.entries) check(RoundingSample.decode(value.encode(protocol),protocol).value.toRawBits().toUInt() == expected)
 }
 println("Kotlin exact binary32 midpoint/subnormal/signed-zero bit patterns pass")
}'''
SWIFT = '''import Foundation
// Compare bit identities independently of floating equality, which merges signed zeros.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Binary32 rounding mismatch") } }
// Preserve exact midpoint/subnormal rounding through XML and native protocol values.
let source=try String(contentsOf:URL(fileURLWithPath:CommandLine.arguments[1]),encoding:.utf8)
for line in source.split(whereSeparator:{ $0.isNewline }) {
 let parts=line.split(separator:"\\t"); let expected=UInt32(parts[1])!
 let value=try RoundingSample.decodeXml(Array(("<sample value=\\\""+String(parts[0])+"\\\"/>").utf8))
 try require(value.value.bitPattern == expected)
 try require(RoundingSample.decodeXml(value.encodeXml()).value.bitPattern == expected)
 for protocolValue in Protocol.allCases { try require(RoundingSample.decode(value.encode(protocolValue),protocolValue).value.bitPattern == expected) }
}
print("Swift exact binary32 midpoint/subnormal/signed-zero bit patterns pass")'''


def main():
    """Compile real generated SDKs and compare every native result with a Fraction-based oracle."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True); parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='kotlin,swift'); parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler, args.build = args.compiler.resolve(), args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(value not in ('kotlin', 'swift') for value in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    model = args.build / 'model.serializer'
    model.write_text('serializer version 1.20.0; namespace rounding { class sample { public float value xml(attribute); }}', encoding='utf-8')
    corpus = args.build / 'rounding.tsv'
    corpus.write_text(''.join(value + '\t' + str(struct.unpack('<I', struct.pack('<f', rounded_single(value)))[0]) + '\n' for value in VALUES), encoding='utf-8')
    command = [args.compiler, '--input', model, '--language', ','.join(languages), '--kotlin.package=']
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        filename, content = ('Main.kt', KOTLIN) if language == 'kotlin' else ('main.swift', SWIFT)
        (source / filename).write_text(content, encoding='utf-8')
        launch = runner.build(language, 'xml_rounding', args.build / language, source)
        sdk.run([*launch, runner.path(language, corpus)])
    print(f'PASS: {len(VALUES)} exact rational binary32 probes across {len(languages)} SDKs and all native protocols', flush=True)


if __name__ == '__main__':
    main()
