"""Qualify direct Go/Java/C# absent, nil, empty and present states with an independent XSD."""
import argparse
import importlib.util
import json
from itertools import product
from pathlib import Path
import subprocess

from lxml import etree

from kotlin_xml_presence_test import MODEL, OPTIONS, XSD, NS


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_presence_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

BOUND_XSD = XSD.replace('<xs:element name="sample"', '<xs:element name="child" type="t:childType"/><xs:element name="sample"')

RICH_MODEL = '''namespace nil_shapes {
  xml(namespace("urn:serializer:nil:shapes"));
  enum state { zero, one }
  class child { public uint8 code; }
  class sample {
    public optional<array<child>> records;
    public optional<array<state>> enumerated;
    public array[2] uint8 fixed_values;
    public optional<uint16> number;
  }
}
'''
RICH_OPTIONS = '''[xml.class.nil_shapes::sample]
presence.records = absent
wrapper.records = records
name.records = record
presence.enumerated = absent
wrapper.enumerated = enumeration
name.enumerated = state
presence.fixed_values = required
wrapper.fixed_values = fixed_values
name.fixed_values = item
presence.number = nil
'''
RICH_XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:nil:shapes" targetNamespace="urn:serializer:nil:shapes" elementFormDefault="qualified">
<xs:complexType name="childType"><xs:sequence><xs:element name="code" type="xs:unsignedByte"/></xs:sequence></xs:complexType>
<xs:simpleType name="stateType"><xs:restriction base="xs:string"><xs:enumeration value="zero"/><xs:enumeration value="one"/></xs:restriction></xs:simpleType>
<xs:element name="child" type="t:childType"/>
<xs:element name="sample"><xs:complexType><xs:sequence>
<xs:element name="records" minOccurs="0"><xs:complexType><xs:sequence><xs:element name="record" type="t:childType" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element>
<xs:element name="enumeration" minOccurs="0"><xs:complexType><xs:sequence><xs:element name="state" type="t:stateType" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element>
<xs:element name="fixed_values"><xs:complexType><xs:sequence><xs:element name="item" type="xs:unsignedByte" minOccurs="2" maxOccurs="2"/></xs:sequence></xs:complexType></xs:element>
<xs:element name="number" type="xs:unsignedShort" nillable="true"/>
</xs:sequence></xs:complexType></xs:element></xs:schema>'''

ALL_MODEL = '''namespace nil_all {
  using schema "presence-all.xsd";
  class sample { public optional<optional<string>> states; public optional<uint16> number; }
}
'''
ALL_OPTIONS = '''[xml.class.nil_all::sample]
presence.states = absent_or_nil
presence.number = absent
'''
ALL_XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" targetNamespace="urn:serializer:nil:all" elementFormDefault="qualified">
<xs:element name="sample"><xs:complexType><xs:all>
<xs:element name="states" minOccurs="0" nillable="true"><xs:simpleType><xs:restriction base="xs:string"><xs:whiteSpace value="collapse"/><xs:length value="2"/><xs:pattern value="[A-Z]{2}"/></xs:restriction></xs:simpleType></xs:element>
<xs:element name="number" minOccurs="0"><xs:simpleType><xs:restriction base="xs:unsignedShort"><xs:minInclusive value="1"/><xs:maxInclusive value="3"/></xs:restriction></xs:simpleType></xs:element>
</xs:all></xs:complexType></xs:element></xs:schema>'''


def all_cases():
    """Combine native nil states with unordered dispatch, exact scalar facets and portable patterns."""
    start = '<sample xmlns="urn:serializer:nil:all" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">'
    states = ['', '<states xsi:nil="true"/>', '<states>AB</states>', '<states> CD </states>']
    numbers = ['', '<number>1</number>', '<number>3</number>']
    documents = [('all', start + ''.join(parts) + '</sample>', True)
                 for pair in product(states, numbers) for parts in (pair, pair[::-1])]
    base = start + '<states>AB</states><number>1</number></sample>'
    documents += [('all', base.replace('<states>', '<states xsi:nil="false">'), True)]
    for before, after in [
        ('<states>AB</states>', '<states/>'), ('AB', 'A'), ('AB', 'ABC'), ('AB', 'Ab'),
        ('<number>1</number>', '<number>0</number>'), ('<number>1</number>', '<number>4</number>'),
        ('<number>1</number>', '<number>65536</number>'), ('<number>1</number>', '<number xsi:nil="true"/>'),
        ('<states>AB</states>', '<states xsi:nil="true"> </states>'),
        ('<states>AB</states>', '<states xsi:nil="true"/><states>AB</states>'),
        ('<number>1</number>', '<number>1</number><number>2</number>'),
        ('</sample>', '<unknown/></sample>'), ('<states>', '<states extra="1">'),
    ]:
        documents.append(('all', base.replace(before, after), False))
    return documents


