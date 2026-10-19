# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify pinned XSD bundle publication and independent exported-schema validation."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

XSD = '{http://www.w3.org/2001/XMLSchema}'


class bundle_cli_test(unittest.TestCase):
    """Exercise the real compiler and preserve edited destinations on every rejected export."""

    def setUp(self):
        """Create a complete source/config pair in uniquely owned temporary storage."""
        BUILD.mkdir(parents=True, exist_ok=True)
        self.directory = tempfile.TemporaryDirectory(prefix='serializer-xsd-export-', dir=BUILD)
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / 'model.serializer'
        self.source.write_text('serializer version 1.20.0; namespace sample { '
                               'class item { public uint32 id; } '
                               'class order { public item nested; public array string labels; } }', encoding='utf-8')
        self.config = self.root / 'model.ini'
        self.config.write_text('[xml]\nenabled=true\n[cpp]\nformat=false\n'
                               '[xml.class.sample::item]\nnamespace=urn:item\n'
                               '[xml.class.sample::order]\nnamespace=urn:order\n'
                               'wrapper.labels=labels\nname.labels=label\n', encoding='utf-8')
        self.bundle = self.root / 'bundle'

    def compile(self, *arguments, success=True):
        """Capture diagnostics and require the specified real CLI success/failure boundary."""
        result = subprocess.run([str(COMPILER), *map(str, arguments)], capture_output=True,
                                text=True, encoding='utf-8', timeout=120)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def export(self, success=True, *extra):
        """Export the same ordinary two-namespace wrapper contract through the new command."""
        return self.compile('--input', self.source, '--config', self.config,
                            '--xml.export-xsd-bundle', self.bundle, *extra, success=success)

    def inventory(self):
        """Recompute the documented pin independently from the complete published inventory."""
        manifest = json.loads((self.bundle / 'manifest.json').read_text(encoding='utf-8'))
        self.assertEqual(manifest['version'], 1)
        self.assertIn(manifest['entry'], manifest['schemas'])
        self.assertEqual(len(manifest['schemas']), 2)
        self.assertEqual(set(path.name for path in self.bundle.iterdir()), set(manifest['schemas']) | {'manifest.json'})
        digest = hashlib.sha256(b'Serializer XSD bundle 1\n')
        for name in sorted(manifest['schemas']):
            encoded = name.encode('utf-8')
            raw = (self.bundle / name).read_bytes()
            digest.update(str(len(encoded)).encode('ascii') + b':' + encoded)
            digest.update(str(len(raw)).encode('ascii') + b':' + raw)
            self.assertEqual(ET.fromstring(raw).tag, XSD + 'schema')
        self.assertEqual(manifest['sha256'], digest.hexdigest())
        self.assertEqual(list(self.root.glob('.serializer-xsd-*')), [])
        return manifest

    def test_bundle_pin_reimport_and_idempotent_publication(self):
        """Verify complete publication, deterministic reruns and compiler consumption of its pin."""
        self.export()
        manifest = self.inventory()
        before = {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in self.bundle.iterdir()}
        self.export()
        self.assertEqual(before, {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in self.bundle.iterdir()})
        generated = self.root / 'model.hpp'
        self.compile('--input', self.source, '--config', self.config, '--xml.xsd', self.bundle / manifest['entry'],
                     '--xml.bundle_sha256', manifest['sha256'], '--cpp.output', generated, '--include-mode', 'inline')
        self.assertTrue(generated.is_file())
        imported = self.root / 'imported.serializer'
        self.compile('--import-xsd', self.bundle / manifest['entry'], '--namespace', 'imported',
                     '--output', imported)
        self.compile('--input', imported, '--cpp.format', 'false', '--cpp.output', self.root / 'imported.hpp',
                     '--include-mode', 'inline')
        if ORACLE:
            self.check_oracle(manifest)

    def test_schema_only_tools_accept_all_without_enabling_a_codec(self):
        """Export and compare shared all metadata while preserving generated-backend rejection."""
        source = self.root / 'all.serializer'
        source.write_text('serializer version 1.20.0; class entry { '
                          'public uint32 id; public string label; }', encoding='utf-8')
        fixed = self.root / 'all.xsd'
        fixed.write_text('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">'
                         '<xs:element name="entry"><xs:complexType><xs:all>'
                         '<xs:element name="id" type="xs:unsignedInt"/>'
                         '<xs:element name="label" type="xs:string"/>'
                         '</xs:all></xs:complexType></xs:element></xs:schema>', encoding='utf-8')
        exported = self.root / 'all-exported.xsd'
        arguments = ('--input', source, '--xml.enabled', 'true', '--xml.xsd', fixed)
        self.compile(*arguments, '--xml.export-xsd', exported)
        self.assertIsNotNone(ET.parse(exported).find('.//' + XSD + 'all'))
        self.compile(*arguments, '--xml.compare-xsd', fixed)
        generated = self.root / 'all.hpp'
        generated.write_text('preserve existing output', encoding='utf-8')
        self.compile(*arguments, '--cpp.output', generated, '--cpp.format', 'false', success=False)
        self.assertEqual(generated.read_text(encoding='utf-8'), 'preserve existing output')
        if ORACLE:
            from lxml import etree
            validator = etree.XMLSchema(etree.parse(str(exported)))
            self.assertTrue(validator.validate(etree.XML(b'<entry><label>x</label><id>1</id></entry>')))
            self.assertFalse(validator.validate(etree.XML(b'<entry><id>1</id><id>2</id></entry>')))

    def check_oracle(self, manifest):
        """Use independently invoked lxml XSD validation for valid/invalid cross-namespace documents."""
        from lxml import etree
        parser = etree.XMLParser(no_network=True, resolve_entities=False)
        schemas = [etree.XMLSchema(etree.parse(str(self.bundle / name), parser)) for name in manifest['schemas']]
        root = etree.Element('{urn:order}order')
        nested = etree.SubElement(root, '{urn:order}nested')
        identifier = etree.SubElement(nested, '{urn:item}id')
        identifier.text = '7'
        labels = etree.SubElement(root, '{urn:order}labels')
        etree.SubElement(labels, '{urn:order}label').text = 'A&B'
        self.assertTrue(all(schema.validate(root) for schema in schemas))
        identifier.text = 'invalid'
        self.assertTrue(all(not schema.validate(root) for schema in schemas))

    def test_modified_or_extra_files_are_not_overwritten(self):
        """Preserve a reviewed directory and unrelated files when regeneration differs."""
        self.export()
        manifest = self.inventory()
        selected = self.bundle / manifest['entry']
        selected.write_bytes(selected.read_bytes() + b'\n<!-- reviewed edit -->\n')
        before = {path.name: path.read_bytes() for path in self.bundle.iterdir()}
        self.export(False)
        self.assertEqual(before, {path.name: path.read_bytes() for path in self.bundle.iterdir()})
        self.assertEqual(list(self.root.glob('.serializer-xsd-*')), [])

    def test_invalid_binding_and_combined_outputs_publish_nothing(self):
        """Reject unsupported invocation/configuration before creating any bundle destination."""
        self.export(False, '--depfile', self.root / 'out.d')
        self.assertFalse(self.bundle.exists())
        self.assertFalse((self.root / 'out.d').exists())
        self.export(False, '--cpp.output', self.root / 'out.hpp')
        self.assertFalse(self.bundle.exists())
        self.assertFalse((self.root / 'out.hpp').exists())
        self.config.write_text(self.config.read_text(encoding='utf-8') + 'name.missing=typo\n', encoding='utf-8')
        self.export(False)
        self.assertFalse(self.bundle.exists())
        self.assertEqual(list(self.root.glob('.serializer-xsd-*')), [])

    def test_exported_scalar_facets_have_independent_oracles(self):
        """Round-trip exact numeric/length/enumeration/pattern facets through actual XSD output."""
        source = self.root / 'facets.serializer'
        source.write_text('serializer version 1; class entry { public uint32 id; public string label; }', encoding='utf-8')
        fixed = self.root / 'fixed.xsd'
        fixed.write_text('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">'
                         '<xs:element name="entry"><xs:complexType><xs:sequence>'
                         '<xs:element name="id"><xs:simpleType><xs:restriction base="xs:unsignedInt">'
                         '<xs:minExclusive value="0"/><xs:maxInclusive value="10"/>'
                         '</xs:restriction></xs:simpleType></xs:element>'
                         '<xs:element name="label"><xs:simpleType><xs:restriction base="xs:string">'
                         '<xs:length value="3"/><xs:enumeration value="AAA"/><xs:enumeration value="BBB"/>'
                         '<xs:pattern value="[AB]{3}"/></xs:restriction></xs:simpleType></xs:element>'
                         '</xs:sequence></xs:complexType></xs:element></xs:schema>', encoding='utf-8')
        exported = self.root / 'facets.xsd'
        self.compile('--input', source, '--xml.enabled', 'true', '--xml.xsd', fixed,
                     '--xml.export-xsd', exported, '--cpp.format', 'false')
        tree = ET.parse(exported)
        self.assertEqual(len(tree.findall('.//' + XSD + 'enumeration')), 2)
        self.assertEqual(tree.find('.//' + XSD + 'minExclusive').get('value'), '0')
        if ORACLE:
            from lxml import etree
            validator = etree.XMLSchema(etree.parse(str(exported)))
            for identifier, label, valid in [('1', 'AAA', True), ('10', 'BBB', True), ('0', 'AAA', False),
                                             ('11', 'AAA', False), ('1', 'ABA', False), ('1', 'AA', False)]:
                document = etree.XML(f'<entry><id>{identifier}</id><label>{label}</label></entry>'.encode())
                self.assertEqual(validator.validate(document), valid)


if __name__ == '__main__':
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument('compiler', type=Path)
    arguments.add_argument('--build', type=Path, default=Path(__file__).resolve().parent.parent / 'out/build/xsd-bundle-cli')
    arguments.add_argument('--oracle', action='store_true', help='Require the pinned lxml independent XSD oracle')
    options, remaining = arguments.parse_known_args()
    COMPILER = options.compiler.resolve()
    BUILD = options.build.resolve()
    ORACLE = options.oracle
    unittest.main(argv=[__file__, *remaining])
