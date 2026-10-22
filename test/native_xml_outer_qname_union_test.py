# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Check native scoped union selection and outer enum identities through owning companions."""
import native_xml_namespace_graph_test as graph
import portable_xml_outer_qname_union_test as scoped
import sys
import re

OWNER_ATTRIBUTES = '--attributes' in sys.argv
OWNER_TEXT = '--owner-text' in sys.argv
SCHEMA_DEFAULTS = '--schema-defaults' in sys.argv or OWNER_ATTRIBUTES or OWNER_TEXT
if '--schema-defaults' in sys.argv:
    sys.argv.remove('--schema-defaults')
for flag in ('--attributes', '--owner-text'):
    if flag in sys.argv:
        sys.argv.remove(flag)
scoped.OWNER_ATTRIBUTES, scoped.OWNER_TEXT = OWNER_ATTRIBUTES, OWNER_TEXT

graph.MODEL = scoped.shared.MODEL.replace('union.xsd', 'all.xsd')
graph.CONFIG = ''
graph.ORACLE_XSD = True
graph.ALL_XSD = scoped.shared.XSD.replace(
    'name="fixed_value" type="QNameFirst" fixed="a:item"',
    'name="fixed_value" type="BooleanFirst" fixed="1"').replace(
    'name="default_value" type="QNameFirst" default="a:item"',
    'name="default_value" type="QNameFirst"')
if SCHEMA_DEFAULTS:
    graph.ALL_XSD = scoped.shared.XSD
if OWNER_ATTRIBUTES or OWNER_TEXT:
    # Keep each occurrence's placement identical to the independently qualified portable fixture.
    def owner_scalar(match):
        """Retain optional schema defaults while mapping required native values to root attributes."""
        declaration = match.group(0)
        if OWNER_TEXT and 'name="default_value"' in declaration:
            return ''
        declaration = declaration.replace('<xs:element ', '<xs:attribute ')
        if ' fixed=' not in declaration and ' default=' not in declaration:
            declaration = declaration.replace('/>', ' use="required"/>')
        return declaration
    graph.ALL_XSD = re.sub(r'<xs:element name="(?:boolean_value|numeric_value|list_value|preserved|fixed_value|default_value)"[^>]*/>', owner_scalar, graph.ALL_XSD)
    graph.ALL_XSD = graph.ALL_XSD.replace('<xs:sequence>', '').replace('</xs:sequence>', '')
    if OWNER_TEXT:
        graph.ALL_XSD = graph.ALL_XSD.replace('<xs:element name="probe"><xs:complexType>',
            '<xs:element name="probe" default="a:item"><xs:complexType><xs:simpleContent><xs:extension base="QNameFirst">').replace(
            '</xs:complexType></xs:element>', '</xs:extension></xs:simpleContent></xs:complexType></xs:element>')
    for field in ('boolean_value', 'numeric_value', 'list_value', 'preserved', 'fixed_value', 'default_value'):
        placement = 'text' if OWNER_TEXT and field == 'default_value' else 'attribute'
        graph.MODEL = graph.MODEL.replace('string ' + field + ';', 'string ' + field + ' xml(' + placement + ');')
graph.KINDS = [('probe', 'OuterProbe', 'outer_probe')]


def probes():
    """Keep bound aliases, first string members and unbound QName fallback independently observable."""
    scoped.ENUMERATION_ONLY = not SCHEMA_DEFAULTS
    result = [('probe', document, valid) for document, valid in scoped.cases()]
    baseline = result[0][1]
    if not OWNER_ATTRIBUTES and not OWNER_TEXT:
        for value in ['p:item', 'missing:item', ' missing:item ', 'literal text', '', ' p:item ']:
            result.append(('probe', baseline.replace('<default_value></default_value>',
                          '<default_value>' + value + '</default_value>'), True))
    return result


def identity(document):
    """Resolve only selected QName members; ordinary string members retain authored whitespace."""
    from lxml import etree
    root = etree.fromstring(document)
    values = []
    items = [(node, node.tag, node.text or '') for node in root]
    if OWNER_ATTRIBUTES or OWNER_TEXT:
        items = [(root, field, (root.text if OWNER_TEXT and field == 'default_value' else root.get(field, '')) or '')
                 for field in ('boolean_value', 'numeric_value', 'list_value', 'preserved', 'fixed_value', 'default_value')]
    for node, field, text in items:
        if field == 'boolean_value':
            values.append((field, graph.shared.expanded(node, text, False)))
        elif SCHEMA_DEFAULTS and field in ('fixed_value', 'default_value'):
            if not text:
                selected = ('QName', [('urn:allowed', 'item')])
            else:
                normalized = ' '.join(text.split())
                try:
                    name = graph.shared.expanded(node, normalized, False)
                    prefix, local = normalized.split(':') if ':' in normalized else ('', normalized)
                    valid_name = bool(local) and (not prefix or prefix == 'xml' or bool(node.nsmap.get(prefix))) and all(part and etree.QName(part).localname == part for part in (prefix, local) if part)
                    selected = ('QName', name) if valid_name and (' ' not in normalized) else ('string', text)
                except (ValueError, AssertionError, KeyError):
                    selected = ('string', text)
            values.append((field, selected))
        elif field == 'list_value':
            values.append((field, [graph.shared.expanded(node, item, False) for item in text.split()]))
        else:
            values.append((field, text))
    return values


graph.probes = probes
graph.identity = identity

if __name__ == '__main__':
    graph.main()