def binding_rejections(args, languages):
    """Reject contradictory fixed-XSD contracts atomically before replacing any backend output."""
    directory = args.build / 'binding-rejections'; directory.mkdir(parents=True, exist_ok=True)
    model = 'serializer version 1.20.0; namespace binding { using schema "binding.xsd"; class sample { public optional<uint16> value; } }'
    schema = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="sample"><xs:complexType><xs:sequence><xs:element name="value" type="xs:unsignedShort" nillable="true"/></xs:sequence></xs:complexType></xs:element></xs:schema>'
    options = '[xml]\nenabled=true\n[xml.class.binding::sample]\npresence.value=nil\n'
    probes = [
        (model, schema.replace('nillable="true"', 'nillable="false"'), options, 'nillability'),
        (model, schema, options.replace('=nil', '=absent'), 'nillability'),
        (model, schema.replace('nillable="true"', 'nillable="true" minOccurs="0"'), options, 'presence/cardinality'),
        (model, schema.replace('nillable="true"', 'nillable="true" maxOccurs="2"'), options, 'presence/cardinality'),
        (model, schema.replace('nillable="true"', 'nillable="yes"'), options, 'boolean'),
        (model, schema.replace('nillable="true"', 'nillable="true" fixed="1"'), options, 'fixed'),
        (model, schema.replace('name="sample"', 'name="sample" nillable="true"'), options, 'document-root'),
        (model.replace('optional<uint16>', 'uint16'), schema, options, 'optional storage depth'),
    ]
    wrapped_model = model.replace('optional<uint16>', 'optional<array<uint16>>')
    wrapped_schema = '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="sample"><xs:complexType><xs:sequence><xs:element name="value" minOccurs="0"><xs:complexType><xs:sequence><xs:element name="item" type="xs:unsignedShort" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element></xs:schema>'
    wrapped_options = options.replace('=nil', '=absent')
    probes.extend([
        (wrapped_model, wrapped_schema.replace('name="value" minOccurs="0"', 'name="value" minOccurs="1"'), wrapped_options, 'mandatory singleton sequence'),
        (wrapped_model, wrapped_schema.replace('name="value"', 'name="value" nillable="true"'), wrapped_options, 'wrapper nillability'),
        (wrapped_model, wrapped_schema.replace('name="item"', 'name="item" nillable="true"'), wrapped_options, 'nillability'),
        (wrapped_model, wrapped_schema.replace('name="value"', 'name="value" fixed="1"'), wrapped_options, 'XSD'),
        (model.replace('optional<uint16> value', 'array[2] uint16 value'), wrapped_schema.replace('minOccurs="0"', 'minOccurs="1"').replace('maxOccurs="unbounded"', 'maxOccurs="3"'), options.replace('=nil', '=required'), 'fixed array extent'),
    ])
    outputs = [directory / sdk.OUTPUTS[language] for language in languages]
    sentinel = b'Existing generated output must survive rejected fixed-XSD contracts.\n'
    reports = []
    for index, (selected_model, selected_schema, selected_options, diagnostic) in enumerate(probes):
        (directory / 'model.serializer').write_text(selected_model, encoding='utf-8')
        (directory / 'binding.xsd').write_text(selected_schema, encoding='utf-8')
        (directory / 'options.ini').write_text(selected_options, encoding='utf-8')
        command = [str(args.compiler), '--input', str(directory / 'model.serializer'), '--config', str(directory / 'options.ini'), '--language', ','.join(languages), '--protocols', 'binary_none', '--go.package', 'main']
        for language, output in zip(languages, outputs):
            output.write_bytes(sentinel); command += [f'--{language}.output', str(output)]
        result = subprocess.run(command, capture_output=True, encoding='utf-8', timeout=60)
        if result.returncode == 0 or diagnostic not in result.stderr:
            raise AssertionError(f'Unexpected binder rejection {index}: {result.stdout} {result.stderr}')
        if any(output.read_bytes() != sentinel for output in outputs):
            raise AssertionError('Rejected optimized XSD contract replaced existing backend output')
        reports.append({'case': index, 'diagnostic': result.stderr.strip()})
    (directory / 'results.json').write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')
    return len(probes)


