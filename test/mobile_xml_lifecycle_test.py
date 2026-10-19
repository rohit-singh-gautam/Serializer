# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify Kotlin/Swift exact revision layouts, model magic and owning digest policies."""
import argparse
from pathlib import Path

import native_xml_lifecycle_test as shared

ROOT, sdk = shared.ROOT, shared.sdk
KINDS = ('numeric', 'dotted', 'floating', 'derived', 'typed', 'plain', 'envelope', 'digests')


def consumer(swift):
    """Emit statically typed consumers; all validity and selected-layout oracles remain external."""
    if swift:
        result = '''import Foundation
// Fail after an unexpectedly successful operation, outside its expected-error catch.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid model published") }
// Keep throwing checks out of the standard library's nonthrowing autoclosure.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Model expectation failed") } }
// Compare exact complete candidates and all four existing native protocol encodings.
for path in CommandLine.arguments.dropFirst() {
 let source=Array(try Data(contentsOf:URL(fileURLWithPath:path))); let valid=path.hasSuffix(".valid.xml")
 let kind=URL(fileURLWithPath:path).lastPathComponent.split(separator:"-")[0]; let output: [UInt8]
 do { switch kind {
'''
        for kind in KINDS:
            name = 'Lifecycle' + kind.title()
            result += f''' case "{kind}":
  let value=try {name}.decodeXml(source); output=try value.encodeXml(); let copy=try {name}.decodeXml(output)
  try require(output == copy.encodeXml())
  for protocolValue in Protocol.allCases {{ try require(value.encode(protocolValue) == copy.encode(protocolValue)) }}
'''
        return result + r''' default: throw SerializerError.invalid("Unknown model")
 } } catch { if valid { print(path); throw error }; continue }
 try require(valid); try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))
}
let source=Array("<digests xmlns=\"urn:serializer:js-lifecycle\" raw=\"\" exact=\"00ff\"><algorithm>00000000000000000000000000000000</algorithm><items/></digests>".utf8)
var value=try LifecycleDigests.decodeXml(source); value.raw=[UInt8](repeating:255,count:1024*1024)
let output=try value.encodeXml(); try require(LifecycleDigests.decodeXml(output).raw == value.raw)
value.raw.append(0); try rejected { _=try value.encodeXml() }
let policy=XmlDigestLimits(maxBytes:1024*1024+1); let expanded=try value.encodeXml(digestLimits:policy)
try rejected { _=try LifecycleDigests.decodeXml(expanded) }
try require(LifecycleDigests.decodeXml(expanded,digestLimits:policy).raw == value.raw)
value.exact.append(0); try rejected { _=try value.encodeXml(digestLimits:policy) }
var holder=LifecycleDigestHolder(); holder.value.raw=[UInt8](repeating:7,count:17)
let nested=try holder.encodeXml(); let small=XmlDigestLimits(maxBytes:16)
try rejected { _=try holder.encodeXml(digestLimits:small) }
try rejected { _=try LifecycleDigestHolder.decodeXml(nested,digestLimits:small) }
holder.value.raw.removeLast(); _=try holder.encodeXml(digestLimits:small)
try rejected { _=try holder.encodeXml(digestLimits:XmlDigestLimits(maxBytes:-1)) }
print("Swift exact model layouts, native identities and owning digest limits pass")
'''
    result = '''@file:OptIn(ExperimentalUnsignedTypes::class)
import java.nio.file.Files
import java.nio.file.Path
// Keep an unexpectedly successful operation outside the expected-failure catch.
fun rejected(action: () -> Unit) { try { action() } catch (_: IllegalArgumentException) { return }; error("Invalid model published") }
// Compare exact complete models, historical layouts and every existing native protocol.
fun main(arguments: Array<String>) {
 for(path in arguments) {
  val source=Files.readAllBytes(Path.of(path)); val valid=path.endsWith(".valid.xml")
  val kind=Path.of(path).fileName.toString().substringBefore('-'); val output: ByteArray
  try { output=when(kind) {
'''
    for kind in KINDS:
        name = 'Lifecycle' + kind.title()
        result += f'''   "{kind}" -> {{
    val value={name}.decodeXml(source); val bytes=value.encodeXml(); val copy={name}.decodeXml(bytes)
    check(bytes.contentEquals(copy.encodeXml()))
    for(protocol in Protocol.entries) check(value.encode(protocol).contentEquals(copy.encode(protocol)))
    bytes
   }}
'''
    return result + r'''   else -> error("Unknown model")
  } } catch(error: IllegalArgumentException) { if(valid) throw IllegalArgumentException(path,error); continue }
  check(valid); Files.write(Path.of(path+".kotlin.xml"),output)
 }
 val source="<digests xmlns=\"urn:serializer:js-lifecycle\" raw=\"\" exact=\"00ff\"><algorithm>00000000000000000000000000000000</algorithm><items/></digests>".toByteArray()
 val value=LifecycleDigests.decodeXml(source); value.raw=UByteArray(1024*1024) { 255u }
 val output=value.encodeXml(); check(LifecycleDigests.decodeXml(output).raw.contentEquals(value.raw))
 value.raw += 0u; rejected { value.encodeXml() }
 val policy=XmlDigestLimits(maxBytes=1024*1024+1); val expanded=value.encodeXml(digestLimits=policy)
 rejected { LifecycleDigests.decodeXml(expanded) }
 check(LifecycleDigests.decodeXml(expanded,digestLimits=policy).raw.contentEquals(value.raw))
 value.exact += 0u; rejected { value.encodeXml(digestLimits=policy) }
 val holder=LifecycleDigestHolder(); holder.value.raw=UByteArray(17) { 7u }
 val nested=holder.encodeXml(); val small=XmlDigestLimits(maxBytes=16)
 rejected { holder.encodeXml(digestLimits=small) }; rejected { LifecycleDigestHolder.decodeXml(nested,digestLimits=small) }
 holder.value.raw=holder.value.raw.copyOf(16); holder.encodeXml(digestLimits=small)
 rejected { XmlDigestLimits(maxBytes=-1) }
 println("Kotlin exact model layouts, native identities and owning digest limits pass")
}
'''


