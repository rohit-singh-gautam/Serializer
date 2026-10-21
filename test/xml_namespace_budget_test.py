# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Compare actual SDK namespace admissions with independent structural and UTF-8 byte counts."""
import argparse
import importlib.util
from pathlib import Path
import xml.parsers.expat


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('serializer_namespace_budget_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sdk)

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


def counts(document):
    """Count physical declarations separately from ordinary attributes using Expat callbacks."""
    result = {'nodes': 0, 'attributes': 0, 'namespaces': 0, 'name_bytes': 0,
              'expanded_name_bytes': 0, 'value_bytes': 0, 'namespace_bytes': 0}
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

    def namespace(prefix, uri):
        """Count physical declarations, including explicit empty-default resets."""
        result['namespaces'] += 1
        result['namespace_bytes'] += len((prefix or '').encode('utf-8')) + len((uri or '').encode('utf-8'))

    def text(value):
        """Count decoded text rather than the escaped presentation's byte length."""
        result['value_bytes'] += len(value.encode('utf-8'))

    parser.StartElementHandler = element
    parser.StartNamespaceDeclHandler = namespace
    parser.CharacterDataHandler = text
    parser.Parse(document, True)
    return result


def main():
    """Generate all selected profiles, run real SDK consumers and assert independent boundaries."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default=','.join(CONSUMERS))
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args()
    args.compiler = args.compiler.resolve(strict=True)
    args.build = args.build.resolve()
    languages = args.language.split(',')
    if len(languages) != len(set(languages)) or any(value not in CONSUMERS for value in languages):
        parser.error('Choose distinct languages from ' + ','.join(CONSUMERS))
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('Repository output must stay beneath ignored out/')
    runner = sdk.Runner(args)
    decisions = 0
    for profile in ('plain', 'namespaced', 'qualified', 'reset'):
        directory = args.build / profile
        directory.mkdir(parents=True, exist_ok=True)
        namespace = '' if profile == 'plain' else 'urn:quota'
        attribute = ', namespace("urn:attribute")' if profile == 'qualified' else ''
        child = ' xml(namespace(""))' if profile == 'reset' else ''
        model = directory / 'model.serializer'
        model.write_text('serializer version 1.20.0; namespace quotas { class sample { '
                         'public string code xml(attribute' + attribute + '); '
                         'public uint32 value' + child + '; } }', encoding='utf-8')
        config = directory / 'model.ini'
        config.write_text('[xml]\nenabled = true\n' +
                          ('namespace = ' + namespace + '\n' if namespace else ''), encoding='utf-8')
        input_path = directory / 'input.xml'
        prefix = 'q:' if namespace else ''
        declarations = ' xmlns:q="urn:quota"' if namespace else ''
        code = 'code="x"'
        if profile == 'qualified':
            declarations += ' xmlns:a="urn:attribute"'
            code = 'a:code="x"'
        value_name = '' if profile == 'reset' else prefix
        input_path.write_text('<' + prefix + 'sample' + declarations + ' ' + code + '><' +
                              value_name + 'value>7</' + value_name + 'value></' + prefix + 'sample>',
                              encoding='utf-8')
        command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages),
                   '--protocols', 'binary_none', '--kotlin.package=']
        for language in languages:
            generated = directory / language
            generated.mkdir(exist_ok=True)
            command += [f'--{language}.output', generated / sdk.OUTPUTS[language]]
        sdk.run(command)
        for language in languages:
            source = directory / 'source' / language
            source.mkdir(parents=True, exist_ok=True)
            filename, content = CONSUMERS[language]
            (source / filename).write_text(content, encoding='utf-8')
            launch = runner.build(language, 'xml_namespace_budget', directory / language, source)
            output = directory / (language + '.xml')
            results = directory / (language + '.txt')
            sdk.run([*launch, *(runner.path(language, path) for path in (input_path, output, results))])
            measured = counts(output.read_bytes())
            required = max(measured['nodes'], measured['attributes'] + measured['namespaces'])
            if language in ('kotlin', 'swift'):
                required = measured['nodes'] + measured['attributes'] + measured['namespaces']
            text_required = (measured['expanded_name_bytes'] if language in ('java', 'csharp') else
                             measured['name_bytes']) + measured['value_bytes'] + measured['namespace_bytes']
            rows = results.read_text(encoding='utf-8').splitlines()
            if len(rows) != 2 * 129:
                raise AssertionError('Missing boundary decisions: ' + language)
            for index, row in enumerate(rows):
                mode, count, encoded, decoded = row.split()
                if mode != ('structure' if index < 129 else 'text') or int(count) != index % 129:
                    raise AssertionError('Duplicate or out-of-order boundary decision: ' + row)
                expected = str(int(count) >= (required if mode == 'structure' else text_required)).lower()
                if encoded != expected or decoded != expected:
                    raise AssertionError(f'{profile}/{language}: {row}, expected {expected}; {measured}')
                decisions += 2
    print(f'PASS: {decisions} independent namespace structural/text admission decisions', flush=True)


if __name__ == '__main__':
    main()
