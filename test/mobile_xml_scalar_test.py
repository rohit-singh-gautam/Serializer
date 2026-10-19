"""Qualify Kotlin/Swift exact scalar, list, union, default and portable pattern contracts."""
import argparse
from pathlib import Path
import subprocess

from lxml import etree
import portable_xml_scalar_test as scalars
import portable_xml_facets_test as facets

ROOT, sdk = scalars.ROOT, scalars.sdk

KOTLIN_NUMERIC = '''import java.nio.file.Files
import java.nio.file.Path
// Reject invalid scalar models before publishing output.
fun rejected(action: () -> Unit) { try { action() } catch (_: IllegalArgumentException) { return }; error("Invalid scalar output published") }
// Compare native widths, normalized text and seven independently edited invalid values.
fun main(arguments: Array<String>) {
  for (path in arguments) {
    val valid=path.endsWith(".valid.xml")
    val value=try { FacetsSample.decodeXml(Files.readAllBytes(Path.of(path))) } catch (error: IllegalArgumentException) { if (valid) throw IllegalArgumentException(path,error); continue }
    check(valid); check(value.integer >= ULong.MAX_VALUE-2u && value.integer < ULong.MAX_VALUE && value.choice == ULong.MAX_VALUE)
    check(value.rounded == 0.1f && value.text == "A😀" && value.code == "é😀" && value.enabled)
    val output=value.encodeXml(); Files.write(Path.of(path+".kotlin.xml"),output)
    for (index in 0..6) { val broken=FacetsSample.decodeXml(output)
      when(index) { 0->broken.integer=ULong.MAX_VALUE; 1->broken.amount = -1.0; 2->broken.rounded=0.11f; 3->broken.text="B😀"; 4->broken.choice=ULong.MAX_VALUE-1u; 5->broken.enabled=false; else->broken.code="é" }
      rejected { broken.encodeXml() }
    }
  }
  println("Kotlin exact numeric facets and normalized Unicode values pass")
}'''

SWIFT_NUMERIC = '''import Foundation
// Keep throwing assertions outside nonthrowing autoclosures.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Scalar expectation failed") } }
// Reject failed scalar output without catching the assertion for a successful return.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid scalar output published") }
// Compare exact widths, normalized text and independently edited invalid values.
for path in CommandLine.arguments.dropFirst() {
  let valid=path.hasSuffix(".valid.xml"); let value: FacetsSample
  do { value=try FacetsSample.decodeXml(Array(Data(contentsOf:URL(fileURLWithPath:path)))) } catch { if valid { print(path); throw error }; continue }
  try require(valid && value.integer >= UInt64.max-2 && value.integer < UInt64.max && value.choice == UInt64.max)
  try require(value.rounded == 0.1 && value.text == "A😀" && value.code == "é😀" && value.enabled)
  let output=try value.encodeXml(); try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))
  for index in 0...6 { var broken=try FacetsSample.decodeXml(output)
    switch index { case 0: broken.integer=UInt64.max; case 1: broken.amount = -1; case 2: broken.rounded=0.11; case 3: broken.text="B😀"; case 4: broken.choice=UInt64.max-1; case 5: broken.enabled=false; default: broken.code="é" }
    try rejected { _=try broken.encodeXml() }
  }
}
print("Swift exact numeric facets and normalized Unicode values pass")'''

KOTLIN_PATTERN = '''import java.nio.file.Files
import java.nio.file.Path
// Require bounded-machine resource failures to retain their precise diagnostics.
fun checkResource(path: String, error: IllegalArgumentException) {
  val file=Path.of(path).fileName.toString()
  if (file.startsWith("work.")) check(error.message == "Portable XML regex work limit exceeded")
  if (file.startsWith("bytes.")) check(error.message == "Portable XML regex input limit exceeded")
}
// Compare independently classified source and authored output using actual prepared programs.
fun main(arguments: Array<String>) {
  for (path in arguments) {
    val valid=path.endsWith(".valid.xml"); val text=Files.readString(Path.of(path+".txt"))
    try { val value=PatternsSample.decodeXml(Files.readAllBytes(Path.of(path)),limits=XmlLimits(maxTextBytes=16*1024*1024)); check(valid && value.value == text) }
    catch (error: IllegalArgumentException) { if (valid) throw error; checkResource(path,error) }
    val candidate=PatternsSample(); candidate.value=text
    try { val output=candidate.encodeXml(); check(valid); Files.write(Path.of(path+".kotlin.xml"),output) }
    catch (error: IllegalArgumentException) { if (valid) throw error; checkResource(path,error) }
  }
  println("Kotlin portable Unicode patterns and deterministic resource bounds pass")
}'''

