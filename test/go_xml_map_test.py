"""Qualify generated Go map tokens, typed key identities and independent XSD cardinality rules."""
import argparse
from pathlib import Path
import re
import subprocess

from lxml import etree


MODEL = '''serializer version 1.20.0;
namespace maps {
  enum key { alpha, beta }
  class child { public uint16 code; public string label; }
  class sample {
    public map(uint32) string integers;
    public map(bool) int16 booleans;
    public map(string) child records;
    public map(key) string enumerated;
    public optional<map<uint64,string>> optional_values;
    public map(int8) string signed_values;
  }
  class holder { public sample value; }
}
'''

CONFIG = '''[xml]
enabled = true
[xml.class.maps::sample]
namespace = urn:serializer:maps
wrapper.integers = integers
map.integers = entry
map.integers.key = id
map.integers.key_placement = attribute
map.integers.value = data
wrapper.booleans = booleans
map.booleans = entry
map.booleans.value = data
wrapper.records = records
map.records = entry
map.records.value = data
wrapper.enumerated = enumerated
map.enumerated = entry
map.enumerated.value = data
wrapper.optional_values = optional_values
map.optional_values = entry
map.optional_values.value = data
wrapper.signed_values = signed_values
map.signed_values = entry
map.signed_values.value = data
[xml.class.maps::child]
namespace = urn:serializer:maps
[xml.class.maps::holder]
namespace = urn:serializer:maps
'''

GO = r'''package main
import ("bytes"; "fmt"; "os"; "strings")
// Preserve old positional options while checking every map before candidate publication.
func main() {
  legacy := XmlOptions{XmlLimits{16<<20,8<<20,100000,100000,128},nil}
  exact := XmlMapLimits{2,20}
  for _, path := range os.Args[1:] {
    data, err := os.ReadFile(path); if err != nil { panic(err) }
    valid := strings.HasSuffix(path,".valid.xml")
    value, err := DecodeMapsSampleXML(data,legacy)
    if valid != (err == nil) || !valid && value != nil { panic(fmt.Sprintf("Oracle differs: %s: %v",path,err)) }
    if !valid { continue }
    recordKey:="A&B";if bytes.Contains(data,[]byte("<key> </key>")){recordKey=" "}
    if len(value.Integers)!=2 || value.Integers[4294967295]!="max" || value.Integers[1]!="one" || value.Booleans[true]!=-32768 || value.Booleans[false]!=32767 || value.Records[recordKey]==nil || value.Records[recordKey].Code!=65535 || value.Records[recordKey].Label!="nested" || value.Enumerated[MapsKey(0)]!="first" || value.SignedValues[-128]!="negative" { panic("Map key/value widths changed") }
    present := bytes.Contains(data,[]byte("optional_values"))
    if (value.OptionalValues!=nil)!=present { panic("Optional map presence changed") }
    encoded, err := value.EncodeXML(legacy); if err != nil { panic(err) }
    copy, err := DecodeMapsSampleXML(encoded,legacy); if err != nil { panic(err) }
    again, err := copy.EncodeXML(legacy); if err != nil || !bytes.Equal(encoded,again) { panic("Deterministic map output changed") }
    if err := os.WriteFile(path+".go.xml",encoded,0600); err != nil { panic(err) }
    // Two entries and twenty key bytes apply independently to each wrapper, through nested records.
    if _,err:=value.EncodeXMLWithMapLimits(exact,legacy);err!=nil{panic(err)}
    if _,err:=DecodeMapsSampleXMLWithMapLimits(data,exact,legacy);err!=nil{panic(err)}
    holder:=NewMapsHolder();holder.Value=value
    nested,err:=holder.EncodeXMLWithMapLimits(exact,legacy);if err!=nil{panic(err)}
    if _,err:=DecodeMapsHolderXMLWithMapLimits(nested,exact,legacy);err!=nil{panic(err)}
    for _,limit:=range []XmlMapLimits{{1,20},{2,9},{-1,20},{2,-1},{0,0}} {
      if partial,err:=value.EncodeXMLWithMapLimits(limit,legacy);partial!=nil||err==nil{panic("Invalid map output bound ignored")}
      if partial,err:=DecodeMapsSampleXMLWithMapLimits(data,limit,legacy);partial!=nil||err==nil{panic("Invalid map input bound ignored")}
      if partial,err:=DecodeMapsHolderXMLWithMapLimits(nested,limit,legacy);partial!=nil||err==nil{panic("Nested map input bound ignored")}
    }
    broken:=*value;broken.Records=map[string]*MapsChild{"bad":nil}
    if partial,err:=broken.EncodeXML();partial!=nil||err==nil{panic("Nil map record output published")}
    brokenEnum:=*value;brokenEnum.Enumerated=map[MapsKey]string{MapsKey(9):"bad"}
    if partial,err:=brokenEnum.EncodeXML();partial!=nil||err==nil{panic("Unknown map enum key published")}
    empty:=*value;empty.Integers=map[uint32]string{};empty.Booleans=map[bool]int16{};empty.Records=map[string]*MapsChild{};empty.Enumerated=map[MapsKey]string{};empty.SignedValues=map[int8]string{}
    emptyOptional:=map[uint64]string{};empty.OptionalValues=&emptyOptional
    emptyBytes,err:=empty.EncodeXMLWithMapLimits(XmlMapLimits{});if err!=nil{panic(err)}
    emptyCopy,err:=DecodeMapsSampleXMLWithMapLimits(emptyBytes,XmlMapLimits{});if err!=nil||emptyCopy.OptionalValues==nil||len(*emptyCopy.OptionalValues)!=0{panic("Empty wrappers at zero map bounds")}
    if err:=os.WriteFile(path+".empty.go.xml",emptyBytes,0600);err!=nil{panic(err)}
    // Cumulative key accounting charges lexical aliases before the native map discards their spelling.
    lexical:=bytes.Replace(data,[]byte("id=\"00001\""),[]byte("id=\"0000000000000000000000000000000000001\""),1)
    if !bytes.Equal(lexical,data) { if partial,err:=DecodeMapsSampleXMLWithMapLimits(lexical,exact,legacy);partial!=nil||err==nil{panic("Lexical key bytes bypassed map bound")} }
    many:=*value;many.Integers=make(map[uint32]string)
    for index:=uint32(0);index<1025;index++{many.Integers[index]="x"}
    if partial,err:=many.EncodeXML();partial!=nil||err==nil{panic("Default map entry limit bypassed")}
    expanded,err:=many.EncodeXMLWithMapLimits(XmlMapLimits{1025,1<<20});if err!=nil{panic(err)}
    if partial,err:=DecodeMapsSampleXML(expanded);partial!=nil||err==nil{panic("Default input map entry limit bypassed")}
    if _,err:=DecodeMapsSampleXMLWithMapLimits(expanded,XmlMapLimits{1025,1<<20});err!=nil{panic(err)}
    largeKey:=*value;largeKey.Records=map[string]*MapsChild{strings.Repeat("x",(1<<20)+1):NewMapsChild()}
    if partial,err:=largeKey.EncodeXML();partial!=nil||err==nil{panic("Default map key byte bound bypassed")}
    largeBytes,err:=largeKey.EncodeXMLWithMapLimits(XmlMapLimits{2,(1<<20)+1});if err!=nil{panic(err)}
    if partial,err:=DecodeMapsSampleXML(largeBytes);partial!=nil||err==nil{panic("Default input key byte bound bypassed")}
    if _,err:=DecodeMapsSampleXMLWithMapLimits(largeBytes,XmlMapLimits{2,(1<<20)+1});err!=nil{panic(err)}
  }
  fmt.Println("Go map typed aliases, optional wrappers, arbitrary key/value order and per-wrapper bounds pass")
}
'''


