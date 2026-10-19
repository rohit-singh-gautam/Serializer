"""Qualify generated Java/C# maps against the same independent typed-key corpus as Go."""
import argparse
import importlib.util
from pathlib import Path

from lxml import etree

import go_xml_map_test as fixture


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_maps_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

JAVA = r'''import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
/** Qualify typed-key maps without changing ordinary native model/storage contracts. */
public final class Main {
  /** Require invalid map operations to fail before a complete result returns. */
  private static void rejected(Runnable operation) {
    try { operation.run(); } catch (IllegalArgumentException expected) { return; }
    throw new AssertionError("Invalid map candidate published");
  }
  /** Compare the independent oracle on both XML directions and nested map bounds. */
  public static void main(String[] args) throws Exception {
    var limits = Schema.Limits.defaults(); var exact = new Schema.XmlMapLimits(2, 20);
    for (String argument : args) {
      byte[] data = Files.readAllBytes(Path.of(argument)); boolean valid = argument.endsWith(".valid.xml");
      Schema.Maps.Sample value;
      try { value = Schema.Maps.Sample.decodeXml(data); }
      catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(argument, expected); continue; }
      if (!valid) throw new AssertionError("Invalid map input accepted: " + argument);
      String document = new String(data, StandardCharsets.UTF_8);
      String key = document.contains("<key> </key>") ? " " : "A&B";
      if (value.integers.size() != 2 || !value.integers.get(-1).equals("max") || !value.integers.get(1).equals("one") || value.booleans.get(true) != -32768 || value.booleans.get(false) != 32767 || value.records.get(key).code != (short)-1 || !value.records.get(key).label.equals("nested") || !value.enumerated.get(Schema.Maps.Key.values()[0]).equals("first") || !value.signedValues.get((byte)-128).equals("negative")) throw new AssertionError("Map scalar widths/identities");
      if (value.optionalValues.isPresent() != document.contains("optional_values")) throw new AssertionError("Optional map presence");
      byte[] encoded = value.encodeXml();
      if (!Arrays.equals(encoded, Schema.Maps.Sample.decodeXml(encoded).encodeXml())) throw new AssertionError("Map order changed");
      Files.write(Path.of(argument + ".java.xml"), encoded);
      value.encodeXml(limits, null, exact); Schema.Maps.Sample.decodeXml(data, limits, null, exact);
      var holder = new Schema.Maps.Holder(); holder.value = value;
      byte[] nested = holder.encodeXml(limits, null, exact);
      Schema.Maps.Holder.decodeXml(nested, limits, null, exact);
      for (var bound : new Schema.XmlMapLimits[]{new Schema.XmlMapLimits(1, 20), new Schema.XmlMapLimits(2, 9), new Schema.XmlMapLimits(0, 0)}) {
        rejected(() -> value.encodeXml(limits, null, bound));
        rejected(() -> Schema.Maps.Sample.decodeXml(data, limits, null, bound));
        rejected(() -> Schema.Maps.Holder.decodeXml(nested, limits, null, bound));
      }
      rejected(() -> new Schema.XmlMapLimits(-1, 20)); rejected(() -> new Schema.XmlMapLimits(2, -1));
      var broken = Schema.Maps.Sample.decodeXml(encoded); broken.records.clear(); broken.records.put("bad", null);
      rejected(broken::encodeXml);
      var empty = Schema.Maps.Sample.decodeXml(encoded);
      empty.integers.clear(); empty.booleans.clear(); empty.records.clear(); empty.enumerated.clear(); empty.signedValues.clear();
      empty.optionalValues = java.util.Optional.of(new java.util.TreeMap<Long, String>(Long::compareUnsigned));
      byte[] emptyBytes = empty.encodeXml(limits, null, new Schema.XmlMapLimits(0, 0));
      var emptyCopy = Schema.Maps.Sample.decodeXml(emptyBytes, limits, null, new Schema.XmlMapLimits(0, 0));
      if (emptyCopy.optionalValues.isEmpty() || !emptyCopy.optionalValues.get().isEmpty()) throw new AssertionError("Empty wrappers/zero bounds");
      Files.write(Path.of(argument + ".empty.java.xml"), emptyBytes);
      byte[] lexical = document.replace("id=\"00001\"", "id=\"0000000000000000000000000000000000001\"").getBytes(StandardCharsets.UTF_8);
      if (!Arrays.equals(data, lexical)) rejected(() -> Schema.Maps.Sample.decodeXml(lexical, limits, null, exact));
      var many = Schema.Maps.Sample.decodeXml(encoded); many.integers.clear();
      for (int index = 0; index < 1025; ++index) many.integers.put(index, "x");
      rejected(many::encodeXml);
      byte[] manyBytes = many.encodeXml(limits, null, new Schema.XmlMapLimits(1025, 1 << 20));
      rejected(() -> Schema.Maps.Sample.decodeXml(manyBytes));
      Schema.Maps.Sample.decodeXml(manyBytes, limits, null, new Schema.XmlMapLimits(1025, 1 << 20));
      var large = Schema.Maps.Sample.decodeXml(encoded); large.records.clear(); large.records.put("x".repeat((1 << 20) + 1), new Schema.Maps.Child());
      rejected(large::encodeXml);
      byte[] largeBytes = large.encodeXml(limits, null, new Schema.XmlMapLimits(2, (1 << 20) + 1));
      rejected(() -> Schema.Maps.Sample.decodeXml(largeBytes));
      Schema.Maps.Sample.decodeXml(largeBytes, limits, null, new Schema.XmlMapLimits(2, (1 << 20) + 1));
    }
    System.out.println("Java typed map aliases, nested limits, optional wrappers and transactional failures pass");
  }
}
'''

