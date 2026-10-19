"""Qualify Java/C# exact scalar facets and portable pattern programs against independent XSDs."""
import argparse
import importlib.util
import math
from pathlib import Path
import subprocess

from lxml import etree

import go_xml_facets_test as numeric_fixture
import go_xml_pattern_test as pattern_fixture


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_facets_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

JAVA_NUMERIC = '''import java.nio.file.Files;
import java.nio.file.Path;
/** Compare exact unsigned, floating, text and attribute facets with the independent oracle. */
public final class Main {
  /** Exercise both directions and require output failures before publication. */
  public static void main(String[] args) throws Exception {
    for (String argument : args) {
      boolean valid = argument.endsWith(".valid.xml"); Schema.Facets.Sample value;
      try { value = Schema.Facets.Sample.decodeXml(Files.readAllBytes(Path.of(argument))); }
      catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(argument, expected); continue; }
      if (!valid) throw new AssertionError("Invalid scalar candidate published: " + argument);
      if (Long.compareUnsigned(value.integer, -3L) < 0 || Long.compareUnsigned(value.integer, -1L) >= 0 || value.choice != -1L || value.rounded != 0.1f || !value.text.equals("A😀") || !value.code.equals("é😀") || !value.enabled) throw new AssertionError("Scalar normalization/width");
      Files.write(Path.of(argument + ".java.xml"), value.encodeXml());
      for (int index = 0; index < 7; ++index) {
        var invalid = Schema.Facets.Sample.decodeXml(value.encodeXml());
        switch (index) { case 0: invalid.integer = -1L; break; case 1: invalid.amount = -1; break; case 2: invalid.rounded = 0.11f; break; case 3: invalid.text = "B😀"; break; case 4: invalid.choice = -2L; break; case 5: invalid.enabled = false; break; default: invalid.code = "é"; }
        try { invalid.encodeXml(); throw new AssertionError("Invalid scalar output published"); } catch (IllegalArgumentException expected) { }
      }
    }
    System.out.println("Java exact numeric, Unicode length, enumeration and whitespace facets pass");
  }
}
'''

CSHARP_NUMERIC = '''using System;
using System.IO;
using static SerializerGenerated.Schema;
// Compare exact unsigned, floating, text and attribute facets with an independent XSD.
internal static class Program {
  // Exercise both directions and require complete output rejection for invalid model values.
  private static void Main(string[] args) {
    foreach (string path in args) {
      bool valid = path.EndsWith(".valid.xml", StringComparison.Ordinal); FacetsSample value;
      try { value = FacetsSample.DecodeXml(File.ReadAllBytes(path)); }
      catch (FormatException expected) { if (valid) throw new Exception(path, expected); continue; }
      if (!valid) throw new Exception("Invalid scalar candidate published: " + path);
      if (value.Integer < ulong.MaxValue - 2 || value.Integer >= ulong.MaxValue || value.Choice != ulong.MaxValue || value.Rounded != 0.1f || value.Text != "A😀" || value.Code != "é😀" || !value.Enabled) throw new Exception("Scalar normalization/width");
      File.WriteAllBytes(path + ".csharp.xml", value.EncodeXml());
      for (int index = 0; index < 7; ++index) {
        var invalid = FacetsSample.DecodeXml(value.EncodeXml());
        switch (index) { case 0: invalid.Integer = ulong.MaxValue; break; case 1: invalid.Amount = -1; break; case 2: invalid.Rounded = 0.11f; break; case 3: invalid.Text = "B😀"; break; case 4: invalid.Choice = ulong.MaxValue - 1; break; case 5: invalid.Enabled = false; break; default: invalid.Code = "é"; break; }
        try { invalid.EncodeXml(); throw new Exception("Invalid scalar output published"); } catch (FormatException) { }
      }
    }
    Console.WriteLine("C# exact numeric, Unicode length, enumeration and whitespace facets pass");
  }
}
'''

JAVA_PATTERN = '''import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
/** Qualify lowered Unicode instructions and deterministic bounds through generated models. */
public final class Main {
  /** Compare both typed directions, including compiler-size and runtime-resource boundaries. */
  public static void main(String[] args) throws Exception {
    for (String argument : args) {
      boolean valid = argument.endsWith(".valid.xml");
      String text = Files.readString(Path.of(argument + ".txt"), StandardCharsets.UTF_8);
      try {
        var value = Schema.Patterns.Sample.decodeXml(Files.readAllBytes(Path.of(argument)));
        if (!valid || !value.value.equals(text)) throw new AssertionError("Input oracle differs: " + argument);
      } catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(argument, expected); checkResource(argument, expected); }
      var value = new Schema.Patterns.Sample(); value.value = text;
      try { byte[] output = value.encodeXml(); if (!valid) throw new AssertionError("Invalid pattern output published: " + argument); Files.write(Path.of(argument + ".java.xml"), output); }
      catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(argument, expected); checkResource(argument, expected); }
    }
    System.out.println("Java Unicode portable patterns, inherited groups, nullable loops and resource bounds pass");
  }
  /** Distinguish bounded-machine rejection from an ordinary logical facet mismatch. */
  private static void checkResource(String path, IllegalArgumentException failure) {
    String name = Path.of(path).getFileName().toString();
    if (name.startsWith("work.") && !failure.getMessage().equals("Portable XML regex work limit exceeded")) throw new AssertionError("Work budget bypassed", failure);
    if (name.startsWith("bytes.") && !failure.getMessage().equals("Portable XML regex input limit exceeded")) throw new AssertionError("Input budget bypassed", failure);
  }
}
'''

