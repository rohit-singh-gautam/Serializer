# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Exercise generated tree-free identity validation, forward references, and bounded indexes."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parent.parent
MODELS = None


class identity_generated_test(unittest.TestCase):
    """Only complete successful messages may return owning models or encoded documents."""
    def document(self, body):
        """Use the declared root while allowing each test to choose exact lexical tuples."""
        return ('<registry xmlns="urn:serializer:identity-test">' + body + '</registry>').encode()

    def decode(self, body, limits=None):
        """Split every provider token into one-byte input slices before invoking generated APIs."""
        data = self.document(body)
        return MODELS.IdentityXmlRegistry.decode_xml_chunks(
            (data[index:index + 1] for index in range(len(data))), limits=limits)

    def test_forward_references_and_independent_provider(self):
        """Canonical numeric aliases resolve even when all requests precede their target keys."""
        body = '<ref id="0007"/><ref id="7"/><entry id="7" label="  R&#x9; D  "/>'
        value = self.decode(body)
        self.assertEqual(value.entries[0].id, (7,))
        self.assertEqual(value.entries[0].label, ('R D',))
        self.assertEqual(self.decode(body.replace('0007', '+0007')).requests[0].id, (7,))
        data = value.encode_xml()
        self.assertEqual(MODELS.IdentityXmlRegistry.decode_xml(data).requests[0].id, (7,))
        from lxml import etree
        schema = etree.XMLSchema(etree.parse(str(ROOT / 'test/resources/xml_identity_generated.xsd')))
        self.assertTrue(schema.validate(etree.fromstring(self.document(body))), str(schema.error_log))
        self.assertTrue(schema.validate(etree.fromstring(data)))

    def test_duplicate_missing_and_unresolved_tuples_reject(self):
        """Keys are required, unique values use token equality, and keyrefs must resolve locally."""
        for body in ('<entry id="7"/><entry id="0007"/>', '<entry/>',
                     '<ref id="8"/><entry id="7"/>',
                     '<entry id="7" label="a b"/><entry id="8" label=" a&#x9;b "/>'):
            with self.subTest(body=body), self.assertRaises(ValueError):
                self.decode(body)
        # Incomplete unique/keyref tuples are omitted, rather than treated as empty strings.
        value = self.decode('<ref/><entry id="7"/><entry id="8"/>')
        self.assertEqual(value.requests[0].id, ())

    def test_output_rejects_invalid_aggregate_before_returning_bytes(self):
        """Generated output enforces the same key/reference invariants as generated input."""
        value = self.decode('<ref id="7"/><entry id="7"/>')
        value.requests[0].id = (8,)
        with self.assertRaises(ValueError):
            value.encode_xml()
        value.requests[0].id = (7,)
        value.entries.append(value.entries[0])
        with self.assertRaises(ValueError):
            value.encode_xml()

    def test_identity_limits_and_callback_failures(self):
        """Index admission is explicit and cancellation/charge failures cannot publish candidates."""
        for bound in ({'max_constraints': 0}, {'max_fields': 0}, {'max_selected_nodes': 0},
                      {'max_tuples': 0}, {'max_tuple_bytes': 0}, {'max_depth': 1},
                      {'max_work_units': 0}):
            limits = MODELS.Limits(xml_identity=MODELS.XmlIdentityLimits(**bound))
            with self.subTest(bound=bound), self.assertRaises(ValueError):
                self.decode('<entry id="7"/>', limits)
        class interrupted:
            """Fail an externally shared operation charge without changing the exception type."""
            def consume(self, resource, count):
                """Reject before retaining any requested logical decoder storage."""
                raise RuntimeError('identity operation interrupted')
        with self.assertRaisesRegex(RuntimeError, 'interrupted'):
            MODELS.IdentityXmlRegistry.decode_xml(self.document('<entry id="7"/>'), context=interrupted())

    def test_backend_preflight_preserves_every_existing_output(self):
        """An invalid identity contract must reject a multi-target batch before replacing any output."""
        python_output = self.build / 'preserved.py'
        java_output = self.build / 'preserved.java'
        sentinel = b'existing authored output must survive\n'
        for path in (python_output, java_output):
            path.write_bytes(sentinel)
        for name in ('xml_identity_generated.serializer','xml_identity_generated.ini'):
            (self.build/name).write_bytes((ROOT/'test/resources'/name).read_bytes())
        schema=(ROOT/'test/resources/xml_identity_generated.xsd').read_text()
        self.assertIn('refer="t:ids"',schema)
        (self.build/'xml_identity_generated.xsd').write_text(schema.replace('refer="t:ids"','refer="t:missing"'),encoding='utf-8')
        result = subprocess.run([str(self.compiler), '--input',
            str(self.build/'xml_identity_generated.serializer'), '--config',
            str(self.build/'xml_identity_generated.ini'), '--language', 'python,java',
            '--python.output', str(python_output), '--java.output', str(java_output)],
            capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('xsd', result.stderr.lower())
        for path in (python_output, java_output):
            self.assertEqual(path.read_bytes(), sentinel)

    def test_selected_key_field_declaration_must_not_be_nillable(self):
        """Reject a selected nillable key declaration even for non-nil text, but permit an empty selector result."""
        source=self.build/'nillable.serializer';config=self.build/'nillable.ini';schema=self.build/'nillable.xsd'
        source.write_text('''serializer version 1.20.0; namespace nullable_identity {
          class entry { public optional<string> name(1); }
          class root { public array<entry> entries(1); }}''',encoding='utf-8')
        schema.write_text('''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:nillable-identity"
          targetNamespace="urn:nillable-identity" elementFormDefault="qualified">
          <xs:complexType name="entry"><xs:sequence><xs:element name="name" type="xs:string" nillable="true"/></xs:sequence></xs:complexType>
          <xs:element name="entry" type="t:entry"/>
          <xs:element name="root"><xs:complexType><xs:sequence><xs:element name="item" type="t:entry" minOccurs="0" maxOccurs="unbounded"/></xs:sequence></xs:complexType>
          <xs:key name="keys"><xs:selector xpath="t:item"/><xs:field xpath="t:name"/></xs:key></xs:element></xs:schema>''',encoding='utf-8')
        config.write_text('[xml]\nenabled=true\nxsd=nillable.xsd\n[xml.class.nullable_identity::entry]\npresence.name=nil\n'
                          '[xml.class.nullable_identity::root]\nname.entries=item\n',encoding='utf-8')
        output=self.build/'nillable.py'
        subprocess.run([str(self.compiler),'--input',str(source),'--config',str(config),'--language','python','--python.output',str(output)],check=True)
        spec=importlib.util.spec_from_file_location('identity_nillable',output);module=importlib.util.module_from_spec(spec)
        sys.modules[spec.name]=module;spec.loader.exec_module(module)
        self.assertTrue(module.NullableIdentityRoot._xml_identity['constraints'][0]['fields'][0]['nillable'])
        empty=module.NullableIdentityRoot.decode_xml(b'<root xmlns="urn:nillable-identity"/>')
        self.assertEqual(empty.entries,[]);empty.encode_xml()
        for text in ('','present'):
            with self.assertRaisesRegex(ValueError,'nillable'):
                module.NullableIdentityRoot.decode_xml(('<root xmlns="urn:nillable-identity"><item><name>'+text+'</name></item></root>').encode())
        item=module.NullableIdentityEntry();empty.entries.append(item)
        for value in ((),('present',)):
            item.name=value
            with self.assertRaisesRegex(ValueError,'nillable'):empty.encode_xml()


def main():
    """Regenerate the schema through the actual compiler and execute the target-language tests."""
    global MODELS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    destination = args.build.resolve() / 'identity_model.py'
    subprocess.run([str(args.compiler.resolve()), '--input', str(ROOT / 'test/resources/xml_identity_generated.serializer'),
                    '--config', str(ROOT / 'test/resources/xml_identity_generated.ini'), '--language', 'python',
                    '--python.output', str(destination)], check=True)
    spec = importlib.util.spec_from_file_location('identity_model', destination)
    MODELS = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = MODELS
    spec.loader.exec_module(MODELS)
    identity_generated_test.compiler = args.compiler.resolve()
    identity_generated_test.build = args.build.resolve()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(identity_generated_test)
    return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
