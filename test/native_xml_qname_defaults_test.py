# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Verify native direct QName/list schema defaults independently from instance namespace bindings."""
import native_xml_namespace_graph_test as graph

graph.MODEL = '''serializer version 1.21.0; namespace defaults {
using schema "all.xsd"; xml(namespace("urn:defaults"));
class scalar { public string value xml(text); public string name xml(attribute); }
class fixed { public string value xml(text); public string name xml(attribute); }
class listed { public string value xml(text); public array<string> values xml(attribute,list); }
class list_fixed { public string value xml(text); public array<string> values xml(attribute,list); }
class local { public string value xml(text); public array<string> values xml(attribute,list); }
}'''
graph.CONFIG = ''
graph.ORACLE_XSD = True
HEAD = '''<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:defaults"
xmlns:q="urn:allowed" targetNamespace="urn:defaults" elementFormDefault="qualified">
<xs:simpleType name="Name"><xs:restriction base="xs:QName"><xs:enumeration value="q:Item"/><xs:enumeration value="q:Other"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Names"><xs:list itemType="xs:QName"/></xs:simpleType>
<xs:simpleType name="AllowedNames"><xs:restriction base="t:Names"><xs:enumeration value="q:Item q:Other"/></xs:restriction></xs:simpleType>
'''
FAMILIES = {
    'scalar': ('t:Name', 'name', 'q:Item', False, [('urn:allowed', 'Item')]),
    'fixed': ('t:Name', 'name', 'q:Item', True, [('urn:allowed', 'Item')]),
    'listed': ('t:AllowedNames', 'values', 'q:Item q:Other', False, [('urn:allowed', 'Item'), ('urn:allowed', 'Other')]),
    'list_fixed': ('t:AllowedNames', 'values', 'q:Item q:Other', True, [('urn:allowed', 'Item'), ('urn:allowed', 'Other')]),
    'local': ('t:Names', 'values', 'Local q:Item', False, [('', 'Local'), ('urn:allowed', 'Item')]),
}
graph.ALL_XSD = HEAD + ''.join(
    f'<xs:element name="{kind}" {"fixed" if fixed else "default"}="{literal}"><xs:complexType><xs:simpleContent><xs:extension base="{base}">'
    f'<xs:attribute name="{attribute}" type="{base}" {"fixed" if fixed else "default"}="{literal}"/>'
    '</xs:extension></xs:simpleContent></xs:complexType></xs:element>'
    for kind, (base, attribute, literal, fixed, _) in FAMILIES.items()) + '</xs:schema>'
graph.KINDS = [(kind, 'Defaults' + ''.join(part.title() for part in kind.split('_')), 'defaults_' + kind)
               for kind in FAMILIES]


def probes():
    """Keep authored schema aliases, missing values and explicit invalid values distinct."""
    result = []
    for kind, (_, attribute, literal, fixed, _) in FAMILIES.items():
        start = f'<t:{kind} xmlns:t="urn:defaults" xmlns:p="urn:allowed" xmlns:q="urn:wrong"'
        close = f'</t:{kind}>'
        explicit = literal.replace('q:', 'p:')
        result += [(kind, start + '>' + close, True),
                   (kind, start + ' xmlns="urn:instance">' + close, True),
                   (kind, start + f' {attribute}="{explicit}">{explicit}' + close, True),
                   (kind, start + f' {attribute}="">' + close, kind == 'local'),
                   (kind, start + f' {attribute}="{explicit}">missing:Item' + close, False),
                   (kind, start + f' {attribute}="missing:Item">' + close, False),
                   (kind, start + ' unknown="no">' + close, False)]
        if kind != 'local':
            result.append((kind, start + f' {attribute}="{literal}">' + close, False))
        if kind in ('scalar', 'fixed'):
            result.append((kind, start + f' {attribute}="p:Other">p:Other' + close, not fixed))
    return result


def identity(document):
    """Observe expanded instance names or the schema-selected literal when XML applies a default."""
    from lxml import etree
    root = etree.fromstring(document)
    kind = etree.QName(root).localname
    _, attribute, _, _, default_names = FAMILIES[kind]
    text = graph.shared.expanded(root, root.text, False) if root.text else default_names
    names = graph.shared.expanded(root, root.attrib[attribute], False) if attribute in root.attrib else default_names
    return text, names


def provider_presentation(document, valid):
    """Materialize schema-owned defaults for providers that reinterpret default prefixes in instance scope."""
    if not valid:
        return document
    from lxml import etree
    root = etree.fromstring(document)
    _, attribute, _, _, names = FAMILIES[etree.QName(root).localname]
    if root.text and attribute in root.attrib:
        return document
    bindings = dict(root.nsmap)
    if any(not uri for uri, _ in names):
        bindings[None] = ''
    tokens = []
    for uri, local in names:
        if not uri:
            tokens.append(local)
            continue
        prefix = next((prefix for prefix, bound in bindings.items() if prefix and bound == uri), None)
        if prefix is None:
            ordinal = 0
            while 'oracle' + str(ordinal) in bindings:
                ordinal += 1
            prefix = 'oracle' + str(ordinal)
            bindings[prefix] = uri
        tokens.append(prefix + ':' + local)
    control = etree.Element(root.tag, nsmap=bindings)
    control.attrib.update(root.attrib)
    control.text = root.text or ' '.join(tokens)
    if attribute not in control.attrib:
        control.set(attribute, ' '.join(tokens))
    result = etree.tostring(control, encoding='utf-8')
    assert identity(document) == identity(result), 'Provider control changed expanded values'
    return result


graph.probes = probes
graph.identity = identity
graph.ORACLE_PRESENTATION = provider_presentation

if __name__ == '__main__':
    graph.main()
