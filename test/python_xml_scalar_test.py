# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated Python scalar facets and provider-independent portable pattern programs."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
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
    ('\\p{L}+', ('abc', 'é', '漢字', '123', '')),
    ('[a-z-[aeiou]]+', ('bcd', 'rhythm', 'ae', 'é', '')),
)


UNION_TYPES = '''
<xs:simpleType name="IntegerFirst"><xs:union memberTypes="xs:int xs:token"/></xs:simpleType>
<xs:simpleType name="StringFirst"><xs:union memberTypes="xs:token xs:int"/></xs:simpleType>
<xs:simpleType name="RawStringFirst"><xs:union memberTypes="xs:string xs:int"/></xs:simpleType>
<xs:simpleType name="SpacedString"><xs:restriction base="xs:string"><xs:pattern value="a b"/></xs:restriction></xs:simpleType>
<xs:simpleType name="UriFirst"><xs:union memberTypes="t:SpacedString xs:anyURI"/></xs:simpleType>
<xs:simpleType name="BooleanFirst"><xs:union memberTypes="xs:boolean xs:int xs:token"/></xs:simpleType>
<xs:simpleType name="DecimalFirst"><xs:union memberTypes="xs:int xs:decimal xs:token"/></xs:simpleType>
<xs:simpleType name="FractionalFloat"><xs:restriction base="xs:float"><xs:pattern value="NaN|[0-9]+[.][0-9]+"/></xs:restriction></xs:simpleType>
<xs:simpleType name="FloatingFirst"><xs:union memberTypes="xs:int t:FractionalFloat xs:double xs:token"/></xs:simpleType>
<xs:simpleType name="IntegerPairs"><xs:restriction><xs:simpleType><xs:list itemType="xs:int"/></xs:simpleType><xs:length value="2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="BooleanPairs"><xs:restriction><xs:simpleType><xs:list itemType="xs:boolean"/></xs:simpleType><xs:length value="2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="ListFirst"><xs:union memberTypes="t:IntegerPairs t:BooleanPairs xs:token"/></xs:simpleType>
<xs:simpleType name="IntegerEnum"><xs:restriction base="t:IntegerFirst"><xs:enumeration value="01"/><xs:enumeration value="ready"/></xs:restriction></xs:simpleType>
<xs:simpleType name="StringEnum"><xs:restriction base="t:StringFirst"><xs:enumeration value="01"/></xs:restriction></xs:simpleType>
<xs:simpleType name="RawStringEnum"><xs:restriction base="t:RawStringFirst"><xs:enumeration value=" 01 "/></xs:restriction></xs:simpleType>
<xs:simpleType name="UriEnum"><xs:restriction base="t:UriFirst"><xs:enumeration value="a  b"/></xs:restriction></xs:simpleType>
<xs:simpleType name="BooleanEnum"><xs:restriction base="t:BooleanFirst"><xs:enumeration value="true"/><xs:enumeration value="02"/></xs:restriction></xs:simpleType>
<xs:simpleType name="DecimalEnum"><xs:restriction base="t:DecimalFirst"><xs:enumeration value="1.0"/></xs:restriction></xs:simpleType>
<xs:simpleType name="FloatingEnum"><xs:restriction base="t:FloatingFirst"><xs:enumeration value="1.0"/><xs:enumeration value="NaN"/></xs:restriction></xs:simpleType>
<xs:simpleType name="ListEnum"><xs:restriction base="t:ListFirst"><xs:enumeration value="1 2"/><xs:enumeration value="true false"/><xs:enumeration value="ready"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Nested"><xs:union memberTypes="t:IntegerEnum xs:token"/></xs:simpleType>
<xs:simpleType name="NestedEnum"><xs:restriction base="t:Nested"><xs:enumeration value="1.0"/></xs:restriction></xs:simpleType>
'''

