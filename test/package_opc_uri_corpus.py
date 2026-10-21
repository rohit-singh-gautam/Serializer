# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Independent fifth-edition OPC URI, physical-to-logical mapping and XSD whitespace controls."""
import io
import struct
import xml.etree.ElementTree as etree
import zipfile

import python_package_test as independent


def named_model(name):
    """Change an inert logical part identity without changing native types or role relationships."""
    parts = independent.independent_parts()
    old = 'Alternate/root.xml'
    parts[name] = parts.pop(old)
    directory, filename = name.rsplit('/', 1)
    parts[directory + '/_rels/' + filename + '.rels'] = parts.pop('Alternate/_rels/root.xml.rels')
    root = etree.fromstring(parts['_rels/.rels']); root[0].set('Target', name)
    parts['_rels/.rels'] = etree.tostring(root)
    manifest = etree.fromstring(parts['[Content_Types].xml'])
    for node in manifest:
        if node.get('PartName') == '/' + old:
            node.set('PartName', '/' + name)
        if node.get('PartName') == '/Alternate/_rels/root.xml.rels':
            node.set('PartName', '/' + directory + '/_rels/' + filename + '.rels')
    parts['[Content_Types].xml'] = etree.tostring(manifest)
    return parts


def cases():
    """Keep ASCII logical naming mandatory, optional Unicode naming unsupported, and nonparts excluded."""
    for suffix in ('a:b.xml', 'a%3Ab.xml', 'a%00b.xml', 'a!b.xml', 'a$b.xml', 'a&b.xml', "a'b.xml",
                   'a(b).xml', 'a+b.xml', 'a,b.xml', 'a;b.xml', 'a=b.xml', 'a@b.xml'):
        yield 'uri-name-' + suffix.encode().hex(), independent.independent_zip(named_model('Alternate/' + suffix)), True
    parts = named_model('Alternate/a%3Ab.xml')
    root = etree.fromstring(parts['_rels/.rels']); root[0].set('Target', '/ALTERNATE/A%3ab.XML')
    parts['_rels/.rels'] = etree.tostring(root)
    yield 'uri-case-equivalent-target', independent.independent_zip(parts), True
    parts = named_model('Alternate/a:b.xml')
    root = etree.fromstring(parts['_rels/.rels']); root[0].set('Target', 'Alternate/./sub/../a:b.xml?choice=one#selected')
    parts['_rels/.rels'] = etree.tostring(root)
    yield 'uri-relative-selector-colon', independent.independent_zip(parts), True
    parts = named_model('Alternate/a:b.xml')
    root = etree.fromstring(parts['_rels/.rels']); root[0].set('Target', 'urn:part')
    parts['_rels/.rels'] = etree.tostring(root)
    yield 'uri-internal-absolute-scheme', independent.independent_zip(parts), False
    parts = independent.independent_parts()
    root = etree.fromstring(parts['_rels/.rels']); root[0].set('Target', 'Alternate//root.xml')
    parts['_rels/.rels'] = etree.tostring(root)
    yield 'uri-internal-empty-segment', independent.independent_zip(parts), False
    parts = independent.independent_parts()
    root = etree.fromstring(parts['_rels/.rels']); root[0].set('Target', '../../../Alternate/root.xml')
    parts['_rels/.rels'] = etree.tostring(root)
    yield 'uri-rfc-root-clamping', independent.independent_zip(parts), True
    for name in ('[trash]/opaque.bin', 'space name.bin', 'name..', 'strange^name.bin', '[trash]/_rels/not-parsed.rels'):
        parts = independent.independent_parts(); parts[name] = b'not XML; excluded physical payload'
        yield 'uri-excluded-' + name.encode().hex(), independent.independent_zip(parts), True
    parts = independent.independent_parts(); parts.update({'Textures/': b'', 'link.bin': b'not followed', 'folder.bin': b'not a part'})
    yield 'uri-excluded-file-attributes', independent.independent_zip(parts, attributes={'link.bin': (3, 0o120777 << 16), 'folder.bin': (0, 0x10)}), True
    parts = independent.independent_parts(); parts.update({'[trash]/a.bin': b'first', '[TRASH]/A.BIN': b'second'})
    yield 'uri-excluded-physical-case-pair', independent.independent_zip(parts), True
    parts = named_model('Alternate/a:b.xml'); parts['Alternate/A:B.XML'] = b'conflicting equivalent part'
    yield 'uri-case-collision', independent.independent_zip(parts), False
    for name in ('caf\u00e9.bin', 'caf%C3%A9.bin'):
        parts = independent.independent_parts(); parts[name] = b'optional non-ASCII logical naming'
        yield 'uri-unsupported-unicode-' + name.encode().hex(), independent.independent_zip(parts), False
    # anyURI and ID collapse XML whitespace; MIME/TargetMode remain their declared string value spaces.
    parts = independent.independent_parts()
    root = etree.fromstring(parts['_rels/.rels'])
    root[0].set('Id', ' \tmain\r\n '); root[0].set('Type', ' \turn:serializer:main-model\n ')
    root[0].set('Target', ' \tAlternate/root.xml\r\n '); parts['_rels/.rels'] = etree.tostring(root)
    manifest = etree.fromstring(parts['[Content_Types].xml'])
    for node in manifest:
        node.set('PartName', ' \t' + node.get('PartName') + '\r\n ')
    parts['[Content_Types].xml'] = etree.tostring(manifest)
    yield 'uri-collapsed-xsd-values', independent.independent_zip(parts), True
    parts = independent.independent_parts(); parts['[trash]/discard.bin'] = b'excluded still integrity checked'
    data = bytearray(independent.independent_zip(parts))
    with zipfile.ZipFile(io.BytesIO(data)) as reader:
        entry = reader.getinfo('[trash]/discard.bin')
        name_bytes, extra_bytes = struct.unpack_from('<HH', data, entry.header_offset + 26)
        data[entry.header_offset + 30 + name_bytes + extra_bytes] ^= 1
    yield 'uri-excluded-corrupt-payload', bytes(data), False
