# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Qualify complete restricted atomic/union list items and ordered selected value identities."""
import portable_xml_outer_union_test as shared
shared.USE_JAXP = True
shared.XSD = shared.XSD.replace('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">',
    '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:a="urn:allowed">').replace(
    '<xs:element name="probe">', '''
<xs:simpleType name="BooleanItems"><xs:list itemType="BooleanFirst"/></xs:simpleType>
<xs:simpleType name="BooleanList"><xs:restriction base="BooleanItems"><xs:enumeration value="1 2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="NumberItems"><xs:list><xs:simpleType><xs:union memberTypes="xs:unsignedShort xs:boolean"/></xs:simpleType></xs:list></xs:simpleType>
<xs:simpleType name="NumberList"><xs:restriction base="NumberItems"><xs:enumeration value="1 true"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Digit"><xs:restriction base="xs:unsignedShort"><xs:maxInclusive value="9"/><xs:pattern value="[0-9]{1,3}"/><xs:enumeration value="1"/><xs:enumeration value="2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="DigitItems"><xs:list itemType="Digit"/></xs:simpleType>
<xs:simpleType name="DigitList"><xs:restriction base="DigitItems"><xs:enumeration value="1 2"/></xs:restriction></xs:simpleType>
<xs:simpleType name="Name"><xs:restriction base="xs:QName"><xs:enumeration value="a:item"/></xs:restriction></xs:simpleType>
<xs:simpleType name="NameItem"><xs:union memberTypes="Name xs:string"/></xs:simpleType>
<xs:simpleType name="NameList"><xs:list itemType="NameItem"/></xs:simpleType>
<xs:element name="probe">''').replace('name="boolean_value" type="Boolean"', 'name="boolean_value" type="BooleanList"').replace(
    'name="numeric_value" type="Number"', 'name="numeric_value" type="NumberList"').replace(
    'name="list_value" type="List"', 'name="list_value" type="DigitList"').replace(
    'name="fixed_value" type="BooleanFirst" fixed="1"', 'name="fixed_value" type="NameList" fixed="a:item literal"').replace(
    'name="default_value" type="NumberFirst" default="1"', 'name="default_value" type="NameList" default="a:item missing:item"')


def cases():
    """Keep atomic item facets distinct from list facets and first-successful union item identities."""
    baseline = dict(boolean_value='true 02', numeric_value='01 true', list_value='001 02',
                    preserved=' raw ', fixed_value='p:item literal', default_value='')
    result = []
    for field, alternatives in {
        'boolean_value': [('1 2', True), ('true 02', True), (' true\t002 ', True), ('01 2', False), ('true true', False), ('true 65536', False)],
        'numeric_value': [('1 true', True), ('01 true', True), ('+001 1', False), ('true 1', False), ('1 01', False), ('65536 true', False)],
        'list_value': [('1 2', True), ('001 02', True), ('1\t2', True), ('+1 2', False), ('0001 2', False), ('2 1', False), ('1 3', False), ('1 10', False)],
        'preserved': [(' raw ', True), ('raw', False), ('  raw  ', False), ('other', False)],
        'fixed_value': [('p:item literal', True), ('q:item literal', True), ('', True), ('p:wrong literal', False), ('missing:item literal', False), ('p:item other', False)],
        'default_value': [('', True), ('p:item missing:item', True), ('literal text', True), ('missing:item', True), ('p:wrong', True)],
    }.items():
        for text, valid in alternatives:
            values = dict(baseline, **{field: text})
            document = '<probe xmlns:p="urn:allowed" xmlns:q="urn:allowed">' + ''.join(
                f'<{key}>{value}</{key}>' for key, value in values.items()) + '</probe>'
            result.append((document, valid))
    return result


def check_output(source, output):
    """Independently compare per-item QName/string selection, including materialized schema defaults."""
    from lxml import etree
    def selected(root, local):
        node = root.find(local)
        text = node.text or ''
        if not text:
            return [('QName', ('urn:allowed', 'item')), ('string', 'literal' if local == 'fixed_value' else 'missing:item')]
        result = []
        for token in text.split():
            parts = token.split(':')
            uri = node.nsmap.get(parts[0], '') if len(parts) == 2 else node.nsmap.get(None, '')
            identity = (uri, parts[-1])
            result.append(('QName', identity) if identity == ('urn:allowed', 'item') else ('string', token))
        return result
    first, second = etree.fromstring(source), etree.fromstring(output)
    for local in ('fixed_value', 'default_value'):
        assert selected(first, local) == selected(second, local), (local, source, output)


shared.cases = cases
shared.CHECK_OUTPUT = check_output
if __name__ == '__main__':
    shared.main()