UNION_FIXTURES = (
    ('integer_enum', 'IntegerEnum', None),
    ('string_enum', 'StringEnum', None),
    ('raw_string_enum', 'RawStringEnum', None),
    ('uri_enum', 'UriEnum', None),
    ('boolean_enum', 'BooleanEnum', None),
    ('decimal_enum', 'DecimalEnum', None),
    ('floating_enum', 'FloatingEnum', None),
    ('list_enum', 'ListEnum', None),
    ('nested_enum', 'NestedEnum', None),
    ('integer_fixed', 'IntegerFirst', '01'),
    ('string_fixed', 'StringFirst', '01'),
    ('raw_string_fixed', 'RawStringFirst', ' 01 '),
    ('list_fixed', 'ListFirst', 'true false'),
)


def document(root, attributes):
    """Construct exact attribute character values without XML whitespace normalization losses."""
    encoded = ' '.join(key + '=' + quoteattr(value, {'\n': '&#10;', '\r': '&#13;', '\t': '&#9;'})
                       for key, value in attributes.items())
    return ('<' + root + ' xmlns="' + NS + '" ' + encoded + '/>').encode('utf-8')


class recording_context:
    """Record logical resource charges without influencing generated scalar admission."""
    def __init__(self):
        """Start an independent complete-operation resource inventory."""
        self.counts = {}
    def consume(self, resource, count):
        """Accumulate the resource amount supplied by the codec."""
        self.counts[resource] = self.counts.get(resource, 0) + count
    def check(self):
        """Allow ordinary explicit provider-boundary checks."""