def rich_cases():
    """Qualify record/enum collection states and required fixed arrays independently of the base fixture."""
    start = '<sample xmlns="urn:serializer:nil:shapes" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">'
    fixed = '<fixed_values><item>255</item><item>0</item></fixed_values>'
    records = ['', '<records/>', '<records><record><code>255</code></record></records>']
    enumerated = ['', '<enumeration/>', '<enumeration><state>zero</state><state>one</state></enumeration>']
    numbers = ['<number xsi:nil="true"/>', '<number>65535</number>']
    documents = [('rich', start + record + state + fixed + number + '</sample>', True)
                 for record, state, number in product(records, enumerated, numbers)]
    base = documents[-1][1]
    for before, after in [
        ('<code>255</code>', '<code>256</code>'), ('<record>', '<record xsi:nil="true">'),
        ('<record><code>255</code></record>', '<record/>'), ('<state>zero</state>', '<state>other</state>'),
        ('<records>', '<records xsi:nil="true">'), ('<enumeration>', '<enumeration xsi:nil="true">'),
        ('<item>255</item>', ''), ('<item>0</item>', '<item>0</item><item>1</item>'),
        (fixed, ''), ('<fixed_values>', '<fixed_values xsi:nil="true">'),
        ('<number>65535</number>', '<number>65536</number>'), ('<number>65535</number>', ''),
        ('<number>65535</number>', '<number xsi:nil="true">0</number>'),
    ]:
        documents.append(('rich', base.replace(before, after), False))
    return documents


