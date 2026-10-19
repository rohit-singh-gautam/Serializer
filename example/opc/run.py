# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Generate Python OPC codecs, exchange C++ packages, and inspect ZIP/XML independently."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

HERE = Path(__file__).resolve().parent
CATALOG_NS = 'urn:serializer:opc-example'
CONTENT_TYPES_NS = 'http://schemas.openxmlformats.org/package/2006/content-types'
RELATIONSHIPS_NS = 'http://schemas.openxmlformats.org/package/2006/relationships'
ATTACHMENT = bytes((0, 1, 254, 255))


def inspect(path):
    """Check generated metadata, actual compression, expanded XML names, and binary bytes."""
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == {
            '[Content_Types].xml', '_rels/.rels', 'Catalog/catalog.xml',
            'Catalog/_rels/catalog.xml.rels', 'Attachments/sample.bin'}
        assert archive.read('Attachments/sample.bin') == ATTACHMENT
        assert archive.getinfo('Catalog/catalog.xml').compress_type == zipfile.ZIP_DEFLATED
        root = ET.fromstring(archive.read('Catalog/catalog.xml'))
        assert root.tag == f'{{{CATALOG_NS}}}catalog'
        assert root.attrib == {'title': 'Parts & tools'}
        assert [child.tag for child in root] == [f'{{{CATALOG_NS}}}item'] * 2
        assert [child.attrib for child in root] == [
            {'id': '7', 'label': 'Bolt <M6>'}, {'id': '8', 'label': 'Nut'}]
        types = ET.fromstring(archive.read('[Content_Types].xml'))
        assert types.tag == f'{{{CONTENT_TYPES_NS}}}Types'
        overrides = {entry.attrib['PartName']: entry.attrib['ContentType'] for entry in types
                     if entry.tag == f'{{{CONTENT_TYPES_NS}}}Override'}
        assert overrides['/Catalog/catalog.xml'] == 'application/vnd.serializer.example.catalog+xml'
        for name, role in [('_rels/.rels', 'catalog'),
                           ('Catalog/_rels/catalog.xml.rels', 'attachment')]:
            links = ET.fromstring(archive.read(name))
            assert links.tag == f'{{{RELATIONSHIPS_NS}}}Relationships'
            assert len(links) == 1
            assert links[0].attrib['Type'] == f'urn:serializer:example:{role}'
            assert links[0].attrib.get('TargetMode', 'Internal') == 'Internal'


def verify(value):
    """Check the same aggregate using the generated Python owning APIs."""
    assert value.main.title == 'Parts & tools'
    assert [(item.id, item.label) for item in value.main.items] == [(7, 'Bolt <M6>'), (8, 'Nut')]
    assert len(value.attachments) == 1
    assert value.attachments[0].name == '/Attachments/sample.bin'
    assert value.attachments[0].data == ATTACHMENT


def main():
    """Require both implementations and check every producer with both readers plus zipfile."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--cpp', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    generated = args.build.resolve() / 'model.py'
    subprocess.run([str(args.compiler.resolve()), '--input', str(HERE / 'model.serializer'),
                    '--language', 'python', '--python.output', str(generated)], check=True)
    spec = importlib.util.spec_from_file_location('opc_example_model', generated)
    models = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = models
    spec.loader.exec_module(models)
    cpp_path = args.build.resolve() / 'cpp.opc'
    python_path = args.build.resolve() / 'python.opc'
    subprocess.run([str(args.cpp.resolve()), str(cpp_path)], check=True)
    verify(models.OpcExampleDocument.load(cpp_path).value)
    catalog = models.OpcExampleCatalog()
    catalog.title = 'Parts & tools'
    for identity, label in ((7, 'Bolt <M6>'), (8, 'Nut')):
        item = models.OpcExampleItem()
        item.id, item.label = identity, label
        catalog.items.append(item)
    value = models.OpcExampleDocument.Input(main=catalog, attachments=[
        models.BinaryPart(ATTACHMENT, '/Attachments/sample.bin', 'application/octet-stream')])
    models.OpcExampleDocument.save(python_path, value)
    verify(models.OpcExampleDocument.load(python_path).value)
    subprocess.run([str(args.cpp.resolve()), '--read', str(python_path)], check=True)
    for path in (cpp_path, python_path):
        inspect(path)
    print('OPC: four C++/Python producer-consumer checks and two independent ZIP/XML checks passed')


if __name__ == '__main__':
    main()