class python_xml_scalar_test(unittest.TestCase):
    """Compare real generated input/output against an independent XSD1.0 provider."""
    def test_numeric_and_unicode_boundaries(self):
        """Preserve exact integer/decimal boundaries, float precision, enums and code-point counts."""
        baseline = dict(large='18446744073709551614', signed='-9223372036854775807',
                        decimal='9007199254740993.000001', single='0.5',
                        extended='1', flag='true', word='é😀')
        missing_fixed = document('numbers', {key: value for key, value in baseline.items() if key != 'flag'})
        self.assertFalse(self.oracle.validate(self.provider.fromstring(missing_fixed)))
        with self.assertRaises(ValueError):
            self.module.ScalarsNumbers.decode_xml(missing_fixed)
        cases = {
            'large': ('18446744073709551613', '18446744073709551614', '18446744073709551615',
                      '9007199254740992', '+18446744073709551614', '-1'),
            'signed': ('-9223372036854775808', '-9223372036854775807', '9223372036854775807',
                       '9223372036854775808', '-0', '+0007'),
            'decimal': ('9007199254740993', '9007199254740993.000001',
                        '9007199254740993.000002', '9.007199254740993e15',
                        '  9007199254740993.000001  '),
            'single': ('0', '-0', '0.5', '1', '1.0000000001', '1.000001', 'NaN', 'INF', '-INF'),
            'extended': ('0', '1', '1.0000000000000002', 'NaN', 'INF', '-INF'),
            'flag': ('true', '1', 'false', '0', 'TRUE', '+1'),
            'word': ('é😀', 'aa', '😀😀', 'é', 'abc', 'é\u0301', 'zz', ''),
        }
        for key, values in cases.items():
            for value in values:
                data = document('numbers', dict(baseline, **{key: value}))
                expected = self.oracle.validate(self.provider.fromstring(data))
                # libxml2 rejects the legal leading '+' alias for unsignedLong; the codec preserves XSD value semantics.
                if key == 'large' and value.startswith('+'):
                    expected = True
                with self.subTest(field=key, value=value, expected=expected):
                    if expected:
                        model = self.module.ScalarsNumbers.decode_xml(data)
                        self.assertTrue(self.oracle.validate(self.provider.fromstring(model.encode_xml())))
                    else:
                        with self.assertRaises(ValueError):
                            self.module.ScalarsNumbers.decode_xml(data)
        model = self.module.ScalarsNumbers.decode_xml(document('numbers', baseline))
        model.large = 18446744073709551615
        with self.assertRaises(ValueError):
            model.encode_xml()
        model.large, model.word = 18446744073709551614, '😀'
        with self.assertRaises(ValueError):
            model.encode_xml()

    def test_portable_patterns_with_provider_unavailable(self):
        """Match the shared approved dialect exactly, including literal anchors and nullable cycles."""
        for index, (expression, values) in enumerate(PATTERNS[:8]):
            model_type = getattr(self.module, 'ScalarsP' + str(index))
            self.assertIsNotNone(model_type._xml_rules[0]['portable_patterns'][0])
            for value in values:
                data = document('p' + str(index), {'value': value})
                expected = self.oracle.validate(self.provider.fromstring(data))
                with self.subTest(expression=expression, value=value, expected=expected):
                    with mock.patch.dict(sys.modules, {'lxml': None}):
                        with mock.patch.object(self.module, '_xml_parse', side_effect=AssertionError('retained tree')):
                            if expected:
                                model = model_type.decode_xml_chunks(data[i:i+1] for i in range(len(data)))
                                self.assertEqual(model.value, value)
                                self.assertTrue(self.oracle.validate(self.provider.fromstring(model.encode_xml())))
                            else:
                                with self.assertRaises(ValueError):
                                    model_type.decode_xml(data)
        with ThreadPoolExecutor(max_workers=4) as pool:
            values = list(pool.map(lambda unused: self.module.ScalarsP2.decode_xml(
                document('p2', {'value': 'abé😀'})).value, range(24)))
        self.assertEqual(values, ['abé😀'] * 24)

    def test_outer_union_value_constraints(self):
        """Compare ordered member identities on input/output, including booleans and list values."""
        cases = {
            'integer_enum': (('01', '1', '+001', ' ready '), ('2', '1.0', 'READY', '')),
            'string_enum': (('01', ' 01 '), ('1', '+001', '1.0', '')),
            'raw_string_enum': ((' 01 ',), ('01', '  01  ', '1', '')),
            'uri_enum': (('a  b', ' a  b '), ('a b', 'ab', '')),
            'boolean_enum': (('true', '1', '2', '+02'), ('false', '0', '+1', '01', 'ready')),
            'decimal_enum': (('1', '+001', '1.0', '01.000'), ('1e0', '2', 'ready', '')),
            'floating_enum': (('1.0', '01.000', 'NaN'), ('1', '1e0', '2.0', 'ready', '')),
            'list_enum': (('1 2', '+01 002', 'true false', 'ready'),
                          ('1 3', 'false true', '1 0', '1', '')),
            'nested_enum': (('1.0', ' 1.0 '), ('1', '01', 'ready', '2')),
            'integer_fixed': (('01', '1', '+001'), ('2', '1.0', 'ready', '')),
            'string_fixed': (('01', ' 01 '), ('1', '+001', 'ready', '')),
            'raw_string_fixed': ((' 01 ',), ('01', '  01  ', '1', '')),
            'list_fixed': (('true false', ' true  false '), ('false true', '1 0', '1 2', 'true', '')),
        }
        oracle_probes = 0
        for name, (accepted, rejected) in cases.items():
            class_name = 'Scalars' + ''.join(piece.title() for piece in name.split('_'))
            model_type = getattr(self.module, class_name)
            for expected, values in ((True, accepted), (False, rejected)):
                for value in values:
                    oracle_probes += 1
                    data = document(name, {'value': value})
                    with self.subTest(name=name, value=value, expected=expected):
                        self.assertEqual(self.oracle.validate(self.provider.fromstring(data)), expected)
                        if expected:
                            with mock.patch.dict(sys.modules, {'lxml': None}):
                                model = model_type.decode_xml_chunks(data[i:i+1] for i in range(len(data)))
                                encoded = model.encode_xml()
                            self.assertTrue(self.oracle.validate(self.provider.fromstring(encoded)))
                            self.assertEqual(model_type.decode_xml(encoded).value, model.value)
                        else:
                            with self.assertRaises(ValueError):
                                model_type.decode_xml(data)
                        candidate = model_type()
                        candidate.value = value
                        if expected:
                            self.assertTrue(self.oracle.validate(self.provider.fromstring(candidate.encode_xml())))
                        else:
                            with self.assertRaises(ValueError):
                                candidate.encode_xml()
            if name.endswith('_fixed'):
                data = document(name, {})
                self.assertTrue(self.oracle.validate(self.provider.fromstring(data)))
                candidate = model_type.decode_xml(data)
                self.assertTrue(self.oracle.validate(self.provider.fromstring(candidate.encode_xml())))
        print('PASS:', oracle_probes, 'ordered union lexical decisions match the independent XSD oracle')

    def test_outer_union_comparison_preserves_callback_failures(self):
        """Keep application failures visible while selecting an enumeration or fixed-value member."""
        rule = self.module.ScalarsIntegerEnum._xml_rules[0]
        marker = ValueError('union comparison callback marker')
        class rejecting_context:
            """Reject a comparison inside selected-member validation."""
            def __init__(self):
                """Permit the comparison's initial charge before rejecting member validation."""
                self.calls = 0
            def consume(self, resource, count):
                """Raise the caller's original exception unchanged."""
                self.calls += 1
                if self.calls == 2:
                    raise marker
        with self.assertRaises(ValueError) as failure:
            self.module._xml_value_equal('1', '01', rule, {}, rejecting_context())
        self.assertIs(failure.exception, marker)
        limits = self.module.Limits(xml_pattern=self.module.XmlPatternLimits(max_work_units=0))
        data = document('floating_enum', {'value': '1.0'})
        with self.assertRaisesRegex(ValueError, 'pattern'):
            self.module.ScalarsFloatingEnum.decode_xml(data, limits)
        candidate = self.module.ScalarsFloatingEnum()
        candidate.value = '1.0'
        with self.assertRaisesRegex(ValueError, 'pattern'):
            candidate.encode_xml(limits)

    def test_outer_union_child_fixed_and_default_values(self):
        """Apply child constraints before publishing candidates and retain empty-element fallbacks."""
        model_type = self.module.ScalarsUnionChildren
        def child_document(value='1', fixed='01', default=''):
            """Build a qualified sequence whose fixed/default values select different member spaces."""
            return ('<union_children xmlns="' + NS + '"><value>' + value +
                    '</value><fixed_value>' + fixed + '</fixed_value><default_value>' + default +
                    '</default_value></union_children>').encode()
        for data in (child_document(), child_document('+001', '', '2'),
                     child_document('ready', '01', 'ready')):
            self.assertTrue(self.oracle.validate(self.provider.fromstring(data)))
            candidate = model_type.decode_xml(data)
            self.assertEqual(candidate.fixed_value, '01')
            self.assertTrue(self.oracle.validate(self.provider.fromstring(candidate.encode_xml())))
            if b'<default_value></default_value>' in data:
                self.assertEqual(candidate.default_value, '01')
        for data in (child_document('2'), child_document('1', '1')):
            self.assertFalse(self.oracle.validate(self.provider.fromstring(data)))
            with self.assertRaises(ValueError):
                model_type.decode_xml(data)
        candidate = model_type.decode_xml(child_document())
        for member, value in (('value', '2'), ('fixed_value', '1')):
            previous = getattr(candidate, member)
            setattr(candidate, member, value)
            with self.assertRaises(ValueError):
                candidate.encode_xml()
            setattr(candidate, member, previous)

    def test_legacy_patterns_retain_explicit_provider_requirement(self):
        """Keep category escapes and class subtraction on the existing genuine XSD provider path."""
        for index, (expression, values) in enumerate(PATTERNS[8:], 8):
            model_type = getattr(self.module, 'ScalarsP' + str(index))
            self.assertIsNone(model_type._xml_rules[0]['portable_patterns'][0])
            for value in values:
                data = document('p' + str(index), {'value': value})
                expected = self.oracle.validate(self.provider.fromstring(data))
                with self.subTest(expression=expression, value=value, expected=expected):
                    if expected:
                        self.assertEqual(model_type.decode_xml(data).value, value)
                    else:
                        with self.assertRaises(ValueError):
                            model_type.decode_xml(data)
        self.module._XML_PATTERN_SCHEMAS.clear()
        with mock.patch.dict(sys.modules, {'lxml': None}):
            with self.assertRaisesRegex(RuntimeError, 'lxml'):
                self.module.ScalarsP8.decode_xml(document('p8', {'value': 'abc'}))

    def test_exact_pattern_limits_and_callback_failures(self):
        """Check byte/state/work equality, reject one-less admission and preserve callback exceptions."""
        module = self.module
        program = module.ScalarsP0._xml_rules[0]['portable_patterns'][0][0]
        context = recording_context()
        self.assertTrue(module._xml_portable_pattern('AB', program, context))
        work = context.counts['work_units']
        policy = module.XmlPatternLimits(max_states=len(program[1]), max_input_bytes=2, max_work_units=work)
        data = document('p0', {'value': 'AB'})
        value = module.ScalarsP0.decode_xml(data, module.Limits(xml_pattern=policy))
        self.assertEqual(value.value, 'AB')
        self.assertEqual(module.ScalarsP0.decode_xml(value.encode_xml(module.Limits(xml_pattern=policy))).value, 'AB')
        for arguments in ({'max_states': len(program[1]) - 1}, {'max_input_bytes': 1},
                          {'max_work_units': work - 1}, {'max_work_units': 0}):
            with self.subTest(arguments=arguments):
                with self.assertRaisesRegex(ValueError, 'pattern'):
                    module.ScalarsP0.decode_xml(data, module.Limits(xml_pattern=module.XmlPatternLimits(**arguments)))
        unicode_data = document('p2', {'value': 'é'})
        self.assertEqual(module.ScalarsP2.decode_xml(unicode_data,
            module.Limits(xml_pattern=module.XmlPatternLimits(max_input_bytes=2))).value, 'é')
        with self.assertRaises(ValueError):
            module.ScalarsP2.decode_xml(unicode_data,
                module.Limits(xml_pattern=module.XmlPatternLimits(max_input_bytes=1)))
        with self.assertRaises(ValueError):
            module.XmlPatternLimits(max_work_units=-1)
        marker = ValueError('application callback marker')
        class rejecting_context:
            """Model an application interruption during pattern matching itself."""
            def consume(self, resource, count):
                """Raise the original callback exception unchanged."""
                raise marker
        with self.assertRaises(ValueError) as failure:
            module._xml_portable_pattern('AB', program, rejecting_context())
        self.assertIs(failure.exception, marker)
        with self.assertRaises(ValueError):
            module.ScalarsP0.decode_xml(data, module.Limits(xml_pattern=object()))