def xsd():
    """Declare map structure and native value-space key uniqueness independently of Serializer."""
    declarations = []
    identities = []
    for name, kind, attribute in (
            ('integers', 'xs:unsignedInt', True), ('booleans', 'xs:boolean', False),
            ('records', 'xs:string', False), ('enumerated', 't:keyType', False),
            ('optional_values', 'xs:unsignedLong', False), ('signed_values', 'xs:byte', False)):
        value_kind = 't:childType' if name == 'records' else 'xs:short' if name == 'booleans' else 'xs:string'
        data = f'<xs:element name="data" type="{value_kind}"/>'
        key = f'<xs:element name="key" type="{kind}"/>'
        entry = '<xs:sequence>' + data + '</xs:sequence>' + f'<xs:attribute name="id" type="{kind}" use="required"/>' if attribute else '<xs:all>' + key + data + '</xs:all>'
        optional = ' minOccurs="0"' if name == 'optional_values' else ''
        declarations.append(f'<xs:element name="{name}"{optional}><xs:complexType><xs:sequence><xs:element name="entry" minOccurs="0" maxOccurs="unbounded"><xs:complexType>{entry}</xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element>')
        identities.append(f'<xs:key name="{name}"><xs:selector xpath="t:{name}/t:entry"/><xs:field xpath="{"@id" if attribute else "t:key"}"/></xs:key>')
    return '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:serializer:maps" targetNamespace="urn:serializer:maps" elementFormDefault="qualified"><xs:simpleType name="keyType"><xs:restriction base="xs:string"><xs:enumeration value="alpha"/><xs:enumeration value="beta"/></xs:restriction></xs:simpleType><xs:complexType name="childType"><xs:sequence><xs:element name="code" type="xs:unsignedShort"/><xs:element name="label" type="xs:string"/></xs:sequence></xs:complexType><xs:element name="sample"><xs:complexType><xs:sequence>' + ''.join(declarations) + '</xs:sequence></xs:complexType>' + ''.join(identities) + '</xs:element></xs:schema>'


