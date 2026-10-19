# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify mobile direct identity indexes against shared XSD, native edits and exact resource policies."""
import argparse
from pathlib import Path
import sys


KOTLIN = r'''@file:OptIn(ExperimentalUnsignedTypes::class)
import java.nio.file.Files
import java.nio.file.Path
import java.util.Optional
// Require complete rejection before XML bytes or a partially valid model can be published.
fun rejected(action: () -> Unit) { try { action() } catch (_: IllegalArgumentException) { return }; error("Invalid identity operation accepted") }
// Match every independent XSD/admission decision through generated owning record entry points.
fun main(arguments: Array<String>) {
    System.setProperty("javax.xml.parsers.DocumentBuilderFactory","direct.identity.MustNotBuildKnownElementTrees")
    rejected { XmlIdentityLimits(-1,0,0,0,0,0,0) }
    for (argument in arguments.indices step 8) {
        val path=arguments[argument]; val valid=path.endsWith(".valid.xml"); val input=Files.readAllBytes(Path.of(path))
        val name=Path.of(path).fileName.toString()
        val limits=XmlIdentityLimits(arguments[argument+1].toInt(),arguments[argument+2].toInt(),arguments[argument+3].toInt(),arguments[argument+4].toInt(),arguments[argument+5].toInt(),arguments[argument+6].toInt(),arguments[argument+7].toInt())
        val output: ByteArray
        try {
            output=when {
                name.startsWith("scalar-nullable-keys") -> {
                    val owner=IdentityXmlNullableKeys.decodeXml(input,identityLimits=limits)
                    val result=owner.encodeXml(identityLimits=limits)
                    owner.items=Optional.of(Optional.of("")); rejected { owner.encodeXml() }; result
                }
                name.startsWith("scalar-key-nil") -> IdentityXmlNilKeyed.decodeXml(input,identityLimits=limits).encodeXml(identityLimits=limits)
                name.startsWith("scalar-key") -> IdentityXmlKeyed.decodeXml(input,identityLimits=limits).encodeXml(identityLimits=limits)
                name.startsWith("scalar-unique") -> IdentityXmlUniqueValue.decodeXml(input,identityLimits=limits).encodeXml(identityLimits=limits)
                name.startsWith("owners") -> IdentityXmlRegistries.decodeXml(input,identityLimits=limits).encodeXml(identityLimits=limits)
                else -> {
                    val value=IdentityXmlRegistry.decodeXml(input,identityLimits=limits); val result=value.encodeXml(identityLimits=limits)
                    val copy=IdentityXmlRegistry.decodeXml(result)
                    for (protocol in Protocol.entries) check(value.encode(protocol).contentEquals(copy.encode(protocol)))
                    if (name.startsWith("forward")) {
                        value.requests[0].id=Optional.of(8u); rejected { value.encodeXml() }; value.requests[0].id=Optional.of(7u)
                        value.entries.add(value.entries[0]); rejected { value.encodeXml() }; value.entries.removeAt(value.entries.lastIndex)
                        value.entries[0].id=Optional.empty(); rejected { value.encodeXml() }
                    }
                    result
                }
            }
        } catch (failure: IllegalArgumentException) { if (valid) throw IllegalArgumentException(path,failure); continue }
        check(valid) { "Invalid identity candidate published: $path" }; Files.write(Path.of(path+".kotlin.xml"),output)
    }
    println("Kotlin direct scoped identities, exact typed tuples, native edits and independent admission pass")
}
'''