CSHARP_PATTERN = '''using System;
using System.IO;
using static SerializerGenerated.Schema;
// Qualify lowered Unicode instructions and deterministic bounds through generated models.
internal static class Program {
  // Compare both typed directions including compiler-size and runtime-resource boundaries.
  private static void Main(string[] args) {
    foreach (string path in args) {
      bool valid = path.EndsWith(".valid.xml", StringComparison.Ordinal);
      string text = File.ReadAllText(path + ".txt", System.Text.Encoding.UTF8);
      try { var value = PatternsSample.DecodeXml(File.ReadAllBytes(path)); if (!valid || value.Value != text) throw new Exception("Input oracle differs: " + path); }
      catch (FormatException expected) { if (valid) throw new Exception(path, expected); CheckResource(path, expected); }
      var candidate = new PatternsSample { Value = text };
      try { byte[] output = candidate.EncodeXml(); if (!valid) throw new Exception("Invalid pattern output published: " + path); File.WriteAllBytes(path + ".csharp.xml", output); }
      catch (FormatException expected) { if (valid) throw new Exception(path, expected); CheckResource(path, expected); }
    }
    Console.WriteLine("C# Unicode portable patterns, inherited groups, nullable loops and resource bounds pass");
  }
  // Distinguish bounded-machine rejection from an ordinary logical facet mismatch.
  private static void CheckResource(string path, FormatException failure) {
    string name = Path.GetFileName(path);
    if (name.StartsWith("work.", StringComparison.Ordinal) && failure.Message != "Portable XML regex work limit exceeded") throw new Exception("Work budget bypassed", failure);
    if (name.StartsWith("bytes.", StringComparison.Ordinal) && failure.Message != "Portable XML regex input limit exceeded") throw new Exception("Input budget bypassed", failure);
  }
}
'''


def numeric_cases(directory):
    """Extend the existing Go oracle with Boolean aliases, attributes and endpoint rounding."""
    boolean = '<xs:element name="enabled"><xs:simpleType><xs:restriction base="xs:boolean"><xs:pattern value="1|true"/></xs:restriction></xs:simpleType></xs:element>'
    attribute = '<xs:attribute name="code" use="required"><xs:simpleType><xs:restriction base="xs:string"><xs:length value="2"/><xs:enumeration value="é😀"/></xs:restriction></xs:simpleType></xs:attribute>'
    xsd = numeric_fixture.XSD.replace('</xs:sequence>', boolean + '</xs:sequence>' + attribute)
    model = 'serializer version 1.20.0; namespace facets { using schema "sample.xsd"; class sample { public uint64 integer; public double amount; public float rounded; public string text; public uint64 choice; public bool enabled; public string code; } }'
    values = ['18446744073709551614', '1', '0.1000000001', '  A😀\t', '018446744073709551615', '1', 'é😀']
    template = '<sample code="{6}"><integer>{0}</integer><amount>{1}</amount><rounded>{2}</rounded><text>{3}</text><choice>{4}</choice><enabled>{5}</enabled></sample>'
    probes = [(template.format(*values), True)]
    replacements = (
        (0, '18446744073709551613', True), (0, '18446744073709551612', False),
        (0, '18446744073709551615', False), (0, '-1', False),
        (1, '-1', False), (1, repr(math.nextafter(-1, 0)), True), (1, '-0', True),
        (1, '1.0000000000000002', False), (1, 'NaN', False), (1, 'INF', False), (1, '1e-999', True),
        (2, '0.1001', False), (2, '0.0999', False), (2, '0.0999999999', True),
        (3, 'B😀', False), (3, 'A😀x', False), (3, 'A😀\u00a0', False),
        (4, '18446744073709551614', False), (5, 'true', True), (5, '0', False), (5, 'false', False),
        (6, 'é', False), (6, 'e😀', False),
    )
    for index, replacement, valid in replacements:
        changed = values.copy(); changed[index] = replacement
        probes.append((template.format(*changed), valid))
    return xsd, model, [(f'numeric-{index}', text.encode(), None, valid)
                        for index, (text, valid) in enumerate(probes)]