CSHARP = r'''using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using static SerializerGenerated.Schema;
// Qualify typed-key maps without changing ordinary native model/storage contracts.
internal static class Program {
  // Require invalid map operations to fail before a complete result returns.
  private static void Rejected(Action operation) {
    try { operation(); } catch (FormatException) { return; } catch (ArgumentOutOfRangeException) { return; }
    throw new Exception("Invalid map candidate published");
  }
  // Compare the independent oracle in both XML directions and through nested records.
  private static void Main(string[] args) {
    var limits = new Limits(); var exact = new XmlMapLimits(2, 20);
    foreach (string path in args) {
      byte[] data = File.ReadAllBytes(path); bool valid = path.EndsWith(".valid.xml", StringComparison.Ordinal);
      MapsSample value;
      try { value = MapsSample.DecodeXml(data); }
      catch (FormatException expected) { if (valid) throw new Exception(path, expected); continue; }
      if (!valid) throw new Exception("Invalid map input accepted: " + path);
      string document = Encoding.UTF8.GetString(data); string key = document.Contains("<key> </key>", StringComparison.Ordinal) ? " " : "A&B";
      if (value.Integers.Count != 2 || value.Integers[uint.MaxValue] != "max" || value.Integers[1] != "one" || value.Booleans[true] != -32768 || value.Booleans[false] != 32767 || value.Records[key].Code != ushort.MaxValue || value.Records[key].Label != "nested" || value.Enumerated[(MapsKey)0] != "first" || value.SignedValues[-128] != "negative") throw new Exception("Map scalar widths/identities");
      if (value.OptionalValues.HasValue != document.Contains("optional_values", StringComparison.Ordinal)) throw new Exception("Optional map presence");
      byte[] encoded = value.EncodeXml();
      if (!encoded.AsSpan().SequenceEqual(MapsSample.DecodeXml(encoded).EncodeXml())) throw new Exception("Map order changed");
      File.WriteAllBytes(path + ".csharp.xml", encoded);
      value.EncodeXml(limits, null, exact); MapsSample.DecodeXml(data, limits, null, exact);
      var holder = new MapsHolder { Value = value }; byte[] nested = holder.EncodeXml(limits, null, exact);
      MapsHolder.DecodeXml(nested, limits, null, exact);
      foreach (var bound in new[] {new XmlMapLimits(1, 20), new XmlMapLimits(2, 9), new XmlMapLimits(0, 0)}) {
        Rejected(() => value.EncodeXml(limits, null, bound));
        Rejected(() => MapsSample.DecodeXml(data, limits, null, bound));
        Rejected(() => MapsHolder.DecodeXml(nested, limits, null, bound));
      }
      Rejected(() => new XmlMapLimits(-1, 20)); Rejected(() => new XmlMapLimits(2, -1));
      var broken = MapsSample.DecodeXml(encoded); broken.Records.Clear(); broken.Records.Add("bad", null!);
      Rejected(() => broken.EncodeXml());
      var badEnum = MapsSample.DecodeXml(encoded); badEnum.Enumerated.Clear(); badEnum.Enumerated.Add((MapsKey)9, "bad");
      Rejected(() => badEnum.EncodeXml());
      var empty = MapsSample.DecodeXml(encoded);
      empty.Integers.Clear(); empty.Booleans.Clear(); empty.Records.Clear(); empty.Enumerated.Clear(); empty.SignedValues.Clear();
      empty.OptionalValues = new ValueTuple<Dictionary<ulong, string>>(new Dictionary<ulong, string>());
      byte[] emptyBytes = empty.EncodeXml(limits, null, new XmlMapLimits(0, 0));
      var emptyCopy = MapsSample.DecodeXml(emptyBytes, limits, null, new XmlMapLimits(0, 0));
      if (!emptyCopy.OptionalValues.HasValue || emptyCopy.OptionalValues.Value.Item1.Count != 0) throw new Exception("Empty wrappers/zero bounds");
      File.WriteAllBytes(path + ".empty.csharp.xml", emptyBytes);
      byte[] lexical = Encoding.UTF8.GetBytes(document.Replace("id=\"00001\"", "id=\"0000000000000000000000000000000000001\"", StringComparison.Ordinal));
      if (!data.AsSpan().SequenceEqual(lexical)) Rejected(() => MapsSample.DecodeXml(lexical, limits, null, exact));
      var many = MapsSample.DecodeXml(encoded); many.Integers.Clear();
      for (uint index = 0; index < 1025; ++index) many.Integers.Add(index, "x");
      Rejected(() => many.EncodeXml()); byte[] manyBytes = many.EncodeXml(limits, null, new XmlMapLimits(1025, 1 << 20));
      Rejected(() => MapsSample.DecodeXml(manyBytes)); MapsSample.DecodeXml(manyBytes, limits, null, new XmlMapLimits(1025, 1 << 20));
      var large = MapsSample.DecodeXml(encoded); large.Records.Clear(); large.Records.Add(new string('x', (1 << 20) + 1), new MapsChild());
      Rejected(() => large.EncodeXml()); byte[] largeBytes = large.EncodeXml(limits, null, new XmlMapLimits(2, (1 << 20) + 1));
      Rejected(() => MapsSample.DecodeXml(largeBytes)); MapsSample.DecodeXml(largeBytes, limits, null, new XmlMapLimits(2, (1 << 20) + 1));
    }
    Console.WriteLine("C# typed map aliases, nested limits, optional wrappers and transactional failures pass");
  }
}
'''


