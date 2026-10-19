# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify generated Python XML lifecycle metadata and exact owning digest projections."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as etree
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parent.parent
NS, META = 'urn:serializer:model-tokens', 'urn:serializer:xml:model'


def revision_document(root, revision, payload, magic=None, attributes=''):
    """Build metadata by expanded identity independently of the generated namespace allocator."""
    metadata = ' xmlns:m="' + META + '" m:revision=' + quoteattr(revision)
    if magic is not None:
        metadata += ' m:magic=' + quoteattr(magic)
    return ('<' + root + ' xmlns="' + NS + '"' + metadata + attributes + '>' +
            payload + '</' + root + '>').encode()


def load_module(path, name):
    """Load one real compiler output through the target SDK's normal import path."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class python_xml_model_test(unittest.TestCase):
    """Exercise native storage, exact historical layouts and reserved XML model metadata together."""
    def test_integer_lifecycle_magic_and_native_wire_controls(self):
        """Select exact uint64 layouts above floating precision and preserve all native protocol bytes."""
        module = self.module
        for revision, payload, old, current in (
            ('9007199254740993', '<old>retained</old>', 'retained', 'new-default'),
            ('9007199254740994', '<current>selected</current>', 'old-default', 'selected'),
            ('9007199254740995', '<current>selected</current>', 'old-default', 'selected')):
            data = revision_document('numeric', revision, payload, '53524c', ' kept="7"')
            with self.subTest(revision=revision):
                with mock.patch.object(module, '_xml_parse', side_effect=AssertionError('retained tree')):
                    value = module.ModelsNumeric.decode_xml_chunks(data[i:i+1] for i in range(len(data)))
                self.assertEqual((value.ver, value.kept, value.old, value.current), (int(revision), 7, old, current))
                before = [value.encode(protocol) for protocol in module.Protocol]
                with mock.patch.object(module, '_xml_write', side_effect=AssertionError('retained tree')):
                    output = value.encode_xml()
                root = etree.fromstring(output)
                self.assertEqual(root.attrib['{' + META + '}revision'], revision)
                self.assertEqual(root.attrib['{' + META + '}magic'], '53524c')
                self.assertEqual([child.tag for child in root], ['{' + NS + '}' + ('old' if old == 'retained' else 'current')])
                self.assertEqual([value.encode(protocol) for protocol in module.Protocol], before)
                for protocol, expected in zip(module.Protocol, before):
                    plain = self.native.ModelsNumeric.decode(expected, protocol,
                        self.native.Limits(read_policy=self.native.ReadPolicy.FLEXIBLE))
                    self.assertEqual(plain.encode(protocol), expected)
                self.assertEqual(module.ModelsNumeric.decode_xml(output).ver, int(revision))
        typed = module.ModelsTyped.decode_xml(
            ('<typed xmlns="' + NS + '" xmlns:m="' + META + '" m:magic="4660"><value>9</value></typed>').encode())
        self.assertEqual(etree.fromstring(typed.encode_xml()).attrib['{' + META + '}magic'], '4660')

    def test_dotted_floating_inherited_and_nested_layouts(self):
        """Compare dotted components numerically and select floating revisions at their declared width."""
        module = self.module
        for revision, element in (('1.2.0', 'old'), ('1.2.9', 'old'), ('1.3.0', 'current'),
                                  ('1.9.65535', 'current'), ('1.10.0', 'current')):
            data = revision_document('dotted', revision, '<' + element + '>5</' + element + '>')
            value = module.ModelsDotted.decode_xml(data)
            self.assertEqual(value.ver, revision)
            self.assertEqual(etree.fromstring(value.encode_xml()).attrib['{' + META + '}revision'], revision)
        for revision, element, expected in (('0.5', 'old', 0.5), ('0.74999999', 'current', 0.75),
                                             ('0.75', 'current', 0.75), ('1', 'current', 1.0)):
            value = module.ModelsFloating.decode_xml(revision_document('floating', revision,
                '<' + element + '>5</' + element + '>'))
            self.assertEqual(value.ver, expected)
            self.assertEqual(module.ModelsFloating.decode_xml(value.encode_xml()).ver, expected)
        derived = module.ModelsDerived.decode_xml(revision_document('derived', '1', '<extra>present</extra>'))
        self.assertEqual((derived.base0.ver, derived.base0.kept, derived.extra), (1, 7, 'present'))
        self.assertEqual(len(etree.fromstring(derived.encode_xml())), 1)
        derived.base0.ver = 3
        self.assertEqual([child.tag for child in etree.fromstring(derived.encode_xml())],
                         ['{' + NS + '}kept', '{' + NS + '}extra'])
        nested = module.ModelsEnvelope()
        nested.value.ver, nested.value.kept, nested.value.current = 9007199254740995, 7, 'selected'
        output = nested.encode_xml()
        self.assertEqual(module.ModelsEnvelope.decode_xml(output).value.current, 'selected')
        with self.assertRaises(ValueError):
            module.ModelsEnvelope.decode_xml(output.replace(b'53524c', b'53524d'))

    def test_invalid_reserved_metadata_rejects_before_construction(self):
        """Reject unknown/missing/wrong metadata and inactive fields before publication."""
        module = self.module
        valid = revision_document('numeric', '9007199254740995', '<current>x</current>', '53524c', ' kept="7"')
        invalid = (
            valid.replace(b' m:revision="9007199254740995"', b''),
            valid.replace(b' m:magic="53524c"', b''),
            valid.replace(b'53524c', b'53524C'),
            valid.replace(b'm:revision=', b'revision='),
            valid.replace(b' m:magic=', b' m:unknown="x" m:magic='),
            valid.replace(b'9007199254740995', b'9007199254740992'),
            valid.replace(b'9007199254740995', b'9007199254740996'),
            valid.replace(b'9007199254740995', b'+9007199254740995'),
            valid.replace(b'9007199254740995', b'09007199254740995'),
        )
        with mock.patch.object(module.ModelsNumeric, '__init__', side_effect=AssertionError('constructed')):
            for data in invalid:
                with self.subTest(data=data):
                    with self.assertRaises(ValueError):
                        module.ModelsNumeric.decode_xml(data)
        for data in (valid.replace(b'<current>x</current>', b'<old>x</old>'),
                     valid.replace(b'<current>x</current>', b''),
                     valid.replace(b'</numeric>', b'</numeric><extra/>')):
            with self.assertRaises((ValueError, module._xml_expat.ExpatError)):
                module.ModelsNumeric.decode_xml(data)
        for revision in ('1.11.0', '1.02.0', '1.2', '1.2.0.0', '65536.1.2', ' 1.2.0', '"1.2.0"'):
            with self.assertRaises(ValueError):
                module.ModelsDotted.decode_xml(revision_document('dotted', revision, '<old>5</old>'))
        for revision in ('0.49', '1.01', '-0', '.5', 'NaN', 'INF', '1e999', '1e-999'):
            with self.assertRaises(ValueError):
                module.ModelsFloating.decode_xml(revision_document('floating', revision, '<old>5</old>'))
        for metadata in ('m:revision="1"', 'm:magic="x"', 'm:unknown="x"'):
            with self.assertRaises(ValueError):
                module.ModelsPlain.decode_xml(('<plain xmlns="' + NS + '" xmlns:m="' + META + '" ' +
                    metadata + '><value>1</value></plain>').encode())

    def test_exact_digest_hex_extent_limits_and_native_storage(self):
        """Project bytes exactly with no hash computation, whitespace tolerance or native layout changes."""
        module = self.module
        data = ('<digests xmlns="' + NS + '" raw="" exact="00fF" maybe="AAbb">'
                '<algorithm>' + '01' * 16 + '</algorithm><items><item>AaBb</item><item>0000</item></items></digests>').encode()
        value = module.ModelsDigests.decode_xml(data)
        self.assertEqual((value.raw, value.exact, value.algorithm, value.items, value.maybe),
                         ([], [0, 255], [1] * 16, [[170, 187], [0, 0]], ([170, 187],)))
        output = value.encode_xml()
        root = etree.fromstring(output)
        self.assertEqual((root.attrib['raw'], root.attrib['exact'], root.attrib['maybe']), ('', '00ff', 'aabb'))
        self.assertTrue(self.digest_oracle.validate(self.provider.fromstring(output)))
        for protocol in module.Protocol:
            wire = value.encode(protocol)
            self.assertEqual(self.native.ModelsDigests.decode(wire, protocol).encode(protocol), wire)
        for data in (data.replace(b'00fF', b'00'), data.replace(b'00fF', b'00gg'),
                     data.replace(b'00fF', b'00 fF'), data.replace(b'AaBb', b'abc'),
                     data.replace(b'01' * 16, b'01' * 15)):
            with self.assertRaises(ValueError):
                module.ModelsDigests.decode_xml(data)
        for bad in ([0], [True, 0], [-1, 0], [0, 256]):
            value.exact = bad
            with self.assertRaises(ValueError):
                value.encode_xml()
        value.exact, value.maybe = [0, 255], ()
        self.assertNotIn('maybe', etree.fromstring(value.encode_xml()).attrib)
        value.raw = [255] * module._XML_DIGEST_MAX_BYTES
        boundary = value.encode_xml()
        self.assertEqual(len(module.ModelsDigests.decode_xml(boundary).raw), module._XML_DIGEST_MAX_BYTES)
        value.raw = [0] * (module._XML_DIGEST_MAX_BYTES + 1)
        with self.assertRaises(ValueError):
            value.encode_xml()
        with self.assertRaises(ValueError):
            module.ModelsDigests.decode_xml(output, module.Limits(max_string_bytes=8))

    def test_shared_mixed_revision_restriction_preserves_output(self):
        """Keep the shared ordinary-owner mixed-content restriction explicit and transactional."""
        source = self.directory / 'invalid.serializer'
        source.write_text('''serializer version 1.20.0; namespace invalid {
          class child { public uint32 value(1); }
          class fragment { public variant(string=text, child=child) value(1); }
          class root { public version uint16 ver(1) { 3 } compatibility { 1 };
            created(2) public array<fragment> content(2); }}''', encoding='utf-8')
        config = self.directory / 'invalid.ini'
        config.write_text('[xml]\nenabled=true\n[xml.class.invalid::root]\nmixed=content\n'
                          'mixed.variant=value\nmixed.text=text\n', encoding='utf-8')
        output = self.directory / 'sentinel.py'
        output.write_text('preserve existing output\n', encoding='utf-8')
        result = subprocess.run([str(self.compiler), '--language', 'python', '--input', str(source),
                                 '--config', str(config), '--python.output', str(output)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('mixed content requires one explicit ordinary ordered field', result.stderr)
        self.assertEqual(output.read_text(encoding='utf-8'), 'preserve existing output\n')


def main():
    """Generate XML-enabled and XML-disabled controls and require an actual independent digest provider."""
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
    with tempfile.TemporaryDirectory(prefix='python-xml-models-', dir=build) as temporary:
        directory = Path(temporary)
        source = directory / 'models.serializer'
        namespace = 'xml(namespace("' + NS + '"));'
        source.write_text('''serializer version 1.20.0; namespace models {
        class numeric { '''+namespace+'''
          public magic(99) { 'SRL' }; public version uint64 ver(1) { 9007199254740995 } compatibility { 9007199254740993 };
          public uint32 kept(2) xml(attribute); obsolete(9007199254740994) public string old(3) { "old-default" };
          created(9007199254740994) public string current(4) { "new-default" }; }
        class dotted { '''+namespace+''' public version version3 ver(1) { "1.10.0" } compatibility { "1.2.0" };
          obsolete("1.3.0") public uint16 old(2); created("1.3.0") public uint16 current(3); }
        class floating { '''+namespace+''' public version float ver(1) { 1.0 } compatibility { 0.5 };
          obsolete(0.75) public uint32 old(2); created(0.75) public uint32 current(3); }
        class base { '''+namespace+''' public version uint16 ver(1) { 3 } compatibility { 1 }; created(2) public uint32 kept(2) { 7 }; }
        class derived : public base { '''+namespace+''' public string extra(10) { "future" }; }
        class typed { '''+namespace+''' public magic uint16(99) { 4660 }; public uint32 value(1); }
        class plain { '''+namespace+''' public uint32 value(1); }
        class envelope { '''+namespace+''' public numeric value(1); }
        class digests { '''+namespace+''' public digest raw(1) xml(attribute); public digest[2] exact(2) xml(attribute);
          public digest(md5) algorithm(3); public array digest[2] items(4); public optional<digest[2]> maybe(5) xml(attribute); }
        }''', encoding='utf-8')
        config = directory / 'models.ini'
        config.write_text('[xml]\nenabled=true\n[xml.class.models::digests]\nwrapper.items=items\nname.items=item\n', encoding='utf-8')
        native_config = directory / 'native.ini'
        native_config.write_text('[xml]\nenabled=false\n', encoding='utf-8')
        command = [str(args.compiler.resolve()), '--language', 'python', '--input', str(source)]
        output, native = directory / 'models.py', directory / 'native.py'
        subprocess.run([*command, '--config', str(config), '--python.output', str(output)], check=True)
        subprocess.run([*command, '--config', str(native_config), '--python.output', str(native)], check=True)
        python_xml_model_test.module = load_module(output, 'serializer_python_xml_model_fixture')
        python_xml_model_test.native = load_module(native, 'serializer_python_xml_model_native')
        python_xml_model_test.provider = provider
        python_xml_model_test.directory, python_xml_model_test.compiler = directory, args.compiler.resolve()
        python_xml_model_test.digest_oracle = provider.XMLSchema(provider.fromstring(('''
        <xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="'''+NS+'''" targetNamespace="'''+NS+'''" elementFormDefault="qualified">
        <xs:simpleType name="Pair"><xs:restriction base="xs:hexBinary"><xs:length value="2"/></xs:restriction></xs:simpleType>
        <xs:simpleType name="Md5"><xs:restriction base="xs:hexBinary"><xs:length value="16"/></xs:restriction></xs:simpleType>
        <xs:element name="digests"><xs:complexType><xs:sequence><xs:element name="algorithm" type="t:Md5"/>
        <xs:element name="items"><xs:complexType><xs:sequence><xs:element name="item" type="t:Pair" minOccurs="0" maxOccurs="unbounded"/>
        </xs:sequence></xs:complexType></xs:element></xs:sequence><xs:attribute name="raw" type="xs:hexBinary" use="required"/>
        <xs:attribute name="exact" type="t:Pair" use="required"/><xs:attribute name="maybe" type="t:Pair"/>
        </xs:complexType></xs:element></xs:schema>''').encode()))
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_xml_model_test))
        if not result.wasSuccessful():
            raise SystemExit(1)


if __name__ == '__main__':
    main()
