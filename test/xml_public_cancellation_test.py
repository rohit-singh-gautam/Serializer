# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify public XML interruption policies from separate C#, Kotlin and Swift libraries."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xml.etree.ElementTree as etree

ROOT = Path(__file__).resolve().parents[1]
MODEL = '''serializer version 1.21.0;
namespace public_xml {
  xml(namespace("urn:serializer:public-xml"));
  class payload { public uint32 number; public string label; }
  class source { public payload current;
    public translator payload as project { number=current.number; label=current.label; };
  }
}'''
XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
 targetNamespace="urn:serializer:public-xml" elementFormDefault="qualified">
 <xs:element name="payload"><xs:complexType><xs:sequence>
 <xs:element name="number" type="xs:unsignedInt"/>
 <xs:element name="label" type="xs:string"/>
 </xs:sequence></xs:complexType></xs:element></xs:schema>'''

CSHARP = r'''using System;
using System.IO;
using System.Linq;
using static SerializerGenerated.Schema;
// This assembly references the compiled generated library, never its source or internals.
internal static class Program {
  // Keep optimized acceptance checks independent of debug assertions.
  private static void Require(bool value,string message) { if(!value)throw new Exception(message); }
  // Preserve the exact caller exception object rather than matching only its type.
  private static void Original(Action action,Exception marker) {
    try { action(); } catch(Exception failure) { Require(ReferenceEquals(failure,marker),"Original error changed");return; }
    throw new Exception("Interrupted external operation returned successfully");
  }
  // Exercise externally visible policies, real prepared System.Xml and complete result publication.
  private static void Main(string[] arguments) {
    Require(typeof(PublicXmlPayload).Assembly!=typeof(Program).Assembly,"Generated types compiled into consumer assembly");
    var source=new PublicXmlSource();source.Current.Number=7;source.Current.Label="external";
    var model=source.Current;var bytes=model.EncodeXml();var before=Enum.GetValues<Protocol>().Select(protocol=>model.Encode(protocol)).ToArray();
    var schema=new XmlSchema(File.ReadAllBytes(arguments[0]));
    var baseline=new Limits(4096,4096,100,32,ReadPolicy.FLEXIBLE);
    int parent=0,child=0;var composed=baseline.WithCancellation(()=>++parent).WithCancellation(()=>++child);
    Require(composed.MaxBytes==baseline.MaxBytes&&composed.MaxStringBytes==baseline.MaxStringBytes&&composed.MaxElements==baseline.MaxElements&&composed.MaxDepth==baseline.MaxDepth&&composed.ReadPolicy==baseline.ReadPolicy,"Policy copy changed bounds");
    Require(model.EncodeXml(composed,schema).SequenceEqual(bytes)&&parent>0&&child>0,"Public policy composition lost callback");
    Func<Limits,object>[] actions={limits=>model.EncodeXml(limits,schema),limits=>PublicXmlPayload.DecodeXml(bytes,limits,schema),limits=>source.WithProject(view=>view.EncodeXml(limits,schema))};
    foreach(var action in actions) {
      int count=0;action(baseline.WithCancellation(()=>++count));Require(count>0,"No operation callback");
      int calls=0;var marker=new Exception("External late completion marker");
      var limits=baseline.WithCancellation(()=>{if(++calls==count)throw marker;});object sentinel=new();object result=sentinel;
      Original(()=>result=action(limits),marker);Require(ReferenceEquals(result,sentinel),"Failed result published");
      int stopped=calls;Original(()=>action(limits),marker);Require(calls==stopped,"Failed wrapper invoked caller again");
    }
    var wrong=new XmlSchema(System.Text.Encoding.UTF8.GetBytes("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='wrong' type='xs:string'/></xs:schema>"));
    int rejectedPolls=0;try {model.EncodeXml(baseline.WithCancellation(()=>++rejectedPolls),wrong);throw new Exception("Wrong prepared schema accepted");}catch(FormatException){}
    var replacement=new Exception("Must not replace provider failure");int observed=0;
    try {model.EncodeXml(baseline.WithCancellation(()=>{if(++observed>rejectedPolls)throw replacement;}),wrong);throw new Exception("Wrong prepared schema accepted");}catch(FormatException){}
    int index=0;foreach(var protocol in Enum.GetValues<Protocol>())Require(before[index++].SequenceEqual(model.Encode(protocol)),"Native wire bytes changed");
    File.WriteAllBytes(arguments[1],model.EncodeXml(baseline.WithCancellation(()=>{}),schema));
    Console.WriteLine("PASS external C# assembly: copied bounds, stacked callbacks, prepared-provider completion, first errors, native controls and recovery");
  }
}'''

KOTLIN = r'''package consumer
import externalxml.*
import java.nio.file.Files
import java.nio.file.Path
private class ConsumerMarker
// Validate real prepared JAXP before making the caller's cancellation observable.
private class CompletingValidator(val delegate: XmlValidator): XmlValidator {
  var completed=false
  var rejection: Throwable?=null
  override fun validate(bytes: ByteArray,limits: XmlLimits) {delegate.validate(bytes,limits);completed=true;rejection?.let{throw it}}
}
// Require the exact thrown application error with no accidental success through the catch.
private fun original(marker: Throwable,action: ()->Unit) {try {action()}catch(failure: Throwable){check(failure===marker);return};error("Interrupted external operation returned")}
// This module sees only the compiled generated JAR and its public declarations.
fun main(arguments: Array<String>) {
  check(PublicXmlPayload::class.java.protectionDomain.codeSource.location!=ConsumerMarker::class.java.protectionDomain.codeSource.location)
  val source=PublicXmlSource();source.current.number=7u;source.current.label="external";val model=source.current
  val bytes=model.encodeXml();val before=Protocol.entries.map{model.encode(it)}
  val provider=PreparedXmlSchema(Files.readAllBytes(Path.of(arguments[0])))
  val baseline=XmlLimits(4096,100,32,4096);var parent=0;var child=0
  val composed=baseline.withCancellation{++parent}.withCancellation{++child}
  check(composed.maxBytes==baseline.maxBytes&&composed.maxNodes==baseline.maxNodes&&composed.maxDepth==baseline.maxDepth&&composed.maxTextBytes==baseline.maxTextBytes)
  check(model.encodeXml(composed,provider).contentEquals(bytes)&&parent>0&&child>0)
  val actions=listOf<(XmlLimits,XmlValidator)->Any>({limits,validator->model.encodeXml(limits,validator)},
    {limits,validator->PublicXmlPayload.decodeXml(bytes,limits,validator)},
    {limits,validator->source.withProject{it.encodeXml(limits,validator)}})
  for(action in actions) {
    val validator=CompletingValidator(provider);val marker=IllegalStateException("External late completion marker");var calls=0
    val limits=baseline.withCancellation{++calls;if(validator.completed)throw marker};val sentinel=Any();var result:Any=sentinel
    original(marker){result=action(limits,validator)};check(validator.completed&&result===sentinel)
    val stopped=calls;original(marker){action(limits,validator)};check(calls==stopped)
    val providerFailure=IllegalArgumentException("Original provider rejection");validator.completed=false;validator.rejection=providerFailure
    original(providerFailure){action(baseline.withCancellation{if(validator.completed)throw marker},validator)}
  }
  for((index,protocol) in Protocol.entries.withIndex())check(before[index].contentEquals(model.encode(protocol)))
  Files.write(Path.of(arguments[1]),model.encodeXml(baseline.withCancellation{},provider))
  println("PASS external Kotlin module: copied bounds, stacked callbacks, real JAXP completion, first errors, native controls and recovery")
}'''

SWIFT = r'''
// Preserve reference identity of caller/provider failures across the separately compiled module boundary.
final class ExternalMarker: Error {}
// Complete genuine prepared libxml2 validation, then make late interruption observable.
final class ExternalCompletingValidator: XmlValidator {
  let provider: PreparedProvider
  var completed=false
  var rejection: ExternalMarker?
  init(_ provider: PreparedProvider){self.provider=provider}
  func validate(_ bytes:[UInt8],_ limits:XmlLimits) throws {try provider.validate(bytes,limits);completed=true;if let failure=rejection{throw failure}}
}
// Reject accidental success and retain the exact original failure instance.
func original(_ marker:ExternalMarker,_ action:() throws->Void) throws {
  do {try action()}catch {try require((error as? ExternalMarker)===marker);return}
  throw SerializerError.invalid("Interrupted external operation returned")
}
let providerBridge=try ProviderBridge(CommandLine.arguments[1])
let prepared=try PreparedProvider(providerBridge,Array(Data(contentsOf:URL(fileURLWithPath:CommandLine.arguments[2]))))
var source=PublicXmlSource();source.current.number=7;source.current.label="external"
let model=source.current;let bytes=try model.encodeXml();let before=try ExternalXml.Protocol.allCases.map{try model.encode($0)}
var baseline=XmlLimits();baseline.maxBytes=4096;baseline.maxNodes=100;baseline.maxDepth=32;baseline.maxTextBytes=4096
var parent=0;var child=0
let composed=baseline.withCancellation{parent+=1}.withCancellation{child+=1}
try require(composed.maxBytes==baseline.maxBytes&&composed.maxNodes==baseline.maxNodes&&composed.maxDepth==baseline.maxDepth&&composed.maxTextBytes==baseline.maxTextBytes)
try require(model.encodeXml(composed,validator:prepared)==bytes && parent>0 && child>0)
let actions:[(XmlLimits,any XmlValidator)throws->Void]=[
  {limits,validator in _=try model.encodeXml(limits,validator:validator)},
  {limits,validator in _=try PublicXmlPayload.decodeXml(bytes,limits,validator:validator)},
  {limits,validator in _=try source.withProject{try $0.encodeXml(limits,validator:validator)}}]
