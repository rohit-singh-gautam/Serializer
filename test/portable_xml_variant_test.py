"""Qualify direct Go/Java/C# element variants against an independently authored XSD choice."""
import argparse
import importlib.util
from pathlib import Path

from lxml import etree


ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('serializer_variant_sdk', ROOT / 'example/run.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)

MODEL = '''serializer version 1.20.0;
namespace variants {
  enum state { zero, one }
  class child { public uint16 code; public string label; }
  class sample {
    public variant(uint32=code,string=text,child=record,state=state) value;
    public union(bool=flag,double=amount) primitive;
  }
  class holder { public sample value; }
}
'''

CONFIG = '''[xml]
enabled = true
[xml.class.variants::sample]
namespace = urn:serializer:variants
wrapper.value = choice
variant.value = element
variant.value.tag.text = {urn:serializer:variants}word
variant.value.type.record = {urn:serializer:variants}childType
variant.value.type.state = {urn:serializer:variants}stateType
wrapper.primitive = primitive
variant.primitive = element
[xml.class.variants::child]
namespace = urn:serializer:variants
[xml.class.variants::holder]
namespace = urn:serializer:variants
'''

XSD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:variants" targetNamespace="urn:serializer:variants" elementFormDefault="qualified">
  <xs:simpleType name="stateType"><xs:restriction base="xs:string"><xs:enumeration value="zero"/><xs:enumeration value="one"/></xs:restriction></xs:simpleType>
  <xs:complexType name="childType"><xs:sequence><xs:element name="code" type="xs:unsignedShort"/><xs:element name="label" type="xs:string"/></xs:sequence></xs:complexType>
  <xs:complexType name="sampleType"><xs:sequence>
    <xs:element name="choice"><xs:complexType><xs:choice>
      <xs:element name="code" type="xs:unsignedInt"/><xs:element name="word" type="xs:string"/><xs:element name="record" type="t:childType"/><xs:element name="state" type="t:stateType"/>
    </xs:choice></xs:complexType></xs:element>
    <xs:element name="primitive"><xs:complexType><xs:choice><xs:element name="flag" type="xs:boolean"/><xs:element name="amount" type="xs:double"/></xs:choice></xs:complexType></xs:element>
  </xs:sequence></xs:complexType>
  <xs:element name="sample" type="t:sampleType"/>
  <xs:element name="holder"><xs:complexType><xs:sequence><xs:element name="value" type="t:sampleType"/></xs:sequence></xs:complexType></xs:element>
</xs:schema>'''


def cases():
    """Exercise every native alternative and independent QName/choice failure boundary."""
    payloads = ['<code>4294967295</code>', '<word>hi🙂&amp;<![CDATA[<>]]></word>',
                '<record><code>65535</code><label>nested</label></record>', '<state>one</state>']
    types = ['xs:unsignedInt', 'xs:string', 't:childType', 't:stateType']
    primitives = ['<flag>0</flag>', '<amount>INF</amount>']
    probes = []
    for selected, payload in enumerate(payloads):
        for primitive, body in enumerate(primitives):
            document = '<sample xmlns="urn:serializer:variants" xmlns:t="urn:serializer:variants" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:i="http://www.w3.org/2001/XMLSchema-instance"><choice>' + payload + '</choice><primitive>' + body + '</primitive></sample>'
            probes.append((selected, primitive, document, True))
            tag = payload[1:payload.index('>')]
            typed = document.replace('<' + tag + '>', '<' + tag + ' i:type="' + types[selected] + '">', 1)
            ptag = 'flag' if primitive == 0 else 'amount'
            typed = typed.replace('<' + ptag + '>', '<' + ptag + ' i:type="xs:' + ('boolean' if primitive == 0 else 'double') + '">')
            probes.append((selected, primitive, typed, True))
            if selected >= 2:
                probes.append((selected, primitive, typed.replace(types[selected], types[selected][2:]), True))
    base = probes[0][2]
    mutations = [
        ('<choice><code>4294967295</code></choice>', '<choice/>'),
        ('<code>4294967295</code>', '<unknown>1</unknown>'),
        ('<code>4294967295</code>', '<code>4294967295</code><code>1</code>'),
        ('<code>4294967295</code>', '<code>1</code><word>other</word>'),
        ('<code>4294967295</code>', '<code>4294967296</code>'),
        ('<code>4294967295</code>', '<code>-1</code>'),
        ('<code>4294967295</code>', '<code><nested/></code>'),
        ('<code>', '<code extra="x">'),
        ('<choice>', '<choice extra="x">'),
        ('<choice>', '<choice i:type="xs:string">'),
        ('<flag>0</flag>', '<flag>yes</flag>'),
        ('<primitive><flag>0</flag></primitive>', '<primitive/>'),
        ('<primitive><flag>0</flag></primitive>', ''),
        ('</primitive>', '<amount>1</amount></primitive>'),
        ('</sample>', '<extra/></sample>'),
        ('<choice>', '<choice>text'),
        ('<code>', '<code i:type="xs:string">'),
        ('<code>', '<code i:type="unbound:unsignedInt">'),
        ('<code>', '<code i:type="xs:missing">'),
        ('<code>', '<code i:type="">'),
        ('<code>', '<code i:type="xs::unsignedInt">'),
        ('<code>', '<code i:type="xs:">'),
        ('<code>', '<code i:type=":unsignedInt">'),
        ('<code>', '<code xmlns:xs="urn:wrong" i:type="xs:unsignedInt">'),
        ('<code>', '<code i:type="xs:unsignedInt" extra="x">'),
        ('<flag>', '<flag i:type="xs:unsignedInt">'),
    ]
    probes.extend((0, 0, base.replace(before, after), False) for before, after in mutations)
    state = next(document for selected, primitive, document, valid in probes if selected == 3 and primitive == 0 and valid)
    probes += [(3, 0, state.replace('<state>one</state>', '<state>missing</state>'), False)]
    record = next(document for selected, primitive, document, valid in probes if selected == 2 and primitive == 0 and valid)
    probes.extend((2, 0, record.replace(before, after), False) for before, after in [
        ('<code>65535</code>', '<code>65536</code>'), ('<label>nested</label>', ''),
        ('<record>', '<record unknown="x">'), ('<label>nested</label>', '<label>nested</label><label>twice</label>')])
    return probes


JAVA = r'''import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
/** Require exact choice identity before returning a complete model or byte sequence. */
public final class Main {
  /** Require invalid variants and global resource policies to fail. */
  private static void rejected(Runnable operation) {
    try { operation.run(); } catch (IllegalArgumentException expected) { return; }
    throw new AssertionError("Invalid variant published");
  }
  /** Compare each independent choice probe and round-trip every native alternative. */
  public static void main(String[] args) throws Exception {
    for (String path : args) {
      byte[] data = Files.readAllBytes(Path.of(path)); boolean valid = path.endsWith(".valid.xml");
      Schema.Variants.Sample value;
      try { value = Schema.Variants.Sample.decodeXml(data); }
      catch (IllegalArgumentException expected) { if (valid) throw new AssertionError(path, expected); continue; }
      if (!valid) throw new AssertionError("Invalid choice accepted: " + path);
      String file = Path.of(path).getFileName().toString(); int selected = file.charAt(1) - '0', primitive = file.charAt(3) - '0';
      if (value.valueIndex != selected || value.primitiveIndex != primitive) throw new AssertionError("Native alternative index");
      if ((selected == 0 && value.valueCode != -1) || (selected == 1 && !value.valueText.equals("hi🙂&<>")) || (selected == 2 && (value.valueRecord.code != -1 || !value.valueRecord.label.equals("nested"))) || (selected == 3 && value.valueState != Schema.Variants.State.values()[1]) || (primitive == 0 && value.primitiveFlag) || (primitive == 1 && value.primitiveAmount != Double.POSITIVE_INFINITY)) throw new AssertionError("Native typed payload");
      byte[] encoded = value.encodeXml();
      if (!Arrays.equals(encoded, Schema.Variants.Sample.decodeXml(encoded).encodeXml())) throw new AssertionError("Canonical alternative output");
      Files.write(Path.of(path + ".java.xml"), encoded);
      var holder = new Schema.Variants.Holder(); holder.value = value; byte[] nested = holder.encodeXml();
      Schema.Variants.Holder.decodeXml(nested);
      var defaults = Schema.Limits.defaults(); var limited = new Schema.Limits(defaults.maxBytes, defaults.maxStringBytes, 1, defaults.maxDepth);
      rejected(() -> value.encodeXml(limited, null)); rejected(() -> Schema.Variants.Sample.decodeXml(data, limited, null)); rejected(() -> Schema.Variants.Holder.decodeXml(nested, limited, null));
      var broken = Schema.Variants.Sample.decodeXml(encoded); broken.valueIndex = -1; rejected(broken::encodeXml);
      broken.valueIndex = 4; rejected(broken::encodeXml); broken.valueIndex = 2; broken.valueRecord = null; rejected(broken::encodeXml);
      broken.valueIndex = 1; broken.valueText = null; rejected(broken::encodeXml); broken.valueIndex = 3; broken.valueState = null; rejected(broken::encodeXml);
      var other = Schema.Variants.Sample.decodeXml(encoded); other.primitiveIndex = 2; rejected(other::encodeXml);
    }
    System.out.println("Java exact variant identities, typed payloads, limits and staged failures pass");
  }
}
'''

CSHARP = r'''using System;
using System.IO;
using static SerializerGenerated.Schema;
// Require exact choice identity before returning a complete model or byte sequence.
internal static class Program {
  // Require invalid variants and global resource policies to fail.
  private static void Rejected(Action operation) {
    try { operation(); } catch (FormatException) { return; }
    throw new Exception("Invalid variant published");
  }
  // Compare each independent choice probe and round-trip every native alternative.
  private static void Main(string[] args) {
    foreach (string path in args) {
      byte[] data = File.ReadAllBytes(path); bool valid = path.EndsWith(".valid.xml", StringComparison.Ordinal);
      VariantsSample value;
      try { value = VariantsSample.DecodeXml(data); }
      catch (FormatException expected) { if (valid) throw new Exception(path, expected); continue; }
      if (!valid) throw new Exception("Invalid choice accepted: " + path);
      string file = Path.GetFileName(path); int selected = file[1] - '0', primitive = file[3] - '0';
      if (value.ValueIndex != selected || value.PrimitiveIndex != primitive) throw new Exception("Native alternative index");
      if ((selected == 0 && value.ValueCode != uint.MaxValue) || (selected == 1 && value.ValueText != "hi🙂&<>") || (selected == 2 && (value.ValueRecord.Code != ushort.MaxValue || value.ValueRecord.Label != "nested")) || (selected == 3 && value.ValueState != (VariantsState)1) || (primitive == 0 && value.PrimitiveFlag) || (primitive == 1 && value.PrimitiveAmount != double.PositiveInfinity)) throw new Exception("Native typed payload");
      byte[] encoded = value.EncodeXml();
      if (!encoded.AsSpan().SequenceEqual(VariantsSample.DecodeXml(encoded).EncodeXml())) throw new Exception("Canonical alternative output");
      File.WriteAllBytes(path + ".csharp.xml", encoded);
      var holder = new VariantsHolder { Value = value }; byte[] nested = holder.EncodeXml(); VariantsHolder.DecodeXml(nested);
      var limited = new Limits(maxElements: 1);
      Rejected(() => value.EncodeXml(limited)); Rejected(() => VariantsSample.DecodeXml(data, limited)); Rejected(() => VariantsHolder.DecodeXml(nested, limited));
      var broken = VariantsSample.DecodeXml(encoded); broken.ValueIndex = -1; Rejected(() => broken.EncodeXml());
      broken.ValueIndex = 4; Rejected(() => broken.EncodeXml()); broken.ValueIndex = 2; broken.ValueRecord = null!; Rejected(() => broken.EncodeXml());
      broken.ValueIndex = 1; broken.ValueText = null!; Rejected(() => broken.EncodeXml()); broken.ValueIndex = 3; broken.ValueState = (VariantsState)9; Rejected(() => broken.EncodeXml());
      var other = VariantsSample.DecodeXml(encoded); other.PrimitiveIndex = 2; Rejected(() => other.EncodeXml());
    }
    Console.WriteLine("C# exact variant identities, typed payloads, limits and staged failures pass");
  }
}
'''

GO = r'''package main
import("bytes";"fmt";"math";"os";"path/filepath";"strings")
// Compare each independent choice probe and require invalid operations to return no candidate.
func main(){for _,path:=range os.Args[1:]{
  data,err:=os.ReadFile(path);if err!=nil{panic(err)};valid:=strings.HasSuffix(path,".valid.xml")
  value,err:=DecodeVariantsSampleXML(data);if valid!=(err==nil)||!valid&&value!=nil{panic(fmt.Sprintf("Oracle differs: %s: %v",path,err))};if !valid{continue}
  file:=filepath.Base(path);selected,primitive:=int(file[1]-'0'),int(file[3]-'0')
  if value.ValueIndex!=selected||value.PrimitiveIndex!=primitive{panic("Native alternative index")}
  if selected==0&&value.ValueCode!=math.MaxUint32||selected==1&&value.ValueText!="hi🙂&<>"||selected==2&&(value.ValueRecord==nil||value.ValueRecord.Code!=math.MaxUint16||value.ValueRecord.Label!="nested")||selected==3&&value.ValueState!=VariantsState(1)||primitive==0&&value.PrimitiveFlag||primitive==1&&!math.IsInf(value.PrimitiveAmount,1){panic("Native typed payload")}
  encoded,err:=value.EncodeXML();if err!=nil{panic(err)};copy,err:=DecodeVariantsSampleXML(encoded);if err!=nil{panic(err)};again,err:=copy.EncodeXML();if err!=nil||!bytes.Equal(encoded,again){panic("Canonical alternative output")}
  if err:=os.WriteFile(path+".go.xml",encoded,0600);err!=nil{panic(err)}
  holder:=NewVariantsHolder();holder.Value=value;nested,err:=holder.EncodeXML();if err!=nil{panic(err)};if _,err:=DecodeVariantsHolderXML(nested);err!=nil{panic(err)}
  limited:=DefaultXmlOptions();limited.Limits.MaxNodes=1
  if candidate,err:=value.EncodeXML(limited);candidate!=nil||err==nil{panic("Output node bound")};if candidate,err:=DecodeVariantsSampleXML(data,limited);candidate!=nil||err==nil{panic("Input node bound")};if candidate,err:=DecodeVariantsHolderXML(nested,limited);candidate!=nil||err==nil{panic("Nested node bound")}
  broken,err:=DecodeVariantsSampleXML(encoded);if err!=nil{panic(err)}
  for _,index:=range []int{-1,4}{broken.ValueIndex=index;if candidate,err:=broken.EncodeXML();candidate!=nil||err==nil{panic("Invalid variant index")}}
  broken.ValueIndex=2;broken.ValueRecord=nil;if candidate,err:=broken.EncodeXML();candidate!=nil||err==nil{panic("Nil record payload")}
  broken.ValueIndex=3;broken.ValueState=VariantsState(9);if candidate,err:=broken.EncodeXML();candidate!=nil||err==nil{panic("Invalid enum payload")}
  other,err:=DecodeVariantsSampleXML(encoded);if err!=nil{panic(err)};other.PrimitiveIndex=2;if candidate,err:=other.EncodeXML();candidate!=nil||err==nil{panic("Invalid union index")}
};fmt.Println("Go exact variant identities, typed payloads, limits and staged failures pass")}
'''


def main():
    """Generate through the real compiler, execute SDKs, and independently validate all output."""
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
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode())); paths = []
    (args.build / 'variants.xsd').write_text(XSD, encoding='utf-8')
    for index, (selected, primitive, document, valid) in enumerate(cases()):
        if oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent XSD decision differs for {index}: {oracle.error_log}')
        path = args.build / f'v{selected}u{primitive}-{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8'); paths.append(path)
    model = args.build / 'model.serializer'; model.write_text(MODEL, encoding='utf-8')
    config = args.build / 'variants.ini'; config.write_text(CONFIG, encoding='utf-8')
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
        launch = runner.build(language, 'xml_variants', args.build / language, source)
        sdk.run([*launch, *(runner.path(language, path) for path in paths)])
        for path in paths:
            emitted = Path(str(path) + f'.{language}.xml')
            if path.name.endswith('.valid.xml'):
                oracle.assertValid(etree.parse(str(emitted)))
            elif emitted.exists():
                raise AssertionError(f'Invalid choice output published: {emitted}')
    print(f'PASS: {len(paths)} independent element-variant probes and resource bounds across {len(languages)} SDKs', flush=True)


if __name__ == '__main__':
    main()