def cases():
    """Share a Cartesian state corpus and independently specified invalid-content boundaries."""
    start = '<sample xmlns="' + NS + '" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">'
    ending = '<nested xsi:nil="true"/></sample>'
    values = ['<nil_value xsi:nil="true"/>', '<nil_value/>', '<nil_value>A&amp;B&#13;🙂</nil_value>']
    states = ['', '<states xsi:nil="true"/>', '<states/>', '<states>A&amp;B&#13;🙂</states>']
    entries = ['', '<entries xsi:nil="true"/>', '<entries/>', '<entries><item/><item>A&amp;B</item></entries>']
    documents = [('sample', start + value + state + entry + ending, True)
                 for value, state, entry in product(values, states, entries)]
    base = start + '<nil_value xsi:nil="true"/>' + ending
    documents += [('sample', base.replace('xsi:nil="true"', 'xsi:nil="1"'), True),
                  ('sample', base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="false"/>'), True),
                  ('sample', base.replace('<nested xsi:nil="true"/>', '<nested xsi:nil="0"><value>child</value></nested>'), True),
                  ('sample', base.replace('xsi:nil', 'n:nil').replace('xmlns:xsi=', 'xmlns:n='), True)]
    invalid = [
        base.replace('<nil_value xsi:nil="true"/>', ''),
        base.replace('xsi:nil="true"', 'xsi:nil="yes"', 1),
        base.replace('xsi:nil="true"', 'xsi:nil=""', 1),
        base.replace('xsi:nil="true"', 'xsi:nil="TRUE"', 1),
        base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="true"> </nil_value>'),
        base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="1"><value/></nil_value>'),
        base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="true" extra="1"/>'),
        base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="false" extra="1"/>'),
        base.replace('<nil_value xsi:nil="true"/>', '<nil_value xsi:nil="false"><nested/></nil_value>'),
        base.replace('<nil_value xsi:nil="true"/>', '<nil_value/><nil_value/>'),
        base.replace('<nested xsi:nil="true"/>', '<states/><states/><nested xsi:nil="true"/>'),
        base.replace('<nested xsi:nil="true"/>', '<entries xsi:nil="true"><item/></entries><nested xsi:nil="true"/>'),
        base.replace('<nested xsi:nil="true"/>', '<entries xsi:nil="true"> </entries><nested xsi:nil="true"/>'),
        base.replace('<nested xsi:nil="true"/>', '<entries extra="1"/><nested xsi:nil="true"/>'),
        base.replace('<nested xsi:nil="true"/>', '<entries><other/></entries><nested xsi:nil="true"/>'),
        base.replace('<nested xsi:nil="true"/>', '<entries><item xsi:nil="true"/></entries><nested xsi:nil="true"/>'),
        base.replace('<nested xsi:nil="true"/>', '<entries/><entries/><nested xsi:nil="true"/>'),
        base.replace('<nested xsi:nil="true"/>', '<nested/>'),
        base.replace('<nested xsi:nil="true"/>', '<nested xsi:nil="false"/>'),
        base.replace('<nested xsi:nil="true"/>', '<nested xsi:nil="true"><value>child</value></nested>'),
        base.replace('<nested xsi:nil="true"/>', '<nested xsi:nil="1" extra="1"/>'),
        base.replace('<nested xsi:nil="true"/>', ''),
        base.replace('</sample>', '<states/></sample>'),
        base.replace('<nil_value xsi:nil="true"/>', '<entries/><nil_value xsi:nil="true"/>'),
        base.replace('<nil_value xsi:nil="true"/>', '<nil_value xmlns:n="urn:wrong" n:nil="true"/>'),
    ]
    documents.extend(('sample', value, False) for value in invalid)
    holder = '<holder xmlns="' + NS + '" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><value><nil_value xsi:nil="false"/><states xsi:nil="1"/><entries/><nested xsi:nil="true"/></value></holder>'
    documents += [('holder', holder, True), ('holder', holder.replace('<entries/>', '<entries xsi:nil="true"><item/></entries>'), False)]
    return documents


JAVA = r'''import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
/** Preserve native Optional layers independently of empty lexical or collection values. */
public final class Main {
  /** Reject invalid native storage and complete-document resource policies. */
  private static void rejected(Runnable operation) {
    try { operation.run(); } catch (IllegalArgumentException expected) { return; }
    throw new AssertionError("Invalid presence candidate published");
  }
  /** Assert absent, nil and present-empty optional states before corpus execution. */
  private static void states() {
    String start = "<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>", end = "<nested xsi:nil='true'/></sample>";
    var absent = Schema.NilFixture.Sample.decodeXml((start + "<nil_value xsi:nil='true'/>" + end).getBytes(StandardCharsets.UTF_8));
    var nil = Schema.NilFixture.Sample.decodeXml((start + "<nil_value/><states xsi:nil='true'/><entries xsi:nil='true'/>" + end).getBytes(StandardCharsets.UTF_8));
    var empty = Schema.NilFixture.Sample.decodeXml((start + "<nil_value/><states/><entries/>" + end).getBytes(StandardCharsets.UTF_8));
    if (absent.states.isPresent() || absent.entries.isPresent() || absent.nilValue.isPresent() || nil.states.isEmpty() || nil.states.get().isPresent() || nil.entries.isEmpty() || nil.entries.get().isPresent() || empty.states.isEmpty() || !empty.states.get().orElseThrow().isEmpty() || empty.entries.isEmpty() || !empty.entries.get().orElseThrow().isEmpty() || nil.nilValue.isEmpty() || !nil.nilValue.get().isEmpty()) throw new AssertionError("Absent/nil/empty states merged");
    empty.states = null; rejected(empty::encodeXml);
  }
  /** Compare the independent oracle and ordinary wire payloads through both XML directions. */
  public static void main(String[] args) throws Exception {
    states();
    for (String path : args) {
      byte[] data = Files.readAllBytes(Path.of(path)); boolean valid = path.endsWith(".valid.xml"), holder = Path.of(path).getFileName().toString().startsWith("holder"); byte[] encoded;
      if (Path.of(path).getFileName().toString().startsWith("rich")) {
        Schema.NilShapes.Sample value;
        try { value = Schema.NilShapes.Sample.decodeXml(data); }
        catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(path, expected); continue; }
        if (!valid) throw new AssertionError("Invalid rich presence accepted: " + path);
        encoded = value.encodeXml();
        if (!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE), Schema.NilShapes.Sample.decodeXml(encoded).encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Record/enum collection payload changed");
        var broken = Schema.NilShapes.Sample.decodeXml(encoded); broken.fixedValues = new byte[1]; rejected(broken::encodeXml);
        Files.write(Path.of(path + ".java.xml"), encoded); continue;
      }
      if (Path.of(path).getFileName().toString().startsWith("all")) {
        Schema.NilAll.Sample value;
        try { value = Schema.NilAll.Sample.decodeXml(data); }
        catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(path, expected); continue; }
        if (!valid) throw new AssertionError("Invalid unordered presence accepted: " + path);
        encoded = value.encodeXml();
        if (!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE), Schema.NilAll.Sample.decodeXml(encoded).encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Unordered presence payload changed");
        var broken = Schema.NilAll.Sample.decodeXml(encoded); broken.states = java.util.Optional.of(java.util.Optional.of("bad")); rejected(broken::encodeXml);
        broken.states = java.util.Optional.empty(); broken.number = java.util.Optional.of((short)4); rejected(broken::encodeXml);
        Files.write(Path.of(path + ".java.xml"), encoded); continue;
      }
      try {
        if (holder) {
          var value = Schema.NilFixture.Holder.decodeXml(data); encoded = value.encodeXml();
          if (!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE), Schema.NilFixture.Holder.decodeXml(encoded).encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Nested wire payload changed");
        } else {
          var value = Schema.NilFixture.Sample.decodeXml(data); encoded = value.encodeXml();
          if (!Arrays.equals(value.encode(Schema.Protocol.BINARY_NONE), Schema.NilFixture.Sample.decodeXml(encoded).encode(Schema.Protocol.BINARY_NONE))) throw new AssertionError("Wire payload changed");
          var defaults = Schema.Limits.defaults(); var limited = new Schema.Limits(defaults.maxBytes, defaults.maxStringBytes, 1, defaults.maxDepth);
          rejected(() -> value.encodeXml(limited, null)); rejected(() -> Schema.NilFixture.Sample.decodeXml(data, limited, null));
        }
      } catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(path, expected); continue; }
      if (!valid) throw new AssertionError("Invalid presence accepted: " + path);
      Files.write(Path.of(path + ".java.xml"), encoded);
    }
    System.out.println("Java native Optional states, wrapped collections, nested records and staged limits pass");
  }
}
'''

CSHARP = r'''using System;
using System.IO;
using System.Text;
using static SerializerGenerated.Schema;
// Preserve native optional tuples independently of empty lexical or collection values.
internal static class Program {
  // Reject invalid native storage and complete-document resource policies.
  private static void Rejected(Action operation) {
    try { operation(); } catch (FormatException) { return; }
    throw new Exception("Invalid presence candidate published");
  }
  // Assert absent, nil and present-empty optional states before corpus execution.
  private static void States() {
    string start = "<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>", end = "<nested xsi:nil='true'/></sample>";
    var absent = NilFixtureSample.DecodeXml(Encoding.UTF8.GetBytes(start + "<nil_value xsi:nil='true'/>" + end));
    var nil = NilFixtureSample.DecodeXml(Encoding.UTF8.GetBytes(start + "<nil_value/><states xsi:nil='true'/><entries xsi:nil='true'/>" + end));
    var empty = NilFixtureSample.DecodeXml(Encoding.UTF8.GetBytes(start + "<nil_value/><states/><entries/>" + end));
    if (absent.States.HasValue || absent.Entries.HasValue || absent.NilValue.HasValue || !nil.States.HasValue || nil.States.Value.Item1.HasValue || !nil.Entries.HasValue || nil.Entries.Value.Item1.HasValue || !empty.States.HasValue || empty.States.Value.Item1?.Item1 != "" || !empty.Entries.HasValue || empty.Entries.Value.Item1?.Item1.Count != 0 || !nil.NilValue.HasValue || nil.NilValue.Value.Item1 != "") throw new Exception("Absent/nil/empty states merged");
    empty.NilValue = new ValueTuple<string>(null!); Rejected(() => empty.EncodeXml());
  }
  // Compare the independent oracle and ordinary wire payloads through both XML directions.
  private static void Main(string[] args) {
    States();
    foreach (string path in args) {
      byte[] data = File.ReadAllBytes(path); bool valid = path.EndsWith(".valid.xml", StringComparison.Ordinal), holder = Path.GetFileName(path).StartsWith("holder", StringComparison.Ordinal); byte[] encoded;
      if (Path.GetFileName(path).StartsWith("rich", StringComparison.Ordinal)) {
        NilShapesSample value;
        try { value = NilShapesSample.DecodeXml(data); }
        catch (FormatException expected) { if (valid) throw new Exception(path, expected); continue; }
        if (!valid) throw new Exception("Invalid rich presence accepted: " + path);
        encoded = value.EncodeXml();
        if (!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(NilShapesSample.DecodeXml(encoded).Encode(Protocol.BINARY_NONE))) throw new Exception("Record/enum collection payload changed");
        var broken = NilShapesSample.DecodeXml(encoded); broken.FixedValues.Clear(); Rejected(() => broken.EncodeXml());
        File.WriteAllBytes(path + ".csharp.xml", encoded); continue;
      }
      if (Path.GetFileName(path).StartsWith("all", StringComparison.Ordinal)) {
        NilAllSample value;
        try { value = NilAllSample.DecodeXml(data); }
        catch (FormatException expected) { if (valid) throw new Exception(path, expected); continue; }
        if (!valid) throw new Exception("Invalid unordered presence accepted: " + path);
        encoded = value.EncodeXml();
        if (!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(NilAllSample.DecodeXml(encoded).Encode(Protocol.BINARY_NONE))) throw new Exception("Unordered presence payload changed");
        var broken = NilAllSample.DecodeXml(encoded); broken.States = new ValueTuple<ValueTuple<string>?>(new ValueTuple<string>("bad")); Rejected(() => broken.EncodeXml());
        broken.States = null; broken.Number = new ValueTuple<ushort>(4); Rejected(() => broken.EncodeXml());
        File.WriteAllBytes(path + ".csharp.xml", encoded); continue;
      }
      try {
        if (holder) {
          var value = NilFixtureHolder.DecodeXml(data); encoded = value.EncodeXml();
          if (!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(NilFixtureHolder.DecodeXml(encoded).Encode(Protocol.BINARY_NONE))) throw new Exception("Nested wire payload changed");
        } else {
          var value = NilFixtureSample.DecodeXml(data); encoded = value.EncodeXml();
          if (!value.Encode(Protocol.BINARY_NONE).AsSpan().SequenceEqual(NilFixtureSample.DecodeXml(encoded).Encode(Protocol.BINARY_NONE))) throw new Exception("Wire payload changed");
          var limited = new Limits(maxElements: 1); Rejected(() => value.EncodeXml(limited)); Rejected(() => NilFixtureSample.DecodeXml(data, limited));
        }
      } catch (FormatException expected) { if (valid) throw new Exception(path, expected); continue; }
      if (!valid) throw new Exception("Invalid presence accepted: " + path);
      File.WriteAllBytes(path + ".csharp.xml", encoded);
    }
    Console.WriteLine("C# native optional states, wrapped collections, nested records and staged limits pass");
  }
}
'''

GO = r'''package main
import("bytes";"fmt";"os";"path/filepath";"strings")
// Assert absent, nil and present-empty native pointers before corpus execution.
func states(){
  start:="<sample xmlns='urn:serializer:nil' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>";end:="<nested xsi:nil='true'/></sample>"
  absent,err:=DecodeNilFixtureSampleXML([]byte(start+"<nil_value xsi:nil='true'/>"+end));if err!=nil{panic(err)}
  nilValue,err:=DecodeNilFixtureSampleXML([]byte(start+"<nil_value/><states xsi:nil='true'/><entries xsi:nil='true'/>"+end));if err!=nil{panic(err)}
  empty,err:=DecodeNilFixtureSampleXML([]byte(start+"<nil_value/><states/><entries/>"+end));if err!=nil{panic(err)}
  if absent.States!=nil||absent.Entries!=nil||absent.NilValue!=nil||nilValue.States==nil||*nilValue.States!=nil||nilValue.Entries==nil||*nilValue.Entries!=nil||empty.States==nil||*empty.States==nil||**empty.States!=""||empty.Entries==nil||*empty.Entries==nil||len(**empty.Entries)!=0||nilValue.NilValue==nil||*nilValue.NilValue!=""{panic("Absent/nil/empty states merged")}
  var broken *NilFixtureChild;empty.Nested=&broken;if candidate,err:=empty.EncodeXML();candidate!=nil||err==nil{panic("Nil native record payload published")}
}
// Compare the independent oracle and ordinary wire payloads through both XML directions.
func main(){states();for _,path:=range os.Args[1:]{
  data,err:=os.ReadFile(path);if err!=nil{panic(err)};valid:=strings.HasSuffix(path,".valid.xml");holder:=strings.HasPrefix(filepath.Base(path),"holder");var encoded []byte
  if strings.HasPrefix(filepath.Base(path),"rich"){
    value,decodeErr:=DecodeNilShapesSampleXML(data);if valid!=(decodeErr==nil)||!valid&&value!=nil{panic(fmt.Sprintf("Oracle differs: %s: %v",path,decodeErr))};if !valid{continue}
    encoded,err=value.EncodeXML();if err!=nil{panic(err)};copy,err:=DecodeNilShapesSampleXML(encoded);if err!=nil{panic(err)}
    before,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};after,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(before,after){panic("Record/enum collection payload changed")}
    broken,err:=DecodeNilShapesSampleXML(encoded);if err!=nil{panic(err)};broken.FixedValues=[]uint8{1};if candidate,err:=broken.EncodeXML();candidate!=nil||err==nil{panic("Invalid fixed array output")}
    if err:=os.WriteFile(path+".go.xml",encoded,0600);err!=nil{panic(err)};continue
  }
  if strings.HasPrefix(filepath.Base(path),"all"){
    value,decodeErr:=DecodeNilAllSampleXML(data);if valid!=(decodeErr==nil)||!valid&&value!=nil{panic(fmt.Sprintf("Oracle differs: %s: %v",path,decodeErr))};if !valid{continue}
    encoded,err=value.EncodeXML();if err!=nil{panic(err)};copy,err:=DecodeNilAllSampleXML(encoded);if err!=nil{panic(err)}
    before,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};after,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(before,after){panic("Unordered presence payload changed")}
    broken,err:=DecodeNilAllSampleXML(encoded);if err!=nil{panic(err)};bad:="bad";inner:=&bad;broken.States=&inner;if candidate,err:=broken.EncodeXML();candidate!=nil||err==nil{panic("Invalid nullable pattern output")}
    broken.States=nil;number:=uint16(4);broken.Number=&number;if candidate,err:=broken.EncodeXML();candidate!=nil||err==nil{panic("Invalid optional bounds output")}
    if err:=os.WriteFile(path+".go.xml",encoded,0600);err!=nil{panic(err)};continue
  }
  if holder{
    value,decodeErr:=DecodeNilFixtureHolderXML(data);if valid!=(decodeErr==nil)||!valid&&value!=nil{panic(fmt.Sprintf("Oracle differs: %s: %v",path,decodeErr))};if !valid{continue}
    encoded,err=value.EncodeXML();if err!=nil{panic(err)};copy,err:=DecodeNilFixtureHolderXML(encoded);if err!=nil{panic(err)}
    before,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};after,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(before,after){panic("Nested wire payload changed")}
  }else{
    value,decodeErr:=DecodeNilFixtureSampleXML(data);if valid!=(decodeErr==nil)||!valid&&value!=nil{panic(fmt.Sprintf("Oracle differs: %s: %v",path,decodeErr))};if !valid{continue}
    encoded,err=value.EncodeXML();if err!=nil{panic(err)};copy,err:=DecodeNilFixtureSampleXML(encoded);if err!=nil{panic(err)}
    before,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};after,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(before,after){panic("Wire payload changed")}
    limited:=DefaultXmlOptions();limited.Limits.MaxNodes=1;if candidate,err:=value.EncodeXML(limited);candidate!=nil||err==nil{panic("Output node bound")};if candidate,err:=DecodeNilFixtureSampleXML(data,limited);candidate!=nil||err==nil{panic("Input node bound")}
  }
  if err:=os.WriteFile(path+".go.xml",encoded,0600);err!=nil{panic(err)}
};fmt.Println("Go native pointer states, wrapped collections, nested records and staged limits pass")}
'''


def main():
    """Run generated SDKs and independently validate every accepted output document."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--language', default='java,csharp,go')
    parser.add_argument('--wsl-languages', default='')
    parser.set_defaults(cpp_library=None, executable=None, sanitize=False)
    args = parser.parse_args(); args.compiler = args.compiler.resolve(); args.build = args.build.resolve()
    languages = args.language.split(',')
    if not languages or len(languages) != len(set(languages)) or any(language not in ('java', 'csharp', 'go') for language in languages):
        parser.error('Select distinct language names from java,csharp,go')
    if args.build == ROOT or ROOT in args.build.parents and 'out' not in args.build.relative_to(ROOT).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode())); rich_oracle = etree.XMLSchema(etree.fromstring(RICH_XSD.encode())); all_oracle = etree.XMLSchema(etree.fromstring(ALL_XSD.encode())); paths = []
    (args.build / 'presence.xsd').write_text(BOUND_XSD, encoding='utf-8')
    (args.build / 'rich-presence.xsd').write_text(RICH_XSD, encoding='utf-8')
    (args.build / 'presence-all.xsd').write_text(ALL_XSD, encoding='utf-8')
    for index, (kind, document, valid) in enumerate(cases() + rich_cases() + all_cases()):
        selected_oracle = rich_oracle if kind == 'rich' else all_oracle if kind == 'all' else oracle
        if selected_oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent XSD decision differs for {index}: {selected_oracle.error_log}')
        path = args.build / f'{kind}-{index}.{"valid" if valid else "invalid"}.xml'; path.write_text(document, encoding='utf-8'); paths.append(path)
    optimized_model = MODEL.replace('namespace nil_fixture {', 'namespace nil_fixture { using schema "presence.xsd";')
    model = args.build / 'model.serializer'; model.write_text(optimized_model + RICH_MODEL + ALL_MODEL, encoding='utf-8')
    config = args.build / 'presence.ini'; config.write_text(OPTIONS + RICH_OPTIONS + ALL_OPTIONS, encoding='utf-8')
    command = [args.compiler, '--input', model, '--config', config, '--language', ','.join(languages), '--protocols', 'binary_none', '--go.package', 'main']
    for language in languages:
        target = args.build / language; target.mkdir(parents=True, exist_ok=True)
        command += [f'--{language}.output', target / sdk.OUTPUTS[language]]
    sdk.run(command); runner = sdk.Runner(args)
    sources = {'java': ('Main.java', JAVA), 'csharp': ('Program.cs', CSHARP), 'go': ('main.go', GO)}
    for language in languages:
        source = args.build / 'source' / language; source.mkdir(parents=True, exist_ok=True)
        filename, content = sources[language]; (source / filename).write_text(content, encoding='utf-8')
        for path in paths:
            Path(str(path) + f'.{language}.xml').unlink(missing_ok=True)
        launch = runner.build(language, 'xml_presence', args.build / language, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                (rich_oracle if path.name.startswith('rich') else all_oracle if path.name.startswith('all') else oracle).assertValid(etree.parse(str(emitted)))
            elif emitted.exists():
                raise AssertionError(f'Invalid presence output published: {emitted}')
    rejected = binding_rejections(args, languages)
    print(f'PASS: {len(paths)} independent presence probes, {rejected} atomic binder rejections and resource bounds across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
