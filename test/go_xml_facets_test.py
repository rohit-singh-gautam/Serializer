"""Compile and exercise generated Go scalar facets against an independent XSD oracle."""
import argparse
from pathlib import Path
import subprocess

from lxml import etree


XSD = """<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
<xs:element name="sample"><xs:complexType><xs:sequence>
<xs:element name="integer"><xs:simpleType><xs:restriction base="xs:unsignedLong">
<xs:minInclusive value="18446744073709551613"/><xs:maxExclusive value="18446744073709551615"/>
</xs:restriction></xs:simpleType></xs:element>
<xs:element name="amount"><xs:simpleType><xs:restriction base="xs:double">
<xs:minExclusive value="-1"/><xs:maxInclusive value="1"/>
</xs:restriction></xs:simpleType></xs:element>
<xs:element name="rounded"><xs:simpleType><xs:restriction base="xs:float">
<xs:minInclusive value="0.1"/><xs:maxInclusive value="0.1"/>
</xs:restriction></xs:simpleType></xs:element>
<xs:element name="text"><xs:simpleType><xs:restriction base="xs:string">
<xs:whiteSpace value="collapse"/><xs:length value="2"/><xs:enumeration value="A😀"/>
</xs:restriction></xs:simpleType></xs:element>
<xs:element name="choice"><xs:simpleType><xs:restriction base="xs:unsignedLong">
<xs:enumeration value="18446744073709551615"/>
</xs:restriction></xs:simpleType></xs:element>
</xs:sequence></xs:complexType></xs:element></xs:schema>"""

GO = r'''package main
import ("fmt"; "os"; "strings")
// Check generated decode/encode publication and normalized model values.
func main() {
  for _, path := range os.Args[1:] {
    data, err := os.ReadFile(path); if err != nil { panic(err) }
    value, err := DecodeFacetsSampleXML(data)
    valid := strings.HasSuffix(path, ".valid.xml")
    if valid != (err == nil) || !valid && value != nil { panic(fmt.Sprintf("Oracle differs: %s: %v", path, err)) }
    if !valid { continue }
    if value.Integer != 18446744073709551614 || value.Text != "A😀" || value.Choice != 18446744073709551615 { panic("Scalar normalization changed model values") }
    encoded, err := value.EncodeXML(); if err != nil { panic(err) }
    if _, err := DecodeFacetsSampleXML(encoded); err != nil { panic(err) }
    for _, field := range []string{"integer", "amount", "rounded", "text", "choice"} {
      candidate := *value
      switch field { case "integer": candidate.Integer = 18446744073709551615; case "amount": candidate.Amount = -1; case "rounded": candidate.Rounded = 0.11; case "text": candidate.Text = "B😀"; case "choice": candidate.Choice = 18446744073709551614 }
      if partial, err := candidate.EncodeXML(); partial != nil || err == nil { panic("Invalid output published: " + field) }
    }
  }
  fmt.Println("Go generated numeric, Unicode length, enumeration and whitespace facets pass independent XSD oracle")
}
'''


def main():
    """Keep generated sources, oracle inputs and SDK outputs in the selected ignored directory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build = args.build.resolve()
    root = Path(__file__).resolve().parent.parent
    if args.build == root or root in args.build.parents and 'out' not in args.build.relative_to(root).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    (args.build / 'sample.xsd').write_text(XSD, encoding='utf-8')
    (args.build / 'sample.serializer').write_text(
        'serializer version 1.20.0; namespace facets { using schema "sample.xsd"; '
        'class sample { public uint64 integer; public double amount; public float rounded; '
        'public string text; public uint64 choice; } }', encoding='utf-8')
    subprocess.run([str(args.compiler.resolve()), '--input', str(args.build / 'sample.serializer'),
                    '--language', 'go', '--go.output', str(args.build / 'schema.go'),
                    '--go.package', 'main', '--protocols', 'binary_none', '--cpp.format', 'false'], check=True)
    (args.build / 'main.go').write_text(GO, encoding='utf-8')
    (args.build / 'go.mod').write_text('module serializer-go-xml-facets\n\ngo 1.23\n', encoding='utf-8')
    base = '<sample><integer>{}</integer><amount>{}</amount><rounded>{}</rounded><text>{}</text><choice>{}</choice></sample>'
    values = ['18446744073709551614', '1', '0.1000000001', '  A😀\t', '018446744073709551615']
    documents = [base.format(*values)]
    probes = (
        (0, '18446744073709551612'), (0, '18446744073709551615'), (0, '-1'),
        (1, '-1'), (1, '1.0000000000000002'), (1, 'NaN'),
        (2, '0.1001'), (2, '0.0999'), (3, 'B😀'), (3, 'A😀x'), (4, '18446744073709551614'),
    )
    for index, replacement in probes:
        changed = values.copy()
        changed[index] = replacement
        documents.append(base.format(*changed))
    oracle = etree.XMLSchema(etree.fromstring(XSD.encode()))
    paths = []
    for index, document in enumerate(documents):
        valid = oracle.validate(etree.fromstring(document.encode()))
        if valid != (index == 0):
            raise AssertionError(f'Unexpected independent oracle result for probe {index}: {oracle.error_log}')
        path = args.build / f'{index}.{"valid" if valid else "invalid"}.xml'
        path.write_text(document, encoding='utf-8')
        paths.append(str(path))
    subprocess.run(['go', 'run', '.', *paths], cwd=args.build, check=True)


if __name__ == '__main__':
    main()