SWIFT_PATTERN = '''import Foundation
// Keep assertions and failed-operation catches independent.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Pattern expectation failed") } }
// Preserve exact bounded-machine failure classes across union/regex operations.
func checkResource(_ path: String, _ error: Error) throws {
  let file=URL(fileURLWithPath:path).lastPathComponent; let message=String(describing:error)
  if file.hasPrefix("work.") { try require(message.contains("Portable XML regex work limit exceeded")) }
  if file.hasPrefix("bytes.") { try require(message.contains("Portable XML regex input limit exceeded")) }
}
// Compare actual native input/output with independently classified Unicode pattern values.
for path in CommandLine.arguments.dropFirst() {
  let valid=path.hasSuffix(".valid.xml"); let text=try String(contentsOf:URL(fileURLWithPath:path+".txt"),encoding:.utf8)
  var limits=XmlLimits(); limits.maxTextBytes=16*1024*1024
  var accepted=false
  do { let value=try PatternsSample.decodeXml(Array(Data(contentsOf:URL(fileURLWithPath:path))),limits); accepted=true; try require(value.value.utf8.elementsEqual(text.utf8)) }
  catch { if valid { throw error }; try checkResource(path,error) }
  try require(accepted == valid)
  var candidate=PatternsSample(); candidate.value=text
  accepted=false
  do { let output=try candidate.encodeXml(); accepted=true; try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml")) }
  catch { if valid { throw error }; try checkResource(path,error) }
  try require(accepted == valid)
}
print("Swift portable Unicode patterns and deterministic resource bounds pass")'''


