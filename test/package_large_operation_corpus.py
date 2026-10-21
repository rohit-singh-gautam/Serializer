# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Independently author and inspect large OPC/XML identity operation fixtures."""

import hashlib
import io
import json
from pathlib import Path
import xml.etree.ElementTree as etree
import zipfile

from package_shared_codec_budget_test import xml_inventory
from python_package_test import CT, RELS, independent_identity_parts, independent_zip


ROOT = Path(__file__).resolve().parents[1]
REGISTRY_COUNT = 3
DEFAULT_ENTRIES_PER_REGISTRY = 25000
OPAQUE_PART = 'Extensions/vendor.bin'
INTERNAL_TARGET = '../Extensions/vendor.bin#selected'
EXTERNAL_TARGET = 'https://example.invalid/a?x=1#selected'
OPAQUE_SIZE_BYTES = 64 * 1024
LABEL_HEX_CHARACTERS = 32
OPAQUE_BYTES = b''.join(hashlib.sha256(f'vendor-{index}'.encode('ascii')).digest()
                        for index in range(OPAQUE_SIZE_BYTES // hashlib.sha256().digest_size))


def fixtures(directory, entries_per_registry=DEFAULT_ENTRIES_PER_REGISTRY):
    """Write DEFLATE fixtures and independent inventory; return their reproducible manifest.

    Each owner repeats the same integer key space with locally unique labels.
    Forward keyrefs precede all entries. Unknown internal/external links and an
    inert opaque part require the caller's explicit preservation policy. The
    invalid archive changes only the final owner's last reference to a missing
    key; it must never reach aggregate validation or publication.
    """
    if not 1 <= entries_per_registry < (1 << 32) - 1:
        raise ValueError('Choose a positive uint32 key space with one spare invalid key')
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    parts = independent_identity_parts()
    inventories = {}
    for owner in range(REGISTRY_COUNT):
        name = f'Registries/registry{owner}.xml'
        references = ''.join(f'<ref id="{index}"/>' for index in range(1, entries_per_registry + 1))
        entries = ''.join(
            f'<entry id="{index}" label="{hashlib.sha256(f"{owner}:{index}".encode("ascii")).hexdigest()[:LABEL_HEX_CHARACTERS]}"/>'
            for index in range(1, entries_per_registry + 1))
        parts[name] = ('<registry xmlns="urn:serializer:identity-test">' + references + entries +
                       '</registry>').encode('utf-8')
        inventories[name] = dict(zip(('elements', 'attributes', 'text_bytes'), xml_inventory(parts[name])))
        (directory / f'registry{owner}.xml').write_bytes(parts[name])
    root_relationships = etree.fromstring(parts['_rels/.rels'])
    etree.SubElement(root_relationships, '{' + RELS + '}Relationship', {
        'Id': 'vendor', 'Type': 'urn:serializer:large-vendor', 'Target': OPAQUE_PART})
    etree.SubElement(root_relationships, '{' + RELS + '}Relationship', {
        'Id': 'external', 'Type': 'urn:serializer:large-external',
        'Target': EXTERNAL_TARGET, 'TargetMode': 'External'})
    parts['_rels/.rels'] = etree.tostring(root_relationships, encoding='utf-8')
    scoped_relationships = etree.Element('{' + RELS + '}Relationships')
    etree.SubElement(scoped_relationships, '{' + RELS + '}Relationship', {
        'Id': 'selected', 'Type': 'urn:serializer:large-selected', 'Target': INTERNAL_TARGET})
    parts['Registries/_rels/registry0.xml.rels'] = etree.tostring(scoped_relationships, encoding='utf-8')
    parts[OPAQUE_PART] = OPAQUE_BYTES
    content_types = etree.fromstring(parts['[Content_Types].xml'])
    etree.SubElement(content_types, '{' + CT + '}Override', {
        'PartName': '/' + OPAQUE_PART, 'ContentType': 'application/octet-stream'})
    parts['[Content_Types].xml'] = etree.tostring(content_types, encoding='utf-8')
    valid = independent_zip(parts)
    (directory / 'valid.opc').write_bytes(valid)
    invalid_parts = dict(parts)
    final_name = f'Registries/registry{REGISTRY_COUNT - 1}.xml'
    needle = f'<ref id="{entries_per_registry}"/>'.encode('ascii')
    replacement = f'<ref id="{entries_per_registry + 1}"/>'.encode('ascii')
    assert invalid_parts[final_name].count(needle) == 1
    invalid_parts[final_name] = invalid_parts[final_name].replace(needle, replacement, 1)
    (directory / 'invalid.opc').write_bytes(independent_zip(invalid_parts))
    manifest = {
        'registries': REGISTRY_COUNT,
        'entries_per_registry': entries_per_registry,
        'requests_per_registry': entries_per_registry,
        'model_elements': REGISTRY_COUNT * (1 + 2 * entries_per_registry),
        'identity_tuples': REGISTRY_COUNT * 3 * entries_per_registry,
        'relationships': 6,
        'xml_inventory': inventories,
        'xml_bytes': sum(len(parts[name]) for name in inventories),
        'zip_bytes': len(valid),
        'valid_sha256': hashlib.sha256(valid).hexdigest(),
        'opaque_part': OPAQUE_PART,
        'opaque_sha256': hashlib.sha256(OPAQUE_BYTES).hexdigest(),
        'internal_target': INTERNAL_TARGET,
        'external_target': EXTERNAL_TARGET,
    }
    (directory / 'inventory.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest


def assert_output(archive_path, entries_per_registry=DEFAULT_ENTRIES_PER_REGISTRY):
    """Check actual output using independent ZIP, XML, fixed XSD and original inert bytes.

    Prefixes, relationship IDs and relative spellings may be regenerated; source
    scope, selected target, external string and identity values must survive.
    The function opens local fixture data only and never resolves external URIs.
    """
    from lxml import etree as lxml_etree

    oracle = lxml_etree.XMLSchema(lxml_etree.parse(str(ROOT / 'test/resources/xml_identity_generated.xsd')))
    payload = Path(archive_path).read_bytes() if not isinstance(archive_path, bytes) else archive_path
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        assert archive.testzip() is None
        assert archive.read(OPAQUE_PART) == OPAQUE_BYTES
        assert len([name for name in archive.namelist() if name.startswith('Registries/registry')]) == REGISTRY_COUNT
        for owner in range(REGISTRY_COUNT):
            document = lxml_etree.fromstring(archive.read(f'Registries/registry{owner}.xml'))
            oracle.assertValid(document)
            references = document.findall('{urn:serializer:identity-test}ref')
            entries = document.findall('{urn:serializer:identity-test}entry')
            assert [int(item.attrib['id']) for item in references] == list(range(1, entries_per_registry + 1))
            assert [int(item.attrib['id']) for item in entries] == list(range(1, entries_per_registry + 1))
            expected_labels = [hashlib.sha256(f'{owner}:{index}'.encode('ascii')).hexdigest()[:LABEL_HEX_CHARACTERS]
                               for index in range(1, entries_per_registry + 1)]
            assert [item.attrib['label'] for item in entries] == expected_labels
        root_links = etree.fromstring(archive.read('_rels/.rels'))
        vendor = [link for link in root_links if link.attrib['Type'] == 'urn:serializer:large-vendor']
        assert len(vendor) == 1 and vendor[0].attrib['Target'] == OPAQUE_PART
        assert vendor[0].attrib.get('TargetMode', 'Internal') == 'Internal'
        external = [link for link in root_links if link.attrib['Type'] == 'urn:serializer:large-external']
        assert len(external) == 1 and external[0].attrib['Target'] == EXTERNAL_TARGET
        assert external[0].attrib.get('TargetMode') == 'External'
        selected = etree.fromstring(archive.read('Registries/_rels/registry0.xml.rels'))
        links = [link for link in selected if link.attrib['Type'] == 'urn:serializer:large-selected']
        assert len(links) == 1 and links[0].attrib['Target'] == INTERNAL_TARGET
