# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Check authored evolution with literal native wire bytes and independent XML/XSD providers."""
import hashlib
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

DIRECTORY = Path(__file__).resolve().parent


def manifest():
    """Read the independently authored field, wire and XML expectations."""
    return json.loads((DIRECTORY / 'manifest.json').read_text(encoding='utf-8'))


def xml_projection(payload, variant):
    """Require expanded names, sequence order and every authored value without using generated code."""
    expected = manifest()
    assert variant in expected['xml_children']
    assert b'<!DOCTYPE' not in payload, 'DOCTYPE is outside this fixture contract'
    root = ET.fromstring(payload)
    prefix = '{' + expected['namespace'] + '}'
    assert root.tag == prefix + expected['root'], 'Expanded root name differs'
    assert root.attrib == {'id': '7'}, 'Attribute identity/value differs'
    assert not (root.text or '').strip(), 'Unexpected root text'
    children = list(root)
    assert len(children) == len(expected['xml_children'][variant]), 'Child inventory differs'
    for child, (name, value) in zip(children, expected['xml_children'][variant], strict=True):
        assert child.tag == prefix + name, 'Expanded child name/order differs'
        assert child.attrib == {} and len(child) == 0, 'Unexpected nested structure'
        assert child.text == value and not (child.tail or '').strip(), 'Child content differs'
    return {'root': root.tag, 'attributes': root.attrib,
            'children': [[child.tag, child.text] for child in children]}


def schema_validator(variant):
    """Prepare the unchanged local XSD through an independent, network-disabled libxml2 provider."""
    from lxml import etree
    parser = etree.XMLParser(resolve_entities=False, no_network=True, load_dtd=False)
    return etree.XMLSchema(etree.parse(str(DIRECTORY / variant / 'order.xsd'), parser))


def inspect_xml(path, variant, validator):
    """Check an actual SDK document against both literal expectations and its independent XSD."""
    from lxml import etree
    payload = Path(path).read_bytes()
    projection = xml_projection(payload, variant)
    parser = etree.XMLParser(resolve_entities=False, no_network=True, load_dtd=False)
    validator.assertValid(etree.fromstring(payload, parser))
    return {'sha256': hashlib.sha256(payload).hexdigest(), 'projection': projection}


def inspect_native(path, name):
    """Compare actual ordinary native output to fixed, generator-independent protocol bytes."""
    payload = Path(path).read_bytes()
    native_payload(payload, name)
    return hashlib.sha256(payload).hexdigest()


def native_payload(payload, name):
    """Reject any changed field identity, order, value or optional representation in literal wire bytes."""
    assert payload == bytes.fromhex(manifest()['native_hex'][name]), 'Native wire bytes differ: ' + name


def self_test():
    """Demonstrate that independent controls reject identity, value, order and wire corruptions."""
    good = (b'<order xmlns="urn:serializer:xsd-evolution" id="7">'
            b'<customer>R&amp;D</customer><note>memo</note></order>')
    xml_projection(good, 'v1')
    invalid = (good.replace(b'xsd-evolution', b'wrong'), good.replace(b'id="7"', b'id="8"'),
               good.replace(b'<customer>', b'<client>').replace(b'</customer>', b'</client>'),
               good.replace(b'<note>memo</note>', b''), good.replace(b'R&amp;D', b'changed'),
               good.replace(b'<note>', b'<note extra="1">'),
               good.replace(b'<customer>R&amp;D</customer><note>memo</note>',
                            b'<note>memo</note><customer>R&amp;D</customer>'),
               good.replace(b'<note>memo</note>', b'<note><nested/></note>'))
    for payload in invalid:
        try:
            xml_projection(payload, 'v1')
        except (AssertionError, ET.ParseError):
            continue
        raise AssertionError('Independent negative control was accepted')
    v2 = good.replace(b'<customer>R&amp;D</customer><note>memo</note>',
                      b'<note>memo</note><client>R&amp;D</client>')
    xml_projection(v2, 'v2')
    assert manifest()['native_hex']['authored.bin'] == '070000000352264401046d656d6f'
    for name in ('authored.bin', 'authored-integer.bin', 'authored-string.bin'):
        payload = bytes.fromhex(manifest()['native_hex'][name])
        try:
            native_payload(bytes([payload[0] ^ 1]) + payload[1:], name)
        except AssertionError:
            continue
        raise AssertionError('Corrupted native protocol control was accepted')
    return {'positive_documents': 2, 'negative_documents': len(invalid), 'negative_native_protocols': 3}


if __name__ == '__main__':
    assert sys.argv[1:] == ['self-test']
    print(json.dumps(self_test()))
