# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated Kotlin/Swift approved HTML fragments against independent ordinary HTML parsers."""
import argparse
import importlib.util
from pathlib import Path
import sys

from html_interop_test import trace, tree_trace
from javascript_html_test import FIXTURES

ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('serializer_mobile_html_sdk',ROOT/'example/run.py')
sdk=importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
INVALID=[
    '<sub><div>x</div></sub>','<sup><p>x</p></sup>','<sub/>','<sup>unclosed',
    '<sub onclick=x>x</sub>',
    '<!DOCTYPE html>','<svg></svg>','<script>x</script>','<p><div>x</div></p>',
    '<ul><span>x</span></ul>','<ul>text</ul>','<li>x</li>','<a><a>x</a></a>','<span/>','<p>unclosed',
    '<br></br>','<p hidden=wrong>x</p>','<p title>x</p>','<p onclick="x">x</p>','<p id=x ID=y>x</p>',
    '<p>&copy;</p>','<p>&amp</p>','<p>&#0;</p>','<p>&#xD800;</p>','<p>&#x110000;</p>',
    '<!--a--b-->','<p title="x"id="y"></p>','<p title=x`y></p>','<?xml version="1.0"?>',
]

KOTLIN=r'''import java.nio.file.Files
import java.nio.file.Path
// Require invalid subset or resource operations to reject before returning a candidate.
fun rejected(action: () -> Unit) {
    try { action() } catch (_: IllegalArgumentException) { return }
    error("Invalid HTML operation accepted")
}
// Exercise ordinary HTML independently of generated XML mappings and native serialization fields.
fun main(arguments: Array<String>) {
    for (argument in arguments) {
        val input=Files.readAllBytes(Path.of(argument)); val valid=argument.endsWith(".valid.html")
        val events=try { Html.parseFragment(input) } catch (error: IllegalArgumentException) { if (valid) throw error; continue }
        check(valid) { "Invalid HTML candidate published: $argument" }
        val output=Html.writeFragment(events)
        check(Html.writeFragment(Html.parseFragment(output)).contentEquals(output))
        Files.write(Path.of(argument+".kotlin.html"),output)
    }
    check(Html.supportedSubset().contains("no doctype"))
    val input="<DIV hidden title=\"x\">é&amp;😀<br></DIV>".toByteArray(Charsets.UTF_8)
    val events=Html.parseFragment(input); val output=Html.writeFragment(events)
    val exact=HtmlLimits(maxInputBytes=input.size,maxOutputBytes=output.size,maxEvents=events.size,
        maxDepth=2,maxAttributesPerElement=2,maxTextBytes=7)
    check(Html.writeFragment(Html.parseFragment(input,exact),exact).contentEquals(output))
    for (short in listOf(exact.copy(maxEvents=events.size-1),exact.copy(maxDepth=1),
        exact.copy(maxAttributesPerElement=1),exact.copy(maxTextBytes=6))) {
        rejected { Html.parseFragment(input,short) }; rejected { Html.writeFragment(events,short) }
    }
    rejected { Html.parseFragment(input,exact.copy(maxInputBytes=input.size-1)) }
    rejected { Html.writeFragment(events,exact.copy(maxOutputBytes=output.size-1)) }
    rejected { HtmlLimits(maxDepth=-1) }; rejected { Html.parseFragment(byteArrayOf(0xc3.toByte(),0x28)) }
    val authored=listOf(HtmlEvent("start_fragment"),HtmlEvent("start_element","span",listOf(HtmlAttribute("hidden",boolean=true))),
        HtmlEvent("text",text="<&\r\""),HtmlEvent("end_element","span"),HtmlEvent("end_fragment"))
    check(Html.writeFragment(authored).toString(Charsets.UTF_8)=="<span hidden>&lt;&amp;&#13;\"</span>")
    rejected { Html.writeFragment(events.map { if (it.kind=="text") it.copy(text="\uD800") else it }) }
    rejected { Html.writeFragment(listOf(HtmlEvent("start_fragment"),HtmlEvent("end_fragment")),HtmlLimits(maxEvents=1)) }
    println("Kotlin HTML traces, explicit balanced subset, fatal UTF-8 and exact policies pass")
}
'''

