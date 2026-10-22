# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Check schema-scoped outer QName union enumeration against equivalent instance prefixes."""
import sys

import portable_xml_outer_union_test as shared
shared.USE_JAXP = True
ENUMERATION_ONLY = False
OWNER_ATTRIBUTES = False
OWNER_TEXT = False

shared.XSD = shared.XSD.replace('<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">',
    '<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:a="urn:allowed">').replace(
    '<xs:element name="probe">', '''
<xs:simpleType name="QNameFirst"><xs:union memberTypes="xs:QName xs:string"/></xs:simpleType>
<xs:simpleType name="QNameChoice"><xs:restriction base="QNameFirst"><xs:enumeration value="a:item"/></xs:restriction></xs:simpleType>
<xs:simpleType name="StringFirst"><xs:union memberTypes="xs:string xs:QName"/></xs:simpleType>
<xs:simpleType name="StringChoice"><xs:restriction base="StringFirst"><xs:enumeration value="q:item"/></xs:restriction></xs:simpleType>
<xs:simpleType name="QNameList"><xs:list itemType="xs:QName"/></xs:simpleType>
<xs:simpleType name="QNameListFirst"><xs:union memberTypes="QNameList xs:string"/></xs:simpleType>
<xs:simpleType name="QNameListChoice"><xs:restriction base="QNameListFirst"><xs:enumeration value="a:one a:two"/></xs:restriction></xs:simpleType>
<xs:element name="probe">''').replace('name="boolean_value" type="Boolean"', 'name="boolean_value" type="QNameChoice"').replace(
    'name="numeric_value" type="Number"', 'name="numeric_value" type="StringChoice"').replace(
    'name="list_value" type="List"', 'name="list_value" type="QNameListChoice"').replace(
    'name="fixed_value" type="BooleanFirst" fixed="1"', 'name="fixed_value" type="QNameFirst" fixed="a:item"').replace(
    'name="default_value" type="NumberFirst" default="1"', 'name="default_value" type="QNameFirst" default="a:item"')


def cases():
    """Distinguish schema QName identity from a selected ordinary string with the same spelling."""
    baseline = dict(boolean_value='p:item', numeric_value='q:item', list_value='p:one p:two',
                    preserved=' raw ', fixed_value='true' if ENUMERATION_ONLY else 'p:item', default_value='')
    result = []
    alternatives_by_field = {
        'boolean_value': [('p:item', True), ('q:item', True), (' p:item ', True),
                          ('missing:item', False), ('p:wrong', False), ('literal text', False)],
        'numeric_value': [('q:item', True), ('p:item', False), (' q:item ', False), ('missing:item', False)],
        'list_value': [('p:one p:two', True), ('q:one p:two', True), (' p:one\tp:two ', True),
                       ('p:two p:one', False), ('p:one', False), ('missing:one p:two', False), ('', False)],
        'fixed_value': [('p:item', True), ('q:item', True), ('', True), ('p:wrong', False), ('missing:item', False)],
        'default_value': [('', True), ('p:item', True), ('literal text', True), ('missing:item', True)],
    }
    if ENUMERATION_ONLY:
        del alternatives_by_field['fixed_value']
        del alternatives_by_field['default_value']
    for field, alternatives in alternatives_by_field.items():
        for text, valid in alternatives:
            values = dict(baseline, **{field: text})
            document = '<probe xmlns:p="urn:allowed" xmlns:q="urn:allowed">' + ''.join(
                f'<{key}>{value}</{key}>' for key, value in values.items()) + '</probe>'
            if OWNER_ATTRIBUTES or OWNER_TEXT:
                from lxml import etree
                root = etree.fromstring(document.encode())
                for child in list(root):
                    text_value = child.text or ''
                    root.remove(child)
                    if OWNER_TEXT and child.tag == 'default_value':
                        root.text = text_value
                    elif text_value or child.tag not in ('fixed_value', 'default_value'):
                        root.set(child.tag, text_value)
                document = etree.tostring(root, encoding='unicode')
            result.append((document, valid))
    original = result[0][0]
    if OWNER_ATTRIBUTES or OWNER_TEXT:
        result.append((original.replace('xmlns:p="urn:allowed"', 'xmlns:p="urn:wrong"'), False))
        result.append((original.replace('xmlns:p="urn:allowed"', 'xmlns:p="urn:wrong"'), False))
    else:
        result.append((original.replace('<boolean_value>', '<boolean_value xmlns:p="urn:wrong">'), False))
        result.append((original.replace('<list_value>', '<list_value xmlns:p="urn:wrong">'), False))
    result.append((original.replace(' xmlns:q="urn:allowed"', ''), True))
    return result


shared.cases = cases

if __name__ == '__main__':
    if '--enumeration-only' in sys.argv:
        sys.argv.remove('--enumeration-only')
        ENUMERATION_ONLY = True
        shared.XSD = shared.XSD.replace('name="fixed_value" type="QNameFirst" fixed="a:item"',
            'name="fixed_value" type="BooleanFirst" fixed="1"').replace(
            'name="default_value" type="QNameFirst" default="a:item"', 'name="default_value" type="NumberFirst" default="1"')
    if '--attributes' in sys.argv or '--owner-text' in sys.argv:
        import re
        OWNER_TEXT = '--owner-text' in sys.argv
        OWNER_ATTRIBUTES = not OWNER_TEXT
        sys.argv.remove('--owner-text' if OWNER_TEXT else '--attributes')
        def owner_scalar(match):
            declaration = match.group(0)
            if OWNER_TEXT and 'name="default_value"' in declaration:
                return ''
            declaration = declaration.replace('<xs:element ', '<xs:attribute ')
            if ' fixed=' not in declaration and ' default=' not in declaration:
                declaration = declaration.replace('/>', ' use="required"/>')
            return declaration
        shared.XSD = re.sub(r'<xs:element name="(?:boolean_value|numeric_value|list_value|preserved|fixed_value|default_value)"[^>]*/>', owner_scalar, shared.XSD)
        shared.XSD = shared.XSD.replace('<xs:sequence>', '').replace('</xs:sequence>', '')
        if OWNER_TEXT:
            shared.XSD = shared.XSD.replace('<xs:element name="probe"><xs:complexType>',
                '<xs:element name="probe" default="a:item"><xs:complexType><xs:simpleContent><xs:extension base="QNameFirst">').replace(
                '</xs:complexType></xs:element>', '</xs:extension></xs:simpleContent></xs:complexType></xs:element>')
    if '--language' not in sys.argv:
        sys.argv += ['--language', 'java,csharp,go']
    shared.main()