def scalar_consumer(swift):
    """Generate a typed SDK consumer using a fixed independent root/native-type inventory."""
    roots = ['sample', 'amount', 'words', 'numbers', 'items', 'fixed_list', 'child_list',
             'optional_child', 'list_all', 'plain_default', 'guard']
    if swift:
        prefix = '''import Foundation
// Keep assertions independent of expected-error catches.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Scalar expectation failed") } }
// Reject edited native output transactionally.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid scalar output published") }
// Verify authoritative scalar/list/default behavior and unchanged native payload identities.
for path in CommandLine.arguments.dropFirst() {
 let valid=path.hasSuffix(".valid.xml"); let file=URL(fileURLWithPath:path).lastPathComponent; let root=String(file.split(separator:"-")[0])
 let input=Array(try Data(contentsOf:URL(fileURLWithPath:path))); var limits=XmlLimits()
 if file.hasPrefix("items-limit") { limits.maxNodes=16 }; if file.hasPrefix("items-text") { limits.maxTextBytes=20 }
 let output: [UInt8]
 do {
 switch root {
'''
    else:
        prefix = '''@file:OptIn(ExperimentalUnsignedTypes::class)
import java.nio.file.Files
import java.nio.file.Path
// Require invalid native output to fail before publishing XML.
fun rejected(action: () -> Unit) { try { action() } catch (_: IllegalArgumentException) { return }; error("Invalid scalar output published") }
// Verify authoritative scalars and list/default values against the independent corpus.
fun main(arguments: Array<String>) { for (path in arguments) {
 val valid=path.endsWith(".valid.xml"); val file=Path.of(path).fileName.toString(); val root=file.substringBefore('-')
 val input=Files.readAllBytes(Path.of(path)); val limits=XmlLimits(maxNodes=if(file.startsWith("items-limit")) 16 else 100000,maxTextBytes=if(file.startsWith("items-text")) 20 else 8388608)
 val output: ByteArray
 try { when(root) {
'''
    for root in roots:
        model = 'Scalars' + ''.join(word.title() for word in root.split('_'))
        if swift:
            prefix += f'''case "{root}":
 var value=try {model}.decodeXml(input,limits)
 output=try value.encodeXml(limits); let copy=try {model}.decodeXml(output,limits)
 try require(value.encode(Protocol.BINARY_NONE) == copy.encode(Protocol.BINARY_NONE))
'''
        else:
            prefix += f'''"{root}" -> {{
 val value={model}.decodeXml(input,limits); output=value.encodeXml(limits); val copy={model}.decodeXml(output,limits)
 check(value.encode(Protocol.BINARY_NONE).contentEquals(copy.encode(Protocol.BINARY_NONE)))
'''
        if root == 'sample':
            prefix += (' try require(value.unit != nil && (value.defaultAttribute == "cm" || value.defaultAttribute == "m"))\n' if swift else ' check(value.unit.isPresent && value.defaultAttribute in listOf("cm","m"))\n')
            if swift:
                prefix += ''' let legacyEncode: (XmlLimits) throws -> [UInt8] = value.encodeXml
 let legacyDecode: ([UInt8],XmlLimits) throws -> ScalarsSample = ScalarsSample.decodeXml
 try require(output == legacyEncode(XmlLimits()))
 _=try legacyDecode(output,XmlLimits())
'''
                prefix += ''' for index in 0...6 { var broken=try ScalarsSample.decodeXml(output)
 switch index { case 0: broken.decimalValue="1e0"; case 1: broken.integerValue="1.0"; case 2: broken.unionValue="unknown"; case 3: broken.listValue="2 1"; case 4: broken.fixedText="bad"; case 5: broken.typed=[1,2]; default: broken.sign="0" }
 try rejected { _=try broken.encodeXml() } }
'''
            else:
                prefix += ''' val legacyEncode: (XmlLimits)->ByteArray = value::encodeXml
 val legacyDecode: (ByteArray,XmlLimits)->ScalarsSample = ScalarsSample::decodeXml
 check(output.contentEquals(legacyEncode(XmlLimits())))
 legacyDecode(output,XmlLimits())
'''
                prefix += ''' for(index in 0..6) { val broken=ScalarsSample.decodeXml(output)
 when(index) { 0->broken.decimalValue="1e0"; 1->broken.integerValue="1.0"; 2->broken.unionValue="unknown"; 3->broken.listValue="2 1"; 4->broken.fixedText="bad"; 5->broken.typed=ushortArrayOf(1u,2u); else->broken.sign="0" }
 rejected { broken.encodeXml() } }
'''
        elif root == 'amount':
            prefix += ' try require(value.unit != nil)\n' if swift else ' check(value.unit.isPresent)\n'
        elif root in ('words', 'fixed_list', 'plain_default'):
            mutation = 'value.value=""' if root == 'plain_default' else 'value.value=["one"]' if root == 'fixed_list' else 'value.value[0]="two words"'
            if not swift and root == 'fixed_list':
                mutation = 'value.value=mutableListOf("one")'
            prefix += mutation + ('; try rejected { _=try value.encodeXml() }\n' if swift else '; rejected { value.encodeXml() }\n')
        if not swift:
            prefix += '}\n'
    if swift:
        # Keep immutable values immutable so the strict SDK treats warnings as failures.
        for root in roots:
            if root not in ('words', 'fixed_list', 'plain_default'):
                model = 'Scalars' + ''.join(word.title() for word in root.split('_'))
                prefix = prefix.replace('var value=try ' + model, 'let value=try ' + model)
        return prefix + '''default: throw SerializerError.invalid("Unknown corpus root")
 }
 } catch { if valid { print(path); throw error }; continue }
 try require(valid); try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))
}
print("Swift exact scalar/list/union/default/native identities pass")'''
    return prefix + '''else -> error("Unknown corpus root")
 } } catch(error: IllegalArgumentException) { if(valid) throw IllegalArgumentException(path,error); continue }
 check(valid); Files.write(Path.of(path+".kotlin.xml"),output)
} ; println("Kotlin exact scalar/list/union/default/native identities pass") }
'''