SWIFT=r'''import Foundation
// Keep potentially throwing checks out of nonthrowing assertion autoclosures.
func require(_ value: Bool) throws { guard value else { throw SerializerError.invalid("HTML expectation failed") } }
// Require invalid subset or resource operations to reject before returning a complete candidate.
func rejected(_ action: () throws -> Void) throws {
    do { try action() } catch { return }
    throw SerializerError.invalid("Invalid HTML operation accepted")
}
// Exercise actual ordinary HTML fragments independently of XML namespaces or parser recovery.
for argument in CommandLine.arguments.dropFirst() {
    let input=Array(try Data(contentsOf:URL(fileURLWithPath:argument))); let valid=argument.hasSuffix(".valid.html")
    let events: [HtmlEvent]
    do { events=try Html.parseFragment(input) } catch { if valid { throw error }; continue }
    try require(valid)
    let output=try Html.writeFragment(events)
    let repeated=try Html.writeFragment(Html.parseFragment(output))
    try require(output == repeated)
    try Data(output).write(to:URL(fileURLWithPath:argument+".swift.html"))
}
try require(Html.supportedSubset().contains("no doctype"))
let input=Array("<DIV hidden title=\"x\">é&amp;😀<br></DIV>".utf8)
let events=try Html.parseFragment(input); let output=try Html.writeFragment(events)
let exact=HtmlLimits(maxInputBytes:input.count,maxOutputBytes:output.count,maxEvents:events.count,
    maxDepth:2,maxAttributesPerElement:2,maxTextBytes:7)
let checked=try Html.writeFragment(Html.parseFragment(input,limits:exact),limits:exact)
try require(checked == output)
for kind in 0..<4 {
    var short=exact
    switch kind { case 0: short.maxEvents -= 1; case 1: short.maxDepth=1; case 2: short.maxAttributesPerElement=1; default: short.maxTextBytes=6 }
    try rejected { _=try Html.parseFragment(input,limits:short) }; try rejected { _=try Html.writeFragment(events,limits:short) }
}
var inputShort=exact; inputShort.maxInputBytes -= 1
try rejected { _=try Html.parseFragment(input,limits:inputShort) }
var outputShort=exact; outputShort.maxOutputBytes -= 1
try rejected { _=try Html.writeFragment(events,limits:outputShort) }
try rejected { _=try Html.parseFragment(input,limits:HtmlLimits(maxDepth:-1)) }
try rejected { _=try Html.parseFragment([0xc3,0x28]) }
let authored=[HtmlEvent("start_fragment"),HtmlEvent("start_element",name:"span",attributes:[HtmlAttribute("hidden",boolean:true)]),
    HtmlEvent("text",text:"<&\r\""),HtmlEvent("end_element",name:"span"),HtmlEvent("end_fragment")]
let authoredOutput=try Html.writeFragment(authored)
try require(String(decoding:authoredOutput,as:UTF8.self) == "<span hidden>&lt;&amp;&#13;\"</span>")
try rejected { _=try Html.writeFragment([HtmlEvent("start_fragment"),HtmlEvent("end_fragment")],limits:HtmlLimits(maxEvents:1)) }
print("Swift HTML traces, explicit balanced subset, fatal UTF-8 and exact policies pass")
'''


def main():
    """Generate actual XML-opted-in SDKs and verify ordinary HTML with both independent token/tree oracles."""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',required=True,type=Path);parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--language',default='kotlin,swift');parser.add_argument('--wsl-languages',default='')
    parser.add_argument('--kotlinc',type=Path,help='Explicit installed SDK; invoke its compiler JAR directly')
    parser.add_argument('--python-dependencies',type=Path)
    parser.set_defaults(cpp_library=None,executable=None,sanitize=False)
    args=parser.parse_args()
    if args.python_dependencies:sys.path.insert(0,str(args.python_dependencies.resolve()))
    from lxml import html as provider
    provider.fragment_fromstring('<p>required independent tree provider</p>')
    args.compiler=args.compiler.resolve();args.build=args.build.resolve();languages=args.language.split(',')
    if not languages or len(set(languages))!=len(languages) or any(value not in ('kotlin','swift') for value in languages):
        parser.error('Choose distinct languages from kotlin,swift')
    if args.build==ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository scratch must remain beneath ignored out/')
    args.build.mkdir(parents=True,exist_ok=True);paths=[]
    for index,document in enumerate(FIXTURES+INVALID):
        valid=index<len(FIXTURES);path=args.build/(str(index)+('.valid.html' if valid else '.invalid.html'))
        path.write_bytes(document.encode());paths.append(path)
    source=args.build/'html.serializer';source.write_text('serializer version 1.20.0; namespace mobile_html { class sample { public uint32 value(1); }}',encoding='utf-8')
    command=[args.compiler,'--language',','.join(languages),'--input',source,'--xml.enabled','true','--kotlin.package=']
    for language in languages:
        directory=args.build/language;directory.mkdir(parents=True,exist_ok=True)
        command += ['--'+language+'.output',directory/sdk.OUTPUTS[language]]
    sdk.run(command);runner=sdk.Runner(args)
    for language in languages:
        directory=args.build/'source'/language;directory.mkdir(parents=True,exist_ok=True)
        filename,content=('Main.kt',KOTLIN) if language=='kotlin' else ('main.swift',SWIFT)
        (directory/filename).write_text(content,encoding='utf-8')
        for path in paths:Path(str(path)+'.'+language+'.html').unlink(missing_ok=True)
        if language=='kotlin' and args.kotlinc:
            home=args.kotlinc.resolve(strict=True).parent.parent
            jar=runner.path(language,args.build/language/'main.jar')
            sdk.run(runner.command(language,'java','-Xmx2g','-cp',runner.path(language,home/'lib/kotlin-compiler.jar'),
                'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler','-kotlin-home',runner.path(language,home),
                '-jvm-target','17','-Werror',runner.path(language,args.build/language/'Schema.kt'),
                runner.path(language,directory/'Main.kt'),'-include-runtime','-d',jar))
            launch=runner.command(language,'java','-jar',jar)
        else:
            launch=runner.build(language,'html_fragments',args.build/language,directory)
        sdk.run([*launch,*(runner.path(language,path) for path in paths)])
        for index,path in enumerate(paths):
            output=Path(str(path)+'.'+language+'.html')
            if index<len(FIXTURES):
                actual=output.read_bytes().decode()
                if trace(FIXTURES[index])!=trace(actual) or tree_trace(FIXTURES[index])!=tree_trace(actual):
                    raise AssertionError('Independent ordinary HTML oracle differs: '+language+': '+str(index))
            elif output.exists():raise AssertionError('Invalid subset candidate published HTML')
    print(f'PASS: {len(paths)} HTML subset decisions, two independent semantic oracles and exact bounds across {len(languages)} actual SDKs',flush=True)


if __name__=='__main__':main()