for action in actions {
  let validator=ExternalCompletingValidator(prepared);let marker=ExternalMarker();var calls=0
  let limits=baseline.withCancellation{calls+=1;if validator.completed{throw marker}}
  var returned=false;try original(marker){try action(limits,validator);returned=true};try require(validator.completed && !returned)
  let stopped=calls;try original(marker){try action(limits,validator)};try require(calls==stopped)
  let providerFailure=ExternalMarker();validator.completed=false;validator.rejection=providerFailure
  try original(providerFailure){try action(baseline.withCancellation{if validator.completed{throw marker}},validator)}
}
for (index,protocolValue) in ExternalXml.Protocol.allCases.enumerated(){try require(before[index]==model.encode(protocolValue))}
try Data(model.encodeXml(baseline.withCancellation{},validator:prepared)).write(to:URL(fileURLWithPath:CommandLine.arguments[3]))
print("PASS external Swift module: copied bounds, stacked callbacks, real libxml2 completion, first errors, native controls and recovery")
'''


def main():
    """Compile generated libraries before external consumers; retain every actual command and SDK result."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', choices=('csharp', 'kotlin', 'swift'), required=True)
    parser.add_argument('--kotlinc', type=Path)
    parser.add_argument('--swiftc', default='swiftc')
    parser.add_argument('--bridge', type=Path)
    args = parser.parse_args()
    compiler, build = args.compiler.resolve(strict=True), args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('In-repository output must remain beneath ignored out/')
    if os.name == 'nt':
        ctypes.windll.kernel32.SetErrorMode(0x8003)
    elif args.language == 'swift':
        parser.error('The Swift prepared-provider profile requires the native Windows bridge')
    build.mkdir(parents=True, exist_ok=True)
    (build / 'verification.json').unlink(missing_ok=True)
    library, consumer = build / 'library', build / 'consumer'
    library.mkdir(exist_ok=True)
    consumer.mkdir(exist_ok=True)
    model, schema, output = build / 'model.serializer', build / 'schema.xsd', build / 'produced.xml'
    model.write_text(MODEL, encoding='utf-8')
    schema.write_text(XSD, encoding='utf-8')
    environment, commands, started = os.environ.copy(), [], time.monotonic()

    def run(command, label):
        """Record exact execution and reject missing or failing SDK checks rather than skip them."""
        argv = [str(value) for value in command]
        result = subprocess.run(argv, cwd=build, env=environment, capture_output=True, text=True,
                                encoding='utf-8', errors='strict', timeout=900)
        (build / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'argv': argv, 'returncode': result.returncode})
        (build / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if result.returncode:
            print(result.stdout + result.stderr)
            result.check_returncode()
        return result.stdout + result.stderr

    language = args.language
    source = library / {'csharp': 'Schema.cs', 'kotlin': 'Schema.kt', 'swift': 'Schema.swift'}[language]
    run([compiler, '--input', model, '--language', language, '--output', source,
         '--xml.runtime_xsd', 'true', '--xml.validation_scope', 'both', '--kotlin.package', 'externalxml'], 'generation')
    if language == 'csharp':
        (library / 'External.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><TargetFramework>net8.0</TargetFramework><Nullable>enable</Nullable><TreatWarningsAsErrors>true</TreatWarningsAsErrors></PropertyGroup></Project>', encoding='utf-8')
        (consumer / 'Consumer.csproj').write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net8.0</TargetFramework><Nullable>enable</Nullable><TreatWarningsAsErrors>true</TreatWarningsAsErrors></PropertyGroup><ItemGroup><ProjectReference Include="../library/External.csproj"/></ItemGroup></Project>', encoding='utf-8')
        (consumer / 'Program.cs').write_text(CSHARP, encoding='utf-8')
        version = run(['dotnet', '--version'], 'sdk-version').strip()
        run(['dotnet', 'build', library / 'External.csproj', '-c', 'Release', '--nologo', '-v:q'], 'library-compile')
        run(['dotnet', 'build', consumer / 'Consumer.csproj', '-c', 'Release', '--nologo', '-v:q'], 'consumer-compile')
        accepted = run(['dotnet', consumer / 'bin/Release/net8.0/Consumer.dll', schema, output], 'acceptance')
    elif language == 'kotlin':
        if not args.kotlinc:
            parser.error('The actual Kotlin compiler is required')
        home = args.kotlinc.resolve(strict=True).parent.parent
        common = ['java', '-Xmx2g', '-cp', home / 'lib/kotlin-compiler.jar',
                  'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler', '-kotlin-home', home, '-jvm-target', '17', '-Werror']
        version = run(common[:7] + ['-version'], 'sdk-version').strip()
        run([*common, '-module-name', 'generated_library', '-d', library / 'generated.jar', source], 'library-compile')
        (consumer / 'Main.kt').write_text(KOTLIN, encoding='utf-8')
        run([*common, '-module-name', 'external_consumer', '-classpath', library / 'generated.jar', '-include-runtime', '-d', consumer / 'consumer.jar', consumer / 'Main.kt'], 'consumer-compile')
        accepted = run(['java', '-cp', str(consumer / 'consumer.jar') + os.pathsep + str(library / 'generated.jar'), 'consumer.MainKt', schema, output], 'acceptance')
    else:
        if not args.bridge:
            parser.error('The existing actual libxml2 provider bridge is required for Swift')
        bridge = args.bridge.resolve(strict=True)
        prefix = (ROOT / 'test/resources/xml_provider/swift/main.swift').read_text(encoding='utf-8').split('// Activate cancellation only after the real provider')[0]
        (consumer / 'main.swift').write_text('import ExternalXml\n' + prefix + SWIFT, encoding='utf-8')
        version = run([args.swiftc, '--version'], 'sdk-version').strip()
        name = 'ExternalXml.dll' if os.name == 'nt' else 'libExternalXml.so'
        run([args.swiftc, '-O', '-warnings-as-errors', '-emit-module', '-emit-library', '-module-name', 'ExternalXml', '-emit-module-path', library / 'ExternalXml.swiftmodule', source, '-o', library / name], 'library-compile')
        executable = consumer / ('consumer.exe' if os.name == 'nt' else 'consumer')
        run([args.swiftc, '-O', '-warnings-as-errors', '-I', library, '-L', library, '-lExternalXml', consumer / 'main.swift', '-o', executable], 'consumer-compile')
        environment['PATH'] = str(library) + os.pathsep + environment.get('PATH', '')
        environment['LD_LIBRARY_PATH'] = str(library) + os.pathsep + environment.get('LD_LIBRARY_PATH', '')
        accepted = run([executable, bridge, schema, output], 'acceptance')
    node = etree.parse(output).getroot()
    assert node.tag == '{urn:serializer:public-xml}payload'
    assert [(child.tag, child.text) for child in node] == [
        ('{urn:serializer:public-xml}number', '7'), ('{urn:serializer:public-xml}label', 'external')]
    report = {'status': 'passed', 'language': language, 'separate_library': True,
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
              'generated_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'sdk': version, 'acceptance': accepted.strip(), 'elapsed_seconds': round(time.monotonic() - started, 3)}
    (build / 'verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
