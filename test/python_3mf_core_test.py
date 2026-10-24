"""Qualify actual Core1.4 XSD import through generated Python XML and OPC package facades."""
import argparse
import importlib.util
import io
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as etree
import zipfile

ROOT = Path(__file__).resolve().parent.parent
FIXTURE = ROOT / 'test/resources/3mf_core_1_4'
CORE = 'http://schemas.microsoft.com/3dmanufacturing/core/2015/02'
CONTENT_TYPES = 'http://schemas.openxmlformats.org/package/2006/content-types'
RELATIONSHIPS = 'http://schemas.openxmlformats.org/package/2006/relationships'
MODEL_MIME = 'application/vnd.ms-package.3dmanufacturing-3dmodel+xml'
MODEL_RELATIONSHIP = 'http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel'


def independent_package(model):
    """Use the standard library to author independent3MF infrastructure at a nondefault model path."""
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('Models/part.model', model)
        archive.writestr('[Content_Types].xml', '<Types xmlns="' + CONTENT_TYPES + '"><Default Extension="rels" '
            'ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            '<Override PartName="/Models/part.model" ContentType="' + MODEL_MIME + '"/></Types>')
        archive.writestr('_rels/.rels', '<Relationships xmlns="' + RELATIONSHIPS + '">'
            '<Relationship Id="model" Type="' + MODEL_RELATIONSHIP + '" Target="Models/part.model"/></Relationships>')
    return output.getvalue()


class python_3mf_core_test(unittest.TestCase):
    """Exercise schema fidelity and package discovery separately from application geometry semantics."""
    def test_legacy_owning_tree_route_is_available_without_token_support(self):
        """Keep the supported repeated-choice/wildcard Core contract on its bounded tree codec."""
        self.assertTrue(self.module._xml_tree_capable(self.model_type))
        with mock.patch.object(self.module, '_xml_token_capable', return_value=False):
            model = self.model_type.decode_xml(self.document)
            copy = self.model_type.decode_xml(model.encode_xml())
        self.assertEqual(copy.xsd_metadata[0].xsd_value, 'Tetrahedron')

    def test_actual_core_import_edit_and_independent_package_consumer(self):
        """Discover and decode a complete tetrahedron, edit it, then inspect the written3MF independently."""
        result = self.module.CorePackageArchive.load(independent_package(self.document))
        model = result.value.model
        self.assertEqual(model.xsd_metadata[0].xsd_value, 'Tetrahedron')
        self.assertEqual(model.xsd_unit, ('millimeter',))
        mesh = model.xsd_resources.xsd_object[0].xsd_mesh[0]
        self.assertEqual(len(mesh.xsd_vertices.xsd_vertex), 4)
        self.assertEqual(len(mesh.xsd_triangles.xsd_triangle), 4)
        self.assertEqual(model.xsd_build.xsd_item[0].xsd_objectid, '1')
        model.xsd_metadata[0].xsd_value = 'Edited tetrahedron'
        mesh.xsd_vertices.xsd_vertex[1].xsd_x = 2.0
        output = self.module.CorePackageArchive.encode(self.module.CorePackageArchive.Input(model=model))
        with zipfile.ZipFile(io.BytesIO(output)) as archive:
            self.assertIsNone(archive.testzip())
            metadata = etree.fromstring(archive.read('[Content_Types].xml'))
            self.assertTrue(any(entry.attrib.get('ContentType') == MODEL_MIME for entry in metadata))
            links = etree.fromstring(archive.read('_rels/.rels'))
            self.assertEqual(links[0].attrib['Type'], MODEL_RELATIONSHIP)
            payload = etree.fromstring(archive.read('3D/3dmodel.model'))
            self.assertEqual(payload.tag, '{' + CORE + '}model')
            self.assertEqual(payload.find('{' + CORE + '}metadata').text, 'Edited tetrahedron')
            self.assertEqual(payload.find('.//{' + CORE + '}vertices')[1].attrib['x'], '2.0')
        self.assertEqual(self.module.CorePackageArchive.load(output).value.model.xsd_metadata[0].xsd_value, 'Edited tetrahedron')

    def test_core_resource_choice_preservation_and_lexical_rejection(self):
        """Preserve foreign resources between known branch items and reject schema-invalid edited values."""
        material = b'<basematerials id="2"><base name="red" displaycolor="#FF0000"/></basematerials>'
        foreign = b'<v:resource xmlns:v="urn:vendor" flag="kept">head<v:child/>tail</v:resource>'
        document = self.document.replace(b'<resources>', b'<resources>' + material + foreign + material.replace(b'id="2"', b'id="3"'))
        model = self.model_type.decode_xml(document)
        output = etree.fromstring(model.encode_xml())
        resources = output.find('{' + CORE + '}resources')
        self.assertEqual([child.tag for child in resources], ['{' + CORE + '}basematerials', '{urn:vendor}resource',
                                                            '{' + CORE + '}basematerials', '{' + CORE + '}object'])
        self.assertEqual(resources[1].text, 'head')
        self.assertEqual(resources[1][0].tail, 'tail')
        for bad in (document.replace(b'xml:lang="en"', b'xml:lang="en_US"'),
                    document.replace(b'id="1"', b'id="-1"'),
                    document.replace(b'id="1"', b'id="0"'),
                    document.replace(b'displaycolor="#FF0000"', b'displaycolor="red"')):
            with self.assertRaises(ValueError): self.model_type.decode_xml(bad)
        model.xsd_resources.xsd_basematerials[0].xsd_base[0].xsd_displaycolor = 'red'
        with self.assertRaises(ValueError): model.encode_xml()


def main():
    """Import the pinned local schema, generate a real package facade and execute the Python consumer."""
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
    with tempfile.TemporaryDirectory(prefix='python-3mf-core-', dir=build) as temporary:
        directory = Path(temporary)
        source = directory / 'core.serializer'
        compiler = str(args.compiler.resolve())
        subprocess.run([compiler, '--import-xsd', str(FIXTURE / 'core.xsd'), '--namespace', 'core_model', '--root', 'model',
                        '--output', str(source)], check=True)
        authored = source.read_text(encoding='utf-8')
        match = re.search(r'class (\w+) stable_ids\s*\{\s*xml\(root\("model"\)', authored)
        if match is None: raise RuntimeError('Imported schema did not declare the requested model root')
        authored = authored.replace('namespace core_model {', 'namespace core_model {\n  xml(unknown(preserve));', 1)
        authored += '\nnamespace core_package { package archive { part model : core_model::' + match[1] + ''' {
          name("/3D/3dmodel.model"); content_type("''' + MODEL_MIME + '''");
          relationship("''' + MODEL_RELATIONSHIP + '''"); compression(deflate);
        } }}\n'''
        source.write_text(authored, encoding='utf-8')
        generated = directory / 'core.py'
        subprocess.run([compiler, '--input', str(source), '--language', 'python', '--python.output', str(generated)], check=True)
        spec = importlib.util.spec_from_file_location('serializer_python_core_fixture', generated)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        python_3mf_core_test.module = module
        python_3mf_core_test.model_type = module._XML_TYPES['{' + CORE + '}model']
        python_3mf_core_test.document = (FIXTURE / 'minimal.model').read_bytes()
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(python_3mf_core_test))
        if not result.wasSuccessful(): raise SystemExit(1)


if __name__ == '__main__': main()