SWIFT = r'''import Foundation
// Keep throwing expectations outside the standard library's nonthrowing assertion autoclosures.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("Identity expectation failed") } }
// Fail after an unexpectedly successful operation, outside its expected-error catch.
func rejected(_ action: () throws -> Void) throws { do { try action() } catch { return }; throw SerializerError.invalid("Invalid identity operation accepted") }
// Match exact owner-local identity semantics without invoking optional runtime schema validation.
let arguments=Array(CommandLine.arguments.dropFirst())
try rejected { _=try IdentityXmlRegistry.decodeXml(Array("<registry xmlns=\"urn:serializer:identity-test\"/>".utf8),identityLimits:XmlIdentityLimits(maxConstraints:-1)) }
for argument in stride(from:0,to:arguments.count,by:8) {
    let path=arguments[argument], valid=path.hasSuffix(".valid.xml"), name=URL(fileURLWithPath:path).lastPathComponent
    let input=Array(try Data(contentsOf:URL(fileURLWithPath:path)))
    let limits=XmlIdentityLimits(maxConstraints:Int(arguments[argument+1])!,maxFields:Int(arguments[argument+2])!,maxDepth:Int(arguments[argument+3])!,maxSelectedNodes:Int(arguments[argument+4])!,maxTuples:Int(arguments[argument+5])!,maxTupleBytes:Int(arguments[argument+6])!,maxWorkUnits:Int(arguments[argument+7])!)
    let output: [UInt8]
    do {
        if name.hasPrefix("scalar-nullable-keys") {
            var owner=try IdentityXmlNullableKeys.decodeXml(input,identityLimits:limits)
            output=try owner.encodeXml(identityLimits:limits); owner.items = .some(.some("")); try rejected { _=try owner.encodeXml() }
        } else if name.hasPrefix("scalar-key-nil") { output=try IdentityXmlNilKeyed.decodeXml(input,identityLimits:limits).encodeXml(identityLimits:limits)
        } else if name.hasPrefix("scalar-key") { output=try IdentityXmlKeyed.decodeXml(input,identityLimits:limits).encodeXml(identityLimits:limits)
        } else if name.hasPrefix("scalar-unique") { output=try IdentityXmlUniqueValue.decodeXml(input,identityLimits:limits).encodeXml(identityLimits:limits)
        } else if name.hasPrefix("owners") { output=try IdentityXmlRegistries.decodeXml(input,identityLimits:limits).encodeXml(identityLimits:limits)
        } else {
            var value=try IdentityXmlRegistry.decodeXml(input,identityLimits:limits); output=try value.encodeXml(identityLimits:limits)
            let copy=try IdentityXmlRegistry.decodeXml(output)
            for protocolValue in Protocol.allCases { try require(value.encode(protocolValue) == copy.encode(protocolValue)) }
            if name.hasPrefix("forward") {
                value.requests[0].id=8; try rejected { _=try value.encodeXml() }; value.requests[0].id=7
                value.entries.append(value.entries[0]); try rejected { _=try value.encodeXml() }; value.entries.removeLast()
                value.entries[0].id=nil; try rejected { _=try value.encodeXml() }
            }
        }
    } catch { if valid { print(path); throw error }; continue }
    try require(valid); try Data(output).write(to:URL(fileURLWithPath:path+".swift.xml"))
}
print("Swift direct scoped identities, exact typed tuples, native edits and independent admission pass")
'''


def main():
    """Generate actual mobile SDKs and independently validate every complete emitted owner scope."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--language',default='kotlin,swift');parser.add_argument('--wsl-languages',default='')
    parser.add_argument('--python-dependencies',type=Path)
    parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import etree
    import portable_xml_identity_test as fixture
    root,sdk=fixture.ROOT,fixture.sdk
    args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if not languages or len(languages)!=len(set(languages)) or any(language not in ('kotlin','swift') for language in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build==root or root in args.build.parents and 'out' not in args.build.relative_to(root).parts:
        parser.error('Repository scratch must remain beneath ignored out/')
    args.build.mkdir(parents=True,exist_ok=True)
    xsd=args.build/'identity.xsd';xsd.write_text(fixture.XSD,encoding='utf-8');provider=etree.XMLSchema(etree.parse(str(xsd)))
    probes=fixture.cases();paths=[]
    for label,document,limits,valid,_ in probes:
        path=args.build/(label+('.valid.xml' if valid else '.invalid.xml'));path.write_text(document,encoding='utf-8');paths.append((path,limits))
    oracle=args.build/'oracle';oracle.mkdir(exist_ok=True);(oracle/'Oracle.java').write_text(fixture.ORACLE,encoding='utf-8')
    sdk.run(['javac','--release','17','-Xlint:all','-Werror',oracle/'Oracle.java'])
    arguments=[]
    for (path,_),(_,_,_,_,xml_valid) in zip(paths,probes):arguments += [path,str(xml_valid).lower()]
    sdk.run(['java','-cp',oracle,'Oracle',xsd,*arguments])
    model=args.build/'model.serializer';model.write_text(fixture.MODEL,encoding='utf-8')
    config=args.build/'identity.ini';config.write_text(fixture.OPTIONS,encoding='utf-8')
    command=[args.compiler,'--input',model,'--config',config,'--language',','.join(languages),'--kotlin.package=']
    for language in languages:
        directory=args.build/language;directory.mkdir(exist_ok=True);command += ['--'+language+'.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language in languages:
        source=args.build/'source'/language;source.mkdir(parents=True,exist_ok=True)
        filename,content=('Main.kt',KOTLIN) if language=='kotlin' else ('main.swift',SWIFT)
        (source/filename).write_text(content,encoding='utf-8')
        for path,_ in paths:Path(str(path)+'.'+language+'.xml').unlink(missing_ok=True)
        launch=runner.build(language,'xml_identity',args.build/language,source);arguments=[]
        for path,limits in paths:arguments += [runner.path(language,path),*limits]
        sdk.run([*launch,*arguments]);emitted_arguments=[]
        for path,_ in paths:
            output=Path(str(path)+'.'+language+'.xml')
            if path.name.endswith('.valid.xml'):
                actual=provider.validate(etree.parse(str(output)))
                if not actual and not(path.name.startswith('scalar-unique-nil') and 'No precomputed value' in str(provider.error_log)):
                    raise AssertionError(str(provider.error_log))
                emitted_arguments += [output,'true']
            elif output.exists():raise AssertionError('Invalid identity output published: '+str(output))
        sdk.run(['java','-cp',oracle,'Oracle',xsd,*emitted_arguments])
    fixture.rejected_bindings(args,languages)
    print(f'PASS: {len(paths)} independent XSD/admission identity probes +5 atomic binding fences across {len(languages)} mobile SDKs',flush=True)


if __name__=='__main__':main()
