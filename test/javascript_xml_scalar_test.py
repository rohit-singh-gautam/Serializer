# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated JS/TS exact scalar facets and shared portable patterns against independent XSD validation."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:scalar-tokens'
PATTERNS = (
    ('[A-Z]{2,4}', ('AB', 'ABCD', 'A', 'ABCDE', 'aB', '')),
    ('^a$', ('^a$', 'a', '^a', 'a$', '^a$z')),
    ('(ab|é|😀)+', ('abé😀', 'é', '😀', '', 'a', 'ab\n')),
    ('[^a-c]+', ('dé😀', 'z\n', 'a', 'dba', '')),
    ('a.b', ('aéb', 'a😀b', 'a b', 'a\nb', 'a\rb', 'ab')),
    ('((a?)*)b?', ('', 'a', 'aaaa', 'aaab', 'b', 'ba', 'abbb')),
    ('(a|ab){0,3}', ('', 'a', 'ababab', 'aaab', 'abababab', 'b')),
    ('[a-zA-Z0-9_\\-]+', ('a_Z-09', 'é', '', 'a b', 'a.b')),
)


def document(root, attributes):
    """Preserve exact attribute values independently of XML presentation whitespace normalization."""
    encoded = ' '.join(key + '=' + quoteattr(value, {'\n': '&#10;', '\r': '&#13;', '\t': '&#9;'})
                       for key, value in attributes.items())
    return '<' + root + ' xmlns="' + NS + '" ' + encoded + '/>'


