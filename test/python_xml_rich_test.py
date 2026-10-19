"""Qualify generated Python XSD facets, QName state, lists and positioned extension handling."""
import argparse
import importlib.util
import io
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as etree

ROOT = Path(__file__).resolve().parent.parent
NS = 'urn:serializer:xml-rich'
DOCUMENT = f'''<document xmlns="{NS}" xmlns:n="urn:serializer:names" xmlns:v="urn:vendor"
 transform="1 0 0" required="n" v:flag="kept"><number>18446744073709551614</number>
 <v:extra v:label="kept">head<v:child/>tail</v:extra>
 <metadata name="n:thing" v:detail="preserved">text</metadata></document>'''.encode()


def load_module(path, name):
    """Import one real compiler-produced module with its standard Python module identity."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class python_xml_rich_test(unittest.TestCase):
    """Exercise generated methods directly; optional presence and hidden XML state remain explicit."""
    def test_facets_defaults_lists_qnames_and_semantic_extensions(self):
        """Round-trip expanded QName identities and mixed foreign subtrees through edited known values."""
        model = self.module.XmlRichDocument.decode_xml(DOCUMENT)
        self.assertEqual(model.number, (18446744073709551614,))
        self.assertEqual(model.unit, ('millimeter',))
        self.assertEqual(model.mode, ('model',))
        self.assertEqual(model.transform, [1.0, 0.0, 0.0])
        self.assertEqual(model.optional_transform, ())
        self.assertTrue(model.data[0].category)
        native = model.encode(self.module.Protocol.BINARY_INTEGER)
        model.xml_bind_namespace('presentation', 'urn:serializer:names')
        self.assertEqual(model.encode(self.module.Protocol.BINARY_INTEGER), native)
        model.unit = ('inch',)
        encoded = model.encode_xml()
        independent = etree.fromstring(encoded)
        self.assertEqual(independent.attrib['unit'], 'inch')
        self.assertEqual(independent.attrib['{urn:vendor}flag'], 'kept')
        extension = independent.find('{urn:vendor}extra')
        self.assertEqual(extension.text, 'head')
        self.assertEqual(extension[0].tail, 'tail')
        self.assertEqual(self.module.XmlRichDocument.decode_xml(encoded).number, model.number)
        padded = DOCUMENT.replace(b'18446744073709551614', b'0' * 5000 + b'18446744073709551614')
        self.assertEqual(self.module.XmlRichDocument.decode_xml(padded).number, model.number)

    def test_invalid_values_choices_and_wildcard_positions_reject(self):
        """Reject exact numeric/length/fixed/QName failures and unsupported sequence placements."""
        bad = (
            DOCUMENT.replace(b'18446744073709551614', b'18446744073709551615'),
            DOCUMENT.replace(b'transform="1 0 0"', b'transform="1 0"'),
            DOCUMENT.replace(b'transform="1 0 0"', b'transform="1 0 0" mode="other"'),
            DOCUMENT.replace(b'name="n:thing"', b'name="n:other"'),
            DOCUMENT.replace(b'required="n"', b'required="unbound"'),
            DOCUMENT.replace(b'</number>', b'</number><label>x</label>'),
            DOCUMENT.replace(b'<number>18446744073709551614</number>', b''),
            DOCUMENT.replace(b'<metadata', b'<v:one/><v:two/><metadata'),
            DOCUMENT.replace(b'<number>', b'<v:before/><number>'),
        )
        for data in bad:
            with self.subTest(data=data):
                with self.assertRaises(ValueError): self.module.XmlRichDocument.decode_xml(data)
        normalized = DOCUMENT.replace(b'<number>18446744073709551614</number>', b'<label>  a&#9;b  </label>')
        self.assertEqual(self.module.XmlRichDocument.decode_xml(normalized).label, ('a b',))

    def test_optional_list_absence_is_not_a_zero_filled_list(self):
        """Keep omitted optional lists absent and validate present lists in both directions."""
        model = self.module.XmlRichDocument.decode_xml(DOCUMENT)
        self.assertEqual(model.optional_transform, ())
        model.optional_transform = ([3.0, 2.0, 1.0],)
        self.assertEqual(self.module.XmlRichDocument.decode_xml(model.encode_xml()).optional_transform,
                         ([3.0, 2.0, 1.0],))
        model.optional_transform = ([1.0],)
        with self.assertRaises(ValueError): model.encode_xml()

    def test_real_xsd_patterns_unions_and_missing_provider(self):
        """Use libxml2 XSD regex semantics, inherited pattern groups and exact union branches."""
        for value in ('BBB', 'CCC', '4294967295'):
            data = ('<sample xmlns="urn:pattern" code="' + value + '"/>').encode()
            self.assertEqual(self.patterns.PatternsSample.decode_xml(data).code, value)
        for value in ('EEE', 'aaa', '4294967296'):
            with self.assertRaises(ValueError):
                self.patterns.PatternsSample.decode_xml(('<sample xmlns="urn:pattern" code="' + value + '"/>').encode())
        self.patterns._XML_PATTERN_SCHEMAS.clear()
        with mock.patch.dict(sys.modules, {'lxml': None}):
            with self.assertRaisesRegex(RuntimeError, 'lxml'):
                self.patterns.PatternsSample.decode_xml(b'<sample xmlns="urn:pattern" code="BBB"/>')

    def test_core_lexical_kinds_match_independent_xsd_provider(self):
        """Check XML Name character rules and the Core language-or-empty union independently."""
        from lxml import etree as provider
        prepared = provider.XMLSchema(provider.fromstring(self.lexical_xsd.encode()))
        cases = {
            'language': ('en-US', 'abcdefgh-12345678', '  en&#9; ', 'en_US', 'abcdefghi', 'en--US', ''),
            'name': (':', '::é:thing', 'é:a', 'a:1', '1a', '-a', 'a b'),
            'token': ('1', ':', '.-:9', 'é&#x300;', '', 'a b', '&#x300;a'),
            'tokens': ('1 : .-:9', '  a&#9;b  ', '', 'a b/c', 'a&#xA0;b'),
            'ncname': ('é_thing', 'a&#x300;', ':', 'a:b', '1a', 'a b'),
            'empty_language': ('', 'en', 'en_US'),
        }
        defaults = {'language': 'en', 'name': 'a', 'token': 'a', 'tokens': 'a', 'ncname': 'a', 'empty_language': ''}
        for field, values in cases.items():
            for value in values:
                attrs = dict(defaults, **{field: value})
                data = ('<sample xmlns="urn:lexical" ' + ' '.join(key + '="' + item + '"' for key, item in attrs.items()) + '/>').encode()
                expected = prepared.validate(provider.fromstring(data))
                # System.Xml rejects empty NMTOKENS, as required by its normative minLength1.
                # libxml2 2.11.9 wrongly admits this one empty built-in-list case.
                if field == 'tokens' and not value: expected = False
                with self.subTest(field=field, value=value, expected=expected):
                    if expected:
                        model = self.lexical.LexicalSample.decode_xml(data)
                        self.assertTrue(prepared.validate(provider.fromstring(model.encode_xml())))
                    else:
                        with self.assertRaises(ValueError): self.lexical.LexicalSample.decode_xml(data)

    def test_union_does_not_swallow_caller_resource_failure(self):
        """Preserve a caller's exception rather than selecting another permissive union branch."""
        class context:
            """Fail once from a nested lexical branch while allowing parser admission."""
            def __init__(self): self.failed, self.hits = False, 0
            def consume(self, resource, amount):
                if resource == 'work_units' and amount == 4: self.hits += 1
                if not self.failed and self.hits == 2:
                    self.failed = True
                    raise ValueError('one-shot caller charge failure')
            def check(self): pass
        selected = context()
        with self.assertRaisesRegex(ValueError, 'one-shot caller charge failure'):
            self.patterns.PatternsSample.decode_xml(b'<sample xmlns="urn:pattern" code="BBB"/>', context=selected)
        self.assertTrue(selected.failed)

    def test_numeric_string_projection_preserves_xsd_value_identity(self):
        """Compare numeric enums/fixed values after lexical validation rather than as string spelling."""
        valid = b'<sample xmlns="urn:serializer:numeric-projection" integer="042" decimal="+1.00" fixed="+01"/>'
        model = self.numeric.XmlNumericProjectionSample.decode_xml(valid)
        self.assertEqual(model.integer, '042')
        self.assertEqual(model.decimal, '+1.00')
        self.assertEqual(model.fixed, '+01')
        self.assertEqual(self.numeric.XmlNumericProjectionSample.decode_xml(model.encode_xml()).fixed, '+01')
        for bad in (valid.replace(b'042', b'0'), valid.replace(b'042', b'43'),
                    valid.replace(b'+1.00', b'1e0'), valid.replace(b'+01', b'2')):
            with self.assertRaises(ValueError): self.numeric.XmlNumericProjectionSample.decode_xml(bad)
        model.fixed = '2'
        with self.assertRaises(ValueError): model.encode_xml()

    def test_floating_values_use_selected_width_nan_and_order_semantics(self):
        """Round both values and facets to their declared IEEE widths, including unordered NaN cases."""
        selected = self.numeric.XmlNumericProjectionFloatingSample
        valid = {'float_enum': '0.1000000001', 'double_enum': '0.100000000000000001',
                 'float_min_inclusive': '0.1', 'float_max_inclusive': '0.1',
                 'double_min_inclusive': '0.1', 'double_max_inclusive': '0.1',
                 'float_min_exclusive': '0.2', 'float_max_exclusive': '0.05',
                 'double_min_exclusive': '0.2', 'double_max_exclusive': '0.05',
                 'nan_min_inclusive': 'NaN', 'nan_max_inclusive': 'NaN',
                 'float_nan_enum': 'NaN', 'double_nan_enum': 'NaN',
                 'float_nan_fixed': 'NaN', 'double_nan_fixed': 'NaN',
                 'float_union': '', 'double_union': '0.05'}
        data = ('<floating_sample xmlns="urn:serializer:numeric-projection" ' +
                ' '.join(key + '="' + value + '"' for key, value in valid.items()) + '/>').encode()
        model = selected.decode_xml(data)
        self.assertTrue(math.isnan(model.float_nan_enum[0]))
        self.assertTrue(math.isnan(model.double_nan_fixed[0]))
        self.assertEqual(selected.decode_xml(model.encode_xml()).float_enum, model.float_enum)
        bad = {'float_enum': '0.10000001', 'double_enum': '0.1000000001',
               'float_min_exclusive': '0.1', 'float_max_exclusive': '0.1',
               'double_min_exclusive': '0.1', 'double_max_exclusive': '0.1',
               'nan_min_inclusive': '0', 'nan_max_inclusive': '0',
               'nan_min_exclusive': 'NaN', 'nan_max_exclusive': 'NaN',
               'float_nan_enum': '0', 'double_nan_fixed': '0',
               'float_min_inclusive': 'NaN', 'double_max_inclusive': 'NaN',
               'float_union': '0.1', 'double_union': '0.1'}
        for field, value in bad.items():
            with self.subTest(field=field, value=value):
                invalid = ('<floating_sample xmlns="urn:serializer:numeric-projection" ' + field + '="' + value + '"/>').encode()
                with self.assertRaises(ValueError): selected.decode_xml(invalid)
        model.float_max_exclusive = (model.float_enum[0],)
        with self.assertRaises(ValueError): model.encode_xml()

    def test_floating_list_equality_and_decimal_midpoint_rounding(self):
        """Compare typed list items by value and avoid float64's rare second-rounding error."""
        data = b'<sample xmlns="urn:floating-list" values="0.1000000001 NaN" fixed_values="0.1000000001 2" midpoint="1.0000000596046448"/>'
        model = self.floating_lists.FloatingListsSample.decode_xml(data)
        self.assertEqual(model.midpoint, (1.0000001192092896,))
        self.assertEqual(len(model.values), 2)
        self.assertTrue(math.isnan(model.values[1]))
        self.floating_lists.FloatingListsSample.decode_xml(model.encode_xml())
        for invalid in (data.replace(b'0.1000000001 NaN', b'0.10000001 NaN'),
                        data.replace(b'0.1000000001 2', b'0.1000000001 3')):
            with self.assertRaises(ValueError): self.floating_lists.FloatingListsSample.decode_xml(invalid)

    def test_qname_children_keep_conflicting_lexical_prefix_scopes(self):
        """Resolve child-local QName declarations before promoting identities into owning XML state."""
        data = b'<scope_sample xmlns="urn:scoped" xmlns:p="urn:parent"><left xmlns:p="urn:left">p:item</left><right xmlns:p="urn:right">p:item</right></scope_sample>'
        model = self.scoped.ScopedScopeSample.decode_xml(data)
        output = model.encode_xml()
        namespaces = dict(item for unused, item in etree.iterparse(io.BytesIO(output), events=('start-ns',)))
        document = etree.fromstring(output)
        for child, expected in zip(document, ('urn:left', 'urn:right')):
            prefix, local = child.text.split(':')
            self.assertEqual((namespaces[prefix], local), (expected, 'item'))
        self.scoped.ScopedScopeSample.decode_xml(output)

    def test_typed_numeric_patterns_validate_original_lexical_input(self):
        """Check original scalar/list spellings before numeric parsing canonicalizes their values."""
        data = b'<pattern_sample xmlns="urn:scoped" code="01" numbers="01 02"/>'
        model = self.scoped.ScopedPatternSample.decode_xml(data)
        self.assertEqual(model.code, 1)
        self.assertEqual(model.numbers, [1, 2])
        for invalid in (data.replace(b'code="01"', b'code="1"'), data.replace(b'01 02', b'1 2')):
            with self.assertRaises(ValueError): self.scoped.ScopedPatternSample.decode_xml(invalid)
        # The model's canonical numeric formatter cannot satisfy this authored lexical contract.
        with self.assertRaises(ValueError): model.encode_xml()


