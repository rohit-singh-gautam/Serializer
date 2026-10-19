"""Qualify generated Swift direct input with the retained-tree parser disabled."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_swift_token_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

MODEL = '''serializer version 1.20.0;
namespace token_fixture {
  xml(namespace("urn:é"));
  class child { public int16 value(1); }
  class sample {
    public uint32 id(1);
    public string label(2);
    public optional<array<string>> entries(3);
    public child nested(4);
    public array int32 values(5);
  }
  class alias_attributes {
    public optional<string> left xml(attribute, name("same"), namespace("urn:left"));
    public optional<string> right xml(attribute, name("same"), namespace("urn:right"));
  }
}
'''
OPTIONS = '''[xml]
enabled = true
[xml.class.token_fixture::sample]
attributes = id
presence.entries = absent
wrapper.entries = entries
name.entries = item
name.values = value
'''
MAIN = r'''import Foundation
#if os(Windows)
import ucrt
#elseif canImport(Glibc)
import Glibc
#else
import Darwin
#endif
// Evaluate throwing checks before Swift's nonthrowing assertion autoclosure.
func require(_ value: Bool, _ message: String = "Swift token expectation failed", _ line: Int = #line) throws {
    guard value else { throw SerializerError.invalid(message + " at line " + String(line)) }
}
// Keep checked failures readable and return a normal failing SDK process status.
func qualify() throws {
// Exercise real generated code while proving the previous tree parser cannot be selected.
srlXmlForbidTree = true
let document = "<sample xmlns='urn:é' id='4294967295'><label>A&amp;B&#13;😀</label><entries><item/><item>x</item></entries><nested><value>-32768</value></nested><value>-2147483648</value><value>2147483647</value></sample>"
let bytes = Array(document.utf8)
var negativeControl = false
do { _ = try srlXmlParse(bytes, XmlLimits()) } catch SerializerError.invalid(let message) { negativeControl = message == "retained XML tree" }
try require(negativeControl)
let value = try TokenFixtureSample.decodeXml(bytes)
try require(value.id == UInt32.max && value.label == "A&B\r😀" && value.entries! == ["", "x"])
try require(value.nested.value == Int16.min && value.values == [Int32.min, Int32.max])
let encoded = try value.encodeXml()
for protocolValue in Protocol.allCases {
    try require(value.encode(protocolValue) == TokenFixtureSample.decodeXml(encoded).encode(protocolValue))
}
let split = document.replacingOccurrences(of: "2147483647", with: "2147<![CDATA[483]]>&#54;47")
try require(TokenFixtureSample.decodeXml(Array(split.utf8)).values[1] == Int32.max)
let absent = document.replacingOccurrences(of: "<entries><item/><item>x</item></entries>", with: "")
try require(TokenFixtureSample.decodeXml(Array(absent.utf8)).entries == nil)
let empty = document.replacingOccurrences(of: "<entries><item/><item>x</item></entries>", with: "<entries/>")
let emptyValue = try TokenFixtureSample.decodeXml(Array(empty.utf8))
try require(emptyValue.entries!.isEmpty)
try require(TokenFixtureSample.decodeXml(emptyValue.encodeXml()).entries!.isEmpty)
// Canonically equivalent raw prefixes identify different expanded attributes in XML.
let aliases = "<alias_attributes xmlns='urn:é' xmlns:é='urn:left' xmlns:e\u{301}='urn:right' é:same='first&#13;value' e\u{301}:same='second&#9;value'/>"
let attributeValue = try TokenFixtureAliasAttributes.decodeXml(Array(aliases.utf8))
try require(attributeValue.left == "first\rvalue" && attributeValue.right == "second\tvalue", "Canonical-equivalent attribute prefixes collapsed")
let repeatedAttributes = try TokenFixtureAliasAttributes.decodeXml(attributeValue.encodeXml())
try require(repeatedAttributes.left == attributeValue.left && repeatedAttributes.right == attributeValue.right)
var exact = XmlLimits(); exact.maxBytes = bytes.count
try require(TokenFixtureSample.decodeXml(bytes, exact).id == UInt32.max)
// Check byte-exact expanded namespace identity despite Swift canonical String equivalence.
try require("urn:é" == "urn:e\u{301}")
for (index, bad) in [document + "<extra/>", String(document.dropLast()), document.replacingOccurrences(of: "urn:é", with: "urn:e\u{301}"),
    document.replacingOccurrences(of: "<sample ", with: "<sample unexpected='1' "),
    document.replacingOccurrences(of: "<value>-2147483648</value>", with: "<value>2147483648</value>"),
    document.replacingOccurrences(of: "<entries>", with: "<entries unexpected='1'>"),
    document.replacingOccurrences(of: "<label>", with: "<label><nested/>"),
    "<!DOCTYPE sample [<!ENTITY local 'expanded'>]>" + document,
    "<?xml version='1.1'?>" + document, "<?xml version='1.0' encoding='ISO-8859-1'?>" + document].enumerated() {
    var rejected = false
    do { _ = try TokenFixtureSample.decodeXml(Array(bad.utf8)) } catch { rejected = true }
    try require(rejected, "Invalid XML candidate published, index " + String(index))
}
let invalidUtf8: [[UInt8]] = [[0xc0, 0x80], [0xed, 0xa0, 0x80], [0xf4, 0x90, 0x80, 0x80], [0xe2, 0x82]]
for bad in invalidUtf8 {
    var rejected = false
    do { _ = try TokenFixtureSample.decodeXml(bad) } catch { rejected = true }
    try require(rejected, "Invalid UTF8 accepted")
}
for budget in ["bytes", "nodes", "depth", "text"] {
    var limits = XmlLimits()
    switch budget { case "bytes": limits.maxBytes = bytes.count - 1; case "nodes": limits.maxNodes = 1; case "depth": limits.maxDepth = 1; default: limits.maxTextBytes = 1 }
    var rejected = false
    do { _ = try TokenFixtureSample.decodeXml(bytes, limits) } catch { rejected = true }
    try require(rejected, "Input bound exceeded")
}
print("Swift direct input/output, optional collections, byte-exact namespaces, lexical boundaries and limits pass")
}
do { try qualify() } catch {
    FileHandle.standardError.write(Data("Swift token failure: \(error)\n".utf8))
    exit(1)
}
'''


def main():
    """Generate and compile actual Swift, with a checked negative control for tree parsing."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    source = args.build / 'model.serializer'
    config = args.build / 'options.ini'
    source.write_text(MODEL, encoding='utf-8')
    config.write_text(OPTIONS, encoding='utf-8')
    output = args.build / 'Schema.swift'
    sdk.run([args.compiler, '--input', source, '--config', config, '--language', 'swift', '--swift.output', output])
    generated = output.read_text(encoding='utf-8')
    signature = 'fileprivate func srlXmlParse(_ bytes: [UInt8], _ limits: XmlLimits) throws -> SrlXmlNode {'
    if generated.count(signature) != 1:
        raise AssertionError('Expected the legacy parser for the negative control')
    generated = generated.replace(signature, signature + '\n    if srlXmlForbidTree { throw SerializerError.invalid("retained XML tree") }')
    generated = 'var srlXmlForbidTree = false\n' + generated
    # The negative control and generated internals intentionally share one Swift file.
    output.write_text(generated + '\n' + MAIN, encoding='utf-8')
    runner = sdk.Runner(args)
    path = lambda name: runner.path('swift', args.build / name)
    sdk.run(runner.command('swift', 'swiftc', '-O', '-warnings-as-errors', path('Schema.swift'), '-o', path('main.exe')))
    sdk.run(runner.command('swift', path('main.exe')))


if __name__ == '__main__':
    main()