def main():
    """Run the maintained SDK compilers and validate both ordinary and empty emitted wrappers."""
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
    args.build.mkdir(parents=True, exist_ok=True)
    oracle = etree.XMLSchema(etree.fromstring(fixture.xsd().encode())); paths = []
    (args.build / 'maps.xsd').write_text(fixture.xsd(), encoding='utf-8')
    for index, (document, valid) in enumerate(fixture.cases()):
        if oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent XSD decision differs for {index}: {oracle.error_log}')
        path = args.build / f'{index}.{"valid" if valid else "invalid"}.xml'; path.write_text(document, encoding='utf-8'); paths.append(path)
    model = args.build / 'model.serializer'; model.write_text(fixture.MODEL, encoding='utf-8')
    config = args.build / 'maps.ini'; config.write_text(fixture.CONFIG, encoding='utf-8')
    command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages), '--protocols', 'binary_none']
    for language in languages:
        target = args.build / language; target.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        (source / ('Main.java' if language == 'java' else 'Program.cs')).write_text(JAVA if language == 'java' else CSHARP, encoding='utf-8')
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
            Path(str(path) + f'.empty.{language}.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_maps', args.build / language, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(emitted)))
                oracle.assertValid(etree.parse(str(path) + f'.empty.{language}.xml'))
            elif emitted.exists():
                raise AssertionError(f'Invalid map output published: {emitted}')
    print(f'PASS: {len(paths)} independent map probes and explicit resource bounds across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