def pattern_cases(large=False):
    """Keep Unicode regex semantics and large prepared program admission independently specified."""
    xsd = pattern_fixture.XSD
    samples = ('A😀', 'A😁🙏', 'A😀😁🙏', 'A🧪', 'B1', 'B12', 'B123', 'B😀', '^$',
               'x^$y', '^', '$', 'Cx', 'Cxy', 'C5', 'C😀', 'C\n', 'C\r', 'D', 'Da',
               'Daa', 'Daaa', '', 'A', 'A😀x', 'AB1', '😀', 'C&', 'C<', 'Cé', 'C中')
    if large:
        xsd = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="sample"><xs:complexType><xs:sequence><xs:element name="value"><xs:simpleType><xs:restriction base="xs:string"><xs:pattern value="a{4096}a{1000}"/></xs:restriction></xs:simpleType></xs:element></xs:sequence></xs:complexType></xs:element></xs:schema>'
        samples = ('a' * 5096, 'a' * 5095, 'a' * 5097, 'a' * 5095 + 'b')
    oracle = etree.XMLSchema(etree.fromstring(xsd.encode()))
    probes = []
    for index, text in enumerate(samples):
        root = etree.Element('sample'); etree.SubElement(root, 'value').text = text
        probes.append((f'pattern-{index}', etree.tostring(root, encoding='utf-8'), text, oracle.validate(root)))
    if large:
        for name, count in (('work', 20000), ('bytes', 8 * 1024 * 1024 + 1)):
            text = 'a' * count
            probes.append((name, ('<sample><value>' + text + '</value></sample>').encode(), text, False))
    model = 'serializer version 1.20.0; namespace patterns { using schema "sample.xsd"; class sample { public string value; } }'
    return xsd, model, probes


def main():
    """Generate and compile real SDK outputs, revalidate output, and preserve rejected artifacts."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(language not in ('java', 'csharp') for language in languages):
        parser.error('Select distinct language names from java,csharp')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True); runner = sdk.Runner(args); count = 0
    for mode in ('numeric', 'pattern', 'large'):
        directory = args.build / mode; directory.mkdir(parents=True, exist_ok=True)
        xsd, model, probes = numeric_cases(directory) if mode == 'numeric' else pattern_cases(mode == 'large')
        oracle = etree.XMLSchema(etree.fromstring(xsd.encode())); paths = []
        for name, document, text, valid in probes:
            # Resource policies may reject otherwise well-formed data before logical pattern matching.
            if name not in ('work', 'bytes') and oracle.validate(etree.fromstring(document)) != valid:
                raise AssertionError(f'Independent oracle disagrees: {name}: {oracle.error_log}')
            path = directory / f'{name}.{"valid" if valid else "invalid"}.xml'; path.write_bytes(document)
            if text is not None: Path(str(path) + '.txt').write_text(text, encoding='utf-8')
            paths.append(path)
        schema = directory / 'sample.xsd'; schema.write_text(xsd, encoding='utf-8')
        model_path = directory / 'model.serializer'; model_path.write_text(model, encoding='utf-8')
        command = [args.compiler, '--input', model_path, '--language', ','.join(languages), '--protocols', 'binary_none']
        for language in languages:
            target = directory / language; target.mkdir(parents=True, exist_ok=True)
            command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
        sdk.run(command)
        original = {language: (directory / language / sdk.OUTPUTS[language]).read_bytes() for language in languages}
        if mode == 'large' and 'java' in languages:
            encoded = original['java'].decode('utf-8').split('Pattern.prepare(new String[]{', 1)[1].split('}),', 1)[0]
            fragments = encoded.split('"')[1::2]
            if sum(map(len, fragments)) <= 65535 or max(map(len, fragments)) > 10000:
                raise AssertionError('Large prepared program must exceed one JVM constant and use bounded fragments')
        for language in languages:
            source = directory / 'source' / language; source.mkdir(parents=True, exist_ok=True)
            content = (JAVA_NUMERIC if mode == 'numeric' else JAVA_PATTERN) if language == 'java' else (CSHARP_NUMERIC if mode == 'numeric' else CSHARP_PATTERN)
            (source / ('Main.java' if language == 'java' else 'Program.cs')).write_text(content, encoding='utf-8')
            for path in paths: Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            launch = runner.build(language, 'xml_' + mode, directory / language, source)
            sdk.run([*launch, *(runner.path(language, path) for path in paths)])
            for path in paths:
                emitted = Path(str(path) + f'.{language}.xml')
                if path.name.endswith('.valid.xml'): oracle.assertValid(etree.parse(str(emitted)))
                elif emitted.exists(): raise AssertionError(f'Invalid model output published: {emitted}')
        if mode == 'pattern':
            schema.write_text(xsd.replace('A[😀-🙏]{1,2}|B[0-9]+|^$|C[^0-9]{1,2}|D(a?)*', r'\p{L}+'), encoding='utf-8')
            for language in languages:
                rejected = subprocess.run([str(value) for value in command[:3]] + ['--language', language, '--protocols', 'binary_none', f'--{language}.output', str(directory / language / sdk.OUTPUTS[language])], capture_output=True, text=True)
                if rejected.returncode == 0 or 'approved portable XSD regex subset' not in rejected.stdout + rejected.stderr:
                    raise AssertionError(f'Unsupported {language} regex needs a precise portable-subset diagnostic')
                if (directory / language / sdk.OUTPUTS[language]).read_bytes() != original[language]: raise AssertionError('Unsupported regex replaced generated source')
        count += len(paths)
    print(f'PASS: {count} independent scalar/pattern and resource probes across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
