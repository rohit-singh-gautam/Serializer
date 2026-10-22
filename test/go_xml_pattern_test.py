"""Qualify generated Go portable XML pattern programs against the independent XSD provider."""
import argparse
from pathlib import Path
import subprocess

from lxml import etree
import xml_unicode_pattern_fixture as unicode_patterns


XSD = r'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
<xs:simpleType name="base"><xs:restriction base="xs:string">
<xs:pattern value="A[😀-🙏]{1,2}|B[0-9]+|^$|C[^0-9]{1,2}|D(a?)*"/>
</xs:restriction></xs:simpleType>
<xs:simpleType name="bounded"><xs:restriction base="base"><xs:pattern value=".{2,3}"/>
</xs:restriction></xs:simpleType>
<xs:element name="sample"><xs:complexType><xs:sequence>
<xs:element name="value" type="bounded"/>
</xs:sequence></xs:complexType></xs:element></xs:schema>'''
XSD = XSD.replace('D(a?)*', 'D(a?)*|' + unicode_patterns.PREFIXED).replace('.{2,3}', '.{2,8}')

GO = r'''package main
import ("fmt"; "os"; "strings")
// Verify independent pattern admissions on both generated directions and runtime bounds.
func main() {
  for _, path := range os.Args[1:] {
    document, err := os.ReadFile(path); if err != nil { panic(err) }
    scalar, err := os.ReadFile(path + ".txt"); if err != nil { panic(err) }
    valid := strings.HasSuffix(path, ".valid.xml")
    candidate, err := DecodePatternsSampleXML(document)
    if valid != (err == nil) || !valid && candidate != nil { panic(fmt.Sprintf("Input oracle differs: %s: %v",path,err)) }
    value := NewPatternsSample(); value.Value = string(scalar)
    encoded, err := value.EncodeXML()
    if valid != (err == nil) || !valid && encoded != nil { panic(fmt.Sprintf("Output oracle differs: %s: %v",path,err)) }
    if valid { copy, err := DecodePatternsSampleXML(encoded); if err != nil || copy.Value != value.Value { panic("Pattern Unicode roundtrip") } }
  }
  program := srlXmlPattern{start:0,states:[]srlXmlPatternState{{accepting:true}}}
  work := srlXmlPatternWork{}
  if _,err:=program.matches("",&work);err==nil||err.Error()!="Portable XML regex work limit exceeded"{panic("Pattern work admission")}
  work.remaining=srlXmlPatternMaxWorkUnits
  if _,err:=program.matches(strings.Repeat("x",srlXmlPatternMaxInputBytes+1),&work);err==nil{panic("Pattern input admission")}
  program.states=make([]srlXmlPatternState,srlXmlPatternMaxStates+1)
  if _,err:=program.matches("",&work);err==nil{panic("Pattern state admission")}
  fmt.Println("Go generated portable pattern groups, Unicode, nullable loops and resource bounds pass independent XSD oracle")
}
'''


def main():
    """Generate prepared programs once, run their real SDK, and reject unsupported syntax precisely."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build = args.build.resolve()
    root = Path(__file__).resolve().parent.parent
    if args.build == root or root in args.build.parents and 'out' not in args.build.relative_to(root).parts:
        parser.error('In-repository output must be beneath the ignored out directory')
    args.build.mkdir(parents=True, exist_ok=True)
    schema = args.build / 'sample.xsd'
    schema.write_text(XSD, encoding='utf-8')
    model = args.build / 'model.serializer'
    model.write_text('serializer version 1.20.0; namespace patterns { using schema "sample.xsd"; '
                     'class sample { public string value; } }', encoding='utf-8')
    output = args.build / 'schema.go'
    command = [str(args.compiler.resolve()), '--input', str(model), '--language', 'go',
               '--go.output', str(output), '--go.package', 'main', '--protocols', 'binary_none']
    subprocess.run(command, check=True)
    original = output.read_bytes()
    (args.build / 'main.go').write_text(GO, encoding='utf-8')
    (args.build / 'go.mod').write_text('module serializer-go-xml-patterns\n\ngo 1.23\n', encoding='utf-8')
    oracle = unicode_patterns.Oracle(XSD)
    samples = ('A😀', 'A😁🙏', 'A😀😁🙏', 'A🧪', 'B1', 'B12', 'B123', 'B😀', '^$',
               'x^$y', '^', '$', 'Cx', 'Cxy', 'C5', 'C😀', 'C\n', 'C\r', 'D', 'Da',
               'Daa', 'Daaa', '', 'A', 'A😀x', 'AB1', '😀', 'C&', 'C<', 'Cé', 'C中')
    samples += unicode_patterns.SAMPLES
    paths = []
    for index, text in enumerate(samples):
        root = etree.Element('sample')
        etree.SubElement(root, 'value').text = text
        document = etree.tostring(root, encoding='utf-8')
        valid = oracle.validate(root)
        path = args.build / f'{index}.{"valid" if valid else "invalid"}.xml'
        path.write_bytes(document)
        Path(str(path) + '.txt').write_text(text, encoding='utf-8')
        paths.append(str(path))
    subprocess.run(['go', 'run', '.', *paths], cwd=args.build, check=True)
    unsupported = XSD.replace(r'A[😀-🙏]{1,2}|B[0-9]+|^$|C[^0-9]{1,2}|D(a?)*', r'\1')
    schema.write_text(unsupported, encoding='utf-8')
    rejected = subprocess.run(command, capture_output=True, text=True)
    if rejected.returncode == 0 or 'XSD' not in rejected.stdout + rejected.stderr:
        raise AssertionError('Unsupported Go pattern requires a precise subset diagnostic')
    if output.read_bytes() != original:
        raise AssertionError('Unsupported pattern replaced previously generated source')


if __name__ == '__main__':
    main()