class javascript_xml_scalar_test(unittest.TestCase):
    """Use real generated codecs and require a mature independent XSD1.0 provider oracle."""
    def run_node(self, body):
        """Execute generated code with DOM parsing and every model DOM reader disabled."""
        source = self.directory / 'scalar-probe.mjs'
        source.write_text("import assert from 'node:assert/strict';\nimport * as Model from './scalars.mjs';\n"
                          "import * as Native from './native.mjs';\nimport {DOMParser} from '@xmldom/xmldom';\n"
                          "DOMParser.prototype.parseFromString=()=>{throw new Error('DOM forbidden');};\n"
                          "for(const item of Object.values(Model))if(typeof item==='function'&&'_srlXmlRead' in item)"
                          "item._srlXmlRead=()=>{throw new Error('DOM model reader forbidden');};\n"
                          "const chunks=(value,size)=>{const bytes=new TextEncoder().encode(value);"
                          "return Array.from({length:Math.ceil(bytes.length/size)},(_,i)=>bytes.subarray(i*size,(i+1)*size));};\n"
                          + body, encoding='utf-8')
        result = subprocess.run(['node', str(source)], capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def test_exact_numeric_unicode_and_native_wire_boundaries(self):
        """Compare integer/decimal precision, float32 rounding and Unicode enum lengths with real XSD validation."""
        baseline = dict(large='18446744073709551614', signed='-9223372036854775807',
                        decimal='9007199254740993.000001', single='0.5', extended='1', flag='true', word='é😀')
        values = {
            'large': ('18446744073709551613', '18446744073709551614', '18446744073709551615',
                      '9007199254740992', '+18446744073709551614', '-1'),
            'signed': ('-9223372036854775808', '-9223372036854775807', '9223372036854775807',
                       '9223372036854775808', '-0', '+0007'),
            'decimal': ('9007199254740993', '9007199254740993.000001', '9007199254740993.000002',
                        '9.007199254740993e15', '  9007199254740993.000001  '),
            'single': ('0', '-0', '0.5', '1', '1.0000000001', '1.000001', 'NaN', 'INF', '-INF'),
            'extended': ('0', '1', '1.0000000000000002', 'NaN', 'INF', '-INF'),
            'flag': ('true', '1', 'false', '0', 'TRUE', '+1'),
            'word': ('é😀', 'aa', '😀😀', 'é', 'abc', 'é\u0301', 'zz', ''),
        }
        cases = []
        for key, alternatives in values.items():
            for value in alternatives:
                xml = document('numbers', dict(baseline, **{key: value}))
                expected = self.oracle.validate(self.provider.fromstring(xml.encode()))
                # libxml2 rejects legal leading '+' unsignedLong aliases; retain the shared exact value contract.
                if key == 'large' and value.startswith('+'):
                    expected = True
                cases.append([xml, expected, key + '=' + value])
        missing_fixed = document('numbers', {key: value for key, value in baseline.items() if key != 'flag'})
        self.assertFalse(self.oracle.validate(self.provider.fromstring(missing_fixed.encode())))
        cases.append([missing_fixed, False, 'missing required fixed attribute'])
        output = self.run_node('''
        const outputs=[];
        for(const [document,expected,label] of ''' + json.dumps(cases) + '''){
          if(!expected){assert.throws(()=>Model.JsScalarsNumbers.decodeXmlChunks(chunks(document,1)),label);continue;}
          const value=Model.JsScalarsNumbers.decodeXmlChunks(chunks(document,1));const xml=value.encodeXml();outputs.push(xml);
          for(const protocol of Object.values(Model.Protocol)){
            const bytes=value.encode(protocol);assert.deepEqual(Native.JsScalarsNumbers.decode(bytes,protocol).encode(protocol),bytes);
            assert.deepEqual(Model.JsScalarsNumbers.decodeXml(xml).encode(protocol),bytes);
          }
        }
        const owner=Model.JsScalarsNumbers.decodeXml(''' + json.dumps(document('numbers', baseline)) + ''');
        assert.equal(owner.large,18446744073709551614n);assert.equal(owner.decimal,'9007199254740993.000001');
        owner.large=18446744073709551615n;assert.throws(()=>owner.encodeXml());
        owner.large=18446744073709551614n;owner.word='😀';assert.throws(()=>owner.encodeXml());
        console.log(JSON.stringify(outputs));
        ''')
        for xml in json.loads(output):
            self.assertTrue(self.oracle.validate(self.provider.fromstring(xml.encode())), self.oracle.error_log)

    def test_portable_pattern_corpus_groups_and_reentrancy(self):
        """Match shared Thompson programs with independent Unicode/literal-anchor/nullable-cycle oracle cases."""
        cases = []
        for index, (expression, values) in enumerate(PATTERNS):
            for value in values:
                xml = document('p' + str(index), {'value': value})
                cases.append(['JsScalarsP' + str(index), xml, self.oracle.validate(self.provider.fromstring(xml.encode())), expression + ': ' + value])
        for value in ('a', 'aa', 'b', 'bb', 'aaa', 'ab', ''):
            xml = document('groups', {'value': value})
            cases.append(['JsScalarsGroups', xml, self.oracle.validate(self.provider.fromstring(xml.encode())), 'intersected groups: ' + value])
        output = self.run_node('''
        const outputs=[];globalThis.RegExp=function(){throw new Error('Dynamic host pattern compilation forbidden');};
        for(const [name,document,expected,label] of ''' + json.dumps(cases) + '''){
          const type=Model[name];assert(Object.isFrozen(type._srlXmlToken.fields[0].rule.patterns[0][0]));
          if(expected){const owner=type.decodeXmlChunks(chunks(document,1));const encoded=owner.encodeXml();outputs.push(encoded);}
          else assert.throws(()=>type.decodeXml(document),label);
        }
        const document=''' + json.dumps(document('p0', {'value': 'AB'})) + ''';
        const limits=new Model.XmlLimits({pattern:new Model.XmlPatternLimits({maxWorkUnits:1024})});
        function* reentrant(){assert.equal(Model.JsScalarsP0.decodeXml(document,limits).value,'AB');yield new TextEncoder().encode(document);}
        assert.equal(Model.JsScalarsP0.decodeXmlChunks(reentrant(),limits).value,'AB');
        await Promise.all(Array.from({length:24},async()=>assert.equal(Model.JsScalarsP0.decodeXml(document,limits).value,'AB')));
        console.log(JSON.stringify(outputs));
        ''')
        for xml in json.loads(output):
            self.assertTrue(self.oracle.validate(self.provider.fromstring(xml.encode())), self.oracle.error_log)

    def test_fixed_defaults_whitespace_and_value_space_enumerations(self):
        """Materialize defaults, compare fixed numeric aliases and normalize only XML whitespace."""
        output = self.run_node('''
        const defaults=Model.JsScalarsDefaults.decodeXml('<defaults xmlns="'''+NS+'''"><text/><amount>007</amount></defaults>');
        assert.deepEqual(defaults.flag,[true]);assert.deepEqual(defaults.label,['fixed value']);assert.equal(defaults.text,'filled');assert.equal(defaults.amount,7);
        assert.throws(()=>Model.JsScalarsDefaults.decodeXml('<defaults xmlns="'''+NS+'''" label="other"><text/><amount>7</amount></defaults>'));
        defaults.amount=8;assert.throws(()=>defaults.encodeXml());defaults.amount=7;
        const simple=Model.JsScalarsSimple.decodeXml('<simple xmlns="'''+NS+'''"/>');assert.equal(simple.value,'a b');assert.deepEqual(simple.unit,['unit']);
        const enums=Model.JsScalarsEnums.decodeXml('<enums xmlns="'''+NS+'''" number="+0007" token="&#9;a  b&#10;" normalized="a&#9;  b"/>');
        assert.equal(enums.number,7);assert.equal(enums.token,'a b');assert.equal(enums.normalized,'a   b');
        for(const number of ['7','0008','+0007'])assert.doesNotThrow(()=>Model.JsScalarsEnums.decodeXml('<enums xmlns="'''+NS+'''" number="'+number+'" token="a b" normalized="a b"/>'));
        assert.throws(()=>Model.JsScalarsEnums.decodeXml('<enums xmlns="'''+NS+'''" number="9" token="a b" normalized="a b"/>'));
        assert.throws(()=>Model.JsScalarsEnums.decodeXml('<enums xmlns="'''+NS+'''" number="7" token="a b" normalized="a b"/>'));
        console.log(JSON.stringify([defaults.encodeXml(),simple.encodeXml(),enums.encodeXml()]));
        ''')
        for xml in json.loads(output):
            self.assertTrue(self.oracle.validate(self.provider.fromstring(xml.encode())), self.oracle.error_log)

    def test_pattern_equalities_cumulative_work_and_bounded_integer_conversion(self):
        """Check exact state/input/work limits, cumulative matches and compact long integer aliases before BigInt."""
        self.run_node('''
        const document=''' + json.dumps(document('p0', {'value': 'AB'})) + ''';
        const limits=values=>new Model.XmlLimits({pattern:new Model.XmlPatternLimits(values)});
        const states=Model.JsScalarsP0._srlXmlToken.fields[0].rule.patterns[0][0][1].length;
        assert.doesNotThrow(()=>Model.JsScalarsP0.decodeXml(document,limits({maxStates:states,maxInputBytes:2})));
        assert.throws(()=>Model.JsScalarsP0.decodeXml(document,limits({maxStates:states-1})));
        assert.throws(()=>Model.JsScalarsP0.decodeXml(document,limits({maxInputBytes:1})));
        assert.throws(()=>Model.JsScalarsP0.decodeXml(document,limits({maxWorkUnits:0})));
        let low=0,high=1024;
        while(low+1<high){const middle=Math.floor((low+high)/2);try{Model.JsScalarsP0.decodeXml(document,limits({maxWorkUnits:middle}));high=middle;}catch{low=middle;}}
        const owner=Model.JsScalarsP0.decodeXml(document,limits({maxWorkUnits:high}));
        assert.throws(()=>Model.JsScalarsP0.decodeXml(document,limits({maxWorkUnits:high-1})));
        assert.doesNotThrow(()=>owner.encodeXml(limits({maxWorkUnits:high})));
        assert.throws(()=>owner.encodeXml(limits({maxWorkUnits:high-1})));
        const pair=''' + json.dumps(document('pair', {'left': 'AB', 'right': 'AB'})) + ''';
        const both=Model.JsScalarsPair.decodeXml(pair,limits({maxWorkUnits:2*high}));
        assert.throws(()=>Model.JsScalarsPair.decodeXml(pair,limits({maxWorkUnits:2*high-1})));
        assert.doesNotThrow(()=>both.encodeXml(limits({maxWorkUnits:2*high})));
        for(const values of [{maxStates:-1},{maxInputBytes:0.5},{maxWorkUnits:Infinity}])assert.throws(()=>new Model.XmlPatternLimits(values));
        const original=globalThis.BigInt;let largeConversion=false;
        globalThis.BigInt=value=>{if(typeof value==='string'&&value.length>20){largeConversion=true;throw new Error('Unbounded BigInt conversion');}return original(value);};
        const raw=value=>'<raw_integer xmlns="'''+NS+'''" value="'+value+'"/>';
        assert.equal(Model.JsScalarsRawInteger.decodeXmlChunks(chunks(raw('0'.repeat(100000)+'7'),31)).value,7n);
        assert.throws(()=>Model.JsScalarsRawInteger.decodeXml(raw('9'.repeat(100000))));assert.equal(largeConversion,false);
        ''')

    def test_actual_typescript_scalar_policy_and_exact_native_types(self):
        """Compile and execute an actual strict consumer of exact native types and the public pattern policy."""
        source = self.directory / 'typed.mts'
        source.write_text("import {JsScalarsP0,XmlLimits,XmlPatternLimits} from './scalars.mjs';\n"
                          "const owner=JsScalarsP0.decodeXmlChunks([new TextEncoder().encode(" + json.dumps(document('p0', {'value': 'AB'}))
                          + ")],new XmlLimits({pattern:new XmlPatternLimits({maxWorkUnits:1024})}));\n"
                          "const value:string=owner.value;if(value!=='AB')throw new Error('typed scalar policy');\n", encoding='utf-8')
        subprocess.run(['node', str(self.typescript), '--strict', '--target', 'ES2020', '--module', 'NodeNext',
                        '--moduleResolution', 'NodeNext', '--skipLibCheck', 'false', str(source)], check=True)
        subprocess.run(['node', str(source.with_suffix('.mjs'))], check=True)

    def test_unsupported_pattern_preflight_preserves_artifacts(self):
        """Keep genuine provider-only expressions explicit until an independent runtime adapter is implemented."""
        for index, expression in enumerate(('\\p{L}+', '[a-z-[aeiou]]+')):
            xsd = self.directory / ('unsupported' + str(index) + '.xsd')
            xsd.write_text('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root"><xs:complexType>'
                           '<xs:attribute name="value" use="required"><xs:simpleType><xs:restriction base="xs:string">'
                           '<xs:pattern value=' + quoteattr(expression) + '/></xs:restriction></xs:simpleType></xs:attribute>'
                           '</xs:complexType></xs:element></xs:schema>', encoding='utf-8')
            source, output = self.directory / ('unsupported' + str(index) + '.serializer'), self.directory / ('unsupported' + str(index) + '.mjs')
            source.write_text('serializer version 1.20.0; namespace unsupported { using schema "' + xsd.name + '"; class root {public string value(1);} }', encoding='utf-8')
            output.write_text('preserve existing output\n', encoding='utf-8')
            result = subprocess.run([str(self.compiler), '--language', 'js', '--input', str(source), '--js.output', str(output)],
                                    capture_output=True, text=True, encoding='utf-8')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('portable', result.stderr.lower())
            self.assertEqual(output.read_text(encoding='utf-8'), 'preserve existing output\n')


def main():
    """Generate real JS/native controls, require exact locked SDKs and an independent lxml provider."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--node-dependencies', type=Path)
    parser.add_argument('--python-dependencies', type=Path)
    args = parser.parse_args()
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree as provider
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    modules = build / 'node_modules'
    if args.node_dependencies:
        if not modules.is_dir():
            shutil.copytree(args.node_dependencies.resolve(), modules)
    else:
        for filename in ('package.json', 'package-lock.json'):
            shutil.copyfile(ROOT / 'test/resources/xml_shapes/typescript' / filename, build / filename)
        subprocess.run(['npm.cmd' if sys.platform == 'win32' else 'npm', 'ci', '--ignore-scripts', '--no-audit', '--no-fund'],
                       cwd=build, check=True)
    for package, version in (('saxes', '6.0.0'), ('xmlchars', '2.2.0'), ('typescript', '5.9.3'), ('@xmldom/xmldom', '0.9.12')):
        if json.loads((modules / package / 'package.json').read_text(encoding='utf-8'))['version'] != version:
            raise RuntimeError('Actual Node SDK dependency does not match its exact lock: ' + package)
    with tempfile.TemporaryDirectory(prefix='javascript-xml-scalars-', dir=build) as temporary:
        directory = Path(temporary)
        source, schema = directory / 'scalars.serializer', directory / 'scalars.xsd'
        native = '''serializer version 1.20.0; namespace js_scalars { using schema "scalars.xsd";
          class numbers { public uint64 large(1); public int64 signed(2); public string decimal(3);
            public float single(4); public double extended(5); public bool flag(6); public string word(7); }
          class defaults { public optional<bool> flag(1); public optional<string> label(2); public string text(3); public int32 amount(4); }
          class simple { public optional<string> unit(1) xml(attribute); public string value(2) xml(text); }
          class enums { public int32 number(1); public string token(2); public string normalized(3); }
          class raw_integer { public uint64 value(1); }
          class groups { public string value(1); }
          class pair { public string left(1); public string right(2); }
        '''
        xsd = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="'''+NS+'''" targetNamespace="'''+NS+'''" elementFormDefault="qualified">
          <xs:simpleType name="Large"><xs:restriction base="xs:unsignedLong"><xs:minInclusive value="18446744073709551613"/><xs:maxExclusive value="18446744073709551615"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Signed"><xs:restriction base="xs:long"><xs:minExclusive value="-9223372036854775808"/><xs:maxInclusive value="9223372036854775807"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Exact"><xs:restriction base="xs:decimal"><xs:minExclusive value="9007199254740993"/><xs:maxInclusive value="9007199254740993.000001"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Single"><xs:restriction base="xs:float"><xs:minExclusive value="0"/><xs:maxInclusive value="1"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Extended"><xs:restriction base="xs:double"><xs:minInclusive value="0"/><xs:maxExclusive value="1.0000000000000002"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Word"><xs:restriction base="xs:string"><xs:length value="2"/><xs:enumeration value="é😀"/><xs:enumeration value="aa"/><xs:enumeration value="😀😀"/><xs:enumeration value="é́"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Token"><xs:restriction base="xs:token"><xs:enumeration value="a b"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Enum"><xs:restriction base="xs:int"><xs:enumeration value="007"/><xs:enumeration value="8"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Code"><xs:restriction base="xs:string"><xs:pattern value="[A-Z]{2,4}"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Alternatives"><xs:restriction base="xs:string"><xs:pattern value="a+"/><xs:pattern value="b+"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Groups"><xs:restriction base="t:Alternatives"><xs:pattern value="aa|bb"/></xs:restriction></xs:simpleType>
          <xs:element name="numbers"><xs:complexType><xs:attribute name="large" type="t:Large" use="required"/><xs:attribute name="signed" type="t:Signed" use="required"/>
          <xs:attribute name="decimal" type="t:Exact" use="required"/><xs:attribute name="single" type="t:Single" use="required"/><xs:attribute name="extended" type="t:Extended" use="required"/>
          <xs:attribute name="flag" type="xs:boolean" fixed="true" use="required"/><xs:attribute name="word" type="t:Word" use="required"/></xs:complexType></xs:element>
          <xs:element name="defaults"><xs:complexType><xs:sequence><xs:element name="text" type="xs:string" default="filled"/>
          <xs:element name="amount" type="xs:int" fixed="7"/></xs:sequence><xs:attribute name="flag" type="xs:boolean" default="true"/>
          <xs:attribute name="label" type="xs:token" fixed=" fixed  value "/></xs:complexType></xs:element>
          <xs:element name="simple" default="a b"><xs:complexType><xs:simpleContent><xs:extension base="t:Token"><xs:attribute name="unit" type="xs:string" default="unit"/></xs:extension></xs:simpleContent></xs:complexType></xs:element>
          <xs:element name="enums"><xs:complexType><xs:attribute name="number" type="t:Enum" use="required"/><xs:attribute name="token" type="t:Token" use="required"/>
          <xs:attribute name="normalized" type="xs:normalizedString" use="required"/></xs:complexType></xs:element>
          <xs:element name="raw_integer"><xs:complexType><xs:attribute name="value" type="xs:unsignedLong" use="required"/></xs:complexType></xs:element>
          <xs:element name="groups"><xs:complexType><xs:attribute name="value" type="t:Groups" use="required"/></xs:complexType></xs:element>
          <xs:element name="pair"><xs:complexType><xs:attribute name="left" type="t:Code" use="required"/><xs:attribute name="right" type="t:Code" use="required"/></xs:complexType></xs:element>
        '''
        for index, (expression, values) in enumerate(PATTERNS):
            native += 'class p' + str(index) + ' { public string value(1); }\n'
            xsd += '<xs:element name="p' + str(index) + '"><xs:complexType><xs:attribute name="value" use="required"><xs:simpleType><xs:restriction base="xs:string"><xs:pattern value=' + quoteattr(expression) + '/></xs:restriction></xs:simpleType></xs:attribute></xs:complexType></xs:element>'
        source.write_text(native + '}', encoding='utf-8')
        schema.write_text(xsd + '</xs:schema>', encoding='utf-8')
        oracle = provider.XMLSchema(provider.parse(str(schema)))
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source),
                        '--js.output', str(directory / 'scalars.mjs'), '--typescript.output', str(directory / 'scalars.d.mts')], check=True)
        subprocess.run([str(args.compiler.resolve()), '--language', 'js,typescript', '--input', str(source), '--xml.enabled', 'false',
                        '--js.output', str(directory / 'native.mjs'), '--typescript.output', str(directory / 'native.d.mts')], check=True)
        javascript_xml_scalar_test.directory, javascript_xml_scalar_test.compiler = directory, args.compiler.resolve()
        javascript_xml_scalar_test.typescript = modules / 'typescript/bin/tsc'
        javascript_xml_scalar_test.oracle, javascript_xml_scalar_test.provider = oracle, provider
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(javascript_xml_scalar_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