def main():
    """Generate real SDKs and verify independent exact-layout and hexadecimal-byte expectations."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--language', default='kotlin,swift')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = args.language.split(',')
    if len(languages) != len(set(languages)) or any(value not in ('kotlin', 'swift') for value in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Scratch must be beneath ignored out/')
    args.build.mkdir(parents=True, exist_ok=True)
    (args.build / 'model.serializer').write_text(shared.MODEL, encoding='utf-8')
    (args.build / 'options.ini').write_text('[xml]\nenabled=true\n[xml.class.lifecycle::digests]\nwrapper.items=items\nname.items=item\n', encoding='utf-8')
    paths = []
    for index, (root, source, valid) in enumerate(shared.cases()):
        path = args.build / f'{root}-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(source, encoding='utf-8'); paths.append((root, path, valid))
    command = [args.compiler, '--input', args.build / 'model.serializer', '--config', args.build / 'options.ini', '--language', ','.join(languages), '--kotlin.package=']
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        (source / ('main.swift' if language == 'swift' else 'Main.kt')).write_text(consumer(language == 'swift'), encoding='utf-8')
        launch = runner.build(language, 'xml_lifecycle', args.build / language, source)
        for _, path, _ in paths:
            Path(str(path) + '.' + language + '.xml').unlink(missing_ok=True)
        sdk.run([*launch, *(runner.path(language, path) for _, path, _ in paths)])
        for root, path, valid in paths:
            output = Path(str(path) + '.' + language + '.xml')
            if valid:
                shared.check_output(root, output.read_bytes())
            elif output.exists():
                raise AssertionError(f'Invalid model published: {output}')
    print(f'PASS: {len(paths)} exact model probes, digest boundary policies and four native protocols across {len(languages)} mobile SDKs', flush=True)


if __name__ == '__main__':
    main()
