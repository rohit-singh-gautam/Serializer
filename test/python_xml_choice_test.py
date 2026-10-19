"""Qualify compiler-generated Python repeated choices against independent XSD acceptance data."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as etree

ROOT = Path(__file__).resolve().parent.parent
# System.Xml XmlSchemaSet 4.0.0.0 independently validated all binary words of length0..7.
# Bit(2**length-1+word) records acceptance, with the least-significant child first.
# These match the C++ oracle; libxml2 over-admits excess repetitions for these content models.
ORACLES = (
    ((0, 2, 1, 2, 1, 3), 'ffed446840000048000000000000004000000000000000000000000000000000'),
    ((1, 2, 0, 2, 1, 2), 'ffed044800000000000000000000000000000000000000000000000000000000'),
    ((2, 3, 2, 3, 1, 2), '40c985c94d8085c9cd480080048085c885488000000000800000008004008000'),
    ((3, 3, 0, 1, 0, 2), 'ff7f647c00400064000000000000004000000000000000000000000000000000'),
    ((1, 1, 2, 3, 2, 3), 'c840000000000000000000000000000000000000000000000000000000000000'),
)


def load_module(path, name):
    """Import one actual compiler output, keeping its model module identity registered."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def schema(bounds, after=''):
    """Build independently readable choice schemas with exact branch and group occurrences."""
    minimum, maximum, known_minimum, known_maximum, foreign_minimum, foreign_maximum = bounds
    return f'''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
      targetNamespace="urn:choice" elementFormDefault="qualified">
      <xs:element name="root"><xs:complexType><xs:sequence>
      <xs:choice minOccurs="{minimum}" maxOccurs="{maximum}">
        <xs:element name="item" type="xs:string" minOccurs="{known_minimum}" maxOccurs="{known_maximum}"/>
        <xs:any namespace="##other" processContents="lax" minOccurs="{foreign_minimum}" maxOccurs="{foreign_maximum}"/>
      </xs:choice>{after}</xs:sequence></xs:complexType></xs:element></xs:schema>'''