def main():
    """Execute all maintained independent scalar and pattern probes through actual native SDKs."""
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
    runner = sdk.Runner(args); total = 0
    for mode in ('scalar', 'numeric', 'pattern', 'large'):
        directory = args.build / mode; directory.mkdir(parents=True, exist_ok=True)
        if mode == 'scalar':
            xsd, model, probes = scalars.XSD, scalars.MODEL, scalars.cases(); oracle = scalars.oracle_schema()
            validate = lambda data: scalars.independent(data, oracle)
        else:
            xsd, model, probes = facets.numeric_cases(directory) if mode == 'numeric' else facets.pattern_cases(mode == 'large')
            oracle = etree.XMLSchema(etree.fromstring(xsd.encode())); validate = lambda data: oracle.validate(etree.fromstring(data))
        paths = []
        for index, probe in enumerate(probes):
            if mode == 'scalar':
                kind, text, valid = probe; name = f'{kind}-{index}'; document = text.encode(); value = None
                skip_oracle = kind in ('guard-work', 'items-limit', 'items-text')
            else:
                name, document, value, valid = probe; skip_oracle = name in ('work', 'bytes')
            if not skip_oracle and validate(document) != valid:
                raise AssertionError(f'Independent scalar oracle differs: {mode}/{name}: {oracle.error_log}')
            path = directory / f'{name}.{"valid" if valid else "invalid"}.xml'; path.write_bytes(document); paths.append(path)
            if value is not None:
                Path(str(path) + '.txt').write_text(value, encoding='utf-8')
        (directory / ('scalars.xsd' if mode == 'scalar' else 'sample.xsd')).write_text(xsd, encoding='utf-8')
        model_path = directory / 'model.serializer'; model_path.write_text(model, encoding='utf-8')
        command = [args.compiler, '--input', model_path, '--language', ','.join(languages), '--protocols', 'binary_none', '--kotlin.package=']
        for language in languages:
            target = directory / language; target.mkdir(parents=True, exist_ok=True)
            command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
        sdk.run(command)
        original = {language: (directory / language / sdk.OUTPUTS[language]).read_bytes() for language in languages}
        for language in languages:
            swift = language == 'swift'; source = directory / 'source' / language; source.mkdir(parents=True, exist_ok=True)
            content = scalar_consumer(swift) if mode == 'scalar' else (SWIFT_NUMERIC if swift else KOTLIN_NUMERIC) if mode == 'numeric' else (SWIFT_PATTERN if swift else KOTLIN_PATTERN)
            (source / ('main.swift' if swift else 'Main.kt')).write_text(content, encoding='utf-8')
            for path in paths:
                Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            launch = runner.build(language, 'xml_' + mode, directory / language, source)
            sdk.run([*launch, *(runner.path(language, path) for path in paths)])
            for path in paths:
                emitted = Path(str(path) + f'.{language}.xml')
                if path.name.endswith('.valid.xml'):
                    if not validate(emitted.read_bytes()):
                        raise AssertionError(f'Independent output oracle differs: {emitted}: {oracle.error_log}')
                elif emitted.exists():
                    raise AssertionError(f'Invalid scalar output published: {emitted}')
        if mode == 'pattern':
            (directory / 'sample.xsd').write_text(xsd.replace('A[😀-🙏]{1,2}|B[0-9]+|^$|C[^0-9]{1,2}|D(a?)*', r'\p{L}+'), encoding='utf-8')
            result = subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=60)
            if result.returncode == 0 or 'approved portable XSD regex subset' not in result.stdout + result.stderr:
                raise AssertionError('Unsupported mobile regex requires its portable-subset diagnostic')
            if any((directory / language / sdk.OUTPUTS[language]).read_bytes() != original[language] for language in languages):
                raise AssertionError('Rejected pattern replaced generated source')
        total += len(paths)
    scalars.rejected_bindings(args, languages)
    print(f'PASS: {total} independent exact scalar/list/union/default/pattern probes and 6 atomic binding gates across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
