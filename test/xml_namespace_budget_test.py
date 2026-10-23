# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compare actual SDK namespace admissions with independent structural and UTF-8 byte counts."""
import argparse
import hashlib
import importlib.util
import os
import json
from pathlib import Path
import shutil
import xml.parsers.expat

from xml_namespace_budget_consumers import CONSUMERS as ADDITIONAL_CONSUMERS


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('serializer_namespace_budget_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdk)
XML_SPEC = importlib.util.spec_from_file_location('serializer_namespace_xml_sdk', ROOT / 'example/xml/run.py')
xml_sdk = importlib.util.module_from_spec(XML_SPEC)
XML_SPEC.loader.exec_module(xml_sdk)

JAVA = '''import java.nio.file.Files;
import java.nio.file.Path;
/** Compare local encode/decode admission before publishing a complete result. */
public final class Main {
  /** Retain one default-policy output, then probe every structural boundary independently. */
  public static void main(String[] args) throws Exception {
    var input=Files.readAllBytes(Path.of(args[0]));
    var value=Schema.Quotas.Sample.decodeXml(input);
    var output=value.encodeXml(); Files.write(Path.of(args[1]),output);
    var rows=new StringBuilder(); var defaults=Schema.Limits.defaults();
    for(String mode:new String[]{"structure","text"}) for(int count=0;count<=128;++count) {
      var limits=new Schema.Limits(defaults.maxBytes,mode.equals("text")?count:defaults.maxStringBytes,mode.equals("structure")?count:defaults.maxElements,defaults.maxDepth);
      boolean encoded=true,decoded=true;
      try { value.encodeXml(limits,null); } catch(IllegalArgumentException expected) { encoded=false; }
      try { Schema.Quotas.Sample.decodeXml(output,limits,null); } catch(IllegalArgumentException expected) { decoded=false; }
      rows.append(mode+" "+count+" "+encoded+" "+decoded+"\\n");
    }
    Files.writeString(Path.of(args[2]),rows);
  }
}
'''

CSHARP = '''using System;
using System.IO;
using System.Text;
using static SerializerGenerated.Schema;
// Compare local encode/decode admission before publishing a complete result.
internal static class Program {
  // Retain default output, then check every structural boundary with fresh limits.
  private static void Main(string[] args) {
    var value=QuotasSample.DecodeXml(File.ReadAllBytes(args[0]));
    var output=value.EncodeXml(); File.WriteAllBytes(args[1],output);
    var rows=new StringBuilder(); var defaults=new Limits();
    foreach(string mode in new[]{"structure","text"}) for(int count=0;count<=128;++count) {
      var limits=new Limits(defaults.MaxBytes,mode=="text"?count:defaults.MaxStringBytes,mode=="structure"?count:defaults.MaxElements,defaults.MaxDepth);
      bool encoded=true,decoded=true;
      try { value.EncodeXml(limits); } catch(FormatException) { encoded=false; }
      try { QuotasSample.DecodeXml(output,limits); } catch(FormatException) { decoded=false; }
      rows.Append($"{mode} {count} {encoded.ToString().ToLowerInvariant()} {decoded.ToString().ToLowerInvariant()}\\n");
    }
    File.WriteAllText(args[2],rows.ToString());
  }
}
'''

KOTLIN = '''import java.nio.file.Files
import java.nio.file.Path
// Retain default output, then independently test both structural admission boundaries.
fun main(args: Array<String>) {
    val value=QuotasSample.decodeXml(Files.readAllBytes(Path.of(args[0])))
    val output=value.encodeXml(); Files.write(Path.of(args[1]),output)
    val rows=StringBuilder()
    for(mode in listOf("structure","text")) for(count in 0..128) {
        val limits=if(mode=="structure") XmlLimits(maxNodes=count) else XmlLimits(maxTextBytes=count); var encoded=true; var decoded=true
        try { value.encodeXml(limits) } catch(_: IllegalArgumentException) { encoded=false }
        try { QuotasSample.decodeXml(output,limits) } catch(_: IllegalArgumentException) { decoded=false }
        rows.append("$mode $count $encoded $decoded\\n")
    }
    Files.writeString(Path.of(args[2]),rows)
}
'''

SWIFT = '''import Foundation
// Probe structural and text admission without catching assertion failures.
let args=Array(CommandLine.arguments.dropFirst())
let value=try QuotasSample.decodeXml(Array(Data(contentsOf:URL(fileURLWithPath:args[0]))))
let output=try value.encodeXml(); try Data(output).write(to:URL(fileURLWithPath:args[1]))
var rows=""
for mode in ["structure","text"] { for count in 0...128 {
    var limits=XmlLimits(); if mode=="structure" { limits.maxNodes=count } else { limits.maxTextBytes=count }
    var encoded=true; var decoded=true
    do { _=try value.encodeXml(limits) } catch { encoded=false }
    do { _=try QuotasSample.decodeXml(output,limits) } catch { decoded=false }
    rows += "\\(mode) \\(count) \\(encoded) \\(decoded)\\n"
}
}
try rows.write(to:URL(fileURLWithPath:args[2]),atomically:true,encoding:.utf8)
'''

CONSUMERS = {'java': ('Main.java', JAVA), 'csharp': ('Program.cs', CSHARP),
             'kotlin': ('Main.kt', KOTLIN), 'swift': ('main.swift', SWIFT)}
CONSUMERS.update(ADDITIONAL_CONSUMERS)


def build_cpp(args, runner, directory, source):
    """Link the selected native runtime and XML provider through actual CMake discovery."""
    shutil.copyfile(source / 'main.cpp', directory / 'main.cpp')
    library = args.cpp_library or args.compiler.parent / (
        'serializer_runtime.lib' if os.name == 'nt' else 'libserializer_runtime.a')
    cmake_path = lambda path: '[=[' + str(path.resolve()).replace('\\', '/') + ']=]'
    (directory / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 4.0)\nproject(xml_namespace_cpp LANGUAGES CXX)\n'
        'find_package(LibXml2 REQUIRED)\nadd_executable(namespace_cpp main.cpp)\n'
        'target_compile_features(namespace_cpp PRIVATE cxx_std_23)\n'
        'target_include_directories(namespace_cpp PRIVATE ' + cmake_path(ROOT / 'include') + ' ' +
        cmake_path(args.compiler.parent / 'generated/include') + ')\n'
        'target_link_libraries(namespace_cpp PRIVATE ' + cmake_path(library) + ' LibXml2::LibXml2)\n'
        'if(MSVC)\ntarget_compile_options(namespace_cpp PRIVATE /W4 /WX /utf-8 /EHsc)\n'
        'else()\ntarget_compile_options(namespace_cpp PRIVATE -Wall -Wextra -Wpedantic -Werror)\nendif()\n',
        encoding='utf-8')
    native = directory / 'native'
    command = ['cmake', '-S', directory, '-B', native, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release']
    if args.libxml2_prefix:
        command.append('-DCMAKE_PREFIX_PATH=' + str(args.libxml2_prefix.resolve()))
    sdk.run(command)
    sdk.run(['cmake', '--build', native, '--parallel', '2'])
    executable = native / ('namespace_cpp.exe' if os.name == 'nt' else 'namespace_cpp')
    if os.name == 'nt' and args.libxml2_prefix:
        for library in (args.libxml2_prefix / 'bin').glob('*.dll'):
            shutil.copyfile(library, executable.parent / library.name)
    return runner.launch('cpp', runner.path('cpp', executable))


def counts(document):
    """Count physical declarations separately from ordinary attributes using Expat callbacks."""
    result = {'nodes': 0, 'attributes': 0, 'namespaces': 0, 'name_bytes': 0,
              'expanded_name_bytes': 0, 'value_bytes': 0, 'namespace_bytes': 0,
              'maximum_value_bytes': 0}
    parser = xml.parsers.expat.ParserCreate(namespace_separator='|')

    def name_bytes(name):
        """Derive both supported expanded-name accounting conventions from actual XML identities."""
        result['name_bytes'] += len(name.replace('|', '').encode('utf-8'))
        result['expanded_name_bytes'] += len(name.encode('utf-8')) + (1 if '|' in name else 0)

    def element(name, attributes):
        """Count each element and its actual non-namespace attributes once."""
        result['nodes'] += 1
        result['attributes'] += len(attributes)
        name_bytes(name)
        for key, value in attributes.items():
            name_bytes(key)
            result['value_bytes'] += len(value.encode('utf-8'))
            result['maximum_value_bytes'] = max(result['maximum_value_bytes'], len(value.encode('utf-8')))

    def namespace(prefix, uri):
        """Count physical declarations, including explicit empty-default resets."""
        result['namespaces'] += 1
        result['namespace_bytes'] += len((prefix or '').encode('utf-8')) + len((uri or '').encode('utf-8'))
        result['maximum_value_bytes'] = max(result['maximum_value_bytes'], len((uri or '').encode('utf-8')))

    def text(value):
        """Count decoded text rather than the escaped presentation's byte length."""
        result['value_bytes'] += len(value.encode('utf-8'))
        result['maximum_value_bytes'] = max(result['maximum_value_bytes'], len(value.encode('utf-8')))

    parser.StartElementHandler = element
    parser.StartNamespaceDeclHandler = namespace
    parser.CharacterDataHandler = text
    parser.Parse(document, True)
    return result


def check_admissions(profile, language, output, results, cpp_token_chunk_bytes=0):
    """Compare every actual decision against independent expanded-name and physical declaration counts."""
    mismatches = []
    measured = counts(output.read_bytes())
    required = max(measured['nodes'], measured['attributes'] + measured['namespaces'])
    if language in ('kotlin', 'swift', 'c'):
        required = measured['nodes'] + measured['attributes'] + measured['namespaces']
    text_required = (measured['expanded_name_bytes'] if language in ('java', 'csharp', 'python') else
                     measured['name_bytes']) + measured['value_bytes'] + measured['namespace_bytes']
    if language == 'c':
        text_required = measured['maximum_value_bytes']
    if language == 'cpp':
        required = max(measured['nodes'], measured['attributes'])
        text_required = measured['value_bytes']
    rows = results.read_text(encoding='utf-8').splitlines()
    modes = ('structure', 'text', 'namespaces') if language == 'cpp' else ('structure', 'text')
    report = {'profile': profile, 'language': language, 'counts': measured,
              'decisions': len(rows)*2,
              'cpp_token_chunk_bytes': cpp_token_chunk_bytes if language == 'cpp' else None}
    if len(rows) != len(modes) * 129:
        raise AssertionError('Missing boundary decisions: ' + language)
    for index, row in enumerate(rows):
        mode, count, encoded, decoded = row.split()
        if mode != modes[index // 129] or int(count) != index % 129:
            raise AssertionError('Duplicate or out-of-order boundary decision: ' + row)
        minimum = required if mode == 'structure' else measured['namespaces'] if mode == 'namespaces' else text_required
        expected = str(int(count) >= minimum).lower()
        if encoded != expected or decoded != expected:
            if not mismatches or not mismatches[-1].startswith(f'{profile}/{language}:'):
                mismatches.append(f'{profile}/{language}: {row}, expected {expected}; {measured}')
    return report, mismatches


def main():
    """Generate all selected profiles, run real SDK consumers and assert independent boundaries."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,kotlin,swift')
    parser.add_argument('--wsl-languages', default='')
    parser.add_argument('--cargo-home', type=Path)
    parser.add_argument('--libxml2-prefix', type=Path)
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--cpp-library', type=Path)
    parser.add_argument('--cpp-token-chunk-bytes', type=int, default=0)
    parser.add_argument('--profile', default='plain,namespaced,qualified,reset,qualified_multiple,xml_attribute')
    parser.set_defaults(executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve(strict=True)
    args.build = args.build.resolve()
    if args.libxml2_prefix:
        args.libxml2_prefix = args.libxml2_prefix.resolve(strict=True)
    if args.node_dependencies:
        args.node_dependencies = args.node_dependencies.resolve(strict=True)
    languages = args.language.split(',')
    profiles = args.profile.split(',')
    if len(profiles) != len(set(profiles)) or any(value not in (
            'plain', 'namespaced', 'qualified', 'reset', 'qualified_multiple', 'xml_attribute') for value in profiles):
        parser.error('Select distinct documented namespace profiles')
    if args.cpp_token_chunk_bytes < 0:
        parser.error('Token chunks must be positive; zero selects retained-tree codecs')
    if len(languages) != len(set(languages)) or any(value not in CONSUMERS for value in languages):
        parser.error('Choose distinct languages from ' + ','.join(CONSUMERS))
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Repository output must stay beneath ignored out/')
    runner = sdk.Runner(args)
    if args.cargo_home:
        os.environ['CARGO_HOME'] = str(args.cargo_home.resolve(strict=True))
        os.environ['CARGO_NET_OFFLINE'] = 'true'
    decisions = 0
    mismatches = []
    reports = []
    for profile in profiles:
        directory = args.build / profile
        directory.mkdir(parents=True, exist_ok=True)
        namespace = '' if profile == 'plain' else 'urn:quota'
        attribute = ', namespace("urn:attribute")' if profile in ('qualified', 'qualified_multiple') else (
            ', namespace("http://www.w3.org/XML/1998/namespace")' if profile == 'xml_attribute' else '')
        extra = 'public string extra xml(attribute, namespace("urn:attribute")); ' if profile == 'qualified_multiple' else ''
        child = ' xml(namespace(""))' if profile == 'reset' else ''
        model = directory / 'model.serializer'
        model.write_text('serializer version 1.20.0; namespace quotas { class sample { '
                         'public string code xml(attribute' + attribute + '); '
                         + extra + 'public uint32 value' + child + '; } }', encoding='utf-8')
        config = directory / 'model.ini'
        config.write_text('[xml]\nenabled = true\n' +
                          ('namespace = ' + namespace + '\n' if namespace else ''), encoding='utf-8')
        input_path = directory / 'input.xml'
        prefix = 'q:' if namespace else ''
        declarations = ' xmlns:q="urn:quota"' if namespace else ''
        code = 'code="x"'
        if profile in ('qualified', 'qualified_multiple'):
            declarations += ' xmlns:a="urn:attribute"'
            code = 'a:code="x"'
            if profile == 'qualified_multiple':
                code += ' a:extra="y"'
        elif profile == 'xml_attribute':
            code = 'xml:code="x"'
        value_name = '' if profile == 'reset' else prefix
        input_path.write_text('<' + prefix + 'sample' + declarations + ' ' + code + '><' +
                              value_name + 'value>7</' + value_name + 'value></' + prefix + 'sample>',
                              encoding='utf-8')
        targets = {('js' if language == 'javascript' else language):
                   directory / language / sdk.OUTPUTS[language] for language in languages}
        if 'typescript' in languages and 'javascript' not in languages:
            targets['js'] = directory / 'typescript/schema.mjs'
        command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(targets),
                   '--protocols', 'binary_none', '--kotlin.package=', '--go.package', 'main']
        for language, destination in targets.items():
            destination.parent.mkdir(exist_ok=True)
            command += [f'--{language}.output', destination]
        sdk.run(command)
        if 'typescript' in languages and 'javascript' in languages:
            shutil.copyfile(targets['js'], directory / 'typescript/schema.mjs')
        for language in languages:
            source = directory / 'source' / language
            source.mkdir(parents=True, exist_ok=True)
            filename, content = CONSUMERS[language]
            (source / filename).write_text(content, encoding='utf-8')
            if language in ('javascript', 'typescript'):
                if args.node_dependencies is None:
                    parser.error('JS/TS require --node-dependencies with installed XML provider packages')
                for package in ('saxes', 'xmlchars', '@xmldom/xmldom'):
                    shutil.copytree(args.node_dependencies / package,
                                    directory / language / 'node_modules' / package, dirs_exist_ok=True)
            if language == 'typescript':
                shutil.copyfile(source / 'main.mts', directory / language / 'main.mts')
                shutil.copyfile(ROOT / 'example/typescript/node.d.mts', directory / language / 'node.d.mts')
                sdk.run(runner.command(language, 'node',
                                      runner.path(language, args.node_dependencies / 'typescript/bin/tsc'),
                                      '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                                      '--moduleResolution', 'NodeNext',
                                      runner.path(language, directory / language / 'main.mts'),
                                      runner.path(language, directory / language / 'node.d.mts')))
                launch = runner.command(language, 'node', runner.path(language, directory / language / 'main.mjs'))
            elif language == 'cpp':
                launch = build_cpp(args, runner, directory / language, source)
            elif language == 'c':
                launch = xml_sdk.build_c(args, runner, directory / language, directory / 'source')
            elif language == 'rust':
                for filename in ('Cargo.toml', 'Cargo.lock'):
                    shutil.copyfile(ROOT / 'example/xml/rust' / filename, directory / language / filename)
                shutil.copyfile(source / 'main.rs', directory / language / 'main.rs')
                sdk.run(runner.command(language, 'cargo', 'build', '--locked', '--release',
                                      '--manifest-path', runner.path(language, directory / language / 'Cargo.toml')))
                executable = directory / language / 'target/release' / (
                    'serializer-xml-example.exe' if os.name == 'nt' and language not in runner.wsl else 'serializer-xml-example')
                launch = runner.launch(language, runner.path(language, executable))
            else:
                launch = runner.build(language, 'xml_namespace_budget', directory / language, source)
            output = directory / (language + '.xml')
            results = directory / (language + '.txt')
            command = [*launch, *(runner.path(language, path) for path in (input_path, output, results))]
            if language == 'cpp' and args.cpp_token_chunk_bytes:
                command.append(str(args.cpp_token_chunk_bytes))
            sdk.run(command)
            report, failures = check_admissions(profile, language, output, results, args.cpp_token_chunk_bytes)
            report['consumer_sha256'] = hashlib.sha256(content.encode('utf-8')).hexdigest()
            module = directory / language / sdk.OUTPUTS[language]
            report['generated_sha256'] = hashlib.sha256(module.read_bytes()).hexdigest()
            reports.append(report)
            mismatches.extend(failures)
            decisions += report['decisions']
    summary = {'compiler_sha256': hashlib.sha256(args.compiler.read_bytes()).hexdigest(),
               'decisions': decisions, 'profiles': reports, 'mismatches': mismatches}
    (args.build / 'summary.json').write_text(json.dumps(summary, indent=2)+'\n', encoding='utf-8')
    if mismatches:
        raise AssertionError('Namespace admission mismatches:\n' + '\n'.join(mismatches))
    print(f'PASS: {decisions} independent namespace structural/text admission decisions', flush=True)


if __name__ == '__main__':
    main()