class python_xml_choice_test(unittest.TestCase):
    """Exercise decode, publication validation and preservation using real generated model classes."""
    def test_exhaustive_repeated_choices_match_independent_oracle(self):
        """Validate all1,275 short ordered branch sequences, including empty-capable groups."""
        for case, ((bounds, encoded), module) in enumerate(zip(ORACLES, self.modules)):
            accepted = bytes.fromhex(encoded)
            for length in range(8):
                for word in range(1 << length):
                    children = ['<e:extra/>' if word & (1 << index) else '<item>' + str(index) + '</item>' for index in range(length)]
                    data = ('<root xmlns="urn:choice" xmlns:e="urn:foreign">' + ''.join(children) + '</root>').encode()
                    offset = (1 << length) - 1 + word
                    expected = bool(accepted[offset // 8] & (1 << (offset % 8)))
                    with self.subTest(case=case, bounds=bounds, length=length, word=word):
                        if not expected:
                            with self.assertRaises(ValueError): module.RepeatedRoot.decode_xml(data)
                            continue
                        model = module.RepeatedRoot.decode_xml(data)
                        output = model.encode_xml()
                        independent = etree.fromstring(output)
                        self.assertEqual([child.tag for child in independent],
                                         ['{urn:foreign}extra' if word & (1 << index) else '{urn:choice}item' for index in range(length)])
                        self.assertEqual(module.RepeatedRoot.decode_xml(output).item, model.item)

    def test_repeated_choice_retains_item_interleaving_and_validates_edits(self):
        """Retained wildcard positions cannot silently move beyond their edited known field."""
        module = self.modules[2]
        data = b'<root xmlns="urn:choice" xmlns:e="urn:foreign"><item>a</item><item>b</item><e:extra/><item>c</item><item>d</item></root>'
        model = module.RepeatedRoot.decode_xml(data)
        self.assertEqual(model.item, ['a', 'b', 'c', 'd'])
        model.item[0] = 'changed'
        self.assertEqual(etree.fromstring(model.encode_xml())[0].text, 'changed')
        model.item = ['one']
        with self.assertRaises(ValueError): model.encode_xml()
        model.item = ['x'] * 10
        with self.assertRaises(ValueError): model.encode_xml()

    def test_choice_stops_before_later_fields_and_singleton_empty_branch(self):
        """Optional alternatives do not consume a later wildcard or require a nonempty branch."""
        module = self.sequence
        for body in ('<last/>', '<item>a</item><e:extra/><last/>', '<e:extra/><item>a</item><last/>'):
            data = ('<root xmlns="urn:choice" xmlns:e="urn:foreign">' + body + '</root>').encode()
            model = module.RepeatedRoot.decode_xml(data)
            self.assertEqual(etree.fromstring(model.encode_xml())[-1].tag, '{urn:choice}last')
        empty = self.empty.EmptyRoot.decode_xml(b'<root xmlns="urn:empty"/>')
        self.assertEqual(empty.item, ())
        self.assertEqual(len(etree.fromstring(empty.encode_xml())), 0)
        with self.assertRaises(ValueError):
            self.sequence.RepeatedRoot.decode_xml(b'<root xmlns="urn:choice" xmlns:e="urn:foreign"><last/><e:extra/></root>')

    def test_repeated_choice_uses_one_caller_operation_context(self):
        """Expose cancellation/resource failures before retaining or publishing an extra branch."""
        class context:
            """Record every shared charge and fail specifically inside repeated-choice dispatch."""
            def __init__(self): self.units, self.failed = 0, False
            def consume(self, resource, amount):
                if resource == 'work_units':
                    self.units += amount
                    if amount == 1 and self.units > 100:
                        self.failed = True
                        raise RuntimeError('choice operation cancelled')
            def check(self): pass
        selected = context()
        with self.assertRaisesRegex(RuntimeError, 'choice operation cancelled'):
            self.modules[0].RepeatedRoot.decode_xml(
                b'<root xmlns="urn:choice" xmlns:e="urn:foreign"><item>a</item><e:extra/></root>', context=selected)
        self.assertTrue(selected.failed)


def main():
    """Generate private fixture modules through the compiler, then run independent SDK checks."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    if build == ROOT or ROOT in build.parents and 'out' not in build.relative_to(ROOT).parts:
        parser.error('Repository scratch must remain under ignored out/')
    build.mkdir(parents=True, exist_ok=True)
    command = [str(args.compiler.resolve()), '--language', 'python']
    with tempfile.TemporaryDirectory(prefix='python-xml-choice-', dir=build) as temporary:
        directory = Path(temporary)
        def generate(label, xsd, fields, namespace='repeated'):
            """Keep each authority schema paired with one independently importable generated module."""
            (directory / (label + '.xsd')).write_text(xsd, encoding='utf-8')
            source = directory / (label + '.serializer')
            source.write_text('serializer version 1.20.0; namespace ' + namespace +
                ' { xml(unknown(preserve)); using schema "' + label + '.xsd"; class root { ' + fields + ' }}', encoding='utf-8')
            output = directory / (label + '.py')
            subprocess.run([*command, '--input', str(source), '--python.output', str(output)], check=True)
            return load_module(output, 'serializer_python_' + label)
        python_xml_choice_test.modules = [generate('repeated_' + str(index), schema(bounds), 'public array<string> item(1);')
                                          for index, (bounds, unused) in enumerate(ORACLES)]
        python_xml_choice_test.sequence = generate('sequence', schema((0, 3, 1, 2, 1, 2),
            '<xs:element name="last" type="xs:string"/>'), 'public array<string> item(1); public string last(2);')
        python_xml_choice_test.empty = generate('empty', '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"
          targetNamespace="urn:empty" elementFormDefault="qualified"><xs:element name="root"><xs:complexType>
          <xs:choice><xs:element name="item" type="xs:string" minOccurs="0"/>
          <xs:element name="other" type="xs:string"/></xs:choice></xs:complexType></xs:element></xs:schema>''',
          'public optional<string> item(1); public optional<string> other(2);', 'empty')
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_xml_choice_test))
        if not result.wasSuccessful(): raise SystemExit(1)


if __name__ == '__main__': main()
