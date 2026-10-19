"""Qualify Java/C# flattened XML over native private/protected multiple-base composition."""
import argparse
import importlib.util
from pathlib import Path
import subprocess

from lxml import etree


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_model_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
NS = 'urn:serializer:inherited'
MODEL = '''serializer version 1.20.0;
namespace inherited {
  xml(namespace("urn:serializer:inherited"));
  class first {
    private uint32 a;
    protected optional<optional<string>> states;
    public uint16 id xml(attribute);
    private array[2] uint8 fixed_values;
  }
  class second {
    private map(uint16) string mapping;
    protected variant(uint8=number,string=text) choice;
  }
  class middle : protected first { private bool c; }
  class record : private middle, protected second { public int16 d; }
}
'''


def options():
    """Apply complete field mappings to each statically flattened owning contract."""
    result = '[xml]\nenabled=true\n'
    for name in ['first', 'second', 'middle', 'record']:
        result += '[xml.class.inherited::' + name + ']\n'
        if name in ['first', 'middle', 'record']:
            result += 'presence.states=absent_or_nil\n'
        if name in ['second', 'record']:
            result += 'wrapper.mapping=mapping\nmap.mapping=entry\nmap.mapping.key=id\nmap.mapping.key_placement=attribute\nmap.mapping.value=data\nwrapper.choice=choice\nvariant.choice=element\n'
    return result


XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:inherited" targetNamespace="urn:serializer:inherited" elementFormDefault="qualified">
<xs:element name="record"><xs:complexType><xs:sequence>
<xs:element name="a" type="xs:unsignedInt"/>
<xs:element name="states" type="xs:string" minOccurs="0" nillable="true"/>
<xs:element name="fixed_values" type="xs:unsignedByte" minOccurs="2" maxOccurs="2"/>
<xs:element name="c" type="xs:boolean"/>
<xs:element name="mapping"><xs:complexType><xs:sequence><xs:element name="entry" minOccurs="0" maxOccurs="unbounded"><xs:complexType><xs:sequence><xs:element name="data" type="xs:string"/></xs:sequence><xs:attribute name="id" type="xs:unsignedShort" use="required"/></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element>
<xs:element name="choice"><xs:complexType><xs:choice><xs:element name="number" type="xs:unsignedByte"/><xs:element name="text" type="xs:string"/></xs:choice></xs:complexType></xs:element>
<xs:element name="d" type="xs:short"/>
</xs:sequence><xs:attribute name="id" type="xs:unsignedShort" use="required"/></xs:complexType>
<xs:key name="mapKeys"><xs:selector xpath="t:mapping/t:entry"/><xs:field xpath="@id"/></xs:key>
</xs:element></xs:schema>'''


def cases():
    """Exercise nonpublic parent state together with exact declaration order and container validation."""
    states = ['', '<states xsi:nil="true"/>', '<states/>', '<states>hello</states>', '<states xsi:nil="false"/>']
    choices = ['<number>255</number>', '<text>hi🙂</text>']
    start = '<record xmlns="' + NS + '" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" id="65535"><a>4294967295</a>'
    tail = '<fixed_values>255</fixed_values><fixed_values>0</fixed_values><c>true</c><mapping><entry id="1"><data>one</data></entry><entry id="65535"><data>max</data></entry></mapping><choice>'
    documents = [(state, choice, start + value + tail + body + '</choice><d>-32768</d></record>', True)
                 for state, value in enumerate(states) for choice, body in enumerate(choices)]
    base = documents[6][2]
    for before, after in [
        ('<a>4294967295</a>', '<a>4294967296</a>'), ('id="65535"><a>', 'id="65536"><a>'),
        ('<a>4294967295</a>', ''), ('<states>hello</states>', '<states xsi:nil="true"> </states>'),
        ('<states>hello</states>', '<states/><states/>'), ('<states>hello</states>', '<states extra="1"/>'),
        ('<fixed_values>255</fixed_values>', ''), ('<fixed_values>0</fixed_values>', '<fixed_values>0</fixed_values><fixed_values>1</fixed_values>'),
        ('<fixed_values>255</fixed_values>', '<fixed_values>256</fixed_values>'), ('<c>true</c>', ''),
        ('<c>true</c>', '<c>yes</c>'), ('<c>true</c>', '<c>true</c><c>false</c>'),
        ('id="65535"><data>max', 'id="0001"><data>max'), ('<data>one</data>', ''),
        ('<number>255</number>', '<number>256</number>'), ('<number>255</number>', '<text>hi</text><number>255</number>'),
        ('<number>255</number>', ''), ('<d>-32768</d>', '<d>-32769</d>'),
        ('</record>', '<a>1</a></record>'), ('<c>true</c>', '<d>-32768</d><c>true</c>'),
    ]:
        documents.append((3, 0, base.replace(before, after), False))
    return documents


JAVA = r'''import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.Map;
import java.util.Optional;
/** Check native private base ownership independently of generated XML helper names. */
public final class Main {
  /** Access the existing native schema storage without changing its declared visibility. */
  private static Field field(Object owner, String name) {
    for (Field field : owner.getClass().getDeclaredFields()) if (field.getName().equalsIgnoreCase(name)) { field.setAccessible(true); return field; }
    throw new AssertionError("Missing native field " + name);
  }
  /** Return one ordinary native schema member. */
  private static Object get(Object owner, String name) throws Exception { return field(owner, name).get(owner); }
  /** Require invalid composed values to fail before complete XML publication. */
  private static void rejected(Runnable operation) { try { operation.run(); } catch (IllegalArgumentException expected) { return; } throw new AssertionError("Invalid composed candidate published"); }
  /** Compare exact flattened order, native optional/variant storage and ordinary wire bytes. */
  public static void main(String[] args) throws Exception {
    for (String path : args) {
      byte[] data = Files.readAllBytes(Path.of(path)); boolean valid = path.endsWith(".valid.xml"); Schema.Inherited.Record value;
      try { value = Schema.Inherited.Record.decodeXml(data); }
      catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(path, expected); continue; }
      if (!valid) throw new AssertionError("Invalid inherited input accepted: " + path);
      String file = Path.of(path).getFileName().toString(); int state = file.charAt(1) - '0', choice = file.charAt(3) - '0';
      Object middle = get(value, "base0"), first = get(middle, "base0"), second = get(value, "base1");
      if (field(first, "a").getModifiers() != java.lang.reflect.Modifier.PRIVATE || (int)get(first, "a") != -1 || (short)get(first, "id") != -1 || !(boolean)get(middle, "c") || value.d != -32768) throw new AssertionError("Native scalar storage or visibility changed");
      byte[] fixed = (byte[])get(first, "fixedValues"); if (fixed.length != 2 || fixed[0] != -1 || fixed[1] != 0) throw new AssertionError("Private fixed array storage");
      Optional<?> outer = (Optional<?>)get(first, "states"); if (outer.isPresent() != (state != 0)) throw new AssertionError("Absent state changed");
      if (state != 0) { Optional<?> inner = (Optional<?>)outer.get(); if (inner.isPresent() != (state != 1) || (state != 1 && !inner.get().equals(state == 3 ? "hello" : ""))) throw new AssertionError("Private nil/value state changed"); }
      Map<?, ?> mapping = (Map<?, ?>)get(second, "mapping"); if (mapping.size() != 2 || !mapping.get((short)-1).equals("max") || (int)get(second, "choiceIndex") != choice || (choice == 0 && (byte)get(second, "choiceNumber") != -1) || (choice == 1 && !get(second, "choiceText").equals("hi🙂"))) throw new AssertionError("Private container storage");
      byte[] encoded = value.encodeXml(); var copy = Schema.Inherited.Record.decodeXml(encoded);
      if (!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE), copy.encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Native base wire layout changed");
      Files.write(Path.of(path + ".java.xml"), encoded);
      var defaults = Schema.Limits.defaults(); var limited = new Schema.Limits(defaults.maxBytes, defaults.maxStringBytes, 1, defaults.maxDepth);
      rejected(() -> value.encodeXml(limited, null)); rejected(() -> Schema.Inherited.Record.decodeXml(data, limited, null));
      rejected(() -> value.encodeXml(defaults, null, new Schema.XmlMapLimits(1, 20)));
      var broken = Schema.Inherited.Record.decodeXml(encoded); field(get(broken, "base0"), "base0").set(get(broken, "base0"), null); rejected(broken::encodeXml);
      field(broken, "base0").set(broken, null); rejected(broken::encodeXml);
    }
    System.out.println("Java private/protected multiple-base XML, owning storage, order and staged failures pass");
  }
}
'''

CSHARP = r'''using System;
using System.Collections;
using System.IO;
using System.Reflection;
using static SerializerGenerated.Schema;
// Check native private base ownership independently of generated XML helper names.
internal static class Program {
  // Access the existing native schema storage without changing its declared visibility.
  private static FieldInfo Field(object owner, string name) => owner.GetType().GetField(name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.IgnoreCase) ?? throw new Exception("Missing native field " + name);
  // Return one ordinary native schema member.
  private static object? Get(object owner, string name) => Field(owner, name).GetValue(owner);
  // Read an optional tuple's boxed underlying payload without losing nil/absence distinctions.
  private static object? Item(object value) => value.GetType().GetField("Item1")!.GetValue(value);
  // Require invalid composed values to fail before complete XML publication.
  private static void Rejected(Action operation) { try { operation(); } catch (FormatException) { return; } throw new Exception("Invalid composed candidate published"); }
  // Compare exact flattened order, native optional/variant storage and ordinary wire bytes.
  private static void Main(string[] args) {
    foreach (string path in args) {
      byte[] data = File.ReadAllBytes(path); bool valid = path.EndsWith(".valid.xml", StringComparison.Ordinal); InheritedRecord value;
      try { value = InheritedRecord.DecodeXml(data); }
      catch (FormatException expected) { if (valid) throw new Exception(path, expected); continue; }
      if (!valid) throw new Exception("Invalid inherited input accepted: " + path);
      string file = Path.GetFileName(path); int state = file[1] - '0', choice = file[3] - '0';
      object middle = Get(value, "base0")!, first = Get(middle, "base0")!, second = Get(value, "base1")!;
      if (!Field(first, "a").IsPrivate || (uint)Get(first, "a")! != uint.MaxValue || (ushort)Get(first, "id")! != ushort.MaxValue || !(bool)Get(middle, "c")! || value.D != short.MinValue) throw new Exception("Native scalar storage or visibility changed");
      var fixedValues = (IList)Get(first, "fixedValues")!; if (fixedValues.Count != 2 || (byte)fixedValues[0]! != 255 || (byte)fixedValues[1]! != 0) throw new Exception("Private fixed array storage");
      object? outer = Get(first, "states"); if ((outer != null) != (state != 0)) throw new Exception("Absent state changed");
      if (state != 0) { object? inner = Item(outer!); if ((inner != null) != (state != 1) || (state != 1 && (string)Item(inner!)! != (state == 3 ? "hello" : ""))) throw new Exception("Private nil/value state changed"); }
      var mapping = (IDictionary)Get(second, "mapping")!;
      if (mapping.Count != 2 || (string)mapping[ushort.MaxValue]! != "max" || (int)Get(second, "choiceIndex")! != choice || (choice == 0 && (byte)Get(second, "choiceNumber")! != 255) || (choice == 1 && (string)Get(second, "choiceText")! != "hi🙂")) throw new Exception("Private container storage");
      byte[] encoded = value.EncodeXml(); var copy = InheritedRecord.DecodeXml(encoded);
      if (!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(copy.Encode(Protocol.BINARY_NONE))) throw new Exception("Native base wire layout changed");
      File.WriteAllBytes(path + ".csharp.xml", encoded);
      var limited = new Limits(maxElements: 1); Rejected(() => value.EncodeXml(limited)); Rejected(() => InheritedRecord.DecodeXml(data, limited));
      Rejected(() => value.EncodeXml(new Limits(), null, new XmlMapLimits(1, 20)));
      var broken = InheritedRecord.DecodeXml(encoded); object parent = Get(broken, "base0")!; Field(parent, "base0").SetValue(parent, null); Rejected(() => broken.EncodeXml());
      Field(broken, "base0").SetValue(broken, null); Rejected(() => broken.EncodeXml());
    }
    Console.WriteLine("C# private/protected multiple-base XML, owning storage, order and staged failures pass");
  }
}
'''


def rejected_shapes(args, languages):
    """Reject repeated/ambiguous bases and helper collisions before replacing existing output."""
    directory = args.build / 'rejected-shapes'; directory.mkdir(parents=True, exist_ok=True)
    models = [
        ('class first { public uint32 same; } class second { public string same; } class record : public first, public second {}', 'ambiguous'),
        ('class first {} class middle : public first {} class record : public first, public middle {}', 'repeated'),
    ]
    for index, (body, diagnostic) in enumerate(models):
        model = directory / 'model.serializer'; model.write_text('serializer version 1.20.0; ' + body, encoding='utf-8')
        command = [str(args.compiler), '--input', str(model), '--language', ','.join(languages), '--xml.enabled', 'true']
        outputs = []
        for language in languages:
            output = directory / sdk.OUTPUTS[language]; output.write_text('preserve existing output', encoding='utf-8'); outputs.append(output)
            command += [f'--{language}.output', str(output)]
        result = subprocess.run(command, capture_output=True, encoding='utf-8', timeout=60)
        if result.returncode == 0 or diagnostic not in result.stdout + result.stderr or any(output.read_text(encoding='utf-8') != 'preserve existing output' for output in outputs):
            raise AssertionError(f'Inherited rejection {index} failed: {result.stdout} {result.stderr}')
    if 'csharp' in languages:
        model = directory / 'model.serializer'; model.write_text('serializer version 1.20.0; class first { private uint32 a; public string srl_xml_field0; } class record : private first {}', encoding='utf-8')
        output = directory / 'collision.cs'; output.write_text('preserve existing output', encoding='utf-8')
        result = subprocess.run([str(args.compiler), '--input', str(model), '--language', 'csharp', '--csharp.output', str(output), '--xml.enabled', 'true'], capture_output=True, encoding='utf-8', timeout=60)
        if result.returncode == 0 or 'helper collides' not in result.stderr or output.read_text(encoding='utf-8') != 'preserve existing output':
            raise AssertionError('XML-only inherited helper collision was not rejected atomically')
        result = subprocess.run([str(args.compiler), '--input', str(model), '--language', 'csharp', '--csharp.output', str(output), '--xml.enabled', 'false'], capture_output=True, encoding='utf-8', timeout=60)
        if result.returncode != 0:
            raise AssertionError('XML-only helper collision changed ordinary native generation')


def main():
    """Generate actual native SDKs and independently validate every flattened XML output."""
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
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode())); paths = []
    (args.build / 'model.xsd').write_text(XSD, encoding='utf-8')
    for index, (state, choice, document, valid) in enumerate(cases()):
        if oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent XSD decision differs for {index}: {oracle.error_log}')
        path = args.build / f's{state}v{choice}-{index}.{"valid" if valid else "invalid"}.xml'; path.write_text(document, encoding='utf-8'); paths.append(path)
    model = args.build / 'model.serializer'; model.write_text(MODEL, encoding='utf-8')
    config = args.build / 'model.ini'; config.write_text(options(), encoding='utf-8')
    command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages), '--protocols', 'binary_none']
    for language in languages:
        directory = args.build / language; directory.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', directory / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    sources = {'java': ('Main.java', JAVA), 'csharp': ('Program.cs', CSHARP)}
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        filename, content = sources[language]; (source / filename).write_text(content, encoding='utf-8')
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_inherited', args.build / language, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(emitted)))
                names = [etree.QName(child).localname for child in etree.parse(str(emitted)).getroot()]
                expected = ['a'] + (['states'] if path.name[1] != '0' else []) + ['fixed_values', 'fixed_values', 'c', 'mapping', 'choice', 'd']
                if names != expected:
                    raise AssertionError(f'Base declaration order changed: {names}')
            elif emitted.exists():
                raise AssertionError(f'Invalid inherited output published: {emitted}')
    rejected_shapes(args, languages)
    print(f'PASS: {len(paths)} independent inherited-model probes across {len(languages)} SDKs, private/protected storage, helper collisions and ordinary wire compatibility', flush=True)


if __name__ == '__main__':
    main()
