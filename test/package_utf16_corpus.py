# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Independent BOM UTF-16 OPC infrastructure cases; generated payload XML stays UTF-8."""
import re
import xml.etree.ElementTree as etree


def cases(parts):
    """Return copied part inventories with strict encoding, scalar and no-DTD controls."""
    manifest = '[Content_Types].xml'
    relationships = [name for name in parts if name.endswith('.rels')]
    selected = [manifest, *relationships]

    def source(name, declaration=True, label='UTF-16'):
        """Keep encoding-like payload text separate from the initial declaration being tested."""
        body = re.sub(r'^<\?xml\s.*?\?>', '', parts[name].decode('utf-8'), count=1)
        body = '<!-- Unicode \u6587\U0001f600; encoding="UTF-16" is comment data -->' + body
        return ('<?xml version="1.0" encoding="' + label + '"?>' if declaration else '') + body

    for endian, bom in [('le', b'\xff\xfe'), ('be', b'\xfe\xff')]:
        codec = 'utf-16-' + endian
        for label, names, declaration in [('manifest', [manifest], True),
                                           ('relationships', relationships, True),
                                           ('all', selected, True), ('no-declaration', selected, False)]:
            changed = dict(parts)
            for name in names:
                changed[name] = bom + source(name, declaration).encode(codec)
                assert etree.fromstring(changed[name]).tag == etree.fromstring(parts[name]).tag
            yield 'utf16-' + endian + '-' + label, changed, True
        for label, payload in [
            ('wrong-utf8-declaration', bom + source(manifest, label='UTF-8').encode(codec)),
            ('endian-declaration', bom + source(manifest, label='UTF-16' + endian.upper()).encode(codec)),
            ('missing-bom', source(manifest).encode(codec)),
            ('odd-byte', bom + source(manifest).encode(codec) + b'\x01'),
            ('isolated-high-surrogate', bom + (b'\x00\xd8' if endian == 'le' else b'\xd8\x00')),
            ('isolated-low-surrogate', bom + (b'\x00\xdc' if endian == 'le' else b'\xdc\x00')),
            ('xml11', bom + source(manifest).replace('version="1.0"', 'version="1.1"', 1).encode(codec)),
            ('dtd', bom + ('<!DOCTYPE Types SYSTEM "file:///serializer-no-fetch.dtd">' + source(manifest, False)).encode(codec)),
        ]:
            changed = dict(parts)
            changed[manifest] = payload
            yield 'utf16-' + endian + '-' + label, changed, False
    for endian, bom in [('le', b'\xff\xfe\x00\x00'), ('be', b'\x00\x00\xfe\xff')]:
        changed = dict(parts)
        changed[manifest] = bom + source(manifest).encode('utf-32-' + endian)
        yield 'utf32-' + endian, changed, False
    changed = dict(parts)
    changed[manifest] = source(manifest).encode('utf-8')
    yield 'utf8-mismatched-declaration', changed, False