def cases():
    """Pin duplicate canonical keys, structural failures and valid wrapper presence separately."""
    base = '''<sample xmlns="urn:serializer:maps">
<integers><entry id="4294967295"><data>max</data></entry><entry id="00001"><data>one</data></entry></integers>
<booleans><entry><data>-32768</data><key>true</key></entry><entry><key>0</key><data>32767</data></entry></booleans>
<records><entry><data><code>65535</code><label>nested</label></data><key>A&amp;B</key></entry><entry><key></key><data><code>0</code><label/></data></entry></records>
<enumerated><entry><key>alpha</key><data>first</data></entry><entry><data>second</data><key>beta</key></entry></enumerated>
<signed_values><entry><data>negative</data><key>-128</key></entry><entry><key>127</key><data>positive</data></entry></signed_values>
</sample>'''
    probes = [(base, True), (base.replace('<signed_values>', '<optional_values/><signed_values>'), True),
              (base.replace('<signed_values>', '<optional_values><entry><data>maximum</data><key>18446744073709551615</key></entry></optional_values><signed_values>'), True),
              (re.sub(r'<(/?)([a-z_]+)', r'<\1m:\2', base).replace('xmlns="', 'xmlns:m="'), True)]
    changes = (
        ('id="00001"', 'id="4294967295"'), ('<key>0</key>', '<key>1</key>'),
        ('<key>0</key>', '<key>true</key>'), ('<key>127</key>', '<key>-128</key>'),
        ('<key>beta</key>', '<key>alpha</key>'), ('<key></key>', '<key>A&amp;B</key>'),
        ('<key>127</key>', '<key>128</key>'), ('id="00001"', 'id="4294967296"'),
        ('<key>true</key>', '<key>True</key>'), ('<key>alpha</key>', '<key>unknown</key>'),
        ('<data>one</data>', ''), ('id="00001"', ''),
        ('<key>0</key>', ''), ('<data>32767</data>', ''),
        ('<data>one</data>', '<data>one</data><data>extra</data>'),
        ('<key>0</key>', '<key>0</key><key>false</key>'),
        ('<integers>', '<integers unknown="x">'), ('id="00001"', 'id="00001" unknown="x"'),
        ('<key>true</key>', '<key attr="x">true</key>'),
        ('<key>true</key>', '<key><nested/></key>'),
        ('<key>true</key>', '<key xmlns="">true</key>'),
        ('<code>65535</code><label>nested</label>', '<label>nested</label><code>65535</code>'),
        ('<code>65535</code>', '<code>65536</code>'),
        ('</integers>', '<unknown/></integers>'), ('</integers>', 'mixed</integers>'),
        ('</records>', '</records><records/>'),
        ('<integers><entry id="4294967295"><data>max</data></entry><entry id="00001"><data>one</data></entry></integers>', ''),
    )
    probes.extend((base.replace(before, after), False) for before, after in changes)
    # Distinct string keys preserve XML whitespace; integer zero spellings share one native key.
    probes.append((base.replace('<key>A&amp;B</key>', '<key> </key>'), True))
    probes.append((base.replace('id="4294967295"', 'id="-0"').replace('id="00001"', 'id="0"'), False))
    return probes


def main():
    """Generate the real Go codec, compile its SDK, and revalidate every emitted map document."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args(); args.build = args.build.resolve()
    root = Path(__file__).resolve().parent.parent
    if args.build == root or root in args.build.parents and 'out' not in args.build.relative_to(root).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    schema = xsd(); oracle = etree.XMLSchema(etree.fromstring(schema.encode())); paths = []
    (args.build / 'maps.xsd').write_text(schema, encoding='utf-8')
    for index, (document, valid) in enumerate(cases()):
        if oracle.validate(etree.fromstring(document.encode())) != valid:
            raise AssertionError(f'Independent XSD decision differs for {index}: {oracle.error_log}')
        path = args.build / f'{index}.{"valid" if valid else "invalid"}.xml'; path.write_text(document, encoding='utf-8')
        Path(str(path) + '.go.xml').unlink(missing_ok=True); paths.append(str(path))
    (args.build / 'model.serializer').write_text(MODEL, encoding='utf-8')
    (args.build / 'maps.ini').write_text(CONFIG, encoding='utf-8')
    subprocess.run([str(args.compiler.resolve()), '--input', str(args.build / 'model.serializer'), '--config', str(args.build / 'maps.ini'), '--language', 'go', '--go.output', str(args.build / 'schema.go'), '--go.package', 'main', '--protocols', 'binary_none'], check=True)
    (args.build / 'main.go').write_text(GO, encoding='utf-8')
    (args.build / 'go.mod').write_text('module serializer-go-xml-maps\n\ngo 1.22\n', encoding='utf-8')
    subprocess.run(['go', 'run', '.', *paths], cwd=args.build, check=True)
    for path in paths:
        if path.endswith('.valid.xml'):
            oracle.assertValid(etree.parse(path + '.go.xml'))
            oracle.assertValid(etree.parse(path + '.empty.go.xml'))
    print(f'PASS: {len(paths)} independent map probes and explicit resource boundaries', flush=True)


if __name__ == '__main__':
    main()