def main():
    """Generate owning fixtures through the real compiler and require an actual independent provider."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--python-dependencies', type=Path)
    args = parser.parse_args()
    if args.python_dependencies:
        sys.path.insert(0, str(args.python_dependencies.resolve()))
    from lxml import etree as provider
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-xml-scalars-', dir=build) as temporary:
        directory = Path(temporary)
        source = directory / 'scalars.serializer'
        schema = directory / 'scalars.xsd'
        native = '''serializer version 1.20.0; namespace scalars { using schema "scalars.xsd";
        class numbers { public uint64 large(1); public int64 signed(2); public string decimal(3);
        public float single(4); public double extended(5); public bool flag(6); public string word(7); }
        '''
        xsd = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="'''+NS+'''" targetNamespace="'''+NS+'''">
        <xs:simpleType name="Large"><xs:restriction base="xs:unsignedLong"><xs:minInclusive value="18446744073709551613"/><xs:maxExclusive value="18446744073709551615"/></xs:restriction></xs:simpleType>
        <xs:simpleType name="Signed"><xs:restriction base="xs:long"><xs:minExclusive value="-9223372036854775808"/><xs:maxInclusive value="9223372036854775807"/></xs:restriction></xs:simpleType>
        <xs:simpleType name="Exact"><xs:restriction base="xs:decimal"><xs:minExclusive value="9007199254740993"/><xs:maxInclusive value="9007199254740993.000001"/></xs:restriction></xs:simpleType>
        <xs:simpleType name="Single"><xs:restriction base="xs:float"><xs:minExclusive value="0"/><xs:maxInclusive value="1"/></xs:restriction></xs:simpleType>
        <xs:simpleType name="Extended"><xs:restriction base="xs:double"><xs:minInclusive value="0"/><xs:maxExclusive value="1.0000000000000002"/></xs:restriction></xs:simpleType>
        <xs:simpleType name="Word"><xs:restriction base="xs:string"><xs:length value="2"/>
        <xs:enumeration value="é😀"/><xs:enumeration value="aa"/><xs:enumeration value="😀😀"/><xs:enumeration value="é́"/>
        </xs:restriction></xs:simpleType>
        <xs:element name="numbers"><xs:complexType>
        <xs:attribute name="large" type="t:Large" use="required"/><xs:attribute name="signed" type="t:Signed" use="required"/>
        <xs:attribute name="decimal" type="t:Exact" use="required"/><xs:attribute name="single" type="t:Single" use="required"/>
        <xs:attribute name="extended" type="t:Extended" use="required"/><xs:attribute name="flag" type="xs:boolean" fixed="true" use="required"/>
        <xs:attribute name="word" type="t:Word" use="required"/></xs:complexType></xs:element>'''
        for index, (expression, unused) in enumerate(PATTERNS):
            native += 'class p' + str(index) + ' { public string value(1); }\n'
            xsd += '<xs:element name="p' + str(index) + '"><xs:complexType><xs:attribute name="value" use="required"><xs:simpleType><xs:restriction base="xs:string"><xs:pattern value=' + quoteattr(expression) + '/></xs:restriction></xs:simpleType></xs:attribute></xs:complexType></xs:element>'
        xsd += UNION_TYPES
        for name, selected_type, fixed in UNION_FIXTURES:
            native += 'class ' + name + ' {\n  public string value(1);\n}\n'
            xsd += '<xs:element name="' + name + '"><xs:complexType><xs:attribute name="value" type="t:' + selected_type + '"'
            xsd += ' use="required"' if fixed is None else ' fixed=' + quoteattr(fixed)
            xsd += '/></xs:complexType></xs:element>'
        native += '''class union_children {
  public string value;
  public string fixed_value;
  public string default_value;
}
'''
        xsd += '''<xs:element name="union_children"><xs:complexType><xs:sequence>
          <xs:element name="value" form="qualified" type="t:IntegerEnum"/>
          <xs:element name="fixed_value" form="qualified" type="t:StringFirst" fixed="01"/>
          <xs:element name="default_value" form="qualified" type="t:IntegerFirst" default="01"/>
          </xs:sequence></xs:complexType></xs:element>'''
        source.write_text(native + '}', encoding='utf-8')
        schema.write_text(xsd + '</xs:schema>', encoding='utf-8')
        oracle = provider.XMLSchema(provider.parse(str(schema)))
        output = directory / 'scalars.py'
        subprocess.run([str(args.compiler.resolve()), '--language', 'python', '--input', str(source),
                        '--python.output', str(output)], check=True)
        spec = importlib.util.spec_from_file_location('serializer_python_xml_scalar_fixture', output)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        python_xml_scalar_test.module, python_xml_scalar_test.provider = module, provider
        python_xml_scalar_test.oracle = oracle
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_xml_scalar_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