def main():
    """Generate rich fixtures and run with an explicit optional XMLSchema provider installation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--python-dependencies', type=Path)
    args = parser.parse_args()
    if args.python_dependencies: sys.path.insert(0, str(args.python_dependencies.resolve()))
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='python-xml-rich-', dir=build) as temporary:
        directory = Path(temporary)
        generated = directory / 'rich.py'
        command = [str(args.compiler.resolve()), '--language', 'python']
        subprocess.run([*command, '--input', str(ROOT / 'test/resources/xml_rich.serializer'), '--python.output', str(generated)], check=True)
        python_xml_rich_test.module = load_module(generated, 'serializer_python_xml_rich_fixture')
        source = directory / 'patterns.serializer'
        source.write_text('serializer version 1.20.0; namespace patterns { using schema "patterns.xsd"; class sample { public string code (1); }}', encoding='utf-8')
        (directory / 'patterns.xsd').write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:pattern" targetNamespace="urn:pattern">
          <xs:simpleType name="Base"><xs:restriction base="xs:string"><xs:pattern value="[A-Z]+"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Code"><xs:restriction base="t:Base"><xs:pattern value="[AB]+"/><xs:pattern value="[CD]+"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Either"><xs:union memberTypes="t:Code xs:unsignedInt"/></xs:simpleType>
          <xs:element name="sample"><xs:complexType><xs:attribute name="code" type="t:Either" use="required"/></xs:complexType></xs:element>
        </xs:schema>''', encoding='utf-8')
        pattern_module = directory / 'patterns.py'
        subprocess.run([*command, '--input', str(source), '--python.output', str(pattern_module)], check=True)
        python_xml_rich_test.patterns = load_module(pattern_module, 'serializer_python_pattern_fixture')
        lexical_source = directory / 'lexical.serializer'
        lexical_source.write_text('''serializer version 1.20.0; namespace lexical { using schema "lexical.xsd";
          class sample { public string language(1); public string name(2); public string token(3);
          public array<string> tokens(4) xml(attribute, list); public string ncname(5); public string empty_language(6); }}''', encoding='utf-8')
        python_xml_rich_test.lexical_xsd = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:lexical" targetNamespace="urn:lexical">
          <xs:simpleType name="Empty"><xs:restriction base="xs:string"><xs:length value="0"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Language"><xs:union memberTypes="xs:language t:Empty"/></xs:simpleType>
          <xs:element name="sample"><xs:complexType>
            <xs:attribute name="language" type="xs:language" use="required"/><xs:attribute name="name" type="xs:Name" use="required"/>
            <xs:attribute name="token" type="xs:NMTOKEN" use="required"/><xs:attribute name="tokens" type="xs:NMTOKENS" use="required"/>
            <xs:attribute name="ncname" type="xs:NCName" use="required"/><xs:attribute name="empty_language" type="t:Language" use="required"/>
          </xs:complexType></xs:element></xs:schema>'''
        (directory / 'lexical.xsd').write_text(python_xml_rich_test.lexical_xsd, encoding='utf-8')
        lexical_module = directory / 'lexical.py'
        subprocess.run([*command, '--input', str(lexical_source), '--python.output', str(lexical_module)], check=True)
        python_xml_rich_test.lexical = load_module(lexical_module, 'serializer_python_lexical_fixture')
        numeric_module = directory / 'numeric.py'
        subprocess.run([*command, '--input', str(ROOT / 'test/resources/xml_numeric_projection.serializer'),
                        '--python.output', str(numeric_module)], check=True)
        python_xml_rich_test.numeric = load_module(numeric_module, 'serializer_python_numeric_fixture')
        floating_source = directory / 'floating_lists.serializer'
        floating_source.write_text('''serializer version 1.20.0; namespace floating_lists { using schema "floating_lists.xsd";
          class sample { public array<float> values(1) xml(attribute,list);
          public optional<array<float>> fixed_values(2) xml(attribute,list); public optional<float> midpoint(3); }}''', encoding='utf-8')
        (directory / 'floating_lists.xsd').write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:floating-list" targetNamespace="urn:floating-list">
          <xs:simpleType name="FloatList"><xs:list itemType="xs:float"/></xs:simpleType>
          <xs:simpleType name="EnumList"><xs:restriction base="t:FloatList"><xs:enumeration value="0.1 NaN"/></xs:restriction></xs:simpleType>
          <xs:element name="sample"><xs:complexType><xs:attribute name="values" type="t:EnumList" use="required"/>
          <xs:attribute name="fixed_values" type="t:FloatList" fixed="0.1 2"/><xs:attribute name="midpoint" type="xs:float"/></xs:complexType></xs:element></xs:schema>''', encoding='utf-8')
        floating_module = directory / 'floating_lists.py'
        subprocess.run([*command, '--input', str(floating_source), '--python.output', str(floating_module)], check=True)
        python_xml_rich_test.floating_lists = load_module(floating_module, 'serializer_python_floating_lists_fixture')
        scoped_source = directory / 'scoped.serializer'
        scoped_source.write_text('''serializer version 1.20.0; namespace scoped { using schema "scoped.xsd";
          class scope_sample { public string left(1); public string right(2); }
          class pattern_sample { public int32 code(1); public array<int32> numbers(2) xml(attribute,list); }}''', encoding='utf-8')
        (directory / 'scoped.xsd').write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:scoped"
          xmlns:left="urn:left" xmlns:right="urn:right" targetNamespace="urn:scoped" elementFormDefault="qualified">
          <xs:simpleType name="Left"><xs:restriction base="xs:QName"><xs:enumeration value="left:item"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Right"><xs:restriction base="xs:QName"><xs:enumeration value="right:item"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Code"><xs:restriction base="xs:int"><xs:pattern value="[0-9]{2}"/></xs:restriction></xs:simpleType>
          <xs:simpleType name="Numbers"><xs:restriction><xs:simpleType><xs:list itemType="xs:int"/></xs:simpleType><xs:pattern value="0[0-9] 0[0-9]"/></xs:restriction></xs:simpleType>
          <xs:element name="scope_sample"><xs:complexType><xs:sequence><xs:element name="left" type="t:Left"/><xs:element name="right" type="t:Right"/></xs:sequence></xs:complexType></xs:element>
          <xs:element name="pattern_sample"><xs:complexType><xs:attribute name="code" type="t:Code" use="required"/><xs:attribute name="numbers" type="t:Numbers" use="required"/></xs:complexType></xs:element></xs:schema>''', encoding='utf-8')
        scoped_module = directory / 'scoped.py'
        subprocess.run([*command, '--input', str(scoped_source), '--python.output', str(scoped_module)], check=True)
        python_xml_rich_test.scoped = load_module(scoped_module, 'serializer_python_scoped_fixture')
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_xml_rich_test))
        if not result.wasSuccessful(): raise SystemExit(1)


if __name__ == '__main__': main()
